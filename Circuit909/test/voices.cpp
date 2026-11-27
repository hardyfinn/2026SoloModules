// Offline checks and demo WAVs for the 909 Circuits voices (no Rack needed).
//   make test   ->  test/out/*.wav and a printed report
#include "../src/Engines909.hpp"
#include "../src/Wav909.hpp"
#include <cstdio>
#include <vector>
#include <string>
using namespace c909;

static const float FS = 48000.f;
static int fails = 0;
static void check(bool ok, const std::string& what) { printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str()); if (!ok) fails++; }

static void wav(const std::string& path, const std::vector<float>& s) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return;
	uint32_t n = s.size(), bytes = n * 2;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(1); w32((uint32_t)FS); w32((uint32_t)FS * 2); w16(2); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (float v : s) { float c = std::max(-1.f, std::min(1.f, v / 10.f)); w16((uint16_t)(int16_t)(c * 32767)); }
	fclose(f);
}

template <class E> static int ki(const char* id) {
	for (int i = 0; i < E::spec().nKnobs; i++) if (std::string(E::spec().knobs[i].id) == id) return i;
	printf("no knob %s\n", id); exit(2);
}
template <class E> static std::vector<float> stockKnobs() {
	std::vector<float> k;
	for (int i = 0; i < E::spec().nKnobs; i++) k.push_back(E::spec().knobs[i].stock);
	return k;
}
template <class E>
static std::vector<float> render(const std::vector<float>& k, float acc, float sec, uint32_t serial = 909, float voct = 0.f, float fs = FS, bool second = false) {
	E e; e.setSampleRate(fs); e.age.setSerial(serial);
	std::vector<float> o;
	e.process(k.data(), voct);
	if (second) e.hit2(acc, k.data()); else e.hit(acc, k.data());
	for (int i = 0; i < sec * fs; i++) { e.process(k.data(), voct); o.push_back(e.out); }
	return o;
}
static float peak(const std::vector<float>& s) { float p = 0; for (float v : s) p = std::max(p, std::fabs(v)); return p; }
static float rms(const std::vector<float>& s, float t0, float t1, float fs = FS) {
	double a = 0; int n = 0;
	for (int i = (int)(t0 * fs); i < (int)(t1 * fs) && i < (int)s.size(); i++) { a += s[i] * s[i]; n++; }
	return n ? std::sqrt(a / n) : 0.f;
}
static float db(float a, float b) { return 20.f * std::log10(std::max(a, 1e-9f) / std::max(b, 1e-9f)); }
static float tail(const std::vector<float>& s, float fs = FS) { float p = peak(s); int l = 0; for (size_t i = 0; i < s.size(); i++) if (std::fabs(s[i]) > p * 0.01f) l = i; return l / fs; }
// mean loudness over 16 hits of one unit
template <class E>
static float loud(std::vector<float> k, float sec, float acc = 0.f, float fs = FS, float voct = 0.f, bool usePeak = false) {
	E e; e.setSampleRate(fs); e.age.setSerial(909);
	double a = 0;
	for (int h = 0; h < 16; h++) {
		e.hit(acc, k.data());
		double s2 = 0; float p = 0.f; int n = (int)(sec * fs);
		for (int i = 0; i < n; i++) { e.process(k.data(), voct); s2 += e.out * e.out; p = std::max(p, std::fabs(e.out)); }
		a += usePeak ? p : std::sqrt(s2 / n);
		for (int i = 0; i < (int)(0.3f * fs); i++) e.process(k.data(), voct);
	}
	return a / 16;
}
template <class E>
static float spreadDb(const char* id, int steps, float sec) {
	auto k = stockKnobs<E>(); int i = ki<E>(id);
	float lo = 1e9f, hi = 0.f;
	for (int j = 0; j <= steps; j++) { k[i] = j / (float)steps; float l = loud<E>(k, sec); lo = std::min(lo, l); hi = std::max(hi, l); }
	return db(hi, lo);
}
// pitch from upward zero crossings in a window
static float pitch(const std::vector<float>& s, float t0, float t1) {
	int a = (int)(t0 * FS), b = (int)(t1 * FS), first = -1, last = -1, n = 0;
	for (int i = a; i + 1 < b && i + 1 < (int)s.size(); i++) if (s[i] <= 0.f && s[i + 1] > 0.f) { if (first < 0) first = i; last = i; n++; }
	return n > 1 ? (n - 1) * FS / (last - first) : 0.f;
}

// frequency of the first whole cycle after t0 (upward zero crossings)
static float cycleHz(const std::vector<float>& s, float t0) {
	int a = -1;
	for (int i = (int)(t0 * FS); i + 1 < (int)s.size(); i++) if (s[i] <= 0.f && s[i + 1] > 0.f) { if (a < 0) a = i; else return FS / (i - a); }
	return 0.f;
}

template <class E>
static void basic(const char* name, float sec, float lo = 2.f, float hi = 8.f) {
	printf("%s\n", name);
	auto k = stockKnobs<E>();
	auto u = render<E>(k, 0.f, sec), a = render<E>(k, 1.f, sec);
	printf("  stock: peak %.2f V unaccented, %.2f V accented, 1%% tail %.3f s\n", peak(u), peak(a), tail(u));
	check(peak(u) > lo && peak(u) < hi, "unaccented peak in range");
	check(peak(a) > peak(u) * 1.2f && peak(a) < 11.f, "accent louder, inside rails");
	Rng r; r.seed(77);
	bool ok = true;
	for (int trial = 0; trial < 40; trial++) {
		std::vector<float> kk(k.size());
		for (size_t i = 0; i < kk.size(); i++) {
			const KnobDef& d = E::spec().knobs[i];
			float v = trial == 0 ? 0.f : trial == 1 ? 1.f : r.uni();
			kk[i] = d.kind == 1 ? std::round(v * 2.f) : d.kind == 2 ? std::round(v) : v;
		}
		auto s = render<E>(kk, trial % 2 ? 1.f : 0.f, 0.6f, trial, trial == 1 ? 2.f : trial == 2 ? -3.f : 0.f);
		for (float v : s) if (!std::isfinite(v) || std::fabs(v) > 12.f) ok = false;
	}
	check(ok, "40 random/extreme settings: finite, inside rails");
	int ai = ki<E>("AGE");
	auto k1 = k; k1[ai] = 1.f;
	auto s1 = render<E>(k1, 0.f, 0.5f, 1), s2 = render<E>(k1, 0.f, 0.5f, 2);
	bool differ = false; for (size_t i = 0; i < s1.size(); i++) if (std::fabs(s1[i] - s2[i]) > 1e-3f) differ = true;
	check(differ, "AGE 1: two units differ");
	auto z1 = render<E>(k, 0.f, 0.5f, 1), z2 = render<E>(k, 0.f, 0.5f, 2);
	bool same = true; for (size_t i = 0; i < z1.size(); i++) if (z1[i] != z2[i]) same = false;
	check(same, "AGE 0: every unit identical");
	float a48 = loud<E>(k, 0.2f, 0.f, 48000.f), a96 = loud<E>(k, 0.2f, 0.f, 96000.f), a44 = loud<E>(k, 0.2f, 0.f, 44100.f);
	printf("  loudness 44.1/96 vs 48 kHz: %+.2f / %+.2f dB\n", db(a44, a48), db(a96, a48));
	check(std::fabs(db(a96, a48)) < 1.f && std::fabs(db(a44, a48)) < 1.f, "same level at 44.1, 48 and 96 kHz");
	float ac = db(loud<E>(k, 0.2f, 1.f), loud<E>(k, 0.2f));
	printf("  accent gain %.1f dB\n", ac);
	check(ac > 3.5f && ac < 6.5f, "accent adds 3.5-6.5 dB");
	// NaN on V/OCT: recovers on the next hit
	{
		E e; e.setSampleRate(FS);
		for (int j = 0; j < 10; j++) e.process(k.data(), NAN);
		e.hit(0.f, k.data());
		float p = 0.f; for (int j = 0; j < 0.2f * FS; j++) { e.process(k.data(), 0.f); p = std::max(p, std::fabs(e.out)); }
		check(std::isfinite(p) && p > 1.f, "recovers after NaN on V/OCT");
	}
	// DRIVE leaves no DC
	{
		auto kd = k; kd[ki<E>("DRIVE")] = 1.f;
		auto s = render<E>(kd, 1.f, 1.5f);
		double m = 0; size_t n0 = (size_t)(1.f * FS);
		for (size_t i = n0; i < s.size(); i++) m += s[i];
		m /= s.size() - n0;
		check(std::fabs(m) < 0.02, "no DC after DRIVE");
	}
	// sleeps after the hit dies, wakes on the next
	{
		E e; e.setSampleRate(FS);
		e.hit(0.f, k.data());
		int n = 0; while (!e.sleeping() && n < 12 * FS) { e.process(k.data(), 0.f); n++; }
		printf("  asleep after %.2f s\n", n / FS);
		check(e.sleeping(), "silent voice sleeps");
		e.hit(0.f, k.data());
		check(!e.sleeping(), "hit wakes it");
	}
}

struct Hit { int step; int voice; bool acc; bool second; };

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	basic<KickEngine>("909 Kick", 1.5f);
	basic<SnareEngine>("909 Snare", 0.8f);
	basic<TomEngine>("909 Tom", 1.f);
	basic<RimEngine>("909 Rim", 0.2f);
	basic<ClapEngine>("909 Clap", 0.8f);
	basic<CymbalsEngine>("909 Cymbals", 1.f);

	printf("specific checks\n");
	{   // kick: lands on BASE, starts well above it
		auto s = render<KickEngine>(stockKnobs<KickEngine>(), 0.f, 1.f);
		float f0 = cycleHz(s, 0.f), f1 = pitch(s, 0.25f, 0.6f);
		printf("  kick: first cycle %.0f Hz, settled %.1f Hz\n", f0, f1);
		check(std::fabs(f1 / 52.f - 1.f) < 0.06f, "kick settles near 52 Hz");
		check(f0 > 2.f * f1, "kick sweep starts more than an octave up");
		auto k = stockKnobs<KickEngine>(); k[ki<KickEngine>("TUNE")] = 1.f;
		auto l = render<KickEngine>(k, 0.f, 1.f);
		float fl = cycleHz(l, 0.03f), fs0 = cycleHz(s, 0.03f);
		printf("  kick cycle after 30 ms: stock TUNE %.0f Hz, TUNE up %.0f Hz\n", fs0, fl);
		check(fl > fs0 * 1.2f, "TUNE up = longer, higher sweep");
		float lo = 1e9f, hi = 0.f;
		for (int st = -12; st <= 12; st += 3) { float p = loud<KickEngine>(stockKnobs<KickEngine>(), 0.3f, 0.f, FS, st / 12.f); lo = std::min(lo, p); hi = std::max(hi, p); }
		printf("  kick over two octaves of V/OCT: %.1f dB loudness spread\n", db(hi, lo));
		check(db(hi, lo) < 2.f, "kick level even over V/OCT");
	}
	{   // snare: noise near the shells at stock
		auto k = stockKnobs<SnareEngine>();
		auto h = k; h[ki<SnareEngine>("SNAPPY")] = 0.f;
		float heads = rms(render<SnareEngine>(h, 0.f, 0.08f), 0.f, 0.08f);
		float full = loud<SnareEngine>(k, 0.08f);
		float noise = std::sqrt(std::max(0.f, full * full - heads * heads));
		printf("  snare first 80 ms: shells %.2f V, noise %.2f V RMS (%.1f dB)\n", heads, noise, db(noise, heads));
		check(std::fabs(db(noise, heads)) < 5.f, "snare noise within 5 dB of the shells");
	}
	{   // toms: three sizes sit together, pitches in order
		auto k = stockKnobs<TomEngine>();
		float lo = 1e9f, hi = 0.f, fp[3];
		for (int r = 0; r < 3; r++) {
			k[ki<TomEngine>("RANGE")] = r; k[ki<TomEngine>("NOISE")] = 0.f;
			float p = loud<TomEngine>(k, 0.3f, 0.f, FS, 0.f, true); lo = std::min(lo, p); hi = std::max(hi, p);
			fp[r] = pitch(render<TomEngine>(k, 0.f, 0.6f), 0.2f, 0.5f);
		}
		printf("  toms settle %.0f / %.0f / %.0f Hz, %.1f dB peak spread\n", fp[0], fp[1], fp[2], db(hi, lo));
		check(fp[0] < fp[1] && fp[1] < fp[2], "LOW < MID < HIGH");
		check(db(hi, lo) < 3.f, "tom sizes within 3 dB");
	}
	{   // rim: STRIKE and TUNE hold the level
		float rs = spreadDb<RimEngine>("STRIKE", 10, 0.05f), rt = spreadDb<RimEngine>("TUNE", 10, 0.05f);
		printf("  rim level spread: STRIKE %.1f dB, TUNE %.1f dB\n", rs, rt);
		check(rs < 6.f && rt < 6.f, "rim STRIKE and TUNE under 6 dB");
	}
	{   // clap: BAND / RES hold the level
		float b = spreadDb<ClapEngine>("BAND", 6, 0.2f), r = spreadDb<ClapEngine>("RES", 6, 0.2f);
		printf("  clap level spread: BAND %.1f dB, RES %.1f dB\n", b, r);
		check(b < 5.f && r < 5.f, "clap BAND/RES under 5 dB");
	}
	{   // cymbals: closed shorter than open, closed chokes open, crash longest, BITS matter
		auto k = stockKnobs<CymbalsEngine>();
		auto ch = render<CymbalsEngine>(k, 0.f, 2.f), oh = render<CymbalsEngine>(k, 0.f, 2.f, 909, 0.f, FS, true);
		auto kc = k; kc[ki<CymbalsEngine>("TYPE")] = 1.f; kc[ki<CymbalsEngine>("DECAY")] = CymbalsEngine::stockFor(ki<CymbalsEngine>("DECAY"), kc.data());
		auto cr = render<CymbalsEngine>(kc, 0.f, 4.f);
		printf("  cymbals 1%% tails: closed %.3f s, open %.3f s, crash %.2f s\n", tail(ch), tail(oh), tail(cr));
		check(tail(ch) < 0.15f && tail(oh) > 0.25f && tail(cr) > 1.f, "closed < open < crash");
		CymbalsEngine e; e.setSampleRate(FS);
		e.hit2(0.f, k.data());
		for (int i = 0; i < 0.05f * FS; i++) e.process(k.data(), 0.f);
		e.hit(0.f, k.data());
		float late = 0.f;
		for (int i = 0; i < 0.4f * FS; i++) { e.process(k.data(), 0.f); if (i > 0.2f * FS) late = std::max(late, std::fabs(e.out)); }
		check(late < 0.05f, "closed hat chokes the open hat");
		auto k2 = k; k2[ki<CymbalsEngine>("BITS")] = 0.f;
		auto hi = render<CymbalsEngine>(k2, 0.f, 0.3f, 909, 0.f, FS, true);
		float d = 0.f; for (size_t i = 0; i < hi.size(); i++) d += std::fabs(hi[i] - oh[i]);
		printf("  BITS 2 vs 6: mean difference %.3f V\n", d / hi.size());
		check(d / hi.size() > 0.02f, "BITS changes the sound");
		std::vector<float> user(44100);
		for (size_t i = 0; i < user.size(); i++) user[i] = std::sin(2.f * PI * 440.f * i / 44100.f) * std::exp(-3.f * i / 44100.f);
		CymbalsEngine u; u.setSampleRate(FS); u.setSample(user, 44100.f);
		auto ku = k; ku[ki<CymbalsEngine>("CUT")] = 0.f;
		u.hit2(0.f, ku.data());
		std::vector<float> us; for (int i = 0; i < 0.3f * FS; i++) { u.process(ku.data(), 0.f); us.push_back(u.out); }
		float up = pitch(us, 0.02f, 0.2f);
		printf("  user WAV 440 Hz plays at %.0f Hz\n", up);
		check(std::fabs(up / 440.f - 1.f) < 0.03f, "user WAV plays at its own pitch at stock TUNE");
	}

	// --- regressions for the 2.1.0 DSP review ---
	printf("review checks\n");
	{   // crash at stock: ENV back to zero and asleep; tail fades smoothly at 6 bits
		auto k = stockKnobs<CymbalsEngine>(); k[ki<CymbalsEngine>("TYPE")] = 1.f;
		CymbalsEngine e; e.setSampleRate(FS);
		e.hit(0.f, k.data());
		std::vector<float> s;
		for (int i = 0; i < 5 * FS; i++) { e.process(k.data(), 0.f); s.push_back(e.out); }
		printf("  crash ENV after 5 s: %.4f V, asleep %d\n", e.env, (int)e.sleeping());
		check(e.env < 0.01f && e.sleeping(), "crash: ENV returns to zero and the module sleeps");
		float r0 = rms(s, 0.05f, 0.15f), r1 = rms(s, 1.4f, 1.5f), r2 = rms(s, 2.0f, 2.1f);
		printf("  crash at 6 bits: %.1f dB at 1.4 s, %.1f dB at 2.0 s\n", db(r1, r0), db(r2, r0));
		check(r1 > 0.f && db(r1, r0) > -45.f && db(r2, r0) < db(r1, r0), "crash tail fades smoothly, not cut off");
	}
	{   // DECAY reads true: open hat 1.0x = 0.7 s to -60 dB
		auto k = stockKnobs<CymbalsEngine>(); k[ki<CymbalsEngine>("BITS")] = 1.f;
		auto s = render<CymbalsEngine>(k, 0.f, 2.f, 909, 0.f, FS, true);
		float p0 = rms(s, 0.f, 0.02f), tEnd = 0.f;
		for (float t = 0.f; t < 1.9f; t += 0.01f) if (rms(s, t, t + 0.02f) > p0 * 0.001f) tEnd = t + 0.01f;
		printf("  open hat -60 dB at %.2f s (DECAY says 0.70 s)\n", tEnd);
		check(std::fabs(tEnd / 0.7f - 1.f) < 0.2f, "open hat DECAY reads true");
	}
	{   // a 44.1 kHz WAV: a 5 kHz sine stays clean (false tones well down)
		std::vector<float> w(44100);
		for (size_t i = 0; i < w.size(); i++) w[i] = 0.8f * std::sin(2.f * PI * 5000.f * i / 44100.f);
		auto k = stockKnobs<CymbalsEngine>();
		k[ki<CymbalsEngine>("BITS")] = 1.f; k[ki<CymbalsEngine>("RECON")] = 1.f; k[ki<CymbalsEngine>("CUT")] = 0.f; k[ki<CymbalsEngine>("DECAY")] = 1.f;
		CymbalsEngine e; e.setSampleRate(FS); e.setSample(w, 44100.f);
		e.hit2(0.f, k.data());
		std::vector<float> s; Svf l1, l2;   // look below 12 kHz: the DAC's own images above are genuine
		for (int i = 0; i < 0.4f * FS; i++) { e.process(k.data(), 0.f); l1.process(e.out, 12000.f, 0.54f, FS); l2.process(l1.lp, 12000.f, 1.31f, FS); s.push_back(l2.lp); }
		// fit a 5 kHz sine with a slowly changing level over short frames; the rest is false
		double sig = 0, res = 0; int N = 480;
		for (int f0 = (int)(0.05f * FS); f0 + N < (int)s.size(); f0 += N) {
			double a = 0, b = 0;
			for (int i = 0; i < N; i++) { double ph = 2 * PI * 5000.0 * (f0 + i) / FS; a += s[f0 + i] * std::cos(ph); b += s[f0 + i] * std::sin(ph); }
			a *= 2.0 / N; b *= 2.0 / N;
			for (int i = 0; i < N; i++) { double ph = 2 * PI * 5000.0 * (f0 + i) / FS; double fit = a * std::cos(ph) + b * std::sin(ph); sig += fit * fit; res += (s[f0 + i] - fit) * (s[f0 + i] - fit); }
		}
		float fal = 10.f * std::log10((float)(res / sig));
		printf("  5 kHz sine in a 44.1 kHz WAV: everything else below 12 kHz %.1f dB\n", fal);
		check(fal < -40.f, "loaded WAV is resampled cleanly");
	}
	{   // WAV reader refuses odd headers
		auto writeHdr = [](const char* path, int fmt, int bits) {
			FILE* f = fopen(path, "wb");
			auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
			auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
			fwrite("RIFF", 1, 4, f); w32(36 + 64); fwrite("WAVEfmt ", 1, 8, f);
			w32(16); w16(fmt); w16(1); w32(44100); w32(44100 * 2); w16((uint16_t)std::max(1, bits / 8)); w16(bits);
			fwrite("data", 1, 4, f); w32(64); for (int i = 0; i < 64; i++) fputc(i * 3, f);
			fclose(f);
		};
		std::vector<float> d; float sr;
		writeHdr("/tmp/c909_4bit.wav", 1, 4); bool b4 = readWavFile("/tmp/c909_4bit.wav", d, sr);
		writeHdr("/tmp/c909_12bit.wav", 1, 12); bool b12 = readWavFile("/tmp/c909_12bit.wav", d, sr);
		writeHdr("/tmp/c909_16bit.wav", 1, 16); bool b16 = readWavFile("/tmp/c909_16bit.wav", d, sr);
		writeHdr("/tmp/c909_f64.wav", 3, 64); bool f64 = readWavFile("/tmp/c909_f64.wav", d, sr);
		check(!b4 && !b12 && b16 && f64, "WAV reader: 4/12-bit refused, 16-bit and 64-bit float read");
	}
	{   // tom skin noise: same at 48 and 96 kHz
		auto k = stockKnobs<TomEngine>(); k[ki<TomEngine>("NOISE")] = 1.f;
		float a = loud<TomEngine>(k, 0.2f, 0.f, 48000.f), b = loud<TomEngine>(k, 0.2f, 0.f, 96000.f);
		printf("  tom NOISE 100%% 96 vs 48 kHz: %+.2f dB\n", db(b, a));
		check(std::fabs(db(b, a)) < 1.f, "tom noise holds its level at 96 kHz");
	}
	{   // rim over V/OCT
		auto k = stockKnobs<RimEngine>();
		float lo = 1e9f, hi = 0.f;
		for (int st = -12; st <= 12; st += 3) { float l = loud<RimEngine>(k, 0.05f, 0.f, FS, st / 12.f); lo = std::min(lo, l); hi = std::max(hi, l); }
		printf("  rim over V/OCT -1..+1: %.1f dB spread\n", db(hi, lo));
		check(db(hi, lo) < 3.f, "rim level even over V/OCT");
	}
	{   // output stage: accent keeps its peak, a clean sine stays clean, DRIVE not a big volume knob
		auto k = stockKnobs<KickEngine>(); k[ki<KickEngine>("ATTACK")] = 0.f;
		float pu = peak(render<KickEngine>(k, 0.f, 0.5f)), pa = peak(render<KickEngine>(k, 1.f, 0.5f));
		printf("  kick accent peak +%.1f dB\n", db(pa, pu));
		check(db(pa, pu) > 4.f, "accent keeps most of its peak");
		auto kd = stockKnobs<KickEngine>(); kd[ki<KickEngine>("DRIVE")] = 1.f;
		float l0 = loud<KickEngine>(stockKnobs<KickEngine>(), 0.1f), l1 = loud<KickEngine>(kd, 0.1f);
		auto hd = stockKnobs<CymbalsEngine>(); hd[ki<CymbalsEngine>("DRIVE")] = 1.f;
		float h0 = loud<CymbalsEngine>(stockKnobs<CymbalsEngine>(), 0.1f), h1 = loud<CymbalsEngine>(hd, 0.1f);
		printf("  DRIVE 1 vs 0 loudness: kick %+.1f dB, hat %+.1f dB\n", db(l1, l0), db(h1, h0));
		check(db(l1, l0) < 6.f && db(h1, h0) < 7.f, "DRIVE adds at most about 6 dB");
		double th = 0, fu = 0;
		for (int i = 0; i < 4800; i++) { float x = 6.f * std::sin(2.f * PI * 100.f * i / FS); float y = opamp(x, 0.f); th += y * std::sin(3 * 2.f * PI * 100.f * i / FS); fu += y * std::sin(2.f * PI * 100.f * i / FS); }
		printf("  6 V sine through the stage at DRIVE 0: 3rd harmonic %.1f dB\n", db(std::fabs((float)th), std::fabs((float)fu)));
		check(db(std::fabs((float)th), std::fabs((float)fu)) < -45.f, "output stage clean at DRIVE 0");
	}
	{   // retriggering a ringing kick and snare: no hard step
		auto rollStep = [&](auto e, std::vector<float> k, int gap) {
			e.setSampleRate(FS); float prev = 0.f, worst = 0.f;
			for (int i = 0; i < FS; i++) { if (i % gap == 0) e.hit(0.f, k.data()); e.process(k.data(), 0.f); if (i > 10) worst = std::max(worst, std::fabs(e.out - prev)); prev = e.out; }
			return worst;
		};
		auto kk = stockKnobs<KickEngine>(); kk[ki<KickEngine>("ATTACK")] = 0.f; kk[ki<KickEngine>("DECAY")] = 1.f;
		auto ks = stockKnobs<SnareEngine>(); ks[ki<SnareEngine>("SNAPPY")] = 0.f;
		float a = rollStep(KickEngine(), kk, (int)(0.03f * FS)), b = rollStep(SnareEngine(), ks, (int)(0.06f * FS));
		printf("  largest step in a roll: kick %.2f V, snare %.2f V\n", a, b);
		check(a < 0.8f && b < 0.8f, "retrigger fades the old tail (no click)");
	}

	// demos: each voice, then the whole kit
	{
		const int bars = 4;
		int step = (int)(60.f / 128.f / 4.f * FS);
		KickEngine kick; SnareEngine snare; TomEngine tom; RimEngine rim; ClapEngine clap; CymbalsEngine cym, ride;
		auto kk = stockKnobs<KickEngine>(), ks = stockKnobs<SnareEngine>(), kt = stockKnobs<TomEngine>();
		auto kr = stockKnobs<RimEngine>(), kc = stockKnobs<ClapEngine>(), ky = stockKnobs<CymbalsEngine>(), kz = stockKnobs<CymbalsEngine>();
		kz[ki<CymbalsEngine>("TYPE")] = 2.f; kz[ki<CymbalsEngine>("DECAY")] = 1.f;
		std::vector<float> o, ok_, os, oc, oh;
		for (int i = 0; i < step * 16 * bars; i++) {
			if (i % step == 0) {
				int s = (i / step) % 16, bar = i / step / 16;
				if (s % 4 == 0) kick.hit(s == 0 ? 1.f : 0.f, kk.data());
				if (s == 4 || s == 12) { snare.hit(0.f, ks.data()); clap.hit(1.f, kc.data()); }
				if (s % 4 == 2) cym.hit2(s == 6 || s == 14 ? 1.f : 0.f, ky.data());
				else cym.hit(0.f, ky.data());
				if (s % 4 == 0) ride.hit(0.f, kz.data());
				if (s == 3 || s == 11 || s == 14) rim.hit(0.f, kr.data());
				if (bar == bars - 1 && s >= 12) { kt[ki<TomEngine>("RANGE")] = s >= 14 ? 0.f : 1.f; if (s % 1 == 0) tom.hit(s == 12 ? 1.f : 0.f, kt.data()); }
			}
			kick.process(kk.data(), 0.f); snare.process(ks.data(), 0.f); tom.process(kt.data(), 0.f);
			rim.process(kr.data(), 0.f); clap.process(kc.data(), 0.f); cym.process(ky.data(), 0.f); ride.process(kz.data(), 0.f);
			float mix = 0.55f * kick.out + 0.3f * snare.out + 0.3f * clap.out + 0.25f * tom.out + 0.2f * rim.out + 0.22f * cym.out + 0.12f * ride.out;
			o.push_back(mix);
			ok_.push_back(kick.out); os.push_back(snare.out + clap.out * 0.8f); oh.push_back(cym.out);
		}
		wav(dir + "/909-kit-stock.wav", o);
		wav(dir + "/909-kick-stock.wav", ok_);
		wav(dir + "/909-snare-clap-stock.wav", os);
		wav(dir + "/909-hats-stock.wav", oh);
	}
	{   // kick variations: stock, long tune, hard attack + drive, deep sub
		std::vector<float> o;
		int step = (int)(60.f / 128.f * FS);
		auto base = stockKnobs<KickEngine>();
		std::vector<std::vector<float>> vs(4, base);
		vs[1][ki<KickEngine>("TUNE")] = 0.85f; vs[1][ki<KickEngine>("DECAY")] = 0.7f;
		vs[2][ki<KickEngine>("ATTACK")] = 1.f; vs[2][ki<KickEngine>("DRIVE")] = 0.6f; vs[2][ki<KickEngine>("SHAPE")] = 0.45f;
		vs[3][ki<KickEngine>("BASE")] = 0.2f; vs[3][ki<KickEngine>("DECAY")] = 0.85f; vs[3][ki<KickEngine>("ATTACK")] = 0.2f;
		for (int v = 0; v < 4; v++) {
			KickEngine e; e.setSampleRate(FS);
			for (int i = 0; i < step * 8; i++) { if (i % step == 0) e.hit((i / step) % 2 == 0 ? 1.f : 0.f, vs[v].data()); e.process(vs[v].data(), 0.f); o.push_back(e.out); }
		}
		wav(dir + "/909-kick-variations.wav", o);
	}
	{   // toms roll and rim/clap with AGE
		std::vector<float> o;
		int step = (int)(60.f / 128.f / 4.f * FS);
		TomEngine e; e.setSampleRate(FS);
		auto k = stockKnobs<TomEngine>();
		for (int i = 0; i < step * 32; i++) {
			if (i % step == 0) { int s = (i / step) % 16; if (s % 2 == 0 || s > 11) { k[ki<TomEngine>("RANGE")] = 2.f - (float)((s / 4) % 3); e.hit(s % 4 == 0 ? 1.f : 0.f, k.data()); } }
			e.process(k.data(), 0.f); o.push_back(e.out);
		}
		wav(dir + "/909-toms.wav", o);
	}
	{   // cymbals: hats with BITS down, crash, ride
		std::vector<float> o;
		int step = (int)(60.f / 128.f / 4.f * FS);
		auto k = stockKnobs<CymbalsEngine>();
		CymbalsEngine e; e.setSampleRate(FS);
		for (int i = 0; i < step * 64; i++) {
			int s = i / step;
			if (i % step == 0) {
				k = stockKnobs<CymbalsEngine>();
				if (s >= 16 && s < 32) k[ki<CymbalsEngine>("BITS")] = 0.1f;
				if (s >= 32 && s < 48) { k[ki<CymbalsEngine>("TYPE")] = 2.f; k[ki<CymbalsEngine>("DECAY")] = 1.f; }
				if (s >= 48) { k[ki<CymbalsEngine>("TYPE")] = 1.f; k[ki<CymbalsEngine>("DECAY")] = 1.f; }
				if (s < 32) { if (s % 4 == 2) e.hit2(0.f, k.data()); else e.hit(s % 2 ? 0.f : 1.f, k.data()); }
				else if (s < 48) { if (s % 2 == 0) e.hit(s % 4 == 0 ? 1.f : 0.f, k.data()); }
				else if (s == 48) e.hit(1.f, k.data());
			}
			e.process(k.data(), 0.f); o.push_back(e.out);
		}
		wav(dir + "/909-cymbals.wav", o);
	}

	printf(fails ? "%d FAILED\n" : "all voice checks passed\n", fails);
	return fails ? 1 : 0;
}
