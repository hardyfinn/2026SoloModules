#pragma once
// Preset gate patterns, one lane each, written on a 16th-note grid.
//   X = accented hit   x = hit   R / r = hit plus a 32nd-note double   - = rest
// 16-character patterns are one bar, 32-character patterns are two bars.
// These are common, widely shared figures (none is a transcription of a record).

namespace tiers {

struct Preset {
	const char* genre;
	const char* name;
	const char* steps;
};

static const Preset* presets(int& count) {
	static const Preset P[] = {
		// ---- house
		{"HOUSE", "Four to the floor kick", "X---X---X---X---"},
		{"HOUSE", "Offbeat open hat", "--X---X---X---X-"},
		{"HOUSE", "Clap on 2 and 4", "----X-------X---"},
		{"HOUSE", "16th hats, offbeat lift", "x-Xxx-Xxx-Xxx-Xx"},
		{"HOUSE", "Shuffle hat", "--xx--xx--xx--Xx"},
		{"HOUSE", "Chicago jack kick", "X---X---X-x-X---"},
		{"HOUSE", "Deep house clave", "X--x--x---x-x---"},
		{"HOUSE", "Shaker push", "x-xX-xx-x-xX-xxx"},
		{"HOUSE", "Organ stab push", "---X--x---X--x--"},
		{"HOUSE", "Ghost snare groove", "----X--x----X-xx"},
		{"HOUSE", "Clap + bar-2 pickup", "----X-------X-------X-------X-xx"},
		// ---- techno
		{"TECHNO", "Rumble kick", "X-xxX-xxX-xxX-xx"},
		{"TECHNO", "Detroit 16th hat", "xxXxxxXxxxXxxxXx"},
		{"TECHNO", "Rolling bass (no downbeat)", "-xxx-xXx-xxx-xXx"},
		{"TECHNO", "8th ride", "X-x-X-x-X-x-X-x-"},
		{"TECHNO", "Tresillo 3-3-2", "X--x--x-X--x--x-"},
		{"TECHNO", "3-3-3-3-4 stab", "X--x--x--x--x---"},
		{"TECHNO", "Electro kick", "X-----x---x-----"},
		{"TECHNO", "Dotted 8ths over 2 bars", "X--x--x--x--x--x--x--x--x--x--x-"},
		{"TECHNO", "Kick with bar-2 drop", "X---X---X---X---X---X---X-------"},
		{"TECHNO", "Clap on 4 only", "------------X---"},
		{"TECHNO", "Ratchet hat", "--X---R---X---Rr"},
		{"TECHNO", "Minimal perc", "---x--x----x-x--"},
		// ---- funk
		{"FUNK", "Breakbeat kick A", "X-x-------x--x--"},
		{"FUNK", "Breakbeat kick B", "X-x-------xx----"},
		{"FUNK", "Breakbeat snare + ghosts", "----X--x-x--X--x"},
		{"FUNK", "Funk 16th hat", "XxxxXxxxXxxxXxxx"},
		{"FUNK", "Bass on the one", "X------xx-x--x--"},
		{"FUNK", "Guitar scratch 16ths", "-x-xx-x--x-xx-x-"},
		{"FUNK", "Son clave 3-2", "X-----x-----x-------x---x-------"},
		{"FUNK", "Rumba clave 3-2", "X-----x-------x-----x---x-------"},
		{"FUNK", "Bossa clave", "X-----x-----x-------x-----x-----"},
		{"FUNK", "Cowbell funk", "X-x-x-X-x-xxx-x-"},
		// ---- disco
		{"DISCO", "Disco kick", "X---X---X---X---"},
		{"DISCO", "Disco hat, open offbeats", "x-X-x-X-x-X-x-X-"},
		{"DISCO", "Disco snare + pickup", "----X-------X-xx"},
		{"DISCO", "Tambourine 16ths", "xxxxXxxxxxxxXxxx"},
		{"DISCO", "Octave bass", "x-X-x-X-x-X-x-X-"},
		{"DISCO", "Philly string stab", "---X---X---X--X-"},
		{"DISCO", "Disco fill, bar 2", "X---X---X---X---X---X---XxxxXxXx"},
		{"DISCO", "Conga run", "---x-xx----x-xxX"},
	};
	count = (int) (sizeof(P) / sizeof(P[0]));
	return P;
}

} // namespace tiers
