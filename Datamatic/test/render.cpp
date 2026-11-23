// Offline checks for Datamatic + Test Pattern, and a demo WAV. No Rack needed.
//   make test      (writes test/datamatic-demo.wav)
#include "../src/DatamaticEngine.hpp"
#include "../src/TestPatternDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>

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
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] / 10.f * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";

	// ---- checks
	{
		datamatic::Engine e;
		datamatic::Settings st;
		st.source = datamatic::SRC_COUNT; st.word = 0b10110001; st.length = 8;
		e.rebuild(st.source, st.word);
		CHECK(e.bytes[0] == 0b10110001, "COUNT first byte is WORD");
		CHECK(e.subBit(0) && !e.subBit(1) && e.subBit(2) && e.subBit(3), "SUB reads the bits forwards");
		st.source = datamatic::SRC_PI; st.word = 0;
		e.rebuild(st.source, st.word);
		CHECK(e.bytes[0] == 0x31 && e.bytes[1] == 0x41, "PI is binary-coded decimal 3,1,4,1");

		// count gates over 16 clocks at RATE 4
		datamatic::Engine g;
		st.rateIndex = 2; st.length = 16; st.bpm = 120.f;
		int edges[4] = {0, 0, 0, 0}; bool last[4] = {false, false, false, false};
		int ticks = 0;
		double secs = 16 * 60.0 / 120.0 / 4.0;
		for (int i = 0; i < (int) (secs * SR); i++) {
			datamatic::Outputs o = g.process(1.0 / SR, false, false, false, 0.f, st);
			if (o.clockTick) ticks++;
			for (int v = 0; v < 4; v++) { if (o.gate[v] && !last[v]) edges[v]++; last[v] = o.gate[v]; }
		}
		std::printf("      16 clocks: ticks %d, edges SUB %d TONE %d CLICK %d NOISE %d\n", ticks, edges[0], edges[1], edges[2], edges[3]);
		CHECK(ticks == 16, "internal clock gives 16 ticks in 16 sixteenths");
		CHECK(edges[2] > 8 && edges[2] < 64, "CLICK fires at sub-clock rate");
		CHECK(edges[0] > 0 && edges[1] > 0 && edges[3] > 0, "every lane plays something from pi");

		// CUT = 1 silences half bars or more
		datamatic::Engine c;
		st.cut = 1.f;
		int cutSamples = 0, total = (int) (secs * 4 * SR);
		for (int i = 0; i < total; i++) if (c.process(1.0 / SR, false, false, false, 0.f, st).cut) cutSamples++;
		CHECK(cutSamples > total / 3, "CUT at full drops at least half the bars");

		// Test Pattern: silence without gates, sound with
		testpattern::Engine tp;
		testpattern::Params p;
		bool off[4] = {false, false, false, false}, on[4] = {true, false, false, false};
		float peak = 0.f;
		for (int i = 0; i < 1000; i++) peak = std::max(peak, std::fabs(tp.process(SR, off, false, 0.f, 0.f, 0.f, p).l));
		CHECK(peak == 0.f, "digital silence when no gate is high");
		peak = 0.f;
		for (int i = 0; i < 4800; i++) peak = std::max(peak, std::fabs(tp.process(SR, on, false, 0.f, 0.f, 0.f, p).l));
		CHECK(peak > 3.f, "SUB sounds while its gate is high");
		CHECK(std::fabs(testpattern::toneHz(1, 0.5f) - 1000.f) < 0.01f, "TONE MID at noon is the 1 kHz test tone");
	}

	// ---- demo: 48 bars at 132 BPM, an arrangement made only from knob moves
	datamatic::Engine seq;
	testpattern::Engine tp;
	datamatic::Settings st;
	testpattern::Params p;
	st.bpm = 132.f;
	double bar = 60.0 / 132.0 * 4.0;
	int bars = 48;
	int n = (int) ((bars * bar + 1.5) * SR);
	std::vector<float> L(n), R(n);
	for (int i = 0; i < n; i++) {
		double t = i / SR;
		int b = (int) (t / bar);
		// arrangement
		st.source = datamatic::SRC_PI; st.word = 0; st.length = 16; st.rateIndex = 0;
		st.mutate = 0.f; st.roll = 0.f; st.cut = 0.f;
		p.toneLevel = 0.f; p.noiseLevel = 0.f; p.toneZone = 2; p.tonePitch = 0.5f; p.toneScan = 0.f;
		p.subShape = 0.f; p.clickColour = 0.f; p.subLen = 1.f;
		if (b >= 8) { st.rateIndex = 1; p.toneLevel = 0.35f; }
		if (b >= 16) { st.source = datamatic::SRC_PRIMES; st.word = 3; st.rateIndex = 2; p.noiseLevel = 0.3f; st.roll = 0.3f; }
		if (b >= 24) { st.source = datamatic::SRC_COUNT; st.word = 0xA5; st.rateIndex = 3; p.toneZone = 1; p.toneScan = 0.5f; p.toneLevel = 0.25f; st.roll = 0.15f; }
		if (b >= 28) { st.cut = 0.6f; p.subShape = 0.5f; }
		if (b >= 32) { st.source = datamatic::SRC_TEXT; st.word = 0; st.length = 32; st.rateIndex = 2; st.mutate = 0.6f; st.cut = 0.15f;
		               p.toneZone = 2; p.toneScan = 0.f; p.toneLevel = 0.35f; p.subShape = 0.f; p.clickColour = 0.3f; }
		if (b >= 40) { st.source = datamatic::SRC_PI; st.word = 40; st.length = 24; st.rateIndex = 4; st.roll = 0.5f; st.mutate = 0.f; st.cut = 0.3f;
		               p.clickColour = 0.f; p.subLen = 0.45f; }
		if (b >= bars) st.cut = 1.f;
		datamatic::Outputs o = seq.process(1.0 / SR, false, false, false, 0.f, st);
		if (b >= bars) o.cut = true;
		testpattern::Frame f = tp.process(SR, o.gate, o.cut, o.data, 0.f, 0.f, p);
		L[i] = f.l; R[i] = f.r;
	}
	float peak = 0.f;
	for (int i = 0; i < n; i++) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
	float g = peak > 0.f ? 8.5f / peak : 1.f;
	for (int i = 0; i < n; i++) { L[i] *= g; R[i] *= g; }
	writeWav(dir + "/test/datamatic-demo.wav", L, R);
	std::printf("demo: %.1f s, peak %.2f V before normalising\n", n / SR, peak);

	std::printf(fails ? "%d FAILED\n" : "all ok\n", fails);
	return fails ? 1 : 0;
}
