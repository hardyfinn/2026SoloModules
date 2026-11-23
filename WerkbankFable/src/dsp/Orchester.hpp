// ORCHESTER: optical-disc keyboard. Port of orch_voice in tools/proto/dynamo_orch_ref.py.
// The disc angle is owned by the module and passed in every sample: all keys share it.
#pragma once
#include "Common.hpp"
#include "OrchTables.hpp"

namespace wb {

struct OrchParams { double wobble = 0.5, tone = 0.5, attack = 0.005, release = 0.03, age = 0.3, octave = 0.0; };

inline void orchNotePersonality(int disc, int semitone, double* NP) {
	unitValues(0xD15C0000ULL + (uint64_t)disc * 1000ULL + (uint64_t)(semitone + 500), NP, 8);
}

struct OrchVoice {
	double fs = 48000;
	int disc = 0;
	const double* D = ORCH_DISCS[0];
	double NP[8] = {};
	double ph[3];
	double fb1[4], fb2[4], fa[4][3];
	double sl = 0, l1 = 0, l2 = 0, hp = 0, env = 0, b1 = 0, b2 = 0, thump = 0;
	double trem = 1;
	XS32 rng;
	// cached coefficients (depend on tone, age, note)
	double la1 = 0, la2 = 0, la3 = 0, cachedTone = -1, cachedAge = -1;
	double gate0 = 0;

	void init(double sampleRate, uint32_t seed) {
		fs = sampleRate; rng.seed(seed);
		reset(0, 0);
	}
	// called at a gate rise (and at init): take the disc and the note's personality, restart the loop
	void reset(int discIndex, int semitone) {
		disc = discIndex; D = ORCH_DISCS[disc];
		orchNotePersonality(disc, semitone, NP);
		for (int i = 0; i < 3; i++) ph[i] = 0.37 * i + 0.21;
		for (int j = 0; j < 4; j++) { fb1[j] = fb2[j] = 0; }
		sl = l1 = l2 = hp = b1 = b2 = thump = 0;
		for (int j = 0; j < 4; j++) {
			fa[j][0] = fa[j][1] = fa[j][2] = 0;
			if (ORCH_FORM[disc][j][2] > 0.0) {
				double g = std::tan(PI * std::min(ORCH_FORM[disc][j][0], 0.45 * fs) / fs), k = 1.0 / ORCH_FORM[disc][j][1];
				fa[j][0] = 1.0 / (1.0 + g * (g + k)); fa[j][1] = g * fa[j][0]; fa[j][2] = g * fa[j][1];
			}
		}
		cachedTone = -1;
	}
	bool silent() const { return env < 1e-5 && gate0 <= 0.5; }

	// th_prev, th: disc angle before and after this sample's advance (0..1)
	double process(double pitch, double gate, double thPrev, double th, const OrchParams& p) {
		gate0 = gate;
		const int ns = (int)D[2];
		const double wobble = p.wobble, age = p.age;
		const double wdepth = 0.0025 * (2.0 * wobble) * (2.0 * wobble) * (1.0 + 2.0 * age);
		const double fdepth = 0.10 * (2.0 * wobble) * (1.0 + 1.5 * age);
		if (p.tone != cachedTone || age != cachedAge) {
			double flp = std::min(D[15] * std::exp2(3.0 * (p.tone - 0.5) + 0.3 * NP[2]) * (1.0 - 0.35 * age), 0.45 * fs);
			double gl = std::tan(PI * flp / fs), kl = 1.3;
			la1 = 1.0 / (1.0 + gl * (gl + kl)); la2 = gl * la1; la3 = gl * la2;
			cachedTone = p.tone; cachedAge = age;
		}
		const double chp = 1.0 - std::exp(-2.0 * PI * 90.0 / fs);
		const double ca = 1.0 - std::exp(-1.0 / (std::max(p.attack, 0.001) * fs / 3.0));
		const double cr = 1.0 - std::exp(-1.0 / (std::max(p.release, 0.001) * fs / 3.0));
		const double csrc = D[12] <= 0.0 ? 1.0 : 1.0 - std::exp(-2.0 * PI * D[12] / fs);
		const double splice = 0.5 + 0.5 * NP[3];
		const double lvl = D[16] * std::pow(10.0, 1.5 * NP[1] / 20.0);
		const double dust = age * age * 6.0 / fs;
		const double nsc = std::sqrt(fs / 48000.0);
		const double cb1 = 1.0 - std::exp(-2.0 * PI * 2724.6 / fs), cb2 = 1.0 - std::exp(-2.0 * PI * 391.8 / fs);

		double w = std::sin(2.0 * PI * th) + 0.3 * std::sin(4.0 * PI * th + 1.0);
		double base = C4 * std::exp2(pitch + p.octave + D[17] + 6.0 * NP[0] / 1200.0) * (1.0 + wdepth * w);
		double x = 0.0;
		for (int i = 0; i < ns; i++) {
			double vr = std::nearbyint(D[6 + i] * ORCH_REV) / ORCH_REV;
			double vib = std::sin(2.0 * PI * (vr * ORCH_REV * th + 0.5 * NP[4 + i]));
			double f = base * std::exp2((D[3 + i] + D[9 + i] * vib) / 1200.0);
			double dt = std::min(f / fs, 0.45);
			ph[i] += dt; if (ph[i] >= 1.0) ph[i] -= 1.0;
			if (D[0] == 0.0) x += 2.0 * ph[i] - 1.0 - blep(ph[i], dt);
			else if (D[0] == 1.0) {
				double p2 = ph[i] - D[1]; if (p2 < 0.0) p2 += 1.0;
				x += (2.0 * ph[i] - 1.0 - blep(ph[i], dt)) - (2.0 * p2 - 1.0 - blep(p2, dt));
			} else {
				for (int h = 0; h < 8; h++)
					if (ORCH_ADD[disc][h] > 0.0 && (h + 1) * f < 0.45 * fs) x += 0.6 * ORCH_ADD[disc][h] * std::sin(2.0 * PI * (h + 1) * ph[i]);
			}
			if (i == 0) trem = 1.0 + D[18] * vib;
		}
		x = x / std::sqrt((double)ns) * trem;
		double rn = rng.next();
		sl += csrc * (x - sl); x = sl;
		double y = D[13] * x;
		for (int j = 0; j < 4; j++) {
			if (ORCH_FORM[disc][j][2] > 0.0) {
				double v3 = x - fb2[j], v1 = fa[j][0] * fb1[j] + fa[j][1] * v3, v2 = fb2[j] + fa[j][1] * fb1[j] + fa[j][2] * v3;
				fb1[j] = 2.0 * v1 - fb1[j]; fb2[j] = 2.0 * v2 - fb2[j];
				y += ORCH_FORM[disc][j][2] * v1 / ORCH_FORM[disc][j][1] * 3.0;
			}
		}
		b1 += cb1 * (rn * nsc - b1); b2 += cb2 * (b1 - b2);
		y += D[14] * 4.0 * (b1 - b2);
		y *= 1.0 + fdepth * std::sin(2.0 * PI * th + PI * NP[7]);
		if ((thPrev < splice && splice <= th) || (th < thPrev && (splice > thPrev || splice <= th)))
			thump = 0.05 + 0.10 * age;
		double rd = rng.next();
		if ((rd + 1.0) * 0.5 < dust) { double rd2 = rng.next(); thump += 0.25 * rd2; }
		y += thump; thump *= std::exp(-1.0 / (0.0015 * fs));
		double v3 = y - l2, v1 = la1 * l1 + la2 * v3, v2 = l2 + la2 * l1 + la3 * v3;
		l1 = 2.0 * v1 - l1; l2 = 2.0 * v2 - l2; y = v2;
		hp += chp * (y - hp); y -= hp;
		env += (gate > 0.5 ? ca : cr) * ((gate > 0.5 ? 1.0 : 0.0) - env);
		return 0.5 * lvl * y * env;
	}
};

// the shared disc: one per module
struct OrchDisc {
	double th = 0, fs = 48000;
	void init(double sampleRate) { fs = sampleRate; th = 0; }
	void advance(double& before, double& after) {
		before = th;
		th += 1.0 / (ORCH_REV * fs);
		if (th >= 1.0) th -= 1.0;
		after = th;
	}
};

}  // namespace wb
