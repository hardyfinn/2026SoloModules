// Clapfall particle tables: every surface is 24 modelled variants built once at
// 48 kHz from small recipes (noise contacts -> body resonances + air + click).
// Port of tools/proto/clapfall_proto.py (render_variant / build_tables).
// No Rack includes, so test/render.cpp can use it.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

namespace clapfall {

static const float TABLE_SR = 48000.f;
static const int NVAR = 24;
static const int NUM_BUILTIN = 22;
static const int USER_INDEX = NUM_BUILTIN;   // SURFACE position 22
static const int NUM_SURFACES = NUM_BUILTIN + 1;

// ---------------------------------------------------------------- random
struct Rng {
	uint64_t s0 = 0x9E3779B97F4A7C15ull, s1 = 0xD1B54A32D192ED03ull;
	explicit Rng(uint64_t seed = 1) { reseed(seed); }
	static uint64_t splitmix(uint64_t& x) {
		uint64_t z = (x += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}
	void reseed(uint64_t seed) { uint64_t x = seed; s0 = splitmix(x); s1 = splitmix(x); }
	uint64_t next() {   // xoroshiro128+
		uint64_t a = s0, b = s1, r = a + b;
		b ^= a;
		s0 = ((a << 24) | (a >> 40)) ^ b ^ (b << 16);
		s1 = (b << 37) | (b >> 27);
		return r;
	}
	double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }       // [0,1)
	double uni(double a, double b) { return a + (b - a) * uni(); }
	double normal() {
		double u1 = uni(), u2 = uni();
		if (u1 < 1e-300) u1 = 1e-300;
		return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
	}
	double expo() { double u = uni(); return -std::log(1.0 - u); }
	int below(int n) { return (int)(uni() * n); }
};

// ---------------------------------------------------------------- recipes
struct Contact { float dly, g, att, dec; };     // ms, linear, ms, ms
struct Mode { float f, q, gdb; };
struct Recipe {
	const char* name;
	float lenMs;
	std::vector<Contact> contacts;
	std::vector<Mode> modes;
	bool air = false; float airDb = 0, airHp = 0, airLp = 0;
	bool click = false; float clickDb = 0, clickLp = 0;
	float var[5] = {8, 25, 3, 25, 40};             // freq %, q %, gain dB, time %, delay %
	bool chirp = false; float chirp0 = 0, chirp1 = 0, chirpMs = 0, chirpDb = 0;
	float excLp = 0;                                // 0 = off
	bool choke = false;
};

inline Recipe mk(const char* name, float len, std::vector<Contact> c, std::vector<Mode> m) {
	Recipe r; r.name = name; r.lenMs = len; r.contacts = c; r.modes = m; return r;
}
inline Recipe& air(Recipe& r, float db, float hp, float lp) { r.air = true; r.airDb = db; r.airHp = hp; r.airLp = lp; return r; }
inline Recipe& click(Recipe& r, float db, float lp) { r.click = true; r.clickDb = db; r.clickLp = lp; return r; }
inline Recipe& var(Recipe& r, float a, float b, float c, float d, float e) { r.var[0] = a; r.var[1] = b; r.var[2] = c; r.var[3] = d; r.var[4] = e; return r; }

// Display names in SURFACE order (index 22 is USER).
static const char* const SURFACE_NAMES[NUM_SURFACES] = {
	"CLAP", "PALMS", "CUPPED", "FINGERS", "GROUP", "SNAP", "SLAP", "STOMP", "STICKS", "BLOCK", "RIM",
	"PING", "SWITCH", "COINS", "GLASS", "TIN", "PEBBLE", "DROP", "CRACKLE", "808", "909", "NOISE", "USER"};

// One recipe per built-in surface; GROUP (index 4) is built from the hands.
inline std::vector<Recipe> makeRecipes() {
	std::vector<Recipe> v(NUM_BUILTIN);
	Recipe r;
	// hands, fitted to third-octave spectra of real claps
	r = mk("CLAP", 50, {{0, 1.0f, 0.2f, 2.8f}, {1.6f, 0.35f, 0.2f, 2.0f}}, {{1600, 3.4f, 0}, {640, 1.8f, -2}, {2450, 3.0f, -6}, {4600, 2.2f, -9}, {980, 2.0f, -5}});
	air(r, -19, 1200, 9000); click(r, -24, 5000); r.excLp = 3000; v[0] = r;
	r = mk("PALMS", 45, {{0, 1.0f, 0.15f, 2.2f}}, {{1950, 3.2f, 0}, {2900, 3.0f, -6}, {760, 2.0f, -7}, {4800, 2.5f, -10}});
	air(r, -20, 1500, 9000); click(r, -20, 6000); r.excLp = 4200; v[1] = r;
	r = mk("CUPPED", 60, {{0, 1.0f, 0.35f, 3.6f}}, {{690, 3.8f, 0}, {1240, 3.0f, -5}, {330, 1.8f, -7}, {2600, 2.5f, -12}});
	air(r, -28, 800, 5000); click(r, -28, 3000); r.excLp = 1800; v[2] = r;
	r = mk("FINGERS", 40, {{0, 1.0f, 0.12f, 1.6f}, {0.9f, 0.5f, 0.12f, 1.4f}}, {{2450, 3.0f, 0}, {1360, 2.6f, -5}, {3900, 3.0f, -6}, {6200, 2.5f, -10}});
	air(r, -18, 2500, 11000); click(r, -18, 8000); r.excLp = 5500; v[3] = r;
	v[4] = v[0]; v[4].name = "GROUP";   // placeholder; GROUP rotates through the hands
	r = mk("SNAP", 50, {{0, 0.25f, 0.1f, 0.8f}, {3.2f, 1.0f, 0.08f, 1.3f}}, {{2750, 5.0f, 0}, {3600, 5.0f, -4}, {1500, 3.0f, -9}, {5600, 4.0f, -8}});
	air(r, -20, 2500, 12000); click(r, -16, 9000); var(r, 7, 20, 3, 20, 25); r.excLp = 6000; v[5] = r;
	r = mk("SLAP", 70, {{0, 1.0f, 0.4f, 4.5f}}, {{240, 2.2f, 0}, {520, 2.0f, -4}, {1150, 2.0f, -8}, {2700, 2.0f, -12}});
	air(r, -24, 600, 6000); click(r, -26, 3000); r.excLp = 1500; v[6] = r;
	r = mk("STOMP", 160, {{0, 1.0f, 0.6f, 7.0f}}, {{92, 5.0f, 0}, {158, 5.0f, -4}, {265, 4.0f, -8}, {710, 2.0f, -12}});
	air(r, -26, 400, 3500); click(r, -26, 2500); var(r, 10, 25, 3, 25, 0); r.excLp = 900; v[7] = r;
	r = mk("STICKS", 90, {{0, 1.0f, 0.05f, 0.5f}}, {{1830, 28, 0}, {2710, 30, -5}, {4650, 35, -6}, {7300, 30, -12}, {980, 12, -14}});
	air(r, -18, 3000, 14000); click(r, -6, 12000); var(r, 5, 20, 3, 20, 0); v[8] = r;
	r = mk("BLOCK", 110, {{0, 1.0f, 0.08f, 0.7f}}, {{860, 22, 0}, {1420, 18, -9}, {2350, 20, -12}, {3900, 15, -18}});
	air(r, -24, 1500, 9000); click(r, -12, 8000); var(r, 6, 20, 3, 20, 0); v[9] = r;
	r = mk("RIM", 80, {{0, 1.0f, 0.05f, 0.6f}}, {{470, 9, -3}, {1720, 16, 0}, {2480, 20, -4}, {3850, 18, -7}, {6100, 14, -12}});
	air(r, -14, 3000, 13000); click(r, -5, 12000); var(r, 4, 20, 3, 20, 0); v[10] = r;
	r = mk("PING", 70, {{0, 1.0f, 0.05f, 0.45f}}, {{5800, 14, 0}, {1080, 7, -5}, {8700, 12, -8}, {2950, 8, -12}});
	air(r, -20, 3000, 14000); click(r, -6, 14000); var(r, 4, 20, 3, 20, 0); v[11] = r;
	r = mk("SWITCH", 45, {{0, 0.7f, 0.04f, 0.35f}, {2.6f, 1.0f, 0.04f, 0.45f}, {7.5f, 0.2f, 0.04f, 0.4f}}, {{3350, 12, 0}, {5200, 14, -3}, {7900, 12, -6}, {1450, 6, -12}});
	air(r, -12, 3500, 15000); click(r, -3, 15000); var(r, 8, 20, 3, 20, 25); v[12] = r;
	r = mk("COINS", 140, {{0, 1.0f, 0.04f, 0.4f}, {4.5f, 0.6f, 0.04f, 0.4f}}, {{8500, 60, 0}, {10900, 70, -2}, {12700, 60, -5}, {6300, 40, -6}, {4100, 30, -14}});
	air(r, -22, 5000, 16000); click(r, -10, 15000); var(r, 9, 25, 4, 30, 60); v[13] = r;
	r = mk("GLASS", 260, {{0, 1.0f, 0.05f, 0.5f}}, {{4880, 140, 0}, {7900, 120, -8}, {2040, 60, -14}, {11300, 90, -15}});
	air(r, -24, 3000, 14000); click(r, -10, 12000); var(r, 12, 25, 3, 30, 0); v[14] = r;
	r = mk("TIN", 150, {{0, 1.0f, 0.06f, 0.7f}}, {{492, 14, 0}, {926, 18, -8}, {2210, 25, -10}, {3990, 30, -6}, {6400, 25, -14}});
	air(r, -18, 2000, 12000); click(r, -8, 10000); var(r, 10, 25, 3, 30, 0); v[15] = r;
	r = mk("PEBBLE", 40, {{0, 1.0f, 0.04f, 0.5f}, {1.1f, 0.4f, 0.04f, 0.4f}}, {{3900, 9, 0}, {5600, 9, -3}, {2500, 6, -8}, {8800, 8, -9}});
	air(r, -12, 3000, 15000); click(r, -4, 14000); var(r, 14, 25, 4, 25, 60); v[16] = r;
	r = mk("DROP", 60, {{0, 0.15f, 0.1f, 0.6f}}, {{2400, 3, -6}});
	air(r, -20, 2000, 9000); click(r, -14, 6000); var(r, 22, 20, 3, 30, 0);
	r.chirp = true; r.chirp0 = 1150; r.chirp1 = 2300; r.chirpMs = 14; r.chirpDb = 0; v[17] = r;
	r = mk("CRACKLE", 12, {{0, 1.0f, 0.02f, 0.18f}}, {{4200, 1.2f, 0}});
	air(r, -3, 1200, 16000); click(r, 0, 16000); var(r, 40, 20, 6, 60, 0); v[18] = r;
	// drum machine style: filtered noise pulses, nearly identical every time
	r = mk("808", 170, {{0, 1.0f, 0.05f, 4.0f}, {9.0f, 0.7f, 1.0f, 30.0f}}, {{1020, 1.5f, 0}});
	air(r, -24, 1500, 9000); var(r, 0.5f, 3, 0.3f, 4, 0); r.choke = true; v[19] = r;
	r = mk("909", 220, {{0, 1.0f, 0.05f, 4.5f}, {10.0f, 0.6f, 1.0f, 42.0f}}, {{1100, 1.2f, 0}, {2300, 1.2f, -1}, {4600, 1.2f, -4}});
	air(r, -9, 2500, 14000); var(r, 0.5f, 3, 0.3f, 4, 0); r.choke = true; v[20] = r;
	r = mk("NOISE", 60, {{0, 1.0f, 0.05f, 6.0f}}, {});
	air(r, 0, 20, 20000); var(r, 0, 0, 0.5f, 30, 0); r.choke = true; v[21] = r;
	return v;
}

// ---------------------------------------------------------------- filters (offline, double)
inline void onepoleLp(std::vector<double>& x, double fc, double sr) {
	double a = std::exp(-2.0 * M_PI * fc / sr), y = 0;
	for (double& s : x) { y = (1 - a) * s + a * y; s = y; }
}
inline void onepoleHp(std::vector<double>& x, double fc, double sr) {
	double a = std::exp(-2.0 * M_PI * fc / sr), y = 0;
	for (double& s : x) { y = (1 - a) * s + a * y; s = s - y; }
}
// RBJ band-pass, 0 dB peak, added (scaled) into out
inline void bpAdd(const std::vector<double>& x, std::vector<double>& out, double f, double q, double scale, double sr) {
	double w = 2 * M_PI * std::min(f, sr * 0.45) / sr, al = std::sin(w) / (2 * q);
	double a0 = 1 + al, b0 = al / a0, b2 = -al / a0, a1 = -2 * std::cos(w) / a0, a2 = (1 - al) / a0;
	double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	for (size_t i = 0; i < x.size(); i++) {
		double y = b0 * x[i] + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1; x1 = x[i]; y2 = y1; y1 = y;
		out[i] += scale * y;
	}
}

inline std::vector<float> renderVariant(const Recipe& rec, Rng& rng) {
	const double sr = TABLE_SR;
	const double fv = rec.var[0], qv = rec.var[1], gv = rec.var[2], tv = rec.var[3], dv = rec.var[4];
	int n = (int)(rec.lenMs * sr / 1000.0);
	double tscale = std::pow(2.0, rng.uni(-1, 1) * tv / 100.0);
	std::vector<double> exc(n, 0.0);
	for (const Contact& c : rec.contacts) {
		double d = c.dly * (1 + rng.uni(-1, 1) * dv / 100.0);
		int i0 = (int)(d * sr / 1000.0);
		if (i0 >= n) continue;
		double a = c.att / 1000.0, dc = c.dec / 1000.0 * tscale;
		// normalise the attack/decay envelope to peak 1
		double emax = 1e-12;
		for (int i = 0; i < n - i0; i++) {
			double t = i / sr, e = (1 - std::exp(-t / a)) * std::exp(-t / dc);
			if (e > emax) emax = e;
			if (t > 6 * (a + dc)) break;
		}
		double gg = (c.dly > 0) ? c.g * std::pow(10.0, rng.uni(-1, 1) * gv / 20.0) : c.g;
		for (int i = 0; i < n - i0; i++) {
			double t = i / sr, e = (1 - std::exp(-t / a)) * std::exp(-t / dc) / emax;
			exc[i0 + i] += gg * e * rng.normal();
		}
	}
	if (rec.excLp > 0) { onepoleLp(exc, rec.excLp, sr); onepoleLp(exc, rec.excLp, sr); }
	std::vector<double> out(n, 0.0);
	double fshift = std::pow(2.0, rng.uni(-1, 1) * fv / 100.0);
	for (const Mode& m : rec.modes) {
		double ff = m.f * fshift * std::pow(2.0, rng.uni(-1, 1) * fv / 100.0 * 0.35);
		double qq = m.q * std::pow(2.0, rng.uni(-1, 1) * qv / 100.0);
		double gg = std::pow(10.0, (m.gdb + rng.uni(-1, 1) * gv) / 20.0);
		bpAdd(exc, out, ff, qq, gg * std::sqrt(qq), sr);
	}
	if (rec.air) {
		std::vector<double> a = exc;
		onepoleHp(a, rec.airHp, sr); onepoleLp(a, rec.airLp, sr);
		double g = std::pow(10.0, rec.airDb / 20.0) * 3.0;
		for (int i = 0; i < n; i++) out[i] += g * a[i];
	}
	if (rec.click) {
		std::vector<double> c(n, 0.0); c[0] = 1.0;
		onepoleLp(c, rec.clickLp, sr); onepoleLp(c, rec.clickLp, sr);
		double cm = 1e-12, om = 0;
		for (int i = 0; i < n; i++) { cm = std::max(cm, std::fabs(c[i])); om = std::max(om, std::fabs(out[i])); }
		double g = std::pow(10.0, rec.clickDb / 20.0) * (om > 0 ? om : 1.0) / cm;
		for (int i = 0; i < n; i++) out[i] += g * c[i];
	}
	if (rec.chirp) {
		double k = fshift * std::pow(2.0, rng.uni(-1, 1) * fv / 100.0);
		double d = rec.chirpMs / 1000.0 * tscale;
		double om = 1e-3;
		for (int i = 0; i < n; i++) om = std::max(om, std::fabs(out[i]));
		double g = std::pow(10.0, rec.chirpDb / 20.0) * om * 4;
		for (int i = 0; i < n; i++) {
			double t = i / sr, tt = std::min(t, d);
			if (t >= d * 3) break;
			double ph = 2 * M_PI * k * (rec.chirp0 * tt + (rec.chirp1 - rec.chirp0) * tt * tt / (2 * d));
			out[i] += g * std::sin(ph) * std::exp(-t / (d * 0.45));
		}
	}
	int fade = (int)(0.003 * sr);
	for (int i = 0; i < fade && i < n; i++) out[n - fade + i] *= 1.0 - (double)i / (fade - 1);
	onepoleHp(out, 30, sr);
	double pk = 1e-9;
	for (double s : out) pk = std::max(pk, std::fabs(s));
	std::vector<float> res(n);
	for (int i = 0; i < n; i++) res[i] = (float)(out[i] / pk);
	return res;
}

// ---------------------------------------------------------------- surfaces
struct Surface {
	std::string name;
	std::vector<std::vector<float>> vars;
	std::vector<float> rates;   // sample rate of each variant
	bool choke = false;
};

inline std::vector<Surface> buildSurfaces(uint64_t seed = 1) {
	std::vector<Recipe> rec = makeRecipes();
	std::vector<Surface> s(NUM_BUILTIN);
	const int groupFrom[6] = {0, 1, 2, 3, 0, 2};   // CLAP PALMS CUPPED FINGERS CLAP CUPPED
	for (int si = 0; si < NUM_BUILTIN; si++) {
		Rng rng(seed * 1000 + si);
		s[si].name = SURFACE_NAMES[si];
		s[si].choke = rec[si].choke;
		for (int v = 0; v < NVAR; v++) {
			const Recipe& r = (si == 4) ? rec[groupFrom[v % 6]] : rec[si];
			s[si].vars.push_back(renderVariant(r, rng));
			s[si].rates.push_back(TABLE_SR);
		}
	}
	return s;
}

}  // namespace clapfall
