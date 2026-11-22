// Bore: the sound engines for Scanner and Bore Room. No Rack code here, so
// test/render.cpp can run everything offline.
//
// How a real MRI scanner makes its sound (see README for sources):
//   * Three gradient coils (X, Y, Z) carry pulsed current inside the magnet.
//     The Lorentz force makes them flex and knock.
//   * Sound = spectrum of the gradient waveform x the scanner's response.
//     So Scanner is a source (real pulse-sequence timing) into a filter
//     (each coil's resonances), then the tube you lie in, then your ears.
//   * Pitch comes from repetition: an EPI readout that flips every echo
//     spacing ES sounds at 1 / (2 x ES).
//
// Signal flow of Scanner:
//   SEQUENCE timing -> gradient waveforms gX gY gZ (also the GX GY GZ CV outs)
//   -> per coil: high-pass (weak below ~200 Hz) + tuned modes + frame thump
//   -> sum + RF clicks + PLANT (cold-head pump, hum, air, table motor)
//   -> BoreRoom (tube reverb) -> EARS (open / earplugs / headphones + BODY)
//
// Levels: internal audio +-1.0 = +-5 V.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace bore {

static const float PI_F = 3.14159265358979f;
static const float SPEED_OF_SOUND = 343.f;

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ------------------------------------------------------------------ basics
struct Rng {
	uint32_t s = 0x9E3779B9u;
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

// RBJ cookbook biquad, transposed direct form II
struct Biquad {
	float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
	float z1 = 0.f, z2 = 0.f;
	void reset() { z1 = z2 = 0.f; }
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void set(float nb0, float nb1, float nb2, float a0, float na1, float na2) {
		b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
	}
	static float w0(float f, float sr) { return 2.f * PI_F * clampf(f, 5.f, sr * 0.45f) / sr; }
	void lowpass(float f, float q, float sr) {
		float w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2.f * q);
		set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void highpass(float f, float q, float sr) {
		float w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2.f * q);
		set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	// band-pass, 0 dB at the peak
	void bandpass(float f, float q, float sr) {
		float w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2.f * q);
		set(al, 0.f, -al, 1 + al, -2 * c, 1 - al);
	}
	void peak(float f, float q, float db, float sr) {
		float A = std::pow(10.f, db / 40.f), w = w0(f, sr), c = std::cos(w), al = std::sin(w) / (2.f * q);
		set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
	}
	void lowshelf(float f, float db, float sr) {
		float A = std::pow(10.f, db / 40.f), w = w0(f, sr), c = std::cos(w), s = std::sin(w);
		float al = s / 2.f * std::sqrt(2.f), sq = 2.f * std::sqrt(A) * al;
		set(A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
		    (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
	}
};

// Delay line with linear interpolation; size fixed at init.
struct Delay {
	std::vector<float> buf;
	int w = 0;
	void init(int n) { buf.assign(std::max(4, n), 0.f); w = 0; }
	void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
	void write(float x) { buf[w] = x; w++; if (w >= (int) buf.size()) w = 0; }
	// d in samples, >= 1
	float read(float d) const {
		int n = (int) buf.size();
		d = clampf(d, 1.f, (float) (n - 2));
		float rp = (float) w - d;
		while (rp < 0.f) rp += (float) n;
		int i0 = (int) rp;
		float fr = rp - (float) i0;
		int i1 = i0 + 1; if (i1 >= n) i1 = 0;
		return buf[i0] + (buf[i1] - buf[i0]) * fr;
	}
};

// ================================================================= BORE ROOM
// The tube: a hard plastic cylinder open at both ends.
//   EARLY  two feedback combs per ear sized from the diameter: the fast
//          flutter between close walls (60 cm tube -> a reflection every 1.75 ms)
//   MODES  tuned resonators: cross modes f = a'mn * c / (pi * d) and length
//          modes n * c / (2 * (L + 0.61 d)), from the size
//   TAIL   a 4-line feedback delay network, short and dark (0.12 - 1.5 s)
//   ROOM   the scanner suite heard through the open ends (louder near the
//          opening)
//   EARS   open, earplugs or headphones; BODY adds the low end that reaches
//          you through the table and your skull (earplugs don't stop it)
struct BoreParams {
	float size = 0.5f;      // 0 head coil .. 0.5 standard 60 cm .. 1 wide bore 70 cm
	float decay = 0.45f;    // 0..1 -> 0.12 .. 1.5 s
	float damp = 0.4f;      // 0 hard plastic .. 1 padded
	float position = 0.f;   // 0 head at the centre .. 1 at the opening
	int ears = 0;           // 0 open, 1 earplugs, 2 headphones
	float body = 0.3f;      // bone conduction amount
};

inline void sizeToGeometry(float s, float& dia, float& len) {
	s = clampf(s, 0.f, 1.f);
	if (s < 0.5f) { float t = s / 0.5f; dia = lerpf(0.34f, 0.60f, t); len = lerpf(0.55f, 1.60f, t); }
	else { float t = (s - 0.5f) / 0.5f; dia = lerpf(0.60f, 0.70f, t); len = lerpf(1.60f, 1.25f, t); }
}
inline float decayToT60(float k) { return 0.12f * std::pow(12.5f, clampf(k, 0.f, 1.f)); }

static const int BORE_MODES = 10;
// zeros of J'm (cross modes of a rigid circular duct): (1,1) (2,1) (0,1) (3,1) (4,1) (1,2)
static const float CROSS_ALPHA[6] = {1.841f, 3.054f, 3.832f, 4.201f, 5.318f, 5.331f};

struct BoreRoom {
	float sr = 48000.f;
	Delay comb[2][2];
	float combLp[2][2] = {{0.f, 0.f}, {0.f, 0.f}};
	Biquad modes[2][BORE_MODES];
	float modeGain[BORE_MODES] = {};
	Delay fdn[4]; float fdnLp[4] = {0.f, 0.f, 0.f, 0.f};
	Delay room[4]; float roomLp[4] = {0.f, 0.f, 0.f, 0.f};
	Biquad ear[2][2];
	Biquad bodyLp[2][2];
	Biquad dc[2];
	float sizeS = -1.f;          // smoothed size
	float lastModeSize = -1.f, lastModeT60 = -1.f;
	int lastEars = -1;
	int ctl = 0;
	float dia = 0.6f, len = 1.6f;
	// tail settings recomputed at control rate
	float fdnD[4] = {}, fdnG[4] = {}, roomD[4] = {}, roomG[4] = {};
	float combD[2][2] = {}, combFb = 0.5f, lpA = 0.3f, earGain = 1.f;

	void init(float sampleRate) {
		sr = sampleRate;
		int maxEarly = (int) (sr * 0.006f) + 8;
		int maxFdn = (int) (sr * 0.030f) + 8;
		int maxRoom = (int) (sr * 0.060f) + 8;
		for (int c = 0; c < 2; c++) for (int k = 0; k < 2; k++) comb[c][k].init(maxEarly);
		for (int i = 0; i < 4; i++) { fdn[i].init(maxFdn); room[i].init(maxRoom); }
		for (int c = 0; c < 2; c++) {
			dc[c].highpass(18.f, 0.7f, sr);
			bodyLp[c][0].lowpass(220.f, 0.7f, sr);
			bodyLp[c][1].lowpass(220.f, 0.7f, sr);
		}
		lastModeSize = -1.f; lastEars = -1; sizeS = -1.f; ctl = 0;
		reset();
	}
	void reset() {
		for (int c = 0; c < 2; c++) {
			for (int k = 0; k < 2; k++) { comb[c][k].clear(); combLp[c][k] = 0.f; }
			for (int m = 0; m < BORE_MODES; m++) modes[c][m].reset();
			ear[c][0].reset(); ear[c][1].reset(); bodyLp[c][0].reset(); bodyLp[c][1].reset(); dc[c].reset();
		}
		for (int i = 0; i < 4; i++) { fdn[i].clear(); room[i].clear(); fdnLp[i] = roomLp[i] = 0.f; }
	}

	// mode frequencies for the current geometry (also used by the display/tests)
	void modeFreqs(float d, float L, float* f) const {
		for (int i = 0; i < 6; i++) f[i] = CROSS_ALPHA[i] * SPEED_OF_SOUND / (PI_F * d);
		float Le = L + 0.61f * d;
		for (int n = 0; n < 4; n++) f[6 + n] = (n + 1) * SPEED_OF_SOUND / (2.f * Le);
	}

	void update(const BoreParams& p) {
		if (sizeS < 0.f) sizeS = p.size;
		sizeS += (p.size - sizeS) * 0.08f;          // ~ 20 ms glide at control rate
		sizeToGeometry(sizeS, dia, len);
		float t60 = decayToT60(p.decay);
		float tw = dia / SPEED_OF_SOUND;              // across the tube, seconds
		float tl = len / SPEED_OF_SOUND;              // along the tube
		// flutter: listener a little off-centre, so two wall distances per ear
		combD[0][0] = tw * sr; combD[0][1] = tw * 0.86f * sr;
		combD[1][0] = tw * 0.93f * sr; combD[1][1] = tw * 0.79f * sr;
		float dn = clampf((t60 - 0.12f) / 1.38f, 0.f, 1.f);
		combFb = (0.42f + 0.42f * std::sqrt(dn)) * (1.f - 0.25f * p.damp);
		float fc = 1200.f * std::pow(7.5f, 1.f - clampf(p.damp, 0.f, 1.f));
		lpA = 1.f - std::exp(-2.f * PI_F * fc / sr);
		const float kw[4] = {1.7f, 2.3f, 2.9f, 3.7f}, kl[4] = {0.6f, 0.9f, 0.5f, 1.1f};
		for (int i = 0; i < 4; i++) {
			float d = (kw[i] * tw + kl[i] * tl);
			fdnD[i] = d * sr;
			fdnG[i] = std::pow(10.f, -3.f * d / t60);
		}
		const float rd[4] = {0.0231f, 0.0313f, 0.0373f, 0.0433f};
		for (int i = 0; i < 4; i++) { roomD[i] = rd[i] * sr; roomG[i] = std::pow(10.f, -3.f * rd[i] / 0.65f); }
		if (std::fabs(sizeS - lastModeSize) > 0.002f || std::fabs(t60 - lastModeT60) > 0.01f) {
			lastModeSize = sizeS; lastModeT60 = t60;
			float f[BORE_MODES];
			modeFreqs(dia, len, f);
			for (int m = 0; m < BORE_MODES; m++) {
				float tm = t60 * (m < 6 ? 0.7f : 0.5f);
				float q = clampf(PI_F * f[m] * tm / 6.91f, 2.f, 45.f);
				modes[0][m].bandpass(f[m], q, sr);
				modes[1][m].bandpass(f[m] * 1.013f, q, sr);
				// cross modes carry most energy at the head (7 T measurements, 500-700 Hz band)
				modeGain[m] = (m < 6 ? 0.4f : 0.3f) * std::sqrt(q / 10.f);
			}
		}
		if (p.ears != lastEars) {
			lastEars = p.ears;
			for (int c = 0; c < 2; c++) {
				if (p.ears == 1) { ear[c][0].lowpass(700.f, 0.6f, sr); ear[c][1].lowpass(1400.f, 0.7f, sr); earGain = 1.6f; }
				else if (p.ears == 2) { ear[c][0].lowpass(1700.f, 0.8f, sr); ear[c][1].lowshelf(300.f, 4.f, sr); earGain = 1.35f; }
				else { ear[c][0] = Biquad(); ear[c][1] = Biquad(); earGain = 1.f; }
				ear[c][0].reset(); ear[c][1].reset();
			}
		}
	}

	// dryAmt: how much direct sound (Bore Room's MIX handles dry outside)
	void process(float inL, float inR, const BoreParams& p, float& outL, float& outR) {
		if (ctl <= 0) { update(p); ctl = 32; }
		ctl--;
		float pos = clampf(p.position, 0.f, 1.f);
		float in[2] = {inL, inR};
		float early[2], md[2];
		for (int c = 0; c < 2; c++) {
			float e = 0.f;
			for (int k = 0; k < 2; k++) {
				float y = comb[c][k].read(combD[c][k]);
				combLp[c][k] += (y - combLp[c][k]) * lpA;
				comb[c][k].write(in[c] + combFb * combLp[c][k]);
				e += y;
			}
			early[c] = e * 0.5f;
			float m = 0.f;
			for (int i = 0; i < BORE_MODES; i++) m += modes[c][i].process(in[c]) * modeGain[i];
			md[c] = m;
		}
		// tail FDN (Hadamard), fed by the flutter and a little direct
		float tin = 0.5f * (early[0] + early[1]) + 0.15f * (in[0] + in[1]);
		float o[4];
		for (int i = 0; i < 4; i++) {
			float y = fdn[i].read(fdnD[i]);
			fdnLp[i] += (y - fdnLp[i]) * lpA;
			o[i] = fdnLp[i] * fdnG[i];
		}
		float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]);
		float h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
		float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]);
		float h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);
		fdn[0].write(tin + h0); fdn[1].write(tin + h1); fdn[2].write(tin + h2); fdn[3].write(tin + h3);
		float tailL = o[0] + o[2], tailR = o[1] + o[3];
		// the suite outside, reached through the open ends
		float rin = 0.25f * (tailL + tailR) + 0.1f * (in[0] + in[1]);
		float r[4];
		for (int i = 0; i < 4; i++) {
			float y = room[i].read(roomD[i]);
			roomLp[i] += (y - roomLp[i]) * 0.25f;
			r[i] = roomLp[i] * roomG[i];
		}
		room[0].write(rin + 0.5f * (r[0] + r[1] + r[2] + r[3]));
		room[1].write(rin + 0.5f * (r[0] - r[1] + r[2] - r[3]));
		room[2].write(-rin + 0.5f * (r[0] + r[1] - r[2] - r[3]));
		room[3].write(0.5f * (r[0] - r[1] - r[2] + r[3]));
		float roomL = r[0] + r[2], roomR = r[1] + r[3];

		float gDir = 1.f - 0.45f * pos, gEarly = 0.9f * (1.f - 0.6f * pos), gMode = 1.f - 0.5f * pos;
		float gTail = 0.7f * (1.f - 0.4f * pos), gRoom = 0.12f + 0.55f * pos;
		float tails[2] = {tailL, tailR}, rooms[2] = {roomL, roomR};
		float outs[2];
		for (int c = 0; c < 2; c++) {
			float w = in[c] * gDir + early[c] * gEarly + md[c] * gMode + tails[c] * gTail + rooms[c] * gRoom;
			w = dc[c].process(w);
			float e = ear[c][1].process(ear[c][0].process(w)) * earGain;
			float b = bodyLp[c][1].process(bodyLp[c][0].process(w));
			e += std::tanh(2.f * b) * 0.6f * clampf(p.body, 0.f, 1.f);
			outs[c] = e;
		}
		outL = outs[0]; outR = outs[1];
	}
};

// ================================================================= SCANNER
enum Mode {
	PRESCAN = 0, LOCALISER, T1_SE, T2_TSE, FLAIR, GRE, EPI, DIFFUSION, TOF, SILENT,
	NUM_MODES,
	// session-only states
	TABLE_IN = 100, TABLE_OUT, PAUSE
};
static const char* const MODE_NAMES[NUM_MODES] = {
	"PRESCAN", "LOCALISER", "T1 SPIN ECHO", "T2 TURBO SE", "FLAIR", "GRADIENT ECHO", "EPI", "DIFFUSION", "ANGIO (TOF)", "SILENT"};
static const char* const MODE_SHORT[NUM_MODES] = {"PRE", "LOC", "T1", "T2", "FLR", "GRE", "EPI", "DWI", "TOF", "SIL"};
// default TR in seconds (the TR knob scales this by 1/4 .. 4)
static const float MODE_TR[NUM_MODES] = {1.2f, 0.6f, 0.5f, 3.0f, 4.0f, 0.2f, 2.0f, 3.5f, 0.5f, 1.0f};

enum PulseKind { TRAP = 0, TRAIN, SWEEP };
// One gradient event on one axis. Times in seconds from the start of the unit.
struct Pulse {
	int kind, axis;          // axis 0 X, 1 Y, 2 Z, 3 RF (clicks)
	float t0, amp, ramp, flat;
	int count;               // TRAIN: lobes
	float period;            // TRAIN: lobe spacing
	bool alt;                // TRAIN: alternate sign
	float f0, f1;            // SWEEP: start / end frequency (flat = length)
	float end;               // t0 + total length
};

struct ScanControls {
	int mode = EPI;
	float trMul = 1.f;       // TR knob, 0.25 .. 4
	float timeMul = 1.f;     // SPACING + V/OCT: >1 slower / lower
	int slices = 8;
	float ramp = 0.f;        // 0 hard .. 1 soft
	float field = 0.5f;      // 0 1.5 T, 0.5 3 T, 1 7 T
	float human = 0.f;
	float drift = 0.f;
	float plant = 0.4f;
	float lx = 1.f, ly = 1.f, lz = 1.f;
	float session = 0.f;     // 0 off, else 5 .. 40 min
	bool clocked = false;
	BoreParams bore;
};

// One coil: the scanner's response for one gradient axis.
struct Coil {
	Biquad hp1, hp2, frame, presence;
	Biquad res[5];
	float gain[5] = {};
	float prevField = -1.f;
	void setup(int axis, float field, float sr) {
		static const float F[3][5] = {{430.f, 950.f, 1600.f, 2450.f, 3300.f},
		                              {470.f, 1010.f, 1720.f, 2600.f, 3500.f},
		                              {1170.f, 1310.f, 2100.f, 2900.f, 640.f}};
		static const float Q[3][5] = {{12.f, 14.f, 10.f, 9.f, 8.f}, {12.f, 14.f, 10.f, 9.f, 8.f}, {14.f, 16.f, 10.f, 9.f, 8.f}};
		static const float G[3][5] = {{1.f, 0.85f, 0.75f, 0.6f, 0.45f}, {1.f, 0.85f, 0.75f, 0.6f, 0.4f}, {1.f, 0.9f, 0.75f, 0.55f, 0.4f}};
		float shift = 1.f + 0.08f * (field - 0.5f);
		for (int i = 0; i < 5; i++) {
			res[i].bandpass(F[axis][i] * shift, Q[axis][i], sr);
			gain[i] = G[axis][i] * 2.6f;
		}
		hp1.highpass(190.f, 0.6f, sr);
		hp2.highpass(140.f, 0.6f, sr);
		frame.bandpass(axis == 2 ? 110.f : 92.f, 2.5f, sr);
		// 'relatively flat above 700 Hz with many peaks': lift the top of the direct path
		presence.peak(axis == 2 ? 2900.f : 2500.f, 0.8f, 6.f, sr);
		prevField = field;
	}
	float process(float g, float bright) {
		float x = hp2.process(hp1.process(g));
		float y = presence.process(x) * (0.7f + 0.6f * bright);
		for (int i = 0; i < 5; i++) y += res[i].process(x) * gain[i];
		y += frame.process(g) * 0.9f;
		return y;
	}
	void reset() { hp1.reset(); hp2.reset(); frame.reset(); for (int i = 0; i < 5; i++) res[i].reset(); }
};

// The machine room: cold-head pump, compressor hum, air, and the table motor.
struct Plant {
	float sr = 48000.f;
	Rng rng;
	double t = 0.0;
	float period = 1.05f;
	float chirpEnv = 0.f, thumpEnv = 0.f, chirpPh = 0.f, thumpPh = 0.f, chirpT = 1.f;
	Biquad chirpBp, airLp, thumpBp;
	float humPh = 0.f;
	bool pumpTrig = false;
	bool fired2 = false;
	// table motor
	float motorEnv = 0.f, motorTarget = 0.f, motorPh = 0.f, motorF = 70.f, motorDir = 1.f;
	float clunk = 0.f;
	Biquad motorLp;
	float kChirp = 0.f, kThump = 0.f, kClunk = 0.f;
	void init(float s) {
		sr = s;
		kChirp = std::exp(-1.f / (sr * 0.035f)); kThump = std::exp(-1.f / (sr * 0.06f)); kClunk = std::exp(-1.f / (sr * 0.04f));
		airLp.lowpass(320.f, 0.7f, sr);
		thumpBp.bandpass(260.f, 1.5f, sr);
		motorLp.lowpass(900.f, 0.7f, sr);
		chirpBp.bandpass(1800.f, 4.f, sr);
	}
	void motor(bool on, float dir) {
		if (on && motorTarget == 0.f) { clunk = 1.f; motorF = dir > 0 ? 55.f : 70.f; }
		if (!on && motorTarget > 0.f) clunk = 0.8f;
		motorTarget = on ? 1.f : 0.f; motorDir = dir;
	}
	float process() {
		pumpTrig = false;
		float dt = 1.f / sr;
		t += dt;
		float ph = (float) (t / period);
		if (ph >= 1.f) { t -= period; ph -= 1.f; chirpEnv = 1.f; chirpT = 0.f; pumpTrig = true; fired2 = false; period = 1.05f + 0.03f * rng.bi(); }
		if (!fired2 && ph >= 0.48f) { fired2 = true; thumpEnv = 1.f; }
		// chirp: expansion valve, a falling squeak with breath
		chirpT += dt;
		float cf = 1700.f * (1.f - 0.55f * std::min(chirpT / 0.09f, 1.f));
		if (chirpEnv > 0.001f && ((int) (chirpT * sr)) % 32 == 0) chirpBp.bandpass(cf * 1.25f, 5.f, sr);
		chirpPh += cf * dt; if (chirpPh > 1.f) chirpPh -= 1.f;
		float chirp = chirpEnv > 0.0005f ? (0.5f * std::sin(2.f * PI_F * chirpPh) + 0.9f * chirpBp.process(rng.bi())) * chirpEnv : 0.f;
		chirpEnv *= kChirp;
		// thump: compression stroke
		thumpPh += 52.f * dt; if (thumpPh > 1.f) thumpPh -= 1.f;
		float thump = thumpEnv > 0.0005f ? (std::sin(2.f * PI_F * thumpPh) + 0.6f * thumpBp.process(rng.bi())) * thumpEnv : 0.f;
		thumpEnv *= kThump;
		// mains hum (50 Hz, UK) and air handling
		humPh += 50.f * dt; if (humPh > 1.f) humPh -= 1.f;
		float hum = 0.05f * std::sin(2.f * PI_F * humPh) + 0.03f * std::sin(4.f * PI_F * humPh) + 0.015f * std::sin(6.f * PI_F * humPh);
		float air = airLp.process(rng.bi()) * 0.35f;
		// table motor: geared whine that rises then settles, clunks at start and stop
		motorEnv += (motorTarget - motorEnv) * (dt / 0.25f);
		motorF += ((motorTarget > 0.f ? 118.f : 60.f) - motorF) * (dt / 1.2f);
		motorPh += motorF * dt; if (motorPh > 1.f) motorPh -= 1.f;
		float saw = 2.f * motorPh - 1.f;
		float mot = motorEnv > 0.0005f ? motorLp.process(saw + 0.3f * std::sin(2.f * PI_F * motorPh * 7.f)) * motorEnv * 0.25f : 0.f;
		float cl = clunk > 0.0005f ? clunk * (std::sin(2.f * PI_F * 70.f * (float) t) + 0.5f * rng.bi()) : 0.f;
		clunk *= kClunk;
		return 0.32f * chirp + 0.5f * thump + hum + air + mot + 0.6f * cl;
	}
};

struct SessionStep { int mode; float share; };   // share of the session length (seconds if < 0)

struct ScannerEngine {
	float sr = 48000.f;
	Rng rng;
	std::vector<Pulse> pulses;
	Coil coil[3];
	Biquad rfHp;
	Plant plant;
	BoreRoom bore;

	// scheduler
	enum State { UNIT, DEAD, WAIT, PAUSED };
	int state = DEAD;
	int curMode = EPI;
	double unitT = 0.0, trT = 0.0;
	float unitDur = 0.f, trLen = 1.f;
	int unitsDone = 0, unitsThisTR = 1;
	bool pendingClock = false;
	bool inversionPending = false;
	int line = 0, trCount = 0, plane = 0, dirIndex = 0;
	float kyTR = 0.f;
	float dirX = 1.f, dirY = 0.f, dirZ = 0.f;
	float driftPitch = 1.f, driftAmp[3] = {1.f, 1.f, 1.f};
	float sweepPh = 0.f;
	float rfPrev = 0.f;
	// session
	int sessStep = -1;
	double sessT = 0.0, stepT = 0.0, pauseLeft = 0.0;
	float stepLen = 0.f;
	bool sessionOn = false;
	int sessionMode = EPI;
	// outputs
	float g[3] = {0.f, 0.f, 0.f};
	float coilOut[3] = {0.f, 0.f, 0.f};
	float outL = 0.f, outR = 0.f;
	bool trigTR = false, trigSlice = false, trigPump = false, trigEOS = false;
	bool scanning = false;
	float fieldNow = -1.f;
	float fieldGain = 0.9f, fieldGainFor = -1.f;

	void init(float s) {
		sr = s;
		for (int a = 0; a < 3; a++) coil[a].setup(a, 0.5f, sr);
		rfHp.highpass(3000.f, 0.7f, sr);
		plant.init(sr);
		bore.init(sr);
		pulses.reserve(512);
		fieldNow = 0.5f;
		restart();
	}
	void restart() {
		state = DEAD; trT = 1e9; unitT = 0.0; pulses.clear(); pendingClock = false;
		line = 0; trCount = 0; plane = 0; dirIndex = 0;
		sessStep = -1; sessT = 0.0; stepT = 0.0; pauseLeft = 0.0;
	}

	// ------------------------------------------------------------ pulse building
	void addTrap(int axis, float t0, float amp, float r, float f) {
		Pulse p;
		p.kind = TRAP; p.axis = axis; p.t0 = t0; p.amp = amp; p.ramp = std::max(r, 1e-5f); p.flat = std::max(f, 0.f);
		p.count = 1; p.period = 0.f; p.alt = false; p.f0 = p.f1 = 0.f;
		p.end = t0 + 2.f * p.ramp + p.flat;
		pulses.push_back(p);
	}
	void addTrain(int axis, float t0, float amp, float r, float period, int count, bool alt) {
		Pulse p;
		p.kind = TRAIN; p.axis = axis; p.t0 = t0; p.amp = amp; p.period = period; p.count = count; p.alt = alt;
		p.ramp = std::min(std::max(r, 1e-5f), period * 0.45f);
		p.flat = std::max(0.f, (alt ? period : period * 0.25f) - 2.f * p.ramp);
		p.f0 = p.f1 = 0.f;
		p.end = t0 + period * count;
		pulses.push_back(p);
	}
	void addSweep(int axis, float t0, float amp, float f0, float f1, float len) {
		Pulse p;
		p.kind = SWEEP; p.axis = axis; p.t0 = t0; p.amp = amp; p.f0 = f0; p.f1 = f1; p.flat = len;
		p.ramp = 0.f; p.count = 1; p.period = 0.f; p.alt = false;
		p.end = t0 + len;
		pulses.push_back(p);
	}
	void rf(float t) { addTrap(3, t, 1.f, 0.00005f, 0.0001f); }

	float rampShape(float u, float soft) const {
		u = clampf(u, 0.f, 1.f);
		return lerpf(u, 0.5f - 0.5f * std::cos(PI_F * u), soft);
	}
	float trapValue(float t, float r, float f, float soft) const {
		if (t < 0.f) return 0.f;
		if (t < r) return rampShape(t / r, soft);
		if (t < r + f) return 1.f;
		float tt = 2.f * r + f - t;
		if (tt <= 0.f) return 0.f;
		return rampShape(tt / r, soft);
	}
	float pulseValue(const Pulse& p, float t, float soft) const {
		float lt = t - p.t0;
		if (lt < 0.f || t >= p.end) return 0.f;
		if (p.kind == TRAP) return p.amp * trapValue(lt, p.ramp, p.flat, soft);
		if (p.kind == TRAIN) {
			int k = (int) (lt / p.period);
			if (k >= p.count) return 0.f;
			float l = lt - k * p.period;
			float s = (p.alt && (k & 1)) ? -1.f : 1.f;
			return s * p.amp * trapValue(l, p.ramp, p.flat, soft);
		}
		// SWEEP: exponential chirp with soft ends
		float d = p.flat, u = lt / d;
		float ph;
		if (std::fabs(p.f1 - p.f0) < 1e-3f) ph = 2.f * PI_F * p.f0 * lt;
		else {
			float k = std::log(p.f1 / p.f0);
			ph = 2.f * PI_F * p.f0 * d / k * (std::exp(k * u) - 1.f);
		}
		float w = std::min(1.f, std::min(u, 1.f - u) * 12.f);
		return p.amp * std::sin(ph) * w;
	}

	float ky() {
		float v = ((line % 64) / 63.f) * 2.f - 1.f;
		line++;
		if (rng.uni() < driftNow * 0.6f) v = rng.bi();
		return v;
	}
	float driftNow = 0.f, rampAdd = 0.f, softNow = 0.f;

	// axis roles for the localiser planes: slice, phase, read
	void roles(int pl, int& S, int& P, int& R) const {
		if (pl == 0) { S = 2; P = 1; R = 0; }
		else if (pl == 1) { S = 1; P = 2; R = 0; }
		else { S = 0; P = 1; R = 2; }
	}

	// Build one unit (a slice, or a burst of lines) of the given sequence.
	// Returns its length in seconds.
	float build(int mode, float m) {
		pulses.clear();
		float R = rampAdd;
		float t = 0.f;
		int S = 2, P = 1, Rd = 0;
		switch (mode) {
		case PRESCAN: {
			// shimming and frequency calibration: warbling sweeps and held test tones
			int ax = rng.next() % 3;
			float len = 0.3f * m + 0.1f * rng.uni();
			float f0 = (180.f + 420.f * rng.uni()) / m;
			bool held = rng.uni() < 0.35f;
			float f1 = held ? f0 : f0 * (rng.uni() < 0.5f ? 2.2f : 0.45f);
			addTrap(2, 0.f, 0.5f, 0.0002f + R, 0.0008f);
			addSweep(ax, 0.004f + R, 0.28f, f0, f1, len);
			if (rng.uni() < 0.5f) addSweep((ax + 1) % 3, 0.004f + len * 0.5f, 0.18f, f1 * 1.5f, f0 * 1.5f, len * 0.5f);
			return len + 0.06f + 2.f * R;
		}
		case LOCALISER: {
			roles(plane, S, P, Rd);
			for (int l = 0; l < 8; l++) {
				float t0 = t;
				float r = 0.00015f + R;
				addTrap(S, t0, 0.35f, r, 0.001f); rf(t0 + r + 0.0005f);
				float t1 = t0 + 2 * r + 0.001f;
				addTrap(P, t1, 0.6f * ky(), r, 0.0005f);
				addTrap(Rd, t1, -0.5f, r, 0.0005f);
				float t2 = t1 + 2 * r + 0.0005f;
				addTrap(Rd, t2, 0.5f, r, 0.0025f * m);
				float t3 = t2 + 2 * r + 0.0025f * m;
				addTrap(S, t3, 0.7f, r, 0.0008f);
				float end = t3 + 2 * r + 0.0008f + 0.0003f;
				t = t0 + std::max(0.008f * m, end - t0);
			}
			return t;
		}
		case T1_SE: {
			float r = 0.0002f + R;
			addTrap(S, 0.f, 0.35f, r, 0.0025f); rf(r + 0.00125f);
			t = 2 * r + 0.0025f;
			addTrap(Rd, t, 0.5f, r, 0.0015f);
			addTrap(P, t, 0.7f * kyTR, r, 0.0015f);
			t += 2 * r + 0.0015f;
			float rc = 0.00025f + R;
			addTrap(S, t, 0.9f, rc, 0.001f); t += 2 * rc + 0.001f;
			addTrap(S, t, 0.35f, r, 0.003f); rf(t + r + 0.0015f); t += 2 * r + 0.003f;
			addTrap(S, t, 0.9f, rc, 0.001f); t += 2 * rc + 0.001f;
			addTrap(Rd, t, 0.5f, r, 0.005f * m); t += 2 * r + 0.005f * m;
			float rs = 0.0003f + R;
			addTrap(S, t, 0.9f, rs, 0.0015f); t += 2 * rs + 0.0015f;
			return std::max(t + 0.001f, 0.018f * m);
		}
		case T2_TSE:
		case FLAIR: {
			if (mode == FLAIR && inversionPending) {
				// inversion: a long adiabatic pulse and a big crusher, then the wait (TI)
				inversionPending = false;
				float r = 0.0004f + R;
				addTrap(S, 0.f, 0.3f, r, 0.010f); rf(0.005f);
				addTrap(S, 2 * r + 0.010f, 1.f, r, 0.004f);
				addTrap(Rd, 2 * r + 0.010f, 0.6f, r, 0.004f);
				return std::min(2.2f, 0.3f * trLen);
			}
			float r = 0.0002f + R, rc = 0.00025f + R;
			addTrap(S, 0.f, 0.35f, r, 0.0025f); rf(r + 0.00125f);
			addTrap(Rd, 2 * r + 0.0025f, 0.5f, r, 0.0012f);
			float es = 0.009f * m;
			float start = 2 * r + 0.0025f + 2 * r + 0.0012f;
			int etl = 12;
			for (int e = 0; e < etl; e++) {
				float t0 = start + e * es;
				float k = 0.6f * ky();
				float a = t0;
				addTrap(S, a, 0.8f, rc, 0.0008f); a += 2 * rc + 0.0008f;
				addTrap(S, a, 0.35f, r, 0.002f); rf(a + r + 0.001f); a += 2 * r + 0.002f;
				addTrap(S, a, 0.8f, rc, 0.0008f);
				addTrap(P, a, k, rc, 0.0008f); a += 2 * rc + 0.0008f;
				addTrap(Rd, a, 0.5f, r, 0.0025f * m); a += 2 * r + 0.0025f * m;
				addTrap(P, a, -k, rc, 0.0006f);
				float need = a + 2 * rc + 0.0006f - t0;
				if (need > es) es = need;
			}
			return start + etl * es + 0.004f;
		}
		case GRE: {
			for (int l = 0; l < 16; l++) {
				float t0 = t, r = 0.00012f + R;
				addTrap(S, t0, 0.4f, r, 0.0008f); rf(t0 + r + 0.0004f);
				float t1 = t0 + 2 * r + 0.0008f;
				addTrap(P, t1, 0.6f * ky(), r, 0.0005f);
				addTrap(Rd, t1, -0.6f, r, 0.0005f);
				float t2 = t1 + 2 * r + 0.0005f;
				addTrap(Rd, t2, 0.6f, r, 0.0016f * m);
				float t3 = t2 + 2 * r + 0.0016f * m;
				addTrap(S, t3, 0.8f, r, 0.0005f);
				addTrap(Rd, t3, 0.8f, r, 0.0005f);
				float end = t3 + 2 * r + 0.0005f + 0.0002f;
				t = t0 + std::max(0.005f * m, end - t0);
			}
			return t;
		}
		case EPI:
		case DIFFUSION: {
			// fat saturation: all three axes kick, then excitation
			float r = 0.0002f + R;
			addTrap(0, 0.f, 0.7f, r, 0.001f); addTrap(1, 0.f, 0.7f, r, 0.001f); addTrap(2, 0.f, 0.7f, r, 0.001f);
			t = 2 * r + 0.0012f;
			addTrap(S, t, 0.4f, r, 0.0026f); rf(t + r + 0.0013f);
			t += 2 * r + 0.0026f;
			if (mode == DIFFUSION) {
				// two big diffusion lobes around the refocusing pulse; direction changes each TR
				bool b0 = (trCount % 12) == 0;
				float a = b0 ? 0.f : 0.95f;
				float rd = 0.0008f + R, fd = 0.016f * std::sqrt(m);
				addTrap(0, t, a * dirX, rd, fd); addTrap(1, t, a * dirY, rd, fd); addTrap(2, t, a * dirZ, rd, fd);
				t += 2 * rd + fd;
				float rc = 0.00025f + R;
				addTrap(S, t, 0.9f, rc, 0.001f); t += 2 * rc + 0.001f;
				addTrap(S, t, 0.35f, r, 0.003f); rf(t + r + 0.0015f); t += 2 * r + 0.003f;
				addTrap(S, t, 0.9f, rc, 0.001f); t += 2 * rc + 0.001f;
				addTrap(0, t, a * dirX, rd, fd); addTrap(1, t, a * dirY, rd, fd); addTrap(2, t, a * dirZ, rd, fd);
				t += 2 * rd + fd;
			}
			addTrap(P, t, -0.5f, r, 0.0006f);
			addTrap(Rd, t, -0.5f, r, 0.0006f);
			t += 2 * r + 0.0006f;
			// the readout: an alternating train; pitch = 1 / (2 x ES). Base C5 (EPI) / A4 (diffusion)
			float base = mode == EPI ? 1.f / (2.f * 523.25f) : 1.f / (2.f * 440.f);
			float es = base * m;
			int lobes = 48;
			addTrain(Rd, t, 0.6f, 0.00008f + R, es, lobes, true);
			addTrain(P, t + es - 0.00006f, 0.12f, 0.00005f + R * 0.5f, es, lobes - 1, false);
			t += es * lobes;
			addTrap(S, t, 0.8f, r, 0.0012f); addTrap(Rd, t, 0.8f, r, 0.0012f);
			t += 2 * r + 0.0012f;
			return t + 0.002f;
		}
		case TOF: {
			for (int l = 0; l < 16; l++) {
				float t0 = t, r = 0.00015f + R;
				addTrap(S, t0, 0.45f, r, 0.001f); rf(t0 + r + 0.0005f);
				float t1 = t0 + 2 * r + 0.001f;
				addTrap(S, t1, -0.5f, r, 0.0006f);
				addTrap(P, t1, 0.6f * ky(), r, 0.0006f);
				addTrap(Rd, t1, -0.5f, r, 0.0006f);
				float t2 = t1 + 2 * r + 0.0006f;
				addTrap(S, t2, 0.5f, r, 0.0006f);
				addTrap(Rd, t2, 0.5f, r, 0.005f * m);
				float t3 = t2 + 2 * r + 0.005f * m;
				addTrap(Rd, t3, 0.9f, r, 0.003f);
				addTrap(S, t3, 0.6f, r, 0.002f);
				float end = t3 + 2 * r + 0.003f + 0.0003f;
				t = t0 + std::max(0.022f * m, end - t0);
			}
			return t;
		}
		case SILENT:
		default: {
			for (int l = 0; l < 12; l++) {
				float t0 = t, r = 0.0025f + R;
				addTrap(S, t0, 0.22f, r, 0.001f); rf(t0 + r + 0.0005f);
				float t1 = t0 + 2 * r + 0.001f;
				addTrap(P, t1, 0.25f * ky(), r, 0.001f);
				addTrap(Rd, t1 + 2 * r + 0.001f, 0.25f, r, 0.004f * m);
				float end = t1 + 4 * r + 0.001f + 0.004f * m + 0.0005f;
				t = t0 + std::max(0.014f * m, end - t0);
			}
			return t;
		}
		}
	}

	int unitsFor(int mode, int slices) const {
		if (mode == PRESCAN) return std::max(1, slices / 6);
		if (mode == LOCALISER) return std::max(1, slices / 2);
		return std::max(1, slices);
	}

	// golden-spiral directions on a hemisphere (30 of them), like a DTI scheme
	void setDirection(int i) {
		float z = 1.f - (i % 30 + 0.5f) / 30.f;
		float rr = std::sqrt(std::max(0.f, 1.f - z * z));
		float ph = 2.39996323f * (float) i;
		dirX = rr * std::cos(ph); dirY = rr * std::sin(ph); dirZ = z;
	}

	const ScanControls* cc = nullptr;
	size_t firstLive = 0;
	static bool pulseEarlier(const Pulse& a, const Pulse& b) { return a.t0 < b.t0; }
	int effectiveMode() const { return sessionOn ? sessionMode : cc->mode; }

	void startTR() {
		const ScanControls& c = *cc;
		curMode = effectiveMode();
		if (curMode < 0 || curMode >= NUM_MODES) { state = PAUSED; return; }
		trT = 0.0; unitsDone = 0;
		trCount++;
		unitsThisTR = unitsFor(curMode, c.slices);
		float tr = MODE_TR[curMode] * c.trMul;
		if (c.human > 0.f) {
			// heart gating: wait for the next beat, beats wander
			float rrInt = 0.85f + 0.15f * rng.bi();
			tr = tr * (1.f + 0.2f * c.human * rng.bi()) + c.human * rng.uni() * rrInt * 0.5f;
		}
		trLen = std::max(0.02f, tr);
		kyTR = ((trCount % 64) / 63.f) * 2.f - 1.f;
		if (rng.uni() < c.drift * 0.5f) kyTR = rng.bi();
		if (curMode == LOCALISER) plane = trCount % 3;
		if (curMode == DIFFUSION) {
			if (c.drift > 0.f && rng.uni() < c.drift) dirIndex = rng.next() % 30;
			else dirIndex++;
			setDirection(dirIndex);
		}
		inversionPending = curMode == FLAIR;
		trigTR = true;
		startUnit();
	}
	void startUnit() {
		const ScanControls& c = *cc;
		// DRIFT: the timing and the axis balance wander a little each unit
		float dr = c.drift;
		driftPitch += (1.f + 0.06f * dr * rng.bi() - driftPitch) * 0.3f;
		for (int a = 0; a < 3; a++) driftAmp[a] += (1.f + 0.35f * dr * rng.bi() - driftAmp[a]) * 0.2f;
		driftNow = dr;
		rampAdd = (curMode == SILENT ? 0.6f + 0.4f * c.ramp : c.ramp) * 0.003f;
		softNow = curMode == SILENT ? 1.f : clampf(c.ramp * 1.6f, 0.f, 1.f);
		unitDur = build(curMode, c.timeMul * driftPitch);
		std::sort(pulses.begin(), pulses.end(), pulseEarlier);
		firstLive = 0;
		unitT = 0.0;
		state = UNIT;
		trigSlice = true;
	}

	// session: a whole examination, in order
	static int sessionLength() { return 15; }
	SessionStep sessionStep(int i) const {
		static const int M[15] = {TABLE_IN, PRESCAN, LOCALISER, PRESCAN, T1_SE, PRESCAN, T2_TSE, FLAIR, PRESCAN, EPI, DIFFUSION, PRESCAN, TOF, TABLE_OUT, PAUSE};
		static const float S[15] = {-9.f, -7.f, -18.f, -5.f, 0.13f, -4.f, 0.17f, 0.15f, -5.f, 0.20f, 0.20f, -4.f, 0.11f, -9.f, -20.f};
		SessionStep st;
		st.mode = M[i % 15]; st.share = S[i % 15];
		return st;
	}
	void sessionAdvance(float minutes) {
		sessStep++;
		if (sessStep >= sessionLength()) sessStep = 0;
		SessionStep st = sessionStep(sessStep);
		stepLen = st.share < 0.f ? -st.share : st.share * minutes * 60.f;
		stepT = 0.0;
		sessionMode = st.mode;
		plant.motor(st.mode == TABLE_IN || st.mode == TABLE_OUT, st.mode == TABLE_IN ? 1.f : -1.f);
		if (st.mode >= NUM_MODES) { state = PAUSED; pulses.clear(); }
		else startTR();
	}
	const char* stepName() const {
		if (!sessionOn) return "";
		if (sessionMode == TABLE_IN) return "TABLE IN";
		if (sessionMode == TABLE_OUT) return "TABLE OUT";
		if (sessionMode == PAUSE) return "BREAK";
		if (pauseLeft > 0.0) return "WAIT";
		return MODE_NAMES[clampi(sessionMode)];
	}
	static int clampi(int m) { return m < 0 ? 0 : (m >= NUM_MODES ? NUM_MODES - 1 : m); }

	void process(const ScanControls& c, float audioIn, bool clockEdge, bool resetEdge) {
		cc = &c;
		float dt = 1.f / sr;
		trigTR = trigSlice = trigEOS = false;
		if (std::fabs(c.field - fieldNow) > 0.01f) {
			fieldNow = c.field;
			for (int a = 0; a < 3; a++) coil[a].setup(a, fieldNow, sr);
		}
		bool wantSession = c.session > 0.f;
		if (wantSession != sessionOn) {
			sessionOn = wantSession;
			sessStep = -1; pauseLeft = 0.0;
			plant.motor(false, 1.f);
			if (sessionOn) sessionAdvance(c.session);
			else { state = DEAD; trT = 1e9; }
		}
		if (resetEdge) {
			if (sessionOn) { sessStep = -1; pauseLeft = 0.0; sessionAdvance(c.session); }
			else startTR();
		}
		if (clockEdge) pendingClock = true;

		// ---- scheduler
		if (sessionOn) {
			stepT += dt;
			if (pauseLeft > 0.0) {
				pauseLeft -= dt;
				if (pauseLeft <= 0.0) sessionAdvance(c.session);
			}
			else if (state == PAUSED && stepT >= stepLen) {
				trigEOS = true;
				sessionAdvance(c.session);
			}
		}
		if (state == UNIT) {
			unitT += dt;
			if (unitT >= unitDur) {
				unitsDone++;
				bool stepOver = sessionOn && stepT >= stepLen;
				if (stepOver) {
					// end of a series: gradients stop, the machine breathes, then the next one
					trigEOS = true;
					state = PAUSED; pulses.clear();
					pauseLeft = 3.0 + 4.0 * rng.uni();
				}
				else if (!sessionOn && c.mode != curMode) startTR();
				else if (c.clocked && pendingClock) { pendingClock = false; startTR(); }
				else if (unitsDone < unitsThisTR) startUnit();
				else { state = c.clocked ? WAIT : DEAD; pulses.clear(); }
			}
		}
		if (state == DEAD || state == WAIT) {
			trT += dt;
			if (c.clocked) {
				if (pendingClock) { pendingClock = false; startTR(); }
			}
			else if (trT >= trLen || (!sessionOn && c.mode != curMode)) startTR();
		}
		if (state == UNIT || state == DEAD || state == WAIT) trT += (state == UNIT ? dt : 0.0);
		if (!c.clocked) pendingClock = false;
		scanning = state == UNIT;

		// ---- gradients
		float gv[4] = {0.f, 0.f, 0.f, 0.f};
		if (state == UNIT) {
			float t = (float) unitT;
			while (firstLive < pulses.size() && pulses[firstLive].end <= t) firstLive++;
			for (size_t i = firstLive; i < pulses.size(); i++) {
				const Pulse& p = pulses[i];
				if (t < p.t0) break;
				if (t >= p.end) continue;
				gv[p.axis] += pulseValue(p, t, softNow);
			}
		}
		float lv[3] = {c.lx, c.ly, c.lz};
		if (c.field != fieldGainFor) { fieldGainFor = c.field; fieldGain = 0.45f * std::pow(2.f, c.field * 2.f); }   // ~ quadratic with B0
		float bright = c.field;
		float sum = 0.f;
		for (int a = 0; a < 3; a++) {
			g[a] = clampf(gv[a] * driftAmp[a], -1.2f, 1.2f);
			float y = coil[a].process(g[a] * fieldGain, bright);
			y = std::tanh(0.8f * y);
			coilOut[a] = y;
			sum += y * lv[a];
		}
		float rfc = rfHp.process(gv[3] - rfPrev) * 0.4f;
		rfPrev = gv[3];
		sum += rfc;
		// table / pump / room
		float pl = plant.process() * c.plant;
		trigPump = plant.pumpTrig;
		float in = sum * 0.8f + pl + audioIn;
		float l, r;
		bore.process(in, in, c.bore, l, r);
		outL = std::tanh(l * 0.9f);
		outR = std::tanh(r * 0.9f);
	}
};

}  // namespace bore
