// Knob tables for the 909 Circuits voices (filled in by the generated VoiceSpecs.hpp).
#pragma once
#include <cmath>

namespace c909 {

enum { DISP_PCT, DISP_EXP, DISP_LIN, DISP_INT, DISP_SW };

struct KnobDef {
	const char* id;
	const char* label;
	const char* tip;
	float stock;      // knob position (0..1) of the stock 909 setting; switches: the stock position
	int disp;
	float lo, hi;     // value range for the display / engine mapping
	const char* unit; // switches: position names, bottom first, '|' separated
	int row, col;
	int kind;         // 0 knob, 1 three-way switch, 2 two-way switch
	bool norand;
};

struct CvDef {
	const char* label;
	int knob;         // -1 = V/OCT
};

struct VoiceSpec {
	const char* slug;
	const char* name;
	int nKnobs;
	const KnobDef* knobs;
	int nCv;
	const CvDef* cv;
	int randRow;      // panel row (2-4) of the RAND / NEAR STOCK buttons
};

// knob position -> value, as the tooltip shows it
inline float knobValue(const KnobDef& d, float k) {
	switch (d.disp) {
		case DISP_EXP: return d.lo * std::pow(d.hi / d.lo, k < 0.f ? 0.f : (k > 1.f ? 1.f : k));
		case DISP_LIN: return d.lo + (d.hi - d.lo) * k;
		case DISP_INT: return std::round(d.lo + (d.hi - d.lo) * k);
		case DISP_PCT: return k;
		default: return k;
	}
}

// NEAR STOCK randomise: a knob position drawn so that values near the stock setting are
// twice as likely as values at the ends of the knob. The density falls in a straight
// line from 2 at stock to 1 at each end; each side gets its share by its length.
// u1, u2: independent uniform numbers in [0, 1).
inline float nearStock(float stock, float u1, float u2) {
	float t = 2.f - std::sqrt(4.f - 3.f * u2);   // 0..1, inverse CDF of (2 - t) / 1.5
	return u1 < stock ? stock - t * stock : stock + t * (1.f - stock);
}

}  // namespace c909
