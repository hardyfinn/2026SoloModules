// Renders demo WAVs with the real engines and very plain synth voices:
//   make test   ->  test/sprout-demo.wav, graft-demo.wav, bloom-demo.wav, together-demo.wav
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../src/LSys.hpp"

using namespace lsys;

static const float SR = 44100.f;
static const float BPM = 124.f;
static const float STEP = 60.f / BPM / 4.f;   // one 16th

struct Noise { Rng r{99}; float next() { return r.uni() * 2.f - 1.f; } };

// ---- drums
struct Kick {
	float t = 1e9f, ph = 0.f, amp = 1.f;
	void hit(float a = 1.f) { t = 0.f; ph = 0.f; amp = a; }
	float run(float dt) {
		if (t > 1.f) return 0.f;
		float f = 48.f + 110.f * std::exp(-t * 28.f);
		ph += f * dt;
		float v = std::sin(6.2831853f * ph) * std::exp(-t * 6.5f) + 0.15f * std::exp(-t * 400.f);
		t += dt;
		return amp * std::tanh(1.6f * v);
	}
};
struct NoiseHit {
	float t = 1e9f, decay, hp = 0.f, lp = 0.f, prev = 0.f, tone, toneAmt, amp = 1.f, cut, ph = 0.f;
	Noise n;
	NoiseHit(float d, float c, float tn, float ta) : decay(d), tone(tn), toneAmt(ta), cut(c) {}
	void hit(float a = 1.f) { t = 0.f; amp = a; }
	float run(float dt) {
		if (t > 1.5f) return 0.f;
		float x = n.next();
		lp += (x - lp) * cut;           // one-pole low pass
		float hpOut = x - lp;           // what's left is the bright part
		ph += tone * dt;
		float v = (hpOut * (1.f - toneAmt) + toneAmt * std::sin(6.2831853f * ph)) * std::exp(-t / decay);
		t += dt;
		return amp * v;
	}
};

// ---- pad and bass for Bloom
struct Voice {
	float freq = 220.f, ph1 = 0.f, ph2 = 0.3f, lp = 0.f, env = 0.f;
	float run(float dt, bool gate, float cutoff) {
		env += ((gate ? 1.f : 0.f) - env) * (gate ? 0.0009f : 0.0004f);
		ph1 += freq * dt; ph2 += freq * 1.006f * dt;
		ph1 -= std::floor(ph1); ph2 -= std::floor(ph2);
		float saw = (2.f * ph1 - 1.f) + (2.f * ph2 - 1.f);
		float a = 1.f - std::exp(-6.2831853f * cutoff * dt);
		lp += (saw - lp) * a;
		return lp * env * 0.18f;
	}
};
struct Bass {
	float freq = 55.f, ph = 0.f, lp = 0.f, lp2 = 0.f, t = 1e9f;
	void hit() { t = 0.f; }
	float run(float dt) {
		ph += freq * dt; ph -= std::floor(ph);
		float saw = 2.f * ph - 1.f;
		float cut = 180.f + 900.f * std::exp(-t * 9.f);
		float a = 1.f - std::exp(-6.2831853f * cut * dt);
		lp += (saw - lp) * a; lp2 += (lp - lp2) * a;
		float e = std::exp(-t * 2.2f);
		t += dt;
		return lp2 * e * 0.55f;
	}
};

struct Wav {
	std::vector<int16_t> d;
	void add(float l, float r) {
		auto s = [](float x) { x = std::tanh(x); return (int16_t) (x * 30000.f); };
		d.push_back(s(l)); d.push_back(s(r));
	}
	void write(const char* path) {
		FILE* f = fopen(path, "wb");
		if (!f) { printf("can't write %s\n", path); return; }
		uint32_t dataBytes = (uint32_t) d.size() * 2, rate = (uint32_t) SR, byteRate = rate * 4;
		uint16_t ch = 2, bits = 16, align = 4, fmt = 1;
		uint32_t riff = 36 + dataBytes, sixteen = 16;
		fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
		fwrite(&sixteen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f);
		fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
		fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f); fwrite(d.data(), 2, d.size(), f);
		fclose(f);
		printf("wrote %s (%.1f s)\n", path, d.size() / 2 / SR);
	}
};

static float hz(float volts) { return 261.6256f * std::pow(2.f, volts); }

// A clock: true on the first sample of each 16th.
struct Clock {
	double t = 0.0; long n = -1;
	bool tick(float dt) {
		long k = (long) std::floor(t / STEP);
		t += dt;
		if (k != n) { n = k; return true; }
		return false;
	}
};

// One scene of the demo: which modules play and how they are set.
struct Scene { int bars; int sproutRule; bool sproutStream; int graftGroove; int bloomRule; int bloomMode; };

static void render(const char* path, const std::vector<Scene>& scenes, bool useSprout, bool useGraft, bool useBloom,
                   bool refKick) {
	const float dt = 1.f / SR;
	Wav w;
	ClockTracker ct;
	ct.period = STEP;
	ct.have = true;
	Sprout sp; sp.grow = 2; sp.startGen = 1;
	Graft gr; gr.every = 1;
	Bloom bl; bl.length = 8; bl.startGen = 1; bl.key = 5;
	Kick kick; NoiseHit snare(0.16f, 0.25f, 190.f, 0.35f), hat(0.035f, 0.6f, 0.f, 0.f), perc(0.05f, 0.05f, 820.f, 0.7f);
	NoiseHit tick(0.02f, 0.7f, 0.f, 0.f), rim(0.03f, 0.08f, 1650.f, 0.6f), clap(0.11f, 0.2f, 0.f, 0.f);
	Voice pad[4]; Bass bass;
	for (const Scene& sc : scenes) {
		sp.rule = sc.sproutRule; sp.stream = sc.sproutStream; sp.reset();
		gr.groove = sc.graftGroove; gr.loadSeed(); gr.reset();
		bl.rule = sc.bloomRule; bl.mode = sc.bloomMode; bl.reset();
		Clock clock;
		long samples = (long) (sc.bars * 16 * STEP * SR);
		for (long i = 0; i < samples; i++) {
			bool edge = clock.tick(dt);
			ct.tick(dt);
			if (edge) ct.edge();
			float L = 0.f, R = 0.f;
			if (refKick && edge && clock.n % 4 == 0) kick.hit(0.8f);
			if (useSprout) {
				sp.process(edge, ct);
				if (sp.fired[0]) tick.hit(0.55f);
				if (sp.fired[1]) rim.hit(0.45f);
				if (sp.fired[2]) clap.hit(0.5f);
			}
			if (useGraft) {
				gr.process(edge, ct);
				if (gr.fired[0]) kick.hit(1.f);
				if (gr.fired[1]) snare.hit(0.6f);
				if (gr.fired[2]) hat.hit(0.35f);
				if (gr.fired[3]) perc.hit(0.35f);
			}
			float k = kick.run(dt) * 0.9f;
			L += k; R += k;
			float s = snare.run(dt); L += s * 0.9f; R += s;
			float h = hat.run(dt); L += h * 0.7f; R += h;
			float p = perc.run(dt); L += p * 1.f; R += p * 0.6f;
			float tk = tick.run(dt); L += tk * 0.6f; R += tk * 0.9f;
			float rm = rim.run(dt); L += rm; R += rm * 0.7f;
			float cl = clap.run(dt); L += cl * 0.8f; R += cl * 0.8f;
			if (useBloom) {
				bl.process(edge, ct);
				if (bl.changed) {
					for (int v = 0; v < bl.nVoices; v++) pad[v].freq = hz(bl.voices[v]);
					bass.freq = hz(bl.bass);
				}
				if (edge && (clock.n % 4 == 2 || bl.changed)) bass.hit();
				for (int v = 0; v < bl.nVoices; v++) {
					float x = pad[v].run(dt, bl.gate, 1400.f);
					L += x * (v % 2 ? 0.7f : 1.f); R += x * (v % 2 ? 1.f : 0.7f);
				}
				float b = bass.run(dt);
				L += b; R += b;
			}
			w.add(0.7f * L, 0.7f * R);
		}
	}
	w.write(path);
}

int main() {
	// Sprout: each rule growing from generation 1, one generation every two bars.
	// A = tick, B = rim, C = clap; a plain four-on-the-floor kick for reference.
	render("test/sprout-demo.wav", {
		{14, 0, false, 0, 0, 0},   // Fibonacci
		{12, 1, false, 0, 0, 0},   // Thue-Morse
		{14, 4, false, 0, 0, 0},   // Tribonacci
		{8, 0, true, 0, 0, 0},     // Fibonacci as an endless stream of 16ths
	}, true, false, false, true);
	// Graft: Detroit groove mutating every bar, then the broken groove.
	render("test/graft-demo.wav", {{24, 0, false, 0, 0, 0}, {16, 0, false, 3, 0, 0}}, false, true, false, false);
	// Bloom: F Dorian, Homecoming then Circle, half a bar per chord, growing.
	render("test/bloom-demo.wav", {{24, 0, false, 0, 0, 1}, {24, 0, false, 0, 2, 2}}, false, false, true, true);
	// All three together.
	render("test/together-demo.wav", {{32, 0, true, 0, 3, 1}}, true, true, true, false);
	// quick table of what each rule grows into
	FILE* f = fopen("test/generations.txt", "w");
	if (f) {
		for (int r = 0; r < SPROUT_RULES; r++) {
			fprintf(f, "%s  (%s)\n", SPROUT_RULESETS[r].name, SPROUT_RULESETS[r].text);
			const std::vector<std::vector<uint8_t>>& g = sproutTables().bar[r];
			for (size_t i = 0; i < g.size() && i < 7; i++) {
				fprintf(f, "  %zu: ", i);
				for (uint8_t s : g[i]) fputc('A' + s, f);
				fputc('\n', f);
			}
		}
		for (int r = 0; r < BLOOM_RULES; r++) {
			fprintf(f, "%s  (%s), F Dorian\n", BLOOM_RULESETS[r].name, BLOOM_RULESETS[r].text);
			const std::vector<std::vector<uint8_t>>& g = bloomTables().gens[r];
			for (size_t i = 0; i < g.size(); i++) {
				fprintf(f, "  %zu:", i);
				for (uint8_t d : g[i]) fprintf(f, " %s", roman(1, d).c_str());
				fputc('\n', f);
			}
		}
		fclose(f);
	}
	return 0;
}
