// Essphor Res: 48-band tuned filter bank and resonator. Follows tools/proto/res_proto.py and
// part B of docs/BUILD-INSTRUCTIONS.md. No Rack dependency. Internal audio: 1.0 = 5 V.
#pragma once
#include "Common.hpp"

namespace essphor {

static const int NB = 48;
static const double C1 = 32.703;
static const double Q0 = 11.0;

// Semitones above C1 of each band for a scale; returns the band count.
inline int bandNotes(int scale, double* out) {
	int base[NB], n = 0;
	for (int o = 0; o < 8; o++) for (int i = 0; i < 6; i++) base[n++] = 12 * o + 2 * i + (o % 2);
	if (scale == SCALE_FREE) { for (int k = 0; k < NB; k++) out[k] = 2.0 * k; return NB; }
	if (scale == 0) { for (int k = 0; k < NB; k++) out[k] = base[k]; std::sort(out, out + NB); return NB; }
	const ScaleDef& sd = SCALES[scale];
	int notes[NB]; int m = 0;
	for (int k = 0; k < NB; k++) {
		int b = base[k], best = 0; bool have = false;
		for (int o = -1; o < 10; o++) for (int i = 0; i < sd.n; i++) {
			int c = 12 * o + sd.deg[i];
			// nearest, ties go up
			if (!have || std::abs(c - b) < std::abs(best - b) || (std::abs(c - b) == std::abs(best - b) && c > best)) { best = c; have = true; }
		}
		bool dup = false;
		for (int j = 0; j < m; j++) if (notes[j] == best) dup = true;
		if (!dup && best >= 0) notes[m++] = best;
	}
	std::sort(notes, notes + m);
	for (int j = 0; j < m; j++) out[j] = notes[j];
	return m;
}

struct ResParams {
	double cutoff = 20000.0;     // Hz
	double res = 0.0;            // 0..1
	double decay = 0.0;          // 0..1
	bool choke = false;
	double pitch = 0.0;          // semitones (knob snapped + CV + V/OCT)
	double slope = -1.0;         // -1 LP, 0 BP, +1 HP
	double tone = 0.0;
	int scale = 0;
	double wet = 1.0;
	bool send = false;
	double age = 0.0;
};

// Band gain curve (curve() in res_proto.py)
inline double resCurve(double f, const ResParams& p) {
	const double q = 0.707 * (1 - p.res) + 12 * p.res * p.res;
	const double w = f / p.cutoff;
	const double den = std::sqrt((1 - w * w) * (1 - w * w) + (w / q) * (w / q));
	const double lp = 1 / den, hp = w * w / den, bp = (w / q) / den;
	double g = p.slope <= 0 ? -p.slope * lp + (1 + p.slope) * bp : p.slope * hp + (1 - p.slope) * bp;
	g *= std::pow(f / 1000.0, p.tone * 0.5);
	return std::max(0.0, std::min(16.0, g));
}

inline double resQ(double f, double decay) {
	if (decay <= 0) return Q0;
	const double T = 0.05 * std::pow(400.0, decay) * std::pow(f / 261.6, -0.3);
	return std::max(Q0, PI * f * T / 6.91);
}

struct ResChannel {
	// per-band state and coefficients (struct of arrays so the band loop vectorises)
	float s1[NB], s2[NB], G[NB], R2[NB], D[NB], gin[NB], gout[NB];
	double fCur[NB], qCur[NB], ginCur[NB], goutCur[NB];
	double eTune[NB], eQ[NB], eG[NB];
	SlowRandom dr[4];
	Rng rng;
	Svf capLo, capHi;
	double gloCur = 0, ghiCur = 0, floCur = 30, fhiCur = 9000;
	bool first = true;

	void init(double fs, uint64_t seed) {
		rng.seed(seed);
		for (int k = 0; k < NB; k++) { eTune[k] = rng.normal(); }
		for (int k = 0; k < NB; k++) { eQ[k] = rng.normal(); }
		for (int k = 0; k < NB; k++) { eG[k] = rng.normal(); }
		for (auto& d : dr) d.init(2.0, fs);
		for (int k = 0; k < NB; k++) { s1[k] = s2[k] = 0; G[k] = 0; R2[k] = 0; D[k] = 1; gin[k] = gout[k] = 0; fCur[k] = 100; qCur[k] = Q0; ginCur[k] = goutCur[k] = 0; }
		capLo = Svf(); capHi = Svf();
		first = true;
	}
};

struct ResEngine {
	ResParams p;
	double fs = 0;
	ResChannel ch[2];
	uint64_t seeds[2] = {1, 2};
	int count = 0;
	double notes[NB];
	int lastScale = -1;
	int blockPos = 0;
	double kSmooth = 1;
	double ageDrive = 1, ageHiss = 0;
	static const int BLOCK = 16;

	void init(double sampleRate) {
		fs = sampleRate;
		for (int c = 0; c < 2; c++) ch[c].init(fs, seeds[c]);
		kSmooth = 1.0 - std::exp(-(double)BLOCK / (0.005 * fs));
		blockPos = 0; lastScale = -1;
	}

	void update() {
		if (p.scale != lastScale) { count = bandNotes(p.scale, notes); lastScale = p.scale; }
		const double age = p.age;
		ageDrive = 1 + 3 * age; ageHiss = std::pow(10.0, (-90 + 36 * age) / 20);
		for (int c = 0; c < 2; c++) {
			ResChannel& C = ch[c];
			double drift[4] = {0, 0, 0, 0};
			if (ESSPHOR_AGE && age > 0) for (int i = 0; i < 4; i++) {
				double y = 0;
				for (int s = 0; s < BLOCK; s++) y = C.dr[i].step(C.rng);   // keep the 2 s time constant per sample
				drift[i] = 6.0 * age * std::max(-1.0, std::min(1.0, 2.0 * y));
			}
			// kept bands
			double fT[NB], qT[NB], gT[NB]; int kept = 0;
			for (int j = 0; j < count; j++) {
				double f = C1 * std::pow(2.0, (notes[j] + p.pitch) / 12.0);
				if (ESSPHOR_AGE && age > 0) f *= std::pow(2.0, (18.0 * age * C.eTune[j] + drift[j & 3]) / 1200.0);
				if (f < 16.0 || f >= 0.45 * fs) continue;
				double q = resQ(f, p.decay);
				double g = resCurve(f, p);
				if (ESSPHOR_AGE && age > 0) { q = std::max(3.0, q * std::pow(2.0, 0.4 * age * C.eQ[j])); g *= std::pow(10.0, 2.0 * age * C.eG[j] / 20.0); }
				fT[kept] = f; qT[kept] = q; gT[kept] = g; kept++;
			}
			const double BG = std::pow(10.0, 1.7 / 20.0);
			for (int k = 0; k < NB; k++) {
				double gi = 0, go = 0;
				if (k < kept) {
					const double pol = (k % 2 == 0) ? 1.0 : -1.0;
					const double mk = std::pow(qT[k] / Q0, 0.25);
					if (!p.choke) { gi = gT[k]; go = pol * mk * BG; }
					else { gi = 1.0; go = gT[k] * pol * mk * BG; }
					if (C.first) { C.fCur[k] = fT[k]; C.qCur[k] = qT[k]; }
					else { C.fCur[k] += (fT[k] - C.fCur[k]) * kSmooth; C.qCur[k] += (qT[k] - C.qCur[k]) * kSmooth; }
				}
				if (C.first) { C.ginCur[k] = gi; C.goutCur[k] = go; }
				else { C.ginCur[k] += (gi - C.ginCur[k]) * kSmooth; C.goutCur[k] += (go - C.goutCur[k]) * kSmooth; }
				const double Gk = std::tan(PI * std::min(C.fCur[k], 0.45 * fs) / fs);
				const double Rk = 1.0 / (2.0 * C.qCur[k]);
				C.G[k] = (float)Gk; C.R2[k] = (float)(2 * Rk); C.D[k] = (float)(1.0 / (1.0 + 2 * Rk * Gk + Gk * Gk));
				C.gin[k] = (float)C.ginCur[k];
				C.gout[k] = (float)(C.goutCur[k] * 2 * Rk);
			}
			// end caps
			double flo = 30, fhi = 9000, glo = 0, ghi = 0;
			if (kept > 0) {
				flo = fT[0] * std::pow(2.0, -4.0 / 12); fhi = fT[kept - 1] * std::pow(2.0, 4.0 / 12);
				glo = -0.7 * resCurve(flo / 2, p);
				const double polLast = ((kept - 1) % 2 == 0) ? 1.0 : -1.0;
				ghi = 0.7 * resCurve(std::min(fhi * 1.5, 0.45 * fs), p) * (polLast * -1.0);
			}
			if (C.first) { C.gloCur = glo; C.ghiCur = ghi; C.floCur = flo; C.fhiCur = fhi; }
			else {
				C.gloCur += (glo - C.gloCur) * kSmooth; C.ghiCur += (ghi - C.ghiCur) * kSmooth;
				C.floCur += (flo - C.floCur) * kSmooth; C.fhiCur += (fhi - C.fhiCur) * kSmooth;
			}
			C.capLo.set(C.floCur, 1.0, fs); C.capHi.set(C.fhiCur, 1.0, fs);
			C.first = false;
		}
	}

	inline double runChannel(ResChannel& C, double x) {
		const double age = p.age;
		double xb = x;
		if (ESSPHOR_AGE && age > 0) xb = std::tanh(x * ageDrive) / ageDrive + C.rng.normal() * ageHiss;
		const float xf = (float)xb;
		float acc = 0.f;
		for (int k = 0; k < NB; k++) {
			const float v = C.gin[k] * xf;
			const float hp = (v - (C.R2[k] + C.G[k]) * C.s1[k] - C.s2[k]) * C.D[k];
			const float bp = C.G[k] * hp + C.s1[k];
			C.s1[k] = C.G[k] * hp + bp;
			const float lp = C.G[k] * bp + C.s2[k];
			C.s2[k] = C.G[k] * bp + lp;
			acc += C.gout[k] * bp;
		}
		double l1, b1, h1, l2, b2, h2;
		C.capLo.run(xb, l1, b1, h1);
		C.capHi.run(xb, l2, b2, h2);
		return acc + C.gloCur * l1 + C.ghiCur * h2;
	}

	void process(double inL, double inR, double& outL, double& outR) {
		if (blockPos == 0) update();
		if (++blockPos >= BLOCK) blockPos = 0;
		const double yL = softlimit(runChannel(ch[0], inL));
		const double yR = softlimit(runChannel(ch[1], inR));
		if (!p.send) {
			const double cw = std::cos(p.wet * PI / 2), sw = std::sin(p.wet * PI / 2);
			outL = inL * cw + yL * sw; outR = inR * cw + yR * sw;
		} else { outL = inL + yL * p.wet; outR = inR + yR * p.wet; }
	}
};

}  // namespace essphor
