// Minimal WAV reading and writing (PCM 8/16/24/32-bit and 32/64-bit float; RIFF and WAVE_FORMAT_EXTENSIBLE).
// Used by the buffer load/save in Essphor and by the offline test renders. No Rack dependency.
#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace essphor {

struct WavData {
	int channels = 0;
	double sampleRate = 0;
	std::vector<float> l, r;     // r empty for mono
};

inline bool readWav(const std::string& path, WavData& out) {
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f) return false;
	std::vector<uint8_t> d;
	{
		uint8_t buf[65536]; size_t n;
		while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
		std::fclose(f);
	}
	auto u16 = [&](size_t i) { return (uint32_t)d[i] | ((uint32_t)d[i + 1] << 8); };
	auto u32 = [&](size_t i) { return u16(i) | (u16(i + 2) << 16); };
	if (d.size() < 12 || std::memcmp(&d[0], "RIFF", 4) || std::memcmp(&d[8], "WAVE", 4)) return false;
	int fmt = 0, ch = 0, bits = 0; uint32_t sr = 0; size_t dataPos = 0, dataLen = 0;
	size_t p = 12;
	while (p + 8 <= d.size()) {
		uint32_t len = u32(p + 4);
		if (!std::memcmp(&d[p], "fmt ", 4) && p + 24 <= d.size()) {
			fmt = (int)u16(p + 8); ch = (int)u16(p + 10); sr = u32(p + 12); bits = (int)u16(p + 22);
			if (fmt == 0xFFFE && p + 34 <= d.size()) fmt = (int)u16(p + 32);
		} else if (!std::memcmp(&d[p], "data", 4)) {
			dataPos = p + 8; dataLen = std::min<size_t>(len, d.size() - dataPos);
			break;
		}
		p += 8 + len + (len & 1);
	}
	if (!dataPos || ch < 1 || sr == 0 || (fmt != 1 && fmt != 3)) return false;
	const int bps = bits / 8;
	if (bps < 1) return false;
	const size_t frames = dataLen / (size_t)(bps * ch);
	out.channels = std::min(ch, 2); out.sampleRate = sr;
	out.l.assign(frames, 0.f); out.r.clear();
	if (ch > 1) out.r.assign(frames, 0.f);
	for (size_t i = 0; i < frames; i++) {
		for (int c = 0; c < std::min(ch, 2); c++) {
			size_t q = dataPos + (i * ch + c) * bps;
			double v = 0;
			if (fmt == 3 && bits == 32) { float x; std::memcpy(&x, &d[q], 4); v = x; }
			else if (fmt == 3 && bits == 64) { double x; std::memcpy(&x, &d[q], 8); v = x; }
			else if (bits == 8) v = (d[q] - 128) / 128.0;
			else if (bits == 16) v = (int16_t)u16(q) / 32768.0;
			else if (bits == 24) { int32_t x = (int32_t)(d[q] | (d[q + 1] << 8) | (d[q + 2] << 16)); if (x & 0x800000) x |= ~0xFFFFFF; v = x / 8388608.0; }
			else if (bits == 32) v = (int32_t)u32(q) / 2147483648.0;
			(c == 0 ? out.l : out.r)[i] = (float)v;
		}
	}
	return true;
}

// 16-bit PCM (float32 when asFloat) stereo or mono writer.
inline bool writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>* r, double sampleRate, bool asFloat = false) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return false;
	const int ch = r ? 2 : 1, bytes = asFloat ? 4 : 2;
	const uint32_t n = (uint32_t)l.size(), dataLen = n * ch * bytes, sr = (uint32_t)std::lround(sampleRate);
	auto w32 = [&](uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; std::fwrite(b, 1, 4, f); };
	auto w16 = [&](uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; std::fwrite(b, 1, 2, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + dataLen); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(asFloat ? 3 : 1); w16((uint16_t)ch); w32(sr); w32(sr * ch * bytes); w16((uint16_t)(ch * bytes)); w16((uint16_t)(bytes * 8));
	std::fwrite("data", 1, 4, f); w32(dataLen);
	std::vector<uint8_t> buf; buf.reserve((size_t)dataLen);
	for (uint32_t i = 0; i < n; i++) for (int c = 0; c < ch; c++) {
		float v = c == 0 ? l[i] : (*r)[i];
		if (asFloat) { uint8_t b[4]; std::memcpy(b, &v, 4); buf.insert(buf.end(), b, b + 4); }
		else { int s = (int)std::lround(std::max(-1.f, std::min(1.f, v)) * 32767.f); buf.push_back((uint8_t)s); buf.push_back((uint8_t)(s >> 8)); }
	}
	std::fwrite(buf.data(), 1, buf.size(), f);
	std::fclose(f);
	return true;
}

// Linear-interpolation resampler for loading files at another rate.
inline std::vector<float> resampleLinear(const std::vector<float>& x, double from, double to) {
	if (x.empty() || from == to) return x;
	const size_t n = (size_t)std::floor(x.size() * to / from);
	std::vector<float> y(n);
	for (size_t i = 0; i < n; i++) {
		double pos = i * from / to; size_t k = (size_t)pos; double t = pos - k;
		float a = x[std::min(k, x.size() - 1)], b = x[std::min(k + 1, x.size() - 1)];
		y[i] = (float)(a + (b - a) * t);
	}
	return y;
}

}  // namespace essphor
