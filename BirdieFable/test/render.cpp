// Offline checks and demo WAVs for Birdie. No Rack needed:  make test
// Fails loudly (exit 1) on any check that does not pass.
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include "../src/dsp/presets.h"
#include "../src/dsp/BirdieCore.hpp"

using namespace birdie;

// the reference engine, compiled as C from tools/proto with renamed symbols
extern "C" {
void ref_bd_init(Birdie* b, double sr);
void ref_bd_note(Birdie* b, const BdParams* p);
void ref_bd_phrase(Birdie* b, const BdParams* p, uint32_t seed);
double ref_bd_process(Birdie* b, const BdParams* p);
}

static int failures = 0, passes = 0;
static void check(bool ok, const char* what, const std::string& detail = "") {
	if (ok) { passes++; printf("  ok    %s %s\n", what, detail.c_str()); }
	else { failures++; printf("  FAIL  %s %s\n", what, detail.c_str()); }
}
static std::string fmt(const char* f, ...) {
	char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b;
}

static void writeWav(const std::string& fn, const std::vector<float>& x, int sr) {
	FILE* f = fopen(fn.c_str(), "wb");
	if (!f) { perror(fn.c_str()); return; }
	int32_t i32; int16_t i16; int32_t ds = (int32_t)x.size() * 2;
	fwrite("RIFF", 1, 4, f); i32 = 36 + ds; fwrite(&i32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	i32 = 16; fwrite(&i32, 4, 1, f); i16 = 1; fwrite(&i16, 2, 1, f); fwrite(&i16, 2, 1, f);
	i32 = sr; fwrite(&i32, 4, 1, f); i32 = sr * 2; fwrite(&i32, 4, 1, f); i16 = 2; fwrite(&i16, 2, 1, f); i16 = 16; fwrite(&i16, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&ds, 4, 1, f);
	for (float v : x) { double s = v / 10.0; s = s > 1 ? 1 : (s < -1 ? -1 : s); i16 = (int16_t)std::lround(s * 32767); fwrite(&i16, 2, 1, f); }
	fclose(f);
}

static std::string slug(const char* name) {
	std::string s;
	for (const char* c = name; *c; c++) s += (*c == ' ') ? '-' : (char)tolower(*c);
	return s;
}

// ---------------------------------------------------------------- engine-level helpers
typedef void (*InitF)(Birdie*, double);
typedef void (*NoteF)(Birdie*, const BdParams*);
typedef void (*PhraseF)(Birdie*, const BdParams*, uint32_t);
typedef double (*ProcF)(Birdie*, const BdParams*);

// Same trigger script as tools/proto/render_ref.c
static std::vector<float> renderScript(const BdPreset& pr, int sr, InitF init, NoteF note, PhraseF phrase, ProcF proc) {
	static Birdie b;
	init(&b, sr);
	BdParams p = pr.p;
	std::vector<float> out;
	int cap = sr * 60, wait = (int)(0.2 * sr), stage = 0, held = p.hold;
	while ((int)out.size() < cap) {
		if (!b.phOn && !b.noteOn) {
			if (wait > 0) wait--;
			else {
				if (stage < 3) { if (held) note(&b, &p); else phrase(&b, &p, pr.seed + stage); }
				else if (stage == 3) note(&b, &p);
				else if (stage == 4) p.hold = 0;
				else break;
				stage++; wait = (int)((held ? 1.5 : 0.45) * sr);
			}
		}
		out.push_back((float)proc(&b, &p));
	}
	return out;
}

struct Steady { double f0, peak, bright; };
static Steady steady(double size, double tension, double pressure, int sr = 48000) {
	static Birdie b;
	bd_init(&b, sr);
	BdParams p = bd_default();
	p.size = size; p.tension = tension; p.pressure = pressure; p.hold = 1; p.breath = 0; p.throat = 0; p.tube = 0; p.beak = 16000;
	int N = sr * 3; double prev = 0, t0 = -1, t1 = 0; int nc = 0; double s2 = 0, d2 = 0, pk = 0;
	for (int n = 0; n < N; n++) {
		double v = bd_process(&b, &p);
		if (n > sr) {
			if (prev <= 0 && v > 0) { double fr = -prev / (v - prev); double t = (n - 1 + fr) / sr; if (t0 < 0) t0 = t; t1 = t; nc++; }
			s2 += v * v; d2 += (v - prev) * (v - prev) * (double)sr * sr; pk = std::max(pk, std::fabs(v));
		}
		prev = v;
	}
	double f = (nc - 1) / (t1 - t0);
	return {f, pk, std::sqrt(d2 / s2) / (2 * M_PI * f)};
}

// ---------------------------------------------------------------- module-level (Core) helpers
struct Rig {
	Core core;
	float kv[NUM_P] = {};
	float in[NUM_I] = {};
	bool conn[NUM_I] = {};
	std::vector<float> out, gate, end, press, pitch;
	Rig(int sr, const BdParams& p, uint32_t seed) {
		core.setSampleRate(sr);
		core.seedRng = 12345u;
		core.seed = seed;
		paramsToKnobs(p, kv);
	}
	void step() {
		core.process(kv, in, conn);
		out.push_back(core.out[OUT_O]); gate.push_back(core.out[GATE_O]); end.push_back(core.out[END_O]);
		press.push_back(core.out[PRESS_O]); pitch.push_back(core.out[PITCH_O]);
		for (int i : {NOTE_I, PHRASE_I, NEW_I}) if (conn[i]) in[i] = 0.f;   // triggers last one sample
	}
	void trig(int i) { conn[i] = true; in[i] = 10.f; }
	bool idle() const { return !core.eng.phOn && !core.eng.noteOn; }
	void run(double sec, const std::function<void()>& every = nullptr) {
		int n = (int)std::lround(sec * core.sr);
		for (int i = 0; i < n; i++) { if (every) every(); step(); }
	}
};

static std::vector<int> rises(const std::vector<float>& g) {
	std::vector<int> r;
	for (size_t i = 0; i < g.size(); i++) if (g[i] > 5.f && (i == 0 || g[i - 1] <= 5.f)) r.push_back((int)i);
	return r;
}
static double rms(const std::vector<float>& x, size_t a, size_t b) {
	double s = 0; b = std::min(b, x.size());
	for (size_t i = a; i < b; i++) s += (double)x[i] * x[i];
	return b > a ? std::sqrt(s / (b - a)) : 0.0;
}

// Octave-band levels (63 Hz .. 8 kHz) by RBJ band-pass, one octave wide.
static std::vector<double> bands(const std::vector<float>& x, int sr) {
	std::vector<double> r;
	for (double fc = 63; fc < 12000; fc *= 2) {
		double w = 2 * M_PI * fc / sr, al = std::sin(w) * std::sinh(std::log(2.0) / 2 * 1.0 * w / std::sin(w));
		double b0 = al, b2 = -al, a0 = 1 + al, a1 = -2 * std::cos(w), a2 = 1 - al;
		double x1 = 0, x2 = 0, y1 = 0, y2 = 0, e = 0;
		for (float v : x) {
			double y = (b0 * v + b2 * x2 - a1 * y1 - a2 * y2) / a0;
			x2 = x1; x1 = v; y2 = y1; y1 = y; e += y * y;
		}
		r.push_back(10 * std::log10(e / x.size() + 1e-20));
	}
	return r;
}

// ---------------------------------------------------------------- demos
// Every preset through the module path: several phrases (seeds s, s+1, ...) then one note.
static std::vector<float> presetDemo(const BdPreset& pr, int sr, double seconds) {
	Rig r(sr, pr.p, pr.seed);
	int bassClock = pr.p.clockPeriod > 0;
	double clockPer = 60.0 / 124.0;            // quarter notes at 124 bpm
	int phrases = 0; double t = 0, wait = 0.2;
	int total = (int)(seconds * sr);
	if (pr.p.hold) {
		// Drone: held tone with a phrase now and then
		double nextP = 2.0;
		r.run(seconds, [&]() {
			t += 1.0 / sr;
			if (t >= nextP) { r.core.seed = pr.seed + phrases++; r.trig(PHRASE_I); nextP += 4.0; }
			if (t > seconds - 1.5) r.kv[HOLD_P] = 0.f;
		});
		return r.out;
	}
	for (int n = 0; n < total; n++) {
		t += 1.0 / sr;
		if (bassClock) {
			r.conn[CLOCK_I] = true;
			double ph = std::fmod(t, clockPer);
			r.in[CLOCK_I] = ph < 0.005 ? 10.f : 0.f;
			// a phrase every bar, starting on the downbeat once the clock is known
			if (t > clockPer * 2 && std::fmod(t, clockPer * 4) < 1.0 / sr && n + sr * 2 < total) {
				r.core.seed = pr.seed + phrases++; r.trig(PHRASE_I);
			}
		}
		else if (r.idle()) {
			wait -= 1.0 / sr;
			if (wait <= 0 && (double)(total - n) / sr > 1.5) {
				if (phrases < 4) { r.core.seed = pr.seed + phrases; r.trig(PHRASE_I); }
				else if (phrases == 4) r.trig(NOTE_I);
				phrases++;
				wait = 0.45 * r.core.P.time + 0.15;
			}
		}
		r.step();
	}
	return r.out;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "test/out";
	static BdPreset pre[32];
	int NP = bd_presets(pre);
	printf("Birdie offline checks (%d presets)\n", NP);

	// 0. presets survive the trip through knob positions
	printf("0. presets as knob positions\n");
	check(NP == 20, "twenty factory presets", fmt("(%d)", NP));
	{
		double worst = 0; const char* wname = "";
		for (int k = 0; k < NP; k++) {
			Rig r(48000, pre[k].p, pre[k].seed);
			r.core.buildParams(r.kv, r.in, r.conn);
			const BdParams &a = pre[k].p, &b = r.core.P;
			double d[] = {std::fabs(std::log(a.size / b.size)), std::fabs(a.tension - b.tension), std::fabs(a.pressure - b.pressure),
			              std::fabs(a.breath - b.breath), std::fabs(a.rough - b.rough), std::fabs(a.v2mix - b.v2mix), std::fabs(a.v2int - b.v2int),
			              std::fabs(a.v2slide - b.v2slide), std::fabs(std::log(a.length / b.length)), std::fabs(a.slide - b.slide),
			              (double)(a.shape != b.shape), std::fabs(a.attack - b.attack), std::fabs(a.release - b.release),
			              std::fabs(std::log(a.buzzRate / b.buzzRate)), std::fabs(a.buzz - b.buzz), std::fabs(a.chop - b.chop),
			              std::fabs(std::log(a.trachea / b.trachea)), std::fabs(a.tube - b.tube), std::fabs(a.throat - b.throat),
			              std::fabs(a.tune - b.tune), (double)(a.follow != b.follow), std::fabs(std::log(a.beak / b.beak)),
			              std::fabs(std::log(a.time / b.time)), (double)(a.hold != b.hold), (double)(a.notes != b.notes),
			              (double)(a.pattern != b.pattern), std::fabs(a.vary - b.vary), std::fabs(a.drift - b.drift),
			              std::fabs(a.accel - b.accel), std::fabs(a.swell - b.swell),
			              a.clockPeriod > 0 ? 0.0 : std::fabs(std::log(a.rate / b.rate))};
			for (double v : d) if (v > worst) { worst = v; wname = pre[k].name; }
		}
		check(worst < 1e-4, "knob mapping round trip", fmt("(worst %.2g in %s)", worst, wname));
	}

	// 1. the C++ engine is the reference, sample for sample
	printf("1. same engine as tools/proto\n");
	{
		double worst = 0; const char* wname = "";
		for (int k = 0; k < NP; k++) {
			auto a = renderScript(pre[k], 48000, bd_init, bd_note, bd_phrase, bd_process);
			auto b = renderScript(pre[k], 48000, ref_bd_init, ref_bd_note, ref_bd_phrase, ref_bd_process);
			double d = a.size() == b.size() ? 0 : 1e9;
			for (size_t i = 0; i < std::min(a.size(), b.size()); i++) d = std::max(d, (double)std::fabs(a[i] - b[i]));
			if (d >= worst) { worst = d; wname = pre[k].name; }
		}
		check(worst < 1e-6, "all twenty presets match the reference", fmt("(worst %.2g V in %s; float output)", worst, wname));
	}

	// 2-4. steady tones
	printf("2. steady tones\n");
	struct Case { double size, tension, pressure, f0; } cases[] = {
		{1, -1, 0.22, 4026.8}, {4, -1, 0.22, 1006.7}, {16, -1, 0.22, 251.7}, {1, 6, 0.22, 6141.5},
		{1, -12, 0.22, 2208.2}, {1, -24, 0.30, 991.4}, {1, -36, 0.40, 410.3}};
	for (auto& c : cases) {
		Steady s = steady(c.size, c.tension, c.pressure);
		check(std::fabs(s.f0 / c.f0 - 1) < 0.005, "pitch", fmt("size %g tension %+g pressure %.2f: %.1f Hz (expect %.1f)", c.size, c.tension, c.pressure, s.f0, c.f0));
	}
	printf("3. pressure barely moves pitch\n");
	{
		double f = steady(1, -1, 0.22).f0, lo = steady(1, -1, 0.08).f0, hi = steady(1, -1, 0.50).f0;
		check(std::fabs(lo / f - 1) < 0.04 && std::fabs(hi / f - 1) < 0.04, "pressure 0.08 and 0.50 within 4%", fmt("(%.0f, %.0f, %.0f Hz)", lo, f, hi));
	}
	printf("4. rich edge\n");
	{
		double top = steady(1, -1, 0.22).bright, bottom = steady(1, -36, 0.40).bright;
		check(top < 1.15, "whistle is pure at tension -1", fmt("(brightness %.2f)", top));
		check(bottom > 8.0, "pulses are rich at tension -36", fmt("(brightness %.2f)", bottom));
	}

	// 5. phrases
	printf("5. phrases\n");
	{
		BdParams p = bd_default(); p.vary = 1.0; p.notes = 8;
		auto run = [&](uint32_t seed) { Rig r(48000, p, seed); r.trig(PHRASE_I); r.run(3.0); return r; };
		Rig a = run(77), b = run(77), c = run(78);
		check(a.out == b.out, "same seed renders identically");
		check(a.out != c.out, "a different seed gives a different phrase");
		check((int)rises(a.gate).size() == p.notes, "NOTES notes, one gate each", fmt("(%d)", (int)rises(a.gate).size()));
		check((int)rises(a.end).size() == 1, "END fires once", fmt("(%d)", (int)rises(a.end).size()));

		BdParams q = bd_default(); q.vary = 0.0;
		bool same = true;
		for (int pat = 0; pat < PT_COUNT; pat++) {
			q.pattern = pat;
			static Birdie e; bd_init(&e, 48000); bd_phrase(&e, &q, 1234u + pat);
			BdType plain; memset(&plain, 0, sizeof plain); plain.lenF = plain.pressF = plain.attF = plain.relF = 1.0; plain.shape = -1;
			for (int k = 0; k < 4; k++) {
				const BdType& t = e.types[k];   // compare by value (VARY 0 can leave -0.0)
				same = same && t.dT == 0 && t.dSlide == 0 && t.lenF == 1 && t.pressF == 1 && t.attF == 1 && t.relF == 1
				       && t.shape == -1 && !t.buzzOn && !t.v2On;
			}
		}
		check(same, "VARY 0: every note type is the panel note, every pattern");

		BdParams r25 = bd_default(); r25.pattern = PT_REPEAT; r25.rate = 25; r25.notes = 12; r25.length = 0.02;
		Rig r(48000, r25, 5); r.trig(PHRASE_I); r.run(1.0);
		auto on = rises(r.gate); int worst = 0;
		for (size_t i = 1; i < on.size(); i++) worst = std::max(worst, std::abs(on[i] - on[i - 1] - 1920));
		check(on.size() == 12 && worst <= 1, "REPEAT at RATE 25: onsets 40 ms apart", fmt("(worst %d samples)", worst));

		BdParams ac = r25; ac.accel = 1.0; ac.notes = 9;
		Rig r2(48000, ac, 5); r2.trig(PHRASE_I); r2.run(1.0);
		auto o2 = rises(r2.gate);
		bool okA = o2.size() == 9;
		if (okA) {
			double first = o2[1] - o2[0], last = o2[8] - o2[7];
			double eFirst = 1920.0, eLast = 1920.0 * std::pow(2.0, -7.0 / 8.0);
			okA = std::fabs(first - eFirst) <= 1.5 && std::fabs(last - eLast) <= 1.5;
			check(okA, "ACCEL +1: slots shrink to half by the end", fmt("(first %.0f, last %.0f samples; expect %.0f, %.0f)", first, last, eFirst, eLast));
		}
		else check(false, "ACCEL +1: slots shrink to half by the end", fmt("(%d notes)", (int)o2.size()));
	}

	// 6. clock lock
	printf("6. clock lock\n");
	{
		BdParams p = bd_default(); p.notes = 8; p.pattern = PT_REPEAT; p.length = 0.05;
		Rig r(48000, p, 9);
		r.kv[CLOCK_P] = 1.f;
		r.kv[RATE_P] = 6.5f / 9.f;     // 4 notes per clock
		r.conn[CLOCK_I] = true;
		int n = 0;
		r.run(3.0, [&]() {
			r.in[CLOCK_I] = (n % 24000) < 48 ? 10.f : 0.f;
			if (n == 24000 * 2 + 1) r.trig(PHRASE_I);
			n++;
		});
		auto on = rises(r.gate); int worst = 0;
		for (size_t i = 1; i < on.size(); i++) worst = std::max(worst, std::abs(on[i] - on[i - 1] - 6000));
		check(on.size() == 8 && worst <= 1, "0.5 s clock, 4 per clock: onsets 125 ms apart", fmt("(%d notes, worst %d samples)", (int)on.size(), worst));
	}

	// 7. NOTE, HOLD, AUTO
	printf("7. NOTE, HOLD, AUTO\n");
	{
		BdParams p = bd_default(); p.length = 0.3;
		Rig r(48000, p, 1); r.run(0.1); r.trig(NOTE_I); r.run(1.0);
		auto on = rises(r.gate);
		int len = 0; for (float g : r.gate) len += g > 5.f;
		size_t endN = on.empty() ? 0 : on[0] + len;
		check(on.size() == 1 && std::abs(len - 14400) <= 1, "NOTE: one note of LENGTH", fmt("(%d notes, %d samples)", (int)on.size(), len));
		check(rms(r.out, endN + 4800, endN + 24000) < 1e-3, "silence after the note", fmt("(rms %.2g V)", rms(r.out, endN + 4800, endN + 24000)));

		Rig h(48000, bd_default(), 1);
		h.run(0.2); h.kv[HOLD_P] = 1.f; size_t a = h.out.size(); h.run(1.0); h.kv[HOLD_P] = 0.f; size_t b = h.out.size(); h.run(0.5);
		double on30 = rms(h.out, a + 1440, a + 2880), off30 = rms(h.out, b + 1440, b + 4800), held = rms(h.out, a + 24000, a + 48000);
		check(on30 > 0.3 * held, "HOLD sounds within 30 ms", fmt("(%.2f V vs %.2f V held)", on30, held));
		check(off30 < 0.01 * held, "silent within 30 ms of release", fmt("(%.2g V)", off30));
		Rig hj(48000, bd_default(), 1);
		hj.conn[HOLD_I] = true; hj.in[HOLD_I] = 10.f; hj.run(0.5);
		check(rms(hj.out, 12000, 24000) > 0.3 * held, "HOLD gate input works too");

		BdParams q = bd_default(); q.notes = 3; q.rate = 10;
		Rig au(48000, q, 3);
		au.kv[AUTO_P] = 1.f; au.kv[GAP_P] = (float)unex(0.5, 0.1, 80.0);
		au.run(4.0);
		auto ends = rises(au.end), gates = rises(au.gate);
		int phr = (int)ends.size(); double worstGap = 0;
		for (int e : ends) {
			for (int g : gates) if (g > e) {
				// the next phrase starts GAP after the last note finished; END fires at that moment
				worstGap = std::max(worstGap, std::fabs((g - e) - 24000.0)); break;
			}
		}
		check(phr >= 4 && worstGap <= 2, "AUTO repeats with the GAP wait", fmt("(%d phrases, worst gap error %.0f samples)", phr, worstGap));
	}

	// 8. sample rates
	printf("8. sample rates\n");
	for (const char* name : {"Whistler", "Crow", "Bass Bird"}) {
		int k = 0; while (k < NP && strcmp(pre[k].name, name)) k++;
		auto ref = renderScript(pre[k], 48000, bd_init, bd_note, bd_phrase, bd_process);
		auto b48 = bands(ref, 48000);
		double top = *std::max_element(b48.begin(), b48.end());
		for (int sr : {44100, 96000}) {
			auto x = renderScript(pre[k], sr, bd_init, bd_note, bd_phrase, bd_process);
			auto bx = bands(x, sr); double worst = 0;
			for (size_t i = 0; i < b48.size(); i++) if (b48[i] > top - 30) worst = std::max(worst, std::fabs(bx[i] - b48[i]));
			check(worst < 1.5, "octave bands match 48 kHz", fmt("(%s at %d: worst %.2f dB)", name, sr, worst));
		}
	}

	// 9. safety
	printf("9. fuzzing\n");
	for (int sr : {44100, 48000, 96000}) {
		Rig r(sr, bd_default(), 42);
		uint32_t s = 99;
		auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) / 16777216.0; };
		bool bad = false; double peak = 0; long n = 0;
		for (int i : {SIZE_I, TENSION_I, PRESSURE_I, ROUGH_I, V2MIX_I, LENGTH_I, SLIDE_I, BUZZRATE_I, BEAK_I, TIME_I, RATE_I, VARY_I, CLOCK_I, HOLD_I}) r.conn[i] = true;
		r.kv[AUTO_P] = 1.f;
		long N = 60L * sr;
		for (n = 0; n < N; n++) {
			if (n % 4800 == 0) {
				for (int p = 0; p < NUM_P; p++) {
					if (p == SHAPE_P) r.kv[p] = (float)(int)(rnd() * SH_COUNT);
					else if (p == PATTERN_P) r.kv[p] = (float)(int)(rnd() * PT_COUNT);
					else if (p == NOTES_P) r.kv[p] = (float)(1 + (int)(rnd() * 32));
					else if (p == NEW_P || p == NOTE_P || p == PHRASE_P) r.kv[p] = rnd() < 0.3 ? 1.f : 0.f;
					else r.kv[p] = (float)rnd();
				}
				for (int i = 0; i < NUM_I; i++) r.in[i] = (float)(rnd() * 20 - 10);
				if (rnd() < 0.2) r.trig(rnd() < 0.5 ? PHRASE_I : NOTE_I);
			}
			// SIZE swept end to end at audio rate
			r.in[SIZE_I] = (float)(6.0 * std::sin(2 * M_PI * 220.0 * n / sr));
			r.in[CLOCK_I] = (n % 3000) < 30 ? 10.f : 0.f;
			r.core.process(r.kv, r.in, r.conn);
			for (int o = 0; o < NUM_O; o++) if (!std::isfinite(r.core.out[o])) bad = true;
			peak = std::max(peak, (double)std::fabs(r.core.out[OUT_O]));
			for (int i : {NOTE_I, PHRASE_I, NEW_I}) r.in[i] = 0.f;
		}
		check(!bad && peak <= 8.0, "60 s of fuzzing: finite, under 8 V", fmt("(%d Hz, peak %.2f V)", sr, peak));
	}
	{
		// denormals: a long silence must not cost more than sound
		Rig r(48000, bd_default(), 1);
		r.trig(NOTE_I); r.run(1.0);
		auto t0 = std::chrono::steady_clock::now(); r.run(20.0);
		double quiet = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		r.kv[HOLD_P] = 1.f; r.run(0.5);
		t0 = std::chrono::steady_clock::now(); r.run(20.0);
		double loud = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		check(quiet < 1.3 * loud, "no denormal slowdown in silence", fmt("(silence %.2f s, sound %.2f s for 20 s; %.1f%% of a core)", quiet, loud, 100 * loud / 20.0));
	}

	// 10. demos
	printf("10. demo WAVs into %s\n", dir.c_str());
	{
		std::vector<float> gallery;
		for (int k = 0; k < NP; k++) {
			double secs = pre[k].p.hold ? 16.0 : (pre[k].p.time > 1.5 ? 20.0 : 12.0);
			auto x = presetDemo(pre[k], 48000, secs);
			writeWav(dir + fmt("/%02d-%s.wav", k + 1, slug(pre[k].name).c_str()), x, 48000);
			size_t keep = std::min(x.size(), (size_t)(8.0 * 48000));
			for (size_t i = 0; i < keep; i++) {
				double fade = std::min(1.0, std::min(i / 2400.0, (keep - i) / 9600.0));
				gallery.push_back((float)(x[i] * fade));
			}
			gallery.insert(gallery.end(), 24000, 0.f);
		}
		writeWav(dir + "/00-gallery.wav", gallery, 48000);

		auto findP = [&](const char* name) { int k = 0; while (k < NP && strcmp(pre[k].name, name)) k++; return k; };

		{	// VARY swept 0 to 1 over eight repeats of one phrase
			int k = findP("Songster"); Rig r(48000, pre[k].p, pre[k].seed);
			for (int i = 0; i < 8; i++) {
				r.kv[VARY_P] = i / 7.f; r.trig(PHRASE_I); r.run(0.1);
				while (!r.idle()) r.step();
				r.run(0.4);
			}
			writeWav(dir + "/demo-vary-sweep.wav", r.out, 48000);
		}
		{	// one seed, bird size stepped 0.5 to 32
			int k = findP("Songster"); Rig r(48000, pre[k].p, pre[k].seed);
			for (double s : {0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0}) {
				r.kv[SIZE_P] = (float)unex(s, 0.5, 128.0); r.run(0.05); r.trig(PHRASE_I); r.run(0.1);
				while (!r.idle()) r.step();
				r.run(0.4);
			}
			writeWav(dir + "/demo-size-steps.wav", r.out, 48000);
		}
		{	// tension swept top to bottom under HOLD at size 4
			BdParams p = bd_default(); p.size = 4; p.pressure = 0.35; p.throat = 0.4; Rig r(48000, p, 1);
			r.kv[HOLD_P] = 1.f; int n = 0, N = 12 * 48000;
			r.run(12.0, [&]() { r.kv[TENSION_P] = 1.f - (float)n++ / N; });
			r.kv[HOLD_P] = 0.f; r.run(0.5);
			writeWav(dir + "/demo-tension-sweep.wav", r.out, 48000);
		}
		{	// Bass Bird locked to a 124 bpm quarter-note clock, eight bars, a new seed every two bars
			int k = findP("Bass Bird"); Rig r(48000, pre[k].p, pre[k].seed);
			double beat = 60.0 / 124.0; int n = 0, bar = 0;
			r.conn[CLOCK_I] = true;
			r.run(beat * 4 * 9, [&]() {
				double t = (double)n / 48000.0;
				r.in[CLOCK_I] = std::fmod(t, beat) < 0.005 ? 10.f : 0.f;
				if (n > 0 && n % (int)std::lround(beat * 4 * 48000) == 0 && bar < 8) {
					if (bar % 2 == 0) r.trig(NEW_I);
					r.trig(PHRASE_I); bar++;
				}
				n++;
			});
			writeWav(dir + "/demo-bass-bird-124.wav", r.out, 48000);
		}
		{	// TIME swept 1 to 16 on Warbler
			int k = findP("Warbler"); Rig r(48000, pre[k].p, pre[k].seed);
			for (int i = 0; i < 6; i++) {
				r.kv[TIME_P] = i / 5.f; r.trig(PHRASE_I); r.run(0.1);
				while (!r.idle()) r.step();
				r.run(0.3);
			}
			writeWav(dir + "/demo-time-stretch.wav", r.out, 48000);
		}
		{	// slow LFO on the second voice's interval under HOLD
			BdParams p = bd_default(); p.size = 3; p.tension = -6; p.v2mix = 0.6; p.v2int = 0; p.pressure = 0.3; p.throat = 0.4;
			Rig r(48000, p, 1); r.kv[HOLD_P] = 1.f; int n = 0;
			r.run(12.0, [&]() { r.kv[V2INT_P] = (float)(0.5 + 0.12 * std::sin(2 * M_PI * 0.15 * n++ / 48000.0)); });
			r.kv[HOLD_P] = 0.f; r.run(0.5);
			writeWav(dir + "/demo-two-voice-lfo.wav", r.out, 48000);
		}
		check(true, "demos written");
	}

	printf("\n%d passed, %d failed\n", passes, failures);
	return failures ? 1 : 0;
}
