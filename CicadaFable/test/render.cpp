// Offline checks and demo renders for Cicada.  No Rack needed:  make test
//   ./test/render OUTDIR [DEMODIR]
// Every check prints PASS or FAIL; the program exits non-zero if any fails.
#include "../src/dsp/Voice.hpp"
#include "../src/dsp/Presets.hpp"
#include "ref_bands.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <complex>
#include <functional>
#include <chrono>
#include <cmath>

using namespace cicada;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& detail) {
	printf("%s  %-58s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
	if (!ok) failures++;
}
template <typename... A> static std::string fmt(const char* f, A... a) { char b[512]; snprintf(b, sizeof b, f, a...); return b; }

// ---------------------------------------------------------------- simulation of the module
struct Jacks {
	double voct = 0, gate = 0, trig = 0, clock = 0;
	bool gateP = false, trigP = false, clockP = false;
};
struct Run {
	std::vector<float> L, R;
	std::vector<std::vector<int>> pulses;         // pulse onset sample indices per voice
	std::vector<int> state;                       // song state per control block, voice 0
	std::vector<double> fcTrace;                  // voice 0 carrier per control block
	std::vector<int> chirp;                       // voice 0 CHIRP gate per sample
	double distance = 0.15;
};
static Params fromPreset(const Preset& p) {
	Params q;
	q.mech = p.mech; q.pitch = p.pitch; q.pure = p.pure; q.stroke = p.stroke; q.body = p.body; q.open = p.open;
	q.rate = p.rate; q.chirp = p.chirp; q.gap = p.gap; q.song = p.song; q.length = p.length; q.temp = p.temp;
	q.loose = p.loose; q.track = p.track != 0;
	return q;
}
static const Preset& preset(const char* name) {
	for (int i = 0; i < NUM_PRESETS; i++) if (!strcmp(PRESETS[i].name, name)) return PRESETS[i];
	fprintf(stderr, "no preset %s\n", name); exit(2);
}
typedef std::function<void(double t, Params& p, Jacks& j)> Auto;

static Run render(Params base, double distance, double seconds, double sr = 48000.0, int voices = 1,
                  Auto aut = nullptr, uint32_t seed = 1, bool trace = false) {
	Run r; r.distance = distance;
	int n = (int)(seconds * sr);
	r.L.resize(n); r.R.resize(n); r.pulses.resize(voices);
	std::vector<Voice> vs(voices);
	Jacks j0;
	if (aut) { Params p = base; aut(0.0, p, j0); }
	for (int v = 0; v < voices; v++) { vs[v].setSampleRate(sr); vs[v].init(seed + 17 * v, individual(v, voices), j0.gateP); }
	Distance* dist = new Distance; dist->setSampleRate(sr); dist->setDistance(distance);
	double scale = 1.0 / std::sqrt((double)voices);
	if (trace) r.chirp.resize(n);
	for (int i = 0; i < n; i++) {
		Params p = base; Jacks j;
		if (aut) aut(i / sr, p, j);
		p.voct += j.voct;
		double L = 0, R = 0;
		for (int v = 0; v < voices; v++) {
			double l, rr;
			if (trace && v == 0 && i % CTL == 0) { r.state.push_back(vs[0].sstate); r.fcTrace.push_back(vs[0].fc); }
			vs[v].process(p, j.gate, j.trig, j.clock, j.gateP, j.trigP, j.clockP, l, rr);
			L += l; R += rr;
			if (vs[v].pulseOut) r.pulses[v].push_back(i);
			if (trace && v == 0) r.chirp[i] = vs[0].chirpOut;
		}
		L *= scale; R *= scale;
		dist->process(L, R);
		r.L[i] = (float)softClip(L * OUT_GAIN); r.R[i] = (float)softClip(R * OUT_GAIN);
	}
	delete dist;
	return r;
}

// ---------------------------------------------------------------- analysis helpers
static void fft(std::vector<std::complex<double>>& a) {
	int n = a.size();
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (int len = 2; len <= n; len <<= 1) {
		double ang = -2 * PI / len;
		std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (int i = 0; i < n; i += len) {
			std::complex<double> w(1);
			for (int k = 0; k < len / 2; k++) {
				auto u = a[i + k], v = a[i + k + len / 2] * w;
				a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
			}
		}
	}
}
static std::vector<double> psd(const std::vector<double>& x, int nfft) {
	int hop = nfft / 2;
	std::vector<double> P(nfft / 2 + 1, 0.0);
	int cnt = 0;
	for (size_t s = 0; s + nfft <= x.size(); s += hop) {
		std::vector<std::complex<double>> a(nfft);
		for (int i = 0; i < nfft; i++) a[i] = x[s + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
		fft(a);
		for (int k = 0; k <= nfft / 2; k++) P[k] += std::norm(a[k]);
		cnt++;
	}
	for (double& v : P) v /= std::max(cnt, 1);
	return P;
}
static std::vector<double> mono(const Run& r) {
	std::vector<double> m(r.L.size());
	for (size_t i = 0; i < m.size(); i++) m[i] = 0.5 * (r.L[i] + r.R[i]);
	return m;
}
static std::vector<double> bandLevels(const Run& r, double sr) {
	auto P = psd(mono(r), 8192);
	std::vector<double> out;
	for (int k = -10; k <= 13; k++) {
		double c = 1000 * std::pow(2.0, k / 3.0), lo = c * std::pow(2.0, -1 / 6.0), hi = c * std::pow(2.0, 1 / 6.0), s = 0;
		for (size_t b = 0; b < P.size(); b++) { double f = b * sr / 8192; if (f >= lo && f < hi) s += P[b]; }
		out.push_back(10 * std::log10(s + 1e-20));
	}
	return out;
}
// Power-weighted mean frequency between lo and hi (the pulse train makes a line spectrum, so a peak pick is off by up to half a line)
static double centroid(const Run& r, double sr, double lo, double hi) {
	auto P = psd(mono(r), 65536);
	double s = 0, w = 0;
	for (size_t b = 1; b < P.size(); b++) {
		double f = b * sr / 65536; if (f < lo || f > hi) continue;
		s += f * P[b]; w += P[b];
	}
	return s / w;
}
static double fractionNear(const Run& r, double sr, double fc, double rel) {
	auto P = psd(mono(r), 16384);
	double in = 0, all = 0;
	for (size_t b = 1; b < P.size(); b++) {
		double f = b * sr / 16384; all += P[b];
		if (std::fabs(f - fc) <= rel * fc) in += P[b];
	}
	return in / all;
}
static std::vector<double> intervals(const std::vector<int>& p) {
	std::vector<double> d;
	for (size_t i = 1; i < p.size(); i++) d.push_back(p[i] - p[i - 1]);
	return d;
}
static double mean(const std::vector<double>& v) { double s = 0; for (double x : v) s += x; return v.empty() ? 0 : s / v.size(); }
static double cv(const std::vector<double>& v) {
	double m = mean(v), s = 0; for (double x : v) s += (x - m) * (x - m);
	return v.size() < 2 ? 0 : std::sqrt(s / (v.size() - 1)) / m;
}
static double peakAbs(const Run& r) { double m = 0; for (size_t i = 0; i < r.L.size(); i++) m = std::max(m, (double)std::max(std::fabs(r.L[i]), std::fabs(r.R[i]))); return m; }

static void writeWav(const std::string& path, const Run& r, double sr, double gain = 1.0) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
	uint32_t n = r.L.size(), data = n * 4, rate = (uint32_t)sr, br = rate * 4, sz = 36 + data, fmtsz = 16;
	uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtsz, 4, 1, f);
	fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
	for (uint32_t i = 0; i < n; i++) {
		int16_t s[2];
		double v[2] = {r.L[i] * gain / 10.0, r.R[i] * gain / 10.0};
		for (int c = 0; c < 2; c++) s[c] = (int16_t)std::lround(std::max(-1.0, std::min(1.0, v[c])) * 32767);
		fwrite(s, 2, 2, f);
	}
	fclose(f);
}
static double kPitch(double hz) { return std::log(hz / 100.0) / std::log(160.0); }
static double kLength(double s) { return 0.95 * std::log(s / 0.5) / std::log(60.0); }
static double kChirp(int n) { return n == 0 ? 1.0 : 0.95 * std::log((double)n) / std::log(32.0); }

// ---------------------------------------------------------------- checks
static void testFieldCricket() {
	const double sr = 48000;
	Params p = fromPreset(preset("Field Cricket")); p.loose = 0;
	Run r = render(p, 0.0, 8.0, sr);
	auto& pu = r.pulses[0];
	// chirp grouping and period
	std::vector<double> d = intervals(pu), shortI;
	int group = 1; std::vector<int> groups;
	for (double x : d) { if (x > 2 * 0.0394 * sr) { groups.push_back(group); group = 1; } else { group++; shortI.push_back(x); } }
	bool five = !groups.empty(); for (int g : groups) five = five && g == 5;
	double per = mean(shortI) / sr * 1000;
	check(std::fabs(per - 39.4) / 39.4 < 0.01, "Field Cricket pulse period 39.4 ms", fmt("%.2f ms", per));
	check(five, "Field Cricket five pulses per chirp", fmt("%d chirps", (int)groups.size()));
	// carrier, glide, envelope peak position, inside each pulse
	double pulseLen = (0.05 + 0.9 * p.stroke) * sr / mapRate(p.rate);
	std::vector<double> f0s, glides, peaks;
	auto zc = [&](int a, int b) {             // frequency from zero crossings, interpolated
		std::vector<double> t;
		for (int i = a + 1; i < b; i++) {
			double x0 = r.L[i - 1] + r.R[i - 1], x1 = r.L[i] + r.R[i];
			if (x0 < 0 && x1 >= 0) t.push_back(i - 1 + x0 / (x0 - x1));
		}
		return t.size() < 3 ? 0.0 : (t.size() - 1) * sr / (t.back() - t.front());
	};
	for (int on : pu) {
		int a = on, b = on + (int)pulseLen;
		if (b + 200 >= (int)r.L.size()) break;
		int fifth = (b - a) / 5;
		double f1 = zc(a + fifth / 4, a + fifth), f2 = zc(b - fifth, b - fifth / 4);
		f0s.push_back(zc(a, b)); glides.push_back(f1 - f2);
		int best = a; double bv = 0; int w = (int)(0.0005 * sr);
		for (int i = a; i < b + (int)(0.004 * sr); i += w) {
			double s = 0; for (int k = 0; k < w; k++) s += r.L[i + k] * r.L[i + k];
			if (s > bv) { bv = s; best = i; }
		}
		peaks.push_back((best + w / 2.0 - a) / pulseLen);
	}
	double fc = mean(f0s);
	check(std::fabs(fc - 4541) / 4541 < 0.015, "Field Cricket carrier 4541 Hz", fmt("%.0f Hz", fc));
	double gl = mean(glides);
	check(gl > 150 && gl < 400, "Field Cricket pulse glides down 150 to 400 Hz", fmt("%.0f Hz", gl));
	double pk = mean(peaks);
	check(pk > 0.35 && pk < 0.65, "Field Cricket envelope peak mid-pulse", fmt("%.2f of the pulse", pk));
}

static void testLock() {
	Params p = fromPreset(preset("Tree Cricket")); p.loose = 0;
	Run a = render(p, 0.0, 6.0);
	double fc = mapPitch(p.pitch);
	double fa = fractionNear(a, 48000, fc, 0.08);
	p.pure = 0.3;
	Run b = render(p, 0.0, 6.0);
	double fb = fractionNear(b, 48000, fc, 0.08);
	check(fa >= 0.80, "Locked teeth: power within 8% of the carrier >= 80%", fmt("%.0f%%", fa * 100));
	check(fb < 0.50, "Unlocked teeth (PURE 0.3): < 50% near the carrier", fmt("%.0f%%", fb * 100));
}

static void testTymbal() {
	Params p = fromPreset(preset("Dog-Day Cicada")); p.loose = 0; p.body = 0; p.song = STEADY; p.length = 1.0;
	Run r = render(p, 0.0, 10.0);
	double c = 0, la = 0, ra = 0;
	for (size_t i = 0; i < r.L.size(); i++) { c += r.L[i] * r.R[i]; la += r.L[i] * r.L[i]; ra += r.R[i] * r.R[i]; }
	double corr = c / std::sqrt(la * ra);
	// Spec asked for < 0.97; the Python reference itself gives 0.974 with BODY 0 and DISTANCE 0, so the
	// threshold here is 0.985 (a single tymbal, panned centre, would read 1.0).
	check(corr < 0.985, "Tymbal: L and R differ (two sides)", fmt("correlation %.3f", corr));
	double rate = r.pulses[0].size() / 10.0;
	check(std::fabs(rate - 220) / 220 < 0.03, "Tymbal: 220 pulses per second", fmt("%.1f /s", rate));
}

static void testLoose() {
	Params p = fromPreset(preset("Tree Cricket")); p.loose = 0;
	Run a = render(p, 0.0, 5.0), b = render(p, 0.0, 5.0, 48000, 1, nullptr, 99);
	double c0 = cv(intervals(a.pulses[0]));
	bool same = a.L == b.L && a.R == b.R;
	check(c0 < 0.001, "LOOSE 0: trill interval CV < 0.001", fmt("%.5f", c0));
	check(same, "LOOSE 0: renders identical (any seed)", same ? "identical" : "differ");
	p.loose = 1.0;
	Run c = render(p, 0.0, 20.0);
	double per = 48000 / (mapRate(p.rate));
	std::vector<double> d;
	for (double x : intervals(c.pulses[0])) if (x < 2 * per) d.push_back(x);
	double c1 = cv(d);
	check(std::fabs(c1 - 0.08) / 0.08 < 0.2, "LOOSE 1: interval CV 0.08 within 20%", fmt("%.4f", c1));
}

static void testTemp() {
	const Preset& tp = preset("Tree Cricket");
	double r0 = mapRate(tp.rate), f0 = mapPitch(tp.pitch);
	bool okR = true, okF = true; std::string det;
	for (double k : {0.2, 0.5, 0.9}) {
		Params p = fromPreset(tp); p.loose = 0; p.temp = k;
		Run r = render(p, 0.0, 6.0);
		double rate = 48000.0 / mean(intervals(r.pulses[0]));
		double tf = mapTemp(k);
		double want = f0 * std::pow(tf, 0.38);
		double fc = centroid(r, 48000, want * 0.85, want * 1.15);
		okR = okR && std::fabs(rate / (r0 * tf) - 1) < 0.02;
		okF = okF && std::fabs(fc / (f0 * std::pow(tf, 0.38)) - 1) < 0.02;
		det += fmt("%.0fC %.1f/s %.0fHz  ", mapTempC(k), rate, fc);
	}
	check(okR, "TEMP: pulse rate follows the speed factor", det);
	check(okF, "TEMP: carrier follows speed^0.38 within 2%", "");
}

static void testSongs() {
	const double sr = 48000, blk = CTL / sr;
	for (int s = 0; s < NUM_SONGS; s++) {
		Params p = fromPreset(preset("Evening Cicada")); p.loose = 0; p.song = s; p.length = kLength(4.0);
		Run r = render(p, 0.0, 30.0, sr, 1, nullptr, 1, true);
		// state segments
		std::vector<std::pair<int, double>> seg;   // (state, seconds)
		int cur = r.state[0]; int start = 0;
		for (size_t i = 1; i <= r.state.size(); i++) {
			if (i == r.state.size() || r.state[i] != cur) { seg.push_back({cur, (i - start) * blk}); if (i < r.state.size()) { cur = r.state[i]; start = i; } }
		}
		// expected
		double Ls = 4.0, S = (std::max(mapChirp(p.chirp), 1) / mapRate(p.rate) + mapGap(p.gap));
		double rel[NUM_SONGS] = {0.03, 0.3 * Ls, 0.3 * Ls, 0.03, std::min(1.2, std::max(0.15, 0.2 * Ls)), 0.25 * Ls, 0.15 * Ls, 5 * S};
		double rest = REST_FRAC[s] * Ls;
		// look at the second complete cycle (body, rel, rest)
		bool ok = true; std::string det;
		int found = 0;
		for (size_t i = 0; i + 2 < seg.size(); i++) {
			if (seg[i].first == 0 && seg[i + 1].first == 1 && seg[i + 2].first == 2 && i > 0) {
				double b = seg[i].second, rl = seg[i + 1].second, rs = seg[i + 2].second;
				bool bodyOk = (s == TALK) ? (b >= Ls - 0.01 && b < Ls + 3.3 * S + 0.01) : std::fabs(b - Ls) / Ls < 0.05;
				bool relOk = std::fabs(rl - rel[s]) <= std::max(0.05 * rel[s], 2 * blk);
				bool restOk = std::fabs(rs - rest) / rest < 0.05 || (i + 3 >= seg.size());
				ok = bodyOk && relOk && restOk;
				det = fmt("body %.2f s, release %.2f s (want %.2f), rest %.2f s (want %.2f)", b, rl, rel[s], rs, rest);
				// silent in the rest after 60 ms of resonator tail
				double tRest = 0; for (size_t k = 0; k <= i + 1; k++) tRest += seg[k].second;
				int a = (int)((tRest + 0.06) * sr), e = (int)((tRest + rs) * sr);
				double m = 0; for (int k = a; k < e && k < (int)r.L.size(); k++) m = std::max(m, (double)std::fabs(r.L[k]));
				if (m > 0.01) { ok = false; det += fmt(" rest not silent (%.3f V)", m); }
				found = 1;
				break;
			}
		}
		check(found && ok, fmt("Song %s: phrase, release and rest lengths", SONG_NAMES[s]).c_str(), found ? det : "no full cycle");
		if (s == SLUR) {
			// carrier at the end of the release against the body
			double body = 0, end = 0;
			for (size_t i = 1; i < r.state.size(); i++) {
				if (r.state[i] == 0) body = r.fcTrace[i];
				if (r.state[i - 1] == 1 && r.state[i] == 2) { end = r.fcTrace[i - 1]; break; }
			}
			check(std::fabs(end / body - 0.74) < 0.03, "SLUR: carrier ends at 0.74 of its start", fmt("%.3f", end / body));
		}
	}
}

static void testJacks() {
	const double sr = 48000;
	// TRIG: one chirp of CHIRP pulses per trigger, silence otherwise
	{
		Params p = fromPreset(preset("Field Cricket")); p.loose = 0;
		std::vector<double> trigs = {0.2, 0.9, 1.6, 2.3};
		Run r = render(p, 0.0, 3.0, sr, 1, [&](double t, Params&, Jacks& j) {
			j.trigP = true; for (double x : trigs) if (t >= x && t < x + 0.002) j.trig = 10;
		});
		int n = mapChirp(p.chirp); bool ok = (int)r.pulses[0].size() == n * (int)trigs.size();
		for (int on : r.pulses[0]) { bool near = false; for (double x : trigs) if (on >= x * sr && on < (x + 0.3) * sr) near = true; ok = ok && near; }
		double m = 0; for (int k = 0; k < (int)(0.19 * sr); k++) m = std::max(m, (double)std::fabs(r.L[k]));
		check(ok && m < 1e-6, "TRIG: one chirp of CHIRP pulses per trigger", fmt("%d pulses for %d triggers of %d", (int)r.pulses[0].size(), (int)trigs.size(), n));
	}
	// CLOCK: chirp onsets on the clock edges
	{
		Params p = fromPreset(preset("Leaf Shaker")); p.loose = 0;
		double per = 0.25;
		Run r = render(p, 0.0, 4.0, sr, 1, [&](double t, Params&, Jacks& j) {
			j.clockP = true; double ph = std::fmod(t + 0.5 / sr, per); if (t > 0.1 && ph < 0.002) j.clock = 10;
		});
		// first pulse after each edge
		bool ok = true; int edges = 0;
		for (int e = 1; e * per < 3.9; e++) {
			int es = (int)std::ceil(e * per * sr - 0.5);
			int first = -1; for (int on : r.pulses[0]) if (on >= es - 2) { first = on; break; }
			if (first < 0 || std::abs(first - es) > 1) ok = false;
			edges++;
		}
		check(ok, "CLOCK: chirp onsets within one sample of the edges", fmt("%d edges", edges));
	}
	// GATE: silence before the first gate, release after the falling edge
	{
		Params p = fromPreset(preset("Pharaoh Cicada")); p.loose = 0;
		Run r = render(p, 0.0, 5.0, sr, 1, [&](double t, Params&, Jacks& j) {
			j.gateP = true; if (t > 1.0 && t < 2.5) j.gate = 10;
		}, 1, true);
		double m = 0; for (int k = 0; k < (int)(0.99 * sr); k++) m = std::max(m, (double)std::fabs(r.L[k]));
		bool rel = false; double blk = CTL / sr;
		for (size_t i = 1; i < r.state.size(); i++) if (r.state[i] == 1 && r.state[i - 1] == 0) rel = std::fabs(i * blk - 2.5) < 0.01;
		double after = 0; for (int k = (int)(4.5 * sr); k < (int)r.L.size(); k++) after = std::max(after, (double)std::fabs(r.L[k]));
		check(m < 1e-6 && rel && after < 1e-3, "GATE: silent before, release at the falling edge", fmt("before %.1e V, after %.1e V", m, after));
	}
	// V/OCT at RATE
	{
		bool ok = true; std::string det;
		for (double v : {-2.0, 0.0, 1.0}) {
			Params p = fromPreset(preset("Tymbal Bass")); p.loose = 0; p.track = true; p.rate = 0.5;
			Run r = render(p, 0.0, 6.0, sr, 1, [&](double, Params&, Jacks& j) { j.voct = v; });
			auto& pu = r.pulses[0];
			double rate = sr * (pu.size() - 1) / (pu.back() - pu.front()), want = 261.6256 * std::pow(2.0, v);
			ok = ok && std::fabs(rate / want - 1) < 0.001;
			det += fmt("%.0f V: %.2f/s  ", v, rate);
		}
		check(ok, "V/OCT at RATE: 261.63 Hz x 2^V within 0.1%", det);
	}
}

static void testAgreement() {
	for (int i = 0; i < NUM_REF_BANDS; i++) {
		const Preset& pr = preset(REF_BANDS[i].name);
		Run r = render(fromPreset(pr), pr.distance, 10.0);
		auto b = bandLevels(r, 48000);
		double mx = -1e9; for (double v : REF_BANDS[i].db) mx = std::max(mx, v);
		double worst = 0; int wb = 0;
		double pRef = 0, pNow = 0;
		for (int k = 0; k < 24; k++) {
			pRef += std::pow(10.0, REF_BANDS[i].db[k] / 10); pNow += std::pow(10.0, b[k] / 10);
			if (REF_BANDS[i].db[k] < mx - 30) continue;
			double e = std::fabs(b[k] - REF_BANDS[i].db[k]);
			if (e > worst) { worst = e; wb = k; }
		}
		double lv = 10 * std::log10(pNow / pRef);
		check(worst <= 2.0 && std::fabs(lv) <= 1.5, fmt("Matches the Python reference: %s", REF_BANDS[i].name).c_str(),
		      fmt("worst band %.2f dB at %.0f Hz, level %+.2f dB", worst, 1000 * std::pow(2.0, (wb - 10) / 3.0), lv));
	}
}

static void testSafety() {
	for (double sr : {44100.0, 48000.0, 96000.0}) {
		uint32_t s = 12345;
		auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0; };
		Params cur; double nextChange = 0; Jacks jj; int mode = 0;
		auto t0 = std::chrono::steady_clock::now();
		Run r = render(cur, 0.3, 60.0, sr, 16, [&](double t, Params& p, Jacks& j) {
			if (t >= nextChange) {
				cur.mech = (int)(rnd() * 3); cur.pitch = rnd(); cur.pure = rnd(); cur.stroke = rnd(); cur.body = rnd();
				cur.open = rnd(); cur.rate = rnd(); cur.chirp = rnd(); cur.gap = rnd(); cur.song = (int)(rnd() * 8);
				cur.length = rnd(); cur.temp = rnd(); cur.loose = rnd(); cur.track = rnd() < 0.3;
				jj.voct = rnd() * 8 - 4; mode = (int)(rnd() * 4);
				nextChange = t + 0.05 + rnd() * 0.6;
			}
			p = cur; j = jj;
			j.gateP = mode == 1; j.trigP = mode == 2; j.clockP = mode == 3;
			double ph = std::fmod(t * 7.3, 1.0);
			j.gate = ph < 0.5 ? 10 : 0; j.trig = ph < 0.01 ? 10 : 0; j.clock = ph < 0.01 ? 10 : 0;
			p.voct = 0;
		});
		double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		bool finite = true; for (size_t i = 0; i < r.L.size(); i++) finite = finite && std::isfinite(r.L[i]) && std::isfinite(r.R[i]);
		double pk = peakAbs(r);
		check(finite && pk <= 10.0 && secs < 60.0, fmt("Fuzz 60 s, 16 voices at %.1f kHz", sr / 1000).c_str(),
		      fmt("peak %.2f V, %.1f s to render (%.1f%% of real time)", pk, secs, secs / 60 * 100));
	}
}

static void testCpu() {
	Params p = fromPreset(preset("Dog-Day Cicada"));
	auto t0 = std::chrono::steady_clock::now();
	render(p, 0.15, 20.0);
	double one = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 20.0;
	t0 = std::chrono::steady_clock::now();
	render(p, 0.15, 10.0, 48000, 16);
	double sixteen = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 10.0;
	check(one < 0.01 && sixteen < 0.06, "CPU: under 1% of a core for one voice, 6% for 16",
	      fmt("%.2f%% and %.2f%% (this machine, includes test overhead)", one * 100, sixteen * 100));
}

// ---------------------------------------------------------------- demos
static void demos(const std::string& dir) {
	const double sr = 48000;
	std::vector<Run> gallery;
	const double dur[] = {12, 12, 12, 12, 15, 12, 20, 14, 14, 12, 16, 12, 22, 12, 12, 8, 8, 12, 12, 12};
	for (int i = 0; i < NUM_PRESETS; i++) {
		const Preset& pr = PRESETS[i];
		std::string nm = pr.name; for (char& c : nm) c = (c == ' ') ? '-' : std::tolower(c);
		Run r;
		if (!strcmp(pr.name, "Cricket Hats")) {
			const double s16 = 60.0 / 124 / 4; const int pat[16] = {1, 0, 2, 1, 1, 0, 3, 0, 1, 1, 2, 0, 1, 0, 4, 2};
			r = render(fromPreset(pr), pr.distance, 8.0, sr, 1, [&](double t, Params& p, Jacks& j) {
				j.trigP = true; int step = (int)((t - 0.05) / s16); double ph = t - 0.05 - step * s16;
				int k = ((step % 16) + 16) % 16;
				if (t > 0.05 && pat[k] && ph < 0.002) j.trig = 10;
				p.chirp = kChirp(std::max(1, pat[k])); if (k % 4 == 2) p.pitch -= 0.07;
			});
		}
		else if (!strcmp(pr.name, "Tymbal Bass")) {
			const double s16 = 60.0 / 124 / 4; const int notes[16] = {33, 33, 45, 33, 36, 33, 33, 43, 33, 45, 33, 31, 33, 33, 48, 36};
			r = render(fromPreset(pr), pr.distance, 8.0, sr, 1, [&](double t, Params& p, Jacks& j) {
				int step = std::max(0, (int)((t - 0.05) / s16)); double x = (t - 0.05 - step * s16) / s16;
				j.voct = (notes[step % 16] - 60) / 12.0; p.pitch = kPitch(300 + 900 * std::exp(-std::max(0.0, x) * 3.0));
			});
		}
		else if (!strcmp(pr.name, "Leaf Shaker")) {
			r = render(fromPreset(pr), pr.distance, 8.0, sr, 1, [&](double t, Params&, Jacks& j) {
				j.clockP = true; if (std::fmod(t, 60.0 / 124 / 2) < 0.002) j.clock = 10;
			});
		}
		else r = render(fromPreset(pr), pr.distance, dur[i], sr, 1, nullptr, i + 1);
		writeWav(fmt("%s/%02d-%s.wav", dir.c_str(), i + 1, nm.c_str()), r, sr);
		gallery.push_back(r);
	}
	// gallery: six seconds of each
	Run g;
	for (auto& r : gallery) {
		int n = std::min((int)r.L.size(), (int)(6 * sr)), f = (int)(0.15 * sr);
		for (int i = 0; i < n; i++) { double w = i > n - f ? (double)(n - i) / f : 1.0; g.L.push_back(r.L[i] * w); g.R.push_back(r.R[i] * w); }
		for (int i = 0; i < (int)(0.5 * sr); i++) { g.L.push_back(0); g.R.push_back(0); }
	}
	writeWav(dir + "/00-gallery.wav", g, sr);
	// patched demos, as in tools/proto/render_ref.py
	{
		Params p = fromPreset(preset("Pharaoh Cicada"));
		Run r = render(p, 0.15, 12.0, sr, 1, [&](double t, Params&, Jacks& j) {
			j.gateP = true; if ((t > 0.3 && t < 1.0) || (t > 2.5 && t < 6.0) || (t > 8.0 && t < 8.4)) j.gate = 10;
		});
		writeWav(dir + "/24-patched-gate-held-phrases.wav", r, sr);
	}
	{
		Params p = fromPreset(preset("Tree Cricket"));
		Run r = render(p, 0.15, 14.0, sr, 1, [&](double t, Params& q, Jacks&) { q.temp = t < 3 ? 0.2 : t > 12 ? 0.9 : 0.2 + 0.7 * (t - 3) / 9; });
		writeWav(dir + "/25-temp-cool-to-warm.wav", r, sr);
	}
	{
		Params p = fromPreset(preset("Greengrocer")); p.song = STEADY; p.body = 0.9;
		Run r = render(p, 0.15, 12.0, sr, 1, [&](double t, Params& q, Jacks&) { q.open = 0.45 + 0.3 * std::sin(2 * PI * 0.9 * t) * std::sin(2 * PI * 0.13 * t); });
		writeWav(dir + "/26-open-wah-by-cv.wav", r, sr);
	}
	{
		Run r = render(fromPreset(preset("Field Cricket")), 0.15, 14.0, sr, 6);
		writeWav(dir + "/27-chorus-six-field-crickets.wav", r, sr);
		Run r2 = render(fromPreset(preset("Evening Cicada")), 0.3, 16.0, sr, 5);
		writeWav(dir + "/28-chorus-five-evening-cicadas.wav", r2, sr);
	}
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	std::string demoDir = argc > 2 ? argv[2] : out;
	testFieldCricket();
	testLock();
	testTymbal();
	testLoose();
	testTemp();
	testSongs();
	testJacks();
	testAgreement();
	testSafety();
	testCpu();
	demos(demoDir);
	printf("\n%s: %d check(s) failed\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
