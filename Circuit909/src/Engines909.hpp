// Sound engines for the 909 Circuits voices. Each is a small circuit model of one TR-909
// voice built from Common909.hpp parts, after the block diagrams in the TR-909 service
// notes. Stock numbers (pitches, sweep and decay times) are fitted to the 909's published
// behaviour, not read from component values; every knob's stock position is an orange dot
// on the panel. Rack-free (see test/voices.cpp).
//
// Built in from the start (lessons of the 808 Circuits DSP review):
//  - accent: one law for every voice, about +5 dB at stock ACCENT (accentGain);
//  - noise has the same in-band level at any sample rate (Rng::noise), and each voice has
//    its own noise sequence;
//  - knob curves are worked out every 16 samples (and on each hit), not every sample;
//  - DRIVE and LEVEL are smoothed (5 ms) and a 10 Hz DC blocker follows the output stage;
//  - VCOs are band-limited; the rim's STRIKE and tuning hold the ring level;
//  - a voice that has been silent for 0.1 s reports sleeping() so the module can skip it.
#pragma once
#include "Common909.hpp"
#include "VoiceSpecs.hpp"
#include <vector>

namespace c909 {

struct EngineBase {
	float fs = 48000.f, out = 0.f, env = 0.f;
	Age age;
	Rng rng;
	float hitPitch = 1.f, hitDecay = 1.f, hitLevel = 1.f;
	float c[16] = {};   // control-rate knob values (as the tooltip shows them)
	int ctr = 0;
	float dcZ = 0.f, levelS = -1.f, driveS = 0.f;
	int quiet = 0;
	bool busy = false;   // something still scheduled (clap bursts)
	// module hooks; only the cymbals use them
	static constexpr bool EXTRA_TRIG = false;
	static constexpr bool USES_SAMPLE = false;
	void hit2(float, const float*) {}
	void setSample(const std::vector<float>&, float) {}
	static std::vector<float> prepareSample(const std::vector<float>&, float) { return std::vector<float>(); }
	void swapSample(std::vector<float>&) {}
	bool hasSample() const { return false; }

	float smK = 0.f, dcK = 0.f, holdK = 0.f, hold = 0.f, lastPre = 0.f;
	explicit EngineBase(uint32_t noiseSeed = 909u) { rng.seed(noiseSeed); setSampleRate(48000.f); }
	void setSampleRate(float s) {
		if (s == fs && smK > 0.f) return;
		fs = s;
		smK = 1.f - std::exp(-1.f / (0.005f * fs));
		dcK = 1.f - std::exp(-2.f * PI * 10.f / fs);
		holdK = std::exp(-1.f / (0.0003f * fs));
	}
	// on a retrigger the old tail fades over about 1 ms instead of jumping (VCO voices)
	void catchTail() { hold = lastPre; }
	void drifts(float ageK) {
		hitPitch = age.drift(ageK, 0.004f);
		hitDecay = age.drift(ageK, 0.06f);
		hitLevel = age.drift(ageK, 0.06f);
	}
	bool sleeping() const { return quiet > (int)(0.1f * fs); }
	float finish(float y, float ageK, float drive, float level) {
		float w = Age::worn(ageK);
		if (hold != 0.f) { y += hold; hold *= holdK; if (std::fabs(hold) < 1e-6f) hold = 0.f; }
		lastPre = y;
		y += age.hiss(ageK, fs);
		float sm = smK;
		if (levelS < 0.f) { levelS = level; driveS = drive; }
		driveS += (drive - driveS) * sm;
		levelS += (level - levelS) * sm;
		float o = opamp(y, driveS);
		dcZ += (o - dcZ) * dcK;
		o -= dcZ;
		if (std::fabs(dcZ) < 1e-20f) dcZ = 0.f;
		o *= levelS * hitLevel * age.part(6, ageK, 0.1f);
		quiet = (std::fabs(y) < 1e-6f && env < 1e-5f && w == 0.f && !busy) ? quiet + 1 : 0;
		return o;
	}
	void woke() { quiet = 0; }
	// a decay coefficient per sample for a time constant tau (seconds)
	float coef(float tau) const { return tau > 0.f ? std::exp(-1.f / (tau * fs)) : 0.f; }
	float t60coef(float t60) const { return t60 > 0.f ? std::exp(-6.9078f / (t60 * fs)) : 0.f; }
};

#define C909_VAL(NS) \
	float V(const float* k, int i) const { return knobValue(NS::spec().knobs[i], k[i]); } \
	void refresh(const float* k) { const VoiceSpec& s = NS::spec(); for (int i = 0; i < s.nKnobs && i < 16; i++) c[i] = knobValue(s.knobs[i], k[i]); derive(k); } \
	void tick(const float* k) { if ((ctr++ & 15) == 0) refresh(k); }

static inline float floorZero(float x) { return std::fabs(x) < 1e-7f ? 0.f : x; }

// ---------------------------------------------------------------- kick
// The 909 bass drum: a ramp VCO, reset on every hit, through a waveshaper towards a sine.
// Its pitch starts high and falls to the base note (TUNE sets how long that takes; a
// slower second fall, BEND, finishes it), into a VCA with an exponential DECAY. ATTACK
// adds the click: a short square pulse, high-passed, and a burst of low-passed noise.
struct KickEngine : EngineBase {
	static const VoiceSpec& spec() { return kick::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C909_VAL(kick)
	KickEngine() : EngineBase(0x909B0u) {}
	Vco vco;
	OnePole pHp, nLp, nLp2;
	float t = 1e9f, pe1 = 0.f, pe2 = 0.f, ae = 0.f, ne = 0.f, amp = 1.f;
	float k1 = 0.f, k2 = 0.f, ka = 0.f, kn = 0.f, pw = 0.001f;
	static constexpr float G = 5.0f, GP = 2.2f, GN = 2.6f;
	void reset() { ae = ne = pe1 = pe2 = 0.f; out = env = 0.f; }
	void derive(const float* k) {
		using namespace kick;
		float ag = k[AGE], worn = Age::worn(ag);
		float tS = c[TUNE] * 1e-3f * age.part(0, ag, 0.2f);
		k1 = coef(tS);
		k2 = coef(4.f * tS);
		ka = t60coef(c[DECAY] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn));
		kn = coef(c[SNAP] * 1e-3f / 3.f);
		pw = std::min(1e-3f, c[SNAP] * 1e-3f * 0.2f);
	}
	void hit(float acc, const float* k) {
		using namespace kick;
		drifts(k[AGE]);
		refresh(k); woke();
		amp = accentGain(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		catchTail();
		vco.reset();
		pe1 = pe2 = 1.f; ae = 1.f; ne = 1.f;
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace kick;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE];
		float f = c[BASE] * std::exp2(voct + c[SWEEP] * pe1 + c[BEND] * pe2) * age.part(1, ag, 0.1f) * hitPitch;
		pe1 *= k1; pe2 *= k2;
		float body = 0.f;
		if (ae > 1e-6f) {
			body = vco.process(f / fs, c[SHAPE]) * ae;
			ae *= ka;
		}
		else ae = 0.f;
		// click: square pulse (high-passed) + low-passed noise burst
		float att = 2.f * k[ATTACK];
		float pulse = pHp.hp(t < pw ? 1.f : 0.f, 160.f, fs);
		float nz = 0.f;
		if (ne > 1e-6f) {
			nz = nLp2.lp(nLp.lp(rng.noise(fs), c[NOISELP] * age.part(4, ag, 0.2f), fs), c[NOISELP] * 1.5f, fs) * ne;
			ne *= kn;
		}
		else ne = 0.f;
		t += dt;
		float click = att * (GP * k[PULSE] * 2.f * pulse + GN * k[NOISE] * 2.f * nz);
		float y = amp * (G * body + click);
		env = clampf(10.f * ae * amp / 1.78f, 0.f, 10.f);
		out = finish(floorZero(y), ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- snare
// Two swept VCOs (about 185 Hz and 330 Hz) through one VCA, plus white noise through a
// high-pass (COLOUR) and low-pass (TONE) into its own VCA (SNAPPY).
struct SnareEngine : EngineBase {
	static const VoiceSpec& spec() { return snare::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C909_VAL(snare)
	SnareEngine() : EngineBase(0x909C5u) {}
	Vco v1, v2;
	Svf nhp, nlp, aa;   // two-pole noise filters (2.1.0)
	float t = 1e9f, pe = 0.f, ae = 0.f, ne = 0.f, amp = 1.f;
	float kp = 0.f, ka = 0.f, ka2 = 0.f, kn = 0.f, ae2 = 0.f;
	static constexpr float G = 3.4f, GN = 7.5f;
	void reset() { ae = ae2 = ne = pe = 0.f; out = env = 0.f; }
	void derive(const float* k) {
		using namespace snare;
		float ag = k[AGE], worn = Age::worn(ag);
		kp = coef(c[BEND] * 1e-3f);
		float T = c[DECAY] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		ka = t60coef(T); ka2 = t60coef(0.7f * T);
		kn = t60coef(c[SNAPDECAY] * 1e-3f * age.part(2, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn));
	}
	void hit(float acc, const float* k) {
		using namespace snare;
		drifts(k[AGE]);
		refresh(k); woke();
		amp = accentGain(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		catchTail();
		v1.reset(); v2.reset();
		pe = 1.f; ae = ae2 = 1.f; ne = 1.f;
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace snare;
		tick(k);
		const float ag = k[AGE];
		float body = 0.f;
		if (ae > 1e-6f) {
			float m = std::exp2(voct + c[SWEEP] * pe);
			float f1 = c[TUNE] * m * age.part(0, ag, 0.12f) * hitPitch;
			float f2 = f1 * c[RATIO] * age.part(1, ag, 0.05f);
			float bal = k[BALANCE];
			body = std::min(1.f, 2.f * (1.f - bal)) * v1.process(f1 / fs, c[SHAPE]) * ae
			     + 0.85f * std::min(1.f, 2.f * bal) * v2.process(f2 / fs, c[SHAPE]) * ae2;
			ae *= ka; ae2 *= ka2; pe *= kp;
		}
		else ae = ae2 = 0.f;
		float n = 0.f;
		if (ne > 1e-6f) {
			float w = rng.noise(fs);
			nhp.process(w, c[NOISECOL] * age.part(4, ag, 0.2f), 0.707f, fs);
			nlp.process(nhp.hp, c[TONE], 0.707f, fs);
			aa.process(nlp.lp, 18000.f, 0.707f, fs);
			n = aa.lp * ne;
			ne *= kn;
		}
		else ne = 0.f;
		float y = amp * (G * body + GN * 2.f * k[SNAPPY] * n);
		env = clampf(amp * (6.f * ae + 8.f * k[SNAPPY] * ne), 0.f, 10.f);
		out = finish(floorZero(y), ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- toms
// The 909 has three tom circuits; each is a swept VCO through a waveshaper and a VCA, with
// a little noise for the skin. All three share one level, so low, mid and high sit together.
struct TomEngine : EngineBase {
	static const VoiceSpec& spec() { return tom::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C909_VAL(tom)
	TomEngine() : EngineBase(0x9097Au) {}
	Vco v1, v2;
	Pink pink;
	OnePole nlp, nhp, tone;
	float t = 1e9f, pe = 0.f, ae = 0.f, ne = 0.f, amp = 1.f;
	float kp = 0.f, ka = 0.f, kaM = 0.f, kn = 0.f;
	static constexpr float G = 4.6f, GN = 9.f;
	void reset() { ae = ne = pe = 0.f; out = env = 0.f; }
	static float baseHz(int r) { static const float hz[3] = {100.f, 145.f, 210.f}; return hz[r]; }
	static float baseT60(int r) { static const float T[3] = {0.62f, 0.5f, 0.42f}; return T[r]; }
	int range(const float* k) const { return (int)clampf(std::round(k[tom::RANGE]), 0.f, 2.f); }
	void derive(const float* k) {
		using namespace tom;
		float ag = k[AGE], worn = Age::worn(ag);
		kp = coef(c[BEND] * 1e-3f);
		float T = baseT60(range(k)) * c[DECAY] * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		ka = t60coef(T);
		kaM = t60coef(T * (1.f - 0.93f * k[MUTE]));
		kn = t60coef(0.09f * age.part(2, ag, 0.25f) * (1.f - 0.6f * k[MUTE]));
	}
	void hit(float acc, const float* k) {
		using namespace tom;
		drifts(k[AGE]);
		refresh(k); woke();
		amp = accentGain(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		catchTail();
		v1.reset(); v2.reset();
		pe = 1.f; ae = 1.f; ne = 1.f;
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace tom;
		tick(k);
		const float ag = k[AGE];
		float body = 0.f;
		if (ae > 1e-6f) {
			float f = baseHz(range(k)) * std::exp2(c[TUNE] + voct + c[SWEEP] * pe) * age.part(0, ag, 0.12f) * hitPitch;
			float dbl = k[DOUBLE];
			body = v1.process(f / fs, c[SHAPE]);
			if (dbl > 0.001f) body = (body + 0.8f * dbl * v2.process(f * 1.035f / fs, c[SHAPE])) / (1.f + 0.5f * dbl);
			body *= ae;
			ae *= (t > 0.006f) ? kaM : ka;
			pe *= kp;
		}
		else ae = 0.f;
		float n = 0.f;
		if (ne > 1e-6f) {
			n = nhp.hp(nlp.lp(pink.process(rng.bi()), 3000.f, fs), 120.f, fs) * ne;   // pink holds its in-band level by itself
			ne *= kn;
		}
		else ne = 0.f;
		t += 1.f / fs;
		float y = tone.lp(amp * (G * body + GN * k[NOISE] * n), c[TONE] * age.part(4, ag, 0.2f), fs);
		env = clampf(10.f * ae * amp / 1.78f, 0.f, 10.f);
		out = finish(floorZero(y), ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- rim shot
// Three high bridged-T resonators from the trigger pulse (about 500 Hz, 1.1 kHz and
// 1.8 kHz) gated by a fast, asymmetric transistor VCA, then high-passed into the crack.
struct RimEngine : EngineBase {
	static const VoiceSpec& spec() { return rim::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C909_VAL(rim)
	RimEngine() : EngineBase(0x909E1u) {}
	PulseShaper sh;
	Resonator r1, r2, r3;
	Svf hp;
	float t = 1e9f, gate = 0.f, gateLen = 0.006f, sg1 = 1.f, sg2 = 1.f, sg3 = 1.f, accPost = 1.f, voctNow = 0.f;
	static constexpr float G = 2.4f, F1 = 500.f, R3 = 1.65f;
	void reset() { r1.reset(); r2.reset(); r3.reset(); gate = 0.f; out = env = 0.f; }
	void derive(const float*) {}
	void hit(float acc, const float* k) {
		using namespace rim;
		drifts(k[AGE]);
		refresh(k); woke();
		float a = accentGain(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		float sMs = c[STRIKE];
		sh.fire(4.f, sMs);
		float f1 = F1 * std::exp2(c[TUNE] + voctNow), f2 = f1 * c[RATIO], f3 = f2 * R3;   // TUNE and V/OCT both
		sg1 = strikeComp(4.f, sMs, f1, F1, fs);
		sg2 = strikeComp(4.f, sMs, f2, F1 * 2.2f, fs);
		sg3 = strikeComp(4.f, sMs, f3, F1 * 2.2f * R3, fs);
		gateLen = c[GATE] * 1e-3f * age.part(2, k[AGE], 0.2f);
		accPost = a;   // accent after the VCA so its curve does not eat it
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace rim;
		voctNow = std::isfinite(voct) ? clampf(voct, -10.f, 10.f) : 0.f;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		gate = (t < gateLen) ? 1.f : gate * std::exp(-dt / 0.0015f);
		if (gate < 1e-9f) gate = 0.f;
		t += dt;
		float f1 = F1 * std::exp2(c[TUNE] + voct) * age.part(0, ag, 0.12f) * hitPitch;
		float f2 = f1 * c[RATIO] * age.part(1, ag, 0.05f), f3 = f2 * R3;
		float T = c[RING] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float s = r1.process(x * sg1, f1, T, fs, 2.f * PI * F1)
		        + 0.8f * r2.process(x * sg2, f2, T * 0.75f, fs, 2.f * PI * F1 * 2.2f)
		        + 0.55f * r3.process(x * sg3, f3, T * 0.55f, fs, 2.f * PI * F1 * 2.2f * R3);
		float v = s * gate * G;
		float d = 1.f + 3.f * k[DRIVE];
		v = v > 0.f ? std::tanh(v * d * 0.25f) * 4.f / std::sqrt(d) : std::tanh(v * d * 0.18f) * 5.5f / std::sqrt(d);
		hp.process(v, c[SNAP] * std::exp2(c[TUNE] + voct) * age.part(4, ag, 0.2f), 0.7f, fs);   // the crack filter follows TUNE
		env = clampf(gate * 10.f * std::max(r1.amp(), r2.amp()), 0.f, 10.f);
		out = finish(floorZero(hp.hp * 1.3f * accPost), ag, 0.f, k[LEVEL]);
	}
};

// ---------------------------------------------------------------- clap
// The 909 clap works like the 808's: white noise through a band-pass, into a VCA driven
// by a sawtooth that fires several quick bursts (the last rings longer), plus a longer
// "reverb" envelope on the same noise. BAND and RES keep the loudness.
struct ClapEngine : EngineBase {
	static const VoiceSpec& spec() { return clap::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C909_VAL(clap)
	ClapEngine() : EngineBase(0x909C1u) {}
	Svf bp;
	OnePole tone;
	float t = 1e9f, envC = 0.f, envT = 0.f, envTr = 0.f, amp = 0.f, gap = 0.01f, kT = 0.f;
	float burstT = 1e9f;
	bool lastBurst = false;
	int burstsLeft = 0;
	float nextAt = 0.f;
	static constexpr float G = 6.5f;
	static constexpr float BAND_STOCK = 1200.f, RES_STOCK = 1.3f;
	void reset() { envC = envT = envTr = 0.f; burstsLeft = 0; out = env = 0.f; }
	void derive(const float* k) {
		using namespace clap;
		float ag = k[AGE], worn = Age::worn(ag);
		kT = t60coef(c[TAIL] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn));
	}
	void hit(float acc, const float* k) {
		using namespace clap;
		drifts(k[AGE]);
		refresh(k); woke();
		amp = accentGain(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		burstsLeft = (int)c[BURSTS];
		gap = c[SPREAD] * 1e-3f * age.part(1, k[AGE], 0.15f);
		nextAt = 0.f;
		envTr = 1.f;
		t = 0.f;
	}
	void process(const float* k, float) {
		using namespace clap;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE];
		if (burstsLeft > 0 && t >= nextAt) {
			envC = 1.f; burstT = 0.f;
			burstsLeft--;
			lastBurst = burstsLeft == 0;
			nextAt += gap * (1.f + 0.08f * ag * rng.bi());
		}
		busy = burstsLeft > 0;
		t += dt;
		float snap = c[SNAP] * 1e-3f;
		if (lastBurst) envC *= std::exp(-dt / (4.5f * snap));
		else envC = std::max(0.f, 1.f - burstT / (2.5f * snap));
		burstT += dt;
		envT += (envTr - envT) * (1.f - std::exp(-dt / 0.003f));
		envTr *= kT;
		if (envT < 1e-7f && envTr < 1e-7f) envT = envTr = 0.f;
		float mix = k[MIX];
		// equal-power MIX (2.1.0): the same loudness across the knob
		float wc = std::cos(0.5f * PI * mix), wt = 0.85f * std::sin(0.5f * PI * mix);
		float e = wc * envC + wt * envT;
		float y = 0.f;
		if (e > 1e-7f || std::fabs(bp.bp) > 1e-7f) {
			float q = c[RES], band = c[BAND] * age.part(0, ag, 0.15f) * hitPitch;
			bp.process(rng.noise(fs), band, q, fs);
			float n = bp.bp / q * 2.f * std::sqrt((q / RES_STOCK) * (BAND_STOCK / clampf(band, 50.f, 20000.f)));
			y = tone.lp(G * n * e * amp, c[TONE] * age.part(4, ag, 0.2f), fs);
		}
		env = clampf(7.f * e * amp, 0.f, 10.f);
		out = finish(floorZero(y), ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- cymbals
// On the real 909 the hats, crash and ride are 6-bit recordings in ROM, clocked out
// through a DAC, a reconstruction filter and an analogue VCA with its own decay. Those
// recordings are Roland's and are not here: this models the path around them. The
// source is either a built-in synthesised metal sound (six square oscillators plus noise,
// restarted on every hit so each hit is the same "recording") or a WAV you load
// (converted to the model's 32 kHz clock and normalised when it is loaded). Each clock
// tick takes one source sample at full level, quantises it to BITS and holds it until the
// next tick (band-limited steps); then RECON, and the VCA, which does all of the fading
// (as on the 909, so the tail fades smoothly instead of crumbling into the 6-bit steps).
// DECAY is a multiple of this type's stock length (open hat 0.7 s, crash 2.2 s, ride
// 1.6 s); CH DEC is the closed hat's length. TRIG = closed hat (chokes the open hat),
// OPEN = open hat; for crash and ride both inputs play the cymbal.
struct CymbalsEngine : EngineBase {
	static const VoiceSpec& spec() { return cymbals::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C909_VAL(cymbals)
	static constexpr bool EXTRA_TRIG = true;
	static constexpr bool USES_SAMPLE = true;
	static constexpr float CLOCK = 32000.f;   // the model's "ROM" clock
	CymbalsEngine() : EngineBase(0x909CCu) {}
	SteppedHold dac;
	Svf recon;
	OnePole hp1, hp2, srcHp1, srcHp2, srcLp;
	float oph[6] = {0, 0, 0, 0, 0, 0};
	float t = 1e9f, ve = 0.f, amp = 1.f, kv = 0.f;
	bool open = false;
	int type = 0;
	size_t sPos = 0;
	std::vector<float> user;   // already at CLOCK and normalised (see prepareSample)
	static constexpr float G = 6.0f;
	void reset() { ve = 0.f; dac.reset(); out = env = 0.f; }
	// the module converts a WAV with prepareSample() off the audio thread, then swaps it in
	void swapSample(std::vector<float>& s) { user.swap(s); sPos = user.size(); }
	void setSample(const std::vector<float>& s, float sr) { std::vector<float> v = prepareSample(s, sr); swapSample(v); }
	bool hasSample() const { return !user.empty(); }
	// resample to CLOCK (windowed sinc, cut at 15 kHz) and normalise to 0.95 peak
	static std::vector<float> prepareSample(const std::vector<float>& in, float sr) {
		std::vector<float> out;
		if (in.empty() || !(sr > 1000.f)) return out;
		double ratio = sr / CLOCK;                 // input samples per output sample
		size_t n = (size_t)(in.size() / ratio);
		out.resize(n);
		float fc = std::min(15000.f, 0.47f * sr) / sr;   // cut-off, cycles per input sample
		const int HALF = 16;
		float scale = (float)std::max(1.0, ratio);
		int half = (int)(HALF * scale);
		for (size_t i = 0; i < n; i++) {
			double x = i * ratio;
			long c = (long)x;
			double acc = 0.0;
			for (long j = c - half + 1; j <= c + half; j++) {
				if (j < 0 || j >= (long)in.size()) continue;
				double d = x - j;
				double a = 2.0 * PI * fc * d;
				double sinc = std::fabs(d) < 1e-9 ? 2.0 * fc : std::sin(a) / (PI * d);
				double wn = 0.5 + 0.5 * std::cos(PI * d / half);   // Hann
				acc += in[j] * sinc * wn;
			}
			out[i] = (float)acc;
		}
		float pk = 0.f;
		for (float v : out) pk = std::max(pk, std::fabs(v));
		if (pk > 1e-6f) for (float& v : out) v *= 0.95f / pk;
		return out;
	}
	int typeOf(const float* k) const { return (int)clampf(std::round(k[cymbals::TYPE]), 0.f, 2.f); }
	static float natural(int ty) { static const float T[3] = {0.7f, 2.2f, 1.6f}; return T[ty]; }
	static float srcCut(int ty) { static const float F[3] = {6500.f, 2800.f, 2200.f}; return F[ty]; }
	void derive(const float* k) {
		using namespace cymbals;
		float ag = k[AGE], worn = Age::worn(ag);
		float wd = age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float T = (type == 0 && !open) ? c[CHDECAY] * 1e-3f : natural(type) * c[DECAY] * age.part(2, ag, 0.15f);
		kv = t60coef(T * wd);
	}
	void start(float acc, const float* k, bool isOpen) {
		using namespace cymbals;
		type = typeOf(k);
		open = isOpen || type > 0;
		drifts(k[AGE]);
		refresh(k); woke();
		amp = accentGain(k[ACCENT], acc) * age.part(5, k[AGE], 0.1f);
		ve = 1.f; sPos = 0;
		// a recording plays the same every time: restart the oscillators and the noise
		for (int i = 0; i < 6; i++) oph[i] = 0.f;
		rng.seed(0x909CCu + 7u * (uint32_t)type);
		srcHp1.z = srcHp2.z = srcLp.z = 0.f;
		t = 0.f;
	}
	void hit(float acc, const float* k) { start(acc, k, false); }
	void hit2(float acc, const float* k) { start(acc, k, true); }
	bool sourceDone() const { return !user.empty() && sPos >= user.size(); }
	// one source sample at the clock rate (the "ROM" read), at full level
	float sourceTick(const float* k) {
		using namespace cymbals;
		if (!user.empty()) return sPos < user.size() ? user[sPos++] : 0.f;
		static const float base[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f};
		static const float mul[3] = {1.6f, 1.15f, 1.35f};
		float m = std::exp2(c[METAL]) * mul[type] * age.part(7, k[AGE], 0.05f);
		float sq = 0.f;
		for (int i = 0; i < 6; i++) {
			oph[i] += base[i] * m * (1.f + 0.003f * i) / CLOCK;
			if (oph[i] >= 1.f) oph[i] -= 1.f;
			sq += oph[i] < 0.48f ? 1.f : -1.f;
		}
		float nz = rng.bi();
		// metal and noise each scaled so the "recording" uses the DAC's range, as a ROM would
		static const float gm[3] = {4.6f, 2.2f, 1.65f}, gn[3] = {1.28f, 0.69f, 0.69f};
		float x = (1.f - k[NOISE]) * sq * (1.4f / 6.f) * gm[type] + k[NOISE] * nz * gn[type];
		if (type == 2) x += 0.25f * gm[type] * (1.f - k[NOISE]) * std::sin(2.f * PI * oph[5] * 2.f);   // ride bell
		x = srcHp2.hp(srcHp1.hp(x, srcCut(type), CLOCK), srcCut(type), CLOCK);
		return srcLp.lp(x, 14000.f, CLOCK);
	}
	void process(const float* k, float voct) {
		using namespace cymbals;
		tick(k);
		const float ag = k[AGE];
		float y = 0.f;
		if (ve > 1e-5f && !(sourceDone() && std::fabs(recon.lp) < 1e-6f)) {
			if (!std::isfinite(voct)) voct = 0.f;
			float clk = CLOCK * std::exp2(c[TUNE] + voct) * age.part(0, ag, 0.04f) * hitPitch;
			clk = clampf(clk, 1000.f, 7.f * fs);
			float levels = std::exp2(c[BITS] - 1.f);
			float s = dac.process(clk, fs, [&]() {
				float v = clampf(sourceTick(k), -1.f, 1.f);
				return clampf(std::round(v * levels), -levels, levels - 1.f) / levels;   // 2^BITS steps
			});
			recon.process(s, c[RECON] * age.part(4, ag, 0.15f), 0.75f, fs);
			float v = recon.lp * ve;
			ve *= kv;
			float cut = c[CUT];
			y = hp2.hp(hp1.hp(v, cut, fs), cut, fs) * G * amp;
		}
		else { ve = 0.f; y = 0.f; }
		t += 1.f / fs;
		env = clampf(10.f * ve * amp / 1.78f, 0.f, 10.f);
		out = finish(floorZero(y), ag, k[DRIVE], k[LEVEL]);
	}
};

#undef C909_VAL

}  // namespace c909
