// WAV reading/writing shared by Thunder and Lightning.
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <algorithm>
#include "ThunderEngine.hpp"

// Minimal WAV reader: PCM 8/16/24/32 bit or float, any channel count (mixed to mono), up to 20 s.
static inline bool readWav(const std::string& path, thunder::Sample& out, std::string& err) {
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		err = "could not open file";
		return false;
	}
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	auto u16 = [&](size_t i) { return (uint32_t) b[i] | ((uint32_t) b[i + 1] << 8); };
	auto u32 = [&](size_t i) { return u16(i) | (u16(i + 2) << 16); };
	if (b.size() < 44 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) {
		err = "not a WAV file";
		return false;
	}
	int fmt = 0, ch = 0, bits = 0;
	uint32_t rate = 0;
	size_t dataPos = 0, dataLen = 0;
	size_t p = 12;
	while (p + 8 <= b.size()) {
		uint32_t len = u32(p + 4);
		if (!std::memcmp(&b[p], "fmt ", 4) && p + 24 <= b.size()) {
			fmt = u16(p + 8);
			ch = u16(p + 10);
			rate = u32(p + 12);
			bits = u16(p + 22);
			if (fmt == 0xFFFE && p + 34 <= b.size()) fmt = u16(p + 32);
		}
		else if (!std::memcmp(&b[p], "data", 4)) {
			dataPos = p + 8;
			dataLen = std::min((size_t) len, b.size() - dataPos);
		}
		p += 8 + len + (len & 1);
	}
	if (!dataPos || ch < 1 || rate < 1000 || !(fmt == 1 || fmt == 3)) {
		err = "unsupported WAV format";
		return false;
	}
	int bps = bits / 8;
	if (bps < 1 || (fmt == 3 && bits != 32)) {
		err = "unsupported bit depth";
		return false;
	}
	size_t frames = dataLen / (bps * ch);
	frames = std::min(frames, (size_t) rate * 20);
	out.data.assign(frames, 0.f);
	out.sr = (float) rate;
	for (size_t i = 0; i < frames; i++) {
		float s = 0.f;
		for (int c = 0; c < ch; c++) {
			size_t q = dataPos + (i * ch + c) * bps;
			float v = 0.f;
			if (fmt == 3) {
				uint32_t w = u32(q);
				std::memcpy(&v, &w, 4);
			}
			else if (bits == 8)
				v = (b[q] - 128) / 128.f;
			else if (bits == 16)
				v = (int16_t) u16(q) / 32768.f;
			else if (bits == 24) {
				int32_t w = (int32_t) ((b[q] << 8) | (b[q + 1] << 16) | (b[q + 2] << 24)) >> 8;
				v = w / 8388608.f;
			}
			else if (bits == 32)
				v = (int32_t) u32(q) / 2147483648.f;
			s += v;
		}
		out.data[i] = s / ch;
	}
	return true;
}

// 32-bit float mono WAV
static inline bool writeWav(const std::string& path, const float* data, int len, float rate) {
	std::ofstream f(path, std::ios::binary);
	if (!f) return false;
	auto w32 = [&](uint32_t v) { f.put(v & 255); f.put((v >> 8) & 255); f.put((v >> 16) & 255); f.put((v >> 24) & 255); };
	auto w16 = [&](uint32_t v) { f.put(v & 255); f.put((v >> 8) & 255); };
	uint32_t bytes = (uint32_t) len * 4;
	f.write("RIFF", 4); w32(36 + bytes); f.write("WAVE", 4);
	f.write("fmt ", 4); w32(16); w16(3); w16(1); w32((uint32_t) rate); w32((uint32_t) rate * 4); w16(4); w16(32);
	f.write("data", 4); w32(bytes);
	f.write((const char*) data, bytes);
	return (bool) f;
}
