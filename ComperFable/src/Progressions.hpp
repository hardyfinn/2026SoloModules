// The built-in progressions and the chart-text parser. The first 14 are the list in tools/proto/presets.py;
// the rest were added in 2.3.0.
#pragma once
#include "dsp/Engine.hpp"
#include <string>
#include <vector>
#include <sstream>

namespace comper {

struct Progression { const char* name; int beats; const char* text; };
static const Progression PROGRESSIONS[] = {
	{"ii-V-I major", 4, "Dm7 G7 Cmaj7 Cmaj7"},
	{"ii-V-i minor", 4, "Dm7b5 G7alt Cm6 Cm6"},
	{"Major then minor", 4, "Dm7 G7 Cmaj7 Cmaj7 Dm7b5 G7alt Cm6 Cm6"},
	{"Jazz blues in F", 2, "F7 F7 Bb7 Bb7 F7 F7 Cm7 F7 Bb7 Bb7 Bdim7 Bdim7 F7 F7 Am7 D7"},
	{"Blues (12 steps)", 4, "F7 Bb7 F7 F7 Bb7 Bdim7 F7 D7 Gm7 C7 F7 C7"},
	{"Falling fifths", 4, "Cm7 F7 Bbmaj7 Ebmaj7 Am7b5 D7alt Gm6 Gm6"},
	{"Turnaround I-vi-ii-V", 2, "Cmaj7 Am7 Dm7 G7"},
	{"Rhythm bridge", 4, "D7 D7 G7 G7 C7 C7 F7 F7"},
	{"Modal two-chord", 4, "Dm7 Dm7 Dm7 Dm7 Ebm7 Ebm7 Dm7 Dm7"},
	{"Bossa", 4, "Fmaj7 Fmaj7 G7 G7 Gm7 C7 Fmaj7 C7"},
	{"Ballad", 2, "Ebmaj7 Cm7 Fm7 Bb7 Gm7 C7alt Fm7 Bb7 Ebmaj7 Ebmaj7 Am7b5 D7alt Gm7 C7 Fm7 Bb7sus"},
	{"Soul vamp", 4, "Em7 A7 Em7 A7 Cmaj7 Bm7 Am7 B7alt"},
	{"House vamp", 4, "Am7 Am7 Fmaj7 Em7 Dm7 Dm7 Fmaj7 E7alt"},
	{"One chord", 4, "Dm7"},
	// ---- added in 2.3.0
	{"House: organ i-iv", 4, "Cm7 Cm7 Fm7 Fm7 Cm7 Cm7 Abmaj7 G7sus"},
	{"House: garage stairs", 4, "Fmaj7 Em7 Dm7 Cmaj7 Fmaj7 Em7 Dm7 G7sus"},
	{"House: deep minor stabs", 4, "Ebm7 Ebm7 Dbmaj7 Dbmaj7 Bmaj7 Bmaj7 Bb7sus Bb7alt"},
	{"House: minor two-chord", 4, "Fm7 Eb6 Fm7 Eb6"},
	{"House: deep major float", 4, "Ebmaj7 Ebmaj7 Abmaj7 Abmaj7"},
	{"House: piano lift", 4, "Am7 Fmaj7 Cmaj7 G7sus"},
	{"House: soulful loop", 4, "Am7 Dm7 G7sus Cmaj7"},
	{"House: organ bass vamp", 4, "Gm7 Gm7 C7sus C7sus"},
	{"House: garage chromatic slide", 4, "Ebmaj7 Dm7 Dbmaj7 Cm7"},
	{"House: deep four-step", 4, "Cm7 Fm7 Bb7sus Ebmaj7"},
	{"House: gospel house", 4, "Fmaj7 G7sus Em7 Am7"},
	{"Gospel: walk-up", 2, "Cmaj7 C7 Fmaj7 F#dim7 Cmaj7 A7alt Dm7 G7sus"},
	{"Gospel: turnaround", 2, "Em7 A7alt Dm7 G7alt Cmaj7 C7 Fmaj7 Fm6"},
	{"Blues: minor in C", 4, "Cm7 Cm7 Cm7 Cm7 Fm7 Fm7 Cm7 Cm7 Ab7 G7alt Cm7 G7alt"},
	{"Blues: shuffle in G", 4, "G7 C7 G7 G7 C7 C7 G7 G7 D7 C7 G7 D7"},
	{"Disco: Dorian pump", 4, "Am7 D7 Am7 D7 Am7 D7 Fmaj7 E7alt"},
	{"Disco: Philly strings", 4, "Fmaj7 Fmaj7 Em7 Am7 Dm7 Dm7 Gm7 C7sus"},
	{"Disco: circle", 4, "Dm7 G7 Cmaj7 Fmaj7 Bm7b5 E7alt Am7 Am7"},
	{"Disco: boogie rise", 4, "Dm7 Em7 Fmaj7 Em7 Dm7 Em7 Fmaj7 G7sus"},
	{"Soul: ballad", 4, "Cmaj7 Am7 Fmaj7 G7sus"},
	{"Soul: Motown walk-up", 2, "Fmaj7 Gm7 Am7 Bbmaj7 C7sus C7 Fmaj7 Fmaj7"},
	{"Soul: neo-soul turn", 4, "Fmaj7 Em7 A7alt Dm7 Gm7 C7sus Fmaj7 Fmaj7"},
	{"Soul: minor circle", 4, "Fm7 Bbm7 Ebmaj7 Abmaj7 Dbmaj7 Gm7b5 C7alt C7alt"},
	{"Funk: one chord", 4, "E7"},
	{"Funk: Dorian", 4, "Em7 Em7 Em7 A7"},
	{"Funk: I-IV", 4, "G7 G7 C7 C7 G7 G7 D7sus D7alt"},
	{"Reggae: one drop", 4, "Am Am D D"},
	{"Reggae: roots minor", 4, "Am G Am G"},
	{"Reggae: rocksteady", 4, "C C F G"},
	{"Reggae: lovers rock", 4, "Cmaj7 Am7 Fmaj7 G"},
};
static const int NUM_PROGRESSIONS = sizeof(PROGRESSIONS) / sizeof(PROGRESSIONS[0]);

static inline bool parseChord(const std::string& tok, Chord& out) {
	if (tok.empty()) return false;
	static const int N[7] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
	char c = tok[0];
	if (c >= 'a' && c <= 'g') c = c - 'a' + 'A';
	if (c < 'A' || c > 'G') return false;
	int root = N[c - 'A']; size_t i = 1;
	if (i < tok.size() && (tok[i] == '#' || tok[i] == 'b')) { root += tok[i] == '#' ? 1 : -1; i++; }
	std::string q = tok.substr(i);
	struct M { const char* s; int q; };
	static const M T[] = {{"maj7", MAJ}, {"M7", MAJ}, {"maj9", MAJ}, {"maj", TRI}, {"6/9", MAJ6}, {"69", MAJ6},
		{"m7b5", HDIM}, {"-7b5", HDIM}, {"mMaj7", MMAJ}, {"mM7", MMAJ}, {"m6", M6}, {"-6", M6}, {"m9", M7}, {"m11", M7},
		{"m7", M7}, {"-7", M7}, {"dim7", DIM}, {"dim", DIM}, {"o7", DIM}, {"o", DIM}, {"7alt", ALT}, {"alt", ALT},
		{"7b9", ALT}, {"7#9", ALT}, {"7#5", ALT}, {"7sus", SUS}, {"7sus4", SUS}, {"sus", SUS}, {"sus4", SUS}, {"13", DOM},
		{"9", DOM}, {"7", DOM}, {"6", MAJ6}, {"min", MIN}, {"m", MIN}, {"-", MIN}, {"", TRI}};   // 2.3.0: bare letters are triads
	for (auto& m : T) if (q == m.s) { out = Chord(pmod(root, 12), m.q); return true; }
	return false;
}

// "Dm7 G7 | Cmaj7 %" -> chords (max 16). On failure returns false and the bad token.
static inline bool parseChart(const std::string& text, std::vector<Chord>& out, std::string& bad) {
	out.clear();
	std::istringstream ss(text); std::string tok;
	while (ss >> tok) {
		if (tok == "|") continue;
		// allow bar lines glued to chords
		std::string t2;
		for (char ch : tok) if (ch != '|') t2.push_back(ch);
		if (t2.empty()) continue;
		if (t2 == "%") {
			if (out.empty()) { bad = t2; return false; }
			out.push_back(out.back());
		} else {
			Chord c;
			if (!parseChord(t2, c)) { bad = t2; return false; }
			out.push_back(c);
		}
		if (out.size() > 16) { bad = "more than 16 steps"; return false; }
	}
	if (out.empty()) { bad = "empty"; return false; }
	return true;
}

static inline std::string chordName(const Chord& c) { return std::string(NOTE_NAMES[c.root]) + Q_NAMES[c.q]; }
static inline std::string noteName(int m) { return std::string(NOTE_NAMES[pmod(m, 12)]) + std::to_string(m / 12 - 1); }

} // namespace comper
