// Rimbaud: a physical model of a snare drum with its rim (no Rack code here,
// so test/render.cpp can run it offline).
//
// How it works, in plain words. Everything is in real units (metres,
// kilograms, newtons) so the parts talk to each other the way they do on a
// real drum:
//
//  * TWO HEADS. The top (batter) head and the thin bottom (snare-side) head are
//    stretched membranes. Each rings at many frequencies at once ("modes"),
//    spaced like the zeros of Bessel functions, not like a harmonic series.
//    The batter has ~150 exact modes plus a cloud of high ones; the snare side ~50.
//  * THE AIR INSIDE couples the heads. Push the top head in and the air pushes
//    the bottom head out. That splits the lowest mode into a pair and is the
//    ONLY way the bottom head (and so the snares) gets moving, just like on a
//    real drum. DEPTH changes the amount of air and so the coupling.
//  * PITCH DROP. Hitting hard stretches the head, which raises its tension for
//    a moment, so the drum starts sharp and falls back as it dies away
//    (computed from the head's real stretch, Mylar stiffness and thickness).
//  * THE STICK is a mass that flies into the head and bounces off a springy
//    tip. Soft hits stay in contact longer and sound darker, hard hits are
//    brighter: that comes out of the contact, it is not a filter. BUZZ keeps the
//    hand pressing after the hit, so the stick bounces several times (a buzz
//    or press stroke).
//  * SNARE WIRES are 8 strips of coiled wire pressed against the bottom head
//    by the strainer (TENSION). When the head moves away faster than the wires
//    can follow they lift off and slap back. That is the rattle: real
//    collisions, with the force going back into the head (so snares also deaden
//    the bottom head, as on a real drum). Each slap also rings the wire coils
//    themselves (the bright fizz).
//  * THE RIM (hoop) is a metal ring with its own modes (flanged steel rings,
//    die-cast is heavier and drier). A RIMSHOT is the tip hitting the head and
//    the shaft hitting the hoop at the same moment. A CROSS-STICK is the stick
//    lying on the head with its butt end striking the hoop: the stick itself
//    rings (a wooden beam), and pushes the head where it lies.
//  * THE SHELL (wood <-> metal) rings a little, driven by the hoop and heads.
//  * MICS: TOP is a close mic over the batter, BOTTOM a mic under the snares,
//    MIX blends them with the sound of the whole drum in the room.
//  * EXT: outside sound pushes on the heads, so a bassline or kick can make the
//    snares buzz in sympathy.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace rimbaud {

static const float PI = 3.14159265358979f;
static const int BLOCK = 16;     // coefficients refreshed every 16 samples
static const int MAXZ = 260;     // Bessel zeros in the table
static const int SHAPE_N = 64;   // radial samples per mode shape
static const int NDIF = 40;      // high "cloud" modes of the batter
static const int MAXT = MAXZ + NDIF;
static const int MAXB = 72;      // snare-side modes
static const int NW = 8;         // snare wire segments
static const int NCOIL = 3;      // coil resonances per segment
static const int NH = 18;        // hoop modes (2 families x 9)
static const int NST = 6;        // stick bending modes
static const int NSH = 8;        // shell modes
static const double JTOP = 34.0; // batter modes up to this Bessel zero
static const double JBOT = 20.0; // snare-side modes up to this zero

// ---------------------------------------------------------------- Bessel table
// J_m(x) by the trapezoid rule on Bessel's integral (exact to rounding here).
static inline double besselJ(int m, double x) {
	const int N = 128;
	double s = 0.0;
	for (int k = 0; k < N; k++) {
		double t = 2.0 * 3.14159265358979323846 * k / N;
		s += std::cos(m * t - x * std::sin(t));
	}
	return s / N;
}

struct ModeTable {
	int n = 0;
	int m[MAXZ];
	int nr[MAXZ];          // radial index (1 = first zero)
	double j[MAXZ];        // the zero j_mn
	double jm1[MAXZ];      // J_{m+1}(j_mn), for masses
	float shape[MAXZ][SHAPE_N + 1];  // J_m(j * r/a), r/a = 0..1
};

static inline ModeTable* buildTable() {
	ModeTable* t = new ModeTable();
	struct Z { int m, n; double j; };
	std::vector<Z> zs;
	for (int m = 0; m <= 44; m++) {
		double step = 0.05, x0 = m == 0 ? 0.5 : (double) m;  // J_m is ~0 below x = m (first zero is above m)
		double fprev = besselJ(m, x0);
		int nr = 0;
		for (double x = x0 + step; x < JTOP; x += step) {
			double f = besselJ(m, x);
			if ((f < 0) != (fprev < 0)) {
				double lo = x - step, hi = x, flo = fprev;
				for (int it = 0; it < 40; it++) {
					double mid = 0.5 * (lo + hi), fm = besselJ(m, mid);
					if ((fm < 0) == (flo < 0)) { lo = mid; flo = fm; } else hi = mid;
				}
				zs.push_back({m, ++nr, 0.5 * (lo + hi)});
			}
			fprev = f;
		}
	}
	std::sort(zs.begin(), zs.end(), [](const Z& a, const Z& b) { return a.j < b.j; });
	t->n = std::min((int) zs.size(), MAXZ);
	for (int k = 0; k < t->n; k++) {
		t->m[k] = zs[k].m;
		t->nr[k] = zs[k].n;
		t->j[k] = zs[k].j;
		t->jm1[k] = besselJ(zs[k].m + 1, zs[k].j);
		for (int i = 0; i <= SHAPE_N; i++) t->shape[k][i] = (float) besselJ(zs[k].m, zs[k].j * i / SHAPE_N);
	}
	return t;
}

static inline const ModeTable& table() {
	static const ModeTable* t = buildTable();
	return *t;
}

// mode shape of table mode k at radius rr (0 centre .. 1 edge), angle th
static inline float shapeAt(int k, float rr, float th) {
	const ModeTable& T = table();
	float p = std::min(std::max(rr, 0.f), 1.f) * SHAPE_N;
	int i = std::min((int) p, SHAPE_N - 1);
	float fr = p - i;
	float v = T.shape[k][i] + (T.shape[k][i + 1] - T.shape[k][i]) * fr;
	return v * std::cos(T.m[k] * th);
}

// ---------------------------------------------------------------- helpers
struct Rng {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v * 2654435761u + 0x6D2B79F5u; if (!s) s = 1; for (int i = 0; i < 4; i++) next(); }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

// Solve F = K * max(0, d0 - G*F)^p for F >= 0 (an implicit, always-stable contact).
// d0 = overlap if no force acted this sample, G = how much the force opens the gap per newton.
static inline double powp(double x, double p) { return p == 1.5 ? x * std::sqrt(x) : std::pow(x, p); }

static inline float solveContact(double d0, double G, double K, double p) {
	if (d0 <= 0.0) return 0.f;
	double lo = 0.0, hi = d0 / G;
	double F = std::min(hi * 0.5, K * powp(d0, p));
	for (int it = 0; it < 12; it++) {
		double x = d0 - G * F;
		if (x <= 0.0) { hi = F; F = 0.5 * (lo + hi); continue; }
		double kx = K * powp(x, p);
		double h = F - kx;
		if (h > 0) hi = F; else lo = F;
		double dh = 1.0 + p * kx / x * G;
		double Fn = F - h / dh;
		if (!(Fn > lo && Fn < hi)) Fn = 0.5 * (lo + hi);
		if (std::fabs(Fn - F) < 1e-7 * (1.0 + F)) { F = Fn; break; }
		F = Fn;
	}
	return (float) F;
}

// a bank of damped modes driven by forces: q[n+1] = a1 q[n] - a2 q[n-1] + g F
// (impulse-invariant: an impulse I gives exactly I/(M w) e^-st sin(wt))
static inline void modeCoefs(float f, float sigma, float M, float dt, float& a1, float& a2, float& g) {
	float w = 2.f * PI * f;
	float r = std::exp(-sigma * dt);
	a1 = 2.f * r * std::cos(w * dt);
	a2 = r * r;
	g = dt * r * std::sin(w * dt) / (w * M);
}

struct Controls {
	float size = 14.f;      // diameter, inches (10..15)
	float depth = 6.5f;     // shell depth, inches (3..8)
	float tuneHz = 240.f;   // batter head fundamental on its own (Hz)
	float reso = 1.5f;      // snare-side head pitch / batter pitch
	float damp = 0.15f;     // muffling (gel pad near the edge) 0..1
	float shell = 0.2f;     // 0 wood .. 1 metal
	float position = 0.3f;  // 0 centre .. 1 edge
	float stick = 0.15f;    // 0 hard tip .. 1 soft mallet
	float buzz = 0.f;       // hand press after the hit, 0..1
	float rim = 0.5f;       // rimshot: share of the stick on the hoop
	float hoop = 0.3f;      // 0 triple-flanged steel .. 1 die-cast
	float xstick = 0.5f;    // cross-stick: short (high click) .. long (low knock)
	bool snares = true;
	float tension = 0.5f;   // strainer: loose (long buzz) .. tight (short, crisp)
	float wires = 0.5f;     // 12 .. 42 strands
	float symp = 0.5f;      // EXT input sensitivity
	float mic = 0.3f;       // MIX: top .. bottom mic
	float level = 1.f;
};

enum Stroke { HEAD = 0, RIMSHOT = 1, CROSS = 2 };

struct Engine {
	// ---- materials (SI)
	static constexpr float RHO_AIR = 1.2f, C_AIR = 343.f;
	static constexpr float SIG_TOP = 0.36f;   // kg/m2, 10 mil coated Mylar
	static constexpr float SIG_BOT = 0.106f;  // kg/m2, 3 mil Mylar
	static constexpr float EH_TOP = 1.2e6f;   // N/m, Young's modulus x thickness
	static constexpr float EH_BOT = 3.6e5f;
	static constexpr float NU = 0.35f;
	static constexpr float K3_LOCAL = 2.f * EH_TOP / (0.03f * 0.03f);   // N/m^3, stretch around a 3 cm dent

	float fs = 48000.f, dt = 1.f / 48000.f;
	int blockPos = 0;
	Rng rng;

	// ---- batter head (top): table modes 0..nT-1, then NDIF cloud modes
	int nT = 0;
	alignas(16) float tq0[MAXT] = {}, tq1[MAXT] = {}, ta1[MAXT] = {}, ta2[MAXT] = {}, tg[MAXT] = {};
	alignas(16) float tF[MAXT] = {};       // external force on each mode this sample (air etc.)
	alignas(16) float tPhiS[MAXT] = {};    // shape at the stick point
	alignas(16) float tPhiX[MAXT] = {};    // shape where the cross-stick lies
	alignas(16) float tMic[MAXT] = {}, tRoom[MAXT] = {};
	float tf0[MAXT], tsig[MAXT], tM[MAXT], tS[MAXT], tK2N[MAXT], tPad[MAXT], tdet[MAXT], tRndS[MAXT], tRndM[MAXT];
	int tm[MAXT];
	float tAmp2[MAXT] = {};
	alignas(16) float tMw2[MAXT] = {};   // M w^2 (stiffness) of each batter mode, for the in-cycle tension change
	int nKC = 0;                         // modes slow enough for that term to stay stable

	// ---- snare-side head (bottom)
	int nB = 0;
	alignas(16) float bq0[MAXB] = {}, bq1[MAXB] = {}, ba1[MAXB] = {}, ba2[MAXB] = {}, bg[MAXB] = {};
	alignas(16) float bMic[MAXB] = {}, bRoom[MAXB] = {};
	alignas(16) float psi[NW][MAXB];   // shape at each wire segment
	float bf0[MAXB], bsig[MAXB], bM[MAXB], bS[MAXB], bK2N[MAXB], bdet[MAXB];
	int bm[MAXB];

	// ---- snare wires
	float wUprev[NW] = {};
	float ww[NW] = {}, wv[NW] = {}, wEq[NW] = {}, wDel[NW] = {}, wFprev[NW] = {}, wEnv[NW] = {};
	float wK[NW], wM[NW], wC[NW], wPos[NW], wJit[NW];
	float wEqTarget = 0.004f, wEqNow = 0.004f;
	float coilA1[NW][NCOIL], coilA2[NW][NCOIL], coilB[NW][NCOIL], coilY1[NW][NCOIL] = {}, coilY2[NW][NCOIL] = {};
	float coilOut = 0.f, coilOutPrev = 0.f;

	// ---- hoop, stick bending modes, shell, cavity ring
	alignas(16) float hq0[NH] = {}, hq1[NH] = {}, ha1[NH], ha2[NH], hg[NH], hMic[NH];
	float hf0[NH], hdet[NH];
	alignas(16) float sq0[NST] = {}, sq1[NST] = {}, sa1[NST], sa2[NST], sg[NST];
	float sPhiShaft[NST], sPhiEnd[NST], sPhiTip[NST];
	alignas(16) float shq0[NSH] = {}, shq1[NSH] = {}, sha1[NSH], sha2[NSH], shg[NSH], shMic[NSH];
	float cavA1 = 0, cavA2 = 0, cavB = 0, cavY1 = 0, cavY2 = 0, cavIn1 = 0, cavIn2 = 0;

	// ---- stick state
	struct Mass {
		bool active = false;
		float s = 0, v = 0, m = 0.045f;
		float K = 1e8f, p = 1.5f;
		float press = 0, pressT = 0, hold = 0;  // buzz: finger spring (N/m), how long (s), aim (m)
		float t = 0, F = 0, Fprev = 0, dprev = 0;
	};
	Mass tip, shaft, butt;
	int stroke = HEAD;
	float xCoupleK = 0.f, xPrev = 0.f;   // cross-stick: stick tip resting on the head
	float strikeRR = 0.3f;

	// ---- geometry, cached
	float a = 0.1778f, D = 0.165f, kappa = 0.f, area = 0.f;
	float T0top = 2000.f, T0bot = 2000.f;
	float lastSize = -1.f;
	float pitchTop = 1.f, pitchBot = 1.f;  // tension-modulation frequency ratios
	float extP = 0.f;

	// ---- output
	float dcT = 0, dcB = 0, dcM = 0, hpT = 0, hpB = 0, hpM = 0;
	float outTop = 0, outBot = 0, outMix = 0;
	float meter = 0;
	float dbg[8] = {};   // raw parts of the sound, for test/render.cpp
	float roomVprev = 0, roomAccPrev = 0;
	int quiet = 0;
	bool asleep = false;
	bool primed = false, lastSnares = true;
	int warm = 0;   // output muted while the wires settle onto the head at start-up

	// level calibration (see test/render.cpp)
	static constexpr float G_TOP = 3.2f, G_BOT = 1.6f, G_ROOM = 3.f;
	static constexpr float W_TOP = 0.15f, W_BOT = 0.3f, W_ROOM = 0.25f;   // wire fizz in each mic

	Engine() {
		table();
		Rng g;
		g.seed(4242);
		for (int k = 0; k < MAXT; k++) {
			tdet[k] = 1.f + 0.004f * g.bi();
			tRndS[k] = g.bi();
			tRndM[k] = g.bi();
		}
		for (int k = 0; k < MAXB; k++) bdet[k] = 1.f + 0.005f * g.bi();
		for (int i = 0; i < NW; i++) wJit[i] = g.bi();
		for (int h = 0; h < NH; h++) hdet[h] = 1.f + 0.02f * g.bi();
		setSampleRate(48000.f);
	}

	void setSampleRate(float sr) {
		if (sr == fs && nT > 0) return;
		fs = sr;
		dt = 1.f / sr;
		lastSize = -1.f;
		Controls c;
		configure(c);
	}

	// Build everything that depends on the size of the drum (masses, mic weights).
	void geometry(const Controls& c) {
		const ModeTable& T = table();
		a = 0.5f * c.size * 0.0254f;
		area = PI * a * a;
		// batter: all table modes, then the high cloud
		nT = 0;
		for (int k = 0; k < T.n && nT < MAXZ; k++) {
			int m = T.m[k];
			double eps = (m == 0) ? 2.0 : 1.0;
			double N = a * a * 0.5 * T.jm1[k] * T.jm1[k] * PI * eps;   // integral of shape^2
			tm[nT] = m;
			tM[nT] = (float) (SIG_TOP * N);
			tS[nT] = m == 0 ? (float) (2.0 * PI * a * a * T.jm1[k] / T.j[k]) : 0.f;
			double kk = T.j[k] / a;
			tK2N[nT] = (float) (kk * kk * N);
			tf0[nT] = (float) (T.j[k] / 2.404825557695773);
			// gel pad near the edge, at 90 degrees from the stick
			float sp = shapeAt(k, 0.86f, 1.57f);
			tPad[nT] = sp * sp / (float) (0.5 * T.jm1[k] * T.jm1[k]);
			// close mic 3/4 of the way out, room = volume (monopole) + a little of the rest
			tMic[nT] = shapeAt(k, 0.72f, 0.9f);
			float fr = (float) T.j[k] / 2.4048f;
			tRoom[nT] = m == 0 ? tS[nT] / area : 0.12f * tRndM[nT] / (1.f + 0.2f * m) * std::min(1.f, fr / 6.f);
			nT++;
		}
		// cloud modes (above the table): averaged mass, random shapes
		float Mavg = 0.f;
		for (int k = nT - 20; k < nT; k++) Mavg += tM[k];
		Mavg /= 20.f;
		for (int d = 0; d < NDIF; d++) {
			int k = nT + d;
			tm[k] = 99;
			tM[k] = Mavg;
			tS[k] = 0.f;
			tK2N[k] = 0.f;
			float u = (d + 0.5f + 0.4f * tRndS[k]) / NDIF;
			tf0[k] = (float) (JTOP / 2.4048) * std::pow(4.5f, u);  // ratio to f01
			tPad[k] = 0.5f;
			tMic[k] = 0.35f * tRndM[k];
			tRoom[k] = 0.06f * tRndS[k];
		}
		// snare side: modes up to JBOT
		nB = 0;
		for (int k = 0; k < T.n && nB < MAXB; k++) {
			if (T.j[k] > JBOT) continue;
			int m = T.m[k];
			double eps = (m == 0) ? 2.0 : 1.0;
			double N = a * a * 0.5 * T.jm1[k] * T.jm1[k] * PI * eps;
			bm[nB] = m;
			bM[nB] = (float) (SIG_BOT * N);
			bS[nB] = m == 0 ? (float) (2.0 * PI * a * a * T.jm1[k] / T.j[k]) : 0.f;
			double kk = T.j[k] / a;
			bK2N[nB] = (float) (kk * kk * N);
			bf0[nB] = (float) (T.j[k] / 2.404825557695773);
			bMic[nB] = shapeAt(k, 0.5f, 1.3f);
			bRoom[nB] = m == 0 ? bS[nB] / area : 0.f;
			// wire segments lie along a diameter (angle 0 and pi), 82 % of the way to the edge
			for (int i = 0; i < NW; i++) {
				float x = -0.82f + 1.64f * (i + 0.5f) / NW;
				psi[i][nB] = shapeAt(k, std::fabs(x), x < 0 ? PI : 0.f);
			}
			nB++;
		}
		for (int i = 0; i < NW; i++) wPos[i] = -0.82f + 1.64f * (i + 0.5f) / NW;
		lastSize = c.size;
	}

	// Recompute frequencies and damping (every block: tension modulation, knobs).
	void configure(const Controls& c) {
		if (std::fabs(c.size - lastSize) > 1e-4f) geometry(c);
		D = c.depth * 0.0254f;
		float V0 = area * D;
		kappa = RHO_AIR * C_AIR * C_AIR / V0;

		const float f01 = c.tuneHz;
		const float fb01 = c.tuneHz * c.reso;
		// static tensions from the pitch (T = sigma (2 pi f a / j01)^2)
		float w1 = 2.f * PI * f01 * a / 2.4048f, wb = 2.f * PI * fb01 * a / 2.4048f;
		T0top = SIG_TOP * w1 * w1;
		T0bot = SIG_BOT * wb * wb;

		// tension modulation: mean stretch of each head from the mode amplitudes
		float st = 0.f, sb = 0.f;
		for (int k = 0; k < nT; k++) st += tAmp2[k] * tK2N[k];
		for (int k = 0; k < nB; k++) {
			float w = 2.f * PI * fb01 * bf0[k] * pitchBot;
			float v = (bq0[k] - bq1[k]) * fs;
			sb += (bq0[k] * bq0[k] + v * v / (w * w)) * bK2N[k];
		}
		float dTt = EH_TOP / (2.f * (1.f - NU)) * st / area;
		float dTb = EH_BOT / (2.f * (1.f - NU)) * sb / area;
		pitchTop = std::min(1.8f, std::sqrt(1.f + dTt / T0top));
		pitchBot = std::min(1.8f, std::sqrt(1.f + dTb / T0bot));

		const float nyq = 0.45f * fs;
		// ---- batter damping: material + radiation + gel pad
		const float damp = c.damp;
		for (int k = 0; k < nT + NDIF; k++) {
			float f = f01 * tf0[k] * tdet[k] * pitchTop;
			if (f > nyq) { ta1[k] = ta2[k] = tg[k] = 0.f; tq0[k] = tq1[k] = 0.f; continue; }
			float Tint = 1.5f / (1.f + f / 900.f);
			float sig = 6.91f / Tint;
			int m = tm[k];
			if (m == 0) sig += (tf0[k] < 1.01f) ? 9.f : 4.f;       // monopole radiation
			else if (m == 1) sig += 1.5f;                            // dipole
			if (m == 99) sig += 10.f + f * 0.004f;                   // the cloud dies fast
			sig += damp * (5.f + 70.f * tPad[k]);
			tsig[k] = sig;
			modeCoefs(f, sig, tM[k], dt, ta1[k], ta2[k], tg[k]);
			float w = 2.f * PI * f;
			tMw2[k] = (k < nT && w * dt < 1.0f) ? tM[k] * w * w : 0.f;
		}
		// ---- snare side
		for (int k = 0; k < nB; k++) {
			float f = fb01 * bf0[k] * bdet[k] * pitchBot;
			if (f > nyq) { ba1[k] = ba2[k] = bg[k] = 0.f; bq0[k] = bq1[k] = 0.f; continue; }
			float Tint = 1.2f / (1.f + f / 1200.f);
			float sig = 6.91f / Tint + (bm[k] == 0 ? 4.f : 0.f) + damp * 4.f;
			bsig[k] = sig;
			modeCoefs(f, sig, bM[k], dt, ba1[k], ba2[k], bg[k]);
		}

		// ---- snare wires: strands, strainer tension and the snare bed
		float strands = 12.f + 30.f * c.wires;
		float wireMass = strands * 0.00045f;                  // kg, all strands
		float L = 1.64f * a;
		float Tw = 10.f + 90.f * c.tension;                    // N, strainer pull
		float bed = 0.0001f + 0.0012f * c.tension;            // m, how far the wires are pressed in (snare bed)
		for (int i = 0; i < NW; i++) {
			float x = wPos[i] / 0.82f;
			wM[i] = wireMass / NW;
			wK[i] = (PI * PI * Tw / L) / NW * (1.f + 1.2f * x * x) * (1.f + 0.12f * wJit[i]);
			wC[i] = 2.f * 0.06f * std::sqrt(wK[i] * wM[i]);
			wDel[i] = bed * (1.f - 0.35f * x * x);
		}
		wEqTarget = c.snares ? -1.f : 0.004f;   // -1 = "on" (use the bed depth)
		// coil resonances (bright fizz)
		static const float CF[NCOIL] = {3100.f, 5200.f, 8300.f};
		for (int i = 0; i < NW; i++)
			for (int j = 0; j < NCOIL; j++) {
				float f = CF[j] * (1.f + 0.13f * wJit[(i + j) % NW]) * (0.9f + 0.25f * c.tension);
				f = std::min(f, nyq);
				float Q = 5.f + 3.f * j;
				float r = std::exp(-PI * f / Q * dt);
				coilA1[i][j] = 2.f * r * std::cos(2.f * PI * f * dt);
				coilA2[i][j] = r * r;
				coilB[i][j] = (1.f - r);
			}

		// ---- hoop: in-plane and out-of-plane bending families, n = 2..10
		float Ch = 90.f + 80.f * c.hoop;                      // die-cast is thicker: higher
		float Mh = 0.17f + 0.25f * c.hoop;                     // modal mass, kg
		for (int h = 0; h < NH; h++) {
			int n = 2 + (h % 9);
			float fam = h < 9 ? 1.f : 0.83f;
			float nn = (float) n;
			float f = Ch * fam * nn * (nn * nn - 1.f) / std::sqrt(nn * nn + 1.f) * hdet[h] * (14.f / c.size);
			f = std::min(f, nyq);
			float T60 = (0.55f - 0.42f * c.hoop) * (n < 4 ? 0.3f : 1.f) / (1.f + f / 2500.f);   // the head collar and lugs damp it
			modeCoefs(f, 6.91f / T60, Mh, dt, ha1[h], ha2[h], hg[h]);
			hMic[h] = (h % 2 ? -1.f : 1.f) * (h < 9 ? 1.f : 0.7f);
		}
		// ---- stick bending modes (free-free hickory beam); XSTICK = free length
		static const float SR[NST] = {1.f, 2.757f, 5.404f, 8.933f, 13.344f, 18.64f};
		static const float SH[NST] = {0.55f, -0.9f, 0.35f, 0.8f, -0.7f, 0.5f};
		float fs1 = 900.f * std::pow(0.38f, c.xstick);         // 900 .. 340 Hz
		for (int s = 0; s < NST; s++) {
			float f = std::min(fs1 * SR[s], nyq);
			float T60 = 0.07f / (1.f + 0.3f * s);   // held in the hand: short
			modeCoefs(f, 6.91f / T60, 0.012f, dt, sa1[s], sa2[s], sg[s]);
			sPhiShaft[s] = SH[s];
			sPhiEnd[s] = (s % 2 ? -1.4f : 1.4f);
			sPhiTip[s] = 1.4f;
		}
		// ---- shell: wood (short, low) .. metal (long, bright)
		static const float SHR[NSH] = {1.f, 1.62f, 2.31f, 2.96f, 3.73f, 4.52f, 5.41f, 6.3f};
		float fsh = (380.f + 260.f * c.shell) * (14.f / c.size) * std::sqrt(6.5f / c.depth);
		for (int s = 0; s < NSH; s++) {
			float f = std::min(fsh * SHR[s], nyq);
			float T60 = (0.05f + 0.9f * c.shell * c.shell) / (1.f + 0.15f * s);
			modeCoefs(f, 6.91f / T60, 0.25f, dt, sha1[s], sha2[s], shg[s]);
			shMic[s] = (s % 2 ? -1.f : 1.f) * (0.4f + 0.6f * c.shell);
		}
		// ---- the first air resonance between the heads (c / 2 depth)
		{
			float f = std::min(C_AIR / (2.f * D), nyq);
			float Q = 12.f;
			float r = std::exp(-PI * f / Q * dt);
			cavA1 = 2.f * r * std::cos(2.f * PI * f * dt);
			cavA2 = r * r;
			cavB = 1.f - r;
		}
	}

	// ---- a hit. vel 0..1 (more allowed). stroke: HEAD, RIMSHOT or CROSS.
	void hit(float vel, int type, const Controls& c) {
		asleep = false;
		quiet = 0;
		stroke = type;
		vel = std::max(vel, 0.f);
		float v0 = 0.3f * std::pow(25.f, std::min(vel, 1.2f));   // m/s, 0.3 .. 7.5 (and a bit over)
		// the stick tip: hard (nylon/wood) .. soft (felt mallet)
		float st = c.stick;
		float pexp = 1.5f + 1.3f * st;
		float d100 = 0.00005f * std::pow(120.f, st);           // squash at 100 N: 0.05 .. 6 mm
		float K = 100.f / std::pow(d100, pexp);
		float msum = 0.022f + 0.02f * st;                       // mass felt at the tip (the hand pivots the stick); mallets a bit heavier

		float rr = (type == CROSS) ? 0.f : (0.03f + 0.9f * c.position);
		strikeRR = rr;
		if (type != CROSS) {
			for (int k = 0; k < nT; k++) tPhiS[k] = shapeAt(k, rr, 0.f);
			for (int d = 0; d < NDIF; d++)
				tPhiS[nT + d] = tRndS[nT + d] * (0.15f + 0.85f * rr) * 0.6f;
			float u = 0.f;
			for (int k = 0; k < nT + NDIF; k++) u += tPhiS[k] * tq0[k];
			tip = Mass();
			tip.active = true;
			tip.m = (type == RIMSHOT) ? msum * (1.f - 0.3f * c.rim) : msum;
			tip.K = K;
			tip.p = pexp;
			tip.s = u + 0.00005f;
			tip.v = -v0;
			if (c.buzz > 0.001f && type == HEAD) {
				// the fingers hold the stick down like a spring aimed below the head
				tip.press = 300.f + 1500.f * c.buzz;        // finger spring, N/m
				tip.hold = 0.001f + 0.004f * c.buzz;         // aimed this far into the head (m)
				tip.pressT = 0.04f + 0.3f * c.buzz;
			}
		}
		if (type == RIMSHOT) {
			float u = 0.f;
			for (int h = 0; h < NH; h++) u += hq0[h];
			shaft = Mass();
			shaft.active = true;
			shaft.m = msum * (0.5f + 1.5f * c.rim);   // the shaft carries the weight near the hand
			shaft.K = 6e8f;       // wood shaft on steel
			shaft.p = 1.5f;
			shaft.s = u + 0.00005f;
			shaft.v = -v0 * 0.95f;
		}
		if (type == CROSS) {
			float rx = 0.4f;                      // where the stick lies on the head
			for (int k = 0; k < nT; k++) tPhiX[k] = shapeAt(k, rx, PI);
			for (int d = 0; d < NDIF; d++) tPhiX[nT + d] = 0.f;
			float u = 0.f;
			for (int h = 0; h < NH; h++) u += hq0[h];
			butt = Mass();
			butt.active = true;
			butt.m = 0.03f;
			butt.K = 8e8f;
			butt.p = 1.5f;
			butt.s = u + 0.00005f;
			butt.v = -(0.08f + 0.22f * v0);
			xCoupleK = 1.2e4f;
			xPrev = 0.f;
		}
	}

	// ---- one sample. ext = outside sound pressure (Pa). Results in outTop/outBot/outMix (V).
	void process(const Controls& c, float ext = 0.f) {
		if (!primed) {
			primed = true;
			configure(c);
			wEqNow = c.snares ? -1.f : 0.004f;
			for (int i = 0; i < NW; i++) ww[i] = c.snares ? -wDel[i] * 0.98f : 0.004f;
			warm = (int) (0.3f * fs);
			lastSnares = c.snares;
		}
		if (c.snares != lastSnares) { lastSnares = c.snares; asleep = false; quiet = 0; }
		if (asleep && std::fabs(ext) < 1e-4f) {
			outTop = outBot = outMix = 0.f;
			return;
		}
		asleep = false;
		if (blockPos == 0) {
			// amplitude^2 of every batter mode (for the pitch drop)
			for (int k = 0; k < nT; k++) {
				float w = 2.f * PI * c.tuneHz * tf0[k] * pitchTop;
				float v = (tq0[k] - tq1[k]) * fs;
				tAmp2[k] = tq0[k] * tq0[k] + v * v / (w * w);
			}
			configure(c);
		}
		if (++blockPos >= BLOCK) blockPos = 0;

		const int NTT = nT + NDIF;
		// ---- air spring: volume change of the drum pushes on both heads
		float Vt = 0.f, Vb = 0.f;
		for (int k = 0; k < nT; k++) Vt += tS[k] * tq0[k];
		for (int k = 0; k < nB; k++) Vb += bS[k] * bq0[k];
		float pin = -kappa * (Vt + Vb) - ext;        // net push outward per m^2 on the top
		float pinB = -kappa * (Vt + Vb) - 0.7f * ext;

		// ---- tension right now (Kirchhoff-Carrier): the head's stretch this instant,
		// minus the slow part already in the frequencies. Wobbling within each cycle
		// is what spreads a hard hit's energy upwards (brighter when harder).
		{
			float S = 0.f;
			for (int k = 0; k < nT; k++) S += tq0[k] * tq0[k] * tK2N[k];
			float eps = EH_TOP / (2.f * (1.f - NU)) * S / area / T0top;
			float res = std::min(std::max(eps - (pitchTop * pitchTop - 1.f), -0.3f), 0.5f);
			for (int k = 0; k < nT; k++) tF[k] -= res * tMw2[k] * tq0[k];
		}

		// ---- the dent under the stick: a deep, local push stretches the head
		// around it (large-deflection membrane), so it stiffens. Hard hits bounce
		// off sooner (shorter contact) and the head can't be dented absurdly deep.
		if (tip.active) {
			float u = 0.f;
			for (int k = 0; k < nT; k++) u += tPhiS[k] * tq0[k];
			u = std::min(std::max(u, -0.006f), 0.006f);
			float Floc = -K3_LOCAL * u * u * u;
			for (int k = 0; k < nT; k++) tF[k] += tPhiS[k] * Floc;
		}

		// ---- free update of the heads
		alignas(16) float tqn[MAXT];
		alignas(16) float bqn[MAXB];
		for (int k = 0; k < NTT; k++) tqn[k] = ta1[k] * tq0[k] - ta2[k] * tq1[k] + tg[k] * (pin * tS[k] + tF[k]);
		for (int k = 0; k < nB; k++) bqn[k] = ba1[k] * bq0[k] - ba2[k] * bq1[k] + bg[k] * (pinB * bS[k]);
		std::fill(tF, tF + NTT, 0.f);
		alignas(16) float hqn[NH];
		alignas(16) float sqn[NST];
		for (int h = 0; h < NH; h++) hqn[h] = ha1[h] * hq0[h] - ha2[h] * hq1[h];
		for (int s = 0; s < NST; s++) sqn[s] = sa1[s] * sq0[s] - sa2[s] * sq1[s];

		// ---- stick tip on the batter (implicit contact)
		if (tip.active) {
			float u = 0.f, G = 0.f;
			for (int k = 0; k < NTT; k++) { u += tPhiS[k] * tqn[k]; G += tg[k] * tPhiS[k] * tPhiS[k]; }
			float Fh = 0.f;
			if (tip.pressT > 0.f) Fh = (tip.t < tip.pressT) ? -tip.press * (tip.s + tip.hold) - 0.05f * tip.v : 2.f;
			float vp = tip.v + dt * Fh / tip.m;
			float sp = tip.s + dt * vp;
			float d0 = u - sp;
			float Gs = dt * dt / tip.m;
			float F = solveContact(d0, G + Gs, tip.K, tip.p);
			// a little loss in the tip (Hunt-Crossley), only while squashing
			float dd = (d0 - tip.dprev) * fs;
			if (F > 0.f && dd > 0.f) F *= 1.f + std::min(0.4f * dd, 1.f) * 0.15f;
			tip.dprev = d0;
			for (int k = 0; k < NTT; k++) tqn[k] -= tg[k] * tPhiS[k] * F;
			tip.v = vp + dt * F / tip.m;
			tip.s = sp + Gs * F;
			tip.t += dt;
			if ((tip.t > tip.pressT && tip.v > 0.f && tip.s - u > 0.002f) || tip.t > 1.5f) tip.active = false;
		}

		// ---- rimshot shaft / cross-stick butt on the hoop
		Mass* hm = shaft.active ? &shaft : (butt.active ? &butt : nullptr);
		if (hm) {
			float u = 0.f, G = 0.f;
			for (int h = 0; h < NH; h++) { u += hqn[h]; G += hg[h]; }
			float vp = hm->v, sp = hm->s + dt * vp;
			float d0 = u - sp;
			float Gs = dt * dt / hm->m;
			float F = solveContact(d0, G + Gs, hm->K, hm->p);
			for (int h = 0; h < NH; h++) hqn[h] -= hg[h] * F;
			hm->v = vp + dt * F / hm->m;
			hm->s = sp + Gs * F;
			hm->t += dt;
			if ((hm->v > 0.f && hm->s - u > 0.002f) || hm->t > 0.5f) hm->active = false;
			// the stick itself rings from the knock
			const float* ph = (hm == &shaft) ? sPhiShaft : sPhiEnd;
			for (int s = 0; s < NST; s++) sqn[s] += sg[s] * ph[s] * F;
			// cross-stick: the butt's knock levers the stick down onto the head
			if (hm == &butt)
				for (int k = 0; k < nT; k++) tqn[k] -= tg[k] * tPhiX[k] * 0.12f * F;
		}
		// cross-stick: the stick lies on the head, pressed by the hand: a damped
		// spring between the stick's tip end and the head, solved implicitly (stable)
		if (xCoupleK > 0.f) {
			float uh = 0.f, us = 0.f, Gh = 0.f, Gs = 0.f;
			for (int k = 0; k < nT; k++) { uh += tPhiX[k] * tqn[k]; Gh += tg[k] * tPhiX[k] * tPhiX[k]; }
			for (int s = 0; s < NST; s++) { us += sPhiTip[s] * sqn[s]; Gs += sg[s] * sPhiTip[s] * sPhiTip[s]; }
			float d0 = us - uh;
			float cdt = 3.f * fs;                       // hand/skin damping (N s/m) / dt
			float keff = xCoupleK + cdt;
			float Fx = (keff * d0 - cdt * xPrev) / (1.f + keff * (Gh + Gs));
			float d = d0 - (Gh + Gs) * Fx;
			xPrev = d;
			for (int k = 0; k < nT; k++) tqn[k] += tg[k] * tPhiX[k] * Fx;
			for (int s = 0; s < NST; s++) sqn[s] -= sg[s] * sPhiTip[s] * Fx;
			xCoupleK *= 0.99997f;
			if (xCoupleK < 100.f) xCoupleK = 0.f;
		}

		// ---- snare wires on the bottom head
		float wTarget = wEqTarget;
		wEqNow += ((wTarget < 0.f ? -1.f : wTarget) - wEqNow) * (dt / 0.025f);
		float coilIn = 0.f;
		float Fw[NW];
		{
			float us[NW], G[NW];
			for (int i = 0; i < NW; i++) {
				float u = 0.f, g = 0.f;
				const float* ps = psi[i];
				for (int k = 0; k < nB; k++) { u += ps[k] * bqn[k]; g += bg[k] * ps[k] * ps[k]; }
				us[i] = u;
				G[i] = g;
			}
			for (int i = 0; i < NW; i++) {
				// rest position: pressed into the bed (on) or lifted away (off)
				float on = std::min(std::max(-wEqNow, 0.f), 1.f);
				float eq = on * (-wDel[i]) + (1.f - on) * 0.004f;
				float Fs = -wK[i] * (ww[i] - eq) - wC[i] * wv[i];
				float vp = wv[i] + dt * Fs / wM[i];
				float sp = ww[i] + dt * vp;
				float d0 = us[i] - sp;
				float Gs = dt * dt / wM[i];
				float F = solveContact(d0, G[i] + Gs, 4e7, 1.5);
				// contact loss (Hunt-Crossley), makes the snares deaden the head
				float dd = (d0 - wEq[i]) * fs;
				wEq[i] = d0;
				if (F > 0.f) F *= std::max(0.f, std::min(2.f, 1.f + 0.35f * dd));
				Fw[i] = F;
				float vRel = vp - (us[i] - wUprev[i]) * fs;   // wire speed relative to the head
				wUprev[i] = us[i];
				wv[i] = vp + dt * F / wM[i];
				ww[i] = sp + Gs * F;
				// a slap (the strip lands back on the head): the coil rings, plus a
				// burst of tiny rattles from the many strands in the strip
				float x = 0.f;
				if (F > 0.f && wFprev[i] == 0.f) {
					float imp = std::min(std::fabs(vRel), 2.f);
					wEnv[i] += imp * 1.5f;
					x += imp * 25.f;
				}
				x += 0.04f * (F - wFprev[i]);
				wFprev[i] = F;
				wEnv[i] *= 0.9985f;
				x += wEnv[i] * rng.bi();
				for (int j = 0; j < NCOIL; j++) {
					float y = coilA1[i][j] * coilY1[i][j] - coilA2[i][j] * coilY2[i][j] + coilB[i][j] * x;
					coilY2[i][j] = coilY1[i][j];
					coilY1[i][j] = y;
					coilIn += y * (j == 2 ? 0.7f : 1.f);
				}
			}
			for (int i = 0; i < NW; i++) {
				if (Fw[i] == 0.f) continue;
				const float* ps = psi[i];
				for (int k = 0; k < nB; k++) bqn[k] -= bg[k] * ps[k] * Fw[i];
			}
		}
		coilOut = coilIn;

		// ---- shell, driven by the hoop and the heads' pull at the edge
		float hoopSum = 0.f;
		for (int h = 0; h < NH; h++) hoopSum += hqn[h];
		float shellIn = 1e5f * hoopSum + 1.0f * (T0top * Vt + T0bot * Vb) / (a * a);
		alignas(16) float shqn[NSH];
		for (int s = 0; s < NSH; s++) shqn[s] = sha1[s] * shq0[s] - sha2[s] * shq1[s] + shg[s] * shellIn;

		// ---- mics (velocities)
		float top = 0.f, room = 0.f, bot = 0.f;
		for (int k = 0; k < NTT; k++) {
			float v = (tqn[k] - tq0[k]);
			top += v * tMic[k];
			room += v * tRoom[k];
		}
		for (int k = 0; k < nB; k++) {
			float v = (bqn[k] - bq0[k]);
			bot += v * bMic[k];
			room += v * bRoom[k];
		}
		top *= fs; bot *= fs; room *= fs;
		float hoopV = 0.f, stickV = 0.f, shellV = 0.f;
		for (int h = 0; h < NH; h++) hoopV += (hqn[h] - hq0[h]) * hMic[h];
		for (int s = 0; s < NST; s++) stickV += (sqn[s] - sq0[s]);
		for (int s = 0; s < NSH; s++) shellV += (shqn[s] - shq0[s]) * shMic[s];
		hoopV *= fs; stickV *= fs; shellV *= fs;
		// air ring between the heads (driven by the heads moving apart / together)
		float Vdiff = Vt - Vb;
		float cav = cavA1 * cavY1 - cavA2 * cavY2 + cavB * (Vdiff - 2.f * cavIn1 + cavIn2) * fs * fs * 0.002f;
		cavIn2 = cavIn1; cavIn1 = Vdiff;
		cavY2 = cavY1; cavY1 = cav;

		// ---- shift states
		std::copy(tq0, tq0 + NTT, tq1); std::copy(tqn, tqn + NTT, tq0);
		std::copy(bq0, bq0 + nB, bq1); std::copy(bqn, bqn + nB, bq0);
		std::copy(hq0, hq0 + NH, hq1); std::copy(hqn, hqn + NH, hq0);
		std::copy(sq0, sq0 + NST, sq1); std::copy(sqn, sqn + NST, sq0);
		std::copy(shq0, shq0 + NSH, shq1); std::copy(shqn, shqn + NSH, shq0);

		// ---- mix to volts
		float T = G_TOP * (top + 0.25f * room + 2.5f * hoopV + 0.4f * stickV + 0.5f * shellV + 0.4f * cav) + W_TOP * coilOut;
		float B = G_BOT * (bot + 0.3f * room + 0.7f * hoopV + 0.1f * stickV + 0.5f * shellV + 0.4f * cav) + W_BOT * coilOut;
		// room: the whole drum (both heads breathing) plus the rim, shell and wires, a bit further away
		float roomAcc = room;
		float R = G_ROOM * room + G_TOP * (1.6f * hoopV + 0.3f * stickV + 0.5f * shellV + 0.3f * cav) + W_ROOM * coilOut;
		float M = (1.f - c.mic) * T + c.mic * B + 0.5f * R;
		dbg[0] = top; dbg[1] = bot; dbg[2] = G_ROOM * roomAcc; dbg[3] = hoopV; dbg[4] = stickV; dbg[5] = shellV; dbg[6] = cav; dbg[7] = coilOut;

		// gentle DC / rumble removal (~20 Hz) and a soft ceiling
		const float kdc = 2.f * PI * 20.f * dt;
		dcT += (T - dcT) * kdc; T -= dcT;
		dcB += (B - dcB) * kdc; B -= dcB;
		dcM += (M - dcM) * kdc; M -= dcM;
		float g = c.level;
		outTop = 10.f * std::tanh(T * g * 0.1f);
		outBot = 10.f * std::tanh(B * g * 0.1f);
		outMix = 10.f * std::tanh(M * g * 0.1f);
		if (!std::isfinite(outTop + outBot + outMix)) reset();
		if (warm > 0) { warm--; float f = warm > 0.05f * fs ? 0.f : 1.f - warm / (0.05f * fs); outTop *= f; outBot *= f; outMix *= f; }
		meter = std::max(meter * 0.9995f, std::min(1.f, std::fabs(outMix) * 0.15f));

		// sleep when everything has died away (saves CPU)
		if (std::fabs(outMix) < 2e-5f && std::fabs(outTop) < 2e-5f && std::fabs(outBot) < 2e-5f
		    && !tip.active && !shaft.active && !butt.active && std::fabs(ext) < 1e-4f && warm == 0) {
			if (++quiet > (int) fs) asleep = true;
		} else quiet = 0;
	}

	void reset() {
		std::fill(tq0, tq0 + MAXT, 0.f); std::fill(tq1, tq1 + MAXT, 0.f); std::fill(tF, tF + MAXT, 0.f);
		std::fill(bq0, bq0 + MAXB, 0.f); std::fill(bq1, bq1 + MAXB, 0.f);
		std::fill(hq0, hq0 + NH, 0.f); std::fill(hq1, hq1 + NH, 0.f);
		std::fill(sq0, sq0 + NST, 0.f); std::fill(sq1, sq1 + NST, 0.f);
		std::fill(shq0, shq0 + NSH, 0.f); std::fill(shq1, shq1 + NSH, 0.f);
		for (int i = 0; i < NW; i++) { ww[i] = 0.f; wv[i] = 0.f; wEnv[i] = 0.f; wFprev[i] = 0.f; wEq[i] = 0.f;
			for (int j = 0; j < NCOIL; j++) coilY1[i][j] = coilY2[i][j] = 0.f; }
		tip.active = shaft.active = butt.active = false;
		xCoupleK = 0.f;
		cavY1 = cavY2 = cavIn1 = cavIn2 = 0.f;
		dcT = dcB = dcM = 0.f; roomVprev = 0.f;
		pitchTop = pitchBot = 1.f;
		std::fill(tAmp2, tAmp2 + MAXT, 0.f);
	}
};

}  // namespace rimbaud
