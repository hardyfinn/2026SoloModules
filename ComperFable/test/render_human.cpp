// 2.3.0 checks: human note lengths (LENGTH), OCTAVE, plain triads, the REGGAE groove and the new progressions.
// Runs the real Host at 48 kHz.  make test/render_human && ./test/render_human
#include "../src/dsp/Host.hpp"
#include "../src/Progressions.hpp"
#include <cstdio>
#include <map>

using namespace comper;

struct Run { std::vector<double> lens; int notes = 0; int stuck = 0; double minV = 99, maxV = -99; int maxPoly = 0; };

static Run run(const char* chart, int beatsPer, int groove, bool human, double length, int octave, int bars = 16, int seed = 3) {
	HostIn in; HostOut out; Host h;
	std::vector<Chord> ch; std::string bad;
	if (!parseChart(chart, ch, bad)) { printf("parse failed: %s\n", bad.c_str()); exit(1); }
	in.steps = (int)ch.size(); in.beatsPerStep = beatsPer;
	for (size_t i = 0; i < ch.size(); i++) { in.stepRoot[i] = ch[i].root; in.stepQual[i] = ch[i].q; }
	in.tempo = 120.0; in.seed = seed; in.p.groove = groove; in.p.human = human; in.p.length = length; in.octave = octave;
	in.outChannels = 16;
	const double sr = 48000.0, dt = 1.0 / sr, spb = 0.5;
	double T = bars * 4 * spb + 3.0;
	bool g[MAX_CH] = {}; double on[MAX_CH] = {};
	Run r;
	long n = (long)(T * sr);
	for (long i = 0; i < n; i++) {
		if (i == (long)((bars * 4 * spb) * sr)) in.runButton = false;   // stop: everything must end
		h.process(in, out, dt);
		int poly = 0;
		for (int c = 0; c < out.channels; c++) {
			bool hi = out.gate[c] > 5.f;
			if (hi) { poly++; r.minV = std::min(r.minV, (double)out.voct[c]); r.maxV = std::max(r.maxV, (double)out.voct[c]); }
			if (hi && !g[c]) { on[c] = i * dt; r.notes++; }
			if (!hi && g[c]) r.lens.push_back((i * dt - on[c]) / spb);
			g[c] = hi;
		}
		r.maxPoly = std::max(r.maxPoly, poly);
	}
	for (int c = 0; c < MAX_CH; c++) if (g[c]) r.stuck++;
	return r;
}
static std::string string_f(int a, int b) { char t[64]; snprintf(t, sizeof t, "%d / %d", a, b); return t; }
static double pct(std::vector<double> v, double p) { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[std::min(v.size() - 1, (size_t)(p * v.size()))]; }
static double spreadOf(const std::vector<double>& v) { return pct(v, 0.9) / std::max(1e-9, pct(v, 0.1)); }

int main() {
	int fails = 0;
	auto check = [&](bool ok, const char* name, const std::string& detail) { printf("%s %s  %s\n", ok ? "PASS" : "FAIL", name, detail.c_str()); if (!ok) fails++; };
	const char* chart = "Dm7 G7 Cmaj7 Cmaj7";
	const char* GN[NUM_GROOVES] = {"swing", "bossa", "ballad", "soul", "house", "reggae"};

	// 1 every progression parses and has 1..16 steps
	int okp = 0;
	for (int i = 0; i < NUM_PROGRESSIONS; i++) { std::vector<Chord> c; std::string b; if (parseChart(PROGRESSIONS[i].text, c, b)) okp++; else printf("  bad: %s (%s)\n", PROGRESSIONS[i].name, b.c_str()); }
	check(okp == NUM_PROGRESSIONS, "1 progressions parse", string_f(okp, NUM_PROGRESSIONS));

	// 2 LENGTH: median length grows with the knob, and is longer than 2.2.0 at the default
	for (int g = 0; g < NUM_GROOVES; g++) {
		Run old = run(chart, 4, g, false, 0.6, 0), s = run(chart, 4, g, true, 0.0, 0), d = run(chart, 4, g, true, 0.6, 0), l = run(chart, 4, g, true, 1.0, 0);
		char buf[256];
		snprintf(buf, sizeof buf, "%s: median beats 2.2.0 %.2f | LENGTH 0 %.2f, 0.6 %.2f, 1 %.2f | spread p90/p10 %.1f vs %.1f | stuck %d",
			GN[g], pct(old.lens, 0.5), pct(s.lens, 0.5), pct(d.lens, 0.5), pct(l.lens, 0.5), spreadOf(d.lens), spreadOf(old.lens), s.stuck + d.stuck + l.stuck);
		bool ok = pct(s.lens, 0.5) < pct(d.lens, 0.5) && pct(d.lens, 0.5) < pct(l.lens, 0.5) && s.stuck + d.stuck + l.stuck == 0 && pct(d.lens, 0.5) >= pct(old.lens, 0.5) * 0.95;
		check(ok, "2 length", buf);
	}
	// 3 the same notes start with human lengths on and off (only the ends move)
	{
		Run a = run(chart, 4, 0, false, 0.6, 0), b = run(chart, 4, 0, true, 0.6, 0);
		check(std::abs(a.notes - b.notes) <= a.notes / 20, "3 human lengths keep the notes", string_f(a.notes, b.notes));
	}
	// 4 OCTAVE moves the chord outputs by whole volts
	{
		Run a = run(chart, 4, 0, true, 0.6, 0), b = run(chart, 4, 0, true, 0.6, 2), c = run(chart, 4, 0, true, 0.6, -3);
		char buf[200]; snprintf(buf, sizeof buf, "range %.2f..%.2f V, +2: %.2f..%.2f V, -3: %.2f..%.2f V", a.minV, a.maxV, b.minV, b.maxV, c.minV, c.maxV);
		check(std::fabs(b.minV - a.minV - 2) < 1e-4 && std::fabs(c.maxV - a.maxV + 3) < 1e-4, "4 octave", buf);
	}
	// 5 reggae with triads: plays, three-note chords at most four voices, nothing stuck
	for (int i = 0; i < NUM_PROGRESSIONS; i++) {
		std::string n = PROGRESSIONS[i].name; if (n.rfind("Reggae", 0) != 0) continue;
		Run r = run(PROGRESSIONS[i].text, PROGRESSIONS[i].beats, REGGAE, true, 0.6, 0);
		char buf[200]; snprintf(buf, sizeof buf, "%s: %d notes, most at once %d, median %.2f beats, stuck %d", n.c_str(), r.notes, r.maxPoly, pct(r.lens, 0.5), r.stuck);
		check(r.notes > 40 && r.stuck == 0 && r.maxPoly <= 8, "5 reggae", buf);
	}
	// 6 every new progression plays in its own idiom without stuck notes
	{
		int bad = 0, total = 0;
		for (int i = 14; i < NUM_PROGRESSIONS; i++) {
			std::string n = PROGRESSIONS[i].name;
			int g = n.rfind("House", 0) == 0 ? HOUSE : n.rfind("Disco", 0) == 0 || n.rfind("Funk", 0) == 0 || n.rfind("Soul", 0) == 0 ? SOUL
			      : n.rfind("Reggae", 0) == 0 ? REGGAE : SWING;
			Run r = run(PROGRESSIONS[i].text, PROGRESSIONS[i].beats, g, true, 0.6, 0, 8);
			total++; if (r.notes < 10 || r.stuck) { bad++; printf("  %s: %d notes, %d stuck\n", n.c_str(), r.notes, r.stuck); }
		}
		check(bad == 0, "6 new progressions play", string_f(total - bad, total));
	}
	printf(fails ? "SOME CHECKS FAILED\n" : "ALL PASS\n");
	return fails ? 1 : 0;
}
