// What the band's modules (Comper, Walker, Rider, Soloist) tell each other through Rack's expander messages, passed along
// the row in any order. Three sections, each valid while its flag is set. No Rack includes.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
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
	// --- added in 2.5.0
	int genre = 0;                 // from Comper: the family of the loaded progression (solo::Genre), 0 jazz
	// the soloists: slot 0 is the leader (the Soloist furthest left), slot 1 a second Soloist
	struct Solo {
		bool on = false;
		bool sounding = false; int pitch = 60; uint32_t noteSerial = 0;   // the note now (serial: one up per new note)
		float energy = 0.f, outside = 0.f;
		int part = 0;              // solo::Part now
		double phraseEnd = -1.0;   // beat of the last note of the phrase now or next (-1 none)
		int rhythmN = 0; float rhythm[16] = {};   // the last phrase's onsets, beats from its first bar line (for Rider's breaks)
		double rhythmBar = 0.0;    // (unused by readers; kept for the display)
		int kind = 0;              // the horn
	} sol[2];
	// the leader's plan (valid when sol[0].on): trading and the end of the tune, as beats on the clock of phi
	double tradeFrom = -1.0, tradeTo = -1.0; int tradeSeg = 4, tradeWith = 0, traders = 1;
	double endBeat = -1.0;         // from here on the band plays the last chord, and stops a bar later
	int section = 0;               // 0 Horns, 1 Turns
	int formMode = 0, soloUnits = 4; double unitBeats = 32.0;   // the leader's form: solo::Form, solo choruses, beats in a chorus
	int seed = 1;                  // the leader's seed (the second Soloist's own line differs by its own seed)
	bool hasSolo() const { return sol[0].on || sol[1].on; }
};

// whose turn it is at a beat (all modules read it the same way)
enum Turn { TURN_FREE = -1, TURN_SOLO0 = 0, TURN_SOLO1 = 1, TURN_DRUMS = 2, TURN_PIANO = 3, TURN_END = 4 };
static inline int turnAt(const CompLink& L, double beat) {
	if (!L.sol[0].on) return TURN_FREE;
	if (L.endBeat >= 0 && beat >= L.endBeat + 4.0) return TURN_END;
	if (L.tradeFrom < 0 || beat < L.tradeFrom || beat >= L.tradeTo) return TURN_FREE;
	int seg = (int)std::floor((beat - L.tradeFrom + 1e-9) / (4.0 * std::max(1, L.tradeSeg)));
	if (seg % 2 == 0) return (L.traders > 1 && (seg / 2) % 2 == 1) ? TURN_SOLO1 : TURN_SOLO0;
	if (L.tradeWith == 0) return TURN_DRUMS;
	if (L.tradeWith == 1) return TURN_PIANO;
	return ((seg / 2) % 2 == 0) ? TURN_DRUMS : TURN_PIANO;
}
static inline bool soloSounding(const CompLink& L) { return (L.sol[0].on && L.sol[0].sounding) || (L.sol[1].on && L.sol[1].sounding); }
static inline float soloEnergy(const CompLink& L) { float e = 0.f; for (int k = 0; k < 2; k++) if (L.sol[k].on) e = std::max(e, L.sol[k].energy); return e; }
static inline float soloOutside(const CompLink& L) { float e = 0.f; for (int k = 0; k < 2; k++) if (L.sol[k].on) e = std::max(e, L.sol[k].outside); return e; }

static inline void linkCopyComp(CompLink& d, const CompLink& s) {
	d.resetCount = s.resetCount; d.running = s.running; d.phi = s.phi; d.spb = s.spb; d.steps = s.steps; d.beatsPerStep = s.beatsPerStep;
	for (int i = 0; i < 16; i++) { d.root[i] = s.root[i]; d.qual[i] = s.qual[i]; }
	d.external = s.external; d.curRoot = s.curRoot; d.curQual = s.curQual; d.groove = s.groove; d.swing = s.swing;
	d.genre = s.genre;
	d.hasComp = s.hasComp; d.planBar = s.planBar; d.planSlots = s.planSlots;
	for (int i = 0; i < 16; i++) { d.planKind[i] = s.planKind[i]; d.planBeat[i] = s.planBeat[i]; d.planPush[i] = s.planPush[i]; }
}
static inline void linkCopyBass(CompLink& d, const CompLink& s) {
	d.hasBass = s.hasBass; d.bassN = s.bassN; for (int i = 0; i < 16; i++) d.bassBeat[i] = s.bassBeat[i];
}
static inline void linkCopyDrum(CompLink& d, const CompLink& s) { d.hasDrum = s.hasDrum; d.bandX = s.bandX; }
static inline void linkCopySolo(CompLink& d, const CompLink& s, int k) {
	d.sol[k] = s.sol[k];
	if (k == 0) { d.tradeFrom = s.tradeFrom; d.tradeTo = s.tradeTo; d.tradeSeg = s.tradeSeg; d.tradeWith = s.tradeWith; d.traders = s.traders; d.endBeat = s.endBeat; d.section = s.section; d.formMode = s.formMode; d.soloUnits = s.soloUnits; d.unitBeats = s.unitBeats; d.seed = s.seed; }
}
// what a module hears: each section from whichever side has it (left first)
static inline CompLink linkMerge(const CompLink* left, const CompLink* right) {
	CompLink m;
	const CompLink* src[2] = {left, right};
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasComp && !m.hasComp) linkCopyComp(m, *src[k]);
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasBass && !m.hasBass) linkCopyBass(m, *src[k]);
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasDrum && !m.hasDrum) linkCopyDrum(m, *src[k]);
	for (int j = 0; j < 2; j++) for (int k = 0; k < 2; k++) if (src[k] && src[k]->sol[j].on && !m.sol[j].on) linkCopySolo(m, *src[k], j);
	return m;
}
// what a module passes on to one side: its own sections, plus whatever came from the other side
static inline CompLink linkForward(const CompLink& own, const CompLink* fromOtherSide) {
	CompLink m;
	if (own.hasComp) linkCopyComp(m, own); else if (fromOtherSide && fromOtherSide->hasComp) linkCopyComp(m, *fromOtherSide);
	if (own.hasBass) linkCopyBass(m, own); else if (fromOtherSide && fromOtherSide->hasBass) linkCopyBass(m, *fromOtherSide);
	if (own.hasDrum) linkCopyDrum(m, own); else if (fromOtherSide && fromOtherSide->hasDrum) linkCopyDrum(m, *fromOtherSide);
	for (int j = 0; j < 2; j++) { if (own.sol[j].on) linkCopySolo(m, own, j); else if (fromOtherSide && fromOtherSide->sol[j].on) linkCopySolo(m, *fromOtherSide, j); }
	return m;
}
