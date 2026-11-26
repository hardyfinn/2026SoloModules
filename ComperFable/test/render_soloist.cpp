// Soloist offline checks (no Rack): the line engine and host over every built-in progression, against a stand-in
// for Comper's link. Notes only (no horn audio) unless a WAV is asked for.
//   c++ -O2 -std=c++17 test/render_soloist.cpp -o test/render_soloist && ./test/render_soloist
#include "../src/dsp/SoloHost.hpp"
#include "../src/Progressions.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>

using namespace solo;

static int grooveFor(const char* name, int genre) {
	std::string n = name;
	if (n.find("Bossa") != std::string::npos) return G_BOSSA;
	if (n.find("allad") != std::string::npos) return G_BALLAD;
	switch (genre) {
		case GEN_HOUSE: case GEN_DISCO: return G_HOUSE;
		case GEN_SOUL: case GEN_FUNK: return G_SOUL;
		case GEN_REGGAE: return G_REGGAE;
		default: return G_SWING;
	}
}
static double bpmFor(int groove) {
	switch (groove) { case G_BALLAD: return 66; case G_HOUSE: return 122; case G_SOUL: return 98; case G_REGGAE: return 76; case G_BOSSA: return 128; default: return 150; }
}

struct Chart { int steps = 1, bps = 4; int root[16] = {}, q[16] = {}; int genre = 0, groove = 0; double bpm = 120; };
static bool loadChart(int pi, Chart& c, int grooveOverride = -1) {
	const comper::Progression& P = comper::PROGRESSIONS[pi];
	std::vector<comper::Chord> ch; std::string bad;
	if (!comper::parseChart(P.text, ch, bad) || ch.empty()) return false;
	c.steps = std::min(16, (int)ch.size()); c.bps = P.beats;
	for (int i = 0; i < c.steps; i++) { c.root[i] = ch[i].root; c.q[i] = ch[i].q; }
	c.genre = genreOf(P.name); c.groove = grooveOverride >= 0 ? grooveOverride : grooveFor(P.name, c.genre); c.bpm = bpmFor(c.groove);
	return true;
}
static void fillComp(CompLink& L, const Chart& c, double phi, bool running) {
	L.hasComp = true; L.running = running; L.phi = phi; L.spb = 60.0 / c.bpm; L.steps = c.steps; L.beatsPerStep = c.bps;
	for (int i = 0; i < 16; i++) { L.root[i] = c.root[i]; L.qual[i] = c.q[i]; }
	L.external = false; L.groove = c.groove; L.genre = c.genre; L.swing = 0.f;
}

struct Run {
	std::vector<SoloHost::Log> log[2];
	std::vector<float> audio;   // mono, when asked
	CompLink lastLink; Plan plan;
	double endBeat = -1;
};
// one or two Soloists over a chart; the second hears the first through the link (one sample late, as in Rack)
static Run run(const Chart& c, SIn base, int nSol, double maxBeats, bool audio = false, SIn* second = nullptr) {
	const double SR = audio ? 48000 : 4000, dt = 1.0 / SR;   // notes only: a coarse clock is enough
	SoloHost h[2]; Run R;
	for (int k = 0; k < nSol; k++) { h[k].setSampleRate(SR); h[k].logOn = true; h[k].renderAudio = audio; }
	CompLink fromSol[2];
	double phi = 0; const double spb = 60.0 / c.bpm;
	long long N = (long long)((maxBeats + 2) * spb * SR);
	for (long long s = 0; s < N; s++) {
		float mix = 0;
		CompLink m0[2];
		for (int k = 0; k < nSol; k++) {
			SIn in = (k == 1 && second) ? *second : base;
			CompLink L; fillComp(L, c, phi, true);
			int o = 1 - k;   // the other Soloist's slot as heard
			if (nSol > 1 && fromSol[o].sol[o].on) linkCopySolo(L, fromSol[o], o);
			in.linkOk = true; in.link = L; in.follower = k == 1;
			SOut out; h[k].process(in, out, dt);
			m0[k] = CompLink(); h[k].fillLink(in, m0[k]);
			mix += out.L;
		}
		for (int k = 0; k < nSol; k++) fromSol[k] = m0[k];
		if (audio) R.audio.push_back(mix * 0.1f);
		phi += dt / spb;
	}
	for (int k = 0; k < nSol; k++) R.log[k] = h[k].log;
	R.lastLink = fromSol[0]; R.plan = h[0].plan; R.endBeat = h[0].plan.endBeat;
	return R;
}

static void writeWav(const char* path, const std::vector<float>& a, int sr = 48000) {
	FILE* f = fopen(path, "wb"); if (!f) return;
	int n = (int)a.size(); int bytes = n * 2;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(1); w32(sr); w32(sr * 2); w16(2); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (float x : a) { int v = (int)std::lround(std::max(-1.f, std::min(1.f, x)) * 32767); w16((uint16_t)(int16_t)v); }
	fclose(f);
}

int main(int argc, char** argv) {
	int fails = 0;
	auto t0 = std::chrono::steady_clock::now();
	// ---- 1. every progression, every horn: chord tones on strong beats, range, rests, the form
	int totalStrong = 0, totalCT = 0, worstPi = -1; double worst = 1;
	for (int pi = 0; pi < comper::NUM_PROGRESSIONS; pi++) {
		Chart c; if (!loadChart(pi, c)) { printf("FAIL parse %s\n", comper::PROGRESSIONS[pi].name); fails++; continue; }
		for (int kind = 0; kind < 3; kind++) {
			SIn in; in.kind = kind; in.outside = 0.0; in.choruses = 2; in.trade = 2; in.tradeWith = 2; in.seed = 1 + pi * 3 + kind;
			Run R = run(c, in, 1, 32 * 8 + 16);
			const auto& lg = R.log[0];
			int strong = 0, ct = 0, outRange = 0, inOther = 0, afterEnd = 0;
			double lastEnd = -1; int rests = 0;
			for (const auto& l : lg) {
				double b = l.beat; Harm h; long long st = (long long)std::floor((b + 1e-9) / c.bps); int si = (int)(st % c.steps);
				h.set(Chord(c.root[si], c.q[si]));
				double fr = b - std::floor(b); int bib = ((int)std::floor(b + 1e-9)) % 4;
				if (fr < 0.02 && (bib == 0 || bib == 2) && l.part != P_HEAD && l.part != P_OUT) { strong++; bool blue = c.genre == GEN_BLUES && isDom(h.q) && comper::pmod(l.pitch - h.root, 12) == 3; if (h.isCT(l.pitch) || blue) ct++; }
				if (l.pitch < horn::RANGE_LO[kind] || l.pitch > horn::RANGE_TOP[kind]) outRange++;
				int t = turnAt(R.plan.asLink(), b);
				if (l.part == P_TRADE && t != TURN_SOLO0) inOther++;
				if (R.endBeat >= 0 && b > R.endBeat + 0.01) afterEnd++;
				if (lastEnd >= 0 && b - lastEnd >= 1.0) rests++;
				lastEnd = b + l.len;
			}
			double r = strong ? ct / (double)strong : 0;
			totalStrong += strong; totalCT += ct;
			if (r < worst) { worst = r; worstPi = pi; }
			bool ok = r >= 0.8 && outRange == 0 && inOther == 0 && afterEnd == 0 && rests >= 6 && lg.size() > 40;
			if (!ok) { fails++; printf("FAIL %-32s %-8s notes %3d strongCT %.2f range %d otherTurn %d afterEnd %d rests %d\n", comper::PROGRESSIONS[pi].name, horn::KIND_NAMES[kind], (int)lg.size(), r, outRange, inOther, afterEnd, rests); }
		}
	}
	printf("form: strong-beat chord tones %.1f%% overall (worst %.2f, %s)\n", 100.0 * totalCT / std::max(1, totalStrong), worst, worstPi >= 0 ? comper::PROGRESSIONS[worstPi].name : "-");
	// ---- 2. determinism
	{
		Chart c; loadChart(0, c); SIn in; in.seed = 7;
		Run a = run(c, in, 1, 200), b = run(c, in, 1, 200);
		bool same = a.log[0].size() == b.log[0].size();
		for (size_t i = 0; same && i < a.log[0].size(); i++) same = a.log[0][i].at == b.log[0][i].at && a.log[0][i].pitch == b.log[0][i].pitch;
		if (!same) { fails++; printf("FAIL determinism\n"); } else printf("determinism: same notes twice (%d)\n", (int)a.log[0].size());
	}
	// ---- 3. trading: every grooves, segments 2/4/8 bars, partners drums/piano/both; turns change on bar lines
	for (int seg = 1; seg <= 3; seg++) for (int with = 0; with < 3; with++) {
		Chart c; loadChart(3, c); SIn in; in.trade = seg; in.tradeWith = with; in.choruses = 1; in.seed = 20 + seg * 3 + with;
		Run R = run(c, in, 1, 32 * 8);
		Plan P = R.plan; int bad = 0, n = 0;
		for (const auto& l : R.log[0]) if (l.part == P_TRADE) { n++; if (turnAt(P.asLink(), l.beat) != TURN_SOLO0) bad++; }
		bool barOk = std::fmod(P.tradeFrom, 4.0) == 0 && std::fmod(P.tradeTo, 4.0) == 0;
		if (bad || n == 0 || !barOk) { fails++; printf("FAIL trade %d bars with %d: %d notes, %d off-turn, from %.1f to %.1f\n", TRADE_BARS[seg], with, n, bad, P.tradeFrom, P.tradeTo); }
	}
	printf("trading: 2s, 4s and 8s with drums, piano and both checked\n");
	// ---- 4. two Soloists: Horns (head harmonised) and Turns (trading between them)
	for (int section = 0; section < 2; section++) {
		Chart c; loadChart(0, c); SIn a; a.section = section; a.trade = 2; a.choruses = 2; a.seed = 3; SIn b = a; b.kind = horn::TRUMPET; b.seed = 4;
		Run R = run(c, a, 2, 32 * 8, false, &b);
		int s0 = 0, s1 = 0, both = 0;
		for (const auto& l : R.log[0]) if (l.part == P_SOLO || l.part == P_TRADE) s0++;
		for (const auto& l : R.log[1]) if (l.part == P_SOLO || l.part == P_TRADE) s1++;
		// overlap of solo notes in solo parts
		for (const auto& x : R.log[0]) if (x.part == P_SOLO || x.part == P_TRADE) for (const auto& y : R.log[1]) if ((y.part == P_SOLO || y.part == P_TRADE) && std::fabs(x.beat - y.beat) < 0.01) both++;
		printf("two soloists, %s: leader %d solo notes, second %d, together %d, traders %d\n", section ? "Turns" : "Horns", s0, s1, both, R.plan.traders);
		if (s0 == 0 || s1 == 0 || both > 0) { fails++; printf("FAIL two soloists %d\n", section); }
	}
	// ---- 5. outside all the way: still in range, plays
	{
		Chart c; loadChart(0, c); SIn in; in.outside = 1.0; in.energy = 0.9; in.form = F_SOLO; in.seed = 5;
		Run R = run(c, in, 1, 128); int bad = 0;
		for (const auto& l : R.log[0]) if (l.pitch < horn::RANGE_LO[0] - 12 || l.pitch > horn::RANGE_TOP[0]) bad++;
		printf("free: %d notes, %d out of range\n", (int)R.log[0].size(), bad);
		if (bad || R.log[0].size() < 50) { fails++; printf("FAIL free\n"); }
	}
	// ---- optional WAV
	if (argc > 2) {
		int pi = atoi(argv[2]); Chart c; loadChart(pi, c); SIn in; in.kind = argc > 3 ? atoi(argv[3]) : 0; in.choruses = 2; in.trade = 2; in.seed = 9;
		Run R = run(c, in, 1, 32 * 6, true); writeWav(argv[1], R.audio);
		printf("wrote %s\n", argv[1]);
	}
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	printf("%s (%d failures, %.1f s)\n", fails ? "SOLOIST FAILED" : "soloist: all checks pass", fails, secs);
	return fails ? 1 : 0;
}
