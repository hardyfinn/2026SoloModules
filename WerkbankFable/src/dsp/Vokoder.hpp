// VOKODER: 20-band channel vocoder. Port of vok_render and carrier in tools/proto/vokoder_ref.py.
#pragma once
#include "Common.hpp"

namespace wb {

static const int VK_NB = 20;
static const double VK_QBAND = 3.6;

inline double vkCentre(int i) {       // geometric centres between 100 Hz and 8 kHz edges
	double lo = 100.0 * std::pow(80.0, (double)i / VK_NB), hi = 100.0 * std::pow(80.0, (double)(i + 1) / VK_NB);
	return std::sqrt(lo * hi);
}

struct Biquad {     // RBJ band-pass (b1 = 0), direct form 1
	double b0 = 0, b2 = 0, a1 = 0, a2 = 0;
	double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	void setBP(double f, double q, double fs) {
		double w = 2.0 * PI * std::min(f, 0.45 * fs) / fs;
		double al = std::sin(w) / (2.0 * q), a0 = 1.0 + al;
		b0 = al / a0; b2 = -al / a0; a1 = -2.0 * std::cos(w) / a0; a2 = (1.0 - al) / a0;
	}
	inline double tick(double x) {
		double y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1; x1 = x; y2 = y1; y1 = y;
		return y;
	}
	void reset() { x1 = x2 = y1 = y2 = 0; }
};

struct VokParams {
	double sliders[VK_NB];
	double attack = 0.004, release = 0.04, shift = 0, sib = 0.7, thresh = 0.5, age = 0.3;
	VokParams() { for (int i = 0; i < VK_NB; i++) sliders[i] = 1.0; }
};

struct VokoderEngine {
	double fs = 48000;
	double U[60] = {};
	Biquad an[VK_NB][2], sy[VK_NB][2];
	double gb[VK_NB], e1[VK_NB], e2[VK_NB];
	double pz = 0, kpe = 0;
	SVF hpD, lpD, nzF;
	double gh = 0, gl = 0, gn = 0;
	double eh = 0, el = 0, uv = 0, sp = 0;
	double kd = 0, kuv = 0;
	XS32 rng;
	double coefAge = -1;
	double sliderCache[VK_NB];
	// outputs besides audio
	double voiced = 0, speechEnv = 0;

	void init(double sampleRate, uint64_t unitSeed, uint32_t noiseSeed) {
		fs = sampleRate;
		unitValues(unitSeed, U, 60);
		rng.seed(noiseSeed);
		for (int i = 0; i < VK_NB; i++) { for (int s = 0; s < 2; s++) { an[i][s].reset(); sy[i][s].reset(); } e1[i] = e2[i] = 0; sliderCache[i] = -1; }
		pz = 0; kpe = 1.0 - std::exp(-2.0 * PI * 2000.0 / fs);
		gh = std::tan(PI * 3000.0 / fs); gl = std::tan(PI * 800.0 / fs); gn = std::tan(PI * 2500.0 / fs);
		hpD.reset(); lpD.reset(); nzF.reset();
		hpD.set(gh, 1.414); lpD.set(gl, 1.414); nzF.set(gn, 1.414);
		eh = el = uv = sp = 0;
		kd = 1.0 - std::exp(-1.0 / (0.008 * fs)); kuv = 1.0 - std::exp(-1.0 / (0.004 * fs));
		coefAge = -1;
	}
	void setCoefs(const VokParams& p) {
		bool ageChanged = p.age != coefAge;
		for (int i = 0; i < VK_NB; i++) {
			if (ageChanged) {
				double c = vkCentre(i);
				double fc = c * (1.0 + p.age * 0.03 * U[3 * i]), q = VK_QBAND * (1.0 + p.age * 0.15 * U[3 * i + 1]);
				double fc2 = c * (1.0 - p.age * 0.03 * U[3 * i]);
				for (int s = 0; s < 2; s++) { an[i][s].setBP(fc, q, fs); sy[i][s].setBP(fc2, q, fs); }
			}
			if (ageChanged || p.sliders[i] != sliderCache[i]) {
				gb[i] = p.sliders[i] * std::pow(10.0, p.age * 1.5 * U[3 * i + 2] / 20.0) * (i % 2 == 0 ? 1.0 : -1.0);
				sliderCache[i] = p.sliders[i];
			}
		}
		coefAge = p.age;
	}
	// band envelope for display
	double bandEnv(int i) const { return e2[i]; }

	double process(double m, double car, bool freeze, const VokParams& p) {
		setCoefs(p);
		const double age = p.age;
		const double kat = 1.0 - std::exp(-1.0 / (p.attack * fs)), krl = 1.0 - std::exp(-1.0 / (p.release * fs));
		const double ksm = 1.0 - std::exp(-1.0 / ((0.003 - 0.002 * age) * fs));
		const double ratio = 0.5 * std::pow(10.0, 1.2 * (p.thresh - 0.5));
		const double floor_ = 0.004;
		const double bleed = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 0.003;
		const double hiss = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 1.5e-4;
		pz += kpe * (m - pz);
		double mp = m + 1.0 * (m - pz);
		double band, low;
		hpD.tick(m, band, low);
		double hf = m - 1.414 * band - low;
		lpD.tick(m, band, low);
		double lf = low;
		eh += kd * (std::fabs(hf) - eh); el += kd * (std::fabs(lf) - el);
		double tgt = (eh > ratio * el && eh > floor_) ? 1.0 : 0.0;
		uv += kuv * (tgt - uv);
		sp += kd * (((eh + el) > floor_ ? 1.0 : 0.0) - sp);
		voiced = 1.0 - uv; speechEnv = std::min(1.0, 4.0 * (eh + el));
		const double ns = std::sqrt(fs / 48000.0);
		double r = rng.next() * ns;
		nzF.tick(r, band, low);
		double nz = r - 1.414 * band - low;
		double c = (1.0 - uv) * car + uv * p.sib * 1.4 * nz;
		for (int i = 0; i < VK_NB; i++) {
			double x = an[i][1].tick(an[i][0].tick(mp));
			if (!freeze) {
				double rect = std::fabs(x);
				e1[i] += (rect > e1[i] ? kat : krl) * (rect - e1[i]);
				e2[i] += ksm * (e1[i] - e2[i]);
			}
		}
		double y = 0.0;
		for (int i = 0; i < VK_NB; i++) {
			// bands shifted in from outside the bank are silent
			double pp = i - p.shift;
			int j = (int)std::floor(pp); double fr = pp - j;
			double ea = (j >= 0 && j < VK_NB) ? e2[j] : 0.0, eb = (j + 1 >= 0 && j + 1 < VK_NB) ? e2[j + 1] : 0.0;
			double e = ea * (1.0 - fr) + eb * fr;
			double x = sy[i][1].tick(sy[i][0].tick(c));
			y += gb[i] * x * e;
		}
		double r2 = rng.next();
		return std::tanh(0.5 * (5.0 * y * sp + bleed * car)) * 2.0 + hiss * r2 * ns;
	}
};

// internal carrier: saw <-> pulse, chord 0 none, 1 fifth, 2 minor, 3 major. One note peaks at 1.
struct VokCarrier {
	double ph[3] = {0, 0.31, 0.62};
	void reset() { ph[0] = 0; ph[1] = 0.31; ph[2] = 0.62; }
	double process(double pitch, double shape, double pw, int chord, double fs) {
		double semi[3] = {0, 0, 0}; int nn = 1;
		if (chord == 1) { nn = 2; semi[1] = 7; }
		if (chord == 2) { nn = 3; semi[1] = 3; semi[2] = 7; }
		if (chord == 3) { nn = 3; semi[1] = 4; semi[2] = 7; }
		double y = 0.0;
		for (int i = 0; i < nn; i++) {
			double dt = std::min(C4 * std::exp2(pitch + semi[i] / 12.0) / fs, 0.45);
			ph[i] += dt; if (ph[i] >= 1.0) ph[i] -= 1.0;
			double saw = 2.0 * ph[i] - 1.0 - blep(ph[i], dt);
			double p2 = ph[i] - pw; if (p2 < 0.0) p2 += 1.0;
			double pul = saw - (2.0 * p2 - 1.0 - blep(p2, dt)) + (2.0 * pw - 1.0);
			y += (1.0 - shape) * saw + shape * pul;
		}
		return y / std::sqrt((double)nn);
	}
};

}  // namespace wb
