// Knob tables for the 808 Circuits voices (filled in by the generated VoiceSpecs.hpp).
#pragma once
#include <cmath>

namespace c808 {

enum { DISP_PCT, DISP_EXP, DISP_LIN, DISP_INT, DISP_SW };

struct KnobDef {
	const char* id;
	const char* label;
	const char* tip;
	float stock;      // knob position (0..1) of the stock 808 setting; switches: the stock position
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

}  // namespace c808
