#pragma once
#include <rack.hpp>
#include <atomic>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelThunder;
extern Model* modelLightning;

// What Lightning (placed to the right of Thunder) reads and writes.
struct ThunderLink {
	float linkOut = 0.f;               // Thunder's OUT, every sample
	uint32_t hitCount = 0;             // goes up by one at every hit
	float hitVals[24] = {};            // knob values the last hit used (memory order)
	float sendVals[24] = {};           // a sound sent back from Lightning
	std::atomic<int> sendState{0};     // 0 idle, 1 writing, 2 ready for Thunder
	virtual ~ThunderLink() {}
};
