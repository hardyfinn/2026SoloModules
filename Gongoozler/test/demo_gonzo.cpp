// A demo tour of Gonzo's starting points (no Rack needed):  make demo
//  0 s   Sinking sun: a bright stick hit that collapses into sub
//  7 s   Glass organ: ORDER -1, pitched, an F minor arpeggio (three Gonzos, one per note)
//  16 s  Bowed sheet, 8 m: BOW swells in and out, no hits
//  25 s  Laser gong: opera type, GLIDE +30
//  30 s  Crotale rain: 6 cm, 15 % of the modes
//  37 s  Whale: 15 m, squeezed modes, bowed and sinking, one hit
//  47 s  Resonator: a little drum loop patched into EXCITE
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

// knob positions (as on the panel) -> engine controls, the same maths as src/Gonzo.cpp
struct Knobs {
	int type = 1; float size = 0.5511f, thick = 0.45f, law = 0.f, order = 0.f, density = 1.f, beat = 0.4f;
	float position = 0.4f, mallet = 0.7f, bow = 0.f, decay = 0.65f, tilt = 0.35f, glide = 0.f, roar = 0.35f;
	float bloom = 0.7f, build = 0.5f, thresh = 0.f, tune = 0.f; bool pitched = false;
	Controls ctl() const {
		Controls c; c.beyond = true;
		c.type = type - 1; c.size = 2.f * std::pow(1000.f, size); c.thick = thick; c.law = std::exp2(law);
		c.order = order; c.density = density; c.beat = beat; c.position = position; c.mallet = mallet; c.bow = bow;
		c.decay = decay; c.freeze = decay >= 1.49f; c.damp = tilt; c.glide = glide; c.roar = roar; c.bloom = bloom;
		c.build = build; c.threshMul = std::pow(10.f, thresh); c.tune = tune / 12.f; c.pitchAll = pitched;
		return c;
	}
};

struct Voice {
	Engine e; Controls c; float gain;
	Voice(Knobs k, float g) : c(k.ctl()), gain(g) { e.setSampleRate(FS); }
};

int main(int argc, char** argv) {
#if defined(__SSE__)
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
	const char* path = argc > 1 ? argv[1] : "gonzo-demo.wav";
	const float LEN = 57.f;

	Knobs sun; sun.mallet = 0.05f; sun.bloom = -0.85f; sun.build = 0.7f; sun.thresh = -1.f; sun.decay = 0.9f; sun.tilt = 0.f;
	Knobs organ; organ.order = -1.f; organ.pitched = true; organ.density = 0.4f; organ.decay = 1.2f; organ.tilt = -0.3f;
	organ.mallet = 0.3f; organ.bloom = 0.2f; organ.roar = 0.1f; organ.beat = 0.15f;
	Knobs sheet; sheet.size = 0.867f; sheet.bow = 0.f; sheet.decay = 1.1f; sheet.roar = 1.2f; sheet.bloom = 0.6f; sheet.build = 1.0f;
	Knobs laser; laser.type = 3; laser.glide = 30.f; laser.mallet = -0.3f; laser.decay = 0.25f; laser.position = 0.05f; laser.bloom = 0.3f; laser.roar = 0.2f;
	Knobs crot; crot.size = 0.159f; crot.density = 0.15f; crot.law = 0.68f; crot.tilt = -0.5f; crot.mallet = 0.1f; crot.decay = 0.9f; crot.bloom = 0.2f;
	Knobs whale; whale.size = 0.958f; whale.law = -1.f; whale.bow = 0.f; whale.bloom = -0.5f; whale.decay = 1.1f; whale.roar = 0.9f;
	Knobs reso; reso.decay = 0.45f; reso.bloom = 0.3f; reso.position = 0.6f; reso.roar = 0.2f; reso.tilt = 0.5f;

	std::vector<Voice> v;
	v.emplace_back(sun, 0.4f);                                        // 0
	v.emplace_back(organ, 0.45f); v.emplace_back(organ, 0.45f); v.emplace_back(organ, 0.45f);  // 1-3
	v.emplace_back(sheet, 0.8f);                                      // 4
	v.emplace_back(laser, 0.5f);                                      // 5
	v.emplace_back(crot, 0.55f);                                      // 6
	v.emplace_back(whale, 0.8f);                                      // 7
	v.emplace_back(reso, 0.7f);                                       // 8

	struct Ev { float t; int voice; float vel; float tune; float pos; };
	std::vector<Ev> ev;
	ev.push_back({0.2f, 0, 1.2f, 0, -1});
	// F minor arpeggio, semitones from C4: F3 Ab3 C4 Eb4 F4 Eb4 C4 Ab3 ...
	const float arp[] = {-7, -4, 0, 3, 5, 3, 0, -4, -7, -4, 0, 3, 8, 7, 3, 0};
	for (int i = 0; i < 16; i++) ev.push_back({7.f + i * 0.5f, 1 + i % 3, 0.45f + 0.25f * (i % 4 == 0), arp[i], -1});
	for (int i = 0; i < 8; i++) ev.push_back({25.f + i * 0.55f, 5, 0.6f + 0.4f * (i % 2), (float) ((i * 5) % 12 - 6), -1});
	Rng rr; rr.seed(5);
	for (int i = 0; i < 40; i++) ev.push_back({30.f + 6.5f * rr.uni(), 6, 0.2f + 0.6f * rr.uni(), 12.f * (rr.uni() - 0.5f), rr.uni()});
	ev.push_back({41.f, 7, 1.f, 0, -1});
	std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });

	std::vector<int16_t> out;
	int n = (int) (LEN * FS);
	size_t next = 0;
	float peak = 0.f;
	float kickPh = 0.f, kickEnv = 0.f, hatEnv = 0.f;
	Rng hatRng; hatRng.seed(9);
	for (int i = 0; i < n; i++) {
		float t = i / FS;
		while (next < ev.size() && ev[next].t <= t) {
			Voice& vo = v[ev[next].voice];
			vo.c.tune = ev[next].tune / 12.f;
			if (ev[next].pos >= 0.f) vo.c.position = ev[next].pos;
			vo.e.trigger(ev[next].vel, vo.c);
			next++;
		}
		// bowed sheet: swell in and out between 16 and 25 s
		{
			float x = (t - 16.f) / 9.f;
			v[4].c.bow = (x > 0.f && x < 1.f) ? 0.55f * std::sin(PI * x) : 0.f;
		}
		// whale: bowed under its hit, 37 .. 47 s
		{
			float x = (t - 37.f) / 10.f;
			v[7].c.bow = (x > 0.f && x < 1.f) ? 0.35f * std::sin(PI * x) : 0.f;
		}
		// resonator: a 124 bpm kick + off-beat hat into EXCITE, 47 .. 55 s
		if (t > 47.f && t < 55.f) {
			float beat = (t - 47.f) * 124.f / 60.f;
			float ph = beat - std::floor(beat);
			if (ph < 1.f / FS * 124.f / 60.f + 1e-6f) { kickEnv = 1.f; kickPh = 0.f; }
			float hph = beat + 0.5f - std::floor(beat + 0.5f);
			if (hph < 1.f / FS * 124.f / 60.f + 1e-6f) hatEnv = 0.6f;
			kickPh += (45.f + 120.f * kickEnv * kickEnv) / FS;
			float kick = std::sin(2.f * PI * kickPh) * kickEnv;
			kickEnv *= 0.99975f;
			float hat = hatRng.bi() * hatEnv;
			hatEnv *= 0.997f;
			v[8].c.excite = 0.5f * kick + 0.25f * hat;  // volts * 0.1
		} else {
			v[8].c.excite = 0.f;
		}
		// each section is grabbed by hand when the next one starts, so the tour stays clear
		const float grabAt[9] = {7.5f, 16.5f, 16.5f, 16.5f, 26.f, 30.5f, 38.f, 47.5f, 56.f};
		for (int k = 0; k < 9; k++) v[k].c.hand = t > grabAt[k] && t < grabAt[k] + 0.6f;
		float L = 0.f, R = 0.f;
		for (auto& vo : v) {
			vo.e.process(vo.c);
			L += vo.e.outL * vo.gain;
			R += vo.e.outR * vo.gain;
		}
		if (t > 47.f && t < 55.f) { L += v[8].c.excite * 3.f; R += v[8].c.excite * 3.f; }  // a little of the dry loop
		float fade = std::min(1.f, (LEN - t) / 2.f);
		L *= fade * 0.08f; R *= fade * 0.08f;
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
