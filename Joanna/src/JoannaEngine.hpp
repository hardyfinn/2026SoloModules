// Joanna sound engine: an upright home piano and the room it stands in.
// No Rack code here, so test/render.cpp can play it offline.
//
// How it works (all in SI units: metres, kilograms, newtons, seconds):
//
//  * 88 fixed keys. Every key has its own strings, hammer, damper and quirks,
//    all drawn from the piano's serial number, so the same serial is always
//    the same instrument.
//  * Strings are modal: each partial of each string is a complex one-pole
//    resonator with the exact stiff-string frequency f_k = k f0 sqrt(1+Bk^2)/sqrt(1+B)
//    and its own decay. For the lowest 16 partials the 2 or 3 strings of a
//    note are coupled through the bridge (z_j -= c * sum of the strings), which
//    gives the beating of slightly mistuned unisons and the two-stage decay
//    (loud prompt sound, then the long quiet aftersound) without being told to.
//  * The hammer is a mass on a nonlinear felt spring with hysteresis
//    (F = K x^p (1 + mu dx/dt)). Contact is solved implicitly each sample, so it
//    can meet a string that is already ringing; loud notes are brighter because
//    felt stiffens as it is squashed, not because of a filter.
//  * Tension modulation of the bass strings gives the "phantom" partials and
//    the longitudinal clang when played hard.
//  * All strings push on one bridge. Every undamped string listens to it, so
//    held keys and the pedal-down halo ring sympathetically, in this piano's
//    own (imperfect) tuning.
//  * Soundboard (48 modes), cabinet (lid, wall behind), action noises, and a
//    shoebox room (image-source early reflections + an 8-line FDN tail).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace joanna {

static const float kPi = 3.14159265358979f;
static const int NKEYS = 88;
static const int KEY_C4 = 39;       // key index of middle C (A0 = 0)
static const int NVOICES = 24;      // string sets that can ring at once
static const int MAXS = 3;          // strings per note
static const int MAXC = 16;         // partials whose strings are coupled through the bridge
static const int MAXB = 112;        // other string modes per note
static const int NBOARD = 48;       // soundboard modes
static const int NBANK = NKEYS * 2; // sympathetic strings (2 partials per key)
static const int NNOISE = 32;       // action noise events at once
static const int NTAPS = 13;        // direct + 12 reflections per ear
static const float PHANTOM = 3e4f;
// action noise levels (calibrated against a mf note: knocks about -28 dB, ticks and rustles lower)
static const float THUMP = 0.08f;    // N into the soundboard (x300 there)
static const float TICK = 0.002f;   // air, before x60
static const float RUSTLE = 0.0006f;  // ~ EA/4 of the string: tension change per (slope)^2, N

// ---------------------------------------------------------------- controls
struct Controls {
	float size = 0.5f;      // 0 spinet .. 1 tall old upright
	float hammer = 0.4f;    // 0 soft voiced felt .. 1 hard (tack piano at the top)
	float touch = 0.5f;     // 0 heavy action .. 1 light action (velocity curve)
	float sustain = 0.5f;   // overall ring
	float symp = 0.5f;      // sympathetic resonance
	float noise = 0.4f;     // action noise level
	float tuning = 0.25f;   // 0 tuned yesterday .. 1 twenty years ago
	float unison = 0.f;     // extra honky-tonk spread, 0..1 = 0..25 cents
	float wear = 0.3f;      // how much it has been played
	float age = 0.3f;       // how old it is
	float fine = 0.f;       // cents, -50..50
	int lid = 1;            // 0 closed, 1 top open, 2 front panel off
	float wall = 0.3f;      // back of the piano to the wall, 0..1 = 3..60 cm
	float room = 0.35f;     // box room .. front room .. hall
	float furnish = 0.35f;  // carpet and sofa .. bare boards
	float distance = 0.4f;  // inside .. player's seat .. across the room .. next room
	float level = 1.f;
	float pedal = 0.f;      // sustain pedal 0..1 (half pedalling works)
	bool soft = false;      // soft pedal (hammer rail moves closer)
	bool felt = false;      // practice felt between hammers and strings
	float pitchRef = 440.f; // A4
	bool freePitch = false; // V/OCT fractions retune the key
	uint32_t serial = 4127;
};

// ---------------------------------------------------------------- random numbers
// Stateless hash, so each quirk of each key has its own stream: moving one
// control never reshuffles the others.
static inline uint32_t mix32(uint64_t x) {
	x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
	x ^= x >> 27; x *= 0x94D049BB133111EBull;
	x ^= x >> 31;
	return (uint32_t)(x >> 32);
}
static inline float urand(uint32_t serial, int key, int field) {
	uint64_t x = (uint64_t)serial * 0x9E3779B97F4A7C15ull + (uint64_t)(key + 1) * 0xD1B54A32D192ED03ull + (uint64_t)(field + 7) * 0x8CB92BA72F3D8DD7ull;
	return (mix32(x) >> 8) * (1.f / 16777216.f);
}
static inline float grand(uint32_t serial, int key, int field) {
	float u1 = std::max(urand(serial, key, field * 2 + 1000), 1e-7f);
	float u2 = urand(serial, key, field * 2 + 1001);
	return std::sqrt(-2.f * std::log(u1)) * std::cos(2.f * kPi * u2);
}
enum Field {
	F_ERR, F_STR0, F_STR1, F_STR2, F_WILD, F_WILDSTR, F_WILDAMT, F_HARD, F_VEL, F_DELAY, F_DAMPQ,
	F_STRIKE, F_H0, F_H1, F_H2, F_SPLIT, F_NOISE, F_BOARD, F_BOARDQ, F_BOARDA, F_BOARDL, F_BOARDR,
	F_SEASON1, F_SEASON2, F_RATTLE, F_RATTLE2, F_CLICK
};

// ---------------------------------------------------------------- small DSP parts
struct OnePole {
	float a = 0.f, y = 0.f;
	void setLP(float fc, float fs) { a = 1.f - std::exp(-2.f * kPi * std::min(fc, 0.45f * fs) / fs); }
	float lp(float x) { y += a * (x - y); return y; }
	float hp(float x) { y += a * (x - y); return x - y; }
};

struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void bandpass(float fc, float q, float fs) {
		float w = 2.f * kPi * std::min(fc, 0.45f * fs) / fs, al = std::sin(w) / (2.f * q), a0 = 1.f + al;
		b0 = al / a0; b1 = 0.f; b2 = -al / a0; a1 = -2.f * std::cos(w) / a0; a2 = (1.f - al) / a0;
	}
	void lowpass(float fc, float q, float fs) {
		float w = 2.f * kPi * std::min(fc, 0.45f * fs) / fs, al = std::sin(w) / (2.f * q), c = std::cos(w), a0 = 1.f + al;
		b0 = (1.f - c) * 0.5f / a0; b1 = (1.f - c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	void highpass(float fc, float q, float fs) {
		float w = 2.f * kPi * std::min(fc, 0.45f * fs) / fs, al = std::sin(w) / (2.f * q), c = std::cos(w), a0 = 1.f + al;
		b0 = (1.f + c) * 0.5f / a0; b1 = -(1.f + c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void clear() { z1 = z2 = 0.f; }
};

// ---------------------------------------------------------------- one key's strings and hammer
// Built when the key is struck (only if a control that shapes it has moved since).
struct KeyModel {
	uint32_t version = 0xffffffffu;
	float frac = 0.f;
	int S = 3, Kc = 0, nB = 0;
	float f0 = 261.6f, L = 0.6f;
	// coupled block, index j * MAXC + k
	float cpr[MAXS * MAXC], cpi[MAXS * MAXC];   // poles
	float cbr[MAXS * MAXC], cbi[MAXS * MAXC];   // hammer force -> mode
	float chw[MAXS * MAXC];                     // mode -> displacement under the hammer
	float cw[MAXS * MAXC];                      // mode -> force on the bridge
	float cdm[MAXS * MAXC];                     // damper loss per sample
	float csr[MAXS * MAXC], csi[MAXS * MAXC];   // bridge motion -> mode (sympathy)
	float cpw[MAXS * MAXC];                     // mode -> string slope (phantoms)
	float ccr[MAXC], cci[MAXC];                 // bridge coupling of the unison
	// free block
	float bpr[MAXB], bpi[MAXB], bbr[MAXB], bbi[MAXB], bhw[MAXB], bw[MAXB], bdm[MAXB], bsr[MAXB], bsi[MAXB], bpw[MAXB];
	float G = 0.f;                              // displacement per newton this sample (implicit contact)
	float hm = 0.008f, hK = 1e9f, hp = 2.5f, hmu = 0.25f;
	float velGain = 1.f, delay = 0.f;
	bool hasDamper = true;
	float damperTime = 0.03f;
	bool phantom = false;
	float plr = 0.f, pli = 0.f, plIn = 0.f;     // longitudinal mode
	float pan = 0.5f;
};

// ---------------------------------------------------------------- a ringing note
struct Voice {
	int key = -1;
	bool active = false;
	const KeyModel* m = nullptr;
	float czr[MAXS * MAXC], czi[MAXS * MAXC];
	float bzr[MAXB], bzi[MAXB];
	int nB = 0;                 // free modes still running (the top ones are cut as they die)
	// hammer
	int wait = -1;              // samples until the hammer arrives
	float hv = 0.f;             // hammer speed (m/s)
	bool contact = false;
	float u = 0.f, uPrev = 0.f, xPrev = 0.f, F = 0.f;
	int hT = 0, hFree = 0;
	// damper: 0 lifted .. 1 resting on the strings
	float damp = 0.f;
	float out = 0.f;            // force on the bridge (N), this sample
	float hpState = 0.f, plzr = 0.f, plzi = 0.f;
	int since = 0;              // samples since struck
	float level = 0.f;          // rough loudness, for voice stealing
	void clear() {
		std::memset(czr, 0, sizeof(czr)); std::memset(czi, 0, sizeof(czi));
		std::memset(bzr, 0, sizeof(bzr)); std::memset(bzi, 0, sizeof(bzi));
		contact = false; wait = -1; F = 0.f; out = 0.f; hpState = 0.f; plzr = plzi = 0.f; level = 0.f;
	}
};

// ---------------------------------------------------------------- action noises
enum Dest { TO_BOARD, TO_AIR, TO_BUS };
// Knocks are smooth pulses (a Hann bump, no hiss: a key hitting felt is not noisy);
// rustles and swishes are filtered noise.
struct NoiseEvent {
	bool on = false, pulse = false;
	int wait = 0, att = 1, n = 0;
	float env = 0.f, dec = 0.99f, gain = 0.f;
	Biquad bp;
	int dest = TO_BOARD;
};

// ---------------------------------------------------------------- the engine
struct Engine {
	float fs = 48000.f, dt = 1.f / 48000.f;
	Controls c, next;
	uint32_t version = 0;
	float lastKey[24];

	// the piano (per key)
	int S[NKEYS];
	float B[NKEYS], Ls[NKEYS], fT[NKEYS], fKey[NKEYS], det[NKEYS][MAXS], hgt[NKEYS][MAXS];
	float tc[NKEYS], velG[NKEYS], dly[NKEYS], dampQ[NKEYS], strike[NKEYS], keyNoise[NKEYS];
	int nSingle = 9, nBreak = 28, nNoDamper = 70;
	KeyModel keys[NKEYS];
	Voice voices[NVOICES];
	int held[NKEYS];
	uint8_t keyActive[NKEYS];
	bool houseFresh = true, tapsSettled = false;

	// soundboard
	float bfm[NBOARD], bQ[NBOARD], bA[NBOARD];
	float bpr_[NBOARD], bpi_[NBOARD], bgin[NBOARD], bgl[NBOARD], bgr[NBOARD], bzr[NBOARD], bzi[NBOARD];
	float yNorm = 1.f;
	Biquad boardHP, boardHP2;
	OnePole boardTop, ageLow[2], radShelf;

	// sympathetic strings (pedal down, or the undamped top)
	float kpr[NBANK], kpi[NBANK], kzr[NBANK], kzi[NBANK], kin[NBANK], kw[NBANK], kdm[NBANK];
	float kinG[NBANK], kgg[NBANK], klift[NBANK];   // input gain with played keys muted; damper factor at this pedal
	float kggPedal = -1.f;
	bool bankBusy = false;

	// noises
	NoiseEvent ev[NNOISE];
	uint32_t rng = 22222u;
	Biquad rattleBP, rattleHP;
	float rattleF = 200.f, rattleOn = 0.f;

	// cabinet and wall
	OnePole lidLP[2];
	Biquad box1[2], box2[2];
	float wallBuf[2][4096];
	int wallPos = 0;
	OnePole wallLP[2];
	float wallDelay = 10.f;

	// room
	float roomBuf[65536];
	int roomPos = 0;
	float tapD[2][NTAPS], tapG[2][NTAPS], tapDT[2][NTAPS], tapGT[2][NTAPS];
	OnePole earlyLP[2], closeHP[2], directLP[2];
	float fdnBuf[8][8192];
	int fdnPos[8], fdnLen[8];
	float fdnG[8];
	OnePole fdnLP[8];
	float tailGain = 0.2f, earlyGain = 1.f, directGain = 1.f, closeMix = 0.f, nextRoom = 0.f, roomNorm = 1.f;
	Biquad nextLP[2], doorLP;
	float sumL = 0.f, sumR = 0.f;

	// state
	float busPrev = 0.f, pedalS = 0.f, pedalPrev = 0.f;
	int ctlCount = 0, pianoCount = 0;
	float outL = 0.f, outR = 0.f, outDry = 0.f, meter = 0.f;
	float outScale = 0.017f;

	Engine() {
		std::memset(held, 0, sizeof(held));
		std::memset(lastKey, 0, sizeof(lastKey));
		reset();
		setSampleRate(48000.f);
	}

	void setSampleRate(float sr) {
		if (sr == fs && version)
			return;
		fs = sr;
		dt = 1.f / sr;
		version++;
		computePiano();
		computeHouse();
	}

	void reset() {
		for (int i = 0; i < NVOICES; i++) { voices[i].clear(); voices[i].active = false; voices[i].key = -1; }
		std::memset(held, 0, sizeof(held));
		std::memset(keyActive, 0, sizeof(keyActive));
		std::memset(kinG, 0, sizeof(kinG));
		kggPedal = -1.f;
		std::memset(bzr, 0, sizeof(bzr)); std::memset(bzi, 0, sizeof(bzi));
		std::memset(kzr, 0, sizeof(kzr)); std::memset(kzi, 0, sizeof(kzi));
		std::memset(wallBuf, 0, sizeof(wallBuf));
		std::memset(roomBuf, 0, sizeof(roomBuf));
		std::memset(fdnBuf, 0, sizeof(fdnBuf));
		for (int i = 0; i < 8; i++) fdnPos[i] = 0;
		for (int i = 0; i < NNOISE; i++) ev[i].on = false;
		busPrev = 0.f;
	}

	// ------------------------------------------------------------ controls in
	// Called every sample; rebuilds what depends on a control only when it moved.
	void setControls(const Controls& nc) {
		pedalS = nc.pedal;
		next = nc;
		if (++ctlCount < 32)
			return;
		ctlCount = 0;
		if (++pianoCount >= 8) pianoCount = 0;
		applyControls(pianoCount == 0);
	}

	// piano-shaping controls are looked at every 256 samples (and at every key press),
	// the house every 32
	void applyControls(bool piano) {
		const Controls& nc = next;
		bool pianoChanged = false;
		if (piano) {
			// quantised so CV noise does not keep rebuilding
			float k[12] = {nc.size, nc.hammer, nc.sustain, nc.tuning, nc.unison, nc.wear, nc.age, nc.fine / 100.f,
			               nc.pitchRef / 1000.f, (float)(nc.serial % 100000) / 100000.f, nc.felt ? 1.f : 0.f, nc.symp};
			for (int i = 0; i < 12; i++) {
				float q = std::round(k[i] * 1024.f);
				if (q != lastKey[i]) { lastKey[i] = q; pianoChanged = true; }
			}
		}
		bool houseChanged = nc.lid != c.lid || std::fabs(nc.wall - c.wall) > 1e-3f || std::fabs(nc.room - c.room) > 1e-3f ||
		                    std::fabs(nc.furnish - c.furnish) > 1e-3f || std::fabs(nc.distance - c.distance) > 1e-3f;
		Controls keep = c;
		c = nc;
		if (!piano) {
			c.size = keep.size; c.hammer = keep.hammer; c.sustain = keep.sustain; c.tuning = keep.tuning;
			c.unison = keep.unison; c.wear = keep.wear; c.age = keep.age; c.fine = keep.fine; c.pitchRef = keep.pitchRef;
			c.serial = keep.serial; c.felt = keep.felt; c.symp = keep.symp;
		}
		if (pianoChanged) {
			version++;
			computePiano();
		}
		if (houseChanged)
			computeHouse();
	}

	// ------------------------------------------------------------ the instrument
	static float s(float k, float b) { return std::sqrt((1.f + b * k * k) / (1.f + b)); }

	float Yre(float f) const {
		float y = 0.6f * (1.f - 0.4f * c.age / (1.f + (f / 220.f) * (f / 220.f)));
		for (int m = 0; m < NBOARD; m++) {
			float u = 2.f * bQ[m] * (f - bfm[m]) / bfm[m];
			y += bA[m] / (1.f + u * u);
		}
		return y * yNorm;
	}
	float Yim(float f) const {
		float y = 0.f;
		for (int m = 0; m < NBOARD; m++) {
			float u = 2.f * bQ[m] * (f - bfm[m]) / bfm[m];
			y += bA[m] * u / (1.f + u * u);
		}
		return y * yNorm;
	}

	void computePiano() {
		const uint32_t sr = c.serial;
		const float size = c.size, t = c.tuning, wear = c.wear, age = c.age;
		nSingle = 8 + (int)std::round(2.f * size);
		nBreak = 26 + (int)std::round(4.f * size);
		nNoDamper = 70;   // G6 and up ring free, as on most uprights
		float sizeFac = 2.0f - 1.4f * size;
		float Lmax = 0.95f + 0.4f * size;
		auto Ltreble = [](int n) { return 0.62f * std::exp2(-(n - KEY_C4) * 0.88f / 12.f); };
		for (int n = 0; n < NKEYS; n++) {
			S[n] = n < nSingle ? 1 : n < nBreak ? 2 : 3;
			if (n < nBreak) {
				B[n] = 6e-4f * sizeFac * std::exp(-0.07f * n);
				Ls[n] = Lmax + (Ltreble(nBreak) * 1.15f - Lmax) * n / (float)nBreak;
			} else {
				B[n] = 2.2e-4f * std::sqrt(sizeFac) * std::exp(0.073f * (n - 28));
				Ls[n] = std::min(Ltreble(n), Lmax);
			}
		}
		// the tuner's stretch: equal temperament F3..F4, then octaves matched by ear
		for (int n = 32; n <= 44; n++)
			fT[n] = 440.f * std::exp2((n - 48) / 12.f);
		for (int n = 45; n < NKEYS; n++)
			fT[n] = 2.f * fT[n - 12] * s(2.f, B[n - 12]);
		for (int n = 31; n >= 0; n--)
			fT[n] = fT[n + 12] * s(3.f, B[n + 12]) / (2.f * s(6.f, B[n]));

		// time since tuning
		float rmsErr = 0.3f + 8.7f * std::pow(t, 1.3f);
		float seasonA = 12.f * std::pow(t, 1.2f);
		float ph1 = 2.f * kPi * urand(sr, 0, F_SEASON1), ph2 = 2.f * kPi * urand(sr, 0, F_SEASON2);
		float sag = -35.f * std::pow(t, 1.5f);
		float spread = 0.4f + 6.6f * std::pow(t, 1.3f);
		float honk = 25.f * c.unison;
		float ref = c.pitchRef / 440.f * std::exp2(c.fine / 1200.f);
		for (int n = 0; n < NKEYS; n++) {
			float x = n / 87.f;
			float curve = seasonA * (0.6f * std::cos(2.f * kPi * x + ph1) + 0.4f * std::cos(4.f * kPi * x + ph2));
			float cents = rmsErr * grand(sr, n, F_ERR) + curve + sag * (0.7f + 0.3f * std::sin(kPi * x));
			fKey[n] = ref * fT[n] * std::exp2(cents / 1200.f);
			int Sn = S[n];
			for (int j = 0; j < MAXS; j++) {
				float d = spread * grand(sr, n, F_STR0 + j) + 0.07f * (j - 1);
				if (Sn == 3) d += honk * (j - 1) * 0.5f + honk * 0.15f * grand(sr, n, F_STR0 + j + 40);
				if (Sn == 2) d += honk * (j == 0 ? -0.5f : 0.5f);
				det[n][j] = d;
			}
			// past about 8 years, a few strings go their own way
			if (t > 0.4f && urand(sr, n, F_WILD) < (t - 0.4f) * 0.15f && Sn > 1) {
				int j = (int)(urand(sr, n, F_WILDSTR) * Sn) % Sn;
				float a = 15.f + 25.f * urand(sr, n, F_WILDAMT);
				det[n][j] += urand(sr, n, F_WILDAMT + 50) < 0.5f ? -a : a;
			}
			if (Sn == 1)
				det[n][1] = 0.f;   // second polarisation, split set in buildKey

			// hammer: contact time at 2 m/s, harder with HAMMER and where it is played most
			float mid = std::exp(-0.5f * ((n - 45.f) / 16.f) * ((n - 45.f) / 16.f));
			float hard = std::exp(-(c.hammer - 0.4f) * 1.3f);
			if (c.hammer > 0.85f) hard *= 1.f - 2.6f * (c.hammer - 0.85f);   // tack piano
			tc[n] = 0.0028f * std::exp2(-n / 29.f) * hard * (1.f - 0.35f * wear * mid) * std::exp(0.15f * (0.2f + wear) * grand(sr, n, F_HARD));
			velG[n] = std::exp(0.15f * (0.15f + wear) * grand(sr, n, F_VEL));
			dly[n] = 0.0015f + 0.006f * wear * urand(sr, n, F_DELAY);
			float q = urand(sr, n, F_DAMPQ);
			dampQ[n] = 1.f - age * (q < 0.25f ? 0.92f : 0.35f * q);   // some dampers leak with age
			strike[n] = (n < 60 ? 0.12f : 0.12f - 0.05f * (n - 60) / 27.f) + 0.006f * (1.f + wear) * grand(sr, n, F_STRIKE) * 0.5f;
			float hs = 0.f;
			for (int j = 0; j < MAXS; j++) {
				hgt[n][j] = std::max(0.3f, 1.f + (0.05f + 0.25f * wear) * grand(sr, n, F_H0 + j));
				if (j < Sn) hs += hgt[n][j];
			}
			for (int j = 0; j < MAXS; j++) hgt[n][j] /= hs;
			keyNoise[n] = 0.6f + 0.8f * urand(sr, n, F_NOISE);
		}

		// soundboard: lowest modes placed like an upright's, the rest denser
		float f1 = 70.f + 45.f * (1.f - size);
		static const float low[8] = {1.f, 1.38f, 1.83f, 2.3f, 2.85f, 3.35f, 4.f, 4.7f};
		for (int m = 0; m < NBOARD; m++) {
			float f;
			if (m < 8)
				f = f1 * low[m] * (1.f + 0.04f * grand(sr, m, F_BOARD));
			else {
				float a = (m - 8) / 39.f;
				f = f1 * 5.2f * std::pow(3200.f / (f1 * 5.2f), a) * (1.f + 0.06f * grand(sr, m, F_BOARD));
			}
			bfm[m] = f;
			bQ[m] = (15.f + 25.f * urand(sr, m, F_BOARDQ)) * (1.f - 0.45f * age);
			bA[m] = (0.3f + 0.5f * urand(sr, m, F_BOARDA)) * (m < 8 ? 1.f : 0.7f);
			float gm = (m < 8 ? 1.f : 0.6f) * (0.5f + 0.5f * urand(sr, m, F_BOARDL));
			bgl[m] = urand(sr, m, F_BOARDL + 60) < 0.5f ? -gm : gm;
			float gr = (m < 8 ? 1.f : 0.6f) * (0.5f + 0.5f * urand(sr, m, F_BOARDR));
			bgr[m] = urand(sr, m, F_BOARDR + 60) < 0.5f ? -gr : gr;
			float r = std::exp(-kPi * f / bQ[m] * dt);
			bpr_[m] = r * std::cos(2.f * kPi * std::min(f, 0.45f * fs) * dt);
			bpi_[m] = r * std::sin(2.f * kPi * std::min(f, 0.45f * fs) * dt);
			bgin[m] = (1.f - r) * 0.55f;
		}
		// mean of the admittance shape over 100..3000 Hz is 1
		yNorm = 1.f;
		float acc = 0.f;
		for (int i = 0; i < 64; i++)
			acc += Yre(100.f * std::pow(30.f, i / 63.f));
		yNorm = 64.f / acc;
		boardHP.highpass(f1 * 0.55f, 0.5f, fs);
		radShelf.setLP(800.f, fs);
		boardHP2.highpass(28.f, 0.7f, fs);
		boardTop.setLP(9000.f, fs);
		ageLow[0].setLP(250.f, fs);
		ageLow[1].setLP(250.f, fs);

		// sympathetic strings: partials 1 and 2 of every key, at this piano's tuning
		float sustMul = 0.5f + c.sustain;
		for (int n = 0; n < NKEYS; n++) {
			for (int p = 0; p < 2; p++) {
				int i = n * 2 + p;
				float k = p + 1.f;
				float f = k * fKey[n] * s(k, B[n]) * std::exp2(det[n][0] / 1200.f);
				float T60 = 60.f * std::exp2(-n / 16.f) * sustMul * (1.f - 0.4f * age);
				float sig = 6.91f / T60 + 7.2e-7f * f * f;
				float r = std::exp(-sig * dt);
				float w = 2.f * kPi * std::min(f, 0.45f * fs) * dt;
				kpr[i] = r * std::cos(w);
				kpi[i] = r * std::sin(w);
				kin[i] = f < 0.45f * fs ? (1.f - r) * 40.f / k : 0.f;
				kw[i] = 1.f / k;
				float T60d = 0.25f * std::exp2(-(n - KEY_C4) / 40.f);
				kdm[i] = 1.f - std::exp(-6.91f / T60d * dt);
			}
		}

		for (int n = 0; n < NKEYS; n++) bankGate(n);
		kggPedal = -1.f;

		// rattle (something in the house buzzes along with a few notes)
		rattleF = 140.f + 280.f * urand(sr, 0, F_RATTLE);
		rattleBP.bandpass(rattleF, 9.f, fs);
		rattleHP.bandpass(2200.f + 1800.f * urand(sr, 0, F_RATTLE2), 3.f, fs);
		rattleOn = std::max(0.f, (wear - 0.5f) * 2.f);
	}

	// evens out the register (measured with test/render: mf notes about equally loud)
	static float keyGain(int n) {
		return 0.9f * std::exp2(-(n - KEY_C4) / 30.f + std::max(0, n - 64) / 13.f);
	}

	// one key's strings and hammer, from the arrays above
	void buildKey(int n, float frac) {
		KeyModel& m = keys[n];
		const float age = c.age, wear = c.wear;
		const int Sn = S[n];
		m.S = Sn;
		m.frac = frac;
		m.version = version;
		float f0 = fKey[n] * std::exp2(frac / 12.f);
		m.f0 = f0;
		float L = Ls[n];
		m.L = L;
		const float T = 750.f;
		float fET = fT[n];
		float mu = T / ((2.f * L * fET) * (2.f * L * fET));
		float M = mu * L * 0.5f;   // modal mass of one string
		float Bn = B[n];
		bool wound = n < nBreak;

		float sustMul = 0.5f + c.sustain;
		float T60a = 60.f * std::exp2(-n / 16.f) * sustMul * (1.f - 0.4f * age);
		float T60p = 9.f * std::exp2(-n / 22.f) * sustMul;
		float killer = (n >= 51 && n <= 63) ? 1.f + 1.5f * age * std::sin(kPi * (n - 51) / 12.f) : 1.f;
		float sigP = 6.91f / T60p * killer;
		float s1 = 7.2e-7f * (wound ? 1.f + 8.f * age : 1.f + 2.f * age);
		float fmax = std::min(5000.f * std::pow(11.f / 5.f, n / 87.f), 0.45f * fs);
		int Kmax = Sn == 1 ? 72 : Sn == 2 ? 48 : 36;

		float x0 = std::min(std::max(strike[n], 0.05f), 0.16f);
		float width = (0.0015f + 0.004f * wear * std::exp(-0.5f * ((n - 45.f) / 16.f) * ((n - 45.f) / 16.f))) + (c.felt ? 0.012f : 0.f);
		float xd = x0 + 0.02f;
		float T60d = 0.25f * std::exp2(-(n - KEY_C4) / 40.f) / std::max(dampQ[n], 0.05f);
		float d0 = 6.91f / T60d;

		m.hasDamper = n < nNoDamper;
		m.damperTime = 0.015f + 0.065f * (1.f - n / 87.f);
		m.phantom = n < 51;
		m.pan = 0.12f + 0.76f * n / 87.f;

		// per-key output balance (the model is physical; this evens out the register a little)
		float gainN = keyGain(n);

		int K = 0;
		float fk[96];
		for (int k = 1; k <= Kmax; k++) {
			float f = k * f0 * s((float)k, Bn);
			if (f > fmax) break;
			fk[K++] = f;
		}
		if (K == 0) { fk[0] = f0; K = 1; }

		int Kc = Sn >= 2 ? std::min(K, MAXC) : 0;
		m.Kc = Kc;
		int nb = 0;
		float G = 0.f;
		std::memset(m.cpw, 0, sizeof(m.cpw));
		for (int k = 1; k <= K; k++) {
			float f = fk[k - 1];
			float w = 2.f * kPi * f;
			float sigI = 6.91f / T60a + s1 * f * f;
			float Y = Yre(f);
			float rr = std::max(-0.6f, std::min(0.6f, 0.3f * Yim(f)));
			float Gk = std::max(0.f, sigP * Y * (1.f + f / 4000.f) - sigI);
			float a = kPi * k * width / L * 0.5f;
			float sinc = a > 1e-4f ? std::sin(a) / a : 1.f;
			float phi = std::sin(kPi * k * x0) * sinc;
			float bmag = dt / (M * w);
			float wout = T * kPi * k / L * gainN;
			float sn = std::sin(kPi * k * xd);
			float dk = d0 * (0.15f + sn * sn) / 0.65f;
			float dm = 1.f - std::exp(-dk * dt);
			float pw = k <= 12 && m.phantom ? kPi * k / L : 0.f;
			// sympathy: the other strings' force on the bridge moves this string's end.
			// Same physics as the unison coupling below (eta = g (1 + i r)), fed by the
			// real bridge force instead of the complex amplitude: z += -2i eta dt / w * F
			auto symp = [&](float g, float& r_, float& i_) {
				r_ = 2.f * g * rr * dt / wout;
				i_ = -2.f * g * dt / wout;
			};

			auto pole = [&](float freq, float sig, float& pr, float& pi, float& br, float& bi) {
				float wd = 2.f * kPi * std::min(freq, 0.49f * fs) * dt;
				float r = std::exp(-sig * dt);
				pr = r * std::cos(wd);
				pi = r * std::sin(wd);
				float rh = std::exp(-sig * dt * 0.5f);
				br = rh * std::cos(wd * 0.5f);
				bi = rh * std::sin(wd * 0.5f);
			};

			if (Sn >= 2 && k <= Kc) {
				for (int j = 0; j < Sn; j++) {
					int i = j * MAXC + (k - 1);
					float pr, pi, br, bi;
					pole(f * std::exp2(det[n][j] / 1200.f), sigI, pr, pi, br, bi);
					m.cpr[i] = pr; m.cpi[i] = pi;
					float h = hgt[n][j];
					m.cbr[i] = br * bmag * phi * h;
					m.cbi[i] = bi * bmag * phi * h;
					m.chw[i] = h * phi;
					m.cw[i] = wout;
					m.cdm[i] = dm;
					symp(Gk / Sn, m.csr[i], m.csi[i]);
					m.cpw[i] = pw;
					G += h * phi * m.cbi[i];
				}
				float g = Gk / Sn * dt;
				m.ccr[k - 1] = g;
				m.cci[k - 1] = g * rr;
			} else if (Sn >= 2) {
				// above the coupled partials: the strings move together (one mode) ...
				if (nb < MAXB) {
					float dm_ = 0.f;
					for (int j = 0; j < Sn; j++) dm_ += det[n][j];
					dm_ /= Sn;
					float pr, pi, br, bi;
					pole(f * std::exp2(dm_ / 1200.f), sigI + Gk, pr, pi, br, bi);
					m.bpr[nb] = pr; m.bpi[nb] = pi;
					m.bbr[nb] = br * bmag * phi; m.bbi[nb] = bi * bmag * phi;
					m.bhw[nb] = phi / Sn;
					m.bw[nb] = wout;
					m.bdm[nb] = dm;
					symp(Gk, m.bsr[nb], m.bsi[nb]);
					m.bpw[nb] = 0.f;
					G += phi / Sn * m.bbi[nb];
					nb++;
				}
				// ... plus a quiet slow mode where uneven hammers feed the aftersound
				if (k <= 28 && nb < MAXB) {
					float spreadMax = 0.f;
					for (int j = 0; j < Sn; j++) spreadMax = std::max(spreadMax, std::fabs(det[n][j]));
					float pr, pi, br, bi;
					pole(f * std::exp2((det[n][Sn - 1]) / 1200.f), sigI + 0.08f * Gk, pr, pi, br, bi);
					float amt = 0.06f + 0.4f * std::fabs(hgt[n][0] - hgt[n][Sn - 1]) + 0.01f * spreadMax;
					m.bpr[nb] = pr; m.bpi[nb] = pi;
					m.bbr[nb] = br * bmag * phi * amt; m.bbi[nb] = bi * bmag * phi * amt;
					m.bhw[nb] = 0.f;
					m.bw[nb] = wout;
					m.bdm[nb] = dm;
					symp(0.08f * Gk, m.bsr[nb], m.bsi[nb]);
					m.bpw[nb] = 0.f;
					nb++;
				}
			} else {
				// single wound string: vertical and horizontal polarisations
				float split = 1.f + 0.0002f + 0.0008f * urand(c.serial, n, F_SPLIT);
				float pr, pi, br, bi;
				if (nb < MAXB) {
					pole(f, sigI + Gk, pr, pi, br, bi);
					m.bpr[nb] = pr; m.bpi[nb] = pi;
					m.bbr[nb] = br * bmag * phi; m.bbi[nb] = bi * bmag * phi;
					m.bhw[nb] = phi;
					m.bw[nb] = wout;
					m.bdm[nb] = dm;
					symp(Gk, m.bsr[nb], m.bsi[nb]);
					m.bpw[nb] = pw;
					G += phi * m.bbi[nb];
					nb++;
				}
				if (k <= 36 && nb < MAXB) {
					pole(f * split, sigI + 0.1f * Gk, pr, pi, br, bi);
					m.bpr[nb] = pr; m.bpi[nb] = pi;
					m.bbr[nb] = br * bmag * phi * 0.12f; m.bbi[nb] = bi * bmag * phi * 0.12f;
					m.bhw[nb] = 0.f;
					m.bw[nb] = wout * 0.35f;
					m.bdm[nb] = dm;
					symp(0.1f * Gk, m.bsr[nb], m.bsi[nb]);
					m.bpw[nb] = 0.f;
					nb++;
				}
			}
		}
		// zero the unused coupled slots so the inner loops can run over MAXC
		for (int j = 0; j < MAXS; j++)
			for (int k = (j < Sn ? Kc : 0); k < MAXC; k++) {
				int i = j * MAXC + k;
				m.cpr[i] = m.cpi[i] = m.cbr[i] = m.cbi[i] = m.chw[i] = m.cw[i] = m.cdm[i] = m.csr[i] = m.csi[i] = m.cpw[i] = 0.f;
			}
		for (int k = Kc; k < MAXC; k++) m.ccr[k] = m.cci[k] = 0.f;
		m.nB = nb;
		m.G = G;

		// hammer: mass, felt exponent, stiffness from the contact time
		float hm = (9.5f - 5.f * n / 87.f) * 1e-3f;
		float p = 2.2f + 0.8f * n / 87.f + 0.4f * (c.hammer - 0.5f);
		float Tc = tc[n];
		float hmu = 0.25f;
		if (c.felt) { p = 1.8f; Tc *= 3.6f; hmu = 0.7f; }
		float I = std::sqrt(kPi) * std::tgamma(1.f + 1.f / (p + 1.f)) / std::tgamma(0.5f + 1.f / (p + 1.f));
		float vref = 2.f;
		float xm = Tc * vref / (2.f * I);
		m.hm = hm;
		m.hp = p;
		m.hK = hm * vref * vref * (p + 1.f) / (2.f * std::pow(xm, p + 1.f));
		m.hmu = hmu;
		m.velGain = velG[n];
		m.delay = dly[n];

		// longitudinal mode of the string (fixed by its length, not its pitch)
		float fL = (wound ? 3600.f : 5100.f) / (2.f * L);
		fL = std::min(fL, 0.45f * fs);
		float rL = std::exp(-kPi * fL / 60.f * dt);
		m.plr = rL * std::cos(2.f * kPi * fL * dt);
		m.pli = rL * std::sin(2.f * kPi * fL * dt);
		m.plIn = 1.f - rL;
	}

	// ------------------------------------------------------------ the house
	void computeHouse() {
		const float r = c.room, fu = c.furnish;
		// cabinet
		float lidFc = c.lid == 0 ? 3500.f : c.lid == 1 ? 6000.f : 12000.f;
		for (int ch = 0; ch < 2; ch++) {
			lidLP[ch].setLP(lidFc, fs);
			box1[ch].bandpass(230.f * (1.f + 0.03f * ch), 6.f, fs);
			box2[ch].bandpass(470.f * (1.f - 0.02f * ch), 7.f, fs);
			wallLP[ch].setLP(2500.f, fs);
		}
		float wallM = 0.03f + 0.57f * c.wall;
		wallDelay = std::min((2.f * wallM / 343.f + 0.0004f) * fs, 4000.f);

		// room
		float Lx = 2.6f + 6.4f * std::pow(r, 1.5f);
		float Ly = 3.0f + 11.f * std::pow(r, 1.5f);
		float Lz = 2.4f + 2.6f * r * r;
		float V = Lx * Ly * Lz, Sa = 2.f * (Lx * Ly + Lx * Lz + Ly * Lz);
		float alpha = 0.45f - 0.37f * fu;
		float rt = std::min(2.4f, 0.161f * V / (Sa * alpha));
		float refl = std::sqrt(1.f - alpha);

		// where the mics are
		float d = c.distance;
		float rmic;
		if (d < 1.f / 3.f) rmic = 0.15f + (0.9f - 0.15f) * d * 3.f;
		else if (d < 2.f / 3.f) rmic = 0.9f + (std::max(1.5f, 0.75f * Ly) - 0.9f) * (d * 3.f - 1.f);
		else rmic = std::max(1.5f, 0.75f * Ly);
		closeMix = d < 1.f / 3.f ? 1.f - d * 3.f : 0.f;
		nextRoom = d > 2.f / 3.f ? (d * 3.f - 2.f) : 0.f;

		float sx = 0.25f * Lx + 0.f, sy = 0.4f + 0.57f * c.wall, sz = 1.0f;   // piano against the wall y = 0
		for (int ch = 0; ch < 2; ch++) {
			float mx = sx + (ch ? 0.2f : -0.2f), my = std::min(sy + rmic, Ly - 0.3f), mz = rmic < 0.5f ? 1.1f : 1.3f;
			int t = 0;
			auto addTap = [&](float ix, float iy, float iz, float g) {
				float dx = ix - mx, dy = iy - my, dz = iz - mz;
				float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
				tapDT[ch][t] = dist;
				tapGT[ch][t] = g / std::max(dist, 0.35f);
				t++;
			};
			addTap(sx, sy, sz, 1.f);
			addTap(-sx, sy, sz, refl); addTap(2 * Lx - sx, sy, sz, refl);
			addTap(sx, -sy, sz, refl); addTap(sx, 2 * Ly - sy, sz, refl);
			addTap(sx, sy, -sz, refl * 0.8f); addTap(sx, sy, 2 * Lz - sz, refl);
			addTap(-sx, -sy, sz, refl * refl); addTap(2 * Lx - sx, -sy, sz, refl * refl);
			addTap(-sx, 2 * Ly - sy, sz, refl * refl); addTap(sx, -sy, -sz, refl * refl * 0.8f);
			addTap(sx, 2 * Ly - sy, 2 * Lz - sz, refl * refl); addTap(-sx, sy, 2 * Lz - sz, refl * refl);
			float d0 = tapDT[ch][0];
			for (int i = 0; i < NTAPS; i++)
				tapDT[ch][i] = std::min((tapDT[ch][i] - d0) / 343.f * fs, 60000.f);
			earlyLP[ch].setLP(2500.f + 7000.f * fu, fs);
			directLP[ch].setLP(std::max(4000.f, 16000.f / (1.f + 0.06f * rmic)), fs);
			closeHP[ch].setLP(50.f, fs);
			nextLP[ch].lowpass(650.f, 0.6f, fs);
		}
		doorLP.lowpass(450.f, 0.7f, fs);

		// FDN tail
		static const int base[8] = {1031, 1327, 1523, 1871, 2111, 2389, 2671, 3011};
		float scale = (0.35f + 1.25f * r) * fs / 48000.f;
		for (int i = 0; i < 8; i++) {
			fdnLen[i] = std::min(8191, std::max(64, (int)(base[i] * scale)));
			fdnG[i] = std::pow(10.f, -3.f * fdnLen[i] / (rt * fs));
			fdnLP[i].setLP(3000.f + 7000.f * fu, fs);
		}
		// levels: direct falls as 1/r; the reverberant field equals the direct sound at the
		// critical distance rc. E is the tail's energy gain for white noise in (measured offline per room and furnish)
		double meanLen = 0.0;
		for (int i = 0; i < 8; i++) meanLen += fdnLen[i];
		meanLen /= 8.0 * fs;
		float E = (float)(rt / meanLen) * (0.002f + 0.0095f * fu - 0.004f * fu * fu);
		float rc = 0.057f * std::sqrt(V / rt);
		tailGain = 1.f / (rc * std::sqrt(std::max(E, 1e-4f)));
		float gd = tapGT[0][0];
		float er = 0.f;
		for (int i = 1; i < NTAPS; i++) er += tapGT[0][i] * tapGT[0][i];
		float tot = std::sqrt(gd * gd + er + 1.f / (rc * rc));
		roomNorm = 1.f / std::pow(tot, 0.75f);
		tapsSettled = false;
		if (houseFresh) {
			std::memcpy(tapD, tapDT, sizeof(tapD));
			std::memcpy(tapG, tapGT, sizeof(tapG));
			houseFresh = false;
		}
	}

	// ------------------------------------------------------------ playing
	float noise() {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
		return (int32_t)rng * (1.f / 2147483648.f);
	}

	void addNoise(int dest, float fc, float q, float attS, float decS, float gain, float waitS = 0.f) {
		if (gain <= 0.f) return;
		int best = 0;
		float bestEnv = 1e9f;
		for (int i = 0; i < NNOISE; i++) {
			if (!ev[i].on) { best = i; bestEnv = -1.f; break; }
			if (ev[i].env < bestEnv) { bestEnv = ev[i].env; best = i; }
		}
		NoiseEvent& e = ev[best];
		e.on = true;
		e.pulse = false;
		e.dest = dest;
		e.wait = (int)(waitS * fs);
		e.att = std::max(1, (int)(attS * fs));
		e.n = 0;
		e.env = 0.f;
		e.dec = std::exp(-6.91f / (decS * fs));
		e.gain = gain;
		e.bp.bandpass(fc, q, fs);
		e.bp.clear();
	}

	// a smooth knock of length durS, through a resonant filter (lowpass when q < 0)
	void addPulse(int dest, float fc, float q, float durS, float gain, float waitS = 0.f) {
		if (gain <= 0.f) return;
		int best = 0;
		float bestEnv = 1e9f;
		for (int i = 0; i < NNOISE; i++) {
			if (!ev[i].on) { best = i; bestEnv = -1.f; break; }
			if (ev[i].env < bestEnv) { bestEnv = ev[i].env; best = i; }
		}
		NoiseEvent& e = ev[best];
		e.on = true;
		e.pulse = true;
		e.dest = dest;
		e.wait = (int)(waitS * fs);
		e.att = std::max(2, (int)(durS * fs));
		e.n = 0;
		e.env = 1.f;
		e.gain = gain;
		if (q < 0.f) e.bp.lowpass(fc, -q, fs); else e.bp.bandpass(fc, q, fs);
		e.bp.clear();
	}

	float velCurve(float v) const {
		return std::pow(std::max(v, 0.f), std::exp((0.5f - c.touch) * 1.6f));
	}

	void bankGate(int n) {
		float g = (held[n] || keyActive[n]) ? 0.f : 1.f;
		kinG[n * 2] = kin[n * 2] * g;
		kinG[n * 2 + 1] = kin[n * 2 + 1] * g;
	}

	Voice* voiceFor(int n) {
		for (int i = 0; i < NVOICES; i++)
			if (voices[i].active && voices[i].key == n) return &voices[i];
		return nullptr;
	}

	Voice* allocVoice(int n) {
		Voice* v = voiceFor(n);
		if (v) return v;
		for (int i = 0; i < NVOICES; i++)
			if (!voices[i].active) { v = &voices[i]; break; }
		if (!v) {
			// steal the quietest note that is not held
			float best = 1e30f;
			for (int i = 0; i < NVOICES; i++) {
				Voice& w = voices[i];
				float l = (w.since < 4096 ? 1e9f : w.level) * (held[w.key] ? 100.f : 1.f);
				if (l < best) { best = l; v = &w; }
			}
		}
		if (v->active && v->key >= 0) { keyActive[v->key] = 0; bankGate(v->key); }
		v->clear();
		keyActive[n] = 1;
		bankGate(n);
		v->active = true;
		v->key = n;
		v->damp = 0.f;
		v->since = 0;
		return v;
	}

	// a key goes down. vel 0..1; vel about 0 = pressed silently (damper lifts, no hammer)
	void noteOn(int n, float vel, float frac = 0.f) {
		n = std::max(0, std::min(NKEYS - 1, n));
		applyControls(true);   // the piano as it is right now
		held[n]++;
		bankGate(n);
		if (!c.freePitch) frac = 0.f;
		KeyModel& m = keys[n];
		Voice* v = voiceFor(n);
		if (m.version != version || std::fabs(m.frac - frac) > 1e-4f) {
			buildKey(n, frac);
		}
		if (!v) v = allocVoice(n);
		v->m = &m;
		v->nB = m.nB;
		v->since = 0;
		if (vel < 0.02f)
			return;   // silent press
		float ve = velCurve(vel) * m.velGain;
		float hv = 0.3f + 4.7f * std::pow(std::min(ve, 1.2f), 1.6f);
		if (c.soft) hv *= 0.6f;
		v->hv = hv;
		v->wait = (int)(m.delay * fs);
		v->contact = false;
		v->hT = 0;
		// key bottoms on the keybed just before the hammer lands
		float nz = c.noise * keyNoise[n] * (0.6f + 0.8f * c.wear);
		addPulse(TO_BOARD, 160.f + 60.f * urand(c.serial, n, F_NOISE + 5), -0.8f, 0.005f, THUMP * nz * std::pow(vel, 1.5f) * (1.f - 0.4f * n / 87.f), std::max(0.f, m.delay - 0.0012f));
		if (c.wear > 0.3f && urand(c.serial, n, F_CLICK) < c.wear - 0.2f)
			addPulse(TO_AIR, 2600.f, 3.f, 0.0004f, TICK * nz * vel, 0.f);
		if (c.hammer > 0.85f && !c.felt)
			addPulse(TO_AIR, 4800.f, 2.f, 0.0002f, TICK * 4.f * (c.hammer - 0.85f) * 6.f * ve, m.delay);
		if (c.felt)
			addNoise(TO_AIR, 3000.f, 0.7f, 0.002f, 0.012f, RUSTLE * nz * vel, m.delay);
	}

	void noteOff(int n) {
		n = std::max(0, std::min(NKEYS - 1, n));
		if (held[n] > 0) held[n]--;
		bankGate(n);
		if (held[n] > 0) return;
		float nz = c.noise * keyNoise[n] * (0.6f + 0.8f * c.wear);
		// key and hammer come back: a light double knock
		addPulse(TO_BOARD, 200.f, -0.8f, 0.004f, THUMP * 0.09f * nz, 0.f);
		addPulse(TO_AIR, 1400.f, 3.f, 0.0005f, TICK * 0.15f * nz, 0.012f);
		if (n < nNoDamper && pedalS < 0.5f) {
			Voice* v = voiceFor(n);
			if (v && v->level > 1e-4f)
				addNoise(TO_AIR, 450.f, 0.8f, 0.004f, 0.02f, RUSTLE * 0.5f * nz, 0.f);
		}
	}

	// ------------------------------------------------------------ one sample
	template <bool DAMP, bool PH>
	static inline void strings(Voice& v, const KeyModel& m, float da, float drive, float& outRet, float& slopeRet) {
		float out = 0.f, slope = 0.f;
		const int S = m.S;
		// coupled unisons
		if (m.Kc > 0) {
			const int KC = (m.Kc + 3) & ~3;   // the slots past Kc are zero
			float sr[MAXC], si[MAXC];
			for (int k = 0; k < KC; k++) { sr[k] = 0.f; si[k] = 0.f; }
			for (int j = 0; j < S; j++) {
				float* zr = v.czr + j * MAXC;
				float* zi = v.czi + j * MAXC;
				const float* pr = m.cpr + j * MAXC;
				const float* pi = m.cpi + j * MAXC;
				const float* dm = m.cdm + j * MAXC;
				const float* sr_ = m.csr + j * MAXC;
				const float* si_ = m.csi + j * MAXC;
				for (int k = 0; k < KC; k++) {
					float a = pr[k] * zr[k] - pi[k] * zi[k];
					float b = pr[k] * zi[k] + pi[k] * zr[k];
					if (DAMP) {
						float g = 1.f - da * dm[k];
						a *= g;
						b *= g;
					}
					a += sr_[k] * drive;
					b += si_[k] * drive;
					zr[k] = a; zi[k] = b;
					sr[k] += a; si[k] += b;
				}
			}
			for (int j = 0; j < S; j++) {
				float* zr = v.czr + j * MAXC;
				float* zi = v.czi + j * MAXC;
				const float* w = m.cw + j * MAXC;
				const float* pw = m.cpw + j * MAXC;
				for (int k = 0; k < KC; k++) {
					zr[k] -= m.ccr[k] * sr[k] - m.cci[k] * si[k];
					zi[k] -= m.ccr[k] * si[k] + m.cci[k] * sr[k];
					out += w[k] * zi[k];
					if (PH) slope += pw[k] * zi[k];
				}
			}
		}
		// free modes
		{
			const int nb = v.nB;
			float* zr = v.bzr;
			float* zi = v.bzi;
			for (int i = 0; i < nb; i++) {
				float a = m.bpr[i] * zr[i] - m.bpi[i] * zi[i];
				float b = m.bpr[i] * zi[i] + m.bpi[i] * zr[i];
				if (DAMP) {
					float g = 1.f - da * m.bdm[i];
					a *= g;
					b *= g;
				}
				a += m.bsr[i] * drive;
				b += m.bsi[i] * drive;
				zr[i] = a; zi[i] = b;
				out += m.bw[i] * b;
				if (PH) slope += m.bpw[i] * b;
			}
		}
		outRet = out;
		slopeRet = slope;
	}

	inline void voiceProcess(Voice& v, float drive) {
		const KeyModel& m = *v.m;
		v.since++;
		// damper
		bool free = held[v.key] > 0 || !m.hasDamper;
		float target = free ? 0.f : (1.f - pedalS) * (1.f - pedalS);
		float rate = target > v.damp ? dt / m.damperTime : dt / 0.012f;
		v.damp += std::max(-rate, std::min(rate, target - v.damp));
		const float da = v.damp;

		float out = 0.f, slope = 0.f;
		// the same arithmetic in four versions, so a lifted damper or a key
		// without phantoms costs nothing
		if (da > 0.f) {
			if (m.phantom) strings<true, true>(v, m, da, drive, out, slope);
			else strings<true, false>(v, m, da, drive, out, slope);
		} else {
			if (m.phantom) strings<false, true>(v, m, da, drive, out, slope);
			else strings<false, false>(v, m, da, drive, out, slope);
		}
		// hammer
		if (v.wait >= 0) {
			if (v.wait == 0) {
				float y = hammerY(v);
				v.contact = true;
				v.u = y - v.hv * dt * 0.5f;
				v.uPrev = v.u - v.hv * dt;
				v.xPrev = 0.f;
				v.F = 0.f;
				v.hT = 0;
				v.hFree = 0;
			}
			v.wait--;
		}
		if (v.contact)
			out += hammerStep(v);

		// tension modulation: phantom partials and the longitudinal clang
		if (m.phantom) {
			float sq = slope * slope;
			v.hpState += 0.004f * (sq - v.hpState);
			float l = sq - v.hpState;
			float a = m.plr * v.plzr - m.pli * v.plzi + m.plIn * l;
			float b = m.plr * v.plzi + m.pli * v.plzr;
			v.plzr = a; v.plzi = b;
			out += (l + a * 3.f) * PHANTOM;
		}
		v.out = out;
	}

	float hammerY(const Voice& v) const {
		const KeyModel& m = *v.m;
		float y = 0.f;
		for (int i = 0; i < MAXS * MAXC; i++) y += m.chw[i] * v.czi[i];
		for (int i = 0; i < v.nB; i++) y += m.bhw[i] * v.bzi[i];
		return y;
	}

	// returns the extra bridge force from the hammer's push this sample
	float hammerStep(Voice& v) {
		const KeyModel& m = *v.m;
		float yFree = hammerY(v);
		float uPred = 2.f * v.u - v.uPrev;
		if (v.hT > 0 && v.F <= 0.f) uPred -= 300.f * dt * dt;   // return spring once the blow is over
		float A = dt * dt / m.hm + m.G;
		float X0 = uPred - yFree;
		float F = 0.f;
		if (X0 > 0.f) {
			F = std::max(0.f, v.F);
			for (int it = 0; it < 5; it++) {
				float x = X0 - A * F;
				if (x <= 0.f) { F = 0.5f * F; continue; }
				float xd = (x - v.xPrev) / dt;
				float damp = std::max(0.f, 1.f + m.hmu * xd);
				float xp = std::pow(x, m.hp);
				float g = m.hK * xp * damp - F;
				float dg = -m.hK * (m.hp * xp / x * A * damp + xp * (damp > 0.f ? m.hmu * A / dt : 0.f)) - 1.f;
				float Fn = F - g / dg;
				F = std::max(0.f, Fn);
			}
		}
		float x = X0 - A * F;
		v.xPrev = std::max(0.f, x);
		v.uPrev = v.u;
		v.u = uPred - dt * dt / m.hm * F;
		v.F = F;
		v.hT++;
		float add = 0.f;
		if (F > 0.f) {
			v.hFree = 0;
			// push the force into the strings
			float* zr = v.czr; float* zi = v.czi;
			for (int i = 0; i < MAXS * MAXC; i++) {
				zr[i] += m.cbr[i] * F;
				zi[i] += m.cbi[i] * F;
				add += m.cw[i] * m.cbi[i] * F;
			}
			for (int i = 0; i < v.nB; i++) {
				v.bzr[i] += m.bbr[i] * F;
				v.bzi[i] += m.bbi[i] * F;
				add += m.bw[i] * m.bbi[i] * F;
			}
		} else {
			v.hFree++;
		}
		if ((v.hFree > (int)(0.004f * fs) && v.hT > (int)(0.002f * fs)) || v.hT > (int)(0.05f * fs))
			v.contact = false;
		return add;
	}

	void process(const Controls& nc, float ext) {
		setControls(nc);

		// pedal noises
		float pd = pedalS - pedalPrev;
		if (std::fabs(pd) > 0.002f) {
			float nz = c.noise * (0.6f + 0.8f * c.wear);
			if (pedalPrev < 0.3f && pedalS >= 0.3f) {
				addPulse(TO_BOARD, 140.f, -0.8f, 0.008f, THUMP * 0.6f * nz);
				addNoise(TO_BUS, 3000.f, 0.5f, 0.03f, 0.15f, 1.2f * c.symp);
				addNoise(TO_AIR, 2500.f, 0.6f, 0.03f, 0.12f, RUSTLE * 0.3f * nz);
				if (c.wear > 0.5f && noise() > 0.4f)
					addNoise(TO_AIR, 1700.f + 600.f * noise(), 14.f, 0.02f, 0.09f, RUSTLE * nz * (c.wear - 0.5f) * 2.f);
			}
			if (pedalPrev > 0.3f && pedalS <= 0.3f) {
				addPulse(TO_BOARD, 170.f, -0.8f, 0.008f, THUMP * 0.45f * nz);
				addNoise(TO_AIR, 500.f, 0.8f, 0.004f, 0.03f, RUSTLE * 0.4f * nz);
			}
			pedalPrev = pedalS;
		}

		// noises
		float nBoard = 0.f, nAir = 0.f, nBus = 0.f;
		for (int i = 0; i < NNOISE; i++) {
			NoiseEvent& e = ev[i];
			if (!e.on) continue;
			if (e.wait > 0) { e.wait--; continue; }
			float x;
			if (e.pulse) {
				float src = 0.f;
				if (e.n < e.att) src = 0.5f - 0.5f * std::cos(2.f * kPi * e.n / e.att);
				e.n++;
				x = e.bp.process(src) * e.gain;
				if (e.n > e.att + (int)(0.06f * fs)) { e.on = false; e.env = 0.f; }
				else e.env = e.n < e.att ? 1.f : 0.5f;
			} else {
				if (e.n < e.att) e.env = (float)(++e.n) / e.att;
				else e.env *= e.dec;
				x = e.bp.process(noise()) * e.env * e.gain;
				if (e.n >= e.att && e.env < 1e-4f) e.on = false;
			}
			if (e.dest == TO_BOARD) nBoard += x; else if (e.dest == TO_AIR) nAir += x; else nBus += x;
		}

		// strings
		float symK = 2.f * c.symp;   // 0.5 = the physical amount
		float bridge = 0.f, sL = 0.f, sR = 0.f;
		float drive0 = (busPrev + ext) * symK;
		for (int i = 0; i < NVOICES; i++) {
			Voice& v = voices[i];
			if (!v.active) continue;
			voiceProcess(v, drive0 - v.out * symK);
			bridge += v.out;
			sL += v.out * (1.f - v.m->pan);
			sR += v.out * v.m->pan;
			// housekeeping
			if ((v.since & 255) == 0) {
				float e = 0.f;
				for (int k = 0; k < MAXS * MAXC; k++) e += v.czr[k] * v.czr[k] + v.czi[k] * v.czi[k];
				for (int k = 0; k < v.nB; k++) e += v.bzr[k] * v.bzr[k] + v.bzi[k] * v.bzi[k];
				v.level = e;
				// drop dead high modes off the end
				while (v.nB > 4) {
					int k = v.nB - 1;
					float ek = v.bzr[k] * v.bzr[k] + v.bzi[k] * v.bzi[k];
					if (ek > 1e-18f * (e + 1e-30f) * 1e6f || v.since < (int)(0.05f * fs)) break;
					v.bzr[k] = v.bzi[k] = 0.f;
					v.nB--;
				}
				if (!v.contact && v.wait < 0 && e < 1e-17f && v.since > (int)(0.05f * fs) && !held[v.key]) {
					v.active = false;
					keyActive[v.key] = 0;
					bankGate(v.key);
				}
			}
		}
		busPrev = bridge;

		// sympathetic strings of keys nobody is playing
		float bank = 0.f;
		{
			float pedal = pedalS;
			float drive = (bridge + nBus * 3.f + ext) * c.symp * 0.006f;
			bool any = pedal > 0.01f || bankBusy;
			int nStart = any ? 0 : nNoDamper;
			float energy = 0.f;
			if (pedal != kggPedal) {
				kggPedal = pedal;
				for (int n = 0; n < NKEYS; n++) {
					float da = n < nNoDamper ? (1.f - pedal) * (1.f - pedal) : 0.f;
					for (int p = 0; p < 2; p++) {
						kgg[n * 2 + p] = 1.f - da * kdm[n * 2 + p];
						klift[n * 2 + p] = 1.f - da;   // a string with its damper down hardly answers at all
					}
				}
			}
			for (int i = nStart * 2; i < NBANK; i++) {
				float a = (kpr[i] * kzr[i] - kpi[i] * kzi[i]) * kgg[i] + kinG[i] * klift[i] * drive;
				float b = (kpr[i] * kzi[i] + kpi[i] * kzr[i]) * kgg[i];
				kzr[i] = a; kzi[i] = b;
				bank += kw[i] * b;
				energy += a * a + b * b;
			}
			if (ctlCount == 0) bankBusy = pedal > 0.01f || energy > 1e-14f;
		}

		// soundboard
		float boardIn = bridge + bank * 0.6f + nBoard * 300.f;   // knocks: about 0.1 .. 30 N into the wood
		float x = boardHP.process(boardIn);
		x = boardHP2.process(x);
		float mL = 0.f, mR = 0.f;
		for (int i = 0; i < NBOARD; i++) {
			float a = bpr_[i] * bzr[i] - bpi_[i] * bzi[i] + bgin[i] * boardIn;
			float b = bpr_[i] * bzi[i] + bpi_[i] * bzr[i];
			bzr[i] = a; bzi[i] = b;
			mL += bgl[i] * b;
			mR += bgr[i] * b;
		}
		x += 1.5f * radShelf.hp(x);   // the board radiates highs better than lows
		float top = boardTop.lp(x);
		float bL = top * 0.8f + mL * 0.7f;
		float bR = top * 0.8f + mR * 0.7f;
		bL -= c.age * 0.37f * ageLow[0].lp(bL);
		bR -= c.age * 0.37f * ageLow[1].lp(bR);

		// rattle: something in the room lifts off its seat when its note comes by
		float rattle = 0.f;
		if (rattleOn > 0.f) {
			float rb = rattleBP.process((bL + bR) * outScale);
			float hit = std::min(1.f, std::max(0.f, std::fabs(rb) - 0.15f));
			rattle = rattleHP.process(hit) * 0.5f * rattleOn * c.noise;
		}

		// cabinet
		float shelf = c.lid == 0 ? 0.35f : c.lid == 1 ? 0.7f : 1.f;
		float boxAmt = c.lid == 0 ? 0.5f : c.lid == 1 ? 0.25f : 0.f;
		float cab[2] = {bL, bR};
		for (int ch = 0; ch < 2; ch++) {
			float lp = lidLP[ch].lp(cab[ch]);
			float y = lp + shelf * (cab[ch] - lp);
			y += boxAmt * (box1[ch].process(cab[ch]) + 0.7f * box2[ch].process(cab[ch]));
			cab[ch] = y;
		}
		// wall behind
		{
			int wp = wallPos;
			for (int ch = 0; ch < 2; ch++) {
				wallBuf[ch][wp] = cab[ch];
				float rp = wp - wallDelay;
				if (rp < 0) rp += 4096.f;
				int i0 = (int)rp;
				float fr = rp - i0;
				float d = wallBuf[ch][i0 & 4095] * (1.f - fr) + wallBuf[ch][(i0 + 1) & 4095] * fr;
				cab[ch] += 0.5f * wallLP[ch].lp(d);
			}
			wallPos = (wallPos + 1) & 4095;
		}
		float actionMul = c.lid == 2 ? 1.6f : c.lid == 1 ? 1.f : 0.6f;
		float airN = nAir * actionMul * 60.f + rattle;
		float srcL = cab[0] * outScale + airN, srcR = cab[1] * outScale + airN;
		outDry = (srcL + srcR) * 0.5f * c.level;

		// room
		float mono = (srcL + srcR) * 0.5f;
		roomBuf[roomPos] = mono;
		float early[2] = {0.f, 0.f};
		// reflections glide to new places when the room or the mics move (Doppler and all);
		// once they have arrived the glide is skipped
		if (!tapsSettled) {
			float err = 0.f;
			for (int ch = 0; ch < 2; ch++)
				for (int i = 0; i < NTAPS; i++) {
					tapD[ch][i] += 0.0005f * (tapDT[ch][i] - tapD[ch][i]);
					tapG[ch][i] += 0.0005f * (tapGT[ch][i] - tapG[ch][i]);
					err = std::max(err, std::max(std::fabs(tapDT[ch][i] - tapD[ch][i]) * 1e-3f, std::fabs(tapGT[ch][i] - tapG[ch][i])));
				}
			if (err < 1e-7f) {
				std::memcpy(tapD, tapDT, sizeof(tapD));
				std::memcpy(tapG, tapGT, sizeof(tapG));
				tapsSettled = true;
			}
		}
		for (int ch = 0; ch < 2; ch++) {
			for (int i = 1; i < NTAPS; i++) {
				float rp = roomPos - tapD[ch][i];
				if (rp < 0) rp += 65536.f;
				int i0 = (int)rp;
				float fr = rp - i0;
				early[ch] += tapG[ch][i] * (roomBuf[i0 & 65535] * (1.f - fr) + roomBuf[(i0 + 1) & 65535] * fr);
			}
			early[ch] = earlyLP[ch].lp(early[ch]);
		}
		roomPos = (roomPos + 1) & 65535;
		// tail
		float fo[8], sum = 0.f;
		for (int i = 0; i < 8; i++) {
			int rp = fdnPos[i] - fdnLen[i];
			if (rp < 0) rp += 8192;
			fo[i] = fdnLP[i].lp(fdnBuf[i][rp]) * fdnG[i];
			sum += fo[i];
		}
		sum *= 0.25f;
		float tL = 0.f, tR = 0.f;
		for (int i = 0; i < 8; i++) {
			float in = fo[i] - sum + mono * ((i & 1) ? 0.35f : -0.35f);
			fdnBuf[i][fdnPos[i]] = in;
			fdnPos[i] = (fdnPos[i] + 1) & 8191;
			if (i & 2) tL += fo[i]; else tR += fo[i];
			if (i & 4) tL -= fo[i] * 0.5f; else tR -= fo[i] * 0.5f;
		}
		float door = doorLP.process(mono) * 0.05f;
		float dir[2] = {srcL, srcR};
		float res[2];
		float closeS[2] = {(sL + nBoard * 300.f) * outScale, (sR + nBoard * 300.f) * outScale};
		for (int ch = 0; ch < 2; ch++) {
			float dch = directLP[ch].lp(dir[ch]) * tapG[ch][0];
			float tail = (ch ? tR : tL) * tailGain;
			float near = (dch + early[ch] + tail) * roomNorm;
			if (closeMix > 0.f) {
				float cs = closeS[ch] - closeHP[ch].lp(closeS[ch]);
				near = near * (1.f - 0.85f * closeMix) + (cs * 2.2f + airN) * closeMix;
			}
			if (nextRoom > 0.f) {
				float through = nextLP[ch].process((tail * 0.7f + early[ch] * 0.2f) * roomNorm) + door;
				near = near * (1.f - nextRoom) + through * nextRoom;
			}
			res[ch] = near * c.level;
		}
		outL = res[0];
		outR = res[1];
		float a = std::fabs(outL) + std::fabs(outR);
		meter = std::max(a * 0.2f, meter * 0.9995f);
	}
};

}  // namespace joanna
