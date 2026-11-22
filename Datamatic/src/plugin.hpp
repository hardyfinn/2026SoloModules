#pragma once
#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelDatamatic;
extern Model* modelTestPattern;

// LINK cable (Datamatic -> Test Pattern), one polyphonic cable:
//   ch1-4 gates SUB, TONE, CLICK, NOISE   ch5 DATA (0-10 V)   ch6 CUT gate
static const int LINK_CHANNELS = 6;
