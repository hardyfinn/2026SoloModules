// Offline checks for the Joanna engine + demo WAVs. No Rack needed.
//   make test      (prints the checks, writes test/out/*.wav)
#include "../src/JoannaEngine.hpp"
#include "../src/JoannaPresets.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <complex>
#include <chrono>
#include <memory>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

using namespace joanna;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static int midi(int m) { return m - 21; }

struct Ev { float t; int key; float vel; float dur; };
struct PedalEv { float t; float v; };
struct Take { std::vector<float> l, r, dry; };

static std::unique_ptr<Engine> fresh(const Controls& c) {
	std::unique_ptr<Engine> e(new Engine());
	e->setSampleRate(SR);
	for (int i = 0; i < 64; i++) e->process(c, 0.f);
	return e;
}

// plays a list of notes; pedal and controls can change over time
static Take play(Controls c, const std::vector<Ev>& evs, float secs, const std::vector<PedalEv>& ped = {}, Engine* use = nullptr) {
	std::unique_ptr<Engine> own;
	Engine* e = use;
	if (!e) { own = fresh(c); e = own.get(); }
	Take t;
	int n = (int)(secs * SR);
	std::vector<int> offAt(evs.size(), -1), onAt(evs.size());
	for (size_t i = 0; i < evs.size(); i++) { onAt[i] = (int)(evs[i].t * SR); offAt[i] = evs[i].dur > 0 ? (int)((evs[i].t + evs[i].dur) * SR) : -1; }
	for (int s = 0; s < n; s++) {
		for (auto& p : ped) if ((int)(p.t * SR) == s) c.pedal = p.v;
		for (size_t i = 0; i < evs.size(); i++) {
			if (onAt[i] == s) e->noteOn(evs[i].key, evs[i].vel);
			if (offAt[i] == s) e->noteOff(evs[i].key);
		}
		e->process(c, 0.f);
		t.l.push_back(e->outL); t.r.push_back(e->outR); t.dry.push_back(e->outDry);
	}
	return t;
}

static float peak(const std::vector<float>& x, int a = 0, int b = -1) {
	if (b < 0) b = x.size();
	float p = 0.f;
	for (int i = a; i < b; i++) p = std::max(p, std::fabs(x[i]));
	return p;
}
static float rmsDb(const std::vector<float>& x, float a, float b) {
	int i0 = (int)(a * SR), i1 = std::min((int)x.size(), (int)(b * SR));
	double s = 0;
	for (int i = i0; i < i1; i++) s += x[i] * x[i];
	return 10.f * std::log10(s / std::max(1, i1 - i0) + 1e-30);
}
static bool finite(const std::vector<float>& x) {
	for (float v : x) if (!std::isfinite(v)) return false;
	return true;
}

static std::vector<float> spectrum(const std::vector<float>& x, int a, int N) {
	std::vector<std::complex<double>> v(N);
	for (int i = 0; i < N; i++) {
		double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1));
		v[i] = (a + i < (int)x.size() ? x[a + i] : 0.f) * w;
	}
	for (int i = 1, j = 0; i < N; i++) {
		int bit = N >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(v[i], v[j]);
	}
	for (int len = 2; len <= N; len <<= 1) {
		std::complex<double> wl = std::polar(1.0, -2 * M_PI / len);
		for (int i = 0; i < N; i += len) {
			std::complex<double> w = 1;
			for (int k = 0; k < len / 2; k++) {
				auto u = v[i + k], t = v[i + k + len / 2] * w;
				v[i + k] = u + t; v[i + k + len / 2] = u - t; w *= wl;
			}
		}
	}
	std::vector<float> p(N / 2);
	for (int i = 0; i < N / 2; i++) p[i] = std::norm(v[i]);
	return p;
}
static float centroid(const std::vector<float>& x, float a, int N) {
	auto p = spectrum(x, (int)(a * SR), N);
	double s = 0, w = 0;
	for (int i = 1; i < (int)p.size(); i++) { s += p[i] * i; w += p[i]; }
	return (float)(s / w * SR / N);
}
// strongest peak between f0 and f1, with parabolic interpolation
static float peakHz(const std::vector<float>& p, int N, float f0, float f1) {
	int a = (int)(f0 * N / SR), b = (int)(f1 * N / SR), best = a;
	for (int i = a; i <= b; i++) if (p[i] > p[best]) best = i;
	double y0 = std::log(p[best - 1] + 1e-30), y1 = std::log(p[best] + 1e-30), y2 = std::log(p[best + 1] + 1e-30);
	double d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2);
	return (float)((best + d) * SR / N);
}
static float bandDb(const std::vector<float>& x, float a, int N, float f0, float f1) {
	auto p = spectrum(x, (int)(a * SR), N);
	double s = 0;
	for (int i = (int)(f0 * N / SR); i <= (int)(f1 * N / SR); i++) s += p[i];
	return 10.f * std::log10(s + 1e-30);
}

// peak-normalised to -1 dBFS (one gain for the whole file, so the takes keep their levels)
static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
	float pk = std::max(peak(l), peak(r));
	float gain = pk > 0.f ? 0.89f / pk : 1.f;
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	int n = l.size(), ch = 2, sr = (int)SR, bytes = n * ch * 2;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(ch); w32(sr); w32(sr * ch * 2); w16(ch * 2); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) {
		float a[2] = {l[i], r[i]};
		for (int c = 0; c < 2; c++) {
			float v = std::max(-1.f, std::min(1.f, a[c] * gain)) * 32767.f;
			int16_t s = (int16_t)v;
			std::fwrite(&s, 2, 1, f);
		}
	}
	std::fclose(f);
}

// ---------------------------------------------------------------- the demo phrase
// A slow gospel turnaround in F at 76 bpm: Fmaj9, Bb9, Am7 D7(#9), Gm9 C13sus, F6/9.
static void phrase(std::vector<Ev>& evs, std::vector<PedalEv>& ped, float t0, float velMul = 1.f) {
	const float b = 60.f / 76.f;
	auto chord = [&](float beat, std::initializer_list<int> notes, float vel, float dur) {
		int i = 0;
		for (int m : notes) { evs.push_back({t0 + beat * b + 0.008f * i, midi(m), vel * velMul * (0.92f + 0.05f * (i % 3)), dur * b}); i++; }
	};
	auto note = [&](float beat, int m, float vel, float dur) { evs.push_back({t0 + beat * b, midi(m), vel * velMul, dur * b}); };
	for (int bar = 0; bar < 4; bar++) {
		ped.push_back({t0 + bar * 4 * b - 0.05f, 0.f});
		ped.push_back({t0 + bar * 4 * b + 0.12f, 1.f});
	}
	ped.push_back({t0 + 16 * b + 2.2f, 0.f});
	// bar 1: Fmaj9
	chord(0, {29, 41}, 0.62f, 3.8f);
	chord(0.02f, {57, 60, 64, 67}, 0.45f, 1.8f);
	note(2, 72, 0.55f, 0.5f); note(2.5f, 74, 0.5f, 0.5f); note(3, 76, 0.6f, 1.f);
	// bar 2: Bb9
	chord(4, {34, 46}, 0.6f, 3.8f);
	chord(4.02f, {56, 60, 62, 65}, 0.45f, 1.9f);
	note(6, 77, 0.58f, 0.5f); note(6.5f, 76, 0.5f, 0.5f); note(7, 74, 0.55f, 1.f);
	// bar 3: Am7 | D7(#9)
	chord(8, {33, 45}, 0.6f, 1.9f);
	chord(8.02f, {55, 60, 64}, 0.45f, 1.9f);
	chord(10, {38, 50}, 0.62f, 1.9f);
	chord(10.02f, {54, 60, 65}, 0.5f, 1.9f);
	note(11, 72, 0.55f, 0.5f); note(11.5f, 69, 0.5f, 0.5f);
	// bar 4: Gm9 | C13sus walk-up
	chord(12, {31, 43}, 0.6f, 1.9f);
	chord(12.02f, {53, 57, 58, 62}, 0.45f, 1.9f);
	note(14, 36, 0.62f, 0.5f); note(14.5f, 37, 0.58f, 0.5f); note(15, 38, 0.64f, 0.5f);
	chord(14.02f, {58, 62, 65, 69}, 0.5f, 1.9f);
	// home: F6/9
	chord(16, {29, 41}, 0.68f, 3.f);
	chord(16.03f, {57, 62, 67, 72}, 0.55f, 3.f);
	note(16.6f, 79, 0.42f, 2.f);
}

int main(int argc, char** argv) {
	std::string outDir = argc > 1 ? argv[1] : "test/out";
#if defined(__x86_64__) || defined(__i386__)
	_mm_setcsr(_mm_getcsr() | 0x8040);   // flush denormals to zero, as Rack does on its audio threads
#endif
	auto t0 = std::chrono::steady_clock::now();
	Controls base;

	// 1. stiff-string partials (single wound string, so no unison spread)
	{
		Controls c = base; c.tuning = 0.f; c.distance = 0.4f;
		int key = 5;
		Take t = play(c, {{0.f, key, 0.7f, 0.f}}, 3.f);
		auto e = fresh(c);
		e->noteOn(key, 0.7f);
		const KeyModel& m = e->keys[key];
		int N = 1 << 17;
		auto p = spectrum(t.dry, (int)(0.2f * SR), N);
		float worst = 0.f;
		for (int k = 3; k <= 9; k++) {
			float want = k * m.f0 * Engine::s((float)k, e->B[key]);
			float got = peakHz(p, N, want * 0.985f, want * 1.015f);
			worst = std::max(worst, std::fabs(1200.f * std::log2(got / want)));
		}
		CHECK(worst < 2.f, "partials 3..9 of A#0 sit on the stiff-string formula (worst %.2f cents)", worst);
	}
	// 2. stretch
	{
		auto e = fresh(base);
		float top = 1200.f * std::log2(e->fT[87] / (440.f * std::exp2((87 - 48) / 12.f)));
		float bot = 1200.f * std::log2(e->fT[0] / (440.f * std::exp2((0 - 48) / 12.f)));
		CHECK(top > 15.f && top < 45.f && bot < -8.f && bot > -45.f, "tuner's stretch: C8 %+.1f cents, A0 %+.1f cents", top, bot);
		bool mono = true;
		for (int n = 1; n < NKEYS; n++) if (e->fT[n] <= e->fT[n - 1]) mono = false;
		CHECK(mono, "stretched tuning rises key by key");
	}
	// 3. hammer contact time
	auto contactMs = [&](int key, float vel, Controls c) {
		auto e = fresh(c);
		e->noteOn(key, vel);
		int ct = 0;
		for (int i = 0; i < (int)(0.05f * SR); i++) {
			e->process(c, 0.f);
			for (auto& v : e->voices) if (v.active && v.contact && v.F > 0.f) ct++;
		}
		return ct * 1000.f / SR;
	};
	{
		float a = contactMs(KEY_C4, 0.2f, base), b = contactMs(KEY_C4, 0.9f, base), h = contactMs(75, 0.5f, base), l = contactMs(10, 0.5f, base);
		CHECK(b < a && h < l, "hammer contact: C4 soft %.2f ms, hard %.2f ms; bass %.2f ms, treble %.2f ms", a, b, l, h);
	}
	// 4. louder = brighter (felt stiffens)
	float cSoft, cLoud;
	{
		Take a = play(base, {{0.f, KEY_C4, 0.25f, 0.f}}, 0.6f), b = play(base, {{0.f, KEY_C4, 0.95f, 0.f}}, 0.6f);
		cSoft = centroid(a.dry, 0.01f, 8192); cLoud = centroid(b.dry, 0.01f, 8192);
		CHECK(cLoud > cSoft * 1.3f, "C4 brightness: soft %.0f Hz, loud %.0f Hz (centroid)", cSoft, cLoud);
		float dB = rmsDb(b.dry, 0.f, 0.3f) - rmsDb(a.dry, 0.f, 0.3f);
		CHECK(dB > 15.f && dB < 45.f, "C4 soft to loud: %.1f dB", dB);
	}
	// 5. two-stage decay
	{
		Controls c = base; c.tuning = 0.f; c.unison = 0.f; c.wear = 0.f;
		Take t = play(c, {{0.f, KEY_C4, 0.6f, 0.f}}, 6.f);
		float early = (rmsDb(t.dry, 0.1f, 0.3f) - rmsDb(t.dry, 0.8f, 1.0f)) / 0.7f;
		float late = (rmsDb(t.dry, 3.f, 3.4f) - rmsDb(t.dry, 5.4f, 5.8f)) / 2.4f;
		CHECK(early > 2.f * late && late > 0.f, "two-stage decay at C4: prompt %.1f dB/s, aftersound %.1f dB/s", early, late);
	}
	// 6. beating of a mistuned bichord
	{
		Controls c = base; c.tuning = 0.f; c.unison = 0.4f;
		int key = 20;
		Take t = play(c, {{0.f, key, 0.7f, 0.f}}, 6.f);
		auto e = fresh(c);
		e->noteOn(key, 0.7f);
		float f4 = 4.f * e->keys[key].f0 * Engine::s(4.f, e->B[key]);
		float dc = e->det[key][1] - e->det[key][0];
		float want = f4 * (std::exp2(dc / 1200.f) - 1.f);
		int N = 1 << 18;
		auto p = spectrum(t.dry, (int)(0.3f * SR), N);
		int a = (int)((f4 - 8.f) * N / SR), b = (int)((f4 + 8.f) * N / SR);
		int p1 = a;
		for (int i = a; i <= b; i++) if (p[i] > p[p1]) p1 = i;
		int p2 = -1;
		for (int i = a + 1; i < b; i++)
			if (std::abs(i - p1) > 4 && p[i] > p[i - 1] && p[i] > p[i + 1] && (p2 < 0 || p[i] > p[p2])) p2 = i;
		float got = std::fabs(p1 - p2) * SR / N;
		CHECK(std::fabs(got - std::fabs(want)) < 0.35f * std::fabs(want), "bichord partial 4 splits by %.2f Hz (strings %.1f cents apart, expect %.2f Hz)", got, dc, std::fabs(want));
	}
	// 7. tuning since the tuner came
	{
		auto rmsCents = [&](float tune) {
			Controls c = base; c.tuning = tune;
			auto e = fresh(c);
			e->applyControls(true);
			double s = 0, m = 0;
			for (int n = 0; n < NKEYS; n++) m += 1200.0 * std::log2(e->fKey[n] / e->fT[n]);
			m /= NKEYS;
			for (int n = 0; n < NKEYS; n++) { double d = 1200.0 * std::log2(e->fKey[n] / e->fT[n]) - m; s += d * d; }
			return std::make_pair((float)std::sqrt(s / NKEYS), (float)m);
		};
		auto a = rmsCents(0.f), b = rmsCents(1.f);
		CHECK(a.first < 1.f && b.first > 6.f && b.second < -15.f, "tuning: just tuned %.2f cents rms; 20 years %.1f cents rms and %.0f cents flat overall", a.first, b.first, b.second);
	}
	// 8. dampers
	{
		Take t = play(base, {{0.f, KEY_C4, 0.6f, 1.f}}, 2.f);
		float drop = rmsDb(t.dry, 0.9f, 1.f) - rmsDb(t.dry, 1.45f, 1.55f);
		CHECK(drop > 40.f, "C4 released: down %.0f dB within half a second", drop);
		Take h = play(base, {{0.f, 80, 0.6f, 0.f}}, 1.5f), r = play(base, {{0.f, 80, 0.6f, 0.2f}}, 1.5f);
		float diff = std::fabs(rmsDb(h.dry, 0.6f, 0.8f) - rmsDb(r.dry, 0.6f, 0.8f));
		CHECK(diff < 3.f, "top keys have no dampers (held vs released differ by %.1f dB)", diff);
	}
	// 9. sustain pedal
	{
		Controls c = base; c.pedal = 1.f;
		Take p = play(c, {{0.f, KEY_C4, 0.6f, 0.5f}}, 1.5f);
		c.pedal = 0.5f;
		Take h = play(c, {{0.f, KEY_C4, 0.6f, 0.5f}}, 1.5f);
		Take n = play(base, {{0.f, KEY_C4, 0.6f, 0.5f}}, 1.5f);
		float a = rmsDb(p.dry, 1.f, 1.2f), b = rmsDb(h.dry, 1.f, 1.2f), d = rmsDb(n.dry, 1.f, 1.2f);
		CHECK(a > b && b > d && a - d > 30.f, "sustain pedal after release: full %.0f dB, half %.0f dB, up %.0f dB", a, b, d);
	}
	// 10. a silently held key sings after a staccato octave below
	{
		Controls c = base; c.symp = 0.7f;
		std::vector<Ev> with = {{0.f, KEY_C4, 0.f, 3.f}, {0.2f, KEY_C4 - 12, 0.8f, 0.15f}};
		std::vector<Ev> without = {{0.2f, KEY_C4 - 12, 0.8f, 0.15f}};
		Take a = play(c, with, 2.5f), b = play(c, without, 2.5f);
		auto e = fresh(c);
		float f = e->fKey[KEY_C4];
		float A = bandDb(a.dry, 1.f, 32768, f * 0.98f, f * 1.02f), B = bandDb(b.dry, 1.f, 32768, f * 0.98f, f * 1.02f);
		float S0 = bandDb(b.dry, 0.2f, 8192, 0.f, 4000.f) + 10.f * std::log10(32768.f / 8192.f);
		CHECK(A - B > 10.f && A - S0 > -50.f && A - S0 < -10.f, "held C4 sings after staccato C3: %.0f dB below the C3 blow (%.0f dB above not holding it)", S0 - A, A - B);
	}
	// 11. pedal-down halo
	{
		Controls c = base;
		std::vector<Ev> ev = {{0.f, KEY_C4, 0.7f, 3.f}};
		Take up = play(c, ev, 2.f);
		c.pedal = 1.f;
		Take dn = play(c, ev, 2.f);
		std::vector<float> d(up.dry.size());
		for (size_t i = 0; i < d.size(); i++) d[i] = dn.dry[i] - up.dry[i];
		float rel = rmsDb(d, 0.5f, 1.5f) - rmsDb(up.dry, 0.5f, 1.5f);
		CHECK(rel > -40.f && rel < -3.f, "pedal-down halo around a held C4: %.0f dB relative to the note", rel);
	}
	// 12. soft and felt pedals
	{
		Controls c = base; c.soft = true;
		Take s = play(c, {{0.f, KEY_C4, 0.7f, 0.f}}, 0.6f);
		c.soft = false; c.felt = true;
		Take f = play(c, {{0.f, KEY_C4, 0.7f, 0.f}}, 0.6f);
		Take n = play(base, {{0.f, KEY_C4, 0.7f, 0.f}}, 0.6f);
		float cs = centroid(s.dry, 0.01f, 8192), cf = centroid(f.dry, 0.01f, 8192), cn = centroid(n.dry, 0.01f, 8192);
		float ls = rmsDb(s.dry, 0.f, 0.3f), ln = rmsDb(n.dry, 0.f, 0.3f);
		CHECK(ls < ln - 3.f && cs < cn, "soft pedal: %.1f dB quieter, centroid %.0f -> %.0f Hz", ln - ls, cn, cs);
		CHECK(cf < 0.7f * cn, "felt: centroid %.0f -> %.0f Hz", cn, cf);
	}
	// 13. restriking a ringing string
	{
		Take t = play(base, {{0.f, KEY_C4, 0.6f, 0.f}, {0.3f, KEY_C4, 0.6f, 0.f}}, 1.f);
		bool differ = false;
		int off = (int)(0.3f * SR);
		for (int i = 0; i < 4800; i++) if (std::fabs(t.dry[i + off] - t.dry[i]) > 1e-3f) differ = true;
		CHECK(differ && finite(t.dry) && peak(t.dry) < 10.f, "restrike on a ringing string differs from the first strike and stays sane (peak %.2f V)", peak(t.dry));
	}
	// 14. serial numbers
	{
		Controls c = base; c.tuning = 0.5f;
		std::vector<Ev> ev = {{0.f, KEY_C4, 0.6f, 0.f}, {0.f, KEY_C4 + 4, 0.6f, 0.f}};
		Take a = play(c, ev, 0.5f), b = play(c, ev, 0.5f);
		c.serial = 999;
		Take d = play(c, ev, 0.5f);
		bool same = true, diff = false;
		for (size_t i = 0; i < a.l.size(); i++) { if (a.l[i] != b.l[i]) same = false; if (std::fabs(a.l[i] - d.l[i]) > 1e-4f) diff = true; }
		CHECK(same && diff, "same serial plays the same piano, another serial a different one");
	}
	// 15. extremes
	{
		bool ok = true;
		float pk = 0.f;
		for (int ext = 0; ext < 2; ext++) {
			Controls c;
			float v = ext ? 1.f : 0.f;
			c.size = c.hammer = c.touch = c.sustain = c.symp = c.noise = c.tuning = c.unison = c.wear = c.age = v;
			c.wall = c.room = c.furnish = v; c.distance = ext ? 0.f : 1.f; c.lid = ext ? 2 : 0;
			c.pedal = 1.f; c.felt = ext; c.soft = !ext;
			std::vector<Ev> ev;
			for (int i = 0; i < 16; i++) ev.push_back({0.01f * i, 3 + i * 5, 1.f, 2.f});
			for (int i = 0; i < 16; i++) ev.push_back({2.5f + 0.05f * i, 40 + i, 1.f, 0.3f});
			Take t = play(c, ev, 6.f);
			ok = ok && finite(t.l) && finite(t.r);
			pk = std::max(pk, std::max(peak(t.l), peak(t.r)));
			ok = ok && rmsDb(t.l, 5.f, 6.f) < rmsDb(t.l, 0.f, 1.f) + 3.f;
		}
		CHECK(ok && pk < 30.f, "every control at both ends, 16-note clusters, pedal down: no runaway (peak %.1f V)", pk);
	}
	// 16. distance
	{
		float last = 1e9f;
		bool mono = true;
		std::string s;
		for (float d : {0.05f, 0.4f, 0.6f, 0.95f}) {
			Controls c = base; c.distance = d; c.furnish = 0.6f;
			Take t = play(c, {{0.f, KEY_C4, 0.7f, 0.08f}}, 1.f);
			float r = rmsDb(t.l, 0.f, 0.03f) - rmsDb(t.l, 0.15f, 0.6f);
			if (r >= last) mono = false;
			last = r;
			char b[32]; std::snprintf(b, 32, " %.1f", r); s += b;
		}
		CHECK(mono, "direct-to-room ratio falls with DISTANCE (dB):%s", s.c_str());
	}
	// 17. levels, DC, silence
	{
		std::vector<Ev> ev;
		for (int m : {29, 41, 48, 55, 60, 64, 67, 70, 74, 77}) ev.push_back({0.f, midi(m), 1.f, 1.5f});
		Take t = play(base, ev, 4.f);
		float pk = std::max(peak(t.l), peak(t.r));
		double dc = 0;
		for (float v : t.l) dc += v;
		dc /= t.l.size();
		Take q = play(base, {{0.f, KEY_C4, 0.5f, 0.5f}}, 1.f);
		CHECK(pk < 10.f && std::fabs(dc) < 0.01, "ff ten-note chord peaks at %.1f V, DC %.4f V; mf C4 peaks at %.2f V", pk, dc, peak(q.l));
		auto e = fresh(base);
		for (int i = 0; i < 48000; i++) e->process(base, 0.f);
		CHECK(std::fabs(e->outL) < 1e-6f && std::fabs(e->outR) < 1e-6f, "silence is silent");
	}
	// 19. action noise: smooth knocks, no hiss tail through the room (2.0.1 fix)
	{
		Take n = play(base, {{0.f, KEY_C4, 0.8f, 0.f}}, 0.4f);
		Controls q = base; q.noise = 0.f;
		Take z = play(q, {{0.f, KEY_C4, 0.8f, 0.f}}, 0.4f);
		std::vector<float> d(n.l.size());
		for (size_t i = 0; i < d.size(); i++) d[i] = n.l[i] - z.l[i];
		float knock = rmsDb(d, 0.f, 0.06f) - rmsDb(z.l, 0.f, 0.06f);
		auto p = spectrum(n.l, (int)(0.03f * SR), 8192);
		double hi = 0, all = 0;
		for (int i = 1; i < 4096; i++) { all += p[i]; if (i * SR / 8192 > 8000.f) hi += p[i]; }
		float hiss = 10.f * std::log10(hi / all + 1e-30);
		CHECK(knock < -20.f && knock > -45.f && hiss < -90.f, "C4 key thump %.0f dB under the note; above 8 kHz after the attack %.0f dB", knock, hiss);
	}
	// 18. speed
	{
		Controls c = base; c.pedal = 1.f;
		auto e = fresh(c);
		for (int i = 0; i < 16; i++) e->noteOn(20 + i * 3, 0.7f);
		auto a = std::chrono::steady_clock::now();
		int n = (int)(2.f * SR);
		for (int i = 0; i < n; i++) e->process(c, 0.f);
		double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
		float pct = (float)(sec / 2.0 * 100.0);
		auto e1 = fresh(base);
		e1->noteOn(KEY_C4, 0.7f);
		a = std::chrono::steady_clock::now();
		for (int i = 0; i < n; i++) e1->process(base, 0.f);
		float pct1 = (float)(std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count() / 2.0 * 100.0);
		auto e0 = fresh(base);
		for (int i = 0; i < (int)SR; i++) e0->process(base, 0.f);
		a = std::chrono::steady_clock::now();
		for (int i = 0; i < n; i++) e0->process(base, 0.f);
		float pct0 = (float)(std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count() / 2.0 * 100.0);
		CHECK(pct < 15.f, "CPU (one core, 48 kHz): 16 notes pedalled %.1f %%, one note %.2f %%, silent %.2f %%", pct, pct1, pct0);
	}

	// ---------------------------------------------------------------- demos
	if (argc > 2 && std::string(argv[2]) == "nodemo") {
		std::printf("%s (%d failed)\n", fails ? "FAILED" : "all passed", fails);
		return fails ? 1 : 0;
	}
	{
		std::vector<float> L, R;
		const int order[7] = {0, 1, 2, 6, 5, 8, 3};
		for (int i = 0; i < 7; i++) {
			Controls c = base;
			presetToControls(PRESETS[order[i]], c);
			std::vector<Ev> ev;
			std::vector<PedalEv> ped;
			phrase(ev, ped, 0.3f);
			Take t = play(c, ev, 17.f, ped);
			L.insert(L.end(), t.l.begin(), t.l.end());
			R.insert(R.end(), t.r.begin(), t.r.end());
			std::printf("demo %d: %s, peak %.1f V\n", i + 1, PRESETS[order[i]].name, std::max(peak(t.l), peak(t.r)));
		}
		writeWav(outDir + "/joanna-demo.wav", L, R);
	}
	{
		std::vector<float> L, R;
		const float b = 60.f / 96.f;
		for (int p = 0; p < NUM_PRESETS; p++) {
			Controls c = base;
			presetToControls(PRESETS[p], c);
			std::vector<Ev> ev;
			std::vector<PedalEv> ped = {{0.f, 1.f}, {4.5f, 0.f}};
			// a short lick over a Cm9 voicing
			for (int m : {36, 48}) ev.push_back({0.05f, midi(m), 0.62f, 3.f * b});
			int ch[4] = {58, 62, 63, 67};
			for (int i = 0; i < 4; i++) ev.push_back({0.06f + 0.01f * i, midi(ch[i]), 0.48f, 2.f * b});
			int lick[6] = {72, 70, 67, 70, 72, 75};
			for (int i = 0; i < 6; i++) ev.push_back({0.05f + (2.f + 0.5f * i) * b, midi(lick[i]), 0.5f + 0.05f * (i % 2), 0.45f * b});
			ev.push_back({0.05f + 5.f * b, midi(43), 0.6f, 1.f});
			ev.push_back({0.05f + 5.f * b, midi(65), 0.5f, 1.f});
			ev.push_back({0.05f + 5.f * b, midi(71), 0.5f, 1.f});
			Take t = play(c, ev, 5.5f, ped);
			L.insert(L.end(), t.l.begin(), t.l.end());
			R.insert(R.end(), t.r.begin(), t.r.end());
		}
		writeWav(outDir + "/joanna-presets.wav", L, R);
	}
	double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	std::printf("%s (%d failed), %.0f s\n", fails ? "FAILED" : "all passed", fails, total);
	return fails ? 1 : 0;
}
