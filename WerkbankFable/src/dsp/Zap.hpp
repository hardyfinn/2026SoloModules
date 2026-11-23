// ZAP: synth percussion. Port of zap_render in tools/proto/zap_blech_ref.py.
#pragma once
#include "Common.hpp"

namespace wb {

inline double zapK(double res) {
	if (res <= 0.9) return 1.4 * std::pow(10.0, -3.0 * res / 0.9);
	return 0.0014 + (res - 0.9) / 0.1 * (-0.02 - 0.0014);
}

struct ZapParams {
	double startHz = 4000, endHz = 100, sweep = 0.03, res = 0.8, decay = 0.4, noise = 0, click = 1, drive = 0, age = 0.3;
};

struct ZapVoice {
	double fs = 48000;
	double U[4] = {};
	XS32 rng;
	Drift drift;
	double ic1 = 0, ic2 = 0, pe = 0, ae = 0, ne = 0, v1 = 0;
	double jit = 1, lvl = 1;

	void init(double sampleRate, uint64_t unitSeed, uint32_t noiseSeed) {
		fs = sampleRate;
		unitValues(unitSeed, U, 4);
		rng.seed(noiseSeed);
		drift.init(fs);
		ic1 = ic2 = pe = ae = ne = v1 = 0; jit = lvl = 1;
	}
	// trig > 0: a hit of that value this sample. pitch: V/OCT in octaves.
	double process(double trig, double pitch, const ZapParams& p) {
		const double age = p.age;
		double r = rng.next();
		double dr = drift.tick(r);
		if (trig > 0.0) {
			double r1 = rng.next(), r2 = rng.next();
			jit = 1.0 + age * 0.012 * r1;
			lvl = std::pow(10.0, age * 1.2 * r2 / 20.0);
			pe = 1.0; ae = trig * lvl; ne = trig * lvl;
			ic1 += 2.0 * p.click * trig * lvl;
		}
		const double cp = std::exp(-1.0 / (std::max(p.sweep, 1e-4) * fs));
		const double ca = p.decay >= 8.0 ? 1.0 : std::exp(-6.908 / (p.decay * fs));
		const double cn = std::exp(-6.908 / (std::min(p.decay, 0.25) * fs));
		const double tune = 1.0 + age * 0.02 * U[0];
		const double lr = std::log(p.startHz / p.endHz);
		const double dg = 1.0 + 3.0 * p.drive, dn = std::tanh(dg);
		const double hiss = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 2.0e-4;
		const double ns = std::sqrt(fs / 48000.0);   // same noise level per Hz at any rate
		double f = p.endHz * std::exp(lr * pe) * std::exp2(pitch) * tune * jit * (1.0 + age * 0.004 * dr);
		f = clampd(f, 10.0, 0.45 * fs);
		pe *= cp;
		double g = std::tan(PI * f / fs);
		double k = zapK(p.res) + 0.05 * v1 * v1;
		double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
		double x = 1.5 * p.noise * ne * r * ns;
		double v3 = x - ic2; v1 = a1 * ic1 + a2 * v3; double v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
		double y = v1 * ae;
		ae *= ca; ne *= cn;
		double r3 = rng.next();
		return std::tanh(dg * y) / dn + hiss * r3 * ns;
	}
};

}  // namespace wb
