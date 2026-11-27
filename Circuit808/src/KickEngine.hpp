// 808 Kick engine: a circuit model of the Roland TR-808 bass drum, after
// K. J. Werner, J. S. Abel, J. O. Smith, "A Physically-Informed, Circuit-Bendable,
// Digital Model of the Roland TR-808 Bass Drum Circuit", DAFx-14.
//
// Blocks, as in the paper:
//   trigger pulse (1 ms, 4-14 V set by accent)
//   -> pulse shaper (low-shelf R162/R163/C40 with diode clamp D53 at about -0.71 V)
//   -> bridged-T resonator (about 49.5 Hz). Its output = input + a band-pass ring,
//      so the shaped pulse itself is heard as the attack click.
//   -> feedback buffer sets the decay (DECAY pot). Here: decay time directly; past the
//      stock range the loop gain goes above one and the drum sustains (self-oscillates).
//   attack jump: while the envelope Q41/Q42 is high (about 6 ms), Q43 shorts part of
//      the bridged-T resistance: pitch jumps up by more than an octave and Q rises.
//   pitch sigh: on big negative swings Q43 leaks (soft-plus fit, alpha 14.315, V0 0.556),
//      lowering the resistance again, so loud early cycles run sharp (about 58 -> 49.5 Hz).
//   -> tone low-pass (one pole) -> output high-pass -> op-amp stage (DRIVE, rails).
// The paper does not print component values, so stock numbers here are fitted to its
// reported behaviour (49.5 Hz, >1 octave jump for ~6 ms, 48-58 Hz sigh).
// Rack-free so test/render.cpp can run it offline.
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace kick808 {

static inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

// Knob positions (0..1) of the stock 808 settings: the red dots on the panel.
struct Stock {
	static constexpr float TUNE_HZ = 49.5f;
	static constexpr float DECAY = 0.45f;
	static constexpr float TONE = 0.5f;
	static constexpr float ACCENT = 0.6f;
	static constexpr float SWEEP = 0.45f;   // 1.125 octave jump
	static constexpr float BEND = 0.4376f;  // 6 ms
	static constexpr float STRIKE = 0.5f;   // 1 ms trigger pulse
	static constexpr float CLICK = 0.5f;    // direct path gain 1
	static constexpr float SIGH = 0.25f;    // the 808's own leak
	static constexpr float TENSION = 0.f;
	static constexpr float DRIVE = 0.f;
	static constexpr float SHELF = 0.33f;   // decay rate the same at every pitch (as the circuit)
	static constexpr float RETRIG = 0.2f;   // the 808's small retrigger pulse
	static constexpr float AGE = 0.f;       // a new unit, every part exactly on value
};

struct Controls {
	float tuneHz = Stock::TUNE_HZ;  // after V/OCT
	float decay = Stock::DECAY;
	float tone = Stock::TONE;
	float accent = Stock::ACCENT;   // VR3: how far an accented hit goes towards 14 V
	float sweep = Stock::SWEEP;
	float bend = Stock::BEND;
	float strike = Stock::STRIKE;
	float click = Stock::CLICK;
	float sigh = Stock::SIGH;
	float tension = Stock::TENSION;
	float drive = Stock::DRIVE;
	float shelf = Stock::SHELF;
	float retrig = Stock::RETRIG;
	float age = Stock::AGE;
	float level = 1.f;
};

// ---- knob -> value maps (shared with the module's tooltips)
// DECAY: 0..0.8 = stock-range ring time (T60) 80 ms .. 1.6 s, 0.8..0.95 = extended to 8 s,
// 0.95..1 = sustain (self-oscillation, rising loop gain).
static inline float decayT60(float d) {
	if (d <= 0.8f) return 0.08f * std::pow(20.f, d / 0.8f);
	return 1.6f * std::pow(5.f, std::min(d - 0.8f, 0.15f) / 0.15f);
}
static inline float toneHz(float t) { return 150.f * std::pow(10000.f / 150.f, t); }   // 150 Hz .. 10 kHz
static inline float sweepOct(float s) { return 2.5f * s; }                             // 0 .. 2.5 octaves
static inline float bendMs(float b) { return std::pow(60.f, b); }                      // 1 .. 60 ms
static inline float strikeMs(float s) { return 0.1f * std::pow(100.f, s); }            // 0.1 .. 10 ms
static inline float clickGain(float c) { return 2.f * c; }                              // 0 .. 2 (stock 1)

// ---- AGE: the 808 was built with +-20 % capacitors and +-5 % resistors (Werner et al.),
// so every unit differed. Each module has a serial number (saved in the patch) that fixes
// where its parts sit inside the tolerance; AGE scales how far off they are. Each hit then
// drifts a little (temperature, supply), and past 0.7 the capacitors are worn: shorter
// ring and some hiss. AGE 0 = the exact stock unit.
struct Age {
	uint32_t state = 1;
	float u[8] = {0};   // per-unit offsets, -1..1
	void setSerial(uint32_t serial) {
		state = serial * 2654435761u + 12345u;
		for (int i = 0; i < 8; i++) u[i] = uniform();
	}
	float uniform() {   // -1..1
		state ^= state << 13; state ^= state >> 17; state ^= state << 5;
		return (state & 0xffffff) / float(0x7fffff) - 1.f;
	}
};

struct Engine {
	float fs = 48000.f;
	// resonator (trapezoidal state-variable form of the bridged-T)
	float ic1 = 0.f, ic2 = 0.f;
	float bp = 0.f, lp = 0.f;
	// pulse shaper
	float shLp = 0.f;
	// hit state
	float t = 1e9f;          // seconds since the last trigger
	float A = 0.f;           // trigger pulse height (V)
	float widthS = 0.001f;   // trigger pulse width
	float holdS = 0.006f;    // attack-jump time
	float jump = 0.f;        // 0..1 envelope of the attack jump
	float tensionAmp = 0.f;
	float retrigAt = 0.f, retrigA = 0.f;
	// output filters
	float toneZ = 0.f, hpZ = 0.f;
	// outputs
	float out = 0.f, env = 0.f;
	// AGE
	Age age;
	float hitPitch = 1.f, hitDecay = 1.f, hitLevel = 1.f;
	uint32_t noiseState = 22222u;

	static constexpr float W_STOCK = 2.f * 3.14159265f * Stock::TUNE_HZ;
	static constexpr float BODY_GAIN = 4.8f;   // fitted: stock unaccented hit peaks near 5 V
	static constexpr float SIGH_NODE = 3.f, SIGH_K = 0.5f;   // fitted: 808 sigh lands in 48-58 Hz

	void setSampleRate(float sr) { fs = sr; }
	void reset() { ic1 = ic2 = bp = lp = shLp = toneZ = hpZ = 0.f; t = 1e9f; jump = 0.f; out = env = 0.f; }

	// accentIn 0..1 (gate or velocity). Latches STRIKE and BEND for this hit.
	void hit(float accentIn, const Controls& c) {
		A = 4.f + 10.f * clampf(c.accent, 0.f, 1.f) * clampf(accentIn, 0.f, 1.f);
		float ag = clampf(c.age, 0.f, 1.f);
		hitPitch = 1.f + 0.004f * ag * age.uniform();
		hitDecay = 1.f + 0.06f * ag * age.uniform();
		hitLevel = 1.f + 0.06f * ag * age.uniform();
		A *= 1.f + 0.1f * ag * age.u[5];   // trigger level trimmer
		widthS = strikeMs(c.strike) * 1e-3f;
		holdS = std::max(bendMs(c.bend) * 1e-3f, widthS);
		// retrigger pulse (C39/R161/D52): small and right after the jump at the stock
		// setting; turned up it grows and moves later into a flammed second hit
		float r = clampf(c.retrig, 0.f, 1.f);
		retrigA = A * 0.5f * r;
		retrigAt = holdS + 0.03f * std::max(0.f, r - Stock::RETRIG) / (1.f - Stock::RETRIG);
		t = 0.f;
	}

	void process(const Controls& c) {
		const float dt = 1.f / fs;
		const float PI = 3.14159265f;

		// trigger pulse and pulse shaper: y = x - (1-d) * LP(x), d = R162/(R162+R163)
		float vtrig = (t < widthS) ? A : 0.f;
		if (t >= retrigAt && t < retrigAt + 0.5e-3f) vtrig += retrigA;
		const float d = 0.3f, tauS = 0.25e-3f;
		shLp += (vtrig - shLp) * (1.f - std::exp(-dt / tauS));
		float x = vtrig - (1.f - d) * shLp;
		// D53: the negative swing is held to about one diode drop
		if (x < 0.f) x = -0.71f * std::tanh(-x / 0.71f);

		// attack jump envelope (Q42 collector): up fast, holds, then falls in ~0.7 ms
		if (t < holdS) jump += (1.f - jump) * (1.f - std::exp(-dt / 0.1e-3f));
		else jump *= std::exp(-dt / 0.7e-3f);
		t += dt;

		// pitch
		float ag = clampf(c.age, 0.f, 1.f);
		float worn = clampf((ag - 0.7f) / 0.3f, 0.f, 1.f);
		float w0 = 2.f * PI * clampf(c.tuneHz * (1.f + 0.12f * ag * age.u[0]) * hitPitch, 5.f, 2000.f);
		float m = std::exp2(sweepOct(c.sweep) * (1.f + 0.15f * ag * age.u[1]) * jump);
		// sigh: Q43 leakage on negative swings of the bridged-T node
		const float alpha = 14.315f, V0 = 0.556f;
		float vneg = -bp * SIGH_NODE;
		float leak = (alpha * (vneg - V0) > 20.f) ? (vneg - V0) : std::log1p(std::exp(alpha * (vneg - V0))) / alpha;
		float sighK = 4.f * c.sigh * (1.f + 0.3f * ag * age.u[2]) * SIGH_K * std::tanh(leak);               // fraction of R_eff shorted
		m *= 1.f / std::sqrt(1.f - clampf(sighK, 0.f, 0.85f));  // f ~ 1/sqrt(R)
		// tension: smooth amplitude -> pitch (like a skin), at all levels
		float amp = std::sqrt(bp * bp + lp * lp);
		tensionAmp += (amp - tensionAmp) * (1.f - std::exp(-dt / 0.004f));
		m *= 1.f + 0.6f * c.tension * std::min(tensionAmp / 2.f, 2.f);

		float w = std::min(w0 * m, 0.45f * 2.f * PI * fs);
		float g = std::tan(0.5f * w * dt);

		// damping from the ring time; Q rises with the jump so the decay rate holds
		float k;
		float dk = clampf(c.decay, 0.f, 1.f);
		if (dk <= 0.95f) {
			// SHELF: the feedback buffer's high shelf. Stock: the same decay rate at any pitch;
			// up: raised pitches (jump, sigh, tension) die faster; down: they ring on
			k = 13.8155f / (w * decayT60(dk) * (1.f + 0.25f * ag * age.u[3]) * hitDecay * (1.f - 0.3f * worn)) * std::pow(m, 3.f * (clampf(c.shelf, 0.f, 1.f) - Stock::SHELF));
		} else {
			float s = (dk - 0.95f) / 0.05f;  // loop gain above one, soft limit holds the level
			k = -0.004f - 0.03f * s + 0.08f * (amp * amp) / (1.3f * 1.3f);
		}
		k = clampf(k, -0.2f, 2.f);

		// resonator, excited with a constant-level drive across tunings
		float u = x * (W_STOCK / w0);
		float a1 = 1.f / (1.f + g * (g + k));
		float a2 = g * a1, a3 = g * a2;
		float v3 = u - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		bp = v1; lp = v2;
		if (!std::isfinite(bp) || !std::isfinite(lp)) { ic1 = ic2 = bp = lp = 0.f; }

		// bridged-T output = input (click) + ring
		float y = clickGain(c.click) * x * 0.35f + BODY_GAIN * bp;

		// tone: one-pole low-pass (TPT)
		float gt = std::tan(PI * std::min(toneHz(c.tone) * (1.f + 0.2f * ag * age.u[4]), 0.45f * fs) * dt);
		float vt = (y - toneZ) * gt / (1.f + gt);
		float lpOut = vt + toneZ;
		toneZ = lpOut + vt;
		// output high-pass about 12 Hz
		float gh = std::tan(PI * 12.f * dt);
		float vh = (lpOut - hpZ) * gh / (1.f + gh);
		float hpLow = vh + hpZ;
		hpZ = hpLow + vh;
		float o = lpOut - hpLow;

		// output op-amp: DRIVE pushes into the rails (about +-10.5 V)
		float pre = 1.f + 11.f * c.drive * c.drive;
		o = 10.5f * std::tanh(o * pre / 10.5f) / (1.f + 0.6f * c.drive);
		if (worn > 0.f) {   // worn parts: a little hiss
			noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
			o += worn * 0.012f * ((noiseState & 0xffff) / 32768.f - 1.f);
		}
		out = o * c.level * hitLevel * (1.f + 0.1f * ag * age.u[6]);
		env = clampf(BODY_GAIN * amp * 1.6f, 0.f, 10.f);
	}
};

}  // namespace kick808
