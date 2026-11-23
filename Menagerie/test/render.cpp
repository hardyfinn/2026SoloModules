// Offline checks and demo for Menagerie (no Rack needed).
//   make test            -> prints level/CPU stats per engine and writes
//                           menagerie-demo.wav (all 16 engines, 8 s each)
// The demo plays in F minor at 124 BPM; each engine sweeps its SHAPE,
// TEXTURE and LIFE across its 8 seconds.
#include "../src/MenagerieDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>
#include <xmmintrin.h>
#include <pmmintrin.h>

using namespace mng;

static const float SR = 48000.f;

static float hz(float semisFromC4) { return 261.6256f * std::exp2(semisFromC4 / 12.f); }

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) { std::perror(path.c_str()); return; }
	uint32_t n = (uint32_t) L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32((uint32_t) SR); u32((uint32_t) SR * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::lrint(clampf(L[i], -1.f, 1.f) * 32000.f);
		int16_t b = (int16_t) std::lrint(clampf(R[i], -1.f, 1.f) * 32000.f);
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

// ------------------------------------------------------------------ stats
static void stats() {
	std::printf("%-10s %6s %6s %6s %6s %8s\n", "engine", "rmsMin", "rmsMax", "peak", "auxPk", "us/s/v");
	static Voice v;
	for (int m = 0; m < NUM_ENGINES; m++) {
		float rmin = 1e9f, rmax = 0.f, pk = 0.f, apk = 0.f;
		bool bad = false;
		double secs = 0.0;
		const float fs[3] = {55.f, 220.f, 880.f};
		const float vals[3] = {0.f, 0.5f, 1.f};
		for (int fi = 0; fi < 3; fi++)
			for (int si = 0; si < 3; si++)
				for (int ti = 0; ti < 3; ti++)
					for (int li = 0; li < 2; li++) {
						v = Voice();
						v.model = m; v.pending = m;
						v.seed(1234 + m * 7 + fi);
						Ctl c;
						c.setRate(SR);
						c.f = fs[fi]; c.shape = vals[si]; c.texture = vals[ti]; c.life = vals[li * 2]; c.decay = 0.5f;
						c.drone = false;
						int N = (int) (SR * 1.0f);
						double acc = 0.0;
						auto t0 = std::chrono::steady_clock::now();
						for (int i = 0; i < N; i++) {
							bool trig = (i % 12000) == 0;
							bool gate = (i % 12000) < 6000;
							float mm, aa;
							v.process(m, c, trig, gate, 1.f, mm, aa);
							if (!std::isfinite(mm) || !std::isfinite(aa)) bad = true;
							acc += mm * mm;
							pk = std::max(pk, std::fabs(mm));
							apk = std::max(apk, std::fabs(aa));
						}
						secs += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
						float rms = (float) std::sqrt(acc / N);
						rmin = std::min(rmin, rms); rmax = std::max(rmax, rms);
					}
		std::printf("%-10s %6.3f %6.3f %6.3f %6.3f %8.1f %s\n", ENGINE_NAMES[m], rmin, rmax, pk, apk,
			secs / 54.0 * 1e6, bad ? "NaN!" : "");
	}
}

// ------------------------------------------------------------------ demo
struct Note { float beat, len; float semis[3]; int n; };
struct Seg {
	int model; bool drone; int voices;
	float s0, s1, t0, t1, l0, l1, decay;
	std::vector<Note> notes;
};

static Note N1(float b, float len, float s) { Note n; n.beat = b; n.len = len; n.semis[0] = s; n.n = 1; return n; }
static Note N3(float b, float len, float a, float bb, float c) { Note n; n.beat = b; n.len = len; n.semis[0] = a; n.semis[1] = bb; n.semis[2] = c; n.n = 3; return n; }

// F minor, semitones from C4: F2=-19 C3=-12 Ab2=-16 Eb3=-9 F3=-7 G3=-5 Ab3=-4 Bb3=-2 C4=0 Eb4=3 F4=5 G4=7
static std::vector<Seg> program() {
	std::vector<Seg> P;
	const float BAR = 4.f;
	auto seg = [&](int m, bool drone, int voices, float s0, float s1, float t0, float t1, float l0, float l1, float d) {
		Seg s; s.model = m; s.drone = drone; s.voices = voices; s.s0 = s0; s.s1 = s1; s.t0 = t0; s.t1 = t1; s.l0 = l0; s.l1 = l1; s.decay = d;
		return s;
	};
	// 0 Gendy: drone, two notes
	{ Seg s = seg(GENDY, true, 1, 0.25f, 0.6f, 0.05f, 0.65f, 0.2f, 0.7f, 0.5f);
	  s.notes = {N1(0, 8, -19), N1(8, 8, -12)}; P.push_back(s); }
	// 1 Herd: drone, pull swept from loose chorus to lock and back out
	{ Seg s = seg(HERD, true, 1, 0.35f, 0.5f, 0.0f, 1.0f, 0.3f, 0.3f, 0.5f);
	  s.notes = {N1(0, 8, -7), N1(8, 8, -4)}; P.push_back(s); }
	// 2 Duffing: 8th-note bass, drive rising into chaos
	{ Seg s = seg(DUFFING, false, 1, 0.2f, 0.9f, 0.15f, 0.8f, 0.2f, 0.5f, 0.25f);
	  const float bl[8] = {-19, -19, -7, -19, -16, -19, -7, -12};
	  for (int i = 0; i < 32; i++) s.notes.push_back(N1(i * 0.5f, 0.3f, bl[i % 8]));
	  P.push_back(s); }
	// 3 Scanned: plucked quarter notes, rest shape morphing
	{ Seg s = seg(SCANNED, false, 1, 0.0f, 1.0f, 0.3f, 0.6f, 0.3f, 0.7f, 0.7f);
	  const float nl[4] = {-7, -4, 0, 3};
	  for (int i = 0; i < 16; i++) s.notes.push_back(N1(i, 0.8f, nl[i % 4]));
	  P.push_back(s); }
	// 4 Walsh: 8th-note lead, formant sweep
	{ Seg s = seg(WALSH, false, 1, 0.0f, 1.0f, 0.45f, 0.55f, 0.3f, 0.3f, 0.35f);
	  const float nl[8] = {0, 3, 5, 7, 5, 3, 0, -2};
	  for (int i = 0; i < 32; i++) s.notes.push_back(N1(i * 0.5f, 0.3f, nl[i % 8]));
	  P.push_back(s); }
	// 5 PLL: drone, changing note each bar, multiple and loop speed moving
	{ Seg s = seg(PLL, true, 1, 0.1f, 0.6f, 0.2f, 0.75f, 0.4f, 0.4f, 0.5f);
	  s.notes = {N1(0, 4, -19), N1(4, 4, -16), N1(8, 4, -12), N1(12, 4, -9)}; P.push_back(s); }
	// 6 Bytebeat: drone through all eight formulas
	{ Seg s = seg(BYTEBEAT, true, 1, 0.0f, 1.0f, 0.3f, 0.3f, 0.5f, 0.5f, 0.5f);
	  s.notes = {N1(0, 16, -7)}; P.push_back(s); }
	// 7 Automaton: rule 110, generations slowing down
	{ Seg s = seg(AUTOMATON, true, 1, 0.56f, 0.56f, 0.25f, 0.6f, 0.2f, 0.2f, 0.5f);
	  s.notes = {N1(0, 8, -19), N1(8, 8, -16)}; P.push_back(s); }
	// 8 Bubbles: a stream plus struck bubbles on the beat
	{ Seg s = seg(BUBBLES, false, 1, 0.4f, 0.6f, 0.3f, 0.85f, 0.45f, 0.65f, 0.5f);
	  const float nl[4] = {17, 20, 24, 15};
	  for (int i = 0; i < 16; i++) s.notes.push_back(N1(i, 0.2f, nl[i % 4]));
	  P.push_back(s); }
	// 9 Spring: quarter notes, dispersion growing into laser zaps
	{ Seg s = seg(SPRING, false, 1, 0.25f, 1.0f, 0.5f, 0.6f, 0.3f, 0.5f, 0.6f);
	  const float nl[4] = {-19, -12, -16, -9};
	  for (int i = 0; i < 16; i++) s.notes.push_back(N1(i, 0.5f, nl[i % 4]));
	  P.push_back(s); }
	// 10 Bounce: drops every two beats, ball changing from rubber to steel
	{ Seg s = seg(BOUNCE, false, 1, 0.1f, 0.45f, 0.15f, 0.9f, 0.3f, 0.3f, 0.55f);
	  for (int i = 0; i < 8; i++) s.notes.push_back(N1(i * 2.f, 0.2f, (i & 1) ? 12.f : 5.f));
	  P.push_back(s); }
	// 11 Howl: drone, gain crossing into feedback
	{ Seg s = seg(HOWL, true, 1, 0.1f, 0.6f, 0.32f, 0.6f, 0.4f, 0.4f, 0.5f);
	  s.notes = {N1(0, 8, -7), N1(8, 8, -4)}; P.push_back(s); }
	// 12 Tine: Fm9 / Dbmaj7 chords on the beat
	{ Seg s = seg(TINE, false, 3, 0.3f, 0.75f, 0.3f, 0.85f, 0.5f, 0.5f, 0.5f);
	  for (int i = 0; i < 16; i++) {
		if ((i / 4) % 2 == 0) s.notes.push_back(N3(i, 0.8f, -4, 0, 7));
		else s.notes.push_back(N3(i, 0.8f, -11, -7, 0));
	  }
	  P.push_back(s); }
	// 13 Tonewheel: stabs on the off-beat
	{ Seg s = seg(TONEWHEEL, false, 3, 0.25f, 0.6f, 0.4f, 0.4f, 0.6f, 0.6f, 0.2f);
	  for (int i = 0; i < 16; i++) {
		if (i % 4 == 3) s.notes.push_back(N3(i + 0.5f, 0.4f, -4, 0, 5));
		else s.notes.push_back(N3(i + 0.5f, 0.25f, -7, -4, 0));
	  }
	  P.push_back(s); }
	// 14 Pad: two-bar chords
	{ Seg s = seg(PAD, false, 3, 0.1f, 0.6f, 0.7f, 0.7f, 0.5f, 0.5f, 0.45f);
	  s.notes = {N3(0, 7.5f, -7, -4, 0), N3(8, 7.5f, -11, -7, -4)}; P.push_back(s); }
	// 15 Harmonium: held chords, stops opening up
	{ Seg s = seg(HARMONIUM, false, 3, 0.3f, 1.0f, 0.2f, 0.7f, 0.5f, 0.5f, 0.3f);
	  s.notes = {N3(0, 3.6f, -7, -4, 0), N3(4, 3.6f, -9, -5, -2), N3(8, 3.6f, -11, -7, -4), N3(12, 3.6f, -12, -9, -5)}; P.push_back(s); }
	(void) BAR;
	return P;
}

static void demo(const std::string& dir) {
	const float BPM = 124.f, spb = 60.f / BPM;
	const float SEG = 16.f * spb;    // 16 beats
	std::vector<Seg> P = program();
	std::vector<float> L, R;
	static Voice voices[3];
	for (size_t si = 0; si < P.size(); si++) {
		const Seg& s = P[si];
		for (int k = 0; k < 3; k++) {
			voices[k] = Voice();
			voices[k].model = voices[k].pending = s.model;
			voices[k].seed(777 + (uint32_t) si * 31 + k);
		}
		int N = (int) (SEG * SR);
		float tail = 0.25f;
		std::vector<float> sl(N), sr(N);
		for (int i = 0; i < N; i++) {
			float t = i / SR, beat = t / spb, x = (float) i / N;
			Ctl c;
			c.setRate(SR);
			c.shape = s.s0 + (s.s1 - s.s0) * x;
			c.texture = s.t0 + (s.t1 - s.t0) * x;
			c.life = s.l0 + (s.l1 - s.l0) * x;
			c.decay = s.decay;
			c.drone = s.drone;
			float L0 = 0.f, R0 = 0.f;
			for (int k = 0; k < s.voices; k++) {
				// current note for this voice
				const Note* cur = nullptr;
				for (const Note& n : s.notes) if (beat >= n.beat) cur = &n;
				float semi = cur ? cur->semis[std::min(k, cur->n - 1)] : 0.f;
				c.f = hz(semi);
				bool trig = false, gate = false;
				for (const Note& n : s.notes) {
					int start = (int) std::lrint(n.beat * spb * SR);
					if (i == start && k < n.n) trig = true;
					if (beat >= n.beat && beat < n.beat + n.len && k < n.n) gate = true;
				}
				float mm, aa;
				voices[k].process(s.model, c, trig, gate, 0.9f, mm, aa);
				L0 += mm; R0 += aa;
			}
			// engines whose AUX is a stereo partner go out in stereo; others mono MAIN
			bool stereo = s.model == BUBBLES || s.model == TINE || s.model == TONEWHEEL || s.model == PAD || s.model == HARMONIUM;
			float gain = s.voices > 1 ? 0.5f : 0.8f;
			sl[i] = L0 * gain;
			sr[i] = (stereo ? R0 : L0) * gain;
			float fade = std::min(1.f, std::min(t / 0.01f, (SEG - t) / tail));
			sl[i] *= fade; sr[i] *= fade;
		}
		double acc = 0.0;
		float pk = 0.f;
		for (int i = 0; i < N; i++) { acc += sl[i] * sl[i] + sr[i] * sr[i]; pk = std::max(pk, std::max(std::fabs(sl[i]), std::fabs(sr[i]))); }
		float rms = (float) std::sqrt(acc / (2.0 * N));
		// even out loudness between engines, keeping peaks under 0.95
		float g = std::min(0.2f / std::max(rms, 1e-4f), 0.95f / std::max(pk, 1e-4f));
		std::printf("demo %-10s rms %.3f peak %.3f gain %.2f\n", ENGINE_NAMES[s.model], rms, pk, g);
		for (int i = 0; i < N; i++) { L.push_back(sl[i] * g); R.push_back(sr[i] * g); }
		for (int i = 0; i < (int) (0.35f * SR); i++) { L.push_back(0.f); R.push_back(0.f); }
	}
	writeWav(dir + "/menagerie-demo.wav", L, R);
	std::printf("wrote %s/menagerie-demo.wav (%.1f s)\n", dir.c_str(), L.size() / SR);
}

int main(int argc, char** argv) {
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	std::string dir = argc > 1 ? argv[1] : ".";
	stats();
	demo(dir);
	return 0;
}
