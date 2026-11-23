// Offline checks for the Rimbaud engine + a demo WAV. No Rack needed.
//   make test      (prints the checks, writes test/out/rimbaud-demo.wav)
#include "../src/RimbaudEngine.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <complex>
#include <chrono>

using namespace rimbaud;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

struct Take { std::vector<float> top, bot, mix; };

// one stroke on a settled drum
static Take render(Controls c, float vel, int stroke, float secs, float ext = 0.f, float extHz = 0.f) {
	Engine e;
	e.setSampleRate(SR);
	for (int i = 0; i < (int) (0.4f * SR); i++) e.process(c);
	Take t;
	int n = (int) (secs * SR);
	if (stroke >= 0) e.hit(vel, stroke, c);
	for (int i = 0; i < n; i++) {
		float x = ext > 0.f ? ext * std::sin(2.f * PI * extHz * i / SR) : 0.f;
		e.process(c, x);
		t.top.push_back(e.outTop); t.bot.push_back(e.outBot); t.mix.push_back(e.outMix);
	}
	return t;
}

static float peak(const std::vector<float>& x, int a = 0, int b = -1) {
	if (b < 0) b = x.size();
	float p = 0.f;
	for (int i = a; i < b; i++) p = std::max(p, std::fabs(x[i]));
	return p;
}
static float rms(const std::vector<float>& x, int a, int b) {
	double s = 0;
	for (int i = a; i < b; i++) s += x[i] * x[i];
	return std::sqrt(s / std::max(1, b - a));
}

// power spectrum of x[a .. a+N) (N power of 2), Hann window
static std::vector<float> spectrum(const std::vector<float>& x, int a, int N) {
	std::vector<std::complex<double>> v(N);
	for (int i = 0; i < N; i++) {
		double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1));
		v[i] = (a + i < (int) x.size() ? x[a + i] : 0.f) * w;
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
static float centroid(const std::vector<float>& x, int a, int N) {
	auto p = spectrum(x, a, N);
	double s = 0, w = 0;
	for (int i = 1; i < (int) p.size(); i++) { s += p[i] * i; w += p[i]; }
	return (float) (s / w * SR / N);
}
static float bandEnergy(const std::vector<float>& x, int a, int N, float f0, float f1) {
	auto p = spectrum(x, a, N);
	double s = 0;
	for (int i = (int) (f0 * N / SR); i < std::min((int) p.size(), (int) (f1 * N / SR)); i++) s += p[i];
	return (float) s;
}
// strongest frequency between f0 and f1 (parabolic interpolation)
static float peakHz(const std::vector<float>& x, int a, int N, float f0, float f1) {
	auto p = spectrum(x, a, N);
	int i0 = (int) (f0 * N / SR), i1 = (int) (f1 * N / SR), best = i0;
	for (int i = i0; i < i1; i++) if (p[i] > p[best]) best = i;
	float l = std::log(p[best - 1] + 1e-30f), m = std::log(p[best] + 1e-30f), r = std::log(p[best + 1] + 1e-30f);
	float d = 0.5f * (l - r) / (l - 2 * m + r);
	return (best + d) * SR / N;
}
// time (s) until the 5 ms RMS of x falls below 'db' under its maximum
static float decayTime(const std::vector<float>& x, float db) {
	int W = (int) (0.005f * SR);
	float mx = 0; std::vector<float> env;
	for (int i = 0; i + W < (int) x.size(); i += W) { float r = rms(x, i, i + W); env.push_back(r); mx = std::max(mx, r); }
	float th = mx * std::pow(10.f, -db / 20.f);
	int last = 0;
	for (int i = 0; i < (int) env.size(); i++) if (env[i] > th) last = i;
	return last * 0.005f;
}
// high band (3-12 kHz) of a signal by a simple 2nd-order highpass, for envelopes
static std::vector<float> highBand(const std::vector<float>& x) {
	std::vector<float> y(x.size());
	float k = std::exp(-2.f * PI * 3000.f / SR), a = 0, b = 0;
	for (size_t i = 0; i < x.size(); i++) { a += (x[i] - a) * (1 - k); float h = x[i] - a; b += (h - b) * (1 - k); y[i] = h - b; }
	return y;
}

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) { std::printf("cannot write %s\n", path.c_str()); return; }
	uint32_t n = L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32((uint32_t) SR); u32((uint32_t) SR * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] / 10.f * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "test/out";
	const int MS = (int) (SR / 1000);
	const ModeTable& T = table();
	std::printf("Bessel table: %d modes, j01 = %.6f (2.404826), j11 = %.6f (3.831706), j21 = %.6f (5.135622)\n",
	            T.n, T.j[0], T.j[1], T.j[2]);
	CHECK(std::fabs(T.j[0] - 2.404826) < 1e-5 && std::fabs(T.j[1] - 3.831706) < 1e-5 && std::fabs(T.j[2] - 5.135622) < 1e-5 && std::fabs(T.j[3] - 5.520078) < 1e-5,
	      "membrane mode table matches the Bessel zeros");

	Controls c0;   // defaults: 14 x 6.5 maple, snares on

	// 1. level and stability at the default
	{
		Take t = render(c0, 0.75f, HEAD, 1.5f);
		float p = peak(t.mix), pt = peak(t.top), pb = peak(t.bot);
		std::printf("      default hit: MIX %.2f V, TOP %.2f V, BOTTOM %.2f V peak\n", p, pt, pb);
		CHECK(p > 3.f && p < 9.9f && pt > 2.f && pt < 9.9f && pb > 2.f && pb < 9.9f, "default hit lands at a healthy level (3-9 V)");
		CHECK(peak(t.mix, (int) (1.3f * SR)) < 0.05f * p, "it dies away (last 0.2 s < -26 dB)");
	}

	// 2. velocity: louder, and the stick bounces off sooner (shorter contact = more top end)
	{
		Controls c = c0; c.snares = false;   // the head on its own (with snares, ghost notes are mostly fizz)
		auto contactMs = [&](float vel) {
			Engine e; e.setSampleRate(SR);
			for (int i = 0; i < (int) (0.4f * SR); i++) e.process(c);
			e.hit(vel, HEAD, c);
			int n = 0;
			for (int i = 0; i < 4800; i++) { e.process(c); if (e.tip.active && e.tip.dprev > 0.f) n++; }
			return n * 1000.f / SR;
		};
		Take s = render(c, 0.3f, HEAD, 0.3f), h = render(c, 1.0f, HEAD, 0.3f);
		float cs = bandEnergy(s.top, 0, 1024, 8000, 20000) / bandEnergy(s.top, 0, 1024, 20, 20000);
		float ch = bandEnergy(h.top, 0, 1024, 8000, 20000) / bandEnergy(h.top, 0, 1024, 20, 20000);
		float ts = contactMs(0.3f), th = contactMs(1.0f);
		std::printf("      soft hit %.2f V, stick on the head %.1f ms, %.1f dB above 8 kHz\n", peak(s.top), ts, 10 * std::log10(cs));
		std::printf("      hard hit %.2f V, stick on the head %.1f ms, %.1f dB above 8 kHz\n", peak(h.top), th, 10 * std::log10(ch));
		CHECK(peak(h.top) > 3.f * peak(s.top), "hard hits are much louder");
		CHECK(th < 0.75f * ts && ch > 2.f * cs, "hard hits bounce off sooner and have more top end (> 3 dB)");
	}

	// 3. pitch drop after a hard hit (tension modulation), snares off
	{
		Controls c = c0; c.snares = false; c.damp = 0.f;
		Take h = render(c, 1.1f, HEAD, 1.2f), s = render(c, 0.25f, HEAD, 1.2f);
		// follow the same (strongest late) mode back to the start of the sound
		float fl = peakHz(h.top, 500 * MS, 8192, 150, 900), fe = peakHz(h.top, 12 * MS, 2048, fl * 0.98f, fl * 1.3f);
		float sl = peakHz(s.top, 500 * MS, 8192, 150, 900), se = peakHz(s.top, 12 * MS, 2048, sl * 0.98f, sl * 1.3f);
		float cents = 1200.f * std::log2(fe / fl), scents = 1200.f * std::log2(se / sl);
		std::printf("      hard hit: %.1f Hz -> %.1f Hz (%.0f cents drop); soft hit %.0f cents\n", fe, fl, cents, scents);
		CHECK(cents > 15.f && cents < 250.f, "hard hits start sharp and fall (15-250 cents)");
		CHECK(scents < cents * 0.5f, "soft hits barely bend");
	}

	// 4. snares: buzz, and they deaden the bottom head
	{
		Controls on = c0, off = c0; off.snares = false;
		Take a = render(on, 0.75f, HEAD, 1.0f), b = render(off, 0.75f, HEAD, 1.0f);
		float hOn = bandEnergy(a.bot, 40 * MS, 8192, 3000, 12000), hOff = bandEnergy(b.bot, 40 * MS, 8192, 3000, 12000);
		std::printf("      bottom mic 3-12 kHz after 40 ms: snares on / off = %.1f dB\n", 10 * std::log10(hOn / hOff));
		CHECK(hOn > 30.f * hOff, "snares on: lots more fizz (> 15 dB)");
		float lOn = bandEnergy(a.bot, 200 * MS, 8192, 150, 800), lOff = bandEnergy(b.bot, 200 * MS, 8192, 150, 800);
		std::printf("      bottom head ring 150-800 Hz after 200 ms: on / off = %.1f dB\n", 10 * std::log10(lOn / lOff));
		CHECK(lOn < lOff, "snares deaden the ring of the bottom head");
	}

	// 5. strainer: loose snares buzz longer than tight ones
	{
		Controls lo = c0, ti = c0; lo.tension = 0.1f; ti.tension = 0.95f;
		Take a = render(lo, 0.75f, HEAD, 1.2f), b = render(ti, 0.75f, HEAD, 1.2f);
		auto ha = highBand(a.bot), hb = highBand(b.bot);
		float ea = rms(ha, 150 * MS, 400 * MS), eb = rms(hb, 150 * MS, 400 * MS);
		std::printf("      buzz 150-400 ms after the hit: loose %.3f V, tight %.3f V rms (%+.1f dB)\n", ea, eb, 20 * std::log10(ea / eb));
		CHECK(ea > 2.f * eb, "loose snares keep buzzing longer (> 6 dB more tail)");
	}

	// 6. position: edge is brighter and ringier than the centre
	{
		Controls ce = c0, ed = c0; ce.position = 0.f; ed.position = 0.95f; ce.snares = ed.snares = false;
		Take a = render(ce, 0.75f, HEAD, 0.5f), b = render(ed, 0.75f, HEAD, 0.5f);
		float ca = centroid(a.top, 5 * MS, 8192), cb = centroid(b.top, 5 * MS, 8192);
		std::printf("      centroid: centre %.0f Hz, edge %.0f Hz\n", ca, cb);
		CHECK(cb > ca * 1.2f, "edge hits are brighter");
	}

	// 7. stick: felt mallet is darker than a hard tip
	{
		Controls h = c0, s = c0; h.stick = 0.f; s.stick = 1.f; h.snares = s.snares = false;
		Take a = render(h, 0.75f, HEAD, 0.3f), b = render(s, 0.75f, HEAD, 0.3f);
		float ca = bandEnergy(a.top, 0, 2048, 2000, 20000) / bandEnergy(a.top, 0, 2048, 20, 20000);
		float cb = bandEnergy(b.top, 0, 2048, 2000, 20000) / bandEnergy(b.top, 0, 2048, 20, 20000);
		std::printf("      energy above 2 kHz: hard tip %.1f dB, mallet %.1f dB\n", 10 * std::log10(ca), 10 * std::log10(cb));
		CHECK(cb < 0.3f * ca, "soft mallet is darker (> 5 dB less above 2 kHz)");
	}

	// 8. rimshot: louder and with more crack than a plain hit
	{
		Take a = render(c0, 0.75f, HEAD, 0.4f), b = render(c0, 0.75f, RIMSHOT, 0.4f);
		float ea = bandEnergy(a.top, 0, 4096, 1000, 8000), eb = bandEnergy(b.top, 0, 4096, 1000, 8000);
		std::printf("      rimshot vs head, TOP 1-8 kHz first 85 ms: %+.1f dB; peaks %.2f / %.2f V\n",
		            10 * std::log10(eb / ea), peak(b.top), peak(a.top));
		CHECK(eb > 2.f * ea, "rimshot has more crack (> 3 dB in 1-8 kHz)");
		Controls dc = c0; dc.hoop = 1.f;
		Take d = render(dc, 0.75f, RIMSHOT, 0.6f);
		float tf = decayTime(highBand(b.top), 30), td = decayTime(highBand(d.top), 30);
		std::printf("      rimshot ring (-30 dB, highs): flanged %.0f ms, die-cast %.0f ms\n", tf * 1000, td * 1000);
		CHECK(td < tf, "die-cast hoop rings shorter than flanged");
	}

	// 9. cross-stick: short woody knock, quieter than a rimshot
	{
		Controls c = c0;
		Take x = render(c, 0.75f, CROSS, 0.6f), r = render(c, 0.75f, RIMSHOT, 0.6f);
		float f = peakHz(x.top, 0, 2048, 250, 3000);
		std::printf("      cross-stick: %.2f V peak, strongest %.0f Hz, -30 dB in %.0f ms\n", peak(x.top), f,
		            decayTime(x.top, 30) * 1000);
		CHECK(peak(x.top) > 0.5f && peak(x.top) < peak(r.top), "cross-stick is audible and quieter than a rimshot");
		// the stick's own ring: short overhang = high click, long = low knock
		auto stickPeak = [&](float xs) {
			Controls cc = c; cc.xstick = xs;
			Engine e; e.setSampleRate(SR);
			for (int i = 0; i < (int) (0.4f * SR); i++) e.process(cc);
			e.hit(0.75f, CROSS, cc);
			std::vector<float> st;
			for (int i = 0; i < 4096; i++) { e.process(cc); st.push_back(e.dbg[4]); }
			return peakHz(st, 0, 4096, 200, 2000);
		};
		float ps = stickPeak(0.f), pl = stickPeak(1.f);
		std::printf("      cross-stick, the stick's ring: short %.0f Hz, long %.0f Hz\n", ps, pl);
		CHECK(ps > 1.8f * pl, "XSTICK turns the knock from click to low knock");
	}

	// 10. buzz stroke: the stick bounces several times
	{
		Controls c = c0; c.buzz = 0.7f; c.snares = false;
		Engine e; e.setSampleRate(SR);
		for (int i = 0; i < (int) (0.4f * SR); i++) e.process(c);
		e.hit(0.6f, HEAD, c);
		int contacts = 0; bool inC = false;
		for (int i = 0; i < (int) (0.5f * SR); i++) {
			e.process(c);
			bool now = e.tip.active && e.tip.s < 1e9f && e.tip.F >= 0.f;
			// count new contacts from the tip's own record of overlap
			float gap = e.tip.dprev;
			bool touching = e.tip.active && gap > 0.f;
			if (touching && !inC) contacts++;
			inC = touching;
			(void) now;
		}
		std::printf("      buzz stroke: %d bounces\n", contacts);
		CHECK(contacts >= 4, "BUZZ makes the stick bounce (>= 4 contacts)");
	}

	// 11. EXT: a loud bass note makes the snares buzz in sympathy
	{
		Controls on = c0, off = c0; off.snares = false;
		float hz = 0.f;
		{   // the drum's own lowest (breathing) mode: find it from a soft centre hit
			Controls cc = off; cc.position = 0.f;
			Take t = render(cc, 0.3f, HEAD, 0.5f);
			hz = peakHz(t.bot, 20 * MS, 8192, 120, 600);
		}
		Take a = render(on, 0, -1, 1.0f, 30.f, hz), b = render(off, 0, -1, 1.0f, 30.f, hz);
		float ea = bandEnergy(a.bot, 200 * MS, 16384, 3000, 12000), eb = bandEnergy(b.bot, 200 * MS, 16384, 3000, 12000);
		std::printf("      EXT sine at %.0f Hz (30 Pa): fizz with snares on / off = %.1f dB\n", hz, 10 * std::log10(ea / eb));
		CHECK(ea > 10.f * eb, "outside sound makes the snares buzz (> 10 dB)");
	}

	// 12. torture: every corner, every stroke, very hard hits, repeated fast
	{
		Rng r; r.seed(7);
		bool ok = true; float worst = 0;
		for (int trial = 0; trial < 60 && ok; trial++) {
			Controls c;
			c.size = 10.f + 5.f * r.uni(); c.depth = 3.f + 5.f * r.uni();
			c.tuneHz = 100.f * std::pow(4.5f, r.uni()) * std::pow(2.f, 2.f * r.bi());
			c.tuneHz = std::min(std::max(c.tuneHz, 40.f), 1800.f);
			c.reso = 0.8f + 1.4f * r.uni(); c.damp = r.uni(); c.shell = r.uni(); c.position = r.uni();
			c.stick = r.uni(); c.buzz = r.uni() < 0.3f ? r.uni() : 0.f; c.rim = r.uni(); c.hoop = r.uni();
			c.xstick = r.uni(); c.snares = r.uni() < 0.7f; c.tension = r.uni(); c.wires = r.uni();
			Engine e; e.setSampleRate(trial % 3 == 0 ? 44100.f : (trial % 3 == 1 ? 48000.f : 96000.f));
			for (int i = 0; i < 20000; i++) e.process(c);
			for (int h = 0; h < 12; h++) {
				e.hit(r.uni() * 1.2f, (int) (r.uni() * 2.99f), c);
				for (int i = 0; i < 2500; i++) {
					e.process(c, r.uni() < 0.2f ? 50.f * r.bi() : 0.f);
					float m = std::max(std::fabs(e.outMix), std::max(std::fabs(e.outTop), std::fabs(e.outBot)));
					if (!std::isfinite(m)) { ok = false; break; }
					worst = std::max(worst, m);
				}
			}
		}
		CHECK(ok, "60 random drums x 12 fast hits at 44.1/48/96 kHz: always finite (loudest %.2f V)", worst);
	}

	// 13. CPU
	{
		Controls c = c0;
		Engine e; e.setSampleRate(48000.f);
		auto t0 = std::chrono::high_resolution_clock::now();
		int n = 48000 * 4;
		for (int i = 0; i < n; i++) { if (i % 12000 == 0) e.hit(0.8f, i % 24000 ? HEAD : RIMSHOT, c); e.process(c); }
		double s = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
		std::printf("      CPU: %.1f %% of one core at 48 kHz while playing (this machine)\n", 100.0 * s / 4.0);
		CHECK(s / 4.0 < 0.25, "cheap enough to run (< 25 %% of a core here)");
	}

	// ---- demo: a short groove showing the strokes, the snares and the tuning
	{
		std::vector<float> L, R;
		auto addSilence = [&](float s) { for (int i = 0; i < (int) (s * SR); i++) { L.push_back(0); R.push_back(0); } };
		const float bpm = 124.f, six = 60.f / bpm / 4.f;
		auto play = [&](Controls c, const char* pat, int bars, float extKick) {
			// pat: 16 chars per bar. 'X' loud, 'x' medium, '.' ghost, 'R' rimshot, 'C' cross-stick, 'B' buzz, '-' rest
			Engine e; e.setSampleRate(SR);
			for (int i = 0; i < (int) (0.4f * SR); i++) e.process(c);
			int n16 = bars * 16, len = (int) (six * SR);
			float kp = 0.f, kenv = 0.f;
			for (int s = 0; s < n16; s++) {
				char ch = pat[s % 16];
				Controls cc = c;
				if (ch == 'X') e.hit(0.95f, HEAD, cc);
				if (ch == 'x') e.hit(0.7f, HEAD, cc);
				if (ch == '.') e.hit(0.28f + 0.06f * (s % 3), HEAD, cc);
				if (ch == 'R') e.hit(0.9f, RIMSHOT, cc);
				if (ch == 'C') e.hit(0.75f, CROSS, cc);
				if (ch == 'B') { cc.buzz = 0.75f; e.hit(0.6f, HEAD, cc); }
				if (extKick > 0.f && s % 4 == 0) { kenv = 1.f; kp = 0.f; }
				for (int i = 0; i < len; i++) {
					// a simple kick drum in the room, also heard by the snare (EXT)
					float kf = 48.f + 120.f * kenv * kenv * kenv;
					kp += kf / SR; kenv *= 0.99985f;
					float kick = std::sin(2.f * PI * kp) * kenv;
					e.process(cc, extKick * kick);
					float k = extKick > 0.f ? 1.2f * kick : 0.f;
					L.push_back(e.outMix + k); R.push_back(e.outMix + k);
				}
			}
			for (int i = 0; i < (int) (0.6f * SR); i++) { e.process(c); L.push_back(e.outMix); R.push_back(e.outMix); }
		};
		Controls c = c0;
		// single strokes: plain, rimshot, cross-stick, buzz
		{
			Engine e; e.setSampleRate(SR);
			for (int i = 0; i < (int) (0.4f * SR); i++) e.process(c);
			const int seq[] = {HEAD, HEAD, RIMSHOT, CROSS, HEAD};
			const float vel[] = {0.4f, 0.85f, 0.85f, 0.75f, 0.6f};
			for (int h = 0; h < 5; h++) {
				Controls cc = c; if (h == 4) cc.buzz = 0.8f;
				e.hit(vel[h], seq[h], cc);
				for (int i = 0; i < (int) (0.9f * SR); i++) { e.process(cc); L.push_back(e.outMix); R.push_back(e.outMix); }
			}
		}
		addSilence(0.2f);
		play(c, "----X--.-.--X-.-", 2, 0.f);                          // backbeat with ghost notes
		Controls t = c; t.tuneHz = 330.f; t.size = 13.f; t.depth = 3.5f; t.tension = 0.9f; t.shell = 0.85f;
		play(t, "--.-R-.---.-R..-", 2, 0.f);                          // tight piccolo, rimshots
		Controls f = c; f.tuneHz = 135.f; f.depth = 8.f; f.tension = 0.15f; f.damp = 0.45f;
		play(f, "----X-------X--x", 2, 0.f);                          // fat, low, loose
		play(c, "--C---C---C--C--", 2, 0.f);                          // cross-stick
		play(c, "BBBBBBBBBBBBBBBB", 1, 0.f);                          // press roll
		Controls s = c; s.snares = false;
		play(s, "----X--.-.--X-.-", 1, 0.f);                          // snares off: a tom-like drum
		play(c, "----------------", 2, 40.f);                         // only a kick: the snare buzzes in sympathy
		addSilence(0.3f);
		std::string p = dir + "/rimbaud-demo.wav";
		writeWav(p, L, R);
		std::printf("wrote %s (%.1f s)\n", p.c_str(), L.size() / SR);
	}

	std::printf(fails ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", fails);
	return fails ? 1 : 0;
}
