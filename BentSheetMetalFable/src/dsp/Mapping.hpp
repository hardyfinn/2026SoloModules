// Knobs and CVs -> engine controls. Shared by the Rack module and the offline tests, so the tests check
// what the panel does. See docs/BUILD-INSTRUCTIONS.md sections 5 and 6.
#pragma once
#include "SheetEngine.hpp"

namespace bent {

static const double C3 = 130.8127826;

// Values after knob + CV, each in the knob's own range.
struct Knobs {
	double pitch = 0;      // octaves (PITCH + V/OCT)
	double taper = 0.7, grip = 0.7, decay = 1.0;
	double bend = 0.45;    // -1 .. 1
	double bendVoct = 0;   // volts on BEND V/OCT
	double shape = 1, spot = 0, hold = 0.9, snap = 0, crack = 0.35;
	double dip = 0, depth = 0.5;
	double bow = 0.27, speed = 0.44, bowPos = 0;
	double bowGate = 1;    // 0 .. 1, already slewed (50 ms)
	double hard = 0.5, roll = 0, inLevel = 0.5;
	double stretch = 1, pick = 0.2, out = 0, age = 0.3;
};

static inline double clampd(double x, double a, double b) { return std::min(std::max(x, a), b); }

static inline double handFromKnob(double x) { return 24.0 * x * std::fabs(x); }
static inline double bowPressure(double x) { return 1.6 * std::pow(std::max(x, 0.0), 1.5); }
static inline double bowSpeed(double x) { return 0.05 * std::pow(24.0, x); }   // 0.05 .. 1.2: above about 1.1 a bow cannot start the sheet from rest
static inline double depthFromKnob(double x) { return x >= 0 ? 4.0 * x * x : x; }
static inline double rollRate(double x) { return x > 0.02 ? 4.0 * std::pow(5.0, x) : 0.0; }

// BEND V/OCT: retune the arch pitch sqrt(1 + hand^2) by v octaves.
static inline double handWithVoct(double hand, double v) {
	if (v == 0) return hand;
	double s = hand < 0 ? -1.0 : 1.0;
	double h2 = std::pow(4.0, clampd(v, -10, 10)) * (1 + hand * hand) - 1;
	return h2 > 0 ? s * std::min(std::sqrt(h2), 30.0) : 0.0;
}

// AGE extras that move over time (section 6, the UNTESTED part). Rack and the tests both drive this.
struct Age {
	Rng r;
	double handWalk = 0, handLp = 0, rosinWalk = 0, rosinLp = 0, heat = 0;
	void init(uint64_t seed) { r.s = seed ? seed : 1; for (int i = 0; i < 8; i++) r.u(); handWalk = handLp = rosinWalk = rosinLp = heat = 0; }
	// dt: seconds since the last call; energy: engine energy
	void update(double dt, double energy) {
		// band-limited random walks: tired hand about 6 Hz, rosin about 0.5 Hz
		double kh = 1 - std::exp(-dt * 2 * PI * 6), kr = 1 - std::exp(-dt * 2 * PI * 0.5);
		handWalk += (r.b() * 1.7 - handWalk) * kh; handLp += (handWalk - handLp) * kh;
		rosinWalk += (r.b() * 1.7 - rosinWalk) * kr; rosinLp += (rosinWalk - rosinLp) * kr;
		// warm-up: 20 s, a held roar (energy about 2e4) gives 1
		double target = clampd(std::sqrt(std::max(energy, 0.0) / 2e4), 0, 1.5);
		heat += (target - heat) * (1 - std::exp(-dt / 20.0));
	}
};

// Apply knobs to the engine (call once per BLK engine samples, then call e.block()).
static inline void mapKnobs(const Knobs& k, Engine& e, const Age* age) {
	Controls& c = e.c;
	double a = clampd(k.age, 0, 1);
	double f1 = C3 * std::pow(2.0, clampd(k.pitch, -5, 5));
	if (age) f1 *= 1 - 0.008 * a * age->heat;
	c.f1 = f1;
	c.weMul = std::sqrt(f1 / C3);
	c.aspect = 5;
	double hand = handWithVoct(handFromKnob(clampd(k.bend, -1, 1)), k.bendVoct);
	// tired hand: only while bowing (the bowing hand is the one that tires)
	if (age && k.bow > 0 && k.bowGate > 0) hand *= 1 + 0.03 * a * age->handLp;
	c.hand = hand;
	c.shape = clampd(k.shape, 0, 1);
	c.spot = 0.8 * clampd(k.spot, -1, 1);
	c.grip = clampd(k.grip, 0, 1);
	c.snap = 1.5 * clampd(k.snap, 0, 1);
	c.hold = clampd(k.hold, 0, 1);
	c.crack = 3.0 * k.crack * k.crack;
	c.dip = clampd(k.dip, 0, 1);
	c.depth = depthFromKnob(clampd(k.depth, -1, 1));
	c.taper = clampd(k.taper, 0, 1);
	c.age = a;
	c.nl = clampd(k.stretch, 0, 3);
	double P = bowPressure(clampd(k.bow, 0, 1)) * clampd(k.bowGate, 0, 1);
	if (age) P *= 1 + 0.2 * a * age->rosinLp;
	c.bowP = P;
	c.bowV = bowSpeed(clampd(k.speed, 0, 1));
	c.bowX = clampd(0.5 + 0.5 * c.spot * c.shape + 0.4 * clampd(k.bowPos, -1, 1), 0.04, 0.96);
	c.hard = clampd(k.hard, 0, 1);
	c.wb = 1; c.back = 1; c.kick = 1; c.shake = 0;
	c.pick = clampd(k.pick, 0, 1);
	c.out = clampd(k.out, 0, 1);
	c.decay = clampd(k.decay, 0.25, 4);
	c.fatigue = 0.3 * a;
	c.tiltHand = 0;
}

// Beater roll: self-strikes at rollRate(ROLL) per second, alternating position and strength.
struct Roll {
	double phase = 0; int side = 0;
	void reset() { phase = 0; side = 0; }
	// returns true when a strike should fire now; v and pos are set
	bool step(double dt, double roll, double vel, double& v, double& pos) {
		double r = rollRate(roll);
		if (r <= 0) { phase = 0; return false; }
		phase += r * dt;
		if (phase >= 1) {
			phase -= std::floor(phase);
			side ^= 1;
			v = (side ? 16.0 : 11.0) * vel / 0.7; pos = side ? 0.37 : 0.58;
			return true;
		}
		return false;
	}
};

// Audio in: volts -> force
static inline double inForce(double volts, double inLevel) { return 3000.0 * inLevel * inLevel * volts / 5.0; }

// The whole module chain without Rack: oversampling, control blocks, AGE, roll, decimation, leveller,
// level and the final soft clip. The Rack module and the preset tests both use this.
static const double OUT_VOLTS = 7.2;   // LEVEL 0.7 * this * leveller gain: Init is about 5 V peak (test 15)
struct Voice {
	Engine* e = nullptr;
	Age age; Roll roll; Decimator dec; Leveller lev;
	double host = 0; bool os = true; int ctr = 0;
	uint64_t seed = 0x9E3779B97F4A7C15ULL;
	double outL = 0, outR = 0, env = 0, bendOut = 0; int snapTrig = 0;
	double envS = 0;
	Voice() { e = new Engine(); }
	~Voice() { delete e; }
	void init(double hostRate) {
		host = hostRate; os = hostRate < 88200.0;
		e->setRate(os ? 2 * hostRate : hostRate); e->build();
		age.init(seed); roll.reset(); dec.reset(); lev.setRate(hostRate); lev.reset();
		ctr = 0; envS = 0;
	}
	// one host sample. strikeVel < 0: no strike. inVolts: audio in. rollVel: VEL for the beater roll
	// (< 0: the strike's velocity, or 0.7 without one).
	void process(const Knobs& k, double strikeVel, double inVolts, double level, double rollVel = -1) {
		const int ctrlLen = BLK / (os ? 2 : 1);
		if (ctr == 0) {
			age.update(BLK / e->fe, e->energy);
			mapKnobs(k, *e, k.age > 0 ? &age : nullptr);
			e->block();
		}
		if (++ctr >= ctrlLen) ctr = 0;
		if (strikeVel > 0) e->strike(60.0 * strikeVel, 0.37);
		double rv, rp;
		if (roll.step(1.0 / host, k.roll, rollVel >= 0 ? rollVel : (strikeVel > 0 ? strikeVel : 0.7), rv, rp)) e->strike(rv, rp);
		e->inForce = inForce(inVolts, k.inLevel); e->inPos = 0.37;
		double l2[2], r2[2]; int snap = 0;
		const int nsub = os ? 2 : 1;
		for (int s = 0; s < nsub; s++) { e->process(); l2[s] = e->outL; r2[s] = e->outR; if (e->snapEvent) snap = 1; }
		double l, r;
		if (os) dec.process(l2, r2, l, r); else { l = l2[0]; r = r2[0]; }
		double g = lev.gain(l, r) * level * OUT_VOLTS;
		outL = 11.0 * std::tanh(l * g / 11.0); outR = 11.0 * std::tanh(r * g / 11.0);
		snapTrig = snap;
		bendOut = clampd(10.0 * e->bend / 24.0, -10, 10);
		double en = clampd(std::sqrt(std::max(e->energy, 0.0) / 2e4), 0, 1);
		envS += (en - envS) * (1 - std::exp(-1.0 / (0.02 * host)));
		env = 10.0 * envS;
	}
};

} // namespace bent
