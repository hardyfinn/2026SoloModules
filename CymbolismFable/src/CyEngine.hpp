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
#include "CyUnreal.hpp"
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
	bool roll = false;       // ROLL gate: roll (contact beaters), sweep (BRUSH), bow (BOW)
	uint64_t seed = 1;       // hammering pattern
	Unreal u;                // Cymbolism Extended (neutral by default)
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
	double life = 0.5;       // seconds before a beater is dropped
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
	double outGain = 0.0008;  // pickup units -> volts (medium crash hit at BOW ~3 V peak)
	double lp = 0, lpA = 1.0;         // ROOM air absorption
	// mic and air: a gentle 2-pole roll-off above about 11 kHz (bright, not fizzy)
	double bq[5] = {1, 0, 0, 0, 0}, bz1 = 0, bz2 = 0;
	void setupAir() {
		double fc = 11000.0, Q = 0.6;
		double w0 = 2 * CY_PI * std::min(fc, 0.45 * fsHost) / fsHost, al = std::sin(w0) / (2 * Q), cs = std::cos(w0);
		double a0 = 1 + al;
		bq[0] = (1 - cs) / 2 / a0; bq[1] = (1 - cs) / a0; bq[2] = bq[0];
		bq[3] = -2 * cs / a0; bq[4] = (1 - al) / a0;
	}
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
		OS = 1;   // 2x oversampling measured at ~3x the CPU for no audible gain below 20 kHz
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
		setupAir();
		firPos = 0;
		beaters.assign(8, Contact());
		rng.seed(ctl.seed);
		applyHammer();
		tuneKey[5] = ctl.hammer;
		tuneKey[6] = (double)ctl.seed;
		updateCoefficients(true);
		mk = mkTarget;
		qpp.assign(N, 0.0);
		prepareNonlinear();
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
		static const double A0[NUM_TYPES_EXT] = {2.5, 0.9, 1.1, 0.6, 0.5, 0.4, 0.8, 1.5, 1.3, 0.33, 0.25, 0.8, 0.15, 0.3, 0.5, 0.4};
		static const double B1[NUM_TYPES_EXT] = {9e-4, 4.8e-4, 5.0e-4, 4.0e-4, 3.3e-4, 2.8e-4, 4.0e-4, 7e-4, 6e-4, 2.4e-4, 2.0e-4, 4.0e-4, 1.5e-4, 2.0e-4, 3.0e-4, 2.5e-4};
		int t = M->gp.type;
		double etaRel = M->sh.mat.eta / CY_MATERIALS[B20].eta;
		double wRel = std::pow(std::max(0.3, M->gp.weight), -0.5);
		const Material& mt = M->sh.mat;
		double a = A0[t] * wRel * std::pow(lam, -0.5) * mt.aMul;
		double b = B1[t] * (0.5 + 0.5 * etaRel) * wRel * mt.bMul;
		(void)i;
		return a + b * fHz + mt.f2 * fHz * fHz;
	}

	double phiAt(int i, double rFrac, double th) const {
		const Shape& sh = M->sh;
		double r = sh.r0 + rFrac * (sh.a - sh.r0);
		return sPhi * M->phi(i, r, th, thOff[i]);
	}

	// cached tuning (recomputed when size, mount, hammer, decay, damp or choke change)
	std::vector<double> wS, cw, gw, sigBase, sigX, nz;
	std::vector<int> band;               // cascade band of each upper mode (-1 none); top-core modes = -2
	int naPrev = -1;
	int nBands = 0;
	std::vector<double> bandF, bandRef;  // centre frequency, reference energy per mode
	double tuneKey[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

	void applyLoss(int i) {
		if (frozen[i]) { c1[i] = c2[i] = g[i] = 0; sig[i] = 0; return; }
		double s = sigBase[i] * lossMul + sigX[i] + sigU[i];
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
			sigU.assign(N, 0); frozen.assign(N, 0); wU.assign(N, 0); wB.assign(N, 0); aFac.assign(NC, 1.0); logF.assign(N, 0); wCol.assign(N, 1.0);
			changed = true;
		}
		// UNREAL frequency law: checked every 4th control block, retuned only on a real change
		if ((ctlCounter & 127) == 1 || changed) {
			ctl.u.freqKey(uKeyNow);
			if (uKeyNow.size() != uKey.size()) changed = true;
			else for (size_t j = 0; j < uKey.size(); j++) if (std::fabs(uKeyNow[j] - uKey[j]) > 1e-6 * (1.0 + std::fabs(uKey[j]))) { changed = true; break; }
			if (changed) uKey = uKeyNow;
			if (std::fabs(ctl.u.colour - colourCache) > 1e-3 * colourCache) updateColour();
		}
		if (changed) {
			if (key[5] != tuneKey[5] || key[6] != tuneKey[6]) applyHammer();
			for (int j = 0; j < 7; j++) tuneKey[j] = key[j];
			retune();
		}
		if (force || changed || std::fabs(ctl.mic - micCache) > 1e-3 || std::fabs(lam - sizeCache) > 1e-4) {
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

	// ---- UNREAL state ----
	std::vector<double> uKey, uKeyNow;
	std::vector<double> wU, wB;          // working w before MOUNT (unreal law, at size); w with MOUNT under the built law
	std::vector<double> aFac;            // per core mode: f_now / f_built (keeps diag(w^2) - A^T A >= 0)
	std::vector<double> sigU;            // thermostat loss (SUSTAIN, FREEZE)
	std::vector<char> frozen;            // modes moved out of the band by the unreal law
	std::vector<double> logF, wCol;      // log2 working frequency, COLOUR weights
	double lossMul = 1.0;                // FREEZE scales the ordinary losses
	double colourCache = -1;
	bool unrealFreq = false;
	UnrealTables ut;
	std::vector<double> fBaseHz, fUHz, uScratch, uTargets;
	double mkTarget = 1.0, mk = 1.0;     // level make-up for bent spectra

	void retune() {
		lam = std::max(0.15, ctl.size);
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
		const Unreal& u = ctl.u;
		bool wasUnreal = unrealFreq;
		unrealFreq = !u.freqNeutral();
		if (unrealFreq) {
			prepareUnrealTables();
			fBaseHz.resize(N);
			for (int i = 0; i < N; i++) fBaseHz[i] = om0[i] / (2 * CY_PI);
			applyFrequencyLaw(u, ut, fBaseHz, fUHz, uScratch, uTargets);
		}
		double tiltE = 1.5 * std::max(-1.0, std::min(1.0, u.tilt));
		for (int i = 0; i < N; i++) {
			const Mode& md = M->modes[i];
			const RadialShape& rs = M->shapes[md.shape];
			double Th = md.m == 0 ? 2 * CY_PI : CY_PI;
			double w0 = om0[i] / lam;
			double Wc = rs.centreW * sPhi, Wr = rs.centreWr * sPhi / lam;
			double dw2 = Th * r0 * (kLine * Wc * Wc + kRot * Wr * Wr);
			double wb = std::sqrt(w0 * w0 + dw2);
			wB[i] = wb;
			double w = wb;
			frozen[i] = 0;
			if (unrealFreq) {
				double wu = 2 * CY_PI * fUHz[i] / lam;
				// TUNE aims at the pitch you hear, so take the MOUNT shift out first
				if (u.tune > 0) wu = std::sqrt(std::max(0.25 * wu * wu, wu * wu - u.tune * dw2));
				// core modes are always kept (the coupling indexes them): clamp into the band
				if (i < NC) wu = std::max(2 * CY_PI * 25.0, std::min(2 * CY_PI * 0.45 * fs, wu));
				w = std::sqrt(wu * wu + dw2);
				double fw = w / (2 * CY_PI);
				if (i >= NC && (fw < 25.0 || fw >= fTop)) frozen[i] = 1;
				if (i < NC) aFac[i] = wu / w0;
				wU[i] = wu;
			}
			else {
				wU[i] = w0;
				if (i < NC) aFac[i] = 1.0;
			}
			double fHz = w / (2 * CY_PI);
			if (unrealFreq ? !frozen[i] : fHz < fTop) NA = i + 1;
			double s = lossAt(fHz, i) / std::max(0.1, ctl.decay);
			if (tiltE != 0.0) s = std::max(0.05, std::min(60.0, s * std::pow(std::max(1.0, fHz) / 1000.0, tiltE)));
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
			if (frozen[i]) { q[i] = qp[i] = 0; if ((int)qpp.size() == N) qpp[i] = 0; }
			logF[i] = std::log2(std::max(1.0, fHz));
			applyLoss(i);
		}
		if (NA != naPrev) {
			// the three state buffers rotate, so modes outside the active range must hold nothing
			for (int i = NA; i < N; i++) { q[i] = qp[i] = 0; if ((int)qpp.size() == N) qpp[i] = 0; }
			naPrev = NA;
		}
		// cascade bands (1/3 octave) above the core, from the built law: coupling follows the
		// shapes' wavelengths, which the unreal law does not change
		double fTopCore = M->fTopCore / lam;
		nBands = (int)std::ceil(3.0 * std::log2(fTop / fTopCore)) + 1;
		if (nBands < 1) nBands = 1;
		bandF.assign(nBands, 0);
		for (int b = 0; b < nBands; b++) bandF[b] = fTopCore * std::pow(2.0, (b + 0.5) / 3.0);
		for (int i = 0; i < N; i++) {
			band[i] = -1;
			double fHz = wB[i] / (2 * CY_PI);
			if (i < NC) { if (fHz > 0.7 * fTopCore) band[i] = -2; continue; }
			if (i >= NA || frozen[i]) continue;
			int b = (int)std::floor(3.0 * std::log2(std::max(1e-9, fHz / fTopCore)));
			band[i] = std::max(0, std::min(nBands - 1, b));
		}
		updateColour();
		computeMakeup();
		if (unrealFreq || wasUnreal) guardE = 0;   // the energy's yardstick moved with the frequencies
		if (unrealFreq || tripScaled) scaleTrips();
		if (unrealFreq || wasUnreal) {
			exciteDirty = true;
			if (bowOn) bowSetup();
			for (auto& c : beaters) if (c.active) refreshContact(c);
			for (auto& c : rivets) refreshContact(c);
		}
		rvReady = false;   // size, hammering and the loss all enter the rivets' tables
	}

	// per-mode tables for the unreal law (built once per model, LOAD's when its position moves)
	double loadPosCache = -1, loadLamCache = -1;
	void prepareUnrealTables() {
		if ((int)ut.twin.size() != N) {
			ut.twin.assign(N, 0);
			std::vector<int> first(M->shapes.size(), -1);
			for (int i = 0; i < N; i++) {
				int s = M->modes[i].shape;
				ut.twin[i] = i;
				if (first[s] < 0) first[s] = i;
				else { ut.twin[i] = first[s]; ut.twin[first[s]] = i; }
			}
			ut.jit.assign(N, 0.0);
			CyRng r;
			r.seed(ctl.seed * 104729 + 3);
			std::vector<double> js(M->shapes.size());
			for (double& j : js) j = 0.04 * r.bi();   // about +-4 cents of residual chorus
			for (int i = 0; i < N; i++) ut.jit[i] = js[M->modes[i].shape];
			ut.tensS.assign(M->tensS.begin(), M->tensS.end());
			ut.fModel.resize(N);
			for (int i = 0; i < N; i++) ut.fModel[i] = M->modes[i].f;
		}
		const Unreal& u = ctl.u;
		if (u.load != 0.0 && (std::fabs(u.loadPos - loadPosCache) > 1e-4 || lam != loadLamCache || (int)ut.loadPhi2.size() != N)) {
			ut.loadPhi2.resize(N);
			const Shape& sh = M->sh;
			double r = sh.r0 + std::min(0.99, std::max(0.0, u.loadPos)) * (sh.a - sh.r0);
			for (int i = 0; i < N; i++) {
				double ph = M->phi(i, r, 0.5 * CY_PI, thOff[i]);   // 90 degrees round from the strike point
				ut.loadPhi2[i] = ph * ph * sPhi * sPhi;   // mass-normalised at the current SIZE
			}
			loadPosCache = u.loadPos;
			loadLamCache = lam;
		}
	}

	// MORPH target: called by the builder (not the audio thread) with TYPE B's model
	void setMorphModel(const Model& B) { ut.fBr = morphRatios(*M, B); }

	// COLOUR weights for SUSTAIN: a Gaussian in log frequency, 1.5 octaves wide
	void updateColour() {
		colourCache = std::max(20.0, ctl.u.colour);
		double lc = std::log2(colourCache);
		for (int i = 0; i < N; i++) {
			double x = (logF[i] - lc) / 1.5;
			wCol[i] = std::exp(-0.5 * x * x);
		}
	}

	// level make-up for bent spectra: a compressed law piles modes onto few pitches and sums
	// coherently (louder), a stretched one pushes modes out of the band (quieter). Measured
	// on CRASH and RIDE at a medium hit (geometric mean of the gain each needed).
	void computeMakeup() {
		if (!unrealFreq || ctl.u.warp == 1.0) { mkTarget = 1.0; return; }
		
		// knots at warp -1, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75, 1, 1.5, 2 (and 2.5 = 4.0)
		static const double X[12] = {-1, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75, 1, 1.5, 2, 2.5};
		static const double Y[12] = {3.0, 3.0, 2.6, 2.0, 1.7, 0.8, 0.5, 0.6, 1.0, 1.8, 2.4, 3.4};

		double w = std::max(-1.0, std::min(2.5, ctl.u.warp));
		int j = 0;
		while (j < 10 && w > X[j + 1]) j++;
		double t = (w - X[j]) / (X[j + 1] - X[j]);
		mkTarget = std::exp((1 - t) * std::log(Y[j]) + t * std::log(Y[j + 1]));
	}

		// ---- statistical cascade above the core (calibrated against the full coupled model) ----
	double cascK = 2.5e-5, cascG = 6e-5, cascP = 2.0;   // rates ~ eps^2 (four-wave), so the wash fades with the core
	double lastEps = 0;
	std::vector<double> bandE, bandN, bandPin;
	void cascadeBlock(double blockSec) {
		if (ctl.bloom == 0) {
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
		nlRatio = Ecore / Eref * ctl.bloom * ctl.bloom;
		lastEps = eps;
		double fTopC = M->fTopCore / lam;
		double bl = std::fabs(ctl.bloom) * ctl.u.flow;   // BLOOM either way; FLOW scales the cascade rates
		double nuOut = std::min(60.0, cascK * bl * eps * fTopC);
		bandPin[0] += 2 * nuOut * Etop;
		std::vector<double> nuB(nBands, 0.0);
		for (int b = 0; b < nBands; b++) {
			if (bandN[b] == 0) continue;
			double nu = std::min(400.0, cascG * bl * eps * bandF[b]);
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

	// Pickup: interpolate the model's precomputed Rayleigh weights between the two
	// neighbouring mic positions, rotate by each pair's orientation and the swing angle,
	// and scale to the current SIZE (mic distances scale with the cymbal).
	double swingAng = 0;
	double widthCache = -1, pickupCache = -1;
	double piezoGain = 3.8e3;   // contact pickup level (matched to the air mic on a medium hit)
	void computeMic() {
		widthCache = ctl.u.width;
		pickupCache = ctl.u.pickup;
		if (extraOn) { micW2.assign(N, 0.0); micV2.assign(N, 0.0); }
		double u = std::min(4.0, std::max(0.0, ctl.mic));
		int p0 = std::min(3, (int)u);
		double t = u - p0;
		int NS = (int)M->shapes.size();
		const float* r0 = &M->micRe[(size_t)p0 * NS];
		const float* i0 = &M->micIm[(size_t)p0 * NS];
		const float* r1 = &M->micRe[(size_t)(p0 + 1) * NS];
		const float* i1 = &M->micIm[(size_t)(p0 + 1) * NS];
		double n0 = (1 - t) * M->micNorm[p0], n1 = t * M->micNorm[p0 + 1];
		for (int i = 0; i < N; i++) {
			if (i >= NA || frozen[i]) { micW[i] = micV[i] = 0; continue; }
			const Mode& md = M->modes[i];
			int s = md.shape;
			double re = n0 * r0[s] + n1 * r1[s];
			double im = n0 * i0[s] + n1 * i1[s];
			double fac = 1.0;
			if (md.m > 0) {
				double ang = md.m * (thOff[i] + swingAng);
				fac = md.par ? -std::sin(ang) : std::cos(ang);
			}
			fac *= sPhi;
			double w = wU[i];
			micW[i] = re * fac;
			micV[i] = -im * fac * w;
			if (extraOn) {
				// stereo: mics at azimuth +-WIDTH; PICKUP crossfades to a contact pickup at the MIC radius
				double ps = ctl.u.width, p = std::min(1.0, std::max(0.0, ctl.u.pickup));
				double facL = 1.0, facR = 1.0;
				if (md.m > 0) {
					double aL = md.m * (thOff[i] + swingAng - ps), aR = md.m * (thOff[i] + swingAng + ps);
					facL = md.par ? -std::sin(aL) : std::cos(aL);
					facR = md.par ? -std::sin(aR) : std::cos(aR);
				}
				facL *= sPhi;
				facR *= sPhi;
				double rr = std::min(0.98, 0.05 + 0.93 * std::min(1.0, u / 2.0));
				const Shape& sh = M->sh;
				double W = M->W(i, sh.r0 + rr * (sh.a - sh.r0)) * sPhi * piezoGain;
				micW[i] = (1 - p) * re * facL;
				micV[i] = (1 - p) * (-im * facL * w) + p * W * facL / sPhi;
				micW2[i] = (1 - p) * re * facR;
				micV2[i] = (1 - p) * (-im * facR * w) + p * W * facR / sPhi;
			}
		}
	}

	// ---- beaters ----
	// contact vectors cover every mode, so a retune while a contact is alive stays in range
	void setupContact(Contact& c) {
		c.phi.resize(N);
		double C = 0;
		for (int i = 0; i < N; i++) {
			c.phi[i] = phiAt(i, c.r, c.th);
			if (i < NA) C += c.phi[i] * c.phi[i] * g[i];
		}
		c.C = C;
	}
	void refreshContact(Contact& c) {
		if ((int)c.phi.size() != N) { setupContact(c); return; }
		double C = 0;
		for (int i = 0; i < NA; i++) C += c.phi[i] * c.phi[i] * g[i];
		c.C = C;
	}

	// strike with speed v (m/s), using the current PLAY / HARDNESS / POSITION
	void strike(double v) {
		int play = ctl.play;
		// swing kick: impulse x distance from the centre (loose MOUNT swings more)
		swingKick(0.04 * v * ctl.position * (0.3 + 0.7 * ctl.mount));
		if (play == BRUSH) { brushTap(v, ctl.position, 0.0); return; }
		if (play == RAIN) { rainBurst(v); return; }
		if (play == BOW) { bowTimer = 0.25 + 0.1 * v; return; }
		strikeRaw(v);
	}
	void strikeRaw(double v) {
		int play = ctl.play;
		double hd = std::min(1.0, std::max(0.0, ctl.hardness));
		auto launch = [&](double mass, double K, double alpha, double lam_, double r, double th, double vv, int delaySteps) {
			for (auto& c : beaters) {
				if (c.active) continue;
				c = Contact();
				wake();
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
				// BOUNCE: a return force holds the beater against the cymbal, so it ricochets
				if (ctl.u.bounce > 0) {
					c.gravity = 5000.0 * ctl.u.bounce * ctl.u.bounce;
					c.life = 1.5;
				}
				return;
			}
		};
		double pos = ctl.position;
		if (play == TIP) launch(0.008, 3e7 * std::pow(10.0, hd), 1.5, 0.2, pos, 0.0, v, 0);
		else if (play == SHOULDER) launch(0.025, 1e7 * std::pow(10.0, hd), 1.5, 0.3, pos, 0.0, v, 0);
		else if (play == MALLET) launch(0.03, 1e9 * std::pow(100.0, hd), 2.5, 0.5, pos, 0.0, v, 0);   // felt: soft, strongly nonlinear
		else if (play == RODS) {
			for (int j = 0; j < 7; j++) {
				double dr = (j - 3) * 0.004 / std::max(0.05, M->sh.a * lam);
				launch(0.002, 1e7, 1.5, 0.3, pos + dr, 0.01 * rng.bi(), v * (0.85 + 0.3 * rng.uni()), (int)(rng.uni() * 1.5e-3 * fs));
			}
		}
		else launch(0.008, 3e7 * std::pow(10.0, hd), 1.5, 0.2, pos, 0.0, v, 0);
	}

	// implicit scalar contact solve: delta + C_tot * F(delta) = delta_pred
	void solveContact(Contact& c) {
		double ws = 0;
		for (int i = 0; i < NA; i++) ws += c.phi[i] * q[i];   // q is already the predicted new state
		double F = contactForce(c, ws);
		if (F > 0)
			for (int i = 0; i < NA; i++) q[i] -= g[i] * c.phi[i] * F;
	}
	// the scalar part: the contact force for a predicted surface height ws; advances the beater
	double contactForce(Contact& c, double ws) {
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
		double yn = ypred + k * k * F / c.mass;
		c.yp = c.y;
		c.y = yn;
		c.force = F;
		c.age++;
		return F;
	}

	// ---- nonlinear membrane force ----
	// Flattened coupling: for each stress mode n, triplets (i <= j, c) with
	//   zeta_n = sB * sum c q_i q_j      (core blocks and extended pairs alike)
	struct Trip { int32_t i, j; float c; };
	std::vector<Trip> trips;
	// under a bent law the quadratic strain moves with the frequencies too (B_ij x r_i r_j), so the
	// coupled core keeps the same shape in energy-normalised coordinates and stays as stable
	std::vector<Trip> tripsU;
	bool tripScaled = false;
	void scaleTrips() {
		if (!unrealFreq) { tripScaled = false; return; }
		if (tripOff.size() != M->stress.size() + 1) prepareNonlinear();
		tripsU = trips;
		for (Trip& t : tripsU) {
			double ri = wU[t.i] / (om0[t.i] / lam), rj = wU[t.j] / (om0[t.j] / lam);
			t.c = (float)(t.c * ri * rj);
		}
		tripScaled = true;
	}
	std::vector<int> tripOff;            // per stress mode, start in trips (size nStress + 1)
	std::vector<double> zbuf, abuf;
	double tripKeep = 0.99;              // share of each stress mode's coupling weight that is kept (1 = all)
	std::vector<double> pruneW, pruneS;
	int nGrad = 0;                       // modes the nonlinear force reaches: [0, nGrad)
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
			// Drop the weakest terms of this stress mode. A term's weight is its coefficient times
			// the two modes' typical amplitudes (~ 1/f at equal energy); the terms kept carry
			// tripKeep of the summed squared weight. The force stays the exact gradient of
			// V = (A.q) z + z^2 / 2 for the terms kept, so stability is untouched.
			if (tripKeep < 1.0) {
				int t0 = tripOff[n], t1 = (int)trips.size();
				pruneW.resize(t1 - t0);
				double tot = 0;
				for (int t = t0; t < t1; t++) {
					double w = std::fabs(trips[t].c) / (std::max(1.0, M->modes[trips[t].i].f) * std::max(1.0, M->modes[trips[t].j].f));
					pruneW[t - t0] = w;
					tot += w * w;
				}
				pruneS = pruneW;
				std::sort(pruneS.begin(), pruneS.end(), std::greater<double>());
				double acc = 0, thr = 0;
				for (double w : pruneS) { acc += w * w; if (acc >= tripKeep * tot) { thr = w; break; } }
				int o = t0;
				for (int t = t0; t < t1; t++) if (pruneW[t - t0] >= thr) trips[o++] = trips[t];
				trips.resize(o);
			}
		}
		tripOff[M->stress.size()] = (int)trips.size();
		nGrad = NC;
		for (const Trip& t : trips) nGrad = std::max(nGrad, std::max(t.i, t.j) + 1);
		nGrad = std::min(nGrad, N);
		zbuf.assign(M->stress.size(), 0.0);
		abuf.assign(M->stress.size(), 0.0);
	}

	double vnl = 0;   // nonlinear potential at the last step
	// ---- saving work: the bloom is switched off while the ring is too quiet for it to matter,
	// and the whole engine sleeps once it is silent and nothing drives it ----
	bool nlOff = false;               // nonlinear force skipped
	int nlHold = 0;                   // samples the bloom stays on after a strike
	double nlRatio = 1;               // core energy / Eref x BLOOM^2 (how nonlinear the ring is now)
	double nlGate = 1e-3;             // below this the bloom is off (0 = never)
	bool asleep = false;
	int quietBlocks = 0;
	double sleepRel = 1e-10;          // kinetic energy under this x a hard hit's energy counts as silent (0 = never sleep)
	long statSteps = 0, statNl = 0, statSleep = 0;   // for tests
	void wake() {
		asleep = false;
		quietBlocks = 0;
		nlOff = false;
		nlHold = (int)(0.1 * fs);
	}
	double kinetic() const {
		double E = 0;
		for (int i = 0; i < NA; i++) { double v = (q[i] - qp[i]) * fs; E += v * v; }
		for (const auto& c : rivets) { double v = (c.y - c.yp) * fs; E += c.mass * v * v; }
		return 0.5 * E;
	}
	// decided once per control block
	void restBlock(bool driven) {
		bool busy = driven || ctl.roll || bowTimer > 0;
		for (auto& c : beaters) busy |= c.active;
		// the bloom
		if (nlHold > 0) nlHold -= 32;
		bool off = nlGate > 0 && !busy && nlHold <= 0 && nlRatio < (nlOff ? 2.0 * nlGate : nlGate);
		if (off && !nlOff) { std::fill(grad.begin(), grad.begin() + nGrad, 0.0); vnl = 0; }
		nlOff = off;
		// sleep
		if (busy || sleepRel <= 0) { quietBlocks = 0; asleep = false; return; }
		if (asleep) return;
		if (kinetic() < sleepRel * hitEnergy()) quietBlocks++;
		else quietBlocks = 0;
		if (quietBlocks >= 128) asleep = true;   // quiet for 85 ms at 48 kHz: longer than the lowest mode's period
	}
	void nonlinearGradient(double bloom) {
		if (tripOff.size() != M->stress.size() + 1) prepareNonlinear();
		std::fill(grad.begin(), grad.begin() + nGrad, 0.0);
		vnl = 0;
		if (bloom == 0) return;
		const double* qd = q.data();
		double* gd = grad.data();
		const Trip* T = tripScaled ? tripsU.data() : trips.data();
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
			const double* af = pumpOn ? aPump.data() : aFac.data();
			for (size_t a = 0; a < sm.aIdx.size(); a++) an += sm.aVal[a] * af[sm.aIdx[a]] * qd[sm.aIdx[a]];
			an *= sA;
			vnl += an * zeta + 0.5 * zeta * zeta;
			double coef = (an + zeta) * bloom * sB;
			for (int u = tripOff[n]; u < e; u++) {
				double cc = coef * T[u].c;
				gd[T[u].i] += cc * qd[T[u].j];
				gd[T[u].j] += cc * qd[T[u].i];
			}
			double za = zeta * sA;
			for (size_t a = 0; a < sm.aIdx.size(); a++) gd[sm.aIdx[a]] += za * sm.aVal[a] * af[sm.aIdx[a]];
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
		rvReady = false;
	}

	// one engine step (fs)
	// The new state is written into the oldest buffer and the three are rotated, so q, qp and qpp
	// always hold steps n+1, n and n-1 without a copy. When nothing touches the shell after the
	// update (no beater, rivet, bow or grain), the pickup is summed in the same pass.
	double step(double excite) {
		statSteps++;
		if (!nlOff) { nonlinearGradient(ctl.bloom); statNl++; }
		// external forces: EXCITE at the strike point
		bool exc = excite != 0.0 && ctl.excite > 0;
		if (exc) {
			if ((int)phiS.size() != N || exciteDirty || excPos != ctl.position) {
				phiS.resize(N);
				for (int i = 0; i < N; i++) phiS[i] = phiAt(i, ctl.position, 0.0);
				excPos = ctl.position;
				exciteDirty = false;
			}
		}
		double fe = exc ? excite * ctl.excite * ctl.force * 1.0 : 0.0;
		bool inj = ((stepCount++ & 3) == 0);
		if (pumpOn) pumpCore();
		bool touched = !rivets.empty() || bowOn || !grainQ.empty();
		for (auto& c : beaters) touched |= c.active;
		const double ik2 = 1.0 / (k * k), i2k = 0.5 / k;
		const int nA = std::min(nGrad, NA), nc = std::min(NC, NA);
		const bool pick = !touched, stereo = extraOn;
		double out = 0, oc = 0, orr = 0;
		{
			const double* Q = q.data();
			const double* QP = qp.data();
			double* QN = qpp.data();
			const double* C1 = c1.data();
			const double* C2 = c2.data();
			const double* G = g.data();
			const double* MW = micW.data();
			const double* MV = micV.data();
			const double* MW2 = stereo ? micW2.data() : MW;
			const double* MV2 = stereo ? micV2.data() : MV;
			// the modes the nonlinear force reaches (the core comes first)
			for (int i = 0; i < nA; i++) {
				double fi = -grad[i];
				if (exc) fi += fe * phiS[i];
				if (inj && nz[i] != 0) fi += nz[i] * rng.bi();
				double qn = C1[i] * Q[i] - C2[i] * QP[i] + G[i] * fi;
				QN[i] = qn;
				if (pick) {
					double v = (qn - QP[i]) * i2k, a = (qn - 2 * Q[i] + QP[i]) * ik2;
					double c = MW[i] * a + MV[i] * v;
					out += c;
					if (stereo) { if (i < nc) oc += c; orr += MW2[i] * a + MV2[i] * v; }
				}
			}
			// the rest: free oscillators, apart from EXCITE and the cascade's noise (every 4th step)
			if (exc || inj) {
				for (int i = nA; i < NA; i++) {
					double fi = 0;
					if (exc) fi += fe * phiS[i];
					if (inj && nz[i] != 0) fi += nz[i] * rng.bi();
					QN[i] = C1[i] * Q[i] - C2[i] * QP[i] + G[i] * fi;
				}
				if (pick) {
					for (int i = nA; i < NA; i++) {
						double v = (QN[i] - QP[i]) * i2k, a = (QN[i] - 2 * Q[i] + QP[i]) * ik2;
						double c = MW[i] * a + MV[i] * v;
						out += c;
						if (stereo) { if (i < nc) oc += c; orr += MW2[i] * a + MV2[i] * v; }
					}
				}
			}
			else if (pick && !stereo) {
				double o = 0;
				for (int i = nA; i < NA; i++) {
					double qn = C1[i] * Q[i] - C2[i] * QP[i];
					QN[i] = qn;
					o += MW[i] * ((qn - 2 * Q[i] + QP[i]) * ik2) + MV[i] * ((qn - QP[i]) * i2k);
				}
				out += o;
			}
			else if (pick) {
				double o = 0, o2 = 0;
				for (int i = nA; i < NA; i++) {
					double qn = C1[i] * Q[i] - C2[i] * QP[i];
					QN[i] = qn;
					double a = (qn - 2 * Q[i] + QP[i]) * ik2, v = (qn - QP[i]) * i2k;
					o += MW[i] * a + MV[i] * v;
					o2 += MW2[i] * a + MV2[i] * v;
				}
				out += o;
				orr += o2;
			}
			else {
				for (int i = nA; i < NA; i++) QN[i] = C1[i] * Q[i] - C2[i] * QP[i];
			}
		}
		// rotate: q <- new, qp <- q, qpp <- qp
		q.swap(qp);
		q.swap(qpp);
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
				if (c.gravity == 0 && c.y - ws > 0.003 && c.y > c.yp) c.active = false;
				if (c.age > (int)(c.life * fs)) c.active = false;
			}
		}
		if (!rivets.empty()) rivetStep();
		if (!grainQ.empty()) fireGrains();
		if (bowOn) bowStep();
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
		double clickLevel = (ctl.mic < 2.5 ? 1.0 : 0.25) * 2e4;
		// pickup after the contacts: acceleration and velocity from the last three states
		if (touched) {
			const double* Q = q.data();
			const double* QP = qp.data();
			const double* QPP = qpp.data();
			if (!stereo) {
				for (int i = 0; i < NA; i++) {
					double v = (Q[i] - QPP[i]) * i2k;
					double a = (Q[i] - 2 * QP[i] + QPP[i]) * ik2;
					out += micW[i] * a + micV[i] * v;
				}
			}
			else {
				for (int i = 0; i < NA; i++) {
					double v = (Q[i] - QPP[i]) * i2k;
					double a = (Q[i] - 2 * QP[i] + QPP[i]) * ik2;
					double c = micW[i] * a + micV[i] * v;
					out += c;
					if (i < nc) oc += c;
					orr += micW2[i] * a + micV2[i] * v;
				}
			}
		}
		if (stereo) {
			// Extended: right mic, and the left mic split into the core and the wash above it
			stepCore = oc;
			stepWash = out - oc + click * clickLevel;
			stepR = orr + click * clickLevel * (1.0 - ctl.u.pickup);
			return out + click * clickLevel * (1.0 - ctl.u.pickup);
		}
		return out + click * clickLevel;
	}

	// ---- rivets (SIZZLE) share one radius, so their sums over the modes are shared: per angular
	// order m, C_m = sum W a q and S_m = sum W b q once, then each rivet reads
	// w(theta_j) = sum_m C_m cos(m theta_j) + S_m sin(m theta_j). Their forces go back the same way.
	std::vector<double> rvA, rvB, rvC, rvS, rvFC, rvFS, rvCos, rvSin;
	std::vector<int> rvM;
	int rvNM = 0;
	bool rvReady = false;
	void rivetTables() {
		rvReady = true;
		if (rivets.empty()) return;
		const Shape& sh = M->sh;
		double r = sh.r0 + rivets[0].r * (sh.a - sh.r0);
		rvA.assign(N, 0.0); rvB.assign(N, 0.0); rvM.assign(N, 0);
		int mMax = 0;
		for (int i = 0; i < N; i++) {
			const Mode& md = M->modes[i];
			double W = sPhi * M->W(i, r), a = md.m * thOff[i];
			if (md.par) { rvA[i] = -W * std::sin(a); rvB[i] = W * std::cos(a); }
			else { rvA[i] = W * std::cos(a); rvB[i] = W * std::sin(a); }
			rvM[i] = md.m;
			mMax = std::max(mMax, md.m);
		}
		rvNM = mMax + 1;
		rvC.assign(rvNM, 0.0); rvS.assign(rvNM, 0.0); rvFC.assign(rvNM, 0.0); rvFS.assign(rvNM, 0.0);
		size_t R = rivets.size();
		rvCos.resize(R * rvNM); rvSin.resize(R * rvNM);
		for (size_t j = 0; j < R; j++) {
			for (int m = 0; m < rvNM; m++) { rvCos[j * rvNM + m] = std::cos(m * rivets[j].th); rvSin[j * rvNM + m] = std::sin(m * rivets[j].th); }
			// the rivet's own compliance
			double C = 0;
			for (int i = 0; i < NA; i++) {
				double ph = rvA[i] * rvCos[j * rvNM + rvM[i]] + rvB[i] * rvSin[j * rvNM + rvM[i]];
				C += ph * ph * g[i];
			}
			rivets[j].C = C;
		}
	}
	void rivetStep() {
		if (!rvReady || (int)rvA.size() != N || rvCos.size() != rivets.size() * (size_t)rvNM) rivetTables();
		std::fill(rvC.begin(), rvC.end(), 0.0);
		std::fill(rvS.begin(), rvS.end(), 0.0);
		{
			const double* Q = q.data();
			const double* A = rvA.data();
			const double* B = rvB.data();
			const int* Mo = rvM.data();
			double* C = rvC.data();
			double* S = rvS.data();
			for (int i = 0; i < NA; i++) { C[Mo[i]] += A[i] * Q[i]; S[Mo[i]] += B[i] * Q[i]; }
		}
		bool any = false;
		for (size_t j = 0; j < rivets.size(); j++) {
			const double* cs = rvCos.data() + j * rvNM;
			const double* sn = rvSin.data() + j * rvNM;
			double ws = 0;
			for (int m = 0; m < rvNM; m++) ws += rvC[m] * cs[m] + rvS[m] * sn[m];
			double F = contactForce(rivets[j], ws);
			if (F > 0) {
				if (!any) { std::fill(rvFC.begin(), rvFC.end(), 0.0); std::fill(rvFS.begin(), rvFS.end(), 0.0); any = true; }
				for (int m = 0; m < rvNM; m++) { rvFC[m] += F * cs[m]; rvFS[m] += F * sn[m]; }
			}
		}
		if (any) {
			double* Q = q.data();
			for (int i = 0; i < NA; i++) Q[i] -= g[i] * (rvA[i] * rvFC[rvM[i]] + rvB[i] * rvFS[rvM[i]]);
		}
	}

	// ---- PUMP: an audio-rate modulation of the tension, on the core (parametric excitation) ----
	bool pumpOn = false;
	double pumpV = 0;                 // current pump (tension units)
	std::vector<double> aPump;
	void pumpCore() {
		int nc = std::min(NC, NA);
		if ((int)aPump.size() != NC) aPump.assign(NC, 1.0);
		for (int i = 0; i < nc; i++) {
			double s = (int)ut.tensS.size() == N ? ut.tensS[i] : (int)M->tensS.size() == N ? M->tensS[i] : 0.0;
			double pf = std::sqrt(std::max(0.05, 1.0 + pumpV * s));
			double w = wS[i] * pf;
			double cs = std::cos(w * k);
			double sg = sig[i];
			double a = 1 + sg * k;
			c1[i] = 2 * cs / a;
			c2[i] = (1 - sg * k) / a;
			g[i] = 2 * (1 - cs) / (w * w) / a;
			aPump[i] = aFac[i] * pf;
		}
	}
	void pumpRelease() {
		for (int i = 0; i < std::min(NC, N); i++) applyLoss(i);
	}

	// ---- RAIN: small random impacts all over the surface ----
	void rainBurst(double v) {
		int n = 40 + (int)(40 * std::min(1.0, v / 5.0));
		for (int j = 0; j < n; j++) rainDrop(v, (uint64_t)(rng.uni() * 0.06 * fs));
	}
	void rainDrop(double v, uint64_t delay) {
		double hd = std::min(1.0, std::max(0.0, ctl.hardness));
		double J = 6e-5 * v * (-std::log(std::max(1e-6, rng.uni())));
		double rr = std::sqrt(0.0025 + 0.95 * rng.uni());   // uniform over the area
		double th = 2 * CY_PI * rng.uni();
		queueGrain(stepCount + delay, J, (300e-6 - 260e-6 * hd) * (0.7 + 0.6 * rng.uni()), rr, th);
	}

	// previous-previous state for the pickup
	std::vector<double> qpp;
	bool exciteDirty = true;
	std::vector<double> phiS;   // EXCITE drive shape
	double excPos = -1;
	uint64_t stepCount = 0;

	// Extended outputs
	struct Extra { float* R = nullptr; float* core = nullptr; float* wash = nullptr; float* env = nullptr; const float* pump = nullptr; };
	bool extraOn = false;
	double stepR = 0, stepCore = 0, stepWash = 0;
	std::vector<double> micW2, micV2;
	struct Chain {
		double dcX = 0, dcY = 0, bz1 = 0, bz2 = 0, lp = 0;
		double run(double x, const double* bq, double lpA) {
			double yv = x - dcX + 0.9995 * dcY;
			dcX = x;
			dcY = yv;
			double ya = bq[0] * yv + bz1;
			bz1 = bq[1] * yv - bq[3] * ya + bz2;
			bz2 = bq[2] * yv - bq[4] * ya;
			lp += lpA * (ya - lp);
			return lp;
		}
	};
	Chain chR, chCore, chWash;
	double envV = 0;
	static double softLimit(double o) {
		double ao = std::fabs(o);
		if (ao > 8.0) o = (o > 0 ? 1 : -1) * (8.0 + 2.0 * std::tanh((ao - 8.0) / 2.0));
		return o;
	}

	// host-rate processing
	void process(const float* excIn, float* out, int n, const Extra* ex = nullptr) {
		if (ex && !extraOn) { extraOn = true; computeMic(); }
		if ((int)qpp.size() != N) qpp.assign(N, 0.0);
		for (int s = 0; s < n; s++) {
			if ((ctlCounter++ & 31) == 0) {
				updateCoefficients();
				setRivets((int)std::lround(ctl.sizzle * 12));
				if (!asleep) cascadeBlock(32.0 / fsHost);
				exciterBlock(32.0 / fsHost);
				bool thermo = thermostatBlock(32.0 / fsHost);
				bool driven = bowOn || !grainQ.empty() || grainFired || thermo || pumpOn;
				if (!asleep) energyGuard((excIn != nullptr && ctl.excite > 0) || driven);
				restBlock(driven);
				mkStep = (mkTarget - mk) / 32.0 * std::min(1.0, 32.0 / (0.05 * fsHost));
				if (ex) {
					envBlock();
					if (std::fabs(ctl.u.width - widthCache) > 1e-3 || std::fabs(ctl.u.pickup - pickupCache) > 1e-3) computeMic();
				}
				grainFired = false;
			}
			double e = excIn ? excIn[s] : 0.0;
			if (ex && ex->pump) {
				pumpV = ex->pump[s] * 0.04;   // +-5 V = +-0.2 of TENSION
				if (pumpV != 0.0) pumpOn = true;
				else if (pumpOn) { pumpOn = false; pumpRelease(); }
			}
			else if (pumpOn) { pumpOn = false; pumpRelease(); }
			if (asleep) {
				if ((e != 0.0 && ctl.excite > 0) || pumpOn) wake();
				else {
					// silent and nothing driving it: no work at all
					statSleep++;
					out[s] = 0.f;
					if (ex) {
						if (ex->R) ex->R[s] = 0.f;
						if (ex->core) ex->core[s] = 0.f;
						if (ex->wash) ex->wash[s] = 0.f;
						if (ex->env) { envV += (envTarget - envV) * (envTarget > envV ? envAtt : envRel); ex->env[s] = (float)envV; }
					}
					continue;
				}
			}
			double acc = 0;
			for (int o = 0; o < OS; o++) {
				double v = step(e);
				if (OS == 1) { acc = v; break; }
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
			// transposed direct form II
			double ya = bq[0] * yv + bz1;
			bz1 = bq[1] * yv - bq[3] * ya + bz2;
			bz2 = bq[2] * yv - bq[4] * ya;
			lp += lpA * (ya - lp);
			double o = lp * outGain * typeTrim();
			if (mk != 1.0 || mkStep != 0.0) { mk += mkStep; o *= mk; }
			if (!std::isfinite(o)) {
				reset();
				o = 0;
				guardEvents++;
			}
			// soft limit above 8 V
			double ao = std::fabs(o);
			if (ao > 8.0) o = (o > 0 ? 1 : -1) * (8.0 + 2.0 * std::tanh((ao - 8.0) / 2.0));
			out[s] = (float)o;
			if (ex) {
				double gsc = outGain * typeTrim() * mk;
				double r = chR.run(stepR, bq, lpA) * gsc;
				double c = chCore.run(stepCore, bq, lpA) * gsc;
				double w = chWash.run(stepWash, bq, lpA) * gsc;
				if (!std::isfinite(r + c + w)) { r = c = w = 0; chR = Chain(); chCore = Chain(); chWash = Chain(); }
				if (ex->R) ex->R[s] = (float)softLimit(r);
				if (ex->core) ex->core[s] = (float)softLimit(c);
				if (ex->wash) ex->wash[s] = (float)softLimit(w);
				if (ex->env) {
					double tgt = envTarget;
					envV += (tgt - envV) * (tgt > envV ? envAtt : envRel);
					ex->env[s] = (float)envV;
				}
			}
		}
	}
	// ENV: a follower of the core's energy, 0..10 V (about 10 V for a hard hit)
	double envTarget = 0, envAtt = 0.05, envRel = 0.0005;
	void envBlock() {
		double Ec = 0;
		for (int i = 0; i < NC && i < NA; i++) {
			double v = (q[i] - qp[i]) * fs;
			Ec += 0.5 * v * v + 0.5 * wS[i] * wS[i] * q[i] * qp[i];
		}
		envTarget = std::min(10.0, 10.0 * std::sqrt(std::max(0.0, Ec) / hitEnergy()));
		envAtt = 1 - std::exp(-1.0 / (0.002 * fsHost));
		envRel = 1 - std::exp(-1.0 / (0.15 * fsHost));
	}

	// per-type output trim (equal loudness across the type table at a medium hit)
	double typeTrim() const {
		static const double T[NUM_TYPES_EXT] = {1.2, 1.0, 1.1, 1.0, 1.0, 0.8, 1.0, 1.15, 1.0, 0.45, 1.6, 2.2, 1.0, 2.5, 0.9, 0.35};
		return ctl.play == BOW ? 6.0 : T[M->gp.type];   // bowing is far quieter than striking
	}

	// ---- brush grains: short half-sine wire impacts, applied as shaped impulses ----
	struct Grain { uint64_t at; double J, T, r, th; };
	std::vector<Grain> grainQ;
	bool grainFired = false;
	double sweepTh = 0;
	void queueGrain(uint64_t at, double J, double T, double r, double th) {
		if (grainQ.size() < 512) grainQ.push_back({at, J, T, r, th});
		wake();
	}
	void fireGrains() {
		for (size_t gi = 0; gi < grainQ.size();) {
			Grain& gr = grainQ[gi];
			if (gr.at > stepCount) { gi++; continue; }
			// impulse J over a half-sine of length T: spectrum cos(pi f T) / (1 - (2 f T)^2)
			double rr = std::min(0.995, std::max(0.0, gr.r));
			for (int i = 0; i < NA; i++) {
				double fT = wS[i] / (2 * CY_PI) * gr.T;
				double x = 2 * fT;
				double H = std::fabs(1 - x * x) < 1e-6 ? CY_PI / 4 : std::cos(CY_PI * fT) / (1 - x * x);
				q[i] += g[i] / k * gr.J * H * phiAt(i, rr, gr.th);
			}
			grainFired = true;
			grainQ[gi] = grainQ.back();
			grainQ.pop_back();
		}
	}
	// a brush tap: 30 to 60 wires over 5 to 25 ms inside a 25 mm patch
	void brushTap(double v, double r0, double th0) {
		double hd = std::min(1.0, std::max(0.0, ctl.hardness));
		int n = 30 + (int)(30 * hd);
		double T = 150e-6 - 110e-6 * hd;
		double spread = 0.025 - 0.02 * hd;
		double Jtot = 0.004 * v;
		double aS = M->sh.a * lam;
		for (int j = 0; j < n; j++) {
			double J = Jtot / n * (-std::log(std::max(1e-6, rng.uni())));
			double dr = 0.0125 * rng.bi() / aS;
			double rr = std::max(0.05, r0 + dr);
			double dth = 0.0125 * rng.bi() / std::max(0.02, rr * aS);
			uint64_t at = stepCount + (uint64_t)(rng.uni() * spread * fs);
			queueGrain(at, J, T * (0.8 + 0.4 * rng.uni()), rr, th0 + dth);
		}
	}

	// ---- bow: smooth friction at the edge, solved implicitly per step ----
	bool bowOn = false;
	double bowTimer = 0, bowFN = 0, bowVr = 0, bowR = -1, bowCB = 0;
	std::vector<double> bowPhi;
	void bowSetup() {
		double r = 0.99 - 0.08 * (1.0 - std::min(1.0, std::max(0.0, ctl.position)));
		bowPhi.resize(N);
		double C = 0;
		// sub-audio rocking / sleeve modes are left out: the bow would just push the cymbal around
		for (int i = 0; i < N; i++) {
			// the hair ribbon (about 15 mm along the edge) averages short waves, and its
			// compliance rolls off what it can drive above a few kHz
			double fi = wS[i] / (2 * CY_PI);
			double x = 0.5 * M->modes[i].m * 0.015 / (M->sh.a * lam);
			double avg = x < 1e-6 ? 1.0 : std::sin(x) / x;
			double hair = 1.0 / (1.0 + (fi / 4000.0) * (fi / 4000.0));
			bowPhi[i] = fi < 40.0 ? 0.0 : phiAt(i, r, 0.0) * avg * hair;
			if (i < NA) C += bowPhi[i] * bowPhi[i] * g[i];
		}
		bowCB = C;
		bowR = r;
	}
	void bowStep() {
		if ((int)bowPhi.size() != N) bowSetup();
		// predicted velocity at the bow point (q holds the predicted new state, qp the old)
		double vp = 0;
		for (int i = 0; i < NA; i++) vp += bowPhi[i] * (q[i] - qp[i]);
		vp /= k;
		double vb = 0.02 * std::min(30.0, std::max(2.0, ctl.rate));
		const double beta = 100.0, s2b = std::sqrt(2 * beta);
		double A = bowCB / k * bowFN;
		double vr = bowVr;
		for (int it = 0; it < 4; it++) {
			double e = std::exp(-beta * vr * vr + 0.5);
			double ph = s2b * vr * e, dph = s2b * e * (1 - 2 * beta * vr * vr);
			double h = vr - vb + vp + A * ph;
			double d = 1 + A * dph;
			if (std::fabs(d) < 0.2) d = d < 0 ? -0.2 : 0.2;
			vr -= h / d;
		}
		bowVr = vr;
		double F = bowFN * s2b * vr * std::exp(-beta * vr * vr + 0.5);
		for (int i = 0; i < NA; i++) q[i] += g[i] * bowPhi[i] * F;
	}

	// ---- swing on the stand: a damped rotation of the cymbal under the mic ----
	double swPsi = 0, swDpsi = 0, swMicPsi = 0;
	void swingKick(double J) { swDpsi += J; }

	// ---- roll / sweep / bow scheduling (control rate) ----
	double rollTimer = 0;
	int rollHand = 0;
	double clickKick = 0;
	void exciterBlock(double dt) {
		int play = ctl.play;
		double rate = std::min(30.0, std::max(2.0, ctl.rate));
		double v = std::max(0.0, ctl.force);
		// roll for the contact beaters
		if (ctl.roll && play != BRUSH && play != BOW && play != RAIN) {
			rollTimer -= dt;
			if (rollTimer <= 0) {
				rollHand ^= 1;
				double pos0 = ctl.position;
				ctl.position = pos0 + (rollHand ? 0.015 : -0.015);
				strikeRaw(v * (1.0 + 0.15 * rng.bi()));
				ctl.position = pos0;
				rollTimer += (1.0 / rate) * (1.0 + 0.08 * rng.bi());
				if (rollTimer < 0) rollTimer = 0;
			}
		}
		else rollTimer = 0;
		// brush sweep: a grain stream moving round the cymbal
		if (ctl.roll && play == BRUSH) {
			double aS = M->sh.a * lam;
			double rr = std::max(0.1, ctl.position);
			sweepTh += 0.05 * rate * dt / std::max(0.02, rr * aS);
			double gps = 300 + 60 * rate;
			int n = (int)(gps * dt + rng.uni());
			double hd = std::min(1.0, std::max(0.0, ctl.hardness));
			for (int j = 0; j < n; j++) {
				double J = 2.5e-5 * v * (-std::log(std::max(1e-6, rng.uni())));
				double dr = 0.0125 * rng.bi() / aS;
				uint64_t at = stepCount + (uint64_t)(rng.uni() * dt * fs);
				queueGrain(at, J, (150e-6 - 110e-6 * hd) * (0.8 + 0.4 * rng.uni()), rr + dr, sweepTh + 0.0125 * rng.bi() / (rr * aS));
			}
		}
		// rain: RATE sets the density (20 .. 2000 drops a second)
		if (ctl.roll && play == RAIN) {
			double dps = 20.0 * std::pow(100.0, (rate - 2.0) / 28.0);
			int n = (int)(dps * dt + rng.uni());
			for (int j = 0; j < n; j++) rainDrop(v, (uint64_t)(rng.uni() * dt * fs));
		}
		// bow: pressure ramps in while bowing (gate or a stroke after STRIKE)
		bool bowing = play == BOW && (ctl.roll || bowTimer > 0);
		if (bowTimer > 0) bowTimer -= dt;
		double target = bowing ? 0.4 + 0.45 * v : 0.0;
		bowFN += (target - bowFN) * std::min(1.0, dt / 0.03);
		if (bowing && !bowOn) { bowSetup(); bowVr = 0.02 * rate; }
		bowOn = bowFN > 1e-3 || bowing;
		if (!bowOn) bowFN = 0;
		// swing: rotation pendulum, 1.5 to 4 Hz, damping from MOUNT
		double mnt = std::min(1.0, std::max(0.0, ctl.mount));
		double wsw = 2 * CY_PI * 2.6 / std::sqrt(lam);
		double zeta = 0.02 + 0.25 * (1 - mnt);
		double acc = -wsw * wsw * swPsi - 2 * zeta * wsw * swDpsi;
		swDpsi += acc * dt;
		swPsi += swDpsi * dt;
		swPsi = std::max(-0.3, std::min(0.3, swPsi));
		if (std::fabs(swPsi - swMicPsi) > 0.002 && (ctlCounter & 127) == 1) {
			swingAng = swPsi;
			swMicPsi = swPsi;
			computeMic();
		}
	}

	// ---- SUSTAIN / SWELL / COLOUR / FREEZE: negative damping under an energy thermostat ----
	bool thermoOn = false, freezeWas = false;
	double freezeE = 0, seedTimer = 0, thermoEt = 0, thermoI = 0;
	double mkStep = 0;
	// energy of a hard hit at the type's usual spot: a stick impulse projected on the modes,
	// with the contact's half-sine spectrum (scaled to match a measured hard CRASH hit)
	double hitE = -1, hitLam = -1, hitKE = 0.1;
	double hitEnergy() {
		if (hitE < 0 || hitLam != lam) {
			double pos = CY_TYPES[std::min(M->gp.type, NUM_TYPES_EXT - 1)].position;
			double J = 0.025 * 8.0, Tc = 0.0006, s = 0;
			for (int i = 0; i < NA; i++) {
				if (frozen[i]) continue;
				double fT = wB[i] / (2 * CY_PI) * Tc, x = 2 * fT;
				double H = std::fabs(1 - x * x) < 1e-6 ? CY_PI / 4 : std::cos(CY_PI * fT) / (1 - x * x);
				double ph = phiAt(i, pos, 0.0);
				s += ph * ph * H * H;
			}
			hitE = std::max(1e-30, 0.5 * J * J * s * hitKE);
			hitLam = lam;
		}
		return hitE;
	}
	bool thermostatBlock(double dt) {
		const Unreal& u = ctl.u;
		bool sus = u.sustain > 0.0;
		bool frz = u.freeze;
		if (!sus && !frz) {
			if (thermoOn) {
				lossMul = 1.0;
				for (int i = 0; i < N; i++) if (sigU[i] != 0.0) { sigU[i] = 0.0; applyLoss(i); }
				for (int i = 0; i < NA; i++) applyLoss(i);
				thermoOn = false;
				thermoI = 0;
			}
			freezeWas = false;
			return false;
		}
		thermoOn = true;
		double E = std::max(0.0, energy() + vnl);   // total: the membrane's share counts too
		double Et, G;
		if (frz) {
			if (!freezeWas) { freezeE = std::max(E, 1e-9 * hitEnergy()); thermoI = 0; }
			freezeWas = true;
			Et = freezeE;
			G = 80.0;
			lossMul = 0.1;
		}
		else {
			freezeWas = false;
			Et = u.sustain * u.sustain * 0.5 * hitEnergy();
			G = std::max(0.5, std::min(30.0, u.swell));
			lossMul = 1.0;
		}
		thermoEt = Et;
		// something has to ring for it to grow: a very soft brush tap when it is silent
		seedTimer -= dt;
		if (E < 1e-7 * Et && seedTimer <= 0) {
			brushTap(0.05, std::max(0.3, ctl.position), 0.0);
			seedTimer = 0.25;
		}
		// proportional-integral: the integral removes the steady offset against the losses
		// (and only runs near the target, so a long swell does not wind it up)
		double err = std::max(-1.0, std::min(1.0, 1.0 - E / std::max(1e-30, Et)));
		if (std::fabs(err) < 0.3) thermoI = std::max(-0.5, std::min(0.8, thermoI + err * dt * (frz ? 10.0 : 3.0)));
		double uu = std::max(-1.0, std::min(1.0, err + thermoI));
		for (int i = 0; i < NA; i++) {
			if (frozen[i]) continue;
			double w = frz ? 1.0 : wCol[i];
			sigU[i] = uu > 0 ? -G * uu * w : G * (-uu);
			applyLoss(i);
		}
		// hard cap
		if (E > 4.0 * Et) {
			double sc = std::sqrt(4.0 * Et / E);
			for (int i = 0; i < NA; i++) { q[i] *= sc; qp[i] *= sc; }
		}
		return true;
	}

	// Energy guard: with nothing driving the shell its (linear) energy can only fall,
	// apart from the small exchange with the nonlinear potential. If it grows by more
	// than that within a control block (explicit stepping beyond its stability limit at
	// high BLOOM), pull the state back. Silent in normal use.
	double guardE = 0;
	void energyGuard(bool driven) {
		bool contact = false;
		for (auto& c : beaters) contact |= c.active;
		contact |= !rivets.empty();   // rivets trade energy with the shell
		double E = energy() + vnl;
		if (!std::isfinite(E)) { reset(); guardEvents++; guardE = 0; return; }
		if (!driven && !contact && guardE > 0 && E > guardE * 1.05) {
			double s = std::sqrt(guardE * 1.05 / E);
			for (int i = 0; i < NA; i++) { q[i] *= s; qp[i] *= s; }
			E = guardE * 1.05;
			guardEvents++;
		}
		// PUMP can grow a mode without limit when BLOOM is low: cap it at a few hard hits
		if (pumpOn && E > 3.0 * hitEnergy()) {
			double sc = std::sqrt(3.0 * hitEnergy() / E);
			for (int i = 0; i < NA; i++) { q[i] *= sc; qp[i] *= sc; }
			E *= sc * sc;
		}
		// under a bent law the measured energy wobbles from block to block (the coupled core sits
		// where the discrete invariant is less exact), so hold the recent peak as the reference:
		// a real runaway still outgrows it within a few blocks
		guardE = unrealFreq ? std::max(E, guardE * 0.995) : E;
	}

	void reset() {
		std::fill(q.begin(), q.end(), 0.0);
		std::fill(qp.begin(), qp.end(), 0.0);
		std::fill(qpp.begin(), qpp.end(), 0.0);
		for (auto& c : beaters) c.active = false;
		rivets.clear();
		dcX = dcY = lp = 0;
		bz1 = bz2 = 0;
		for (auto& r : clk) r[0] = r[1] = 0;
		grainQ.clear();
		bowOn = false; bowFN = 0; bowTimer = 0;
		swPsi = swDpsi = 0;
	}

	// total energy of the modes (kinetic + linear potential), for tests and the guard
	double energy() const {
		double E = 0;
		for (int i = 0; i < NA; i++) {
			// the discrete invariant of the exact-frequency update: v^2 + (2 (1 - cos wk) / k^2) q q'
			// (w^2 q q' drifts for modes high in the band, which a bent law fills)
			double v = (q[i] - qp[i]) * fs;
			double w2 = 2.0 * (1.0 - cw[i]) * fs * fs;
			E += 0.5 * v * v + 0.5 * w2 * q[i] * qp[i];
		}
		return E;
	}
};

} // namespace cy
