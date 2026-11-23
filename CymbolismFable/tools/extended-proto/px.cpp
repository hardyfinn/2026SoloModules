// Prototypes for Cymbolism Extended, on a hacked copy of the engine (scratch only).
#include "CyEngine.hpp"
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
using namespace cy;

static void writeWav(const std::string& path, const std::vector<float>& x, int sr) {
	FILE* fp = fopen(path.c_str(), "wb");
	int n = (int)x.size(), bytes = n * 3;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, fp); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); w32(36 + bytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(1); w32(sr); w32(sr * 3); w16(3); w16(24);
	fwrite("data", 1, 4, fp); w32(bytes);
	for (float v : x) {
		double s = std::max(-1.0, std::min(1.0, (double)v / 10.0));
		int32_t iv = (int32_t)std::lround(s * 8388607.0);
		unsigned char b[3] = {(unsigned char)(iv & 255), (unsigned char)((iv >> 8) & 255), (unsigned char)((iv >> 16) & 255)};
		fwrite(b, 1, 3, fp);
	}
	fclose(fp);
}

struct Proto {
	std::shared_ptr<Model> M;
	Engine E;
	std::vector<double> f0;                    // base frequencies (Hz, with hammering)
	std::vector<std::vector<double>> a0;       // base A values
	std::vector<double> sig0;
	void init(int type, Controls c) {
		GeomParams gp; gp.fromType(type); gp.sampleRate = 48000;
		M = buildModel(gp);
		E.ctl = c;
		E.init(M, 48000);
		f0.resize(E.N);
		for (int i = 0; i < E.N; i++) f0[i] = E.om0[i] / (2 * CY_PI);
		for (auto& sm : M->stress) a0.push_back(sm.aVal);
		sig0 = E.sigBase;
	}
	// set new frequencies; A is rescaled per mode so diag(w^2) - A^T A stays PSD
	void setFreqs(const std::vector<double>& f) {
		for (int i = 0; i < E.N; i++) E.om0[i] = 2 * CY_PI * f[i];
		for (size_t n = 0; n < M->stress.size(); n++) {
			auto& sm = M->stress[n];
			for (size_t a = 0; a < sm.aIdx.size(); a++) sm.aVal[a] = a0[n][a] * f[sm.aIdx[a]] / f0[sm.aIdx[a]];
		}
		E.retune();
		E.computeMic();
		sig0 = E.sigBase;
	}
	std::vector<float> run(double dur, std::function<void(double)> sc) {
		int n = (int)(dur * 48000);
		std::vector<float> out(n);
		for (int s = 0; s < n; s += 32) { sc((double)s / 48000); E.process(nullptr, out.data() + s, std::min(32, n - s)); }
		return out;
	}
	// SUSTAIN: regulated negative damping around fc (octaves bw), holding the energy at Et
	void sustain(double G, double Et, double fc, double bw) {
		double En = E.energy();
		double u = std::max(-1.0, std::min(1.0, 1.0 - En / Et));
		for (int i = 0; i < E.NA; i++) {
			double x = std::log2(std::max(1.0, E.wS[i] / (2 * CY_PI)) / fc) / bw;
			double w = std::exp(-0.5 * x * x);
			E.sigBase[i] = sig0[i] - G * u * (u > 0 ? w : 1.0);
			E.applyLoss(i);
		}
	}
	void sustainOff() { for (int i = 0; i < E.NA; i++) { E.sigBase[i] = sig0[i]; E.applyLoss(i); } }
};

static std::vector<double> tune(const std::vector<double>& f, double amt, std::vector<int> pcs, double rootHz) {
	std::vector<double> o(f.size());
	CyRng r; r.seed(99);
	for (size_t i = 0; i < f.size(); i++) {
		double semis = 12 * std::log2(f[i] / rootHz);
		double best = 1e9, bt = semis;
		for (int oct = -6; oct < 10; oct++) for (int pc : pcs) { double t = oct * 12 + pc; if (std::fabs(t - semis) < best) { best = std::fabs(t - semis); bt = t; } }
		double jit = 0.04 * r.bi();   // a few cents of residual chorus
		double s2 = semis + amt * (bt + jit - semis);
		o[i] = rootHz * std::pow(2.0, s2 / 12);
	}
	return o;
}
static std::vector<double> warp(const std::vector<double>& f, double g, double pivot) {
	std::vector<double> o(f.size());
	for (size_t i = 0; i < f.size(); i++) o[i] = std::min(40000.0, std::max(8.0, pivot * std::pow(f[i] / pivot, g)));
	return o;
}
static void append(std::vector<float>& a, const std::vector<float>& b) { a.insert(a.end(), b.begin(), b.end()); }
static Controls defaults(int type) { Controls c; c.play = CY_TYPES[type].play; c.position = CY_TYPES[type].position; c.mic = 1.0; return c; }

int main(int argc, char** argv) {
	std::string out = argv[1];
	std::string which = argc > 2 ? argv[2] : "all";
	auto want = [&](const char* n) { return which == "all" || which == n; };
	const int SR = 48000;

	if (want("tuned")) {
		// crash pulled onto D minor 9 (D F A C E): 0%, 50%, 100%, then a hard hit and a roll at 100%
		std::vector<float> w;
		for (double amt : {0.0, 0.5, 1.0}) {
			Proto P; P.init(1, defaults(1));
			P.setFreqs(tune(P.f0, amt, {0, 3, 7, 10, 14 - 12}, 146.83));
			bool hit = false;
			append(w, P.run(4.0, [&](double t) { if (!hit) { P.E.strike(3.0); hit = true; } (void)t; }));
		}
		{
			Proto P; P.init(1, defaults(1));
			P.setFreqs(tune(P.f0, 1.0, {0, 3, 7, 10, 2}, 146.83));
			bool hit = false;
			append(w, P.run(5.0, [&](double t) { if (!hit) { P.E.strike(8.0); hit = true; } (void)t; }));
		}
		{
			// ride pattern on a tuned ride, chord changes every bar (Dm9 -> G7 -> Cmaj7)
			Proto P; Controls c = defaults(4); P.init(4, c);
			std::vector<std::vector<int>> ch = {{0, 3, 7, 10, 2}, {5, 9, 0, 3, 7}, {10, 2, 5, 9, 0}};
			int last = -1, bar = -1;
			append(w, P.run(9.0, [&](double t) {
				int b = (int)(t / 2.4);
				if (b != bar && b < 3) { bar = b; P.setFreqs(tune(P.f0, 1.0, ch[b], 146.83)); }
				int k = (int)(t / 0.3);
				if (k != last && t < 7.2) { last = k; P.E.strike(k % 4 == 0 ? 3.0 : 1.6); }
			}));
		}
		writeWav(out + "/x-tuned.wav", w, SR);
		printf("tuned ok\n");
	}
	if (want("warp")) {
		std::vector<float> w;
		for (double g : {1.0, 0.75, 0.5, 1.3, 1.7}) {
			Proto P; P.init(1, defaults(1));
			P.setFreqs(warp(P.f0, g, 700.0));
			bool hit = false;
			append(w, P.run(3.5, [&](double t) { if (!hit) { P.E.strike(5.0); hit = true; } (void)t; }));
		}
		writeWav(out + "/x-warp.wav", w, SR);
		printf("warp ok\n");
	}
	if (want("swell")) {
		// reverse cymbal played live: SUSTAIN gate grows the ring from nothing, then lets go
		std::vector<float> w;
		for (double fc : {400.0, 4000.0}) {
			Proto P; Controls c = defaults(1); c.play = BRUSH; P.init(1, c);
			bool seeded = false;
			append(w, P.run(6.0, [&](double t) {
				if (!seeded) { P.E.strike(0.3); seeded = true; }
				if (t < 2.5) P.sustain(5.0, 0.6, fc, 2.0); else P.sustainOff();
			}));
		}
		writeWav(out + "/x-reverse-swell.wav", w, SR);
		printf("swell ok\n");
	}
	if (want("drone")) {
		// self-sustaining cymbal: energy held constant, colour moved from low to high, BLOOM 1.5
		Proto P; Controls c = defaults(7); c.play = BRUSH; c.bloom = 1.5; P.init(7, c);
		bool seeded = false;
		auto w = P.run(20.0, [&](double t) {
			if (!seeded) { P.E.strike(0.5); seeded = true; }
			double fc = 150.0 * std::pow(40.0, std::min(1.0, t / 14.0));
			if (t < 16) P.sustain(6.0, 0.25 + 0.5 * std::min(1.0, t / 10), fc, 1.2); else P.sustainOff();
		});
		writeWav(out + "/x-drone.wav", w, SR);
		printf("drone ok guard %d\n", P.E.guardEvents);
	}
	if (want("bloom")) {
		// same hard crash hit: BLOOM +1, BLOOM -1 (glide reversed), slow-motion bloom, instant bloom
		std::vector<float> w;
		struct V { double bloom, speed; };
		for (V v : {V{1, 1}, V{-1, 1}, V{1.5, 0.08}, V{1.5, 8}}) {
			Proto P; Controls c = defaults(1); c.bloom = v.bloom; P.init(1, c);
			P.E.xSpeed = v.speed;
			bool hit = false;
			append(w, P.run(4.5, [&](double t) { if (!hit) { P.E.strike(7.0); hit = true; } (void)t; }));
			printf("  bloom %.1f speed %.2f guard %d\n", v.bloom, v.speed, P.E.guardEvents);
		}
		writeWav(out + "/x-bloom.wav", w, SR);
	}
	if (want("morph")) {
		// crash -> china -> bell frequencies while a mallet rolls; shapes and couplings stay the crash's
		Proto P; Controls c = defaults(1); c.play = MALLET; c.hardness = 0.5; c.rate = 9; c.force = 2.0; c.position = 0.85; P.init(1, c);
		auto match = [&](int type) {
			GeomParams gp; gp.fromType(type); gp.sampleRate = 48000;
			auto B = buildModel(gp);
			std::map<std::pair<int, int>, std::vector<double>> tab;   // (m, par) -> ascending f
			for (auto& md : B->modes) tab[{md.m, md.par}].push_back(md.f);
			for (auto& kv : tab) std::sort(kv.second.begin(), kv.second.end());
			std::map<std::pair<int, int>, std::vector<int>> mine;
			for (int i = 0; i < P.E.N; i++) mine[{P.M->modes[i].m, P.M->modes[i].par}].push_back(i);
			std::vector<double> fb(P.E.N, 0.0);
			for (auto& kv : mine) {
				auto idx = kv.second;
				std::sort(idx.begin(), idx.end(), [&](int a, int b) { return P.f0[a] < P.f0[b]; });
				auto it = tab.find(kv.first);
				for (size_t k = 0; k < idx.size(); k++) {
					if (it != tab.end() && k < it->second.size()) fb[idx[k]] = it->second[k];
					else if (it != tab.end() && !it->second.empty()) fb[idx[k]] = P.f0[idx[k]] * it->second.back() / P.f0[idx[std::min(k, it->second.size() - 1)]];
					else fb[idx[k]] = P.f0[idx[k]];
				}
			}
			return fb;
		};
		auto fChina = match(7), fBell = match(9);
		int lastK = -1;
		auto w = P.run(16.0, [&](double t) {
			P.E.ctl.roll = t < 13.5;
			int k = (int)(t / 0.02);
			if (k == lastK) return;
			lastK = k;
			std::vector<double> f(P.E.N);
			double x = t < 6 ? t / 6 : 1.0, y = t < 6 ? 0.0 : std::min(1.0, (t - 6) / 6);
			for (int i = 0; i < P.E.N; i++) {
				double a = P.f0[i] * std::pow(fChina[i] / P.f0[i], x);
				f[i] = a * std::pow(fBell[i] / a, y);
			}
			P.setFreqs(f);
		});
		writeWav(out + "/x-morph.wav", w, SR);
		printf("morph ok guard %d\n", P.E.guardEvents);
	}
	if (want("invert")) {
		// spectrum mirrored about a pivot: the big slow shapes ring high, the fine ripples ring low
		std::vector<float> w;
		for (double piv : {0.0, 900.0, 2500.0}) {
			Proto P; P.init(4, defaults(4));
			if (piv > 0) {
				std::vector<double> f(P.E.N);
				for (int i = 0; i < P.E.N; i++) f[i] = std::min(30000.0, std::max(12.0, piv * piv / P.f0[i]));
				P.setFreqs(f);
			}
			int last = -1;
			append(w, P.run(5.0, [&](double t) { int k = (int)(t / 0.6); if (k != last && t < 2.0) { last = k; P.E.strike(2.5); } }));
		}
		writeWav(out + "/x-invert.wav", w, SR);
		printf("invert ok\n");
	}
	if (want("load")) {
		// a 40 g weight sliding from the bell to the edge and back while the ride rings
		Proto P; P.init(4, defaults(4));
		int last = -1, lastK = -1;
		auto w = P.run(10.0, [&](double t) {
			int k = (int)(t / 2.0);
			if (k != last && t < 8) { last = k; P.E.strike(3.0); }
			int kk = (int)(t / 0.01);
			if (kk == lastK) return;
			lastK = kk;
			double r = 0.55 + 0.4 * std::sin(2 * CY_PI * 0.35 * t);
			std::vector<double> f(P.E.N);
			double sPhi = P.E.sPhi;
			for (int i = 0; i < P.E.N; i++) {
				double ph = sPhi * P.M->phi(i, P.M->sh.r0 + r * (P.M->sh.a - P.M->sh.r0), 0.0, P.E.thOff[i]);
				f[i] = P.f0[i] / std::sqrt(1.0 + 0.04 * ph * ph);
			}
			P.setFreqs(f);
		});
		writeWav(out + "/x-load.wav", w, SR);
		printf("load ok\n");
	}
	if (want("rewind")) {
		// REWIND: at the gate the ring runs backwards (swap q and qp, losses negated, cascade paused,
		// energy capped at the peak since the strike), then flips forward again: a boomerang
		std::vector<float> w;
		for (double v : {2.0, 7.0}) {
			Proto P; P.init(1, defaults(1));
			bool hit = false, rev = false, done = false;
			double Epk = 0;
			append(w, P.run(7.0, [&](double t) {
				if (!hit) { P.E.strike(v); hit = true; }
				double En = P.E.energy();
				if (!rev) Epk = std::max(Epk, En);
				if (!rev && !done && t > 1.6) {
					rev = true;
					std::swap(P.E.q, P.E.qp);
					P.E.xSpeed = 0;
				}
				if (rev) { P.E.guardE = 0;
					double u = En < 0.8 * Epk ? 1.0 : 0.0;
					for (int i = 0; i < P.E.NA; i++) { P.E.sigBase[i] = u > 0 ? -P.sig0[i] : P.sig0[i]; P.E.applyLoss(i); }
					if (u == 0 || t > 3.4) { rev = false; done = true; P.E.xSpeed = 1; std::swap(P.E.q, P.E.qp); P.sustainOff(); }
				}
			}));
			printf("  rewind v %.0f guard %d\n", v, P.E.guardEvents);
		}
		writeWav(out + "/x-rewind.wav", w, SR);
	}
	return 0;
}
