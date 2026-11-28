// Crate DSP: everything that makes the sound, with no Rack dependency so it can
// be rendered and tested offline (test/render.cpp). Spec: vcv-modules/crate.md.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace crate {

static const double SP_RATE = 26040.0;   // the fixed DAC / ADC clock

// ---- small helpers -------------------------------------------------------

inline float softClip(float x) {
	float a = std::fabs(x);
	if (a < 0.8f) return x;
	float y = 0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f);
	return x < 0.f ? -y : y;
}

inline int16_t quantise12(double v) {
	double c = std::round(v * 2047.0);
	if (c > 2047.0) c = 2047.0;
	if (c < -2048.0) c = -2048.0;
	return (int16_t)c;
}

inline uint64_t mix64(uint64_t z) {
	z += 0x9E3779B97F4A7C15ull;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
	return z ^ (z >> 31);
}

struct Rng {
	uint64_t s = 1;
	void seed(uint64_t v) { s = mix64(v) | 1; }
	uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
	float uni() { return (next() & 0xFFFFFF) / 16777216.f; }          // 0..1
	float bi() { return uni() * 2.f - 1.f; }                          // -1..1
};

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(double fc, double fs, double q) {
		double w = 2.0 * M_PI * fc / fs;
		double cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
		double a0 = 1.0 + alpha;
		b0 = (1.0 - cw) / 2.0 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
		a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
	}
	double process(double x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
};

struct Butter4 {
	Biquad s[2];
	void set(double fc, double fs) {
		fc = std::min(fc, 0.45 * fs);
		s[0].lowpass(fc, fs, 0.54119610);
		s[1].lowpass(fc, fs, 1.30656296);
	}
	double process(double x) { return s[1].process(s[0].process(x)); }
	void reset() { s[0].reset(); s[1].reset(); }
};

// ---- capture: source audio -> 12-bit memory at 26.04 kHz (spec 3.3) --------

struct CaptureResult {
	std::vector<int16_t> codes;
	bool clipped = false;
};

// k = SPEED factor (1, 1.35, 2.34), gain linear, maxSec = memory length.
inline CaptureResult capture(const std::vector<float>& src, double fsSrc, float gain, double k, double maxSec) {
	CaptureResult r;
	size_t L = src.size();
	if (L < 4 || fsSrc <= 0) return r;

	// x4 polyphase upsampler, 64-tap Blackman-windowed sinc
	const int PH = 4, TAPS = 16, NT = PH * TAPS;
	double h[NT];
	const double D = (NT - 1) / 2.0;
	for (int t = 0; t < NT; t++) {
		double x = (t - D) / PH * 0.9;            // cutoff 0.45 fs_src
		double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
		double w = 0.42 - 0.5 * std::cos(2 * M_PI * t / (NT - 1)) + 0.08 * std::cos(4 * M_PI * t / (NT - 1));
		h[t] = s * w;
	}
	for (int p = 0; p < PH; p++) {                 // unity gain per phase
		double sum = 0;
		for (int j = 0; j < TAPS; j++) sum += h[p + PH * j];
		for (int j = 0; j < TAPS; j++) h[p + PH * j] /= sum;
	}

	const double fs4 = fsSrc * PH;
	Butter4 lp;
	lp.set(std::min(11000.0 / k, 0.45 * fsSrc), fs4);

	const double step = k * fs4 / SP_RATE;
	const size_t L4 = L * PH;
	size_t Nmax = (size_t)(maxSec * SP_RATE);
	double avail = (double)L4 - D - 3.0;
	if (avail <= 0) return r;
	size_t N = std::min((size_t)std::floor(avail / step), Nmax);
	r.codes.reserve(N);

	double ring[4] = {0, 0, 0, 0};
	double p = D;                                  // compensates the upsampler delay
	for (size_t n = 0; n < L4 && r.codes.size() < N; n++) {
		int ph = (int)(n % PH);
		long base = (long)(n / PH);
		double u = 0;
		for (int j = 0; j < TAPS; j++) {
			long idx = base - j;
			if (idx >= 0 && idx < (long)L) u += h[ph + PH * j] * src[idx];
		}
		double g = gain * u;
		if (std::fabs(g) > 0.8) r.clipped = true;
		ring[n & 3] = lp.process(softClip((float)g));

		while (r.codes.size() < N) {
			size_t i = (size_t)std::floor(p);
			if (n < i + 2) break;
			double t = p - (double)i;
			double xm1 = ring[(i - 1) & 3], x0 = ring[i & 3], x1 = ring[(i + 1) & 3], x2 = ring[(i + 2) & 3];
			double c1 = 0.5 * (x1 - xm1);
			double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
			double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
			double v = ((c3 * t + c2) * t + c1) * t + x0;
			r.codes.push_back(quantise12(v));
			p += step;
		}
	}
	return r;
}

// ---- half-band decimator (2 -> 1), 31 taps, Kaiser -----------------------

struct HalfBand {
	static const int NT = 31;
	float h[NT];
	float buf[NT * 2] = {};
	int w = 0;
	HalfBand() {
		auto i0 = [](double x) {
			double s = 1, t = 1;
			for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
			return s;
		};
		const double beta = 8.0, c = (NT - 1) / 2.0;
		double sum = 0;
		for (int n = 0; n < NT; n++) {
			double x = (n - c) / 2.0;
			double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
			double r = (n - c) / c;
			h[n] = (float)(0.5 * s * i0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / i0(beta));
			sum += h[n];
		}
		for (int n = 0; n < NT; n++) h[n] = (float)(h[n] / sum);
	}
	void push(float x) {
		buf[w] = x; buf[w + NT] = x;
		w = (w + 1) % NT;
	}
	float out() const {
		float y = 0;
		for (int n = 0; n < NT; n++) y += h[n] * buf[w + n];
		return y;
	}
	void reset() { std::fill(buf, buf + NT * 2, 0.f); }
};

// ---- the voice (spec 5-8, 10) --------------------------------------------

struct Frame {
	bool trig = false;          // rising edge this sample
	float ratioSemis = 0.f;     // TUNE + mutation + V/OCT*12, in semitones
	double k = 1.0;             // SPEED factor
	float vel = 1.f;
	float decay = 1.f;          // 0..1
	float start = 0.f, end = 1.f;
	bool loop = false;
	int channel = 0;            // 0 = 1·2, 1 = 3·4, 2 = 5·6, 3 = 7·8
	float level = 0.8f;
	float age = 0.f;
	bool thru = false;          // THRU or recording
	bool trigConnected = false;
	float in = 0.f;             // volts
	float gain = 1.f;           // linear
};

struct Result {
	float out = 0.f;            // volts
	bool end = false;           // reached END this sample
	bool inClip = false;
};

struct Voice {
	// memory (owned elsewhere)
	const int16_t* mem = nullptr;
	size_t N = 0;

	double fsE = 48000, fsI = 96000;
	int os = 2;
	uint32_t seed = 1;

	// playback
	double clk = 0, pos = 0;
	bool playing = false;
	size_t startIdx = 0;
	float vHeld = 0.f, prevOut = 0.f;
	int16_t lastCode = 0;

	// envelope
	enum Stage { OFF, ATTACK, DECAY, RELEASE };
	Stage stage = OFF;
	float env = 0.f, vel = 1.f, eSmooth = 0.f, thruGate = 0.f;

	// thru
	float lastIn = 0.f;
	double thruPrev = 0, thruCur = 0;
	Butter4 thruLP;

	// filters
	double s12[4] = {0, 0, 0, 0};
	Butter4 f34, f56;
	float lastScale = -1.f;
	int chanCur = 0, chanPrev = 0;
	float xfade = 1.f;
	float dcX = 0.f, dcY = 0.f, dcR = 0.999f;
	HalfBand dec;
	int decPhase = 0;

	// AGE
	float dacTable[4096];
	float tableAge = -1.f;
	float ek[12];
	float w1 = 0, w1t = 0, w2 = 0, w2t = 0;
	int walkCount = 0;
	Rng rng;
	int block = 0;
	float drift = 0.f, ageScale = 1.f;

	Voice() {
		setSeed(1);
		setSampleRate(48000);
	}

	void setSeed(uint32_t s) {
		seed = s;
		Rng r; r.seed(s);
		for (int b = 0; b < 12; b++) ek[b] = r.bi() * (b <= 7 ? 0.02f : 0.006f);
		rng.seed(s ^ 0xA5A5A5A5u);
		tableAge = -1.f;
	}

	void setSampleRate(double fs) {
		fsE = fs;
		os = fs < 88000 ? 2 : 1;
		fsI = fs * os;
		thruLP.set(11000, fsI); thruLP.reset();
		f34.reset(); f56.reset();
		lastScale = -1.f;
		for (double& s : s12) s = 0;
		dcR = 1.f - (float)(2 * M_PI * 5.0 / fsI);
		dcX = dcY = 0;
		dec.reset();
		vHeld = prevOut = 0;
		clk = 0;
	}

	void setMemory(const int16_t* m, size_t n) {
		mem = m; N = n;
		playing = false; stage = OFF; env = 0;
		pos = 0;
	}

	bool active() const { return playing || stage != OFF; }

	// Fade out quickly so memory can be swapped (spec 11).
	void fadeOut() {
		if (playing) playing = false;
		if (stage != OFF) stage = RELEASE;
	}

	void buildTable(float a) {
		auto S = [&](int u) {
			double s = 0;
			for (int b = 0; b < 12; b++) if (u & (1 << b)) s += (double)(1 << b) * (1.0 + a * ek[b]);
			return s;
		};
		double off = S(2048) / 2048.0 - 1.0;
		for (int u = 0; u < 4096; u++) dacTable[u] = (float)(S(u) / 2048.0 - 1.0 - off);
		tableAge = a;
	}

	int16_t readMem(size_t idx, float a) const {
		int16_t c = mem[idx];
		if (a > 0.6f) {
			uint64_t hsh = mix64(((uint64_t)seed << 32) ^ (uint64_t)idx);
			float prob = 0.004f * (a - 0.6f) / 0.4f;
			if ((hsh & 0xFFFF) / 65536.f < prob) {
				int bit = (int)((hsh >> 16) % 6);
				int u = (c + 2048) ^ (1 << bit);
				c = (int16_t)(u - 2048);
			}
		}
		return c;
	}

	size_t idxOf(float frac) const {
		frac = std::min(std::max(frac, 0.f), 1.f);
		return std::min((size_t)(frac * (double)N), N);
	}

	void controlBlock(const Frame& f) {
		float a = f.age;
		if (std::fabs(a - tableAge) > 0.002f || (a == 0.f && tableAge != 0.f)) buildTable(a);
		// slow random walks for clock and filter drift (~0.15 Hz)
		double blockDt = 32.0 / fsI;
		if (--walkCount <= 0) {
			walkCount = (int)(fsI / 32.0 / 0.3);
			w1t = rng.bi(); w2t = rng.bi();
		}
		float c = (float)(1.0 - std::exp(-2 * M_PI * 0.15 * blockDt));
		w1 += (w1t - w1) * c;
		w2 += (w2t - w2) * c;
		drift = a * (-0.008f + 0.006f * w1);
		ageScale = std::pow(2.f, -0.6f * a) * (1.f + 0.05f * a * w2);
		if (a == 0.f) { drift = 0.f; ageScale = 1.f; }
		if (ageScale != lastScale) {
			f34.set(5000.0 * ageScale, fsI);
			f56.set(10000.0 * ageScale, fsI);
			lastScale = ageScale;
		}
	}

	Result process(const Frame& f) {
		Result res;
		double ratio = std::pow(2.0, f.ratioSemis / 12.0) / f.k;
		ratio = std::min(std::max(ratio, 1.0 / 16.0), 8.0);

		size_t endIdx = 0;
		auto computeEnd = [&]() {
			endIdx = idxOf(f.end);
			if (endIdx < startIdx + 16) endIdx = std::min(N, startIdx + 16);
		};

		// trigger
		if (f.trig) {
			if (N >= 16) {
				startIdx = idxOf(f.start);
				if (startIdx + 16 > N) startIdx = N - 16;
				pos = (double)startIdx;
				playing = true;
				stage = ATTACK; env = 0.f; vel = f.vel;
			}
			else if (f.thru) {
				stage = ATTACK; env = 0.f; vel = f.vel;
			}
		}
		if (N >= 16) computeEnd();

		if (f.channel != chanCur) {
			chanPrev = chanCur; chanCur = f.channel; xfade = 0.f;
		}

		const float atkInc = (float)(1.0 / (0.0003 * fsI));
		const float relInc = (float)(1.0 / (0.001 * fsI));
		float decCoef = 1.f;
		if (f.decay < 0.98f) {
			double tau = 0.02 * std::pow(500.0, f.decay / 0.98);
			decCoef = (float)std::exp(-1.0 / (tau * fsI));
		}
		const float eCoef = (float)(1.0 - std::exp(-1.0 / (0.002 * fsI)));
		const float gCoef = (float)(1.0 - std::exp(-1.0 / (0.001 * fsI)));
		const float xfInc = (float)(1.0 / (0.005 * fsI));
		const float noiseAmp = f.age > 0.f ? std::pow(10.f, (-78.f + 24.f * f.age) / 20.f) * 1.7320508f : 0.f;

		float inNow = std::isfinite(f.in) ? f.in : 0.f;
		float outE = 0.f;

		for (int i = 0; i < os; i++) {
			if (block-- <= 0) { controlBlock(f); block = 31; }

			// thru path input (linear interpolation up to the internal rate)
			float inI = (os == 2 && i == 0) ? 0.5f * (lastIn + inNow) : inNow;
			float xg = inI / 5.f * f.gain;
			if (std::fabs(xg) > 0.8f) res.inClip = true;
			thruPrev = thruCur;
			thruCur = thruLP.process(softClip(xg));

			// fixed-rate DAC clock with drop-sample memory read
			double inc = SP_RATE * (1.0 + drift) / fsI;
			clk += inc;
			float cNow = 0.f, cPrev = 0.f;
			while (clk >= 1.0) {
				clk -= 1.0;
				double d = clk / inc;            // how long ago the tick happened, 0..1
				int16_t code = 0;
				if (playing) {
					code = readMem((size_t)pos, f.age);     // NO interpolation
					lastCode = code;
					pos += ratio;
					if (pos >= (double)endIdx) {
						if (f.loop) {
							size_t ns = idxOf(f.start);
							if (ns + 16 <= endIdx) startIdx = ns;
							pos -= (double)(endIdx - startIdx);
							if (pos >= (double)endIdx || pos < (double)startIdx) pos = (double)startIdx;
						}
						else {
							playing = false;
							stage = RELEASE;
							res.end = true;
						}
					}
				}
				else if (f.thru) {
					code = quantise12(thruCur * (1.0 - d) + thruPrev * d);
				}
				else if (stage == RELEASE) {
					code = lastCode;
				}
				float vNew = dacTable[code + 2048];
				float h = vNew - vHeld;
				float dd = (float)d;
				cNow += -h * (1.f - dd) * (1.f - dd) * 0.5f;
				cPrev += h * dd * dd * 0.5f;
				vHeld = vNew;
			}
			float stair = vHeld + cNow;
			prevOut += cPrev;
			float sig = prevOut;
			prevOut = stair;

			if (noiseAmp > 0.f) sig += noiseAmp * rng.bi();

			// envelope
			switch (stage) {
				case ATTACK: env += atkInc; if (env >= 1.f) { env = 1.f; stage = DECAY; } break;
				case DECAY:
					env *= decCoef;
					if (env < 3.2e-5f) { env = 0.f; stage = OFF; playing = false; }
					break;
				case RELEASE: env -= relInc; if (env <= 0.f) { env = 0.f; stage = OFF; } break;
				default: break;
			}
			float gateT = (f.thru && !f.trigConnected && !playing) ? 1.f : 0.f;
			thruGate += (gateT - thruGate) * gCoef;
			float e = std::max(env, thruGate);
			float amp = e * f.level * (stage != OFF ? vel : 1.f);
			float y = sig * amp;

			// channel filters (all run, so switching is click-free)
			eSmooth += (e - eSmooth) * eCoef;
			double fc12 = 250.0 * std::pow(14000.0 / 250.0, (double)eSmooth) * ageScale;
			fc12 = std::min(fc12, 0.45 * fsI);
			double g = std::tan(M_PI * fc12 / fsI), G = g / (1.0 + g);
			double v12 = std::tanh(1.2 * y) / 1.2;
			for (int s = 0; s < 4; s++) {
				double v = (v12 - s12[s]) * G;
				double o = v + s12[s];
				s12[s] = o + v;
				v12 = o;
			}
			float o[4];
			o[0] = (float)v12;
			o[1] = (float)f34.process(y);
			o[2] = (float)f56.process(y);
			o[3] = y;
			float yo;
			if (xfade < 1.f) {
				xfade = std::min(1.f, xfade + xfInc);
				yo = o[chanPrev] + (o[chanCur] - o[chanPrev]) * xfade;
			}
			else yo = o[chanCur];

			// DC blocker
			float dcOut = yo - dcX + dcR * dcY;
			dcX = yo; dcY = dcOut;

			if (os == 2) {
				dec.push(dcOut);
				if (i == 1) outE = dec.out();
			}
			else outE = dcOut;
		}
		lastIn = inNow;
		res.out = std::isfinite(outE) ? outE * 5.f : 0.f;
		if (!std::isfinite(outE)) { for (double& s : s12) s = 0; f34.reset(); f56.reset(); dec.reset(); }
		return res;
	}
};

// ---- WAV I/O (mono-summed float) ----------------------------------------

inline bool readWav(const char* path, std::vector<float>& out, double& fs, double maxSec) {
	FILE* fp = std::fopen(path, "rb");
	if (!fp) return false;
	std::vector<uint8_t> d;
	std::fseek(fp, 0, SEEK_END);
	long len = std::ftell(fp);
	std::fseek(fp, 0, SEEK_SET);
	if (len < 44 || len > 512L * 1024 * 1024) { std::fclose(fp); return false; }
	d.resize(len);
	size_t got = std::fread(d.data(), 1, len, fp);
	std::fclose(fp);
	if ((long)got != len || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
	auto u16 = [&](size_t o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8); };
	auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
	int fmt = 0, ch = 0, bits = 0;
	uint32_t rate = 0;
	size_t dataOff = 0, dataLen = 0;
	size_t o = 12;
	while (o + 8 <= d.size()) {
		uint32_t sz = u32(o + 4);
		if (!std::memcmp(&d[o], "fmt ", 4) && o + 8 + 16 <= d.size()) {
			fmt = (int)u16(o + 8); ch = (int)u16(o + 10); rate = u32(o + 12); bits = (int)u16(o + 22);
			if (fmt == 0xFFFE && sz >= 40 && o + 8 + 26 <= d.size()) fmt = (int)u16(o + 8 + 24);
		}
		else if (!std::memcmp(&d[o], "data", 4)) {
			dataOff = o + 8;
			dataLen = std::min((size_t)sz, d.size() - dataOff);
			break;
		}
		o += 8 + sz + (sz & 1);
	}
	if (!dataOff || ch < 1 || rate < 1000 || !(fmt == 1 || fmt == 3)) return false;
	int bps = bits / 8;
	if (bps < 1 || bps > 8) return false;
	size_t frames = dataLen / (bps * ch);
	frames = std::min(frames, (size_t)(maxSec * rate));
	out.assign(frames, 0.f);
	const uint8_t* p = d.data() + dataOff;
	for (size_t i = 0; i < frames; i++) {
		double sum = 0;
		for (int c = 0; c < ch; c++) {
			const uint8_t* s = p + (i * ch + c) * bps;
			double v = 0;
			if (fmt == 1) {
				if (bps == 1) v = (s[0] - 128) / 128.0;
				else {
					int32_t x = 0;
					for (int b = 0; b < std::min(bps, 4); b++) x |= (int32_t)s[bps - 1 - b] << (24 - 8 * b);
					v = x / 2147483648.0;
				}
			}
			else if (bps == 4) { float x; std::memcpy(&x, s, 4); v = x; }
			else if (bps == 8) { double x; std::memcpy(&x, s, 8); v = x; }
			sum += std::isfinite(v) ? v : 0.0;
		}
		out[i] = (float)(sum / ch);
	}
	fs = rate;
	return true;
}

inline bool writeWavFloat(const char* path, const std::vector<float>& x, double fs) {
	FILE* fp = std::fopen(path, "wb");
	if (!fp) return false;
	auto w32 = [&](uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; std::fwrite(b, 1, 4, fp); };
	auto w16 = [&](uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; std::fwrite(b, 1, 2, fp); };
	uint32_t dataLen = (uint32_t)(x.size() * 4);
	std::fwrite("RIFF", 1, 4, fp); w32(36 + dataLen); std::fwrite("WAVE", 1, 4, fp);
	std::fwrite("fmt ", 1, 4, fp); w32(16); w16(3); w16(1); w32((uint32_t)fs); w32((uint32_t)fs * 4); w16(4); w16(32);
	std::fwrite("data", 1, 4, fp); w32(dataLen);
	std::fwrite(x.data(), 4, x.size(), fp);
	bool ok = !std::ferror(fp);
	std::fclose(fp);
	return ok;
}

} // namespace crate
