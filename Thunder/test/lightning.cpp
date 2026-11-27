// Offline checks for Lightning's capture + playback, fed by the real Thunder engine. No Rack needed.
//   make test   (also writes test/out/lightning-*.wav)
#include "../src/ThunderEngine.hpp"
#include "../src/ThunderPresets.hpp"
#include "../src/LightningCore.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace thunder;
using namespace lightning;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static void wav(const std::string& path, const std::vector<float>& x, float gain = 0.1f) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	int n = x.size(), sr = (int) SR, bytes = n * 2;
	auto w32 = [&](int v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](short v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(1); w32(sr); w32(sr * 2); w16(2); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (float v : x) { float s = std::max(-1.f, std::min(1.f, v * gain)); w16((short) (s * 32767)); }
	std::fclose(f);
}

static Controls preset(int i, float* vals = nullptr) {
	Controls c;
	valuesToControls(factoryPreset(i).v, c);
	c.level = 0.8f;
	if (vals) for (int k = 0; k < NUM_VALS; k++) vals[k] = factoryPreset(i).v[k];
	return c;
}

// one isolated hit, rendered until silent (reference)
static std::vector<float> isolated(int p) {
	Engine e;
	e.setSampleRate(SR);
	Controls c = preset(p);
	c.wear = 0.f;
	e.hit(0.f, c);
	std::vector<float> o;
	for (int i = 0; i < (int) (SR * 9); i++) { e.process(c); o.push_back(e.out); }
	return o;
}

static double energy(const std::vector<float>& x, int n = -1) {
	double s = 0;
	if (n < 0) n = x.size();
	for (int i = 0; i < n && i < (int) x.size(); i++) s += (double) x[i] * x[i];
	return s;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "test/out";

	// 1. one Thunder hit, captured the way Lightning sees it (exact start from Thunder)
	for (int p : {0, 3, 4, 6}) {
		Engine e;
		e.setSampleRate(SR);
		Controls c = preset(p);
		c.wear = 0.f;
		Recorder r;
		r.setSampleRate(SR);
		float vals[NUM_VALS];
		preset(p, vals);
		for (int i = 0; i < 4800; i++) { e.process(c); r.push(e.out, false, 0.f); } // silence first
		e.hit(0.f, c);
		r.startHit(r.w, true, vals, quietForThunder(vals));
		int guard = 0;
		std::vector<float> ref; // everything Thunder played from the hit on
		while (!r.haveDone && guard++ < (int) (SR * 9)) { e.process(c); r.push(e.out, false, 0.f); ref.push_back(e.out); }
		Clip clip;
		clip.data.resize((size_t) (MAX_SEC * SR));
		clip.len = r.copyDone(clip.data);
		for (int i = 0; i < (int) (SR * 3); i++) { e.process(c); ref.push_back(e.out); }
		double eRef = energy(ref), eCap = energy(clip.data, clip.len);
		float pk = 0.f;
		for (int i = 0; i < clip.len; i++) pk = std::max(pk, std::fabs(clip.data[i]));
		float tail = 0.f;
		for (int i = std::max(0, clip.len - (int) (0.01f * SR)); i < clip.len; i++) tail = std::max(tail, std::fabs(clip.data[i]));
		CHECK(r.haveDone && clip.len > 0, "%-12s kept: %.0f ms long", factoryPreset(p).name, clip.len * 1000.f / SR);
		CHECK(eCap / eRef > 0.999, "%-12s nothing cut off (%.4f%% of the energy)", factoryPreset(p).name, 100.0 * eCap / eRef);
		CHECK(tail < pk * 0.003f, "%-12s trimmed tail is silent (last 10 ms %.1f dB under peak)", factoryPreset(p).name, 20 * std::log10(std::max(tail, 1e-9f) / pk));
		int firstLoud = 0;
		while (firstLoud < clip.len && std::fabs(clip.data[firstLoud]) < pk * 0.01f) firstLoud++;
		CHECK(firstLoud < (int) (0.003f * SR), "%-12s no silence in front (%.2f ms)", factoryPreset(p).name, firstLoud * 1000.f / SR);
		CHECK(std::fabs(r.doneVals[4] - vals[4]) < 1e-6f, "%-12s Thunder's settings stored with it", factoryPreset(p).name);
	}

	// 2. hits that overlap: each kept hit ends where the next begins
	{
		Engine e;
		e.setSampleRate(SR);
		Controls c = preset(2); // Boom, long decay
		Recorder r;
		r.setSampleRate(SR);
		int gap = (int) (0.25f * SR);
		uint32_t serials = 0;
		int64_t lastLen = 0;
		for (int h = 0; h < 4; h++) {
			e.hit(0.f, c);
			r.startHit(r.w, true, nullptr);
			for (int i = 0; i < gap; i++) { e.process(c); r.push(e.out, false, 0.f); }
			if (r.doneSerial != serials) { serials = r.doneSerial; lastLen = r.doneEnd - r.doneStart; }
		}
		CHECK(serials == 3 && lastLen == gap, "overlapping hits: %u finished, each %.0f ms (= the gap)", serials, lastLen * 1000.f / SR);
	}

	// 3. audio IN: hits found by level, even on top of a ringing tail; trimmed start is tight
	{
		Engine e;
		e.setSampleRate(SR);
		Recorder r;
		r.setSampleRate(SR);
		int maxErr = 0, clipsChecked = 0;
		int presets[6] = {0, 4, 6, 5, 3, 7};
		for (int h = 0; h < 6; h++) {
			Controls c = preset(presets[h]);
			e.hit(0.f, c);
			uint32_t before = r.doneSerial;
			for (int i = 0; i < (int) (0.6f * SR); i++) { e.process(c); r.push(e.out, true, 0.2f); }
			(void) before;
		}
		// one more hit closes the last one; then check every hit's trimmed start
		CHECK(r.startCount == 6, "audio IN: %u of 6 hits found by level (each lands on the tail of the one before)", r.startCount);
		Engine e2;
		e2.setSampleRate(SR);
		for (int h = 0; h < 6; h++) {
			Recorder r2;
			r2.setSampleRate(SR);
			Controls c = preset(presets[h]);
			for (int i = 0; i < 2400; i++) r2.push(0.f, true, 0.2f);
			e2.reset();
			e2.hit(0.f, c);
			for (int i = 0; i < (int) (9.f * SR) && !r2.haveDone; i++) { e2.process(c); r2.push(e2.out, true, 0.2f); }
			if (!r2.haveDone) continue;
			Clip clip;
			clip.data.resize((size_t) (MAX_SEC * SR));
			clip.len = r2.copyDone(clip.data);
			int64_t offset = r2.doneStart - 2400; // where the kept clip begins vs the true start (before trimming)
			float pk = 0.f;
			for (int i = 0; i < clip.len; i++) pk = std::max(pk, std::fabs(clip.data[i]));
			int fl = 0;
			while (fl < clip.len && std::fabs(clip.data[fl]) < pk * 0.01f) fl++;
			(void) offset;
			maxErr = std::max(maxErr, fl);
			clipsChecked++;
		}
		CHECK(clipsChecked == 6 && maxErr < (int) (0.0015f * SR), "audio IN: %d clips, each starts %.2f ms before the sound at most", clipsChecked, maxErr * 1000.f / SR);
	}

	// 4. playback: TUNE 0 plays the clip back exactly; +12 is twice as fast; retrigger does not click
	{
		std::vector<float> ref = isolated(3);
		Clip clips[2];
		clips[0].data.assign(ref.begin(), ref.begin() + (int) (0.5f * SR));
		clips[0].len = clips[0].data.size();
		clips[0].rate = SR;
		SlotPlayer pl;
		pl.trigger(0);
		float err = 0.f;
		for (int i = 0; i < clips[0].len; i++) err = std::max(err, std::fabs(pl.process(clips, SR, 0.f, 1.f) - clips[0].data[i]));
		CHECK(err < 1e-5f, "TUNE 0 plays the kept hit back unchanged (max error %.2g V)", err);
		pl.trigger(0);
		int n = 0;
		while (pl.v[0].active || pl.v[1].active) { pl.process(clips, SR, 12.f, 1.f); n++; }
		CHECK(std::abs(n - clips[0].len / 2) < 4, "TUNE +12 plays an octave up (%d vs %d samples)", n, clips[0].len / 2);
		// a clip recorded at 96 kHz plays at the right speed in a 48 kHz engine
		Clip hi[2];
		hi[0].data.assign(1000, 0.1f);
		hi[0].len = 1000;
		hi[0].rate = 96000.f;
		SlotPlayer ph;
		ph.trigger(0);
		n = 0;
		while (ph.v[0].active) { ph.process(hi, SR, 0.f, 1.f); n++; }
		CHECK(std::abs(n - 500) < 3, "96 kHz sound in a 48 kHz patch keeps its pitch (%d samples for 1000)", n);
		// retrigger mid-sound: the old voice fades over 3 ms
		SlotPlayer pr;
		pr.trigger(0);
		float prev = 0.f, jump = 0.f;
		for (int i = 0; i < (int) (0.2f * SR); i++) {
			if (i == (int) (0.05f * SR)) pr.trigger(0);
			float y = pr.process(clips, SR, 0.f, 1.f);
			if (i > 10) jump = std::max(jump, std::fabs(y - prev));
			prev = y;
		}
		float pk = 0.f;
		for (int i = 0; i < clips[0].len; i++) pk = std::max(pk, std::fabs(clips[0].data[i]));
		float natural = 0.f;
		for (int i = 11; i < clips[0].len; i++) natural = std::max(natural, std::fabs(clips[0].data[i] - clips[0].data[i - 1]));
		CHECK(jump <= natural * 1.6f + 0.05f, "retrigger is smooth (biggest step %.3f V, the sound's own %.3f V)", jump, natural);
		// DECAY shortens
		SlotPlayer pd;
		pd.trigger(0);
		double eFull = 0, eShort = 0;
		for (int i = 0; i < clips[0].len; i++) { float y = pd.process(clips, SR, 0.f, 1.f); eFull += y * y; }
		pd.trigger(0);
		for (int i = 0; i < clips[0].len; i++) { float y = pd.process(clips, SR, 0.f, 0.2f); eShort += y * y; }
		CHECK(eShort < eFull * 0.7, "DECAY down shortens the sound (%.0f%% of the energy)", 100.0 * eShort / eFull);
	}

	// 5. demo: MUTATE wanders FM Perc, Lightning keeps 4 of the hits, then plays them as a kit
	{
		Engine e;
		e.setSampleRate(SR);
		Controls c = preset(3);
		c.mutate = 0.9f;
		Recorder r;
		r.setSampleRate(SR);
		std::vector<float> audition;
		Clip clips[4][2];
		int kept = 0;
		int keepAt[4] = {2, 5, 9, 13}; // the hits we "liked"
		int gap = (int) (0.35f * SR);
		for (int h = 0; h < 16; h++) {
			e.hit(0.f, c);
			r.startHit(r.w, true, nullptr, 0.08f);
			if (kept < 4 && h == keepAt[kept] + 1) { // KEEP pressed while the liked hit rang: it lands when the next begins
				clips[kept][0].data.resize((size_t) (MAX_SEC * SR));
				clips[kept][0].len = r.copyDone(clips[kept][0].data);
				clips[kept][0].rate = SR;
				kept++;
			}
			for (int i = 0; i < gap; i++) { e.process(c); r.push(e.out, false, 0.f); audition.push_back(e.out); }
		}
		CHECK(kept == 4, "demo: kept 4 hits from a mutating Thunder");
		wav(dir + "/lightning-1-auditioning.wav", audition);
		// a 2-bar groove from the 4 slots, slot 2 tuned down 5, slot 4 up 7
		const char* lanes[4] = {"x.....x...x.....", "....x.......x..x", "..x...x.x.x...x.", ".x.x.x.x.x.x.x.x"};
		float tune[4] = {0.f, -5.f, 0.f, 7.f};
		float decay[4] = {1.f, 1.f, 0.55f, 0.35f};
		float level[4] = {1.f, 0.9f, 0.7f, 0.5f};
		SlotPlayer pl[4];
		std::vector<float> o;
		int step = (int) (SR * 60.f / 118.f / 4.f);
		for (int s = 0; s < 64; s++) {
			for (int k = 0; k < 4; k++)
				if (lanes[k][s % 16] == 'x') pl[k].trigger(0);
			for (int i = 0; i < step; i++) {
				float m = 0.f;
				for (int k = 0; k < 4; k++) m += pl[k].process(clips[k], SR, tune[k], decay[k]) * level[k];
				o.push_back(m);
			}
		}
		wav(dir + "/lightning-2-kept-kit-groove.wav", o, 0.08f);
	}

	std::printf(fails ? "%d checks FAILED\n" : "all Lightning checks passed (0 failed)\n", fails);
	return fails ? 1 : 0;
}
