// The engines compiled a second time with the AGE code removed (ESSPHOR_AGE=0), for checks 9 and 16a.
#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
typedef std::vector<double> Vec;
struct Stereo { Vec l, r; };
namespace noage {
// params: a GrainParams / ResParams from the normal build (same layout), copied byte for byte
Stereo runGrains(const Stereo& x, double fs, const void* params, size_t size, uint64_t seed);
Vec runRes(const Stereo& x, double fs, const void* params, size_t size);
}
