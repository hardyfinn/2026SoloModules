// The reference scenes, ported control for control from tools/proto/scenes.h.
#pragma once
#include "../src/dsp/MoltenEngine.hpp"
#include <functional>
#include <vector>
using namespace molten;

static double lerpS(double a, double b, double x) { x = std::max(0.0, std::min(1.0, x)); return a + (b - a) * x; }
static double xlerpS(double a, double b, double x) { x = std::max(0.0, std::min(1.0, x)); return a * std::pow(b / a, x); }
static double segS(double t, double a, double b) { return std::max(0.0, std::min(1.0, (t - a) / (b - a))); }

struct Scene {
	const char* name; double dur; double f1;
	std::function<void(double, Engine&)> ctl;
};

// the reference's base(): everything at its prototype value
static void baseS(Controls& c) {
	c = Controls();
	c.pick = c.amp = c.bright = c.out = 0; c.nu = 4e-5; c.bloom = 1; c.focus = 0.5; c.fc = 262; c.curve = 0.6; c.eta = 1.0;
	c.tune = 1; c.damp = 0; c.gain = 40; c.pos = 0.31; c.decay = 1; c.mallet = 900; c.noise = 1e-4; c.strikeX = 0.23;
	c.heat = 0; c.rate = 3; c.xf = 0.3; c.wf = 2; c.quench = 0; c.latent = 0.5; c.uc = 0; c.cond = 4; c.sag = 0.6; c.tens = 1;
	c.loose = 5; c.liqnl = 400; c.air = 0; c.cast = 1; c.ping = 0.5; c.self = 0; c.slag = 0;
}

static std::vector<Scene> makeScenes() {
	std::vector<Scene> s;
	// 1 HEAT ALONE: one held note at a fixed, modest drive. Only HEAT moves: cold, red-hot, molten, then a quench.
	s.push_back({"heat", 45, 65.4, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.focus = 0.5; c.fc = 196; c.drive = 2500 * std::min(1.0, t / 2) * (t > 41 ? std::max(0.0, 1 - (t - 41) / 2) : 1);
		c.heat = t < 5 ? 0 : t < 17 ? lerpS(0, 1.0, (t - 5) / 12) : t < 27 ? lerpS(1.0, 1.8, (t - 17) / 10) : t < 32 ? 1.8 : 0;
		c.quench = t > 32 ? 0.3 * segS(t, 32, 32.3) : 0; c.rate = t > 32 ? 0.4 : 3; }});
	// 2 CAST: melted, stirred by the loop and quenched four times; each time it sets as a different bell.
	s.push_back({"cast", 40, 55.0, [](double t, Engine& e) { static int k2 = -1; if (t == 0) k2 = -1;
		Controls& c = e.c; baseS(c);
		c.focus = 0.2; c.fc = 147; c.gain = 40; double L = 9.5; int k = (int)(t / L); if (k > 3) k = 3; double x = t - k * L;
		c.heat = x < 3.2 ? 1.85 : 0; c.rate = x < 3.2 ? 6 : 0.5; c.quench = x > 3.2 && x < 6 ? 0.5 : 0; c.uc = 0; c.latent = 0.3;
		c.drive = x < 3.2 ? 5000 : x < 6 ? xlerpS(5000, 900, (x - 3.2) / 2.8) : 900; if (t > 37) c.drive = 0;
		static const double sx[4] = {0.2, 0.55, 0.33, 0.71}; c.pos = sx[k];
		if (x > 6.3 && k != k2) { k2 = k; e.strike(420); } }});
	// 3 FLAME: steady drive, a narrow flame walks along the body and melts it a patch at a time.
	s.push_back({"flame", 42, 65.4, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.focus = 0.25; c.fc = 220; c.gain = 35; c.drive = 3200 * std::min(1.0, t / 2) * (t > 38 ? std::max(0.0, 1 - (t - 38) / 2) : 1);
		c.wf = 0.16; c.cond = 0.6; c.air = 0.9; c.xf = 0.05 + 0.9 * (0.5 - 0.5 * std::cos(2 * PI * t / 26)); c.heat = 2.0 * segS(t, 2, 6); c.rate = 9; c.latent = 0.25; c.ping = 0.2;
		if (t > 34) { c.heat = 0; c.quench = 0.4; } }});
	// 4 PUSHED: hard drive with the amp up, HEAT rocking across the melting point faster and faster, quench at the end.
	s.push_back({"pushed", 34, 49.0, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.focus = 0.4; c.fc = 110; c.gain = 80; c.bright = 1; c.pick = 1; c.amp = 0.6 * segS(t, 2, 12); c.out = 0.6 * segS(t, 10, 20);
		c.drive = xlerpS(3000, 40000, segS(t, 0, 10)); double phs = 0.12 * t + 0.03 * t * t; c.heat = 1.0 + 0.8 * std::sin(2 * PI * phs); c.rate = 5 + t * 0.5;
		c.latent = 0.25; c.loose = 3; c.liqnl = 40; c.ping = 1;
		if (t > 26) { c.heat = 0; c.quench = 0.5; c.drive = xlerpS(40000, 500, segS(t, 26, 30)); } if (t > 33) c.drive = 0; }});
	return s;
}
