// Facet wave ROM: every waveform is generated here in code when the plugin loads.
// No recordings and no data from any Roland instrument.
//
// Four kinds of wave:
//   LOOP    one band-limited cycle (10 mip levels so high notes never alias)
//   MOVING  12 band-limited cycles that the voice scans slowly back and forth, so the loop breathes
//   ATTACK  a short one-shot rendered from our own small physical models (pluck, mallet, breath...)
//   NOISE   a 1 second noise loop (its colour follows the key, like PCM noise does)
//   USER    a WAV file you load from the right-click menu (one-shot or loop)
#pragma once
#include <cmath>
#include <complex>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <functional>

namespace facet {

static const float PI_F = 3.14159265358979f;
static const int TBL = 2048;     // samples per cycle
static const int NMIP = 10;      // mip levels: 512, 256, ... 1 harmonics
static const int MAXH = 512;
static const int MOV_FRAMES = 12;
static const float SMP_SR = 48000.f;   // rate the one-shots and noise are rendered at
static const float ROOT_HZ = 261.6256f; // C4: one-shots play at their rendered pitch on C4
static const int NUM_USER = 4;

enum WaveKind { WK_LOOP, WK_MOVING, WK_ATTACK, WK_NOISE, WK_USER };

struct Wave {
	std::string name;
	int kind = WK_LOOP;
	int frames = 1;
	float scanHz = 0.f;          // MOVING: how fast the frames are scanned
	std::vector<float> tbl;      // frames * NMIP * (TBL + 1)
	std::vector<float> smp;      // ATTACK / NOISE / USER, at smpRate
	float smpRate = SMP_SR;
	bool loop = false;
	const float* table(int frame, int mip) const { return &tbl[((size_t) frame * NMIP + mip) * (TBL + 1)]; }
};

// ---------------------------------------------------------------- small FFT (radix 2, in place)
inline void fft(std::vector<std::complex<double>>& a, bool inverse) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) {
		size_t bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (size_t len = 2; len <= n; len <<= 1) {
		double ang = 2.0 * M_PI / (double) len * (inverse ? 1.0 : -1.0);
		std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (size_t i = 0; i < n; i += len) {
			std::complex<double> w(1.0);
			for (size_t k = 0; k < len / 2; k++) {
				std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
				a[i + k] = u + v;
				a[i + k + len / 2] = u - v;
				w *= wl;
			}
		}
	}
	if (inverse)
		for (auto& x : a) x /= (double) n;
}

// harmonic set: x(t) = sum_n Re(C[n] e^{i 2 pi n t}),  n = 1..MAXH
typedef std::vector<std::complex<double>> Harm;

inline Harm harmFromTime(const std::function<double(double)>& f) {
	const int N = 8192;
	std::vector<std::complex<double>> a(N);
	for (int i = 0; i < N; i++) a[i] = f((double) i / N);
	fft(a, false);
	Harm h(MAXH + 1);
	for (int n = 1; n <= MAXH; n++) h[n] = a[n] * (2.0 / N);
	return h;
}

// sine partials: amplitude amp(n), phase 0 (sin)
inline Harm harmFromAmps(const std::function<double(int)>& amp) {
	Harm h(MAXH + 1);
	for (int n = 1; n <= MAXH; n++) h[n] = std::complex<double>(0.0, -amp(n)); // sin = Re(-i e^{i..})
	return h;
}

// band-limited mips of one cycle into dst (NMIP * (TBL + 1) floats)
inline void buildMips(const Harm& h, float* dst) {
	std::vector<std::complex<double>> y(TBL);
	for (int m = 0; m < NMIP; m++) {
		int hmax = MAXH >> m;
		std::fill(y.begin(), y.end(), std::complex<double>(0.0));
		for (int n = 1; n <= hmax; n++) {
			// gentle roll-off over the top quarter of each mip, so mip cross-fades are smooth
			double g = 1.0;
			double top = hmax * 0.75;
			if (hmax > 4 && n > top) g = 0.5 + 0.5 * std::cos(M_PI * (n - top) / (hmax - top + 1));
			y[n] = h[n] * g * (TBL / 2.0);
			y[TBL - n] = std::conj(y[n]);
		}
		fft(y, true);
		float* t = dst + m * (TBL + 1);
		for (int i = 0; i < TBL; i++) t[i] = (float) y[i].real();
		t[TBL] = t[0];
	}
}

struct Rng {
	uint32_t s;
	explicit Rng(uint32_t seed) : s(seed * 2654435761u + 12345u) {}
	float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

// ---------------------------------------------------------------- the ROM
struct Rom {
	std::vector<Wave> waves;
	int firstUser = 0;

	static Rom& get() {
		static Rom rom;
		return rom;
	}

	int size() const { return (int) waves.size(); }

  private:
	void normalizeTable(Wave& w) {
		// RMS of mip 0 over all frames -> 0.3, then make sure nothing peaks above 1
		double rms = 0.0, peak = 0.0;
		for (int f = 0; f < w.frames; f++) {
			const float* t = w.table(f, 0);
			for (int i = 0; i < TBL; i++) {
				rms += t[i] * t[i];
				peak = std::max(peak, (double) std::fabs(t[i]));
			}
		}
		rms = std::sqrt(rms / (TBL * w.frames));
		if (rms < 1e-9) return;
		double g = 0.3 / rms;
		if (peak * g > 1.0) g = 1.0 / peak;
		for (auto& x : w.tbl) x = (float) (x * g);
	}

	void addLoop(const std::string& name, const Harm& h) {
		Wave w;
		w.name = name;
		w.kind = WK_LOOP;
		w.tbl.resize((size_t) NMIP * (TBL + 1));
		buildMips(h, w.tbl.data());
		normalizeTable(w);
		waves.push_back(std::move(w));
	}

	void addMoving(const std::string& name, float scanHz, const std::function<Harm(double)>& frame) {
		Wave w;
		w.name = name;
		w.kind = WK_MOVING;
		w.frames = MOV_FRAMES;
		w.scanHz = scanHz;
		w.tbl.resize((size_t) MOV_FRAMES * NMIP * (TBL + 1));
		for (int f = 0; f < MOV_FRAMES; f++) buildMips(frame((double) f / (MOV_FRAMES - 1)), &w.tbl[(size_t) f * NMIP * (TBL + 1)]);
		normalizeTable(w);
		waves.push_back(std::move(w));
	}

	void addSample(const std::string& name, int kind, bool loop, std::vector<float> x) {
		Wave w;
		w.name = name;
		w.kind = kind;
		w.loop = loop;
		float pk = 0.f;
		for (float v : x) pk = std::max(pk, std::fabs(v));
		if (pk > 0.f)
			for (float& v : x) v *= (kind == WK_NOISE ? 0.6f : 0.9f) / pk;
		// one-shots: 2 ms fade at the very end so nothing clicks when the sample runs out
		if (!loop) {
			int nf = std::min((int) x.size(), 96);
			for (int i = 0; i < nf; i++) x[x.size() - 1 - i] *= (float) i / nf;
		}
		w.smp = std::move(x);
		waves.push_back(std::move(w));
	}

	// ------------------------------------------------ helpers
	static double saw(double t) { return 2.0 * (t - std::floor(t)) - 1.0; }
	static double pulse(double t, double w) { return (t - std::floor(t)) < w ? 1.0 : -1.0; }
	// formant gain for harmonic n of C4
	static double formant(int n, const double* F, const double* B, const double* A, int k) {
		double f = n * ROOT_HZ, g = 0.0;
		for (int i = 0; i < k; i++) {
			double d = (f - F[i]) / B[i];
			g += A[i] / (1.0 + d * d);
		}
		return g;
	}
	static Harm vowel(int v, double tilt = 0.45) {
		// a, e, i, o, u  (F1 F2 F3)
		static const double FV[5][3] = {{800, 1150, 2900}, {480, 1720, 2520}, {300, 2250, 3000}, {450, 800, 2830}, {330, 640, 2400}};
		static const double B[3] = {90, 110, 170};
		static const double A[3] = {1.0, 0.6, 0.25};
		return harmFromAmps([=](int n) { return std::pow(n, -tilt) * (0.04 + formant(n, FV[v], B, A, 3)); });
	}
	static Harm vowelMix(double x, double tilt = 0.45) {
		// 0..1 across a e i o u
		static const double FV[5][3] = {{800, 1150, 2900}, {480, 1720, 2520}, {300, 2250, 3000}, {450, 800, 2830}, {330, 640, 2400}};
		static const double B[3] = {90, 110, 170};
		static const double A[3] = {1.0, 0.6, 0.25};
		double p = x * 4.0;
		int i = std::min(3, (int) p);
		double fr = p - i;
		double F[3];
		for (int k = 0; k < 3; k++) F[k] = FV[i][k] + (FV[i + 1][k] - FV[i][k]) * fr;
		return harmFromAmps([=](int n) { return std::pow(n, -tilt) * (0.04 + formant(n, F, B, A, 3)); });
	}

	// time-domain one-shot renderers ------------------------------------------
	static std::vector<float> karplus(double hz, double secs, double damp, double bright, uint32_t seed) {
		Rng r(seed);
		int n = (int) (SMP_SR * secs);
		double dl = SMP_SR / hz;
		int L = (int) dl + 2;
		std::vector<double> buf(L, 0.0);
		// noise burst, low-passed by 'bright'
		double lp = 0.0;
		for (int i = 0; i < L; i++) {
			lp += bright * (r.bi() - lp);
			buf[i] = lp;
		}
		std::vector<float> out(n);
		double w = 0.0;
		int wi = 0;
		double prev = 0.0;
		for (int i = 0; i < n; i++) {
			double rp = wi - dl;
			while (rp < 0) rp += L;
			int a = (int) rp;
			double f = rp - a;
			double s = buf[a % L] * (1 - f) + buf[(a + 1) % L] * f;
			double y = damp * (0.5 * (s + prev));
			prev = s;
			buf[wi] = y;
			wi = (wi + 1) % L;
			out[i] = (float) s;
			(void) w;
		}
		return out;
	}
	struct Mode {
		double ratio, amp, decay;
	};
	static std::vector<float> modal(double hz, double secs, const std::vector<Mode>& modes, double noiseMs = 0.0, double noiseAmt = 0.0, uint32_t seed = 1) {
		Rng r(seed);
		int n = (int) (SMP_SR * secs);
		std::vector<float> out(n, 0.f);
		for (const Mode& m : modes) {
			double f = hz * m.ratio;
			if (f > SMP_SR * 0.45) continue;
			double ph = r.uni() * 0.2;
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				out[i] += (float) (m.amp * std::exp(-t / m.decay) * std::sin(2 * M_PI * (f * t + ph)));
			}
		}
		if (noiseAmt > 0.0) {
			int nn = (int) (SMP_SR * noiseMs * 0.001);
			double hp = 0.0, prev = 0.0;
			for (int i = 0; i < std::min(n, nn * 4); i++) {
				double x = r.bi();
				hp = 0.7 * (hp + x - prev);
				prev = x;
				out[i] += (float) (noiseAmt * hp * std::exp(-(double) i / nn));
			}
		}
		// 1 ms fade-in so no wave starts with a step
		for (int i = 0; i < 48 && i < n; i++) out[i] *= i / 48.f;
		return out;
	}
	static void bandpass(std::vector<float>& x, double fc, double q) {
		double w = 2 * M_PI * fc / SMP_SR, al = std::sin(w) / (2 * q);
		double b0 = al, b2 = -al, a0 = 1 + al, a1 = -2 * std::cos(w), a2 = 1 - al;
		double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
		for (float& v : x) {
			double y = (b0 * v + b2 * x2 - a1 * y1 - a2 * y2) / a0;
			x2 = x1;
			x1 = v;
			y2 = y1;
			y1 = y;
			v = (float) y;
		}
	}
	static void onepoleLP(std::vector<float>& x, double fc) {
		double a = 1 - std::exp(-2 * M_PI * fc / SMP_SR), y = 0;
		for (float& v : x) v = (float) (y += a * (v - y));
	}

	Rom() {
		// ================= analog-style loops
		addLoop("Syn Saw", harmFromAmps([](int n) { return 1.0 / n; }));
		addLoop("Syn Square", harmFromAmps([](int n) { return n % 2 ? 1.0 / n : 0.0; }));
		addLoop("Pulse 25", harmFromTime([](double t) { return pulse(t, 0.25); }));
		addLoop("Pulse 12", harmFromTime([](double t) { return pulse(t, 0.125); }));
		addLoop("Pulse 5", harmFromTime([](double t) { return pulse(t, 0.05); }));
		addLoop("Triangle", harmFromAmps([](int n) { return n % 2 ? ((n / 2) % 2 ? -1.0 : 1.0) / (double) (n * n) : 0.0; }));
		addLoop("Sine", harmFromAmps([](int n) { return n == 1 ? 1.0 : 0.0; }));
		addLoop("Soft Saw", harmFromAmps([](int n) { return std::pow(n, -1.6); }));
		addLoop("Double Saw", harmFromAmps([](int n) { return 1.0 / n + (n % 2 ? 0.0 : 0.6 / (n / 2)); }));
		addLoop("Saw Square", harmFromAmps([](int n) { return 1.0 / n + (n % 2 ? 0.8 / n : 0.0); }));
		addLoop("Hollow Pulse", harmFromTime([](double t) { return pulse(t, 1.0 / 3.0); }));
		addLoop("Sync Lo", harmFromTime([](double t) { return saw(t * 2.3); }));
		addLoop("Sync Hi", harmFromTime([](double t) { return saw(t * 4.7) * (1.0 - 0.5 * t); }));
		addLoop("Organ 8 4", harmFromAmps([](int n) { return n == 1 ? 1.0 : n == 2 ? 0.8 : n == 4 ? 0.35 : 0.0; }));
		addLoop("Organ Full", harmFromAmps([](int n) {
			switch (n) {
				case 1: return 1.0; case 2: return 0.9; case 3: return 0.7; case 4: return 0.6;
				case 6: return 0.45; case 8: return 0.4; case 10: return 0.2; case 12: return 0.2; case 16: return 0.25;
				default: return 0.0;
			}
		}));
		addLoop("Reed", harmFromAmps([](int n) { return n % 2 ? 1.0 / n : 0.12 / n; }));

		// ================= digital and spectral loops
		addLoop("Glass", harmFromAmps([](int n) {
			switch (n) {
				case 1: return 1.0; case 3: return 0.3; case 7: return 0.42; case 12: return 0.36;
				case 19: return 0.26; case 27: return 0.18; case 38: return 0.1;
				default: return 0.0;
			}
		}));
		addLoop("Spectrum 1", harmFromAmps([](int n) { return (1.0 / n) * (1.0 + 4.0 * std::exp(-std::pow((n - 12) / 3.0, 2))); }));
		addLoop("Spectrum 2", harmFromAmps([](int n) {
			return (1.0 / std::sqrt(n)) * (0.15 + std::exp(-std::pow((n - 6) / 1.5, 2)) + 0.8 * std::exp(-std::pow((n - 24) / 4.0, 2)));
		}));
		addLoop("Wire", harmFromAmps([](int n) { return n % 2 ? std::pow(n, -0.5) * std::exp(-n / 90.0) : 0.0; }));
		addLoop("Bell Tone", harmFromAmps([](int n) {
			switch (n) {
				case 1: return 1.0; case 2: return 0.6; case 3: return 0.45; case 5: return 0.5;
				case 8: return 0.35; case 13: return 0.28; case 21: return 0.18;
				default: return 0.0;
			}
		}));
		addLoop("Digi Bell", harmFromTime([](double t) { return std::sin(2 * M_PI * t + 2.5 * std::sin(2 * M_PI * 3 * t)); }));
		addLoop("FM Bright", harmFromTime([](double t) { return std::sin(2 * M_PI * t + 3.0 * std::sin(2 * M_PI * t)); }));
		addLoop("FM Hollow", harmFromTime([](double t) { return std::sin(2 * M_PI * t + 2.0 * std::sin(2 * M_PI * 2 * t)); }));
		addLoop("FM Clang", harmFromTime([](double t) { return std::sin(2 * M_PI * 2 * t + 2.2 * std::sin(2 * M_PI * 7 * t)); }));
		addLoop("FM Tine", harmFromTime([](double t) { return std::sin(2 * M_PI * t + 0.8 * std::sin(2 * M_PI * 14 * t)); }));
		addLoop("FM Bass", harmFromTime([](double t) { return std::sin(2 * M_PI * t + 1.5 * std::sin(2 * M_PI * t + 0.6 * std::sin(2 * M_PI * t))); }));
		addLoop("Vox Ah", vowel(0));
		addLoop("Vox Ee", vowel(2));
		addLoop("Vox Oh", vowel(3));
		addLoop("Vox Oo", vowel(4));
		addLoop("Odd Comb", harmFromAmps([](int n) { return n % 2 ? std::pow(n, -0.8) : 0.0; }));
		addLoop("Even Comb", harmFromAmps([](int n) { return n == 1 ? 1.0 : (n % 2 ? 0.0 : std::pow(n, -0.8)); }));
		addLoop("Hollow Bell", harmFromAmps([](int n) {
			switch (n) {
				case 1: return 1.0; case 3: return 0.5; case 6: return 0.4; case 10: return 0.3; case 15: return 0.22; case 21: return 0.15;
				default: return 0.0;
			}
		}));
		addLoop("Crystal", harmFromAmps([](int n) { return n == 1 ? 0.35 : (n >= 8 && n <= 16 ? 1.0 / (n - 6) : 0.0); }));
		addLoop("Notch Saw", harmFromAmps([](int n) { return n % 5 == 0 ? 0.0 : 1.0 / n; }));
		addLoop("Digi Brass", harmFromAmps([](int n) {
			static const double F[2] = {1200, 2600}, B[2] = {500, 700}, A[2] = {1.0, 0.4};
			return (1.0 / n) * (0.3 + formant(n, F, B, A, 2));
		}));
		addLoop("Digi String", harmFromAmps([](int n) {
			static const double F[2] = {3000, 900}, B[2] = {1200, 400}, A[2] = {0.8, 0.4};
			return std::pow(n, -1.1) * (0.4 + formant(n, F, B, A, 2));
		}));
		addLoop("Ocarina", harmFromAmps([](int n) { return n == 1 ? 1.0 : n == 2 ? 0.08 : n == 3 ? 0.05 : 0.0; }));
		addLoop("Add Pad", harmFromAmps([](int n) { return std::pow(n, -1.5) * (n % 3 == 0 ? 0.5 : 1.0); }));
		addLoop("Bitwave 1", harmFromTime([](double t) { return std::floor(t * 16.0) / 7.5 - 1.0; }));
		addLoop("Bitwave 2", harmFromTime([](double t) { return std::sin(2 * M_PI * (std::floor(t * 8.0) + 0.5) / 8.0); }));
		addLoop("Fold Sine", harmFromTime([](double t) {
			double x = 2.6 * std::sin(2 * M_PI * t);
			while (x > 1.0 || x < -1.0) x = x > 1.0 ? 2.0 - x : -2.0 - x;
			return x;
		}));
		addLoop("Harm 3 5", harmFromAmps([](int n) { return n == 1 ? 1.0 : n == 3 ? 0.7 : n == 5 ? 0.6 : 0.0; }));
		addLoop("Nasal", harmFromAmps([](int n) {
			static const double F[2] = {1500, 3200}, B[2] = {200, 300}, A[2] = {1.0, 0.5};
			return std::pow(n, -0.6) * (0.1 + formant(n, F, B, A, 2));
		}));

		// ================= moving loops (12 frames each, scanned back and forth)
		// "shimmer": every harmonic gets its own slow amplitude wobble, which is how an ensemble sounds
		auto shimmer = [](double x, uint32_t seed, double depth, const std::function<double(int)>& base) {
			Rng r(seed);
			return harmFromAmps([=](int n) mutable {
				Rng rr(seed * 977u + (uint32_t) n * 131u);
				double ph = rr.uni();
				return base(n) * (1.0 - depth + depth * (0.5 + 0.5 * std::sin(2 * M_PI * (x + ph))));
			});
		};
		addMoving("Fat Saw", 0.7f, [=](double x) { return shimmer(x, 3, 0.55, [](int n) { return 1.0 / n; }); });
		addMoving("Fat Square", 0.6f, [=](double x) { return shimmer(x, 5, 0.55, [](int n) { return n % 2 ? 1.0 / n : 0.18 / n; }); });
		addMoving("String Ens", 0.45f, [=](double x) {
			return shimmer(x, 7, 0.6, [](int n) {
				static const double F[2] = {2800, 700}, B[2] = {1400, 300}, A[2] = {0.7, 0.3};
				return std::pow(n, -1.2) * (0.4 + formant(n, F, B, A, 2));
			});
		});
		addMoving("Choir Pad", 0.35f, [=](double x) {
			Harm a = vowelMix(0.15 + 0.6 * x, 0.5);
			Harm s = shimmer(x, 11, 0.5, [](int) { return 1.0; });
			for (int n = 1; n <= MAXH; n++) a[n] *= std::abs(s[n]);
			return a;
		});
		addMoving("Vox Morph", 0.25f, [](double x) { return vowelMix(x); });
		addMoving("Shimmer", 0.9f, [=](double x) {
			return shimmer(x, 13, 0.9, [](int n) { return n == 1 ? 0.6 : (n >= 4 && n <= 28 ? 0.6 / std::sqrt(n) : 0.0); });
		});
		addMoving("Metal Sweep", 0.3f, [](double x) {
			return harmFromAmps([=](int n) { return std::pow(n, -0.7) * std::fabs(std::cos(M_PI * n * (0.08 + 0.25 * x))); });
		});
		addMoving("Spectrum Swp", 0.2f, [](double x) {
			double c = 3.0 + 37.0 * x;
			return harmFromAmps([=](int n) { return 0.12 / n + std::exp(-std::pow((n - c) / (1.5 + c * 0.15), 2)) / std::sqrt(n); });
		});
		addMoving("Breath Pad", 0.3f, [](double x) {
			return harmFromAmps([=](int n) { return std::pow(n, -2.0 + 0.6 * x) * (n % 2 ? 1.0 : 0.3 + 0.7 * x); });
		});
		addMoving("Glass Pad", 0.5f, [=](double x) {
			return shimmer(x, 17, 0.8, [](int n) {
				switch (n) {
					case 1: return 1.0; case 2: return 0.2; case 4: return 0.35; case 7: return 0.4; case 11: return 0.3;
					case 16: return 0.25; case 23: return 0.18; case 31: return 0.12;
					default: return 0.0;
				}
			});
		});
		addMoving("Organ Leslie", 1.6f, [=](double x) {
			return shimmer(x, 19, 0.45, [](int n) {
				switch (n) {
					case 1: return 1.0; case 2: return 0.8; case 3: return 0.6; case 4: return 0.5; case 6: return 0.35; case 8: return 0.35;
					default: return 0.0;
				}
			});
		});
		addMoving("Brass Swell", 0.25f, [](double x) { return harmFromAmps([=](int n) { return std::pow(n, -(1.9 - 1.0 * x)); }); });
		addMoving("PWM", 0.5f, [](double x) { return harmFromTime([=](double t) { return pulse(t, 0.08 + 0.42 * x); }); });
		addMoving("Sync Sweep", 0.3f, [](double x) { return harmFromTime([=](double t) { return saw(t * (1.0 + 3.0 * x)); }); });
		addMoving("Digital Pad", 0.4f, [](double x) {
			int steps = 6 + (int) (26 * x);
			return harmFromTime([=](double t) { return std::sin(2 * M_PI * (std::floor(t * steps) + 0.5) / steps) * 0.7 + 0.3 * saw(t); });
		});

		// ================= attacks (one-shots, rendered at C4)
		const double C4 = ROOT_HZ;
		addSample("Pluck", WK_ATTACK, false, karplus(C4, 1.2, 0.996, 0.7, 21));
		addSample("Harp", WK_ATTACK, false, karplus(C4, 2.0, 0.999, 0.35, 23));
		addSample("Pizz", WK_ATTACK, false, karplus(C4, 0.45, 0.985, 0.5, 25));
		addSample("Mallet", WK_ATTACK, false, modal(C4, 0.9, {{1, 1.0, 0.35}, {3.93, 0.45, 0.08}, {9.54, 0.2, 0.03}}, 4, 0.3, 27));
		addSample("Vibe", WK_ATTACK, false, modal(C4, 2.0, {{1, 1.0, 1.2}, {4.0, 0.35, 0.3}, {10.0, 0.12, 0.08}}, 3, 0.15, 29));
		addSample("Bell Strike", WK_ATTACK, false, modal(C4, 2.5, {{0.5, 0.5, 1.6}, {1, 1.0, 1.2}, {1.19, 0.5, 0.9}, {1.5, 0.45, 0.7}, {2.0, 0.35, 0.6}, {2.52, 0.3, 0.4}, {3.01, 0.2, 0.3}, {4.2, 0.15, 0.2}}, 3, 0.2, 31));
		addSample("Glass Ping", WK_ATTACK, false, modal(C4, 1.4, {{1, 1.0, 0.7}, {2.76, 0.6, 0.35}, {5.40, 0.4, 0.18}, {8.93, 0.25, 0.1}}, 2, 0.1, 33));
		{
			Rng r(35);
			std::vector<Mode> ms;
			for (int i = 0; i < 12; i++) ms.push_back({1.0 + 7.0 * r.uni(), 0.3 + 0.7 * r.uni(), 0.1 + 0.6 * r.uni()});
			addSample("Metal Hit", WK_ATTACK, false, modal(C4, 1.2, ms, 6, 0.8, 37));
		}
		addSample("Hammer", WK_ATTACK, false, modal(C4, 0.25, {{8.1, 0.5, 0.02}, {13.2, 0.4, 0.015}}, 8, 1.0, 39));
		{
			std::vector<Mode> ms;
			for (int n = 1; n <= 10; n++) ms.push_back({n * std::sqrt(1.0 + 0.0004 * n * n), 1.0 / n, 0.5 / std::sqrt((double) n)});
			addSample("Piano Atk", WK_ATTACK, false, modal(C4, 0.7, ms, 5, 0.35, 41));
		}
		{
			int n = (int) (SMP_SR * 0.8);
			std::vector<float> x(n);
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				double idx = 1.2 * std::exp(-t / 0.04);
				x[i] = (float) (std::exp(-t / 0.5) * std::sin(2 * M_PI * C4 * t + idx * std::sin(2 * M_PI * C4 * 14 * t)));
			}
			addSample("EP Atk", WK_ATTACK, false, x);
		}
		addSample("Kalimba", WK_ATTACK, false, modal(C4, 0.9, {{1, 1.0, 0.4}, {5.4, 0.35, 0.06}, {9.0, 0.15, 0.03}}, 2, 0.2, 43));
		addSample("Steel Drum", WK_ATTACK, false, modal(C4, 1.2, {{1, 1.0, 0.6}, {2.0, 0.6, 0.45}, {3.02, 0.35, 0.3}, {4.1, 0.15, 0.15}}, 3, 0.2, 45));
		addSample("Timpani", WK_ATTACK, false, modal(C4 * 0.5, 1.6, {{1, 1.0, 0.8}, {1.5, 0.5, 0.6}, {1.98, 0.35, 0.5}, {2.44, 0.2, 0.4}}, 15, 0.6, 47));
		addSample("Wood Knock", WK_ATTACK, false, modal(C4, 0.15, {{1, 1.0, 0.04}, {2.8, 0.5, 0.02}}, 2, 0.3, 49));
		{
			Rng r(51);
			int n = (int) (SMP_SR * 0.06);
			std::vector<float> x(n);
			for (int i = 0; i < n; i++) x[i] = (float) (r.bi() * std::exp(-(double) i / (SMP_SR * 0.012)));
			bandpass(x, 2200, 1.5);
			addSample("Snap", WK_ATTACK, false, x);
		}
		{
			Rng r(53);
			int n = (int) (SMP_SR * 0.25);
			std::vector<float> x(n), s(n);
			for (int i = 0; i < n; i++) x[i] = (float) (r.bi() * std::exp(-(double) i / (SMP_SR * 0.05)));
			bandpass(x, 2500, 1.2);
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				x[i] += (float) (0.6 * std::min(1.0, t / 0.03) * std::exp(-t / 0.15) * std::sin(2 * M_PI * C4 * t));
			}
			addSample("Breath Chiff", WK_ATTACK, false, x);
		}
		{
			Rng r(55);
			int n = (int) (SMP_SR * 0.35);
			std::vector<float> x(n);
			for (int i = 0; i < n; i++) x[i] = (float) (r.bi() * std::exp(-(double) i / (SMP_SR * 0.03)));
			bandpass(x, 1800, 2.0);
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				double e = std::min(1.0, t / 0.05) * std::exp(-t / 0.25);
				x[i] += (float) (e * (std::sin(2 * M_PI * C4 * t) + 0.3 * std::sin(4 * M_PI * C4 * t)));
			}
			addSample("Flute Chiff", WK_ATTACK, false, x);
		}
		{
			Rng r(57);
			int n = (int) (SMP_SR * 0.4);
			std::vector<float> x(n);
			double ph = 0.0;
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				ph += C4 * (1.0 + 0.004 * r.bi()) / SMP_SR;
				double e = std::min(1.0, t / 0.02) * std::exp(-t / 0.18);
				x[i] = (float) (e * (saw(ph) * 0.6 + r.bi() * 0.4));
			}
			onepoleLP(x, 5000);
			addSample("Bow Scrape", WK_ATTACK, false, x);
		}
		{
			int n = (int) (SMP_SR * 0.45);
			std::vector<float> x(n);
			double ph = 0.0, lp = 0.0;
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				double semis = -1.2 * std::exp(-t / 0.04);
				ph += C4 * std::pow(2.0, semis / 12.0) / SMP_SR;
				double fc = 300 + 5000 * std::min(1.0, t / 0.06) * std::exp(-t / 0.3);
				double a = 1 - std::exp(-2 * M_PI * fc / SMP_SR);
				lp += a * (saw(ph) - lp);
				x[i] = (float) (lp * std::min(1.0, t / 0.01) * std::exp(-t / 0.35));
			}
			addSample("Brass Blat", WK_ATTACK, false, x);
		}
		{
			Rng r(59);
			int n = (int) (SMP_SR * 0.3);
			std::vector<float> x(n), v(n);
			for (int i = 0; i < n; i++) x[i] = (float) (i < SMP_SR * 0.025 ? r.bi() * 0.8 : 0.0);
			bandpass(x, 3200, 2.5);
			// voiced 'a' after the consonant: a pulse train through two formants
			double ph = 0.0;
			for (int i = 0; i < n; i++) {
				ph += C4 / SMP_SR;
				v[i] = (float) (pulse(ph, 0.12) * std::min(1.0, std::max(0.0, (i / SMP_SR - 0.02) / 0.03)) * std::exp(-(i / SMP_SR) / 0.2));
			}
			std::vector<float> v2 = v;
			bandpass(v, 800, 5);
			bandpass(v2, 1150, 6);
			for (int i = 0; i < n; i++) x[i] += 2.0f * v[i] + 1.2f * v2[i];
			addSample("Vox Ka", WK_ATTACK, false, x);
		}
		{
			int n = (int) (SMP_SR * 0.6);
			std::vector<float> x(n);
			const double iv[4] = {1.0, 1.4983, 2.0, 2.5198};
			double ph[4] = {0, 0.3, 0.6, 0.1}, lp = 0.0;
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR, s = 0.0;
				for (int k = 0; k < 4; k++) {
					ph[k] += C4 * iv[k] / SMP_SR;
					s += saw(ph[k]);
				}
				double fc = 400 + 7000 * std::exp(-t / 0.12);
				lp += (1 - std::exp(-2 * M_PI * fc / SMP_SR)) * (s - lp);
				x[i] = (float) (lp * std::min(1.0, t / 0.004) * std::exp(-t / 0.22));
			}
			addSample("Orch Stab", WK_ATTACK, false, x);
		}
		{
			int n = (int) (SMP_SR * 0.18);
			std::vector<float> x(n);
			double ph = 0.0;
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				ph += C4 * (1.0 + 7.0 * std::exp(-t / 0.02)) / SMP_SR;
				x[i] = (float) (std::sin(2 * M_PI * ph) * std::exp(-t / 0.07));
			}
			addSample("Digital Zap", WK_ATTACK, false, x);
		}
		{
			Rng r(61);
			int n = (int) (SMP_SR * 0.2);
			std::vector<float> x(n, 0.f);
			for (int k = 0; k < 40; k++) {
				int at = (int) (r.uni() * r.uni() * n * 0.8);
				double amp = r.bi();
				for (int j = 0; j < 30 && at + j < n; j++) x[at + j] += (float) (amp * std::exp(-j / 6.0) * (j % 2 ? -1 : 1));
			}
			addSample("Spark", WK_ATTACK, false, x);
		}
		addSample("Slap", WK_ATTACK, false, modal(C4 * 0.5, 0.3, {{1, 1.0, 0.08}, {2.0, 0.4, 0.04}, {7.3, 0.3, 0.01}}, 3, 0.7, 63));
		{
			int n = (int) (SMP_SR * 0.35);
			std::vector<float> x(n);
			double ph = 0.0, lp = 0.0;
			for (int i = 0; i < n; i++) {
				double t = i / SMP_SR;
				ph += C4 / SMP_SR;
				double fc = 800 + 9000 * std::exp(-t / 0.05);
				lp += (1 - std::exp(-2 * M_PI * fc / SMP_SR)) * (pulse(ph, 0.2) - lp);
				x[i] = (float) (lp * std::exp(-t / 0.15));
			}
			addSample("Clav Atk", WK_ATTACK, false, x);
		}

		// ================= noise loops (1 s)
		const int NN = (int) SMP_SR;
		{
			Rng r(71);
			std::vector<float> x(NN);
			for (auto& v : x) v = r.bi();
			addSample("White Noise", WK_NOISE, true, x);
		}
		{
			Rng r(73);
			std::vector<float> x(NN);
			double b0 = 0, b1 = 0, b2 = 0;
			for (auto& v : x) {
				double w = r.bi();
				b0 = 0.99765 * b0 + w * 0.0990460;
				b1 = 0.96300 * b1 + w * 0.2965164;
				b2 = 0.57000 * b2 + w * 1.0526913;
				v = (float) (b0 + b1 + b2 + w * 0.1848);
			}
			addSample("Pink Noise", WK_NOISE, true, x);
		}
		{
			std::vector<float> x(NN);
			const double f[6] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0}; // six square oscillators, cymbal-style
			for (int i = 0; i < NN; i++) {
				double s = 0;
				for (int k = 0; k < 6; k++) s += pulse(f[k] * 2.0 * i / SMP_SR, 0.5);
				x[i] = (float) s;
			}
			std::vector<float> y = x;
			bandpass(x, 7000, 1.0);
			bandpass(y, 3500, 1.5);
			for (int i = 0; i < NN; i++) x[i] += 0.6f * y[i];
			addSample("Metal Noise", WK_NOISE, true, x);
		}
		{
			Rng r(75);
			std::vector<float> x(NN);
			for (auto& v : x) v = r.bi();
			bandpass(x, 1000, 4.0);
			addSample("Band Noise", WK_NOISE, true, x);
		}
		{
			Rng r(77);
			std::vector<float> x(NN);
			for (int i = 0; i < NN; i++) x[i] = r.bi() * (float) (0.6 + 0.4 * std::sin(2 * M_PI * 3.0 * i / NN) * std::sin(2 * M_PI * 5.0 * i / NN));
			bandpass(x, 1600, 0.8);
			addSample("Breath Noise", WK_NOISE, true, x);
		}
		{
			Rng r(79);
			std::vector<float> x(NN, 0.f);
			for (int i = 0; i < NN; i++)
				if (r.uni() < 0.004f) x[i] = r.bi();
			onepoleLP(x, 6000);
			addSample("Crackle", WK_NOISE, true, x);
		}
		{
			Rng r(81);
			std::vector<float> x(NN);
			// resonant band sweeping once per loop, so the loop point is seamless
			double s1 = 0, s2 = 0;
			for (int i = 0; i < NN; i++) {
				double fc = 500 + 2500 * (0.5 - 0.5 * std::cos(2 * M_PI * i / NN));
				double g = std::tan(M_PI * fc / SMP_SR), k = 0.25;
				double v = r.bi();
				double hp = (v - (k + g) * s1 - s2) / (1 + g * (g + k));
				double bp = g * hp + s1;
				s1 = g * hp + bp;
				double lp = g * bp + s2;
				s2 = g * bp + lp;
				x[i] = (float) bp;
			}
			addSample("Wind", WK_NOISE, true, x);
		}
		{
			Rng r(83);
			std::vector<float> x(NN);
			double prev = 0;
			for (auto& v : x) {
				double w = r.bi();
				v = (float) (w - prev);
				prev = w;
			}
			addSample("Hiss", WK_NOISE, true, x);
		}

		// ================= user slots (empty until a WAV is loaded)
		firstUser = (int) waves.size();
		for (int i = 0; i < NUM_USER; i++) {
			Wave w;
			w.name = "User " + std::to_string(i + 1);
			w.kind = WK_USER;
			waves.push_back(std::move(w));
		}
	}
};

} // namespace facet
