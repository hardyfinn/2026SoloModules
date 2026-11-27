#pragma once
#include <rack.hpp>
#include <atomic>
#include <vector>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelThunder;
extern Model* modelLightning;

// What Lightning (placed to the right of Thunder) reads and writes.
// Values: the 24 memory values (memory order), then [24] accent 0..1 and [25] level.
struct ThunderLink {
	float linkOut = 0.f;               // Thunder's OUT, every sample
	uint32_t hitCount = 0;             // goes up by one at every hit
	float hitVals[26] = {};            // what the last hit used
	float sendVals[26] = {};           // a sound sent back from Lightning
	std::atomic<int> sendState{0};     // 0 idle, 1 writing, 2 ready for Thunder
	// full-length render of one hit, done on Thunder's background thread
	std::atomic<int> renderState{0};   // 0 idle, 1 asked, 2 rendering, 3 done (waiting for its owner)
	const void* renderOwner = nullptr; // the Lightning that asked
	int renderSlot = 0;
	float renderVals[26] = {};
	float renderSr = 48000.f;
	std::vector<float> renderOut;      // owner swaps this out when done
	int renderLen = 0;
	virtual ~ThunderLink() {}
};
