// Chirp engine: one pitch-swept voice, no Rack dependency (shared by the module
// and by test/render.cpp, which renders the demo WAV offline).
//
// The idea: almost every animal call that "chirps" is the same gesture. A tone
// starts at one pitch and slides fast to another while it fades. Bats sweep
// down over an octave in a few milliseconds, frogs and birds sweep up, crickets
// and cicadas repeat a short sweep many times a second. An 808 kick is the same
// thing again: a sine that drops from high to low while it decays.
//
// Per trigger the voice plays a TRAIN of REPEAT chirps spaced 1/RATE apart.
// Each chirp: pitch glides from (PITCH + SWEEP octaves) down/up to PITCH over
// TIME, bent by CURVE; loudness jumps up in 0.3 ms then decays over DECAY.
#pragma once
#include <cmath>
#include <cstdint>

namespace chirp {

static const float PI2 = 6.28318530718f;

// Knob (0..1) to value maps, shared with the module's tooltips.
inline float mapPitch(float k) { return (k - 0.5f) * 10.f; }               // V/OCT, 0 V = C4
inline float mapSweep(float k) { return (k - 0.5f) * 12.f; }               // octaves, +/-6
inline float mapTime(float k) { return 0.0005f * std::pow(4000.f, k); }    // 0.5 ms .. 2 s
inline float mapCurve(float k) { return k * 2.f - 1.f; }                   // -1 .. +1
inline float mapDecay(float k) { return 0.002f * std::pow(2000.f, k); }    // 2 ms .. 4 s
inline int mapRepeat(float k) { int n = 1 + (int)std::floor(k * 15.f + 0.5f); return n < 1 ? 1 : (n > 16 ? 16 : n); }
inline float mapRate(float k) { return 0.5f * std::pow(400.f, k); }        // 0.5 .. 200 Hz

// Inverses (value to knob), used by the preset table.
inline float knobPitch(float v) { return v / 10.f + 0.5f; }
inline float knobSweep(float o) { return o / 12.f + 0.5f; }
inline float knobTime(float s) { return std::log(s / 0.0005f) / std::log(4000.f); }
inline float knobCurve(float c) { return (c + 1.f) * 0.5f; }
inline float knobDecay(float s) { return std::log(s / 0.002f) / std::log(2000.f); }
inline float knobRepeat(int n) { return (n - 1) / 15.f; }
inline float knobRate(float hz) { return std::log(hz / 0.5f) / std::log(400.f); }

struct Settings {
	float pitch = 0.f;    // V/OCT of the landing pitch (0 V = C4 = 261.63 Hz)
	float sweep = 2.f;    // octaves above (+) or below (-) the landing pitch where the chirp starts
	float time = 0.05f;   // seconds to glide
	float curve = -0.5f;  // -1 = drops fast then settles (kick), 0 = even, +1 = holds then dives
	float decay = 0.3f;   // seconds to fade to -60 dB
	float tone = 0.f;     // 0 = pure sine, 0.5 = saw-like, 1 = buzzy and squared
	int repeat = 1;       // chirps per trigger
	float rate = 10.f;    // chirps per second inside a train
	bool loop = false;    // keep chirping at RATE without triggers
	float level = 1.f;    // accent
};

struct Voice {
	// train state
	bool active = false;
	int left = 0;          // chirps still to start
	float sinceStart = 0.f; // seconds since the last chirp started
	float t = 1e9f;        // seconds into the current chirp
	// sound state
	float phase = 0.f, fb = 0.f, env = 0.f, attackFrom = 0.f, attackPos = 1.f;
	float dc = 0.f;
	float lastPitch = 0.f;
	bool wasLoop = false;

	void trigger(const Settings& s) {
		active = true;
		left = s.loop ? (1 << 30) : s.repeat;
		startChirp(true);
	}
	void startChirp(bool first) {
		left--;
		sinceStart = 0.f;
		t = 0.f;
		attackFrom = env;
		attackPos = 0.f;
		if (first && env < 0.001f) { phase = 0.f; fb = 0.f; }
	}

	// One sample. Writes the audio (+/-1), the envelope (0..1) and the pitch contour (V/OCT).
	void process(const Settings& s, float sr, float& out, float& envOut, float& pitchOut) {
		const float dt = 1.f / sr;
		if (s.loop && !active) { active = true; left = 1 << 30; startChirp(true); }
		if (wasLoop && !s.loop) left = 0;  // loop switched off: let the current chirp finish
		wasLoop = s.loop;

		// next chirp in the train
		if (active) {
			sinceStart += dt;
			if (sinceStart >= 1.f / s.rate) {
				if (left > 0 || s.loop) startChirp(false);
				else active = false;
			}
		}

		// pitch contour
		float p = s.time > 0.f ? t / s.time : 1.f;
		if (p > 1.f) p = 1.f;
		float k = std::pow(2.f, s.curve * 3.f);  // 1/8 .. 8
		float shaped = std::pow(p, k);
		float v = s.pitch + s.sweep * (1.f - shaped);
		lastPitch = v;
		float f = 261.6256f * std::pow(2.f, v);
		float fmax = 0.45f * sr; if (fmax > 22000.f) fmax = 22000.f;
		if (f < 1.f) f = 1.f;
		if (f > fmax) f = fmax;

		// loudness: 0.3 ms ramp from wherever it was, then exponential decay
		const float att = 0.0003f;
		if (attackPos < 1.f) {
			attackPos += dt / att;
			if (attackPos > 1.f) attackPos = 1.f;
			env = attackFrom + (1.f - attackFrom) * attackPos;
		} else {
			env *= std::exp(-6.9f * dt / s.decay);
		}
		t += dt;

		// oscillator: feedback phase modulation turns the sine toward a saw,
		// then soft drive squares it off for buzz. Both ease off at high pitch to
		// keep aliasing down.
		float tone = s.tone;
		float hf = 1.f - f / 9000.f; if (hf < 0.f) hf = 0.f;
		float beta = (tone < 0.5f ? tone * 2.f : 1.f) * 1.3f * hf;
		float drive = 1.f + (tone > 0.5f ? (tone - 0.5f) * 2.f * 9.f * hf : 0.f);
		phase += f * dt;
		phase -= std::floor(phase);
		float y = std::sin(PI2 * phase + beta * fb);
		fb = 0.5f * (fb + y);
		if (drive > 1.001f) y = std::tanh(drive * y) / std::tanh(drive);
		// remove the DC a driven, asymmetric wave can carry
		dc += (y - dc) * (20.f * PI2 * dt);
		float dcAmt = tone * 4.f; if (dcAmt > 1.f) dcAmt = 1.f;
		y -= dc * dcAmt;

		out = y * env * s.level;
		envOut = env;
		pitchOut = v;
		if (env < 1e-6f && !active) env = 0.f;
	}
};

} // namespace chirp
