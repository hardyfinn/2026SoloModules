// Offline checks for the Riddim engine + demo WAVs. No Rack needed.
//   make test      (writes riddim-tour.wav and riddim-kits.wav into the folder given, default .)
#include "../src/RiddimDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>

using namespace riddim;
static const float SR = 48000.f;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); fails++; } else std::printf("ok:   %s\n", msg); } while (0)

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	uint32_t n = L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32((uint32_t) SR); u32((uint32_t) SR * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

struct Player {
	DrumEngine eng; Seq seq; Ctl c; float peak = 0.f; bool bad = false;
	float lvl[10]; float swing = 0.f; float stepSamples; float pend[10]; uint8_t pl[10];
	std::vector<float> L, R;
	Player(float bpm) {
		eng.setSampleRate(SR); stepSamples = SR * 60.f / bpm / 4.f;
		for (int i = 0; i < 10; i++) { lvl[i] = 0.8f; pend[i] = -1.f; }
	}
	void bar(int riddim, int var, bool fill, float mutate = 0.f) {
		for (int s = 0; s < 16; s++) {
			uint8_t lv[10];
			bool odd = s & 1;
			seq.fillArmed = fill;
			seq.next(riddim, var, false, mutate, false, lv);
			for (int t = 0; t < 10; t++) {
				if (!lv[t]) continue;
				if (odd && swing > 0.f) { pend[t] = swing * 0.6f * stepSamples; pl[t] = lv[t]; }
				else eng.hit(t, lv[t], c);
			}
			for (int n = 0; n < (int) stepSamples; n++) {
				for (int t = 0; t < 10; t++)
					if (pend[t] >= 0.f && --pend[t] < 0.f) eng.hit(t, pl[t], c);
				float v[10]; eng.process(c, v);
				static const float pan[10] = {0.f, -0.05f, 0.25f, -0.2f, 0.3f, 0.35f, 0.3f, -0.35f, 0.4f, -0.3f};
				float l = 0, r = 0;
				for (int t = 0; t < 10; t++) {
					float a = (pan[t] + 1.f) * 0.25f * PI_F;
					l += v[t] * lvl[t] * std::cos(a) * 1.414f; r += v[t] * lvl[t] * std::sin(a) * 1.414f;
				}
				l = std::tanh(l * 0.7f) / 0.7f * 0.6f; r = std::tanh(r * 0.7f) / 0.7f * 0.6f;
				if (!std::isfinite(l) || !std::isfinite(r)) bad = true;
				peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
				L.push_back(l); R.push_back(r);
			}
		}
	}
};

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	CHECK(NUM_RIDDIMS == 24, "24 riddims");
	// every riddim has a kick or a one-drop rim/snare, and A != B
	bool allHit = true, allDiff = true;
	for (int r = 0; r < NUM_RIDDIMS; r++) {
		int hits = 0, diff = 0;
		for (int t = 0; t < 10; t++) for (int s = 0; s < 16; s++) { hits += RIDDIMS[r].a[t][s] > 0; diff += RIDDIMS[r].a[t][s] != RIDDIMS[r].b[t][s]; }
		if (hits < 8) allHit = false;
		if (diff == 0) allDiff = false;
	}
	CHECK(allHit, "every riddim has at least 8 hits per bar");
	CHECK(allDiff, "every riddim's B variation differs from A");

	// each voice makes sound, in each kit, and stays finite and bounded
	bool sounds = true, finite = true; float maxPk = 0.f;
	for (int k = 0; k < 8; k++) {
		for (int t = 0; t < 10; t++) {
			DrumEngine e; e.setSampleRate(SR); Ctl c; c.kit = k;
			e.hit(t, 3, c);
			float en = 0.f, pk = 0.f;
			for (int n = 0; n < (int) SR / 2; n++) { float o[10]; e.process(c, o); for (int i = 0; i < 10; i++) { if (!std::isfinite(o[i])) finite = false; en += o[i] * o[i]; pk = std::max(pk, std::fabs(o[i])); } }
			if (en < 0.5f) { sounds = false; std::printf("   silent: kit %d voice %d (%g)\n", k, t, en); }
			maxPk = std::max(maxPk, pk);
		}
	}
	CHECK(sounds, "every voice sounds in every kit");
	CHECK(finite, "no NaN/inf in voices");
	CHECK(maxPk <= 1.05f, "voices stay within about 1.0");
	// closed hat chokes the open hat
	{
		DrumEngine e; e.setSampleRate(SR); Ctl c; c.kit = 1;
		e.hit(OHAT, 2, c); e.hit(CHAT, 2, c);
		CHECK(!e.v[OHAT].active(), "closed hat chokes open hat");
	}
	// fill replaces the last beat; mutate keeps hits in range
	{
		Seq s; s.fillArmed = true; uint8_t lv[10]; bool fillOk = false;
		for (int i = 0; i < 16; i++) { s.fillArmed = true; s.next(4, 0, false, 0.f, false, lv); if (i == 15 && (lv[SNARE] || lv[TOM] || lv[RIM] || lv[KICK])) fillOk = true; }
		CHECK(fillOk, "fill plays on the last step of the bar");
		Seq m; bool rng = true;
		for (int i = 0; i < 400; i++) { m.next(i % 24, 1, false, 1.f, false, lv); for (int t = 0; t < 10; t++) if (lv[t] > 3) rng = false; }
		CHECK(rng, "mutate keeps levels 0-3");
	}

	// ---- demo 1: a tour of 12 riddims at 90 BPM, A bar then B bar with a fill
	{
		Player p(90.f);
		const int tour[12] = {0, 1, 2, 4, 5, 8, 12, 14, 17, 18, 7, 9};
		for (int i = 0; i < 12; i++) {
			p.c.kit = RIDDIMS[tour[i]].kit;
			p.seq.reset();
			p.bar(tour[i], 0, false); p.bar(tour[i], 2, true);
		}
		CHECK(!p.bad && p.peak < 1.05f, "tour: finite and not clipping");
		std::printf("tour peak %.3f\n", p.peak);
		writeWav(dir + "/riddim-tour.wav", p.L, p.R);
	}
	// ---- demo 2: Dem Bow through all eight kits, then with swing and mutate
	{
		Player p(95.f);
		for (int k = 0; k < 8; k++) { p.c.kit = k; p.seq.reset(); p.bar(4, 0, false); }
		p.swing = 0.4f; p.c.kit = 2;
		for (int i = 0; i < 2; i++) p.bar(4, 0, false, 0.5f);
		p.c.crush = 0.6f; p.bar(4, 2, true, 0.3f);
		CHECK(!p.bad && p.peak < 1.05f, "kits: finite and not clipping");
		writeWav(dir + "/riddim-kits.wav", p.L, p.R);
	}
	std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
