// Port of simulate() from tools/proto/comper_ref.py: the offline host the golden cases come from.
#pragma once
#include "../src/dsp/Engine.hpp"
#include <vector>
#include <tuple>

namespace comper {

struct SimNote { double on, off; int pitch, vel; };
struct SimChart {
	std::vector<std::pair<double, Chord>> chart; double total = 16;
	Chord at(double beat) const {
		double b = std::fmod(beat, total); Chord cur = chart[0].second;
		for (auto& e : chart) if (e.first <= b + 1e-9) cur = e.second;
		return cur;
	}
	bool next(double beat, Chord& out) const {
		Chord cur = at(beat); double b = std::fmod(beat, total);
		for (auto& e : chart) if (e.first > b + 1e-9 && e.second != cur) { out = e.second; return true; }
		for (auto& e : chart) if (e.second != cur) { out = e.second; return true; }
		return false;
	}
};
struct SimSolo { double on, off; int pitch; };

static const double LEAD_BEATS = 0.12;
static const double EXT_LATE_S = 0.004;

static inline void simulate(const SimChart& ch, double bpm, int nbars, int seed, const Params& prm,
                            const std::vector<SimSolo>& solo, bool external,
                            std::vector<SimNote>& notes, std::vector<std::pair<double, double>>& pedal) {
	Engine eng(seed); eng.p = prm;
	double spb = 60.0 / bpm;
	notes.clear(); pedal.clear();
	struct Held { double nominal, cap; int pitch, vel; double on, extra; };
	std::vector<Held> held;
	auto release = [&](double now, double tNext) {
		for (auto& h : held) {
			double off = std::min(h.nominal + h.cap, std::max(h.nominal + 0.9 * spb, tNext - LEAD_BEATS * spb));
			off = std::max(off, std::min(now, h.nominal + h.cap)) + h.extra;
			notes.push_back({h.on, std::max(off, h.on + 0.02), h.pitch, h.vel});
		}
		held.clear();
	};
	bool hasLastExt = false; Chord lastExt; double lastHitS = -9.0;
	for (int bar = 0; bar < nbars; bar++) {
		eng.beginBar(bar, bpm);
		for (int slot = 0; slot < eng.slots; slot++) {
			double beat = bar * 4 + eng.slotBeats[slot];
			double nominal = beat * spb;
			Ctx ctx; ctx.spb = spb;
			double now; Chord cur;
			if (external) {
				ctx.leadS = -EXT_LATE_S; now = nominal + EXT_LATE_S;
				cur = ch.at(beat);
				ctx.chordAt = [cur](double) { return cur; };
			} else {
				ctx.leadS = LEAD_BEATS * spb; now = nominal - ctx.leadS;
				ctx.chordAt = [&ch](double b) { return ch.at(b); };
				ctx.nextChange = [&ch](double b, Chord& o) { return ch.next(b, o); };
			}
			if (!solo.empty()) {
				ctx.soloConnected = true;
				bool snd = false, rec = false; bool anyEnd = false; double maxEnd = -1e18;
				bool hasLow = false; int low = 999;
				for (auto& s : solo) {
					if (s.on <= now && now < s.off) snd = true;
					if (s.off <= now) { anyEnd = true; maxEnd = std::max(maxEnd, s.off); }
					if (now - 2.5 * spb < s.on && s.on <= now) rec = true;
					if (now - 2.0 * spb < s.off && s.on <= now) { hasLow = true; low = std::min(low, s.pitch); }
				}
				ctx.soloSounding = snd; ctx.soloRecent = rec;
				ctx.soloGap = anyEnd ? (now - maxEnd) / spb : 99.0;
				ctx.hasSoloLow = hasLow; ctx.soloLow = (double)low;
			}
			bool have = false; Result res;
			if (external && hasLastExt && cur != lastExt) {
				release(now, nominal + LEAD_BEATS * spb);
				double u = eng.rr.u();
				bool hasCell = eng.cellmap[slot] >= 0;
				if (!hasCell && now - lastHitS > 0.15 * spb && u < 0.5 + 0.5 * eng.p.density) {
					res = eng.strike(bar, slot, L_, -4.0, ctx, true); have = true;
				}
			}
			if (external) { lastExt = cur; hasLastExt = true; }
			if (!have) res = eng.decide(bar, slot, ctx);
			if (res.hit) { release(now, nominal); lastHitS = now; }
			for (auto& e : res.notes) {
				double on = nominal + e.t;
				if (e.lng) held.push_back({nominal, eng.capBeats() * spb, e.pitch, e.vel, on, e.extra});
				else notes.push_back({on, on + e.dur + e.extra, e.pitch, e.vel});
			}
			for (auto& pv : res.pedal) pedal.push_back({nominal + pv.first, pv.second});
		}
	}
	release(0.0, 1e9);
	std::sort(notes.begin(), notes.end(), [](const SimNote& a, const SimNote& b) {
		return std::tie(a.on, a.off, a.pitch, a.vel) < std::tie(b.on, b.off, b.pitch, b.vel);
	});
}

} // namespace comper
