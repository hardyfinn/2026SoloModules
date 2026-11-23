// BLECH: six rhythm-box drum voices. Port of blech_render in tools/proto/zap_blech_ref.py.
#pragma once
#include "Common.hpp"
#include "BlechTables.hpp"

namespace wb {

struct BlechParams {
	int box = 0;
	double tune[6] = {0, 0, 0, 0, 0, 0};
	double decay[6] = {1, 1, 1, 1, 1, 1};
	double dyn = 0.7, pedPitch = 0.0, tone = 1.0, age = 0.3;
};

struct BlechEngine {
	double fs = 48000;
	double U[24] = {};
	XS32 rng;
	Drift drift;
	double ic1[6], ic2[6], bv[6], pe[6], n1[6], n2[6], ne[6], na[6], nt[6], hp[6], cl[6], clk[6], lp[6], lvl[6];
	int boxOf[6];            // box latched per voice at its last hit
	double leakAll = 0;
	uint64_t n = 0;

	void init(double sampleRate, uint64_t unitSeed, uint32_t noiseSeed) {
		fs = sampleRate;
		unitValues(unitSeed, U, 24);
		rng.seed(noiseSeed);
		drift.init(fs);
		for (int v = 0; v < 6; v++) {
			ic1[v] = ic2[v] = bv[v] = pe[v] = n1[v] = n2[v] = ne[v] = na[v] = nt[v] = hp[v] = cl[v] = clk[v] = lp[v] = 0;
			lvl[v] = 1; boxOf[v] = 0;
		}
		leakAll = 0; n = 0;
	}
	// trig[6]: > 0 = hit of that value. out[6] receives each voice.
	void process(const double* trig, const BlechParams& P, double* out) {
		const double age = P.age;
		const double vel0 = 0.25 + P.dyn;
		const double ctone = 1.0 - std::exp(-2.0 * PI * std::min(600.0 * std::pow(30.0, P.tone), 0.45 * fs) / fs);
		const double cclk = 1.0 - std::exp(-2.0 * PI * 2500.0 / fs);
		const double hiss = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 1.5e-4;
		const double hum = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 3.0e-4;
		double r = rng.next();
		double dr = drift.tick(r);
		const double ns = std::sqrt(fs / 48000.0);
		const double rn = r * ns;
		double leak = 0.0;
		for (int v = 0; v < 6; v++) {
			if (trig[v] > 0.0) boxOf[v] = P.box;
			const double* p = BLECH_BOX[boxOf[v]][v];
			if (trig[v] > 0.0) {
				double r1 = rng.next();
				lvl[v] = std::pow(10.0, age * (1.5 * r1 + 1.0 * U[4 * v + 2]) / 20.0);
				double a = std::min(vel0 * trig[v], 1.8) * lvl[v];
				if (p[0] > 0.0) { ic1[v] += 2.0 * a; pe[v] = 1.0; }
				nt[v] = a; na[v] = 0.0; ne[v] = 1.0;
				clk[v] = a * std::nearbyint(0.001 * fs);
			}
			double y = 0.0;
			if (p[0] > 0.0) {
				double T60 = p[1] * P.decay[v] * (1.0 - 0.25 * age * std::fabs(U[4 * v + 1]));
				double f = p[0] * std::exp2(P.tune[v] + P.pedPitch) * (1.0 + age * 0.04 * U[4 * v]) * (1.0 + age * 0.006 * dr);
				f *= 1.0 + p[2] * pe[v] * std::min(nt[v], 1.5);
				pe[v] *= std::exp(-1.0 / (p[3] * fs));
				f = std::min(f, 0.45 * fs);
				double g = std::tan(PI * f / fs);
				double k = 6.908 / (PI * f * T60) + 0.03 * bv[v] * bv[v];
				double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
				double v3 = -ic2[v], v1 = a1 * ic1[v] + a2 * v3, v2 = ic2[v] + a2 * ic1[v] + a3 * v3;
				ic1[v] = 2.0 * v1 - ic1[v]; ic2[v] = 2.0 * v2 - ic2[v]; bv[v] = v1;
				y += p[4] * v1;
			}
			double c = 0.0;
			if (clk[v] > 0.0) {
				c = nt[v]; clk[v] -= nt[v];
				if (clk[v] < 1e-9) clk[v] = 0.0;
			}
			cl[v] += cclk * (c - cl[v]);
			y += p[10] * cl[v]; leak += cl[v];
			if (p[5] > 0.0) {
				double T60 = p[7] * P.decay[v] * (1.0 - 0.25 * age * std::fabs(U[4 * v + 1]));
				na[v] += (1.0 - na[v]) * (1.0 - std::exp(-1.0 / (p[8] * fs)));
				double e = nt[v] * na[v] * ne[v];
				ne[v] *= std::exp(-6.908 / (T60 * fs));
				double f = std::min(p[5] * std::exp2(0.5 * (P.tune[v] + P.pedPitch)) * (1.0 + age * 0.08 * U[4 * v + 3]), 0.45 * fs);
				double g = std::tan(PI * f / fs), k = p[6];
				double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
				double v3 = rn - n2[v], v1 = a1 * n1[v] + a2 * v3, v2 = n2[v] + a2 * n1[v] + a3 * v3;
				n1[v] = 2.0 * v1 - n1[v]; n2[v] = 2.0 * v2 - n2[v];
				double z;
				if (v == 2) z = rn - k * v1 - v2;
				else {
					z = v1;
					double fh = v == 1 ? 900.0 : 3000.0;
					hp[v] += (1.0 - std::exp(-2.0 * PI * fh / fs)) * (z - hp[v]); z = z - hp[v];
				}
				y += p[9] * e * z;
			}
			y = std::tanh(1.2 * y) / 0.8337;
			lp[v] += ctone * (y - lp[v]);
			out[v] = lp[v];
		}
		double r4 = rng.next();
		double common = age * 0.02 * leakAll + hiss * r4 * ns + hum * std::sin(2.0 * PI * 50.0 * (double)n / fs);
		leakAll = leak;
		for (int v = 0; v < 6; v++) out[v] += common / 6.0;
		n++;
	}
};

}  // namespace wb
