// Knobs and CVs -> engine controls. Shared by the Rack module and the offline tests, so the tests
// check what the panel actually does. See docs/BUILD-INSTRUCTIONS.md sections 4 and 5.
#pragma once
#include "MoltenEngine.hpp"

namespace molten {

static const double C4 = 261.6255653;

// Values after knob + CV, each in the knob's own range.
struct Knobs {
	// HEAT
	double heat = 0.0;       // 0 .. 1 (0.5 = melting point)
	double mass = 0.3;       // 0 .. 1
	double flame = 0.3;      // 0 .. 1
	double width = 1.0;      // 0 .. 1
	double quenchAmt = 0.4;  // 0 .. 1
	bool quenching = false;  // button or gate held
	double latent = 0.45;    // 0 .. 1
	double supercool = 0.0;  // 0 .. 1
	double self = 0.0;       // 0 .. 1
	// METAL
	double tension = 0.0;    // octaves, -2 .. +1
	double sag = 0.67;       // 0 .. 1
	double loose = 0.625;    // 0 .. 1
	double cast = 0.5;       // 0 .. 1
	double pings = 0.3;      // 0 .. 1
	int body = 0;            // 0 .. 15
	double size = 65.4;      // Hz, 20 .. 200
	double thick = 0.5;      // 0 .. 1
	double curve = 0.6;      // 0 .. 1
	double decay = 1.0;      // 0.15 .. 4
	// LOOP
	double drive = 0.35;     // 0 .. 1
	double gain = 60.0;      // 20 .. 600
	double focus = 0.5;      // 0 .. 1
	double pitch = 0.0;      // octaves about C4 (PITCH + V/OCT)
	bool trackPlate = true;  // TRACK switch: PLATE or FOCUS
	// TOUCH
	double pos = 0.31;       // 0.05 .. 0.95
	double grab = 0.0;       // 0 .. 1
	double release = 0.0;    // 0 .. 1
	double gate = 1.0;       // 0 .. 1, already slewed (4 ms)
	// SAT
	double pick = 0.2, amp = 0.2, out = 0.0;
	// EXTRA
	double mallet = 0.5;     // 0 .. 1
	double inLevel = 0.5;    // 0 .. 1
	double bright = 0.5;     // 0 .. 1
	double age = 0.3;        // 0 .. 1
};

static inline double clampd(double x, double a, double b) { return std::min(std::max(x, a), b); }

// DRIVE knob -> force scale before pitch scaling (as Plate Driver).
static inline double driveForce(double k) { return k <= 0.0 ? 0.0 : 150.0 * std::pow(600.0, clampd(k, 0.0, 1.0)); }

// Plate Driver's drive scaling with the sung pitch (its test 6 fitted it on the same sheet).
static inline double driveScale(double fs) {
	double x = fs / C4;
	if (x <= 1.0) return x;
	x = std::min(x, 4.0);
	return x * x * (1 + C4 * x / 700.0) / (1 + C4 / 700.0);
}

// AGE state: slow wander, warm-up, flickering flame, and the fixed unevenness of this piece of metal.
struct Age {
	Rng g;
	double walk = 0, walkV = 0;
	double warm = 0;
	double flick = 0, flickV = 0;
	double u[N] = {};
	double zu[K] = {}, zv[K] = {}, zs[K] = {};
	uint64_t seed = 1;
	int zonesBody = -1;
	double zonesAmt = -1;
	void init(uint64_t s) {
		seed = s; g.seed(s * 0x9E3779B97F4A7C15ULL + 1);
		for (int j = 0; j < N; j++) u[j] = g.bi();
		for (int k = 0; k < K; k++) { zu[k] = g.bi(); zv[k] = g.bi(); zs[k] = g.uni(); }
		walk = walkV = 0; warm = 0; flick = flickV = 0; zonesBody = -1; zonesAmt = -1;
	}
	void update(double dt, double energy) {
		double k = 1 - std::exp(-dt * 2 * PI * 0.3);          // about 0.3 Hz random walk
		walkV += k * (g.bi() * 1.7 - walkV);
		walk += k * (walkV - walk);
		walk = clampd(walk, -1, 1);
		double kf = 1 - std::exp(-dt * 2 * PI * 3.0);         // flame flicker, noise low-passed at 3 Hz
		flickV += kf * (g.bi() * 1.7 - flickV);
		flick += kf * (flickV - flick);
		double target = std::min(1.0, energy / 2e6);
		warm += (target - warm) * (1 - std::exp(-dt / 20.0));
	}
};

static const double HUM_AT_1 = 6.3;          // as Plate Driver: about -38 dB under a pure tone at DRIVE 0.4
// Raw engine units -> volts after the leveller: a pure tone at DRIVE 0.4 (C4, default knobs, cold) at LEVEL 0.7 is 5 V peak.
static const double OUT_VOLTS = 5.0 / (0.7 * 1378.0);

struct Mapped { double fs = C4; double f1 = 65.4; };

// Fill the engine controls. Rebuilds the mode table when BODY changes.
static inline Mapped mapKnobs(const Knobs& k, Engine& e, Age* age) {
	if (k.body != e.body) { e.build(k.body); if (age) age->zonesBody = -1; }
	Controls& c = e.c;
	Mapped m;
	const double a = age ? clampd(k.age, 0.0, 1.0) : 0.0;
	m.fs = C4 * std::pow(2.0, clampd(k.pitch, -3.0, 3.0));
	m.f1 = clampd(k.size, 10.0, 400.0);
	e.f1 = m.f1;
	c.pos = clampd(k.pos, 0.05, 0.95);
	if (k.trackPlate) {
		int an = e.anchor(C4 / m.f1, c.pos);
		c.tune = m.fs / (m.f1 * e.r[an]);
	} else {
		c.tune = 1.0;
	}
	c.fc = m.fs;
	c.focus = clampd(k.focus, 0, 1);
	c.curve = clampd(k.curve, 0, 1);
	c.bloom = std::pow(4.0, 1 - 2 * clampd(k.thick, 0, 1));
	c.decay = clampd(k.decay, 0.15, 4.0);
	c.drive = driveForce(k.drive) * driveScale(m.fs) * clampd(k.gate, 0, 1);
	c.gain = clampd(k.gain, 20, 600);
	c.damp = 8 * clampd(k.grab, 0, 1) + (k.gate < 0.5 ? 8 * clampd(k.release, 0, 1) : 0.0);
	c.pick = clampd(k.pick, 0, 1); c.amp = clampd(k.amp, 0, 1); c.out = clampd(k.out, 0, 1);
	c.mallet = 300.0 * std::pow(10.0, clampd(k.mallet, 0, 1));
	c.inLevel = clampd(k.inLevel, 0, 1) * 600.0 / 5.0;
	c.bright = clampd(k.bright, 0, 1);
	c.nu = 4e-5; c.eta = 1.0;
	c.strikeX = 0.23;
	// ---- heat
	const double W = clampd(k.width, 0, 1);
	c.heat = k.quenching ? 0.0 : 2.0 * clampd(k.heat, 0, 1);
	c.quench = k.quenching ? clampd(k.quenchAmt, 0, 1) : 0.0;
	c.rate = 12.0 * std::pow(0.0125, clampd(k.mass, 0, 1));
	c.xf = 0.05 + 0.9 * clampd(k.flame, 0, 1);
	c.wf = 0.08 * std::pow(25.0, W);
	c.cond = 0.6 + 3.4 * W * W;
	c.air = 0.9 * (1 - W) * (1 - W);
	c.latent = 0.1 + 0.9 * clampd(k.latent, 0, 1);
	c.uc = 0.6 * clampd(k.supercool, 0, 1);
	c.self = clampd(k.self, 0, 1);
	c.tens = std::pow(2.0, clampd(k.tension, -2, 1));
	c.sag = 0.9 * clampd(k.sag, 0, 1);
	c.loose = 8.0 * clampd(k.loose, 0, 1);
	c.liqnl = 4.0 * std::pow(10.0, 3.2 * clampd(k.loose, 0, 1));
	c.cast = 2.0 * clampd(k.cast, 0, 1);
	c.ping = clampd(k.pings, 0, 1);
	// ---- AGE
	c.noise = 1e-4 * (1 + 400 * a * a);
	c.hum = HUM_AT_1 * a;
	c.ampBias = 0.2 * a;
	c.slag = a;
	if (age && a > 0) {
		c.gain *= 1 + 0.25 * a * age->walk;
		c.tune *= 1 - 0.012 * a * age->warm;
		c.decay *= 1 - 0.3 * a * age->warm;
		c.heat *= 1 + 0.08 * a * age->flick;
		if (std::fabs(a - age->zonesAmt) > 1e-4 || age->zonesBody != e.body) {
			for (int j = 0; j < N; j++) e.detune[j] = 1 + 0.015 * a * age->u[j];
			e.setZones(age->zu, age->zv, age->zs, a);
			age->zonesAmt = a; age->zonesBody = e.body;
		}
	} else if (e.detune[0] != 1.0 || (age && age->zonesAmt != 0)) {
		double z[K] = {}, o[K] = {};
		for (int j = 0; j < N; j++) e.detune[j] = 1.0;
		e.setZones(z, z, o, 0.0);
		if (age) age->zonesAmt = 0;
	}
	return m;
}

} // namespace molten
