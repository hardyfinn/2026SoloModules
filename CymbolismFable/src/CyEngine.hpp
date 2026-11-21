// Cymbolism Fable: the cymbal engine (pure C++, no Rack).
//
// Runs one Model at 2x the host rate:
//   - every mode (core and upper) is an exact-frequency modal oscillator
//     (Stormer-Verlet / leapfrog with the exact cos(w k) linear update),
//   - the core plus the extended upper-mode pairs feel the shell's nonlinear
//     membrane force  -dV_nl/dq,  V_nl = sum_n (A_n.q) zeta_n + zeta_n^2 / 2,
//   - beaters and rivets touch the shell through Hertz contacts solved
//     implicitly per step (scalar Newton), so stiff sticks stay stable,
//   - the output is a Rayleigh-integral microphone, decimated to the host rate.
// SIZE is an exact uniform scale of the built model (no rebuild).
#pragma once
#include "CyModes.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

namespace cy {

struct Controls {
	double size = 1.0;       // uniform scale lambda relative to the type's reference size
	double decay = 1.0;      // T60 multiplier
	double bloom = 1.0;      // nonlinearity 0..2
	double damp = 0.0;       // tape 0..1
	double mount = 0.5;      // 0 tight .. 1 loose
	double hammer = 0.3;     // irregularity 0..1
	double mic = 1.0;        // 0 BELL, 1 BOW, 2 EDGE, 3 OVERHEAD, 4 ROOM
	double sizzle = 0.0;     // 0..1 -> 0..12 rivets
	double position = 0.9;   // strike point, fraction of radius (0 bell .. 1 edge)
	double hardness = 0.5;
	int play = SHOULDER;
	double excite = 0.0;     // external exciter gain 0..1
	double force = 3.0;      // m/s for EXCITE scaling and BOW
	double rate = 8.0;       // roll / sweep / bow speed
	bool choke = false;
	bool roll = false;
	uint64_t seed = 1;       // hammering pattern
};

// one Hertz contact between a point mass and the shell
struct Contact {
	bool active = false;
	double mass = 0.008;     // kg
	double K = 1e8, alpha = 1.5, lam = 0.2;
	double y = 0, yp = 0;    // height of the contact face (m), now and previous step
	double delta = 0;        // last penetration
	double r = 0.5, th = 0; // position on the cymbal (fraction of radius, angle)
	double gravity = 0;      // m/s^2 (rivets)
	bool expires = true;     // beaters fly off and expire, rivets stay
	int age = 0;
	std::vector<double> phi; // shape vector at the contact point (scaled)
	double C = 0;            // compliance sum (phi . g)
	double force = 0;
};

class Engine {
public:
	std::shared_ptr<const Model> M;
	double fsHost = 48000, fs = 96000, k = 1.0 / 96000;
	int OS = 2;
	int N = 0, NC = 0, NA = 0;        // modes, core modes, active modes (below the output band)

	// state
	std::vector<double> q, qp, f;     // displacement now, previous, external force
	std::vector<double> om0;          // reference angular frequency (with hammering)
	std::vector<double> c1, c2, g;    // update coefficients
	std::vector<double> sig;          // loss
	std::vector<double> grad;
	std::vector<double> y;            // B q scratch (core)
	std::vector<double> thOff;        // pair orientation offsets (hammering)
	std::vector<double> micW, micV;   // pickup weights on acceleration and velocity

	Controls ctl;
	double lam = 1.0;                 // current size scale
	double sA = 1.0, sB = 1.0, sPhi = 1.0;
	std::vector<Contact> beaters;     // up to 8
	std::vector<Contact> rivets;

	// output decimator
	std::vector<double> firH;
	std::vector<double> firBuf;
	int firPos = 0;
	double dcX = 0, dcY = 0;
	double outGain = 0.004;  // pickup units -> volts (medium crash hit ~2 V peak, hard ~7 V)
	double lp = 0, lpA = 1.0;         // ROOM air absorption
	double lastEnergy = 0;
	int guardEvents = 0;
	int ctlCounter = 0;
	double micCache = -1, sizeCache = -1;
	CyRng rng;

	// stick click (three damped stick modes driven by the contact force)
	double clk[3][2] = {};
	double clkC1[3], clkC2[3], clkG[3];

	void init(std::shared_ptr<const Model> model, double hostRate) {
		M = model;
		fsHost = hostRate;
		OS = M->gp.quality >= 2 ? 2 : 1;
		fs = fsHost * OS;
		k = 1.0 / fs;
		N = (int)M->modes.size();
		NC = M->nCore;
		q.assign(N, 0.0);
		qp.assign(N, 0.0);
		f.assign(N, 0.0);
		grad.assign(N, 0.0);
		y.assign(NC, 0.0);
		om0.resize(N);
		c1.resize(N);
		c2.resize(N);
		g.resize(N);
		sig.resize(N);
		thOff.assign(N, 0.0);
		micW.assign(N, 0.0);
		micV.assign(N, 0.0);
		// half-band decimation FIR (Kaiser-windowed sinc, 63 taps)
		int L = 63;
		firH.resize(L);
		double beta = 8.0;
		auto i0 = [](double x) { double s = 1, t = 1; for (int j = 1; j < 30; j++) { t *= (x / (2 * j)) * (x / (2 * j)); s += t; } return s; };
		double sum = 0;
		for (int i = 0; i < L; i++) {
			double n = i - (L - 1) / 2.0;
			double sinc = n == 0 ? 0.5 : std::sin(CY_PI * 0.5 * n) / (CY_PI * n);
			double wv = i0(beta * std::sqrt(1 - std::pow(2.0 * i / (L - 1) - 1, 2))) / i0(beta);
			firH[i] = sinc * wv;
			sum += firH[i];
		}
		for (double& h : firH) h /= sum;
		firBuf.assign(2 * L, 0.0);
		firPos = 0;
		beaters.assign(8, Contact());
		rng.seed(ctl.seed);
		applyHammer();
		tuneKey[5] = ctl.hammer;
		tuneKey[6] = (double)ctl.seed;
		updateCoefficients(true);
		// stick click modes (5B hickory: ~1.1, 2.9, 5.4 kHz)
		const double cf[3] = {1100, 2900, 5400}, ct[3] = {0.035, 0.025, 0.02};
		for (int i = 0; i < 3; i++) {
			double w = 2 * CY_PI * cf[i];
			double s = 6.91 / ct[i];
			double a = 1 + s * k;
			clkC1[i] = 2 * std::cos(w * k) / a;
			clkC2[i] = (1 - s * k) / a;
			clkG[i] = k * k / a;
		}
	}

	// HAMMER: pair splitting, frequency jitter, pair orientation (upper modes)
	void applyHammer() {
		CyRng r;
		r.seed(ctl.seed * 7919 + 17);
		double h = ctl.hammer;
		for (int i = 0; i < N; i++) {
			const Mode& md = M->modes[i];
			double jitter = (i < NC ? 0.4 : 1.0) * 0.03 * h * r.bi();
			double split = 0.0125 * h * (0.3 + 0.7 * r.uni());
			double fr = md.f * (1.0 + jitter);
			if (md.m > 0) fr *= (md.par ? 1.0 - split : 1.0 + split);
			om0[i] = 2 * CY_PI * fr;
			if (md.m > 0 && md.par == 0 && i >= NC) {
				double off = (0.15 + 0.85 * h) * r.bi() * CY_PI / std::max(1, md.m);
				thOff[i] = off;
				if (i + 1 < N) thOff[i + 1] = off;
			}
		}
		// pairs of the core: keep the twins aligned (the coupling tensors assume it)
	}

	// T60 law per type (calibrated: constant + viscoelastic linear term), x alloy and weight
	double lossAt(double fHz, int i) const {
		static const double A0[NUM_TYPES] = {2.5, 0.9, 1.1, 0.6, 0.5, 0.4, 0.8, 1.5, 1.3, 0.33};
		static const double B1[NUM_TYPES] = {9e-4, 4.8e-4, 5.0e-4, 4.0e-4, 3.3e-4, 2.8e-4, 4.0e-4, 7e-4, 6e-4, 2.4e-4};
		int t = M->gp.type;
		double etaRel = M->sh.mat.eta / CY_MATERIALS[B20].eta;
		double wRel = std::pow(std::max(0.3, M->gp.weight), -0.5);
		double a = A0[t] * wRel * std::pow(lam, -0.5);
		double b = B1[t] * (0.5 + 0.5 * etaRel) * wRel;
		(void)i;
		return a + b * fHz;
	}

	double phiAt(int i, double rFrac, double th) const {
		const Shape& sh = M->sh;
		double r = sh.r0 + rFrac * (sh.a - sh.r0);
		return sPhi * M->phi(i, r, th, thOff[i]);
	}

	// cached tuning (recomputed when size, mount, hammer, decay, damp or choke change)
	std::vector<double> wS, cw, gw, sigBase, sigX, nz;
	std::vector<int> band;               // cascade band of each upper mode (-1 none); top-core modes = -2
	int nBands = 0;
	std::vector<double> bandF, bandRef;  // centre frequency, reference energy per mode
	double tuneKey[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

	void applyLoss(int i) {
		double s = sigBase[i] + sigX[i];
		double a = 1 + s * k;
		c1[i] = 2 * cw[i] / a;
		c2[i] = (1 - s * k) / a;
		g[i] = gw[i] / a;
		sig[i] = s;
	}

	void updateCoefficients(bool force = false) {
		double key[8] = {ctl.size, ctl.mount, ctl.decay, ctl.damp, ctl.choke ? 1.0 : 0.0, ctl.hammer, (double)ctl.seed, 0};
		bool changed = force;
		for (int j = 0; j < 7; j++) if (key[j] != tuneKey[j]) changed = true;
		if ((int)wS.size() != N) {
			wS.assign(N, 0); cw.assign(N, 0); gw.assign(N, 0); sigBase.assign(N, 0); sigX.assign(N, 0); nz.assign(N, 0); band.assign(N, -1);
			changed = true;
		}
		if (changed) {
			if (key[5] != tuneKey[5] || key[6] != tuneKey[6]) applyHammer();
			for (int j = 0; j < 7; j++) tuneKey[j] = key[j];
			retune();
		}
		if (force || std::fabs(ctl.mic - micCache) > 1e-3 || std::fabs(lam - sizeCache) > 1e-4) {
			computeMic();
			micCache = ctl.mic;
			sizeCache = lam;
		}
		lpA = 1.0;
		if (ctl.mic > 3.0) {
			double fc = 14000 - 5000 * std::min(1.0, ctl.mic - 3.0);
			lpA = 1 - std::exp(-2 * CY_PI * fc / fsHost);
		}
	}

	void retune() {
		lam = std::max(0.3, ctl.size);
		sA = 1.0 / lam;
		sB = std::pow(lam, -3.5);
		sPhi = std::pow(lam, -1.5);
		double fTop = std::min(20000.0, 0.45 * fs);
		NA = 0;
		const Shape& sh = M->sh;
		double mnt = std::min(1.0, std::max(0.0, ctl.mount));
		double kLine = 4e6 * std::pow(0.004, mnt), kRot = 40.0 * std::pow(0.004, mnt);
		double cLine = 300.0 * std::pow(0.01, mnt);
		double r0 = sh.r0 * lam;
		double chokeS = ctl.choke ? 1.0 : 0.0;
		for (int i = 0; i < N; i++) {
			const Mode& md = M->modes[i];
			const RadialShape& rs = M->shapes[md.shape];
			double Th = md.m == 0 ? 2 * CY_PI : CY_PI;
			double w0 = om0[i] / lam;
			double Wc = rs.centreW * sPhi, Wr = rs.centreWr * sPhi / lam;
			double dw2 = Th * r0 * (kLine * Wc * Wc + kRot * Wr * Wr);
			double w = std::sqrt(w0 * w0 + dw2);
			double fHz = w / (2 * CY_PI);
			if (fHz < fTop) NA = i + 1;
			double s = lossAt(fHz, i) / std::max(0.1, ctl.decay);
			s += 0.5 * cLine * Th * r0 * Wc * Wc;
			if (ctl.damp > 0) {
				double wd = M->W(i, sh.r0 + 0.85 * (sh.a - sh.r0)) * sPhi;
				s += ctl.damp * 18.0 * (wd * wd) / std::max(1e-12, meanEdgeSq());
			}
			if (chokeS > 0) s += 70.0 * rs.edgeE / std::max(1e-12, meanEdgeSq0());
			wS[i] = w;
			sigBase[i] = s;
			cw[i] = std::cos(w * k);
			gw[i] = 2 * (1 - cw[i]) / (w * w);
			applyLoss(i);
		}
		// cascade bands (1/3 octave) above the core
		double fTopCore = M->fTopCore / lam;
		nBands = (int)std::ceil(3.0 * std::log2(fTop / fTopCore)) + 1;
		if (nBands < 1) nBands = 1;
		bandF.assign(nBands, 0);
		for (int b = 0; b < nBands; b++) bandF[b] = fTopCore * std::pow(2.0, (b + 0.5) / 3.0);
		for (int i = 0; i < N; i++) {
			band[i] = -1;
			double fHz = wS[i] / (2 * CY_PI);
			if (i < NC) { if (fHz > 0.7 * fTopCore) band[i] = -2; continue; }
			if (i >= NA) continue;
			int b = (int)std::floor(3.0 * std::log2(std::max(1e-9, fHz / fTopCore)));
			band[i] = std::max(0, std::min(nBands - 1, b));
		}
	}

	// ---- statistical cascade above the core (calibrated against the full coupled model) ----
	double cascK = 2.5e-5, cascG = 6e-5, cascP = 2.0;   // rates ~ eps^2 (four-wave), so the wash fades with the core
	double lastEps = 0;
	std::vector<double> bandE, bandN, bandPin;
	void cascadeBlock(double blockSec) {
		if (ctl.bloom <= 0) {
			for (int i = 0; i < N; i++) if (sigX[i] != 0 || nz[i] != 0) { sigX[i] = 0; nz[i] = 0; applyLoss(i); }
			return;
		}
		bandE.assign(nBands, 0.0);
		bandN.assign(nBands, 0.0);
		bandPin.assign(nBands + 1, 0.0);
		double Etop = 0, nTop = 0;
		for (int i = 0; i < NA; i++) {
			int b = band[i];
			if (b == -1) continue;
			double v = (q[i] - qp[i]) * fs;
			double e = 0.5 * v * v + 0.5 * wS[i] * wS[i] * q[i] * qp[i];
			if (b == -2) { Etop += e; nTop += 1; }
			else { bandE[b] += e; bandN[b] += 1; }
		}
		// nonlinearity strength: the core's energy over the energy at which the edge moves by
		// about one thickness (wave-turbulence rates scale with it)
		double Ecore = 0;
		for (int i = 0; i < NC; i++) {
			double v = (q[i] - qp[i]) * fs;
			Ecore += 0.5 * v * v + 0.5 * wS[i] * wS[i] * q[i] * qp[i];
		}
		double Eref = M->Eref * std::pow(lam, 3.0) / std::pow(lam, 2.0);   // E ~ w^2 q^2: x lam^-2 lam^5
		double eps = std::pow(std::max(0.0, Ecore / Eref), cascP);
		lastEps = eps;
		double fTopC = M->fTopCore / lam;
		double nuOut = std::min(60.0, cascK * ctl.bloom * eps * fTopC);
		bandPin[0] += 2 * nuOut * Etop;
		std::vector<double> nuB(nBands, 0.0);
		for (int b = 0; b < nBands; b++) {
			if (bandN[b] == 0) continue;
			double nu = std::min(400.0, cascG * ctl.bloom * eps * bandF[b]);
			nuB[b] = nu;
			if (b + 1 < nBands) bandPin[b + 1] += 2 * nu * bandE[b];
		}
		for (int i = 0; i < NA; i++) {
			int b = band[i];
			if (b == -1) continue;
			double sx = 0, nzz = 0;
			if (b == -2) sx = nuOut;
			else {
				sx = nuB[b];
				if (bandPin[b] > 0) nzz = std::sqrt(2.0 * bandPin[b] / bandN[b] * fs * 4.0) * 1.7320508;   // uniform +-1 has variance 1/3; every 4th step
			}
			if (sx != sigX[i]) { sigX[i] = sx; applyLoss(i); }
			nz[i] = nzz;
		}
		(void)blockSec;
	}

	double edgeSqCache = -1, edgeSq0Cache = -1;
	double meanEdgeSq() {
		if (edgeSqCache < 0) {
			double s = 0;
			const Shape& sh = M->sh;
			for (int i = 0; i < N; i++) { double w = M->W(i, sh.r0 + 0.85 * (sh.a - sh.r0)); s += w * w; }
			edgeSqCache = s / std::max(1, N);
		}
		return edgeSqCache * sPhi * sPhi;
	}
	double meanEdgeSq0() {
		if (edgeSq0Cache < 0) {
			double s = 0;
			for (int i = 0; i < N; i++) s += M->shapes[M->modes[i].shape].edgeE;
			edgeSq0Cache = s / std::max(1, N);
		}
		return edgeSq0Cache;
	}

	// Rayleigh-integral pickup for an unbaffled thin disc (dipole kernel), per mode:
	//   W_i = int phi_i cos(gamma) (1 + i k R) e^{-i k R} / R^2 dS
	// out = Re(W_i) a_i - Im(W_i) w_i v_i
	void computeMic() {
		const Shape& sh = M->sh;
		double a = sh.a * lam;
		struct MicPos { double x, yv, z; };
		auto micAt = [&](double u) -> MicPos {
			// close positions are relative to the cymbal (height above the surface)
			double zc, zp;
			if (u <= 1.0) {   // BELL .. BOW
				double rf = 0.05 + 0.45 * u;
				sh.z(sh.r0 + rf * (sh.a - sh.r0), zc, zp);
				return {rf * a + 0.03 * (1 - u), 0.0, zc * lam + 0.06 + 0.02 * u};
			}
			if (u <= 2.0) {   // BOW .. EDGE
				double t = u - 1.0;
				double rf = 0.5 + 0.48 * t;
				sh.z(sh.r0 + rf * (sh.a - sh.r0), zc, zp);
				return {rf * a, 0.0, zc * lam + 0.08 - 0.02 * t};
			}
			if (u <= 3.0) {   // EDGE .. OVERHEAD
				double t = u - 2.0;
				return {a * 0.98 * (1 - t) + 0.4 * t, 0.0, 0.06 * (1 - t) + 0.9 * t};
			}
			double t = std::min(1.0, u - 3.0);   // OVERHEAD .. ROOM
			double ang = 35.0 * CY_PI / 180.0;
			return {0.4 * (1 - t) + 3.0 * std::cos(ang) * t, 0.0, 0.9 * (1 - t) + 3.0 * std::sin(ang) * t};
		};
		MicPos mp = micAt(std::min(4.0, std::max(0.0, ctl.mic)));
		const int NR = 48, NT = 96;
		const double c0 = 343.0;
		// surface points (on the scaled cymbal), with normals and area
		std::vector<double> pr(NR), pz(NR), pnr(NR), pnz(NR), pA(NR);
		for (int j = 0; j < NR; j++) {
			double rf = (j + 0.5) / NR;
			double r = sh.r0 + rf * (sh.a - sh.r0);
			double zz, zp;
			sh.z(r, zz, zp);
			double nrm = std::sqrt(1 + zp * zp);
			pr[j] = r * lam;
			pz[j] = zz * lam;
			pnr[j] = -zp / nrm;
			pnz[j] = 1.0 / nrm;
			pA[j] = pr[j] * ((sh.a - sh.r0) * lam / NR) * nrm * (2 * CY_PI / NT);
		}
		std::vector<std::complex<double>> W(N);
		std::vector<double> ct(NT), st(NT);
		for (int t = 0; t < NT; t++) {
			ct[t] = std::cos(2 * CY_PI * (t + 0.5) / NT);
			st[t] = std::sin(2 * CY_PI * (t + 0.5) / NT);
		}
		// precompute geometry per surface point
		std::vector<double> Rr(NR * NT), cg(NR * NT);
		for (int j = 0; j < NR; j++)
			for (int t = 0; t < NT; t++) {
				double px = pr[j] * ct[t], py = pr[j] * st[t], pzz = pz[j];
				double dx = mp.x - px, dy = mp.yv - py, dz = mp.z - pzz;
				double R = std::sqrt(dx * dx + dy * dy + dz * dz);
				double nx = pnr[j] * ct[t], ny = pnr[j] * st[t], nz = pnz[j];
				Rr[j * NT + t] = R;
				cg[j * NT + t] = (dx * nx + dy * ny + dz * nz) / R;
			}
		for (int i = 0; i < NA; i++) {
			const Mode& md = M->modes[i];
			double kk = (om0[i] / lam) / c0;
			std::complex<double> acc(0, 0);
			for (int j = 0; j < NR; j++) {
				double rabs = M->sh.r0 + (j + 0.5) / NR * (sh.a - sh.r0);
				double Wr = M->W(i, rabs) * sPhi;
				if (Wr == 0) continue;
				for (int t = 0; t < NT; t++) {
					double th = 2 * CY_PI * (t + 0.5) / NT;
					double a1 = md.m * (th - thOff[i]);
					double ph = Wr * (md.par ? std::sin(a1) : std::cos(a1));
					double R = Rr[j * NT + t];
					std::complex<double> e = std::polar(1.0, -kk * R);
					acc += ph * cg[j * NT + t] * std::complex<double>(1.0, kk * R) * e / (R * R) * pA[j];
				}
			}
			W[i] = acc;
		}
		for (int i = 0; i < N; i++) {
			if (i >= NA) { micW[i] = micV[i] = 0; continue; }
			double w = om0[i] / lam;
			micW[i] = W[i].real();
			micV[i] = -W[i].imag() * w;
		}
	}

	// ---- beaters ----
	void setupContact(Contact& c) {
		c.phi.resize(NA);
		double C = 0;
		for (int i = 0; i < NA; i++) {
			c.phi[i] = phiAt(i, c.r, c.th);
			C += c.phi[i] * c.phi[i] * g[i];
		}
		c.C = C;
	}

	// strike with speed v (m/s), using the current PLAY / HARDNESS / POSITION
	void strike(double v) {
		int play = ctl.play;
		double hd = std::min(1.0, std::max(0.0, ctl.hardness));
		auto launch = [&](double mass, double K, double alpha, double lam_, double r, double th, double vv, int delaySteps) {
			for (auto& c : beaters) {
				if (c.active) continue;
				c = Contact();
				c.active = true;
				c.mass = mass;
				c.K = K;
				c.alpha = alpha;
				c.lam = lam_;
				c.r = std::min(0.995, std::max(0.0, r));
				c.th = th;
				setupContact(c);
				double ws = 0;
				for (int i = 0; i < NA; i++) ws += c.phi[i] * q[i];
				double gap = 1e-6 + vv * k * delaySteps;
				c.y = ws + gap;
				c.yp = c.y + vv * k;    // moving down at vv
				c.age = 0;
				return;
			}
		};
		double pos = ctl.position;
		if (play == TIP) launch(0.008, 3e7 * std::pow(10.0, hd), 1.5, 0.2, pos, 0.0, v, 0);
		else if (play == SHOULDER) launch(0.025, 1e7 * std::pow(10.0, hd), 1.5, 0.3, pos, 0.0, v, 0);
		else if (play == MALLET) launch(0.03, 1e4 * std::pow(100.0, hd), 2.8, 0.9, pos, 0.0, v, 0);
		else if (play == RODS) {
			for (int j = 0; j < 7; j++) {
				double dr = (j - 3) * 0.004 / std::max(0.05, M->sh.a * lam);
				launch(0.002, 1e7, 1.5, 0.3, pos + dr, 0.01 * rng.bi(), v * (0.85 + 0.3 * rng.uni()), (int)(rng.uni() * 1.5e-3 * fs));
			}
		}
		else {
			// BRUSH and BOW: provisional (grains / friction come later): a soft wide contact
			launch(0.02, 3e5, 2.0, 1.0, pos, 0.0, v * 0.5, 0);
		}
	}

	// implicit scalar contact solve: delta + C_tot * F(delta) = delta_pred
	void solveContact(Contact& c) {
		double ws = 0;
		for (int i = 0; i < NA; i++) ws += c.phi[i] * q[i];   // q is already the predicted new state
		double ypred = 2 * c.y - c.yp - k * k * c.gravity;
		double dpred = ws - ypred;
		double F = 0;
		double Ctot = c.C + k * k / c.mass;
		if (dpred > 0) {
			// Newton on h(d) = d + Ctot*K*d^alpha*(1 + lam*(d - d_old)/k) - dpred
			double d = std::min(dpred, std::pow(dpred / (Ctot * c.K), 1.0 / c.alpha));
			double dOld = std::max(0.0, c.delta);
			for (int it = 0; it < 30; it++) {
				double dd = std::max(d, 0.0);
				double damp = std::max(0.0, 1 + c.lam * (dd - dOld) / k);
				double Fd = c.K * std::pow(dd, c.alpha) * damp;
				double h = dd + Ctot * Fd - dpred;
				double dF = c.K * (c.alpha * std::pow(std::max(dd, 1e-30), c.alpha - 1) * damp + std::pow(dd, c.alpha) * c.lam / k);
				double step = h / (1 + Ctot * dF);
				d = dd - step;
				if (d < 0) d = 0.5 * dd;
				if (std::fabs(step) < 1e-15 + 1e-10 * std::fabs(dd)) break;
			}
			d = std::max(0.0, d);
			double damp = std::max(0.0, 1 + c.lam * (d - dOld) / k);
			F = c.K * std::pow(d, c.alpha) * damp;
			c.delta = d;
		}
		else c.delta = dpred;
		// apply
		if (F > 0)
			for (int i = 0; i < NA; i++) q[i] -= g[i] * c.phi[i] * F;
		double yn = ypred + k * k * F / c.mass;
		c.yp = c.y;
		c.y = yn;
		c.force = F;
		c.age++;
	}

	// ---- nonlinear membrane force ----
	// Flattened coupling: for each stress mode n, triplets (i <= j, c) with
	//   zeta_n = sB * sum c q_i q_j      (core blocks and extended pairs alike)
	struct Trip { int32_t i, j; float c; };
	std::vector<Trip> trips;
	std::vector<int> tripOff;            // per stress mode, start in trips (size nStress + 1)
	std::vector<double> zbuf, abuf;
	int tripLimit = -1;                  // optional cap on extended pairs (quality)
	void prepareNonlinear() {
		trips.clear();
		tripOff.assign(M->stress.size() + 1, 0);
		for (size_t n = 0; n < M->stress.size(); n++) {
			tripOff[n] = (int)trips.size();
			const StressMode& sm = M->stress[n];
			for (const BBlock& bb : sm.blocks) {
				const auto& C1 = M->classes[bb.c1];
				const auto& C2 = M->classes[bb.c2];
				int n1 = (int)C1.idx.size(), n2 = (int)C2.idx.size();
				for (int a = 0; a < n1; a++)
					for (int b = 0; b < n2; b++) {
						int i = C1.start() + a, j = C2.start() + b;
						double v = bb.v[a * n2 + b];
						if (v == 0.0) continue;
						if (i < j) trips.push_back({i, j, (float)(2.0 * v)});   // (i,j) and (j,i) are both stored in the blocks
						else if (i == j) trips.push_back({i, j, (float)v});
					}
			}
			for (size_t p = 0; p < sm.pi.size(); p++) trips.push_back({sm.pi[p], sm.pj[p], (float)sm.pc[p]});
		}
		tripOff[M->stress.size()] = (int)trips.size();
		zbuf.assign(M->stress.size(), 0.0);
		abuf.assign(M->stress.size(), 0.0);
	}

	double vnl = 0;   // nonlinear potential at the last step
	void nonlinearGradient(double bloom) {
		std::fill(grad.begin(), grad.begin() + std::max(NC, NA), 0.0);
		vnl = 0;
		if (bloom <= 0) return;
		if (tripOff.size() != M->stress.size() + 1) prepareNonlinear();
		const double* qd = q.data();
		double* gd = grad.data();
		const Trip* T = trips.data();
		size_t NS = M->stress.size();
		for (size_t n = 0; n < NS; n++) {
			const StressMode& sm = M->stress[n];
			double z0 = 0, z1 = 0;
			int e = tripOff[n + 1];
			int t = tripOff[n];
			for (; t + 1 < e; t += 2) {
				z0 += T[t].c * qd[T[t].i] * qd[T[t].j];
				z1 += T[t + 1].c * qd[T[t + 1].i] * qd[T[t + 1].j];
			}
			if (t < e) z0 += T[t].c * qd[T[t].i] * qd[T[t].j];
			// BLOOM scales the stress-function amplitude zeta (not A): V = (A.q) z + z^2/2 with
			// z = bloom * zeta = 1/2 (z + A.q)^2 - 1/2 (A.q)^2 stays bounded below for any BLOOM.
			double zeta = (z0 + z1) * sB * bloom;
			double an = 0;
			for (size_t a = 0; a < sm.aIdx.size(); a++) an += sm.aVal[a] * qd[sm.aIdx[a]];
			an *= sA;
			vnl += an * zeta + 0.5 * zeta * zeta;
			double coef = (an + zeta) * bloom * sB;
			for (int u = tripOff[n]; u < e; u++) {
				double cc = coef * T[u].c;
				gd[T[u].i] += cc * qd[T[u].j];
				gd[T[u].j] += cc * qd[T[u].i];
			}
			double za = zeta * sA;
			for (size_t a = 0; a < sm.aIdx.size(); a++) gd[sm.aIdx[a]] += za * sm.aVal[a];
		}
	}

	// rivets (SIZZLE)
	void setRivets(int n) {
		if ((int)rivets.size() == n) return;
		rivets.clear();
		CyRng r;
		r.seed(ctl.seed * 31 + 5);
		for (int j = 0; j < n; j++) {
			Contact c;
			c.active = true;
			c.expires = false;
			c.mass = 0.0004 * (0.8 + 0.4 * r.uni());
			c.K = 2e8 * (0.5 + r.uni());
			c.alpha = 1.5;
			c.lam = 0.1;
			c.gravity = 9.81;
			c.r = 0.82;
			c.th = 2 * CY_PI * (j + 0.2 * r.bi()) / std::max(1, n);
			setupContact(c);
			double ws = 0;
			for (int i = 0; i < NA; i++) ws += c.phi[i] * q[i];
			double dst = std::pow(c.mass * 9.81 / c.K, 1.0 / c.alpha);
			c.y = ws - dst;
			c.yp = c.y;
			rivets.push_back(c);
		}
	}

	// one engine step (fs)
	double step(double excite) {
		nonlinearGradient(ctl.bloom);
		// external forces: EXCITE at the strike point
		bool exc = excite != 0.0 && ctl.excite > 0;
		static thread_local std::vector<double> phiS;
		if (exc) {
			if ((int)phiS.size() != NA || exciteDirty) {
				phiS.resize(NA);
				for (int i = 0; i < NA; i++) phiS[i] = phiAt(i, ctl.position, 0.0);
				exciteDirty = false;
			}
		}
		double fe = exc ? excite * ctl.excite * ctl.force * 4.0 : 0.0;
		bool inj = ((stepCount++ & 3) == 0);
		for (int i = 0; i < NA; i++) {
			double fi = -grad[i];
			if (exc) fi += fe * phiS[i];
			if (inj && nz[i] != 0) fi += nz[i] * rng.bi();
			double qn = c1[i] * q[i] - c2[i] * qp[i] + g[i] * fi;
			qp[i] = q[i];
			q[i] = qn;
		}
		// contacts (implicit, on the predicted state)
		double clickF = 0;
		for (auto& c : beaters) {
			if (!c.active) continue;
			solveContact(c);
			clickF += c.force * (c.mass < 0.02 ? 1.0 : 0.6);
			// expire when far above and moving away
			double ws = 0;
			if (c.age > 64 && (c.age & 15) == 0) {
				for (int i = 0; i < NA; i++) ws += c.phi[i] * q[i];
				if (c.y - ws > 0.003 && c.y > c.yp) c.active = false;
				if (c.age > (int)(0.5 * fs)) c.active = false;
			}
		}
		for (auto& c : rivets) solveContact(c);
		// stick click
		double click = 0;
		if (ctl.play == TIP || ctl.play == SHOULDER || ctl.play == RODS) {
			for (int j = 0; j < 3; j++) {
				double x = clkC1[j] * clk[j][0] - clkC2[j] * clk[j][1] + clkG[j] * clickF * (j == 0 ? 1.0 : 0.6);
				clk[j][1] = clk[j][0];
				clk[j][0] = x;
				click += x;
			}
		}
		// pickup: acceleration and velocity
		double out = 0;
		double ik2 = 1.0 / (k * k), i2k = 0.5 / k;
		// (acceleration from the central difference of the last three states)
		for (int i = 0; i < NA; i++) {
			double v = (q[i] - qPrev2(i)) * i2k;
			double a = (q[i] - 2 * qp[i] + qPrev2(i)) * ik2;
			out += micW[i] * a + micV[i] * v;
		}
		rotatePrev2();
		double clickLevel = (ctl.mic < 2.5 ? 1.0 : 0.25) * 2e4;
		return out + click * clickLevel;
	}

	// previous-previous state for the pickup
	std::vector<double> qpp;
	double qPrev2(int i) const { return qpp[i]; }
	void rotatePrev2() {}
	bool exciteDirty = true;
	uint64_t stepCount = 0;

	// host-rate processing
	void process(const float* excIn, float* out, int n) {
		if ((int)qpp.size() != N) qpp.assign(N, 0.0);
		for (int s = 0; s < n; s++) {
			if ((ctlCounter++ & 31) == 0) {
				updateCoefficients();
				setRivets((int)std::lround(ctl.sizzle * 12));
				cascadeBlock(32.0 / fsHost);
				energyGuard(excIn != nullptr && ctl.excite > 0);
			}
			double e = excIn ? excIn[s] : 0.0;
			double acc = 0;
			for (int o = 0; o < OS; o++) {
				// keep q^{n-1} for the pickup
				for (int i = 0; i < NA; i++) qpp[i] = qp[i];
				double v = step(e);
				// FIR decimator
				int L = (int)firH.size();
				firBuf[firPos] = v;
				firBuf[firPos + L] = v;
				firPos = (firPos + 1) % L;
				if (o == OS - 1) {
					for (int t = 0; t < L; t++) acc += firH[t] * firBuf[firPos + t];
				}
			}
			// DC block + ROOM lowpass
			double yv = acc - dcX + 0.9995 * dcY;
			dcX = acc;
			dcY = yv;
			lp += lpA * (yv - lp);
			double o = lp * outGain;
			if (!std::isfinite(o)) {
				reset();
				o = 0;
				guardEvents++;
			}
			// soft limit above 8 V
			double ao = std::fabs(o);
			if (ao > 8.0) o = (o > 0 ? 1 : -1) * (8.0 + 2.0 * std::tanh((ao - 8.0) / 2.0));
			out[s] = (float)o;
		}
	}

	// Energy guard: with nothing driving the shell its (linear) energy can only fall,
	// apart from the small exchange with the nonlinear potential. If it grows by more
	// than that within a control block (explicit stepping beyond its stability limit at
	// high BLOOM), pull the state back. Silent in normal use.
	double guardE = 0;
	void energyGuard(bool driven) {
		bool contact = false;
		for (auto& c : beaters) contact |= c.active;
		double E = energy() + vnl;
		if (!std::isfinite(E)) { reset(); guardEvents++; guardE = 0; return; }
		if (!driven && !contact && guardE > 0 && E > guardE * 1.05) {
			double s = std::sqrt(guardE * 1.05 / E);
			for (int i = 0; i < NA; i++) { q[i] *= s; qp[i] *= s; }
			E = guardE * 1.05;
			guardEvents++;
		}
		guardE = E;
	}

	void reset() {
		std::fill(q.begin(), q.end(), 0.0);
		std::fill(qp.begin(), qp.end(), 0.0);
		std::fill(qpp.begin(), qpp.end(), 0.0);
		for (auto& c : beaters) c.active = false;
		rivets.clear();
		dcX = dcY = lp = 0;
		for (auto& r : clk) r[0] = r[1] = 0;
	}

	// total energy of the modes (kinetic + linear potential), for tests and the guard
	double energy() const {
		double E = 0;
		for (int i = 0; i < NA; i++) {
			double v = (q[i] - qp[i]) * fs;
			double w = om0[i] / lam;
			E += 0.5 * v * v + 0.5 * w * w * q[i] * qp[i];
		}
		return E;
	}
};

} // namespace cy
