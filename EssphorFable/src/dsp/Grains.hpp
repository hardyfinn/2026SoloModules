// Essphor grain engine. Follows tools/proto/mosaic_proto.py and section 5 of docs/BUILD-INSTRUCTIONS.md.
// No Rack dependency. Internal audio: 1.0 = 5 V.
#pragma once
#include "Common.hpp"
#include <vector>

namespace essphor {

enum ScanMode { SCAN_STRETCH, SCAN_POSITION, SCAN_DELAY_SYNC, SCAN_DELAY_FREE };
enum SprayMode { SPRAY_RANDOM, SPRAY_RANDOM_MONO, SPRAY_WARP, SPRAY_WARP_MONO };
enum RandPitchMode { RP_DETUNE, RP_RANDOM, RP_RANDOM_MONO };
enum FbMode { FB_GRAIN, FB_SOS };

static const int MAX_GRAINS = 64;
#ifndef FB_CAL_VALUE
#define FB_CAL_VALUE 2.0
#endif
static const double FB_CAL = FB_CAL_VALUE;   // feedback loop gain at FEEDBACK 1 (before the limiter)
static const int NUM_RATE_DIVS = 9;
static const double RATE_DIVS[NUM_RATE_DIVS] = {16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 0.0625};   // beats: 4/1 .. 1/64
static const char* const RATE_DIV_NAMES[NUM_RATE_DIVS] = {"4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
static const int NUM_DELAY_DIVS = 11;
static const double DELAY_DIVS[NUM_DELAY_DIVS] = {0.0625, 0.125, 0.25, 0.5, 0.75, 1, 1.5, 2, 4, 8, 16};
static const char* const DELAY_DIV_NAMES[NUM_DELAY_DIVS] = {"1/64", "1/32", "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1/1", "2/1", "4/1"};

// Knob mappings (knob position 0..1 to physical value), shared with the Rack wrapper's displays.
inline int rateDivIndex(double k) { return std::max(0, std::min(NUM_RATE_DIVS - 1, (int)std::lround(k * (NUM_RATE_DIVS - 1)))); }
inline double rateFreeHz(double k) { return 0.25 * std::pow(1000.0, std::max(0.0, std::min(1.0, k))); }
inline double sizeBeats(double k) { return (1.0 / 64.0) * std::pow(512.0, std::max(0.0, std::min(1.0, k))); }
inline double stretchSpeed(double k) { return -2.0 + 4.0 * k; }
inline int delayDivIndex(double k) { return std::max(0, std::min(NUM_DELAY_DIVS - 1, (int)std::lround(k * (NUM_DELAY_DIVS - 1)))); }
inline double delayFreeSec(double k, double bufSec) { k = std::max(0.0, std::min(1.0, k)); return k * k * (bufSec - 0.05); }

struct GrainParams {
	double pitch = 0.0;          // semitones: knob + CV + V/OCT, before quantising
	int scale = 0;
	bool sync = true;
	double rateKnob = 0.75;      // SYNC: snapped to RATE_DIVS; FREE: 0.25..250 Hz
	double sizeKnob = 4.0 / 9.0; // 1/64 .. 8 beats
	double contour = 0.0;
	int scanMode = SCAN_STRETCH;
	double scan = 0.625;
	int sprayMode = SPRAY_RANDOM;
	double spray = 0.0;
	double pattern = 0.0;
	double wet = 0.5;
	bool send = false;
	int fbMode = FB_GRAIN;
	double feedback = 0.0;
	int rpMode = RP_DETUNE;
	double randPitch = 0.0, randRate = 0.0, randSize = 0.0, randAmp = 0.0, randRev = 0.0;
	double age = 0.0;
	bool lock = false;
	bool run = true;             // false: no new grains (clock stopped and "keep running" off)
};

struct Grain {
	bool on = false;
	double age = 0, len = 1, pos = 0, inc = 1, gL = 0, gR = 0, peak = 0.5, lin = 0;
};

struct GrainEngine {
	GrainParams p;
	double fs = 0;
	double bufSec = 4.0;
	int Bn = 0;
	std::vector<float> bufL, bufR;
	int w = 0;                   // write head: the next sample to be written
	double play = 0.0;           // STRETCH playhead
	double lockA = 0.0;          // 0 = writing new audio, 1 = locked
	Grain g[MAX_GRAINS];
	Rng rng, patRng;
	uint64_t patternSeed = 1;
	double patternVals[16];
	int step = 0;
	SlowRandom warp0, warp1, drift;
	double hpL = 0, hpR = 0, aHp = 0, fbEnv = 0, fbRel = 0;
	double wetL = 0, wetR = 0;   // previous sample's wet signal, for feedback
	double lpL1 = 0, lpL2 = 0, lpR1 = 0, lpR2 = 0;
	// grid
	double gridPrev = 0, nextTarget = 0; long long nextK = 0; bool gridMode = true; int gridDiv = -1; bool needResync = true;
	// outputs for the wrapper
	bool fired = false; double firedLevel = 0; int active = 0; bool beat = false;
	double lastBeat = 0;

	void setPatternSeed(uint64_t s) {
		patternSeed = s ? s : 1;
		patRng.seed(patternSeed);
		for (int i = 0; i < 16; i++) patternVals[i] = patRng.bipolar();
	}

	void init(double sampleRate, double seconds) {
		fs = sampleRate; bufSec = seconds;
		Bn = std::max(1024, (int)std::lround(bufSec * fs));
		bufL.assign(Bn, 0.f); bufR.assign(Bn, 0.f);
		w = 0; play = 0; lockA = p.lock ? 1.0 : 0.0;
		for (auto& gr : g) gr.on = false;
		warp0.init(0.35, fs); warp1.init(0.35, fs); drift.init(1.5, fs);
		aHp = 1.0 - std::exp(-2 * PI * 30.0 / fs);
		fbRel = std::exp(-1.0 / (0.2 * fs));
		hpL = hpR = fbEnv = wetL = wetR = 0;
		lpL1 = lpL2 = lpR1 = lpR2 = 0;
		needResync = true;
	}
	void seed(uint64_t s) { rng.seed(s); }
	void clearBuffer() { std::fill(bufL.begin(), bufL.end(), 0.f); std::fill(bufR.begin(), bufR.end(), 0.f); }

	// Replace the buffer contents (mono or stereo, already at fs) and lock.
	void loadBuffer(const std::vector<float>& l, const std::vector<float>& r) {
		for (int i = 0; i < Bn; i++) {
			bufL[i] = i < (int)l.size() ? l[i] : 0.f;
			bufR[i] = i < (int)r.size() ? r[i] : bufL[i];
		}
		w = 0; lockA = 1.0;
	}

	inline double readHermite(const std::vector<float>& b, double pos) const {
		double fl = std::floor(pos);
		int i1 = (int)fl; double t = pos - fl;
		i1 %= Bn; if (i1 < 0) i1 += Bn;
		int i0 = i1 - 1; if (i0 < 0) i0 += Bn;
		int i2 = i1 + 1; if (i2 >= Bn) i2 -= Bn;
		int i3 = i2 + 1; if (i3 >= Bn) i3 -= Bn;
		double y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
		double c1 = 0.5 * (y2 - y0);
		double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
		double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
		return ((c3 * t + c2) * t + c1) * t + y1;
	}

	double wrap(double x) const { x = std::fmod(x, (double)Bn); return x < 0 ? x + Bn : x; }

	void spawn(double bps) {
		const double age = p.age;
		const double rateHz = p.sync ? bps / RATE_DIVS[rateDivIndex(p.rateKnob)] : rateFreeHz(p.rateKnob);
		const double sizeSec0 = sizeBeats(p.sizeKnob) / std::max(1e-6, bps);
		// 1. length
		double sizeSec = sizeSec0 * std::pow(2.0, -3.0 * rng.uniform() * p.randSize);
		sizeSec = std::max(0.002, std::min(bufSec / 2, sizeSec));
		double L = std::max(2.0, sizeSec * fs);
		// 2. pitch
		double semi = p.pitch + patternVals[step & 15] * 24.0 * p.pattern;
		double panr = 0.0;
		if (p.rpMode != RP_DETUNE) {
			semi += rng.bipolar() * 24.0 * p.randPitch;
			if (p.rpMode == RP_RANDOM) panr = rng.bipolar() * std::min(1.0, 2.0 * p.randPitch);
		}
		semi = quantise(semi, p.scale);
		if (p.rpMode == RP_DETUNE) semi += rng.bipolar() * p.randPitch;
		semi = std::max(-36.0, std::min(36.0, semi));
		if (ESSPHOR_AGE && age > 0) semi += age * 0.35 * std::max(-1.0, std::min(1.0, 3.0 * drift.y));
		const double r = std::pow(2.0, semi / 12.0);
		// 3. direction
		const bool rev = rng.uniform() < p.randRev;
		// 4. playhead
		const bool locked = lockA >= 1.0;
		double start;
		switch (p.scanMode) {
			case SCAN_STRETCH: start = play; break;
			case SCAN_POSITION: start = (locked ? w : 0) + p.scan * Bn; break;
			case SCAN_DELAY_SYNC: start = w - std::min(DELAY_DIVS[delayDivIndex(p.scan)] / std::max(1e-6, bps), bufSec - 0.05) * fs; break;
			default: start = w - delayFreeSec(p.scan, bufSec) * fs; break;
		}
		// 5. spray
		double pan = panr, so;
		if (p.sprayMode == SPRAY_RANDOM || p.sprayMode == SPRAY_RANDOM_MONO) {
			so = rng.bipolar() * p.spray * fs;
			if (p.sprayMode == SPRAY_RANDOM) pan += rng.bipolar() * std::min(1.0, 2.0 * p.spray);
		} else {
			so = warp0.y * p.spray * fs * 3.0;
			if (p.sprayMode == SPRAY_WARP) pan += std::max(-1.0, std::min(1.0, 3.0 * warp1.y)) * std::min(1.0, 2.0 * p.spray);
		}
		pan = std::max(-1.0, std::min(1.0, pan));
		start = wrap(start + so);
		// 6. keep clear of the write head
		if (!locked) {
			const double guard = 0.001 * fs;
			double d = wrap(w - start);
			double lo = guard, hi = Bn - guard;
			for (int it = 0; it < 40; it++) {
				if (!rev) { lo = guard + std::max(0.0, (r - 1) * L); hi = Bn - guard - std::max(0.0, (1 - r) * L); }
				else { lo = guard; hi = Bn - guard - (1 + r) * L; }
				if (lo <= hi) break;
				L *= 0.5;
			}
			d = std::max(lo, std::min(hi, d));
			start = wrap(w - d);
		}
		// 7. level
		const double decor = std::min(1.0, 8 * p.spray + 4 * p.pattern + 4 * p.randPitch + 2 * p.randRev + std::fabs(semi));
		const double comp = std::pow(std::max(1.0, rateHz * sizeSec), -(1.0 - 0.5 * decor));
		const double gain = (1.0 - rng.uniform() * p.randAmp) * comp;
		// 8. stereo
		const double th = (pan + 1) * PI / 4;
		// 9. window
		const double c = std::max(-1.0, std::min(1.0, p.contour));
		const double e = std::min(0.45, 0.001 * fs / L);
		// 10. voice
		int v = -1; double best = -1;
		for (int i = 0; i < MAX_GRAINS; i++) {
			if (!g[i].on) { v = i; break; }
			double f = g[i].age / g[i].len;
			if (f > best) { best = f; v = i; }
		}
		Grain& gr = g[v];
		gr.on = true; gr.age = 0; gr.len = L; gr.pos = start; gr.inc = rev ? -r : r;
		gr.gL = gain * std::sqrt(2.0) * std::cos(th); gr.gR = gain * std::sqrt(2.0) * std::sin(th);
		gr.peak = std::max(e, std::min(1 - e, 0.5 + 0.5 * c)); gr.lin = std::fabs(c);
		step++;
		fired = true; firedLevel = gain;
	}

	// One sample. inL/inR internal units. Returns outputs through outL/outR (internal, after the limiter).
	void process(double inL, double inR, const Clock& clk, double& outL, double& outR) {
		fired = false; beat = false;
		const double age = p.age;
		if (clk.resetNow) { step = 0; needResync = true; gridPrev = 0; }
		// smooth random signals
		warp0.step(rng); warp1.step(rng);
		if (ESSPHOR_AGE && age > 0) drift.step(rng);
		// AGE record path
		double rL = inL, rR = inR;
		if (ESSPHOR_AGE && age > 0) {
			const double fc = std::min(0.45 * fs, 18000.0 * std::pow(5.0 / 18.0, age));
			const double a = 1.0 - std::exp(-2 * PI * fc / fs);
			lpL1 += a * (rL - lpL1); lpL2 += a * (lpL1 - lpL2); rL = lpL2;
			lpR1 += a * (rR - lpR1); lpR2 += a * (lpR1 - lpR2); rR = lpR2;
			const double dr = 1 + 2 * age;
			const double hiss = std::pow(10.0, (-84 + 34 * age) / 20);
			rL = std::tanh(rL * dr) / dr + rng.normal() * hiss;
			rR = std::tanh(rR * dr) / dr + rng.normal() * hiss;
			const double q = std::pow(2.0, 1 - (16 - 7 * age));
			rL = std::round(rL / q + rng.bipolar() * 0.5 * (1 - age)) * q;
			rR = std::round(rR / q + rng.bipolar() * 0.5 * (1 - age)) * q;
		}
		// feedback signal from the previous sample's grains
		double fbL = 0, fbR = 0;
		{
			const double pk = std::max(std::fabs(wetL), std::fabs(wetR));
			fbEnv = std::max(pk, fbEnv * fbRel);
			hpL += aHp * (wetL - hpL); hpR += aHp * (wetR - hpR);
			if (p.fbMode == FB_GRAIN && p.feedback > 0) {
				const double lim = std::min(1.0, 0.5 / std::max(fbEnv, 1e-9));
				const double k = FB_CAL * p.feedback * lim;
				fbL = std::tanh((wetL - hpL) * k); fbR = std::tanh((wetR - hpR) * k);
			}
		}
		// lock fade (10 ms) and recording
		const double lockStep = 1.0 / (0.01 * fs);
		if (p.lock) lockA = std::min(1.0, lockA + lockStep); else lockA = std::max(0.0, lockA - lockStep);
		if (lockA < 1.0) {
			double nL, nR;
			if (p.fbMode == FB_SOS) {
				nL = sosclip(rL + p.feedback * bufL[w]); nR = sosclip(rR + p.feedback * bufR[w]);
			} else { nL = rL + fbL; nR = rR + fbR; }
			bufL[w] = (float)((1 - lockA) * nL + lockA * bufL[w]);
			bufR[w] = (float)((1 - lockA) * nR + lockA * bufR[w]);
			if (++w >= Bn) w = 0;
		}
		// STRETCH playhead
		if (p.scanMode == SCAN_STRETCH) play = wrap(play + stretchSpeed(p.scan));
		// grain grid
		const bool sync = p.sync;
		const int div = sync ? rateDivIndex(p.rateKnob) : -1;
		double gpos;
		if (sync) gpos = clk.phase / RATE_DIVS[div];
		else gpos = gridPrev + rateFreeHz(p.rateKnob) / fs;
		if (sync != gridMode || div != gridDiv || needResync) {
			gridMode = sync; gridDiv = div;
			if (needResync && clk.resetNow) { nextK = 0; }
			else nextK = (long long)std::floor(gpos) + 1;
			if (!sync && needResync && clk.resetNow) gpos = 0;
			nextTarget = (double)nextK;
			needResync = false;
		}
		gridPrev = gpos;
		if (std::floor(clk.phase) > std::floor(lastBeat)) beat = true;
		lastBeat = clk.phase;
		if (p.run && gpos >= nextTarget) {
			spawn(clk.bps);
			nextK = std::max(nextK + 1, (long long)std::floor(gpos) + 1);
			const double rateHz = sync ? clk.bps / RATE_DIVS[div] : rateFreeHz(p.rateKnob);
			double jit = rng.bipolar() * 0.5 * p.randRate;
			if (ESSPHOR_AGE && age > 0) jit += rng.normal() * 0.004 * age * rateHz;
			jit = std::max(-0.95, std::min(0.95, jit));
			nextTarget = nextK + jit;
		} else if (!p.run && gpos >= nextTarget) {
			nextK = (long long)std::floor(gpos) + 1; nextTarget = nextK;
		}
		// render
		double sL = 0, sR = 0; int n = 0;
		for (int i = 0; i < MAX_GRAINS; i++) {
			Grain& gr = g[i];
			if (!gr.on) continue;
			n++;
			const double ph = gr.age / gr.len;
			const double u = ph < gr.peak ? ph / gr.peak : (1 - ph) / (1 - gr.peak);
			const double win = (1 - gr.lin) * (0.5 - 0.5 * std::cos(PI * u)) + gr.lin * u;
			const double pos = gr.pos + gr.inc * gr.age;
			sL += readHermite(bufL, pos) * win * gr.gL;
			sR += readHermite(bufR, pos) * win * gr.gR;
			gr.age += 1.0;
			if (gr.age >= gr.len) gr.on = false;
		}
		active = n;
		wetL = sL; wetR = sR;
		double oL, oR;
		if (!p.send) {
			const double cw = std::cos(p.wet * PI / 2), sw = std::sin(p.wet * PI / 2);
			oL = inL * cw + sL * sw; oR = inR * cw + sR * sw;
		} else { oL = inL + sL * p.wet; oR = inR + sR * p.wet; }
		outL = softlimit(oL); outR = softlimit(oR);
	}
};

}  // namespace essphor
