#include "WaveTables.hpp"
#include "Dsp.hpp"
#include "../generated/Sounds.hpp"
#include <complex>
#include <mutex>
#include <functional>

namespace hk {

// top of each band's octave, Hz; a harmonic is kept while it stays below 12 kHz there
static int maxHarmForBand(int b, int cap) {
	float top = 32.7032f * std::exp2((float) (b + 1));
	int h = (int) std::floor(12000.f / top);
	return std::max(1, std::min(cap, h));
}

static void fft(std::vector<std::complex<double>>& a, bool inverse) {
	int n = (int) a.size();
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (int len = 2; len <= n; len <<= 1) {
		double ang = 2 * PI_D / len * (inverse ? 1 : -1);
		std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (int i = 0; i < n; i += len) {
			std::complex<double> w(1);
			for (int j = 0; j < len / 2; j++) {
				auto u = a[i + j], v = a[i + j + len / 2] * w;
				a[i + j] = u + v; a[i + j + len / 2] = u - v;
				w *= wl;
			}
		}
	}
}

// harmonic amplitude functions for the single-cycle waves (n >= 1)
static double singleAmp(const std::string& w, int n, uint32_t seed) {
	auto gauss = [](double x) { return std::exp(-x * x); };
	if (w == "SINE") return n == 1;
	if (w == "SAW") return n <= 64 ? 1.0 / n : 0;
	if (w == "SAW 2") return n <= 16 ? 1.0 / n : 0;
	if (w == "SQUARE") return (n & 1) ? 1.0 / n : 0;
	if (w == "SQR 2") return ((n & 1) && n <= 7) ? 1.0 / n : 0;
	if (w == "PULSE") return std::sin(0.25 * PI_D * n) / n;
	if (w == "PULSE 2") return std::sin(0.12 * PI_D * n) / n;
	if (w == "TRIANG") return (n & 1) ? (((n - 1) / 2) % 2 ? -1.0 : 1.0) / (double) (n * n) : 0;
	auto table = [&](std::initializer_list<std::pair<int, double>> t) { for (auto& p : t) if (p.first == n) return p.second; return 0.0; };
	if (w == "BELL") return table({{1, 1}, {4, .6}, {7, .35}, {11, .4}, {15, .2}, {21, .12}});
	if (w == "BELL 2") return table({{1, .8}, {3, .3}, {6, .6}, {9, .25}, {14, .3}, {19, .15}, {27, .1}});
	if (w == "BASS") return n == 1 ? 1.0 : (1.0 / n) * std::exp(-n / 6.0) * std::fabs(std::sin(n * PI_D / 5)) * 2;
	if (w == "BASS 2") return n == 1 ? 1.0 : (1.0 / n) * std::exp(-n / 12.0) * std::fabs(std::sin(n * PI_D / 7)) * 2;
	if (w == "PIANO") return n <= 40 ? std::fabs(std::sin(n * PI_D / 8)) * std::pow(n, -1.2) * std::exp(-n / 18.0) : 0;
	if (w == "EL.PNO") return table({{1, 1}, {2, .3}, {3, .1}, {5, .04}, {7, .03}});
	if (w == "E.PNO2") return table({{1, 1}, {2, .15}, {4, .2}, {6, .08}, {13, .12}, {14, .1}});
	if (w == "ORGAN") return table({{1, .8}, {2, 1}, {3, .7}, {4, .6}, {6, .2}, {8, .4}});
	if (w == "REED") return n <= 24 ? ((n & 1) ? 1.0 : 0.3) / n * (1 + 2 * gauss((n - 5) / 2.0)) : 0;
	if (w == "REED 2") return n <= 30 ? std::pow(n, -0.8) * (1 + 3 * gauss((n - 9) / 3.0)) : 0;
	if (w == "BRASS") return n <= 24 ? n * std::exp(-n / 3.0) : 0;
	if (w == "CLAV") return n <= 48 ? (std::sin(0.08 * PI_D * n) / n) * (1 + 0.5 * gauss((n - 10) / 4.0)) : 0;
	if (w == "VOICE 1" || w == "VOICE 2") {
		if (n > 32) return 0;
		const double F1[3] = {700, 1100, 2600}, B1[3] = {110, 130, 180}, G1[3] = {1, .6, .3};
		const double F2[3] = {350, 800, 2400}, B2[3] = {90, 120, 180}, G2[3] = {1, .4, .15};
		bool one = w == "VOICE 1";
		double s = 0;
		for (int k = 0; k < 3; k++) s += (one ? G1[k] : G2[k]) * gauss((130.8 * n - (one ? F1[k] : F2[k])) / (one ? B1[k] : B2[k]));
		return s / n;
	}
	if (w == "FORMT 3") return n <= 24 ? (1.0 / n) * (0.2 + 4 * gauss((n - 9) / 2.0)) : 0;
	if (w == "SYNTH 1") return n <= 40 ? (1.0 / n) * std::fabs(std::cos(n * PI_D / 6)) + 0.3 / n : 0;
	if (w == "SYNTH 2") return n <= 12 ? std::pow(n, -0.7) : 0;
	if (w == "GRIT 1") {
		if (n > 40) return 0;
		for (int f : {1, 2, 3, 5, 8, 13, 21, 34}) if (f == n) return 1.0 / n;
		return 0.1 / n;
	}
	if (w == "GRIT 2") return ((n & 1) && n <= 31) ? std::pow(n, -0.5) * (hash01(seed, n) < 0.5 ? -1 : 1) : 0;
	if (w == "GLINT 1") return table({{1, .5}, {8, .6}, {9, 1}, {10, .6}, {16, .3}, {17, .4}});
	if (w == "GLINT 3") return table({{1, .4}, {2, .3}, {12, .5}, {13, 1}, {14, .5}, {26, .3}});
	return 0;
}

static void finishBand(WaveBand& b, const std::vector<double>& x, double norm) {
	int n = (int) x.size();
	b.f16.resize(n); b.q8.resize(n); b.q6.resize(n); b.c8.resize(n + 1); b.c6.resize(n + 1);
	b.c8[0] = b.c6[0] = 0;
	for (int i = 0; i < n; i++) {
		double v = clampf((float) (x[i] * norm), -127.f, 127.f);
		b.f16[i] = (float) (std::round(v / 128.0 * 32767.0) / 32767.0);
		int q = (int) std::lround(v);
		b.q8[i] = (int8_t) q;
		int q6 = (int) std::lround(v / 4.0) * 4;
		q6 = std::max(-124, std::min(124, q6));
		b.q6[i] = (int8_t) q6;
		b.c8[i + 1] = b.c8[i] + q;
		b.c6[i + 1] = b.c6[i] + q6;
	}
}

static double peakOf(const std::vector<double>& x) { double p = 1e-12; for (double v : x) p = std::max(p, std::fabs(v)); return p; }

// spectrum (bins = cycles per loop) -> loop of n samples
static std::vector<double> fromSpectrum(const std::vector<double>& mag, int n, uint32_t seed, int maxBin) {
	std::vector<std::complex<double>> a(n);
	for (int k = 1; k < n / 2 && k <= maxBin; k++) {
		if (mag[k] == 0) continue;
		double ph = 2 * PI_D * hash01(seed, k);
		a[k] = std::polar(mag[k], ph);
		a[n - k] = std::conj(a[k]);
	}
	fft(a, true);
	std::vector<double> x(n);
	for (int i = 0; i < n; i++) x[i] = a[i].real();
	return x;
}

static std::vector<Wave> build() {
	std::vector<Wave> out(NUM_WAVES);
	for (int wi = 0; wi < NUM_WAVES; wi++) {
		std::string name = WAVE_NAMES[wi];
		Wave& W = out[wi];
		uint32_t seed = 4000 + wi;
		bool loop = name == "ALIEN" || name == "BREATH" || name == "STEAM" || name == "METAL" || name == "CHIME";
		bool one = name == "CLICK" || name == "THUMP" || name == "PLINK" || name == "PLUCK" || name == "MALLET" || name == "PICK 1" || name == "SLAP" || name == "CHIFF";
		if (!loop && !one) {
			W.kind = WK_SINGLE; W.len = 256;
			double norm = 0;
			for (int b = 0; b < 8; b++) {
				int mh = maxHarmForBand(b, 127);
				std::vector<double> x(256, 0.0);
				for (int h = 1; h <= mh; h++) {
					double a = singleAmp(name, h, seed);
					if (a == 0) continue;
					for (int i = 0; i < 256; i++) x[i] += a * std::sin(2 * PI_D * h * i / 256.0);
				}
				if (b == 0) norm = 120.0 / peakOf(x);
				finishBand(W.band[b], x, norm);
			}
		}
		else if (loop) {
			W.kind = WK_LOOP; W.len = 8192;
			const int N = 8192;
			double norm = 0;
			for (int b = 0; b < 8; b++) {
				int mh = maxHarmForBand(b, 127);
				int maxBin = mh * 32;
				std::vector<double> x(N, 0.0);
				if (name == "ALIEN") {
					for (int i = 0; i < N; i++) {
						double t = i / (double) N;
						double m = 0.5 - 0.5 * std::cos(2 * PI_D * t);
						// +-6 cents of wobble, two cycles per loop, integral zero so the loop stays 32 cycles
						double dev = 0.0035 * 32.0;
						double phase = 32.0 * t + dev * std::sin(2 * PI_D * 2 * t) / (2 * PI_D * 2);
						double s = 0;
						for (int h = 1; h <= std::min(mh, 32); h++) {
							double a = (1 - m) * singleAmp("VOICE 1", h, seed) + m * singleAmp("VOICE 2", h, seed);
							s += a * std::sin(2 * PI_D * h * phase);
						}
						x[i] = s;
					}
				}
				else {
					std::vector<double> mag(N / 2, 0.0);
					if (name == "BREATH") {
						for (int k = 1; k < N / 2; k++) { double r = k / 32.0; mag[k] = 1.0 / std::sqrt(1 + 1.5 * 1.5 * (r / 6 - 6 / r) * (r / 6 - 6 / r)); }
					}
					else if (name == "STEAM") {
						for (int k = 1; k < N / 2; k++) { double r = k / 32.0; mag[k] = 1.0 / std::sqrt(1 + std::pow(10.0 / r, 4)); }
					}
					else if (name == "METAL") {
						const double R[5] = {1, 2.76, 5.40, 8.93, 13.34}, A[5] = {1, .6, .4, .25, .15};
						for (int j = 0; j < 5; j++) mag[(int) std::lround(R[j] * 32)] += A[j] * N / 2;
					}
					else if (name == "CHIME") {
						const double R[7] = {1, 2, 3.01, 4.17, 5.43, 6.8, 8.21}, A[7] = {1, .7, .5, .45, .3, .2, .12};
						for (int j = 0; j < 7; j++) mag[(int) std::lround(R[j] * 32)] += A[j] * N / 2;
					}
					x = fromSpectrum(mag, N, seed, maxBin);
					if (name == "BREATH") {
						double p = peakOf(x);
						for (int i = 0; i < N; i++) x[i] = x[i] / p + 0.2 * std::sin(2 * PI_D * 32.0 * i / N);
					}
					if (name == "STEAM") {
						for (int i = 0; i < N; i++) x[i] *= 0.6 + 0.4 * std::sin(2 * PI_D * 3.0 * i / N);
					}
				}
				if (b == 0) norm = 120.0 / peakOf(x);
				finishBand(W.band[b], x, norm);
			}
		}
		else {
			W.kind = WK_ONESHOT; W.len = 4096;
			const int N = 4096;
			Noise nz; nz.seed(seed);
			std::vector<double> x(N, 0.0), noise(N);
			for (auto& v : noise) v = nz.next();
			auto bp = [&](double h, double q) {
				Biquad f; f.bandpass((float) h, (float) q, 256.f);
				std::vector<double> y(N);
				for (int i = 0; i < N; i++) y[i] = f.process((float) noise[i]);
				return y;
			};
			auto e = [](double t, double tau) { return std::exp(-t / tau); };
			std::vector<double> click(N);
			for (int i = 0; i < N; i++) { double t = i / 256.0; click[i] = ((t < 1 ? (2 * (t - std::floor(t)) - 1) : 0) + noise[i]) * e(t, 0.5); }
			if (name == "CLICK") x = click;
			else if (name == "THUMP") {
				Biquad lp; lp.lowpass(2.f, 0.7f, 256.f);
				for (int i = 0; i < N; i++) { double t = i / 256.0; x[i] = std::sin(2 * PI_D * 0.5 * t) * e(t, 3) + 0.5 * lp.process((float) noise[i]) * 4 * e(t, 1); }
			}
			else if (name == "PLINK") {
				for (int i = 0; i < N; i++) { double t = i / 256.0; x[i] = std::sin(2 * PI_D * t) * e(t, 4) + 0.6 * std::sin(2 * PI_D * 4.2 * t) * e(t, 2.5) + 0.4 * std::sin(2 * PI_D * 9.1 * t) * e(t, 1.5); }
			}
			else if (name == "PLUCK") {
				for (int i = 0; i < N; i++) { double t = i / 256.0, s = 0; for (int n = 1; n <= 100; n++) s += (1.0 / n) * std::sin(2 * PI_D * n * t) * e(t * n, 8); x[i] = s; }
			}
			else if (name == "MALLET") {
				for (int i = 0; i < N; i++) { double t = i / 256.0; x[i] = std::sin(2 * PI_D * 4 * t) * e(t, 2) + 0.5 * click[i]; }
			}
			else if (name == "PICK 1") {
				auto y = bp(8, 2);
				for (int i = 0; i < N; i++) { double t = i / 256.0; x[i] = 3 * y[i] * e(t, 1) + (2 * (t - std::floor(t)) - 1) * e(t, 3); }
			}
			else if (name == "SLAP") {
				for (int i = 0; i < N; i++) { double t = i / 256.0, ph = t - std::floor(t); x[i] = (ph < 0.05 ? 1.0 : -0.05) * e(t, 2) + 0.5 * noise[i] * e(t, 0.7); }
			}
			else if (name == "CHIFF") {
				auto y = bp(3, 2);
				for (int i = 0; i < N; i++) { double t = i / 256.0; x[i] = 3 * y[i] * std::min(1.0, t / 2.0) * e(t, 6); }
			}
			double norm = 120.0 / peakOf(x);
			for (int b = 0; b < 8; b++) {
				int mh = maxHarmForBand(b, 127);
				std::vector<double> y = x;
				if (mh < 120) {
					Biquad f1, f2; f1.lowpass((float) mh, 0.54f, 256.f); f2.lowpass((float) mh, 1.31f, 256.f);
					for (int i = 0; i < N; i++) y[i] = f2.process(f1.process((float) x[i]));
				}
				finishBand(W.band[b], y, norm);
			}
		}
	}
	return out;
}

const std::vector<Wave>& waveTables() {
	static std::vector<Wave> tables;
	static std::once_flag once;
	std::call_once(once, [] { tables = build(); });
	return tables;
}

} // namespace hk
