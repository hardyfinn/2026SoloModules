// Shared effects: bucket-brigade stereo chorus (spec 9.1) and cassette stage (spec 9.2).
#pragma once
#include "Dsp.hpp"

namespace hk {

// chorus and tape constants (est.)
namespace fxc {
static constexpr float CH_RATE_I = 0.513f, CH_RATE_II = 0.863f; // Hz
static constexpr float CH_DMIN = 1.66e-3f, CH_DMAX = 5.35e-3f;   // s
static constexpr float CH_HP = 80.f, CH_LP = 9000.f;
static constexpr float CH_HISS_DB = -72.f;
static constexpr float CH_OUT = 0.7f;
static constexpr float TP_WOW_HZ = 0.55f, TP_FLUT_HZ = 7.3f;
static constexpr float TP_WOW_DEPTH = 0.0012f, TP_FLUT_DEPTH = 0.0006f; // fraction of speed at TAPE 1
static constexpr float TP_BASE_DELAY = 1.0e-3f;
static constexpr float TP_EMPH_HZ = 3000.f, TP_EMPH_DB = 6.f;
static constexpr float TP_BUMP_HZ = 75.f, TP_BUMP_Q = 1.2f, TP_BUMP_DB = 2.5f;
static constexpr float TP_LP_HI = 18000.f, TP_LP_LO = 6500.f;
static constexpr float TP_HISS_DB0 = -70.f, TP_HISS_DB1 = 14.f;
}

// 4-point Hermite read from a circular buffer
inline float hermiteRead(const std::vector<float>& buf, int mask, int wpos, float delaySamples) {
	float rp = (float) wpos - delaySamples;
	int i = (int) std::floor(rp);
	float f = rp - i;
	float xm1 = buf[(i - 1) & mask], x0 = buf[i & mask], x1 = buf[(i + 1) & mask], x2 = buf[(i + 2) & mask];
	float c1 = 0.5f * (x1 - xm1);
	float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
	float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
	return ((c3 * f + c2) * f + c1) * f + x0;
}

// 3-pole Butterworth low-pass (biquad Q=1 + one-pole)
struct Lp3 {
	Biquad b; OnePole p;
	void set(float fc, float fs) { b.lowpass(fc, 1.f, fs); p.setLP(fc, fs); }
	float process(float x) { return p.lp(b.process(x)); }
	void reset() { b.reset(); p.reset(); }
};

// One side of the chorus: mono in, one modulated BBD line. Two of these share an LFO.
struct ChorusLine {
	std::vector<float> buf; int mask = 0, w = 0;
	OnePole hp; Lp3 pre, post;
	void init(float fs) {
		int n = 1; while (n < (int) (fs * 0.012f)) n <<= 1;
		buf.assign(n, 0.f); mask = n - 1; w = 0;
		hp.setLP(fxc::CH_HP, fs); pre.set(fxc::CH_LP, fs); post.set(fxc::CH_LP, fs);
	}
	void reset() { std::fill(buf.begin(), buf.end(), 0.f); hp.reset(); pre.reset(); post.reset(); }
	float process(float x, float delaySamples, float hiss) {
		float in = ftanh(pre.process(hp.hp(x)));
		buf[w & mask] = in + hiss;
		float y = hermiteRead(buf, mask, w, delaySamples);
		w++;
		return post.process(y);
	}
};

struct Chorus {
	float fs = 48000.f;
	double ph = 0.0;
	ChorusLine a, b; // a feeds the left output, b the right
	Noise nz;
	// crossfade state so switching modes never clicks
	float wet = 0.f;
	void init(float sampleRate) { fs = sampleRate; a.init(fs); b.init(fs); nz.seed(901); ph = 0.0; wet = 0.f; }
	void reset() { a.reset(); b.reset(); }
	// mode 0 off, 1 = I, 2 = II. In-place on L, R. A mono signal (L == R) is the spec's
	// single chorus; a stereo one runs each side through its own line.
	void process(float* L, float* R, int n, int mode, float grit) {
		if (mode == 0 && wet <= 0.f) return;
		float rate = mode == 2 ? fxc::CH_RATE_II : fxc::CH_RATE_I;
		float target = mode == 0 ? 0.f : 1.f;
		float wstep = 1.f / (0.02f * fs);
		float mid = 0.5f * (fxc::CH_DMIN + fxc::CH_DMAX) * fs, amp = 0.5f * (fxc::CH_DMAX - fxc::CH_DMIN) * fs;
		float hissA = dbToGain(fxc::CH_HISS_DB) * (grit / 0.5f);
		for (int i = 0; i < n; i++) {
			ph += rate / fs; if (ph >= 1.0) ph -= 1.0;
			float tri = (float) (4.0 * std::fabs(ph - 0.5) - 1.0);
			float l1 = a.process(L[i], mid + amp * tri, hissA * nz.next());
			float l2 = b.process(R[i], mid - amp * tri, hissA * nz.next());
			if (wet < target) wet = std::min(target, wet + wstep); else if (wet > target) wet = std::max(target, wet - wstep);
			// dry + line, x0.7 (bypass gain 1 when off)
			float gd = 1.f + (fxc::CH_OUT - 1.f) * wet, gw = fxc::CH_OUT * wet;
			L[i] = L[i] * gd + l1 * gw;
			R[i] = R[i] * gd + l2 * gw;
		}
	}
};

struct Tape {
	float fs = 48000.f;
	std::vector<float> bufL, bufR; int mask = 0, w = 0;
	double wowPh = 0.0, flutPh = 0.0;
	Biquad emphL, emphR, deL, deR, bumpL, bumpR, lpL, lpR;
	OnePole fnLp1, fnLp2; // band-limited noise for random flutter
	Noise nz, hissNz;
	float hPrevL = 0.f, hPrevR = 0.f;
	float mix = 0.f;      // 0 = bypassed
	float lastTape = -1.f;
	int coefCounter = 0;
	void init(float sampleRate) {
		fs = sampleRate;
		int n = 1; while (n < (int) (fs * 0.006f)) n <<= 1;
		bufL.assign(n, 0.f); bufR.assign(n, 0.f); mask = n - 1; w = 0;
		emphL.highShelf(fxc::TP_EMPH_HZ, fxc::TP_EMPH_DB, fs); emphR = emphL;
		deL.highShelf(fxc::TP_EMPH_HZ, -fxc::TP_EMPH_DB, fs); deR = deL;
		fnLp1.setLP(25.f, fs); fnLp2.setLP(25.f, fs);
		nz.seed(77); hissNz.seed(78);
		lastTape = -1.f; mix = 0.f;
	}
	void setCoefs(float tape) {
		bumpL.peak(fxc::TP_BUMP_HZ, fxc::TP_BUMP_Q, fxc::TP_BUMP_DB * tape, fs); bumpR.b0 = bumpL.b0; bumpR.b1 = bumpL.b1; bumpR.b2 = bumpL.b2; bumpR.a1 = bumpL.a1; bumpR.a2 = bumpL.a2;
		float t = clampf((tape - 0.01f) / 0.99f, 0.f, 1.f);
		float fc = fxc::TP_LP_HI * std::pow(fxc::TP_LP_LO / fxc::TP_LP_HI, t);
		lpL.lowpass(fc, 0.7071f, fs); lpR.b0 = lpL.b0; lpR.b1 = lpL.b1; lpR.b2 = lpL.b2; lpR.a1 = lpL.a1; lpR.a2 = lpL.a2;
	}
	void process(float* L, float* R, int n, float tape) {
		if (tape <= 0.f && mix <= 0.f) { lastTape = -1.f; return; } // true bypass
		if (std::fabs(tape - lastTape) > 1e-4f) { setCoefs(std::max(tape, 0.f)); lastTape = tape; }
		float target = tape > 0.f ? 1.f : 0.f;
		float mstep = 1.f / (0.01f * fs);
		float t = std::max(tape, 0.f);
		// delay amplitudes from speed deviation: A = dev / (2 pi f)
		float aw = fxc::TP_WOW_DEPTH * t / (2.f * PI_F * fxc::TP_WOW_HZ) * fs;
		float af = fxc::TP_FLUT_DEPTH * t / (2.f * PI_F * fxc::TP_FLUT_HZ) * fs;
		float base = fxc::TP_BASE_DELAY * fs;
		float drive = 1.f + 3.f * t;
		float hissG = t > 0.f ? dbToGain(fxc::TP_HISS_DB0 + fxc::TP_HISS_DB1 * t) : 0.f;
		// +6 dB/oct above 1 kHz: y = (1+k) w[n] - k w[n-1], normalised to unit RMS
		float k = fs / (2.f * PI_F * 1000.f);
		float hnorm = hissG * std::sqrt(3.f) / std::sqrt((1.f + k) * (1.f + k) + k * k);
		for (int i = 0; i < n; i++) {
			wowPh += fxc::TP_WOW_HZ / fs; if (wowPh >= 1.0) wowPh -= 1.0;
			flutPh += fxc::TP_FLUT_HZ / fs; if (flutPh >= 1.0) flutPh -= 1.0;
			float rn = fnLp2.lp(fnLp1.lp(nz.next())) * 6.f; // roughly unit-ish band-limited noise
			float d = base + aw * (float) std::sin(2.0 * PI_D * wowPh) + af * ((float) std::sin(2.0 * PI_D * flutPh) + 0.5f * rn);
			float xl = L[i], xr = R[i];
			// record: pre-emphasis, saturation, then the transport delay
			float rl = ftanh(drive * emphL.process(xl)) / drive;
			float rr = ftanh(drive * emphR.process(xr)) / drive;
			bufL[w & mask] = rl; bufR[w & mask] = rr;
			float pl = hermiteRead(bufL, mask, w, d), pr = hermiteRead(bufR, mask, w, d);
			w++;
			// play: de-emphasis, head bump, gap loss
			float hl = hissNz.next(), hr = hissNz.next();
			float sl = ((1.f + k) * hl - k * hPrevL) * hnorm, sr = ((1.f + k) * hr - k * hPrevR) * hnorm;
			hPrevL = hl; hPrevR = hr;
			float yl = lpL.process(bumpL.process(deL.process(pl)) + sl);
			float yr = lpR.process(bumpR.process(deR.process(pr)) + sr);
			if (mix < target) mix = std::min(target, mix + mstep); else if (mix > target) mix = std::max(target, mix - mstep);
			L[i] = xl + (yl - xl) * mix;
			R[i] = xr + (yr - xr) * mix;
		}
	}
};

} // namespace hk
