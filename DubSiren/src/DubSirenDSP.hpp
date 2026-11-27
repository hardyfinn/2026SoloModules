// Dub Siren engine: a component-level model of the classic UK sound-system
// transistor siren (the NJD SE-1 circuit, as rebuilt in boxes like the
// Rigsmith NS1), from Zack Nelson's redrawn schematic of the NJD.
//
// What is modelled, part by part:
//  LFO       T1/T2 BC557 astable, C3/C4 10 uF, 1k collector loads (R14/R15),
//            base resistors picked by S3 (BEAT): 47k / 22k / 10k, OFF leaves
//            T2 without a base resistor so the sweep stops with its output low.
//  Shaper    LFO square -> D4 -> node A -> R22 1k -> C5 100 uF (the "shaped
//            LFO"). C5 drains through R22+R23 (2.2k) to ground in speeds 1-3,
//            or through R23+R24 (47k) when S3 is OFF: a slow 5 s fall.
//  S1 SIREN  pulls the LFO line to +V: C5 charges fast, the sweep is held
//            high, and on release the LFO restarts from the top.
//  S2 TONE   +V -> R25 10k -> node A: C5 rises slowly (fully only when BEAT
//            is OFF, otherwise R23 holds it down).
//  S4 MODE   what feeds each half of the oscillator's base resistors:
//              1: +V | shaped        2: +V | square
//              3: square | square, plus +V through D1/D2 and 220k to both
//              4: shaped | shaped
//  S5 PITCH  base resistors 470k / 220k / 100k / 68k (both halves).
//  Osc       T3/T4 BC550 astable, C1/C2 10 nF, 4.7k collectors. Each half
//            lasts until its base, charging toward its source voltage through
//            its resistor, reaches 0.6 V; a source below that never gets
//            there, so the oscillator stalls (the stutter of modes 1-2). The
//            collectors recover through 4.7k, rounding the rising edges.
//  Output    T4 collector -> C9 100n / R26 15k -> C8 100n / R27 15k (two
//            ~106 Hz high-passes: the thin, sagging square) -> R3/R30.
//  Supply    TRIG switches +9 V; C10 then holds the circuit up for a moment,
//            so letting go makes everything sag, slow and fall in pitch.
// Extras (not in the circuit): SPEED scales the LFO, DEPTH scales how far
// the modulation pulls away from +V, V/OCT scales the pitch resistors,
// RELEASE sets how long C10 holds up, and the echo (FEEDBACK/TIME/ERA).
//
// The circuit runs at 4x the sample rate. Header-only and Rack-free so
// test/render.cpp can run it offline.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace dubsiren {

static const float TWO_PI = 6.2831853f;
static const float VCC = 9.f;
static const float VBE = 0.6f;
static const float VSAT = 0.1f;
static const int OS = 4;
// S5: pitch base resistors, PITCH 1..4
static const float R_PITCH[4] = {470e3f, 220e3f, 100e3f, 68e3f};
// S3: LFO base resistors, BEAT 1..3 (OFF = none on T2)
static const float R_LFO[3] = {47e3f, 22e3f, 10e3f};

static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float onePoleK(float hz, float dt) { return 1.f - std::exp(-TWO_PI * hz * dt); }
static inline float tauK(float tau, float dt) { return tau <= 0.f ? 1.f : 1.f - std::exp(-dt / tau); }

struct Rng {
	uint32_t s = 0x9E3779B9u;
	float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
	float bi() { return 2.f * uni() - 1.f; }
};

struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(float fc, float fs, float q) {
		float w = TWO_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		float a0 = 1.f + al;
		b0 = (1.f - c) / 2.f / a0; b1 = (1.f - c) / a0; b2 = b0;
		a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};

struct Settings {
	int pitch = 1;           // S5, 0..3
	float voct = 0.f;        // V/OCT: scales both pitch resistors
	int mode = 3;            // S4, 0..3 (MODE 1..4)
	int beat = 0;            // S3, 0..2 = speeds 1-3, 3 = OFF
	float speed = 1.f;       // LFO rate multiplier (SPEED)
	float depth = 1.f;       // 1 = the circuit as built
	float release = 0.35f;   // C10 hold-up time constant, seconds
	float feedback = 0.f;    // echo repeats 0..1.15 (0 = no echo)
	float time = 0.33f;      // echo time, seconds
	bool era80 = true;       // 80s tape echo / 90s chip echo
	bool power = false;      // TRIG
	bool siren = false;      // S1 held
	bool tone = false;       // S2 held
	bool sync = false;       // one-sample pulse: restart the LFO (CLOCK)
};

// Two-transistor astable multivibrator, solved per side.
// Voltages are "as if NPN" (the PNP LFO is the same thing upside down).
struct Astable {
	bool on[2] = {true, false};
	float vb[2] = {VBE, -5.f};
	float vc[2] = {VSAT, VCC};
	bool flipped = false;     // a transistor turned on this step
	// vth/rth: Thevenin source feeding each base; C: timing caps; rc: collector loads
	void step(const float* vth, const float* rth, float vcc, float c, float rc, float dt) {
		flipped = false;
		float kr = tauK(rc * c, dt);
		for (int i = 0; i < 2; i++) {
			if (on[i]) {
				vc[i] = VSAT;
				vb[i] = VBE;
				if (vth[i] < VBE + 0.02f) on[i] = false;   // lost its base drive
			} else {
				vc[i] += (vcc - vc[i]) * kr;
				vb[i] += (vth[i] - vb[i]) * tauK(rth[i] * c, dt);
			}
		}
		for (int i = 0; i < 2; i++) {
			if (!on[i] && vb[i] >= VBE && vth[i] > VBE && vcc > 0.8f) {
				int j = 1 - i;
				float drop = vc[i] - VSAT;
				vc[i] = VSAT;
				vb[i] = VBE;
				on[i] = true;
				vb[j] = std::min(vb[j], VBE) - drop;   // the falling collector kicks the other base down
				on[j] = false;
				flipped = true;
				break;
			}
		}
	}
	// hold side i on and the other side freshly kicked off
	void force(int i, float vcc) {
		int j = 1 - i;
		on[i] = true; vb[i] = VBE; vc[i] = VSAT;
		on[j] = false; vb[j] = VBE - (vcc - VSAT);
	}
};

// ---------------------------------------------------------------- echo
struct Echo {
	std::vector<float> buf;
	int w = 0;
	float sr = 48000.f;
	float dSmooth = 0.33f;
	float hp = 0.f, lp1 = 0.f, lp2 = 0.f;
	float wowPh = 0.f, flutPh = 0.f, drift = 0.f;
	float shPh = 0.f, shVal = 0.f;
	Rng rng;
	void setRate(float r) {
		sr = r;
		buf.assign((size_t) (sr * 2.2f) + 8, 0.f);
		w = 0;
	}
	float read(float d) {
		int n = (int) buf.size();
		float pos = (float) w - d;
		while (pos < 0.f) pos += (float) n;
		int i0 = (int) pos;
		float f = pos - i0;
		float y0 = buf[(i0 - 1 + n) % n], y1 = buf[i0 % n], y2 = buf[(i0 + 1) % n], y3 = buf[(i0 + 2) % n];
		float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
		return ((c3 * f + c2) * f + c1) * f + y1;
	}
	// in: roughly +-1. Returns the wet signal.
	float process(float in, float timeS, float fb, bool era80, float dt) {
		if (buf.empty()) return 0.f;
		dSmooth += (timeS - dSmooth) * tauK(era80 ? 0.18f : 0.06f, dt);   // tape bends slowly when TIME moves
		float d = dSmooth * sr;
		float lim = (float) buf.size() - 8.f;
		float wet;
		if (era80) {
			// 80s: tape echo, wow + flutter + drift, dark top, a little hiss
			wowPh += 0.55f * dt; if (wowPh > 1.f) wowPh -= 1.f;
			flutPh += 6.8f * dt; if (flutPh > 1.f) flutPh -= 1.f;
			drift += (rng.bi() - drift) * tauK(0.8f, dt);
			float m = 1.f + 0.0035f * std::sin(TWO_PI * wowPh) + 0.0011f * std::sin(TWO_PI * flutPh) + 0.0015f * drift;
			wet = read(clampf(d * m, 8.f, lim));
			lp1 += (wet - lp1) * onePoleK(2900.f, dt);
			lp2 += (lp1 - lp2) * onePoleK(4200.f, dt);
			wet = lp2 + 0.0012f * rng.bi();
		} else {
			// 90s: PT2399-style chip. Longer delay = slower clock = darker and grittier
			wet = read(clampf(d, 8.f, lim));
			float chipRate = clampf(44000.f * 0.12f / std::max(dSmooth, 0.02f), 7000.f, sr);
			shPh += chipRate * dt;
			if (shPh >= 1.f) {
				shPh -= std::floor(shPh);
				const float q = 1.f / 1024.f;
				shVal = std::round((wet + 0.6f * q * rng.bi()) / q) * q;
			}
			float cut = clampf(chipRate * 0.42f, 2200.f, 9500.f);
			lp1 += (shVal - lp1) * onePoleK(cut, dt);
			lp2 += (lp1 - lp2) * onePoleK(cut * 1.3f, dt);
			wet = lp2 + 0.002f * rng.bi() * std::min(1.f, dSmooth * 2.f);
		}
		// feedback loop: each pass loses lows, saturation lets it run away
		float x = wet;
		hp += (x - hp) * onePoleK(era80 ? 110.f : 70.f, dt);
		x -= hp;
		float loop = fb * x;
		if (era80) loop = std::tanh(1.4f * loop) / 1.4f;
		else loop = loop / (1.f + std::fabs(loop) * 0.9f) * 1.05f;
		buf[w] = in + loop;
		w = (w + 1) % (int) buf.size();
		return wet;
	}
};

// ---------------------------------------------------------------- the siren
struct Engine {
	float sr = 48000.f;
	Astable lfo, osc;
	float vcc = 0.f;
	float vc5 = 0.f, vA = 0.f;
	float hp1 = 0.f, hp1x = 0.f, hp2 = 0.f, hp2x = 0.f;
	Biquad dec1, dec2;
	bool wasSiren = false;
	float led = 0.f;      // ACTIVE LED (T5 follows node A)
	float sqOut = 0.f;    // LFO line, for the display
	float drift = 0.f;
	Rng rng;
	Echo echo;

	void setRate(float r) {
		sr = r;
		echo.setRate(r);
		float fs = r * OS, fc = std::min(18000.f, 0.42f * r);
		dec1.lowpass(fc, fs, 0.5412f);
		dec2.lowpass(fc, fs, 1.3066f);
	}

	// returns the output in volts (about +-5)
	float process(const Settings& s, float extIn, float dt) {
		float dto = dt / OS;
		float speedR = 1.f / clampf(s.speed, 0.05f, 20.f);
		float pitchScale = std::exp2(-clampf(s.voct, -5.f, 5.f));
		drift += (rng.bi() - drift) * tauK(2.f, dt);
		float rPitch = R_PITCH[s.pitch & 3] * pitchScale * (1.f + 0.003f * drift);
		bool sirenEdgeDown = !s.siren && wasSiren;
		wasSiren = s.siren;
		float y = 0.f;
		for (int k = 0; k < OS; k++) {
			// supply: regulator when TRIG is on, C10 holding up when it's off
			if (s.power) vcc += (VCC - vcc) * tauK(0.0015f, dto);
			else vcc += (0.f - vcc) * tauK(std::max(s.release, 0.005f), dto);

			// ---- LFO (S3). Side 1 = T2, whose collector is the LFO line.
			float lth[2], lr[2];
			lth[0] = lth[1] = vcc;
			if (s.beat < 3) { lr[0] = lr[1] = R_LFO[s.beat] * speedR; }
			else { lr[0] = 10e3f * speedR; lr[1] = 1e12f; lth[1] = 0.f; }   // OFF: T2 gets no base current
			if (s.siren || (k == 0 && (s.sync || sirenEdgeDown))) lfo.force(1, vcc);
			else lfo.step(lth, lr, vcc, 10e-6f, 1e3f, dto);
			// T2 on -> line at +V; off -> it falls as C4 recharges through R14
			float vsq = s.siren ? vcc : clampf(vcc - lfo.vc[1], 0.f, vcc);
			sqOut = vsq;

			// ---- shaper: node A and C5
			float g22 = 1.f / 1e3f;
			float gJ = s.beat < 3 ? 1.f / 2.2e3f : 1.f / 49.2e3f;
			float g25 = s.tone ? 1.f / 10e3f : 0.f;
			vA = (vcc * g25 + vc5 * g22) / (g25 + gJ + g22);
			if (vsq - VBE > vA) vA = vsq - VBE;               // D4 conducts
			vc5 += (vA - vc5) * tauK(1e3f * 100e-6f, dto);
			// the oscillator's base resistors also draw a little from C5
			int loads = s.mode == 0 ? 1 : (s.mode == 3 ? 2 : 0);
			if (loads) vc5 -= (vc5 + 1.f) * loads * dto / (rPitch * 100e-6f);
			vc5 = clampf(vc5, 0.f, VCC);

			// ---- S4: what feeds each half
			float src[2];
			float shaped = vc5, square = vsq;
			switch (s.mode) {
				case 0: src[0] = vcc; src[1] = shaped; break;
				case 1: src[0] = vcc; src[1] = square; break;
				case 2: src[0] = square; src[1] = square; break;
				default: src[0] = shaped; src[1] = shaped; break;
			}
			float d = clampf(s.depth, 0.f, 1.5f);
			float oth[2], orr[2];
			for (int i = 0; i < 2; i++) {
				float v = vcc - d * (vcc - src[i]);
				if (s.mode == 2) {
					// D1/D2 + R8/R9 220k from +V in parallel with the pitch resistor
					float rb = 220e3f, vb = std::max(vcc - VBE, 0.f);
					float g1 = 1.f / rPitch, g2 = 1.f / rb;
					oth[i] = (v * g1 + vb * g2) / (g1 + g2);
					orr[i] = 1.f / (g1 + g2);
				} else {
					oth[i] = v;
					orr[i] = rPitch;
				}
			}
			osc.step(oth, orr, vcc, 10e-9f, 4.7e3f, dto);

			// ---- output: T4 collector, decimation filter
			float c = osc.vc[1];
			y = dec2.process(dec1.process(c));
		}
		// C9/R26 and C8/R27 high-passes (~106 Hz and ~120 Hz loaded)
		float a1 = 1.f / (1.f + TWO_PI * 106.f * dt), a2 = 1.f / (1.f + TWO_PI * 120.f * dt);
		hp1 = a1 * (hp1 + y - hp1x); hp1x = y;
		hp2 = a2 * (hp2 + hp1 - hp2x); hp2x = hp1;
		float dry = hp2 * 0.62f;   // 9 V of collector swing -> about +-3.5 V

		// T5 + LED follow node A
		led = clampf((vA - 2.2f) / 5.f, 0.f, 1.f);

		// echo (FEEDBACK fully left = no echo at all)
		float wetLevel = clampf(s.feedback * 2.2f, 0.f, 1.f) * 0.9f;
		float in = (dry + extIn) * 0.2f;
		float wet = echo.process(in, s.time, s.feedback, s.era80, dt) * 5.f;
		float out = dry + wetLevel * wet + (wetLevel > 0.f ? 0.f : extIn);
		return 6.f * std::tanh(out / 6.f);
	}
};

}  // namespace dubsiren
