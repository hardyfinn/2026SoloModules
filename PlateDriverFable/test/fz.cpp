// Plate Driver offline checks (no Rack needed): make test
// Every check prints PASS or FAIL. Demo WAVs go to the folder given as the first argument.
#include "../src/dsp/PlateEngine.hpp"
#include "../src/dsp/Mapping.hpp"
#include "scenes.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <complex>
#include <chrono>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdarg>
#include <functional>
using namespace plate;

static int nFail = 0, nPass = 0;
static void report(bool ok, const char* name, const std::string& detail) {
	std::printf("%s  %-34s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
	if (ok) nPass++; else nFail++;
}
static std::string fmt(const char* f, ...) {
	char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b;
}

// ------------------------------------------------------------------ WAV
static void writeWav(const std::string& path, const std::vector<float>& st, int sr) {
	FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
	uint32_t n = st.size() / 2, data = n * 2 * 3;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 6); w16(6); w16(24);
	std::fwrite("data", 1, 4, f); w32(data);
	for (float x : st) { int32_t v = (int32_t)std::lround(std::max(-1.0f, std::min(1.0f, x)) * 8388607.0); uint8_t b[3] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16)}; std::fwrite(b, 1, 3, f); }
	std::fclose(f);
}
static std::vector<float> readRaw(const std::string& path) {
	std::vector<float> v; FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return v;
	std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET); v.resize(n / 4); size_t got = std::fread(v.data(), 4, v.size(), f); v.resize(got); std::fclose(f); return v;
}

// ------------------------------------------------------------------ FFT + band levels
static void fft(std::vector<std::complex<double>>& a) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
	for (size_t len = 2; len <= n; len <<= 1) {
		double ang = -2 * PI / len; std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (size_t i = 0; i < n; i += len) { std::complex<double> w(1); for (size_t j = 0; j < len / 2; j++) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
	}
}
// power spectrum of mono x[from .. from+len) (averaged over 8192-point frames)
static std::vector<double> spectrum(const std::vector<double>& x, long from, long len, int nfft = 8192) {
	std::vector<double> P(nfft / 2 + 1, 0.0); int frames = 0;
	for (long s = from; s + nfft <= from + len && s + nfft <= (long)x.size(); s += nfft / 2) {
		std::vector<std::complex<double>> a(nfft);
		for (int i = 0; i < nfft; i++) a[i] = x[s + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
		fft(a); for (int i = 0; i <= nfft / 2; i++) P[i] += std::norm(a[i]); frames++;
	}
	if (frames) for (double& p : P) p /= frames;
	return P;
}
static std::vector<double> thirdOctaves(const std::vector<double>& P, double sr, int nfft = 8192) {
	std::vector<double> b;
	for (double fc = 50; fc <= 16000; fc *= std::pow(2.0, 1.0 / 3)) {
		double lo = fc * std::pow(2.0, -1.0 / 6), hi = fc * std::pow(2.0, 1.0 / 6), s = 0;
		for (int i = 0; i < (int)P.size(); i++) { double f = i * sr / nfft; if (f >= lo && f < hi) s += P[i]; }
		b.push_back(10 * std::log10(s + 1e-30));
	}
	return b;
}
// strongest component (optionally only between lo and hi Hz), parabolic interpolation
static double peakFreq(const std::vector<double>& x, long from, long len, double sr, double lo = 0, double hi = 1e9) {
	int nfft = 1; while (nfft * 2 <= len && nfft < (1 << 18)) nfft *= 2;
	std::vector<std::complex<double>> a(nfft);
	for (int i = 0; i < nfft; i++) a[i] = x[from + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
	fft(a); int bi = 1; double bm = 0;
	int i0 = std::max(1, (int)(lo * nfft / sr)), i1 = std::min(nfft / 2 - 1, (int)(hi * nfft / sr));
	for (int i = i0; i <= i1; i++) if (std::norm(a[i]) > bm) { bm = std::norm(a[i]); bi = i; }
	double l = std::log(std::norm(a[bi - 1]) + 1e-30), c = std::log(std::norm(a[bi]) + 1e-30), r = std::log(std::norm(a[bi + 1]) + 1e-30);
	double d = 0.5 * (l - r) / (l - 2 * c + r);
	return (bi + d) * sr / nfft;
}

// ------------------------------------------------------------------ engine-rate render of a scene
static std::vector<double> lastL, lastR;
struct Run { std::vector<float> st; std::vector<double> prT; long guards = 0; double secs = 0; };
static Run renderScene(const Scene& sc, double fe = 96000, int type = SHEET, int cast = 0) {
	Engine* e = new Engine(); e->setRate(fe); if (getenv("KMUL")) e->kmul = atof(getenv("KMUL")); e->build(type, cast); e->reset(); e->f1 = sc.f1;
	Run r; long n = (long)(sc.dur * fe); r.st.resize(2 * n);
	auto t0 = std::chrono::steady_clock::now();
	for (long i = 0; i < n; i++) {
		if (i % BLK == 0) sc.ctl(i / fe, *e);
		e->process(0.0);
		r.st[2 * i] = (float)e->outL; r.st[2 * i + 1] = (float)e->outR;
		if (e->meterN == 0) r.prT.push_back(e->pr);
	}
	r.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	r.guards = e->guards; delete e; return r;
}
static std::vector<double> monoOf(const std::vector<float>& st) { std::vector<double> m(st.size() / 2); for (size_t i = 0; i < m.size(); i++) m[i] = 0.5 * ((double)st[2 * i] + st[2 * i + 1]); return m; }

// ------------------------------------------------------------------ full module chain (knobs -> host-rate volts)
struct Chain {
	Engine* e; Age age; Decimator dec; Leveller lev; double host; bool os; int ctr = 0; double gateS = 1;
	double level = 0.7; bool ageOn = true;
	Chain(double host_, uint64_t seed = 1) : host(host_) {
		e = new Engine(); if (getenv("KMUL")) e->kmul = atof(getenv("KMUL")); os = host < 88200; e->setRate(os ? 2 * host : host); e->build(SHEET, 0); e->reset();
		age.init(seed); lev.setRate(host); dec.reset(); lev.reset();
	}
	~Chain() { delete e; }
	// call once per host sample; in = volts at IN; strike velocity > 0 triggers
	void step(const Knobs& k0, double inV, double strikeVel, double& L, double& R) {
		Knobs k = k0;
		if (ctr == 0) {
			if (ageOn) age.update(BLK / e->fe, e->emean);
			mapKnobs(k, *e, ageOn ? &age : nullptr);
		}
		if (++ctr >= BLK / (os ? 2 : 1)) ctr = 0;
		if (strikeVel > 0) e->strike(420 * strikeVel);
		double inS = inV * e->c.inLevel;
		double l2[2], r2[2];
		for (int s = 0; s < (os ? 2 : 1); s++) { e->process(inS); l2[s] = e->outL; r2[s] = e->outR; }
		double l, r;
		if (os) dec.process(l2, r2, l, r); else { l = l2[0]; r = r2[0]; }
		double g = lev.gain(l, r);
		double s = level * OUT_SCALE;
		L = 11 * std::tanh(l * g * s / 11); R = 11 * std::tanh(r * g * s / 11);
	}
	static double OUT_SCALE;
};
double Chain::OUT_SCALE = OUT_VOLTS;

static double GUARD = 1e9;
int main(int argc,char**argv){ int mode=atoi(argv[1]); GUARD = atof(argv[2]);

	// ---------------------------------------------------------- 10. fuzz (and 11: other sample rates)
	auto randomKnobs = [](Rng& g) {
		Knobs k; k.type = (int)(g.uni() * 4) % 4; k.cast = (int)(g.uni() * 16) % 16; k.size = 20 * std::pow(10.0, g.uni());
		k.thick = g.uni(); k.curve = g.uni(); k.stretch = 2 * g.uni(); k.decay = 0.15 + 3.85 * g.uni(); k.drive = g.uni();
		k.gain = 20 * std::pow(30.0, g.uni()); k.focus = g.uni(); k.pitch = -5 + 10 * g.uni(); k.trackPlate = g.uni() < 0.5;
		k.phase = g.uni(); k.driverPos = 0.05 + 0.9 * g.uni(); k.spread = 0.5 * g.uni(); k.grab = g.uni() < 0.3 ? g.uni() : 0; k.release = g.uni();
		k.gate = g.uni() < 0.8 ? 1 : 0; k.pick = g.uni(); k.amp = g.uni(); k.drv = g.uni(); k.rat = g.uni(); k.bleed = g.uni(); k.out = g.uni();
		k.pump = g.uni(); k.rate = g.uni(); k.mallet = g.uni(); k.inLevel = g.uni(); k.bright = g.uni(); k.age = g.uni();
		return k;
	};
	auto fuzz = [&](double host, int patches, double secs, const char* name, int mode) {
		std::atomic<int> next(0), bad(0); std::atomic<long> guards(0), softs(0); std::mutex mu; std::string det;
		auto worker = [&]() {
			for (;;) {
				int p = next++; if (p >= patches) break;
				if (getenv("PATCH") && p != atoi(getenv("PATCH"))) continue;
				Rng g; g.seed(1000 + p * 7919ULL);
				Chain ch(host, 77 + p); ch.e->guardE = GUARD; Knobs k = randomKnobs(g); k.type = p % 4; auto fix = [&](Knobs& q) { if (mode & 1) q.pitch = std::max(-3.0, std::min(3.0, q.pitch)); if (mode & 2) q.inLevel = 0; if (mode & 4) q.rat = 0; if (mode & 8) { q.pick = q.amp = q.drv = q.bleed = q.out = 0; } if (mode & 16) q.pump = 0; if (mode & 32) q.drive = std::min(q.drive, 0.5); if (mode & 64) q.thick = std::max(q.thick, 0.5); if (mode & 256) q.stretch = 1; if (mode & 512) q.phase = 0; if (mode & 1024) q.spread = 0; if (mode & 2048) q.age = 0; if (mode & 4096) q.decay = std::min(q.decay, 1.0); if (mode & 8192) q.gain = std::min(q.gain, 100.0); if (mode & 16384) q.pitch = std::max(-1.0, std::min(1.0, q.pitch)); }; fix(k);
				long n = (long)(secs * host), stepN = (long)(0.5 * host); double peak = 0; bool nanSeen = false; double inBurst = 0;
				for (long i = 0; i < n; i++) {
					if (i % stepN == 0 && i) { Knobs k2 = randomKnobs(g); k2.type = k.type; k2.cast = k.cast; if (g.uni() < 0.5) k = k2; else { k.drive = k2.drive; k.pitch = k2.pitch; k.focus = k2.focus; k.rat = k2.rat; } fix(k); }
					double sv = (!(mode & 128) && i % (long)(0.37 * host) == 0) ? g.uni() : 0;
					if (i % (long)(1.3 * host) == 0) inBurst = 0.2 * host;
					double inV = 0; if (inBurst > 0) { inBurst--; inV = 10 * g.bi(); }
					double L, R; long g0 = ch.e->guards; ch.step(k, inV, sv, L, R);
					if (g0 == 0 && ch.e->guards > 0 && getenv("TRACE")) { std::lock_guard<std::mutex> l(mu);
						std::printf("p%d t=%.3f type %d size %.0f thick %.2f curve %.2f stretch %.2f decay %.2f drive %.2f gain %.0f focus %.2f pitch %.2f track %d phase %.2f pos %.2f spread %.2f grab %.2f gate %.0f age %.2f in %.2f rat %.2f pump %.2f | E %.3g Smax %.3g cap %.3g tune %.3f cdrive %.3g w0 %.1f wN %.0f damp %.2f strike %.2f inV %.1f lamT2 %.3g nuT %.3g bloom %.2f\n",
						p, i / host, k.type, k.size, k.thick, k.curve, k.stretch, k.decay, k.drive, k.gain, k.focus, k.pitch, (int)k.trackPlate, k.phase, k.driverPos, k.spread, k.grab, k.gate, k.age, k.inLevel, k.rat, k.pump,
						ch.e->energy, ch.e->smaxSeen, ch.e->smax, ch.e->c.tune, ch.e->c.drive, ch.e->w[0], ch.e->w[N - 1], ch.e->c.damp, sv, inV, ch.e->c.bloom*ch.e->c.curve*ch.e->c.curve*ch.e->g2s*ch.e->T*ch.e->T, ch.e->c.bloom*ch.e->c.nu*ch.e->c.curve*ch.e->c.curve*ch.e->g2s*ch.e->T, ch.e->c.bloom);
						int idx[N]; for (int j = 0; j < N; j++) idx[j] = j; Engine* E = ch.e;
						std::sort(idx, idx + N, [&](int a, int b) { return E->q[a]*E->q[a]*E->w[a]*E->w[a] > E->q[b]*E->q[b]*E->w[b]*E->w[b]; });
						for (int t = 0; t < 6; t++) { int j = idx[t]; std::printf("  mode %d f %.0f on %.0f q %.3g e %.3g dW %.2f\n", j, E->w[j]/6.2832, E->on[j], E->q[j], 0.5*E->q[j]*E->q[j]*E->w[j]*E->w[j], E->dW[j]); }
						for (int cc = 0; cc < C; cc++) std::printf("  S%d Sbar %.3g Sprev %.3g\n", cc, E->Sbar[cc], E->Sprev[cc]);
						std::printf("  curve %.3g stretch %.3g eta %.3g bloom %.3g nu %.3g rat %.3g dx %.3g dv %.3g F? gain %.3g amp %.3g drv %.3g\n", E->c.curve, E->c.stretch, E->c.eta, E->c.bloom, E->c.nu, E->c.rat, E->dx, E->dv, E->c.gain, E->c.amp, E->c.drv); }
					if (!std::isfinite(L) || !std::isfinite(R)) nanSeen = true;
					peak = std::max({peak, std::fabs(L), std::fabs(R)});
				}
				if (getenv("TRACE")) {}
				guards += ch.e->guards; softs += ch.e->softN;
				if (nanSeen || peak > 11.0 || ch.e->guards) { bad++; std::lock_guard<std::mutex> l(mu); if (det.size() < 600) det += fmt("p%d t%d %s g%ld pitch %.1f drive %.2f size %.0f thick %.2f; ", p, k.type, nanSeen ? "NaN" : "", ch.e->guards, k.pitch, k.drive, k.size, k.thick); }
			}
		};
		std::vector<std::thread> th; int nt = std::max(1u, std::thread::hardware_concurrency());
		for (int i = 0; i < nt; i++) th.emplace_back(worker); for (auto& t : th) t.join();
		report(bad == 0, name, fmt("%d patches x %.0f s at %.1f kHz, %d bad, guard events %ld, ceiling %.2f%%. %s", patches, secs, host / 1000, (int)bad, (long)guards, 100.0 * softs / (patches * secs * host * 2), det.c_str()));
	};

fuzz(48000, getenv("NP") ? atoi(getenv("NP")) : 40, getenv("SECS") ? atof(getenv("SECS")) : 6, "fz", mode);
return 0;}
