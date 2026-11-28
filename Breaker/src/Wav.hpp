// Small WAV reader: PCM 8/16/24/32-bit and 32/64-bit float, mono or stereo (more
// channels: the first two are used). Mono files are copied to both sides.
#pragma once
#include "Analysis.hpp"
#include <cstdio>
#include <cstring>

namespace breaker {

inline std::shared_ptr<Audio> loadWav(const std::string& path) {
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f) return nullptr;
	std::vector<uint8_t> d;
	uint8_t buf[65536];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
	std::fclose(f);
	auto u16 = [&](size_t i) { return (uint32_t) d[i] | ((uint32_t) d[i + 1] << 8); };
	auto u32 = [&](size_t i) { return u16(i) | (u16(i + 2) << 16); };
	if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return nullptr;
	int format = 0, channels = 0, bits = 0;
	float rate = 44100.f;
	size_t pos = 12;
	while (pos + 8 <= d.size()) {
		size_t len = u32(pos + 4), body = pos + 8;
		if (body + len > d.size()) len = d.size() - body;
		if (!std::memcmp(d.data() + pos, "fmt ", 4) && len >= 16) {
			format = u16(body); channels = u16(body + 2); rate = (float) u32(body + 4); bits = u16(body + 14);
			if (format == 0xFFFE && len >= 26) format = u16(body + 24);   // WAVE_FORMAT_EXTENSIBLE
		}
		else if (!std::memcmp(d.data() + pos, "data", 4) && channels > 0 && bits >= 8) {
			int bytes = bits / 8;
			size_t frames = len / (bytes * channels);
			if (!frames) return nullptr;
			auto a = std::make_shared<Audio>();
			a->rate = rate;
			a->L.resize(frames); a->R.resize(frames);
			for (size_t fr = 0; fr < frames; fr++) {
				float v[2] = {0, 0};
				for (int c = 0; c < std::min(channels, 2); c++) {
					size_t i = body + (fr * channels + c) * bytes;
					float x = 0.f;
					if (format == 3 && bits == 32) { uint32_t u = u32(i); std::memcpy(&x, &u, 4); }
					else if (format == 3 && bits == 64) { double dd; std::memcpy(&dd, &d[i], 8); x = (float) dd; }
					else if (bits == 16) x = (int16_t) u16(i) / 32768.f;
					else if (bits == 24) x = (int32_t) ((u16(i) << 8) | ((uint32_t) d[i + 2] << 24)) / 2147483648.f;
					else if (bits == 32) x = (int32_t) u32(i) / 2147483648.f;
					else if (bits == 8) x = (d[i] - 128) / 128.f;
					v[c] = x;
				}
				a->L[fr] = v[0];
				a->R[fr] = channels > 1 ? v[1] : v[0];
			}
			return a;
		}
		pos = body + len + (len & 1);
	}
	return nullptr;
}

} // namespace breaker
