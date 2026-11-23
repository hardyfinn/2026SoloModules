// Menagerie: sixteen sound engines behind one small set of controls.
// Every engine here is our own; none are taken from Mutable Instruments
// Plaits or Braids. MIT licence (see LICENSE.txt).
//
// This header has no Rack dependency so test/render.cpp can build it alone.
// Each engine gets the same controls (Ctl): pitch, SHAPE, TEXTURE, LIFE and
// DECAY, all 0..1, and writes a MAIN and an AUX sample (about +-1).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace mng {

static const float TWO_PI = 6.28318530718f;
static const float PI_F = 3.14159265359f;

enum EngineId {
	GENDY, HERD, DUFFING, SCANNED, WALSH, PLL, BYTEBEAT, AUTOMATON,
	BUBBLES, SPRING, BOUNCE, HOWL, TINE, TONEWHEEL, PAD, HARMONIUM,
	NUM_ENGINES
};

static const char* const ENGINE_NAMES[NUM_ENGINES] = {
	"GENDY", "HERD", "DUFFING", "SCANNED", "WALSH", "PLL", "BYTEBEAT", "AUTOMATON",
	"BUBBLES", "SPRING", "BOUNCE", "HOWL", "TINE", "TONEWHEEL", "PAD", "HARMONIUM"};

// What SHAPE / TEXTURE / LIFE do in each engine (shown on the screen).
static const char* const ENGINE_HINTS[NUM_ENGINES][3] = {
	{"points", "roughness", "wander"},
	{"spread", "pull", "stray"},
	{"wells", "drive", "unrest"},
	{"rest shape", "tension", "hand"},
	{"formant", "brightness", "drift"},
	{"multiple", "loop speed", "slip"},
	{"formula", "shift", "bit flips"},
	{"rule", "gen speed", "mutate"},
	{"size spread", "rise", "density"},
	{"dispersion", "tone", "drips"},
	{"bounciness", "ball", "surface"},
	{"distance", "gain", "walk"},
	{"hammer", "pickup", "tremolo"},
	{"drawbars", "dirt", "vibrato"},
	{"pulse width", "chorus", "PWM"},
	{"stops", "pressure", "bellows"}};

// Engines that make their own decay (the trigger strikes them); the others
// sit behind the built-in envelope when TRIG is patched.
inline bool isPercussive(int m) { return m == BUBBLES || m == SPRING || m == BOUNCE || m == TINE; }

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

struct SineTable {
	float t[4097];
	SineTable() {
		for (int i = 0; i <= 4096; i++) t[i] = (float) std::sin(6.283185307179586 * i / 4096.0);
	}
};
// sine of a phase given in cycles
inline float fsin(float ph) {
	static const SineTable tab;
	ph -= std::floor(ph);
	float x = ph * 4096.f;
	int i = (int) x;
	if (i > 4095) i = 4095;
	float fr = x - i;
	return tab.t[i] + (tab.t[i + 1] - tab.t[i]) * fr;
}

// Band-limiting correction for a jump; t and dt in cycles.
inline float polyblep(float t, float dt) {
	if (dt <= 0.f) return 0.f;
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.f;
	}
	if (t > 1.f - dt) {
		t = (t - 1.f) / dt;
		return t * t + t + t + 1.f;
	}
	return 0.f;
}
inline float frac(float x) { return x - std::floor(x); }

struct Rng {
	uint32_t s;
	Rng() : s(0x9E3779B9u) {}
	void seed(uint32_t x) { s = x ? x : 1u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

struct Ctl {
	float f, sr, dt, dcR;
	float shape, texture, life, decay;
	bool drone;
	Ctl() : f(261.63f), sr(48000.f), dt(1.f / 48000.f), dcR(0.9984f),
		shape(0.5f), texture(0.5f), life(0.f), decay(0.5f), drone(true) {}
	void setRate(float s) { sr = s; dt = 1.f / s; dcR = 1.f - TWO_PI * 12.f / s; }
};

struct DCBlock {
	float x1, y1;
	DCBlock() : x1(0.f), y1(0.f) {}
	void reset() { x1 = y1 = 0.f; }
	float process(float x, float R) { float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

// Zero-delay state-variable filter (Simper/Zavalishin form).
struct SVF {
	float ic1, ic2, g, k, a1, a2, a3;
	SVF() : ic1(0.f), ic2(0.f), g(0.f), k(1.f), a1(0.f), a2(0.f), a3(0.f) {}
	void reset() { ic1 = ic2 = 0.f; }
	void set(float fc, float q, float sr) {
		fc = clampf(fc, 5.f, sr * 0.45f);
		g = std::tan(PI_F * fc / sr);
		k = 1.f / q;
		a1 = 1.f / (1.f + g * (g + k));
		a2 = g * a1;
		a3 = g * a2;
	}
	void process(float v0, float& lp, float& bp, float& hp) {
		float v3 = v0 - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		lp = v2; bp = v1; hp = v0 - k * v1 - v2;
	}
};

template <int N>
struct Delay {
	float buf[N];
	int w;
	Delay() : w(0) { std::memset(buf, 0, sizeof(buf)); }
	void reset() { std::memset(buf, 0, sizeof(buf)); w = 0; }
	void write(float x) { buf[w] = x; w = (w + 1) & (N - 1); }
	// d samples ago (1 = the newest sample)
	float read(float d) const {
		d = clampf(d, 1.f, (float) (N - 3));
		float rp = (float) w - d;
		if (rp < 0.f) rp += (float) N;
		int i = (int) rp;
		float fr = rp - i;
		float a = buf[i & (N - 1)], b = buf[(i + 1) & (N - 1)];
		return a + (b - a) * fr;
	}
};

// A 32-step wave with band-limited steps (Walsh and Automaton).
inline float stepped32(const float* T, float ph, float inc) {
	float x = ph * 32.f;
	int s = (int) x;
	if (s > 31) s = 31;
	float y = T[s];
	if (inc < 1.f / 32.f) {
		float t0 = ph - s / 32.f;
		if (t0 < inc) y += 0.5f * (T[s] - T[(s + 31) & 31]) * polyblep(t0, inc);
		float t1 = ph - (s + 1) / 32.f + 1.f;
		if (t1 > 1.f - inc) y += 0.5f * (T[(s + 1) & 31] - T[s]) * polyblep(t1, inc);
	}
	return y;
}

// =================================================================== 0 GENDY
// Xenakis-style dynamic stochastic synthesis: a cycle is a chain of
// breakpoints; every cycle each breakpoint's height and spacing take a random
// step (mirrored at the edges). The cycle length stays 1/f so it tracks pitch.
// SHAPE = number of points, TEXTURE = height step (clean..raw),
// LIFE = spacing step and pitch jitter. AUX = a soft low-passed copy.
struct Gendy {
	static const int MAXN = 32;
	float amp[MAXN], dur[MAXN];
	float pos, jit, lp;
	int idx, n;
	DCBlock dc;
	Gendy() { Rng r; reset(r); }
	void reset(Rng& r) {
		for (int i = 0; i < MAXN; i++) { amp[i] = r.bi() * 0.7f; dur[i] = 1.f; }
		pos = 0.f; jit = 0.f; lp = 0.f; idx = 0; n = 12; dc.reset();
	}
	void trigger(const Ctl&, float, Rng&) {}
	void newCycle(const Ctl& c, Rng& r) {
		n = 3 + (int) (c.shape * c.shape * 29.f + 0.5f);
		float as = 0.003f + c.texture * c.texture * 0.3f;
		float ds = 0.01f + c.life * c.life * 0.3f;
		for (int i = 0; i < n; i++) {
			float a = amp[i] + r.bi() * as;
			if (a > 1.f) a = 2.f - a;
			if (a < -1.f) a = -2.f - a;
			amp[i] = a;
			float d = dur[i] + r.bi() * ds;
			if (d > 1.8f) d = 3.6f - d;
			if (d < 0.25f) d = 0.5f - d;
			dur[i] = d;
		}
		jit = jit * 0.96f + r.bi() * c.life * c.life * 0.004f;
	}
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		float sum = 0.f;
		for (int i = 0; i < n; i++) sum += dur[i];
		float f = c.f * (1.f + jit);
		pos += f * c.dt * sum / dur[idx];
		int guard = 0;
		while (pos >= 1.f && guard++ < 64) {
			pos -= 1.f;
			idx++;
			if (idx >= n) { idx = 0; newCycle(c, r); }
		}
		if (pos >= 1.f) pos = 0.f;
		float y = amp[idx] + (amp[(idx + 1) % n] - amp[idx]) * pos;
		y = dc.process(y, c.dcR);
		lp += (y - lp) * clampf(TWO_PI * 3.f * c.f * c.dt, 0.f, 1.f);
		m = y * 0.9f;
		a = lp * 1.2f;
	}
};

// =================================================================== 1 HERD
// Eight saw oscillators that try to fall into step with each other
// (Kuramoto coupling through their mean phase). SHAPE = how far apart their
// own pitches are, TEXTURE = how hard they pull together (chorus .. unison
// lock), LIFE = each one strays on its own and the pull breathes.
// AUX = the same herd as sines.
struct Herd {
	static const int N = 8;
	float ph[N], det[N], drift[N];
	Herd() { Rng r; reset(r); }
	void reset(Rng& r) {
		for (int i = 0; i < N; i++) {
			ph[i] = r.uni();
			det[i] = ((float) i - 3.5f) / 3.5f + r.bi() * 0.08f;
			drift[i] = 0.f;
		}
	}
	void trigger(const Ctl&, float, Rng&) {}
	void process(const Ctl& c, Rng& r, float w, float& m, float& a) {
		float spread = 0.08f + c.shape * c.shape * 7.f;   // semitones at the edge
		float sn[N], cs[N];
		float cx = 0.f, sy = 0.f;
		for (int i = 0; i < N; i++) {
			sn[i] = fsin(ph[i]);
			cs[i] = fsin(ph[i] + 0.25f);
			cx += cs[i]; sy += sn[i];
		}
		cx /= N; sy /= N;
		float R = std::sqrt(cx * cx + sy * sy);
		float edgeHz = c.f * (std::exp2(spread / 12.f) - 1.f);
		float K = c.texture * c.texture * (2.4f * edgeHz + 0.02f * c.f) * (1.f + c.life * w * 0.9f);
		if (K < 0.f) K = 0.f;
		float saw = 0.f, sine = 0.f;
		for (int i = 0; i < N; i++) {
			drift[i] = drift[i] * 0.99996f + r.bi() * c.life * 0.003f;
			float fi = c.f * std::exp2((spread * det[i] + drift[i]) / 12.f);
			float inc = clampf((fi + K * (sy * cs[i] - cx * sn[i])) * c.dt, 0.f, 0.45f);
			saw += 2.f * ph[i] - 1.f - polyblep(ph[i], inc);
			sine += sn[i];
			ph[i] += inc;
			if (ph[i] >= 1.f) ph[i] -= 1.f;
		}
		float norm = 1.f / (std::sqrt((float) N) + ((float) N - std::sqrt((float) N)) * R);
		m = std::tanh(saw * norm * 0.8f);
		a = std::tanh(sine * norm * 1.0f);
	}
};

// =================================================================== 2 DUFFING
// A driven nonlinear spring, x'' + c x' + alpha x + x^3 = g cos(wt), with the
// drive at the played pitch. SHAPE bends the spring from one stiff well
// (alpha = +1, a clean tone) to two wells (alpha = -1, where it can jump
// between them); TEXTURE = drive (sine .. period doubling .. chaos);
// LIFE = the damping and drive wander, moving it in and out of chaos.
// AUX = the spring's velocity (brighter).
struct Duffing {
	float x, v, ph;
	DCBlock dc, dc2;
	Duffing() { Rng r; reset(r); }
	void reset(Rng&) { x = 0.1f; v = 0.f; ph = 0.f; dc.reset(); dc2.reset(); }
	void trigger(const Ctl&, float, Rng&) {}
	void process(const Ctl& c, Rng&, float w, float& m, float& a) {
		const float OMEGA = 1.2f;
		float alpha = 1.f - 2.f * c.shape;
		float g = (0.05f + c.texture * c.texture * 0.85f) * (1.f + c.life * 0.25f * w);
		float damp = 0.25f + 0.12f * c.life * w;
		float h = (TWO_PI / OMEGA) * c.f * c.dt;
		int ns = (int) std::ceil(h / 0.07f);
		if (ns < 1) ns = 1;
		if (ns > 24) ns = 24;
		float hs = h / ns, dph = c.f * c.dt / ns;
		for (int k = 0; k < ns; k++) {
			float c0 = fsin(ph + 0.25f), c1 = fsin(ph + 0.5f * dph + 0.25f), c2 = fsin(ph + dph + 0.25f);
			float k1x = v, k1v = -damp * v - alpha * x - x * x * x + g * c0;
			float x2 = x + 0.5f * hs * k1x, v2 = v + 0.5f * hs * k1v;
			float k2x = v2, k2v = -damp * v2 - alpha * x2 - x2 * x2 * x2 + g * c1;
			float x3 = x + 0.5f * hs * k2x, v3 = v + 0.5f * hs * k2v;
			float k3x = v3, k3v = -damp * v3 - alpha * x3 - x3 * x3 * x3 + g * c1;
			float x4 = x + hs * k3x, v4 = v + hs * k3v;
			float k4x = v4, k4v = -damp * v4 - alpha * x4 - x4 * x4 * x4 + g * c2;
			x += hs / 6.f * (k1x + 2.f * k2x + 2.f * k3x + k4x);
			v += hs / 6.f * (k1v + 2.f * k2v + 2.f * k3v + k4v);
			ph += dph;
			if (ph >= 1.f) ph -= 1.f;
		}
		if (!std::isfinite(x) || !std::isfinite(v) || std::fabs(x) > 8.f) { x = 0.1f; v = 0.f; }
		float gain = 0.8f / (0.15f + g);
		m = std::tanh(dc.process(x, c.dcR) * gain);
		a = std::tanh(dc2.process(v, c.dcR) * gain * 0.7f);
	}
};

// =================================================================== 3 SCANNED
// Scanned synthesis: a ring of 48 masses on springs moves slowly (a few
// thousand updates a second) and its shape is read out at the played pitch.
// The ring rests on a shape chosen by SHAPE (sine .. ramp .. soft square ..
// lumpy); TRIG plucks it. TEXTURE = spring tension (lazy wobble .. fast
// shimmer); LIFE = an invisible hand that keeps nudging it.
// AUX = the ring's movement only (silent when it is still).
struct Scanned {
	static const int N = 48;
	float x[N], v[N], rest[N], prof[4][N];
	float ph;
	int cnt;
	DCBlock dc, dc2;
	Scanned() { Rng r; reset(r); }
	void reset(Rng& r) {
		for (int i = 0; i < N; i++) {
			float t = (float) i / N;
			prof[0][i] = std::sin(TWO_PI * t);
			prof[1][i] = t < 0.8f ? t / 0.8f * 2.f - 1.f : (1.f - t) / 0.2f * 2.f - 1.f;
			prof[2][i] = std::tanh(4.f * std::sin(TWO_PI * t)) / std::tanh(4.f);
			prof[3][i] = r.bi();
		}
		for (int pass = 0; pass < 2; pass++) {
			float tmp[N];
			for (int i = 0; i < N; i++) tmp[i] = 0.25f * prof[3][(i + N - 1) % N] + 0.5f * prof[3][i] + 0.25f * prof[3][(i + 1) % N];
			std::memcpy(prof[3], tmp, sizeof(tmp));
		}
		float pk = 1e-6f, mean = 0.f;
		for (int i = 0; i < N; i++) mean += prof[3][i] / N;
		for (int i = 0; i < N; i++) { prof[3][i] -= mean; pk = std::max(pk, std::fabs(prof[3][i])); }
		for (int i = 0; i < N; i++) prof[3][i] /= pk;
		setRest(0.f);
		for (int i = 0; i < N; i++) { x[i] = rest[i]; v[i] = 0.f; }
		ph = 0.f; cnt = 0; dc.reset(); dc2.reset();
	}
	void setRest(float shape) {
		float s = clampf(shape, 0.f, 1.f) * 3.f;
		int k = (int) s;
		if (k > 2) k = 2;
		float fr = s - k;
		for (int i = 0; i < N; i++) rest[i] = prof[k][i] + (prof[k + 1][i] - prof[k][i]) * fr;
	}
	void bump(float pos, float width, float amt) {
		for (int i = 0; i < N; i++) {
			float d = std::fabs((float) i - pos);
			d = std::min(d, (float) N - d);
			x[i] += amt * std::exp(-(d / width) * (d / width));
		}
	}
	void trigger(const Ctl& c, Rng& r, float vel) { bump(r.uni() * N, 2.f + (1.f - c.shape) * 5.f, vel * (r.uni() < 0.5f ? -1.1f : 1.1f)); }
	void trigger(const Ctl& c, float vel, Rng& r) { trigger(c, r, vel); }
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		if (--cnt <= 0) {
			cnt = std::max(1, (int) (c.sr / 12000.f));
			setRest(c.shape);
			float tension = std::min(0.6f, 0.004f * std::pow(150.f, c.texture));
			const float center = 0.004f, damp = 0.006f;
			if (r.uni() < c.life * 0.006f) bump(r.uni() * N, 3.f, r.bi() * 0.4f * c.life);
			for (int i = 0; i < N; i++) {
				float F = tension * (x[(i + N - 1) % N] + x[(i + 1) % N] - 2.f * x[i]) + center * (rest[i] - x[i]) - damp * v[i];
				v[i] += F;
			}
			for (int i = 0; i < N; i++) x[i] += v[i];
		}
		float p = ph * N;
		int i0 = (int) p;
		if (i0 >= N) i0 = N - 1;
		float fr = p - i0;
		int i1 = (i0 + 1) % N;
		float y = x[i0] + (x[i1] - x[i0]) * fr;
		float vy = v[i0] + (v[i1] - v[i0]) * fr;
		ph += c.f * c.dt;
		if (ph >= 1.f) ph -= 1.f;
		m = std::tanh(dc.process(y, c.dcR) * 0.85f);
		a = std::tanh(dc2.process(vy, c.dcR) * 25.f);
	}
};

// =================================================================== 4 WALSH
// Additive synthesis with Walsh functions (square-wave "harmonics" ordered by
// how often they flip) instead of sines: a 32-step wave, rebuilt each cycle.
// SHAPE = formant (which flip-rate is boosted), TEXTURE = brightness (how
// slowly the higher ones fall away), LIFE = each component's level drifts.
// AUX = the formant alone (thinner, vocal).
struct WalshTable {
	float w[32][32];
	WalshTable() {
		for (int k = 0; k < 32; k++)
			for (int n = 0; n < 32; n++) {
				int p = k ^ (k >> 1), rev = 0;
				for (int b = 0; b < 5; b++) if (n & (1 << b)) rev |= 1 << (4 - b);
				int x = p & rev, cnt = 0;
				while (x) { x &= x - 1; cnt++; }
				w[k][n] = (cnt & 1) ? -1.f : 1.f;
			}
	}
};
inline const WalshTable& walshTable() { static const WalshTable t; return t; }

struct Walsh {
	float T[32], F[32], drift[32];
	float ph;
	bool built;
	Walsh() { Rng r; reset(r); }
	void reset(Rng&) {
		for (int i = 0; i < 32; i++) { T[i] = F[i] = 0.f; drift[i] = 0.f; }
		ph = 0.f; built = false;
	}
	void trigger(const Ctl&, float, Rng&) {}
	void build(const Ctl& c, Rng& r) {
		const WalshTable& W = walshTable();
		float tilt = 2.2f - 1.8f * c.texture;
		float peak = 1.f + c.shape * 30.f, width = 1.2f + c.texture * 2.5f;
		float t[32], f[32];
		for (int n = 0; n < 32; n++) t[n] = f[n] = 0.f;
		for (int k = 1; k < 32; k++) {
			drift[k] = drift[k] * 0.97f + r.bi() * 0.09f * c.life;
			float mult = std::max(0.f, 1.f + drift[k] * 2.f);
			float ak = std::pow((float) k, -tilt) * 0.6f;
			float z = (k - peak) / width;
			float fk = std::exp(-z * z) * 0.5f;
			for (int n = 0; n < 32; n++) {
				t[n] += (ak + fk) * mult * W.w[k][n];
				f[n] += fk * mult * W.w[k][n];
			}
		}
		float pt = 1e-6f, pf = 1e-6f;
		for (int n = 0; n < 32; n++) { pt = std::max(pt, std::fabs(t[n])); pf = std::max(pf, std::fabs(f[n])); }
		for (int n = 0; n < 32; n++) { T[n] = t[n] / pt; F[n] = f[n] / pf; }
		built = true;
	}
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		if (!built) build(c, r);
		float inc = c.f * c.dt;
		m = stepped32(T, ph, inc) * 0.95f;
		a = stepped32(F, ph, inc) * 0.85f;
		ph += inc;
		if (ph >= 1.f) { ph -= 1.f; build(c, r); }
	}
};

// =================================================================== 5 PLL
// A phase-locked loop in the style of the CD4046 chip: a square VCO, divided
// by N, is compared with the played pitch by an XOR gate, and the filtered
// result steers the VCO, so it locks to N times the pitch. SHAPE = N (1..8),
// TEXTURE = loop filter (slow swoops and hunting .. tight lock .. fast loop
// where the ripple becomes buzz), LIFE = the divider slips to neighbouring
// numbers now and then. AUX = the XOR comparator itself (ring-mod grit).
struct PhaseLock {
	float pr, pv, lp, cf;
	int cnt, N, hold;
	DCBlock dc;
	PhaseLock() { Rng r; reset(r); }
	void reset(Rng&) { pr = 0.f; pv = 0.f; lp = 0.5f; cf = 0.f; cnt = 0; N = 1; hold = 0; dc.reset(); }
	void trigger(const Ctl&, float, Rng&) {}
	void process(const Ctl& c, Rng& r, float w, float& m, float& a) {
		const int OS = 4;
		int baseN = 1 + (int) (c.shape * 7.f + 0.5f);
		float d = c.dt / OS;
		float fc = 0.3f * std::exp2(c.texture * 13.f);
		float al = 1.f - std::exp(-TWO_PI * fc * d);
		if (cf <= 0.f) cf = c.f;
		cf += (c.f - cf) * std::min(1.f, al * OS * 4.f);
		float sq = 0.f, xo = 0.f;
		for (int o = 0; o < OS; o++) {
			float fv = (float) N * cf * std::exp2((lp - 0.5f) * 2.f + c.life * 0.35f * w);
			float incv = clampf(fv * d, 0.f, 0.45f);
			sq += (pv < 0.5f ? 1.f : -1.f) + polyblep(pv, incv) - polyblep(frac(pv + 0.5f), incv);
			float pd = ((float) cnt + pv) / (float) N;
			bool ds = pd < 0.5f, rs = pr < 0.5f;
			float pc = (ds != rs) ? 1.f : 0.f;
			lp += al * (pc - lp);
			xo += pc * 2.f - 1.f;
			pv += incv;
			if (pv >= 1.f) { pv -= 1.f; if (++cnt >= N) cnt = 0; }
			pr += c.f * d;
			if (pr >= 1.f) {
				pr -= 1.f;
				if (hold > 0) hold--;
				else {
					N = baseN;
					if (r.uni() < c.life * c.life * 0.3f) {
						N = std::max(1, baseN + (int) (r.uni() * 5.f) - 2);
						hold = 1 + (int) (r.uni() * 12.f);
					}
				}
				if (cnt >= N) cnt %= N;
			}
		}
		m = sq / OS * 0.35f;
		a = dc.process(xo / OS, c.dcR) * 0.6f;
	}
};

// =================================================================== 6 BYTEBEAT
// One-line formula music. A counter runs at 256 steps per cycle of the played
// pitch and a formula of shifts, ANDs and XORs turns it into 8-bit samples.
// SHAPE = which formula (8), TEXTURE = the shift amount inside it,
// LIFE = random bits of the slow counter flip, mutating the pattern.
// TRIG restarts the formula from zero. AUX = 4-bit version.
struct Bytebeat {
	double t;
	uint32_t mask, lastS;
	float lp;
	DCBlock dc, dc2;
	Bytebeat() { Rng r; reset(r); }
	void reset(Rng&) { t = 0.0; mask = 0; lastS = 0; lp = 0.f; dc.reset(); dc2.reset(); }
	void trigger(const Ctl&, float, Rng&) { t = 0.0; }
	static uint32_t formula(int k, uint32_t t, int p) {
		switch (k) {
			case 0: return t * (1 + ((t >> (11 + p)) & 3));
			case 1: return t ^ (t >> p);
			case 2: return ((t * 3) & (t >> (5 + p))) | ((t * 2) & (t >> (7 + p)));
			case 3: return t * ((t >> (8 + p)) | (t >> (9 + p)));
			case 4: return t & (t >> (4 + p));
			case 5: return t * (((t >> 12) | (t >> (7 + p))) & 63 & (t >> 4));
			case 6: return ((t * 5) & (t >> (5 + p))) | ((t * 3) & (t >> (8 + p)));
			default: return t * (42 & (t >> (7 + p)));
		}
	}
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		int k = (int) (c.shape * 7.99f);
		int p = 1 + (int) (c.texture * 6.99f);
		t += (double) c.f * 256.0 * (double) c.dt;
		if (t >= 4294967296.0) t -= 4294967296.0;
		uint32_t ti = (uint32_t) t;
		uint32_t s = ti >> 8;
		if (s != lastS) {
			lastS = s;
			if (c.life < 0.01f) mask = 0;
			else if (r.uni() < c.life * c.life * 0.06f) mask ^= 1u << (r.next() % 8);
		}
		uint32_t v = formula(k, ti ^ (mask << 8), p) & 255u;
		float y = v / 127.5f - 1.f;
		lp += (y - lp) * 0.6f;
		m = dc.process(lp, c.dcR) * 0.6f;
		a = dc2.process(((v >> 4) << 4) / 127.5f - 1.f, c.dcR) * 0.7f;
	}
};

// =================================================================== 7 AUTOMATON
// One row of a 1-D cellular automaton (32 cells) is one cycle of the wave;
// every few cycles the next generation replaces it. SHAPE = the rule
// (90, 30, 45, 73, 110, 105, 150, 54), TEXTURE = cycles per generation
// (buzzing change .. slowly evolving timbre), LIFE = random cells flip.
// TRIG reseeds (one live cell, or a random row with LIFE past half).
// AUX = the previous generation.
static const uint8_t CA_RULES[8] = {90, 30, 45, 73, 110, 105, 150, 54};
struct Automaton {
	uint32_t row, prev;
	float T[32], P[32];
	float ph;
	int gen;
	Automaton() { Rng r; reset(r); }
	void reset(Rng&) { row = 1u << 16; prev = row; ph = 0.f; gen = 0; table(row, T); table(prev, P); }
	static uint32_t step(uint32_t x, uint8_t rule) {
		uint32_t out = 0;
		for (int i = 0; i < 32; i++) {
			uint32_t l = (x >> ((i + 1) & 31)) & 1u, cc = (x >> i) & 1u, rr = (x >> ((i + 31) & 31)) & 1u;
			if ((rule >> ((l << 2) | (cc << 1) | rr)) & 1u) out |= 1u << i;
		}
		return out;
	}
	static void table(uint32_t x, float* T) {
		float cell[32], mean = 0.f, pk = 1e-6f;
		for (int i = 0; i < 32; i++) cell[i] = ((x >> i) & 1u) ? 1.f : -1.f;
		for (int i = 0; i < 32; i++) { T[i] = 0.25f * cell[(i + 31) & 31] + 0.5f * cell[i] + 0.25f * cell[(i + 1) & 31]; mean += T[i] / 32.f; }
		for (int i = 0; i < 32; i++) { T[i] -= mean; pk = std::max(pk, std::fabs(T[i])); }
		for (int i = 0; i < 32; i++) T[i] = pk > 0.01f ? T[i] / pk : 0.f;
	}
	void trigger(const Ctl& c, float, Rng& r) {
		prev = row;
		row = c.life > 0.5f ? (r.next() | 1u) : (1u << 16);
		table(row, T); table(prev, P);
		gen = 0;
	}
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		float inc = c.f * c.dt;
		m = stepped32(T, ph, inc) * 0.55f;
		a = stepped32(P, ph, inc) * 0.55f;
		ph += inc;
		if (ph >= 1.f) {
			ph -= 1.f;
			int every = 1 + (int) (c.texture * c.texture * 127.f);
			if (++gen >= every) {
				gen = 0;
				prev = row;
				row = step(row, CA_RULES[(int) (c.shape * 7.99f)]);
				if (r.uni() < c.life) row ^= 1u << (r.next() & 31);
				if (row == 0u || row == 0xFFFFFFFFu) row = r.next() | 1u;
				table(row, T); table(prev, P);
			}
		}
	}
};

// =================================================================== 8 BUBBLES
// Bubble physics (Minnaert resonance): each bubble is a sine whose pitch is set
// by its size and rises as it nears the surface, dying away quickly.
// TRIG releases one bubble at the played pitch. SHAPE = size spread (all the
// same .. two octaves either side), TEXTURE = rise (flat blip .. cartoon
// bloop), LIFE = how many bubble up by themselves (with TRIG unpatched there is
// always a trickle). DECAY = how long they ring. MAIN/AUX = stereo pair.
struct Bubbles {
	static const int NB = 12;
	float ph[NB], f[NB], df[NB], amp[NB], mul[NB], gl[NB], gr[NB];
	bool on[NB];
	Bubbles() { Rng r; reset(r); }
	void reset(Rng&) { for (int i = 0; i < NB; i++) { on[i] = false; amp[i] = 0.f; } }
	void spawn(const Ctl& c, Rng& r, float vel, float spreadOct) {
		int slot = 0;
		float best = 1e9f;
		for (int i = 0; i < NB; i++) {
			float s = on[i] ? amp[i] : -1.f;
			if (s < best) { best = s; slot = i; }
		}
		float f0 = clampf(c.f * std::exp2(r.bi() * spreadOct), 30.f, c.sr * 0.4f);
		float d = 0.043f * f0 + 0.0014f * std::pow(f0, 1.5f);
		d /= (0.2f + c.decay * 3.f);
		float xi = c.texture * c.texture * 1.6f;
		ph[slot] = 0.f;
		f[slot] = f0;
		df[slot] = f0 * xi * d * c.dt;
		amp[slot] = vel * (0.5f + 0.5f * r.uni());
		mul[slot] = std::exp(-d * c.dt);
		float p = 0.5f + r.bi() * 0.45f;
		gl[slot] = std::cos(p * PI_F * 0.5f) * 1.41f;
		gr[slot] = std::sin(p * PI_F * 0.5f) * 1.41f;
		on[slot] = true;
	}
	void trigger(const Ctl& c, float vel, Rng& r) { spawn(c, r, vel, c.shape * 0.15f); }
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		float rate = c.life * c.life * 90.f;
		if (c.drone) rate = std::max(rate, 3.f);
		if (rate > 0.f && r.uni() < rate * c.dt) spawn(c, r, 0.35f + 0.65f * r.uni(), c.shape * 2.f);
		float L = 0.f, R = 0.f;
		for (int i = 0; i < NB; i++) {
			if (!on[i]) continue;
			float s = fsin(ph[i]) * amp[i];
			L += s * gl[i];
			R += s * gr[i];
			ph[i] += f[i] * c.dt;
			if (ph[i] >= 1.f) ph[i] -= 1.f;
			f[i] += df[i];
			amp[i] *= mul[i];
			if (amp[i] < 1e-4f || f[i] > c.sr * 0.45f) on[i] = false;
		}
		m = std::tanh(L * 0.5f);
		a = std::tanh(R * 0.5f);
	}
};

// =================================================================== 9 SPRING
// A spring as an instrument: a delay loop tuned to the played pitch with a
// chain of all-pass filters inside, which smear each hit into the falling
// "pew" chirp of a spring tank. SHAPE = dispersion (plain ring .. laser zap),
// TEXTURE = tone (dark thud .. bright zing) and the length of the hit,
// LIFE = drips (random little hits) and spring wobble. DECAY = ring time.
// With TRIG unpatched the spring hums on a little noise. AUX = a middle tap.
struct Spring {
	static const int MAXK = 48;
	static const int DN = 16384;
	Delay<DN> dl;
	float st[MAXK];
	float lpz, imp, burstAmp;
	int burst;
	DCBlock dc;
	Spring() { Rng r; reset(r); }
	void reset(Rng&) { dl.reset(); for (int i = 0; i < MAXK; i++) st[i] = 0.f; lpz = 0.f; imp = 0.f; burst = 0; burstAmp = 0.f; dc.reset(); }
	void trigger(const Ctl& c, float vel, Rng&) {
		imp += vel;
		burst = (int) ((0.0005f + c.texture * 0.012f) * c.sr);
		burstAmp = vel;
	}
	void process(const Ctl& c, Rng& r, float w, float& m, float& a) {
		float P = c.sr / c.f;
		float ac = clampf(-0.45f - 0.3f * c.shape + c.life * 0.06f * w, -0.9f, -0.1f);
		float gd = (1.f - ac) / (1.f + ac);
		int K = 2 + (int) (c.shape * 46.f);
		int Kmax = (int) (0.8f * P / gd);
		if (K > Kmax) K = std::max(0, Kmax);
		float D = P - K * gd - 0.5f;
		float T60 = 0.05f * std::pow(160.f, c.decay);
		float g = std::pow(10.f, -3.f * P / (c.sr * T60));
		float al = 0.12f + 0.88f * c.texture;
		float e = imp;
		imp = 0.f;
		if (burst > 0) { e += r.bi() * burstAmp * 0.5f; burst--; }
		if (c.drone) e += r.bi() * 0.002f;
		float dripRate = c.life * c.life * 6.f + (c.drone ? 0.7f : 0.f);
		if (r.uni() < dripRate * c.dt) e += (0.2f + 0.6f * r.uni()) * (r.uni() < 0.5f ? -1.f : 1.f);
		float y = dl.read(D);
		for (int k = 0; k < K; k++) {
			float out = ac * y + st[k];
			st[k] = y - ac * out;
			y = out;
		}
		lpz += al * (y - lpz);
		float xin = e + lpz * g;
		xin = 2.f * std::tanh(xin * 0.5f);
		dl.write(xin);
		m = std::tanh(dc.process(lpz, c.dcR) * 2.5f);
		a = std::tanh(dl.read(D * 0.5f) * 2.f);
	}
};

// =================================================================== 10 BOUNCE
// A ball dropped on a hard floor: the gaps between bounces shrink until the
// rattle turns into a buzz and stops. Each impact rings a resonator at the
// played pitch. SHAPE = bounciness (lively .. dead), TEXTURE = what the ball
// is (rubber thud .. ping-pong click .. steel ring), LIFE = an uneven floor
// (jittered bounces and pitch). DECAY = drop height. With TRIG unpatched it is
// dropped again after it settles. AUX = the impact clicks alone.
struct Bounce {
	SVF res;
	float tNext, T, vel, rest, clickEnv, pitchJ, clp;
	bool active;
	Bounce() { Rng r; reset(r); }
	void reset(Rng&) { res.reset(); tNext = 0.f; T = 0.f; vel = 0.f; rest = 0.f; clickEnv = 0.f; pitchJ = 0.f; clp = 0.f; active = false; }
	void trigger(const Ctl& c, float v, Rng&) {
		T = 0.04f + c.decay * c.decay * 1.2f;
		tNext = T * 0.5f;
		vel = v;
		active = true;
		rest = 0.f;
	}
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		float e = 0.95f - c.shape * 0.55f;
		float imp = 0.f;
		if (active) {
			tNext -= c.dt;
			if (tNext <= 0.f) {
				imp = std::sqrt(vel);
				clickEnv = imp;
				float ee = clampf(e * (1.f + c.life * 0.12f * r.bi()), 0.1f, 0.985f);
				T *= ee;
				vel *= ee * ee;
				tNext += T;
				pitchJ = r.bi() * c.life * 0.03f;
				if (T < 0.0006f || vel < 1e-4f) active = false;
			}
		}
		else if (c.drone) {
			rest += c.dt;
			if (rest > 0.25f + (1.f - c.life) * 0.9f) trigger(c, 0.8f, r);
		}
		float q = 1.5f + c.texture * c.texture * 90.f;
		res.set(c.f * (1.f + pitchJ), q, c.sr);
		float click = 0.f;
		if (clickEnv > 1e-4f) {
			click = r.bi() * clickEnv;
			clickEnv *= std::exp(-c.dt / 0.0008f);
		}
		clp += (click - clp) * clampf(TWO_PI * (300.f + 9000.f * c.texture * c.texture) * c.dt, 0.f, 1.f);
		click = clp;
		float lp, bp, hp;
		res.process(imp / (2.f * res.g) + click * 0.1f, lp, bp, hp);
		m = std::tanh(bp * 1.4f + click * 0.6f);
		a = click;
	}
};

// =================================================================== 11 HOWL
// Microphone-to-speaker feedback in a small room, with the room's strongest
// mode tuned to the played pitch. SHAPE = mic distance (close: one pure howl;
// far: many neighbouring modes compete and it jumps between them),
// TEXTURE = amp gain (just below: rings and dies; above: screams and
// saturates), LIFE = someone walking the mic around (slides and swaps).
// TRIG claps into the mic. AUX = the mic signal before the amp.
struct Howl {
	static const int DN = 16384;
	Delay<DN> dl;
	SVF bp;
	float mic, clap;
	int clapN;
	DCBlock dc;
	Howl() { Rng r; reset(r); }
	void reset(Rng&) { dl.reset(); bp.reset(); mic = 0.f; clap = 0.f; clapN = 0; dc.reset(); }
	void trigger(const Ctl& c, float vel, Rng&) { clapN = (int) (0.004f * c.sr); clap = vel; }
	void process(const Ctl& c, Rng& r, float w, float& m, float& a) {
		float P = c.sr / c.f;
		int nmax = std::max(1, (int) (0.012f * c.sr / P));
		int n = std::min(nmax, 1 + (int) (c.shape * 6.f));
		float D = P * n * (1.f + c.life * 0.05f * w) - 1.f;
		float q = 4.f + (1.f - c.shape) * 22.f;
		bp.set(c.f * (1.f + c.life * 0.01f * w), q, c.sr);
		float G = 0.6f + c.texture * 1.6f;
		float noise = r.bi() * 0.0005f;
		if (clapN > 0) { noise += r.bi() * clap * 0.6f; clapN--; }
		float s = std::tanh(G * (mic + noise));
		dl.write(s);
		float lp, b, hp;
		bp.process(dl.read(D), lp, b, hp);
		mic = b * bp.k;
		m = dc.process(s, c.dcR) * 0.75f;
		a = mic * 0.75f;
	}
};

// =================================================================== 12 TINE
// An electric piano: a struck metal tine (with a short bright knock from the
// hammer) swinging in front of a magnetic pickup whose bell-shaped field adds
// the bark. SHAPE = hammer (soft and round .. hard bark), TEXTURE = pickup
// position (off-centre mellow .. centred bell), LIFE = stereo tremolo depth
// (MAIN and AUX are the two sides). DECAY = sustain.
struct Tine {
	float ph, ph2, amp, amp2, mulA, mulB, trem;
	DCBlock dc;
	Tine() { Rng r; reset(r); }
	void reset(Rng&) { ph = ph2 = 0.f; amp = amp2 = 0.f; mulA = mulB = 0.f; trem = 0.f; dc.reset(); }
	void trigger(const Ctl& c, float vel, Rng&) {
		amp = vel * (0.25f + c.shape * 1.1f);
		amp2 = amp * 0.5f * c.shape;
		float tau = (0.4f + c.decay * 7.f) * std::sqrt(261.63f / c.f);
		mulA = std::exp(-c.dt / tau);
		mulB = std::exp(-c.dt / 0.012f);
	}
	void process(const Ctl& c, Rng&, float w, float& m, float& a) {
		float x = amp * fsin(ph);
		if (7.1f * c.f < 0.45f * c.sr) x += amp2 * fsin(ph2);
		ph += c.f * c.dt;
		if (ph >= 1.f) ph -= 1.f;
		ph2 += 7.1f * c.f * c.dt;
		if (ph2 >= 1.f) ph2 -= std::floor(ph2);
		amp *= mulA;
		amp2 *= mulB;
		float o = 0.08f + (1.f - c.texture) * 1.1f;
		const float kk = 2.5f;
		float u = x + o;
		float y = 1.f / (1.f + kk * u * u) - 1.f / (1.f + kk * o * o);
		y = dc.process(-2.f * y, c.dcR);
		trem += (3.5f + 2.f * w) * c.dt;
		if (trem >= 1.f) trem -= 1.f;
		float s = fsin(trem), depth = c.life * 0.9f;
		m = std::tanh(y * (1.f - depth * 0.5f * (1.f + s)));
		a = std::tanh(y * (1.f - depth * 0.5f * (1.f - s)));
	}
};

// =================================================================== 13 TONEWHEEL
// A tonewheel organ: nine drawbars of near-sine wheels on tempered pitches
// (the top ones fold back an octave), with key click, leakage from other
// wheels, percussion on the third, and a vibrato/chorus scanner.
// SHAPE = drawbar registration (eight settings from flute to full),
// TEXTURE = dirt (click, leakage, preamp drive), LIFE = vibrato depth, then
// chorus. MAIN/AUX = the scanner swinging opposite ways (wide stereo).
static const float TW_SEMI[9] = {-12.f, 7.f, 0.f, 12.f, 19.f, 24.f, 28.f, 31.f, 36.f};
static const uint8_t TW_REG[8][9] = {
	{0, 0, 8, 0, 0, 0, 0, 0, 0},
	{8, 0, 8, 0, 0, 0, 0, 0, 0},
	{8, 8, 8, 0, 0, 0, 0, 0, 0},
	{6, 8, 8, 6, 0, 0, 0, 0, 0},
	{8, 8, 8, 8, 0, 0, 0, 0, 0},
	{0, 0, 6, 8, 7, 6, 4, 0, 0},
	{8, 0, 0, 0, 0, 0, 8, 8, 8},
	{8, 8, 8, 8, 8, 8, 8, 8, 8}};
inline float drawbarAmp(int lvl) { return lvl > 0 ? std::exp2((lvl - 8) * 0.5f) : 0.f; }

struct Tonewheel {
	float ph[9], lph[3], lsemi[3];
	float perc, percMul, click, vib;
	int clickN;
	Delay<2048> dl;
	SVF clickF;
	DCBlock dc, dc2;
	Tonewheel() { Rng r; reset(r); }
	void reset(Rng& r) {
		for (int k = 0; k < 9; k++) ph[k] = r.uni();
		for (int i = 0; i < 3; i++) { lph[i] = r.uni(); lsemi[i] = (float) ((int) (r.uni() * 48.f) - 12); }
		perc = 0.f; percMul = 0.f; click = 0.f; vib = 0.f; clickN = 0;
		dl.reset(); clickF.reset(); dc.reset(); dc2.reset();
	}
	void trigger(const Ctl& c, float vel, Rng&) {
		perc = vel * 0.6f;
		percMul = std::exp(-c.dt / 0.25f);
		clickN = (int) (0.004f * c.sr);
		click = vel * (0.05f + 0.3f * c.texture);
	}
	void process(const Ctl& c, Rng& r, float, float& m, float& a) {
		float s = c.shape * 7.f;
		int i0 = (int) s;
		if (i0 > 6) i0 = 6;
		float fr = s - i0;
		float sum = 0.f;
		for (int k = 0; k < 9; k++) {
			float A = drawbarAmp(TW_REG[i0][k]) * (1.f - fr) + drawbarAmp(TW_REG[i0 + 1][k]) * fr;
			float fk = c.f * std::exp2(TW_SEMI[k] / 12.f);
			while (fk > 5900.f) fk *= 0.5f;
			if (A > 0.f) sum += A * (fsin(ph[k]) + 0.02f * fsin(3.f * ph[k]));
			ph[k] += fk * c.dt;
			if (ph[k] >= 1.f) ph[k] -= 1.f;
		}
		sum += perc * fsin(ph[4]);
		perc *= percMul;
		for (int i = 0; i < 3; i++) {
			float lf = c.f * std::exp2(lsemi[i] / 12.f);
			while (lf > 5900.f) lf *= 0.5f;
			sum += c.texture * 0.03f * fsin(lph[i]);
			lph[i] += lf * c.dt;
			if (lph[i] >= 1.f) lph[i] -= 1.f;
		}
		if (clickN > 0) {
			clickF.set(2800.f, 0.7f, c.sr);
			float lp, bp, hp;
			clickF.process(r.bi() * click, lp, bp, hp);
			sum += bp * 0.8f;
			clickN--;
		}
		float y = std::tanh(sum * 0.24f * (1.f + c.texture * 3.f)) * 0.85f;
		dl.write(y);
		vib += 6.9f * c.dt;
		if (vib >= 1.f) vib -= 1.f;
		float depth = std::min(c.life * 2.f, 1.f) * 0.0009f * c.sr;
		float centre = 0.0012f * c.sr;
		float sv = fsin(vib);
		float v1 = dl.read(centre + depth * sv), v2 = dl.read(centre - depth * sv);
		float cm = clampf((c.life - 0.5f) * 2.f, 0.f, 1.f) * 0.5f;
		m = dc.process(v1 * (1.f - cm) + y * cm, c.dcR);
		a = dc2.process(v2 * (1.f - cm) + y * cm, c.dcR);
	}
};

// =================================================================== 14 PAD
// A poly pad: band-limited saw and pulse with a sub
// square, a gentle fixed tone filter and a bucket-brigade style stereo
// chorus. SHAPE = saw .. square .. thin pulse, TEXTURE = chorus (off, I, II,
// deeper, with a little BBD hiss), LIFE = pulse-width movement and slow
// detune. MAIN/AUX = left/right of the chorus.
struct Pad {
	float ph, sp, lfo, cph, l1, l2, wl, wr;
	Delay<4096> dl;
	DCBlock dc, dc2;
	Pad() { Rng r; reset(r); }
	void reset(Rng& r) { ph = r.uni(); sp = 0.f; lfo = r.uni(); cph = 0.f; l1 = l2 = wl = wr = 0.f; dl.reset(); dc.reset(); dc2.reset(); }
	void trigger(const Ctl&, float, Rng&) {}
	void process(const Ctl& c, Rng& r, float w, float& m, float& a) {
		float f = c.f * (1.f + c.life * 0.004f * w);
		float inc = clampf(f * c.dt, 0.f, 0.45f);
		lfo += (0.7f + 0.5f * w) * c.dt;
		if (lfo >= 1.f) lfo -= 1.f;
		float pw = clampf(0.5f - c.shape * 0.42f + c.life * 0.3f * fsin(lfo), 0.05f, 0.95f);
		float saw = 2.f * ph - 1.f - polyblep(ph, inc);
		float pul = (ph < pw ? 1.f : -1.f) + polyblep(ph, inc) - polyblep(frac(ph - pw + 1.f), inc);
		float sub = (sp < 0.5f ? 1.f : -1.f) + polyblep(sp, inc * 0.5f) - polyblep(frac(sp + 0.5f), inc * 0.5f);
		ph += inc;
		if (ph >= 1.f) ph -= 1.f;
		sp += inc * 0.5f;
		if (sp >= 1.f) sp -= 1.f;
		float osc = saw * (1.f - c.shape) * 0.6f + pul * std::min(1.f, c.shape * 2.f) * 0.5f + sub * 0.2f;
		float fc = std::min(f * 10.f + 2000.f, c.sr * 0.4f);
		float al = 1.f - std::exp(-TWO_PI * fc * c.dt);
		l1 += al * (osc - l1);
		l2 += al * (l1 - l2);
		float y = dc.process(l2, c.dcR);
		float rate = c.texture < 0.5f ? 0.513f : 0.863f;
		cph += rate * c.dt;
		if (cph >= 1.f) cph -= 1.f;
		float tri = 4.f * std::fabs(cph - 0.5f) - 1.f;
		float wet = clampf(c.texture * 2.5f, 0.f, 1.f);
		float dmod = (0.0006f + 0.0014f * c.texture) * c.sr;
		float centre = 0.0033f * c.sr;
		dl.write(y + r.bi() * 0.002f * c.texture);
		float bl = 1.f - std::exp(-TWO_PI * 7000.f * c.dt);
		wl += bl * (dl.read(centre + dmod * tri) - wl);
		wr += bl * (dl.read(centre - dmod * tri) - wr);
		float dry = 1.f - wet * 0.3f;
		m = std::tanh((y * dry + wl * wet * 0.8f) * 1.1f);
		a = std::tanh((y * dry + wr * wet * 0.8f) * 1.1f);
	}
};

// =================================================================== 15 HARMONIUM
// A bellows-blown free-reed organ. Each reed lets air through in short puffs
// once per cycle; more pressure makes the puffs narrower and the tone
// buzzier, with breath noise on top. SHAPE = stops (8' .. 8'+4' .. 16'+8'+4'),
// TEXTURE = pressure (soft breathy .. loud reedy), LIFE = bellows pumping
// and the celeste rank's beating. MAIN/AUX = two sides (celeste leans right).
struct Harmonium {
	float ph[4], prev[4], bph;
	SVF breathF;
	DCBlock dc, dc2;
	Harmonium() { Rng r; reset(r); }
	void reset(Rng& r) { for (int i = 0; i < 4; i++) { ph[i] = r.uni(); prev[i] = 0.f; } bph = 0.f; breathF.reset(); dc.reset(); dc2.reset(); }
	void trigger(const Ctl&, float, Rng&) {}
	static float reed(float p, float thr) { float s = fsin(p) - thr; return s > 0.f ? s / (1.f - thr) : 0.f; }
	void process(const Ctl& c, Rng& r, float w, float& m, float& a) {
		bph += 0.6f * c.dt;
		if (bph >= 1.f) bph -= 1.f;
		float bel = 1.f + c.life * 0.18f * w + c.life * 0.06f * fsin(bph);
		float p = (0.3f + 0.7f * c.texture) * bel;
		float thr = clampf(0.1f + 0.65f * c.texture * bel, 0.f, 0.9f);
		float f8 = c.f * (1.f - 0.004f * (bel - 1.f));
		float cel = std::exp2((3.f + c.life * 9.f) / 1200.f);
		float fr[4] = {f8 * 0.5f, f8, f8 * 2.f, f8 * cel};
		float s = c.shape;
		float amp[4] = {clampf((s - 0.45f) * 2.5f, 0.f, 1.f), 1.f, clampf(s * 3.f, 0.f, 1.f) * 0.7f, 0.25f + c.life * 0.5f};
		float y[4];
		for (int i = 0; i < 4; i++) {
			float rv = reed(ph[i], thr);
			y[i] = rv - 0.75f * prev[i];
			prev[i] = rv;
			ph[i] += fr[i] * c.dt;
			if (ph[i] >= 1.f) ph[i] -= 1.f;
		}
		breathF.set(2500.f, 1.5f, c.sr);
		float lp, bp, hp;
		breathF.process(r.bi() * 0.14f * c.texture * p, lp, bp, hp);
		float main = (amp[0] * y[0] + amp[1] * y[1] + amp[2] * y[2] + amp[3] * 0.5f * y[3]) * p + bp;
		float aux = (amp[0] * y[0] + amp[1] * 0.5f * y[1] + amp[2] * y[2] + amp[3] * y[3]) * p + bp;
		m = std::tanh(dc.process(main, c.dcR) * 2.2f);
		a = std::tanh(dc2.process(aux, c.dcR) * 2.2f);
	}
};

// =================================================================== VOICE
// One voice holds every engine (only the chosen one runs), the envelope, the
// LIFE wanderer and the click-free model switch.
struct Voice {
	Rng rng;
	Gendy gendy; Herd herd; Duffing duffing; Scanned scanned;
	Walsh walsh; PhaseLock pll; Bytebeat bytebeat; Automaton automaton;
	Bubbles bubbles; Spring spring; Bounce bounce; Howl howl;
	Tine tine; Tonewheel tonewheel; Pad pad; Harmonium harmonium;

	int model, pending;
	float fade;
	float env;
	int stage;         // 0 idle/decay, 1 attack, 2 hold
	float wv, wt, wtimer;
	float lastF;
	bool started;

	Voice() : model(0), pending(0), fade(1.f), env(0.f), stage(0), wv(0.f), wt(0.f), wtimer(0.f), lastF(0.f), started(false) {}

	void seed(uint32_t s) { rng.seed(s); resetEngine(model); }

	void resetEngine(int m) {
		switch (m) {
			case GENDY: gendy.reset(rng); break;
			case HERD: herd.reset(rng); break;
			case DUFFING: duffing.reset(rng); break;
			case SCANNED: scanned.reset(rng); break;
			case WALSH: walsh.reset(rng); break;
			case PLL: pll.reset(rng); break;
			case BYTEBEAT: bytebeat.reset(rng); break;
			case AUTOMATON: automaton.reset(rng); break;
			case BUBBLES: bubbles.reset(rng); break;
			case SPRING: spring.reset(rng); break;
			case BOUNCE: bounce.reset(rng); break;
			case HOWL: howl.reset(rng); break;
			case TINE: tine.reset(rng); break;
			case TONEWHEEL: tonewheel.reset(rng); break;
			case PAD: pad.reset(rng); break;
			default: harmonium.reset(rng); break;
		}
		started = false;
	}

	void strike(const Ctl& c, float vel) {
		switch (model) {
			case GENDY: gendy.trigger(c, vel, rng); break;
			case HERD: herd.trigger(c, vel, rng); break;
			case DUFFING: duffing.trigger(c, vel, rng); break;
			case SCANNED: scanned.trigger(c, vel, rng); break;
			case WALSH: walsh.trigger(c, vel, rng); break;
			case PLL: pll.trigger(c, vel, rng); break;
			case BYTEBEAT: bytebeat.trigger(c, vel, rng); break;
			case AUTOMATON: automaton.trigger(c, vel, rng); break;
			case BUBBLES: bubbles.trigger(c, vel, rng); break;
			case SPRING: spring.trigger(c, vel, rng); break;
			case BOUNCE: bounce.trigger(c, vel, rng); break;
			case HOWL: howl.trigger(c, vel, rng); break;
			case TINE: tine.trigger(c, vel, rng); break;
			case TONEWHEEL: tonewheel.trigger(c, vel, rng); break;
			case PAD: pad.trigger(c, vel, rng); break;
			default: harmonium.trigger(c, vel, rng); break;
		}
	}

	void run(const Ctl& c, float w, float& m, float& a) {
		switch (model) {
			case GENDY: gendy.process(c, rng, w, m, a); break;
			case HERD: herd.process(c, rng, w, m, a); break;
			case DUFFING: duffing.process(c, rng, w, m, a); break;
			case SCANNED: scanned.process(c, rng, w, m, a); break;
			case WALSH: walsh.process(c, rng, w, m, a); break;
			case PLL: pll.process(c, rng, w, m, a); break;
			case BYTEBEAT: bytebeat.process(c, rng, w, m, a); break;
			case AUTOMATON: automaton.process(c, rng, w, m, a); break;
			case BUBBLES: bubbles.process(c, rng, w, m, a); break;
			case SPRING: spring.process(c, rng, w, m, a); break;
			case BOUNCE: bounce.process(c, rng, w, m, a); break;
			case HOWL: howl.process(c, rng, w, m, a); break;
			case TINE: tine.process(c, rng, w, m, a); break;
			case TONEWHEEL: tonewheel.process(c, rng, w, m, a); break;
			case PAD: pad.process(c, rng, w, m, a); break;
			default: harmonium.process(c, rng, w, m, a); break;
		}
	}

	float attackTime(const Ctl& c) const {
		if (model == PAD) return 0.01f + 0.6f * c.decay * c.decay;
		if (model == HARMONIUM) return 0.035f;
		return 0.002f;
	}

	// trig: rising edge this sample; gate: the TRIG input is high; level 0..~1.
	void process(int wantModel, const Ctl& c, bool trig, bool gate, float level, float& m, float& a) {
		// click-free model change: fade out 3 ms, switch, fade back in
		if (wantModel != model) pending = wantModel;
		if (pending != model) {
			fade -= c.dt / 0.003f;
			if (fade <= 0.f) {
				fade = 0.f;
				model = pending;
				resetEngine(model);
			}
		}
		else if (fade < 1.f) fade = std::min(1.f, fade + c.dt / 0.003f);

		// LIFE wanderer: a smooth random line
		float rate = 0.15f + c.life * 2.f;
		wtimer -= c.dt * rate;
		if (wtimer <= 0.f) { wtimer += 1.f; wt = rng.bi(); }
		wv += (wt - wv) * std::min(1.f, c.dt * rate * 3.f);

		bool perc = isPercussive(model);
		if (c.drone) {
			// unpatched TRIG: percussive engines strike on new notes
			if (perc && (!started || std::fabs(std::log2(c.f / std::max(lastF, 1.f))) > 0.008f)) strike(c, level);
		}
		else if (trig) {
			strike(c, perc ? level : 1.f);
			stage = 1;
		}
		started = true;
		lastF = c.f;

		float mm = 0.f, aa = 0.f;
		run(c, wv, mm, aa);
		if (!std::isfinite(mm) || !std::isfinite(aa)) { resetEngine(model); mm = aa = 0.f; }

		float g;
		if (c.drone) g = level;
		else if (perc) g = 1.f;
		else {
			if (stage == 1) {
				env += c.dt / attackTime(c);
				if (env >= 1.f) { env = 1.f; stage = 2; }
			}
			else if (stage == 2) {
				if (!gate) stage = 0;
			}
			if (stage == 0) {
				float dtime = 0.02f * std::pow(400.f, c.decay);
				env *= std::exp(-c.dt * 4.6f / dtime);
			}
			g = env * env * level;
		}
		m = mm * g * fade;
		a = aa * g * fade;
	}
};

}  // namespace mng
