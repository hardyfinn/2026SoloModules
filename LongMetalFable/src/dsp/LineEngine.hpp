// Long Metal engine: a dispersive, periodically supported, bidirectional waveguide.
// Port of tools/proto/longmetal_ref.c (the definition of the sound). No Rack code here.
// Spec: docs/BUILD-INSTRUCTIONS.md section 3.
//
// Layout is structure-of-arrays in double so the per-cell work vectorises across cells.
// Field and function names follow the reference so the offline tests can run its scene scripts.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace longmetal {

// Double throughout: in real every cell's fixed (a, b) and (cos, sin) pairs miss a^2 + b^2 = 1 by one rounding,
// always in the same direction, and a lossless line drifted 1 to 2% in 10 s. Double keeps it below 1e-9.
typedef double real;

static const int NMAX = 320;   // cells
static const int MMAX = 4;     // allpasses per cell per direction
static const int DMAX = 64;    // delay buffer length (power of two)
static const int NAX = 8;      // axles
static const double SRNOM = 48000.0;   // the engine is defined at this rate; PITCH re-clocks it
static const double PI = 3.14159265358979323846;

struct Params {
	int N = 256, M = 4;
	int d = 2;
	double a0 = -0.9;
	double ties = 0;
	double rhoL = -0.95, rhoR = -0.95;
	double g = 0.9995;
	double lowcut = 0, lowf = 500;
	double hidamp = 0.002, hif = 5000;
	double warp = 0, swell = 0, loose = 0;
	double grip = 0, snap = 12, gstuck = 0.9;
	double age = 0;
	double pick = 20;
	double sat = 0.6;
	// AGE extras (module only; 0 in the reference scenes)
	double dead = 0;    // dead and over-tight sleepers: rndT > 1.2 scales the support by 1 - dead, < -1.2 by 1 + dead
	double worn = 0;    // worn wheels: wider spread of wheel-flat sizes
};

struct Axle { double off, lp, hp, amp, ampT, flat, wph, lastpos; };

struct Engine {
	Params p;
	double fe = SRNOM;   // real engine rate: only real-time quantities (train, strike length) use it

	// line state (real, SoA)
	alignas(32) real dR[DMAX][NMAX], dL[DMAX][NMAX];
	alignas(32) real sR[MMAX][NMAX], sL[MMAX][NMAX];
	alignas(32) real llR[NMAX], llL[NMAX], lhR[NMAX], lhL[NMAX];
	alignas(32) real xr[NMAX], xl[NMAX];          // per-sample working values (rOut, lOut after the cell)
	alignas(32) real rIn[NMAX], lIn[NMAX];
	alignas(32) real inj[NMAX];
	alignas(32) real envc[NMAX], gmul[NMAX];
	alignas(32) real jc[NMAX], js[NMAX];          // junction rotation
	alignas(32) real rndDf[NMAX], rndAf[NMAX], rndTf[NMAX], rndGf[NMAX];
	alignas(32) real thBase[NMAX];
	int dIdx[NMAX];
	double rndD[NMAX], rndA[NMAX], rndT[NMAX], rndG[NMAX];
	// GRIP
	double es[NMAX], gl[NMAX], bz[NMAX];
	int stuck[NMAX];
	int wp = 0;
	long guard = 0, nsnap = 0;
	bool snapEvent = false;   // a cell let go this sample
	bool injected = false;    // anything injected this sample
	int idle = 0;             // samples with nothing in the line
	real wE = 0;             // energy written this sample

	// caches
	double cTies = -1, cAge = -1, cDead = -1; int cN = -1;
	double cDage = -1; int cD = -1;
	double cLowf = -1, cHif = -1; real kl = 0, kh = 0;

	// rng (xorshift64, same as the reference)
	uint64_t rs = 88172645463325252ULL;
	double urand() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (rs >> 11) * (1.0 / 9007199254740992.0); }
	double nrand() { return urand() + urand() + urand() + urand() - 2.0; }

	// train
	Axle ax[NAX];
	int nax = 0;
	double trainPos = 1e9, trainSpeed = 0, trainSide = 1;
	double roughLevel = 0, flatLevel = 0, jointLevel = 0, roughBright = 0.35, wheelHz = 5.5, roughFloor = 0.15, roughDens = 120;
	int joints[3] = {0, 0, 0};
	int njoint = 0;
	int trainCnt = 0;
	bool clackEvent = false;   // joint clack or a lead-axle wheel flat this sample

	// strike
	double stPos = 0, stAmp = 0; int stN = 0, stLen = 1;

	void line_init() {
		std::memset(dR, 0, sizeof dR); std::memset(dL, 0, sizeof dL); std::memset(sR, 0, sizeof sR); std::memset(sL, 0, sizeof sL);
		std::memset(llR, 0, sizeof llR); std::memset(llL, 0, sizeof llL); std::memset(lhR, 0, sizeof lhR); std::memset(lhL, 0, sizeof lhL);
		std::memset(xr, 0, sizeof xr); std::memset(xl, 0, sizeof xl); std::memset(envc, 0, sizeof envc); std::memset(inj, 0, sizeof inj);
		for (int i = 0; i < NMAX; i++) { rndD[i] = nrand(); rndA[i] = nrand(); rndT[i] = nrand(); rndG[i] = nrand(); }
		for (int i = 0; i < NMAX; i++) { rndDf[i] = (real)rndD[i]; rndAf[i] = (real)rndA[i]; rndTf[i] = (real)rndT[i]; rndGf[i] = (real)std::fabs(rndG[i]); }
		wp = 0;
		for (int i = 0; i < NMAX; i++) { stuck[i] = 1; gl[i] = -1; bz[i] = 0; es[i] = 0; gmul[i] = 1; }
		cTies = cAge = cDead = -1; cN = -1; cD = -1; cDage = -1; cLowf = cHif = -1;
		idle = 0;
	}
	void grip_reset() { for (int i = 0; i < NMAX; i++) { stuck[i] = 1; gl[i] = -1; es[i] = 0; } }
	// clear every wave without touching the random tables (used by the silence detector)
	void clearWaves() {
		std::memset(dR, 0, sizeof dR); std::memset(dL, 0, sizeof dL); std::memset(sR, 0, sizeof sR); std::memset(sL, 0, sizeof sL);
		std::memset(llR, 0, sizeof llR); std::memset(llL, 0, sizeof llL); std::memset(lhR, 0, sizeof lhR); std::memset(lhL, 0, sizeof lhL);
		std::memset(xr, 0, sizeof xr); std::memset(xl, 0, sizeof xl); std::memset(envc, 0, sizeof envc);
		for (int i = 0; i < NMAX; i++) bz[i] = 0;
	}

	inline void inject(double pos, double v) {
		if (v == 0) return;
		if (pos < 0 || pos > p.N - 1) return;
		int i = (int)pos; double f = pos - i; if (i >= p.N - 1) { i = p.N - 2; f = 1; }
		inj[i] += (real)(v * (1 - f)); inj[i + 1] += (real)(v * f);
		injected = true;
	}

	void refreshCaches() {
		if (p.ties != cTies || p.age != cAge || p.N != cN || p.dead != cDead) {
			cTies = p.ties; cAge = p.age; cN = p.N; cDead = p.dead;
			for (int i = 0; i < NMAX; i++) {
				double th = p.ties * (1 + 0.5 * p.age * rndT[i]);
				if (p.dead > 0) { if (rndT[i] > 1.2) th *= 1 - p.dead; else if (rndT[i] < -1.2) th *= 1 + p.dead; }
				thBase[i] = (real)th;
				jc[i] = (real)std::cos(th); js[i] = (real)std::sin(th);
			}
		}
		if (p.d != cD || p.age != cDage) {
			cD = p.d; cDage = p.age;
			for (int i = 0; i < NMAX; i++) {
				int d = (int)(p.d + p.age * 1.5 * rndD[i] + 0.5); if (d < 1) d = 1; if (d > DMAX - 1) d = DMAX - 1;
				dIdx[i] = d;
			}
		}
		if (p.lowf != cLowf || p.hif != cHif) {
			cLowf = p.lowf; cHif = p.hif;
			kl = (real)(1 - std::exp(-2 * PI * p.lowf / SRNOM));
			kh = (real)(1 - std::exp(-2 * PI * p.hif / SRNOM));
		}
	}

	// total wave energy (exact for a lossless line): every sample still in flight plus allpass states
	double total_energy() {
		refreshCaches();
		double E = 0;
		for (int i = 0; i < p.N; i++) {
			int d = dIdx[i];
			for (int k = 0; k <= d; k++) { int q = (wp - k) & (DMAX - 1); E += (double)dR[q][i] * dR[q][i] + (double)dL[q][i] * dL[q][i]; }
			for (int m = 0; m < p.M; m++) E += (double)sR[m][i] * sR[m][i] + (double)sL[m][i] * sL[m][i];
		}
		return E;
	}

	void line_step(double* oL, double* oR) {
		const int N = p.N, M = p.M;
		refreshCaches();
		snapEvent = false;
		// skip the cell loop when nothing is in the line
		if (!injected && p.g <= 1.0 && idle > 4 * DMAX) {
			wp = (wp + 1) & (DMAX - 1);
			for (int i = 0; i < N; i++) { dR[wp][i] = 0; dL[wp][i] = 0; }
			*oL = 0; *oR = 0;
			return;
		}
		const real age = (real)p.age, a0 = (real)p.a0, warp = (real)p.warp, swell = (real)p.swell;
		// 1. read, envelope, allpasses (vectorised over cells)
		for (int i = 0; i < N; i++) {
			int rp = (wp - dIdx[i]) & (DMAX - 1);
			xr[i] = dR[rp][i]; xl[i] = dL[rp][i];
		}
		const bool pushed = (p.warp != 0 || p.swell != 0);
		real aR[NMAX], aL[NMAX], bR[NMAX], bL[NMAX];
		for (int i = 0; i < N; i++) {
			real e = xr[i] * xr[i] + xl[i] * xl[i];
			eIn[i] = e;
			envc[i] += 0.002 * (e - envc[i]);
			real a = a0 * (1. + 0.08 * age * rndAf[i]);
			real ar = a, al = a;
			if (pushed) {
				real be = swell * envc[i] / (1. + envc[i]);
				ar = a + be + warp * xr[i] / (1. + std::fabs(xr[i]));
				al = a + be + warp * xl[i] / (1. + std::fabs(xl[i]));
			}
			ar = std::min(0.5, std::max(-0.985, ar)); al = std::min(0.5, std::max(-0.985, al));
			aR[i] = ar; aL[i] = al;
			bR[i] = std::sqrt(1. - ar * ar); bL[i] = std::sqrt(1. - al * al);
		}
		for (int m = 0; m < M; m++) {
			real* SR_ = sR[m]; real* SL_ = sL[m];
			for (int i = 0; i < N; i++) {
				real y = aR[i] * xr[i] + bR[i] * SR_[i]; SR_[i] = bR[i] * xr[i] - aR[i] * SR_[i]; xr[i] = y;
				real z = aL[i] * xl[i] + bL[i] * SL_[i]; SL_[i] = bL[i] * xl[i] - aL[i] * SL_[i]; xl[i] = z;
			}
		}
		// 2. losses
		const real lc = (real)p.lowcut, hd = (real)p.hidamp;
		if (lc != 0.) for (int i = 0; i < N; i++) {
			llR[i] += kl * (xr[i] - llR[i]); xr[i] -= lc * llR[i];
			llL[i] += kl * (xl[i] - llL[i]); xl[i] -= lc * llL[i];
		}
		if (hd != 0.) for (int i = 0; i < N; i++) {
			lhR[i] += kh * (xr[i] - lhR[i]); xr[i] += hd * (lhR[i] - xr[i]);
			lhL[i] += kh * (xl[i] - lhL[i]); xl[i] += hd * (lhL[i] - xl[i]);
		}
		// 3. GRIP (scalar, ascending cells: keeps the reference's random order)
		if (p.grip > 0) gripPass();
		// 4. gain, live ceiling
		const real g0 = (real)p.g;
		if (p.grip > 0) {
			for (int i = 0; i < N; i++) {
				real g = g0 * (1. - 0.004 * age * rndGf[i]) * gmul[i];
				xr[i] *= g; xl[i] *= g;
			}
		} else {
			for (int i = 0; i < N; i++) {
				real g = g0 * (1. - 0.004 * age * rndGf[i]);
				xr[i] *= g; xl[i] *= g;
			}
		}
		if (p.g > 1.0) {
			const real lim = (real)p.sat, il = 1. / lim;
			for (int i = 0; i < N; i++) { xr[i] = lim * fastTanh(xr[i] * il); xl[i] = lim * fastTanh(xl[i] * il); }
		}
		// 5. supports
		if (p.loose != 0) {
			const real lo = (real)p.loose;
			// th = base + delta, delta in [0, 0.8): rotate the cached (cos, sin) of the base by delta (series to the
			// 9th power, error below 1e-7), then renormalise so the junction stays an exact rotation. No branches.
			for (int i = 0; i < N - 1; i++) {
				real A = std::fabs(xr[i]) + std::fabs(xl[i + 1]);
				real dl = lo * A / (1.0 + A), d2 = dl * dl;
				real sd = dl * (1.0 - d2 / 6.0 * (1.0 - d2 / 20.0 * (1.0 - d2 / 42.0 * (1.0 - d2 / 72.0))));
				real cd = 1.0 - d2 / 2.0 * (1.0 - d2 / 12.0 * (1.0 - d2 / 30.0 * (1.0 - d2 / 56.0)));
				real c = jc[i] * cd - js[i] * sd, s = js[i] * cd + jc[i] * sd;
				real r = 1.0 / std::sqrt(c * c + s * s); s *= r; c *= r;
				rIn[i + 1] = c * xr[i] + s * xl[i + 1];
				lIn[i] = -s * xr[i] + c * xl[i + 1];
			}
		} else {
			for (int i = 0; i < N - 1; i++) {
				rIn[i + 1] = jc[i] * xr[i] + js[i] * xl[i + 1];
				lIn[i] = -js[i] * xr[i] + jc[i] * xl[i + 1];
			}
		}
		rIn[0] = (real)p.rhoL * xl[0]; lIn[N - 1] = (real)p.rhoR * xr[N - 1];
		// 6. write
		wp = (wp + 1) & (DMAX - 1);
		real* WR = dR[wp]; real* WL = dL[wp];
		real E = 0;
		for (int i = 0; i < N; i++) {
			real r = rIn[i] + 0.5 * inj[i], l = lIn[i] + 0.5 * inj[i];
			inj[i] = 0;
			WR[i] = r; WL[i] = l;
			E += r * r + l * l;
		}
		wE = E;
		if (!(E < 1e12)) {   // guard: zero anything not below 1e6 (or NaN)
			for (int i = 0; i < N; i++) {
				if (!(std::fabs(WR[i]) < 1e6)) { WR[i] = 0; guard++; }
				if (!(std::fabs(WL[i]) < 1e6)) { WL[i] = 0; guard++; }
			}
		}
		// silence detector
		if (!injected && p.g <= 1.0 && E < 1e-14) { if (++idle == 4 * DMAX) clearWaves(); }
		else idle = 0;
		injected = false;
		// 7. listener
		int k = (int)p.pick; double f = p.pick - k; if (k < 0) { k = 0; f = 0; } if (k >= N - 1) { k = N - 2; f = 1; }
		*oL = xr[k] * (1 - f) + xr[k + 1] * f; *oR = xl[k] * (1 - f) + xl[k + 1] * f;
	}

	alignas(32) real eIn[NMAX];   // each cell's input energy this sample (GRIP's slow envelope)
	void gripPass() {
		const int N = p.N;
		for (int i = 0; i < N; i++) {
			if (gl[i] < 0) gl[i] = p.gstuck;
			double thr = p.grip * (1 + 0.6 * p.age * rndT[i]); if (thr < 0.1 * p.grip) thr = 0.1 * p.grip;
			es[i] += 2e-5 * ((double)eIn[i] - es[i]);
			if (stuck[i] && envc[i] > thr) { stuck[i] = 0; es[i] = thr; bz[i] = p.snap * std::sqrt(p.grip); nsnap++; snapEvent = true; }
			else if (!stuck[i] && es[i] < thr * 0.005) stuck[i] = 1;
			gl[i] += 0.004 * ((stuck[i] ? p.gstuck : 1.0) - gl[i]);
			gmul[i] = (real)gl[i];
			if (bz[i] > 1e-9) { inj[i] += (real)(bz[i] * nrand()); bz[i] *= 0.996; injected = true; }
		}
	}

	static inline real fastTanh(real x) {
		// Pade 7/6, clamped: max error ~1e-4, monotone, |y| < 1
		if (x > 4.97) return 1.; if (x < -4.97) return -1.;
		real x2 = x * x;
		real y = x * (135135. + x2 * (17325. + x2 * (378. + x2))) / (135135. + x2 * (62370. + x2 * (3150. + x2 * 28.)));
		return std::max(-1., std::min(1., y));
	}

	// ---------------- train ----------------
	void train_init(int n) {
		nax = n; double o = 0;
		for (int k = 0; k < n; k++) {
			ax[k].off = o; o += (k % 2 == 0) ? 3.0 : 9.0 + (k % 4 == 1 ? 0 : 5.0); ax[k].lp = ax[k].hp = 0; ax[k].amp = 1; ax[k].ampT = 1;
			ax[k].flat = (k % 3 == 0) ? 1.0 : (k % 5 == 0 ? 0.5 : 0.0); ax[k].wph = urand(); ax[k].lastpos = 1e9;
		}
	}
	void train_step() {
		trainCnt++;
		clackEvent = false;
		for (int k = 0; k < nax; k++) {
			Axle* a = &ax[k];
			double pos = trainPos + trainSide * a->off;
			if ((trainCnt & 1023) == 0) a->ampT = 0.5 + urand();
			a->amp += 0.0005 * (a->ampT - a->amp);
			double n = roughFloor * nrand(); if (urand() < roughDens / fe) { double u = urand(); n += (urand() < 0.5 ? -1 : 1) * 12.0 / (0.15 + u * u * 3); }
			a->lp += roughBright * (n - a->lp); a->hp += 0.01 * (a->lp - a->hp);
			double v = roughLevel * a->amp * (a->lp - a->hp) * 0.5;
			a->wph += wheelHz / fe * (1 + 0.01 * k);
			if (a->wph >= 1) { a->wph -= 1; double u = urand(); v += flatLevel * a->flat * (0.6 + 0.8 * u + (p.worn > 0 ? 2.0 * p.worn * u * u * u * u : 0.0)) * 6.0; if (k == 0 && flatLevel > 0 && pos >= 0 && pos <= p.N - 1) clackEvent = true; }
			for (int j = 0; j < njoint; j++) {
				double jp = joints[j];
				if ((a->lastpos - jp) * (pos - jp) < 0 && std::fabs(a->lastpos) < 1e8) { v += jointLevel * 14.0 * (0.7 + 0.6 * urand()); if (jointLevel > 0) clackEvent = true; }
			}
			a->lastpos = pos;
			inject(pos, v);
		}
		trainPos += trainSpeed / fe;
	}

	// ---------------- strike ----------------
	void strike(double pos, double amp, double ms) { stPos = pos; stAmp = amp; stLen = (int)(ms * 0.001 * fe); if (stLen < 1) stLen = 1; stN = stLen; }
	void strike_step() {
		if (stN > 0) { double ph = (double)(stLen - stN + 0.5) / stLen; inject(stPos, stAmp * 0.5 * (1 - std::cos(2 * PI * ph)) / stLen * 8.0); stN--; }
	}
};

}  // namespace longmetal
