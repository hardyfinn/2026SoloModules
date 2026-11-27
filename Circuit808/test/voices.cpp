// Offline checks and demo WAVs for the 808 Circuits voices (no Rack needed).
//   make test   ->  test/out/*.wav and a printed report
#include "../src/VoiceEngines.hpp"
#include <cstdio>
#include <vector>
#include <string>
using namespace c808;

static const float FS = 48000.f;
static int fails = 0;
static void check(bool ok, const std::string& what) { printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str()); if (!ok) fails++; }

static void wav(const std::string& path, const std::vector<float>& s) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return;
	uint32_t n = s.size(), bytes = n * 2;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(1); w32((uint32_t)FS); w32((uint32_t)FS * 2); w16(2); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (float v : s) { float c = std::max(-1.f, std::min(1.f, v / 10.f)); w16((uint16_t)(int16_t)(c * 32767)); }
	fclose(f);
}

template <class E>
static std::vector<float> stockKnobs() {
	std::vector<float> k;
	for (int i = 0; i < E::spec().nKnobs; i++) k.push_back(E::spec().knobs[i].stock);
	return k;
}

template <class E>
static std::vector<float> render(const std::vector<float>& k, float acc, float sec, uint32_t serial = 808, float voct = 0.f) {
	E e; e.setSampleRate(FS); e.age.setSerial(serial);
	std::vector<float> o;
	e.hit(acc, k.data());
	for (int i = 0; i < sec * FS; i++) { e.process(k.data(), voct); o.push_back(e.out); }
	return o;
}

static float peak(const std::vector<float>& s) { float p = 0; for (float v : s) p = std::max(p, std::fabs(v)); return p; }
static float t60(const std::vector<float>& s) { float p = peak(s); int l = 0; for (size_t i = 0; i < s.size(); i++) if (std::fabs(s[i]) > p * 0.001f) l = i; return l / FS; }

template <class E>
static int knobIndex(const char* id) {
	for (int i = 0; i < E::spec().nKnobs; i++) if (std::string(E::spec().knobs[i].id) == id) return i;
	return -1;
}

// a two-bar pattern at 124 BPM: 16ths with given steps, odd steps accented
template <class E>
static void demo(const std::string& dir, const std::string& name, std::vector<float> k, const std::vector<int>& steps, uint32_t serial = 808) {
	E e; e.setSampleRate(FS); e.age.setSerial(serial);
	std::vector<float> o;
	int step = 60.f / 124.f / 4.f * FS;
	for (int i = 0; i < step * 32; i++) {
		if (i % step == 0) {
			int s = (i / step) % 16;
			for (size_t j = 0; j < steps.size(); j++) if (steps[j] == s) e.hit(j % 2 == 0 ? 1.f : 0.f, k.data());
		}
		e.process(k.data(), 0.f); o.push_back(e.out);
	}
	wav(dir + "/" + name + ".wav", o);
}

template <class E>
static void basic(const char* name) {
	printf("%s\n", name);
	auto k = stockKnobs<E>();
	auto u = render<E>(k, 0.f, 2.f), a = render<E>(k, 1.f, 2.f);
	printf("  stock: peak %.2f V unaccented, %.2f V accented, T60 %.3f s\n", peak(u), peak(a), t60(u));
	check(peak(u) > 2.f && peak(u) < 8.f, "unaccented peak 2-8 V");
	check(peak(a) > peak(u) * 1.2f && peak(a) < 11.f, "accent louder, inside rails");
	// extremes and random settings stay finite and inside the rails
	Rng r; r.seed(77);
	bool ok = true;
	for (int trial = 0; trial < 40; trial++) {
		std::vector<float> kk(k.size());
		for (size_t i = 0; i < kk.size(); i++) {
			const KnobDef& d = E::spec().knobs[i];
			float v = trial == 0 ? 0.f : trial == 1 ? 1.f : r.uni();
			kk[i] = d.kind == 1 ? std::round(v * 2.f) : d.kind == 2 ? std::round(v) : v;
		}
		auto s = render<E>(kk, trial % 2 ? 1.f : 0.f, 0.6f, trial, trial == 1 ? 2.f : 0.f);
		for (float v : s) if (!std::isfinite(v) || std::fabs(v) > 12.f) ok = false;
	}
	check(ok, "40 random/extreme settings: finite, inside rails");
	// AGE
	int ai = knobIndex<E>("AGE");
	auto k1 = k; k1[ai] = 1.f;
	auto s1 = render<E>(k1, 0.f, 0.5f, 1), s2 = render<E>(k1, 0.f, 0.5f, 2);
	bool differ = false; for (size_t i = 0; i < s1.size(); i++) if (std::fabs(s1[i] - s2[i]) > 1e-3f) differ = true;
	check(differ, "AGE 1: two units differ");
	auto z1 = render<E>(k, 0.f, 0.5f, 1), z2 = render<E>(k, 0.f, 0.5f, 2);
	bool same = true; for (size_t i = 0; i < z1.size(); i++) if (z1[i] != z2[i]) same = false;
	// noise voices draw noise from their own generator, the same for every unit
	check(same, "AGE 0: every unit identical");
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	basic<SnareEngine>("808 Snare");
	basic<ClapEngine>("808 Clap");
	basic<TomEngine>("808 Tom/Conga");
	basic<RimEngine>("808 Rim");
	basic<ClaveEngine>("808 Clave");
	basic<MaracasEngine>("808 Maracas");
	basic<CowbellEngine>("808 Cowbell");

	// a few specific checks
	{
		auto k = stockKnobs<ClapEngine>();
		k[knobIndex<ClapEngine>("MIX")] = 0.f;   // bursts only
		auto s = render<ClapEngine>(k, 0.f, 0.2f);
		// count separate bursts: envelope peaks in 2 ms windows
		int bursts = 0; float prev = 0; bool rising = false;
		for (size_t i = 0; i + 96 < s.size(); i += 96) {
			float p = 0; for (int j = 0; j < 96; j++) p = std::max(p, std::fabs(s[i + j]));
			if (p > prev * 1.5f && p > 0.3f) { if (!rising) bursts++; rising = true; } else if (p < prev) rising = false;
			prev = p;
		}
		printf("clap stock bursts counted: %d\n", bursts);
		check(bursts >= 3 && bursts <= 5, "clap: about 4 bursts at stock");
	}
	{
		auto k = stockKnobs<TomEngine>();
		k[knobIndex<TomEngine>("MODE")] = 1.f;
		check(TomEngine::stockFor(knobIndex<TomEngine>("SLAP"), k.data()) == 0.f, "conga: stock SLAP is 0");
	}

	// demos
	{
		auto k = stockKnobs<SnareEngine>();
		demo<SnareEngine>(dir, "snare-01-stock", k, {4, 12});
		auto b = k; b[knobIndex<SnareEngine>("SNAPPY")] = 0.9f; b[knobIndex<SnareEngine>("SNAPDECAY")] = 0.8f; b[knobIndex<SnareEngine>("NOISECOL")] = 0.25f;
		demo<SnareEngine>(dir, "snare-02-long-brush", b, {4, 12});
		auto m = k; m[knobIndex<SnareEngine>("RATIO")] = 0.85f; m[knobIndex<SnareEngine>("BALANCE")] = 0.8f; m[knobIndex<SnareEngine>("DECAY")] = 0.65f; m[knobIndex<SnareEngine>("SNAPPY")] = 0.2f;
		demo<SnareEngine>(dir, "snare-03-metallic-heads", m, {4, 12, 14});
	}
	{
		auto k = stockKnobs<ClapEngine>();
		demo<ClapEngine>(dir, "clap-01-stock", k, {4, 12});
		auto c = k; c[knobIndex<ClapEngine>("BURSTS")] = 1.f; c[knobIndex<ClapEngine>("SPREAD")] = 0.8f; c[knobIndex<ClapEngine>("TAIL")] = 0.7f;
		demo<ClapEngine>(dir, "clap-02-crowd", c, {4, 12});
		auto s = k; s[knobIndex<ClapEngine>("BURSTS")] = 0.f; s[knobIndex<ClapEngine>("BAND")] = 0.8f; s[knobIndex<ClapEngine>("MIX")] = 0.15f;
		demo<ClapEngine>(dir, "clap-03-snap", s, {4, 12, 15});
	}
	{
		auto k = stockKnobs<TomEngine>();
		demo<TomEngine>(dir, "tom-01-stock-mid-tom", k, {0, 3, 6, 10});
		auto c = k; c[knobIndex<TomEngine>("MODE")] = 1.f; c[knobIndex<TomEngine>("SLAP")] = 0.f;
		demo<TomEngine>(dir, "tom-02-stock-mid-conga", c, {0, 3, 6, 10, 11, 14});
		auto z = k; z[knobIndex<TomEngine>("BEND")] = 0.6f; z[knobIndex<TomEngine>("RANGE")] = 2.f; z[knobIndex<TomEngine>("DECAY")] = 0.7f;
		demo<TomEngine>(dir, "tom-03-syndrum-zap", z, {0, 6, 10});
		auto mu = c; mu[knobIndex<TomEngine>("MUTE")] = 0.8f; mu[knobIndex<TomEngine>("SLAP")] = 0.4f;
		demo<TomEngine>(dir, "tom-04-muted-slap-conga", mu, {0, 2, 3, 6, 8, 11, 14});
	}
	{
		auto k = stockKnobs<RimEngine>();
		demo<RimEngine>(dir, "rim-01-stock", k, {3, 6, 11, 14});
		auto w = k; w[knobIndex<RimEngine>("RING")] = 0.75f; w[knobIndex<RimEngine>("GATE")] = 0.9f; w[knobIndex<RimEngine>("SNAP")] = 0.2f;
		demo<RimEngine>(dir, "rim-02-woody-ring", w, {3, 6, 11, 14});
	}
	{
		auto k = stockKnobs<ClaveEngine>();
		demo<ClaveEngine>(dir, "clave-01-stock", k, {0, 3, 6, 10, 12});
		auto l = k; l[knobIndex<ClaveEngine>("TUNE")] = 0.2f; l[knobIndex<ClaveEngine>("RING")] = 0.55f; l[knobIndex<ClaveEngine>("BEND")] = 0.3f;
		demo<ClaveEngine>(dir, "clave-02-log-drum", l, {0, 3, 6, 10, 12});
		auto o = k; o[knobIndex<ClaveEngine>("RING")] = 0.6f; o[knobIndex<ClaveEngine>("OVERTONE")] = 0.6f; o[knobIndex<ClaveEngine>("RATIO")] = 0.3f; o[knobIndex<ClaveEngine>("TUNE")] = 0.4f;
		demo<ClaveEngine>(dir, "clave-03-marimba-ish", o, {0, 3, 6, 10, 12});
	}
	{
		auto k = stockKnobs<MaracasEngine>();
		demo<MaracasEngine>(dir, "maracas-01-stock", k, {0, 2, 4, 6, 8, 10, 12, 14});
		auto s = k; s[knobIndex<MaracasEngine>("SHAKE")] = 0.4f; s[knobIndex<MaracasEngine>("LENGTH")] = 0.6f; s[knobIndex<MaracasEngine>("GRAINS")] = 0.5f;
		demo<MaracasEngine>(dir, "maracas-02-shaker-grains", s, {0, 4, 8, 12});
	}
	{
		auto k = stockKnobs<CowbellEngine>();
		demo<CowbellEngine>(dir, "cowbell-01-stock", k, {0, 3, 6, 10, 12});
		auto a = k; a[knobIndex<CowbellEngine>("INTERVAL")] = 0.0f; a[knobIndex<CowbellEngine>("SHAPE")] = 0.1f; a[knobIndex<CowbellEngine>("TUNE")] = 0.7f;
		demo<CowbellEngine>(dir, "cowbell-02-agogo-clank", a, {0, 3, 6, 10, 12});
		auto r = k; r[knobIndex<CowbellEngine>("SHAPE")] = 1.f; r[knobIndex<CowbellEngine>("DECAY")] = 0.8f; r[knobIndex<CowbellEngine>("INTERVAL")] = 0.66f;
		demo<CowbellEngine>(dir, "cowbell-03-long-bell", r, {0, 6, 12});
	}

	printf(fails ? "%d FAILED\n" : "all voice checks passed\n", fails);
	return fails ? 1 : 0;
}
