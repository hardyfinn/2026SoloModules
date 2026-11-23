// A short demo piece for Gongoozler (no Rack needed):  make demo
// 0 s     tam-tam: three soft "priming" touches, then one big hit that blooms; a hand grabs it
// 12 s    Peking-opera gongs: the large one falls, the small one rises
// 19.5 s  gamelan: kempul on the beats, the gong ageng (slow ombak beating) at the end of each phrase
// 30 s    wind gong: a roll that swells, one big hit, then a towel (MUTE) is laid on it
#include "../src/GongEngine.hpp"
#if defined(__SSE__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
#include <cstdio>
#include <vector>
#include <algorithm>

using namespace gong;
static const float FS = 48000.f;

struct Voice {
	Engine e;
	Controls c;
	Voice(Controls cc) : c(cc) { e.setSampleRate(FS); }
};

int main(int argc, char** argv) {
#if defined(__SSE__)
	// Rack flushes tiny numbers to zero; do the same here so long tails don't crawl
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
	const char* path = argc > 1 ? argv[1] : "gongoozler-demo.wav";
	const float LEN = 44.f;

	Controls tt; tt.type = TAMTAM; tt.size = 90; tt.thick = 0.45f; tt.beat = 0.4f; tt.position = 0.4f; tt.mallet = 0.75f;
	tt.decay = 0.7f; tt.damp = 0.35f; tt.bloom = 0.75f; tt.build = 0.5f; tt.roar = 0.35f;
	Controls ob; ob.type = OPERA; ob.size = 32; ob.glide = 2.5f; ob.position = 0.1f; ob.mallet = 0.4f; ob.decay = 0.4f; ob.damp = 0.45f;
	ob.bloom = 0.3f; ob.build = 0.3f; ob.roar = 0.2f; ob.tune = -7.f / 12.f; ob.width = 0.5f;
	Controls os = ob; os.size = 22; os.glide = -2.5f; os.mallet = 0.2f; os.decay = 0.3f; os.damp = 0.4f; os.bloom = 0.2f; os.roar = 0.15f; os.tune = 7.f / 12.f;
	Controls ag; ag.type = BOSSED; ag.size = 90; ag.thick = 0.7f; ag.beat = 0.45f; ag.glide = 0.3f; ag.position = 0.f; ag.mallet = 0.85f;
	ag.decay = 0.8f; ag.damp = 0.55f; ag.bloom = 0.2f; ag.build = 0.6f; ag.roar = 0.15f; ag.tune = -2.f;
	Controls kp = ag; kp.size = 50; kp.thick = 0.6f; kp.beat = 0.3f; kp.glide = 0.f; kp.mallet = 0.7f; kp.decay = 0.6f; kp.damp = 0.5f; kp.bloom = 0.15f; kp.roar = 0.1f;
	Controls wg; wg.type = WIND; wg.size = 60; wg.thick = 0.3f; wg.position = 0.55f; wg.mallet = 0.55f; wg.decay = 0.55f; wg.damp = 0.3f;
	wg.bloom = 0.85f; wg.build = 0.3f; wg.roar = 0.45f;

	std::vector<Voice> v = {Voice(tt), Voice(ob), Voice(os), Voice(ag), Voice(kp), Voice(wg)};
	struct Ev { float t; int voice; float vel; float tune; };
	std::vector<Ev> ev;
	// tam-tam
	ev.push_back({0.0f, 0, 0.12f, 0}); ev.push_back({1.1f, 0, 0.16f, 0}); ev.push_back({2.2f, 0, 0.2f, 0});
	ev.push_back({3.4f, 0, 1.25f, 0});
	// opera gongs: 1 = large (falls), 2 = small (rises); a jingju-like pattern
	float t0 = 12.f, b = 0.42f;
	int pat[] = {2, 0, 2, 2, 1, 0, 2, 0, 2, 2, 1, 0, 2, 2, 2, 1};
	float pv[] = {0.7f, 0, 0.5f, 0.8f, 1.f, 0, 0.7f, 0, 0.5f, 0.8f, 1.f, 0, 0.6f, 0.7f, 0.9f, 1.1f};
	for (int i = 0; i < 16; i++) if (pat[i]) ev.push_back({t0 + i * b, pat[i], pv[i], 0});
	// gamelan: kempul on beats 2 and 4 (two pitches), gong ageng at the end of each 8-beat phrase
	float g0 = 19.5f, gb = 0.62f;
	for (int phrase = 0; phrase < 2; phrase++) {
		float p0 = g0 + phrase * 8 * gb;
		ev.push_back({p0 + 1 * gb, 4, 0.6f, -12.f + 5.f}); // F-ish
		ev.push_back({p0 + 3 * gb, 4, 0.65f, -12.f + 7.f});
		ev.push_back({p0 + 5 * gb, 4, 0.6f, -12.f + 3.f});
		ev.push_back({p0 + 7 * gb, 3, 1.0f, -24.f});  // gong ageng closes the phrase
	}
	// wind gong: swelling roll, big hit
	float w0 = 31.f;
	for (int i = 0; i < 28; i++) { float x = i / 27.f; ev.push_back({w0 + x * 3.2f, 5, 0.08f + 0.5f * x * x, 0}); }
	ev.push_back({w0 + 3.45f, 5, 1.3f, 0});

	std::vector<int16_t> out;
	int n = (int) (LEN * FS);
	size_t next = 0;
	std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
	float peak = 0.f;
	for (int i = 0; i < n; i++) {
		float t = i / FS;
		while (next < ev.size() && ev[next].t <= t) {
			Voice& vo = v[ev[next].voice];
			if (ev[next].voice == 4) vo.c.tune = ev[next].tune / 12.f;
			vo.e.trigger(ev[next].vel, vo.c);
			next++;
		}
		v[0].c.hand = t > 11.2f && t < 11.8f;          // hand grabs the tam-tam
		v[5].c.mute = t > 40.5f ? std::min(1.f, (t - 40.5f) * 0.6f) : 0.f;  // towel on the wind gong
		float L = 0.f, R = 0.f;
		const float mix[6] = {0.8f, 0.55f, 0.5f, 0.75f, 0.55f, 0.7f};
		for (int k = 0; k < 6; k++) {
			v[k].e.process(v[k].c);
			L += v[k].e.outL * mix[k];
			R += v[k].e.outR * mix[k];
		}
		float fade = std::min(1.f, (LEN - t) / 1.5f);
		L *= fade * 0.1f; R *= fade * 0.1f;
		peak = std::max(peak, std::max(std::fabs(L), std::fabs(R)));
		out.push_back((int16_t) std::lrint(std::max(-1.f, std::min(1.f, L)) * 32767));
		out.push_back((int16_t) std::lrint(std::max(-1.f, std::min(1.f, R)) * 32767));
	}
	FILE* fp = fopen(path, "wb");
	if (!fp) return 1;
	uint32_t bytes = out.size() * 2;
	auto u32 = [&](uint32_t x) { fwrite(&x, 4, 1, fp); };
	auto u16 = [&](uint16_t x) { fwrite(&x, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); u32(36 + bytes); fwrite("WAVEfmt ", 1, 8, fp);
	u32(16); u16(1); u16(2); u32(48000); u32(192000); u16(4); u16(16);
	fwrite("data", 1, 4, fp); u32(bytes);
	fwrite(out.data(), 2, out.size(), fp);
	fclose(fp);
	printf("wrote %s (%.0f s, peak %.2f of full scale)\n", path, LEN, peak);
	return 0;
}
