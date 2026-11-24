// Comper host: the real-time version of simulate() in tools/proto/comper_ref.py.
// Owns the clock, the chord source, the soloist follower, the note queue, long-note release,
// the pedal and voice allocation. Sample-driven, no Rack types.
#pragma once
#include "Engine.hpp"
#include <cstring>

namespace comper {

static const double HOST_LEAD_BEATS = 0.12;
static const double HOST_EXT_LATE_S = 0.004;
static const int MAX_CH = 16;

enum Source { SRC_CHART, SRC_CV, SRC_POLY };
static const char* const SOURCE_NAMES[3] = {"CHART", "CV", "POLY"};

struct HostIn {
	// chart
	int steps = 4, beatsPerStep = 4, key = 0;
	int stepRoot[16] = {2, 7, 0, 0}, stepQual[16] = {M7, DOM, MAJ, MAJ};
	bool keyConnected = false; float keyV = 0.f;
	// CV source
	bool rootConnected = false; float rootV = 0.f;
	bool qualConnected = false; float qualV = 0.f;
	// POLY source
	bool chordConnected = false; int chordChannels = 0; float chordV[16] = {};
	bool chordGateConnected = false; int chordGateChannels = 0; float chordGate[16] = {};
	// clock and transport
	double tempo = 120.0;
	bool clockConnected = false; float clock = 0.f; int ppb = 1;
	float reset = 0.f;
	bool runConnected = false; float runGate = 0.f; bool runButton = true;
	// soloist
	bool soloConnected = false; float solo = 0.f;
	bool soloPitchConnected = false; float soloPitch = 0.f;
	// player
	Params p; int seed = 1;
	int outChannels = 12; bool bassEveryBar = false; bool resetReplays = true;
};

struct HostOut {
	int channels = 12;
	float voct[MAX_CH] = {}, gate[MAX_CH] = {}, vel[MAX_CH] = {};
	float pedal = 0.f;
	float bassV = 0.f, bassGate = 0.f;
	float change = 0.f, eoc = 0.f;
	int scaleN = 0; float scale[8] = {};
};

struct Host {
	Engine eng;
	int engSeed = -1;
	double t = 0.0;            // seconds since construction
	double phi = 0.0;          // beats since reset
	double spb = 0.5;
	bool running = true;
	bool waitingFirstPulse = false;

	// clock input
	bool clkHigh = false, rstHigh = false;
	int pulses = 0; double lastPulseT = 0.0, lastPulsePhi = 0.0;
	double periods[3] = {0.5, 0.5, 0.5}; int nPeriods = 0;

	// decision pointer
	int nextBar = 0, nextSlot = 0;
	bool barBegun = false;     // nextBar has had beginBar
	bool anyDecided = false; int lastBar = 0, lastSlot = 0;
	double lastHitT = -99.0;

	// chord source
	int source = SRC_CHART;
	bool haveExt = false; Chord extChord; Chord extCand; double extCandSince = 0.0; bool haveCand = false;
	Chord srcChord; bool haveSrc = false;
	long long lastStepAbs = -999999; long long lastBarAbs = -999999;
	double changeUntil = -1.0, eocUntil = -1.0, bassLowUntil = -1.0;

	// soloist
	double soloAbove = -99.0; bool soloSnd = false; double soloEnd = -99.0; bool soloEver = false;
	double soloRises[8] = {-99, -99, -99, -99, -99, -99, -99, -99}; int soloRiseIdx = 0;
	struct SoloNote { double on, off; int pitch; bool open; };
	SoloNote soloNotes[16]; int soloNoteIdx = 0;
	double soloSampleAt = -1.0; double soloCurPitchV = 0.0; bool soloNoteOpen = false;

	// notes
	struct Event { double at; int type; int id; int pitch; int vel; double value; };   // type 0 off, 1 on, 2 pedal
	std::vector<Event> queue;
	struct Held { double nominal, cap, on, extra; int id; };
	std::vector<Held> held;
	int nextId = 1;
	struct Chan { bool gate = false; double lowSince = -1.0; int owner = 0; double riseAt = -1.0; int pitch = 60, vel = 64; double onT = 0.0; float voct = 0.f, velV = 0.f; };
	Chan ch[MAX_CH];
	double pedalTarget = 0.0, pedalOut = 0.0;

	// for the display and lights
	Notes lastVoicing; Chord lastChord; bool haveLast = false;
	double hitAt = -99.0;
	int stats_steals = 0, stats_retrig = 0;

	Host() { queue.reserve(256); held.reserve(64); }

	// ---------------------------------------------------------------- chord source
	Chord chartAt(const HostIn& in, double beat) const {
		int steps = std::max(1, std::min(16, in.steps));
		double total = (double)steps * in.beatsPerStep;
		double b = std::fmod(beat, total); if (b < 0) b += total;
		int s = (int)std::floor((b + 1e-9) / in.beatsPerStep);
		if (s >= steps) s = steps - 1;
		return Chord(pmod(in.stepRoot[s] + keyOffset(in), 12), in.stepQual[s]);
	}
	int keyOffset(const HostIn& in) const {
		int k = in.key;
		if (in.keyConnected) k += (int)std::round(in.keyV * 12.f);
		return k;
	}
	bool chartNext(const HostIn& in, double beat, Chord& out) const {
		int steps = std::max(1, std::min(16, in.steps));
		double total = (double)steps * in.beatsPerStep;
		double b = std::fmod(beat, total); if (b < 0) b += total;
		int s = (int)std::floor((b + 1e-9) / in.beatsPerStep);
		if (s >= steps) s = steps - 1;
		Chord cur = chartAt(in, beat);
		for (int i = 1; i < steps; i++) {
			int k = (s + i) % steps;
			Chord c(pmod(in.stepRoot[k] + keyOffset(in), 12), in.stepQual[k]);
			if (c != cur) { out = c; return true; }
		}
		return false;
	}
	int stepAt(const HostIn& in, double beat) const {
		int steps = std::max(1, std::min(16, in.steps));
		double total = (double)steps * in.beatsPerStep;
		double b = std::fmod(beat, total); if (b < 0) b += total;
		return std::min(steps - 1, (int)std::floor((b + 1e-9) / in.beatsPerStep));
	}

	// ---------------------------------------------------------------- transport
	void allOff() {
		for (int c = 0; c < MAX_CH; c++) {
			if (ch[c].gate || ch[c].riseAt >= 0) { ch[c].gate = false; ch[c].lowSince = t; }
			ch[c].riseAt = -1.0; ch[c].owner = 0;
		}
		queue.clear(); held.clear();
		pedalTarget = 0.0;
	}
	uint32_t resetCount = 0;   // for the band (the link): goes up at every reset
	int planBar = -1;          // for the band: the last bar begun, whose figure is eng.cellmap / slotBeats / pushOk
	void doReset(const HostIn& in, double startBeat = 0.0) {
		resetCount++; planBar = -1;
		phi = startBeat;
		nextBar = 0; nextSlot = 0; barBegun = false; anyDecided = false;
		lastHitT = -99.0;
		allOff();
		if (in.resetReplays || engSeed < 0) { eng.reset(in.seed); engSeed = in.seed; }
		waitingFirstPulse = in.clockConnected;
		lastStepAbs = -999999; lastBarAbs = -999999;
	}

	// ---------------------------------------------------------------- notes
	void schedule(double at, int type, int id, int pitch, int vel, double value = 0.0) {
		Event e{at, type, id, pitch, vel, value};
		auto it = queue.end();
		while (it != queue.begin()) {
			auto pr = it - 1;
			if (pr->at < at || (pr->at == at && pr->type <= type)) break;
			it = pr;
		}
		queue.insert(it, e);
	}
	void noteOn(const HostIn& in, int id, int pitch, int vel) {
		int nch = std::max(1, std::min(MAX_CH, in.outChannels));
		for (int c = 0; c < nch; c++) {
			if ((ch[c].gate || ch[c].riseAt >= 0) && ch[c].pitch == pitch && ch[c].owner != 0) {
				ch[c].gate = false; ch[c].lowSince = t; ch[c].riseAt = t + 0.002;
				ch[c].owner = id; ch[c].pitch = pitch; ch[c].vel = vel; stats_retrig++;
				return;
			}
		}
		int best = -1;
		for (int c = 0; c < nch; c++) {
			if (ch[c].gate || ch[c].riseAt >= 0) continue;
			if (t - ch[c].lowSince < 0.001 - 1e-9) continue;
			if (best < 0 || ch[c].lowSince < ch[best].lowSince) best = c;
		}
		if (best >= 0) {
			Chan& k = ch[best];
			k.gate = true; k.owner = id; k.pitch = pitch; k.vel = vel; k.onT = t; k.riseAt = -1.0;
			k.voct = (float)((pitch - 60) / 12.0); k.velV = (float)(vel / 127.0 * 10.0);
			return;
		}
		int old = -1;
		for (int c = 0; c < nch; c++) {
			if (ch[c].riseAt >= 0) continue;
			if (old < 0 || ch[c].onT < ch[old].onT) old = c;
		}
		if (old < 0) old = 0;
		Chan& k = ch[old];
		k.gate = false; k.lowSince = t; k.riseAt = t + 0.002; k.owner = id; k.pitch = pitch; k.vel = vel;
		stats_steals++;
	}
	void noteOff(int id) {
		for (int c = 0; c < MAX_CH; c++) {
			if (ch[c].owner == id) {
				if (ch[c].riseAt >= 0) ch[c].riseAt = -1.0;   // never rose: cancel
				if (ch[c].gate) { ch[c].gate = false; ch[c].lowSince = t; }
				ch[c].owner = 0;
			}
		}
	}
	void release(double now, double tNext) {
		for (auto& h : held) {
			double off = std::min(h.nominal + h.cap, std::max(h.nominal + 0.9 * spb, tNext - HOST_LEAD_BEATS * spb));
			off = std::max(off, std::min(now, h.nominal + h.cap)) + h.extra;
			off = std::max(off, h.on + 0.02);
			schedule(off, 0, h.id, 0, 0);
		}
		held.clear();
	}
	void releaseNow() {
		for (auto& h : held) schedule(std::max(t, h.on + 0.02), 0, h.id, 0, 0);
		held.clear();
	}
	void take(const Result& res, double nominal) {
		for (auto& e : res.notes) {
			double on = std::max(t, nominal + e.t);
			int id = nextId++;
			schedule(on, 1, id, e.pitch, e.vel);
			if (e.lng) held.push_back({nominal, eng.capBeats() * spb, on, e.extra, id});
			else schedule(on + e.dur + e.extra, 0, id, 0, 0);
		}
		for (auto& pv : res.pedal) schedule(std::max(t, nominal + pv.first), 2, 0, 0, 0, pv.second);
		if (res.hit || !res.notes.empty()) {
			if (res.hit) {
				hitAt = t;
				lastVoicing.clear();
				for (auto& e : res.notes) lastVoicing.push_back(e.pitch);
				std::sort(lastVoicing.begin(), lastVoicing.end());
				lastChord = res.chord; haveLast = true;
			}
		}
	}

	// ---------------------------------------------------------------- soloist
	void soloState(const HostIn& in, Ctx& ctx) {
		ctx.soloConnected = in.soloConnected;
		if (!in.soloConnected) return;
		ctx.soloSounding = soloSnd;
		ctx.soloGap = (soloEver && !soloSnd) ? (t - soloEnd) / spb : (soloEver ? 0.0 : 99.0);
		if (!soloEver) ctx.soloGap = 99.0;
		bool rec = false;
		for (double r : soloRises) if (t - 2.5 * spb < r && r <= t) rec = true;
		ctx.soloRecent = rec;
		ctx.hasSoloLow = false;
		if (in.soloPitchConnected) {
			int low = 999;
			for (auto& n : soloNotes) {
				if (n.on > t || n.on < -50) continue;
				if (n.open || t - 2.0 * spb < n.off) low = std::min(low, n.pitch);
			}
			if (low < 999) { ctx.hasSoloLow = true; ctx.soloLow = low; }
		}
	}
	void soloProcess(const HostIn& in) {
		if (!in.soloConnected) { soloSnd = false; return; }
		if (std::fabs(in.solo) > 0.5f) soloAbove = t;
		bool snd = t - soloAbove < 0.030;
		if (snd && !soloSnd) {
			soloRises[soloRiseIdx] = t; soloRiseIdx = (soloRiseIdx + 1) % 8;
			soloSampleAt = t + 0.002; soloEver = true;
		}
		if (!snd && soloSnd) {
			soloEnd = t;
			if (soloNoteOpen) { soloNotes[(soloNoteIdx + 15) % 16].off = t; soloNotes[(soloNoteIdx + 15) % 16].open = false; soloNoteOpen = false; }
		}
		soloSnd = snd;
		if (in.soloPitchConnected && snd) {
			bool start = soloSampleAt >= 0 && t >= soloSampleAt;
			if (!start && soloNoteOpen && std::fabs(in.soloPitch - soloCurPitchV) * 12.0 > 0.5) {
				soloNotes[(soloNoteIdx + 15) % 16].off = t; soloNotes[(soloNoteIdx + 15) % 16].open = false;
				start = true;
			}
			if (start) {
				soloSampleAt = -1.0; soloCurPitchV = in.soloPitch;
				SoloNote& n = soloNotes[soloNoteIdx]; soloNoteIdx = (soloNoteIdx + 1) % 16;
				n.on = t; n.off = t; n.open = true; n.pitch = (int)std::round(in.soloPitch * 12.0) + 60;
				soloNoteOpen = true;
			}
		}
	}

	// ---------------------------------------------------------------- external chords
	bool readExternal(const HostIn& in, Chord& out) {
		if (source == SRC_CV) {
			int r = pmod((int)std::round(in.rootV * 12.f) + keyOffset(in), 12);
			int q = in.qualConnected ? std::max(0, std::min(9, (int)std::floor(in.qualV))) : in.stepQual[0];
			out = Chord(r, q); return true;
		}
		std::vector<int> ns;
		int n = in.chordChannels;
		for (int c = 0; c < n; c++) {
			bool on = !in.chordGateConnected || (c < in.chordGateChannels ? in.chordGate[c] > 1.f : in.chordGate[0] > 1.f);
			if (on) ns.push_back((int)std::round(in.chordV[c] * 12.f) + 60);
		}
		if (ns.empty()) return false;
		return detectChord(ns, haveExt ? extChord.q : DOM, out);
	}

	void params(const HostIn& in, bool withGroove) {
		int g = eng.p.groove;
		eng.p = in.p;
		if (!withGroove) eng.p.groove = g;
	}

	double leadBeats() const {
		return source == SRC_CHART ? HOST_LEAD_BEATS : -HOST_EXT_LATE_S / spb;
	}
	double slotBeat(int bar, int slot) const { return bar * 4 + eng.slotBeats[slot]; }

	void fillCtx(const HostIn& in, Ctx& ctx) {
		ctx.spb = spb;
		if (source == SRC_CHART) {
			ctx.leadS = HOST_LEAD_BEATS * spb;
			const HostIn* pin = &in;
			ctx.chordAt = [this, pin](double b) { return chartAt(*pin, b); };
			ctx.nextChange = [this, pin](double b, Chord& o) { return chartNext(*pin, b, o); };
		} else {
			ctx.leadS = -HOST_EXT_LATE_S;
			Chord cur = extChord;
			ctx.chordAt = [cur](double) { return cur; };
			ctx.nextChange = nullptr;
		}
		soloState(in, ctx);
	}

	// ---------------------------------------------------------------- one sample
	void process(const HostIn& in, HostOut& out, double dt, double startBeatOnReset = 0.0) {
		// source
		int src = (in.chordConnected) ? SRC_POLY : in.rootConnected ? SRC_CV : SRC_CHART;
		if (src != source) { source = src; haveExt = false; haveCand = false; }
		if (engSeed < 0) doReset(in, startBeatOnReset);

		// transport
		bool rst = in.reset > (rstHigh ? 0.1f : 1.0f);
		bool rstEdge = rst && !rstHigh; rstHigh = rst;
		bool runNow = in.runConnected ? in.runGate > 1.f : in.runButton;
		if (rstEdge) doReset(in, startBeatOnReset);
		if (!runNow && running) { allOff(); }
		running = runNow;

		// clock
		bool clk = in.clock > (clkHigh ? 0.1f : 1.0f);
		bool clkEdge = clk && !clkHigh; clkHigh = clk;
		int ppb = std::max(1, in.ppb);
		if (in.clockConnected) {
			if (clkEdge) {
				if (pulses > 0) {
					double per = t - lastPulseT;
					periods[nPeriods % 3] = per; nPeriods++;
					double m;
					if (nPeriods >= 3) {
						double a = periods[0], b = periods[1], c = periods[2];
						m = std::max(std::min(a, b), std::min(std::max(a, b), c));
					} else m = per;
					spb = std::max(0.05, std::min(4.0, m * ppb));
				}
				pulses++; lastPulseT = t;
				if (waitingFirstPulse) { phi = 0.0; waitingFirstPulse = false; }
				else phi = std::round(phi * ppb) / ppb;
				lastPulsePhi = phi;
			}
			if (pulses < 2) spb = 0.5;
		} else {
			spb = 60.0 / std::max(20.0, in.tempo);
			waitingFirstPulse = false; pulses = 0; nPeriods = 0;
		}

		soloProcess(in);

		// external chord
		if (source != SRC_CHART) {
			Chord c;
			if (readExternal(in, c)) {
				double need = source == SRC_POLY ? 0.003 : 0.002;
				if (!haveExt) { extChord = c; haveExt = true; haveCand = false; }
				else if (c != extChord) {
					if (!haveCand || c != extCand) { extCand = c; extCandSince = t; haveCand = true; }
					else if (t - extCandSince >= need - 1e-9) {
						extChord = c; haveCand = false;
						if (running && anyDecided) externalChange(in);
					}
				} else haveCand = false;
			} else if (!haveExt) { extChord = Chord(0, in.stepQual[0]); haveExt = true; }
		}

		if (running && !waitingFirstPulse) {
			// decisions
			for (int guard = 0; guard < 64; guard++) {
				double sb = barBegun ? slotBeat(nextBar, nextSlot) : nextBar * 4.0;
				if (phi < sb - leadBeats()) break;
				if (!barBegun) {
					if (in.seed != engSeed) { eng.reset(in.seed); engSeed = in.seed; }
					params(in, true);
					eng.beginBar(nextBar, in.clockConnected ? 60.0 / spb : in.tempo);
					barBegun = true; planBar = nextBar;
					sb = slotBeat(nextBar, nextSlot);
				} else params(in, false);
				Ctx ctx; fillCtx(in, ctx);
				double nominal = t + (sb - phi) * spb;
				double nowExact = nominal - ctx.leadS;
				Result res = eng.decide(nextBar, nextSlot, ctx);
				if (res.hit) { release(nowExact, nominal); lastHitT = t; }
				take(res, nominal);
				anyDecided = true; lastBar = nextBar; lastSlot = nextSlot;
				nextSlot++;
				if (nextSlot >= eng.slots) { nextSlot = 0; nextBar++; barBegun = false; }
			}
			// long notes that reach their cap with no next hit
			for (size_t i = 0; i < held.size();) {
				if (t >= held[i].nominal + held[i].cap + held[i].extra) {
					schedule(std::max(t, held[i].on + 0.02), 0, held[i].id, 0, 0);
					held.erase(held.begin() + i);
				} else i++;
			}
		}

		// events
		while (!queue.empty() && queue.front().at <= t + 1e-9) {
			Event e = queue.front(); queue.erase(queue.begin());
			if (e.type == 0) noteOff(e.id);
			else if (e.type == 1) noteOn(in, e.id, e.pitch, e.vel);
			else pedalTarget = e.value;
		}
		// delayed rises
		for (int c = 0; c < MAX_CH; c++) {
			Chan& k = ch[c];
			if (k.riseAt >= 0 && t >= k.riseAt - 1e-9) {
				k.riseAt = -1.0; k.gate = true; k.onT = t;
				k.voct = (float)((k.pitch - 60) / 12.0); k.velV = (float)(k.vel / 127.0 * 10.0);
			}
		}

		// chart position, bass, change, eoc
		Chord cur; bool haveCur = true;
		if (source == SRC_CHART) {
			cur = chartAt(in, std::max(0.0, phi));
			int steps = std::max(1, std::min(16, in.steps));
			long long stepAbs = (long long)std::floor((std::max(0.0, phi) + 1e-9) / in.beatsPerStep);
			if (stepAbs != lastStepAbs) {
				if (lastStepAbs != -999999 && running && stepAbs % steps == 0) eocUntil = t + 0.001;
				lastStepAbs = stepAbs;
			}
		} else if (haveExt) cur = extChord;
		else haveCur = false;
		long long barAbs = (long long)std::floor(std::max(0.0, phi) / 4.0 + 1e-9);
		if (haveCur) {
			if (!haveSrc || cur != srcChord) {
				if (haveSrc && running) { changeUntil = t + 0.001; bassLowUntil = t + 0.005; }
				srcChord = cur; haveSrc = true;
			}
		}
		if (barAbs != lastBarAbs) {
			if (lastBarAbs != -999999 && in.bassEveryBar && running) bassLowUntil = t + 0.005;
			lastBarAbs = barAbs;
		}

		// pedal
		double target = running ? pedalTarget : 0.0;
		double stepP = dt / 0.005;
		if (pedalOut < target) pedalOut = std::min(target, pedalOut + stepP);
		else pedalOut = std::max(target, pedalOut - stepP);

		// outputs
		int nch = std::max(1, std::min(MAX_CH, in.outChannels));
		out.channels = nch;
		for (int c = 0; c < MAX_CH; c++) {
			out.voct[c] = ch[c].voct; out.vel[c] = ch[c].velV; out.gate[c] = (ch[c].gate && running) ? 10.f : 0.f;
		}
		out.pedal = (float)(pedalOut * 10.0);
		if (haveSrc) {
			int m = 28 + pmod(srcChord.root - 4, 12);
			out.bassV = (float)((m - 60) / 12.0);
			out.scaleN = SCALE_LEN[srcChord.q];
			for (int i = 0; i < out.scaleN; i++) out.scale[i] = (float)((srcChord.root + SCALES[srcChord.q][i]) / 12.0);
		}
		out.bassGate = (running && t >= bassLowUntil) ? 10.f : 0.f;
		out.change = t < changeUntil ? 10.f : 0.f;
		out.eoc = t < eocUntil ? 10.f : 0.f;

		// time
		t += dt;
		if (running && !waitingFirstPulse) {
			phi += dt / spb;
			if (in.clockConnected && pulses >= 1) phi = std::min(phi, lastPulsePhi + 1.5 / ppb);
		}
	}

	void externalChange(const HostIn& in) {
		// the chord changed under us: let go now, and maybe catch the new chord straight away
		releaseNow();
		double u = eng.rr.u();
		if (!eng.begun) return;
		bool cellSoon = false;
		if (barBegun) {
			double sb = slotBeat(nextBar, nextSlot);
			cellSoon = (sb - phi) < 0.15 && eng.cellmap[nextSlot] >= 0;
		} else cellSoon = (nextBar * 4.0 - phi) < 0.15;
		bool recent = t - lastHitT <= 0.15 * spb;
		if (!cellSoon && !recent && u < 0.5 + 0.5 * eng.p.density) {
			params(in, false);
			Ctx ctx; fillCtx(in, ctx); ctx.leadS = 0.0;
			Result res = eng.strike(lastBar, lastSlot, L_, -4.0, ctx, true);
			lastHitT = t;
			take(res, t);
		}
	}
};

} // namespace comper
