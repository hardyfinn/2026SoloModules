// Undertow: the bass player. A two-bar field of dice rolls is read through the style's rhythm weights
// at play time, so DENSITY, FOLLOW, SLIDE and ACCENT act live and the line always fits the chord now.
// DRIFT re-rolls a few steps every two bars; NEW re-rolls the lot.
#pragma once
#include "Link.hpp"

namespace bv {

enum BassStyle { BS_OFFBEAT, BS_ROLLING, BS_ACID, BS_REESE, BS_DUB, BS_ELECTRO, BS_CHICAGO, BS_PEDAL, BS_SKIPPY, BS_SUB, BS_BROKEN, BS_COUNT };
static inline const char* bassStyleName(int s) {
	static const char* N[BS_COUNT] = {"OFFBEAT", "ROLLING", "ACID", "REESE", "DUB", "ELECTRO", "CHICAGO", "PEDAL", "SKIPPY", "SUB", "BROKEN"};
	return N[std::max(0, std::min(BS_COUNT - 1, s))];
}
struct BassStyleDef {
	float w[16];        // chance of a note on each sixteenth at noon DENSITY
	float lenLo, lenHi; // gate length in sixteenths
	float octJump;      // chance of an octave jump
	float slideScale;   // how much SLIDE applies
	float colour;       // how far FOLLOW can take it from the root (0..1)
	bool holdToNext;    // notes hold to the next note
};
static inline const BassStyleDef& bassStyle(int s) {
	static const BassStyleDef D[BS_COUNT] = {
		{{0, 0, 1, .15f, 0, 0, 1, .15f, 0, 0, 1, .15f, 0, 0, 1, .2f}, 1.f, 1.8f, .12f, .3f, .6f, false},
		{{0, .85f, .85f, .85f, 0, .85f, .85f, .85f, 0, .85f, .85f, .85f, 0, .85f, .85f, .85f}, .7f, 1.f, .2f, .2f, .5f, false},
		{{.85f, .5f, .6f, .5f, .7f, .5f, .6f, .5f, .8f, .5f, .6f, .5f, .7f, .5f, .6f, .55f}, .6f, 1.f, .3f, 1.f, 1.f, false},
		{{1, 0, 0, 0, 0, 0, 0, 0, .25f, 0, 0, 0, 0, 0, .3f, 0}, 4.f, 16.f, .05f, .6f, .35f, true},
		{{1, 0, 0, .35f, 0, 0, .6f, 0, .45f, 0, 0, .3f, 0, 0, .5f, 0}, 2.f, 4.f, .1f, .1f, .5f, false},
		{{1, 0, 0, .75f, 0, 0, .8f, 0, .3f, 0, .7f, 0, .25f, 0, .45f, .3f}, .8f, 1.2f, .45f, .2f, .6f, false},
		{{1, 0, .35f, 0, .45f, 0, .85f, 0, .3f, 0, .8f, 0, .3f, .2f, .6f, .2f}, 1.f, 2.f, .15f, .3f, 1.f, false},
		{{.9f, .45f, .9f, .45f, .9f, .45f, .9f, .45f, .9f, .45f, .9f, .45f, .9f, .45f, .9f, .45f}, .6f, .9f, .05f, .1f, .2f, false},
		{{1, 0, 0, .6f, 0, 0, .5f, .3f, 0, .3f, .7f, 0, 0, .5f, 0, .3f}, .8f, 1.5f, .2f, .3f, .7f, false},     // SKIPPY (2-step)
		{{1, 0, 0, 0, 0, 0, 0, 0, .5f, 0, 0, .3f, 0, 0, 0, 0}, 3.f, 8.f, 0.f, .4f, .3f, true},                 // SUB
		{{.7f, .3f, .2f, .5f, .1f, .6f, .2f, .3f, .5f, .2f, .6f, .1f, .4f, .3f, .1f, .5f}, .4f, 1.2f, .35f, .4f, .8f, false}, // BROKEN
	};
	return D[std::max(0, std::min(BS_COUNT - 1, s))];
}

struct UndertowEngine {
	// knobs
	int style = BS_OFFBEAT;
	float density = 0.55f, follow = 0.4f, slide = 0.2f, accent = 0.3f, drift = 0.2f, space = 0.6f;
	int octave = 0;
	uint16_t kickMask = 0x1111;      // where the kick is (learned from KICK, or four to the floor)

	// the dice: 32 sixteenths x 6 rolls
	float uOn[32] = {}, uRole[32] = {}, uLen[32] = {}, uAcc[32] = {}, uSlide[32] = {}, uOct[32] = {};
	Rng rng;
	int lastNote = 36;
	uint16_t barMask = 0;

	UndertowEngine() { reroll(0xB455u); }

	void rollStep(int i) {
		uOn[i] = rng.uni(); uRole[i] = rng.uni(); uLen[i] = rng.uni(); uAcc[i] = rng.uni(); uSlide[i] = rng.uni(); uOct[i] = rng.uni();
	}
	void reroll(uint32_t seed) { rng.seed(seed); for (int i = 0; i < 32; i++) rollStep(i); }
	void driftPattern() {
		if (drift <= 0.001f) return;
		int k = (int)std::floor(drift * 4.f + rng.uni());
		for (int j = 0; j < k; j++) rollStep(rng.range(32));
	}

	float hitChance(int i, float energy, uint16_t stabMask) const {
		const BassStyleDef& d = bassStyle(style);
		int b = i & 15;
		float w = d.w[b];
		float dens = (0.25f + 1.5f * density) * (0.6f + 0.8f * energy);
		float p = w >= 0.99f ? (density < 0.08f ? 0.f : 1.f) : std::min(1.f, w * dens);
		bool strictKick = style == BS_OFFBEAT || style == BS_ROLLING || style == BS_PEDAL || style == BS_SKIPPY;
		if (kickMask & (1 << b)) p *= (1.f - space * (strictKick ? 1.f : 0.5f));
		if ((stabMask & (1 << b)) && style != BS_REESE) p *= (1.f - 0.4f * space);
		return p;
	}
	bool hitAt(int i, float energy, uint16_t stabMask) const { return uOn[i & 31] < hitChance(i & 31, energy, stabMask); }

	// pick the note for a hit
	int noteFor(const Chart& c, int64_t step, int i) {
		const BassStyleDef& d = bassStyle(style);
		int slot = c.slotAtStep(step);
		int root = c.root[slot];
		uint16_t ct = c.chordMask(slot);
		uint16_t sc = chordScale(c, slot);
		int base = 33 + ((root - 33) % 12 + 12) % 12;     // A1..G#2
		base += 12 * octave;
		float f = follow * d.colour;
		float r = uRole[i];
		int note = base;
		// approach the next chord: the last hit before a change
		int toCh = c.stepsToChange(step);
		if (c.n > 1 && toCh <= 2 && f > 0.25f && r < f) {
			int nroot = c.root[c.nextSlot(step)];
			int target = base + ((nroot - root) % 12 + 12) % 12;
			if (target - base > 6) target -= 12;
			int k = (int)(uSlide[i] * 3.f);
			note = k == 0 ? target - 1 : k == 1 ? target + 1 : target - 5;
		} else if (r < 0.55f - 0.3f * f) {
			note = base;                                               // root
		} else if (r < 0.7f - 0.2f * f) {
			note = base + 7;                                           // fifth
			if (!(ct & (1 << ((root + 7) % 12)))) note = base + 12;   // no fifth in the chord: octave
		} else if (r < 0.85f - 0.1f * f) {
			// another chord tone
			int k = 1 + (int)(uLen[i] * 3.f);
			note = stepInMask(ct, base, k);
		} else {
			// passing note from the scale (only off the beat)
			note = (i & 1) ? stepInMask(sc, base, (uOct[i] < 0.5f) ? 1 : -1) : base;
		}
		if (uOct[i] < d.octJump && note < base + 12) note += 12;
		while (note > base + 15) note -= 12;
		while (note < base - 7) note += 12;
		if (style == BS_PEDAL || (style == BS_REESE && follow < 0.5f)) note = (uOct[i] < d.octJump) ? base + 12 : base;
		return note;
	}

	// one sixteenth
	NoteEvent tick(const Chart& c, int64_t step, float energy, uint16_t stabMask) {
		NoteEvent e;
		int i = (int)(step % 32);
		int b = (int)(step % 16);
		if (b == 0) barMask = 0;
		if (i == 0 && step > 0) driftPattern();
		// a chord change on a step the style leaves empty: REESE and DUB catch it
		bool change = c.isChange(step) && c.n > 1;
		bool hit = hitAt(i, energy, stabMask) || (change && (style == BS_REESE || style == BS_DUB || style == BS_SUB) && density > 0.15f);
		if (!hit) return e;
		const BassStyleDef& d = bassStyle(style);
		e.hit = true; e.n = 1;
		e.note[0] = noteFor(c, step, i);
		// length: to the next note for held styles, otherwise from the style's range
		int next = 1;
		while (next < 16 && !hitAt(i + next, energy, stabMask)) next++;
		if (d.holdToNext) e.gateSteps = std::max(1.f, std::min((float)next, (float)c.stepsToChange(step)) - 0.2f);
		else e.gateSteps = std::min((float)next, d.lenLo + (d.lenHi - d.lenLo) * uLen[i]);
		e.accent = uAcc[i] < accent * (b % 4 == 0 ? 1.2f : 0.8f);
		e.slide = next == 1 && uSlide[i] < slide * d.slideScale;
		if (e.slide) e.gateSteps = 1.05f;   // tie into the next note
		e.vel[0] = e.accent ? 1.f : 0.7f;
		lastNote = e.note[0];
		barMask |= 1 << b;
		return e;
	}
};

} // namespace bv
