// Offline checks and demo WAVs for the 808 Kick engine (no Rack needed).
//   make test   ->  test/out/*.wav and a printed report
#include "../src/KickEngine.hpp"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
using namespace kick808;

static const float FS = 48000.f;

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

static std::vector<float> render(Controls c, float accentIn, float seconds, std::vector<float>* bodyOut = nullptr) {
	Engine e; e.setSampleRate(FS);
	std::vector<float> o;
	e.hit(accentIn, c);
	for (int i = 0; i < seconds * FS; i++) {
		e.process(c);
		o.push_back(e.out);
		if (bodyOut) bodyOut->push_back(e.bp);
	}
	return o;
}

static float peak(const std::vector<float>& s, int a = 0, int b = -1) {
	if (b < 0) b = s.size();
	float p = 0; for (int i = a; i < b; i++) p = std::max(p, std::fabs(s[i])); return p;
}

// average frequency from upward zero crossings between t0 and t1
static float freqBetween(const std::vector<float>& s, float t0, float t1) {
	int a = t0 * FS, b = std::min<int>(t1 * FS, s.size() - 1);
	double first = -1, last = -1; int n = 0;
	for (int i = a; i < b; i++) if (s[i] <= 0 && s[i + 1] > 0) {
		double x = i + s[i] / (s[i] - s[i + 1]);
		if (first < 0) first = x;
		last = x; n++;
	}
	return n > 1 ? (n - 1) / ((last - first) / FS) : 0.f;
}

static float t60(const std::vector<float>& s) {
	float p = peak(s); int lastAbove = 0;
	for (size_t i = 0; i < s.size(); i++) if (std::fabs(s[i]) > p * 0.001f) lastAbove = i;
	return lastAbove / FS;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	int fails = 0;
	auto check = [&](bool ok, const char* what) { printf("  [%s] %s\n", ok ? "ok" : "FAIL", what); if (!ok) fails++; };

	Controls stock;
	std::vector<float> body;
	auto s = render(stock, 0.f, 2.f, &body);
	auto sa = render(stock, 1.f, 2.f);
	printf("stock unaccented: peak %.2f V, accented peak %.2f V, T60 %.2f s\n", peak(s), peak(sa), t60(s));
	float f1 = freqBetween(body, 0.010f, 0.060f), f2 = freqBetween(body, 0.060f, 0.150f), f3 = freqBetween(body, 0.4f, 1.0f);
	float fa1 = 0; { std::vector<float> b2; render(stock, 1.f, 1.f, &b2); fa1 = freqBetween(b2, 0.010f, 0.060f); }
	printf("pitch (body): 10-60 ms %.1f Hz, 60-150 ms %.1f Hz, 0.4-1 s %.1f Hz; accented 10-60 ms %.1f Hz\n", f1, f2, f3, fa1);
	check(peak(s) > 3.f && peak(s) < 8.f, "unaccented peak 3-8 V");
	check(peak(sa) > peak(s) * 1.4f && peak(sa) < 11.f, "accent louder, inside rails");
	check(std::fabs(f3 - 49.5f) < 1.5f, "settles at 49.5 Hz");
	check(f1 > 51.f && f1 < 60.f, "early sigh sharp (48-58 Hz region)");
	check(fa1 > f1, "accented hit sighs more");

	// self-oscillation is bounded
	Controls sus = stock; sus.decay = 1.f;
	auto ss = render(sus, 1.f, 6.f);
	float late = peak(ss, 5 * FS, 6 * FS);
	printf("sustain: late peak %.2f V\n", late);
	check(late > 1.f && late < 11.f, "DECAY at max sustains, bounded");

	// extremes don't blow up
	Controls x = stock; x.sweep = 1; x.bend = 1; x.strike = 1; x.click = 1; x.sigh = 1; x.tension = 1; x.drive = 1; x.tone = 1; x.decay = 0.99f; x.shelf = 1; x.retrig = 1; x.tuneHz = 2000;
	auto xe = render(x, 1.f, 3.f);
	bool fin = true; for (float v : xe) if (!std::isfinite(v) || std::fabs(v) > 12.f) fin = false;
	check(fin, "all knobs at max: finite, inside rails");
	x.tuneHz = 5; x.decay = 0; x.strike = 0; auto xl = render(x, 0.f, 1.f);
	fin = true; for (float v : xl) if (!std::isfinite(v)) fin = false;
	check(fin, "low extremes finite");

	// tune keeps level
	Controls hi = stock; hi.tuneHz = 99.f; auto sh = render(hi, 0.f, 1.f);
	printf("tuned up an octave: peak %.2f V\n", peak(sh));
	check(peak(sh) > peak(s) * 0.6f && peak(sh) < peak(s) * 1.6f, "level roughly constant across tune");

	// AGE: two serials differ, AGE 0 identical
	{
		Controls a = stock; a.age = 1.f; Engine e1, e2; e1.setSampleRate(FS); e2.setSampleRate(FS);
		e1.age.setSerial(1); e2.age.setSerial(2);
		std::vector<float> b1, b2; e1.hit(0, a); e2.hit(0, a);
		for (int i = 0; i < FS; i++) { e1.process(a); e2.process(a); b1.push_back(e1.bp); b2.push_back(e2.bp); }
		float fa = freqBetween(b1, 0.4f, 0.9f), fb = freqBetween(b2, 0.4f, 0.9f);
		printf("AGE 1: serial 1 %.1f Hz, serial 2 %.1f Hz\n", fa, fb);
		check(std::fabs(fa - fb) > 0.5f && std::fabs(fa - 49.5f) < 49.5f * 0.13f, "AGE: units differ, within tolerance");
		Engine z1, z2; z1.setSampleRate(FS); z2.setSampleRate(FS); z1.age.setSerial(1); z2.age.setSerial(2);
		z1.hit(0, stock); z2.hit(0, stock); bool same = true;
		for (int i = 0; i < FS / 2; i++) { z1.process(stock); z2.process(stock); if (z1.out != z2.out) same = false; }
		check(same, "AGE 0: every unit identical");
	}
	// attack jump: first zero crossing comes much sooner than without the jump
	{
		auto firstDown = [&](Controls c) { Engine e; e.setSampleRate(FS); e.hit(0, c); float prev = 0;
			for (int i = 0; i < FS / 10; i++) { e.process(c); if (i > 10 && prev > 0 && e.bp <= 0) return i / FS * 1000.f; prev = e.bp; } return 0.f; };
		Controls nj = stock; nj.sweep = 0;
		float a = firstDown(stock), b = firstDown(nj);
		printf("first zero crossing: stock %.2f ms, no jump %.2f ms\n", a, b);
		check(a < b * 0.75f, "attack jump raises the first half-cycle's pitch");
	}
	// demos: a 4-on-the-floor bar at 124 BPM per setting
	auto bar = [&](Controls c, const char* name, bool acc) {
		Engine e; e.setSampleRate(FS); std::vector<float> o;
		int beat = 60.f / 124.f * FS;
		for (int i = 0; i < beat * 8; i++) {
			if (i % beat == 0) e.hit(acc && (i / beat) % 2 == 0 ? 1.f : 0.f, c);
			e.process(c); o.push_back(e.out);
		}
		wav(dir + "/" + name + ".wav", o);
	};
	bar(stock, "kick-01-stock", true);
	Controls lng = stock; lng.decay = 0.78f; bar(lng, "kick-02-long-decay", true);
	Controls boom = stock; boom.sigh = 1.f; boom.decay = 0.85f; boom.tuneHz = 45.f; bar(boom, "kick-03-deep-sigh", true);
	Controls zap = stock; zap.sweep = 0.9f; zap.bend = 0.8f; zap.tuneHz = 55.f; bar(zap, "kick-04-zap", false);
	Controls tens = stock; tens.tension = 1.f; tens.decay = 0.6f; bar(tens, "kick-05-tension", true);
	Controls dist = stock; dist.drive = 0.8f; dist.click = 0.8f; dist.tone = 0.75f; bar(dist, "kick-06-driven", true);
	Controls tom = stock; tom.tuneHz = 130.f; tom.decay = 0.5f; tom.sweep = 0.3f; bar(tom, "kick-07-tuned-up", true);
	Controls flam = stock; flam.retrig = 0.8f; bar(flam, "kick-09-retrig-flam", true);
	Controls knock = stock; knock.shelf = 0.f; knock.sweep = 0.7f; knock.bend = 0.6f; bar(knock, "kick-10-low-shelf-knock", true);
	Controls old = stock; old.age = 1.f; { Engine e; e.setSampleRate(FS); std::vector<float> o; int beat = 60.f / 124.f * FS; e.age.setSerial(808); for (int i = 0; i < beat * 8; i++) { if (i % beat == 0) e.hit(0.5f, old); e.process(old); o.push_back(e.out); } wav(dir + "/kick-11-age-full.wav", o); }
	Controls drone = stock; drone.decay = 1.f; bar(drone, "kick-08-sustain", false);

	printf(fails ? "%d FAILED\n" : "all checks passed\n", fails);
	return fails ? 1 : 0;
}
