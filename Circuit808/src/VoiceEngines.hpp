// Sound engines for the 808 Circuits voices (all but the kick, see KickEngine.hpp).
// Each is a small circuit model of one TR-808 voice built from Common808.hpp parts.
// Stock numbers (frequencies, ring and envelope times) are the commonly published
// figures for the 808 voices and are fitted, not read from component values; every
// knob's stock position is a red dot on the panel. Rack-free (see test/voices.cpp).
//
// Shared behaviour (2.2.0):
//  - accent: one law for every voice, about +5 dB at stock ACCENT (accentLaw);
//  - noise has the same in-band level at any sample rate (Rng::noise);
//  - STRIKE and tuning hold the ring level (strikeComp), STRIKE above 1 ms rounds edges;
//  - knob curves are worked out every 16 samples (and on each hit), not every sample;
//  - DRIVE and LEVEL are smoothed (5 ms) and a 10 Hz DC blocker follows the output stage;
//  - a voice that has been silent for 0.1 s reports sleeping() so the module can skip it.
// All noise voices share one noise sequence, as the real 808 has one noise generator.
#pragma once
#include "Common808.hpp"
#include "VoiceSpecs.hpp"

namespace c808 {

static const float VCA_ACC = 0.748f;   // VCA voices: gain at an unaccented 4 V trigger

struct EngineBase {
	float fs = 48000.f, out = 0.f, env = 0.f;
	Age age;
	Rng rng;
	float hitPitch = 1.f, hitDecay = 1.f, hitLevel = 1.f;
	// control-rate knob values (c[i] = the knob's value as the tooltip shows it)
	float c[16] = {};
	int ctr = 0;
	// output stage state
	float dcZ = 0.f, levelS = -1.f, driveS = 0.f;
	int quiet = 0;
	bool busy = false;   // a voice sets this while something is still scheduled (bursts, shake)
	void setSampleRate(float s) { fs = s; }
	void drifts(float ageK) {
		hitPitch = age.drift(ageK, 0.004f);
		hitDecay = age.drift(ageK, 0.06f);
		hitLevel = age.drift(ageK, 0.06f);
	}
	bool sleeping() const { return quiet > (int)(0.1f * fs); }
	// worn-part hiss (ahead of the output amp), op-amp, smoothed LEVEL, DC blocker
	float finish(float y, float ageK, float drive, float level) {
		const float dt = 1.f / fs;
		float w = Age::worn(ageK);
		y += age.hiss(ageK);
		float sm = 1.f - std::exp(-dt / 0.005f);
		if (levelS < 0.f) { levelS = level; driveS = drive; }
		driveS += (drive - driveS) * sm;
		levelS += (level - levelS) * sm;
		float o = opamp(y, driveS);
		dcZ += (o - dcZ) * (1.f - std::exp(-dt * 2.f * PI * 10.f));
		o -= dcZ;
		if (std::fabs(dcZ) < 1e-20f) dcZ = 0.f;
		o *= levelS * hitLevel * age.part(6, ageK, 0.1f);
		quiet = (std::fabs(y) < 1e-6f && env < 1e-5f && w == 0.f && !busy) ? quiet + 1 : 0;
		return o;
	}
	void woke() { quiet = 0; }
};

// V(k, i): knob value straight from the table; refresh() / tick() fill the cache c[].
#define C808_VAL(NS) \
	float V(const float* k, int i) const { return knobValue(NS::spec().knobs[i], k[i]); } \
	void refresh(const float* k) { const VoiceSpec& s = NS::spec(); for (int i = 0; i < s.nKnobs && i < 16; i++) c[i] = knobValue(s.knobs[i], k[i]); } \
	void tick(const float* k) { if ((ctr++ & 15) == 0) refresh(k); }

// ---------------------------------------------------------------- snare
// Two bridged-T shells (about 180 Hz and 330 Hz) from the trigger pulse, balanced by TONE
// as on the 808, plus white noise through a high-pass (COLOUR) and low-pass (HISS) into
// its own VCA (SNAPPY). Accent raises shells and noise together.
struct SnareEngine : EngineBase {
	static const VoiceSpec& spec() { return snare::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(snare)
	PulseShaper sh;
	Resonator r1, r2;
	OnePole nhp, nlp, aa;
	float t = 1e9f, nEnv = 0.f, nAmp = 0.f, sg1 = 1.f, sg2 = 1.f;
	static constexpr float G = 0.85f, GN = 10.9f;
	void reset() { r1.reset(); r2.reset(); nEnv = 0.f; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace snare;
		refresh(k); woke();
		float a = accentLaw(triggerVolts(k[ACCENT], acc)) * age.part(5, k[AGE], 0.1f);
		float sMs = c[STRIKE];
		sh.fire(4.f * a, sMs);
		float f1 = c[TUNE], f2 = f1 * c[RATIO];
		sg1 = strikeComp(4.f * a, sMs, f1, 180.f, fs);
		sg2 = strikeComp(4.f * a, sMs, f2, 330.f, fs);
		drifts(k[AGE]);
		nEnv = 1.f; nAmp = VCA_ACC * a;
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace snare;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		float m = c[SWEEP] > 0.f ? std::exp2(c[SWEEP] * std::exp(-t / 0.003f)) : 1.f;
		t += dt;
		float f1 = c[TUNE] * std::exp2(voct) * age.part(0, ag, 0.12f) * hitPitch;
		float f2 = f1 * c[RATIO] * age.part(1, ag, 0.05f);
		float T1 = c[DECAY] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float b1 = r1.process(x * sg1, f1 * m, T1, fs, 2.f * PI * 180.f);
		float b2 = r2.process(x * sg2, f2 * m, T1 * 0.6f, fs, 2.f * PI * 330.f);
		float bal = k[TONE];
		float body = G * (std::min(1.f, 2.f * (1.f - bal)) * b1 + 0.8f * std::min(1.f, 2.f * bal) * b2) + 0.1f * x;
		float n = 0.f;
		if (nEnv > 1e-7f) {
			float w = rng.noise(fs);
			n = nhp.hp(w, c[NOISECOL] * age.part(4, ag, 0.2f), fs);
			n = nlp.lp(n, c[HISS], fs);
			n = aa.lp(n, 18000.f, fs);
			nEnv *= decayMul(c[SNAPDECAY] * 1e-3f * age.part(2, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn), dt);
		}
		else nEnv = 0.f;
		float noise = GN * n * nEnv * nAmp * k[SNAPPY] * 2.f;
		env = clampf(G * std::max(r1.amp(), r2.amp()) * 1.4f + 6.f * nEnv * nAmp * k[SNAPPY], 0.f, 10.f);
		out = finish(body + noise, ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- clap
// White noise through a band-pass, into a VCA driven by a sawtooth that fires several
// quick, evenly spaced bursts (the last one rings longer), plus a separate "reverb"
// envelope on the same noise. BAND and RES keep the loudness (bandwidth-normalised).
struct ClapEngine : EngineBase {
	static const VoiceSpec& spec() { return clap::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(clap)
	Svf bp;
	OnePole tone;
	float t = 1e9f, envC = 0.f, envT = 0.f, envTr = 0.f, amp = 0.f, gap = 0.01f;
	float burstT = 1e9f;   // time since the current burst fired
	bool lastBurst = false;
	int burstsLeft = 0;
	float nextAt = 0.f;
	static constexpr float G = 10.5f;
	static constexpr float BAND_STOCK = 1000.f, RES_STOCK = 1.5f;
	void reset() { envC = envT = envTr = 0.f; burstsLeft = 0; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace clap;
		refresh(k); woke();
		float A = triggerVolts(k[ACCENT], acc);
		amp = VCA_ACC * accentLaw(A) * age.part(5, k[AGE], 0.1f);
		drifts(k[AGE]);
		burstsLeft = (int)c[BURSTS];
		gap = c[SPREAD] * 1e-3f * age.part(1, k[AGE], 0.15f);
		nextAt = 0.f;
		envTr = 1.f;
		t = 0.f;
	}
	void process(const float* k, float) {
		using namespace clap;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		if (burstsLeft > 0 && t >= nextAt) {
			envC = 1.f; burstT = 0.f;
			burstsLeft--;
			lastBurst = burstsLeft == 0;
			// even spacing, plus a little hand slop with AGE
			nextAt += gap * (1.f + 0.08f * ag * rng.bi());
		}
		busy = burstsLeft > 0;
		t += dt;
		float snap = c[SNAP] * 1e-3f;
		if (lastBurst) envC *= std::exp(-dt / (4.5f * snap));         // the last clap rings on
		else envC = std::max(0.f, 1.f - burstT / (2.5f * snap));       // sawtooth ramp
		burstT += dt;
		// tail: rises over ~3 ms, then decays
		envT += (envTr - envT) * (1.f - std::exp(-dt / 0.003f));
		envTr *= decayMul(c[TAIL] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn), dt);
		if (envT < 1e-7f && envTr < 1e-7f) envT = envTr = 0.f;
		float mix = k[MIX];
		float wc = std::min(1.f, 2.f * (1.f - mix)), wt = 0.6f * std::min(1.f, 2.f * mix);
		float e = wc * envC + wt * envT;
		float y = 0.f;
		if (e > 1e-7f || std::fabs(bp.bp) > 1e-7f) {
			float q = c[RES], band = c[BAND] * age.part(0, ag, 0.15f) * hitPitch;
			bp.process(rng.noise(fs), band, q, fs);
			// unity peak, then held to the stock loudness across BAND and RES
			float n = bp.bp / q * 2.f * std::sqrt((q / RES_STOCK) * (BAND_STOCK / clampf(band, 50.f, 20000.f)));
			y = tone.lp(G * n * e * amp, c[TONE] * age.part(4, ag, 0.2f), fs);
		}
		env = clampf(7.f * e * amp, 0.f, 10.f);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- tom / conga
// One bridged-T with a downward pitch bend; toms add a pink-noise layer (SLAP).
// All three sizes share one level reference, so low, mid and high sit together.
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
	OnePole nlp, nhp, tone;
	float t = 1e9f, nEnv = 0.f, nAmp = 0.f, sg = 1.f;
	static constexpr float G = 3.0f, F_REF = 130.f;
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
		refresh(k); woke();
		float a = accentLaw(triggerVolts(k[ACCENT], acc)) * age.part(5, k[AGE], 0.1f);
		float sMs = c[STRIKE];
		sh.fire(4.f * a, sMs);
		int range = (int)clampf(std::round(k[RANGE]), 0.f, 2.f);
		float f = baseHz(range, k[MODE] > 0.5f) * std::exp2(c[TUNE]);
		sg = strikeComp(4.f * a, sMs, f, F_REF, fs);
		drifts(k[AGE]);
		nEnv = 1.f; nAmp = VCA_ACC * a;
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace tom;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		int range = (int)clampf(std::round(k[RANGE]), 0.f, 2.f);
		bool conga = k[MODE] > 0.5f;
		float x = sh.process(dt) * sg;
		float m = c[BEND] > 0.f ? std::exp2(c[BEND] * std::exp(-t / 0.03f)) : 1.f;
		float muteK = (t > 0.006f) ? (1.f - 0.92f * k[MUTE]) : 1.f;
		t += dt;
		float base = baseHz(range, conga);
		float f = base * std::exp2(c[TUNE] + voct) * age.part(0, ag, 0.12f) * hitPitch;
		float T = baseT60(range, conga) * c[DECAY] * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn) * muteK;
		const float wRef = 2.f * PI * F_REF;
		float b1 = r1.process(x, f * m, T, fs, wRef);
		float dbl = k[DOUBLE];
		float b2 = 0.f;
		if (dbl > 0.001f) b2 = r2.process(x, f * m * (1.f + 0.035f * dbl), T * 0.85f, fs, wRef);
		else r2.reset();
		float body = G * (b1 + 0.7f * dbl * b2);
		float n = 0.f;
		if (nEnv > 1e-7f) {
			nEnv *= decayMul(0.16f * age.part(2, ag, 0.25f) * muteK, dt);
			float p = nlp.lp(pink.process(rng.noise(fs)), 2500.f, fs);
			n = nhp.hp(p, 200.f, fs) * nEnv * nAmp * k[SLAP] * 6.f;
		}
		else nEnv = 0.f;
		float y = tone.lp(body + n, c[TONE] * age.part(4, ag, 0.2f), fs);
		env = clampf(G * r1.amp() * 1.4f, 0.f, 10.f);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
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
	float t = 1e9f, gate = 0.f, gateLen = 0.008f, sg1 = 1.f, sg2 = 1.f, accPost = 1.f;
	static constexpr float G = 2.6f;
	void reset() { r1.reset(); r2.reset(); gate = 0.f; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace rim;
		refresh(k); woke();
		float a = accentLaw(triggerVolts(k[ACCENT], acc)) * age.part(5, k[AGE], 0.1f);
		float sMs = c[STRIKE];
		sh.fire(4.f * a, sMs);
		float f1 = 455.f * std::exp2(c[TUNE]), f2 = f1 * c[RATIO];
		sg1 = strikeComp(4.f * a, sMs, f1, 455.f, fs);
		sg2 = strikeComp(4.f * a, sMs, f2, 1667.f, fs);
		drifts(k[AGE]);
		gateLen = c[GATE] * 1e-3f * age.part(2, k[AGE], 0.2f);
		accPost = std::pow(a, 0.75f);   // most of the accent after the VCA, so its curve does not eat it
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace rim;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		gate = (t < gateLen) ? 1.f : gate * std::exp(-dt / 0.0015f);
		if (gate < 1e-9f) gate = 0.f;
		t += dt;
		float f1 = 455.f * std::exp2(c[TUNE] + voct) * age.part(0, ag, 0.12f) * hitPitch;
		float f2 = f1 * c[RATIO] * age.part(1, ag, 0.05f);
		float T = c[RING] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float s = r1.process(x * sg1, f1, T, fs, 2.f * PI * 455.f) + 0.8f * r2.process(x * sg2, f2, T * 0.7f, fs, 2.f * PI * 1667.f);
		// the transistor VCA: asymmetric, harder with DRIVE
		float v = s * gate * G / accPost;
		float d = 1.f + 3.f * k[DRIVE];
		v = v > 0.f ? std::tanh(v * d * 0.25f) * 4.f / std::sqrt(d) : std::tanh(v * d * 0.18f) * 5.5f / std::sqrt(d);
		hp.process(v, c[SNAP] * age.part(4, ag, 0.2f), 0.7f, fs);
		env = clampf(gate * 10.f * std::max(r1.amp(), r2.amp()), 0.f, 10.f);
		out = finish(hp.hp * 1.2f * accPost, ag, 0.f, k[LEVEL]);
	}
};

// ---------------------------------------------------------------- claves
// One high bridged-T (2.5 kHz) with a very short ring; OVERTONE adds a second one.
struct ClaveEngine : EngineBase {
	static const VoiceSpec& spec() { return clave::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(clave)
	PulseShaper sh;
	Resonator r1, r2;
	float t = 1e9f, sg1 = 1.f, sg2 = 1.f, voctHit = 0.f;
	static constexpr float G = 1.0f, F0 = 2500.f, RATIO_STOCK = 2.4f;
	void reset() { r1.reset(); r2.reset(); out = env = 0.f; }
	void hit(float acc, const float* k) { hitAt(acc, k, voctHit); }
	void hitAt(float acc, const float* k, float voct) {
		using namespace clave;
		refresh(k); woke();
		float a = accentLaw(triggerVolts(k[ACCENT], acc)) * age.part(5, k[AGE], 0.1f);
		float sMs = c[STRIKE];
		sh.fire(4.f * a, sMs);
		float f = F0 * std::exp2(c[TUNE] + voct);
		sg1 = strikeComp(4.f * a, sMs, f, F0, fs);
		sg2 = strikeComp(4.f * a, sMs, f * c[RATIO], F0 * RATIO_STOCK, fs);
		drifts(k[AGE]);
		t = 0.f;
	}
	void process(const float* k, float voct) {
		using namespace clave;
		tick(k);
		voctHit = voct;   // the next hit's compensation follows the played pitch
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float x = sh.process(dt);
		float T = c[RING] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		float m = c[BEND] > 0.f ? std::exp2(c[BEND] * std::exp(-t / std::max(0.004f, 0.35f * T))) : 1.f;
		t += dt;
		float f = F0 * std::exp2(c[TUNE] + voct) * age.part(0, ag, 0.12f) * hitPitch;
		const float wRef = 2.f * PI * F0;
		float b1 = r1.process(x * sg1, f * m, T, fs, wRef);
		float b2 = 0.f;
		if (k[OVERTONE] > 0.001f) b2 = r2.process(x * sg2, f * m * c[RATIO], T * 0.6f, fs, wRef);
		else r2.reset();
		float y = G * (b1 + 0.8f * k[OVERTONE] * b2);
		env = clampf(G * r1.amp() * 1.4f, 0.f, 10.f);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- maracas
// White noise, high-passed, through a short attack-decay VCA. GRAINS swaps the smooth
// noise for separate beads; beads come at a rate per second that follows the envelope,
// with a floor, so every hit has some.
struct MaracasEngine : EngineBase {
	static const VoiceSpec& spec() { return maracas::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(maracas)
	Svf hp;
	OnePole aa;
	float t = 1e9f, e = 0.f, peak = 0.f, amp = 0.f;
	float shakeAt = -1.f, shakeAmp = 0.f;
	static constexpr float G = 6.8f, BEAD = 3.f;
	void reset() { e = 0.f; shakeAt = -1.f; out = env = 0.f; }
	void start(float a) { peak = a; t = 0.f; }
	void hit(float acc, const float* k) {
		using namespace maracas;
		refresh(k); woke();
		float A = triggerVolts(k[ACCENT], acc);
		amp = VCA_ACC * accentLaw(A) * age.part(5, k[AGE], 0.1f);
		drifts(k[AGE]);
		start(1.f);
		float s = k[SHAKE];
		if (s > 0.01f) { shakeAt = (0.06f + 0.14f * s) * (1.f + 0.1f * k[AGE] * rng.bi()); shakeAmp = 0.35f + 0.4f * s; }
		else shakeAt = -1.f;
	}
	void process(const float* k, float) {
		using namespace maracas;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		if (shakeAt >= 0.f && t >= shakeAt) { float a2 = shakeAmp; shakeAt = -1.f; start(a2); }
		busy = shakeAt >= 0.f;
		float atk = c[ATTACK] * 1e-3f;
		float T = c[LENGTH] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn);
		if (t < atk) e += (peak - e) * std::min(1.f, dt / std::max(atk - t, dt));
		else e *= decayMul(T, dt);
		if (e < 1e-8f) e = 0.f;
		t += dt;
		float y = 0.f;
		if (e > 0.f || std::fabs(hp.bp) > 1e-7f) {
			// noise source: smooth white <-> separate beads
			float g = k[GRAINS];
			float src = rng.noise(fs) * (1.f - g);
			if (g > 0.f) {
				float rate = (400.f + 1600.f * (1.f - g)) * clampf(e / std::max(peak, 1e-3f), 0.15f, 1.f);
				if (rng.uni() < rate / fs) src += g * (0.5f + 0.5f * rng.uni()) * (rng.bi() > 0.f ? BEAD : -BEAD) * std::sqrt(fs / 48000.f);
			}
			hp.process(src, c[CUT] * age.part(4, ag, 0.2f), 0.7f, fs);
			y = G * aa.lp(hp.hp, 18000.f, fs) * e * amp;
		}
		env = clampf(8.f * e * amp, 0.f, 10.f);
		out = finish(y, ag, k[DRIVE], k[LEVEL]);
	}
};

// ---------------------------------------------------------------- cowbell
// Two square oscillators (540 Hz and 800 Hz on the 808) into a VCA whose envelope drops
// fast to a break point and rings on from there; the gated sum then goes through the
// band-pass, which rounds the onset as in the circuit.
struct CowbellEngine : EngineBase {
	static const VoiceSpec& spec() { return cowbell::spec(); }
	static float stockFor(int i, const float*) { return spec().knobs[i].stock; }
	C808_VAL(cowbell)
	float ph1 = 0.f, ph2 = 0.f;
	Svf bp;
	float t = 1e9f, e1 = 0.f, e2 = 0.f, amp = 0.f;
	static constexpr float G = 6.2f, RES_STOCK = 2.f;
	void reset() { e1 = e2 = 0.f; out = env = 0.f; }
	void hit(float acc, const float* k) {
		using namespace cowbell;
		refresh(k); woke();
		float A = triggerVolts(k[ACCENT], acc);
		amp = VCA_ACC * accentLaw(A) * age.part(5, k[AGE], 0.1f);
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
		if (!std::isfinite(ph) || ph < 0.f || ph >= 1.f) ph = 0.f;
		float y = (ph < w) ? 1.f : -1.f;
		y += blep(ph, d);
		float p2 = ph - w; if (p2 < 0.f) p2 += 1.f;
		y -= blep(p2, d);
		return y;
	}
	void process(const float* k, float voct) {
		using namespace cowbell;
		tick(k);
		const float dt = 1.f / fs, ag = k[AGE], worn = Age::worn(ag);
		float f1 = 540.f * std::exp2(c[TUNE] + voct) * age.part(0, ag, 0.06f) * hitPitch;
		float f2 = f1 * c[INTERVAL] * age.part(1, ag, 0.04f);
		float w = c[WIDTH] * 0.01f;
		float s = 0.5f * (pulse(ph1, f1, w) + pulse(ph2, f2, w));
		e1 *= std::exp(-dt / 0.012f);
		e2 *= decayMul(c[DECAY] * 1e-3f * age.part(3, ag, 0.25f) * hitDecay * (1.f - 0.3f * worn), dt);
		if (e1 < 1e-8f) e1 = 0.f;
		if (e2 < 1e-8f) e2 = 0.f;
		t += dt;
		float e = e1 + e2;
		float q = c[RES];
		bp.process(s * e, c[FILTER] * age.part(4, ag, 0.15f), q, fs);
		// partly normalised for Q, pinned to the stock level: more RES rings more
		float n = bp.bp / std::sqrt(q * RES_STOCK);
		env = clampf(8.f * e * amp, 0.f, 10.f);
		out = finish(G * n * amp * 2.f, ag, k[DRIVE], k[LEVEL]);
	}
};

#undef C808_VAL

}  // namespace c808
