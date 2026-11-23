// Ranges that differ between Cymbolism and Cymbolism Lite (shared with test/lite_check).
#pragma once
#include "CyGeometry.hpp"

namespace cy {

// Lite narrows the four ranges that cost
// the most CPU (size, weight, rivets, the bow) and runs only the MID and ECO engines.
struct CyLimits {
	float sizeMax;     // SIZE knob top
	float sizeCvMax;   // engine size clamp after SIZE CV
	float weightMin;   // WEIGHT knob bottom (and clamp after CV)
	int sizzleMax;     // rivets
	int numPlays;      // beaters on the PLAY switch (Lite drops the bow)
	int defaultQuality;
	bool lite;
};
static const CyLimits CY_FULL = {1.5f, 2.0f, 0.5f, 12, NUM_PLAYS, 1, false};
static const CyLimits CY_LITE = {1.2f, 1.2f, 0.8f, 6, NUM_PLAYS - 1, 3, true};

}  // namespace cy
