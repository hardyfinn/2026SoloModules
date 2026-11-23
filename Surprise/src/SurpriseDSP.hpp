// Surprise engine: a follower that reacts to novelty, not loudness.
// The input is split into 8 bands; every 20 ms the band levels make one
// "frame". The last few frames (a short context) are compared with
// everything heard during MEMORY seconds. If something close has been heard
// before, the surprise is low; if nothing like it is in memory, it is high.
// So a loop goes quiet on the output after it has played once, and a new
// sound, a fill, a drop-out or a change of pattern stands out.
// BORED rises while nothing new happens and falls when something does.
// No Rack code here, so test/render.cpp can use it.
#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>

namespace surprise {

static const int BANDS = 8;
static const int DIMS = BANDS + 1;          // 8 band shapes + overall level
static const int CONTEXT = 3;               // frames compared at once (60 ms)
static const float HOP = 0.02f;             // seconds per frame
static const int MAX_FRAMES = 1600;         // 32 s of memory
static const float BAND_HZ[BANDS] = {60.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f};

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

struct Biquad {
	float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void bandpass(float fc, float q, float sr) {
		fc = std::min(fc, 0.45f * sr);
		float w = 2.f * 3.14159265f * fc / sr, al = std::sin(w) / (2.f * q), a0 = 1.f + al;
		b0 = al / a0; b1 = 0.f; b2 = -al / a0; a1 = -2.f * std::cos(w) / a0; a2 = (1.f - al) / a0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};

struct NoveltySettings {
	float memory = 8.f;     // seconds of the past to compare with (0.5 .. 32)
	float sens = 1.f;       // 0..2
	float focus = 0.f;      // -1 lows .. 0 all .. +1 highs
	float decay = 0.3f;     // seconds for the output to fall back
	float thresh = 0.4f;    // 0..1, where TRIG fires
	float boredRise = 60.f; // seconds of nothing new to reach full BORED
	bool freeze = false;    // stop learning
};

struct NoveltyEngine {
	float sr = 48000.f;
	Biquad bp[BANDS];
	float acc[BANDS] = {};
	int accN = 0, hopN = 960;
	float frames[MAX_FRAMES][DIMS];
	int written = 0;         // total frames written
	int head = 0;            // next slot
	float ctx[CONTEXT][DIMS] = {};
	int ctxN = 0;
	// outputs
	float raw = 0.f;         // surprise of the latest frame, 0..1
	float out = 0.f;         // smoothed, 0..1
	float bandOut[BANDS] = {};
	float bored = 0.f;       // 0..1
	bool trig = false;       // this sample
	bool high = false;
	float level = 0.f;       // input level of the latest frame, 0..1 (for the screen)

	NoveltyEngine() { std::memset(frames, 0, sizeof(frames)); setRate(48000.f); }
	void setRate(float r) {
		sr = r;
		for (int b = 0; b < BANDS; b++) bp[b].bandpass(BAND_HZ[b], 1.4f, sr);
		hopN = std::max(1, (int) std::round(HOP * sr));
	}
	void forget() { written = 0; head = 0; ctxN = 0; }

	void makeFrame(float* f) {
		float mean = 0.f;
		for (int b = 0; b < BANDS; b++) {
			float e = acc[b] / std::max(accN, 1);
			float db = 10.f * std::log10(e + 1e-9f);          // about -90 .. +20 for 5 V signals
			f[b] = clampf(db, -80.f, 20.f) / 20.f;
			mean += f[b];
		}
		mean /= BANDS;
		for (int b = 0; b < BANDS; b++) f[b] -= mean;      // spectral shape
		f[BANDS] = 0.5f * mean;                            // overall level counts a little
		level = clampf((mean * 20.f + 70.f) / 80.f, 0.f, 1.f);
	}

	// Called once per frame: compare the context with memory.
	void analyse(const NoveltySettings& s) {
		float w[DIMS], wsum = 0.f;
		for (int b = 0; b < BANDS; b++) {
			w[b] = std::exp2(2.f * s.focus * (b - 3.5f) / 3.5f);
			wsum += w[b];
		}
		w[BANDS] = 1.f;
		wsum += 1.f;
		int avail = std::min(written, MAX_FRAMES);
		int span = std::min(avail, (int) (s.memory / HOP));
		// candidates: memory frames that end at least CONTEXT frames ago, with a full context
		float best = 1e9f;
		int bestK = -1;
		for (int age = CONTEXT; age < span; age++) {
			if (age + CONTEXT - 1 >= avail) break;
			float d = 0.f;
			for (int c = 0; c < CONTEXT; c++) {
				int idx = (head - 1 - age - c + 2 * MAX_FRAMES) % MAX_FRAMES;
				const float* m = frames[idx];
				const float* x = ctx[c];
				for (int k = 0; k < DIMS; k++) d += w[k] * std::fabs(x[k] - m[k]);
				if (d >= best * wsum * CONTEXT) break;   // already worse
			}
			d /= wsum * CONTEXT;
			if (d < best) { best = d; bestK = age; }
		}
		if (bestK < 0) best = 1.f;   // nothing to compare with yet: all new
		raw = clampf((best - 0.04f) * 3.f * s.sens, 0.f, 1.f);
		for (int b = 0; b < BANDS; b++) {
			float db = 1.f;
			if (bestK >= 0) {
				int idx = (head - 1 - bestK + 2 * MAX_FRAMES) % MAX_FRAMES;
				db = std::fabs(ctx[0][b] - frames[idx][b]);
			}
			bandOut[b] = clampf((db - 0.05f) * 1.6f * s.sens, 0.f, 1.f);
		}
	}

	void process(float in, const NoveltySettings& s, float dt) {
		trig = false;
		for (int b = 0; b < BANDS; b++) {
			float y = bp[b].process(in);
			acc[b] += y * y;
		}
		accN++;
		if (accN >= hopN) {
			float f[DIMS];
			makeFrame(f);
			for (int b = 0; b < BANDS; b++) acc[b] = 0.f;
			accN = 0;
			// newest context frame first
			std::memmove(ctx[1], ctx[0], sizeof(float) * DIMS * (CONTEXT - 1));
			std::memcpy(ctx[0], f, sizeof(f));
			ctxN = std::min(ctxN + 1, CONTEXT);
			if (ctxN >= CONTEXT) analyse(s);
			if (!s.freeze) {
				std::memcpy(frames[head], f, sizeof(f));
				head = (head + 1) % MAX_FRAMES;
				written++;
			}
		}
		// smoothing: fast rise, DECAY fall
		float target = raw;
		if (target > out) out += (target - out) * (1.f - std::exp(-dt / 0.005f));
		else out += (target - out) * (1.f - std::exp(-dt / std::max(s.decay, 0.005f)));
		// trigger with hysteresis
		if (!high && out > s.thresh) { high = true; trig = true; }
		else if (high && out < s.thresh * 0.6f) high = false;
		// boredom: climbs while nothing is new, drains when something is
		if (out < 0.15f) bored += dt / std::max(s.boredRise, 1.f);
		bored -= out * dt * 1.5f;
		bored = clampf(bored, 0.f, 1.f);
	}
};

} // namespace surprise
