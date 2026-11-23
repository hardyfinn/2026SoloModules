// Offline check of the Lilt engine + a demo WAV (no Rack needed).
//   make test   (writes lilt-demo.wav in the folder given, default .)
// A steady kick plays the pulse; the four Lilt trigger channels play a hat,
// a rim, a tom and a low accent, so you can hear them lean against the beat.
// The demo walks through four 8-bar sections:
//   1 strict ratios (DRIFT 0)   2 DRIFT 35 %   3 + MUTATE 60 %   4 + GRID 70 %
#include "../src/LiltDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>

using namespace lilt;

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return;
	uint32_t n = (uint32_t) L.size(), data = n * 4, riff = 36 + data, sr32 = sr, br = sr * 4, fmtLen = 16;
	uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	fwrite(&fmtLen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr32, 4, 1, f);
	fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) (std::max(-1.f, std::min(1.f, L[i])) * 32000.f);
		int16_t b = (int16_t) (std::max(-1.f, std::min(1.f, R[i])) * 32000.f);
		fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
	}
	fclose(f);
}

struct Voice {
	float env = 0.f, ph = 0.f, decay = 0.1f, freq = 200.f, drop = 0.f, noise = 0.f, amp = 0.f, pan = 0.5f;
	Rng r;
	void hit(float a) { env = 1.f; ph = 0.f; amp = a; }
	float tick(float dt) {
		if (env < 1e-4f) return 0.f;
		float f = freq * (1.f + drop * env * env);
		ph += f * dt;
		float s = std::sin(TWO_PI * ph) * (1.f - noise) + r.bi() * noise;
		float out = s * env * amp;
		env *= std::exp(-dt / decay);
		return out;
	}
};

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const int sr = 48000;
	const float dt = 1.f / sr, bpm = 124.f;
	const int barsPerSection = 8, sections = 4;
	const int total = (int) (sr * 60.f / bpm * 4 * barsPerSection * sections);
	std::vector<float> L(total), R(total);

	RelabiEngine eng;
	RelabiControls c;
	RelabiFrame fr;
	c.bpm = bpm;
	c.thresh = 0.f;
	c.spread = 0.55f;

	Voice kick; kick.freq = 48.f; kick.drop = 3.f; kick.decay = 0.16f; kick.pan = 0.5f;
	Voice v[4];
	v[0].freq = 7000.f; v[0].noise = 0.95f; v[0].decay = 0.025f; v[0].pan = 0.7f;   // hat
	v[1].freq = 1700.f; v[1].noise = 0.25f; v[1].decay = 0.03f;  v[1].pan = 0.3f;   // rim
	v[2].freq = 160.f;  v[2].drop = 0.6f;   v[2].decay = 0.18f;  v[2].pan = 0.62f;  // tom
	v[3].freq = 92.f;   v[3].drop = 1.2f;   v[3].noise = 0.1f; v[3].decay = 0.3f; v[3].pan = 0.4f;  // low accent
	const float gain[4] = {0.22f, 0.3f, 0.45f, 0.5f};

	int counts[sections][NUM_CH] = {};
	int onGrid[sections] = {}, allTrig[sections] = {};
	float sumMin = 9, sumMax = -9;
	const int secLen = total / sections;
	for (int i = 0; i < total; i++) {
		int s = i / secLen;
		c.drift = s == 0 ? 0.f : 0.35f;
		c.mutate = s >= 2 ? 0.6f : 0.f;
		c.grid = s == 3 ? 0.7f : 0.f;
		eng.process(dt, c, false, fr);
		sumMin = std::min(sumMin, fr.sum); sumMax = std::max(sumMax, fr.sum);
		if (fr.pulse) kick.hit(0.8f);
		for (int k = 0; k < NUM_CH; k++) {
			if (!fr.trig[k]) continue;
			v[k].hit(gain[k] * (0.45f + 0.55f * fr.acc));
			counts[s][k]++;
			allTrig[s]++;
			float q = eng.beat * GRID_DIV;
			q -= std::floor(q);
			if (q < 0.002f || q > 0.998f) onGrid[s]++;
		}
		float k0 = kick.tick(dt);
		float l = k0 * 0.5f, r = k0 * 0.5f;
		for (int k = 0; k < NUM_CH; k++) {
			float x = v[k].tick(dt);
			l += x * (1.f - v[k].pan);
			r += x * v[k].pan;
		}
		L[i] = std::tanh(l * 1.2f) * 0.8f;
		R[i] = std::tanh(r * 1.2f) * 0.8f;
	}
	printf("sum range %.3f .. %.3f\n", sumMin, sumMax);
	const char* names[sections] = {"strict", "drift", "drift+mutate", "drift+mutate+grid"};
	for (int s = 0; s < sections; s++)
		printf("%-18s trigs %3d %3d %3d %3d   on 1/16 grid %d of %d\n", names[s], counts[s][0], counts[s][1], counts[s][2], counts[s][3], onGrid[s], allTrig[s]);
	writeWav(dir + "/lilt-demo.wav", L, R, sr);
	printf("wrote %s/lilt-demo.wav\n", dir.c_str());
	return 0;
}
