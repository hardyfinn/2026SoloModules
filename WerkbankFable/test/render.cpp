// Offline checks and demo renders for Werkbank. No Rack needed:  make test
//   ./test/render OUTDIR
// Every check prints PASS or FAIL; the program exits non-zero if any fails.
// Check numbers follow section 9 of docs/BUILD-INSTRUCTIONS.md.
#include "../src/dsp/Zap.hpp"
#include "../src/dsp/Blech.hpp"
#include "../src/dsp/Dynamo.hpp"
#include "../src/dsp/Orchester.hpp"
#include "../src/dsp/Vokoder.hpp"
#include "../src/dsp/Sprecher.hpp"
#include "../src/dsp/Takt.hpp"
#include "ref_data.h"
#include "parse_cases.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <complex>
#include <chrono>
#include <functional>
#include <map>

using namespace wb;
typedef std::vector<double> Sig;

static int failures = 0;
static void check(bool ok, const std::string& what, const std::string& detail) {
	printf("%s  %-58s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
	if (!ok) failures++;
}
template <typename... A> static std::string fmt(const char* f, A... a) { char b[1024]; snprintf(b, sizeof b, f, a...); return b; }

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
// exact-length DFT by Bluestein's chirp-z (the reference uses numpy's exact-length rfft)
static std::vector<std::complex<double>> dftExact(const Sig& x) {
	size_t N = x.size(), M = 1;
	while (M < 2 * N - 1) M <<= 1;
	std::vector<std::complex<double>> w(N), a(M), b(M);
	for (size_t n = 0; n < N; n++) {
		double ang = PI * (double)((n * n) % (2 * N)) / (double)N;
		w[n] = std::complex<double>(std::cos(ang), -std::sin(ang));
	}
	for (size_t n = 0; n < N; n++) a[n] = x[n] * w[n];
	b[0] = std::conj(w[0]);
	for (size_t n = 1; n < N; n++) b[n] = b[M - n] = std::conj(w[n]);
	fft(a); fft(b);
	for (size_t k = 0; k < M; k++) a[k] *= b[k];
	for (auto& v : a) v = std::conj(v);
	fft(a);
	std::vector<std::complex<double>> X(N / 2 + 1);
	for (size_t k = 0; k <= N / 2; k++) X[k] = std::conj(a[k]) / (double)M * w[k];
	return X;
}
// third-octave band levels exactly as make_reference.py: dB re 1 unit RMS
static std::vector<double> bands(const Sig& x, double fs) {
	auto X = dftExact(x);
	size_t N = x.size();
	std::vector<double> out;
	for (int b = 0; b < REF_NB; b++) {
		double c = REF_CENTRES[b], lo = c * std::pow(2.0, -1.0 / 6), hi = c * std::pow(2.0, 1.0 / 6), s = 0;
		for (size_t k = 0; k < X.size(); k++) { double f = k * fs / N; if (f >= lo && f < hi) s += std::norm(X[k]); }
		out.push_back(10 * std::log10(2 * s / ((double)N * (double)N) + 1e-14));
	}
	return out;
}
static double rmsDb(const Sig& x, size_t a = 0, size_t b = 0) {
	if (b == 0 || b > x.size()) b = x.size();
	double s = 0; for (size_t i = a; i < b; i++) s += x[i] * x[i];
	return 10 * std::log10(s / std::max<size_t>(1, b - a) + 1e-14);
}
static double peakAbs(const Sig& x, size_t a = 0, size_t b = 0) {
	if (b == 0 || b > x.size()) b = x.size();
	double m = 0; for (size_t i = a; i < b; i++) m = std::max(m, std::fabs(x[i])); return m;
}
static bool finite(const Sig& x) { for (double v : x) if (!std::isfinite(v)) return false; return true; }
static void writeWav(const std::string& path, const Sig& x, double fs, double gain = 0.5) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	uint32_t n = (uint32_t)x.size(), bytes = n * 2, sr = (uint32_t)fs, br = sr * 2;
	uint16_t ch = 1, ba = 2, bits = 16, pcm = 1; uint32_t sz = 36 + bytes, fmtLen = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
	fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
	for (double v : x) { int16_t s = (int16_t)std::max(-32767.0, std::min(32767.0, v * gain * 32767.0)); fwrite(&s, 2, 1, f); }
	fclose(f);
}
static uint64_t U(uint64_t seed) { return seed; }

// ---------------------------------------------------------------- case renderers (mirror make_reference.py)
static const double S16 = 0.125;
static Sig hits(const char* pattern, int bars, double fs, double step = S16) {
	int L = (int)strlen(pattern);
	Sig t((size_t)(int)(L * bars * step * fs), 0.0);
	for (int b = 0; b < bars; b++)
		for (int i = 0; i < L; i++)
			if (pattern[i] != '.') t[(size_t)(int)((b * L + i) * step * fs)] = pattern[i] == 'X' ? 1.6 : 1.0;
	return t;
}
static Sig renderZap(const ZapParams& p, double fs, const Sig* trig = nullptr, const Sig* pitch = nullptr, uint64_t useed = 1, uint32_t nseed = 12345) {
	Sig tz = trig ? *trig : hits("x...", 4, fs, 0.0625);
	ZapVoice v; v.init(fs, useed, nseed);
	Sig o(tz.size());
	for (size_t n = 0; n < tz.size(); n++) o[n] = v.process(tz[n], pitch ? (*pitch)[n] : 0.0, p);
	return o;
}
static const char* PAT[6] = {"x.....x.x.......", "....x.......x...", "x.x.x.x.x.x.x.X.", "........x.......", ".......x..x.....", "...x.....x....x."};
static std::vector<Sig> renderBlech(int box, double fs, double age = 0.0, int bars = 2) {
	std::vector<Sig> tr; for (int v = 0; v < 6; v++) tr.push_back(hits(PAT[v], bars, fs));
	BlechEngine e; e.init(fs, 2, 999);
	BlechParams P; P.box = box; P.age = age;
	std::vector<Sig> out(6, Sig(tr[0].size()));
	double t[6], o[6];
	for (size_t n = 0; n < tr[0].size(); n++) {
		for (int v = 0; v < 6; v++) t[v] = tr[v][n];
		e.process(t, P, o);
		for (int v = 0; v < 6; v++) out[v][n] = o[v];
	}
	return out;
}
static const int STEPS[16] = {0, 12, 0, 7, 0, 12, 3, 10, 0, 12, 0, 7, 5, 12, 3, 15};
static const char* GATES = "X.xxx.xxX.xxxx.x";
static void line(double octv, int bars, double glen, double fs, Sig& p, Sig& g, Sig& a) {
	size_t n = (size_t)(int)(16 * bars * S16 * fs);
	p.assign(n, 0); g.assign(n, 0); a.assign(n, 0);
	for (int b = 0; b < bars; b++)
		for (int i = 0; i < 16; i++) {
			size_t i0 = (size_t)(int)((b * 16 + i) * S16 * fs), i1 = (size_t)(int)((b * 16 + i + 1) * S16 * fs);
			for (size_t k = i0; k < i1 && k < n; k++) p[k] = octv + STEPS[i] / 12.0;
			if (GATES[i] != '.') {
				size_t ge = i0 + (size_t)(int)(glen * S16 * fs);
				for (size_t k = i0; k < ge && k < n; k++) g[k] = 1;
				for (size_t k = i0; k < i1 && k < n; k++) a[k] = GATES[i] == 'X' ? 1.0 : 0.0;
			}
		}
}
//                                   w1 w2 det  pw  mix sub noi cut  res  env  key   fA    fD   fS   aA    aD   aS  glide drv age
static const double DYN_BASS[20] =  {0, 0, .10, .5, .5, .4, 0, .30, .25, .45, .3, .002, .18, .0, .002, .30, .6, 0, .3, 0};
static const double DYN_PLUCK[20] = {0, 1, 12., .3, .3, 0, 0, .35, .70, .60, .5, .002, .08, .0, .002, .12, .0, 0, .2, 0};
static const double DYN_LEAD[20] =  {1, 1, .15, .35, .5, 0, 0, .55, .20, .25, .5, .020, .40, .6, .010, .30, .9, .04, .3, 0};
static Sig renderDynamo(const double* P, double octv, double fs, int bars = 2) {
	Sig p, g, a; line(octv, bars, 0.5, fs, p, g, a);
	DynamoVoice v; v.init(fs, 1, 1);
	Sig o(p.size());
	for (size_t n = 0; n < p.size(); n++) o[n] = v.process(p[n], g[n], a[n], 0.0, P);
	return o;
}
static Sig renderOrch(int disc, double fs, double age = 0.0, double dur = 3.0, std::vector<int> notes = {-3, 0, 4, 9}, double len = 2.6) {
	size_t N = (size_t)(int)(dur * fs);
	Sig out(N, 0.0);
	OrchDisc D; D.init(fs);
	std::vector<OrchVoice> vs(notes.size());
	for (size_t i = 0; i < notes.size(); i++) { vs[i].init(fs, 777 + 31 * (uint32_t)i); vs[i].reset(disc, notes[i]); }
	OrchParams P; P.age = age;
	XS32 h; h.seed(5);
	double hiss = std::pow(10.0, (-66.0 + 20.0 * age) / 20.0);
	size_t gEnd = (size_t)(int)(len * fs);
	for (size_t n = 0; n < N; n++) {
		double a, b; D.advance(a, b);
		double gate = n < gEnd ? 1.0 : 0.0, s = 0;
		for (size_t i = 0; i < notes.size(); i++) s += vs[i].process(notes[i] / 12.0, gate, a, b, P);
		s += gate * hiss * h.next() * 1.73;
		out[n] = s;
	}
	return out;
}
// SPRECHER offline "say", as in sprecher_ref.say with gate_len None (hold off)
struct SayOpts { std::string engine = "formant"; double base = 110, rate = 1, size = 1, whisper = 0, age = 0, gap = 0.12; };
static Sig say(const std::string& text, int lang, const SayOpts& o, double fs) {
	auto words = SpText::get().parse(text, lang);
	std::vector<std::vector<SpFrame>> fr;
	std::vector<double> starts;
	double t = 0.05;
	for (auto& w : words) {
		fr.push_back(spWordTargets(w.ph, o.rate));
		double ln = fr.back().size() * SP_CTRL;
		starts.push_back(t);
		t += std::max(ln, ln) + o.gap;
	}
	size_t N = (size_t)(int)(t * fs) + (size_t)(int)(0.2 * fs);
	SpScheduler sch; sch.init(fs);
	SpFormant F; SpChip C;
	F.init(fs, 7, 4242); C.init(fs, 7, 4242);
	SpVoiceParams vp; vp.size = o.size; vp.whisper = o.whisper; vp.age = o.age;
	Sig out(N);
	size_t wi = 0;
	for (size_t n = 0; n < N; n++) {
		if (wi < fr.size() && n == (size_t)(int)(starts[wi] / SP_CTRL) * (size_t)sch.hop) { auto f = fr[wi]; sch.start(std::move(f)); wi++; }
		sch.tick(false, false);
		out[n] = o.engine == "chip" ? C.process(sch.row, o.base, vp) : F.process(sch.row, o.base, vp);
	}
	return out;
}
static Sig renderSung(double fs) {
	auto words = SpText::get().parse("music machine computer world", 0);
	size_t N = (size_t)(int)(2.8 * fs);
	SpScheduler sch; sch.init(fs);
	SpFormant F; F.init(fs, 7, 4242);
	SpVoiceParams vp; vp.age = 0.0;
	const int semis[4] = {0, 3, 7, 5};
	Sig out(N);
	for (size_t n = 0; n < N; n++) {
		bool gate = false;
		for (int i = 0; i < 4; i++) {
			double t0 = 0.1 + 0.6 * i;
			size_t a = (size_t)(int)(t0 / SP_CTRL) * sch.hop, b = (size_t)(int)((t0 + 0.4) / SP_CTRL) * sch.hop;
			if (n == a) { auto f = spWordTargets(words[i].ph, 1.0); sch.start(std::move(f)); }
			if (n >= a && n < b) gate = true;
		}
		double f0 = 110.0;
		for (int i = 0; i < 4; i++) if (n >= (size_t)(int)(0.6 * i * fs)) f0 = 110.0 * std::pow(2.0, semis[i] / 12.0);
		sch.tick(gate, true);
		out[n] = F.process(sch.row, f0, vp);
	}
	return out;
}
static Sig renderVok(int chord, double shape, double shift, double fs, double age = 0.0, Sig* modOut = nullptr) {
	SayOpts so; so.rate = 0.85;
	Sig mod = say("we are the music machine", 0, so, fs);
	if (modOut) *modOut = mod;
	VokoderEngine v; v.init(fs, 3, 77);
	VokCarrier c;
	VokParams P; P.shift = shift; P.age = age;
	Sig o(mod.size());
	for (size_t n = 0; n < mod.size(); n++) o[n] = v.process(mod[n], c.process(-1.0, shape, 0.3, chord, fs), false, P);
	return o;
}

static std::map<std::string, std::function<Sig(double)>> cases() {
	std::map<std::string, std::function<Sig(double)>> m;
	auto zp = [](double s, double e, double sw, double r, double d, double nz, double c, double dr) {
		ZapParams p; p.startHz = s; p.endHz = e; p.sweep = sw; p.res = r; p.decay = d; p.noise = nz; p.click = c; p.drive = dr; p.age = 0.0; return p; };
	m["zap_kick"] = [=](double fs) { return renderZap(zp(400, 50, .03, .80, .40, 0, 1, 0), fs); };
	m["zap_pew_snare"] = [=](double fs) { return renderZap(zp(6000, 180, .02, .60, .15, .5, 1, .2), fs); };
	m["zap_tom"] = [=](double fs) { return renderZap(zp(800, 200, .05, .95, .60, 0, 1, 0), fs); };
	m["zap_tick"] = [=](double fs) { return renderZap(zp(9000, 3000, .004, .30, .03, .2, 1, 0), fs); };
	m["zap_drone"] = [=](double fs) { return renderZap(zp(1000, 1000, .03, 1.0, 8.0, 0, 1, 0), fs); };
	for (int b = 0; b < 3; b++)
		m[fmt("blech_box%d", b + 1)] = [=](double fs) { auto o = renderBlech(b, fs); Sig s(o[0].size()); for (auto& v : o) for (size_t n = 0; n < s.size(); n++) s[n] += v[n]; return s; };
	const char* vn[6] = {"bass", "snare", "hat", "cymbal", "tom", "clave"};
	for (int v = 0; v < 6; v++) m[std::string("blech_box1_") + vn[v]] = [=](double fs) { return renderBlech(0, fs)[v]; };
	m["dynamo_bass"] = [](double fs) { return renderDynamo(DYN_BASS, -2.0, fs); };
	m["dynamo_pluck"] = [](double fs) { return renderDynamo(DYN_PLUCK, -2.0, fs); };
	m["dynamo_lead"] = [](double fs) { return renderDynamo(DYN_LEAD, 0.0, fs); };
	const char* dn[8] = {"choir", "strings", "flute", "organ", "cello", "horn", "vox", "reed"};
	for (int d = 0; d < 8; d++) m[std::string("orch_") + dn[d]] = [=](double fs) { return renderOrch(d, fs); };
	auto sp = [](const char* text, int lang, const char* eng, SayOpts o) { o.engine = eng; return [=](double fs) { return say(text, lang, o, fs); }; };
	SayOpts d0;
	m["spr_en_numbers_formant"] = sp("1 2 3 4 5 6 7 8", 0, "formant", d0);
	m["spr_de_numbers_formant"] = sp("1 2 3 4 5 6 7 8", 1, "formant", d0);
	m["spr_en_numbers_chip"] = sp("1 2 3 4 5 6 7 8", 0, "chip", d0);
	m["spr_de_words_chip"] = sp("wir sind die roboter musik maschine", 1, "chip", d0);
	m["spr_en_words_formant"] = sp("we are the music machine computer world", 0, "formant", d0);
	SayOpts sm = d0; sm.size = 0.75; sm.base = 220; m["spr_small"] = sp("hello computer", 0, "formant", sm);
	SayOpts gi = d0; gi.size = 1.35; gi.base = 65; m["spr_giant"] = sp("hello computer", 0, "formant", gi);
	SayOpts wh = d0; wh.whisper = 1.0; m["spr_whisper"] = sp("1 2 3 4", 0, "formant", wh);
	m["spr_ja"] = sp("1 2 3 4 5 6 7 8", 5, "formant", d0);
	m["spr_fr_es_it_ru"] = [](double fs) { Sig s; for (int l : {2, 3, 4, 6}) { Sig x = say("1 2 3 4", l, SayOpts(), fs); s.insert(s.end(), x.begin(), x.end()); } return s; };
	m["spr_sung"] = [](double fs) { return renderSung(fs); };
	m["vok_saw_major"] = [](double fs) { return renderVok(3, 0.0, 0.0, fs); };
	m["vok_pulse_single"] = [](double fs) { return renderVok(0, 1.0, 0.0, fs); };
	m["vok_shift_up"] = [](double fs) { return renderVok(3, 0.0, 2.0, fs); };
	m["vok_shift_down"] = [](double fs) { return renderVok(3, 0.0, -2.0, fs); };
	return m;
}

static double centroid(const Sig& x, double fs) {
	size_t M = 1; while (M < x.size()) M <<= 1;
	std::vector<std::complex<double>> a(M); for (size_t i = 0; i < x.size(); i++) a[i] = x[i];
	fft(a);
	double s = 0, w = 0; for (size_t k = 1; k <= M / 2; k++) { double m = std::abs(a[k]); s += m * k * fs / M; w += m; }
	return s / w;
}
static double pitchOf(const Sig& x, size_t a, size_t b, double fs, double lo, double hi) {
	size_t n = 1; while (n * 2 <= b - a) n <<= 1;
	std::vector<std::complex<double>> s(n);
	for (size_t i = 0; i < n; i++) s[i] = x[a + i] * (0.5 - 0.5 * std::cos(2 * PI * i / n));
	fft(s);
	size_t best = 1; double bm = 0;
	for (size_t k = 1; k < n / 2 - 1; k++) { double f = k * fs / n; if (f < lo || f > hi) continue; double m = std::abs(s[k]); if (m > bm) { bm = m; best = k; } }
	double A = std::log(std::abs(s[best - 1]) + 1e-30), B = std::log(std::abs(s[best]) + 1e-30), C = std::log(std::abs(s[best + 1]) + 1e-30);
	double d = 0.5 * (A - C) / (A - 2 * B + C);
	return (best + d) * fs / n;
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	const double FS = 48000;
	auto C = cases();

	// ---- 1. agreement with the reference
	printf("1. third-octave bands against the Python reference (48 kHz)\n");
	const std::vector<std::string> strict = {"zap_kick", "zap_tom", "zap_drone", "blech_box1_bass", "blech_box1_tom", "blech_box1_clave", "orch_organ"};
	for (int i = 0; i < REF_NCASES; i++) {
		const RefCase& R = REF_CASES[i];
		auto it = C.find(R.name);
		if (it == C.end()) { check(false, R.name, "no renderer"); continue; }
		Sig x = it->second(FS);
		writeWav(out + "/" + R.name + ".wav", x, FS);
		auto b = bands(x, FS);
		bool st = std::find(strict.begin(), strict.end(), R.name) != strict.end();
		double tol = st ? 0.5 : 1.5, worst = 0; int wb_ = -1;
		for (int k = 0; k < REF_NB; k++) {
			if (R.bands[k] < -60) continue;
			double d = std::fabs(b[k] - R.bands[k]);
			if (d > worst) { worst = d; wb_ = k; }
		}
		check(worst <= tol && finite(x) && std::fabs(x.size() / FS - R.seconds) < 0.01, std::string("1 ") + R.name,
			fmt("worst %.2f dB at %.0f Hz (tol %.1f), rms %.2f vs %.2f dB", worst, wb_ >= 0 ? REF_CENTRES[wb_] : 0.0, tol, rmsDb(x), R.rmsDb));
	}

	// ---- 2. ZAP
	printf("2. ZAP\n");
	for (double f : {50.0, 1000.0}) {
		ZapParams p; p.startHz = p.endHz = f; p.res = 1.0; p.decay = 8.0; p.age = 0.0;
		Sig t((size_t)(FS * 2), 0.0); t[0] = 1;
		Sig o = renderZap(p, FS, &t);
		double pk = peakAbs(o, o.size() - 4800);
		check(std::fabs(pk - 0.82) <= 0.03, fmt("2 self-oscillation level at %.0f Hz", f), fmt("%.3f units", pk));
	}
	{
		ZapParams p; p.startHz = p.endHz = 100; p.res = 0.7; p.decay = 8.0; p.age = 0.0;
		Sig t((size_t)(FS * 4), 0.0); t[0] = 1;
		Sig o = renderZap(p, FS, &t);
		// ring time: first time the 10 ms peak falls under -40 dB of the first peak
		double p0 = peakAbs(o, 0, 480); size_t k = 0;
		for (size_t s = 0; s + 480 < o.size(); s += 480) if (peakAbs(o, s, s + 480) < p0 * 0.01) { k = s; break; }
		// expected from the damping: exp(-pi f k t) = 0.01
		double kk = zapK(0.7), tExp = std::log(100.0) / (PI * 100 * kk);
		const double tRef = 1.84;   // the Python reference, measured the same way
		check(k > 0 && std::fabs(k / FS - tRef) / tRef < 0.05, "2 ring time at RESONANCE 0.7, 100 Hz", fmt("%.3f s; Python %.2f s; linear damping alone %.2f s", k / FS, tRef, tExp));
	}

	// ---- 3. BLECH
	printf("3. BLECH\n");
	{
		const char* vn[6] = {"BASS", "SNARE", "HAT", "CYMBAL", "TOM", "CLAVE"};
		for (int v = 0; v < 6; v++) {
			BlechEngine e; e.init(FS, 2, 999); BlechParams P; P.age = 0.0;
			size_t N = (size_t)(FS * 3); Sig o(N);
			double t[6] = {0, 0, 0, 0, 0, 0}, oo[6];
			for (size_t n = 0; n < N; n++) { t[v] = n == 0 ? 1.0 : 0.0; e.process(t, P, oo); o[n] = oo[v]; }
			const double* p = BLECH_BOX[0][v];
			const double REFM[6] = {0.35, 0.14, 0.05, 0.71, 0.18, 0.04};   // the Python reference, measured the same way
			double T60 = REFM[v];
			// envelope: 5 ms peaks; find where it falls 60 dB under its maximum
			double pk = 0; size_t pkAt = 0;
			std::vector<double> env;
			for (size_t s = 0; s + 240 < N; s += 240) { env.push_back(peakAbs(o, s, s + 240)); if (env.back() > pk) { pk = env.back(); pkAt = env.size() - 1; } }
			// measure the slope between -10 and -40 dB and extrapolate to 60
			double t10 = -1, t40 = -1;
			for (size_t i = pkAt; i < env.size(); i++) {
				if (t10 < 0 && env[i] < pk * std::pow(10, -10 / 20.0)) t10 = i * 0.005;
				if (t40 < 0 && env[i] < pk * std::pow(10, -40 / 20.0)) { t40 = i * 0.005; break; }
			}
			double meas = (t40 - t10) * 2.0;
			bool ok = t10 > 0 && t40 > 0 && std::fabs(meas - T60) <= 0.10 * T60 + 0.0101;   // 10 % plus one 5 ms window
			check(ok, fmt("3 decay of %s at DECAY 1x", vn[v]), fmt("%.3f s (reference %.3f s; table T60 %.3f s)", meas, T60, p[0] > 0 ? p[1] : p[7]));
		}
		// isolation: a hit on SNARE must not reach BASS's output
		BlechEngine e; e.init(FS, 2, 999); BlechParams P; P.age = 0.0;
		double t[6] = {0}, oo[6], mx[6] = {0};
		for (int n = 0; n < 24000; n++) { t[1] = n == 0; e.process(t, P, oo); for (int v = 0; v < 6; v++) mx[v] = std::max(mx[v], std::fabs(oo[v])); }
		check(mx[0] < mx[1] * 0.01, "3 a SNARE hit leaves BASS silent at AGE 0", fmt("BASS peak %.2e, SNARE peak %.2f", mx[0], mx[1]));
	}

	// ---- 4. DYNAMO
	printf("4. DYNAMO\n");
	{
		double P[20]; std::copy(DYN_BASS, DYN_BASS + 20, P);
		P[D_MIX] = 0; P[D_SUB] = 0; P[D_CUT] = 1.0; P[D_ENV] = 0; P[D_KEY] = 0; P[D_RES] = 0; P[D_AS] = 1.0; P[D_DRIVE] = 0; P[D_AGE] = 0;
		for (double oct : {-2.0, 0.0, 2.0}) {
			DynamoVoice v; v.init(FS, 1, 1);
			size_t N = (size_t)FS; Sig o(N);
			for (size_t n = 0; n < N; n++) o[n] = v.process(oct, 1.0, 0, 0, P);
			double f = C4 * std::pow(2.0, oct);
			double m = pitchOf(o, N / 4, N, FS, f * 0.9, f * 1.1);
			double c = 1200 * std::log2(m / f);
			check(std::fabs(c) < 2.0, fmt("4 tuning at %.0f Hz", f), fmt("%+.2f cents", c));
		}
		// release: gate falls; 2 s later under -90 dB (A DECAY 0.4 s)
		double Q[20]; std::copy(DYN_BASS, DYN_BASS + 20, Q); Q[D_AD] = 0.4; Q[D_AGE] = 0.3;
		DynamoVoice v; v.init(FS, 1, 1);
		size_t N = (size_t)(FS * 3); Sig o(N);
		for (size_t n = 0; n < N; n++) o[n] = v.process(-2.0, n < FS * 0.5 ? 1.0 : 0.0, 0, 0, Q);
		double lvl = rmsDb(o, (size_t)(FS * 2.5), N);
		check(lvl < -90, "4 two seconds after the gate falls, below -90 dB", fmt("%.1f dB", lvl));
		// aliasing: saw at C6, filter open
		std::copy(DYN_BASS, DYN_BASS + 20, P);
		P[D_MIX] = 0; P[D_SUB] = 0; P[D_CUT] = 1.0; P[D_ENV] = 0; P[D_KEY] = 0; P[D_RES] = 0; P[D_AS] = 1.0; P[D_DRIVE] = 0; P[D_AGE] = 0;
		DynamoVoice w; w.init(FS, 1, 1);
		N = 65536 * 2; Sig s(N);
		for (size_t n = 0; n < N; n++) s[n] = w.process(2.0, 1.0, 0, 0, P);
		size_t M = 65536; std::vector<std::complex<double>> a(M);
		for (size_t i = 0; i < M; i++) a[i] = s[N - M + i] * (0.5 - 0.5 * std::cos(2 * PI * i / M));
		fft(a);
		double f1 = C4 * 4, fund = 0, worst = 0;
		for (size_t k = 1; k < M / 2; k++) {
			double f = k * FS / M, mag = std::abs(a[k]);
			double h = f / f1, dist = std::fabs(h - std::round(h)) * f1;
			if (std::fabs(f - f1) < 20) fund = std::max(fund, mag);
			else if (dist > 40 && f < 20000) worst = std::max(worst, mag);
		}
		double rel = 20 * std::log10(worst / fund);
		check(rel < -60, "4 alias components for a saw at C6, filter open", fmt("worst %.1f dB re fundamental", rel));
	}

	// ---- 5. ORCHESTER
	printf("5. ORCHESTER\n");
	{
		// two voices started at different times share the wobble: compare their pitch modulation
		size_t N = (size_t)(FS * 8);
		OrchDisc D; D.init(FS);
		OrchVoice a, b; a.init(FS, 1); b.init(FS, 2); a.reset(3, 0); b.reset(3, 12);
		OrchParams P; P.age = 0.0; P.wobble = 1.0;
		Sig xa(N), xb(N);
		size_t startB = (size_t)(FS * 0.77);
		for (size_t n = 0; n < N; n++) {
			double t0, t1; D.advance(t0, t1);
			xa[n] = a.process(0.0, 1.0, t0, t1, P);
			if (n == startB) b.reset(3, 12);
			xb[n] = n >= startB ? b.process(1.0, 1.0, t0, t1, P) : 0.0;
		}
		// zero-crossing period tracks over 1.8 s from 2 s on
		auto track = [&](const Sig& x, double f) {
			std::vector<double> pts; double last = -1;
			Sig y(x.size()); double lp = 0, c = 1 - std::exp(-2 * PI * f * 1.5 / FS);
			for (size_t n = 0; n < x.size(); n++) { lp += c * (x[n] - lp); y[n] = lp; }
			for (size_t n = (size_t)(FS * 2); n < (size_t)(FS * 7.4); n++)
				if (y[n - 1] < 0 && y[n] >= 0) { double z = n - 1 + y[n - 1] / (y[n - 1] - y[n]); if (last > 0) pts.push_back(FS / (z - last)); last = z; }
			return pts;
		};
		auto pa = track(xa, C4), pb = track(xb, C4 * 2);
		// average over one revolution phase bins
		double cs = 0, sn = 0, cs2 = 0, sn2 = 0;
		double tA = 2.0;
		for (size_t i = 0; i < pa.size(); i++) { tA += 1.0 / pa[i]; double th = 2 * PI * tA / ORCH_REV; cs += pa[i] * std::cos(th); sn += pa[i] * std::sin(th); }
		double tB = 2.0;
		for (size_t i = 0; i < pb.size(); i++) { tB += 1.0 / pb[i]; double th = 2 * PI * tB / ORCH_REV; cs2 += pb[i] * std::cos(th); sn2 += pb[i] * std::sin(th); }
		double d = std::fabs(std::remainder(std::atan2(sn, cs) - std::atan2(sn2, cs2), 2 * PI)) * 180 / PI;
		check(d < 5.0, "5 two keys started apart share the disc wobble phase", fmt("phase difference %.2f degrees", d));
		double lo = 1e9, hi = -1e9;
		for (int k = 0; k < 8; k++) { double r = rmsDb(renderOrch(k, FS), 0, (size_t)(FS * 2.5)); lo = std::min(lo, r); hi = std::max(hi, r); }
		check(hi - lo <= 3.5, "5 all eight discs within 3.5 dB for the reference chord", fmt("spread %.2f dB", hi - lo));
	}

	// ---- 6. VOKODER
	printf("6. VOKODER\n");
	{
		VokoderEngine v; v.init(FS, 3, 77); VokParams P; P.age = 0.3; VokCarrier c;
		double mx = 0;
		for (int n = 0; n < 48000; n++) mx = std::max(mx, std::fabs(v.process(0.0, c.process(-1, 0, .3, 3, FS), false, P)));
		check(mx == 0.0, "6 silence in gives silence out at AGE 0.3", fmt("peak %.3g", mx));
		XS32 r; r.seed(9);
		VokoderEngine u; u.init(FS, 3, 77); double vs = 0;
		for (int n = 0; n < 48000; n++) { u.process(0.3 * r.next() * 1.7, 0, false, P); if (n > 24000) vs += u.voiced; }
		check(vs / 24000 < 0.1, "6 white noise is classed unvoiced", fmt("voiced %.2f of the time", vs / 24000));
		VokoderEngine w; w.init(FS, 3, 77); double ph = 0, lp = 0; vs = 0;
		for (int n = 0; n < 48000; n++) {
			ph += 150.0 / FS; if (ph >= 1) ph -= 1;
			lp = 0.1 * (2 * ph - 1) + 0.9 * lp;
			w.process(lp, 0, false, P); if (n > 24000) vs += w.voiced;
		}
		check(vs / 24000 > 0.9, "6 a low-passed 150 Hz saw is classed voiced", fmt("voiced %.2f of the time", vs / 24000));
		// a held vowel onto a noise carrier: the output spectrum is the envelope, so its centroid shows the shift
		auto heldVowel = [&](double shift) {
			SpScheduler s; s.init(FS); s.start(spWordTargets(SpText::get().phonemes("'AA"), 1.0));
			SpFormant F; F.init(FS, 7, 4242); SpVoiceParams sv; sv.age = 0.0;
			VokoderEngine v; v.init(FS, 3, 77); VokParams P; P.shift = shift; P.sib = 0; P.age = 0.0;
			XS32 r; r.seed(11);
			size_t N = (size_t)(FS * 1.6), d = (size_t)(FS * 0.05); Sig o;
			for (size_t n = 0; n < N; n++) {
				bool g = n >= d && n < d + (size_t)(FS * 1.5);
				if (n < d) { s.tick(false, true); continue; }
				s.tick(g, true);
				double y = v.process(F.process(s.row, 110, sv), r.next() * 1.7, false, P);
				if (n > FS * 0.3 && n < FS * 1.4) o.push_back(y);
			}
			return centroid(o, FS);
		};
		double c0 = heldVowel(0), c2 = heldVowel(2), cm = heldVowel(-2);
		check(c2 / c0 > 1.3 && c2 / c0 < 1.8 && cm < c0, "6 FORMANT +2 raises the envelope centroid 30 to 80 %", fmt("%.0f Hz to %.0f Hz (x%.2f); -2 gives %.0f Hz", c0, c2, c2 / c0, cm));
	}

	// ---- 7. SPRECHER
	printf("7. SPRECHER\n");
	{
		int bad = 0, total = 0; std::string firstBad;
		for (auto& pc : PARSE_CASES) {
			auto w = SpText::get().parse(pc.text, pc.lang);
			std::string got;
			for (size_t i = 0; i < w.size(); i++) { if (i) got += " | "; got += SpText::phString(w[i].ph); }
			total++;
			if (got != pc.expect) { bad++; if (firstBad.empty()) firstBad = std::string(pc.text) + " -> " + got + " (want " + pc.expect + ")"; }
		}
		check(bad == 0, "7 parse matches the Python for all test texts", bad ? firstBad : fmt("%d texts", total));
		int nWords = 0, badW = 0; std::string worst;
		SpVoiceParams vp; vp.age = 0.3;
		for (int l = 0; l < 7; l++)
			for (int i = 0; i < SP_DICT_N[l]; i++) {
				auto ph = SpText::get().phonemes(SP_DICT[l][i].ph);
				auto fr = spWordTargets(ph, 1.0);
				SpFormant F; F.init(FS, 7, 4242);
				Sig o(fr.size() * 240 + 2400);
				SpScheduler s; s.init(FS); s.start(std::move(fr));
				for (size_t n = 0; n < o.size(); n++) { s.tick(false, false); o[n] = F.process(s.row, 110, vp); }
				double r = rmsDb(o); nWords++;
				if (!finite(o) || r < -30 || r > -3) { badW++; if (worst.empty()) worst = fmt("%s/%s %.1f dB", SP_LANGS[l], SP_DICT[l][i].word, r); }
			}
		check(badW == 0, "7 every dictionary word renders, RMS -30 to -3 dB", badW ? worst : fmt("%d words", nWords));
		// HOLD: a 1 s gate on "music"
		auto w = SpText::get().parse("music", 0);
		SpScheduler s; s.init(FS); s.start(spWordTargets(w[0].ph, 1.0));
		SpFormant F; F.init(FS, 7, 4242);
		size_t N = (size_t)(FS * 2), lastLoud = 0; Sig o(N);
		for (size_t n = 0; n < N; n++) { s.tick(n < FS, true); o[n] = F.process(s.row, 110, vp); if (s.busy()) lastLoud = n; }
		check(lastLoud / FS >= 0.9, "7 HOLD: a 1 s gate on \"music\" lasts at least 0.9 s", fmt("%.2f s", lastLoud / FS));
	}

	// ---- 8. TAKT
	printf("8. TAKT\n");
	{
		int bad = 0;
		for (int note = 0; note < 12; note++)
			for (int s = -24; s <= 36; s++) {
				int q = taktQuantise(s, (uint16_t)(1 << note));
				int d = ((q - note) % 12 + 12) % 12;
				// nearest note: distance <= 6, and ties go down
				int dist = std::abs(q - s);
				if (d != 0 || dist > 6 || (dist == 6 && q > s)) bad++;
			}
		check(bad == 0, "8 quantiser: all 12 single-note masks, ties go down", fmt("%d errors", bad));
		auto run = [](TaktCore& T, TaktSettings& S, int clocks, std::function<void(int, TaktRowOut*)> each, double period = 0.125) {
			TaktRowOut o[2]; bool end;
			const double dt = 1.0 / 48000;
			int per = (int)(period * 48000);
			for (int c = 0; c < clocks; c++)
				for (int i = 0; i < per; i++) {
					T.process(dt, i == 0, false, true, S, o, end);
					each(c * per + i, o);
				}
		};
		{
			TaktCore T; TaktSettings S;
			std::vector<float> pitches; std::vector<int> gates;
			run(T, S, 16, [&](int n, TaktRowOut* o) { if (n % 6000 == 10) { pitches.push_back(o[0].voct); gates.push_back(o[0].gate); } });
			const int semi[16] = {0, 12, 0, 7, 0, 12, 3, 10, 0, 12, 0, 7, 5, 12, 3, 15};
			const char* modes = "PRPPPRPPPRPPPPRP";
			int e = 0; float held = 0;
			for (int i = 0; i < 16; i++) {
				if (modes[i] == 'P') held = semi[i] / 12.f;
				if (std::fabs(pitches[i] - held) > 1e-4 || gates[i] != (modes[i] == 'P')) e++;
			}
			check(e == 0, "8 default pattern: pitches, rests hold pitch, gates", fmt("%d errors", e));
		}
		{
			TaktCore T; TaktSettings S; S.gate = 0.5;
			int high = 0; run(T, S, 4, [&](int n, TaktRowOut* o) { if (n < 6000) high += o[0].gate; });
			check(std::abs(high - 3000) <= 2, "8 GATE 50 % gives a half-step gate", fmt("%d samples of 3000", high));
		}
		{
			TaktCore T; TaktSettings S; S.gate = 0.3;
			T.pat[0].mode[0][1] = T_TIE; T.pat[0].mode[0][2] = T_TIE;
			int low = 0; run(T, S, 3, [&](int n, TaktRowOut* o) { if (n > 50 && n < 18000 - 50) low += !o[0].gate; });
			check(low == 0, "8 TIE keeps the gate high across steps", fmt("%d low samples", low));
		}
		{
			TaktCore T; TaktSettings S; S.gate = 1.0;
			for (int i = 0; i < 16; i++) T.pat[0].mode[0][i] = T_PLAY;
			int lows = 0; bool prev = false; int falls = 0;
			run(T, S, 3, [&](int n, TaktRowOut* o) { if (n > 0 && n < 6000 * 3 - 1) { if (prev && !o[0].gate) falls++; if (!o[0].gate) lows++; } prev = o[0].gate; });
			check(falls == 2 && lows >= 90 && lows <= 100, "8 GATE 100 % re-triggers with a 1 ms gap", fmt("%d drops, %d low samples", falls, lows));
		}
		{
			TaktCore T; TaktSettings S; T.pat[0].last[0] = 5; T.pat[0].last[1] = 3;
			for (int i = 0; i < 16; i++) { T.pat[0].mode[0][i] = T_PLAY; T.pat[0].slider[0][i] = i / 24.f; T.pat[0].slider[1][i] = i / 24.f; }
			std::vector<int> a, b;
			run(T, S, 12, [&](int n, TaktRowOut* o) { if (n % 6000 == 10) { a.push_back((int)std::lround(o[0].voct * 12)); b.push_back((int)std::lround(o[1].voct * 12)); } });
			int e = 0; for (int i = 0; i < 12; i++) { if (a[i] != i % 5) e++; if (b[i] != i % 3) e++; }
			check(e == 0, "8 LAST: row A of 5 against row B of 3", fmt("%d errors", e));
		}
		{
			TaktCore T; TaktSettings S; S.series = true;
			for (int r = 0; r < 2; r++) for (int i = 0; i < 16; i++) { T.pat[0].mode[r][i] = T_PLAY; T.pat[0].slider[r][i] = (r * 16 + i) / 48.f; }
			T.pat[0].last[1] = 4; S.range = 4;
			std::vector<int> a, b;
			run(T, S, 40, [&](int n, TaktRowOut* o) { if (n % 6000 == 10) { a.push_back((int)std::lround(o[0].voct * 12)); b.push_back((int)std::lround(o[1].voct * 12)); } });
			int e = 0; for (int i = 0; i < 40; i++) { if (a[i] != i % 20 || b[i] != a[i]) e++; }
			check(e == 0, "8 SERIES chains A then B (B ending at LAST B)", fmt("%d errors", e));
		}
		{
			TaktCore T; TaktSettings S; S.atBar = true;
			for (int i = 0; i < 16; i++) { T.pat[0].mode[0][i] = T_PLAY; T.pat[0].slider[0][i] = 0; }
			std::vector<int> a;
			run(T, S, 20, [&](int n, TaktRowOut* o) {
				if (n == 6000 * 3 + 100) S.keyTranspose = 5;
				if (n % 6000 == 10) a.push_back((int)std::lround(o[0].voct * 12));
			});
			int e = 0; for (int i = 0; i < 20; i++) if (a[i] != (i < 16 ? 0 : 5)) e++;
			TaktCore T2; TaktSettings S2;
			for (int i = 0; i < 16; i++) { T2.pat[0].mode[0][i] = T_PLAY; T2.pat[0].slider[0][i] = 0; }
			std::vector<int> b;
			run(T2, S2, 6, [&](int n, TaktRowOut* o) { if (n == 6000 * 3 + 100) S2.keyTranspose = 5; if (n % 6000 == 10) b.push_back((int)std::lround(o[0].voct * 12)); });
			for (int i = 0; i < 6; i++) if (b[i] != (i < 4 ? 0 : 5)) e++;
			check(e == 0, "8 transposition waits for step 1 at BAR, acts at once at NOW", fmt("%d errors", e));
		}
		{
			TaktCore T; TaktSettings S;
			T.pat[1].clear(); for (int i = 0; i < 16; i++) T.pat[1].slider[0][i] = 7 / 24.f;
			for (int i = 0; i < 16; i++) { T.pat[0].mode[0][i] = T_PLAY; T.pat[0].slider[0][i] = 0; }
			std::vector<int> a; int switches = 0;
			run(T, S, 20, [&](int n, TaktRowOut* o) { if (n == 6000 * 5) T.requested = 1; if (T.switched) switches++; if (n % 6000 == 10) a.push_back((int)std::lround(o[0].voct * 12)); });
			int e = 0; for (int i = 0; i < 20; i++) if (a[i] != (i < 16 ? 0 : 7)) e++;
			check(e == 0 && switches == 1, "8 a pattern change waits for step 1", fmt("%d errors, %d switches", e, switches));
		}
	}

	// ---- 9. other sample rates and extremes
	printf("9. sample rates and extremes\n");
	for (double fs : {44100.0, 96000.0}) {
		int bad = 0; std::string worstName; double worstAll = 0;
		for (int i = 0; i < REF_NCASES; i++) {
			const RefCase& R = REF_CASES[i];
			if (std::string(R.name) == "zap_drone") continue;    // checked by pitch and level below
			Sig x = C[R.name](fs);
			auto b = bands(x, fs);
			double worst = 0; int wk = 0;
			for (int k = 0; k < REF_NB; k++) {
				// Quiet bands are mostly leakage and luck, and above 10 kHz the 48 kHz reference's filters are
				// squeezed by the approach to Nyquist, so those are not compared at other rates.
				double top = -1e9; for (int q = 0; q < REF_NB; q++) top = std::max(top, R.bands[q]);
				if (R.bands[k] < std::max(-50.0, top - 40.0) || REF_CENTRES[k] > 10000) continue;   // and bands 40 dB under the loudest
				if ((std::string(R.name).find("chip") != std::string::npos) && REF_CENTRES[k] > 8000) continue;
				double d = std::fabs(b[k] - R.bands[k]);
				if (d > worst) { worst = d; wk = k; }
			}
			// noise-driven sounds get a different random sequence at each rate: allow 4 dB there
			std::string nm = R.name;
			bool noisy = nm.find("snare") != std::string::npos || nm.find("hat") != std::string::npos || nm.find("cymbal") != std::string::npos ||
				nm.find("whisper") != std::string::npos || nm.find("tick") != std::string::npos;
			if (worst > (noisy ? 4.0 : 2.5) || !finite(x)) { bad++; printf("      %-26s %.2f dB at %.0f Hz (%.1f vs %.1f)\n", R.name, worst, REF_CENTRES[wk], b[wk], R.bands[wk]); }
			if (worst > worstAll) { worstAll = worst; worstName = R.name; }
		}
		check(bad == 0, fmt("9 all cases at %.1f kHz (2.5 dB; noise-driven 4 dB)", fs / 1000), fmt("%d over; worst %.2f dB (%s)", bad, worstAll, worstName.c_str()));
	}
	for (double fs : {44100.0, 96000.0}) {
		Sig a = C["zap_drone"](48000.0), b = C["zap_drone"](fs);
		double fa = pitchOf(a, a.size() / 2, a.size(), 48000.0, 900, 1100), fb = pitchOf(b, b.size() / 2, b.size(), fs, 900, 1100);
		double c = 1200 * std::log2(fb / fa), l = rmsDb(b) - rmsDb(a);
		check(std::fabs(c) < 5 && std::fabs(l) < 0.5, fmt("9 ZAP self-oscillation at %.1f kHz: same pitch and level", fs / 1000), fmt("%+.2f cents, %+.2f dB", c, l));
	}
	{
		bool ok = true;
		ZapParams zp; zp.startHz = 16000; zp.endHz = 20; zp.sweep = 1e-3; zp.res = 1; zp.decay = 8; zp.noise = 1; zp.click = 1; zp.drive = 1; zp.age = 1;
		Sig t(48000, 0); for (size_t i = 0; i < t.size(); i += 100) t[i] = 1.6;
		Sig pz(48000, 2.4);   // +12 V would be +12 octaves: the module clamps V/OCT to +-10 V
		ok &= finite(renderZap(zp, FS, &t, &pz));
		BlechEngine be; be.init(FS, 5, 5); BlechParams bp; bp.age = 1; bp.dyn = 1; bp.pedPitch = 1; bp.tone = 0;
		for (int v = 0; v < 6; v++) { bp.tune[v] = 1; bp.decay[v] = 4; }
		double tr[6], oo[6];
		for (int n = 0; n < 48000; n++) { for (int v = 0; v < 6; v++) tr[v] = n % 50 == 0 ? 1.8 : 0; be.process(tr, bp, oo); for (double q : oo) ok &= std::isfinite(q); }
		double P[20]; std::copy(DYN_LEAD, DYN_LEAD + 20, P); P[D_RES] = 1; P[D_CUT] = 1; P[D_ENV] = 1; P[D_DRIVE] = 1; P[D_AGE] = 1; P[D_NOISE] = 1; P[D_SUB] = 1;
		DynamoVoice dv; dv.init(FS, 3, 3);
		for (int n = 0; n < 48000; n++) ok &= std::isfinite(dv.process(n % 2000 < 1000 ? 10.0 : -10.0, n % 300 < 150, 1, 10.0, P));
		OrchDisc D; D.init(FS); OrchVoice ov; ov.init(FS, 3); ov.reset(7, 120);
		OrchParams op; op.wobble = 1; op.tone = 1; op.age = 1; op.attack = 0.001;
		for (int n = 0; n < 48000; n++) { double a, b; D.advance(a, b); ok &= std::isfinite(ov.process(10.0, 1, a, b, op)); }
		VokoderEngine ve; ve.init(FS, 3, 3); VokParams vp; vp.age = 1; vp.shift = 5; vp.sib = 1; vp.thresh = 0; vp.attack = 0.001; vp.release = 0.01;
		for (int i = 0; i < VK_NB; i++) vp.sliders[i] = 4;
		XS32 r; r.seed(3);
		for (int n = 0; n < 48000; n++) ok &= std::isfinite(ve.process(r.next() * 2.4 * 4, 2.4, false, vp));
		SpFormant F; F.init(FS, 1, 1); SpChip Ch; Ch.init(FS, 1, 1); SpVoiceParams sv; sv.size = 0.7; sv.whisper = 1; sv.age = 1;
		SpScheduler s; s.init(FS); s.start(spWordTargets(SpText::get().parse("[S T R IY T] computer", 0)[0].ph, 2.0));
		for (int n = 0; n < 48000; n++) { s.tick(true, true); ok &= std::isfinite(F.process(s.row, 440 * 1024.0, sv)) && std::isfinite(Ch.process(s.row, 440 * 1024.0, sv)); }
		sv.size = 1.4;
		for (int n = 0; n < 48000; n++) { s.tick(false, true); ok &= std::isfinite(F.process(s.row, 1.0, sv)) && std::isfinite(Ch.process(s.row, 1.0, sv)); }
		check(ok, "9 no NaN or infinity with controls at their extremes", ok ? "all finite" : "non-finite output");
	}

	// ---- 10. CPU
	printf("10. CPU (real-time factor at 48 kHz on this machine)\n");
	{
		auto time = [&](std::function<void(int)> f, int n) {
			auto t0 = std::chrono::steady_clock::now(); f(n);
			double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			return (n / FS) / s;
		};
		const int N = 48000 * 4;
		volatile double sink = 0;
		double zr = time([&](int n) { ZapVoice v[16]; for (int c = 0; c < 16; c++) v[c].init(FS, c, c + 1); ZapParams p;
			for (int i = 0; i < n; i++) for (int c = 0; c < 16; c++) sink = sink + v[c].process(i % 6000 == c ? 1.0 : 0.0, 0, p); }, N);
		double br = time([&](int n) { BlechEngine e; e.init(FS, 1, 1); BlechParams p; double t[6] = {0}, o[6];
			for (int i = 0; i < n; i++) { for (int v = 0; v < 6; v++) t[v] = i % 3000 == v; e.process(t, p, o); sink = sink + o[0]; } }, N);
		double dr = time([&](int n) { DynamoVoice v[16]; for (int c = 0; c < 16; c++) v[c].init(FS, c, c + 1);
			for (int i = 0; i < n; i++) for (int c = 0; c < 16; c++) sink = sink + v[c].process(-1 + c / 12.0, 1, 0, 0, DYN_BASS); }, N);
		double orr = time([&](int n) { OrchDisc D; D.init(FS); OrchVoice v[16]; for (int c = 0; c < 16; c++) { v[c].init(FS, c + 1); v[c].reset(1, c); } OrchParams p;
			for (int i = 0; i < n; i++) { double a, b; D.advance(a, b); for (int c = 0; c < 16; c++) sink = sink + v[c].process(c / 12.0, 1, a, b, p); } }, N);
		double vr = time([&](int n) { VokoderEngine v; v.init(FS, 1, 1); VokCarrier c; VokParams p; XS32 r; r.seed(1);
			for (int i = 0; i < n; i++) sink = sink + v.process(r.next() * 0.3, c.process(-1, 0, .3, 3, FS), false, p); }, N);
		double sr = time([&](int n) { SpFormant F; F.init(FS, 1, 1); SpVoiceParams p; SpScheduler s; s.init(FS);
			auto w = SpText::get().parse("computer world", 0);
			for (int i = 0; i < n; i++) { if (i % 48000 == 0) s.start(spWordTargets(w[0].ph, 1.0)); s.tick(false, false); sink = sink + F.process(s.row, 110, p); } }, N);
		double cr = time([&](int n) { SpChip F; F.init(FS, 1, 1); SpVoiceParams p; SpScheduler s; s.init(FS);
			auto w = SpText::get().parse("computer world", 0);
			for (int i = 0; i < n; i++) { if (i % 48000 == 0) s.start(spWordTargets(w[0].ph, 1.0)); s.tick(false, false); sink = sink + F.process(s.row, 110, p); } }, N);
		printf("      ZAP x16 %.0fx   BLECH %.0fx   DYNAMO x16 %.1fx   ORCHESTER x16 %.1fx   VOKODER %.0fx   SPRECHER formant %.0fx, chip %.0fx\n", zr, br, dr, orr, vr, sr, cr);
		check(br > 10 && vr > 10 && sr > 10 && cr > 10, "10 single-voice modules under 10 % of a core", fmt("worst %.0fx real time", std::min({br, vr, sr, cr})));
		check(zr > 3 && dr > 3 && orr > 3, "10 sixteen voices of ZAP, DYNAMO, ORCHESTER", fmt("ZAP %.1fx, DYNAMO %.1fx, ORCHESTER %.1fx", zr, dr, orr));
	}

	printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
