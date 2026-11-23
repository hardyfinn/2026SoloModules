// DYNAMO: two-oscillator ladder-filter voice. Port of dynamo_voice in tools/proto/dynamo_orch_ref.py.
// Runs at 2x the engine rate and decimates with Decimator2.
#pragma once
#include "Common.hpp"

namespace wb {

// P[] indices, as in the Python
enum DynP { D_W1, D_W2, D_DET, D_PW, D_MIX, D_SUB, D_NOISE, D_CUT, D_RES, D_ENV, D_KEY,
	D_FA, D_FD, D_FS, D_AA, D_AD, D_AS, D_GLIDE, D_DRIVE, D_AGE, D_NP };

struct DynamoVoice {
	double fs = 48000, f2 = 96000;
	int os = 2;     // 2x at 44.1/48 kHz, 1x from 88.2 kHz up: the filter always runs near 96 kHz
	double U[8] = {};
	XS32 rng;
	Drift drift;
	Decimator2 dec;
	double ph1 = 0.3, ph2 = 0.7, phs = 0.1;
	double s[4] = {0, 0, 0, 0}, y4 = 0;
	double fe = 0, ae = 0, g0 = 0, acc = 0, pg = 0, et = 1;
	int fst = 0, ast = 0;
	bool first = true;
	double buf[2];

	void init(double sampleRate, uint64_t unitSeed, uint32_t noiseSeed) {
		fs = sampleRate; os = fs < 70000.0 ? 2 : 1; f2 = os * fs;
		unitValues(unitSeed, U, 8);
		rng.seed(noiseSeed);
		drift.init(fs);
		dec.reset();
		ph1 = 0.3; ph2 = 0.7; phs = 0.1;
		for (int j = 0; j < 4; j++) s[j] = 0;
		y4 = fe = ae = g0 = acc = 0; et = 1; fst = ast = 0; first = true;
	}
	bool silent() const { return ae < 1e-6 && fst == 0 && ast == 0; }

	// one engine-rate sample. P = the 20 parameters.
	double process(double pitch, double gate, double accent, double cutcv, const double* P) {
		if (first) { pg = pitch; first = false; }
		const double age = P[D_AGE];
		const double t1 = age * 4.0 * U[0] / 1200.0, t2 = age * 6.0 * U[1] / 1200.0;
		const double vt = 1.0 + age * 0.004 * U[2];
		const double pwo = age * 0.04 * U[3], cuo = age * 0.08 * U[4];
		const double crv = age * 0.08;
		const double hiss = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 1.5e-4;
		const double bleed = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 6.0e-4;
		const double cg = P[D_GLIDE] <= 1e-4 ? 0.0 : std::exp(-1.0 / (P[D_GLIDE] * fs));
		const double dg = 0.5 + 2.5 * P[D_DRIVE];
		const double r = 4.3 * P[D_RES];
		const double ns = std::sqrt(f2 / 96000.0);

		double rn = rng.next();
		double dr = drift.tick(rn);
		if (gate > 0.5 && g0 <= 0.5) {
			fst = 1; ast = 1; acc = accent;
			double r1 = rng.next(); et = 1.0 + age * 0.10 * r1;
		}
		if (gate <= 0.5) { fst = 0; ast = 0; }
		g0 = gate;
		if (fst == 1) {
			fe += (1.25 - fe) * (1.0 - std::exp(-1.0 / (P[D_FA] * et * fs / 1.609)));
			if (fe >= 1.0) { fe = 1.0; fst = 2; }
		} else if (fst == 2) {
			fe += (P[D_FS] - fe) * (1.0 - std::exp(-2.3 / (P[D_FD] * et * fs)));
		} else {
			fe += (0.0 - fe) * (1.0 - std::exp(-2.3 / (P[D_FD] * et * fs)));
		}
		if (ast == 1) {
			ae += (1.25 - ae) * (1.0 - std::exp(-1.0 / (P[D_AA] * et * fs / 1.609)));
			if (ae >= 1.0) { ae = 1.0; ast = 2; }
		} else if (ast == 2) {
			ae += (P[D_AS] - ae) * (1.0 - std::exp(-2.3 / (P[D_AD] * et * fs)));
		} else {
			ae += (0.0 - ae) * (1.0 - std::exp(-2.3 / (P[D_AD] * et * fs)));
		}
		pg = pitch + cg * (pg - pitch);
		double pv = pg * vt + age * (5.0 * dr) / 1200.0;
		double fr1 = C4 * std::exp2(pv + t1);
		double fr2 = C4 * std::exp2(pv + t2 + P[D_DET] / 12.0 + age * 3.0 * drift.d0 / 1200.0);
		double d1 = std::min(fr1 / f2, 0.45), dd2 = std::min(fr2 / f2, 0.45), dsub = 0.5 * d1;
		double pw = clampd(P[D_PW] + pwo, 0.05, 0.95);
		double amt = P[D_ENV] * 7.0 * (1.0 + 0.5 * acc);
		double fc = 20.0 * std::pow(1000.0, P[D_CUT]) * std::exp2(amt * fe + P[D_KEY] * pg + cutcv + cuo + age * 0.03 * dr);
		fc = clampd(fc, 10.0, 0.45 * f2);
		double g = std::tan(PI * fc / f2), G = g / (1.0 + g);
		double amp = ae * (1.0 + 0.4 * acc) + bleed;
		const double norm = (1.0 + 0.6 * r) / (0.35 + 0.65 * std::tanh(dg * 0.6));
		for (int k = 0; k < os; k++) {
			double x1, x2, saw, p2;
			ph1 += d1; if (ph1 >= 1.0) ph1 -= 1.0;
			saw = 2.0 * ph1 - 1.0 - blep(ph1, d1);
			if (P[D_W1] < 0.5) x1 = saw + crv * (saw * saw - 0.3333);
			else {
				p2 = ph1 - pw; if (p2 < 0.0) p2 += 1.0;
				x1 = saw - (2.0 * p2 - 1.0 - blep(p2, d1)) + (2.0 * pw - 1.0);
			}
			ph2 += dd2; if (ph2 >= 1.0) ph2 -= 1.0;
			saw = 2.0 * ph2 - 1.0 - blep(ph2, dd2);
			if (P[D_W2] < 0.5) x2 = saw + crv * (saw * saw - 0.3333);
			else {
				p2 = ph2 - pw; if (p2 < 0.0) p2 += 1.0;
				x2 = saw - (2.0 * p2 - 1.0 - blep(p2, dd2)) + (2.0 * pw - 1.0);
			}
			phs += dsub; if (phs >= 1.0) phs -= 1.0;
			p2 = phs - 0.5; if (p2 < 0.0) p2 += 1.0;
			double sq = (2.0 * phs - 1.0 - blep(phs, dsub)) - (2.0 * p2 - 1.0 - blep(p2, dsub));
			double rn2 = rng.next();
			double x = (1.0 - P[D_MIX]) * x1 + P[D_MIX] * x2 + P[D_SUB] * sq + 1.2 * P[D_NOISE] * rn2 * ns;
			double u = std::tanh(dg * (0.6 * x) - r * y4);
			for (int j = 0; j < 4; j++) { double v = (u - s[j]) * G; u = v + s[j]; s[j] = u + v; }
			y4 = u;
			buf[k] = y4 * norm * amp + hiss * rn2 * ns;
		}
		return os == 2 ? dec.process(buf[0], buf[1]) : buf[0];
	}
};

}  // namespace wb
