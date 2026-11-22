// Demo + check for Surprise (no Rack needed): make test
// Plays a drum loop that changes now and then, runs the real engine on it,
// and writes:
//   test/surprise-demo.wav  - left/right: the drums, plus a pad whose filter
//                             opens with SURPRISE and a bell on each TRIG
//   test/surprise-trace.csv - time, surprise, bored, trig (for the picture)
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../src/SurpriseDSP.hpp"

using namespace surprise;

static const float SR = 44100.f;
static const float BPM = 124.f;
static const float STEP = 60.f / BPM / 4.f;

struct Noise { uint32_t s = 12345; float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (2.f / 16777216.f) - 1.f; } };
struct Kick {
	float t = 9, ph = 0;
	void hit() { t = 0; ph = 0; }
	float run(float dt) { if (t > 1) return 0; float f = 48 + 110 * std::exp(-t * 28); ph += f * dt; float v = std::sin(6.2831853f * ph) * std::exp(-t * 6.5f); t += dt; return std::tanh(1.6f * v); }
};
struct Hit {
	float t = 9, decay, cut, tone, mix, ph = 0, lp = 0, amp = 1; Noise n;
	Hit(float d, float c, float tn, float m) : decay(d), cut(c), tone(tn), mix(m) {}
	void hit(float a = 1) { t = 0; amp = a; }
	float run(float dt) { if (t > 1.5f) return 0; float x = n.next(); lp += (x - lp) * cut; ph += tone * dt; float v = ((x - lp) * (1 - mix) + mix * std::sin(6.2831853f * ph)) * std::exp(-t / decay); t += dt; return amp * v; }
};
struct Bassline {
	float ph = 0, lp = 0, t = 9, f = 55;
	void hit(float hz) { t = 0; f = hz; }
	float run(float dt) { ph += f * dt; ph -= std::floor(ph); float a = 1 - std::exp(-6.2831853f * (150 + 700 * std::exp(-t * 12)) * dt); lp += ((2 * ph - 1) - lp) * a; float e = std::exp(-t * 5); t += dt; return lp * e * 0.6f; }
};

int main() {
	const float dt = 1.f / SR;
	const char* hatA = "..x...x...x...x.";
	const char* hatB = "xxxxxxxxxxxxxxxx";
	const char* clap = "....x.......x...";
	const char* percB = "...x..x...x..x..";
	const char* roll = "....x.x.xxxxxxxx";
	// bar plan (1-based): 1-8 loop A, 9-12 new hats + perc, 13-16 A again,
	// 17 a fill, 18-20 A, 21 drop-out (hats only), 22-28 A + bassline
	int bars = 28;
	Kick kick; Hit hat(0.035f, 0.6f, 0, 0), cl(0.12f, 0.2f, 0, 0), perc(0.05f, 0.05f, 820, 0.7f), snr(0.15f, 0.25f, 190, 0.35f);
	Bassline bass;
	NoveltyEngine eng; eng.setRate(SR);
	NoveltySettings st; st.memory = 24.f; st.decay = 0.4f; st.thresh = 0.4f; st.boredRise = 30.f;
	// demo pad
	float padPh[3] = {0, 0.3f, 0.6f}, padLp = 0, padLp2 = 0;
	const float padHz[3] = {174.61f, 207.65f, 261.63f};  // F, Ab, C
	float bellT = 9, bellPh = 0;
	std::vector<int16_t> wav;
	FILE* csv = fopen("test/surprise-trace.csv", "w");
	long total = (long) (bars * 16 * STEP * SR);
	long lastStep = -1;
	int trigs = 0;
	float surpriseBar[64] = {};
	int barSamples[64] = {};
	for (long i = 0; i < total; i++) {
		double t = i / SR;
		long step = (long) (t / STEP);
		int bar = (int) (step / 16) + 1, s = (int) (step % 16);
		if (step != lastStep) {
			lastStep = step;
			bool dropout = bar == 21;
			bool fill = bar == 17;
			bool newPat = bar >= 9 && bar <= 12;
			if (!dropout && s % 4 == 0 && !(fill && s >= 8)) kick.hit();
			const char* hp = newPat ? hatB : hatA;
			if (hp[s] == 'x') hat.hit(newPat ? 0.25f : 0.4f);
			if (!dropout && !fill && clap[s] == 'x') cl.hit(0.6f);
			if (fill && roll[s] == 'x') snr.hit(0.3f + 0.04f * s);
			if (newPat && percB[s] == 'x') perc.hit(0.5f);
			if (bar >= 22 && (s == 0 || s == 3 || s == 6 || s == 10)) bass.hit(s == 6 ? 51.91f : 43.65f);
		}
		float drums = 0.9f * kick.run(dt) + hat.run(dt) * 0.7f + cl.run(dt) * 0.8f + perc.run(dt) + snr.run(dt) + bass.run(dt);
		eng.process(5.f * drums, st, dt);
		if (eng.trig) { bellT = 0; bellPh = 0; trigs++; }
		// pad: filter opens with surprise
		float pad = 0;
		for (int k = 0; k < 3; k++) { padPh[k] += padHz[k] * dt; padPh[k] -= std::floor(padPh[k]); pad += 2 * padPh[k] - 1; }
		float cut = 120.f * std::pow(2.f, eng.out * 6.f);
		float a = 1 - std::exp(-6.2831853f * cut * dt);
		padLp += (pad - padLp) * a; padLp2 += (padLp - padLp2) * a;
		bellPh += 1568.f * dt;
		float bell = std::sin(6.2831853f * bellPh) * std::exp(-bellT * 5.f) * 0.18f + std::sin(6.2831853f * bellPh * 2.76f) * std::exp(-bellT * 9.f) * 0.08f;
		bellT += dt;
		float L = drums * 0.7f + padLp2 * 0.16f + bell * 0.8f, R = drums * 0.7f + padLp2 * 0.16f + bell;
		auto c = [](float x) { return (int16_t) (std::tanh(x) * 30000.f); };
		wav.push_back(c(L)); wav.push_back(c(R));
		if (csv && i % 441 == 0) fprintf(csv, "%.3f,%.4f,%.4f,%d\n", t, eng.out, eng.bored, eng.high ? 1 : 0);
		if (bar < 64) { surpriseBar[bar] += eng.out; barSamples[bar]++; }
	}
	if (csv) fclose(csv);
	FILE* f = fopen("test/surprise-demo.wav", "wb");
	uint32_t bytes = (uint32_t) wav.size() * 2, rate = (uint32_t) SR, br = rate * 4, riff = 36 + bytes, sixteen = 16;
	uint16_t fmt = 1, ch = 2, al = 4, bits = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sixteen, 4, 1, f);
	fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&al, 2, 1, f);
	fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(wav.data(), 2, wav.size(), f);
	fclose(f);
	printf("wrote test/surprise-demo.wav (%.1f s), %d triggers\n", wav.size() / 2 / SR, trigs);
	for (int b = 1; b <= bars; b++) printf("bar %2d  surprise %.2f\n", b, surpriseBar[b] / std::max(barSamples[b], 1));
	// checks: the loop should become familiar, changes should stand out
	float settled = surpriseBar[7] / barSamples[7], changed = surpriseBar[9] / barSamples[9], back = surpriseBar[14] / barSamples[14];
	float fill = surpriseBar[17] / barSamples[17];
	bool ok = settled < 0.08f && changed > 0.15f && back < changed && fill > settled * 2.f;
	printf("%s  loop settles (%.2f), new pattern stands out (%.2f), the old loop is remembered (%.2f), fill (%.2f)\n",
	       ok ? "PASS" : "FAIL", settled, changed, back, fill);
	return ok ? 0 : 1;
}
