// Offline checks and demo renders for Sawstack.  No Rack needed:  make test
//   ./test/render OUTDIR [DEMODIR]
// Every check prints PASS or FAIL; the program exits non-zero if any fails.
// The check numbers follow section 7 of docs/BUILD-INSTRUCTIONS.md.
#include "../src/dsp/Voice.hpp"
#include "ref_bands.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <complex>
#include <chrono>
#include <functional>
#include <cmath>

using namespace sawstack;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& detail) {
	printf("%s  %-62s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
	if (!ok) failures++;
}
template <typename... A> static std::string fmt(const char* f, A... a) { char b[512]; snprintf(b, sizeof b, f, a...); return b; }

// ---------------------------------------------------------------- helpers
static void fft(std::vector<std::complex<double>>& a) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) {
		size_t bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (size_t len = 2; len <= n; len <<= 1) {
		double ang = -2 * PI / len;
		std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (size_t i = 0; i < n; i += len) {
			std::complex<double> w(1);
			for (size_t j = 0; j < len / 2; j++) {
				auto u = a[i + j], v = a[i + j + len / 2] * w;
				a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
			}
		}
	}
}
static std::vector<double> powerSpectrum(const std::vector<double>& x, size_t n) {   // last n samples, Hann
	std::vector<std::complex<double>> a(n);
	size_t off = x.size() - n;
	for (size_t i = 0; i < n; i++) a[i] = x[off + i] * (0.5 - 0.5 * std::cos(2 * PI * i / n));
	fft(a);
	std::vector<double> p(n / 2 + 1);
	for (size_t i = 0; i <= n / 2; i++) p[i] = std::norm(a[i]);
	return p;
}
static std::vector<double> bandLevels(const std::vector<double>& x, double sr) {
	const int n = 8192, hop = 4096;
	std::vector<double> P(n / 2 + 1, 0.0);
	int cnt = 0;
	std::vector<std::complex<double>> a(n);
	for (size_t s = 0; s + n <= x.size(); s += hop) {
		for (int i = 0; i < n; i++) a[i] = x[s + i] * (0.5 - 0.5 * std::cos(2 * PI * i / n));
		fft(a);
		for (int i = 0; i <= n / 2; i++) P[i] += std::norm(a[i]);
		cnt++;
	}
	std::vector<double> out;
	for (int k = REF_K0; k < REF_K0 + REF_NB; k++) {
		double c = 1000 * std::pow(2.0, k / 3.0), lo = c * std::pow(2.0, -1 / 6.0), hi = c * std::pow(2.0, 1 / 6.0);
		double s = 0;
		for (int i = 0; i <= n / 2; i++) { double f = i * sr / n; if (f >= lo && f < hi) s += P[i] / cnt; }
		out.push_back(10 * std::log10(s + 1e-20));
	}
	return out;
}
// frequency from upward crossings of a level, with linear interpolation, over the given span
static double freqByCrossings(const std::vector<double>& x, double sr, double level, size_t from) {
	double first = -1, last = -1; int n = 0;
	bool armed = false;
	for (size_t i = from + 1; i < x.size(); i++) {
		if (x[i] < level - 0.2) armed = true;
		if (armed && x[i - 1] < level && x[i] >= level) {
			double t = (i - 1) + (level - x[i - 1]) / (x[i] - x[i - 1]);
			if (first < 0) first = t; else n++;
			last = t; armed = false;
		}
	}
	return n > 0 ? n * sr / (last - first) : 0.0;
}
static double cents(double a, double b) { return 1200 * std::log2(a / b); }
static double rms(const std::vector<double>& x, size_t from = 0) {
	double s = 0; for (size_t i = from; i < x.size(); i++) s += x[i] * x[i];
	return std::sqrt(s / std::max<size_t>(1, x.size() - from));
}

// render the five oscillators at fs (2x) with fixed controls
static std::vector<std::vector<double>> renderOscs(Voice& v, double seconds, double pitch, double spread, double timbre, double rich, int voicing) {
	size_t n = (size_t)(seconds * v.fs);
	std::vector<std::vector<double>> o(NOSC, std::vector<double>(n));
	double buf[NOSC];
	for (size_t s = 0; s < n; s++) {
		if (s % 2 == 0) v.prepare(pitch, std::min(std::max(spread, 0.0), 1.0), voicingRatios(voicing));
		v.oscSub(timbre, rich, buf);
		for (int i = 0; i < NOSC; i++) o[i][s] = buf[i];
	}
	return o;
}

// ---------------------------------------------------------------- WAV
static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) { printf("cannot write %s\n", path.c_str()); return; }
	uint32_t n = (uint32_t)L.size(), data = n * 4, riff = 36 + data, sr32 = sr, br = sr * 4, fmtLen = 16;
	uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
	fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr32, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t)std::lround(std::max(-1.f, std::min(1.f, L[i])) * 32767), b = (int16_t)std::lround(std::max(-1.f, std::min(1.f, R[i])) * 32767);
		fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
	}
	fclose(f);
}

// ---------------------------------------------------------------- a whole module voice with envelopes (as src/Sawstack.cpp wires it)
struct Controls {   // all functions of time in seconds
	std::function<double(double)> pitch = [](double) { return 0.0; };
	std::function<double(double)> gate = [](double) { return 0.0; };
	std::function<double(double)> spread = [](double) { return 0.25; };
	std::function<double(double)> timbre = [](double) { return 1.0; };
	std::function<double(double)> rich = [](double) { return 0.0; };
	std::function<double(double)> amp;            // empty = envelope 2
	double richEnv = 0.0;                         // RICHNESS CV attenuverter on envelope 1
	double timbreEnv = 0.0;                       // extra: envelope 1 into TIMBRE (stands in for a patch cable)
	double spreadEnv = 0.0;                       // extra: envelope 1 into SPREAD
	double rise1 = 0.002, fall1 = 0.1, rise2 = 0.002, fall2 = 0.3;
	bool loop1 = false;                           // END 1 patched to GATE 1
	double age = 0.6, drive = 0.35;
	int voicing = OCTAVES;
	uint64_t seed = 1;
	bool mono = false;
	bool noise = true;                            // false: drift and jitter off
	double envAge = -1;                           // envelope AGE, -1 = same as the voice
};
static void renderScene(const Controls& c, double seconds, double sr, std::vector<double>& L, std::vector<double>& R) {
	Voice v; v.init(sr, c.seed, c.age, c.drive);
	if (!c.noise) { v.kdrift = 0; v.kjit = 0; v.age = -1; v.setAgeDrive(c.age, c.drive); }
	const double ea = c.envAge >= 0 ? c.envAge : c.age;
	Envelope e1, e2; e1.init(c.seed * 3 + 1); e2.init(c.seed * 3 + 2);
	size_t n = (size_t)(seconds * sr);
	L.assign(n, 0); R.assign(n, 0);
	bool end1 = false, end2 = false;
	for (size_t s = 0; s < n; s++) {
		double t = s / sr;
		double g = c.gate(t);
		double g1 = c.loop1 ? ((end1 || s == 0) ? 10.0 : 0.0) : g;
		double env1 = e1.process(g1, c.rise1, c.fall1, ea, sr, end1);
		double env2 = e2.process(g, c.rise2, c.fall2, ea, sr, end2);
		double amp = c.amp ? c.amp(t) : env2;
		double l, r;
		v.process(c.pitch(t), c.spread(t) + c.spreadEnv * env1, c.timbre(t) + c.timbreEnv * env1, c.rich(t) + c.richEnv * env1,
		          amp, c.voicing, c.mono, l, r);
		L[s] = l; R[s] = r;
	}
}

static std::function<double(double)> constant(double x) { return [x](double) { return x; }; }
static std::function<double(double)> ramp(double t0, double t1, double a, double b) {
	return [=](double t) { return t <= t0 ? a : t >= t1 ? b : a + (b - a) * (t - t0) / (t1 - t0); };
}
struct Seq {
	std::vector<int> notes; double step, frac;   // notes: semitones from C4; -999 = rest
	double pitch(double t) const {
		int i = std::min((int)(t / step), (int)notes.size() - 1);
		for (int j = i; j >= 0; j--) if (notes[j] != -999) return notes[j] / 12.0;
		return 0.0;
	}
	double gate(double t) const {
		int i = (int)(t / step);
		if (i >= (int)notes.size() || notes[i] == -999) return 0.0;
		return (t - i * step) < step * frac ? 10.0 : 0.0;
	}
};

// ---------------------------------------------------------------- checks
static void check1_unit() {
	double worst = 0;
	for (const RefUnit& ru : REF_UNITS) {
		Unit u = Unit::make(ru.seed);
		for (int i = 0; i < NOSC; i++) {
			double v[9] = {u.tune[i], u.vtrack[i], u.strack[i], u.shp[i], u.dc[i], u.lvl_db[i], u.lp_hz[i], u.droop[i], u.ph0[i]};
			for (int j = 0; j < 9; j++) worst = std::max(worst, std::fabs(v[j] - ru.v[i * 9 + j]) / std::max(1.0, std::fabs(ru.v[i * 9 + j])));
		}
	}
	check(worst < 1e-12, "1. unit numbers match the Python (seeds 1, 2, 12345)", fmt("worst difference %.2g", worst));
}

static double aliasFloor(double sr) {
	Voice v; v.init(sr, 1, 0.0, 0.0);
	auto o = renderOscs(v, 3.0, 2.0, 0.0, 1.0, 0.0, OCTAVES);   // C6
	const size_t n = 131072;
	auto P = powerSpectrum(o[2], n);
	double f1 = F0 * 4, df = v.fs / n, fund = 0, worst = 0;
	for (size_t i = 6; i < P.size(); i++) {
		double f = i * df;
		if (f >= 20000) break;
		double h = std::round(f / f1);
		bool harm = h >= 1 && std::fabs(f - h * f1) < 30;
		if (harm && h == 1) fund = std::max(fund, P[i]);
		if (!harm) worst = std::max(worst, P[i]);
	}
	return 10 * std::log10(fund / worst);
}
static void check2_alias(double sr, const char* label) {
	double d = aliasFloor(sr);
	check(d >= 55, fmt("2. alias floor, clean saw at C6 (%s)", label).c_str(), fmt("%.1f dB below the fundamental", d));
}

static void check3_tuning(double sr, const char* label) {
	double worst = 0;
	for (int oct = -3; oct <= 3; oct++) {
		Voice v; v.init(sr, 1, 0.0, 0.0);
		double secs = oct < -1 ? 4.0 : 1.5;
		auto o = renderOscs(v, secs, oct, 0.0, 0.0, 0.0, OCTAVES);
		double f = freqByCrossings(o[2], v.fs, 0.0, (size_t)(0.1 * v.fs));
		worst = std::max(worst, std::fabs(cents(f, F0 * std::pow(2.0, oct))));
	}
	check(worst < 0.1, fmt("3. tuning C1 to C7, AGE 0 (%s)", label).c_str(), fmt("worst %.4f cents", worst));
	Voice v; v.init(sr, 1, 0.0, 0.0);
	auto o = renderOscs(v, 2.0, 0.0, 1.0, 0.0, 0.0, OCTAVES);
	double w2 = 0;
	for (int i = 0; i < NOSC; i++) {
		double f = freqByCrossings(o[i], v.fs, 0.0, (size_t)(0.1 * v.fs));
		w2 = std::max(w2, std::fabs(cents(f, F0 * std::pow(2.0, i - 2))));
	}
	check(w2 < 0.1, fmt("3. SPREAD full gives exact octaves, AGE 0 (%s)", label).c_str(), fmt("worst %.4f cents", w2));
}

static double lockResidual(double ratioOct, double detune) {
	Voice v; v.init(48000, 3, 1.0, 0.0);
	v.overrideUnit = true; v.kdrift = 0; v.kjit = 0;
	double tune[5] = {-3.3, -4.1, 0.0, ratioOct + detune / 1200.0, -5.2};
	double ph0[5] = {0.1, 0.3, 0.0, 0.52, 0.9};
	for (int i = 0; i < NOSC; i++) {
		v.tune[i] = tune[i]; v.vtrack[i] = 1; v.strack[i] = 1; v.shp[i] = 1; v.dc[i] = 0; v.lvl[i] = 1;
		v.lpc[i] = std::exp(-2 * PI * 20000.0 / v.fs); v.droop[i] = 0; v.ph[i] = ph0[i];
	}
	v.age = -1; v.setAgeDrive(1.0, 0.0);
	auto o = renderOscs(v, 6.0, 0.0, 0.0, 1.0, 0.0, OCTAVES);
	size_t from = o[2].size() / 2;
	double fa = freqByCrossings(o[2], v.fs, 0.3, from), fb = freqByCrossings(o[3], v.fs, 0.3, from);
	return cents(fb / std::pow(2.0, ratioOct), fa);
}
static void check4_locking() {
	double u1 = lockResidual(0.0, 1.0), o1 = lockResidual(1.0, 1.0), u4 = lockResidual(0.0, 4.0);
	check(std::fabs(u1) < 0.1, "4. two oscillators 1.0 cent apart lock (AGE 1)", fmt("residual %+.3f cents", u1));
	check(std::fabs(o1) < 0.1, "4. an octave plus 1.0 cent locks (AGE 1)", fmt("residual %+.3f cents", o1));
	check(std::fabs(u4) > 3.0, "4. 4 cents apart stays apart (AGE 1)", fmt("residual %+.3f cents", u4));
	double f5 = lockResidual(0.5849625007211562, 0.5);
	printf("      (a fifth plus 0.5 cent: residual %+.3f cents)\n", f5);
}

static void check5_mono() {
	double lv[NOSC];
	for (int i = 0; i < NOSC; i++) {
		Voice v; v.init(48000, 1, 0.0, 0.0); v.soloMask = 1 << i;
		std::vector<double> x(48000);
		for (auto& s : x) { double l, r; v.process(0.0, 0.0, 1.0, 0.0, 1.0, OCTAVES, true, l, r); s = l; }
		lv[i] = 20 * std::log10(rms(x, 4800));
	}
	double lo = *std::min_element(lv, lv + NOSC), hi = *std::max_element(lv, lv + NOSC);
	check(hi - lo < 0.1, "5. mono: every oscillator at the same level", fmt("spread %.3f dB", hi - lo));
}

static void check6_envelopes() {
	const double sr = 48000;
	for (double age : {0.0, 0.6, 1.0}) {
		double sumR = 0, sumF = 0; int n = age == 0 ? 1 : 40;
		for (int k = 0; k < n; k++) {
			Envelope e; e.init(100 + k);
			bool end = false; int top = -1, endAt = -1;
			for (int s = 0; s < (int)(4 * sr); s++) {
				double g = (s >= 100 && s < 100 + (int)sr) ? 10.0 : 0.0;
				double v = e.process(g, 0.5, 2.0, age, sr, end);
				if (top < 0 && v >= 1.0) top = s;
				if (endAt < 0 && end) { endAt = s; break; }
			}
			sumR += (top - 100) / sr; sumF += (endAt - (100 + sr)) / sr;
		}
		double r = sumR / n, f = sumF / n;
		check(std::fabs(r / 0.5 - 1) < 0.01 && std::fabs(f / 2.0 - 1) < 0.01, fmt("6. envelope times true at AGE %.1f", age).c_str(),
		      fmt("rise %.4f s, fall %.4f s (mean of %d)", r, f, n));
	}
	// END patched to GATE loops
	{
		Envelope e; e.init(5); bool end = false; int cycles = 0; bool prevEnd = false;
		double g = 10.0;
		for (int s = 0; s < (int)(2 * sr); s++) {
			e.process(g, 0.05, 0.05, 0.6, sr, end);
			g = end ? 10.0 : 0.0;
			if (end && !prevEnd) cycles++;
			prevEnd = end;
		}
		check(cycles >= 15 && cycles <= 21, "6. END patched to GATE loops", fmt("%d cycles in 2 s at 50 ms + 50 ms", cycles));
	}
	// retrigger during the fall rises from the current level
	{
		Envelope e; e.init(6); bool end = false; double v = 0, atRetrig = 0, after = 0;
		for (int s = 0; s < (int)sr; s++) {
			double g = (s < 1000) ? 10.0 : (s >= 24000 && s < 25000) ? 10.0 : 0.0;
			v = e.process(g, 0.01, 1.0, 0.0, sr, end);
			if (s == 23999) atRetrig = v;
			if (s == 24001) after = v;
		}
		check(atRetrig > 0.3 && after > atRetrig && after < atRetrig + 0.01, "6. retrigger in the fall rises from the current level",
		      fmt("%.3f then %.3f", atRetrig, after));
	}
}

static void check7_closed() {
	for (double age : {0.6, 1.0}) {
		Voice v; v.init(48000, 1, age, 0.35);
		std::vector<double> x(96000);
		for (auto& s : x) { double l, r; v.process(-1.0, 0.2, 1.0, 0.0, 0.0, OCTAVES, false, l, r); s = l; }
		double r = rms(x, 24000);
		if (age == 0.6) check(r == 0.0, "7. closed VCA exactly silent at AGE 0.6", r == 0.0 ? "silent" : fmt("%.1f dBFS", 20 * std::log10(r)));
		else {
			double db = 20 * std::log10(r);
			check(db > -72 && db < -65, "7. closed VCA floor at AGE 1", fmt("%.1f dBFS", db));
		}
	}
}

static void check8_reference() {
	const double sr = 48000, secs = 10.0;
	struct S { double pitch, spread, timbre, rich; uint64_t seed; };
	S scenes[4] = {{-1.0, 0.0, 1.0, 0.0, 3}, {0.0, 1.0, 0.0, 0.0, 4}, {-2.0, 0.2, 1.0, 0.7, 6}, {-1.0, 0.28, 0.5, 0.0, 5}};
	for (int pass = 0; pass < 2; pass++) {
		bool noise = pass == 1;
		const RefBands* ref = noise ? REF_BANDS : REF_BANDS_DET;
		for (int k = 0; k < NUM_REF_BANDS; k++) {
			std::vector<double> L, R;
			Controls c; c.noise = noise;
			if (k < 4) {
				S s = scenes[k];
				c.pitch = constant(s.pitch); c.spread = constant(s.spread); c.timbre = constant(s.timbre); c.rich = constant(s.rich);
				c.amp = constant(1.0); c.seed = s.seed;
			} else {
				Seq q{{-24, -999, -24, -12, -24, -999, -21, -24, -24, -999, -24, -12, -17, -999, -19, -14}, 0.121, 0.55};
				c.pitch = [q](double t) { return q.pitch(std::fmod(t, 16 * q.step)); };
				c.gate = [q](double t) { return q.gate(std::fmod(t, 16 * q.step)); };
				c.spread = constant(0.26); c.timbre = constant(1.0); c.rich = constant(0.1); c.richEnv = 0.6;
				c.rise1 = 0.002; c.fall1 = 0.11; c.rise2 = 0.002; c.fall2 = 0.16; c.seed = 7;
				if (!noise) c.envAge = 0.0;
			}
			renderScene(c, secs, sr, L, R);
			std::vector<double> m(L.size());
			for (size_t i = 0; i < L.size(); i++) m[i] = 0.5 * (L[i] + R[i]);
			auto b = bandLevels(m, sr);
			double top = *std::max_element(ref[k].db, ref[k].db + REF_NB);
			double worst = 0, eC = 0, eR = 0; int wb = 0, used = 0;
			for (int j = 0; j < REF_NB; j++) {
				eC += std::pow(10.0, b[j] / 10); eR += std::pow(10.0, ref[k].db[j] / 10);
				if (ref[k].db[j] < top - 40) continue;      // leakage floor, not part of the sound
				used++;
				double d = std::fabs(b[j] - ref[k].db[j]);
				if (d > worst) { worst = d; wb = j; }
			}
			double tol = noise ? 3.0 : 0.5;
			double total = 10 * std::log10(eC / eR);
			check(worst < tol && std::fabs(total) < 0.5,
			      fmt("8. matches the reference, %s: %s", noise ? "noise on" : "noise off", ref[k].name).c_str(),
			      fmt("worst band %.2f dB at %.0f Hz (%d bands), total %+.2f dB", worst, 1000 * std::pow(2.0, (REF_K0 + wb) / 3.0), used, total));
		}
	}
}

static void check10_speed() {
	const double sr = 48000; const int voices = 16; const int n = (int)sr * 2;
	std::vector<Voice> v(voices);
	std::vector<Envelope> e(voices);
	for (int i = 0; i < voices; i++) { v[i].init(sr, 1 + i, 0.6, 0.35); e[i].init(i); }
	double acc = 0;
	OutFilter of; of.init(sr);
	auto t0 = std::chrono::steady_clock::now();
	for (int s = 0; s < n; s++) {
		double l2[2] = {0, 0}, r2[2] = {0, 0};
		for (int i = 0; i < voices; i++) {
			bool end; double env = e[i].process((s / 4800) % 2 ? 10.0 : 0.0, 0.01, 0.2, 0.6, sr, end);
			v[i].processOS(-1.0 + i * 0.1, 0.3, 0.8, 0.3 * env, env, OCTAVES, false, l2, r2);
		}
		double l, r; of.process(l2, r2, false, l, r); acc += l + r;
	}
	double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	double factor = (n / sr) / el;
	check(factor >= 3.0, "10. 16 voices at 48 kHz, faster than real time", fmt("x%.1f real time on this machine%s", factor, acc != acc ? " (NaN)" : ""));
}

static void check11_extremes() {
	bool bad = false;
	double vals[] = {-13, -12, -1, 0, 0.5, 1, 1.5, 12, 13};
	for (double p : vals) for (double sp : vals) for (double tb : {-1.0, 0.0, 0.5, 1.0, 2.0}) for (double rc : {-1.0, 0.0, 1.0, 2.0}) {
		Voice v; v.init(48000, 9, 1.0, 1.0);
		for (int s = 0; s < 400; s++) {
			double l, r; v.process(std::min(std::max(p, -10.0), 10.0), sp, tb, rc, 1.0, s % 3, s % 2, l, r);
			if (!std::isfinite(l) || !std::isfinite(r)) bad = true;
		}
	}
	Envelope e; e.init(1); bool end;
	for (int s = 0; s < 10000; s++) { double x = e.process((s % 7) * 4.0 - 12, 0.0005, 20.0, 1.0, 48000, end); if (!std::isfinite(x)) bad = true; }
	check(!bad, "11. no NaN or infinity at the extremes", bad ? "found one" : "clean");
}

// ---------------------------------------------------------------- demos
static std::vector<float> fadeOut(std::vector<double> x, double sr) {
	std::vector<float> y(x.size());
	size_t n = (size_t)(0.03 * sr);
	for (size_t i = 0; i < x.size(); i++) {
		double g = 1.0;
		if (i < n) g = (double)i / n;
		if (i + n > x.size()) g = (double)(x.size() - i) / n;
		y[i] = (float)(x[i] * g);
	}
	return y;
}
static void demos(const std::string& dir) {
	const double sr = 48000;
	std::vector<float> gL, gR;
	auto save = [&](const char* name, const Controls& c, double secs) {
		std::vector<double> L, R; renderScene(c, secs, sr, L, R);
		auto l = fadeOut(L, sr), r = fadeOut(R, sr);
		writeWav(dir + "/" + name + ".wav", l, r, (int)sr);
		gL.insert(gL.end(), l.begin(), l.end()); gR.insert(gR.end(), r.begin(), r.end());
		gL.insert(gL.end(), (size_t)(0.6 * sr), 0.f); gR.insert(gR.end(), (size_t)(0.6 * sr), 0.f);
		printf("      demo %s\n", name);
	};
	Controls c;
	c.amp = constant(1.0); c.pitch = constant(-1.0); c.spread = ramp(2, 13, 0, 1); c.timbre = constant(1.0); c.seed = 3;
	save("01-spread-sweep-saw", c, 16);
	c.pitch = constant(0.0); c.timbre = constant(0.0); c.seed = 4;
	save("02-spread-sweep-sine", c, 16);
	c.pitch = constant(-1.0); c.timbre = constant(0.6); c.voicing = FIFTHS;
	save("03-spread-sweep-fifths", c, 16);
	c.pitch = constant(0.585); c.timbre = constant(0.0); c.voicing = HARMONICS;
	save("04-spread-sweep-harmonics", c, 16);
	c = Controls(); c.amp = constant(1.0); c.pitch = constant(-1.0); c.spread = constant(0.28); c.timbre = ramp(1, 10, 0, 1); c.seed = 5;
	save("05-timbre-sweep", c, 12);
	c.pitch = constant(-2.0); c.spread = constant(0.2); c.timbre = constant(1.0);
	c.rich = [](double t) { return t < 1 ? 0 : t < 6 ? (t - 1) / 5 : t < 11 ? (11 - t) / 5 : 0; }; c.seed = 6;
	save("06-richness-sweep", c, 12);
	{
		Seq q{{-24, -999, -24, -12, -24, -999, -21, -24, -24, -999, -24, -12, -17, -999, -19, -14,
		       -24, -999, -24, -12, -24, -999, -21, -24, -24, -999, -24, -12, -17, -999, -19, -14}, 0.121, 0.55};
		c = Controls(); c.pitch = [q](double t) { return q.pitch(t); }; c.gate = [q](double t) { return q.gate(t); };
		c.spread = constant(0.26); c.rich = constant(0.1); c.richEnv = 0.6; c.fall1 = 0.11; c.fall2 = 0.16; c.seed = 7;
		save("07-sync-sweep-bass", c, 4.9);
	}
	{
		Seq q{{0, 3, 7, 10, 12, 10, 7, 3, -2, 2, 5, 9, 12, 9, 5, 2}, 0.5, 0.7};
		c = Controls(); c.pitch = [q](double t) { return q.pitch(t) - 1; }; c.gate = [q](double t) { return q.gate(t); };
		c.spread = constant(0.3); c.timbre = constant(0.35); c.timbreEnv = 0.55; c.rise1 = 0.3; c.fall1 = 0.6; c.rise2 = 0.02; c.fall2 = 0.9; c.seed = 8;
		save("08-chorale-lead", c, 10.5);
	}
	{
		Seq q{{0, -999, 7, 0, 3, -999, 10, 7, 0, -999, 5, 0, 3, 2, -999, -2, 0, -999, 7, 0, 3, -999, 10, 7, 0, -999, 5, 0, 3, 2, -999, -2}, 0.14, 0.4};
		c = Controls(); c.pitch = [q](double t) { return q.pitch(t) - 1; }; c.gate = [q](double t) { return q.gate(t); };
		c.spread = constant(0.93); c.timbre = constant(0.15); c.timbreEnv = 0.85; c.fall1 = 0.08; c.fall2 = 0.22; c.seed = 10;
		save("09-octave-stack-plucks", c, 6);
	}
	c = Controls(); c.amp = constant(1.0); c.pitch = constant(-1.0); c.spread = constant(0.15); c.spreadEnv = 0.85; c.timbre = constant(0.55);
	c.loop1 = true; c.rise1 = 2.0; c.fall1 = 2.0; c.seed = 9;
	save("10-looping-envelope-into-spread", c, 16);
	c = Controls(); c.amp = constant(1.0); c.pitch = constant(-2.0); c.seed = 11;
	c.spread = [](double t) { return 0.45 + 0.4 * std::sin(2 * PI * t / 19 - 1.2); };
	c.timbre = [](double t) { return 0.5 + 0.5 * std::sin(2 * PI * t / 11); };
	c.rich = [](double t) { return std::max(0.0, 0.35 * std::sin(2 * PI * t / 7.3)); };
	save("11-drone-all-moving", c, 24);
	for (int k = 0; k < 3; k++) {
		double ages[3] = {0.0, 0.6, 1.0}, drives[3] = {0.0, 0.35, 0.75};
		const char* names[3] = {"12a-unison-factory-fresh", "12b-unison-default", "12c-unison-tired-and-driven"};
		c = Controls(); c.amp = constant(1.0); c.pitch = constant(-1.0); c.spread = ramp(6, 10, 0, 0.3); c.age = ages[k]; c.drive = drives[k]; c.seed = 3;
		save(names[k], c, 12);
	}
	writeWav(dir + "/00-gallery.wav", gL, gR, (int)sr);
	// poly pad: four voices, one per chord note, each its own unit
	{
		const double secs = 12;
		size_t n = (size_t)(secs * sr);
		std::vector<double> L(n, 0), R(n, 0);
		int chords[3][4] = {{-12, -5, 0, 3}, {-14, -7, -2, 2}, {-17, -10, -5, 0}};
		for (int vix = 0; vix < 4; vix++) {
			Controls cc; cc.seed = 0x5A5A + vix * 0x9E3779B97F4A7C15ULL;
			cc.pitch = [=](double t) { int i = std::min(2, (int)(t / 4)); return chords[i][vix] / 12.0; };
			cc.gate = [](double t) { return std::fmod(t, 4.0) < 3.0 ? 10.0 : 0.0; };
			cc.spread = constant(0.3); cc.timbre = constant(0.8); cc.rise2 = 0.8; cc.fall2 = 1.2;
			std::vector<double> l, r; renderScene(cc, secs, sr, l, r);
			for (size_t i = 0; i < n; i++) { L[i] += 0.5 * l[i]; R[i] += 0.5 * r[i]; }
		}
		writeWav(dir + "/13-poly-pad.wav", fadeOut(L, sr), fadeOut(R, sr), (int)sr);
		printf("      demo 13-poly-pad\n");
	}
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	std::string demoDir = argc > 2 ? argv[2] : out;
	printf("Sawstack offline checks\n");
	check1_unit();
	check2_alias(48000, "48 kHz");
	check3_tuning(48000, "48 kHz");
	check4_locking();
	check5_mono();
	check6_envelopes();
	check7_closed();
	check8_reference();
	check2_alias(44100, "44.1 kHz"); check3_tuning(44100, "44.1 kHz");
	check2_alias(96000, "96 kHz"); check3_tuning(96000, "96 kHz");
	check10_speed();
	check11_extremes();
	{
		double worst = 0;
		for (double x = -4; x <= 4; x += 1e-4) worst = std::max(worst, std::fabs(fastTanh(x) - std::tanh(x)));
		check(worst < 0.001, "    fast tanh error over +-4", fmt("%.2g", worst));
	}
	if (failures == 0) demos(demoDir);
	printf("%s: %d failure%s\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
