// Walker's host: follows Comper (link) or cables, plans the line just ahead of time with the engine,
// schedules the notes, plays them on the upright, and writes the outputs. Sample-driven. No Rack includes.
#pragma once
#include "WalkerEngine.hpp"
#include "Upright.hpp"
#include "Link.hpp"
#include <deque>

namespace walker {

enum WSource { WS_IDLE, WS_LINK, WS_CABLE, WS_LEARNING, WS_PLAY };
static const char* const WSOURCE_NAMES[5] = {"-", "LINK", "CABLE", "LEARNING", "PLAY"};
static const double EXT_LATE_S = 0.004;

struct WIn {
	bool linkOk = false; CompLink link;
	// cables
	bool clockConnected = false; float clock = 0.f; int ppb = 1; float reset = 0.f;
	bool rootConnected = false; float rootV = 0.f; bool qualConnected = false; float qualV = 0.f;
	bool chordConnected = false; int chordChannels = 0; float chordV[16] = {};
	float layout = 0.f;
	// played as a voice
	bool gateInConnected = false; float gateIn = 0.f; float voctIn = 0.f; bool velInConnected = false; float velIn = 0.f;
	// controls
	int feelPos = 0;               // 0 FOLLOW, 1..6 WALK..HOUSE
	BP bp; int seed = 1; upright::Tone tone; double level = 1.0;
	int tuneBars = 0;              // 0: from RESET
	bool voctSlides = true;
};
struct WOut { float audio = 0.f, voct = 0.f, gate = 0.f, vel = 0.f; };

struct WalkerHost {
	double sr = 48000;
	upright::Bass bass;
	Player pl;
	int source = WS_IDLE;
	// transport
	double phi = 0.0, spb = 0.5; bool running = false;
	uint32_t lastResetCount = 0; bool haveResetCount = false;
	// cable clock (as Comper's host)
	bool clkHigh = false, rstHigh = false, waitingFirstPulse = false;
	int pulses = 0; double lastPulseT = 0, lastPulsePhi = 0; double periods[3] = {0.5, 0.5, 0.5}; int nPeriods = 0;
	double t = 0.0;
	// planning
	bool started = false; int startedFeel = -1; double origin = 0.0;
	int seedNow = 1; int pendingSeed = -1;
	bool learnedMode = false;
	// the chart as the planner sees it (absolute beats)
	int steps = 4, bps = 4; int sroot[16] = {}, squal[16] = {};
	// learning (cable mode)
	Chord rec[256]; bool recHeard[256] = {}; int tuneLen = 0; double lastResetPhi = -1;
	Chord prevBeatChord; bool havePrevBeat = false; long long nextBlind = 0;
	// queue
	struct Meta { double onsetBeat; bool fired; int id; };
	std::deque<Note> qn; std::deque<Meta> qm;
	int nextId = 1; int noteIdx = 0;
	// gate / pitch outputs
	struct Fired { long long on, off; int pitch; double vel, cents, scoop, vib; int id; };
	std::deque<Fired> fired;
	// display
	int lastPitch = -1; Chord curChord; bool haveCur = false; double pluckAt = -99;
	// play mode
	bool gateInHigh = false; int playId = -1; int playString = -1; int hand = 2;
	int playVoiceIdx = 0;
	// for the tests
	struct FireLog { int id; long long at; int pitch; double vel, dur; bool stopped, dead; };
	bool logOn = false; std::vector<FireLog> log;

	WalkerHost() { pl.chordAt = [this](double b) { return chordAbs(b + origin); }; pl.reset(1); }
	void setSampleRate(double s) { sr = s; bass.setSampleRate(s); }

	Chord chordAbs(double beat) const {
		if (learnedMode && tuneLen > 0) {
			long long b = (long long)std::floor(beat + 1e-9); int i = (int)(((b % tuneLen) + tuneLen) % tuneLen);
			return rec[i];
		}
		long long st = (long long)std::floor((beat + 1e-9) / bps);
		int s = (int)(((st % steps) + steps) % steps);
		return Chord(sroot[s], squal[s]);
	}
	Chord chordNowExternal(const WIn& in) {
		if (in.linkOk && in.link.external) return Chord(pmod(in.link.curRoot, 12), std::max(0, std::min(9, in.link.curQual)));
		Chord c(0, comper::SUS);
		bool have = false;
		if (in.chordConnected && in.chordChannels > 0) {
			std::vector<int> ns;
			for (int i = 0; i < in.chordChannels; i++) ns.push_back((int)std::round(in.chordV[i] * 12.f) + 60);
			Chord d; if (comper::detectChord(ns, havePrevBeat ? prevBeatChord.q : comper::DOM, d)) { c = d; have = true; }
		}
		if (in.rootConnected) {
			c.root = pmod((int)std::round(in.rootV * 12.f), 12);
			if (in.qualConnected) c.q = std::max(0, std::min(9, (int)std::floor(in.qualV)));
			else if (!have) c.q = comper::SUS;
		} else if (in.qualConnected && !have) c.q = std::max(0, std::min(9, (int)std::floor(in.qualV)));
		return c;
	}
	int wantFeel(const WIn& in) const {
		if (in.feelPos > 0) return std::min(5, in.feelPos - 1);
		if (source == WS_LINK) { static const int M[5] = {WALK, BOSSA, BALLAD, SOUL, HOUSE}; return M[std::max(0, std::min(4, in.link.groove))]; }
		return WALK;
	}
	// for the band (the link): planned notes not yet played that fall off the beat, as beats on the clock of phi
	int offBeats(double* out, int maxN) const {
		int n = 0;
		for (size_t i = 0; i < qn.size() && i < qm.size() && n < maxN; i++) {
			if (qm[i].fired) continue;
			double b = origin + qn[i].beat;
			double fr = b - std::floor(b);
			if (fr > 0.05 && b >= phi) out[n++] = b;
		}
		return n;
	}
	// ---------------------------------------------------------------- reset
	void releaseAll() {
		for (auto& f : fired) f.off = std::min(f.off, bass.sampleNow);
		bass.allOff();
		qn.clear(); qm.clear();
	}
	void doReset(const WIn& in) {
		releaseAll();
		seedNow = in.seed; pendingSeed = -1;
		pl.reset(seedNow); started = false; startedFeel = -1; origin = 0; noteIdx = 0;
		havePrevBeat = false; nextBlind = 0;
		learnedMode = false;
	}
	// ---------------------------------------------------------------- planning
	void plan(const WIn& in) {
		double lead = std::max(0.5, 0.2 / spb);
		int guard = 0;
		while (guard++ < 16) {
			if (!started) {
				pl.p = in.bp; pl.p.feel = wantFeel(in); pl.bpm = 60.0 / spb;
				pl.startFeel(pl.p.feel); started = true; startedFeel = pl.p.feel;
			}
			double b0 = pl.nextStepBeat();
			if (phi < origin + b0 - lead) break;
			// bar line: new seed, or a new feel
			if (std::fmod(b0, 4.0) == 0.0) {
				int bar = (int)(b0 / 4);
				if (pendingSeed >= 0) {
					seedNow = pendingSeed; pendingSeed = -1;
					pl.reset(seedNow); pl.p = in.bp; pl.p.feel = wantFeel(in); pl.bpm = 60.0 / spb;
					pl.startFeelAt(pl.p.feel, bar, false); startedFeel = pl.p.feel;
				} else if (wantFeel(in) != startedFeel) {
					pl.p.feel = wantFeel(in); pl.startFeelAt(pl.p.feel, bar, true); startedFeel = pl.p.feel;
				}
			}
			int f = startedFeel; pl.p = in.bp; pl.p.feel = f; pl.bpm = 60.0 / spb;
			std::vector<Event> ev = pl.step();
			perform(ev);
		}
	}
	void perform(const std::vector<Event>& ev) {
		size_t before = qn.size();
		double spbE = 60.0 / pl.bpm;
		pl.perform(ev, qn, [this](Note* a) { onPrevChanged(a); });
		for (size_t i = before; i < qn.size(); i++) qm.push_back(Meta{origin + qn[i].t / spbE, false, nextId++});
	}
	void onPrevChanged(Note* a) {
		for (size_t i = 0; i < qn.size() && i < qm.size(); i++) if (&qn[i] == a) {
			if (qm[i].fired) {
				if (logOn) for (auto& l : log) if (l.id == qm[i].id) { l.dur = a->dur; l.stopped = a->stopped; }
				bass.release(qm[i].id, a->dur, a->stopped);
				for (auto& f : fired) if (f.id == qm[i].id) f.off = f.on + (long long)std::llround(a->dur * sr);
			}
			return;
		}
	}
	// ---------------------------------------------------------------- firing
	void fire(const WIn& in) {
		bool lay = in.layout > 1.f;
		for (size_t i = 0; i < qn.size(); i++) {
			if (qm[i].fired) continue;
			double until = (qm[i].onsetBeat - phi) * spb * sr;
			if (until >= 32) continue;
			qm[i].fired = true;
			long long at = bass.sampleNow + std::max(0LL, (long long)std::llround(until));
			const Note& n = qn[i];
			int idx = noteIdx++;
			if (lay) continue;
			upright::NoteSpec ns; ns.pitch = n.pitch; ns.vel = n.vel; ns.string = n.string; ns.fret = n.fret; ns.dead = n.dead;
			ns.scoop = n.scoop; ns.cents = n.cents; ns.vib = n.vib;
			bass.noteOn(ns, (int64_t)seedNow * 7919 + 31 * idx + 5, qm[i].id, at);
			bass.release(qm[i].id, n.dur, n.stopped);
			for (auto& f : fired) if (f.off > at - (long long)(0.001 * sr)) f.off = std::max(f.on, at - (long long)(0.001 * sr));
			fired.push_back(Fired{at, at + (long long)std::llround(n.dur * sr), n.pitch, n.vel, n.cents, n.scoop, n.vib, qm[i].id});
			if (fired.size() > 16) fired.pop_front();
			if (logOn) log.push_back(FireLog{qm[i].id, at, n.pitch, n.vel, n.dur, n.stopped, n.dead});
		}
		// drop notes that are fired, released, and no longer needed by the engine (it may still shorten the last one)
		while (qn.size() > 1 && qm.front().fired && &qn.front() != pl.last) { qn.pop_front(); qm.pop_front(); }
	}
	// ---------------------------------------------------------------- one sample
	void process(const WIn& in, WOut& out, double dt) {
		// which source is in charge
		int src = in.gateInConnected ? WS_PLAY : (in.linkOk && !in.link.external) ? WS_LINK
			: (in.clockConnected || (in.linkOk && in.link.external)) ? (learnedMode ? WS_CABLE : WS_LEARNING) : WS_IDLE;
		bool cableLike = src == WS_CABLE || src == WS_LEARNING;
		int srcClass = src == WS_LEARNING ? WS_CABLE : src, prevClass = source == WS_LEARNING ? WS_CABLE : source;
		if (srcClass != prevClass) { doReset(in); haveResetCount = false; if (srcClass == WS_CABLE) { pulses = 0; nPeriods = 0; waitingFirstPulse = true; phi = 0; tuneLen = 0; lastResetPhi = -1; for (auto& h : recHeard) h = false; } }
		source = src;
		bass.tone = in.tone;
		if (in.seed != seedNow && pendingSeed != in.seed) pendingSeed = in.seed;
		if (pendingSeed == seedNow) pendingSeed = -1;

		if (src == WS_LINK) {
			const CompLink& L = in.link;
			if (!haveResetCount || L.resetCount != lastResetCount) { lastResetCount = L.resetCount; haveResetCount = true; doReset(in); }
			phi = L.phi; spb = std::max(0.05, L.spb); running = L.running;
			steps = std::max(1, std::min(16, L.steps)); bps = std::max(1, L.beatsPerStep);
			for (int i = 0; i < 16; i++) { sroot[i] = pmod(L.root[i], 12); squal[i] = std::max(0, std::min(9, L.qual[i])); }
			BP bp = in.bp; (void)bp;
			if (running) { plan(withSwing(in)); fire(in); }
			curChord = chordAbs(phi); haveCur = true;
		} else if (cableLike) {
			cableTransport(in);
			if (running) cablePlan(in);
			if (running) fire(in);
		} else if (src == WS_PLAY) {
			playMode(in);
		}
		// audio
		out.audio = (float)(bass.process() * 0.5 * in.level);
		// notes out
		long long now = bass.sampleNow - 1;
		const Fired* cur = nullptr;
		for (auto& f : fired) if (f.on <= now) cur = &f;
		if (cur) {
			bool g = now < cur->off;
			out.gate = g ? 10.f : 0.f;
			double v = (cur->pitch - 60) / 12.0;
			if (in.voctSlides) {
				double ts = (now - cur->on) / sr;
				double c = cur->cents - cur->scoop * std::exp(-ts / 0.035);
				if (cur->vib > 0) c += cur->vib * std::min(1.0, std::max(0.0, (ts - 0.25) / 0.3)) * std::sin(2 * M_PI * 5.2 * ts);
				v += c / 1200.0;
			}
			out.voct = (float)v; out.vel = (float)(cur->vel * 10.0);
			if (now == cur->on) { lastPitch = cur->pitch; pluckAt = t; }
		} else { out.gate = 0.f; }
		t += dt;
	}
	WIn withSwing(const WIn& in) const { WIn w = in; w.bp.swing = in.link.swing; return w; }
	// ---------------------------------------------------------------- cables
	void cableTransport(const WIn& in) {
		if (in.linkOk && in.link.external) {
			const CompLink& L = in.link;
			if (!haveResetCount || L.resetCount != lastResetCount) {
				if (haveResetCount) noteResetLength(phi);
				lastResetCount = L.resetCount; haveResetCount = true; cableReset(in);
			}
			phi = L.phi; spb = std::max(0.05, L.spb); running = L.running;
			return;
		}
		bool rst = in.reset > (rstHigh ? 0.1f : 1.0f); bool rstEdge = rst && !rstHigh; rstHigh = rst;
		if (rstEdge) { noteResetLength(phi); cableReset(in); waitingFirstPulse = true; }
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
		if (running) { phi += 1.0 / (sr * spb); if (pulses >= 1) phi = std::min(phi, lastPulsePhi + 1.5 / ppb); }
	}
	void noteResetLength(double atPhi) {
		if (atPhi <= 0.5) return;
		int bars = (int)std::llround(atPhi / 4.0);
		if (bars >= 1 && bars * 4 <= 256) {
			int L = bars * 4; bool all = true;
			for (int i = 0; i < L; i++) all = all && recHeard[i];
			if (all) { lastResetPhi = atPhi; resetLenFromReset = L; }
		}
	}
	int resetLenFromReset = 0;
	void cableReset(const WIn& in) {
		doReset(in);
		phi = 0; nextBlind = 0;
		int L = in.tuneBars > 0 ? in.tuneBars * 4 : resetLenFromReset;
		if (in.tuneBars == 0 && L > 0) { tuneLen = L; learnedMode = true; }   // the pass just finished is the tune
		else { for (auto& h : recHeard) h = false; tuneLen = 0; }
	}
	void cablePlan(const WIn& in) {
		int L = in.tuneBars > 0 ? in.tuneBars * 4 : (learnedMode ? tuneLen : 0);
		// record the chord of each beat, a few ms after it arrives
		long long b = nextBlind;
		if (phi >= b + EXT_LATE_S / spb) {
			Chord c = chordNowExternal(in);
			curChord = c; haveCur = true;
			int slot = L > 0 ? (int)(b % L) : (int)(b % 256);
			rec[slot] = c; recHeard[slot] = true;
			bool changed = havePrevBeat && c != prevBeatChord;
			if (!learnedMode) {
				bool blindDone = false;
				// blind: one note, decided now (before a possible switch, which re-seeds the player)
				{ std::vector<Event> ev = {pl.blindBeat((int)b, c.root, c.q, b > 0 && changed)}; perform(ev); blindDone = true; }
				// fixed tune length: once every beat has been heard, switch to planning at the next pass
				if (L > 0) {
					bool all = true; for (int i = 0; i < L; i++) all = all && recHeard[i];
					if (all && (b + 1) % L == 0) {
						learnedMode = true; tuneLen = L; origin = (double)(b + 1);
						pl.reset(seedNow); started = false;
					}
				}
				if (!blindDone) {
					// blind: one note, decided now
					std::vector<Event> ev = {pl.blindBeat((int)b, c.root, c.q, b > 0 && changed)};
					perform(ev);
				}
			}
			prevBeatChord = c; havePrevBeat = true;
			nextBlind = b + 1;
		}
		if (learnedMode) { if (source == WS_LEARNING) source = WS_CABLE; plan(in); }
	}
	// ---------------------------------------------------------------- played as a voice
	void playMode(const WIn& in) {
		bool g = in.gateIn > (gateInHigh ? 0.1f : 1.0f);
		if (g && !gateInHigh) {
			double m = 60.0 + 12.0 * in.voctIn; m = std::max(28.0, std::min(67.0, m));
			int pitch = (int)std::lround(m); double cents = (m - pitch) * 100.0;
			int s, f; noteString(pitch, hand, s, f);
			if (f > 0 && std::abs(f - hand) > 3) hand = f;
			if (playId >= 0) { bass.releaseNow(playId, s == playString); endFired(playId); }
			upright::NoteSpec ns; ns.pitch = pitch; ns.vel = in.velInConnected ? std::max(0.05f, std::min(1.f, in.velIn / 10.f)) : 0.75; ns.string = s; ns.fret = f; ns.cents = cents;
			playId = nextId++; playString = s;
			bass.noteOn(ns, (int64_t)seedNow * 7919 + 31 * (noteIdx++) + 5, playId, bass.sampleNow);
			fired.push_back(Fired{bass.sampleNow, (long long)1 << 60, pitch, ns.vel, cents, 0, 0, playId});
			if (fired.size() > 16) fired.pop_front();
		}
		if (!g && gateInHigh && playId >= 0) { bass.releaseNow(playId, false); endFired(playId); playId = -1; }
		gateInHigh = g;
		running = false;
	}
	void endFired(int id) { for (auto& f : fired) if (f.id == id) f.off = std::min(f.off, bass.sampleNow); }
};

} // namespace walker
