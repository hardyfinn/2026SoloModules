#pragma once
// Vinylizer Fable: the record chain as physics, no Rack dependency (used by the module and test/render.cpp).
//
// Signal order (internal units: 1.0 = 5 cm/s peak groove velocity; 5 V in at 0 dB cut level = 12 cm/s):
//   1 ELLIPTIC  bass to mono (console, before the lathe)
//     record curve (RIAA pre-emphasis, matched to the playback curve) x cut level
//   3 CUT       side-length high-pass, excursion limiter on groove displacement
//   2 HF LIMIT  cutter-protection limiter on treble acceleration
//   3 CUT       cutter head roll-off, coil heating, drive-amp saturation
//   4 MEDIUM    plating loss, wear, pre/post echo from neighbouring grooves
//   9 PLATTER   groove read through a modulated delay: eccentric hole, wow, flutter (+ warp wow)
//   -- 4x oversampled, in metres of groove-wall displacement --
//   6 TRACKING  tracking angle error, one-sided wall contact (mistracking) with tracking force
//   5 TRACING   the stylus as a circle rolling over the groove wall (exact geometry), scanning loss
//   7 CARTRIDGE coil inductance + load capacitance, tip resonance, crosstalk, azimuth
//   8 ARM       arm/compliance resonance high-pass, warps
//  10 PHONO     input overload, RIAA playback (with part tolerance), stage colour, subsonic
#include <cmath>
#include <vector>
#include <algorithm>
#include <complex>
#include <cstdint>

namespace vinyl {

static constexpr double kPi = 3.14159265358979323846;
static constexpr float kVref = 0.05f;  // m/s for internal 1.0 (the 5 cm/s reference level)
static constexpr float kInScale = 0.48f;  // 5 V in at 0 dB cut = 2.4 units = 12 cm/s peak, a normal hot programme peak

inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
	double z1 = 0, z2 = 0;
	inline float process(float xin) {
		double x = xin;
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return (float) y;
	}
	inline double processD(double x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
	void setAnalog(double B0, double B1, double B2, double A0, double A1, double A2, double K) {
		double K2 = K * K;
		double a0 = A0 + A1 * K + A2 * K2;
		b0 = (B0 + B1 * K + B2 * K2) / a0;
		b1 = (2 * B0 - 2 * B2 * K2) / a0;
		b2 = (B0 - B1 * K + B2 * K2) / a0;
		a1 = (2 * A0 - 2 * A2 * K2) / a0;
		a2 = (A0 - A1 * K + A2 * K2) / a0;
	}
	static double warpK(double fs, double f) {
		f = std::min(f, 0.45 * fs);
		double w = 2 * kPi * f;
		return w / std::tan(w / (2 * fs));
	}
	void lowpass(double fs, double f, double q) {
		f = std::min(f, 0.45 * fs);
		double w = 2 * kPi * f;
		setAnalog(1, 0, 0, 1, 1 / (q * w), 1 / (w * w), warpK(fs, f));
	}
	void highpass(double fs, double f, double q) {
		f = std::min(f, 0.45 * fs);
		double w = 2 * kPi * f;
		setAnalog(0, 0, 1 / (w * w), 1, 1 / (q * w), 1 / (w * w), warpK(fs, f));
	}
	void peak(double fs, double f, double q, double db) {
		f = std::min(f, 0.45 * fs);
		double w = 2 * kPi * f, A = std::pow(10.0, db / 40.0);
		setAnalog(1, A / (q * w), 1 / (w * w), 1, 1 / (A * q * w), 1 / (w * w), warpK(fs, f));
	}
	double mag(double fs, double f) const {
		std::complex<double> z1c = std::polar(1.0, -2 * kPi * f / fs), z2c = z1c * z1c;
		return std::abs((b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c));
	}
};

struct OnePole {
	float a = 1, y = 0;
	void set(float fs, float f) {
		f = std::min(f, 0.45f * fs);
		a = 1.f - std::exp(-2.f * (float) kPi * f / fs);
	}
	inline float lp(float x) { y += a * (x - y); return y; }
	inline float hp(float x) { return x - lp(x); }
};

// RIAA playback curve (3180 / 318 / 75 us) by matched-z, so its exact inverse (the record curve) is stable.
// Normalised to 0 dB at 1 kHz.
inline void riaa(Biquad& bq, double fs, double t1, double t2, double t3, bool record) {
	double p1 = std::exp(-1.0 / (t1 * fs)), z2 = std::exp(-1.0 / (t2 * fs)), p3 = std::exp(-1.0 / (t3 * fs));
	// playback: (1 - z2 q) / ((1 - p1 q)(1 - p3 q))
	double nb0 = 1, nb1 = -z2, nb2 = 0;
	double da1 = -(p1 + p3), da2 = p1 * p3;
	Biquad t;
	t.b0 = nb0; t.b1 = nb1; t.b2 = nb2; t.a1 = da1; t.a2 = da2;
	double g = 1.0 / t.mag(fs, 1000.0);
	if (!record) {
		bq.b0 = nb0 * g; bq.b1 = nb1 * g; bq.b2 = 0; bq.a1 = da1; bq.a2 = da2;
	}
	else {
		// inverse: (1 + da1 q + da2 q^2) / (g (1 - z2 q))
		bq.b0 = 1.0 / g; bq.b1 = da1 / g; bq.b2 = da2 / g; bq.a1 = -z2; bq.a2 = 0;
	}
}

inline double besselI0(double x) {
	double s = 1, t = 1;
	for (int k = 1; k < 40; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
	return s;
}

inline std::vector<float> designLowpass(int N, double fc, double beta) {
	std::vector<float> h(N);
	double c = (N - 1) / 2.0, sum = 0;
	for (int i = 0; i < N; i++) {
		double x = i - c;
		double s = (x == 0) ? 2 * fc : std::sin(2 * kPi * fc * x) / (kPi * x);
		double r = x / c;
		double w = besselI0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / besselI0(beta);
		h[i] = (float)(s * w);
		sum += h[i];
	}
	for (auto& v : h) v = (float)(v / sum);
	return h;
}

struct Upsampler {
	std::vector<float> h, x;
	int os = 1, taps = 1, hist = 1, pos = 0;
	void init(const std::vector<float>& hh, int osr) {
		h = hh; os = osr; taps = (int) h.size();
		hist = (taps + os - 1) / os;
		x.assign(hist * 2, 0.f); pos = 0;
	}
	void push(float v) { pos = (pos + 1) % hist; x[pos] = v; x[pos + hist] = v; }
	// phase k of the newest input
	float phase(int k) const {
		float acc = 0;
		int m = 0;
		for (int i = k; i < taps; i += os, m++) acc += h[i] * x[pos + hist - m];
		return acc * os;
	}
};

struct Downsampler {
	std::vector<float> h, x;
	int taps = 1, pos = 0;
	void init(const std::vector<float>& hh) { h = hh; taps = (int) h.size(); x.assign(taps * 2, 0.f); pos = 0; }
	void push(float v) { pos = (pos + 1) % taps; x[pos] = v; x[pos + taps] = v; }
	float out() const {
		float acc = 0;
		for (int i = 0; i < taps; i++) acc += h[i] * x[pos + taps - i];
		return acc;
	}
};

struct Ring {
	std::vector<float> b;
	int mask = 0, w = 0;
	void init(int minSize) {
		int n = 1;
		while (n < minSize) n <<= 1;
		b.assign(n, 0.f); mask = n - 1; w = 0;
	}
	inline void push(float v) { b[w] = v; w = (w + 1) & mask; }
	inline float at(int delay) const { return b[(w - 1 - delay) & mask]; }  // delay 0 = newest
	inline float frac(float d) const {  // Hermite, d >= 1
		int i = (int) d;
		float f = d - i;
		float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
		// samples go back in time as index grows: interpolate between y1 (newer) and y2 (older)
		float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
		return ((c3 * f + c2) * f + c1) * f + c0;
	}
};

// Smooth random: new random target at rate hz, cosine interpolation. Roughly +-1.
struct SmoothRandom {
	uint32_t s = 22222;
	float a = 0, b = 0, ph = 0;
	float rnd() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s / 4294967295.f) * 2.f - 1.f; }
	float next(float hz, float fs) {
		ph += hz / fs;
		if (ph >= 1.f) { ph -= std::floor(ph); a = b; b = rnd(); }
		float t = 0.5f - 0.5f * std::cos((float) kPi * ph);
		return a + (b - a) * t;
	}
};

struct Settings {
	bool ellOn = true; float ellFreq = 150, ellAmount = 1; int ellSlope = 1;
	bool hflOn = true; float hflThreshDb = 0, hflFreq = 6000, hflRelease = 0.05f;
	bool cutOn = true; float cutLevelDb = 0, sideMin = 12, head = 0.5f;
	int disc = 2, speed = 0;  // disc 0=7" 1=10" 2=12"; speed 0=33 1=45
	bool medOn = true; int medType = 0; float echo = 0.3f, wear = 0;
	bool trcOn = true; int stylus = 0; float trcAmount = 1;
	bool trkOn = true; float force = 2.5f, balance = 0; int align = 0;
	bool cartOn = true; int cartModel = 0; float loadPf = 300, sepDb = 20, azimuth = 0;
	bool armOn = true; float armRes = 10, armDamp = 0.5f, warpMm = 0;
	bool pltOn = true; int drive = 0; float eccMm = 0.15f, wow = 0.2f, flutter = 0.2f;
	bool phoOn = true; int phoType = 1; float phoGainDb = 0, tol = 0; int subsonic = 2;  // 0 off, 1 IEC 20 Hz, 2 18 Hz high-pass
	float needle = 0;
	bool truePreEcho = false;
};

// ---- MASTER expander: what the cutting engineer was handed ----
struct MasterSettings {
	bool on = false;
	int source = 0;       // 0 half-inch tape 30 ips, 1 quarter-inch 15 ips, 2 1630, 3 DAT, 4 CD master
	float drive = 0.3f, print = 0, nr = 0, hiss = 0.3f;
	float convert = 0.5f; // converter quality: 0 early, 1 transparent
	int bits = 1;         // 0 = 14 bit, 1 = 16 bit
	int emphasis = 0;     // 0 correct, 1 missed (bright), 2 doubled (dull)
	bool delayLine = false;
	float loudDb = 0;
};

struct MasterStage {
	MasterSettings s;
	float limGR = 0;  // dB, for a light

	void init(float sampleRate) {
		fs = sampleRate;
		print[0].init((int)(1.8f * fs)); print[1].init((int)(1.8f * fs));
		nrEnv = 0; limGain = 1; ctr = 0;
		update();
	}

	// l, r in volts
	void process(float& l, float& r) {
		if (--ctr <= 0) { ctr = 32; update(); }
		float x[2] = {l * 0.2f, r * 0.2f};
		bool tape = s.source <= 1;
		if (tape) {
			float hiA = 0;
			float lo[2], hi[2];
			for (int c = 0; c < 2; c++) {
				float v = bump[c].process(lfHP[c].process(x[c]));
				if (s.source == 1) v = hfLP[c].process(v);
				// record EQ lifts the treble before the oxide, so treble saturates first
				float e = v + emphK * empHP[c].hp(v);
				e = satT * std::tanh(e / satT);
				v = e / (1.f + emphK) + (emphK / (1.f + emphK)) * deLP[c].lp(e);
				lo[c] = nrSplit[c].lp(v); hi[c] = v - lo[c];
				hiA = std::max(hiA, std::fabs(hi[c]));
			}
			// noise reduction decoded at the wrong level: quiet treble comes out too bright or too dull
			nrEnv += (hiA - nrEnv) * (hiA > nrEnv ? nrAtk : nrRel);
			float g = dbToGain(nrDb * (1.f - std::min(1.f, nrEnv / 0.05f)));
			for (int c = 0; c < 2; c++) {
				float v = lo[c] + hi[c] * g;
				float n = rnd();
				v += hissAmp * (0.3f * n + hissHP[c].hp(n));
				print[c].push(v);
				if (printGain > 0) v += printGain * printHP[c].hp(printLP[c].lp(print[c].at(printDelay)));
				x[c] = v;
			}
		}
		if (!tape || s.delayLine) {
			for (int c = 0; c < 2; c++) {
				float v = x[c];
				if (aaOn) v = aa[c][2].process(aa[c][1].process(aa[c][0].process(v)));
				v += dnl * (v > 0 ? 1.f : -1.f);
				float d = dither ? (rnd() + rnd()) * 0.5f : 0.f;
				v = qStep * std::floor(v / qStep + 0.5f + d);
				x[c] = clampf(v, -2.f, 2.f);  // 0 dBFS = 10 V
			}
		}
		if (!tape && s.emphasis != 0)
			for (int c = 0; c < 2; c++) x[c] = emph[c].process(x[c]);
		if (s.loudDb > 0.05f) {
			float pk = std::max(std::fabs(x[0]), std::fabs(x[1])) * loudG;
			float target = pk > 1.f ? 1.f / pk : 1.f;
			if (target < limGain) limGain = target; else limGain += (target - limGain) * limRel;
			x[0] *= loudG * limGain; x[1] *= loudG * limGain;
			limGR = 20.f * std::log10(std::max(limGain, 1e-4f));
		}
		else limGR = 0;
		l = x[0] * 5.f; r = x[1] * 5.f;
	}

private:
	float fs = 48000;
	int ctr = 0;
	Biquad bump[2], lfHP[2], hfLP[2], aa[2][3], emph[2];
	OnePole empHP[2], deLP[2], nrSplit[2], hissHP[2], printLP[2], printHP[2];
	Ring print[2];
	uint32_t rs = 777;
	float emphK = 2, satT = 5, nrEnv = 0, nrAtk = 0.01f, nrRel = 0.001f, nrDb = 0, hissAmp = 0, printGain = 0;
	int printDelay = 1000;
	bool aaOn = false, dither = true;
	float dnl = 0, qStep = 1e-4f, loudG = 1, limGain = 1, limRel = 0.001f;

	float rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs / 4294967295.f) * 2.f - 1.f; }

	void update() {
		bool q = s.source == 1;
		for (int c = 0; c < 2; c++) {
			bump[c].peak(fs, q ? 60.0 : 110.0, 1.0, q ? 1.8 : 1.0);
			lfHP[c].highpass(fs, q ? 22.0 : 28.0, 0.707);
			hfLP[c].lowpass(fs, 20000.0, 0.6);
			empHP[c].set(fs, 5000.f);
			deLP[c].set(fs, 5000.f / (1.f + emphK));
			nrSplit[c].set(fs, 3000.f);
			hissHP[c].set(fs, 2000.f);
			printLP[c].set(fs, 2500.f); printHP[c].set(fs, 400.f);
		}
		emphK = q ? 3.5f : 2.f;
		float d = clampf(s.drive, 0, 1);
		satT = q ? 4.f + (0.8f - 4.f) * d : 5.f + (1.2f - 5.f) * d;
		nrAtk = 1.f - std::exp(-1.f / (0.002f * fs));
		nrRel = 1.f - std::exp(-1.f / (0.08f * fs));
		nrDb = 3.f * s.nr;
		hissAmp = s.hiss > 0.f ? dbToGain(-78.f + 30.f * s.hiss) * (q ? 1.4f : 1.f) : 0.f;
		printGain = s.print > 0.f ? dbToGain(-60.f + 28.f * s.print) : 0.f;
		printDelay = std::min((int)((q ? 1.65f : 0.82f) * fs), (int)(1.75f * fs));  // one turn of the reel
		// converter
		float cq = clampf(s.convert, 0, 1);
		float fc = s.source == 3 ? 21500.f : 20000.f;
		aaOn = cq < 0.95f && fc < 0.46f * fs;
		const double qe[3] = {0.55, 1.2, 4.0}, qb[3] = {0.518, 0.707, 1.93};
		for (int c = 0; c < 2; c++)
			for (int i = 0; i < 3; i++) aa[c][i].lowpass(fs, fc, qe[i] + (qb[i] - qe[i]) * cq);
		qStep = 4.f / (s.bits ? 65536.f : 16384.f);
		dither = cq > 0.5f;
		dnl = (1.f - cq) * 1.5f * qStep;
		for (int c = 0; c < 2; c++) {
			if (s.emphasis == 1) emph[c].setAnalog(1, 50e-6, 0, 1, 15e-6, 0, 2.0 * fs);
			else emph[c].setAnalog(1, 15e-6, 0, 1, 50e-6, 0, 2.0 * fs);
		}
		loudG = dbToGain(s.loudDb);
		limRel = 1.f - std::exp(-1.f / (0.05f * fs));
	}
};

// ---- PRESSING expander: the plant and the life of the disc ----
struct PressSettings {
	bool on = false;
	int stock = 0;  // 0 virgin vinyl, 1 regrind, 2 styrene, 3 flexi, 4 picture disc
	float noise = 1, nonfill = 0, stamper = 0.2f, weight = 120, dust = 0, scratch = 0, cueBurn = 0;
};

struct Scratch { float theta, needle, depth; };

struct Meters {
	float hflGR = 0, excGR = 0, mistrack = 0, overload = 0;
	float radius = 0.146f, grooveSpeed = 0.5f, pitch = 0.1e-3f, rEnd = 0.06f, rOut = 0.146f, rMin = 0.06f;
	float trackErrDeg = 0, sideDb = 0, contactUm = 0;
	float fluff = 0;
	bool nonfill = false, rotation = false, skip = false;  // rotation and skip are set here and cleared by the reader
	int latency = 0;
};

class Engine {
public:
	Settings s;
	Meters m;
	MasterStage master;
	PressSettings ps;
	static constexpr int kMaxScratch = 11;
	Scratch scratches[kMaxScratch];
	int nScratch = 3;

	void clearScratches() {
		scratches[0] = {1.0f, 0.20f, 0.6f}; scratches[1] = {3.1f, 0.55f, 0.8f}; scratches[2] = {5.2f, 0.85f, 1.0f};
		nScratch = 3;
	}
	void addScratch(float theta, float needle, float depth) {
		if (nScratch < kMaxScratch) { scratches[nScratch] = {theta, needle, depth}; nScratch++; }
	}
	void addScratchHere() { addScratch(prnd01() * 6.2831f, s.needle, 0.6f + 0.4f * prnd01()); }
	void clean() { fluff = 0; }

	void init(float sampleRate) {
		fs = sampleRate;
		os = fs < 60000 ? 4 : (fs < 120000 ? 2 : 1);
		fsOS = fs * os;
		if (os > 1) {
			int N = 48 * os + 1;
			auto h = designLowpass(N, 0.46 / os, 7.0);
			half = (N - 1) / 2;
			for (int c = 0; c < 2; c++) { up[c].init(h, os); dn[c].init(h); }
		}
		else half = 0;
		osLatency = (2 * half + kNmax) / os;
		center = (int) std::round(0.012f * fs);
		cutBuf[0].init((int)(4.2f * fs)); cutBuf[1].init((int)(4.2f * fs));
		groove[0].init(2 * center + 16); groove[1].init(2 * center + 16);
		maxDry = (int)(1.9f * fs) + center + osLatency + 8;
		dryBuf[0].init(maxDry + 4); dryBuf[1].init(maxDry + 4);
		for (int c = 0; c < 2; c++) {
			for (auto& v : hist[c]) v = 0;
			osY[c] = sp[c] = sv[c] = tPrev[c] = outPrev[c] = 0;
			exY[c] = 0; hiPrev[c] = 0; hfX1[c] = hfY1[c] = 0;
			bodyHP[c].reset();
		}
		histPos = 0;
		hfEnv = 0; hfGain = 1; exEnv = 0; exGain = 1; zPrev = zcPrev = 0;
		theta = 0; wowInt = 0; zrelPrev = 0;
		ctr = 0;
		master.init(fs);
		if (!scratchInit) { clearScratches(); scratchInit = true; }
		for (auto& t : ticks) t.n = 0;
		fluff = 0; nfActive = false; nfGap = 4; pEnv = 0;
		update();
	}

	void process(float inL, float inR, float& wetL, float& wetR, float& dryL, float& dryR) {
		if (--ctr <= 0) { ctr = 32; update(); }
		dryBuf[0].push(inL); dryBuf[1].push(inR);
		dryL = dryBuf[0].at(latency); dryR = dryBuf[1].at(latency);
		if (master.s.on) master.process(inL, inR);

		float xL = inL * kInScale, xR = inR * kInScale;

		// 1 ELLIPTIC: fade the side signal below the crossover
		if (s.ellOn) {
			float M = 0.5f * (xL + xR), S = 0.5f * (xL - xR);
			float hp;
			if (s.ellSlope == 0) hp = ellOP.hp(S);
			else if (s.ellSlope == 1) hp = ellB[0].process(S);
			else hp = ellB[2].process(ellB[1].process(S));
			S += s.ellAmount * (hp - S);
			xL = M + S; xR = M - S;
		}

		// record curve and cut level -> groove velocity
		float v[2] = {rec[0].process(xL) * gCut, rec[1].process(xR) * gCut};

		if (s.cutOn) {
			// side length: high-pass and excursion (groove displacement) limiter, in metres
			for (int c = 0; c < 2; c++) {
				v[c] = sideHP[c].process(v[c]);
				exY[c] = exLeak * exY[c] + v[c] * kVref / fs;
			}
			float d = std::max(std::fabs(exY[0]), std::fabs(exY[1]));
			if (d > exEnv) exEnv = d; else exEnv += (d - exEnv) * exRel;
			float gt = exEnv > yMax ? yMax / exEnv : 1.f;
			exGain += (gt - exGain) * (gt < exGain ? exAtk : 1.f);
			for (int c = 0; c < 2; c++) {
				float lo = exSplit[c].lp(v[c]);
				v[c] += (exGain - 1.f) * lo;
			}
			// vertical depth: the cutter lifts out of the lacquer going up, and may only dig so deep
			float z = (exY[0] - exY[1]) * 0.70710678f;
			float zc = z;
			if (z > zUp) zc = zUp + (z - zUp) / (1.f + (z - zUp) / 3e-6f);
			else if (z < -zDown) zc = -zDown + (z + zDown) / (1.f - (z + zDown) / 10e-6f);
			float dv = ((zc - zcPrev) - (z - zPrev)) * fs / kVref;
			zPrev = z; zcPrev = zc;
			v[0] += 0.70710678f * dv; v[1] -= 0.70710678f * dv;
			float depthGR = zc != z ? -6.f : 0.f;
			m.excGR = std::min(m.excGR * 0.9995f, std::min(depthGR, 20.f * std::log10(std::max(exGain, 1e-4f))));
		}

		// 2 HF LIMIT: limiter on treble acceleration
		if (s.hflOn) {
			float lo[2], hi[2], a = 0;
			for (int c = 0; c < 2; c++) {
				lo[c] = hfSplit[c].lp(v[c]);
				hi[c] = v[c] - lo[c];
				a = std::max(a, std::fabs(hi[c] - hiPrev[c]) * accNorm);
				hiPrev[c] = hi[c];
			}
			if (a > hfEnv) hfEnv = a; else hfEnv += (a - hfEnv) * hfRel;
			float gt = hfEnv > hfThr ? hfThr / hfEnv : 1.f;
			hfGain += (gt - hfGain) * hfSmooth;
			// the gain cell is a low-pass whose corner slides down: at gain g the band edge is cut by g
			float fc = hfGain >= 0.999f ? 0.4999f * fs : std::min(0.4999f * fs, s.hflFreq * hfGain / std::sqrt(std::max(1e-6f, 1.f - hfGain * hfGain)));
			float t = std::tan((float) kPi * fc / fs);
			float b = t / (1.f + t), a1 = (t - 1.f) / (t + 1.f);
			for (int c = 0; c < 2; c++) {
				float yv = b * (v[c] + hfX1[c]) - a1 * hfY1[c];
				hfX1[c] = v[c]; hfY1[c] = yv;
				v[c] = hfGain >= 0.999f ? v[c] : yv;
			}
			m.hflGR = std::min(m.hflGR * 0.9995f, 20.f * std::log10(std::max(hfGain, 1e-4f)));
		}

		// 3 CUT: cutter head roll-off, drive amp ceiling
		if (s.cutOn) {
			for (int c = 0; c < 2; c++) {
				if (headOn) v[c] = headLP[c].process(v[c]);
				v[c] = satThr * std::tanh(v[c] / satThr);  // a safety ceiling only: cutting amps had large headroom
			}
		}

		// 4 MEDIUM
		if (s.medOn) {
			for (int c = 0; c < 2; c++) {
				if (s.medType != 2 && plateOn) v[c] = plateLP[c].lp(v[c]);
				if (wearEff > 0.01f) {
					if (wearHz < 0.45f * fs) v[c] = wearLP[c].lp(v[c]);
					float lo = wearSplit[c].lp(v[c]), hi = v[c] - lo;
					v[c] = lo + wearThr * std::tanh(hi / wearThr);
				}
			}
		}
		if (ps.on && pressLenOn) {  // tired stamper / cue burn: the shortest groove shapes are rounded off
			for (int c = 0; c < 2; c++) {
				v[c] = pressLP[c].lp(v[c]);
				float lo = pressSplit[c].lp(v[c]), hi = v[c] - lo;
				v[c] = lo + pressThr * std::tanh(hi / pressThr);
			}
		}
		float g[2];
		for (int c = 0; c < 2; c++) {
			cutBuf[c].push(v[c]);
			if (s.truePreEcho) {
				g[c] = cutBuf[c].at(trev);
				if (s.medOn) g[c] += ePre * echoLP[c].lp(v[c]) + ePost * echoLP2[c].lp(cutBuf[c].at(2 * trev));
			}
			else {
				g[c] = v[c];
				if (s.medOn) g[c] += ePost * echoLP[c].lp(cutBuf[c].at(trev));
			}
			groove[c].push(g[c]);
		}

		// 9 PLATTER: read the groove at a moving delay
		float dev = 0;
		if (s.pltOn) {
			float w = 0, f = 0;
			float t = (float) theta;
			switch (s.drive) {
				case 0:  // direct drive: servo once per turn, tiny motor ripple
					w = 0.6f * std::sin(t) + 0.4f * wowRnd.next(0.4f, fs);
					f = 0.4f * std::sin(cogPh) + 0.6f * flRnd.next(9.f, fs);
					break;
				case 1:  // belt: belt stretch and pulley
					w = wowRnd.next(0.8f, fs) + 0.3f * std::sin(beltPh);
					f = flRnd.next(10.f, fs) + 0.3f * std::sin(cogPh);
					break;
				default:  // cheap deck
					w = wowRnd.next(1.5f, fs) + 0.5f * std::sin(beltPh);
					f = flRnd.next(12.f, fs) + 0.5f * std::sin(cogPh);
					break;
			}
			dev = wowPct * w + flPct * f;
		}
		double thetaPrev = theta;
		theta += 2 * kPi * revHz * (1.0 + dev) / fs;
		bool wrapped = false;
		if (theta > 2 * kPi) { theta -= 2 * kPi; wrapped = true; m.rotation = true; }
		cogPh += 2.f * (float) kPi * cogHz / fs; if (cogPh > 2 * kPi) cogPh -= 2.f * (float) kPi;
		beltPh += 2.f * (float) kPi * beltHz / fs; if (beltPh > 2 * kPi) beltPh -= 2.f * (float) kPi;
		wowInt = wowInt * wowLeak - dev / fs;
		float d = (float) center;
		if (s.pltOn) d += (wowInt - eccTerm * std::sin((float) theta)) * fs;
		if (s.armOn) d += zrelPrev * 0.364f / vg * fs;  // warp wow through vertical tracking angle
		d = clampf(d, 3.f, 2.f * center + 8.f);
		float wl = groove[0].frac(d), wr = -groove[1].frac(d);  // to wall coordinates
		if (ps.on) pressing(wl, wr, thetaPrev, wrapped);

		// -- oversampled stylus / groove contact --
		float o[2] = {0, 0};
		if (os > 1) { up[0].push(wl); up[1].push(wr); }
		for (int k = 0; k < os; k++) {
			float u[2];
			if (os > 1) { u[0] = up[0].phase(k); u[1] = up[1].phase(k); }
			else { u[0] = wl; u[1] = wr; }
			double vm[2], t[2];
			for (int c = 0; c < 2; c++) {
				vm[c] = (double) u[c] * kVref;
				osY[c] = osLeak * osY[c] + vm[c] * dtos;
				t[c] = osY[c];
			}
			if (s.trkOn) {  // 6 tracking angle error: reading point moves along the groove with lateral excursion
				double lat = (osY[0] - osY[1]) * 0.70710678, vert = (osY[0] + osY[1]) * 0.70710678;
				double tau = (alpha * lat + beta * vert) / vg;
				t[0] += tau * vm[0]; t[1] += tau * vm[1];
			}
			for (int c = 0; c < 2; c++) hist[c][histPos] = t[c];
			int ci = (histPos - kNmax) & kHmask;
			histPos = (histPos + 1) & kHmask;
			for (int c = 0; c < 2; c++) {
				double y0 = hist[c][ci];
				double yc = y0;
				if (s.trcOn) {  // 5 tracing: circle of radius rho rolling over the wall
					// best contact point on the sampled wall, then refined between samples so the result
					// grows smoothly with level (centre height = y + rho (sqrt(1 + slope^2) - 1) for gentle slopes)
					int jb = 0;
					double mx = y0;
					for (int j = 1; j <= nTrace; j++) {
						double a = hist[c][(ci - j) & kHmask] + traceTab[j], b = hist[c][(ci + j) & kHmask] + traceTab[j];
						if (a > mx) { mx = a; jb = -j; }
						if (b > mx) { mx = b; jb = j; }
					}
					int ja = jb < 0 ? -jb : jb;
					if (ja + 1 <= kNmax) {
						int jm = jb - 1, jp = jb + 1;
						double fm = hist[c][(ci + jm) & kHmask] + traceTab[jm < 0 ? -jm : jm];
						double fp = hist[c][(ci + jp) & kHmask] + traceTab[jp < 0 ? -jp : jp];
						double den = fm - 2 * mx + fp;
						if (den < 0) {
							double pk = mx - (fp - fm) * (fp - fm) / (8 * den);
							if (pk > mx) mx = pk;
						}
					}
					yc = y0 + s.trcAmount * (mx - y0);
				}
				double out = yc;
				double body = yc - bodyHP[c].processD(yc);  // below the arm resonance the cartridge body follows the groove
				if (s.trkOn) {  // 6 one-sided contact: tracking force plus the cantilever spring around the body
					sv[c] += (-aMax[c] - kOverM * (sp[c] - body)) * dtos;
					sp[c] += sv[c] * dtos;
					if (sp[c] <= yc) { sv[c] = (yc - tPrev[c]) / dtos; sp[c] = yc; }
					else mistrackN++;
					tPrev[c] = yc;
					out = sp[c];
				}
				double vo = (out - osLeak * outPrev[c]) / dtos;
				outPrev[c] = out;
				float vu = (float)(vo / kVref);
				// linear playback filters run here too, so their top octave isn't bent by the base-rate Nyquist
				if (s.trcOn) vu = scanLP[c].process(vu);
				if (s.cartOn) {
					if (s.cartModel != 2) vu = cartE[c].process(vu);
					vu = cartM[c].process(vu);
				}
				if (os > 1) { dn[c].push(vu); if (k == 0) o[c] = dn[c].out(); }
				else o[c] = vu;
			}
		}
		if (++mistrackCtr >= 256) {
			float frac = mistrackN / (256.f * os * 2.f);
			m.mistrack = std::min(1.f, std::max(m.mistrack * 0.9f, frac * 20.f));
			mistrackN = 0; mistrackCtr = 0;
		}
		float pL = o[0], pR = -o[1];  // back to channel signals
		if (ps.on && fluff > 0.002f) { pL = fluffLP[0].lp(pL); pR = fluffLP[1].lp(pR); }  // fluff on the tip

		// 7 CARTRIDGE
		if (s.cartOn) {
			float xl = xtHP[0].hp(pL), xr = xtHP[1].hp(pR);
			float nL = pL + xtLR * (pR + 2.f * xr);
			float nR = pR + xtRL * (pL + 2.f * xl);
			pL = nL; pR = nR;
		}

		// 8 ARM: arm / compliance resonance; warps move the groove vertically
		if (s.armOn) {
			pL = armHP[0].process(pL); pR = armHP[1].process(pR);
			float t = (float) theta;
			float z = warpAmp * (0.8f * std::sin(t) + 0.35f * std::sin(2 * t + 1.1f) + 0.15f * std::sin(3 * t + 0.4f));
			float zrel = warpHP.process(z);
			float zv = (zrel - zrelPrev) * fs / kVref;
			zrelPrev = zrel;
			pL += 0.7071f * zv; pR -= 0.7071f * zv;
		}

		// 10 PHONO
		float y[2] = {pL, pR};
		for (int c = 0; c < 2; c++) {
			float x = y[c];
			if (s.phoOn) {
				x *= phoG;
				float a = std::fabs(x);
				if (a > 0.8f * phoThr) m.overload = 1.f;
				switch (s.phoType) {
					case 0: {  // tube: soft, asymmetric
						const float b = 0.25f, tb = std::tanh(b), sech2 = 1.f - tb * tb;
						x = phoThr * (std::tanh(x / phoThr + b) - tb) / sech2;
					} break;
					case 1:  // op-amp: clean, then a hard knee
						x = x / std::pow(1.f + std::pow(a / phoThr, 8.f), 0.125f);
						break;
					default:  // DJ mixer: less headroom, firmer knee
						x = x / std::pow(1.f + std::pow(a / phoThr, 4.f), 0.25f);
						break;
				}
			}
			x = pb[c].process(x);
			if (s.phoOn) {
				x /= phoG;
				if (s.phoType == 0) { x = x + 0.03f * x * x; x = tubeHP[c].hp(x); }
				else if (s.phoType == 2) x = djHP[c].hp(x);
				if (s.subsonic == 2) x = subHP[c].process(x);
				else if (s.subsonic == 1) x = iecHP[c].hp(x);
			}
			y[c] = x;
		}
		m.overload *= 0.9997f;
		wetL = y[0] / (gCut * kInScale);
		wetR = y[1] / (gCut * kInScale);
	}

private:
	static constexpr int kNmax = 32;
	static constexpr int kHsize = 128, kHmask = 127;
	float fs = 48000, fsOS = 192000;
	int os = 4, half = 32, osLatency = 24, center = 576, latency = 600, maxDry = 0, ctr = 0;
	Upsampler up[2];
	Downsampler dn[2];
	Ring cutBuf[2], groove[2], dryBuf[2];
	double hist[2][kHsize] = {};
	int histPos = 0;
	double osY[2] = {}, sp[2] = {}, sv[2] = {}, tPrev[2] = {}, outPrev[2] = {};
	double osLeak = 1, dtos = 1, alpha = 0, vg = 0.3, aMax[2] = {1e9, 1e9};
	double traceTab[kNmax + 2] = {};
	double beta = 0.052, kOverM = 0;
	Biquad bodyHP[2];
	int nTrace = 0;
	int mistrackN = 0, mistrackCtr = 0;

	OnePole ellOP; Biquad ellB[3];
	Biquad rec[2], pb[2], sideHP[2], headLP[2];
	float gCut = 1;
	float exY[2] = {}, exLeak = 1, exEnv = 0, exGain = 1, exAtk = 0.1f, exRel = 0.001f, yMax = 1e-4f;
	OnePole exSplit[2], hfSplit[2];
	float zPrev = 0, zcPrev = 0, zUp = 30e-6f, zDown = 100e-6f;
	float hfX1[2] = {}, hfY1[2] = {};
	bool headOn = true;
	float hiPrev[2] = {}, accNorm = 1, hfEnv = 0, hfGain = 1, hfThr = 1, hfRel = 0.001f, hfSmooth = 0.2f;
	float satThr = 30.f;
	OnePole plateLP[2], wearLP[2], wearSplit[2], echoLP[2], echoLP2[2];
	float wearEff = 0, wearThr = 10, ePre = 0, ePost = 0, wearHz = 1e9f;
	bool plateOn = false;
	int trev = 86400;
	double theta = 0;
	float cogPh = 0, beltPh = 0, cogHz = 7, beltHz = 0.6f, revHz = 0.5556f;
	float wowInt = 0, wowLeak = 1, eccTerm = 0, wowPct = 0, flPct = 0;
	SmoothRandom wowRnd, flRnd;

	// pressing
	struct Tick { int n = 0, len = 1; float amp = 0, mixL = 0, mixR = 0; };
	Tick ticks[6];
	bool scratchInit = false;
	uint32_t prs = 4242;
	float fluff = 0, pEnv = 0, noiseAmp = 0, burstK = 0, pressThr = 10, ringK = 0, tickRate = 0, nfState = 0, nfTheta = 0, nfWidth = 0.2f;
	bool nfActive = false, pressLenOn = false;
	int nfGap = 4, nfTurns = 0, nfWall = 0;
	SmoothRandom burstRnd;
	OnePole pressLP[2], pressSplit[2], noiseHP[2], fluffLP[2], nfLP;
	Biquad ringBP;

	float prnd() { prs ^= prs << 13; prs ^= prs >> 17; prs ^= prs << 5; return (prs / 4294967295.f) * 2.f - 1.f; }
	float prnd01() { return 0.5f + 0.5f * prnd(); }

	void startTick(float amp, float widthM, float mixL, float mixR) {
		for (auto& t : ticks) if (t.n <= 0) {
			t.len = (int) clampf((float)(widthM / vg) * fs, 3.f, 60.f);
			t.n = t.len; t.amp = amp; t.mixL = mixL; t.mixR = mixR;
			return;
		}
	}

	// Defects are shapes in the groove wall (wall coordinates, velocity units), read by the stylus like the music.
	void pressing(float& wl, float& wr, double thetaPrev, bool wrapped) {
		// surface roughness: mostly vertical, rougher where the groove is heavily modulated
		pEnv += ((std::fabs(wl) + std::fabs(wr)) * 0.25f - pEnv) * 0.001f;
		float a = noiseAmp * (1.f + 1.5f * std::min(1.f, pEnv));
		if (burstK > 0) { float b = std::fabs(burstRnd.next(3.f, fs)); a *= 1.f + burstK * b * b * b * b; }
		if (ps.stock == 4) a *= 1.f + 0.3f * std::sin((float) theta);
		float nv = prnd(), nl = prnd();
		float vV = a * (0.7f * nv + 0.8f * noiseHP[0].hp(nv));
		float vL = 0.5f * a * (0.7f * nl + 0.8f * noiseHP[1].hp(nl));
		wl += 0.7071f * (vV + vL); wr += 0.7071f * (vV - vL);

		// non-fill: a patch where the vinyl never reached the stamper, at the same place each turn for a few turns
		if (wrapped) {
			if (nfActive) { if (--nfTurns <= 0) { nfActive = false; nfGap = 2 + (int)(prnd01() * 24.f * (1.05f - ps.nonfill)); } }
			else if (ps.nonfill > 0.01f && --nfGap <= 0) {
				nfActive = true; nfTheta = prnd01() * 6.0f; nfWidth = 0.08f + 0.22f * prnd01();
				nfTurns = 3 + (int)(prnd01() * 6.f); nfWall = prnd() > 0 ? 1 : 0;
			}
		}
		m.nonfill = false;
		if (nfActive) {
			float dth = (float) theta - nfTheta;
			if (dth > 0 && dth < nfWidth) {
				m.nonfill = true;
				if (prnd01() < 900.f / fs) nfState = prnd() * 2.5f * ps.nonfill;
				float win = std::sin((float) kPi * dth / nfWidth);
				float n = nfLP.lp(nfState) * win;
				nfState *= 0.7f;
				if (nfWall) wr += n; else wl += n;
			}
		}

		// dust: fluff builds up on the tip, particles are bumps in the groove
		if (ps.dust > 0) {
			fluff = std::min(1.f, fluff + ps.dust / (180.f * fs));
			if (prnd01() < tickRate / fs) {
				float r = prnd01();
				startTick(0.3f + 1.6f * r * r, 40e-6f, prnd(), prnd());
			}
		}
		m.fluff = fluff;

		// scratches: a radial cut crossed once per turn while the needle is in its part of the side
		if (ps.scratch > 0.01f) {
			for (int i = 0; i < nScratch; i++) {
				const Scratch& sc = scratches[i];
				if (std::fabs(sc.needle - s.needle) > 0.07f) continue;
				bool hit = wrapped ? (sc.theta > thetaPrev || sc.theta <= theta) : (sc.theta > thetaPrev && sc.theta <= theta);
				if (hit) {
					float d = ps.scratch * sc.depth;
					startTick(7.f * d, 90e-6f, 1.f, -0.6f);
					if (d > 0.8f) m.skip = true;
				}
			}
		}
		for (auto& t : ticks) if (t.n > 0) {
			float u = 1.f - 2.f * (float) t.n / t.len;  // -1..1 across the bump
			float w = -3.3f * u * std::exp(-4.f * u * u) * t.amp;
			wl += t.mixL * w; wr += t.mixR * w;
			t.n--;
		}

		// a thin disc rings under the stylus
		if (ringK > 0) {
			float r = ringBP.process((wl + wr) * 0.7071f) * ringK * 0.7071f;
			wl += r; wr += r;
		}
	}
	Biquad scanLP[2];
	OnePole xtHP[2], iecHP[2];
	Biquad cartE[2], cartM[2], armHP[2], warpHP, subHP[2];
	float xtLR = 0, xtRL = 0, warpAmp = 0, zrelPrev = 0;
	float phoG = 1, phoThr = 10;
	OnePole tubeHP[2], djHP[2];

	void update() {
		// JIS S 8502: music starts inside 168 / 242 / 293 mm and ends outside 106 / 115.2 / 115.2 mm diameter
		const float rOutT[3] = {0.0840f, 0.1210f, 0.1465f}, rMinT[3] = {0.0530f, 0.0576f, 0.0576f};
		int di = std::min(std::max(s.disc, 0), 2);
		float rpm = s.speed ? 45.f : 33.333f;
		revHz = rpm / 60.f;
		float rOut = rOutT[di], rMin = rMinT[di];
		float pitch = clampf((rOut - rMin) / (rpm * s.sideMin), 0.04e-3f, 0.25e-3f);
		float rEnd = std::max(rMin, rOut - rpm * s.sideMin * pitch);
		float r = rOut - clampf(s.needle, 0, 1) * (rOut - rEnd);
		vg = 2 * kPi * r * revHz;
		m.radius = r; m.grooveSpeed = (float) vg; m.pitch = pitch; m.rEnd = rEnd; m.rOut = rOut; m.rMin = rMin;

		// cut
		const float pRef = 0.18e-3f;
		float sideDb = s.cutOn ? 20.f * std::log10(std::min(1.f, pitch / pRef)) : 0.f;
		m.sideDb = sideDb;
		gCut = s.cutOn ? dbToGain(s.cutLevelDb + sideDb) : 1.f;
		float sideHz = clampf(20.f * std::pow(pRef / pitch, 1.5f), 20.f, 60.f);
		bool flexi = ps.on && ps.stock == 3;
		if (flexi) { gCut *= 0.5f; sideHz = std::max(sideHz, 70.f); }  // flexi discs: shallow, quiet cut with little bass
		for (int c = 0; c < 2; c++) sideHP[c].highpass(fs, sideHz, 0.707);
		yMax = 0.45f * pitch;
		exLeak = std::exp(-2.f * (float) kPi * 5.f / fs);
		exAtk = 1.f - std::exp(-1.f / (0.002f * fs));
		exRel = 1.f - std::exp(-1.f / (0.15f * fs));
		for (int c = 0; c < 2; c++) exSplit[c].set(fs, 250.f);
		// cutter head: flat past 20 kHz when modern (HEAD 0), 16 kHz with a small peak when old (HEAD 1)
		float hf = 25000.f - 9000.f * s.head;
		headOn = hf < 0.45f * fs;
		for (int c = 0; c < 2; c++) headLP[c].lowpass(fs, hf, 0.62 + 0.35 * s.head);
		satThr = 30.f;
		// vertical groove depth limits (rest depth about 35 um, 90 degree groove)
		zUp = 30e-6f; zDown = 100e-6f;

		// elliptic
		ellOP.set(fs, s.ellFreq);
		ellB[0].highpass(fs, s.ellFreq, 0.7071);
		ellB[1].highpass(fs, s.ellFreq, 0.5412);
		ellB[2].highpass(fs, s.ellFreq, 1.3066);

		// hf limit
		for (int c = 0; c < 2; c++) hfSplit[c].set(fs, s.hflFreq);
		accNorm = fs / (2.f * (float) kPi * s.hflFreq);
		hfThr = 4.f * dbToGain(s.hflThreshDb);
		hfRel = 1.f - std::exp(-1.f / (std::max(s.hflRelease, 0.001f) * fs));
		hfSmooth = 1.f - std::exp(-1.f / (0.0002f * fs));

		// record and playback curves (the matched pair)
		for (int c = 0; c < 2; c++) riaa(rec[c], fs, 3180e-6, 318e-6, 75e-6, true);
		double t1 = 3180e-6, t2 = 318e-6, t3 = 75e-6;
		if (s.phoOn) {
			t1 *= 1 + 0.05 * s.tol; t2 *= 1 - 0.05 * s.tol; t3 *= 1 + 0.05 * s.tol;
			if (s.phoType == 2) { t1 *= 1.12; t3 *= 1.12; }
		}
		for (int c = 0; c < 2; c++) riaa(pb[c], fs, t1, t2, t3, false);

		// medium
		// losses set by groove wavelength, so they bite harder as the groove slows towards the label
		float plateLen = s.medType == 0 ? 2.0e-6f : 1.0e-6f;  // effective smoothing length of the plating chain
		float plateHz = (float) vg / ((float) kPi * plateLen);
		const float wearRate[3] = {0.08f, 0.08f, 1.0f};
		wearEff = s.medOn ? s.wear * wearRate[std::min(std::max(s.medType, 0), 2)] : 0.f;
		float wearLen = 0.15e-6f * wearEff;  // worn-off radius of the shortest groove curves
		wearHz = wearLen > 0 ? (float) vg / ((float) kPi * wearLen) : 0.45f * fs;
		for (int c = 0; c < 2; c++) {
			plateLP[c].set(fs, plateHz);
			wearLP[c].set(fs, wearHz);
			plateOn = plateHz < 0.45f * fs;
			wearSplit[c].set(fs, 3000.f);
			echoLP[c].set(fs, 1200.f); echoLP2[c].set(fs, 1200.f);
		}
		wearThr = 6.f / (1.f + 0.05f * wearEff);
		trev = (int) std::round(fs / revHz);
		const float echoType[3] = {1.f, 0.3f, 1.3f};
		float E = s.echo > 0.f ? dbToGain(-60.f + 34.f * s.echo) * echoType[std::min(std::max(s.medType, 0), 2)]
			* clampf(0.12e-3f / pitch, 0.5f, 2.f) : 0.f;
		ePre = E; ePost = s.truePreEcho ? 0.6f * E : E;

		// platter
		const float driveScale[3] = {0.15f, 1.f, 2.5f};
		int dv = std::min(std::max(s.drive, 0), 2);
		wowPct = s.wow * 0.003f * driveScale[dv] + (flexi ? 0.002f : 0.f);  // a flexi slips on the mat
		flPct = s.flutter * 0.002f * driveScale[dv];
		cogHz = dv == 0 ? revHz * 12.f : (dv == 1 ? revHz * 15.f : 25.f);
		beltHz = dv == 2 ? 1.1f : 0.6f;
		wowLeak = std::exp(-2.f * (float) kPi * 0.05f / fs);
		eccTerm = (s.eccMm * 1e-3f / r) / (2.f * (float) kPi * revHz);

		// oversampled section
		dtos = 1.0 / fsOS;
		osLeak = std::exp(-2 * kPi * 5.0 / fsOS);
		const double n1T[2] = {0.06606, 0.06033}, n2T[2] = {0.12090, 0.11742};
		int al = s.align ? 1 : 0;
		alpha = (r - n1T[al]) * (r - n2T[al]) / (2 * 0.23 * r);
		m.trackErrDeg = (float)(alpha * 180 / kPi);
		const double meffT[3] = {0.8e-6, 0.4e-6, 0.3e-6};
		double Fwall = s.force * 9.81e-3 / 1.41421356;
		double me = meffT[std::min(std::max(s.cartModel, 0), 2)];
		aMax[0] = Fwall * (1 + 0.35 * s.balance) / me;
		aMax[1] = Fwall * (1 - 0.35 * s.balance) / me;
		if (ps.on) { aMax[0] *= 1.0 - 0.7 * fluff; aMax[1] *= 1.0 - 0.7 * fluff; }  // riding on fluff
		// cantilever compliance (m/N): DJ stiff, hi-fi MM soft, MC medium
		const double compT[3] = {0.010, 0.020, 0.012};
		kOverM = 1.0 / (compT[std::min(std::max(s.cartModel, 0), 2)] * me);
		{
			double q = 0.3 + (1.0 - s.armDamp) * 2.7;
			for (int c = 0; c < 2; c++) bodyHP[c].highpass(fsOS, s.armRes, q);
		}
		beta = std::tan(3.0 * kPi / 180.0);  // vertical tracking angle mismatch, about 3 degrees
		const double rhoT[3] = {17.8e-6, 7.6e-6, 3.0e-6};
		int st = std::min(std::max(s.stylus, 0), 2);
		double rho = rhoT[st];
		double dx = vg / fsOS;
		nTrace = (int) std::floor(rho / dx);
		if (nTrace > kNmax - 1) nTrace = kNmax - 1;
		for (int j = 0; j <= nTrace + 1; j++) {
			double u = j * dx;
			traceTab[j] = u <= rho ? std::sqrt(rho * rho - u * u) - rho : -u * u / (2 * rho);
		}
		// scanning loss: the groove wall gives way under the tip (Hertz contact), and wavelengths near the
		// contact length are averaged out. Contact radius a = (3 F R / 4 E*)^(1/3).
		{
			const double Estar[3] = {3.4e9, 3.4e9, 1.8e9};  // vinyl pressing, DMM pressing, acetate lacquer
			double Ra = rho;  // radius along the groove
			double E = Estar[std::min(std::max(s.medType, 0), 2)];
			if (ps.on && s.medType != 2) { const double Es[5] = {3.4e9, 3.2e9, 3.3e9, 1.5e9, 2.8e9}; E = Es[std::min(std::max(ps.stock, 0), 4)]; }
			double a = std::cbrt(3.0 * Fwall * Ra / (4.0 * E));
			double scanHz = 1.7 * vg / (kPi * a);
			for (int c = 0; c < 2; c++) scanLP[c].lowpass(fsOS, scanHz, 0.5);
			m.contactUm = (float)(2e6 * a);
		}

		// cartridge
		{
			// DJ: Shure M44-7 sheet gives 720 mH and a 400-500 pF / 47k optimum load
			const double Lt[3] = {0.72, 0.45, 0.0}, Rt[3] = {630, 1300, 10}, RpT[3] = {120e3, 180e3, 1e9};
			const double mf[3] = {17000, 22000, 32000}, mq[3] = {1.3, 1.8, 2.5}, mg[3] = {4.0, 3.0, 3.0};
			int cm = std::min(std::max(s.cartModel, 0), 2);
			double L = Lt[cm], R = Rt[cm], Rl = 47000, C = s.loadPf * 1e-12, Rp = RpT[cm];
			if (cm != 2) {
				// coil (L with eddy-current loss Rp across it, series R) into the 47k || C load
				double B0 = Rl * Rp, B1 = Rl * L;
				double A0 = Rl * Rp + R * Rp, A1 = Rl * L + R * L + L * Rp + Rl * C * R * Rp, A2 = Rl * C * (R * L + L * Rp);
				double k = A0 / B0;
				double f0 = 1.0 / (2 * kPi * std::sqrt(L * C));
				for (int c = 0; c < 2; c++) cartE[c].setAnalog(B0 * k, B1 * k, 0, A0, A1, A2, Biquad::warpK(fsOS, f0));
			}
			for (int c = 0; c < 2; c++) { cartM[c].peak(fsOS, mf[cm], mq[cm], mg[cm]); xtHP[c].set(fs, 6000.f); }
			float xt = dbToGain(-s.sepDb);
			xtLR = xt * (1.f + 0.8f * s.azimuth);
			xtRL = xt * (1.f - 0.8f * s.azimuth);
		}

		// arm
		{
			double q = 0.3 + (1.0 - s.armDamp) * 2.7;
			for (int c = 0; c < 2; c++) armHP[c].highpass(fs, s.armRes, q);
			warpHP.highpass(fs, s.armRes, q);
			warpAmp = s.warpMm * 1e-3f;
			if (ps.on) warpAmp += 0.5e-3f * clampf((180.f - ps.weight) / 90.f, 0.f, 1.f);  // thin discs are never flat
		}

		// phono
		// overload in 5 cm/s units at the input (about 5 mV each for a 1 mV/cm/s cartridge)
		const float thrT[3] = {14.f, 20.f, 8.f};
		phoThr = thrT[std::min(std::max(s.phoType, 0), 2)];
		phoG = dbToGain(s.phoGainDb);
		for (int c = 0; c < 2; c++) {
			tubeHP[c].set(fs, 6.f); djHP[c].set(fs, 25.f);
			subHP[c].highpass(fs, 18.f, 0.7071);
			iecHP[c].set(fs, 20.02f);  // IEC amendment, 7950 us
		}

		// pressing
		{
			int sk = std::min(std::max(ps.stock, 0), 4);
			const float base[5] = {0.0012f, 0.0045f, 0.0020f, 0.0060f, 0.0110f};
			const float burst[5] = {0.f, 3.f, 0.5f, 1.f, 0.3f};
			const float wearMul[5] = {1.f, 1.2f, 6.f, 3.f, 1.5f};
			float st = clampf(ps.stamper, 0, 1);
			float cueW = clampf(1.f - s.needle / 0.03f, 0.f, 1.f) * ps.cueBurn;
			noiseAmp = base[sk] * ps.noise * (1.f + 2.f * st) * (1.f + 3.f * cueW);
			burstK = burst[sk];
			float len = 3.0e-6f * st * st + 6.0e-6f * cueW * wearMul[sk] / 6.f + 1.0e-6f * cueW;
			float hz = len > 1e-8f ? (float) vg / ((float) kPi * len) : 1e9f;
			pressLenOn = hz < 0.45f * fs;
			pressThr = 14.f / (1.f + 4.f * st + 4.f * cueW);
			float thin = clampf((180.f - ps.weight) / 90.f, 0.f, 1.f);
			ringK = 0.35f * thin;
			tickRate = ps.dust * 7.f;
			for (int c = 0; c < 2; c++) {
				pressLP[c].set(fs, std::min(hz, 0.45f * fs)); pressSplit[c].set(fs, 3000.f);
				noiseHP[c].set(fs, 3000.f);
				fluffLP[c].set(fs, 21000.f / (1.f + 10.f * fluff * fluff));
			}
			nfLP.set(fs, 6000.f);
			double w = 2 * kPi * 230.0;
			ringBP.setAnalog(0, 1.0 / (6.0 * w), 0, 1, 1.0 / (6.0 * w), 1.0 / (w * w), Biquad::warpK(fs, 230.0));
		}

		latency = center + osLatency + (s.truePreEcho ? trev : 0);
		latency = std::min(latency, maxDry);
		m.latency = latency;
	}
};

}  // namespace vinyl
