// A 16-bar demo played by the C++ engines (the same code the Rack modules run):
// TAKT drives DYNAMO, BLECH and ZAP play the beat, ORCHESTER holds chords, SPRECHER counts,
// and for the second half SPRECHER speaks through VOKODER.
//   make test/demo && ./test/demo out.wav
#include "../src/dsp/Zap.hpp"
#include "../src/dsp/Blech.hpp"
#include "../src/dsp/Dynamo.hpp"
#include "../src/dsp/Orchester.hpp"
#include "../src/dsp/Vokoder.hpp"
#include "../src/dsp/Sprecher.hpp"
#include "../src/dsp/Takt.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace wb;

static void writeWav(const char* path, const std::vector<double>& x, int fs) {
	FILE* f = std::fopen(path, "wb");
	if (!f) return;
	uint32_t n = (uint32_t)x.size(), bytes = n * 2;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(1); u32(fs); u32(fs * 2); u16(2); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (double v : x) { int16_t s = (int16_t)std::lround(std::max(-1.0, std::min(1.0, v)) * 32767.0); std::fwrite(&s, 2, 1, f); }
	std::fclose(f);
}

int main(int argc, char** argv) {
	const char* path = argc > 1 ? argv[1] : "demo.wav";
	const double fs = 48000.0, dt = 1.0 / fs;
	const int S16 = 6000;                    // 120 BPM sixteenths
	const int BARS = 16, N = BARS * 16 * S16 + (int)(2.5 * fs);
	std::vector<double> out(N, 0.0);

	TaktCore takt;                            // pattern 1 is the built-in octave motor
	TaktSettings ts; ts.gate = 0.45;
	DynamoVoice bass; bass.init(fs, 11, 11);
	double DP[D_NP] = {0, 0, .10, .5, .5, .4, 0, .30, .25, .45, .3, .002, .18, .0, .002, .30, .6, 0, .3, .3};

	BlechEngine drums; drums.init(fs, 2, 999);
	BlechParams BP; BP.box = 2; BP.age = 0.3;
	const char* PAT[6] = {"x.....x.x.......", "....x.......x...", "x.x.x.x.x.x.x.X.", "................", ".......x..x.....", "...x.....x....x."};
	ZapVoice zap; zap.init(fs, 1, 12345);
	ZapParams ZP; ZP.startHz = 6000; ZP.endHz = 180; ZP.sweep = 0.02; ZP.res = 0.6; ZP.decay = 0.18; ZP.noise = 0.5; ZP.click = 1; ZP.drive = 0.2;

	OrchDisc disc; disc.init(fs);
	OrchVoice orch[3];
	for (int i = 0; i < 3; i++) orch[i].init(fs, 777 + 31 * i);
	OrchParams OP; OP.attack = 0.25; OP.release = 0.8; OP.wobble = 0.6;
	const int CH[4][3] = {{0, 3, 7}, {-4, 0, 3}, {-2, 2, 5}, {0, 3, 7}};

	auto words = SpText::get().parse("1 2 3 4 5 6 7 8", 0);
	SpScheduler sch; sch.init(fs); sch.reserve(4096);
	SpFormant voice; voice.init(fs, 7, 4242);
	SpVoiceParams VP; VP.age = 0.3;
	VokoderEngine vok; vok.init(fs, 3, 77);
	VokParams KP; KP.release = 0.06;
	VokCarrier car;
	int word = 0;

	for (int n = 0; n < N; n++) {
		const int step = n / S16, inStep = n % S16, bar = step / 16;
		const bool playing = bar < BARS;
		const bool edge = playing && inStep == 0;
		// TAKT -> DYNAMO
		TaktRowOut ro[2]; bool end = false;
		takt.process(dt, edge, false, playing, ts, ro, end);
		double b = bass.process(ro[0].voct - 2.0, ro[0].gate ? 1.0 : 0.0, ro[0].accent ? 1.0 : 0.0, 0.0, DP);
		// BLECH and ZAP
		double t[6] = {0, 0, 0, 0, 0, 0}, d[6];
		if (edge && bar >= 1) for (int v = 0; v < 6; v++) { char c = PAT[v][step % 16]; if (c == 'x') t[v] = 1.0; if (c == 'X') t[v] = 1.6; }
		if (bar < 2) t[2] = 0;   // hats from bar 3
		drums.process(t, BP, d);
		double dr = d[0] + d[1] + d[2] + d[4] + d[5];
		double z = zap.process(edge && bar >= 4 && (step % 16 == 4 || step % 16 == 12) ? 1.0 : 0.0, 0.0, ZP);
		// ORCHESTER: one chord per bar, cycling through four
		double th0, th1; disc.advance(th0, th1);
		const int ci = bar % 4;
		const bool chordGate = playing && bar >= 2;
		double o = 0;
		for (int i = 0; i < 3; i++) {
			if (n % (16 * S16) == 0 && chordGate) orch[i].reset(0, CH[ci][i]);
			o += orch[i].process(CH[ci][i] / 12.0, chordGate ? 1.0 : 0.0, th0, th1, OP);
		}
		// SPRECHER counts on every second beat from bar 3
		if (edge && bar >= 2 && step % 8 == 0) { sch.startCopy(spWordTargets(words[word].ph, 1.0)); word = (word + 1) % (int)words.size(); }
		sch.tickRate(false, false, 1.0);
		double sp = voice.process(sch.row, 110.0, VP);
		double s;
		if (bar < 10) s = sp;
		else s = vok.process(sp * 1.5, car.process(CH[ci][0] / 12.0 - 1.0, 0.0, 0.3, 2, fs), false, KP);
		out[n] = 0.45 * b + 0.55 * dr + 0.35 * z + 0.30 * o + (bar < 10 ? 0.55 : 0.45) * s;
	}
	double pk = 1e-9;
	for (double v : out) pk = std::max(pk, std::fabs(v));
	for (double& v : out) v *= 0.89 / pk;
	writeWav(path, out, (int)fs);
	std::printf("wrote %s (%.1f s)\n", path, N / fs);
	return 0;
}
