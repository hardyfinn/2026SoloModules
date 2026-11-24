// Knobs and CVs -> engine controls. Shared by the Rack module and the offline tests, so the tests
// check what the panel actually does. See docs/BUILD-INSTRUCTIONS.md section 5 and 6.
#pragma once
#include "PlateEngine.hpp"

namespace plate {

static const double C4 = 261.6255653;

// Values after knob + CV, each in the knob's own range.
struct Knobs {
	int type = SHEET;
	int cast = 0;
	double size = 65.4;      // Hz, 20 .. 200
	double thick = 0.5;      // 0 .. 1
	double curve = 0.6;      // 0 .. 1
	double stretch = 1.0;    // 0 .. 2
	double decay = 1.0;      // 0.15 .. 4
	double drive = 0.35;     // 0 .. 1
	double gain = 60.0;      // 20 .. 600
	double focus = 0.5;      // 0 .. 1
	double pitch = 0.0;      // octaves about C4 (PITCH + V/OCT)
	bool trackPlate = true;  // TRACK switch: PLATE or FOCUS
	double phase = 0.0;      // 0 .. 1 -> 0 .. 4 ms
	double driverPos = 0.31; // 0.05 .. 0.95
	double spread = 0.0;     // 0 .. 0.5
	double grab = 0.0;       // 0 .. 1
	double release = 0.0;    // 0 .. 1
	double gate = 1.0;       // 0 .. 1, already slewed (4 ms)
	double pick = 0.2, amp = 0.2, drv = 0.1, rat = 0.0, bleed = 0.1, out = 0.0;
	double pump = 0.0, rate = 0.3;
	double mallet = 0.5;     // 0 .. 1
	double inLevel = 0.5;    // 0 .. 1
	double bright = 0.5;     // 0 .. 1
	double age = 0.3;        // 0 .. 1
};

static inline double clampd(double x, double a, double b) { return std::min(std::max(x, a), b); }

// DRIVE knob -> force scale before pitch scaling.
static inline double driveForce(double k) { return k <= 0.0 ? 0.0 : 150.0 * std::pow(600.0, clampd(k, 0.0, 1.0)); }

// The force needed to reach the splitting amplitude grows with the sung pitch. Fitted to the force at
// which a SHEET plate first splits (focus 0.5), C1 to C6: nearly flat below C4, steep above
// (higher modes lose energy faster and need more motion for the same stretch).
static inline double driveScale(double fs) {
	double x = fs / C4;
	if (x <= 1.0) return x;
	x = std::min(x, 4.0);   // capped at C6: above that the plate's modes are past the top of the scale anyway
	return x * x * (1 + C4 * x / 700.0) / (1 + C4 / 700.0);
}

// AGE state: slow wander, dents and warm-up. One per module instance.
struct Age {
	Rng g;
	double walk = 0, walkV = 0;
	double heat = 0;
	double u[N] = {};
	uint64_t seed = 1;
	int dentType = -1;
	double dentAmt = 0;
	void init(uint64_t s) {
		seed = s; g.seed(s * 0x9E3779B97F4A7C15ULL + 1);
		for (int j = 0; j < N; j++) u[j] = g.bi();
		walk = walkV = 0; heat = 0;
	}
	// dt = time since last call (control rate)
	void update(double dt, double energy) {
		// band-limited random walk, about 0.3 Hz, roughly unit spread
		double k = 1 - std::exp(-dt * 2 * PI * 0.3);
		walkV += k * (g.bi() * 1.7 - walkV);
		walk += k * (walkV - walk);
		walk = clampd(walk, -1, 1);
		double target = std::min(1.0, energy / 2e6);
		heat += (target - heat) * (1 - std::exp(-dt / 20.0));
	}
};

// Hum amplitude at AGE 1 (pickup units), about -38 dB under a pure tone at DRIVE 0.4 (set by test).
static const double HUM_AT_1 = 6.3;

// Raw engine units -> volts after the leveller: a pure tone at DRIVE 0.4 (C4, default knobs) at LEVEL 0.7 is 5 V peak.
static const double OUT_VOLTS = 5.0 / (0.7 * 907.0);

struct Mapped {
	double fs = C4;
	double f1 = 65.4;
};

// Fill the engine controls. Rebuilds the mode table when TYPE or CAST change.
static inline Mapped mapKnobs(const Knobs& k, Engine& e, Age* age) {
	if (k.type != e.type || k.cast != e.cast) {
		e.build(k.type, k.cast);
		if (age) age->dentType = -1;
	}
	Controls& c = e.c;
	Mapped m;
	const double a = age ? clampd(k.age, 0.0, 1.0) : 0.0;
	m.fs = C4 * std::pow(2.0, clampd(k.pitch, -3.0, 3.0));
	m.f1 = clampd(k.size, 10.0, 400.0);
	e.f1 = m.f1;
	c.posD = clampd(k.driverPos, 0.05, 0.95);
	double xp = c.posD + clampd(k.spread, 0, 0.5);
	if (xp > 0.95) xp = 0.05 + (xp - 0.95);
	c.posP = clampd(xp, 0.05, 0.95);
	if (k.trackPlate) {
		int an = e.anchor(C4 / m.f1, c.posD, c.posP);
		c.tune = m.fs / (m.f1 * e.r[an]);
	} else {
		c.tune = 1.0;
	}
	c.fc = m.fs;
	c.focus = clampd(k.focus, 0, 1);
	c.curve = clampd(k.curve, 0, 1);
	c.stretch = clampd(k.stretch, 0, 2);
	c.bloom = std::pow(4.0, 1 - 2 * clampd(k.thick, 0, 1));
	c.decay = clampd(k.decay, 0.15, 4.0);
	c.drive = driveForce(k.drive) * driveScale(m.fs) * clampd(k.gate, 0, 1);
	c.gain = clampd(k.gain, 20, 600);
	c.delay = clampd(k.phase, 0, 1) * 0.004;
	c.damp = 8 * clampd(k.grab, 0, 1) + (k.gate < 0.5 ? 8 * clampd(k.release, 0, 1) : 0.0);
	c.pick = clampd(k.pick, 0, 1); c.amp = clampd(k.amp, 0, 1); c.drv = clampd(k.drv, 0, 1);
	c.rat = clampd(0.78 * k.rat + 0.3 * a * a, 0, 0.85);   // above about 0.8 the head barely touches and the loop dies
	c.bleed = clampd(k.bleed, 0, 1); c.out = clampd(k.out, 0, 1);
	c.pump = clampd(k.pump, 0, 1); c.rate = clampd(k.rate, 0, 1);
	c.mallet = 300.0 * std::pow(10.0, clampd(k.mallet, 0, 1));
	c.inLevel = clampd(k.inLevel, 0, 1) * 600.0 / 5.0;
	c.bright = clampd(k.bright, 0, 1);
	c.nu = 4e-5; c.eta = 1.0;
	c.noise = 1e-4 * (1 + 400 * a * a);
	c.hum = HUM_AT_1 * a;
	c.ampBias = 0.2 * a;
	if (age && a > 0) {
		c.gain *= 1 + 0.25 * a * age->walk;
		c.tune *= 1 - 0.012 * a * age->heat;
		c.decay *= 1 - 0.3 * a * age->heat;
		if (std::fabs(a - age->dentAmt) > 1e-4 || age->dentType != e.type) {
			for (int j = 0; j < N; j++) e.detune[j] = 1 + 0.015 * a * age->u[j];
			age->dentAmt = a; age->dentType = e.type; e.coefDirty = true;
		}
	} else if (e.detune[0] != 1.0 || (age && age->dentAmt != 0)) {
		for (int j = 0; j < N; j++) e.detune[j] = 1.0;
		if (age) age->dentAmt = 0;
		e.coefDirty = true;
	}
	return m;
}

} // namespace plate
