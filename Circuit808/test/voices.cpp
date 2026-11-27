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
static std::vector<float> render(const std::vector<float>& k, float acc, float sec, uint32_t serial = 808, float voct = 0.f, float fs = FS) {
	E e; e.setSampleRate(fs); e.age.setSerial(serial);
	std::vector<float> o;
	e.process(k.data(), voct);   // settle the output stage
	e.hit(acc, k.data());
	for (int i = 0; i < sec * fs; i++) { e.process(k.data(), voct); o.push_back(e.out); }
	return o;
}
static float rms(const std::vector<float>& s, float t0, float t1, float fs = FS) {
	double a = 0; int n = 0;
	for (int i = (int)(t0 * fs); i < (int)(t1 * fs) && i < (int)s.size(); i++) { a += s[i] * s[i]; n++; }
	return n ? std::sqrt(a / n) : 0.f;
}
static float db(float a, float b) { return 20.f * std::log10(std::max(a, 1e-9f) / std::max(b, 1e-9f)); }
// mean loudness over 16 hits of one unit (noise voices vary hit to hit)
template <class E>
static float loud(std::vector<float> k, float sec, float acc = 0.f, float fs = FS, float voct = 0.f, bool usePeak = false) {
	E e; e.setSampleRate(fs); e.age.setSerial(808);
	double a = 0;
	for (int h = 0; h < 16; h++) {
		e.hit(acc, k.data());
		double s2 = 0; float p = 0.f; int n = (int)(sec * fs);
		for (int i = 0; i < n; i++) { e.process(k.data(), voct); s2 += e.out * e.out; p = std::max(p, std::fabs(e.out)); }
		a += usePeak ? p : std::sqrt(s2 / n);
		for (int i = 0; i < (int)(0.3f * fs); i++) e.process(k.data(), voct);
	}
	return a / 16;
}
template <class E>
static float spreadDb(const char* id, int steps, float sec, float acc = 0.f) {
	auto k = stockKnobs<E>();
	int ki = -1;
	for (int i = 0; i < E::spec().nKnobs; i++) if (std::string(E::spec().knobs[i].id) == id) ki = i;
	float lo = 1e9f, hi = 0.f;
	for (int j = 0; j <= steps; j++) { k[ki] = j / (float)steps; float l = loud<E>(k, sec, acc); lo = std::min(lo, l); hi = std::max(hi, l); }
	return db(hi, lo);
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

	// --- regressions for the 2.2.0 DSP review ---
	printf("review checks\n");
	{   // clave stock = 2.5 kHz: count zero crossings over the ring
		auto s = render<ClaveEngine>(stockKnobs<ClaveEngine>(), 0.f, 0.02f);
		int zc = 0; for (size_t i = 49; i + 1 < 0.012f * FS; i++) if (s[i] <= 0.f && s[i + 1] > 0.f) zc++;
		float hz = zc / (0.012f - 49 / FS);
		printf("  clave stock pitch about %.0f Hz\n", hz);
		check(std::fabs(hz / 2500.f - 1.f) < 0.05f, "clave stock near 2.5 kHz");
	}
	{   // snare: noise about as loud as the heads over the first 60 ms at stock SNAPPY
		auto k = stockKnobs<SnareEngine>();
		auto h = k; h[knobIndex<SnareEngine>("SNAPPY")] = 0.f;
		auto n = k; n[knobIndex<SnareEngine>("TONE")] = 0.f;
		float heads = rms(render<SnareEngine>(h, 0.f, 0.06f), 0.f, 0.06f);
		float full = loud<SnareEngine>(k, 0.06f);
		float noise = std::sqrt(std::max(0.f, full * full - heads * heads));
		printf("  snare first 60 ms: heads %.2f V, noise %.2f V RMS (%.1f dB)\n", heads, noise, db(noise, heads));
		check(std::fabs(db(noise, heads)) < 4.f, "snare noise within 4 dB of the heads");
	}
	{   // noise voices: same loudness at 48 and 96 kHz
		float c48 = loud<ClapEngine>(stockKnobs<ClapEngine>(), 0.2f, 0.f, 48000.f), c96 = loud<ClapEngine>(stockKnobs<ClapEngine>(), 0.2f, 0.f, 96000.f);
		float m48 = loud<MaracasEngine>(stockKnobs<MaracasEngine>(), 0.1f, 0.f, 48000.f), m96 = loud<MaracasEngine>(stockKnobs<MaracasEngine>(), 0.1f, 0.f, 96000.f);
		float s48 = loud<SnareEngine>(stockKnobs<SnareEngine>(), 0.2f, 0.f, 48000.f), s96 = loud<SnareEngine>(stockKnobs<SnareEngine>(), 0.2f, 0.f, 96000.f);
		printf("  96 vs 48 kHz: clap %+.1f dB, maracas %+.1f dB, snare %+.1f dB\n", db(c96, c48), db(m96, m48), db(s96, s48));
		check(std::fabs(db(c96, c48)) < 1.f && std::fabs(db(m96, m48)) < 1.5f && std::fabs(db(s96, s48)) < 1.f, "noise level holds at 96 kHz");
	}
	{   // STRIKE and tuning no longer volume knobs
		float tS = spreadDb<TomEngine>("STRIKE", 10, 0.3f), sS = spreadDb<SnareEngine>("STRIKE", 10, 0.1f);
		float cS = spreadDb<ClaveEngine>("STRIKE", 10, 0.03f), cT = spreadDb<ClaveEngine>("TUNE", 12, 0.03f);
		float rS = spreadDb<RimEngine>("STRIKE", 10, 0.03f);
		printf("  STRIKE level spread: tom %.1f, snare %.1f, clave %.1f, rim %.1f dB; clave TUNE %.1f dB\n", tS, sS, cS, rS, cT);
		check(tS < 6.f && sS < 6.f && cS < 6.f && rS < 6.f, "STRIKE moves level by under 6 dB");
		check(cT < 6.f, "clave TUNE moves level by under 6 dB");
		auto k = stockKnobs<ClaveEngine>();
		float lo = 1e9f, hi = 0.f;
		for (int st = -12; st <= 12; st++) { float l = loud<ClaveEngine>(k, 0.03f, 0.f, FS, st / 12.f); lo = std::min(lo, l); hi = std::max(hi, l); }
		printf("  clave over two octaves of V/OCT: %.1f dB spread\n", db(hi, lo));
		check(db(hi, lo) < 4.f, "clave plays evenly over V/OCT");
	}
	{   // accent depth similar on every voice
		float a[7];
		a[0] = db(loud<SnareEngine>(stockKnobs<SnareEngine>(), 0.2f, 1.f), loud<SnareEngine>(stockKnobs<SnareEngine>(), 0.2f));
		a[1] = db(loud<ClapEngine>(stockKnobs<ClapEngine>(), 0.2f, 1.f), loud<ClapEngine>(stockKnobs<ClapEngine>(), 0.2f));
		a[2] = db(loud<TomEngine>(stockKnobs<TomEngine>(), 0.2f, 1.f), loud<TomEngine>(stockKnobs<TomEngine>(), 0.2f));
		a[3] = db(loud<RimEngine>(stockKnobs<RimEngine>(), 0.05f, 1.f), loud<RimEngine>(stockKnobs<RimEngine>(), 0.05f));
		a[4] = db(loud<ClaveEngine>(stockKnobs<ClaveEngine>(), 0.03f, 1.f), loud<ClaveEngine>(stockKnobs<ClaveEngine>(), 0.03f));
		a[5] = db(loud<MaracasEngine>(stockKnobs<MaracasEngine>(), 0.06f, 1.f), loud<MaracasEngine>(stockKnobs<MaracasEngine>(), 0.06f));
		a[6] = db(loud<CowbellEngine>(stockKnobs<CowbellEngine>(), 0.2f, 1.f), loud<CowbellEngine>(stockKnobs<CowbellEngine>(), 0.2f));
		printf("  accent gain dB: snare %.1f clap %.1f tom %.1f rim %.1f clave %.1f maracas %.1f cowbell %.1f\n", a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
		bool ok = true; for (float v : a) if (v < 3.f || v > 6.5f) ok = false;
		check(ok, "accent adds 3-6.5 dB on every voice");
	}
	{   // level knobs that should not be volume knobs
		float cb = spreadDb<ClapEngine>("BAND", 6, 0.2f), cr = spreadDb<ClapEngine>("RES", 6, 0.2f), wr = spreadDb<CowbellEngine>("RES", 6, 0.2f);
		printf("  level spread: clap BAND %.1f, clap RES %.1f, cowbell RES %.1f dB\n", cb, cr, wr);
		check(cb < 5.f && cr < 5.f && wr < 7.f, "clap BAND/RES and cowbell RES hold the level");
		auto k = stockKnobs<TomEngine>();
		float lo = 1e9f, hi = 0.f;
		for (int m = 0; m < 2; m++) for (int r = 0; r < 3; r++) {
			k[knobIndex<TomEngine>("MODE")] = m; k[knobIndex<TomEngine>("RANGE")] = r; k[knobIndex<TomEngine>("SLAP")] = 0.f;
			float l = loud<TomEngine>(k, 0.2f, 0.f, FS, 0.f, true); lo = std::min(lo, l); hi = std::max(hi, l);
			printf("    %s %s: %.2f V peak\n", m ? "conga" : "tom", r == 0 ? "low" : r == 1 ? "mid" : "high", l);
		}
		printf("  tom/conga sizes: %.1f dB peak spread\n", db(hi, lo));
		check(db(hi, lo) < 4.f, "tom and conga sizes within 4 dB");
	}
	{   // maracas GRAINS at max: no silent hits, same density at 96 kHz
		auto k = stockKnobs<MaracasEngine>(); k[knobIndex<MaracasEngine>("GRAINS")] = 1.f;
		int silent = 0;
		for (int i = 0; i < 100; i++) { auto s = render<MaracasEngine>(k, 0.f, 0.06f, 808 + i); if (peak(s) < 0.3f) silent++; }
		printf("  maracas GRAINS 1: %d silent hits in 100\n", silent);
		check(silent <= 2, "maracas GRAINS 1 has beads in every hit");
		k[knobIndex<MaracasEngine>("GRAINS")] = 0.8f;
		float l48 = loud<MaracasEngine>(k, 0.06f, 0.f, 48000.f), l96 = loud<MaracasEngine>(k, 0.06f, 0.f, 96000.f);
		printf("  maracas GRAINS 0.8 96 vs 48 kHz: %+.1f dB\n", db(l96, l48));
		check(std::fabs(db(l96, l48)) < 2.f, "bead density per second, not per sample");
	}
	{   // cowbell: no click at the hit
		float worst = 0.f, steady = 0.f;
		for (int i = 0; i < 50; i++) {
			CowbellEngine e; e.setSampleRate(FS); e.age.setSerial(808);
			auto k = stockKnobs<CowbellEngine>();
			for (int j = 0; j < 37 * i + 5; j++) e.process(k.data(), 0.f);
			e.hit(0.f, k.data());
			float prev = e.out;
			for (int j = 0; j < 0.1f * FS; j++) {
				e.process(k.data(), 0.f);
				float d = std::fabs(e.out - prev); prev = e.out;
				if (j < 0.003f * FS) worst = std::max(worst, d); else if (j < 0.05f * FS) steady = std::max(steady, d);
			}
		}
		printf("  cowbell largest step: onset %.2f V, ringing %.2f V\n", worst, steady);
		check(worst < steady * 1.5f, "cowbell onset no sharper than the ring");
	}
	{   // NaN on V/OCT: recovers on the next hit
		CowbellEngine e; e.setSampleRate(FS);
		auto k = stockKnobs<CowbellEngine>();
		for (int j = 0; j < 10; j++) e.process(k.data(), NAN);
		e.hit(0.f, k.data());
		float p = 0.f; for (int j = 0; j < 0.1f * FS; j++) { e.process(k.data(), 0.f); p = std::max(p, std::fabs(e.out)); }
		check(std::isfinite(p) && p > 3.f, "cowbell recovers after NaN on V/OCT");
	}
	{   // DRIVE leaves no DC
		auto k = stockKnobs<TomEngine>(); k[knobIndex<TomEngine>("DRIVE")] = 1.f;
		auto s = render<TomEngine>(k, 1.f, 1.f);
		double m = 0; for (size_t i = (size_t)(0.5f * FS); i < s.size(); i++) m += s[i];
		m /= s.size() - (size_t)(0.5f * FS);
		printf("  tom DRIVE 1 mean 0.5-1 s: %.4f V\n", m);
		check(std::fabs(m) < 0.01, "no DC after DRIVE");
	}
	{   // silent voices go to sleep, and wake on a hit
		ClapEngine e; e.setSampleRate(FS);
		auto k = stockKnobs<ClapEngine>();
		e.hit(0.f, k.data());
		int n = 0; while (!e.sleeping() && n < 10 * FS) { e.process(k.data(), 0.f); n++; }
		printf("  clap asleep after %.2f s\n", n / FS);
		check(e.sleeping() && n < 6 * FS, "silent voice sleeps");
		e.hit(0.f, k.data());
		check(!e.sleeping(), "hit wakes it");
	}

	// demos
	{
		auto k = stockKnobs<SnareEngine>();
		demo<SnareEngine>(dir, "snare-01-stock", k, {4, 12});
		auto b = k; b[knobIndex<SnareEngine>("SNAPPY")] = 0.9f; b[knobIndex<SnareEngine>("SNAPDECAY")] = 0.8f; b[knobIndex<SnareEngine>("NOISECOL")] = 0.25f;
		demo<SnareEngine>(dir, "snare-02-long-brush", b, {4, 12});
		auto m = k; m[knobIndex<SnareEngine>("RATIO")] = 0.85f; m[knobIndex<SnareEngine>("TONE")] = 0.8f; m[knobIndex<SnareEngine>("DECAY")] = 0.65f; m[knobIndex<SnareEngine>("SNAPPY")] = 0.2f;
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
