// Signal: the melody player. Writes a short motif (a list of moves through the chord's notes) and plays it
// round and round over the bar line. MUTATE changes a little of it every time round and keeps the changes,
// so the line evolves; FREEZE stops it; NEW writes a fresh one. The motif is stored as moves, not notes,
// so it always lands on the chord now.
#pragma once
#include "Link.hpp"

namespace bv {

enum SigStyle { SS_STRINGS, SS_ARP, SS_HOOK, SS_CALL, SS_BLEEP, SS_LEAD, SS_ACID, SS_GLITCH, SS_COUNT };
static inline const char* sigStyleName(int s) {
	static const char* N[SS_COUNT] = {"STRING LINE", "ARP", "HOOK", "CALL+ANSWER", "BLEEP", "LEAD", "ACID LINE", "GLITCH"};
	return N[std::max(0, std::min(SS_COUNT - 1, s))];
}
static inline int motifLen(int sel) {
	static const int L[12] = {3, 4, 5, 6, 7, 8, 12, 16, 24, 32, 48, 64};
	return L[std::max(0, std::min(11, sel))];
}
struct SigStyleDef {
	float w[16];          // chance of a note at each place in the motif (by position mod 16) at noon DENSITY
	float lenLo, lenHi;   // gate in sixteenths
	int maxMove;          // biggest move between notes, in chord-scale steps
	bool chordOnly;       // moves through chord tones only
	int centre;           // register centre (MIDI)
};
static inline const SigStyleDef& sigStyle(int s) {
	static const SigStyleDef D[SS_COUNT] = {
		{{1, 0, 0, 0, .2f, 0, 0, 0, .6f, 0, 0, 0, .2f, 0, 0, 0}, 4.f, 8.f, 1, false, 67},
		{{1, .8f, 1, .8f, 1, .8f, 1, .8f, 1, .8f, 1, .8f, 1, .8f, 1, .8f}, .5f, .8f, 1, true, 69},
		{{1, 0, .7f, .3f, 0, .75f, 0, .5f, .2f, 0, .7f, 0, .3f, .5f, 0, .2f}, .8f, 1.6f, 2, false, 72},
		{{1, 0, .6f, 0, .7f, 0, .5f, .3f, 0, 0, 0, 0, 0, 0, 0, 0}, 1.f, 2.f, 2, false, 70},
		{{.6f, .2f, .35f, .2f, .4f, .2f, .35f, .25f, .5f, .2f, .35f, .2f, .4f, .2f, .35f, .25f}, .3f, .6f, 5, false, 84},
		{{1, 0, .6f, 0, .7f, 0, .5f, .3f, .8f, 0, .5f, .2f, .6f, 0, .4f, .3f}, 1.f, 3.f, 2, false, 72},
		{{.85f, .5f, .65f, .5f, .75f, .5f, .65f, .55f, .8f, .5f, .65f, .5f, .75f, .5f, .65f, .6f}, .5f, 1.f, 3, false, 60}, // ACID LINE
		{{.5f, .1f, .3f, .6f, .1f, .4f, .2f, .1f, .6f, .3f, .1f, .4f, .2f, .5f, .1f, .3f}, .15f, .4f, 6, false, 76},       // GLITCH
	};
	return D[std::max(0, std::min(SS_COUNT - 1, s))];
}

struct MotifStep { float uOn = 0.f; int move = 0; float uLen = 0.f; float uVel = 0.f; float uOut = 0.f; };

struct SignalEngine {
	// knobs
	int style = SS_STRINGS, motifSel = 7;
	float range = 0.5f, density = 0.5f, answer = 0.6f, outside = 0.1f, mutate = 0.3f;
	bool freeze = false;
	int registerOct = 0;

	MotifStep motif[64], home[64];
	Rng rng;
	int gen = 0;
	int pos = 0;                  // place in the motif
	int lastNote = 72;
	int anchor = 72;              // the note the motif is measured from (moves with the chords)
	uint16_t barMask = 0;
	bool started = false;

	SignalEngine() { newMotif(0x516Eu); }

	void rollStep(MotifStep& m) {
		const SigStyleDef& d = sigStyle(style);
		m.uOn = rng.uni(); m.uLen = rng.uni(); m.uVel = rng.uni(); m.uOut = rng.uni();
		int mm = d.maxMove;
		// mostly small moves, the occasional leap
		float r = rng.uni();
		int mv = r < 0.25f ? 0 : r < 0.75f ? 1 : r < 0.92f ? std::min(2, mm) : mm;
		m.move = rng.uni() < 0.5f ? -mv : mv;
	}
	void newMotif(uint32_t seed) {
		rng.seed(seed);
		for (int i = 0; i < 64; i++) { rollStep(motif[i]); home[i] = motif[i]; }
		gen = 0;
	}
	// once round the motif: change a little, and now and then put one thing back the way it was
	void mutateOnce() {
		if (freeze || mutate <= 0.001f) return;
		int L = motifLen(motifSel);
		if (rng.uni() > 0.25f + 0.75f * mutate) return;
		int k = 1 + (int)(mutate * mutate * 3.f);
		for (int j = 0; j < k; j++) {
			int i = rng.range(L);
			MotifStep& m = motif[i];
			switch (rng.range(5)) {
				case 0: m.move += rng.uni() < 0.5f ? -1 : 1; m.move = std::max(-6, std::min(6, m.move)); break;
				case 1: m.uOn = rng.uni(); break;                                   // a note appears or goes
				case 2: m.uLen = rng.uni(); break;
				case 3: { int i2 = (i + 1) % L; std::swap(motif[i], motif[i2]); } break;
				case 4: m.move = -m.move; break;                                    // turn a move upside down
			}
		}
		if (rng.uni() < 0.35f * (1.f - mutate)) { int i = rng.range(L); motif[i] = home[i]; }
		gen++;
	}

	float hitChance(int p, int64_t step, float energy, uint16_t stabMask, uint16_t bassMask) const {
		const SigStyleDef& d = sigStyle(style);
		float w = d.w[p % 16];
		float dens = (0.2f + 1.6f * density) * (0.6f + 0.8f * energy);
		float pr = std::min(1.f, w * dens);
		if (w >= 0.99f && density > 0.1f) pr = 1.f;
		int b = (int)(step % 16);
		// ANSWER: step out of the way of the chords, into the gaps
		if (stabMask & (1 << b)) pr *= 1.f - 0.75f * answer;
		else if (answer > 0.3f && style != SS_ARP) pr = std::min(1.f, pr * (1.f + 0.6f * (answer - 0.3f)));
		if ((bassMask & (1 << b)) && style == SS_BLEEP) pr *= 1.f - 0.5f * answer;
		// CALL+ANSWER: play in the first bar of each two, answer (lightly) in the second
		if (style == SS_CALL && ((step / 16) & 1)) pr *= 0.15f + 0.3f * (1.f - answer);
		return pr;
	}

	NoteEvent tick(const Chart& c, int64_t step, float energy, uint16_t stabMask, uint16_t bassMask, float rangeCv) {
		NoteEvent e;
		const SigStyleDef& d = sigStyle(style);
		int L = motifLen(motifSel);
		int b = (int)(step % 16);
		if (b == 0) barMask = 0;
		pos = (int)(step % L);
		if (pos == 0 && step > 0) mutateOnce();
		int slot = c.slotAtStep(step);
		uint16_t ct = c.chordMask(slot);
		uint16_t sc = d.chordOnly ? ct : chordScale(c, slot);
		int centre = d.centre + 12 * registerOct;
		float rg = std::max(0.f, std::min(1.f, range + rangeCv));
		int span = 5 + (int)(rg * 26.f);                     // 5..31 semitones around the centre
		// at the start of the motif, and on chord changes, find the anchor again near the centre
		if (pos == 0 || c.isChange(step) || !started) { anchor = nearestIn(ct, centre); if (!started) lastNote = anchor; started = true; }
		const MotifStep& m = motif[pos];
		float pr = hitChance(pos, step, energy, stabMask, bassMask);
		if (m.uOn >= pr) return e;
		// the note: walk from the last note by this step's move, kept inside the range
		int from = (pos == 0) ? anchor : lastNote;
		if (!(sc & (1 << ((from % 12 + 12) % 12)))) from = nearestIn(sc, from);
		int note = stepInMask(sc, from, m.move);
		int lo = centre - span / 2, hi = centre + span / 2;
		while (note > hi) note = stepInMask(sc, note, -std::max(1, popcount16(sc)));
		while (note < lo) note = stepInMask(sc, note, std::max(1, popcount16(sc)));
		// on the beat (and on chord changes) ANSWER pulls it onto a chord tone
		bool strong = (b % 4) == 0 || c.isChange(step);
		if (strong && answer > 0.5f && !(ct & (1 << (note % 12)))) note = nearestIn(ct, note);
		// OUTSIDE: a chromatic neighbour or an avoid note now and then
		float outP = strong ? std::max(0.f, outside - 0.5f) : outside * 0.8f;
		if (m.uOut < outP) note += (m.uOut < outP * 0.5f) ? 1 : -1;
		if (style == SS_BLEEP && m.uVel < 0.25f) note += 12;
		e.hit = true; e.n = 1; e.note[0] = note;
		e.gateSteps = d.lenLo + (d.lenHi - d.lenLo) * m.uLen;
		if (style == SS_STRINGS) {
			int toNext = 1; while (toNext < 16 && motif[(pos + toNext) % L].uOn >= hitChance((pos + toNext) % L, step + toNext, energy, stabMask, bassMask)) toNext++;
			e.gateSteps = std::max(1.f, std::min((float)toNext, e.gateSteps * 2.f) - 0.1f);
		}
		e.vel[0] = 0.55f + 0.3f * m.uVel + ((b % 4) == 0 ? 0.15f : 0.f);
		e.vel[0] = std::min(1.f, e.vel[0]);
		e.mod = std::max(0.f, std::min(1.f, (note - lo) / (float)std::max(1, hi - lo)));
		lastNote = note;
		barMask |= 1 << b;
		return e;
	}
};

} // namespace bv
