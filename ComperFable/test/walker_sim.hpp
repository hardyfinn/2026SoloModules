// The offline loop of walker_ref.py (play_line / play_blind) on the C++ engine, for the tests.
#pragma once
#include "../src/dsp/WalkerEngine.hpp"
#include <vector>
namespace walker {
struct SimChart {
	std::vector<std::pair<double, Chord>> chart; double total = 16;
	Chord at(double beat) const {
		double b = std::fmod(beat, total); if (b < 0) b += total;
		Chord cur = chart[0].second;
		for (auto& x : chart) if (x.first <= b + 1e-6) cur = x.second;
		return cur;
	}
};
static inline std::vector<Note> simulateLine(const SimChart& ch, double bpm, int nbars, int seed, const BP& p, bool blind) {
	Player pl; pl.chordAt = [&ch](double b) { return ch.at(b); }; pl.bpm = bpm; pl.p = p; pl.reset(seed);
	std::vector<Note> out; out.reserve(4096);
	if (blind) {
		for (int b = 0; b < nbars * 4; b++) {
			Chord c = ch.at(b);
			std::vector<Event> ev = {pl.blindBeat(b, c.root, c.q, b > 0 && c != ch.at(b - 1))};
			pl.perform(ev, out);
		}
		return out;
	}
	pl.startFeel(p.feel);
	while (pl.nextStepBeat() < nbars * 4) pl.perform(pl.step(), out);
	return out;
}
}
