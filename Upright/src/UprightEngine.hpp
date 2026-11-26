// Upright: a dry, convincing upright piano core. No Rack dependency.
//
// Signal path per key (all modal: every partial is a complex one-pole
// resonator z <- p z + g F, read out as Im(z)):
//
//   hammer force pulse F(t)  (felt: shorter and sharper when played harder)
//     -> string partials (stretched by inharmonicity, 1-3 detuned strings,
//        strike-point comb, soundboard colouring, pitch dependent decay)
//     -> "aftersound" partials (the slow second stage of the decay)
//     -> longitudinal bass modes (the metallic clank of hard bass notes)
//   the same F(t) also knocks a shared soundboard (woody body of the attack)
//   -> stereo by key position and soundboard position -> optional small room
//
// Units: Hz, seconds, dB/s. Output is roughly +-1 for loud chords (the Rack
// module scales it to +-5 V).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace upright {

static const float PI = 3.14159265358979f;

struct Rng {
	uint32_t s;
	explicit Rng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

static inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
static inline float lerpTable(const float* xs, const float* ys, int n, float x) {
	if (x <= xs[0]) return ys[0];
	for (int i = 1; i < n; i++)
		if (x <= xs[i]) {
			float t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
			return ys[i - 1] + t * (ys[i] - ys[i - 1]);
		}
	return ys[n - 1];
}

// ---------------------------------------------------------------- the piano
// m = MIDI note number as a float (21 = A0, 60 = C4, 108 = C8).

// Inharmonicity coefficient B of a small upright (short, stiff bass strings).
static inline float inharmB(float m) {
	static const float xs[] = {21, 33, 48, 60, 72, 84, 96, 108};
	static const float ys[] = {-3.05f, -3.45f, -3.85f, -3.7f, -3.35f, -2.95f, -2.55f, -2.1f};
	return std::pow(10.f, lerpTable(xs, ys, 8, m));
}
// Railsback stretch of a tuned piano, in cents.
static inline float stretchCents(float m) {
	float d = (m - 62.f) / 12.f;
	return d < 0.f ? -3.f * d * d : 2.2f * d * d;
}
static inline int stringsFor(float m) { return m < 29.f ? 1 : (m < 41.f ? 2 : 3); }
// Strike point as a fraction of the string length.
static inline float strikePoint(float m) {
	static const float xs[] = {21, 60, 108};
	static const float ys[] = {0.122f, 0.115f, 0.075f};
	return lerpTable(xs, ys, 3, m);
}
// Hammer contact time at mezzo-forte (seconds).
static inline float contactTime(float m) {
	static const float xs[] = {21, 48, 60, 84, 96, 108};
	static const float ys[] = {2.0e-3f, 1.5e-3f, 1.3e-3f, 0.75e-3f, 0.5e-3f, 0.32e-3f};
	return lerpTable(xs, ys, 6, m);
}
// Two-stage decay, measured against a recorded piano. Stage one ("prompt
// sound") is fast; once the strings in a unison drift out of phase the
// bridge stops draining them and a quiet "aftersound" rings on (Weinreich).
// dB/s of the prompt sound at low frequencies:
static inline float promptRate(float m) {
	static const float xs[] = {21, 30, 36, 48, 60, 72, 84, 96, 108};
	static const float ys[] = {4.f, 11.f, 19.f, 23.f, 25.f, 27.f, 28.f, 30.f, 31.f};
	return lerpTable(xs, ys, 9, m);
}
static inline float afterRate(float m) {
	static const float xs[] = {21, 36, 60, 84, 96, 108};
	static const float ys[] = {2.0f, 2.3f, 2.f, 1.6f, 1.8f, 2.2f};
	return lerpTable(xs, ys, 6, m);
}
// level of the aftersound relative to the prompt sound (amplitude)
static inline float afterLevel(float m) {
	static const float xs[] = {21, 29, 36, 60, 84, 108};
	static const float ys[] = {0.35f, 0.28f, 0.22f, 0.13f, 0.07f, 0.1f};
	return lerpTable(xs, ys, 6, m);
}
// Damper: T60 once the key is released (no dampers above about F#6).
static inline float damperDbPerSec(float m) {
	if (m >= 90.f) return 0.f;
	static const float xs[] = {21, 48, 72, 89};
	static const float ys[] = {0.55f, 0.32f, 0.18f, 0.14f};
	return 60.f / lerpTable(xs, ys, 4, m);
}

// Static soundboard colouring of the strings: a small upright board radiates
// little below ~90 Hz, is strongest 150-600 Hz and rolls off gently up top.
// The ripple (+-3 dB, fixed for this instrument) stands for the dense board
// modes; partials on a strong spot also lose energy to the board faster.
struct Board {
	float ph[3];
	void init(uint32_t seed) {
		Rng r(seed);
		for (float& p : ph) p = r.uni();
	}
	float rippleDb(float f) const {
		float l = std::log2(std::max(f, 20.f));
		return 1.8f * std::sin(2.f * PI * (1.3f * l + ph[0]))
		     + 1.2f * std::sin(2.f * PI * (3.1f * l + ph[1]))
		     + 0.8f * std::sin(2.f * PI * (7.3f * l + ph[2]));
	}
	float gain(float f) const {
		float fc = 110.f;
		float x = f / fc;
		float hp = (x * x) / std::sqrt((1.f - x * x) * (1.f - x * x) + 2.f * x * x); // 2nd order, Q 0.7
		float mid = 1.f + 0.6f * std::exp(-0.5f * std::pow(std::log2(f / 320.f) / 1.1f, 2.f)); // body hump
		float lp = 1.f / std::sqrt(1.f + std::pow(f / 3200.f, 2.f));
		return hp * mid * lp * std::pow(10.f, rippleDb(f) / 20.f);
	}
};

// ---------------------------------------------------------------- one voice
static const int MAXOSC = 320;
static const int MAXPULSE = 2048;

struct Voice {
	// structure-of-arrays oscillator bank
	alignas(16) float re[MAXOSC], im[MAXOSC];
	alignas(16) float pr[MAXOSC], pi[MAXOSC];      // pole now
	alignas(16) float prU[MAXOSC], piU[MAXOSC];    // undamped pole
	alignas(16) float prD[MAXOSC], piD[MAXOSC];    // damped pole
	alignas(16) float g[MAXOSC];                   // drive gain (string modes)
	alignas(16) float gl[MAXOSC];                  // drive gain (longitudinal, F^2)
	alignas(16) float wl[MAXOSC], wr[MAXOSC];
	int n = 0;
	float pulse[MAXPULSE];
	int pulseLen = 0, pulsePos = 0;
	float pulseSq = 0.f;          // scale for the F^2 drive
	float boardW[48];             // where this key's bridge excites the board
	float knock = 0.f;            // how much this key knocks the board
	bool active = false;
	bool held = false;            // key down
	bool damped = false;
	int channel = -1;
	float m = 60.f;
	float age = 0.f;              // seconds since strike (for stealing)
	float level = 0.f;            // rough amplitude, for stealing and culling
};

struct Controls {
	float tone = 0.f;     // -1 soft felt .. +1 hard felt
	float detune = 0.25f; // 0 freshly tuned unisons .. 1 old piano
	float reverb = 0.f;   // 0 dry .. 1
	float volume = 1.f;
};

// ---------------------------------------------------------------- small room
// 8-line FDN, Hadamard mix, damped. Deliberately plain: a little air, not a
// "space". Wet only.
struct Room {
	static const int N = 8;
	float* buf[N] = {};
	int len[N] = {}, pos[N] = {};
	float lp[N] = {};
	float g = 0.f, damp = 0.f;
	float pre[4096] = {};
	int prePos = 0, preLen = 1;
	void init(float sr) {
		static const float ms[N] = {29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.3f, 67.1f, 73.3f};
		for (int i = 0; i < N; i++) {
			delete[] buf[i];
			len[i] = std::max(8, (int)(ms[i] * 1e-3f * sr * 0.8f));
			buf[i] = new float[len[i]]();
			pos[i] = 0;
			lp[i] = 0.f;
		}
		float rt = 1.1f;                                   // seconds
		float meanLen = 0.f;
		for (int i = 0; i < N; i++) meanLen += len[i];
		meanLen /= N * sr;
		g = std::pow(10.f, -3.f * meanLen / rt);
		damp = std::exp(-2.f * PI * 4500.f / sr);
		preLen = std::min(4095, (int)(0.012f * sr));
		std::memset(pre, 0, sizeof(pre));
	}
	~Room() { for (auto b : buf) delete[] b; }
	void process(float inL, float inR, float& outL, float& outR) {
		pre[prePos] = 0.5f * (inL + inR);
		int rp = prePos - preLen; if (rp < 0) rp += 4096;
		float x = pre[rp];
		prePos = (prePos + 1) & 4095;
		float o[N];
		for (int i = 0; i < N; i++) o[i] = buf[i][pos[i]];
		// fast Hadamard
		float h[N];
		for (int i = 0; i < N; i++) h[i] = o[i];
		for (int s = 1; s < N; s <<= 1)
			for (int i = 0; i < N; i += 2 * s)
				for (int j = i; j < i + s; j++) { float a = h[j], b = h[j + s]; h[j] = a + b; h[j + s] = a - b; }
		const float norm = 0.35355339f;
		for (int i = 0; i < N; i++) {
			float v = h[i] * norm * g;
			lp[i] = v + damp * (lp[i] - v);
			buf[i][pos[i]] = lp[i] + x * ((i & 1) ? 0.3f : -0.3f);
			if (++pos[i] >= len[i]) pos[i] = 0;
		}
		outL = o[0] + o[2] - o[4] + o[6];
		outR = o[1] - o[3] + o[5] + o[7];
		outL *= 0.5f; outR *= 0.5f;
	}
};

// ---------------------------------------------------------------- engine
struct Engine {
	static const int MAXV = 32;
	static const int NB = 48;          // soundboard knock modes
	float sr = 48000.f, dt = 1.f / 48000.f;
	Voice voices[MAXV];
	Board board;
	// shared soundboard knock bank
	float bRe[NB] = {}, bIm[NB] = {}, bPr[NB] = {}, bPi[NB] = {}, bWl[NB] = {}, bWr[NB] = {};
	float bPhase[NB] = {}, bRate[NB] = {};
	Room room;
	float keyNorm[128];               // loudness normalisation per key
	bool sustain = false;
	float outL = 0.f, outR = 0.f;
	int cullCounter = 0;
	uint32_t seed = 0x51ee7a11u;
	Controls lastC;

	Engine() { setSampleRate(48000.f); }

	void setSampleRate(float s) {
		sr = s; dt = 1.f / s;
		board.init(seed);
		for (auto& v : voices) { v.active = false; v.n = 0; }
		initKnockBank();
		room.init(sr);
		// loudness per key at mezzo-forte, neutral tone
		Controls c;
		for (int m = 0; m < 128; m++) keyNorm[m] = 1.f;
		for (int m = 21; m <= 108; m++) {
			Voice& v = voices[0];
			build(v, (float)m, 0.5f, c, 0);
			double e = 0.0;
			for (int j = 0; j < v.n; j++) {
				// response amplitude of each mode to the pulse
				double w = std::atan2(v.piU[j], v.prU[j]);
				double cr = 0, ci = 0;
				for (int k = 0; k < v.pulseLen; k++) { cr += v.pulse[k] * std::cos(w * k); ci -= v.pulse[k] * std::sin(w * k); }
				double a = v.g[j] * std::sqrt(cr * cr + ci * ci);
				double rate = -std::log(std::max(1e-9, (double)std::hypot(v.prU[j], v.piU[j]))) * sr; // 1/s
				// weight by how long it rings (energy over the first ~1.5 s), so
				// loudness is judged as heard, not just the first instant
				double T = 1.5;
				double ring = (1.0 - std::exp(-2 * rate * T)) / (2 * rate * T);
				double wsum = 0.5 * (v.wl[j] + v.wr[j]);
				e += a * a * wsum * wsum * ring;
			}
			float r = (float)std::sqrt(e);
			// real uprights: treble a little quieter, top octave quieter still
			float tilt = std::pow(10.f, (-0.06f * std::max(0.f, m - 60.f) - 0.05f * std::max(0.f, 60.f - m)) / 20.f);
			keyNorm[m] = r > 0.f ? 0.02f * tilt / r : 1.f;
		}
		for (int m = 0; m < 21; m++) keyNorm[m] = keyNorm[21];
		for (int m = 109; m < 128; m++) keyNorm[m] = keyNorm[108];
		voices[0].active = false; voices[0].n = 0;
	}

	void initKnockBank() {
		Rng r(seed ^ 0x8badf00du);
		for (int i = 0; i < NB; i++) {
			// log-spaced with jitter, 70 Hz .. 4 kHz
			float t = (i + 0.5f + 0.4f * r.bi()) / NB;
			float f = 70.f * std::pow(4000.f / 70.f, t);
			float t60 = clampf(0.30f * std::pow(150.f / f, 0.6f), 0.025f, 0.45f);
			float sigma = 6.9078f / t60;
			float rr = std::exp(-sigma * dt);
			bPr[i] = rr * std::cos(2.f * PI * f * dt);
			bPi[i] = rr * std::sin(2.f * PI * f * dt);
			float env = board.gain(f) / (1.f + std::pow(f / 900.f, 1.5f));
			float s = r.uni() < 0.5f ? -1.f : 1.f;
			float pan = 0.6f * r.bi();
			bWl[i] = s * env * (1.f - pan);
			bWr[i] = s * env * (1.f + pan) * (r.uni() < 0.3f ? -1.f : 1.f);
			bPhase[i] = r.uni();
			bRate[i] = 0.5f + 2.5f * r.uni();
			bRe[i] = bIm[i] = 0.f;
		}
	}

	// ---- build a voice for note m at velocity vel (0..1)
	void build(Voice& v, float m, float vel, const Controls& c, uint32_t salt) {
		float mc = clampf(m, 21.f, 108.f);
		int key = (int)std::lround(mc);
		Rng r(seed * 2654435761u + (uint32_t)key * 97u + salt * 0u);   // per key: fixed instrument
		float f0 = 440.f * std::pow(2.f, (m - 69.f) / 12.f) * std::pow(2.f, stretchCents(mc) / 1200.f);
		float B = inharmB(mc) * (1.f + 0.15f * r.bi());
		int S = stringsFor(mc);
		float x0 = strikePoint(mc) * (1.f + 0.04f * r.bi());
		float fmax = std::min(0.43f * sr, 12000.f);

		// hammer: speed ratio about 1:12 from pp to ff; felt makes the contact
		// shorter as the blow gets harder (Stulov / Hall: T ~ v^-0.4)
		float vnorm = 0.05f + 0.95f * std::pow(clampf(vel, 0.f, 1.f), 1.2f);
		float hard = std::pow(2.f, -0.9f * c.tone);
		float Tc = contactTime(mc) * hard * std::pow(vnorm / 0.5f, -0.28f);
		int L = std::max(3, std::min(MAXPULSE, (int)std::ceil(Tc * sr)));
		// felt force pulse. A soft blow is a smooth hump; a hard blow
		// compresses the felt into its stiff range and the force grows a sharp
		// peak (Stulov felt, p ~ 3), which is where the bright, "clanky" top of
		// a loud piano note comes from.
		float vc = clampf(vel, 0.f, 1.f);
		float h = clampf(0.2f + 0.6f * vc * vc + 0.25f * c.tone + 0.2f * clampf((50.f - mc) / 29.f, 0.f, 1.f), 0.f, 0.95f);
		float skew = 0.5f - 0.1f * vc;      // peak position as a fraction
		float tau = 0.09f;                  // cusp width as a fraction of contact
		double area = 0.0;
		for (int k = 0; k < L; k++) {
			float t = (k + 0.5f) / L;
			float u = t < skew ? 0.5f * t / skew : 0.5f + 0.5f * (t - skew) / (1.f - skew);
			float smooth = std::pow(std::sin(PI * u), 2.4f);
			float cusp = std::exp(-std::fabs(t - skew) / tau) * std::sin(PI * u);
			float p = (1.f - h) * smooth + h * cusp;
			v.pulse[k] = p;
			area += p;
		}
		// total impulse: grows a little faster than hammer speed (felt and
		// string both respond more than proportionally)
		float imp = std::pow(vnorm, 1.25f);
		for (int k = 0; k < L; k++) v.pulse[k] *= (float)(imp / area);
		v.pulseLen = L;
		v.pulsePos = 0;
		v.pulseSq = 0.f;

		// unison detuning in cents (fixed per key + DETUNE amount)
		float spread = 0.2f + 4.0f * std::pow(clampf(c.detune, 0.f, 1.f), 1.4f);
		float cents[3];
		if (S == 1) cents[0] = 0.f;
		else if (S == 2) { float d = spread * (0.5f + 0.5f * r.uni()); cents[0] = -0.5f * d; cents[1] = 0.5f * d; }
		else {
			float d1 = spread * (0.5f + 0.5f * r.uni()), d2 = spread * (0.3f + 0.7f * r.uni());
			cents[0] = -d1; cents[1] = 0.15f * spread * r.bi(); cents[2] = d2;
		}
		float keyOffset = 0.6f * spread * r.bi() * 0.5f;   // whole key a touch off
		for (int s = 0; s < S; s++) cents[s] += keyOffset;
		float strDecay[3];
		for (int s = 0; s < S; s++) strDecay[s] = 1.f + 0.12f * r.bi();

		// stereo: player's view, bass left; partials pick up slightly
		// different soundboard positions in the two mics
		float pan = clampf((mc - 64.f) / 44.f, -1.f, 1.f) * 0.5f;
		float panL = 1.41421f * std::cos(0.25f * PI * (1.f + pan));   // constant power
		float panR = 1.41421f * std::sin(0.25f * PI * (1.f + pan));

		float dPrompt = promptRate(mc), dAfter = afterRate(mc), aLvl = afterLevel(mc);
		float dDamp = damperDbPerSec(mc);
		auto pole = [&](float f, float dbs, float& prr, float& pii) {
			float sigma = dbs / 8.6859f;
			float rr = std::exp(-sigma * dt);
			prr = rr * std::cos(2.f * PI * f * dt);
			pii = rr * std::sin(2.f * PI * f * dt);
		};
		int n = 0;
		auto add = [&](float f, float dbs, float gain, float glong, float wL, float wR) {
			if (n >= MAXOSC) return;
			pole(f, dbs, v.prU[n], v.piU[n]);
			pole(f, std::max(dbs, dDamp > 0.f ? dDamp * (1.f + f / 2000.f) : dbs), v.prD[n], v.piD[n]);
			v.pr[n] = v.prU[n]; v.pi[n] = v.piU[n];
			v.re[n] = v.im[n] = 0.f;
			v.g[n] = gain; v.gl[n] = glong;
			v.wl[n] = wL; v.wr[n] = wR;
			n++;
		};

		Rng rp(seed ^ ((uint32_t)key * 7919u));
		for (int k = 1; k <= 110; k++) {
			float fk = k * f0 * std::sqrt(1.f + B * k * k) / std::sqrt(1.f + B);
			if (fk > fmax) break;
			float comb = std::sin(PI * k * x0);
			float bg = board.gain(fk);
			float ripple = board.rippleDb(fk);
			float gk = comb * bg;
			// stage-one decay: rises with frequency (air + internal + bridge)
			float d1 = dPrompt * (1.f + fk / 5000.f) * (1.f + 0.12f * ripple);
			float d2 = dAfter * (1.f + fk / 3000.f) * (1.f + 0.05f * ripple);
			float wobble = 0.12f * rp.bi();
			float wL = panL * (1.f + wobble), wR = panR * (1.f - wobble);
			for (int s = 0; s < S; s++) {
				float fs = fk * std::pow(2.f, (cents[s] + 0.03f * k * rp.bi()) / 1200.f);
				add(fs, d1 * strDecay[s], gk / S, 0.f, wL, wR);
			}
			// aftersound (horizontal polarisation + coupled unison mode)
			if (k <= 16) {
				float fa = fk * std::pow(2.f, (0.5f * (cents[0] + cents[S - 1]) + 0.05f) / 1200.f);
				float lvl = aLvl * (0.8f + 0.5f * clampf(c.detune, 0.f, 1.f));
				add(fa, d2, gk * lvl, 0.f, wL * 0.9f, wR * 1.1f);
			}
		}
		// longitudinal modes (bass and tenor only): driven by F^2, so they
		// come in when the key is struck hard
		if (mc < 58.f) {
			float fL = 560.f * std::pow(2.f, (mc - 21.f) / 30.f);
			float lv = 0.9f * (1.f - (mc - 21.f) / 37.f);
			for (int i = 1; i <= 3; i++) {
				float f = fL * i * (1.f + 0.004f * rp.bi());
				if (f > fmax) break;
				add(f, 28.f + 10.f * i, 0.f, lv / i, panL, panR);
			}
		}
		v.n = n;
		// board knock: where on the board this key's bridge sits
		float pos = (mc - 21.f) / 87.f;
		for (int i = 0; i < NB; i++) v.boardW[i] = std::cos(2.f * PI * (bPhase[i] + bRate[i] * pos));
		v.knock = 0.04f * std::pow(2.f, (mc - 60.f) / 24.f) * (mc > 84.f ? std::pow(2.f, (mc - 84.f) / 18.f) : 1.f);
		v.m = m;
		v.age = 0.f;
		v.level = 1.f;
	}

	// ---- keys
	// One voice per channel; a new note on the same channel lets the old one
	// ring on with its damper down. Restriking the same pitch hits the strings
	// that are still moving.
	void noteOn(int ch, float m, float vel, const Controls& c) {
		// one voice per channel; a repeated key just adds a new strike to the
		// strings (the model is linear, so a second voice on the same key is
		// exactly the same strings struck again)
		for (auto& v : voices)
			if (v.active && v.channel == ch && v.held) release(v);
		float keyGain = keyNorm[(int)clampf(std::round(m), 0.f, 127.f)];
		Voice* v = nullptr;
		for (auto& x : voices) if (!x.active) { v = &x; break; }
		if (!v) {   // steal the quietest released voice, else the oldest
			float best = 1e9f;
			for (auto& x : voices) { float s = x.level + (x.held ? 10.f : 0.f); if (s < best) { best = s; v = &x; } }
		}
		build(*v, m, vel, c, 0);
		for (int j = 0; j < v->n; j++) { v->g[j] *= keyGain; v->gl[j] *= keyGain; }
		v->active = true; v->held = true; v->damped = false; v->channel = ch;
	}
	void noteOff(int ch) {
		for (auto& v : voices) if (v.active && v.channel == ch && v.held) { v.held = false; if (!sustain) damp(v, true); }
	}
	void release(Voice& v) { v.held = false; if (!sustain) damp(v, true); }
	void damp(Voice& v, bool on) {
		if (v.damped == on) return;
		v.damped = on;
		for (int j = 0; j < v.n; j++) { v.pr[j] = on ? v.prD[j] : v.prU[j]; v.pi[j] = on ? v.piD[j] : v.piU[j]; }
	}
	void setSustain(bool s) {
		if (s == sustain) return;
		sustain = s;
		for (auto& v : voices) if (v.active && !v.held) damp(v, !s);
	}

	// ---- audio
	void process(const Controls& c) {
		float L = 0.f, R = 0.f, knockIn[NB];
		bool anyKnock = false;
		for (int i = 0; i < NB; i++) knockIn[i] = 0.f;
		for (auto& v : voices) {
			if (!v.active) continue;
			float x = 0.f, x2 = 0.f;
			if (v.pulsePos < v.pulseLen) {
				x = v.pulse[v.pulsePos++];
				// F^2 term, normalised so a mezzo-forte blow gives x2 ~ x * 0.5
				x2 = x * x * (float)v.pulseLen;
				float kx = x * v.knock;
				for (int i = 0; i < NB; i++) knockIn[i] += kx * v.boardW[i];
				anyKnock = true;
			}
			float l = 0.f, r = 0.f;
			const int n = v.n;
			if (x != 0.f) {
				for (int j = 0; j < n; j++) {
					float nr = v.pr[j] * v.re[j] - v.pi[j] * v.im[j] + v.g[j] * x + v.gl[j] * x2;
					float ni = v.pr[j] * v.im[j] + v.pi[j] * v.re[j];
					v.re[j] = nr; v.im[j] = ni;
					l += v.wl[j] * ni; r += v.wr[j] * ni;
				}
			}
			else {
				for (int j = 0; j < n; j++) {
					float nr = v.pr[j] * v.re[j] - v.pi[j] * v.im[j];
					float ni = v.pr[j] * v.im[j] + v.pi[j] * v.re[j];
					v.re[j] = nr; v.im[j] = ni;
					l += v.wl[j] * ni; r += v.wr[j] * ni;
				}
			}
			L += l; R += r;
		}
		// soundboard knock
		float kl = 0.f, kr = 0.f;
		for (int i = 0; i < NB; i++) {
			float nr = bPr[i] * bRe[i] - bPi[i] * bIm[i] + (anyKnock ? knockIn[i] : 0.f);
			float ni = bPr[i] * bIm[i] + bPi[i] * bRe[i];
			bRe[i] = nr; bIm[i] = ni;
			kl += bWl[i] * ni; kr += bWr[i] * ni;
		}
		L += kl; R += kr;

		// culling every 64 samples: drop modes that have died away
		if (++cullCounter >= 64) {
			cullCounter = 0;
			for (auto& v : voices) {
				if (!v.active) continue;
				v.age += 64.f * dt;
				if (v.pulsePos < v.pulseLen) continue;
				float maxA = 0.f;
				for (int j = 0; j < v.n;) {
					float a2 = v.re[j] * v.re[j] + v.im[j] * v.im[j];
					float w = std::max(std::fabs(v.wl[j]), std::fabs(v.wr[j]));
					if (a2 * w * w < 1e-12f) {     // -120 dB
						int last = v.n - 1;
						v.re[j] = v.re[last]; v.im[j] = v.im[last];
						v.pr[j] = v.pr[last]; v.pi[j] = v.pi[last];
						v.prU[j] = v.prU[last]; v.piU[j] = v.piU[last];
						v.prD[j] = v.prD[last]; v.piD[j] = v.piD[last];
						v.g[j] = v.g[last]; v.gl[j] = v.gl[last];
						v.wl[j] = v.wl[last]; v.wr[j] = v.wr[last];
						v.n--;
					}
					else { maxA = std::max(maxA, a2 * w * w); j++; }
				}
				v.level = std::sqrt(maxA);
				if (v.n == 0) v.active = false;
			}
		}

		float vol = c.volume;
		float wl = 0.f, wr = 0.f;
		if (c.reverb > 0.001f) {
			room.process(L, R, wl, wr);
			float w = 0.7f * c.reverb;
			L += w * wl; R += w * wr;
		}
		outL = L * vol;
		outR = R * vol;
	}
};

} // namespace upright
