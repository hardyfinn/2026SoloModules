// Bent Sheet Metal engine: one strip of spring steel, 6 width families x 40 lengthwise modes, with a
// 48-point curvature grid. A port of tools/proto/bent_ref.c (the definition of the sound); see
// docs/BUILD-INSTRUCTIONS.md section 3. No Rack code here, so the offline tests use exactly this.
#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace bent {

static const double PI = 3.14159265358979323846;
static const int NF = 6, M = 40, G = 48, N = NF * M;
static const int BLK = 32;

static const double KAP2[NF] = {0, 0, 1, 2.757, 5.404, 8.933};
static const double GAM[NF] = {0, 0.35, 1, 1, 1, 1};
static const double EB[NF] = {0, 0, 1, 0.45, 0.25, 0.15};
static const double LN[NF] = {0.3, 0.6, 1.0, 2.0, 3.2, 4.5};

// The reference's xorshift64, same seed, same draw order.
struct Rng {
	uint64_t s = 0x2545F4914F6CDD1DULL;
	double u() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (s >> 11) * (1.0 / 9007199254740992.0); }
	double b() { return 2 * u() - 1; }
};

// Engine-level controls (already mapped from knobs; reference names in comments).
struct Controls {
	double f1 = 130.81;     // F1 * cTune: flat cut-on of family 2, Hz
	double weMul = 1.0;     // sqrt(cTune) in the reference: scales the snap-well frequency
	double aspect = 5;      // cAspect
	double hand = 0;        // cBendHand
	double shape = 0;       // cShape
	double spot = 0;        // cSpot
	double grip = 0.5;      // cGrip
	double snap = 0;        // cSnap
	double hold = 0.5;      // cHold
	double crack = 1;       // cCrack
	double dip = 0;         // cDip
	double depth = 1;       // cDepth
	double taper = 0;       // cTaper
	double age = 0;         // cAge
	double nl = 1;          // cNL (STRETCH)
	double bowP = 0;        // cBowP
	double bowV = 1;        // cBowV
	double bowX = 0.5;      // cBowX
	double hard = 0.5;      // cHard
	double wb = 1;          // cWB
	double back = 1;        // cBack
	double kick = 1;        // cKick
	double shake = 0;       // cShake
	double pick = 0;        // cPick
	double out = 0;         // cOut
	double decay = 1;       // DECAY (not in the reference: 1 there)
	double tiltHand = 0;    // AGE extras: added to hand (Rack side)
	double fatigue = 0;     // AGE extras: constant bias force on the bend, units of we^2
};

struct Engine {
	double fe = 96000, T = 1.0 / 96000;
	Controls c;
	Rng rng;
	uint64_t rngAfterBuild = 0;

	// geometry and fixed random data
	double xg[G], xi[G], aL[G], aR[G], dent[G];
	double phiMG[M][G];   // phi[m][g]
	double phiGM[G][M];   // same, transposed
	float phiMGf[M][G], phiGMf[G][M];   // float copies
	// Half-grid tables: phi[m][G-1-g] = (-1)^m phi[m][g], so each transform runs on 24 points and splits
	// into even-m and odd-m halves (half the multiplies).
	float phE[M / 2][G / 2], phO[M / 2][G / 2];     // m = 2i and m = 2i+1, g < G/2
	float phEt[G / 2][M / 2], phOt[G / 2][M / 2];
	double det[N], mu[N], mu2[N];
	// per-block coefficients
	double f0[N], w[N], al[N], a1[N], a2[N], b[N], Gw[N];
	double Kg[G], dg[G], wet[G], tap[NF][G];
	double c0 = 1, s0 = 0, Ybow = 0, kHand = 0, kHP = 0;
	double shG[G];
	int gb = G / 2;
	// state
	double q[N], qp[N];
	double u[NF][G], upv[NF][G], fg[NF][G];
	double bend = 0, bendv = 0, benda = 0, bowD = 0, handS = 0;
	double hpL = 0, hpR = 0;
	// per-sample inputs
	double strikeV = 0, strikePos = 0.37; bool strikePending = false;
	double inForce = 0, inPos = 0.37;
	// outputs and diagnostics
	double outL = 0, outR = 0, energy = 0;
	long guards = 0, limits = 0, clamps = 0;
	int snapEvent = 0;     // +1 / -1 on the sample the bend changes sign (with |bendv| > 2), else 0
	bool idle = false;

	void setRate(double rate) { fe = rate; T = 1.0 / rate; }

	// geometry and random data, in the reference's draw order
	void build() {
		rng = Rng();
		for (int g = 0; g < G; g++) { xg[g] = (g + 0.5) / G; xi[g] = 2 * xg[g] - 1; dent[g] = 0; }
		for (int g = 0; g < G; g++) {
			double r = rng.b(); aL[g] = std::cos(PI * xg[g] / 2) * (0.6 + 0.4 * r) * 0.5;
			r = rng.b(); aR[g] = std::sin(PI * xg[g] / 2) * (0.6 + 0.4 * r) * 0.5;
		}
		for (int k = 0; k < 5; k++) {
			double cc = rng.u(), s = 0.03 + 0.05 * rng.u(), a = rng.b();
			for (int g = 0; g < G; g++) dent[g] += a * std::exp(-std::pow((xg[g] - cc) / s, 2));
		}
		for (int m = 0; m < M; m++) for (int g = 0; g < G; g++) {
			phiMG[m][g] = std::sqrt(2.0 / G) * std::sin((m + 1) * PI * xg[g]);
			phiGM[g][m] = phiMG[m][g];
			phiMGf[m][g] = phiGMf[g][m] = (float)phiMG[m][g];
			if (g < G / 2) { if (m % 2 == 0) phE[m / 2][g] = phEt[g][m / 2] = (float)phiMG[m][g]; else phO[m / 2][g] = phOt[g][m / 2] = (float)phiMG[m][g]; }
		}
		for (int j = 0; j < N; j++) {
			det[j] = rng.b(); mu2[j] = rng.b();
			int n = j / M, m = j % M;
			mu[j] = n == 0 ? ((m % 2 == 0) ? 1.0 / (m + 1) : 0.15 * rng.b() / (m + 1)) : 0.25 * rng.b() / (m + 1);
		}
		rngAfterBuild = rng.s;
		reset();
	}

	void reset() {
		std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp);
		std::memset(u, 0, sizeof u); std::memset(upv, 0, sizeof upv); std::memset(fg, 0, sizeof fg);
		bend = bendv = benda = bowD = handS = 0; hpL = hpR = 0;
		strikePending = false; inForce = 0;
		outL = outR = energy = 0; guards = limits = clamps = 0; snapEvent = 0; idle = false;
	}

	void strike(double v, double pos) { strikeV = v; strikePos = pos; strikePending = true; idle = false; }

	// reference coefs(): once per BLK engine samples, after the controls are set
	void block() {
		const double FS = fe;
		double a = std::pow(PI / (4.730 * c.aspect), 2);
		for (int n = 0; n < NF; n++) for (int m = 0; m < M; m++) {
			int j = n * M + m; double mm = m + 1;
			double r = n == 1 ? a * mm * mm + 0.5 * std::sqrt(a) * mm : KAP2[n] + a * mm * mm;
			double f = c.f1 * r * (1 + det[j] * (0.003 + 0.02 * c.age));
			if (f > 0.42 * FS) f = 0.42 * FS;
			f0[j] = f; w[j] = 2 * PI * f;
			double t60 = c.decay * 20.0 / (1 + f / 500.0) / (1 + 3 * c.age);
			al[j] = 6.908 / t60;
			a1[j] = 2 * std::exp(-al[j] * T) * std::cos(w[j] * T);
			a2[j] = std::exp(-2 * al[j] * T);
			b[j] = std::exp(-al[j] * T) * T * std::sin(w[j] * T) / w[j];
			double kt = std::sqrt(r) + 0.05, be = 1.2 * std::fabs(c.depth) / kt;
			Gw[j] = std::sqrt(be / (1 + be));
			if (c.depth < 0) {
				double x = w[j] * T, lim = 0.6 * (1 + std::cos(x)) / (x * std::sin(x) + 1e-9);
				if (Gw[j] * Gw[j] > lim) Gw[j] = std::sqrt(lim);
			}
		}
		c0 = std::cos(c.shape * PI / 2); s0 = 1.6 * std::sin(c.shape * PI / 2);
		for (int g = 0; g < G; g++) shG[g] = c0 + s0 * (xi[g] - c.spot);
		kHand = 1 - std::exp(-T / 0.002); kHP = 2 * PI * 60 * T;
		double w1 = 2 * PI * c.f1;
		double km = 0.18 * FS / (c.f1 * c.wb);
		for (int g = 0; g < G; g++) {
			double x = xg[g];
			Kg[g] = bend * (c0 + s0 * (xi[g] - c.spot)) + c.age * 1.2 * dent[g];
			if (Kg[g] > km) Kg[g] = km;
			if (Kg[g] < -km) Kg[g] = -km;
			double e0 = std::max(0.0, 1 - x / 0.22), e1 = std::max(0.0, 1 - (1 - x) / 0.16);
			dg[g] = c.grip * c.grip * 900 * (e0 * e0 + e1 * e1);
			double z = (c.dip - x) / 0.05; z = z < 0 ? 0 : (z > 1 ? 1 : z);
			wet[g] = z * z * (3 - 2 * z); dg[g] += wet[g] * 3;
			double wd = std::exp(-1.1 * c.taper * (x - 0.5));
			for (int n = 0; n < NF; n++) {
				tap[n][g] = w1 * w1 * KAP2[n] * KAP2[n] * (1 / (wd * wd * wd * wd) - 1);
				if (tap[n][g] > 0.5 * FS * FS) tap[n][g] = 0.5 * FS * FS;
			}
		}
		gb = (int)(c.bowX * G); if (gb < 1) gb = 1; if (gb > G - 2) gb = G - 2;
		Ybow = 0;
		for (int n = 0; n < NF; n++) for (int m = 0; m < M; m++) Ybow += EB[n] * EB[n] * b[n * M + m] * phiMG[m][gb] * phiMG[m][gb];
		Ybow *= FS;
		if (c.bowP > 0 || c.shake != 0 || std::fabs(c.hand + c.tiltHand - handS) > 1e-6) idle = false;
	}

	// Diagnostic (tests only): mode energy plus the potential of the stretching and the taper at the current
	// state. This is the quantity the stretching law conserves when the shape is held still.
	double fullEnergy() const {
		double E = 0;
		for (int j = 0; j < N; j++) { double v = (q[j] - qp[j]) * fe; E += 0.5 * v * v + 0.5 * w[j] * w[j] * q[j] * q[j]; }
		double wB = 2 * PI * c.f1 * c.wb, eta = 6e5 * c.nl;
		for (int g = 0; g < G; g++) {
			double uu[NF], Q = 0;
			for (int n = 0; n < NF; n++) { double s = 0; for (int m = 0; m < M; m++) s += phiMG[m][g] * q[n * M + m]; uu[n] = s; Q += LN[n] * s * s; }
			for (int n = 0; n < NF; n++) { double S = wB * GAM[n] * Kg[g] * uu[n] + 0.5 * eta * Q; E += 0.5 * S * S + 0.5 * tap[n][g] * uu[n] * uu[n]; }
		}
		return E;
	}

	static double frc(double d, double* dd) {
		const double ve = 0.02, vs = 0.25, mus = 0.9, mud = 0.25;
		double th = std::tanh(d / ve), ex = std::exp(-std::fabs(d) / vs);
		double m = mud + (mus - mud) * ex;
		*dd = (1 - th * th) / ve * m + th * (-(mus - mud) * ex / vs) * (d >= 0 ? 1 : -1);
		return th * m;
	}

	void process() {
		const double FS = fe;
		snapEvent = 0;
		if (idle && inForce == 0 && !strikePending) { outL = outR = 0; hpL = hpR = 0; return; }
		double w1 = 2 * PI * c.f1, wB = w1 * c.wb, eta = 6e5 * c.nl;
		// forward transform (float, half grid, even/odd split)
		for (int n = 0; n < NF; n++) {
			const int H = G / 2;
			float e[H], o[H];
			for (int g = 0; g < H; g++) { e[g] = 0.f; o[g] = 0.f; }
			const double* qq = q + n * M;
			for (int i = 0; i < M / 2; i++) {
				const float qe = (float)qq[2 * i], qo = (float)qq[2 * i + 1];
				const float* pe = phE[i]; const float* po = phO[i];
				for (int g = 0; g < H; g++) { e[g] += qe * pe[g]; o[g] += qo * po[g]; }
			}
			for (int g = 0; g < H; g++) {
				upv[n][g] = u[n][g]; u[n][g] = e[g] + o[g];
				upv[n][G - 1 - g] = u[n][G - 1 - g]; u[n][G - 1 - g] = e[g] - o[g];
			}
		}
		// stretch and forces on the grid, vectorised over the grid points
		double dVdb = 0, kb = 0;
		{
			double Q[G], GU[G], Ss[G], sc[G], wBK[G], vv[NF][G];
			for (int g = 0; g < G; g++) { Q[g] = 0; GU[g] = 0; wBK[g] = wB * Kg[g]; }
			for (int n = 0; n < NF; n++) for (int g = 0; g < G; g++) {
				double x = u[n][g]; Q[g] += LN[n] * x * x; GU[g] += GAM[n] * x; vv[n][g] = (x - upv[n][g]) * FS;
			}
			int lim = 0;
			for (int g = 0; g < G; g++) {
				Ss[g] = wBK[g] * GU[g] + 0.5 * NF * eta * Q[g];
				double kloc = wBK[g] * wBK[g] + eta * 4.5 * std::fabs(Ss[g]) + eta * eta * 4.5 * Q[g] * NF;
				double k2 = kloc * T * T;
				sc[g] = k2 > 1.0 ? 1.0 / k2 : 1.0;
				lim += k2 > 1.0 ? 1 : 0;
			}
			limits += lim;
			double dacc[G], kacc[G];
			for (int g = 0; g < G; g++) { dacc[g] = 0; kacc[g] = 0; }
			for (int n = 0; n < NF; n++) {
				const double gm = GAM[n], ln = LN[n];
				for (int g = 0; g < G; g++) {
					double x = u[n][g];
					double Sn = wBK[g] * gm * x + 0.5 * eta * Q[g];
					fg[n][g] = -tap[n][g] * x - sc[g] * (wBK[g] * gm * Sn + eta * ln * x * Ss[g]) - dg[g] * vv[n][g];
					dacc[g] += gm * Sn * x;
					kacc[g] += gm * gm * x * x;
				}
			}
			for (int g = 0; g < G; g++) { dVdb += sc[g] * dacc[g] * wB * shG[g]; kb += kacc[g] * shG[g] * shG[g]; }
			kb *= wB * wB;
		}
		if (strikePending) {
			int gs = (int)(strikePos * G); gs = std::min(std::max(gs, 0), G - 1);
			for (int n = 0; n < NF; n++) for (int m = 0; m < M; m++) {
				int j = n * M + m;
				qp[j] -= strikeV * phiMG[m][gs] * T * std::exp(-f0[j] / (400 + 6000 * c.hard * c.hard)) * (n == 1 ? 0.5 : 1);
			}
			strikePending = false;
		}
		if (inForce != 0) {
			int gs = (int)(inPos * G); gs = std::min(std::max(gs, 0), G - 1);
			for (int n = 0; n < NF; n++) fg[n][gs] += inForce * (n < 2 ? 0.3 : 1);
		}
		// back transform (float, half grid, even/odd split) + inertial kick from the bend's own acceleration
		double fm[N];
		for (int n = 0; n < NF; n++) {
			const int H = G / 2, HM = M / 2;
			float e[HM], o[HM];
			for (int i = 0; i < HM; i++) { e[i] = 0.f; o[i] = 0.f; }
			for (int g = 0; g < H; g++) {
				const float fs = (float)(fg[n][g] + fg[n][G - 1 - g]), fd = (float)(fg[n][g] - fg[n][G - 1 - g]);
				const float* pe = phEt[g]; const float* po = phOt[g];
				for (int i = 0; i < HM; i++) { e[i] += fs * pe[i]; o[i] += fd * po[i]; }
			}
			for (int i = 0; i < HM; i++) {
				int j = n * M + 2 * i; fm[j] = e[i] - c.kick * 0.05 * mu[j] * benda;
				j++; fm[j] = o[i] - c.kick * 0.05 * mu[j] * benda;
			}
		}
		// water: added mass as a projection of the total force on the wet part
		if (c.dip > 0.001) {
			double ft[N];
			for (int j = 0; j < N; j++) ft[j] = Gw[j] * (fm[j] - w[j] * w[j] * q[j] - 2 * al[j] * (q[j] - qp[j]) * FS);
			const int H = G / 2, HM = M / 2;
			double sg = c.depth < 0 ? -1 : 1;
			for (int n = 0; n < NF; n++) {
				float e[H], o[H];
				for (int g = 0; g < H; g++) { e[g] = 0.f; o[g] = 0.f; }
				for (int i = 0; i < HM; i++) {
					const float fe_ = (float)ft[n * M + 2 * i], fo_ = (float)ft[n * M + 2 * i + 1];
					const float* pe = phE[i]; const float* po = phO[i];
					for (int g = 0; g < H; g++) { e[g] += fe_ * pe[g]; o[g] += fo_ * po[g]; }
				}
				float ps[H], pd[H];
				for (int g = 0; g < H; g++) {
					float lo = (e[g] + o[g]) * (float)wet[g], hi = (e[g] - o[g]) * (float)wet[G - 1 - g];
					ps[g] = lo + hi; pd[g] = lo - hi;
				}
				float ae[HM], ao[HM];
				for (int i = 0; i < HM; i++) { ae[i] = 0.f; ao[i] = 0.f; }
				for (int g = 0; g < H; g++) {
					const float* pe = phEt[g]; const float* po = phOt[g]; const float a = ps[g], b_ = pd[g];
					for (int i = 0; i < HM; i++) { ae[i] += a * pe[i]; ao[i] += b_ * po[i]; }
				}
				for (int i = 0; i < HM; i++) {
					int j = n * M + 2 * i; fm[j] -= sg * Gw[j] * ae[i];
					j++; fm[j] -= sg * Gw[j] * ao[i];
				}
			}
		}
		// modal update
		double qn[N], E = 0;
		for (int j = 0; j < N; j++) qn[j] = a1[j] * q[j] - a2[j] * qp[j] + b[j] * fm[j];
		if (c.bowP > 0) {
			double vfree = 0;
			for (int n = 2; n < NF; n++) for (int m = 0; m < M; m++) { int j = n * M + m; vfree += EB[n] * phiMG[m][gb] * (qn[j] - q[j]); }
			vfree *= FS;
			double Fmax = c.bowP * c.bowP * 2000, YP = Ybow * Fmax, rhs = c.bowV - vfree, d = bowD, dd;
			for (int it = 0; it < 6; it++) {
				double fr = frc(d, &dd); double gq = d + YP * fr - rhs, gp = 1 + YP * dd;
				if (std::fabs(gp) < 0.2) gp = gp < 0 ? -0.2 : 0.2;
				double st = gq / gp; if (st > 0.5) st = 0.5; if (st < -0.5) st = -0.5; d -= st;
				if (std::fabs(st) < 1e-12) break;
			}
			bowD = d; double Fb = Fmax * frc(d, &dd);
			for (int n = 2; n < NF; n++) for (int m = 0; m < M; m++) { int j = n * M + m; qn[j] += EB[n] * b[j] * phiMG[m][gb] * Fb; }
		}
		for (int j = 0; j < N; j++) {
			double v = (qn[j] - q[j]) * FS; E += 0.5 * v * v + 0.5 * w[j] * w[j] * qn[j] * qn[j];
			qp[j] = q[j]; q[j] = qn[j];
		}
		if (E > 5e6 && E < 1e12) { double k = std::sqrt(5e6 / E); clamps++; for (int j = 0; j < N; j++) { q[j] *= k; qp[j] *= k; } }
		if (!(E < 1e12)) {
			guards++;
			if (!std::isfinite(E)) { std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp); std::memset(u, 0, sizeof u); std::memset(upv, 0, sizeof upv); bend = bendv = benda = 0; }
			else for (int j = 0; j < N; j++) { q[j] *= 0.5; qp[j] *= 0.5; }
		}
		energy = E;
		// outputs: area pickups
		double L = 0, R = 0;
		for (int n = 0; n < NF; n++) {
			double o = n == 0 ? 0.5 : 1, sl = 0, sr = 0;
			for (int g = 0; g < G; g++) { double v = u[n][g] - upv[n][g]; sl += aL[g] * v; sr += aR[g] * v; }
			L += o * sl * FS; R += o * ((n & 1) ? -sr : sr) * FS;
		}
		{
			double uLs = 0, uRs = 0;
			for (int n = 2; n < NF; n++) for (int g = 0; g < G; g++) { uLs += aL[g] * u[n][g]; uRs += aR[g] * u[n][g]; }
			double gl = 1 + 0.85 * c.pick * std::tanh(uLs * 2500), gr = 1 + 0.85 * c.pick * std::tanh(uRs * 2500);
			L /= gl * gl; R /= gr * gr;
			double g = 1 + 14 * c.out * c.out, r = 1.5;
			L = r * std::tanh(g * L / r) / std::sqrt(g); R = r * std::tanh(g * R / r) / std::sqrt(g);
		}
		hpL += (L - hpL) * kHP; hpR += (R - hpR) * kHP;
		outL = L - hpL; outR = R - hpR;
		// the bend: a slow mass held by the hand, optional double well
		{
			double we = 2 * PI * 7.0 * c.weMul, wh = 2 * PI * (1.5 + 60 * c.hold * c.hold), z = 0.12 + 0.6 * c.hold;
			handS += (c.hand + c.tiltHand - handS) * kHand;
			double acc = -wh * wh * (bend - handS) - we * we * c.snap * (-2.2 * bend + bend * bend * bend / 2.25)
				- 2 * z * std::sqrt(wh * wh + we * we) * bendv - c.back * dVdb + c.shake + c.fatigue * we * we;
			double kt = wh * wh + c.back * kb + we * we * c.snap * 3 * bend * bend / 2.25, ct = 2 * z * std::sqrt(wh * wh + we * we);
			double b0 = bend, dv = T * acc / (1 + T * ct + T * T * kt);
			benda = dv * FS; if (benda > 3e5) benda = 3e5; if (benda < -3e5) benda = -3e5;
			bendv += dv; bend += bendv * T;
			if (bend > 30) { bend = 30; if (bendv > 0) bendv = 0; }
			if (bend < -30) { bend = -30; if (bendv < 0) bendv = 0; }
			if ((b0 < 0) != (bend < 0)) {
				double A = 0.08 * c.crack * std::fabs(bendv);
				for (int j = 0; j < N; j++) qp[j] -= A * mu2[j] * T * std::exp(-f0[j] / (700 + 3000 * c.hard)) * (bendv > 0 ? 1 : -1);
				if (std::fabs(bendv) > 2) snapEvent = bendv > 0 ? 1 : -1;
			}
		}
		// idle detection: nothing moving and nothing exciting it
		if (E < 1e-14 && c.bowP <= 0 && inForce == 0 && std::fabs(bendv) < 1e-6 && std::fabs(c.hand + c.tiltHand - handS) < 1e-6 && c.shake == 0
			&& std::fabs(outL) < 1e-9 && std::fabs(outR) < 1e-9) idle = true;
	}
};

// 2x decimator (31-tap windowed-sinc half-band), as in Plate Driver.
struct Decimator {
	static const int NT = 31;
	double h[NT];
	double buf[2][64] = {};
	int pos = 0;
	Decimator() {
		const int Mx = NT - 1; double sum = 0;
		for (int i = 0; i < NT; i++) {
			double n = i - Mx / 2.0;
			double s = n == 0 ? 0.5 : std::sin(PI * 0.5 * n) / (PI * n);
			double wv = 0.42 - 0.5 * std::cos(2 * PI * i / Mx) + 0.08 * std::cos(4 * PI * i / Mx);
			h[i] = s * wv; sum += h[i];
		}
		for (int i = 0; i < NT; i++) h[i] /= sum;
	}
	void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
	void process(const double* l2, const double* r2, double& L, double& R) {
		for (int k = 0; k < 2; k++) { buf[0][pos] = buf[0][pos + 32] = l2[k]; buf[1][pos] = buf[1][pos + 32] = r2[k]; pos = (pos + 1) & 31; }
		double sl = 0, sr = 0;
		for (int i = 0; i < NT; i++) { int idx = pos + 32 - 1 - i; sl += h[i] * buf[0][idx]; sr += h[i] * buf[1][idx]; }
		L = sl; R = sr;
	}
};

// Slow 2:1 level rider: the same law post.py applied to the sketches.
struct Leveller {
	double ms = 0, k = 0, ref = 1.0;
	bool on = true;
	void setRate(double rate) { k = 1 - std::exp(-1.0 / (0.075 * rate)); }
	void reset() { ms = 0; }
	double gain(double L, double R) {
		ms += ((L * L + R * R) * 0.5 - ms) * k;
		if (!on) return 1.0;
		return std::pow(std::max(std::sqrt(ms), 0.01 * ref) / ref, -0.5);
	}
};

} // namespace bent
