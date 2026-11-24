// What Comper tells a Walker sitting next to it, through Rack's expander messages. No Rack includes.
#pragma once
#include <cstdint>
struct CompLink {
	uint32_t magic = 0x434F4D50;   // "COMP"
	uint32_t resetCount = 0;       // goes up by one at every reset
	uint32_t stamp = 0;            // goes up by one every sample (a stale message means no link)
	bool running = false;
	double phi = 0.0;              // beats since reset, as Comper's host has it
	double spb = 0.5;              // seconds per beat now
	int steps = 4, beatsPerStep = 4;
	int root[16] = {}, qual[16] = {};   // after KEY transposition
	bool external = false;         // Comper's chords come from CV or POLY, not the chart
	int curRoot = 0, curQual = 0;  // the chord Comper is on now
	int groove = 0;                // 0 SWING 1 BOSSA 2 BALLAD 3 SOUL 4 HOUSE
	float swing = 0.f;             // Comper's SWING parameter, -1..1
};
