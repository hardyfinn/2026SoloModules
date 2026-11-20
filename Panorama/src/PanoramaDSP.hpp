// Panorama: steel pan engine (no Rack dependency, so test/render.cpp can use it).
//
// Every note area of the chosen pan exists all the time as a small bank of
// modal resonators (complex one-pole filters):
//   fundamental, octave, twelfth (the "compound fifth" tuners also tune),
//   a quiet double octave, and two short untuned "ping" modes.
// A strike is a rubber-tipped stick: a half-sine force pulse whose length
// sets how bright the hit is. Nothing is a sample or a voice: hitting a
// note that is still ringing just adds to it, and nothing gets stolen.
//
// What makes it sound like a pan rather than a bell:
//   NEIGHBOURS  each note area is fed a little of the areas around it on the
//               drum face. Pans are laid out so neighbours share partials
//               (fourths and fifths), so they sing along faintly.
//   BLOOM       the fundamental drives the octave through a squared term
//               (and fundamental x octave drives the twelfth), so the
//               octave swells just after the hit, as on a real pan.
//   GLIDE       pitch depends on how hard the area is moving: the note
//               starts a few cents off and settles.
//   Imperfect tuning: every note's fundamental, octave and twelfth have their
//               own fixed random error (re-rollable), scaled by DETUNE and
//               PARTIALS, plus hand edits per note.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace panorama {

static const int MAX_NOTES = 32;
static const int NUM_MODES = 6;  // fund, oct, twelfth, double oct, ping a, ping b
static const int NUM_NB = 6;
static const int MAX_PULSES = 48;
static const int NUM_TYPES = 3;
static const int NUM_BODY = 4;
static const float TWO_PI_F = 6.28318530718f;

enum PanType { TENOR = 0, DOUBLE_SECOND = 1, CELLO = 2 };
static const char* const TYPE_NAMES[NUM_TYPES] = {"TENOR", "DOUBLE SECOND", "TRIPLE CELLO"};
inline const char* pcName(int pc) {
	static const char* const n[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
	return n[((pc % 12) + 12) % 12];
}

// ---- small deterministic random (per-note tuning errors) ----
struct Rng {
	uint32_t s;
	explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
	uint32_t next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float gauss() {  // sum of 4 uniforms, roughly normal, clipped at +-2.4
		float g = (uni() + uni() + uni() + uni() - 2.f) * 1.732f;
		return std::max(-2.4f, std::min(2.4f, g));
	}
};

// ---- the three pans ----
struct TypeSpec {
	int lowMidi, highMidi;   // note range
	int numPans;
	int pcOrder[3][6];       // pitch classes on each pan, in order round the rim
	int pcCount;             // pitch classes per pan
	float t60Low;            // fundamental decay at the lowest note (s)
	float stickMs;           // stick contact time at mid STICK (ms)
	float bodyF[NUM_BODY], bodyT[NUM_BODY], bodyAmp;  // skirt / shell
	float twelfthAmp;        // how strongly the twelfth is tuned in
};

static const TypeSpec TYPES[NUM_TYPES] = {
	// Low-C tenor: C4..E6, one pan in the circle of fifths, octave notes inside
	{60, 88, 1, {{0, 7, 2, 9, 4, 11}, {}, {}}, 12, 2.4f, 0.75f,
	 {340.f, 820.f, 1720.f, 3150.f}, {0.10f, 0.07f, 0.16f, 0.09f}, 0.05f, 1.0f},
	// Double seconds: F#3..C#6, two pans split by whole tones
	{54, 85, 2, {{0, 2, 4, 6, 8, 10}, {1, 3, 5, 7, 9, 11}, {}}, 6, 2.9f, 0.95f,
	 {260.f, 610.f, 1380.f, 2600.f}, {0.13f, 0.09f, 0.14f, 0.08f}, 0.06f, 1.0f},
	// Triple cello: B2..E5, three pans in minor-third (diminished) groups
	{47, 76, 3, {{0, 3, 6, 9}, {1, 4, 7, 10}, {2, 5, 8, 11}}, 4, 3.4f, 1.3f,
	 {170.f, 430.f, 980.f, 2050.f}, {0.18f, 0.12f, 0.16f, 0.09f}, 0.08f, 0.8f},
};

// tenor pans put all 12 pitch classes round the rim in fifths; fill that here
static const int TENOR_ORDER[12] = {0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5};

struct NoteArea {
	int midi = 60;
	int pan = 0, ring = 0;
	float px = 0.f, py = 0.f;   // position on its pan face, -1..1
	float sx = 0.f;             // stereo position across the whole instrument, -1..1
	float size = 0.1f;          // drawn radius (fraction of pan radius)
	int nb[NUM_NB];
	float nbw[NUM_NB];
	int numNb = 0;
};

struct Layout {
	int type = TENOR;
	int count = 0;
	NoteArea n[MAX_NOTES];

	void build(int t) {
		type = t;
		const TypeSpec& T = TYPES[t];
		count = 0;
		int ringOf[3][12];
		for (int p = 0; p < 3; p++)
			for (int k = 0; k < 12; k++) ringOf[p][k] = 0;
		for (int m = T.lowMidi; m <= T.highMidi && count < MAX_NOTES; m++) {
			int pc = m % 12;
			int pan = 0, slot = 0;
			if (t == TENOR) {
				for (int k = 0; k < 12; k++)
					if (TENOR_ORDER[k] == pc) slot = k;
			}
			else {
				for (int p = 0; p < T.numPans; p++)
					for (int k = 0; k < T.pcCount; k++)
						if (T.pcOrder[p][k] == pc) { pan = p; slot = k; }
			}
			int slots = (t == TENOR) ? 12 : T.pcCount;
			NoteArea& a = n[count++];
			a.midi = m;
			a.pan = pan;
			a.ring = ringOf[pan][slot]++;
			static const float RR[2][4] = {{0.76f, 0.47f, 0.2f, 0.05f}, {0.71f, 0.38f, 0.1f, 0.05f}};
			float r = RR[t == CELLO ? 1 : 0][std::min(a.ring, 3)];
			// octave notes sit straight in from their lower note; the innermost ring shifts half a slot
			float ang = TWO_PI_F * (slot + (a.ring >= 2 ? 0.5f : 0.f)) / slots - TWO_PI_F * 0.25f;
			a.px = r * std::cos(ang);
			a.py = r * std::sin(ang);
			float base = (t == TENOR) ? 0.17f : (t == DOUBLE_SECOND ? 0.2f : 0.25f);
			a.size = base * (a.ring == 0 ? 1.f : a.ring == 1 ? 0.78f : 0.58f);
			float panX = T.numPans == 1 ? 0.f : (-1.f + 2.f * pan / (T.numPans - 1));
			a.sx = std::max(-1.f, std::min(1.f, panX * 0.6f + a.px * (T.numPans == 1 ? 0.9f : 0.4f)));
		}
		// neighbours on the same pan (nearest areas), weights fall with distance
		for (int i = 0; i < count; i++) {
			NoteArea& a = n[i];
			a.numNb = 0;
			float bestD[NUM_NB];
			for (int k = 0; k < NUM_NB; k++) { bestD[k] = 1e9f; a.nb[k] = -1; a.nbw[k] = 0.f; }
			for (int j = 0; j < count; j++) {
				if (j == i || n[j].pan != a.pan) continue;
				float dx = n[j].px - a.px, dy = n[j].py - a.py;
				float d = std::sqrt(dx * dx + dy * dy);
				for (int k = 0; k < NUM_NB; k++) {
					if (d < bestD[k]) {
						for (int q = NUM_NB - 1; q > k; q--) { bestD[q] = bestD[q - 1]; a.nb[q] = a.nb[q - 1]; }
						bestD[k] = d;
						a.nb[k] = j;
						break;
					}
				}
			}
			float sum = 0.f;
			for (int k = 0; k < NUM_NB; k++) {
				if (a.nb[k] < 0) continue;
				a.nbw[k] = std::exp(-bestD[k] / 0.32f);
				sum += a.nbw[k];
				a.numNb = k + 1;
			}
			// rows sum to at most 1, so the coupling can never run away
			if (sum > 1e-6f)
				for (int k = 0; k < a.numNb; k++) a.nbw[k] /= std::max(sum, 1.f);
		}
	}

	int nearest(float midi) const {
		int best = 0;
		float bd = 1e9f;
		for (int i = 0; i < count; i++) {
			float d = std::fabs(n[i].midi - midi);
			if (d < bd) { bd = d; best = i; }
		}
		return best;
	}
};

// Per-note tuning: a fixed random error (from the seed) and hand edits, in cents.
struct Tuning {
	uint32_t seed = 20261120u;
	float rnd[NUM_TYPES][MAX_NOTES][3];    // unit-size errors: fund, oct, twelfth
	float hand[NUM_TYPES][MAX_NOTES][3];   // hand edits (cents)
	float ping[NUM_TYPES][MAX_NOTES][2];   // untuned mode ratios

	Tuning() {
		std::memset(hand, 0, sizeof(hand));
		roll(seed);
	}
	void roll(uint32_t s) {
		seed = s;
		Rng r(s);
		for (int t = 0; t < NUM_TYPES; t++)
			for (int i = 0; i < MAX_NOTES; i++) {
				rnd[t][i][0] = r.gauss();
				rnd[t][i][1] = r.gauss();
				rnd[t][i][2] = r.gauss() * 1.4f + 0.25f;  // twelfths are the least accurate, slightly stretched
				ping[t][i][0] = 2.32f + 0.5f * r.uni();
				ping[t][i][1] = 4.6f + 1.4f * r.uni();
			}
	}
	void clearHand() { std::memset(hand, 0, sizeof(hand)); }
};

struct Controls {
	int type = TENOR;
	float stick = 0.5f;      // 0 soft rubber .. 1 hard
	float position = 0.25f;  // 0 centre .. 1 edge of the note area
	float decay = 1.f;       // multiplier 0.25..2.5
	float damp = 0.f;        // 0..1 constant hand damping
	float mute = 0.f;        // palm mute held (0 or 1; a float so Controls has no padding)
	float neighbours = 0.35f;// 0..1
	float bloom = 0.5f;      // 0..1
	float glide = -6.f;      // cents at full force (negative = starts flat)
	float detune = 4.f;      // cents (scale of the random fundamental error)
	float partials = 8.f;    // cents (scale of the random octave/twelfth error)
	float tune = 0.f;        // cents, whole instrument
	float width = 0.6f;
};

struct Mode {
	float zr = 0.f, zi = 0.f;
	float pr = 0.f, pi = 0.f;
	float r = 0.f;
	float w = 0.f;      // nominal radians/sample (before glide)
	float g = 0.f;      // 2(1-r): unity peak gain for coupled/driven input
	float amp = 0.f;    // strike weight
	float cpl = 0.f;    // how much this mode hears its neighbours
	bool on = false;
};

struct Pulse {
	int note = -1;
	int pos = 0, len = 1;
	float force = 0.f;
	float amp[NUM_MODES];
};

struct PanEngine {
	float fs = 48000.f;
	Layout lay;
	Tuning tun;
	Controls cur;     // what the engine is using
	Controls target;  // what the module wants; picked up every 16 samples
	Mode m[MAX_NOTES][NUM_MODES];
	float y[MAX_NOTES];        // last output of each area (for coupling)
	float energy[MAX_NOTES];   // for the screen
	float bend[MAX_NOTES];     // per-area pitch ratio (Bend mode)
	bool active[MAX_NOTES];
	Pulse pulses[MAX_PULSES];
	int blockPos = 0;
	// body / skirt
	float bzr[NUM_BODY], bzi[NUM_BODY], bpr[NUM_BODY], bpi[NUM_BODY], bg[NUM_BODY];
	// stick click noise
	float clickEnv = 0.f, clickSx = 0.f, clickHp = 0.f, clickLast = 0.f;
	Rng noise{12345u};
	float outGain = 4.5f;
	bool fullDirty = true;

	PanEngine() { reset(); }

	void reset() {
		std::memset(m, 0, sizeof(m));
		for (int i = 0; i < MAX_NOTES; i++) { y[i] = 0.f; energy[i] = 0.f; bend[i] = 1.f; active[i] = false; }
		for (int p = 0; p < MAX_PULSES; p++) pulses[p].note = -1;
		for (int b = 0; b < NUM_BODY; b++) bzr[b] = bzi[b] = 0.f;
		lay.build(cur.type);
		blockPos = 0;
		fullDirty = true;
		update(cur);
	}

	void setSampleRate(float sr) {
		fs = sr;
		fullDirty = true;
		update(cur);
	}

	// total tuning error of one partial of one note, in cents
	float errCents(int t, int i, int k, const Controls& c) const {
		float scale = (k == 0) ? c.detune : c.partials;
		return tun.rnd[t][i][k] * scale + tun.hand[t][i][k];
	}

	// recompute every mode's pole; called every 16 samples (cheap)
	void update(const Controls& cIn) {
		const Controls c = cIn;
		target = c;
		if (c.type != lay.type) {
			cur = c;
			std::memset(m, 0, sizeof(m));
			for (int i = 0; i < MAX_NOTES; i++) { y[i] = 0.f; bend[i] = 1.f; }
			for (int p = 0; p < MAX_PULSES; p++) pulses[p].note = -1;
			lay.build(c.type);
			fullDirty = true;
		}
		bool changed = std::memcmp(&c, &cur, sizeof(Controls)) != 0 || fullDirty;
		cur = c;
		fullDirty = false;
		const TypeSpec& T = TYPES[c.type];
		const float damp = c.mute > 0.5f ? 1.f : c.damp;
		for (int b = 0; b < NUM_BODY; b++) {
			float r = std::exp(-6.9078f / (T.bodyT[b] * fs * std::max(0.3f, c.decay) / (1.f + damp * 3.f)));
			float w = TWO_PI_F * T.bodyF[b] / fs;
			bpr[b] = r * std::cos(w);
			bpi[b] = r * std::sin(w);
			bg[b] = 2.f * (1.f - r);
		}
		// only areas that are sounding need their poles refreshed (glide);
		// everything is refreshed when a control moves
		for (int i = 0; i < lay.count; i++)
			if (changed || active[i]) updateNote(i);
	}

	void updateNote(int i) {
		const Controls& c = cur;
		const TypeSpec& T = TYPES[c.type];
		const float damp = c.mute > 0.5f ? 1.f : c.damp;
		{
			const NoteArea& a = lay.n[i];
			float f0 = 440.f * std::pow(2.f, (a.midi - 69) / 12.f + c.tune / 1200.f) * bend[i];
			// glide: frequency follows how hard the fundamental is moving (amplitude squared)
			float amp2 = m[i][0].zr * m[i][0].zr + m[i][0].zi * m[i][0].zi;
			float gl = c.glide * std::min(amp2 * 1.3f, 1.5f);
			float t60 = T.t60Low * std::pow(f0 / (440.f * std::pow(2.f, (T.lowMidi - 69) / 12.f)), -0.55f) * c.decay;
			float dampDiv = 1.f + damp * (c.mute > 0.5f ? 30.f : 12.f);
			float ratios[NUM_MODES] = {1.f, 2.f, 3.f, 4.f, tun.ping[c.type][i][0], tun.ping[c.type][i][1]};
			float cents[NUM_MODES] = {errCents(c.type, i, 0, c), errCents(c.type, i, 1, c),
			                          errCents(c.type, i, 2, c), errCents(c.type, i, 1, c) * 0.7f + 6.f, 0.f, 0.f};
			float tScale[NUM_MODES] = {1.f, 0.8f, 0.5f, 0.3f, 0.06f, 0.035f};
			// the glide moves the fundamental most, the partials a little less
			float glScale[NUM_MODES] = {1.f, 0.85f, 0.7f, 0.6f, 0.4f, 0.4f};
			for (int k = 0; k < NUM_MODES; k++) {
				Mode& md = m[i][k];
				float f = f0 * ratios[k] * std::pow(2.f, (cents[k] + gl * glScale[k]) / 1200.f);
				if (f > 0.45f * fs || f < 20.f) {
					md.on = false;
					md.zr = md.zi = 0.f;
					continue;
				}
				md.on = true;
				float t = std::max(0.012f, t60 * tScale[k] / dampDiv);
				float r = std::exp(-6.9078f / (t * fs));
				float w = TWO_PI_F * f / fs;
				md.r = r;
				md.w = w;
				md.pr = r * std::cos(w);
				md.pi = r * std::sin(w);
				md.g = 2.f * (1.f - r);
			}
			// coupling sensitivity
			m[i][0].cpl = 1.f;
			m[i][1].cpl = 0.9f;
			m[i][2].cpl = 0.75f;
			m[i][3].cpl = 0.4f;
			m[i][4].cpl = 0.f;
			m[i][5].cpl = 0.f;
		}
	}

	// strike note area i with force 0..1.25 (stick + position from the controls)
	void strike(int i, float force, float edgeBleed = 1.f) {
		if (i < 0 || i >= lay.count || force <= 0.f) return;
		const Controls& c = cur;
		const TypeSpec& T = TYPES[c.type];
		const NoteArea& a = lay.n[i];
		float p = c.position;
		float hard = c.stick;
		// contact time: softer stick and gentler hit = longer contact = darker
		float ms = T.stickMs * (1.9f - 1.55f * hard) * (1.15f - 0.3f * std::min(force, 1.f));
		int len = std::max(2, (int) (ms * 0.001f * fs));
		float tw = T.twelfthAmp * (a.ring == 0 ? 1.f : a.ring == 1 ? 0.55f : 0.3f);
		float amps[NUM_MODES] = {
			1.f - 0.45f * p,
			0.22f + 0.45f * p,
			(0.1f + 0.32f * p) * tw,
			0.05f + 0.1f * p,
			(0.035f + 0.06f * p + 0.05f * hard),
			(0.02f + 0.04f * p + 0.06f * hard),
		};
		// small stick pulses roll off the top notes; players hit those a bit harder
		float comp = std::pow(2.f, (a.midi - T.lowMidi) / 12.f * 0.45f);
		for (int k = 0; k < NUM_MODES; k++) amps[k] *= comp;
		if (!active[i]) updateNote(i);
		addPulse(i, force, len, amps);
		// edge hits knock the areas next door directly
		if (edgeBleed > 0.f) {
			float bleed = force * (0.04f + 0.16f * p) * edgeBleed;
			for (int k = 0; k < std::min(3, a.numNb); k++) {
				float am[NUM_MODES] = {0.5f, 0.6f, 0.4f, 0.1f, 0.2f, 0.1f};
				addPulse(a.nb[k], bleed * a.nbw[k], len + len / 2, am);
			}
		}
		// stick click
		float ck = force * (0.015f + 0.06f * hard * hard);
		clickEnv = std::max(clickEnv, ck);
		clickSx = a.sx;
		// knock the body
		for (int b = 0; b < NUM_BODY; b++) bzr[b] += force * T.bodyAmp * (b == 0 ? 0.7f : 1.f);
	}

	void addPulse(int note, float force, int len, const float* amps) {
		int slot = -1;
		for (int k = 0; k < MAX_PULSES; k++)
			if (pulses[k].note < 0) { slot = k; break; }
		if (slot < 0) {  // full: replace the one nearest its end
			int best = 0, left = 1 << 30;
			for (int k = 0; k < MAX_PULSES; k++) {
				int l = pulses[k].len - pulses[k].pos;
				if (l < left) { left = l; best = k; }
			}
			slot = best;
		}
		Pulse& P = pulses[slot];
		P.note = note;
		P.pos = 0;
		P.len = len;
		P.force = force;
		for (int k = 0; k < NUM_MODES; k++) P.amp[k] = amps[k];
		active[note] = true;
	}

	// one sample; returns stereo
	void process(float& outL, float& outR) {
		if (++blockPos >= 16) {
			blockPos = 0;
			update(target);
		}
		const Controls& c = cur;
		// stick pulses (half-sine, area = force)
		for (int k = 0; k < MAX_PULSES; k++) {
			Pulse& P = pulses[k];
			if (P.note < 0) continue;
			float s = P.force * std::sin(3.14159265f * (P.pos + 0.5f) / P.len) * (1.5708f / P.len);
			Mode* mm = m[P.note];
			for (int q = 0; q < NUM_MODES; q++) mm[q].zr += P.amp[q] * s;
			if (++P.pos >= P.len) P.note = -1;
		}
		// sympathetic input from the neighbours (previous sample's outputs)
		float kc = 0.85f * c.neighbours;
		float bloom = c.bloom * 5.f;
		float L = 0.f, R = 0.f;
		float halfW = c.width;
		for (int i = 0; i < lay.count; i++) {
			const NoteArea& a = lay.n[i];
			float in = 0.f;
			for (int k = 0; k < a.numNb; k++) in += a.nbw[k] * y[a.nb[k]];
			in *= kc;
			if (!active[i] && std::fabs(in) < 1e-7f) {
				y[i] = 0.f;
				continue;
			}
			Mode* mm = m[i];
			// nonlinear transfer: fund^2 -> octave, fund*octave -> twelfth
			float f1 = mm[0].zr, f2 = mm[1].zr;
			float nlOct = bloom * f1 * f1;
			float nlTw = bloom * 0.8f * f1 * f2;
			float out = 0.f, en = 0.f;
			for (int q = 0; q < NUM_MODES; q++) {
				Mode& md = mm[q];
				if (!md.on) continue;
				float x = md.cpl * in;
				if (q == 1) x += nlOct;
				else if (q == 2) x += nlTw;
				float zr = md.pr * md.zr - md.pi * md.zi + md.g * x;
				float zi = md.pi * md.zr + md.pr * md.zi;
				md.zr = zr;
				md.zi = zi;
				out += zr;
				en += zr * zr + zi * zi;
			}
			y[i] = out;
			energy[i] = en;
			if (en < 1e-12f) {
				bool pending = false;
				for (int k = 0; k < MAX_PULSES; k++)
					if (pulses[k].note == i) pending = true;
				if (!pending) {
					active[i] = false;
					for (int q = 0; q < NUM_MODES; q++) mm[q].zr = mm[q].zi = 0.f;
				}
			}
			else
				active[i] = true;
			float pan = 0.5f + 0.5f * a.sx * halfW;
			L += out * std::sqrt(1.f - pan);
			R += out * std::sqrt(pan);
		}
		// body / skirt: shared ring, knocked by strikes and fed by the whole drum
		float bus = 0.f;
		for (int i = 0; i < lay.count; i++) bus += y[i];
		float body = 0.f;
		for (int b = 0; b < NUM_BODY; b++) {
			float zr = bpr[b] * bzr[b] - bpi[b] * bzi[b] + bg[b] * 0.12f * bus;
			float zi = bpi[b] * bzr[b] + bpr[b] * bzi[b];
			bzr[b] = zr;
			bzi[b] = zi;
			body += zr;
		}
		// stick click: short bright noise
		float click = 0.f;
		if (clickEnv > 1e-5f) {
			float nz = noise.uni() * 2.f - 1.f;
			clickHp = 0.6f * (clickHp + nz - clickLast);
			clickLast = nz;
			click = clickHp * clickEnv;
			clickEnv *= std::exp(-1.f / (0.0006f * fs));
		}
		float cp = 0.5f + 0.5f * clickSx * halfW;
		L += body * 0.707f + click * std::sqrt(1.f - cp);
		R += body * 0.707f + click * std::sqrt(cp);
		// soft ceiling around +-9 V (piles of long rolls can stack up)
		outL = 9.f * std::tanh(L * outGain / 9.f);
		outR = 9.f * std::tanh(R * outGain / 9.f);
	}
};

}  // namespace panorama
