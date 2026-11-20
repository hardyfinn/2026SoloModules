// Offline checks + demo WAV for the Hangover engine (no Rack needed).
//   make test      (or: g++ -O2 -std=c++17 test/render.cpp -o test/render && ./test/render .)
// Writes hangover-demo.wav: the same groove clean, with the default dirt, then filthy.
#include "../src/HangoverDSP.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace hangover;

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) { printf("cannot write %s\n", path.c_str()); return; }
	uint32_t n = (uint32_t) L.size();
	uint32_t dataBytes = n * 4;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(dataBytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] / 10.f * 32767.f));
		w16((uint16_t) a); w16((uint16_t) b);
	}
	fclose(f);
}

struct Hit { int step; int note; float vel; };  // note -1 = gu, -2 = roll on, -3 = roll off, -4 mute on, -5 mute off

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const int sr = 48000;
	HangoverEngine e;
	e.setSampleRate((float) sr);
	e.reseed(7);
	e.p.scale = 0;  // D Kurd
	e.p.root = 2;
	e.p.decay = 4.5f;
	e.p.force = 0.75f;
	e.p.touch = 0.3f;
	e.p.strike = 0.3f;
	e.p.gu = 0.6f;

	// one bar of 16ths, a typical handpan groove (D Kurd: 0 ding, 1 A3, 2 Bb3, 3 C4, 4 D4, 5 E4, 6 F4, 7 G4, 8 A4)
	std::vector<Hit> bar = {
		{0, 0, 0.9f}, {0, -1, 0.7f}, {2, 4, 0.5f}, {3, 6, 0.55f}, {4, 1, 0.7f}, {6, 5, 0.5f}, {7, 4, 0.45f},
		{8, 0, 0.8f}, {10, 3, 0.55f}, {11, 2, 0.5f}, {12, 8, 0.6f}, {13, 7, 0.45f}, {14, 6, 0.5f}, {15, 5, 0.4f},
	};
	std::vector<Hit> barB = {
		{0, 0, 0.9f}, {0, -1, 0.8f}, {2, 4, 0.5f}, {3, 6, 0.55f}, {4, 1, 0.7f}, {6, 5, 0.5f},
		{8, 0, 0.8f}, {8, -1, 0.5f}, {10, 6, 0.6f}, {12, 8, 0.7f}, {12, -2, 0.f}, {15, -3, 0.f},
	};
	const float step = 60.f / 104.f / 4.f;
	struct Section { const char* name; float dirt; int bars; };
	// dirt: knob values for tuning, beat, rattle, bleed, air, skin, slop, room
	const float defaults[8] = {0.3f, 0.3f, 0.2f, 0.3f, 0.3f, 0.3f, 0.2f, 0.25f};
	std::vector<Section> secs = {{"clean", 0.f, 4}, {"default dirt", 0.5f, 4}, {"filthy", 1.f, 4}};

	std::vector<float> L, R;
	int totalBadSamples = 0;
	float peak = 0.f;
	int barIndex = 0;
	for (auto& s : secs) {
		float m = s.dirt * 2.f;
		HgParams& p = e.p;
		p.tuning = std::min(1.f, defaults[0] * m); p.beat = std::min(1.f, defaults[1] * m);
		p.rattle = std::min(1.f, defaults[2] * m); p.bleed = std::min(1.f, defaults[3] * m);
		p.air = std::min(1.f, defaults[4] * m); p.skin = std::min(1.f, defaults[5] * m);
		p.slop = std::min(1.f, defaults[6] * m); p.room = std::min(1.f, defaults[7] * m);
		if (s.dirt >= 1.f) { p.room = 0.45f; p.rattle = 0.75f; }
		float secPeak = 0.f;
		for (int b = 0; b < s.bars; b++, barIndex++) {
			const std::vector<Hit>& pat = (b % 2 == 1) ? barB : bar;
			for (int st = 0; st < 16; st++) {
				for (auto& h : pat) {
					if (h.step != st) continue;
					if (h.note >= 0) e.strike(h.note, h.vel);
					else if (h.note == -1) e.strikeGu(h.vel);
					else if (h.note == -2) e.setRoll(true);
					else if (h.note == -3) e.setRoll(false);
				}
				// last bar of each section: palm mute on the final beat
				if (b == s.bars - 1 && st == 12) { e.muteGate = 1.f; e.palm(); }
				int n = (int) (step * sr);
				for (int i = 0; i < n; i++) {
					float l, r;
					e.process(l, r);
					if (!std::isfinite(l) || !std::isfinite(r)) { totalBadSamples++; l = r = 0.f; }
					L.push_back(l); R.push_back(r);
					secPeak = std::max(secPeak, std::max(std::fabs(l), std::fabs(r)));
				}
			}
			e.muteGate = 0.f;
		}
		printf("%-13s peak %.2f V  rattles %d\n", s.name, secPeak, e.rattleHits); e.rattleHits = 0;
		peak = std::max(peak, secPeak);
	}
	// tail: a slow descending line on the ding and fields, then let it ring
	int order[9] = {8, 7, 6, 5, 4, 3, 2, 1, 0};
	for (int k = 0; k < 9; k++) {
		e.strike(order[k], 0.55f + 0.04f * k);
		int n = (int) (step * 2 * sr);
		for (int i = 0; i < n; i++) { float l, r; e.process(l, r); L.push_back(l); R.push_back(r); }
	}
	for (int i = 0; i < sr * 4; i++) { float l, r; e.process(l, r); L.push_back(l); R.push_back(r); }

	// silence check: after a long time with nothing played the output should die away
	HangoverEngine q;
	q.setSampleRate((float) sr);
	q.strike(0, 1.f);
	float late = 0.f;
	for (int i = 0; i < sr * 30; i++) { float l, r; q.process(l, r); if (i > sr * 28) late = std::max(late, std::fabs(l)); }

	// every scale and root lays out sensibly
	int badScale = 0;
	for (int sc = 0; sc < NUM_SCALES; sc++)
		for (int rt = 0; rt < 12; rt++) {
			q.p.scale = sc; q.p.root = rt; q.updateCoefs();
			for (int i = 1; i < q.nNotes; i++) if (q.note[i].midi <= q.note[i - 1].midi) badScale++;
			if (q.note[0].freq < 100.f || q.note[0].freq > 210.f) badScale++;
		}
	// nearest-note quantising: D3 = -10/12 V
	q.p.scale = 0; q.p.root = 2; q.updateCoefs();
	int nq = q.nearest(-10.f / 12.f), nq2 = q.nearest(-3.f / 12.f);

	writeWav(dir + "/hangover-demo.wav", L, R, sr);
	printf("samples %zu  bad %d  peak %.2f V  late tail %.5f V  bad scales %d  D3->%d  A3->%d\n",
	       L.size(), totalBadSamples, peak, late, badScale, nq, nq2);
	bool ok = totalBadSamples == 0 && peak > 1.f && peak < 10.f && late < 0.05f && badScale == 0 && nq == 0 && nq2 == 1;
	printf(ok ? "OK\n" : "FAILED\n");
	return ok ? 0 : 1;
}
