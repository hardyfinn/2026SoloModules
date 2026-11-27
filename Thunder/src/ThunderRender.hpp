// Renders one Thunder hit offline, start to finish, from the values it was played with.
// Used by Thunder's background thread when Lightning keeps a hit. No Rack code here.
#pragma once
#include "ThunderEngine.hpp"
#include "ThunderPresets.hpp"
#include "LightningCore.hpp"
#include <memory>

namespace thunder {

// v: 24 memory values + [24] accent (0..1) + [25] level. samples: Thunder's 3 sample slots.
// out is resized (at least MAX_SEC worth, so Lightning can reuse it). Returns the sound's length.
inline int renderHit(const float* v, const Sample* samples, float sr, std::vector<float>& out) {
	std::unique_ptr<Engine> e(new Engine());
	e->setSampleRate(sr);
	Controls c;
	valuesToControls(v, c);
	if (c.algoA == 10 || c.algoB == 10)
		for (int k = 0; k < 3; k++) e->samples[k] = samples[k];
	c.level = v[25];
	c.lock = true;    // MUTATE's wander is already folded into v
	c.mutate = 0.f;
	c.wear = 0.f;     // a clean copy
	c.sendPatched = false;
	int maxLen = (int) (lightning::MAX_SEC * sr);
	size_t cap = (size_t) maxLen + 16;
	if (out.size() < cap) out.resize(cap);
	float quiet = lightning::quietForThunder(v);
	int quietLen = (int) (quiet * sr);
	e->hit(std::max(0.f, std::min(1.f, v[24])), c);
	float peak = 0.f;
	int quietSince = -1, n = 0;
	for (; n < maxLen; n++) {
		e->process(c);
		float x = e->out;
		out[(size_t) n] = x;
		float a = std::fabs(x);
		if (a > peak) peak = a;
		float th = std::max(peak * 0.0005f, 2e-4f);
		if (a > th) quietSince = -1;
		else if (quietSince < 0) quietSince = n;
		if (peak > 1e-3f && quietSince >= 0 && n - quietSince > quietLen) break;
	}
	bool full = n >= maxLen; // a drone, or very long echoes: fade the last 50 ms
	// end: last sample above -66 dB of the peak
	float th = std::max(peak * 0.0005f, 2e-4f);
	int end = std::min(n, maxLen);
	int j = end - 1;
	while (j > 0 && std::fabs(out[(size_t) j]) < th) j--;
	int len = std::min(end, j + 1 + (int) (0.002f * sr));
	if (len <= 0) return 0;
	int fo = std::min(len / 2, (int) ((full ? 0.05f : 0.002f) * sr));
	for (int i = 0; i < fo; i++) out[(size_t) (len - 1 - i)] *= (float) i / fo;
	return len;
}

} // namespace thunder
