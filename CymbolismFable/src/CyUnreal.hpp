// Cymbolism Extended: the UNREAL settings and the frequency law (pure C++, no Rack).
//
// Every setting here changes one physical law of the cymbal model, never the signal.
// With all of them at their neutral values the engine runs exactly as Cymbolism.
//
// The frequency pipeline (per mode, at control rate):
//   f_base   model frequency x hammering
//   MORPH    f_base^(1-x) * f_B^x                 (B: TYPE B's matched frequencies)
//   LOAD     f / sqrt(1 + m * phi(x_load)^2)
//   TENSION  f * sqrt(max(0.05, 1 + tau * s_i))
//   WARP     pivot * (f / pivot)^gamma
//   TUNE     pulled towards the nearest allowed pitch (twins share a target)
//   V/OCT    f * 2^volts
// SIZE (/ lambda) and MOUNT are applied afterwards by the engine, as before.
#pragma once
#include "CyModes.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace cy {

enum Scale { SC_CHROMATIC = 0, SC_MAJOR, SC_MINOR, SC_DORIAN, SC_PENTMAJ, SC_PENTMIN, SC_BLUES, SC_WHOLE, SC_FIFTHS, SC_OCTAVES, SC_HARMONIC, SC_CHORD, NUM_SCALES };
static const char* CY_SCALE_NAMES[NUM_SCALES] = {"CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PENT MAJ", "PENT MIN", "BLUES", "WHOLE TONE", "FIFTHS", "OCTAVES", "HARMONIC", "CHORD"};
static const char* CY_NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct Unreal {
	// frequency law
	double morph = 0.0;       // 0..1 towards TYPE B's frequencies
	double load = 0.0;        // kg (0 .. 0.5)
	double loadPos = 0.5;     // fraction of radius
	double tension = 0.0;     // live tension -1..1 (no rebuild)
	double warp = 1.0;        // gamma -1..2.5
	double pivot = 700.0;     // Hz
	double tune = 0.0;        // 0..1
	int scale = SC_PENTMIN;
	double root = 0.0;        // semitones above C
	double voct = 0.0;        // volts, transposes every mode
	int nChord = 0;           // CHORD input notes (volts, any octave)
	float chord[16] = {};
	// losses
	double tilt = 0.0;        // -1..1
	// energy thermostat
	double sustain = 0.0;     // 0..1 (effective level: the module folds HOLD in)
	double swell = 6.0;       // rise rate, 1/s (0.5..30)
	double colour = 1000.0;   // Hz
	bool freeze = false;
	// nonlinearity
	double flow = 1.0;        // cascade rate multiplier (0.05..10)
	// contact
	double bounce = 0.0;      // 0..1
	// pickup
	double width = 0.0;       // stereo half-angle, radians (0 .. pi/2)
	double pickup = 0.0;      // 0 AIR .. 1 PIEZO

	bool freqNeutral() const {
		return morph == 0.0 && load == 0.0 && tension == 0.0 && warp == 1.0 && tune == 0.0 && voct == 0.0;
	}
	// a fingerprint of everything the frequency table depends on
	void freqKey(std::vector<double>& k) const {
		k.clear();
		k.push_back(morph); k.push_back(load); k.push_back(loadPos); k.push_back(tension);
		k.push_back(warp); k.push_back(pivot); k.push_back(tune); k.push_back(scale);
		k.push_back(root); k.push_back(voct); k.push_back(tilt);
		if (scale == SC_CHORD) { k.push_back(nChord); for (int i = 0; i < nChord; i++) k.push_back(chord[i]); }
	}
};

// Allowed pitches for TUNE, in semitones relative to C4 (261.63 Hz), sorted, covering 8 Hz .. 40 kHz.
inline void tuneTargets(const Unreal& u, std::vector<double>& t) {
	t.clear();
	static const int SC[][12] = {
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
		{0, 2, 4, 5, 7, 9, 11, -1},
		{0, 2, 3, 5, 7, 8, 10, -1},
		{0, 2, 3, 5, 7, 9, 10, -1},
		{0, 2, 4, 7, 9, -1},
		{0, 3, 5, 7, 10, -1},
		{0, 3, 5, 6, 7, 10, -1},
		{0, 2, 4, 6, 8, 10, -1},
		{0, 7, -1},
		{0, -1},
	};
	double root = u.root;
	if (u.scale == SC_HARMONIC) {
		// the harmonic series of the root (an octave below middle-C's root, so the low modes find partials)
		double base = root - 24.0;
		for (int n = 1; n <= 512; n++) t.push_back(base + 12.0 * std::log2((double)n));
		for (int o = 1; o <= 4; o++) t.push_back(base - 12.0 * o);
		std::sort(t.begin(), t.end());
		return;
	}
	std::vector<double> pcs;
	if (u.scale == SC_CHORD) {
		if (u.nChord > 0) {
			for (int i = 0; i < u.nChord; i++) {
				double s = 12.0 * u.chord[i];
				double pc = s - 12.0 * std::floor(s / 12.0);
				pcs.push_back(pc);
			}
		}
		else {
			// minor ninth on ROOT
			const double mn9[5] = {0, 3, 7, 10, 14};
			for (double s : mn9) pcs.push_back(std::fmod(root + s, 12.0));
		}
	}
	else {
		int si = std::max(0, std::min(9, u.scale));
		for (int j = 0; j < 12 && SC[si][j] >= 0; j++) {
			pcs.push_back(std::fmod(root + SC[si][j], 12.0));
			if (si == 0) continue;
		}
	}
	for (int oct = -6; oct <= 8; oct++)
		for (double pc : pcs) t.push_back(oct * 12.0 + pc);
	std::sort(t.begin(), t.end());
}

inline double nearestTarget(const std::vector<double>& t, double s) {
	auto it = std::lower_bound(t.begin(), t.end(), s);
	if (it == t.end()) return t.back();
	if (it == t.begin()) return *it;
	double hi = *it, lo = *(it - 1);
	return (s - lo <= hi - s) ? lo : hi;
}

// Per-mode inputs for the pipeline, owned by the engine.
struct UnrealTables {
	std::vector<double> fBr;     // MORPH: TYPE B's matched frequency / this mode's built frequency (empty = none)
	std::vector<double> fModel;  // built frequency before hammering
	std::vector<double> tensS;   // TENSION sensitivity per mode (from the model)
	std::vector<double> loadPhi2;// phi(x_load)^2 per mode (mass-normalised, at reference size)
	std::vector<int> twin;       // the other mode of a pair (or itself)
	std::vector<double> jit;     // TUNE residual spread per mode (semitones, shared by twins)
};

// MORPH: match each mode of A to B's mode with the same angular order, parity and rank in
// frequency; modes B has no partner for continue B's last ratio in that class.
inline std::vector<double> morphRatios(const Model& A, const Model& B) {
	std::map<std::pair<int, int>, std::vector<double>> tab;
	for (const Mode& md : B.modes) tab[{md.m, md.par}].push_back(md.f);
	for (auto& kv : tab) std::sort(kv.second.begin(), kv.second.end());
	std::map<std::pair<int, int>, std::vector<int>> mine;
	int N = (int)A.modes.size();
	for (int i = 0; i < N; i++) mine[{A.modes[i].m, A.modes[i].par}].push_back(i);
	std::vector<double> r(N, 1.0);
	for (auto& kv : mine) {
		auto idx = kv.second;
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return A.modes[a].f < A.modes[b].f; });
		auto it = tab.find(kv.first);
		double last = 1.0;
		for (size_t k = 0; k < idx.size(); k++) {
			if (it != tab.end() && k < it->second.size()) last = it->second[k] / std::max(1e-6, A.modes[idx[k]].f);
			r[idx[k]] = last;
		}
	}
	return r;
}

// Fill fOut (Hz, at reference size) from fBase through the pipeline.
inline void applyFrequencyLaw(const Unreal& u, const UnrealTables& T, const std::vector<double>& fBase, std::vector<double>& fOut,
                              std::vector<double>& scratch, std::vector<double>& targets) {
	int N = (int)fBase.size();
	fOut.resize(N);
	for (int i = 0; i < N; i++) {
		double f = fBase[i];
		if (u.morph != 0.0 && (int)T.fBr.size() == N) f = f * std::pow(T.fBr[i], u.morph);
		if (u.load != 0.0 && (int)T.loadPhi2.size() == N) f = f / std::sqrt(1.0 + u.load * T.loadPhi2[i]);
		if (u.tension != 0.0 && (int)T.tensS.size() == N) f = f * std::sqrt(std::max(0.05, 1.0 + u.tension * T.tensS[i]));
		if (u.warp != 1.0) {
			// bend the built law, then put the hammering back so a compressed law still beats
			double h = (int)T.fModel.size() == N ? fBase[i] / T.fModel[i] : 1.0;
			f = h * u.pivot * std::pow(std::max(1e-3, f / h / u.pivot), u.warp);
		}
		fOut[i] = f;
	}
	if (u.tune != 0.0) {
		tuneTargets(u, targets);
		const double C4 = 261.6255653;
		scratch.resize(N);
		for (int i = 0; i < N; i++) scratch[i] = 12.0 * std::log2(std::max(1e-3, fOut[i]) / C4);
		for (int i = 0; i < N; i++) {
			int tw = (int)T.twin.size() == N ? T.twin[i] : i;
			double sc = 0.5 * (scratch[i] + scratch[tw]);   // pair centre: twins share a target
			double bt = nearestTarget(targets, sc);
			double jit = (int)T.jit.size() == N ? T.jit[i] : 0.0;
			double s2 = scratch[i] + u.tune * (bt + jit - sc);
			fOut[i] = C4 * std::exp2(s2 / 12.0);
		}
	}
	if (u.voct != 0.0) {
		double m = std::exp2(u.voct);
		for (int i = 0; i < N; i++) fOut[i] *= m;
	}
}

} // namespace cy
