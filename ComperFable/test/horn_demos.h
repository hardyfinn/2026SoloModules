// The demo phrases for the three horns (test/render_horn demo DIR). Pitches are concert MIDI notes.
static const char* HN[3] = {"tenor", "trumpet", "trombone"};
static Ev E(double beat, double len, double pitch, double vel, bool slur, double tongue, double scoop = 0, double fall = 0, double vib = 0, double growl = 0, double air = 0, bool gliss = false) {
	return {beat, len, pitch, vel, slur, tongue, scoop, fall, vib, growl, air, gliss};
}
static double swell(double t) { double x = (t - 0.3) / 6.0; if (x < 0) x = 0; if (x > 1) x = 1; return 0.08 + 0.92 * std::sin(M_PI * x); }
static void demos(const std::string& dir, int SR) {
	FILE* log = fopen((dir + "/notes.txt").c_str(), "w");
	std::vector<Phrase> all;
	const int MAJ[8] = {0, 2, 4, 5, 7, 9, 11, 12};
	for (int k = 0; k < 3; k++) {
		int root = k == 0 ? 46 : (k == 1 ? 53 : 41);              // Bb2, F3, F2
		// 1 range: a two-octave major scale up and down, light legato tongue
		{ Phrase ph; ph.name = std::string(HN[k]) + "-1-scale"; ph.kind = k; ph.bpm = 132; ph.swing = 0.5; ph.room = 0.5;
		  std::vector<int> ps; for (int o = 0; o < 2; o++) for (int i = 0; i < 7; i++) ps.push_back(root + 12 * o + MAJ[i]); ps.push_back(root + 24);
		  for (int i = (int)ps.size() - 2; i >= 0; i--) ps.push_back(ps[i]);
		  double b = 0; for (size_t i = 0; i < ps.size(); i++) { bool last = i + 1 == ps.size(); ph.ev.push_back(E(b, last ? 3.0 : 0.46, ps[i], 0.62, false, 0.35, 0, 0, last ? 0.6 : 0)); b += 0.5; }
		  all.push_back(ph); }
		// 2 dynamics: one long note from a whisper to full and back
		{ Phrase ph; ph.name = std::string(HN[k]) + "-2-swell"; ph.kind = k; ph.bpm = 60; ph.swing = 0.5; ph.room = 0.5; ph.breath = swell;
		  ph.ev.push_back(E(0, 6.0, k == 0 ? 58 : (k == 1 ? 65 : 53), 1.0, false, 0.2));
		  all.push_back(ph); }
		// 3 bebop: a ii-V-I line, swung, tongued off the beat and slurred onto it, last note held with vibrato and a fall
		{ Phrase ph; ph.name = std::string(HN[k]) + "-3-bebop"; ph.kind = k; ph.bpm = 168; ph.swing = 0.62; ph.room = 0.5;
		  int tr = k == 0 ? 0 : (k == 1 ? 5 : -7);
		  const int L[] = {63, 67, 70, 74, 72, 70, 69, 67,   75, 74, 73, 72, 69, 65, 67, 68,   69, 70, 62, 65, 67, 65, 62};
		  double b = 0; int n = sizeof L / sizeof L[0];
		  for (int i = 0; i < n; i++) { bool on = (i % 2) == 0; ph.ev.push_back(E(b, 0.5, L[i] + tr, on ? 0.50 : 0.66, on && i > 0, on ? 0.3 : 0.55)); b += 0.5; }
		  ph.ev.push_back(E(b, 3.5, 58 + tr, 0.6, true, 0.3, 0, 3.0, 0.8));
		  // and a second chorus, harder
		  b += 5.0;
		  const int L2[] = {70, 72, 74, 75, 77, 75, 74, 72,   70, 69, 67, 65, 63, 62, 60, 58,   57, 58, 62, 65, 69, 65, 62};
		  for (int i = 0; i < n; i++) { bool on = (i % 2) == 0; ph.ev.push_back(E(b, 0.5, L2[i] + tr, on ? 0.62 : 0.82, on && i > 0, on ? 0.35 : 0.7)); b += 0.5; }
		  ph.ev.push_back(E(b, 3.0, 70 + tr, 0.85, false, 0.8, 1.0, 0, 0.9));
		  all.push_back(ph); }
		// 4 blues: slow, scoops, a growl, a blue note, a long fall
		{ Phrase ph; ph.name = std::string(HN[k]) + "-4-blues"; ph.kind = k; ph.bpm = 76; ph.swing = 0.64; ph.room = 0.55;
		  int tr = k == 0 ? 0 : (k == 1 ? 5 : -7);
		  ph.ev.push_back(E(0.0, 1.4, 65 + tr, 0.75, false, 0.5, 2.0, 0, 0.7));
		  ph.ev.push_back(E(1.5, 0.5, 68 + tr, 0.70, false, 0.5));
		  ph.ev.push_back(E(2.0, 2.4, 70 + tr, 0.90, true, 0.5, 0, 0, 0.8, 0.8));
		  ph.ev.push_back(E(4.5, 0.5, 68 + tr, 0.65, false, 0.4));
		  ph.ev.push_back(E(5.0, 0.5, 65 + tr, 0.60, true, 0.4));
		  ph.ev.push_back(E(5.5, 0.5, 63 + tr, 0.65, false, 0.4));
		  ph.ev.push_back(E(6.0, 1.0, 61 + tr, 0.70, true, 0.4, 0, 0, 0.3, 0, 0, true));
		  ph.ev.push_back(E(7.0, 3.0, 58 + tr, 0.72, true, 0.4, 0, 5.0, 0.8, 0, 0, true));
		  ph.ev.push_back(E(11.0, 0.5, 58 + tr, 0.8, false, 0.9));
		  ph.ev.push_back(E(11.5, 0.5, 61 + tr, 0.8, false, 0.9));
		  ph.ev.push_back(E(12.0, 3.0, 65 + tr, 0.95, false, 0.9, 1.5, -4.0, 0.9, k == 0 ? 0.6 : 0.4));
		  all.push_back(ph); }
		// 5 ballad: soft and close, breathy, long tones
		{ Phrase ph; ph.name = std::string(HN[k]) + "-5-ballad"; ph.kind = k; ph.bpm = 58; ph.swing = 0.5; ph.room = 0.6;
		  int tr = k == 0 ? -12 : (k == 1 ? 0 : -12);
		  double air = k == 0 ? 0.7 : 0.35;
		  ph.ev.push_back(E(0.0, 1.9, 62 + tr, 0.34, false, 0.1, 0, 0, 0.5, 0, air));
		  ph.ev.push_back(E(2.0, 0.9, 65 + tr, 0.36, true, 0.1, 0, 0, 0.3, 0, air));
		  ph.ev.push_back(E(3.0, 0.9, 69 + tr, 0.40, true, 0.1, 0, 0, 0.3, 0, air));
		  ph.ev.push_back(E(4.0, 3.5, 72 + tr, 0.44, true, 0.1, 0, 0, 0.9, 0, air));
		  ph.ev.push_back(E(8.0, 0.9, 70 + tr, 0.36, false, 0.15, 0.7, 0, 0.3, 0, air));
		  ph.ev.push_back(E(9.0, 0.9, 67 + tr, 0.34, true, 0.1, 0, 0, 0.3, 0, air));
		  ph.ev.push_back(E(10.0, 4.5, 64 + tr, 0.32, true, 0.1, 0, 0, 1.0, 0, air));
		  all.push_back(ph); }
	}
	// trombone only: what the slide does
	{ Phrase ph; ph.name = "trombone-6-slide"; ph.kind = 2; ph.bpm = 96; ph.swing = 0.6; ph.room = 0.5;
	  ph.ev.push_back(E(0.0, 0.5, 46, 0.7, false, 0.6));
	  ph.ev.push_back(E(0.5, 1.5, 53, 0.8, true, 0.5, 0, 0, 0.5, 0, 0, true));
	  ph.ev.push_back(E(2.5, 0.5, 58, 0.75, false, 0.6));
	  ph.ev.push_back(E(3.0, 0.5, 56, 0.7, true, 0.5, 0, 0, 0, 0, 0, true));
	  ph.ev.push_back(E(3.5, 0.5, 58, 0.7, true, 0.5, 0, 0, 0, 0, 0, true));
	  ph.ev.push_back(E(4.0, 2.0, 62, 0.85, true, 0.5, 0, 6.0, 0.8, 0, 0, true));
	  ph.ev.push_back(E(7.0, 3.0, 53, 0.95, false, 0.8, 4.0, 0, 0.9, 0.7));
	  all.push_back(ph); }
	for (auto& ph : all) renderPhrase(ph, dir, SR, log);
	if (log) fclose(log);
	printf("demo: %d phrases\n", (int)all.size());
}
