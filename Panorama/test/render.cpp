// Offline checks + demo WAV for the Panorama engine (no Rack needed).
//   make test   ->  ./test/render <outdir>
// Writes panorama-demo.wav and prints peak levels and a stability check.
#include "../src/PanoramaDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>

using namespace panorama;

static const float FS = 48000.f;

struct Wav {
	std::vector<float> l, r;
	void write(const std::string& path) {
		FILE* f = fopen(path.c_str(), "wb");
		uint32_t n = l.size(), data = n * 4, riff = 36 + data, fmt = 16, sr = 48000, br = 48000 * 4;
		uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
		fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
		fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
		fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
		fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
		for (uint32_t i = 0; i < n; i++) {
			int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, l[i] / 10.f * 32767.f));
			int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, r[i] / 10.f * 32767.f));
			fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
		}
		fclose(f);
	}
};

struct Ev { float t; int midi; float force; float hold; };

// play a list of events; hold > 0 means a roll for that long
static float play(PanEngine& e, Wav& w, const std::vector<Ev>& evs, float len, float rollHz = 0.f) {
	int n = (int) (len * FS);
	size_t next = 0;
	struct Roll { int note; float until, nextT, force; int alt; };
	std::vector<Roll> rolls;
	Rng jit(77);
	float peak = 0.f;
	for (int s = 0; s < n; s++) {
		float t = s / FS;
		while (next < evs.size() && evs[next].t <= t) {
			const Ev& v = evs[next++];
			int i = e.lay.nearest((float) v.midi);
			e.strike(i, v.force);
			if (v.hold > 0.f && rollHz > 0.f) rolls.push_back({i, t + v.hold, t + 1.f / rollHz, v.force * 0.7f, 0});
		}
		for (auto& r : rolls) {
			if (r.note < 0) continue;
			if (t >= r.until) { r.note = -1; continue; }
			if (t >= r.nextT) {
				r.alt ^= 1;
				e.strike(r.note, r.force * (r.alt ? 0.82f : 1.f) * (0.95f + 0.1f * jit.uni()));
				r.nextT += (1.f / rollHz) * (0.92f + 0.16f * jit.uni());
			}
		}
		float L, R;
		e.process(L, R);
		w.l.push_back(L);
		w.r.push_back(R);
		peak = std::max(peak, std::max(std::fabs(L), std::fabs(R)));
	}
	return peak;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	Wav w;
	PanEngine e;
	e.setSampleRate(FS);
	e.outGain = 6.5f;
	Controls c;

	// single hits for level calibration
	{
		PanEngine t;
		t.setSampleRate(FS);
		t.outGain = 6.5f;
		Wav tmp;
		for (int ty = 0; ty < NUM_TYPES; ty++) {
			Controls cc;
			cc.type = ty;
			t.update(cc);
			t.reset();
			int lo = TYPES[ty].lowMidi, hi = TYPES[ty].highMidi;
			float pk = play(t, tmp, {{0.f, lo, 1.f, 0.f}}, 1.5f);
			float pk2 = play(t, tmp, {{0.f, (lo + hi) / 2, 1.f, 0.f}}, 1.5f);
			float pk3 = play(t, tmp, {{0.f, hi, 1.f, 0.f}}, 1.5f);
			printf("%-14s full hit peaks: low %.2f V  mid %.2f V  high %.2f V\n", TYPE_NAMES[ty], pk, pk2, pk3);
		}
	}

	// 1. tenor melody (D major-ish calypso line), a roll at the end
	c.type = TENOR;
	e.update(c);
	e.reset();
	std::vector<Ev> mel;
	const int line[] = {74, 76, 78, 81, 78, 76, 74, 71, 74, 76, 74, 69, 71, 74, 76, 78};
	const float dur[] = {.25f, .25f, .5f, .25f, .25f, .5f, .25f, .25f, .25f, .25f, .5f, .25f, .25f, .25f, .25f, 1.f};
	float t = 0.2f;
	for (int i = 0; i < 16; i++) {
		mel.push_back({t, line[i], 0.6f + 0.3f * ((i % 4) == 0), i == 15 ? 2.2f : 0.f});
		t += dur[i] * 0.5f * 60.f / 60.f;
	}
	float pk = play(e, w, mel, t + 3.2f, 14.f);
	printf("tenor melody peak %.2f V\n", pk);

	// 2. same phrase with perfect tuning, then very rough tuning
	for (int pass = 0; pass < 2; pass++) {
		c.detune = pass == 0 ? 0.f : 14.f;
		c.partials = pass == 0 ? 0.f : 30.f;
		e.update(c);
		std::vector<Ev> ph;
		const int notes[] = {62, 66, 69, 74, 69, 66};
		for (int i = 0; i < 6; i++) ph.push_back({0.1f + i * 0.22f, notes[i], 0.75f, 0.f});
		ph.push_back({1.5f, 62, 0.8f, 0.f}); ph.push_back({1.5f, 66, 0.7f, 0.f}); ph.push_back({1.5f, 69, 0.7f, 0.f});
		pk = play(e, w, ph, 3.6f);
		printf("tuning pass %d peak %.2f V\n", pass, pk);
	}
	c.detune = 4.f;
	c.partials = 8.f;

	// 3. double seconds strumming chords with rolls
	c.type = DOUBLE_SECOND;
	e.update(c);
	std::vector<Ev> ch;
	const int chords[4][3] = {{62, 66, 69}, {59, 62, 66}, {55, 59, 62}, {57, 61, 64}};
	for (int k = 0; k < 4; k++)
		for (int j = 0; j < 3; j++) ch.push_back({0.1f + k * 1.0f + j * 0.03f, chords[k][j], 0.55f, 0.85f});
	pk = play(e, w, ch, 5.4f, 12.f);
	printf("double second chords peak %.2f V\n", pk);

	// 4. cello bass line, soft stick, edge hits
	c.type = CELLO;
	c.stick = 0.25f;
	c.position = 0.55f;
	e.update(c);
	std::vector<Ev> bass;
	const int bl[] = {50, 50, 57, 50, 55, 55, 62, 57, 50};
	for (int i = 0; i < 9; i++) bass.push_back({0.1f + i * 0.375f, bl[i], 0.85f, 0.f});
	pk = play(e, w, bass, 5.0f);
	printf("cello bass peak %.2f V\n", pk);

	// 5. tenor, NEIGHBOURS and BLOOM high: one hard hit, listen to the shimmer
	c.type = TENOR;
	c.stick = 0.8f;
	c.position = 0.5f;
	c.neighbours = 1.f;
	c.bloom = 1.f;
	c.glide = -15.f;
	e.update(c);
	pk = play(e, w, {{0.1f, 67, 1.1f, 0.f}, {1.6f, 72, 1.1f, 0.f}}, 4.5f);
	printf("shimmer peak %.2f V\n", pk);
	w.write(dir + "/panorama-demo.wav");

	// stability: 30 s of random hard hits at max coupling and bloom, all types
	bool ok = true;
	for (int ty = 0; ty < NUM_TYPES; ty++) {
		PanEngine s;
		s.setSampleRate(44100.f);
		s.outGain = 6.5f;
		Controls cc;
		cc.type = ty;
		cc.neighbours = 1.f;
		cc.bloom = 1.f;
		cc.decay = 2.5f;
		cc.glide = 30.f;
		cc.detune = 30.f;
		cc.partials = 50.f;
		s.update(cc);
		Rng r(ty + 1);
		float big = 0.f;
		for (int i = 0; i < 44100 * 30; i++) {
			if (r.uni() < 0.0006f) s.strike((int) (r.uni() * s.lay.count), 1.25f);
			float L, R;
			s.process(L, R);
			if (!std::isfinite(L) || !std::isfinite(R)) { ok = false; break; }
			big = std::max(big, std::fabs(L));
		}
		printf("stability %-14s peak %.2f V %s\n", TYPE_NAMES[ty], big, ok ? "ok" : "NaN!");
	}
	return ok ? 0 : 1;
}
