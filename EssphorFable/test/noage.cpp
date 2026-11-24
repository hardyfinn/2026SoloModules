#define ESSPHOR_AGE 0
#define essphor essphor_noage
#include "../src/dsp/Grains.hpp"
#include "../src/dsp/Res.hpp"
#include "noage.hpp"
#include <cstring>
namespace noage {
Stereo runGrains(const Stereo& x, double fs, const void* params, size_t size, uint64_t seed) {
	essphor_noage::GrainEngine e;
	if (size != sizeof e.p) return Stereo();
	std::memcpy((void*)&e.p, params, size);
	e.seed(seed); e.setPatternSeed(seed); e.init(fs, 4);
	essphor_noage::Clock clk; clk.reset();
	Stereo o; o.l.resize(x.l.size()); o.r.resize(x.l.size());
	for (size_t i = 0; i < x.l.size(); i++) { clk.tick(1.0 / fs, false, false, i == 0, 120); double a, b; e.process(x.l[i], x.r[i], clk, a, b); o.l[i] = a; o.r[i] = b; }
	return o;
}
Vec runRes(const Stereo& x, double fs, const void* params, size_t size) {
	essphor_noage::ResEngine e;
	if (size != sizeof e.p) return Vec();
	std::memcpy((void*)&e.p, params, size);
	e.init(fs);
	Vec y(x.l.size());
	for (size_t i = 0; i < x.l.size(); i++) { double a, b; e.process(x.l[i], x.r[i], a, b); y[i] = a + b; }
	return y;
}
}
