// Rider's host: follows the band (link) or a clock cable, plans each bar just ahead of time with the engine,
// fires the strokes on the kit, and writes the outputs. Sample-driven. No Rack includes.
#pragma once
#include "RiderEngine.hpp"
#include "RiderKit.hpp"
#include "Link.hpp"
#include <vector>
#include <string>

namespace rider {

enum RSource { RS_IDLE, RS_LINK, RS_CABLE };
static const char* const RSOURCE_NAMES[3] = {"-", "LINK", "CLOCK"};
enum TrigGroup { TG_KICK, TG_SNARE, TG_HAT, TG_RIDE, TG_CRASH, TG_TOM, NUM_TG };
// which trigger a player voice fires (-1: none)
static inline int trigGroup(int voice) {
	switch (voice) {
		case KICK: return TG_KICK;
		case SNARE: case STICK: case BR_TAP: return TG_SNARE;
		case HATF: case HATC: case HATO: case BR_HATF: return TG_HAT;
		case RIDE: case BELL: return TG_RIDE;
		case CRASH: return TG_CRASH;
		case TOM_HI: case TOM_LO: return TG_TOM;
		default: return -1;
	}
}
enum DispEvent { DE_NONE, DE_FILL, DE_BIGFILL, DE_CAUGHT, DE_BOMB };
static const char* const DISP_EVENT_NAMES[5] = {"", "FILL", "BIG FILL", "CAUGHT", "BOMB"};

struct RIn {
	CompLink link;                 // the merged message (sections valid by their flags)
	bool clockConnected; float clock; int ppb; float reset;
	float fill; float layout;
	int feelPos;                   // 0 FOLLOW, 1..5 SWING..HOUSE
	RP rp;                         // swing is ignored (it comes from the link)
	int seed;
	int chorusBars;                // the menu's chorus length (without a chart)
	riderkit::KitParams kit;
	RIn() : clockConnected(false), clock(0.f), ppb(1), reset(0.f), fill(0.f), layout(0.f), feelPos(0), seed(1), chorusBars(12) {}
};
struct ROut {
	float L, R;
	float trig[NUM_TG];
	float vel[NUM_TG];
	float energy;
	ROut() : L(0.f), R(0.f), energy(0.f) { for (int i = 0; i < NUM_TG; i++) { trig[i] = 0.f; vel[i] = 0.f; } }
};

struct RiderHost {
	double sr = 48000;
	Rider eng;
	riderkit::Kit kit;
	int source = RS_IDLE;
	// transport
	double phi = 0.0, spb = 0.5; bool running = false;
	uint32_t lastResetCount = 0; bool haveResetCount = false;
	bool clkHigh = false, rstHigh = false, waitingFirstPulse = true;
	int pulses = 0; double lastPulseT = 0, lastPulsePhi = 0; double periods[3] = {0.5, 0.5, 0.5}; int nPeriods = 0;
	double t = 0.0;
	long long sampleNow = 0;
	// planning
	int nextBar = 0; int seedNow = 1; bool seeded = false;
	int pendingFill = 0; bool fillHigh = false;
	int feelNow = SWING; int chorusNow = 12;
	struct QS { double onset, offs; int voice, vel; double dur; int bar; double beat; int idx; };
	std::vector<QS> queue;
	int strokeIdx = 0;
	// kit clock (48 kHz) and resampling
	double acc = 0.0; double hist[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
	// outputs
	int trigLeft[NUM_TG] = {0, 0, 0, 0, 0, 0};
	float velOut[NUM_TG] = {0, 0, 0, 0, 0, 0};
	// display
	int dispEvent = DE_NONE; int dispEventBar = -1; int fillBar = -1; double hitAt = -99;
	Stats lastStats;
	// for the tests
	struct Log { long long at; int bar; double beat; int voice, vel, idx; };
	bool logOn = false; std::vector<Log> log;

	RiderHost() { queue.reserve(256); }
	void setSampleRate(double s) { sr = s; }

	void doReset(const RIn& in) {
		seedNow = in.seed; seeded = true;
		eng.reset(seedNow); lastStats = Stats();
		nextBar = 0; queue.clear(); strokeIdx = 0; pendingFill = 0;
		dispEvent = DE_NONE; dispEventBar = -1; fillBar = -1;
	}

	int wantFeel(const RIn& in) const {
		if (in.feelPos > 0) return std::min(4, in.feelPos - 1);
		if (in.link.hasComp) return std::max(0, std::min(4, in.link.groove));
		return SWING;
	}
	int wantChorus(const RIn& in) const {
		if (in.link.hasComp && !in.link.external)
			return std::max(1, (int)std::lround(std::max(1, in.link.steps) * std::max(1, in.link.beatsPerStep) / 4.0));
		return std::max(1, in.chorusBars);
	}

	// ---------------------------------------------------------------- planning
	void plan(const RIn& in) {
		for (int guard = 0; guard < 4; guard++) {
			if (phi < nextBar * 4.0 - 0.08) break;
			const CompLink& L = in.link;
			if (L.hasComp && !L.external && !(L.planBar >= nextBar || phi >= nextBar * 4.0 + 0.02)) break;
			planOne(in);
		}
	}
	void planOne(const RIn& in) {
		if (!seeded || in.seed != seedNow) { seedNow = in.seed; seeded = true; eng.reset(seedNow); lastStats = Stats(); }
		const CompLink& L = in.link;
		RP p = in.rp;
		p.feel = wantFeel(in); feelNow = p.feel;
		p.swing = L.hasComp ? (double)L.swing : 0.0;
		BarIn bi;
		if (L.hasComp && !L.external && L.planBar == nextBar) {
			bi.hasPiano = true; bi.slots = (L.planSlots == 16) ? 16 : 8;
			for (int s = 0; s < 16; s++) { bi.kind[s] = L.planKind[s]; bi.slotBeat[s] = L.planBeat[s]; bi.push[s] = L.planPush[s]; }
		}
		if (L.hasBass) {
			double a = nextBar * 4.0;
			for (int i = 0; i < std::min(16, L.bassN); i++)
				if (L.bassBeat[i] >= a && L.bassBeat[i] < a + 4.0 && bi.nbass < 16) bi.bassOff[bi.nbass++] = L.bassBeat[i] - a;
		}
		bi.forceFill = pendingFill; pendingFill = 0;
		chorusNow = wantChorus(in);
		double bpm = 60.0 / spb;
		const std::vector<Stroke>& ev = eng.planBar(nextBar, bpm, chorusNow, p, bi);
		for (const Stroke& s : ev) {
			QS q; q.onset = nextBar * 4.0 + s.beat; q.offs = s.offs; q.voice = s.voice; q.vel = s.vel; q.dur = s.dur;
			q.bar = nextBar; q.beat = s.beat; q.idx = strokeIdx++;
			queue.push_back(q);
		}
		// what to show
		const Stats& st = eng.stats;
		int de = DE_NONE;
		if (eng.lastFill == 2) de = DE_BIGFILL;
		else if (eng.lastFill == 1) de = DE_FILL;
		else if (st.caught > lastStats.caught) de = DE_CAUGHT;
		else if (st.bombs > lastStats.bombs) de = DE_BOMB;
		if (de != DE_NONE) { dispEvent = de; dispEventBar = nextBar; }
		if (eng.lastFill) fillBar = nextBar;
		lastStats = st;
		nextBar++;
	}

	// ---------------------------------------------------------------- firing
	void fire(const RIn& in) {
		bool lay = in.layout > 1.f;
		size_t w = 0;
		for (size_t i = 0; i < queue.size(); i++) {
			const QS& q = queue[i];
			double until = (q.onset - phi) * spb + q.offs;
			if (until > 0) { queue[w++] = q; continue; }
			if (lay) continue;
			if (q.voice == BR_SWEEP) kit.sweepNow(q.dur * spb, q.vel);
			else kit.strokeNow(q.voice, q.vel);
			int g = trigGroup(q.voice);
			if (g >= 0) { trigLeft[g] = std::max(1, (int)std::lround(0.001 * sr)); velOut[g] = 10.f * q.vel / 127.f; }
			hitAt = t;
			if (logOn) log.push_back(Log{sampleNow, q.bar, q.beat, q.voice, q.vel, q.idx});
		}
		queue.resize(w);
	}

	// ---------------------------------------------------------------- cable clock (as Walker's)
	void cableTransport(const RIn& in) {
		bool rst = in.reset > (rstHigh ? 0.1f : 1.0f); bool rstEdge = rst && !rstHigh; rstHigh = rst;
		if (rstEdge) { doReset(in); phi = 0; waitingFirstPulse = true; }
		bool clk = in.clock > (clkHigh ? 0.1f : 1.0f); bool clkEdge = clk && !clkHigh; clkHigh = clk;
		int ppb = std::max(1, in.ppb);
		if (clkEdge) {
			if (pulses > 0) {
				double per = t - lastPulseT; periods[nPeriods % 3] = per; nPeriods++;
				double m;
				if (nPeriods >= 3) { double a = periods[0], b = periods[1], c = periods[2]; m = std::max(std::min(a, b), std::min(std::max(a, b), c)); }
				else m = per;
				spb = std::max(0.05, std::min(4.0, m * ppb));
			}
			pulses++; lastPulseT = t;
			if (waitingFirstPulse) { phi = 0.0; waitingFirstPulse = false; }
			else phi = std::round(phi * ppb) / ppb;
			lastPulsePhi = phi;
		}
		if (pulses < 2) spb = 0.5;
		running = !waitingFirstPulse && pulses >= 1;
	}

	// ---------------------------------------------------------------- one sample
	void process(const RIn& in, ROut& out, double dt) {
		int src = in.link.hasComp ? RS_LINK : in.clockConnected ? RS_CABLE : RS_IDLE;
		if (src != source) {
			doReset(in); haveResetCount = false;
			if (src == RS_CABLE) { pulses = 0; nPeriods = 0; waitingFirstPulse = true; phi = 0; clkHigh = false; }
			if (src == RS_IDLE) running = false;
		}
		source = src;
		// FILL input: a set-up in the next bar planned
		bool f = in.fill > (fillHigh ? 0.1f : 1.0f);
		if (f && !fillHigh) pendingFill = in.rp.energy >= 0.5 ? 2 : 1;
		fillHigh = f;

		if (src == RS_LINK) {
			const CompLink& L = in.link;
			if (!haveResetCount || L.resetCount != lastResetCount) { lastResetCount = L.resetCount; haveResetCount = true; doReset(in); }
			phi = L.phi; spb = std::max(0.05, L.spb); running = L.running;
			if (running) { plan(in); fire(in); }
		} else if (src == RS_CABLE) {
			cableTransport(in);
			if (running) { plan(in); fire(in); }
			if (running) { phi += 1.0 / (sr * spb); phi = std::min(phi, lastPulsePhi + 1.5 / std::max(1, in.ppb)); }
		}

		// the kit: always 48 kHz
		kit.kp = in.kit;
		double l, r;
		if (sr == 48000.0) {
			kit.process(l, r);
		} else {
			acc += 48000.0 * dt;
			while (acc >= 1.0) {
				acc -= 1.0;
				double a, b; kit.process(a, b);
				for (int k = 0; k < 3; k++) { hist[0][k] = hist[0][k + 1]; hist[1][k] = hist[1][k + 1]; }
				hist[0][3] = a; hist[1][3] = b;
			}
			double x = acc, o[2];
			for (int c = 0; c < 2; c++) {
				const double* y = hist[c];
				double c0 = y[1], c1 = 0.5 * (y[2] - y[0]), c2 = y[0] - 2.5 * y[1] + 2.0 * y[2] - 0.5 * y[3], c3 = 0.5 * (y[3] - y[0]) + 1.5 * (y[1] - y[2]);
				o[c] = ((c3 * x + c2) * x + c1) * x + c0;
			}
			l = o[0]; r = o[1];
		}
		double g = 18.0 * in.kit.level;
		out.L = (float)(l * g); out.R = (float)(r * g);
		for (int i = 0; i < NUM_TG; i++) {
			out.trig[i] = trigLeft[i] > 0 ? 10.f : 0.f;
			if (trigLeft[i] > 0) trigLeft[i]--;
			out.vel[i] = velOut[i];
		}
		out.energy = (float)(10.0 * std::max(0.0, std::min(1.0, in.rp.energy)));
		t += dt; sampleNow++;
	}

	// ---------------------------------------------------------------- display
	int barNow() const { return std::max(0, (int)std::floor(phi / 4.0 + 1e-9)); }
	int barInChorus() const { return barNow() % std::max(1, chorusNow) + 1; }
	int eventNow() const { return (dispEventBar >= 0 && barNow() <= dispEventBar) ? dispEvent : DE_NONE; }
	bool fillNow() const { return fillBar >= 0 && barNow() == fillBar && running; }
};

} // namespace rider
