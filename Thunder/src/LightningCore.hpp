// Lightning core: catches whole hits from a mono signal and plays them back from 4 slots.
// No Rack code here, so test/lightning.cpp can run it offline.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lightning {

static const int NUM_SLOTS = 4;
static const float MAX_SEC = 8.f;    // longest hit a slot keeps
static const int NUM_VALS = 24;      // Thunder memory values

// ---- the recorder: a ring of the last few seconds plus where each hit started and ended ----
struct Recorder {
	std::vector<float> ring;
	int64_t size = 0;
	int64_t w = 0; // samples written so far
	float sr = 48000.f;

	// the hit being heard now
	bool inHit = false;
	int64_t hitStart = 0, quietSince = -1;
	float peak = 0.f;
	bool curExact = false; // start came from Thunder (exact), not from level detection
	float curVals[NUM_VALS];

	// the last finished hit
	bool haveDone = false;
	int64_t doneStart = 0, doneEnd = 0;
	float donePeak = 0.f;
	bool doneExact = false;
	float doneVals[NUM_VALS];
	uint32_t doneSerial = 0;

	// how long it must stay quiet before a hit counts as over (longer when echoes may follow)
	float quietSec = 0.08f;
	uint32_t startCount = 0;

	// level detection (audio IN): a hit starts when the fast envelope jumps well above the slow one
	float fastEnv = 0.f, slowEnv = 0.f;
	int64_t lastOnset = -1000000;

	Recorder() {
		std::fill(curVals, curVals + NUM_VALS, 0.f);
		std::fill(doneVals, doneVals + NUM_VALS, 0.f);
	}

	void setSampleRate(float s) {
		if (s == sr && size > 0) return;
		sr = s;
		size = (int64_t) (sr * (MAX_SEC + 2.f));
		ring.assign((size_t) size, 0.f);
		w = 0;
		inHit = false;
		haveDone = false;
	}

	float at(int64_t i) const { return ring[(size_t) (i % size)]; }

	void finish(int64_t end) {
		inHit = false;
		if (peak < 1e-3f) return; // nothing audible: not a hit
		haveDone = true;
		doneStart = hitStart;
		doneEnd = std::max(end, hitStart + 1);
		donePeak = peak;
		doneExact = curExact;
		std::copy(curVals, curVals + NUM_VALS, doneVals);
		doneSerial++;
	}

	// a new hit starts at sample index `start` (a later start ends the hit before it)
	void startHit(int64_t start, bool exact, const float* vals, float quiet = 0.08f) {
		if (inHit) finish(start);
		inHit = true;
		startCount++;
		quietSec = quiet;
		hitStart = start;
		peak = 0.f;
		quietSince = -1;
		curExact = exact;
		if (vals) std::copy(vals, vals + NUM_VALS, curVals);
		else std::fill(curVals, curVals + NUM_VALS, 0.f);
	}

	// one sample in. detect: find hit starts by level (audio IN); thresh in volts
	void push(float x, bool detect, float thresh) {
		if (detect) {
			float a = std::fabs(x);
			fastEnv = std::max(a, fastEnv * std::exp(-1.f / (0.002f * sr)));
			slowEnv += (fastEnv - slowEnv) * (1.f - std::exp(-1.f / (0.03f * sr)));
			if (fastEnv > thresh && fastEnv > slowEnv * 2.5f && w - lastOnset > (int64_t) (0.05f * sr)) {
				lastOnset = w;
				startHit(std::max<int64_t>(0, w - (int64_t) (0.002f * sr)), false, nullptr, 0.25f);
			}
		}
		ring[(size_t) (w % size)] = x;
		w++;
		if (!inHit) return;
		float a = std::fabs(x);
		if (a > peak) peak = a;
		float th = std::max(peak * 0.0005f, 2e-4f); // -66 dB under the peak
		if (a > th) quietSince = -1;
		else if (quietSince < 0) quietSince = w;
		if (peak > 1e-3f && quietSince >= 0 && w - quietSince > (int64_t) (quietSec * sr)) finish(quietSince);
		else if (w - hitStart >= (int64_t) (MAX_SEC * sr)) finish(w);
		else if (peak < 1e-3f && w - hitStart > (int64_t) sr) inHit = false; // silence: give up
	}

	// copy the last finished hit, trimmed, into dst (dst keeps its capacity). Returns length.
	int copyDone(std::vector<float>& dst) const {
		if (!haveDone) return 0;
		int64_t s = doneStart, e = doneEnd;
		if (w - s > size) return 0; // already overwritten
		// start: Thunder's is exact; a detected one moves to the first audible sample (minus 0.5 ms)
		if (!doneExact) {
			float th = std::max(donePeak * 0.01f, 2e-4f);
			int64_t i = s;
			while (i < e && std::fabs(at(i)) < th) i++;
			s = std::max(s, i - (int64_t) (0.0005f * sr));
		}
		// end: last sample above -66 dB of the peak
		float th = std::max(donePeak * 0.0005f, 2e-4f);
		int64_t j = e - 1;
		while (j > s && std::fabs(at(j)) < th) j--;
		e = std::min(e, j + 1 + (int64_t) (0.002f * sr));
		int64_t len = std::min<int64_t>(e - s, (int64_t) dst.size());
		if (len <= 0) return 0;
		for (int64_t i = 0; i < len; i++) dst[(size_t) i] = at(s + i);
		// 2 ms fade out (and 0.3 ms in for detected starts) so nothing clicks
		int fo = std::min<int>((int) len / 2, (int) (0.002f * sr));
		for (int i = 0; i < fo; i++) dst[(size_t) (len - 1 - i)] *= (float) i / fo;
		if (!doneExact) {
			int fi = std::min<int>((int) len / 2, (int) (0.0003f * sr));
			for (int i = 0; i < fi; i++) dst[(size_t) i] *= (float) i / fi;
		}
		return (int) len;
	}
};

// how long a Thunder hit may go quiet before its delay echoes come back (memory values 18-20)
inline float quietForThunder(const float* v) {
	float send = v[18], time = 0.05f * std::pow(20.f, std::max(0.f, std::min(1.f, v[19])));
	if (send < 0.01f) return 0.08f;
	return time * 1.2f + 0.1f;
}

// ---- a kept sound ----
struct Clip {
	std::vector<float> data; // capacity; only the first len samples are the sound
	int len = 0;
	float rate = 48000.f;
};

// ---- playback: two voices per slot so a retrigger fades the old one (3 ms) instead of cutting ----
struct Voice {
	bool active = false;
	int buf = 0;
	double pos = 0.0;
	float env = 1.f;
	float fade = 1.f;
	bool fading = false;
};

inline float hermite(const float* d, int len, double pos) {
	int i = (int) pos;
	float t = (float) (pos - i);
	auto s = [&](int k) { return (k >= 0 && k < len) ? d[k] : 0.f; };
	float y0 = s(i - 1), y1 = s(i), y2 = s(i + 1), y3 = s(i + 2);
	float c1 = 0.5f * (y2 - y0);
	float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
	float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
	return ((c3 * t + c2) * t + c1) * t + y1;
}

struct SlotPlayer {
	Voice v[2];
	float lastPos = 0.f; // 0..1, for the display

	void trigger(int buf) {
		for (int k = 0; k < 2; k++)
			if (v[k].active) v[k].fading = true;
		int k = !v[0].active ? 0 : !v[1].active ? 1 : (v[0].fade < v[1].fade ? 0 : 1);
		Voice& n = v[k];
		n.active = true;
		n.buf = buf;
		n.pos = 0.0;
		n.env = 1.f;
		n.fade = 1.f;
		n.fading = false;
	}

	void stopBuffer(int buf) {
		for (int k = 0; k < 2; k++)
			if (v[k].buf == buf) v[k].active = false;
	}

	// semis: tune; decay 0..1 (1 = the whole sound); clips[2] = this slot's double buffer
	float process(const Clip* clips, float sr, float semis, float decay) {
		float out = 0.f;
		float envMul = 1.f;
		if (decay < 0.99f) {
			float tau = 0.015f * std::pow(200.f, decay); // 15 ms .. 3 s
			envMul = std::exp(-1.f / (tau * sr));
		}
		float fadeStep = 1.f / (0.003f * sr);
		float ratio = std::pow(2.f, semis / 12.f);
		for (int k = 0; k < 2; k++) {
			Voice& vo = v[k];
			if (!vo.active) continue;
			const Clip& c = clips[vo.buf];
			if (c.len <= 0 || vo.pos >= c.len) {
				vo.active = false;
				continue;
			}
			float s = hermite(c.data.data(), c.len, vo.pos);
			out += s * vo.env * vo.fade;
			vo.pos += ratio * c.rate / sr;
			vo.env *= envMul;
			if (vo.fading) {
				vo.fade -= fadeStep;
				if (vo.fade <= 0.f) vo.active = false;
			}
			if (vo.env < 1e-4f) vo.active = false;
			if (!vo.fading) lastPos = (float) (vo.pos / c.len);
		}
		if (!v[0].active && !v[1].active) lastPos = 0.f;
		return out;
	}
};

} // namespace lightning
