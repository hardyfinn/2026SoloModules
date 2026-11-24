// What the band's modules (Comper, Walker, Rider) tell each other through Rack's expander messages, passed along
// the row in any order. Three sections, each valid while its flag is set. No Rack includes.
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
	// --- added in 2.2.0
	bool hasComp = false;          // everything above, and the figure below, come from a Comper
	int planBar = -1;              // the bar the figure is for (bar 0 starts at phi 0); -1 none yet
	int planSlots = 8;             // 8 or 16
	int planKind[16] = {};         // -1 no hit, 0 short, 1 long
	double planBeat[16] = {};      // beats from the bar line
	bool planPush[16] = {};        // the upbeat anticipates the next chord
	bool hasBass = false;          // from a Walker
	int bassN = 0;                 // planned bass notes that fall off the beat
	double bassBeat[16] = {};      // their beats, on the same clock as phi
	bool hasDrum = false;          // from a Rider
	float bandX = 0.f;             // -1..1: BAND * 2 * (ENERGY - 0.5)
};

static inline void linkCopyComp(CompLink& d, const CompLink& s) {
	d.resetCount = s.resetCount; d.running = s.running; d.phi = s.phi; d.spb = s.spb; d.steps = s.steps; d.beatsPerStep = s.beatsPerStep;
	for (int i = 0; i < 16; i++) { d.root[i] = s.root[i]; d.qual[i] = s.qual[i]; }
	d.external = s.external; d.curRoot = s.curRoot; d.curQual = s.curQual; d.groove = s.groove; d.swing = s.swing;
	d.hasComp = s.hasComp; d.planBar = s.planBar; d.planSlots = s.planSlots;
	for (int i = 0; i < 16; i++) { d.planKind[i] = s.planKind[i]; d.planBeat[i] = s.planBeat[i]; d.planPush[i] = s.planPush[i]; }
}
static inline void linkCopyBass(CompLink& d, const CompLink& s) {
	d.hasBass = s.hasBass; d.bassN = s.bassN; for (int i = 0; i < 16; i++) d.bassBeat[i] = s.bassBeat[i];
}
static inline void linkCopyDrum(CompLink& d, const CompLink& s) { d.hasDrum = s.hasDrum; d.bandX = s.bandX; }
// what a module hears: each section from whichever side has it (left first)
static inline CompLink linkMerge(const CompLink* left, const CompLink* right) {
	CompLink m;
	const CompLink* src[2] = {left, right};
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasComp && !m.hasComp) linkCopyComp(m, *src[k]);
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasBass && !m.hasBass) linkCopyBass(m, *src[k]);
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasDrum && !m.hasDrum) linkCopyDrum(m, *src[k]);
	return m;
}
// what a module passes on to one side: its own sections, plus whatever came from the other side
static inline CompLink linkForward(const CompLink& own, const CompLink* fromOtherSide) {
	CompLink m;
	if (own.hasComp) linkCopyComp(m, own); else if (fromOtherSide && fromOtherSide->hasComp) linkCopyComp(m, *fromOtherSide);
	if (own.hasBass) linkCopyBass(m, own); else if (fromOtherSide && fromOtherSide->hasBass) linkCopyBass(m, *fromOtherSide);
	if (own.hasDrum) linkCopyDrum(m, own); else if (fromOtherSide && fromOtherSide->hasDrum) linkCopyDrum(m, *fromOtherSide);
	return m;
}
