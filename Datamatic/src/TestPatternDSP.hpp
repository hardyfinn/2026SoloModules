// Test Pattern: the four Ikeda voices. Plain C++ (no Rack) for offline tests.
//   SUB   low sine, starts at zero phase, stops dead when the gate falls (or
//         after LEN). SHAPE drives it from pure sine towards a square.
//   TONE  sine in three zones: LO 100-1000 Hz, MID 250-4000 Hz (centre = the
//         1 kHz test tone), HI 4-18 kHz. BEAT detunes the right side by up to
//         20 Hz so left and right interfere. SCAN lets the DATA byte pick a
//         harmonic (1..16) at each new note.
//   CLICK a rectangular impulse 1-32 samples wide, COLOUR dulls it, SPREAD
//         throws alternate clicks hard left / right.
//   NOISE a white noise burst 1-300 ms, COLOUR thins it to hiss, SPREAD
//         makes left and right independent.
// No envelopes anywhere: the hard edges are part of the sound. CUT silences
// everything instantly.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace testpattern {

static const float TWO_PI = 6.28318530718f;

struct Params {
	float subPitch = 0.4f, subShape = 0.f, subLen = 1.f, subLevel = 0.8f;
	int toneZone = 2;   // 0 LO, 1 MID, 2 HI
	float tonePitch = 0.5f, toneBeat = 0.15f, toneScan = 0.f, toneLevel = 0.45f;
	float clickWidth = 0.f, clickColour = 0.f, clickSpread = 0.7f, clickLevel = 0.8f;
	float noiseLen = 0.35f, noiseColour = 0.3f, noiseSpread = 1.f, noiseLevel = 0.35f;
};

struct Frame {
	float l = 0.f, r = 0.f;
	float voice[4] = {0.f, 0.f, 0.f, 0.f};   // mono per voice, before level
	bool active[4] = {false, false, false, false};
};

inline float subHz(float k) { return 25.f * std::pow(2.f, k * std::log2(160.f / 25.f)); }
inline float toneHz(int zone, float k) {
	switch (zone) {
		case 0: return 100.f * std::pow(10.f, k);              // 100..1000
		case 1: return 1000.f * std::pow(4.f, (k - 0.5f) * 2.f);   // 250..4000, 1 kHz at noon
		default: return 4000.f * std::pow(4.5f, k);            // 4k..18k
	}
}
inline float clickWidthSamples(float k) { return std::round(std::pow(32.f, k)); }
inline float noiseSeconds(float k) { return 0.001f * std::pow(300.f, k); }
inline float subSeconds(float k) { return k >= 0.99f ? 1e9f : 0.002f * std::pow(500.f, k); }

struct Engine {
	bool lastGate[4] = {false, false, false, false};
	// SUB
	float subPhase = 0.f, subTime = 0.f;
	bool subOn = false;
	// TONE
	float tonePhaseL = 0.f, tonePhaseR = 0.f, toneRatio = 1.f;
	bool toneOn = false;
	// CLICK
	int clickLeft = 0;
	float clickLpL = 0.f, clickLpR = 0.f;
	float clickPan = 0.f;
	int clickCount = 0;
	// NOISE
	float noiseLeft = 0.f;
	float hpL = 0.f, hpR = 0.f, hpInL = 0.f, hpInR = 0.f;
	uint32_t rng = 22222u;

	float noise() {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
		return (rng >> 8) * (2.f / 16777216.f) - 1.f;
	}

	// gates: SUB, TONE, CLICK, NOISE. data 0..10 V. voct for SUB and TONE.
	Frame process(float sr, const bool gate[4], bool cut, float data, float subVoct, float toneVoct, const Params& p) {
		Frame f;
		float dt = 1.f / sr;
		bool rise[4];
		for (int i = 0; i < 4; i++) { rise[i] = gate[i] && !lastGate[i]; lastGate[i] = gate[i]; }

		// ---- SUB
		if (rise[0]) { subPhase = 0.f; subTime = 0.f; subOn = true; }
		if (!gate[0]) subOn = false;
		if (subOn) {
			float hz = std::min(sr * 0.45f, subHz(p.subPitch) * std::pow(2.f, subVoct));
			float s = std::sin(TWO_PI * subPhase);
			if (p.subShape > 0.001f) {
				float k = 1.f + p.subShape * p.subShape * 40.f;
				s = std::tanh(k * s) / std::tanh(k);
			}
			f.voice[0] = s;
			subPhase += hz * dt;
			subPhase -= std::floor(subPhase);
			subTime += dt;
			if (subTime >= subSeconds(p.subLen)) subOn = false;
		}
		f.active[0] = subOn;

		// ---- TONE
		if (rise[1]) {
			tonePhaseL = tonePhaseR = 0.f;
			toneOn = true;
			int n = 1 + (int) (std::max(0.f, std::min(0.999f, data / 10.f)) * (1.f + std::round(p.toneScan * 15.f)));
			toneRatio = (float) std::max(1, n);
		}
		if (!gate[1]) toneOn = false;
		float toneL = 0.f, toneR = 0.f;
		if (toneOn) {
			float hz = toneHz(p.toneZone, p.tonePitch) * std::pow(2.f, toneVoct) * toneRatio;
			while (hz > sr * 0.45f) hz *= 0.5f;
			float beat = 20.f * p.toneBeat * p.toneBeat;
			toneL = std::sin(TWO_PI * tonePhaseL);
			toneR = std::sin(TWO_PI * tonePhaseR);
			tonePhaseL += hz * dt; tonePhaseL -= std::floor(tonePhaseL);
			tonePhaseR += std::min(sr * 0.49f, hz + beat) * dt; tonePhaseR -= std::floor(tonePhaseR);
			f.voice[1] = 0.5f * (toneL + toneR);
		}
		f.active[1] = toneOn;

		// ---- CLICK
		if (rise[2]) {
			clickLeft = (int) clickWidthSamples(p.clickWidth);
			clickCount++;
			// alternate sides; the DATA byte's lowest bit can flip the order
			bool right = ((clickCount + (int) (data * 25.5f)) & 1);
			clickPan = p.clickSpread * (right ? 1.f : -1.f);
		}
		float imp = 0.f;
		if (clickLeft > 0) { imp = 1.f; clickLeft--; }
		float a = 1.f - p.clickColour * 0.97f;   // one-pole lowpass amount
		float cl = imp * (1.f - std::max(0.f, clickPan));
		float cr = imp * (1.f + std::min(0.f, clickPan));
		clickLpL += a * (cl - clickLpL);
		clickLpR += a * (cr - clickLpR);
		float clickGain = 1.f + p.clickColour * 3.f;   // keep dull clicks audible
		f.voice[2] = 0.5f * (clickLpL + clickLpR) * clickGain;
		f.active[2] = imp > 0.f;

		// ---- NOISE
		if (rise[3]) noiseLeft = noiseSeconds(p.noiseLen);
		float nl = 0.f, nr = 0.f;
		if (noiseLeft > 0.f) {
			nl = noise();
			float ind = noise();
			nr = nl + p.noiseSpread * (ind - nl);
			noiseLeft -= dt;
		}
		// one-pole highpass: COLOUR 0 = white, 1 = thin hiss (~10 kHz)
		float fc = 20.f * std::pow(500.f, p.noiseColour);
		float g = std::exp(-TWO_PI * fc * dt);
		hpL = g * (hpL + nl - hpInL); hpInL = nl;
		hpR = g * (hpR + nr - hpInR); hpInR = nr;
		float hgain = 1.f + p.noiseColour * 1.5f;
		f.voice[3] = 0.5f * (hpL + hpR) * hgain;
		f.active[3] = noiseLeft > 0.f;

		if (cut) {
			for (int i = 0; i < 4; i++) { f.voice[i] = 0.f; f.active[i] = false; }
			return f;
		}
		float s = f.voice[0] * p.subLevel;
		f.l = s + toneL * p.toneLevel + clickLpL * clickGain * p.clickLevel + hpL * hgain * p.noiseLevel;
		f.r = s + toneR * p.toneLevel + clickLpR * clickGain * p.clickLevel + hpR * hgain * p.noiseLevel;
		f.l *= 5.f; f.r *= 5.f;
		for (int i = 0; i < 4; i++) f.voice[i] *= 5.f;
		return f;
	}
};

} // namespace testpattern
