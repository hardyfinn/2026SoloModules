// Demo renders through the C++ engines: the eighteen design sketches (same settings as
// tools/proto/make_sketches.py and make_sketches2.py) and one render per factory preset.
// Included by render.cpp. Sources: tools/proto/source/*.wav, run from the plugin folder.
#pragma once
#include "presets.h"

typedef std::function<double(double)> Fn;
static Fn K(double v) { return [v](double) { return v; }; }
static Fn ramp(double t0, double t1, double a, double b) { return [=](double t) { return a + (b - a) * std::min(1.0, std::max(0.0, (t - t0) / (t1 - t0))); }; }
static Fn lfo(double hz, double lo, double hi) { return [=](double t) { return lo + (hi - lo) * (0.5 - 0.5 * std::cos(2 * PI * hz * t)); }; }

static Stereo loadSrc(const char* name, double from, double len) {
	WavData w; Stereo s;
	if (!readWav(std::string("tools/proto/source/") + name, w)) { printf("missing source %s\n", name); return s; }
	size_t a = (size_t)(from * w.sampleRate), n = (size_t)(len * w.sampleRate);
	for (size_t i = a; i < a + n && i < w.l.size(); i++) { s.l.push_back(w.l[i]); s.r.push_back(w.r.empty() ? w.l[i] : w.r[i]); }
	return s;
}
static Stereo tail(Stereo s, double sec, double fs) { s.l.resize(s.l.size() + (size_t)(sec * fs), 0.0); s.r.resize(s.r.size() + (size_t)(sec * fs), 0.0); return s; }
static void save(const std::string& path, const Stereo& y, double norm = 0.7) {
	double pk = 1e-9; for (size_t i = 0; i < y.l.size(); i++) pk = std::max(pk, std::max(std::fabs(y.l[i]), std::fabs(y.r[i])));
	double g = norm > 0 ? norm / pk : 1.0;
	std::vector<float> l(y.l.size()), r(y.l.size());
	for (size_t i = 0; i < l.size(); i++) { l[i] = (float)(y.l[i] * g); r[i] = (float)(y.r[i] * g); }
	writeWav(path, l, &r, 44100);
	printf("demo  %s\n", path.c_str());
}

// Python-style sketch description: physical units, functions of time.
struct Sk {
	Fn pitch = K(0), rateHz = K(8), sizeSec = K(0.25), contour = K(0), scan = K(0.25), spray = K(0), pattern = K(0), wet = K(1), feedback = K(0);
	Fn randPitch = K(0), randRate = K(0), randSize = K(0), randAmp = K(0), randRev = K(0), age = K(0);
	std::function<bool(double)> lock = [](double) { return false; };
	int scanMode = SCAN_DELAY_FREE, sprayMode = SPRAY_RANDOM, rpMode = RP_DETUNE, scale = 0;
	double bpm = 122;
};
static Stereo runSk(const Stereo& x, const Sk& s, double fs = 44100) {
	GrainEngine e; e.seed(1); e.setPatternSeed(1);
	auto apply = [&](double t) {
		GrainParams& p = e.p;
		p.sync = false; p.pitch = s.pitch(t); p.rateKnob = rateKnob(s.rateHz(t)); p.sizeKnob = sizeKnob(s.sizeSec(t), s.bpm);
		p.contour = s.contour(t); p.scanMode = s.scanMode;
		double sc = s.scan(t);
		p.scan = s.scanMode == SCAN_DELAY_FREE ? delayKnob(sc) : s.scanMode == SCAN_STRETCH ? (sc + 2) / 4 : sc;
		p.sprayMode = s.sprayMode; p.spray = s.spray(t); p.pattern = s.pattern(t); p.wet = s.wet(t); p.feedback = s.feedback(t);
		p.rpMode = s.rpMode; p.randPitch = s.randPitch(t); p.randRate = s.randRate(t); p.randSize = s.randSize(t); p.randAmp = s.randAmp(t); p.randRev = s.randRev(t);
		p.age = s.age(t); p.lock = s.lock(t); p.scale = s.scale;
	};
	apply(0); e.init(fs, 4);
	return runGrains(e, x, fs, s.bpm, [&](GrainEngine&, size_t i) { if (i % 32 == 0) apply(i / fs); });
}
static Stereo runRes(const Stereo& x, std::function<void(ResParams&)> set, double fs = 44100) {
	ResEngine e; set(e.p); e.init(fs);
	Stereo y{Vec(x.l.size()), Vec(x.l.size())};
	for (size_t i = 0; i < x.l.size(); i++) { double a, b; e.process(x.l[i], x.r[i], a, b); y.l[i] = a; y.r[i] = b; }
	return y;
}

static void renderDemos(const std::string& out) {
	const double fs = 44100, beat = 60.0 / 122, bar = 4 * beat;
	Stereo H = loadSrc("house-stabs-122bpm.wav", 0.3, 8 * bar), B = loadSrc("ballad-piano.wav", 0.3, 20);
	if (H.l.empty() || B.l.empty()) return;
	auto P = [&](const char* n) { return out + "/" + n + ".wav"; };
	{ Sk s; s.wet = K(0); save(P("01-dry-house-chords"), runSk(H, s)); }
	{ Sk s; s.scan = K(1.5 * beat); s.rateHz = K(4 / beat); s.sizeSec = K(beat / 4 * 1.5); s.pitch = K(12); s.wet = K(0.55); s.feedback = K(0.35); save(P("02-grain-delay-eighths-octave-up"), runSk(tail(H, 3, fs), s)); }
	{ Sk s; s.scanMode = SCAN_STRETCH; s.scan = K(0.5); s.rateHz = K(8 / beat); s.sizeSec = K(beat); s.spray = K(0.06); save(P("03-stretch-half-speed-cloud"), runSk(tail(H, 3, fs), s)); }
	{ Sk s; s.scan = K(beat); s.rateHz = K(4 / beat); s.sizeSec = K(beat / 4); s.contour = K(-0.9); s.pattern = K(0.5); s.scale = 10; s.wet = K(0.8); save(P("04-pattern-arpeggio-from-chords"), runSk(tail(H, 3, fs), s)); }
	{ Sk s; s.scan = K(beat / 2); s.rateHz = K(2 / beat); s.sizeSec = K(beat / 2); s.contour = ramp(0, 14, -1, 1); save(P("05-contour-sweep-pluck-to-swell"), runSk(tail(H, 3, fs), s)); }
	{ Sk s; s.scanMode = SCAN_POSITION; s.scan = lfo(0.08, 0.05, 0.95); s.rateHz = K(8 / beat); s.sizeSec = K(beat / 2); s.spray = K(0.01);
	  s.lock = [=](double t) { return t > 2 * bar; }; s.wet = ramp(2 * bar - 0.2, 2 * bar + 0.2, 0, 1); save(P("06-lock-then-scan-position"), runSk(tail(H, 6, fs), s)); }
	Stereo B10{Vec(B.l.begin(), B.l.begin() + (size_t)(10 * fs)), Vec(B.r.begin(), B.r.begin() + (size_t)(10 * fs))};
	{ Sk s; s.wet = K(0); save(P("07-ballad-dry"), runSk(B10, s)); }
	{ Sk s; s.scan = K(0.6); s.rateHz = K(7); s.sizeSec = K(0.45); s.randRev = K(0.6); s.rpMode = RP_RANDOM; s.randPitch = K(0.5); s.scale = 1; s.spray = K(0.1); s.wet = K(0.6); save(P("08-ballad-reverse-and-random-pitch"), runSk(tail(B, 3, fs), s)); }
	{ Sk s; s.scan = K(0.5); s.rateHz = K(12); s.sizeSec = K(0.3); s.pitch = K(12); s.scale = 18; s.feedback = K(0.7); s.spray = K(0.05); s.wet = K(0.5); save(P("09-ballad-feedback-shimmer"), runSk(tail(B, 6, fs), s)); }
	{ Sk s; s.scan = K(1.2); s.rateHz = K(10); s.sizeSec = K(0.6); s.sprayMode = SPRAY_WARP; s.spray = K(0.15); s.randPitch = K(0.08); s.wet = K(0.75); save(P("10-ballad-warp"), runSk(tail(B, 3, fs), s)); }
	{ Sk s; s.scanMode = SCAN_STRETCH; s.scan = K(0.25); s.rateHz = ramp(0, 15, 30, 220); s.sizeSec = K(0.03); s.contour = K(-0.5); save(P("11-fast-grains-pitched-buzz"), runSk(tail(H, 3, fs), s)); }
	{ Sk s; s.scan = K(beat); s.rateHz = K(4 / beat); s.sizeSec = K(beat / 2); s.randRate = K(1); s.randSize = K(0.8); s.randAmp = K(0.8); s.randRev = K(0.3); s.spray = K(0.2); s.pitch = K(-12); s.wet = K(0.7);
	  save(P("12-random-rate-size-amp-scatter"), runSk(tail(H, 3, fs), s)); }
	Stereo H4{Vec(H.l.begin(), H.l.begin() + (size_t)(4 * bar * fs)), Vec(H.r.begin(), H.r.begin() + (size_t)(4 * bar * fs))};
	for (double age : {0.0, 0.5, 1.0}) {
		Sk s; s.scan = K(beat); s.rateHz = K(4 / beat); s.sizeSec = K(beat / 2); s.pitch = K(-12); s.spray = K(0.03); s.feedback = K(0.5); s.wet = K(0.8); s.age = K(age);
		save(P(fmt("13-age-%03d-grains-octave-down", (int)(age * 100)).c_str()), runSk(tail(H4, 3, fs), s));
	}
	save(P("14-resonator-pentatonic-ring-on-house"), runRes(tail(H4, 4, fs), [](ResParams& p) { p.scale = 10; p.pitch = 9; p.decay = 0.7; p.wet = 0.6; }));
	Stereo B12{Vec(B.l.begin(), B.l.begin() + (size_t)(12 * fs)), Vec(B.r.begin(), B.r.begin() + (size_t)(12 * fs))};
	save(P("15-resonator-fifths-long-ring-on-piano"), runRes(tail(B12, 6, fs), [](ResParams& p) { p.scale = 17; p.pitch = 3; p.decay = 0.85; p.wet = 0.5; }));
	save(P("16-resonator-as-filter-lowpass-resonant"), runRes(H4, [](ResParams& p) { p.cutoff = 700; p.res = 0.6; p.slope = -1; }));
	save(P("17-resonator-band-pass-chromatic-ring"), runRes(tail(H4, 3, fs), [](ResParams& p) { p.cutoff = 900; p.res = 0.3; p.slope = 0; p.decay = 0.55; }));
	{
		Sk s; s.scanMode = SCAN_STRETCH; s.scan = K(0.5); s.rateHz = K(9); s.sizeSec = K(0.5); s.spray = K(0.08); s.randRev = K(0.3);
		Stereo y = runSk(tail(B12, 2, fs), s);
		save(P("18-grains-into-resonator-major-on-piano"), runRes(tail(y, 4, fs), [](ResParams& p) { p.scale = 1; p.pitch = 3; p.decay = 0.75; p.wet = 0.55; }));
	}
	// one render per factory preset, on the house chords (Essphor, Essphor Res) at 122 BPM
	for (int i = 0; i < NUM_GRAIN_PRESETS; i++) {
		GrainEngine e; e.seed(1); e.setPatternSeed(1); GRAIN_PRESETS[i].apply(e.p); e.init(fs, 4);
		Stereo x = tail(H, 4, fs);
		std::function<void(GrainEngine&, size_t)> each = nullptr;
		if (GRAIN_PRESETS[i].demoLockAt > 0) {
			double at = GRAIN_PRESETS[i].demoLockAt * bar;
			each = [=](GrainEngine& g, size_t k) { double t = k / fs; g.p.lock = t > at; if (g.p.scanMode == SCAN_POSITION) g.p.scan = 0.5 + 0.45 * std::sin(2 * PI * 0.1 * t); };
		}
		save(out + "/preset-essphor-" + GRAIN_PRESETS[i].file + ".wav", runGrains(e, x, fs, 122, each));
	}
	for (int i = 0; i < NUM_RES_PRESETS; i++) {
		Stereo x = tail(H, 4, fs);
		const ResPreset& rp = RES_PRESETS[i];
		ResEngine e; rp.apply(e.p); e.init(fs);
		Stereo y{Vec(x.l.size()), Vec(x.l.size())};
		const double base = e.p.cutoff;
		for (size_t k = 0; k < x.l.size(); k++) {
			if (rp.demoSweep) e.p.cutoff = base * std::pow(2.0, 2.5 * std::sin(2 * PI * 0.125 * k / fs));
			double a, b; e.process(x.l[k], x.r[k], a, b); y.l[k] = a; y.r[k] = b;
		}
		save(out + "/preset-res-" + rp.file + ".wav", y);
	}
}
