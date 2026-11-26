// Offline checks for Comper (no Rack needed):  make test
// 1 golden engine, 2 chord recognition, 3 host against golden, 4 host behaviour,
// 5 musical checks on the C++ engine, 6 listening through the host, 7 CPU,
// and a MIDI file per preset rendered by the host.
#include "simulate.hpp"
#include "../src/dsp/Host.hpp"
#include "golden.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <cstdarg>
using namespace comper;

static int failures = 0;
static void check(const char* name, bool ok, const std::string& detail = "") {
	printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.empty() ? "" : "  ", detail.c_str());
	if (!ok) failures++;
}
static std::string fmt(const char* f, ...) { char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }

static Params toParams(const double* v) {
	Params p; p.style = (int)v[0]; p.spread = v[1]; p.reg = v[2]; p.voices = (int)v[3]; p.lead = v[4]; p.out = v[5]; p.roots = (int)v[6];
	p.groove = (int)v[7]; p.density = v[8]; p.swing = v[9]; p.repeat = v[10]; p.push = v[11]; p.loose = v[12]; p.timing = v[13];
	p.roll = v[14]; p.weight = v[15]; p.slop = v[16]; p.pedal = v[17]; p.listen = v[18]; return p;
}
static SimChart chartOf(const GCase& c) {
	SimChart ch; ch.total = c.total;
	for (int k = 0; k < c.nchart; k++) ch.chart.push_back({c.chart[k].beat, Chord(c.chart[k].root, c.chart[k].q)});
	return ch;
}
static std::vector<SimSolo> soloOf(const GCase& c) {
	std::vector<SimSolo> s; for (int k = 0; k < c.nsolo; k++) s.push_back({c.solo[k].on, c.solo[k].off, c.solo[k].pitch}); return s;
}
static const GCase* findCase(const char* name) {
	for (int i = 0; i < NUM_GOLDEN; i++) if (!strcmp(GOLDEN[i].name, name)) return &GOLDEN[i];
	return nullptr;
}

// ---------------------------------------------------------------- host runner
struct HNote { double on, off; int pitch, vel; int ch; };
struct HostRun {
	std::vector<HNote> notes; int steals = 0, retrig = 0; double maxSimul = 0; bool overlapBad = false; bool shortLow = false;
	std::vector<std::pair<double, float>> pedal; double seconds = 0, cpu = 0;
};
struct RunOpts {
	double sr = 48000; bool extClock = false; int ppb = 1; double rampTo = 0; int channels = 12;
	bool feedSolo = false; double soloListen = -1; bool stopAt = false; double stopTime = 0;
};
static bool hostChartFromCase(const GCase& c, HostIn& in) {
	int n = c.nchart; if (n > 16) return false;
	double bps = c.total / n;
	if (bps != 1 && bps != 2 && bps != 4 && bps != 8) return false;
	for (int k = 0; k < n; k++) if (std::fabs(c.chart[k].beat - k * bps) > 1e-9) return false;
	in.steps = n; in.beatsPerStep = (int)bps;
	for (int k = 0; k < n; k++) { in.stepRoot[k] = c.chart[k].root; in.stepQual[k] = c.chart[k].q; }
	return true;
}
// golden time 0 = host beat 0; the host starts half a beat early so nothing is clamped
static HostRun runHost(const GCase& c, const RunOpts& o) {
	HostRun R;
	HostIn in; HostOut out; Host h;
	hostChartFromCase(c, in);
	in.p = toParams(c.prm); in.seed = c.seed; in.tempo = c.bpm; in.outChannels = o.channels;
	if (o.soloListen >= 0) in.p.listen = o.soloListen;
	double spb = 60.0 / c.bpm, dt = 1.0 / o.sr;
	double pre = 0.5 * spb;
	double len = c.nbars * 4 * spb + 4.0;
	int N = (int)(len * o.sr);
	auto sol = soloOf(c);
	in.soloConnected = o.feedSolo && !sol.empty(); in.soloPitchConnected = in.soloConnected;
	int open[MAX_CH]; double onT[MAX_CH]; int vel[MAX_CH]; float vo[MAX_CH]; double lastFall[MAX_CH];
	for (int k = 0; k < MAX_CH; k++) { open[k] = 0; lastFall[k] = -1; }
	// external clock: pulses at the golden beat grid
	in.clockConnected = o.extClock; in.ppb = o.ppb;
	double clkPhase = 0; int pulseCount = 0;
	auto t0 = std::chrono::steady_clock::now();
	float lastPed = -1;
	for (int i = 0; i < N; i++) {
		double ht = i * dt;
		double gt = ht - pre;               // golden time
		if (o.extClock) {
			double bpm = c.bpm;
			if (o.rampTo > 0) bpm = c.bpm + (o.rampTo - c.bpm) * std::min(1.0, std::max(0.0, ht / (len - 4.0)));
			// pulse train: 1 ms pulses, ppb per beat; first pulse at golden time 0
			double per = 60.0 / bpm / o.ppb;
			if (gt >= 0) {
				clkPhase += dt / per;
				if (pulseCount == 0) { clkPhase = 0; pulseCount = 1; }
			}
			double frac = clkPhase - std::floor(clkPhase);
			in.clock = (gt >= 0 && frac * per < 0.001) ? 10.f : 0.f;
		}
		if (in.soloConnected) {
			bool on = false; double pv = 0;
			for (auto& s : sol) if (s.on <= gt && gt < s.off) { on = true; pv = (s.pitch - 60) / 12.0; }
			in.solo = on ? 10.f : 0.f; in.soloPitch = (float)pv;
		}
		if (o.stopAt) in.runButton = gt < o.stopTime;
		h.process(in, out, dt, o.extClock ? 0.0 : -0.5);
		int sim = 0;
		for (int k = 0; k < MAX_CH; k++) {
			bool g = out.gate[k] > 5.f;
			if (g) sim++;
			if (g && !open[k]) {
				if (lastFall[k] >= 0 && gt - lastFall[k] < 0.001 - 1e-9) R.shortLow = true;
				if (k >= out.channels) R.overlapBad = true;
				open[k] = 1; onT[k] = gt; vel[k] = (int)std::lround(out.vel[k] / 10.0 * 127.0); vo[k] = out.voct[k];
			} else if (!g && open[k]) {
				open[k] = 0; lastFall[k] = gt;
				R.notes.push_back({onT[k], gt, (int)std::lround(vo[k] * 12.0) + 60, vel[k], k});
			} else if (g && out.voct[k] != vo[k]) R.overlapBad = true;   // pitch changed under a held gate
		}
		R.maxSimul = std::max(R.maxSimul, (double)sim);
		if (out.pedal != lastPed) { R.pedal.push_back({gt, out.pedal}); lastPed = out.pedal; }
	}
	for (int k = 0; k < MAX_CH; k++) if (open[k]) R.notes.push_back({onT[k], 1e9, (int)std::lround(vo[k] * 12.0) + 60, vel[k], k});
	R.cpu = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	R.seconds = len;
	R.steals = h.stats_steals; R.retrig = h.stats_retrig;
	std::sort(R.notes.begin(), R.notes.end(), [](const HNote& a, const HNote& b) { return a.on < b.on || (a.on == b.on && a.pitch < b.pitch); });
	return R;
}

// match host notes to golden notes by pitch and on time
struct Match { int matched = 0, missing = 0, extra = 0, velBad = 0, delayed = 0, offBad = 0; double worstOn = 0; };
static Match compare(const GCase& c, const HostRun& R, double tolOn, bool checkOff) {
	Match m;
	const double endT = c.nbars * 4 * 60.0 / c.bpm;        // the golden take stops here; the host plays on
	std::vector<bool> used(R.notes.size(), false);
	for (int k = 0; k < c.nnotes; k++) {
		const GNote& g = c.notes[k];
		int best = -1; double bd = 1e9;
		for (size_t j = 0; j < R.notes.size(); j++) {
			if (used[j] || R.notes[j].pitch != g.pitch) continue;
			double d = R.notes[j].on - g.on;
			if (d > -tolOn && d < tolOn + 0.0021 && std::fabs(d) < bd) { bd = std::fabs(d); best = (int)j; }
		}
		if (best < 0) { m.missing++; continue; }
		used[best] = true; m.matched++;
		double d = R.notes[best].on - g.on;
		if (d > tolOn) m.delayed++;
		else m.worstOn = std::max(m.worstOn, std::fabs(d));
		if (std::abs(R.notes[best].vel - g.vel) > 0) m.velBad++;
		if (checkOff) {
			// a later note of the same pitch that starts before this one ends cuts it short (retrigger)
			bool cut = false;
			for (int q = 0; q < c.nnotes; q++) if (q != k && c.notes[q].pitch == g.pitch && c.notes[q].on > g.on - 1e-9 && c.notes[q].on < g.off + 0.0021) cut = true;
			bool finalRelease = g.off > endT - 0.12 * 60.0 / c.bpm - 1e-9;   // ended by the take stopping
			if (!cut && !finalRelease && R.notes[best].off < 1e8 && std::fabs(R.notes[best].off - g.off) > tolOn + 1e-9) m.offBad++;
		}
	}
	for (size_t j = 0; j < R.notes.size(); j++) if (!used[j] && R.notes[j].on < endT - 0.12 * 60.0 / c.bpm - 0.03) m.extra++;
	return m;
}

// ---------------------------------------------------------------- MIDI
static void vlq(std::string& s, unsigned n) {
	unsigned char b[5]; int k = 0; b[k++] = n & 0x7f; n >>= 7;
	while (n) { b[k++] = (n & 0x7f) | 0x80; n >>= 7; }
	while (k) s.push_back((char)b[--k]);
}
static void writeMidi(const std::string& path, double bpm, const std::vector<HNote>& notes, const std::vector<std::pair<double, float>>& pedal,
                      const std::vector<SimSolo>& solo) {
	const int ppq = 480; double tps = ppq * bpm / 60.0;
	struct E { long t; int pri; std::string m; };
	auto track = [&](std::vector<E> ev) {
		std::sort(ev.begin(), ev.end(), [](const E& a, const E& b) { return a.t < b.t || (a.t == b.t && a.pri < b.pri); });
		std::string d; long last = 0;
		for (auto& e : ev) { vlq(d, (unsigned)(e.t - last)); d += e.m; last = e.t; }
		vlq(d, 960); d += std::string("\xff\x2f\x00", 3);
		return d;
	};
	std::vector<std::string> tr;
	std::string t0; t0 += std::string("\x00\xff\x51\x03", 4); unsigned us = (unsigned)(60e6 / bpm);
	t0.push_back((char)(us >> 16)); t0.push_back((char)(us >> 8)); t0.push_back((char)us); t0 += std::string("\x00\xff\x2f\x00", 4);
	tr.push_back(t0);
	std::vector<E> ev; ev.push_back({0, 0, std::string("\xc0\x00", 2)});
	for (auto& n : notes) {
		if (n.off > 1e8) continue;
		long a = std::max(0L, (long)(n.on * tps)), b = std::max(a + 1, (long)(n.off * tps));
		ev.push_back({a, 2, std::string{(char)0x90, (char)n.pitch, (char)n.vel}});
		ev.push_back({b, 1, std::string{(char)0x80, (char)n.pitch, 0}});
	}
	for (auto& p : pedal) {
		long a = std::max(0L, (long)(p.first * tps)); int v = (int)std::lround(p.second / 10.0 * 127.0);
		if (v != 0 && v != 127 && p.second != 0.f) { /* keep only settled values */ }
		ev.push_back({a, 0, std::string{(char)0xb0, 64, (char)std::min(127, std::max(0, v))}});
	}
	tr.push_back(track(ev));
	if (!solo.empty()) {
		std::vector<E> s; s.push_back({0, 0, std::string("\xc1\x42", 2)});
		for (auto& n : solo) {
			long a = (long)(n.on * tps), b = std::max(a + 1, (long)(n.off * tps));
			s.push_back({a, 2, std::string{(char)0x91, (char)n.pitch, 90}});
			s.push_back({b, 1, std::string{(char)0x81, (char)n.pitch, 0}});
		}
		tr.push_back(track(s));
	}
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	auto be32 = [&](unsigned v) { fputc(v >> 24, f); fputc(v >> 16, f); fputc(v >> 8, f); fputc(v, f); };
	auto be16 = [&](unsigned v) { fputc(v >> 8, f); fputc(v, f); };
	fwrite("MThd", 1, 4, f); be32(6); be16(1); be16((unsigned)tr.size()); be16(ppq);
	for (auto& d : tr) { fwrite("MTrk", 1, 4, f); be32((unsigned)d.size()); fwrite(d.data(), 1, d.size(), f); }
	fclose(f);
}

// chords (groups of notes starting within 90 ms)
static std::vector<std::vector<SimNote>> groups(std::vector<SimNote> n) {
	std::sort(n.begin(), n.end(), [](const SimNote& a, const SimNote& b) { return a.on < b.on; });
	std::vector<std::vector<SimNote>> g;
	for (auto& x : n) { if (!g.empty() && x.on - g.back()[0].on < 0.09) g.back().push_back(x); else g.push_back({x}); }
	return g;
}

int main(int argc, char** argv) {
	std::string outDir = argc > 1 ? argv[1] : "test/out";

	// 1. golden, engine level
	{
		int bad = 0, total = 0; std::string first;
		for (int i = 0; i < NUM_GOLDEN; i++) {
			const GCase& c = GOLDEN[i];
			std::vector<SimNote> n; std::vector<std::pair<double, double>> pd;
			simulate(chartOf(c), c.bpm, c.nbars, c.seed, toParams(c.prm), soloOf(c), c.external, n, pd);
			bool ok = (int)n.size() == c.nnotes && (int)pd.size() == c.npedal;
			for (int k = 0; ok && k < c.nnotes; k++)
				ok = n[k].pitch == c.notes[k].pitch && n[k].vel == c.notes[k].vel && std::fabs(n[k].on - c.notes[k].on) < 1e-6 && std::fabs(n[k].off - c.notes[k].off) < 1e-6;
			for (int k = 0; ok && k < c.npedal; k++) ok = std::fabs(pd[k].first - c.pedal[k].t) < 1e-6 && pd[k].second == c.pedal[k].v;
			if (!ok) { bad++; if (first.empty()) first = c.name; }
			total += c.nnotes;
		}
		check("1 golden: C++ engine reproduces every reference case", bad == 0, fmt("%d cases, %d notes, %d differ %s", NUM_GOLDEN, total, bad, first.c_str()));
	}
	// 2. chord recognition
	{
		int bad = 0;
		for (int i = 0; i < NUM_DETECT; i++) {
			std::vector<int> ns(DETECT[i].notes, DETECT[i].notes + DETECT[i].n); Chord o;
			detectChord(ns, DETECT[i].lastq, o);
			if (o.root != DETECT[i].root || o.q != DETECT[i].q) bad++;
		}
		check("2 chord recognition table", bad == 0, fmt("%d cases, %d differ", NUM_DETECT, bad));
	}
	// 3. host against golden
	{
		int cases = 0, bad = 0, notes = 0, delayed = 0, steals = 0; double worst = 0; std::string first;
		for (int i = 0; i < NUM_GOLDEN; i++) {
			const GCase& c = GOLDEN[i];
			HostIn tmp;
			if (c.nsolo || c.external || !hostChartFromCase(c, tmp)) continue;
			RunOpts o; HostRun R = runHost(c, o);
			Match m = compare(c, R, 1.5 / o.sr, true);
			cases++; notes += c.nnotes; delayed += m.delayed; steals += R.steals; worst = std::max(worst, m.worstOn);
			bool ok = m.missing == 0 && m.extra == 0 && m.velBad == 0 && m.offBad == 0 && !R.overlapBad && !R.shortLow;
			if (!ok) { bad++; if (first.empty()) first = fmt("%s (missing %d extra %d vel %d off %d overlap %d short %d)", c.name, m.missing, m.extra, m.velBad, m.offBad, (int)R.overlapBad, (int)R.shortLow); }
		}
		check("3 host at 48 kHz plays the golden notes", bad == 0,
		      fmt("%d cases, %d notes, worst on-time error %.3f samples, %d notes moved 2 ms by a retrigger, %d steals; %s", cases, notes, worst * 48000, delayed, steals, first.c_str()));
	}
	// 4. host behaviour
	{
		const GCase* c = findCase("preset 01_Medium Swing");
		RunOpts o; HostRun a = runHost(*c, o);
		RunOpts e; e.extClock = true; e.ppb = 4; HostRun b = runHost(*c, e);
		// external clock: same notes within 2 ms (the first decision is late by design: nothing before the first pulse)
		Match m = compare(*c, b, 0.002, false);
		check("4a external clock (4 ppb) matches the internal clock", m.missing <= 3 && m.extra <= 3, fmt("matched %d missing %d extra %d", m.matched, m.missing, m.extra));
		RunOpts r; r.extClock = true; r.rampTo = 160; HostRun rr = runHost(*c, r);
		bool stuck = false; for (auto& n : rr.notes) if (n.off > 1e8) stuck = true;
		check("4b tempo ramp 132 to 160 bpm: no stuck notes", !stuck && !rr.overlapBad, fmt("%zu notes", rr.notes.size()));
		RunOpts s; s.stopAt = true; s.stopTime = 8.0; HostRun st = runHost(*c, s);
		bool late = false; for (auto& n : st.notes) if (n.off > 8.0 + 1.0 / 48000 + 1e-9) late = true;
		check("4c RUN off drops every gate within one sample", !late);
		HostRun a2 = runHost(*c, o);
		bool same = a.notes.size() == a2.notes.size();
		for (size_t i = 0; same && i < a.notes.size(); i++) same = a.notes[i].on == a2.notes[i].on && a.notes[i].pitch == a2.notes[i].pitch;
		check("4d reset replays the same take", same);
		bool anyBad = false, anyShort = false; int steals = 0;
		for (int i = 0; i < NUM_GOLDEN; i++) {
			const GCase& g = GOLDEN[i]; HostIn tmp; if (!hostChartFromCase(g, tmp) || g.external) continue;
			RunOpts q; q.channels = 8; q.feedSolo = g.nsolo > 0; HostRun x = runHost(g, q);
			anyBad |= x.overlapBad; anyShort |= x.shortLow; steals += x.steals;
		}
		check("4e 8 channels: one note per channel, gates low at least 1 ms between notes", !anyBad && !anyShort, fmt("%d stolen notes over all cases", steals));
	}
	// 5. musical checks on the C++ engine
	{
		const GCase* c = findCase("grid groove0 style1");
		SimChart ch = chartOf(*c);
		auto vl = [&](double lead) {
			double mv = 0; int n = 0;
			for (int seed = 1; seed <= 20; seed++) {
				Params p; p.lead = lead; p.spread = 0.0;
				std::vector<SimNote> ns; std::vector<std::pair<double, double>> pd;
				simulate(ch, 132, 16, seed, p, {}, false, ns, pd);
				auto g = groups(ns);
				for (size_t i = 1; i < g.size(); i++) {
					Notes a, b; for (auto& x : g[i - 1]) a.push_back(x.pitch); for (auto& x : g[i]) b.push_back(x.pitch);
					std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
					if (a != b && a.size() == b.size()) { mv += moveCost(b, &a) / b.size(); n++; }
				}
			}
			return mv / n;
		};
		double on = vl(1.0), off = vl(0.0);
		check("5a voice leading on under 1.6, off over 2.5 semitones per voice", on < 1.6 && off > 2.5, fmt("on %.2f off %.2f", on, off));
		int cnt[3] = {0, 0, 0}; double ds[3] = {0.0, 0.5, 1.0};
		for (int k = 0; k < 3; k++) for (int seed = 1; seed <= 20; seed++) {
			Params p; p.density = ds[k]; std::vector<SimNote> ns; std::vector<std::pair<double, double>> pd;
			simulate(ch, 132, 16, seed, p, {}, false, ns, pd); cnt[k] += (int)groups(ns).size();
		}
		check("5b DENSITY raises the number of chords", cnt[0] < cnt[1] && cnt[1] < cnt[2], fmt("%d %d %d per 20 takes", cnt[0], cnt[1], cnt[2]));
		Params p0; p0.loose = 0;
		std::vector<SimNote> ns; std::vector<std::pair<double, double>> pd;
		simulate(ch, 132, 16, 3, p0, {}, false, ns, pd);
		bool oneVel = true, noRoll = true;
		for (auto& x : ns) if (x.vel != ns[0].vel) oneVel = false;
		for (auto& g : groups(ns)) if (g.back().on - g.front().on > 1e-9) noRoll = false;
		check("5c LOOSE 0 is a machine: one velocity, no roll", oneVel && noRoll);
	}
	// 6. listening through the host
	{
		const GCase* c = findCase("listen 1.0");
		auto sol = soloOf(*c);
		auto onTop = [&](const HostRun& R, int& hits) {
			std::vector<SimNote> sn; for (auto& n : R.notes) sn.push_back({n.on, n.off, n.pitch, n.vel});
			auto g = groups(sn); int top = 0; hits = (int)g.size();
			for (auto& x : g) { double t = x[0].on; for (auto& s : sol) if (s.on <= t && t < s.off) { top++; break; } }
			return (double)top / std::max(1, hits);
		};
		RunOpts a; a.feedSolo = true; a.soloListen = 0.0; HostRun r0 = runHost(*c, a);
		RunOpts b; b.feedSolo = true; b.soloListen = 1.0; HostRun r1 = runHost(*c, b);
		int h0, h1; double o0 = onTop(r0, h0), o1 = onTop(r1, h1);
		check("6 listening through the host: chords on top of the soloist fall by a third or more", o1 < o0 * 0.667,
		      fmt("LISTEN 0: %d chords, %.0f%% on the soloist; LISTEN 1: %d chords, %.0f%%", h0, o0 * 100, h1, o1 * 100));
	}
	// 7. CPU
	{
		const GCase* c = findCase("sweep density=1.0");
		RunOpts o; HostRun R = runHost(*c, o);
		double pct = R.cpu / R.seconds * 100.0;
		check("7 CPU far under 1% of a core at 48 kHz", pct < 0.5, fmt("%.3f%% of a core (busy blues, includes the test's own gate reading)", pct));
	}
	// MIDI per preset, rendered by the host
	{
		int n = 0;
		for (int i = 0; i < NUM_GOLDEN; i++) {
			const GCase& c = GOLDEN[i];
			if (strncmp(c.name, "preset ", 7)) continue;
			RunOpts o; o.feedSolo = c.nsolo > 0; HostRun R = runHost(c, o);
			writeMidi(outDir + "/" + std::string(c.name + 7) + ".mid", c.bpm, R.notes, R.pedal, soloOf(c));
			n++;
		}
		// demonstrations
		auto demo = [&](const char* name, const char* file, bool solo, double listen) {
			const GCase* c = findCase(name); if (!c) { printf("no case %s\n", name); return; }
			RunOpts o; o.feedSolo = solo; o.soloListen = listen; HostRun R = runHost(*c, o);
			writeMidi(outDir + "/" + file, c->bpm, R.notes, R.pedal, solo ? soloOf(*c) : std::vector<SimSolo>());
			n++;
		};
		demo("sweep loose=0.0", "demo-loose-0.mid", false, -1);
		demo("sweep loose=2.0", "demo-loose-2.mid", false, -1);
		demo("sweep lead=0.0", "demo-lead-0.mid", false, -1);
		demo("sweep out=1.0", "demo-out-1.mid", false, -1);
		demo("listen 1.0", "demo-listen-0.mid", true, 0.0);
		demo("listen 1.0", "demo-listen-1.mid", true, 1.0);
		printf("wrote %d MIDI files to %s\n", n, outDir.c_str());
	}
	printf(failures ? "%d FAILED\n" : "ALL PASS\n", failures);
	return failures ? 1 : 0;
}
