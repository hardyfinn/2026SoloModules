// Sound engines for the 808 Circuits voices (all but the kick, see KickEngine.hpp).
// Each is a small circuit model of one TR-808 voice built from Common808.hpp parts.
// Stock numbers (frequencies, ring and envelope times) are the commonly published
// figures for the 808 voices and are fitted, not read from component values; every
// knob's stock position is a red dot on the panel. Rack-free (see test/render.cpp).
#pragma once
#include "Common808.hpp"
#include "VoiceSpecs.hpp"

namespace c808 {

static inline float accGain(float A) { return 1.4f * std::sqrt(A / 14.f); }   // VCA voices: accent

struct EngineBase {
	float fs = 48000.f, out = 0.f, env = 0.f;
	Age age;
	Rng rng;
	float hitPitch = 1.f, hitDecay = 1.f, hitLevel = 1.f;
	void setSampleRate(float s) { fs = s; }
	void drifts(float ageK) {
		hitPitch = age.drift(ageK, 0.004f);
		hitDecay = age.drift(ageK, 0.06f);
		hitLevel = age.drift(ageK, 0.06f);
	}
	float finish(float y, float ageK, float drive, float level) {
		y += age.hiss(ageK);
		return opamp(y, drive) * level * hitLevel * age.part(6, ageK, 0.1f);
	}
};

#define C808_VAL(NS) float V(const float* k, int i) const { return knobValue(NS::spec().knobs[i], k[i]); }

// ---------------------------------------------------------------- snare
// Two bridged-T heads (about 180 Hz and 330 Hz) from the trigger pulse, plus white noise
// through a high-pass (COLOUR) and low-pass (TONE) into its own VCA (SNAPPY).
struct SnareEngine : EngineBase {
	static const VoiceSpec& spec() { return snare::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(snare)
	PulseShaper sh;
	Resonator r1, r2;
	OnePole nhp, nlp;
	float t = 1e9f, nEnv = 0.f, nAmp = 0.f;
	static constexpr float G = 1.7f, GN = 1.45f;
	void reset() { r1.reset(); r2.reset(); nEnv = 0.f; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace snare;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		sh.fire(A, V(k, STRIKE) * 1e-3f);
		drifts(k[AGE]);
		nEnv = 1.f; nAmp = accGain(A);
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace snare;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		float m = std::exp2(V(k, SWEEP) * std::exp(-t / 0.003f));
		t += dt;
		float f1 = V(k, TUNE) * std::exp2(voct) * age.part(0, ag, 0.12f) * hitPitch;
		float f2 = f1 * V(k, RATIO) * age.part(1, ag, 0.05f);
		float T1 = V(k, DECAY) * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float b1 = r1.process(x, f1 * m, T1, fs, 2.f * PI * 180.f);
		float b2 = r2.process(x, f2 * m, T1 * 0.6f, fs, 2.f * PI * 330.f);
		float bal = k[BALANCE];
		float body = G * (std::min(1.f, 2.f * (1.f - bal)) * b1 + 0.8f * std::min(1.f, 2.f * bal) * b2) + 0.1f * x;
		float w = rng.bi();
		float n = nhp.hp(w, V(k, NOISECOL) * age.part(4, ag, 0.2f), fs);
		n = nlp.lp(n, V(k, TONE), fs);
		nEnv *= decayMul(V(k, SNAPDECAY) * 1e-3f * age.part(2, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn), dt);
		float noise = GN * n * nEnv * nAmp * k[SNAPPY] * 2.f;
		out = finish(body + noise, ag, k[DRIVE], k[LEVEL]);
		env = clampf(G * std::max(r1.amp(), r2.amp()) * 1.4f + 6.f * nEnv * nAmp * k[SNAPPY], 0.f, 10.f);
	}
};

// ---------------------------------------------------------------- clap
// White noise through a band-pass, into a VCA driven by a sawtooth that fires several
// quick bursts, plus a separate longer "reverb" envelope on the same noise.
struct ClapEngine : EngineBase {
	static const VoiceSpec& spec() { return clap::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(clap)
	Svf bp;
	OnePole tone;
	float t = 1e9f, envC = 0.f, envT = 0.f, envTr = 0.f, amp = 0.f, gap = 0.01f;
	int burstsLeft = 0, nextBurst = 0;
	float nextAt = 0.f;
	static constexpr float G = 10.5f;
	void reset() { envC = envT = envTr = 0.f; burstsLeft = 0; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace clap;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		amp = accGain(A);
		drifts(k[AGE]);
		burstsLeft = (int)V(k, BURSTS);
		gap = V(k, SPREAD) * 1e-3f * age.part(1, k[AGE], 0.15f);
		nextAt = 0.f;
		envTr = 1.f;
		t = 0.f;
	}
	void process(const float* k, float) {
		using namespace clap;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		if (burstsLeft > 0 && t >= nextAt) {
			envC = 1.f;
			burstsLeft--;
			// each gap a touch longer than the last, plus a little hand slop with AGE
			nextAt += gap * (1.f + 0.04f * (float)(burstsLeft)) * (1.f + 0.08f * ag * rng.bi());
		}
		t += dt;
		envC *= std::exp(-dt / (V(k, SNAP) * 1e-3f));
		// tail: rises over ~3 ms, then decays
		envT += (envTr - envT) * (1.f - std::exp(-dt / 0.003f));
		envTr *= decayMul(V(k, TAIL) * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn), dt);
		float w = rng.bi();
		float q = V(k, RES);
		bp.process(w, V(k, BAND) * age.part(0, ag, 0.15f) * hitPitch, q, fs);
		float n = bp.bp / q * 2.f;   // unity peak
		float mix = k[MIX];
		float wc = std::min(1.f, 2.f * (1.f - mix)), wt = 0.6f * std::min(1.f, 2.f * mix);
		float e = wc * envC + wt * envT;
		float y = tone.lp(G * n * e * amp, V(k, TONE) * age.part(4, ag, 0.2f), fs);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
		env = clampf(7.f * e * amp, 0.f, 10.f);
	}
};

// ---------------------------------------------------------------- tom / conga
// One bridged-T with a downward pitch bend; toms add a pink-noise layer (SLAP).
struct TomEngine : EngineBase {
	static const VoiceSpec& spec() { return tom::spec(); }
	static float stockFor(int i, const float* k) {
		if (i == tom::SLAP && k[tom::MODE] > 0.5f) return 0.f;   // conga: no noise
		return spec().knobs[i].stock;
	}
	C808_VAL(tom)
	PulseShaper sh;
	Resonator r1, r2;
	Pink pink;
	OnePole nlp, tone;
	float t = 1e9f, nEnv = 0.f, nAmp = 0.f;
	static constexpr float G = 3.0f;
	void reset() { r1.reset(); r2.reset(); nEnv = 0.f; out = env = 0.f; }
	static float baseHz(int range, bool conga) {
		static const float tomHz[3] = {90.f, 130.f, 180.f};
		static const float congaHz[3] = {165.f, 250.f, 370.f};
		return conga ? congaHz[range] : tomHz[range];
	}
	static float baseT60(int range, bool conga) {
		static const float tomT[3] = {0.55f, 0.45f, 0.38f};
		static const float congaT[3] = {0.26f, 0.21f, 0.17f};
		return conga ? congaT[range] : tomT[range];
	}
	void hit(float acc, const float* k) {
		using namespace tom;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		sh.fire(A, V(k, STRIKE) * 1e-3f);
		drifts(k[AGE]);
		nEnv = 1.f; nAmp = accGain(A);
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace tom;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		int range = (int)clampf(std::round(k[RANGE]), 0.f, 2.f);
		bool conga = k[MODE] > 0.5f;
		float x = sh.process(dt);
		float m = std::exp2(V(k, BEND) * std::exp(-t / 0.03f));
		float muteK = (t > 0.006f) ? (1.f - 0.92f * k[MUTE]) : 1.f;
		t += dt;
		float base = baseHz(range, conga);
		float f = base * std::exp2(V(k, TUNE) + voct) * age.part(0, ag, 0.12f) * hitPitch;
		float T = baseT60(range, conga) * V(k, DECAY) * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn) * muteK;
		float wRef = 2.f * PI * base;
		float b1 = r1.process(x, f * m, T, fs, wRef);
		float dbl = k[DOUBLE];
		float b2 = r2.process(x, f * m * (1.f + 0.035f * dbl), T * 0.85f, fs, wRef);
		float body = G * (conga ? 0.75f : 1.f) * (b1 + 0.7f * dbl * b2);
		nEnv *= decayMul(0.16f * age.part(2, ag, 0.25f) * muteK, dt);
		float n = nlp.lp(pink.process(rng.bi()), 2500.f, fs) * nEnv * nAmp * k[SLAP] * 6.f;
		float y = tone.lp(body + n, V(k, TONE) * age.part(4, ag, 0.2f), fs);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
		env = clampf(G * r1.amp() * 1.4f, 0.f, 10.f);
	}
};

// ---------------------------------------------------------------- rim shot
// Two high bridged-T resonators (about 455 Hz and 1.67 kHz) gated by a fast transistor
// VCA (which also bends the waveform), then high-passed into the crack.
struct RimEngine : EngineBase {
	static const VoiceSpec& spec() { return rim::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(rim)
	PulseShaper sh;
	Resonator r1, r2;
	Svf hp;
	float t = 1e9f, gate = 0.f, gateLen = 0.008f;
	static constexpr float G = 2.6f;
	void reset() { r1.reset(); r2.reset(); gate = 0.f; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace rim;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		sh.fire(A, V(k, STRIKE) * 1e-3f);
		drifts(k[AGE]);
		gateLen = V(k, GATE) * 1e-3f * age.part(2, k[AGE], 0.2f);
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace rim;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		gate = (t < gateLen) ? 1.f : gate * std::exp(-dt / 0.0015f);
		t += dt;
		float f1 = 455.f * std::exp2(V(k, TUNE) + voct) * age.part(0, ag, 0.12f) * hitPitch;
		float f2 = f1 * V(k, RATIO) * age.part(1, ag, 0.05f);
		float T = V(k, RING) * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float s = r1.process(x, f1, T, fs, 2.f * PI * 455.f) + 0.8f * r2.process(x, f2, T * 0.7f, fs, 2.f * PI * 1667.f);
		// the transistor VCA: asymmetric, harder with DRIVE
		float v = s * gate * G;
		float d = 1.f + 3.f * k[DRIVE];
		v = v > 0.f ? std::tanh(v * d * 0.25f) * 4.f / std::sqrt(d) : std::tanh(v * d * 0.18f) * 5.5f / std::sqrt(d);
		hp.process(v, V(k, SNAP) * age.part(4, ag, 0.2f), 0.7f, fs);
		out = finish(hp.hp * 1.2f, ag, 0.f, k[LEVEL]);
		env = clampf(gate * 10.f * std::max(r1.amp(), r2.amp()), 0.f, 10.f);
	}
};

// ---------------------------------------------------------------- claves
// One high bridged-T (about 2.5 kHz) with a very short ring; OVERTONE adds a second one.
struct ClaveEngine : EngineBase {
	static const VoiceSpec& spec() { return clave::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(clave)
	PulseShaper sh;
	Resonator r1, r2;
	float t = 1e9f;
	static constexpr float G = 1.0f;
	void reset() { r1.reset(); r2.reset(); out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace clave;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		sh.fire(A, V(k, STRIKE) * 1e-3f);
		drifts(k[AGE]);
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace clave;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		float T = V(k, RING) * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float m = std::exp2(V(k, BEND) * std::exp(-t / std::max(0.004f, 0.35f * T)));
		t += dt;
		float f = 2500.f * std::exp2(V(k, TUNE) + voct) * age.part(0, ag, 0.12f) * hitPitch;
		float wRef = 2.f * PI * 2500.f;
		float b1 = r1.process(x, f * m, T, fs, wRef);
		float b2 = r2.process(x, f * m * V(k, RATIO), T * 0.6f, fs, wRef);
		float y = G * (b1 + 0.8f * k[OVERTONE] * b2);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
		env = clampf(G * r1.amp() * 1.4f, 0.f, 10.f);
	}
};

// ---------------------------------------------------------------- maracas
// White noise, high-passed, through a short attack-decay VCA.
struct MaracasEngine : EngineBase {
	static const VoiceSpec& spec() { return maracas::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(maracas)
	Svf hp;
	float t = 1e9f, e = 0.f, peak = 0.f, amp = 0.f;
	float shakeAt = -1.f, shakeAmp = 0.f;
	static constexpr float G = 6.8f;
	void reset() { e = 0.f; shakeAt = -1.f; out = env = 0.f; }
	void start(float a) { peak = a; t = 0.f; }
	void hit(float acc, const float* k) {
		using namespace maracas;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		amp = accGain(A);
		drifts(k[AGE]);
		start(1.f);
		float s = k[SHAKE];
		if (s > 0.01f) { shakeAt = (0.06f + 0.14f * s) * (1.f + 0.1f * k[AGE] * rng.bi()); shakeAmp = 0.35f + 0.4f * s; }
		else shakeAt = -1.f;
	}
	void process(const float* k, float) {
		using namespace maracas;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		if (shakeAt >= 0.f && t >= shakeAt) { float a2 = shakeAmp; shakeAt = -1.f; start(a2); }
		float atk = V(k, ATTACK) * 1e-3f;
		float T = V(k, LENGTH) * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		if (t < atk) e += (peak - e) * std::min(1.f, dt / std::max(atk - t, dt));
		else e *= decayMul(T, dt);
		t += dt;
		// noise source: smooth white <-> sparse beads
		float g = k[GRAINS];
		float src = rng.bi() * (1.f - g);
		float p = 0.0002f + 0.004f * (1.f - g);
		if (g > 0.f && rng.uni() < p) src += g * (0.5f + 0.5f * rng.uni()) * (rng.bi() > 0.f ? 6.f : -6.f);
		hp.process(src, V(k, CUT) * age.part(4, ag, 0.2f), 0.7f, fs);
		float y = G * hp.hp * e * amp;
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
		env = clampf(8.f * e * amp, 0.f, 10.f);
	}
};

// ---------------------------------------------------------------- cowbell
// Two square oscillators (540 Hz and 800 Hz on the 808) summed into a band-pass, then a
// VCA whose envelope drops fast to a break point and rings on from there.
struct CowbellEngine : EngineBase {
	static const VoiceSpec& spec() { return cowbell::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(cowbell)
	float ph1 = 0.f, ph2 = 0.f;
	Svf bp;
	float t = 1e9f, e1 = 0.f, e2 = 0.f, amp = 0.f;
	static constexpr float G = 6.2f;
	void reset() { e1 = e2 = 0.f; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace cowbell;
		float A = triggerVolts(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		amp = accGain(A);
		drifts(k[AGE]);
		float L = 0.08f + 0.5f * k[SHAPE];
		e1 = 1.f - L; e2 = L;
		t = 0.f;
	}
	static float blep(float t, float dt) {   // polyBLEP
		if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
		if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
		return 0.f;
	}
	float pulse(float& ph, float hz, float w) {
		float d = clampf(hz / fs, 0.f, 0.45f);
		ph += d; if (ph >= 1.f) ph -= 1.f;
		float y = (ph < w) ? 1.f : -1.f;
		y += blep(ph, d);
		float p2 = ph - w; if (p2 < 0.f) p2 += 1.f;
		y -= blep(p2, d);
		return y;
	}
	void process(const float* k, float voct) {
		using namespace cowbell;
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float f1 = 540.f * std::exp2(V(k, TUNE) + voct) * age.part(0, ag, 0.06f) * hitPitch;
		float f2 = f1 * V(k, INTERVAL) * age.part(1, ag, 0.04f);
		float w = V(k, WIDTH) * 0.01f;
		float s = 0.5f * (pulse(ph1, f1, w) + pulse(ph2, f2, w));
		float q = V(k, RES);
		bp.process(s, V(k, FILTER) * age.part(4, ag, 0.15f), q, fs);
		float n = bp.bp / q;
		e1 *= std::exp(-dt / 0.012f);
		e2 *= decayMul(V(k, DECAY) * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn), dt);
		t += dt;
		float e = e1 + e2;
		out = finish(G * n * e * amp * 2.f, ag, k[DRIVE], k[LEVEL]);
		env = clampf(8.f * e * amp, 0.f, 10.f);
	}
};

#undef C808_VAL

}  // namespace c808
