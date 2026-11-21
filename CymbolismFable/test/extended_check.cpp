// Cymbolism Extended: offline checks of the UNREAL engine (no Rack).  make test
#include "../src/CyEngine.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
using namespace cy;

static Controls defaults(int type) {
	Controls c;
	c.play = CY_TYPES[type].play;
	c.position = CY_TYPES[type].position;
	c.mic = 1.0;
	return c;
}
static std::map<std::string, std::shared_ptr<Model>> cacheM;
static std::shared_ptr<Model> model(int type, int quality = 1) {
	std::string k = std::to_string(type) + "/" + std::to_string(quality);
	auto it = cacheM.find(k);
	if (it != cacheM.end()) return it->second;
	GeomParams gp;
	gp.fromType(type);
	gp.quality = quality;
	gp.sampleRate = 48000;
	auto M = buildModel(gp);
	cacheM[k] = M;
	return M;
}
typedef std::function<void(Engine&, double)> Script;
struct Run { std::vector<float> out; double peak = 0; int guard = 0; double secs = 0; };
static Run run(int type, Controls c, double dur, Script sc, int quality = 1) {
	Engine E;
	E.ctl = c;
	E.init(model(type, quality), 48000);
	Run r;
	int n = (int)(dur * 48000);
	r.out.resize(n);
	auto t0 = std::chrono::steady_clock::now();
	for (int s = 0; s < n; s += 32) {
		if (sc) sc(E, (double)s / 48000);
		E.process(nullptr, r.out.data() + s, std::min(32, n - s));
	}
	r.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	for (float v : r.out) r.peak = std::max(r.peak, (double)std::fabs(v));
	r.guard = E.guardEvents;
	return r;
}
static Script hitAt0(double v) {
	return [v](Engine& E, double t) { if (t == 0) E.strike(v); };
}
static int fails = 0;
static void check(bool ok, const char* what) {
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) fails++;
}

// fraction of spectral energy (100 Hz .. 8 kHz) within 20 cents of a target pitch
static double tunedFraction(const std::vector<float>& x, int from, int len, const Unreal& u) {
	std::vector<double> tg;
	tuneTargets(u, tg);
	int N = 1;
	while (N < len) N <<= 1;
	std::vector<std::complex<double>> X(N);
	for (int i = 0; i < len; i++) X[i] = x[from + i] * (0.5 - 0.5 * std::cos(2 * CY_PI * i / len));
	// iterative radix-2 FFT
	for (int i = 1, j = 0; i < N; i++) {
		int bit = N >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(X[i], X[j]);
	}
	for (int L = 2; L <= N; L <<= 1) {
		std::complex<double> wl = std::polar(1.0, -2 * CY_PI / L);
		for (int i = 0; i < N; i += L) {
			std::complex<double> w = 1;
			for (int j = 0; j < L / 2; j++) {
				auto a = X[i + j], b = X[i + j + L / 2] * w;
				X[i + j] = a + b;
				X[i + j + L / 2] = a - b;
				w *= wl;
			}
		}
	}
	double tot = 0, in = 0;
	for (int b = 1; b < N / 2; b++) {
		double f = (double)b * 48000 / N;
		if (f < 100 || f > 8000) continue;
		double e = std::norm(X[b]);
		tot += e;
		double s = 12 * std::log2(f / 261.6255653);
		if (std::fabs(nearestTarget(tg, s) - s) < 0.2) in += e;
	}
	return tot > 0 ? in / tot : 0;
}

int main(int argc, char** argv) {
	std::string only = argc > 1 ? argv[1] : "all";
	auto want = [&](const char* n) { return only == "all" || only == n; };

	if (want("x1")) {
		printf("X1 neutral UNREAL settings change nothing\n");
		// every unreal field at its default vs the same with the struct rebuilt explicitly
		for (int type : {1, 4})
			for (double v : {1.0, 7.0}) {
				Controls c = defaults(type);
				Run a = run(type, c, 2.0, hitAt0(v));
				Controls c2 = c;
				c2.u = Unreal();
				c2.u.pivot = 1234;   // PIVOT does nothing at WARP 1
				c2.u.colour = 300;   // COLOUR does nothing with SUSTAIN off
				c2.u.scale = SC_MAJOR;
				Run b = run(type, c2, 2.0, hitAt0(v));
				double d = 0;
				for (size_t i = 0; i < a.out.size(); i++) d = std::max(d, (double)std::fabs(a.out[i] - b.out[i]));
				char msg[128];
				snprintf(msg, sizeof msg, "%s v=%.0f max difference %.2g V", CY_TYPES[type].name, v, d);
				check(d <= 1e-6 * std::max(1e-3, a.peak), msg);
			}
	}
	if (want("x3")) {
		printf("X3 level of a medium hit across WARP and TUNE (peak 1..10 V, loudness within 14 dB of neutral)\n");
		auto rms = [](const Run& r) { double e = 0; size_t n = std::min<size_t>(r.out.size(), 48000); for (size_t i = 0; i < n; i++) e += r.out[i] * r.out[i]; return std::sqrt(e / n); };
		for (int type : {1, 4}) {
			Run ref = run(type, defaults(type), 1.0, hitAt0(3.0));
			for (double tune : {0.0, 1.0})
				for (double w : {-1.0, 0.0, 0.5, 1.0, 2.5}) {
					Controls c = defaults(type);
					c.u.warp = w;
					c.u.tune = tune;
					Run r = run(type, c, 1.0, hitAt0(3.0));
					double db = 20 * std::log10(rms(r) / rms(ref));
					char msg[160];
					snprintf(msg, sizeof msg, "%s WARP %+.1f TUNE %3.0f%%: peak %.2f V, loudness %+.1f dB, guard %d", CY_TYPES[type].name, w, tune * 100, r.peak, db, r.guard);
					check(r.peak >= 1.0 && r.peak <= 10.0 && std::fabs(db) <= 14 && r.guard == 0, msg);
				}
		}
	}
	if (want("x4")) {
		printf("X4 TUNE 100%% minor pentatonic, a soft hit (BLOOM pulls loud hits off pitch, as it should)\n");
		for (int type : {1, 4}) {
			Controls c = defaults(type);
			c.u.tune = 1.0;
			c.u.scale = SC_PENTMIN;
			c.u.root = 2;
			Run r = run(type, c, 2.0, hitAt0(1.0));
			double fr = tunedFraction(r.out, 24000, 65536, c.u);
			Controls c0 = defaults(type);
			Run r0 = run(type, c0, 2.0, hitAt0(1.0));
			double fr0 = tunedFraction(r0.out, 24000, 65536, c.u);
			char msg[128];
			snprintf(msg, sizeof msg, "%s: %.0f%% of the energy on scale notes (untuned %.0f%%)", CY_TYPES[type].name, fr * 100, fr0 * 100);
			check(fr >= 0.9, msg);
		}
	}
	if (want("x2")) {
		printf("X2 fuzz: random UNREAL settings, a hard hit each\n");
		CyRng r;
		r.seed(4242);
		int bad = 0, guards = 0;
		double worst = 0;
		int nRuns = only == "x2" ? 200 : 40;
		for (int k = 0; k < nRuns; k++) {
			int type = (int)(r.uni() * NUM_TYPES) % NUM_TYPES;
			Controls c = defaults(type);
			c.play = (int)(r.uni() * NUM_PLAYS) % NUM_PLAYS;
			c.bloom = -2 + 4 * r.uni();
			c.u.flow = std::exp2(-4.3 + 7.6 * r.uni());
			c.u.warp = -1 + 3.5 * r.uni();
			c.u.pivot = 100 * std::pow(50.0, r.uni());
			c.u.tune = r.uni() < 0.5 ? 0 : r.uni();
			c.u.scale = (int)(r.uni() * NUM_SCALES) % NUM_SCALES;
			c.u.root = (int)(r.uni() * 12);
			c.u.voct = r.uni() < 0.5 ? 0 : -3 + 6 * r.uni();
			c.u.tilt = -1 + 2 * r.uni();
			c.u.sustain = r.uni() < 0.7 ? 0 : r.uni();
			c.u.swell = 0.5 + 29.5 * r.uni();
			c.u.colour = 60 * std::pow(166.0, r.uni());
			c.u.freeze = r.uni() < 0.1;
			c.u.load = r.uni() < 0.5 ? 0 : 0.5 * r.uni();
			c.u.loadPos = r.uni();
			c.roll = r.uni() < 0.3;
			Run x = run(type, c, 3.0, [&](Engine& E, double t) {
				if (t == 0) E.strike(8.0);
				if (t > 1.5 && t < 1.6) { E.ctl.u.warp = std::max(-1.0, E.ctl.u.warp - 0.01); }   // a retune during the ring
			});
			bool ok = std::isfinite(x.peak) && x.guard == 0 && x.peak < 10.5;
			if (!ok) {
				bad++;
				printf("    run %d: type %d play %d bloom %.2f flow %.2f warp %.2f pivot %.0f tune %.2f voct %.2f tilt %.2f sus %.2f frz %d load %.2f: peak %.2f guard %d\n", k, type,
				       c.play, c.bloom, c.u.flow, c.u.warp, c.u.pivot, c.u.tune, c.u.voct, c.u.tilt, c.u.sustain, (int)c.u.freeze, c.u.load, x.peak, x.guard);
			}
			guards += x.guard;
			worst = std::max(worst, x.peak);
		}
		char msg[128];
		snprintf(msg, sizeof msg, "%d runs: %d bad, worst peak %.2f V, %d guard events", nRuns, bad, worst, guards);
		check(bad == 0, msg);
	}
	if (want("x5")) {
		printf("X5 SUSTAIN and FREEZE hold the energy\n");
		{
			Controls c = defaults(1);
			c.u.sustain = 0.6;
			c.u.swell = 8;
			c.u.colour = 2000;
			double Et = 0;
			double lo = 1e9, hi = 0, after = 0;
			int guard = 0;
			run(1, c, 26.0, [&](Engine& E, double t) {
				if (t == 0) E.strike(1.0);
				if (t >= 22.0) E.ctl.u.sustain = 0;
				Et = 0.36 * 0.5 * E.hitEnergy();
				double e = E.energy() / Et;
				if (t > 2.0 && t < 22.0) { lo = std::min(lo, e); hi = std::max(hi, e); }
				if (t > 25.9) after = e;
				guard = E.guardEvents;
			});
			char msg[160];
			snprintf(msg, sizeof msg, "SUSTAIN: energy %.2f..%.2f of target for 20 s, %.3f after release, guard %d", lo, hi, after, guard);
			check(lo > 0.8 && hi < 1.2 && after < 0.2 && guard == 0, msg);
		}
		{
			Controls c = defaults(1);
			c.bloom = 1.5;
			double E0 = 0, lo = 1e9, hi = 0;
			int guard = 0;
			run(1, c, 32.0, [&](Engine& E, double t) {
				if (t == 0) E.strike(5.0);
				if (t >= 1.0) E.ctl.u.freeze = true;
				if (t >= 1.0 && E0 == 0) E0 = E.energy() + E.vnl;
				if (t > 1.5) { double e = (E.energy() + E.vnl) / E0; lo = std::min(lo, e); hi = std::max(hi, e); }
				if (getenv("CY_TRACE") && std::fmod(t, 0.5) < 32.0 / 48000) printf("    t %.1f E/E0 %.3f\n", t, E0 > 0 ? E.energy() / E0 : 0.0);
				guard = E.guardEvents;
			});
			char msg[160];
			snprintf(msg, sizeof msg, "FREEZE: energy %.2f..%.2f of the frozen level for 30 s, guard %d", lo, hi, guard);
			check(lo > 0.95 && hi < 1.05 && guard == 0, msg);
		}
	}
	if (want("x7")) {
		printf("X7 stereo and PICKUP\n");
		auto runX = [&](int type, Controls c, double dur, std::vector<float>& L, std::vector<float>& R, std::vector<float>& C, std::vector<float>& W, std::vector<float>& V) {
			Engine E;
			E.ctl = c;
			E.init(model(type), 48000);
			int n = (int)(dur * 48000);
			L.assign(n, 0); R.assign(n, 0); C.assign(n, 0); W.assign(n, 0); V.assign(n, 0);
			for (int s = 0; s < n; s += 32) {
				if (s == 0) E.strike(3.0);
				Engine::Extra ex;
				ex.R = R.data() + s; ex.core = C.data() + s; ex.wash = W.data() + s; ex.env = V.data() + s;
				E.process(nullptr, L.data() + s, std::min(32, n - s), &ex);
			}
		};
		auto rmsv = [](const std::vector<float>& x) { double e = 0; for (float v : x) e += v * v; return std::sqrt(e / x.size()); };
		Controls c = defaults(1);
		std::vector<float> L, R, C, W, V;
		runX(1, c, 1.5, L, R, C, W, V);
		Run mono = run(1, c, 1.5, hitAt0(3.0));
		double dLR = 0, dM = 0, envMax = 0;
		for (size_t i = 0; i < L.size(); i++) { dLR = std::max(dLR, (double)std::fabs(L[i] - R[i])); dM = std::max(dM, (double)std::fabs(L[i] - mono.out[i])); envMax = std::max(envMax, (double)V[i]); }
		char msg[160];
		snprintf(msg, sizeof msg, "WIDTH 0: max |L-R| %.2g V, max |L-mono| %.2g V", dLR, dM);
		check(dLR < 1e-5 && dM < 1e-5, msg);
		snprintf(msg, sizeof msg, "CORE %.3f V rms, WASH %.3f V rms, ENV peak %.2f V", rmsv(C), rmsv(W), envMax);
		check(rmsv(C) > 0.01 && rmsv(W) > 0.01 && envMax > 0.5 && envMax <= 10.0, msg);
		c.u.width = 0.6;
		runX(1, c, 1.5, L, R, C, W, V);
		double corr = 0, eL = 0, eR = 0;
		for (size_t i = 0; i < L.size(); i++) { corr += L[i] * R[i]; eL += L[i] * L[i]; eR += R[i] * R[i]; }
		snprintf(msg, sizeof msg, "WIDTH 34 deg: L/R correlation %.2f, levels %.3f / %.3f V rms", corr / std::sqrt(eL * eR), std::sqrt(eL / L.size()), std::sqrt(eR / R.size()));
		check(corr / std::sqrt(eL * eR) < 0.95 && std::fabs(std::log(eL / eR)) < 1.0, msg);
		c.u.width = 0;
		c.u.pickup = 1.0;
		runX(1, c, 1.5, L, R, C, W, V);
		double db = 20 * std::log10(rmsv(L) / rmsv(mono.out));
		snprintf(msg, sizeof msg, "PIEZO: %+.1f dB against AIR on a medium hit", db);
		check(std::fabs(db) < 4, msg);
	}
	if (want("x6")) {
		printf("X6 PUMP: parametric growth above a threshold\n");
		int type = 1;
		Controls c = defaults(type);
		c.bloom = 0;   // the linear threshold
		Engine probe;
		probe.ctl = c;
		probe.init(model(type), 48000);
		int i0 = -1;
		for (int i = 0; i < probe.NC; i++) {
			double f = probe.wS[i] / (2 * CY_PI);
			if (f > 60 && std::fabs(model(type)->tensS[i]) > 0.3 && (i0 < 0 || f < probe.wS[i0] / (2 * CY_PI))) i0 = i;
		}
		double w = probe.wS[i0], sg = probe.sig[i0], si = model(type)->tensS[i0];
		double dTh = 4 * sg / (w * std::fabs(si));   // depth of w^2 modulation needed: eps > 4 sigma / w
		printf("  mode %d at %.1f Hz, loss %.2f /s, sensitivity %.2f, threshold depth %.4f (%.2f V)\n", i0, w / (2 * CY_PI), sg, si, dTh, dTh / 0.04);
		for (double mult : {0.5, 2.0}) {
			Engine E;
			E.ctl = c;
			E.init(model(type), 48000);
			E.q[i0] = E.qp[i0] = 1e-6;
			double a0 = 0, a1 = 0;
			int n = 48000 * 4;
			std::vector<float> L(32), P(32);
			for (int s = 0; s < n; s += 32) {
				for (int j = 0; j < 32; j++) P[j] = (float)(mult * dTh / 0.04 * std::cos(2 * w * (s + j) / 48000.0));
				Engine::Extra ex;
				ex.pump = P.data();
				E.process(nullptr, L.data(), 32, &ex);
				double amp = std::hypot(E.q[i0], (E.q[i0] - E.qp[i0]) * 48000 / w);
				if (s < 48000 / 2) a0 = std::max(a0, amp);
				if (s > n - 48000 / 2) a1 = std::max(a1, amp);
			}
			char msg[128];
			snprintf(msg, sizeof msg, "pump %.1fx threshold: amplitude x%.3g over 3.5 s", mult, a1 / a0);
			check(mult < 1 ? a1 < a0 : a1 > 3 * a0, msg);
		}
	}
	if (want("x9")) {
		printf("X9 retunes during a contact, a roll and a bow stroke\n");
		for (int play : {SHOULDER, RODS, MALLET, BRUSH, BOW}) {
			Controls c = defaults(1);
			c.play = play;
			c.roll = true;
			c.sizzle = 0.5;
			Run x = run(1, c, 2.0, [&](Engine& E, double t) {
				if (t == 0) E.strike(4.0);
				E.ctl.u.warp = 1.0 + 1.5 * std::sin(2 * CY_PI * 2 * t);   // sweeps -0.5 .. 2.5
				E.ctl.u.pivot = 400 + 300 * std::sin(2 * CY_PI * 3 * t);
				E.ctl.size = 1.0 + 0.4 * std::sin(2 * CY_PI * 1.3 * t);
			});
			char msg[128];
			snprintf(msg, sizeof msg, "%s: finite %d, peak %.2f V, guard %d", CY_PLAY_NAMES[play], (int)std::isfinite(x.peak), x.peak, x.guard);
			check(std::isfinite(x.peak) && x.peak < 10.5, msg);
		}
	}
	printf(fails ? "extended_check: %d FAILED\n" : "extended_check: all passed\n", fails);
	return fails ? 1 : 0;
}
