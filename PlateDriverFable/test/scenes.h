// The reference scenes, ported control for control from tools/proto/plate_driver_ref.c.
#pragma once
#include "../src/dsp/PlateEngine.hpp"
#include <functional>
using namespace plate;

static double lerpS(double a, double b, double x) { x = std::max(0.0, std::min(1.0, x)); return a + (b - a) * x; }
static double xlerpS(double a, double b, double x) { x = std::max(0.0, std::min(1.0, x)); return a * std::pow(b / a, x); }
static double segS(double t, double a, double b) { return std::max(0.0, std::min(1.0, (t - a) / (b - a))); }
static double slewS(double* st, double target, double rate) { *st += (target - *st) * rate; return *st; }

struct Scene {
	const char* name; double dur; double f1;
	std::function<void(double, Engine&)> ctl;
};

static void baseS(Controls& c) {
	c = Controls();
	c.pick = c.amp = c.drv = c.rat = c.pump = c.bright = c.bleed = c.out = 0; c.rate = 0.3; c.nu = 0; c.stretch = 1; c.bloom = 1;
	c.focus = 0.5; c.fc = 262; c.curve = 0.6; c.eta = 1.0; c.tune = 1; c.damp = 0; c.delay = 0; c.gain = 40; c.posD = c.posP = 0.31;
	c.decay = 1; c.mallet = 900; c.noise = 1e-4; c.ratDamp = 12;   // the reference's rattle head
}

static std::vector<Scene> makeScenes() {
	std::vector<Scene> s;
	s.push_back({"swell", 40, 65.4, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.drive = t < 20 ? xlerpS(250, 60000, t / 20) : (t < 24 ? 60000 : xlerpS(60000, 150, (t - 24) / 14)); }});
	s.push_back({"melody", 27, 65.4, [](double t, Engine& e) { static double stT = 1, stD = 0; if (t == 0) { stT = 1; stD = 0; }
		Controls& c = e.c; baseS(c);
		static const int nt[14] = {0, 3, 7, 5, 3, 10, 7, 12, 10, 7, 3, 5, 0, 0}; double L = 1.7; int k = (int)(t / L); if (k > 13) k = 13; double x = t / L - k;
		slewS(&stT, std::pow(2, nt[k] / 12.0), 0.02); c.tune = stT; c.fc = 262 * c.tune; c.focus = 0.8;
		double acc = (k == 5 || k == 7 || k == 12) ? 1.0 : 0.0; double pk = acc ? 9000 : 2600; double d = 600 + (pk - 600) * std::sin(PI * std::min(1.0, x * 1.15)); if (t > 23.4) d = 0;
		c.drive = slewS(&stD, d, 0.01); }});
	s.push_back({"overblow", 34, 49.0, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.focus = 1.0; c.fc = xlerpS(90, 2400, t / 30); c.drive = xlerpS(1500, 7000, t / 30); c.gain = 60; if (t > 30) c.drive = 0; }});
	s.push_back({"infinite", 36, 55.0, [](double t, Engine& e) { static int st = 0; if (t == 0) st = 0; Controls& c = e.c; baseS(c);
		c.focus = 0.0; c.fc = 200; c.gain = 25;
		if (!st && t >= 1.0) { st = 1; e.strike(420); }
		c.drive = t < 5 ? 0 : t < 9 ? xlerpS(300, 3000, (t - 5) / 4) : t < 20 ? xlerpS(3000, 45000, (t - 9) / 11) : t < 25 ? 45000 : 0; }});
	s.push_back({"drone", 46, 27.5, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.focus = 0.35; c.fc = 110; c.gain = 40; c.posD = c.posP = 0.31 + 0.12 * std::sin(2 * PI * t / 37);
		double l = 0.5 - 0.5 * std::cos(2 * PI * t / 11.0); c.drive = xlerpS(900, 38000, l * l) * std::min(1.0, t / 3) * (t > 41 ? std::max(0.0, 1 - (t - 41) / 1.5) : 1); }});
	s.push_back({"position", 32, 82.4, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.focus = 0.15; c.fc = 330; c.gain = 30; c.drive = 2600 * (t > 27 ? 0 : 1); c.posD = c.posP = 0.12 + 0.7 * t / 28.0; }});
	s.push_back({"pulse", 10, 49.0, [](double t, Engine& e) { static int last = -1; if (t == 0) last = -1; Controls& c = e.c; baseS(c);
		static const int nt[16] = {0, 0, 12, 0, 3, 0, 10, 0, 0, 7, 0, 12, 0, 3, 5, 0}; static const int ac[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
		double st = 60.0 / 125 / 4; int k = (int)(t / st); double x = t / st - k; int s_ = k % 16; c.tune = std::pow(2, nt[s_] / 12.0); c.fc = 196 * c.tune; c.focus = 0.7; c.gain = 500;
		double open = std::min(1.0, t / (st * 48)); int on = x < 0.55 && t < st * 64;
		c.drive = on ? (ac[s_] ? xlerpS(9000, 30000, open) : xlerpS(2500, 6000, open)) : 0; c.damp = on ? 0 : xlerpS(8, 0.5, open);
		if (k != last && t < st * 64) { last = k; e.strike(ac[s_] ? 260 : 120); } }});
	s.push_back({"h-stages", 52, 49.0, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.nu = 4e-5; c.focus = 0.6; c.fc = 196; c.drive = 2600 * std::min(1.0, t / 1.5); c.bright = 1;
		c.pick = std::sin(PI * segS(t, 4, 11)); c.amp = std::sin(PI * segS(t, 11, 18)); c.drv = std::sin(PI * segS(t, 18, 25));
		c.bleed = 0.6 * (std::sin(PI * segS(t, 18, 25)) + std::sin(PI * segS(t, 25, 32))); c.rat = 0.95 * std::sin(PI * segS(t, 25, 32)); c.out = std::sin(PI * segS(t, 32, 39));
		if (t > 39) { double z = segS(t, 39, 46); c.pick = c.amp = c.drv = c.out = z; c.rat = 0.9 * z; c.bleed = 0.6 * z; } if (t > 48) c.drive = 0; }});
	s.push_back({"h-pump", 36, 36.7, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.nu = 4e-5; c.focus = 0.3; c.fc = 147; c.drive = 16000 * std::min(1.0, t / 2); c.bright = 0.4; c.amp = 0.3; c.pump = std::min(1.0, t / 6); c.rate = segS(t, 6, 30); if (t > 32) c.drive = 0; }});
	s.push_back({"h-max", 36, 55.0, [](double t, Engine& e) { Controls& c = e.c; baseS(c);
		c.nu = 6e-5; c.bloom = 2.0; c.bright = 1; c.focus = 0.5; c.fc = xlerpS(110, 880, 0.5 - 0.5 * std::cos(2 * PI * t / 17)); c.gain = 80;
		c.drive = xlerpS(3000, 90000, segS(t, 0, 12)); c.pick = 1; c.amp = segS(t, 2, 14); c.drv = segS(t, 8, 20); c.rat = 0.85 * segS(t, 14, 24); c.pump = 0.6 * segS(t, 20, 28); c.rate = 0.8;
		c.bleed = 0.6 * segS(t, 8, 20); c.out = segS(t, 22, 30); if (t > 31) c.drive = 0; }});
	s.push_back({"h-lead", 27, 65.4, [](double t, Engine& e) { static double stT = 1, stD = 0; if (t == 0) { stT = 1; stD = 0; } Controls& c = e.c; baseS(c);
		c.nu = 4e-5; static const int nt[14] = {0, 3, 7, 5, 3, 10, 7, 12, 10, 7, 3, 5, 0, 0}; double L = 1.7; int k = (int)(t / L); if (k > 13) k = 13; double x = t / L - k;
		slewS(&stT, std::pow(2, nt[k] / 12.0), 0.02); c.tune = stT; c.fc = 262 * c.tune; c.focus = 0.8; c.bright = 0.8; c.pick = 1.0; c.amp = 0.75; c.gain = 120; c.bleed = 0.35; c.out = 0.45;
		double acc = (k == 5 || k == 7 || k == 12) ? 1.0 : 0.0; c.drv = acc ? 0.7 : 0.15; double pk = acc ? 14000 : 5000; double d = 1500 + (pk - 1500) * std::sin(PI * std::min(1.0, x * 1.15)); if (t > 23.4) d = 0;
		c.drive = slewS(&stD, d, 0.01); }});
	s.push_back({"h-pulse", 10, 49.0, [](double t, Engine& e) { static int last = -1; if (t == 0) last = -1; Controls& c = e.c; baseS(c);
		c.nu = 4e-5; static const int nt[16] = {0, 0, 12, 0, 3, 0, 10, 0, 0, 7, 0, 12, 0, 3, 5, 0}; static const int ac[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
		double st = 60.0 / 125 / 4; int k = (int)(t / st); double x = t / st - k; int s_ = k % 16; c.tune = std::pow(2, nt[s_] / 12.0); c.fc = 98 * c.tune; c.focus = 0.7; c.gain = 500; c.bright = 0.8;
		double open = std::min(1.0, t / (st * 48)); int on = x < 0.55 && t < st * 64; c.amp = 0.5 + 0.4 * open; c.pick = 0.8; c.drv = ac[s_] ? 0.8 * open : 0.2 * open; c.rat = ac[s_] ? 0.7 : 0.35;
		c.bleed = 0.4; c.out = 0.6 * open;
		c.drive = on ? (ac[s_] ? xlerpS(12000, 40000, open) : xlerpS(4000, 9000, open)) : 0; c.damp = on ? 0 : xlerpS(8, 1.2, open);
		if (k != last && t < st * 64) { last = k; e.strike(ac[s_] ? 260 : 120); } }});
	return s;
}
