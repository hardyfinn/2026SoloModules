// WAV reading for 909 Cymbals (Rack-free so the offline test can check it).
// PCM 8/16/24/32-bit and 32/64-bit float, any channel count (mixed to mono). Anything else
// is refused rather than loaded as silence.
#pragma once
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include "Common909.hpp"

namespace c909 {

static inline bool readWavFile(const std::string& path, std::vector<float>& out, float& sr) {
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f) return false;
	std::vector<uint8_t> b;
	uint8_t buf[65536];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && b.size() < (size_t)200e6) b.insert(b.end(), buf, buf + n);
	std::fclose(f);
	auto u16 = [&](size_t i) { return (uint32_t)b[i] | ((uint32_t)b[i + 1] << 8); };
	auto u32 = [&](size_t i) { return u16(i) | (u16(i + 2) << 16); };
	if (b.size() < 12 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) return false;
	int fmt = 0, ch = 0, bits = 0, align = 0;
	size_t p = 12;
	while (p + 8 <= b.size()) {
		uint32_t len = u32(p + 4);
		size_t body = p + 8;
		if (!std::memcmp(&b[p], "fmt ", 4) && body + 16 <= b.size()) {
			fmt = (int)u16(body); ch = (int)u16(body + 2); sr = (float)u32(body + 4); align = (int)u16(body + 12); bits = (int)u16(body + 14);
			if (fmt == 0xFFFE && body + 26 <= b.size()) fmt = (int)u16(body + 24);
		}
		else if (!std::memcmp(&b[p], "data", 4) && ch > 0) {
			// only formats we can read: PCM 8/16/24/32, float 32/64
			bool pcm = fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
			bool flt = fmt == 3 && (bits == 32 || bits == 64);
			if (!pcm && !flt) return false;
			int bps = bits / 8;
			int frameBytes = std::max(align, bps * ch);
			if (frameBytes < bps * ch || ch > 64) return false;
			size_t bytes = std::min((size_t)len, b.size() - body);
			size_t frames = bytes / frameBytes;
			out.assign(frames, 0.f);
			for (size_t i = 0; i < frames; i++) {
				float acc = 0.f;
				for (int c = 0; c < ch; c++) {
					size_t q = body + i * frameBytes + c * bps;
					float v = 0.f;
					if (flt && bits == 32) { float x; std::memcpy(&x, &b[q], 4); v = x; }
					else if (flt) { double x; std::memcpy(&x, &b[q], 8); v = (float)x; }
					else if (bits == 8) v = (b[q] - 128) / 128.f;
					else if (bits == 16) v = (int16_t)u16(q) / 32768.f;
					else if (bits == 24) v = (int32_t)((b[q] << 8) | (b[q + 1] << 16) | ((uint32_t)b[q + 2] << 24)) / 2147483648.f;
					else if (bits == 32) v = (int32_t)u32(q) / 2147483648.f;
					acc += std::isfinite(v) ? clampf(v, -4.f, 4.f) : 0.f;
				}
				out[i] = acc / ch;
			}
			return frames > 0 && sr > 1000.f;
		}
		p = body + len + (len & 1);
	}
	return false;
}

}  // namespace c909
