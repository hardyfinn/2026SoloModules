// Offline checks for the Keel engine (no Rack needed): make test
// Optional: ./test/render some-kick.wav  measures a real kick (16-bit PCM WAV).
#include "../src/KeelEngine.hpp"
#include <cstdio>
#include <vector>
#include <string>

using namespace keel;

static int fails = 0;
static void check(bool ok, const char* what) {
	printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
	if (!ok) fails++;
}

// a simple analogue-style kick: sine that falls from about 3.3x to its pitch in ~12 ms,
// exponential body, 2 ms click of noise. `hz` = its pitch with this V/OCT applied.
static std::vector<float> kick(float hz, float fs, float decay = 0.4f, float len = 0.6f, uint64_t seed = 1) {
	Rng r;
	r.seed(seed);
	std::vector<float> v((size_t) (len * fs));
	double ph = 0.0;
	for (size_t i = 0; i < v.size(); i++) {
		float t = i / fs;
		float f = hz * (1.f + 2.3f * std::exp(-t / 0.012f));
		ph += f / fs;
		float s = 8.f * std::sin(2.f * PI * (float) ph) * std::exp(-t / decay);
		if (t < 0.002f) s += 3.f * (r.uni() * 2.f - 1.f);
		v[i] = s;
	}
	return v;
}

static float measure(Engine& e, const std::vector<float>& v, bool useOnsetDetector = false) {
	int before = e.meter.hits;
	bool started = false;
	for (size_t i = 0; i < v.size(); i++) {
		if (!useOnsetDetector && i == 0) { e.meter.onset(); started = true; }
		if (useOnsetDetector && e.onsetDet.process(v[i])) { e.meter.onset(); started = true; }
		e.meter.process(v[i]);
	}
	for (int i = 0; i < 2000; i++) e.meter.process(0.f);
	e.meter.finish();
	(void) started;
	return e.meter.hits != before ? e.meter.hz : -1.f;
}

static bool loadWav(const char* path, std::vector<float>& out, float& fs) {
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	std::vector<unsigned char> b;
	unsigned char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
	fclose(f);
	if (b.size() < 44 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) return false;
	size_t p = 12;
	int ch = 1, bits = 16;
	fs = 44100.f;
	while (p + 8 <= b.size()) {
		uint32_t sz = b[p + 4] | (b[p + 5] << 8) | (b[p + 6] << 16) | ((uint32_t) b[p + 7] << 24);
		if (!std::memcmp(&b[p], "fmt ", 4)) {
			ch = b[p + 10] | (b[p + 11] << 8);
			fs = (float) (b[p + 12] | (b[p + 13] << 8) | (b[p + 14] << 16) | ((uint32_t) b[p + 15] << 24));
			bits = b[p + 22] | (b[p + 23] << 8);
		}
		if (!std::memcmp(&b[p], "data", 4)) {
			if (bits != 16) return false;
			size_t frames = std::min((size_t) sz, b.size() - p - 8) / (2 * ch);
			out.resize(frames);
			for (size_t i = 0; i < frames; i++) {
				float s = 0.f;
				for (int c = 0; c < ch; c++) {
					const unsigned char* q = &b[p + 8 + (i * ch + c) * 2];
					s += (int16_t) (q[0] | (q[1] << 8)) / 32768.f;
				}
				out[i] = 5.f * s / ch;
			}
			return true;
		}
		p += 8 + sz + (sz & 1);
	}
	return false;
}

int main(int argc, char** argv) {
	if (argc > 1) {
		for (int a = 1; a < argc; a++) {
			std::vector<float> v;
			float fs;
			if (!loadWav(argv[a], v, fs)) { printf("%s: can't read\n", argv[a]); continue; }
			Engine e;
			e.setSampleRate(fs);
			float hz = measure(e, v);
			int m; float c;
			noteOf(hz, m, c);
			printf("%s: body %.2f Hz  %s%d %+.0f c\n", argv[a], hz, pcName(m), m / 12 - 1, c);
		}
		return 0;
	}

	const float rates[3] = {44100.f, 48000.f, 96000.f};
	printf("meter\n");
	for (float fs : rates) {
		Engine e;
		e.setSampleRate(fs);
		float worst = 0.f;
		for (float hz : {41.2f, 49.5f, 57.f, 65.4f, 82.f}) {
			float got = measure(e, kick(hz, fs));
			worst = std::max(worst, std::fabs(1200.f * std::log2(got / hz)));
		}
		printf("  %.0f Hz rate: worst error %.1f cents over 41-82 Hz kicks\n", fs, worst);
		check(worst < 25.f, "meter reads a swept kick's body within 25 cents");
	}
	{
		Engine e;
		float got = measure(e, kick(49.5f, 48000.f, 0.08f, 0.3f));
		printf("  short kick (80 ms decay): %.2f Hz\n", got);
		check(got > 0.f && std::fabs(1200.f * std::log2(got / 49.5f)) < 35.f, "short kick still measured within 35 cents");
		std::vector<float> two = kick(49.5f, 48000.f);
		std::vector<float> b = kick(55.f, 48000.f, 0.4f, 0.6f, 7);
		two.insert(two.end(), b.begin(), b.end());
		Engine e2;
		int before = e2.meter.hits, onsets = 0;
		for (float x : two) {
			if (e2.onsetDet.process(x)) { e2.meter.onset(); onsets++; }
			e2.meter.process(x);
		}
		e2.meter.finish();
		printf("  onset detector: %d onsets for 2 hits, last %.2f Hz\n", onsets, e2.meter.hz);
		check(onsets == 2 && e2.meter.hits - before == 2, "audio onset detector finds each hit once");
		check(std::fabs(1200.f * std::log2(e2.meter.hz / 55.f)) < 25.f, "second hit measured as its own pitch");
	}

	printf("note placement\n");
	{
		Engine e;
		Settings s;
		struct Case { int root, scale; const char* name; float want; } cases[] = {
			{0, MINOR, "C minor", 65.41f}, {9, MINOR, "A minor", 55.f}, {5, MINOR, "F minor", 43.65f},
			{7, MINOR, "G minor", 49.f}, {2, PHRYGIAN, "D phrygian", 36.71f}};
		bool ok = true;
		for (auto& c : cases) {
			s.root = c.root; s.scale = c.scale;
			e.trigger(s, true);
			printf("  %-11s with a 49.5 Hz kick -> %s%d %.2f Hz, V/OCT %+.3f\n", c.name, pcName(e.targetMidi),
			       e.targetMidi / 12 - 1, e.targetHz, e.voctTarget(s));
			ok &= std::fabs(e.targetHz / c.want - 1.f) < 0.002f && std::fabs(e.voctTarget(s)) <= 0.5f + 1e-4f;
		}
		check(ok, "root goes to the octave nearest the kick (never more than a tritone away)");
		s.root = 9; s.octave = -1;
		e.trigger(s, true);
		check(std::fabs(e.targetHz - 27.5f) < 0.05f, "OCTAVE -1 drops it an octave");
		s.octave = 0; s.fineCents = 50.f;
		e.trigger(s, true);
		check(std::fabs(1200.f * std::log2(e.targetHz / 55.f) - 50.f) < 0.5f, "FINE adds cents");
		s.fineCents = 0.f;
		s.followChord = true; s.chordPc = 5;   // F under A minor
		e.trigger(s, true);
		check(e.targetMidi % 12 == 5, "FOLLOW CHORD uses the chord's lowest note");
		s.chordPc = -1;
		e.trigger(s, true);
		check(e.targetMidi % 12 == 9, "no chord patched: falls back to the key root");
		// interval flavours stay in key
		s.followChord = false; s.root = 9; s.scale = MINOR;
		check(ivSemis(IV_THIRD, 9, 9, MINOR) == 3 && ivSemis(IV_THIRD, 0, 0, MAJOR) == 4, "3rd is minor in minor, major in major");
		check(ivSemis(IV_SEVENTH, 9, 9, MINOR) == 10 && ivSemis(IV_SEVENTH, 0, 0, MAJOR) == 11, "7th follows the key");
		check(ivSemis(IV_FIFTH, 11, 0, MAJOR) == 6, "5th of B in C major is the in-key F (diminished)");
		check(pcOfVolts(0.f) == 0 && pcOfVolts(0.75f) == 9 && pcOfVolts(-1.f / 12.f) == 11, "V/OCT to pitch class");
	}

	printf("pattern, MUTATE + LOCK\n");
	{
		Engine e;
		e.rng.seed(42);
		Settings s;
		s.steps = 4;
		bool onlyRootFifth = true;
		for (int k = 0; k < 200; k++) {
			e.pat.mutate(e.rng, 1.f, 0.f);
			for (int i = 0; i < MAXSTEPS; i++) onlyRootFifth &= e.pat.iv[i] == IV_ROOT || e.pat.iv[i] == IV_FIFTH;
		}
		check(onlyRootFifth, "SPREAD 0: only roots and fifths");
		int seen[NIV] = {};
		for (int k = 0; k < 400; k++) {
			e.pat.mutate(e.rng, 1.f, 1.f);
			for (int i = 0; i < MAXSTEPS; i++) seen[e.pat.iv[i]]++;
		}
		bool all = true;
		for (int i = 0; i < NIV; i++) all &= seen[i] > 100;
		check(all, "SPREAD 1: all six intervals appear");
		e.pat.clear();
		int wraps = 0;
		e.trigger(s, true);
		for (int k = 0; k < 12; k++) wraps += e.trigger(s, false) ? 1 : 0;
		check(wraps == 3 && e.pat.pos == 0, "4 steps: loop ends every 4 triggers");
		int changes = 0;
		for (int k = 0; k < 50; k++) changes += e.pat.mutate(e.rng, 0.f, 1.f) ? 1 : 0;
		check(changes == 0, "MUTATE 0 never changes a step");
		e.pat.iv[1] = IV_OCTAVE;
		e.trigger(s, true);
		e.trigger(s, false);
		float rootV = std::log2(midiHz(e.noteFor(s, IV_ROOT)) / 49.5f);
		check(e.pat.pos == 1 && std::fabs(e.voctTarget(s) - (rootV + 1.f)) < 1e-3f, "octave step sends the kick up 1 V");
	}

	printf("AUTO and LEARN\n");
	for (float fs : rates) {
		Engine e;
		e.setSampleRate(fs);
		Settings s;
		s.root = 9;                  // A minor -> A1 55 Hz
		s.kickHz = 49.5f;            // what Keel thinks
		s.autoTune = true;
		const float realNative = 57.f;   // what the kick really is at 0 V
		float err = 0.f;
		for (int hit = 0; hit < 5; hit++) {
			e.trigger(s, hit == 0);
			e.voctHit = e.voctTarget(s);
			float played = realNative * std::exp2(e.voctHit);
			measure(e, kick(played, fs));
			e.measured(s);
			err = e.lastErrCents;
			if (hit == 0) printf("  %.0f Hz rate: first hit %+.0f c off", fs, err);
		}
		printf(", after 5 hits %+.1f c\n", err);
		check(std::fabs(err) < 12.f, "AUTO pulls an unknown kick into tune within 5 hits");
		float learned = e.learnedKickHz();
		check(std::fabs(1200.f * std::log2(learned / realNative)) < 20.f, "LEARN finds the kick's own pitch");
	}

	printf("undertone\n");
	for (float fs : rates) {
		Engine e;
		e.setSampleRate(fs);
		std::vector<float> k = kick(49.5f, fs);
		Undertone& u = e.under;
		u.setXover(120.f);
		// REPLACE 0: highs + lows = the kick (all-pass), level within 1 dB
		double pin = 0, pout = 0;
		for (size_t i = 0; i < k.size(); i++) {
			if (i == 0) u.onset();
			u.process(k[i], 55.f, 0.f, 0.f, 0.f, 1.f);
			pin += k[i] * k[i];
			pout += u.mix * u.mix;
		}
		float db0 = 10.f * std::log10((float) (pout / pin));
		// REPLACE 1: the body is now the sub, at the target
		Engine e2;
		e2.setSampleRate(fs);
		Undertone& u2 = e2.under;
		u2.setXover(120.f);
		std::vector<float> mixed(k.size());
		bool finite = true;
		float peak = 0.f;
		for (size_t i = 0; i < k.size(); i++) {
			if (i == 0) u2.onset();
			u2.process(k[i], 55.f, 1.f, 7.f, 0.3f, 1.f);
			mixed[i] = u2.mix;
			finite &= std::isfinite(u2.mix);
			peak = std::max(peak, std::fabs(u2.mix));
		}
		Engine m;
		m.setSampleRate(fs);
		float heard = measure(m, mixed);
		// and with no DROP the body is exactly on target
		Engine e3;
		e3.setSampleRate(fs);
		e3.under.setXover(120.f);
		std::vector<float> pure(k.size());
		for (size_t i = 0; i < k.size(); i++) {
			if (i == 0) e3.under.onset();
			e3.under.process(k[i], 55.f, 1.f, 0.f, 0.f, 1.f);
			pure[i] = e3.under.mix;
		}
		Engine m3;
		m3.setSampleRate(fs);
		float heardPure = measure(m3, pure);
		check(std::fabs(1200.f * std::log2(heardPure / 55.f)) < 10.f, "REPLACE 1, DROP 0: body on target within 10 cents");
		printf("  %.0f Hz rate: REPLACE 0 level %+.2f dB; REPLACE 1 body %.2f Hz (target 55), peak %.1f V\n", fs, db0, heard, peak);
		check(std::fabs(db0) < 1.f, "REPLACE 0 passes the kick through");
		check(std::fabs(1200.f * std::log2(heard / 55.f)) < 30.f, "REPLACE 1, DROP 7: body still within 30 cents (sigh is early)");
		check(finite && peak < 14.f && peak > 3.f, "sub layer level sane");
	}

	printf(fails ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", fails);
	return fails ? 1 : 0;
}
