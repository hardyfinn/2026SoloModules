// Soloist's host: follows the band (link) or cables, runs the form (head, solos, trading, head out, end),
// asks the line engine for one phrase at a time just ahead of the beat, plays the notes on the horn with
// swing and human timing, harmonises the leader's head when it is the second Soloist, and writes the
// outputs and its part of the band's link. Sample-driven. No Rack includes.
#pragma once
#include "SoloEngine.hpp"
#include "Horn.hpp"
#include "Link.hpp"
#include <deque>
#include <vector>

namespace solo {

enum Form { F_TUNE = 0, F_SOLO, F_CUE };
static const char* const FORM_NAMES[3] = {"Tune", "Solo", "Cue"};
static const int TRADE_BARS[4] = {0, 2, 4, 8};
static const int KIND_OFF = 3;

struct SIn {
	bool linkOk = false; CompLink link;      // the merged message from the row
	bool follower = false;                   // a Soloist further left leads
	// cables (without Comper)
	bool clockConnected = false; float clock = 0.f; int ppb = 1; float reset = 0.f;
	bool chordConnected = false; int chordChannels = 0; float chordV[16] = {};
	bool playConnected = false; float play = 0.f; float tradeGate = 0.f;
	// controls
	int kind = horn::TENOR;                  // 0..2 the horns, 3 off (notes only)
	double reg = 0.5; int octave = 0; double room = 0.35, level = 1.0;
	double density = 0.5, space = 0.4, motif = 0.4, outside = 0.1, energy = 0.5, loose = 1.0, length = 1.0;
	int form = F_TUNE, choruses = 3, trade = 2, tradeWith = 2, section = 0;
	int seed = 1;
};
struct SOut { float L = 0.f, R = 0.f, voct = 0.f, gate = 0.f, vel = 0.f, breath = 0.f, phrase = 0.f, form = 0.f; };

// the form, as the leader sets it and every Soloist reads it
struct Plan {
	int mode = F_TUNE, soloUnits = 3; double U = 32.0;
	double tradeFrom = -1, tradeTo = -1; int tradeSeg = 4, tradeWith = 2, traders = 1;
	double endBeat = -1; int section = 0; int nSoloists = 1;
	int partAt(double b) const {
		if (mode == F_CUE) return P_CUE;
		if (tradeFrom >= 0 && b >= tradeFrom && b < tradeTo) return P_TRADE;
		if (mode == F_SOLO) return P_SOLO;
		int u = (int)std::floor(b / U + 1e-9);
		if (u < 1) return P_HEAD;
		if (u < 1 + soloUnits) return P_SOLO;
		if (tradeFrom >= 0 && u < (int)std::lround(tradeTo / U)) return P_TRADE;
		if (b < endBeat) return P_OUT;
		return P_END;
	}
	CompLink asLink() const {
		CompLink L; L.sol[0].on = true; L.tradeFrom = tradeFrom; L.tradeTo = tradeTo; L.tradeSeg = tradeSeg; L.tradeWith = tradeWith;
		L.traders = traders; L.endBeat = endBeat; return L;
	}
	// which Soloist (0 leader, 1 second) has the floor at beat b; -1 none
	int soloistAt(double b) const {
		int part = partAt(b);
		if (part == P_TRADE) { int t = turnAt(asLink(), b); return t == TURN_SOLO0 ? 0 : t == TURN_SOLO1 ? 1 : -1; }
		if (part == P_SOLO) {
			if (nSoloists < 2) return 0;
			int u = (int)std::floor(b / U + 1e-9) - (mode == F_TUNE ? 1 : 0);
			return u % 2 == 0 ? 0 : 1;
		}
		if (part == P_CUE) return 0;
		return -1;
	}
};

struct SoloHost {
	double sr = 48000;
	horn::Horn hornV; horn::Room room;
	Line line;
	// transport
	double phi = 0.0, spb = 0.5; bool running = false; double t = 0.0; long long sampleNow = 0;
	uint32_t lastResetCount = 0; bool haveResetCount = false;
	bool clkHigh = false, rstHigh = false, waitingFirstPulse = true;
	int pulses = 0; double lastPulseT = 0, lastPulsePhi = 0; double periods[3] = {0.5, 0.5, 0.5}; int nPeriods = 0;
	bool linked = false;
	// the chart
	int steps = 1, bps = 4; int sroot[16] = {}, squal[16] = {}; bool external = true; Chord extChord = Chord(2, comper::M7);
	int groove = G_SWING, genre = GEN_JAZZ; float swing = 0.f;
	// form
	Plan plan; int partNow = P_IDLE; int lastPart = -1;
	double cueTradeFrom = -1; bool tradeHigh = false, playHigh = false;
	// planning
	int seedNow = 1; double nextStart = 0; bool started = false;
	std::deque<SNote> queue;                 // planned, not yet fired
	std::vector<SNote> headNotes; uint64_t headKey = 0; double headBeats = 0;
	int scheduledHeadUnit = -1;
	// firing
	struct Ev { long long at; int type; SNote n; uint32_t id; };   // type 0 on, 1 off
	std::deque<Ev> evs;
	uint32_t nextId = 1, curId = 0; bool gate = false; int curPitch = 60; double curVel = 0; long long gateLowUntil = -1;
	double noteOnT = 0; SNote curNote;
	uint32_t noteSerial = 0;
	// follower harmony
	uint32_t leadSerial = 0; bool leadSnd = false;
	// outputs
	double breath = 0; long long phraseTrigUntil = -1, formTrigUntil = -1;
	// display
	struct Dot { double beat; int pitch; };
	std::deque<Dot> dots;
	double energyNow = 0.5;
	Rng hr = Rng(99);
	// tests
	struct Log { long long at; int pitch; double vel, len; bool slur; int part; double beat; };
	bool logOn = false; std::vector<Log> log;
	bool renderAudio = true;                 // tests: notes only

	SoloHost() { line.chordAt = [this](double b) { return chordAbs(b); }; line.reset(1); }
	void setSampleRate(double s) { sr = s; hornV.setSampleRate(s); room.setSampleRate(s); }

	// ---------------------------------------------------------------- chart
	Chord chordAbs(double beat) const {
		if (external) return extChord;
		long long st = (long long)std::floor((beat + 1e-9) / bps);
		int s = (int)(((st % steps) + steps) % steps);
		return Chord(sroot[s], squal[s]);
	}
	double unitBeats() const {
		if (external) return 32.0;
		double cb = steps * bps;
		double reps = std::ceil(32.0 / cb - 1e-9);
		return cb * std::max(1.0, reps);
	}

	// ---------------------------------------------------------------- reset
	void doReset(const SIn& in) {
		seedNow = in.seed; line.reset(seedNow + (in.follower ? 500 : 0));
		queue.clear(); evs.clear(); nextStart = 0; started = false; scheduledHeadUnit = -1; headKey = 0;
		if (gate) hornV.noteOff();
		gate = false; cueTradeFrom = -1; lastPart = -1; dots.clear();
		hr = Rng((int64_t)seedNow * 3 + 1);
	}

	// ---------------------------------------------------------------- the plan (leader: from the knobs; second: from the link)
	void makePlan(const SIn& in) {
		if (in.follower && in.linkOk && in.link.sol[0].on) {
			const CompLink& L = in.link;
			plan.mode = L.formMode; plan.soloUnits = L.soloUnits; plan.U = L.unitBeats;
			plan.tradeFrom = L.tradeFrom; plan.tradeTo = L.tradeTo; plan.tradeSeg = L.tradeSeg; plan.tradeWith = L.tradeWith;
			plan.traders = L.traders; plan.endBeat = L.endBeat; plan.section = L.section; plan.nSoloists = 2;
			return;
		}
		plan.mode = in.form; plan.soloUnits = std::max(1, in.choruses); plan.U = unitBeats();
		plan.section = in.section;
		plan.nSoloists = (in.linkOk && in.link.sol[1].on) ? 2 : 1;
		plan.traders = (plan.nSoloists > 1 && in.section == 1) ? 2 : 1;
		int segBars = TRADE_BARS[std::max(0, std::min(3, in.trade))];
		plan.tradeWith = std::max(0, std::min(2, in.tradeWith));
		double tradeLen = 0;
		if (segBars > 0) { plan.tradeSeg = segBars; tradeLen = plan.U * std::ceil(4.0 * 4.0 * segBars * plan.traders / plan.U - 1e-9); }
		plan.tradeFrom = plan.tradeTo = -1;
		if (plan.mode == F_TUNE && segBars > 0) { plan.tradeFrom = (1 + plan.soloUnits) * plan.U; plan.tradeTo = plan.tradeFrom + tradeLen; }
		if (cueTradeFrom >= 0) {
			int sb = segBars > 0 ? segBars : 4; plan.tradeSeg = sb;
			double len = plan.U * std::ceil(4.0 * 4.0 * sb * plan.traders / plan.U - 1e-9);
			if (phi < cueTradeFrom + len) { plan.tradeFrom = cueTradeFrom; plan.tradeTo = cueTradeFrom + len; }
			else cueTradeFrom = -1;
		}
		plan.endBeat = -1;
		if (plan.mode == F_TUNE) {
			double units = 1 + plan.soloUnits + (plan.tradeFrom >= 0 && cueTradeFrom < 0 ? (plan.tradeTo - plan.tradeFrom) / plan.U : 0) + 1;
			plan.endBeat = units * plan.U;
		}
	}
	int myIndex(const SIn& in) const { return in.follower ? 1 : 0; }

	// the energy at beat b
	double energyAt(const SIn& in, double b) const {
		double E = std::max(0.0, std::min(1.0, in.energy));
		int part = plan.partAt(b);
		if (plan.mode == F_CUE) return E;
		double depth = 0.35 + 0.65 * E;
		if (part == P_HEAD || part == P_OUT || part == P_END) return 0.25 + 0.3 * E;
		if (part == P_TRADE) return std::min(1.0, 0.2 + 0.85 * depth);
		double x;
		if (plan.mode == F_TUNE) x = (b - plan.U) / (plan.soloUnits * plan.U);
		else { double per = 4.0 * plan.U; x = std::fmod(std::max(0.0, b), per) / per; }
		return std::max(0.0, std::min(1.0, 0.3 + 0.7 * arcShape(x) * depth));
	}
	SParams paramsAt(const SIn& in, double b) const {
		SParams p; p.kind = in.kind >= KIND_OFF ? horn::TENOR : in.kind; p.groove = groove; p.genre = genre;
		p.density = in.density; p.space = in.space; p.motif = in.motif; p.outside = in.outside;
		p.energy = energyAt(in, b); p.reg = in.reg; p.length = in.length;
		if (plan.partAt(b) == P_TRADE) p.space = std::min(p.space, 0.15);   // trading: say it in the bars you have
		return p;
	}

	// ---------------------------------------------------------------- planning
	// the end of this Soloist's turn that starts at b (or the next change of part)
	double turnEnd(double b, int me) const {
		int part = plan.partAt(b);
		double x = std::floor(b / 4.0 + 1e-9) * 4.0 + 4.0;
		for (int i = 0; i < 128; i++, x += 4.0) if (plan.partAt(x) != part || plan.soloistAt(x) != me) return x;
		return x;
	}
	double nextTurn(double b, int me) const {
		double x = std::ceil(b / 4.0 - 1e-9) * 4.0;
		for (int i = 0; i < 256; i++, x += 4.0) {
			int part = plan.partAt(x);
			if (part == P_END) return 1e12;
			if (plan.soloistAt(x) == me) return x;
			if ((part == P_HEAD || part == P_OUT) && me == 0) return x;
		}
		return x;
	}
	void ensureHead(const SIn& in) {
		uint64_t k = (uint64_t)seedNow * 1000003ULL + (uint64_t)steps * 131 + bps * 17 + groove * 7 + genre * 3 + (uint64_t)(in.kind) * 911 + (uint64_t)std::lround(in.reg * 100) * 7919;
		for (int i = 0; i < steps; i++) k = k * 31 + sroot[i] * 13 + squal[i];
		if (k == headKey && !headNotes.empty()) return;
		headKey = k; headBeats = plan.U;
		SParams p = paramsAt(in, 0); p.energy = 0.45;
		Rng keep = line.hr; line.hr = Rng((int64_t)seedNow * 31 + 7);
		// the tonic: the last chord when it sounds like home (maj7, 6, m6, mMaj7), else the first
		Chord fin = chordAbs(0), lastC = chordAbs(steps * bps - 0.5);
		if (lastC.q == comper::MAJ || lastC.q == comper::MAJ6 || lastC.q == comper::M6 || lastC.q == comper::MMAJ) fin = lastC;
		headNotes = line.head(headBeats, p, fin.root, fin.q);
		line.hr = keep;
	}
	void planAhead(const SIn& in) {
		const int me = myIndex(in);
		const double lead = std::max(0.5, 0.25 / spb);
		for (int guard = 0; guard < 8; guard++) {
			if (phi < nextStart - lead) break;
			double B = nextStart;
			int part = plan.partAt(B);
			bool playOk = !in.playConnected || in.play > 1.f;
			if (part == P_END) { nextStart = 1e12; break; }
			if (!playOk) { nextStart = std::max(B, std::ceil(phi * 2.0) / 2.0) + 0.5; break; }
			if (part == P_HEAD || part == P_OUT) {
				int u = (int)std::floor(B / plan.U + 1e-9);
				if (me == 0 && !external) {
					ensureHead(in);
					if (scheduledHeadUnit != u) {
						scheduledHeadUnit = u;
						double base = u * plan.U;
						for (const SNote& h : headNotes) {
							if (h.last && part != P_OUT) continue;
							if (h.last && plan.partAt(base + h.beat) != P_END && plan.partAt(base + h.beat) != P_OUT) continue;
							SNote n = h; n.beat += base; if (n.beat + 1e-9 < B) continue;
							queue.push_back(n);
						}
					}
					nextStart = (u + 1) * plan.U;
					continue;
				}
				if (me == 0 && external) {   // no chart to write a head on: a riff-like phrase instead
					SParams p = paramsAt(in, B); p.motif = 0.95; p.density *= 0.6; p.outside = 0;
					double lim = (u + 1) * plan.U, nx;
					auto ns = line.phrase(B, p, spb, lim, nx);
					for (auto& n : ns) queue.push_back(n);
					nextStart = ns.empty() ? lim : nx; continue;
				}
				// the second Soloist plays the head with the leader (harmony, live), or rests
				nextStart = (u + 1) * plan.U; continue;
			}
			int who = plan.soloistAt(B);
			if (who == me) {
				double lim = turnEnd(B, me), nx;
				SParams p = paramsAt(in, B);
				auto ns = line.phrase(B, p, spb, lim, nx);
				for (auto& n : ns) queue.push_back(n);
				nextStart = ns.empty() ? lim : std::max(nx, B + 0.5);
				if (nextStart >= lim) nextStart = std::max(nextStart, nextTurn(lim, me));
			} else {
				nextStart = nextTurn(B + 0.01, me);
			}
		}
	}

	// ---------------------------------------------------------------- timing
	double swungBeat(double b) const {
		double base = std::floor(b), f = b - base;
		bool jazzEighths = !sixteenths(groove, genre);
		if (std::fabs(f - 1.0 / 3) < 0.02 || std::fabs(f - 2.0 / 3) < 0.02) return b;   // triplets stay
		if (jazzEighths) {
			double s = groove == G_BOSSA ? 0.5 + 0.03 * swing : 0.5 + 0.14 + 0.08 * swing;
			if (groove == G_BALLAD) s = 0.5 + 0.10 + 0.06 * swing;
			s = std::max(0.45, std::min(0.75, s));
			return base + (f < 0.5 ? f / 0.5 * s : s + (f - 0.5) / 0.5 * (1 - s));
		}
		double s16 = std::max(0.45, std::min(0.66, 0.5 + 0.04 * (swing + 0.5)));
		double h = std::floor(f * 2) * 0.5, g = (f - h) * 2;
		return base + h + 0.5 * (g < 0.5 ? g / 0.5 * s16 : s16 + (g - 0.5) / 0.5 * (1 - s16));
	}
	// fire the planned notes due within the next few ms
	void fireDue(const SIn& in) {
		while (!queue.empty()) {
			const SNote& n = queue.front();
			double sb = swungBeat(n.beat);
			double until = (sb - phi) * spb;
			if (until > 0.004) break;
			SNote x = n; queue.pop_front();
			if (until < -0.25) continue;   // too late (a jump in the clock)
			double human = in.loose * (0.009 + 0.008 * hr.g());   // a horn player sits a hair behind
			long long at = sampleNow + std::max(0LL, (long long)std::llround((until + human) * sr));
			// how long: up to the next note when it slurs, else its own length
			double endSb = swungBeat(x.beat + x.len);
			long long off = at + std::max(1LL, (long long)std::llround((endSb - sb) * spb * sr));
			bool nextSlurs = !queue.empty() && queue.front().slur && queue.front().beat - (x.beat + x.len) < 0.3;
			uint32_t id = nextId++;
			evs.push_back(Ev{at, 0, x, id});
			if (!nextSlurs) evs.push_back(Ev{off, 1, x, id});
			if (logOn) log.push_back(Log{at, x.pitch, x.vel, x.len, x.slur, plan.partAt(x.beat), x.beat});
		}
	}
	void runEvents(const SIn& in) {
		for (size_t i = 0; i < evs.size();) {
			if (evs[i].at > sampleNow) { i++; continue; }
			Ev e = evs[i]; evs.erase(evs.begin() + i);
			if (e.type == 0) startNote(in, e.n, e.id);
			else if (e.id == curId) stopNote(e.n.fall);
		}
	}
	void startNote(const SIn& in, const SNote& n, uint32_t id) {
		bool slur = n.slur && gate;
		horn::Note hn; hn.pitch = n.pitch; hn.vel = n.vel; hn.slur = slur; hn.tongue = n.tongue; hn.scoop = n.scoop; hn.gliss = n.gliss;
		hornV.vibrato = n.vib; hornV.growl = n.growl; hornV.air = n.air;
		if (in.kind < KIND_OFF) hornV.noteOn(hn);
		if (gate && !slur) gateLowUntil = sampleNow + (long long)(0.001 * sr);   // a new tongued note retriggers
		gate = true; curId = id; curPitch = n.pitch; curVel = n.vel; noteOnT = t; curNote = n; noteSerial++;
		if (n.first) phraseTrigUntil = sampleNow + (long long)(0.001 * sr);
		dots.push_back(Dot{phi, n.pitch}); while (dots.size() > 48) dots.pop_front();
	}
	void stopNote(double fall) {
		if (!gate) return;
		hornV.noteOff(fall); gate = false;
	}
	void silence() { queue.clear(); for (size_t i = 0; i < evs.size();) { if (evs[i].type == 0) evs.erase(evs.begin() + i); else i++; } if (gate) stopNote(0); }

	// the second Soloist in Horns: a third or sixth under the leader, on the chord
	int harmony(int lead) const {
		Harm h; h.set(chordAbs(phi));
		static const int DOWN[4] = {3, 4, 8, 9};
		int best = lead - 3, bs = -99;
		for (int d : DOWN) {
			int c = lead - d; int s = h.isCT(c) ? 3 : h.inScale(c) ? 1 : 0; if (d >= 8) s -= 1;
			if (s > bs) { bs = s; best = c; }
		}
		int lo = horn::RANGE_LO[std::min(2, (int)hornV.kind)], hi = horn::RANGE_HI[std::min(2, (int)hornV.kind)];
		return Line::fold(best, lo, hi);
	}
	void follow(const SIn& in) {
		const CompLink::Solo& L = in.link.sol[0];
		bool snd = L.on && L.sounding;
		if (snd && (!leadSnd || L.noteSerial != leadSerial)) {
			SNote n; n.pitch = harmony(L.pitch); n.vel = 0.55; n.slur = leadSnd && gate; n.tongue = 0.45; n.beat = -1;   // -1: a harmony note, not planned
			n.vib = 0.4;
			startNote(in, n, nextId++);
		}
		if (!snd && leadSnd && gate) stopNote(0);
		leadSnd = snd; leadSerial = L.noteSerial;
	}

	// ---------------------------------------------------------------- one sample
	void process(const SIn& in, SOut& out, double dt) {
		linked = in.linkOk && in.link.hasComp;
		if (in.kind < KIND_OFF && hornV.kind != in.kind) { hornV.setKind((horn::Kind)in.kind); gate = false; }
		// transport and chart
		if (linked) {
			const CompLink& L = in.link;
			if (!haveResetCount || L.resetCount != lastResetCount) { lastResetCount = L.resetCount; haveResetCount = true; doReset(in); }
			phi = L.phi; spb = std::max(0.05, L.spb); running = L.running; waitingFirstPulse = false;
			external = L.external;
			steps = std::max(1, std::min(16, L.steps)); bps = std::max(1, L.beatsPerStep);
			for (int i = 0; i < 16; i++) { sroot[i] = pmod(L.root[i], 12); squal[i] = std::max(0, std::min(comper::NUM_Q - 1, L.qual[i])); }
			if (external) extChord = Chord(pmod(L.curRoot, 12), std::max(0, std::min(comper::NUM_Q - 1, L.curQual)));
			groove = std::max(0, std::min(5, L.groove)); genre = std::max(0, std::min(NUM_GENRES - 1, L.genre)); swing = L.swing;
		} else {
			cableTransport(in);
			external = true; groove = G_SWING; genre = GEN_JAZZ; swing = 0.f;
			if (in.chordConnected && in.chordChannels > 0) {
				std::vector<int> ns; for (int i = 0; i < in.chordChannels; i++) ns.push_back((int)std::round(in.chordV[i] * 12.f) + 60);
				Chord d; if (comper::detectChord(ns, extChord.q, d)) extChord = d;
			}
		}
		if (in.seed != seedNow) { seedNow = in.seed; line.reset(seedNow + (in.follower ? 500 : 0)); headKey = 0; }
		// TRADE gate: trading from the next bar line
		bool tg = in.tradeGate > (tradeHigh ? 0.1f : 1.0f);
		if (tg && !tradeHigh && !in.follower && running) cueTradeFrom = std::ceil(phi / 4.0 - 1e-9) * 4.0;
		tradeHigh = tg;
		makePlan(in);
		// PLAY: a mute in every form; in CUE the solo starts when it rises
		bool pl = !in.playConnected || in.play > 1.f;
		if (!pl && playHigh) silence();
		if (pl && !playHigh) nextStart = std::max(nextStart, std::ceil(phi * 2.0 - 1e-9) / 2.0);
		playHigh = pl;

		if (running && !waitingFirstPulse) {
			if (!started) { started = true; nextStart = std::max(0.0, std::ceil(phi * 2.0 - 1e-9) / 2.0); if (phi < 0.05) nextStart = 0; }
			planAhead(in);
			fireDue(in);
		} else if (!running && gate) silence();
		runEvents(in);
		bool harmonise = in.follower && in.linkOk && in.link.sol[0].on && in.link.section == 0;
		int partHere = plan.partAt(phi);
		if (harmonise && (partHere == P_HEAD || partHere == P_OUT || partHere == P_END)) follow(in);
		else if (leadSnd) { leadSnd = false; if (gate && curNote.beat < 0) stopNote(0); }

		partNow = running ? partHere : P_IDLE;
		if (partNow != lastPart) { if (lastPart >= 0) formTrigUntil = sampleNow + (long long)(0.001 * sr); lastPart = partNow; }
		energyNow = energyAt(in, phi);

		// audio
		double y = (in.kind < KIND_OFF && renderAudio) ? hornV.process() * 0.6 * in.level : 0.0, l, r;
		room.process(y, in.room, l, r);
		out.L = (float)(l * 5.0); out.R = (float)(r * 5.0);
		// notes out
		double v = (curPitch - 60) / 12.0 + in.octave;
		double ts = t - noteOnT;
		if (curNote.scoop > 0) { double x = std::min(1.0, ts / (0.10 + 0.06 * curNote.scoop)); v -= curNote.scoop * (1 - x * x * (3 - 2 * x)) / 12.0; }
		if (curNote.vib > 0) v += curNote.vib * 0.2 / 12.0 * std::min(1.0, std::max(0.0, (ts - 0.18) / 0.35)) * std::sin(2 * M_PI * 5.3 * ts);
		out.voct = (float)v;
		bool g = gate && sampleNow >= gateLowUntil;
		out.gate = g ? 10.f : 0.f;
		out.vel = (float)(curVel * 10.0);
		double target = gate ? curVel * 10.0 : 0.0;
		breath += (target - breath) * (1.0 - std::exp(-dt / (target > breath ? 0.015 : 0.06)));
		out.breath = (float)breath;
		out.phrase = sampleNow < phraseTrigUntil ? 10.f : 0.f;
		out.form = sampleNow < formTrigUntil ? 10.f : 0.f;
		t += dt; sampleNow++;
		if (!linked && running) { phi += dt / spb; if (pulses >= 1) phi = std::min(phi, lastPulsePhi + 1.5 / std::max(1, in.ppb)); }
	}

	// what this Soloist tells the band
	void fillLink(const SIn& in, CompLink& m) const {
		int k = myIndex(in);
		CompLink::Solo& s = m.sol[k];
		s.on = true; s.sounding = gate; s.pitch = curPitch; s.noteSerial = noteSerial;
		s.energy = (float)energyNow; s.outside = (float)in.outside; s.part = partNow; s.kind = in.kind;
		s.phraseEnd = line.lastPhraseEnd;
		s.rhythmN = line.lastRhythmN; for (int i = 0; i < 16; i++) s.rhythm[i] = line.lastRhythm[i];
		if (k == 0) {
			m.tradeFrom = plan.tradeFrom; m.tradeTo = plan.tradeTo; m.tradeSeg = plan.tradeSeg; m.tradeWith = plan.tradeWith; m.traders = plan.traders;
			m.endBeat = plan.endBeat; m.section = plan.section; m.formMode = plan.mode; m.soloUnits = plan.soloUnits; m.unitBeats = plan.U; m.seed = seedNow;
		}
	}

	// ---------------------------------------------------------------- cables (as Rider's)
	void cableTransport(const SIn& in) {
		bool rst = in.reset > (rstHigh ? 0.1f : 1.0f); bool rstEdge = rst && !rstHigh; rstHigh = rst;
		if (rstEdge) { doReset(in); phi = 0; waitingFirstPulse = true; }
		if (!in.clockConnected) { running = false; return; }
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
};

} // namespace solo
