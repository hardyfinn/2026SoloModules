// Offline check and demo render for the Belleville trio (no Rack needed).
//   make test       runs the checks
//   ./test/render_trio out.wav STYLE BASS SIG BPM BARS SEED   renders a demo with simple test synths
#include "../src/dsp/StrataEngine.hpp"
#include "../src/dsp/UndertowEngine.hpp"
#include "../src/dsp/SignalEngine.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
using namespace bv;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Voice { float f = 0, ph = 0, env = 0, amp = 0, cut = 0, lp = 0, lp2 = 0, glideTo = 0; bool on = false; float acc = 0; };

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path, "wb"); if (!f) return;
	int n = (int)L.size(); int bytes = n * 4;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) { int16_t a = (int16_t)(std::max(-1.f, std::min(1.f, L[i])) * 32000), b = (int16_t)(std::max(-1.f, std::min(1.f, R[i])) * 32000); fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f); }
	fclose(f);
}
static float mtof(float n) { return 440.f * std::pow(2.f, (n - 69.f) / 12.f); }

struct Run {
	StrataEngine S; UndertowEngine U; SignalEngine G;
	std::vector<NoteEvent> se, ue, ge;
};

static void play(Run& r, int bars, float energy = 0.5f) {
	r.S.energy = energy;
	r.S.checkReload();
	for (int64_t st = 0; st < bars * 16; st++) {
		NoteEvent a = r.S.tick(st);
		NoteEvent b = r.U.tick(r.S.chart, st, energy, r.S.plannedMask());
		NoteEvent c = r.G.tick(r.S.chart, st, energy, r.S.plannedMask(), r.U.barMask, 0.f);
		r.se.push_back(a); r.ue.push_back(b); r.ge.push_back(c);
	}
}

static void checks() {
	// every style, progression, rhythm and voicing: notes are sane and in the chord
	for (int st = 0; st < ST_COUNT; st++) for (int p = 0; p < progsInStyle(st); p++) for (int rh = 0; rh < RH_COUNT; rh++) for (int vo = 0; vo < VO_COUNT; vo++) {
		StrataEngine S; S.style = st; S.progSel = p; S.rhythm = rh; S.voicing = vo; S.drift = 0.f; S.colour = 0.5f;
		S.checkReload();
		int hits = 0;
		for (int64_t s = 0; s < S.chart.loopSteps * 2; s++) {
			NoteEvent e = S.tick(s);
			if (!e.hit) continue;
			hits++;
			CHECK(e.n >= 2 && e.n <= 6, "strata n=%d st%d p%d", e.n, st, p);
			for (int k = 0; k < e.n; k++) CHECK(e.note[k] >= 30 && e.note[k] <= 96, "strata note %d out of range st%d p%d rh%d vo%d", e.note[k], st, p, rh, vo);
			// a pushed hit may play the next chord; otherwise all notes are chord tones
			int slot = S.chart.slotAtStep(s), nslot = S.chart.nextSlot(s);
			uint16_t m = S.chart.chordMask(slot) | S.chart.chordMask(nslot);
			if (vo != VO_MEMORY) for (int k = 0; k < e.n; k++) CHECK(m & (1 << (e.note[k] % 12)), "strata non-chord note st%d p%d rh%d vo%d", st, p, rh, vo);
		}
		CHECK(hits > 0, "strata silent st%d p%d rh%d", st, p, rh);
	}
	// bass: in range, mostly chord tones on the beat
	for (int bs = 0; bs < BS_COUNT; bs++) for (int st = 0; st < ST_COUNT; st++) {
		Run r; r.S.style = st; r.U.style = bs; r.U.follow = 0.6f;
		play(r, 32);
		int hits = 0, onBeatCT = 0, onBeat = 0;
		for (size_t i = 0; i < r.ue.size(); i++) {
			const NoteEvent& e = r.ue[i];
			if (!e.hit) continue;
			hits++;
			CHECK(e.note[0] >= 20 && e.note[0] <= 60, "bass note %d style %d", e.note[0], bs);
			CHECK(e.gateSteps > 0.f && e.gateSteps <= 16.f, "bass gate %f", e.gateSteps);
			if (i % 4 == 0) { onBeat++; if (r.S.chart.chordMask(r.S.chart.slotAtStep(i)) & (1 << (e.note[0] % 12))) onBeatCT++; }
		}
		CHECK(hits > 8, "bass %s too quiet in %s: %d", bassStyleName(bs), styleName(st), hits);
		if (onBeat > 4) CHECK(onBeatCT >= onBeat * 0.6f, "bass %s off the chord on the beat (%d/%d)", bassStyleName(bs), onBeatCT, onBeat);
	}
	// kick space: with SPACE at full, offbeat bass never sits on a four-to-the-floor kick
	{ Run r; r.U.style = BS_ROLLING; r.U.space = 1.f; play(r, 16);
	  for (size_t i = 0; i < r.ue.size(); i++) if (r.ue[i].hit) CHECK(i % 4 != 0, "bass on the kick at %d", (int)i); }
	// melody: in range, motif mutates, FREEZE holds it
	for (int ss = 0; ss < SS_COUNT; ss++) {
		Run r; r.G.style = ss; r.G.newMotif(77); play(r, 32);
		int hits = 0;
		for (auto& e : r.ge) if (e.hit) { hits++; CHECK(e.note[0] >= 40 && e.note[0] <= 108, "melody note %d style %d", e.note[0], ss); }
		CHECK(hits > 6, "melody %s too quiet: %d", sigStyleName(ss), hits);
	}
	{ SignalEngine a; a.mutate = 1.f; a.newMotif(5); MotifStep before[64]; for (int i = 0; i < 64; i++) before[i] = a.motif[i];
	  for (int k = 0; k < 20; k++) a.mutateOnce();
	  int diff = 0; for (int i = 0; i < 64; i++) diff += before[i].move != a.motif[i].move || before[i].uOn != a.motif[i].uOn;
	  CHECK(diff > 0 && a.gen > 0, "mutate did nothing");
	  a.freeze = true; int g = a.gen; for (int k = 0; k < 20; k++) a.mutateOnce(); CHECK(a.gen == g, "freeze did not hold"); }
	// drift: moves chords away, HOLD stops it, chords stay in the chart
	{ StrataEngine S; S.drift = 1.f; S.checkReload(); for (int64_t s = 0; s < 64 * 20; s++) S.tick(s);
	  CHECK(S.driftedCount >= 0 && S.driftedCount <= S.baseN, "drift count");
	  StrataEngine H; H.drift = 1.f; H.hold = true; H.checkReload(); for (int64_t s = 0; s < 64 * 20; s++) H.tick(s); CHECK(H.driftedCount == 0, "hold did not hold"); }
	// external chords: Strata plays the notes it is given
	{ StrataEngine S; int notes[4] = {53, 56, 60, 63}; S.setExternal(4, notes); S.rhythm = RH_STAB1; S.checkReload();
	  NoteEvent e = S.tick(0); CHECK(e.hit, "external: no hit");
	  for (int k = 0; k < e.n; k++) { int pc = e.note[k] % 12; CHECK(pc == 5 || pc == 8 || pc == 0 || pc == 3, "external: wrong note %d", e.note[k]); } }
	// determinism
	{ Run a, b; play(a, 16); play(b, 16); bool same = true;
	  for (size_t i = 0; i < a.ge.size(); i++) same = same && a.ge[i].hit == b.ge[i].hit && a.ge[i].note[0] == b.ge[i].note[0] && a.ue[i].note[0] == b.ue[i].note[0];
	  CHECK(same, "not deterministic"); }
}

// ---- demo render with plain test synths (saw stabs, a squelchy bass, a soft lead, a four-to-the-floor click for time)
static void render(const char* path, int style, int bstyle, int sstyle, float bpm, int bars, uint32_t seed, int rhythm, int voicing, float drift, float mutate) {
	const int sr = 44100;
	Run r; r.S.style = style; r.S.rhythm = rhythm; r.S.voicing = voicing; r.S.drift = drift; r.S.newSeed(seed); r.S.spread = 0.35f; r.S.colour = 0.55f;
	r.U.style = bstyle; r.U.reroll(seed * 3 + 1); r.U.follow = 0.5f; r.U.slide = bstyle == BS_ACID ? 0.5f : 0.15f; r.U.accent = 0.35f;
	r.G.style = sstyle; r.G.newMotif(seed * 7 + 3); r.G.mutate = mutate; r.G.answer = 0.6f;
	r.S.checkReload();
	float stepSec = 60.f / bpm / 4.f; int stepN = (int)(stepSec * sr);
	int total = stepN * 16 * bars + sr * 2;
	std::vector<float> L(total, 0.f), R(total, 0.f);
	Voice cv[6], bv1, mv;
	int cGateOff = 0, bGateOff = 0, mGateOff = 0;
	float swing = 0.54f;
	struct Pending { int at; NoteEvent e; int who; };
	std::vector<Pending> pend;
	float clickEnv = 0.f, clickPh = 0.f;
	float dl[22050] = {}; int dlw = 0;   // stab echo
	for (int i = 0; i < total; i++) {
		if (i % stepN == 0 && i / stepN < 16 * bars) {
			int64_t st = i / stepN;
			int delay = (int)(swingDelaySteps(st, swing) * stepN);
			NoteEvent a = r.S.tick(st);
			NoteEvent b = r.U.tick(r.S.chart, st, 0.55f, r.S.plannedMask());
			NoteEvent c = r.G.tick(r.S.chart, st, 0.55f, r.S.plannedMask(), r.U.barMask, 0.f);
			if (a.hit) pend.push_back({i + delay, a, 0});
			if (b.hit) pend.push_back({i + delay, b, 1});
			if (c.hit) pend.push_back({i + delay, c, 2});
			if (st % 4 == 0) clickEnv = 1.f;
		}
		for (size_t k = 0; k < pend.size();) {
			if (pend[k].at == i) {
				NoteEvent& e = pend[k].e;
				if (pend[k].who == 0) { for (int v = 0; v < 6; v++) { cv[v].on = v < e.n; if (v < e.n) { cv[v].f = mtof(e.note[v]); cv[v].amp = e.vel[v]; cv[v].env = 0; } } cGateOff = i + (int)(e.gateSteps * stepN); }
				if (pend[k].who == 1) { bool tie = bv1.on && i < bGateOff + 10; bv1.glideTo = mtof(e.note[0]); if (!tie) { bv1.f = bv1.glideTo; bv1.env = 0; } bv1.on = true; bv1.acc = e.accent ? 1.f : 0.f; bGateOff = i + (int)(e.gateSteps * stepN); }
				if (pend[k].who == 2) { mv.f = mtof(e.note[0]); mv.amp = e.vel[0]; mv.env = 0; mv.on = true; mGateOff = i + (int)(e.gateSteps * stepN); }
				pend.erase(pend.begin() + k);
			} else k++;
		}
		bool cg = i < cGateOff, bg = i < bGateOff, mg = i < mGateOff;
		// chords: two detuned saws per note, low-passed
		float cs = 0.f;
		for (int v = 0; v < 6; v++) if (cv[v].on) {
			Voice& o = cv[v];
			o.env += cg ? (1.f - o.env) * 0.004f : -o.env * 0.0012f;
			o.ph += o.f / sr; if (o.ph > 1) o.ph -= 1;
			float p2 = std::fmod(o.ph * 1.004f + 0.3f, 1.f);
			float s = (o.ph * 2 - 1) + (p2 * 2 - 1);
			o.lp += (s - o.lp) * 0.10f; o.lp2 += (o.lp - o.lp2) * 0.10f;
			cs += o.lp2 * o.env * o.amp * 0.11f;
		}
		float echo = dl[(dlw + 22050 - (int)(stepN * 3) % 22050) % 22050];
		dl[dlw] = cs + echo * (style == ST_DUB ? 0.55f : 0.3f); dlw = (dlw + 1) % 22050;
		// bass: saw with a resonant-ish envelope filter
		bv1.f += (bv1.glideTo - bv1.f) * 0.004f;
		bv1.env += bg ? (1.f - bv1.env) * 0.05f : -bv1.env * 0.004f;
		bv1.ph += bv1.f / sr; if (bv1.ph > 1) bv1.ph -= 1;
		float bsaw = bv1.ph * 2 - 1;
		bv1.cut += ((bg ? 0.05f + 0.12f * bv1.acc : 0.02f) - bv1.cut) * 0.0008f;
		bv1.lp += (bsaw - bv1.lp) * (0.02f + bv1.cut); bv1.lp2 += (bv1.lp - bv1.lp2) * (0.03f + bv1.cut);
		float bs = (bv1.lp2 + 0.6f * std::sin(bv1.ph * 6.2831853f)) * bv1.env * 0.32f;
		// melody: triangle-ish
		mv.env += mg ? (1.f - mv.env) * 0.01f : -mv.env * 0.0007f;
		mv.ph += mv.f / sr; if (mv.ph > 1) mv.ph -= 1;
		float tri = 1.f - 4.f * std::fabs(mv.ph - 0.5f);
		mv.lp += (tri - mv.lp) * 0.3f;
		float ms = mv.lp * mv.env * mv.amp * 0.16f;
		// a quiet click on the beat so the timing is easy to hear (the trio has no drums)
		clickEnv *= 0.9985f; clickPh += 55.f / sr;
		float ck = std::sin(clickPh * 6.2831853f * (1.f + 3.f * clickEnv)) * clickEnv * clickEnv * 0.35f;
		L[i] = cs * 0.9f + echo * 0.35f + bs + ms * 0.8f + ck;
		R[i] = cs * 0.9f + echo * 0.45f + bs + ms * 1.1f + ck;
	}
	writeWav(path, L, R, sr);
	printf("wrote %s (%s, %s, %s, %d bars)\n", path, styleName(style), bassStyleName(bstyle), sigStyleName(sstyle), bars);
}

int main(int argc, char** argv) {
	if (argc > 1) {
		render(argv[1], argc > 2 ? atoi(argv[2]) : 0, argc > 3 ? atoi(argv[3]) : 0, argc > 4 ? atoi(argv[4]) : 0,
		       argc > 5 ? atof(argv[5]) : 124.f, argc > 6 ? atoi(argv[6]) : 16, argc > 7 ? atoi(argv[7]) : 1,
		       argc > 8 ? atoi(argv[8]) : RH_OFFBEAT, argc > 9 ? atoi(argv[9]) : VO_CLOSE, argc > 10 ? atof(argv[10]) : 0.3f, argc > 11 ? atof(argv[11]) : 0.35f);
		return 0;
	}
	checks();
	printf(fails ? "%d checks FAILED\n" : "all trio checks passed\n", fails);
	return fails ? 1 : 0;
}
