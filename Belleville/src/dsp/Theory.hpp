// Belleville: the shared harmony toolkit (chord qualities, progressions, scales, the chart).
// Plain C++ with no Rack includes, so the test programs can use it too.
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <algorithm>

namespace bv {

// ---------- random numbers (small, fast, saved in patches as one number) ----------
struct Rng {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }          // 0..1
	int range(int n) { return n <= 1 ? 0 : (int)(uni() * n) % n; }      // 0..n-1
};

// ---------- chord qualities ----------
enum Qual {
	Q_MAJ, Q_MIN, Q_MAJ7, Q_MIN7, Q_DOM7, Q_MIN9, Q_MAJ9, Q_MIN11, Q_69, Q_SUS2, Q_SUS4, Q_7SUS4,
	Q_ADD9, Q_MIN6, Q_DOM9, Q_MADD9, Q_M7B5, Q_FIFTH, Q_MAJ7S11, Q_9SUS4, Q_MIN69, Q_DOM13, Q_COUNT
};
struct QualDef { const char* suffix; int n; int iv[6]; int simpler; int richer; bool minor; };
static inline const QualDef& qdef(int q) {
	static const QualDef T[Q_COUNT] = {
		{"",      3, {0, 4, 7},             Q_MAJ,   Q_ADD9,    false},  // MAJ
		{"m",     3, {0, 3, 7},             Q_MIN,   Q_MADD9,   true },  // MIN
		{"maj7",  4, {0, 4, 7, 11},         Q_MAJ,   Q_MAJ9,    false},  // MAJ7
		{"m7",    4, {0, 3, 7, 10},         Q_MIN,   Q_MIN9,    true },  // MIN7
		{"7",     4, {0, 4, 7, 10},         Q_MAJ,   Q_DOM9,    false},  // DOM7
		{"m9",    5, {0, 3, 7, 10, 14},     Q_MIN7,  Q_MIN11,   true },  // MIN9
		{"maj9",  5, {0, 4, 7, 11, 14},     Q_MAJ7,  Q_MAJ7S11, false},  // MAJ9
		{"m11",   6, {0, 3, 7, 10, 14, 17}, Q_MIN9,  Q_MIN11,   true },  // MIN11
		{"6/9",   5, {0, 4, 7, 9, 14},      Q_MAJ,   Q_69,      false},  // 69
		{"sus2",  3, {0, 2, 7},             Q_SUS2,  Q_9SUS4,   false},  // SUS2
		{"sus4",  3, {0, 5, 7},             Q_SUS4,  Q_7SUS4,   false},  // SUS4
		{"7sus4", 4, {0, 5, 7, 10},         Q_SUS4,  Q_9SUS4,   false},  // 7SUS4
		{"add9",  4, {0, 4, 7, 14},         Q_MAJ,   Q_69,      false},  // ADD9
		{"m6",    4, {0, 3, 7, 9},          Q_MIN,   Q_MIN69,   true },  // MIN6
		{"9",     5, {0, 4, 7, 10, 14},     Q_DOM7,  Q_DOM13,   false},  // DOM9
		{"m(add9)",4,{0, 3, 7, 14},         Q_MIN,   Q_MIN9,    true },  // MADD9
		{"m7b5",  4, {0, 3, 6, 10},         Q_M7B5,  Q_M7B5,    true },  // M7B5
		{"5",     3, {0, 7, 12},            Q_FIFTH, Q_SUS2,    false},  // FIFTH
		{"maj7#11",5,{0, 4, 7, 11, 18},     Q_MAJ7,  Q_MAJ7S11, false},  // MAJ7S11
		{"9sus4", 5, {0, 5, 7, 10, 14},     Q_7SUS4, Q_9SUS4,   false},  // 9SUS4
		{"m6/9",  5, {0, 3, 7, 9, 14},      Q_MIN6,  Q_MIN69,   true },  // MIN69
		{"13",    5, {0, 4, 10, 14, 21},    Q_DOM9,  Q_DOM13,   false},  // DOM13
	};
	return T[std::max(0, std::min(Q_COUNT - 1, q))];
}
// COLOUR 0..1: plainer chords to the left, richer ones to the right, as written at noon
static inline int colourQual(int q, float colour) {
	if (colour < 0.2f) return qdef(qdef(q).simpler).simpler;
	if (colour < 0.4f) return qdef(q).simpler;
	if (colour < 0.65f) return q;
	if (colour < 0.85f) return qdef(q).richer;
	return qdef(qdef(q).richer).richer;
}
static inline uint16_t qualMask(int root, int q) {
	const QualDef& d = qdef(q);
	uint16_t m = 0;
	for (int i = 0; i < d.n; i++) m |= 1 << (((root + d.iv[i]) % 12 + 12) % 12);
	return m;
}
static inline const char* pcName(int pc) {
	static const char* N[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
	return N[((pc % 12) + 12) % 12];
}
static inline std::string chordName(int root, int q) { return std::string(pcName(root)) + qdef(q).suffix; }

// ---------- modes ----------
enum Mode { M_AUTO, M_AEOLIAN, M_DORIAN, M_PHRYGIAN, M_IONIAN, M_MIXOLYDIAN, M_LYDIAN, M_COUNT };
static inline const char* modeName(int m) {
	static const char* N[M_COUNT] = {"auto", "minor", "dorian", "phrygian", "major", "mixolydian", "lydian"};
	return N[std::max(0, std::min(M_COUNT - 1, m))];
}
static inline uint16_t modeMask(int key, int mode) {
	static const int S[M_COUNT][7] = {
		{0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
		{0, 2, 4, 5, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 6, 7, 9, 11}};
	uint16_t m = 0;
	for (int i = 0; i < 7; i++) m |= 1 << ((key + S[std::max(0, std::min(M_COUNT - 1, mode))][i]) % 12);
	return m;
}

// ---------- styles and progressions ----------
enum Style { ST_DETROIT, ST_CHICAGO, ST_DEEP, ST_GARAGE, ST_DUB, ST_WAREHOUSE, ST_ACID, ST_MINIMAL,
             ST_ELECTRO, ST_AMBIENT, ST_IDM, ST_PIANO, ST_DISCO, ST_PROGRESSIVE, ST_MELODIC, ST_SOULFUL, ST_COUNT };
static inline const char* styleName(int s) {
	static const char* N[ST_COUNT] = {"DETROIT", "CHICAGO", "DEEP", "GARAGE", "DUB", "WAREHOUSE", "ACID", "MINIMAL",
		"ELECTRO", "AMBIENT", "IDM", "PIANO", "DISCO", "PROGRESSIVE", "MELODIC", "SOULFUL"};
	return N[std::max(0, std::min(ST_COUNT - 1, s))];
}
struct ProgDef { int style; const char* name; int mode; int n; int deg[8]; int q[8]; int beats[8]; };
// degrees are semitones above the key; beats are quarter notes (4 = one bar)
static inline const ProgDef* progTable(int* count) {
	static const ProgDef P[] = {
		{ST_DETROIT, "Belleville", M_AEOLIAN, 4, {0, 8, 10, 5}, {Q_MIN9, Q_MAJ7, Q_69, Q_MIN9}, {4, 4, 4, 4}},
		{ST_DETROIT, "Night Drive", M_AEOLIAN, 2, {0, 3}, {Q_MIN11, Q_MAJ7}, {8, 8}},
		{ST_DETROIT, "Strings", M_AEOLIAN, 4, {0, 10, 8, 7}, {Q_MIN9, Q_69, Q_MAJ7, Q_7SUS4}, {4, 4, 4, 4}},
		{ST_DETROIT, "Hi-Tech", M_DORIAN, 4, {0, 3, 5, 3}, {Q_MIN7, Q_MAJ7, Q_MIN9, Q_MAJ7}, {4, 4, 4, 4}},
		{ST_DETROIT, "Planing", M_DORIAN, 4, {0, 2, 3, 5}, {Q_MIN9, Q_MIN9, Q_MIN9, Q_MIN9}, {4, 4, 4, 4}},
		{ST_DETROIT, "Transmat", M_DORIAN, 2, {0, 5}, {Q_MIN11, Q_MIN11}, {8, 8}},
		{ST_CHICAGO, "Gospel", M_IONIAN, 4, {5, 4, 2, 7}, {Q_MAJ7, Q_MIN7, Q_MIN7, Q_9SUS4}, {4, 4, 4, 4}},
		{ST_CHICAGO, "Jack", M_DORIAN, 2, {0, 5}, {Q_MIN7, Q_DOM9}, {8, 8}},
		{ST_CHICAGO, "Aeolian Walk", M_AEOLIAN, 4, {0, 8, 10, 0}, {Q_MIN, Q_MAJ, Q_MAJ, Q_MIN}, {4, 4, 4, 4}},
		{ST_CHICAGO, "Rain", M_IONIAN, 4, {9, 5, 0, 7}, {Q_MIN9, Q_MAJ7, Q_ADD9, Q_9SUS4}, {4, 4, 4, 4}},
		{ST_CHICAGO, "Testament", M_AEOLIAN, 6, {0, 10, 8, 5, 7, 0}, {Q_MIN9, Q_69, Q_MAJ7, Q_MIN7, Q_7SUS4, Q_MIN9}, {2, 2, 4, 2, 2, 4}},
		{ST_CHICAGO, "Piano Turn", M_IONIAN, 4, {0, 9, 2, 7}, {Q_MAJ7, Q_MIN7, Q_MIN7, Q_DOM7}, {4, 4, 4, 4}},
		{ST_DEEP, "Mystery", M_DORIAN, 2, {0, 5}, {Q_MIN9, Q_MIN9}, {8, 8}},
		{ST_DEEP, "Can You", M_AEOLIAN, 4, {0, 8, 3, 10}, {Q_MIN9, Q_MAJ9, Q_MAJ9, Q_69}, {4, 4, 4, 4}},
		{ST_DEEP, "East Side", M_IONIAN, 4, {0, 2, 4, 2}, {Q_MAJ9, Q_MIN9, Q_MIN7, Q_MIN9}, {4, 4, 4, 4}},
		{ST_DEEP, "After Hours", M_DORIAN, 4, {0, 3, 5, 7}, {Q_MIN11, Q_MAJ7S11, Q_MIN9, Q_MIN7}, {4, 4, 4, 4}},
		{ST_DEEP, "Sunrise", M_IONIAN, 4, {5, 7, 4, 9}, {Q_MAJ9, Q_9SUS4, Q_MIN7, Q_MIN9}, {4, 4, 4, 4}},
		{ST_GARAGE, "Uptown", M_IONIAN, 4, {5, 4, 2, 7}, {Q_MAJ9, Q_MIN7, Q_MIN9, Q_DOM9}, {4, 4, 4, 4}},
		{ST_GARAGE, "Paradise", M_DORIAN, 4, {0, 5, 10, 3}, {Q_MIN9, Q_DOM9, Q_MAJ9, Q_MAJ9}, {4, 4, 4, 4}},
		{ST_GARAGE, "Turnaround", M_IONIAN, 4, {2, 7, 4, 9}, {Q_MIN7, Q_DOM9, Q_MIN7, Q_DOM7}, {4, 4, 4, 4}},
		{ST_GARAGE, "Organ Vamp", M_DORIAN, 2, {0, 5}, {Q_MIN7, Q_DOM13}, {4, 4}},
		{ST_DUB, "One Chord", M_DORIAN, 1, {0}, {Q_MIN7}, {16}},
		{ST_DUB, "Step Down", M_DORIAN, 2, {0, 10}, {Q_MIN7, Q_MIN7}, {8, 8}},
		{ST_DUB, "Echo Chamber", M_DORIAN, 2, {0, 5}, {Q_MIN11, Q_MIN11}, {12, 4}},
		{ST_DUB, "Deep Water", M_AEOLIAN, 2, {0, 8}, {Q_MADD9, Q_MAJ7}, {8, 8}},
		{ST_WAREHOUSE, "Stab Up", M_AEOLIAN, 4, {0, 5, 3, 10}, {Q_MIN, Q_MIN, Q_MIN, Q_MIN}, {4, 4, 4, 4}},
		{ST_WAREHOUSE, "Hoover", M_AEOLIAN, 5, {0, 10, 8, 7, 5}, {Q_MIN7, Q_MIN7, Q_MIN7, Q_MIN7, Q_MIN7}, {4, 2, 2, 4, 4}},
		{ST_WAREHOUSE, "Chord Memory", M_DORIAN, 4, {0, 7, 5, 3}, {Q_MIN9, Q_MIN9, Q_MIN9, Q_MIN9}, {4, 4, 4, 4}},
		{ST_WAREHOUSE, "Fifths", M_PHRYGIAN, 3, {0, 3, 5}, {Q_FIFTH, Q_FIFTH, Q_FIFTH}, {8, 4, 4}},
		{ST_ACID, "Static", M_PHRYGIAN, 1, {0}, {Q_MIN}, {16}},
		{ST_ACID, "Two Up", M_DORIAN, 2, {0, 10}, {Q_7SUS4, Q_SUS2}, {8, 8}},
		{ST_ACID, "Phrygian", M_PHRYGIAN, 2, {0, 1}, {Q_MIN, Q_MAJ}, {8, 8}},
		{ST_MINIMAL, "Hold", M_DORIAN, 1, {0}, {Q_MIN9}, {16}},
		{ST_MINIMAL, "Breath", M_DORIAN, 2, {0, 0}, {Q_MIN7, Q_MIN9}, {8, 8}},
		{ST_MINIMAL, "Shift", M_DORIAN, 2, {0, 5}, {Q_SUS2, Q_SUS2}, {12, 4}},
		{ST_MINIMAL, "Pulse", M_AEOLIAN, 2, {0, 10}, {Q_MIN11, Q_69}, {12, 4}},
		// --- more of the classic styles
		{ST_DETROIT, "Galaxy", M_AEOLIAN, 4, {0, 7, 8, 10}, {Q_MIN9, Q_MIN7, Q_MAJ9, Q_69}, {4, 4, 4, 4}},
		{ST_DETROIT, "Sunday", M_AEOLIAN, 4, {0, 8, 3, 10}, {Q_MIN11, Q_MAJ7S11, Q_MAJ9, Q_9SUS4}, {4, 4, 4, 4}},
		{ST_DETROIT, "Quick Strings", M_DORIAN, 4, {0, 3, 5, 7}, {Q_MIN9, Q_MAJ7, Q_MIN9, Q_MIN7}, {2, 2, 2, 2}},
		{ST_DETROIT, "Moodswing", M_DORIAN, 2, {0, 10}, {Q_MIN7, Q_69}, {8, 8}},
		{ST_CHICAGO, "Trax", M_DORIAN, 3, {0, 3, 5}, {Q_MIN7, Q_MAJ7, Q_DOM9}, {8, 4, 4}},
		{ST_CHICAGO, "Warehouse Piano", M_AEOLIAN, 4, {0, 5, 10, 3}, {Q_MIN, Q_MIN, Q_MAJ, Q_MAJ}, {4, 4, 4, 4}},
		{ST_CHICAGO, "Diva", M_IONIAN, 4, {9, 2, 7, 0}, {Q_MIN7, Q_MIN7, Q_DOM9, Q_MAJ7}, {4, 4, 4, 4}},
		{ST_DEEP, "Lonely Hours", M_AEOLIAN, 4, {0, 10, 5, 3}, {Q_MIN9, Q_69, Q_MIN11, Q_MAJ9}, {4, 4, 4, 4}},
		{ST_DEEP, "Moonlight", M_IONIAN, 2, {2, 7}, {Q_MIN9, Q_DOM13}, {8, 8}},
		{ST_DEEP, "Afterglow", M_IONIAN, 4, {0, 5, 9, 7}, {Q_MAJ9, Q_MAJ9, Q_MIN9, Q_9SUS4}, {4, 4, 4, 4}},
		{ST_GARAGE, "Sunday Gospel", M_IONIAN, 4, {5, 7, 4, 9}, {Q_MAJ7, Q_DOM9, Q_MIN7, Q_MIN7}, {4, 4, 4, 4}},
		{ST_GARAGE, "Shelter", M_DORIAN, 4, {0, 3, 10, 5}, {Q_MIN9, Q_MAJ9, Q_69, Q_DOM9}, {4, 4, 4, 4}},
		{ST_DUB, "Slow Tide", M_AEOLIAN, 2, {0, 3}, {Q_MIN9, Q_MAJ7}, {8, 8}},
		{ST_DUB, "Basement", M_DORIAN, 2, {0, 7}, {Q_MIN7, Q_MIN7}, {12, 4}},
		{ST_WAREHOUSE, "Rave Signal", M_AEOLIAN, 4, {0, 0, 8, 10}, {Q_MIN, Q_MIN, Q_MAJ, Q_MAJ}, {2, 2, 2, 2}},
		{ST_WAREHOUSE, "Hardcore", M_AEOLIAN, 4, {0, 3, 5, 8}, {Q_MIN7, Q_MIN7, Q_MIN7, Q_MIN7}, {4, 4, 4, 4}},
		{ST_ACID, "Squelch Up", M_PHRYGIAN, 2, {0, 3}, {Q_FIFTH, Q_FIFTH}, {12, 4}},
		{ST_ACID, "Tritone", M_PHRYGIAN, 2, {0, 6}, {Q_MIN, Q_MIN}, {8, 8}},
		{ST_MINIMAL, "Two Tone", M_DORIAN, 2, {0, 0}, {Q_SUS4, Q_SUS2}, {8, 8}},
		{ST_MINIMAL, "Glacier", M_DORIAN, 1, {0}, {Q_MIN11}, {16}},
		// --- ELECTRO: cold minor, phrygian turns, bare fifths
		{ST_ELECTRO, "Aquabahn", M_PHRYGIAN, 2, {0, 1}, {Q_MIN, Q_MAJ}, {8, 8}},
		{ST_ELECTRO, "Clear Line", M_AEOLIAN, 4, {0, 3, 10, 5}, {Q_MIN7, Q_MAJ, Q_MAJ, Q_MIN7}, {4, 4, 4, 4}},
		{ST_ELECTRO, "Machine", M_AEOLIAN, 4, {0, 10, 8, 7}, {Q_FIFTH, Q_FIFTH, Q_FIFTH, Q_FIFTH}, {4, 4, 4, 4}},
		{ST_ELECTRO, "Planet", M_AEOLIAN, 3, {0, 5, 7}, {Q_MIN, Q_MIN, Q_MIN}, {8, 4, 4}},
		{ST_ELECTRO, "Deep Sea", M_PHRYGIAN, 4, {0, 1, 0, 10}, {Q_MIN7, Q_MAJ7, Q_MIN7, Q_MIN7}, {4, 4, 4, 4}},
		// --- AMBIENT techno: lydian colour, slow, open
		{ST_AMBIENT, "Polygon", M_LYDIAN, 2, {0, 7}, {Q_MAJ7S11, Q_69}, {8, 8}},
		{ST_AMBIENT, "Bytes", M_DORIAN, 4, {0, 2, 3, 10}, {Q_MIN9, Q_SUS2, Q_MAJ7S11, Q_69}, {4, 4, 4, 4}},
		{ST_AMBIENT, "Drift Glass", M_IONIAN, 2, {0, 4}, {Q_MAJ9, Q_MIN9}, {8, 8}},
		{ST_AMBIENT, "Lunar", M_AEOLIAN, 2, {0, 8}, {Q_MIN11, Q_MAJ7S11}, {8, 8}},
		{ST_AMBIENT, "Ice", M_DORIAN, 4, {0, 5, 10, 3}, {Q_SUS2, Q_SUS2, Q_SUS2, Q_SUS2}, {4, 4, 4, 4}},
		// --- IDM: odd chord lengths that slide against the bar, cluster colours
		{ST_IDM, "Odd Hours", M_DORIAN, 4, {0, 10, 8, 5}, {Q_MIN7, Q_69, Q_MAJ7, Q_MIN9}, {3, 3, 3, 7}},
		{ST_IDM, "Folded", M_AEOLIAN, 3, {0, 7, 3}, {Q_MIN9, Q_9SUS4, Q_MAJ7S11}, {5, 5, 6}},
		{ST_IDM, "Cluster Ballad", M_LYDIAN, 4, {0, 11, 9, 4}, {Q_MAJ7S11, Q_MIN7, Q_MAJ9, Q_MIN11}, {4, 4, 4, 4}},
		{ST_IDM, "Tri Fold", M_AEOLIAN, 3, {0, 6, 3}, {Q_MIN11, Q_MAJ7, Q_MIN9}, {6, 5, 5}},
		{ST_IDM, "Half Light", M_IONIAN, 2, {0, 4}, {Q_MAJ7, Q_MIN7}, {7, 9}},
		// --- PIANO house: big major anthems
		{ST_PIANO, "Anthem", M_IONIAN, 4, {0, 7, 9, 5}, {Q_MAJ, Q_MAJ, Q_MIN, Q_MAJ}, {4, 4, 4, 4}},
		{ST_PIANO, "Hands Up", M_IONIAN, 4, {9, 5, 0, 7}, {Q_MIN7, Q_MAJ7, Q_MAJ, Q_MAJ}, {4, 4, 4, 4}},
		{ST_PIANO, "Italo", M_IONIAN, 4, {0, 4, 5, 7}, {Q_MAJ, Q_MIN, Q_MAJ, Q_DOM7}, {4, 4, 4, 4}},
		{ST_PIANO, "Gospel Lift", M_IONIAN, 4, {5, 7, 4, 9}, {Q_MAJ, Q_MAJ, Q_MIN7, Q_MIN}, {4, 4, 4, 4}},
		{ST_PIANO, "Minor Anthem", M_AEOLIAN, 4, {0, 8, 3, 10}, {Q_MIN, Q_MAJ, Q_MAJ, Q_MAJ}, {4, 4, 4, 4}},
		// --- DISCO / filter house: ii-V loops, dominant ninths
		{ST_DISCO, "Filter", M_MIXOLYDIAN, 2, {2, 7}, {Q_MIN7, Q_DOM9}, {8, 8}},
		{ST_DISCO, "Funk Loop", M_DORIAN, 2, {0, 5}, {Q_MIN7, Q_DOM9}, {4, 4}},
		{ST_DISCO, "Strings Up", M_IONIAN, 4, {5, 4, 2, 0}, {Q_MAJ7, Q_MIN7, Q_MIN7, Q_MAJ7}, {4, 4, 4, 4}},
		{ST_DISCO, "Boogie", M_IONIAN, 4, {9, 2, 7, 0}, {Q_MIN9, Q_DOM9, Q_MAJ7, Q_MAJ7}, {4, 4, 4, 4}},
		{ST_DISCO, "Loft", M_DORIAN, 4, {0, 5, 3, 10}, {Q_MIN9, Q_DOM9, Q_MAJ7, Q_DOM13}, {4, 4, 4, 4}},
		// --- PROGRESSIVE: long chords, eight-bar journeys
		{ST_PROGRESSIVE, "Journey", M_AEOLIAN, 4, {0, 8, 3, 10}, {Q_MIN, Q_MAJ, Q_MAJ, Q_MAJ}, {8, 8, 8, 8}},
		{ST_PROGRESSIVE, "Northern", M_AEOLIAN, 4, {0, 10, 8, 7}, {Q_MADD9, Q_SUS2, Q_MAJ7, Q_7SUS4}, {8, 8, 8, 8}},
		{ST_PROGRESSIVE, "Uplift", M_AEOLIAN, 3, {8, 10, 0}, {Q_MAJ, Q_MAJ, Q_MIN}, {4, 4, 8}},
		{ST_PROGRESSIVE, "Horizon", M_DORIAN, 4, {0, 5, 3, 10}, {Q_MIN9, Q_SUS2, Q_MAJ9, Q_SUS4}, {8, 8, 8, 8}},
		// --- MELODIC techno: dark minor, one-note tension
		{ST_MELODIC, "Nocturne", M_AEOLIAN, 4, {0, 8, 3, 10}, {Q_MIN, Q_MAJ, Q_MAJ, Q_SUS2}, {8, 8, 8, 8}},
		{ST_MELODIC, "Tension", M_PHRYGIAN, 4, {0, 1, 0, 10}, {Q_MIN, Q_MAJ, Q_MIN, Q_MAJ}, {4, 4, 4, 4}},
		{ST_MELODIC, "Arp Rise", M_AEOLIAN, 3, {0, 5, 7}, {Q_MADD9, Q_MIN, Q_SUS4}, {8, 4, 4}},
		{ST_MELODIC, "Cold Light", M_AEOLIAN, 2, {0, 8}, {Q_MADD9, Q_MAJ7}, {16, 16}},
		// --- SOULFUL house: Rhodes ii-V-I and gospel turns
		{ST_SOULFUL, "Rhodes", M_IONIAN, 5, {2, 7, 0, 4, 9}, {Q_MIN9, Q_DOM13, Q_MAJ9, Q_MIN7, Q_DOM7}, {4, 4, 4, 2, 2}},
		{ST_SOULFUL, "Back Room", M_DORIAN, 5, {0, 5, 10, 9, 2}, {Q_MIN9, Q_DOM9, Q_MAJ9, Q_M7B5, Q_DOM7}, {4, 4, 4, 2, 2}},
		{ST_SOULFUL, "Higher", M_IONIAN, 4, {5, 7, 9, 4}, {Q_MAJ9, Q_9SUS4, Q_MIN9, Q_MIN7}, {4, 4, 4, 4}},
		{ST_SOULFUL, "Testify", M_IONIAN, 4, {0, 9, 2, 7}, {Q_MAJ9, Q_MIN9, Q_MIN9, Q_DOM13}, {4, 4, 4, 4}},
	};
	*count = (int)(sizeof(P) / sizeof(P[0]));
	return P;
}
static inline int progCount() { int n; progTable(&n); return n; }
static inline const ProgDef& progDef(int i) { int n; const ProgDef* P = progTable(&n); return P[((i % n) + n) % n]; }
// the i-th progression of a style (wraps)
static inline int progIndex(int style, int i) {
	int ids[16], k = 0, N = progCount();
	for (int j = 0; j < N && k < 16; j++) if (progDef(j).style == style) ids[k++] = j;
	if (k == 0) return 0;
	return ids[((i % k) + k) % k];
}
static inline int progsInStyle(int style) {
	int k = 0, N = progCount();
	for (int j = 0; j < N; j++) if (progDef(j).style == style) k++;
	return k;
}

// ---------- the chart everybody reads ----------
struct Chart {
	int loopSteps = 64;            // sixteenth notes in the loop
	int n = 1;
	int start[32] = {}, len[32] = {}, root[32] = {}, qual[32] = {};
	int key = 0;
	uint16_t scale = 0xAD5;        // pitch classes of the key's mode
	int slotAt(int stepInLoop) const {
		int s = ((stepInLoop % loopSteps) + loopSteps) % loopSteps;
		for (int i = n - 1; i >= 0; i--) if (s >= start[i]) return i;
		return 0;
	}
	int slotAtStep(int64_t step) const { return slotAt((int)(step % loopSteps)); }
	// steps from this step until the next chord change (1 = the change is on the next step)
	int stepsToChange(int64_t step) const {
		int s = (int)(((step % loopSteps) + loopSteps) % loopSteps);
		int i = slotAt(s);
		return start[i] + len[i] - s;
	}
	int nextSlot(int64_t step) const { return (slotAtStep(step) + 1) % n; }
	bool isChange(int64_t step) const {
		int s = (int)(((step % loopSteps) + loopSteps) % loopSteps);
		for (int i = 0; i < n; i++) if (start[i] == s) return n > 1 || s == 0;
		return false;
	}
	uint16_t chordMask(int i) const { return qualMask(root[i], qual[i]); }
};

// the notes worth playing over a chord: its tones plus the key's scale, without the notes a semitone above a chord tone
static inline uint16_t chordScale(const Chart& c, int slot, bool keepAvoid = false) {
	uint16_t ct = c.chordMask(slot), m = ct;
	for (int pc = 0; pc < 12; pc++) {
		if (!(c.scale & (1 << pc)) || (ct & (1 << pc))) continue;
		bool avoid = ct & (1 << ((pc + 11) % 12));
		if (!avoid || keepAvoid) m |= 1 << pc;
	}
	return m;
}
// the nearest MIDI note at or around `near` whose pitch class is in mask
static inline int nearestIn(uint16_t mask, int near) {
	if (!mask) return near;
	for (int d = 0; d < 12; d++) {
		if (mask & (1 << (((near - d) % 12 + 12) % 12))) return near - d;
		if (mask & (1 << (((near + d) % 12 + 12) % 12))) return near + d;
	}
	return near;
}
// move `steps` notes of the mask up (or down) from a note that is in the mask
static inline int stepInMask(uint16_t mask, int note, int steps) {
	if (!mask) return note + steps;
	int dir = steps >= 0 ? 1 : -1;
	for (int k = 0; k < std::abs(steps); k++) {
		do { note += dir; } while (!(mask & (1 << ((note % 12 + 12) % 12))));
	}
	return note;
}
static inline float midiToV(float n) { return (n - 60.f) / 12.f; }
static inline int popcount16(uint16_t m) { int c = 0; while (m) { c += m & 1; m >>= 1; } return c; }

} // namespace bv
