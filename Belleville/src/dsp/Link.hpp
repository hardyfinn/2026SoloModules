// What Strata, Undertow and Signal tell each other through Rack's expander messages, passed along the row
// in any order. Three sections, each valid while its flag is set. No Rack includes.
#pragma once
#include "Theory.hpp"

namespace bv {

struct TrioLink {
	uint32_t magic = 0x42454C4C;   // "BELL"
	uint32_t stamp = 0;            // one up every sample (a stale message means no link)
	// --- from Strata
	bool hasStrata = false;
	bool running = false;
	uint32_t resetCount = 0;
	int64_t step = -1;             // sixteenth notes since reset (-1 before the first clock)
	float stepSamples = 5000.f;    // length of a sixteenth now, in samples
	float swing = 0.5f;            // 0.5 straight .. 0.66 heavy (shared so the trio swings together)
	float energy = 0.5f;           // ENERGY input, 0..1
	int style = 0;
	Chart chart;
	uint16_t stabMask = 0;         // Strata's hits in the bar now (bit = sixteenth)
	// --- from Undertow
	bool hasBass = false;
	uint16_t bassMask = 0;         // the bass's hits in the bar now
	int bassNote = 36;
	// --- from Signal
	bool hasSig = false;
	uint16_t sigMask = 0;
	int sigNote = 72;
};

static inline void copyStrata(TrioLink& d, const TrioLink& s) {
	d.hasStrata = s.hasStrata; d.running = s.running; d.resetCount = s.resetCount; d.step = s.step;
	d.stepSamples = s.stepSamples; d.swing = s.swing; d.energy = s.energy; d.style = s.style;
	d.chart = s.chart; d.stabMask = s.stabMask;
}
static inline void copyBass(TrioLink& d, const TrioLink& s) { d.hasBass = s.hasBass; d.bassMask = s.bassMask; d.bassNote = s.bassNote; }
static inline void copySig(TrioLink& d, const TrioLink& s) { d.hasSig = s.hasSig; d.sigMask = s.sigMask; d.sigNote = s.sigNote; }

// what a module hears: each section from whichever side has it (left first)
static inline TrioLink linkMerge(const TrioLink* left, const TrioLink* right) {
	TrioLink m;
	const TrioLink* src[2] = {left, right};
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasStrata && !m.hasStrata) copyStrata(m, *src[k]);
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasBass && !m.hasBass) copyBass(m, *src[k]);
	for (int k = 0; k < 2; k++) if (src[k] && src[k]->hasSig && !m.hasSig) copySig(m, *src[k]);
	return m;
}
// what a module passes to one side: its own sections, plus whatever came from the other side
static inline TrioLink linkForward(const TrioLink& own, const TrioLink* other) {
	TrioLink m;
	if (own.hasStrata) copyStrata(m, own); else if (other && other->hasStrata) copyStrata(m, *other);
	if (own.hasBass) copyBass(m, own); else if (other && other->hasBass) copyBass(m, *other);
	if (own.hasSig) copySig(m, own); else if (other && other->hasSig) copySig(m, *other);
	return m;
}

// one step's worth of notes from any of the three players
struct NoteEvent {
	bool hit = false;
	int n = 0;                     // notes (1 for bass and melody, up to 6 for chords)
	int note[6] = {};
	float vel[6] = {};
	float gateSteps = 1.f;         // how long the gate stays high, in sixteenths
	bool accent = false, slide = false;
	bool change = false;           // the chord changed on this step
	float mod = 0.f;               // Signal's contour, 0..1
};

// swing: the offbeat sixteenths come late by this fraction of a sixteenth
static inline float swingDelaySteps(int64_t step, float swing) {
	return (step & 1) ? (swing - 0.5f) * 2.f : 0.f;
}

} // namespace bv
