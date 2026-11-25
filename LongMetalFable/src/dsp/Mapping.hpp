// Knobs and CVs -> engine, plus the whole module chain without Rack (clocking for PITCH, train placement,
// PASS, strike, audio in, output chain). Shared by the Rack module and the offline tests, so the tests check
// what the panel does. See docs/BUILD-INSTRUCTIONS.md sections 3.9, 3.10, 5 and 6.
#pragma once
#include "LineEngine.hpp"
#include <vector>

namespace longmetal {

static inline double clampd(double x, double a, double b) { return std::min(std::max(x, a), b); }

// Values after knob + CV, each in the knob's own range.
struct Knobs {
	double pitch = 0;        // octaves (PITCH + V/OCT)
	double length = 320;     // cells, 16..320
	double spacing = 9;      // 1..40
	bool heavy = false;      // GAUGE
	double stiff = 0.51, supports = 0.71, endA = 0.31, endB = 0.31;
	double live = 0.225, ground = 0.5, bright = 0.42;
	double listen = 0.12, distance = 0.7, passTime = 30;
	double train = 0.5, speed = 0.49, flats = 0.5;
	double hard = 0.8, inLevel = 0.5;
	double warp = 0, swell = 0, loose = 0, grip = 0;
	double age = 0.35;
	double even = 1.0, width = 1, level = 0.7;
};

// ---- section 5 mappings ----
static inline int cellsFromKnob(double x) { int n = 16 + 8 * (int)std::lround((clampd(x, 16, 320) - 16) / 8.0); return std::min(320, std::max(16, n)); }
static inline double a0FromStiff(double x) { x = clampd(x, 0, 1); return -0.95 * (1 - (1 - x) * (1 - x)); }
static inline double tiesFromKnob(double x) { x = clampd(x, 0, 1); return 1.3 * x * x; }
static inline double endFromKnob(double x) { return 0.98 * clampd(x, -1, 1); }
static inline double gainFromLive(double x) {
	x = clampd(x, 0, 1);
	if (x < 0.5) return 1 - 0.02 * std::pow((0.5 - x) / 0.5, 2.5);
	return 1 + 0.03 * std::pow((x - 0.5) / 0.5, 1.5);
}
static inline double lowcutFromKnob(double x) { return 0.07 * clampd(x, 0, 1); }
static inline double hidampFromKnob(double x) { x = 1 - clampd(x, 0, 1); return 0.012 * x * x; }
static inline double gripFromKnob(double x) { x = clampd(x, 0, 1); return x <= 0 ? 0.0 : 2e-5 * std::pow(1000.0, x); }
static inline double roughFromTrain(double x) { x = 2 * clampd(x, 0, 1); return x * x; }
static inline double wheelHzFromSpeed(double x) { return 2.0 * std::pow(8.0, clampd(x, 0, 1)); }
static inline double strikeAmp(double vel) { return 100.0 * std::pow(clampd(vel, 0, 1), 2.5); }
static inline double strikeMs(double hard) { double x = 1 - clampd(hard, 0, 1); return 0.06 + 0.6 * x * x; }
static inline double inScale(double inLevel) { return 4.0 * inLevel * inLevel / 5.0; }
static inline double pitchClamp(double p) { return clampd(p, -2.5, 1.0); }

// band-limited random walk (AGE thermal drift)
struct Walk {
	uint64_t s = 0x2545F4914F6CDD1DULL; double w = 0, l = 0;
	double u() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (s >> 11) * (1.0 / 9007199254740992.0); }
	void step(double dt, double hz) { double k = 1 - std::exp(-dt * 2 * PI * hz); w += ((2 * u() - 1) * 1.7 - w) * k; l += (w - l) * k; }
};

// 4-point, 3rd-order Hermite between y1 and y2
static inline double hermite(double y0, double y1, double y2, double y3, double t) {
	double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
	return ((c3 * t + c2) * t + c1) * t + y1;
}

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(double fc, double fs) {
		double K = std::tan(PI * clampd(fc / fs, 1e-5, 0.49)), q = 0.70710678, n = 1 / (1 + K / q + K * K);
		b0 = K * K * n; b1 = 2 * b0; b2 = b0; a1 = 2 * (K * K - 1) * n; a2 = (1 - K / q + K * K) * n;
	}
	double process(double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
	void reset() { z1 = z2 = 0; }
};

static const double OUT_SCALE = 100.0;  // LEVEL 0.7 * this * leveller: Init peaks near 5 V (test 18 checks it)
static const double ENV_SCALE = 56.0;   // a live line at its ceiling reads about 8 V

struct Voice {
	Engine* e = nullptr;
	double host = 48000;
	// clocking
	double phase = 0;
	double hist[2][4] = {{0}};
	Biquad lpL, lpR; double lpFe = -1; bool lpOn = false;
	double inPrev = 0;
	// train placement and PASS
	int passState = 0;     // 0 idle, 1 running, 2 fading out, 3 fading in
	double passT = 0, passFrom = 0, passTo = 0, passDur = 30, passSide = 1, passFade = 1;
	double side = 1;
	int lastN = -1;
	bool gripWas = false;
	// output chain
	double hpL = 0, hpR = 0, hpxL = 0, hpxR = 0;
	double levPow = 0;
	std::vector<double> dlyL, dlyR; int dlyW = 0, dlyN = 1;
	double envS = 0;
	double clackHold = 0;
	Walk drift;
	// results of the last host sample
	double outL = 0, outR = 0, env = 0;
	bool clack = false, passGate = false;
	double src = 0;

	Voice() { e = new Engine(); }
	~Voice() { delete e; }

	void init(double hostRate) {
		host = hostRate;
		e->line_init(); e->train_init(8);
		e->trainPos = 1e9; e->trainSpeed = 0;
		phase = 0; std::memset(hist, 0, sizeof hist); lpL.reset(); lpR.reset(); lpFe = -1; inPrev = 0;
		passState = 0; passFade = 1; lastN = -1; gripWas = false;
		hpL = hpR = hpxL = hpxR = 0; levPow = 0; envS = 0; clackHold = 0;
		dlyN = std::max(1, (int)std::lround(0.005 * hostRate));
		dlyL.assign(dlyN, 0.0); dlyR.assign(dlyN, 0.0); dlyW = 0;
	}

	void triggerPass() { if (passState == 0 || passState == 3) { passState = 1; passT = 0; } }

	// Map the knobs into the engine (every host sample: LIVE, DISTANCE, STIFF and SUPPORTS work at audio rate)
	void mapKnobs(const Knobs& k) {
		Params& p = e->p;
		int N = cellsFromKnob(k.length);
		if (lastN >= 0 && N > lastN) {
			// cells that come back into the line start silent
			for (int i = lastN; i < N; i++) {
				for (int q = 0; q < DMAX; q++) { e->dR[q][i] = 0; e->dL[q][i] = 0; }
				for (int m = 0; m < MMAX; m++) { e->sR[m][i] = 0; e->sL[m][i] = 0; }
				e->llR[i] = e->llL[i] = e->lhR[i] = e->lhL[i] = 0; e->envc[i] = 0;
			}
		}
		lastN = N;
		p.N = N;
		p.M = k.heavy ? 4 : 3;
		p.d = (int)clampd(std::lround(k.spacing), 1, 40);
		double age = clampd(k.age, 0, 1);
		p.a0 = a0FromStiff(k.stiff) * (1 + 0.02 * age * drift.l);
		p.ties = tiesFromKnob(k.supports);
		p.rhoL = endFromKnob(k.endA); p.rhoR = endFromKnob(k.endB);
		p.g = gainFromLive(k.live);
		p.lowcut = lowcutFromKnob(k.ground); p.lowf = 500;
		p.hidamp = hidampFromKnob(k.bright); p.hif = 6000;
		p.warp = 0.6 * clampd(k.warp, 0, 1);
		p.swell = clampd(k.swell, 0, 1);
		p.loose = 0.8 * clampd(k.loose, 0, 1);
		double gr = gripFromKnob(k.grip);
		if (gr > 0 && !gripWas) e->grip_reset();
		gripWas = gr > 0;
		p.grip = gr; p.snap = 12; p.gstuck = 0.9;
		p.age = age; p.dead = age; p.worn = age;
		p.pick = 2 + clampd(k.listen, 0, 1) * (N - 5);
		p.sat = 0.6;
		src = p.pick + clampd(k.distance, -1, 1) * (N - 1);
		e->njoint = 3;
		e->joints[0] = (int)std::lround(0.27 * N); e->joints[1] = (int)std::lround(0.47 * N); e->joints[2] = (int)std::lround(0.67 * N);
		double rough = roughFromTrain(k.train) * passFade;
		e->roughLevel = rough;
		e->flatLevel = e->jointLevel = rough * 2 * clampd(k.flats, 0, 1);
		double sp = clampd(k.speed, 0, 1);
		e->wheelHz = wheelHzFromSpeed(sp);
		e->roughDens = 120.0 * (0.3 + 1.43 * sp);
		e->roughBright = 0.15 + 0.4 * sp;
		// which side the train trails on: follows DISTANCE, with a dead band so noise does not flip it
		double dist = clampd(k.distance, -1, 1);
		if (dist > 0.05) side = 1; else if (dist < -0.05) side = -1;
	}

	void placeTrain(const Knobs& k, double dt) {
		Params& p = e->p;
		passGate = false;
		switch (passState) {
		case 0:
			e->trainPos = src; e->trainSide = side; passFade = 1; break;
		case 1: {
			if (passT == 0) {
				passFrom = src; passSide = side; passDur = clampd(k.passTime, 4, 120);
				// to just past the far end of the line (the spec's mirror point is usually off the line,
				// which left the end of a pass silent for a third of PASS TIME; see README)
				passTo = passSide > 0 ? -60.0 : (p.N - 1) + 60.0;
			}
			passT += dt;
			double x = std::min(1.0, passT / passDur);
			e->trainPos = passFrom + (passTo - passFrom) * x; e->trainSide = passSide;
			passGate = true;
			if (x >= 1) { passState = 2; passT = 0; }
		} break;
		case 2:
			passT += dt; passFade = std::max(0.0, 1 - passT);
			if (passT >= 1) {
				passState = 3; passT = 0; passFade = 0;
				e->trainPos = src; e->trainSide = side;
				for (int a = 0; a < e->nax; a++) e->ax[a].lastpos = 1e9;   // the jump back is not a joint crossing
			}
			break;
		case 3:
			passT += dt; passFade = std::min(1.0, passT);
			e->trainPos = src; e->trainSide = side;
			if (passT >= 1) { passState = 0; passFade = 1; }
			break;
		}
	}

	// one host sample. strikeVel < 0: no strike. inVolts: audio in.
	void process(const Knobs& k, double strikeVel, bool passTrig, double inVolts) {
		const double dt = 1.0 / host;
		if (k.age > 0) drift.step(dt, 0.2);
		if (passTrig) triggerPass();
		mapKnobs(k);
		placeTrain(k, dt);
		Params& p = e->p;

		const double fe = SRNOM * std::pow(2.0, pitchClamp(k.pitch));
		e->fe = fe;
		if (strikeVel > 0) e->strike(src, strikeAmp(strikeVel), strikeMs(k.hard));
		// anti-alias when the engine runs faster than the host
		lpOn = fe > host * 1.0001;
		if (lpOn && std::fabs(fe - lpFe) > 1e-6 * fe) { lpL.lowpass(0.45 * host, fe); lpR.lowpass(0.45 * host, fe); lpFe = fe; }

		const double inNow = inVolts * inScale(clampd(k.inLevel, 0, 1));
		const double ratio = fe / host;
		phase += ratio;
		int steps = (int)std::floor(phase);
		phase -= steps;
		bool clackNow = false;
		for (int j = 0; j < steps; j++) {
			double frac = (double)(j + 1) / steps;
			double in = inPrev + (inNow - inPrev) * frac;
			e->train_step();
			if (e->clackEvent) clackNow = true;
			e->strike_step();
			if (in != 0) e->inject(src, in);
			if (p.g > 1.0) e->inject(0.5 * p.N, 2e-5 * e->nrand());
			double l, r; e->line_step(&l, &r);
			if (e->snapEvent) clackNow = true;
			if (lpOn) { l = lpL.process(l); r = lpR.process(r); }
			for (int q = 0; q < 3; q++) { hist[0][q] = hist[0][q + 1]; hist[1][q] = hist[1][q + 1]; }
			hist[0][3] = l; hist[1][3] = r;
		}
		inPrev = inNow;
		// position of this host sample: between hist[1] and hist[2], 'phase' of the way
		double L = hermite(hist[0][0], hist[0][1], hist[0][2], hist[0][3], phase);
		double R = hermite(hist[1][0], hist[1][1], hist[1][2], hist[1][3], phase);

		// output chain at host rate
		const double hpk = std::exp(-2 * PI * 20.0 / host);
		hpL = hpk * (hpL + L - hpxL); hpxL = L;
		hpR = hpk * (hpR + R - hpxR); hpxR = R;
		double mid = 0.5 * (hpL + hpR), side_ = 0.5 * (hpL - hpR) * clampd(k.width, 0, 1);
		double oL = mid + side_, oR = mid - side_;
		// leveller: 5 ms look-ahead, fast attack, 250 ms release on the power of both channels
		double pw = 0.5 * (oL * oL + oR * oR);
		double ka = 1 - std::exp(-dt / 0.005), kr = 1 - std::exp(-dt / 0.25);
		levPow += (pw > levPow ? ka : kr) * (pw - levPow);
		const double ref = 0.05;
		double rms = std::sqrt(levPow);
		double gain = std::pow(std::max(rms, 0.003 * ref) / ref, -0.67 * clampd(k.even, 0, 1));
		double dl = dlyL[dlyW], dr = dlyR[dlyW];
		dlyL[dlyW] = oL; dlyR[dlyW] = oR; dlyW = (dlyW + 1) % dlyN;
		double vg = OUT_SCALE * clampd(k.level, 0, 1) * gain;
		outL = 11.0 * std::tanh(vg * dl / 11.0);
		outR = 11.0 * std::tanh(vg * dr / 11.0);

		// ENV: energy around the listener
		{
			int c = (int)p.pick, a = std::max(0, c - 8), b = std::min(p.N, c + 8); double s = 0;
			for (int i = a; i < b; i++) s += e->envc[i];
			double ev = std::sqrt(std::max(0.0, s / std::max(1, b - a)));
			envS += (ev - envS) * (1 - std::exp(-dt / 0.02));
			env = clampd(ENV_SCALE * envS, 0, 10);
		}
		// CLACK: joint clacks, the lead axle's wheel flat, GRIP releases; at most one per 30 ms
		clackHold = std::max(0.0, clackHold - dt);
		clack = false;
		if (clackNow && clackHold <= 0) { clack = true; clackHold = 0.03; }
	}
};

}  // namespace longmetal
