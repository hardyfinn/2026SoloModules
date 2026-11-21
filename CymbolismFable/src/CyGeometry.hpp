// Cymbolism Fable: cymbal geometry, materials and the type table.
// Pure C++ (no Rack), shared by the module and test/render.cpp.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>

namespace cy {

static const double CY_PI = 3.14159265358979323846;

struct CyRng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v ? v : 0x1234567ull; for (int i = 0; i < 4; i++) next(); }
	uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
	double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
	double bi() { return uni() * 2.0 - 1.0; }
	// approx. unit-variance noise (sum of 4 uniforms)
	double gauss() { return (uni() + uni() + uni() + uni() - 2.0) * 1.7320508; }
};

enum Alloy { B20 = 0, B12, B8, BRASS, NICKEL, STEEL, NUM_ALLOYS };

struct Material {
	const char* name;
	double E, rho, nu;
	double eta;   // internal loss factor (sigma = pi f eta)
};

static const Material CY_MATERIALS[NUM_ALLOYS] = {
	{"B20", 1.04e11, 8600.0, 0.34, 1.6e-4},
	{"B12", 1.08e11, 8700.0, 0.34, 2.6e-4},
	{"B8", 1.10e11, 8800.0, 0.34, 3.6e-4},
	{"BRASS", 1.00e11, 8500.0, 0.34, 6.5e-4},
	{"NICKEL SILVER", 1.25e11, 8700.0, 0.33, 4.0e-4},
	{"STEEL", 2.00e11, 7850.0, 0.29, 1.1e-4},
};

enum Play { TIP = 0, SHOULDER, MALLET, RODS, BRUSH, BOW, NUM_PLAYS };
static const char* CY_PLAY_NAMES[NUM_PLAYS] = {"TIP", "SHOULDER", "MALLET", "RODS", "BRUSH", "BOW"};

struct CymbalType {
	const char* name;
	double sizeIn;      // diameter, inches
	double edgeMm;      // edge thickness, mm
	double taper;       // centre / edge thickness
	double rise;        // bow rise, fraction of radius
	double bellDia;     // bell diameter, fraction of the full diameter (0 = no bell)
	double bellMm;      // bell height above the bow, mm
	double flangeDeg;   // edge flange (positive = turned up, china)
	int play;           // default beater
	double position;    // default strike position 0 bell .. 1 edge
	int rivets;         // default rivet count
};

static const int NUM_TYPES = 10;
static const CymbalType CY_TYPES[NUM_TYPES] = {
	{"SPLASH", 10, 0.55, 1.6, 0.060, 0.30, 12, 0, SHOULDER, 0.92, 0},
	{"CRASH", 18, 0.85, 1.7, 0.070, 0.25, 18, 0, SHOULDER, 0.95, 0},
	{"THIN CRASH", 17, 0.65, 1.5, 0.060, 0.24, 16, 0, SHOULDER, 0.95, 0},
	{"CRASH RIDE", 20, 1.10, 1.8, 0.080, 0.23, 20, 0, TIP, 0.60, 0},
	{"RIDE", 21, 1.40, 1.9, 0.080, 0.22, 22, 0, TIP, 0.55, 0},
	{"HEAVY RIDE", 22, 1.90, 1.8, 0.090, 0.26, 26, 0, TIP, 0.55, 0},
	{"FLAT RIDE", 20, 1.10, 1.4, 0.015, 0.0, 0, 0, TIP, 0.55, 0},
	{"CHINA", 18, 0.75, 1.5, 0.050, 0.22, 22, 20, SHOULDER, 0.95, 0},
	{"SWISH", 20, 0.90, 1.5, 0.050, 0.20, 18, 15, TIP, 0.60, 6},
	{"BELL", 8, 3.00, 1.2, 0.250, 0.60, 25, 0, TIP, 0.10, 0},
};

// Everything that changes the mode set (a "rebuild").
struct GeomParams {
	int type = 1;
	double sizeIn = 18.0;
	double edgeMm = 0.85;
	double taper = 1.7;
	double rise = 0.07;
	double bell = 1.0;       // x the type's bell size
	double flangeDeg = 0.0;
	int alloy = B20;
	double mount = 0.5;      // 0 tight .. 1 loose
	int quality = 1;         // 0 ECO, 1 HIGH, 2 ULTRA
	double sampleRate = 48000.0;

	// Quantised key for the model cache.
	std::string key() const {
		char buf[256];
		snprintf(buf, sizeof(buf), "%d|%.2f|%.3f|%.2f|%.4f|%.2f|%.1f|%d|%.2f|%d|%.0f", type, sizeIn, edgeMm, taper, rise, bell,
		         flangeDeg, alloy, mount, quality, sampleRate);
		return buf;
	}
};

// Derived shape of the shell, in SI units.
struct Shape {
	double a, r0, rb;   // outer radius, hole radius, bell radius
	double H;           // bow rise at rb (m)
	double Hb;          // bell height above the bow (m)
	double he, T, Tb;   // edge thickness (m), taper, bell thickness factor
	double rf, tanF;    // flange start radius, tan(flange angle)
	Material mat;

	void make(const GeomParams& g) {
		const CymbalType& ct = CY_TYPES[g.type];
		a = g.sizeIn * 0.0254 * 0.5;
		r0 = 0.0065;
		double bellFrac = ct.bellDia * g.bell;
		if (bellFrac > 0.75) bellFrac = 0.75;
		rb = bellFrac * a;
		if (rb < r0 + 0.002) rb = r0;     // no bell
		H = g.rise * a;
		Hb = (rb > r0) ? ct.bellMm * 1e-3 * g.bell * (g.sizeIn / ct.sizeIn > 0 ? 1.0 : 1.0) : 0.0;
		he = g.edgeMm * 1e-3;
		T = g.taper;
		Tb = T * 1.15;
		rf = 0.86 * a;
		tanF = std::tan(g.flangeDeg * CY_PI / 180.0);
		mat = CY_MATERIALS[g.alloy];
	}

	// bow slope at rb (the bow is a parabola from (rb, H) to (a, 0))
	double bowZ(double r) const { return H * (a * a - r * r) / (a * a - rb * rb); }
	double bowZp(double r) const { return -2.0 * H * r / (a * a - rb * rb); }

	// mid-surface height z(r) and slope z'(r)
	void z(double r, double& zz, double& zp) const {
		if (r >= rb || rb <= r0) {
			zz = bowZ(r);
			zp = bowZp(r);
		}
		else {
			// cubic Hermite from (r0, H+Hb, slope 0) to (rb, H, bow slope)
			double L = rb - r0;
			double t = (r - r0) / L;
			double t2 = t * t, t3 = t2 * t;
			double h00 = 2 * t3 - 3 * t2 + 1, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
			double d00 = 6 * t2 - 6 * t, d01 = -6 * t2 + 6 * t, d11 = 3 * t2 - 2 * t;
			double s1 = bowZp(rb) * L;
			zz = h00 * (H + Hb) + h01 * H + h11 * s1;
			zp = (d00 * (H + Hb) + d01 * H + d11 * s1) / L;
		}
		if (tanF != 0.0 && r > rf) {
			double u = r - rf;
			zz += tanF * u * u / (2.0 * (a - rf));
			zp += tanF * u / (a - rf);
		}
	}

	// thickness h(r)
	double h(double r) const {
		double hb = Tb * he;
		if (rb <= r0) {
			// no bell: taper from centre to edge
			double t = (r - r0) / (a - r0);
			return he * (T + (1.0 - T) * t);
		}
		double hbow = he * (T + (1.0 - T) * (r - rb) / (a - rb));
		// smooth blend over 4 mm around the bell join
		double w = 0.004;
		double s = 0.5 + 0.5 * std::tanh((r - rb) / (0.5 * w));
		double hbowc = (r < rb) ? he * T : hbow;
		return hb * (1.0 - s) + hbowc * s;
	}

	double D(double r) const {
		double hh = h(r);
		return mat.E * hh * hh * hh / (12.0 * (1.0 - mat.nu * mat.nu));
	}
};

} // namespace cy
