// Panorama: steel pan engine (no Rack dependency, so test/render.cpp can use it).
//
// Every note area of the chosen pan exists all the time as a small bank of
// modal resonators (complex one-pole filters):
//   fundamental, octave, twelfth (the "compound fifth" tuners also tune),
//   a quiet double octave, and two short untuned "ping" modes.
// A strike is a rubber-tipped stick: a small mass on a stiffening rubber
// spring (Hertz contact with Hunt-Crossley losses, F = K d^1.6 (1 + lam d'))
// that lands on the moving note area and is pushed back off it. A harder hit
// compresses the rubber more, so it is shorter and brighter by itself; a
// light, high note area gives way under the stick, so it is darker.
// Where it lands sets how much it reaches each mode (the octave and twelfth
// have nodal lines through the middle of the note).
// Nothing is a sample or a voice: hitting a note that is still ringing just
// adds to it, and nothing gets stolen.
//
// What makes it sound like a pan rather than a bell:
//   NEIGHBOURS  each note area is fed a little of the areas around it on the
//               drum face. Pans are laid out so neighbours share partials
//               (fourths and fifths), so they sing along faintly.
//   BLOOM       the hammered note area is a shallow shell, so its modes are
//               coupled by a quadratic term: fundamental^2 drives the
//               octave and fundamental x octave the twelfth (the 1:2:3
//               internal resonances tuners build in). It is written as an
//               energy (H = e u0^2 u1 + ...), so energy moves up and back
//               down again, it is never made: the octave swells in after
//               the hit, harder hits bloom more, a badly tuned octave less.
//   NEIGHBOURS  are coupled the same energy-correct way (a symmetric spring
//               between areas), so sympathetic ringing cannot run away.
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

// int_0^1 (1-x^(a+1))^-1/2 dx: sizes a power-law contact spring
inline float contactIntegral(float alpha) {
	const int N = 64;
	float n = alpha + 1.f, sum = 0.f;
	for (int i = 0; i < N; i++) {
		float y = (i + 0.5f) / N;  // x = 1 - y^2
		float d = 1.f - std::pow(1.f - y * y, n);
		sum += 2.f * y / std::sqrt(std::max(d, 1e-12f));
	}
	return sum / N;
}

static const int MAX_CONTACTS = 24;
static constexpr float STICK_MASS = 0.35f;   // relative to the lowest note area
static constexpr float STICK_ALPHA = 1.6f;   // a rubber ball tip: Hertz, a little stiffer

// the stick in contact with one note area
struct Contact {
	int note = -1;
	float xs = 0.f, vs = 0.f;   // stick tip position (into the steel) and velocity
	float K = 0.f, lam = 0.f, ms = STICK_MASS, ret = 30.f, dRef = 1e-3f;
	float ucPrev = 0.f;
	int sepN = 0, life = 0, sepMax = 100, lifeMax = 3000;
	float phi[NUM_MODES];
	void start(int n, float uc, float v, float T, float lambda, float retract, float fs) {
		static const float IA = contactIntegral(STICK_ALPHA);
		note = n;
		lam = lambda; ret = retract;
		dRef = T / (2.f * IA);
		K = (STICK_ALPHA + 1.f) * ms / (2.f * std::pow(dRef, STICK_ALPHA + 1.f));
		xs = uc; vs = v; ucPrev = uc;
		sepN = 0; life = 0;
		sepMax = (int) (0.006f * fs);   // a re-contact in this time is the same hit
		lifeMax = (int) (0.08f * fs);
	}
	inline float step(float uc, float dt) {
		float d = xs - uc;
		float F = 0.f;
		if (d > 0.f) {
			float dd = vs - (uc - ucPrev) / dt;
			F = K * std::pow(d, STICK_ALPHA) * (1.f + lam * dd);
			if (F < 0.f) F = 0.f;
			sepN = 0;
		}
		else sepN++;
		ucPrev = uc;
		vs -= F / ms * dt;
		if (vs < 0.f) vs -= ret * dt;  // the wrist pulls the stick back
		xs += vs * dt;
		if ((sepN > sepMax && d < -0.3f * dRef) || ++life > lifeMax) note = -1;
		return F;
	}
};

struct NoteArea {
	int midi = 60;
	int pan = 0, ring = 0;
	float px = 0.f, py = 0.f;   // position on its pan face, -1..1
	float sx = 0.f;             // stereo position across the whole instrument, -1..1
	float size = 0.1f;          // drawn radius (fraction of pan radius)
	int nb[NUM_NB];
	float nbw[NUM_NB];
	int numNb = 0;
	float M = 1.f;              // modal mass (big low areas carry more steel)
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
		// symmetric copy (an energy-correct spring needs w_ij = w_ji)
		for (int i = 0; i < MAX_NOTES; i++)
			for (int j = 0; j < MAX_NOTES; j++) W[i][j] = 0.f;
		for (int i = 0; i < count; i++)
			for (int k = 0; k < n[i].numNb; k++) {
				int j = n[i].nb[k];
				W[i][j] = W[j][i] = std::max(W[i][j], n[i].nbw[k]);
			}
		float maxRow = 1.f;
		for (int i = 0; i < count; i++) {
			float r = 0.f;
			for (int j = 0; j < count; j++) r += W[i][j];
			maxRow = std::max(maxRow, r);
		}
		for (int i = 0; i < count; i++) {
			wn[i] = 0;
			for (int j = 0; j < count; j++) {
				W[i][j] /= maxRow;
				if (W[i][j] > 0.f && wn[i] < MAX_WN) { wj[i][wn[i]] = j; wv[i][wn[i]] = W[i][j]; wn[i]++; }
			}
		}
		// modal mass: lower and bigger (outer ring) areas are heavier
		float fLow = 440.f * std::pow(2.f, (T.lowMidi - 69) / 12.f);
		for (int i = 0; i < count; i++) {
			float f = 440.f * std::pow(2.f, (n[i].midi - 69) / 12.f);
			n[i].M = std::pow(fLow / f, 0.6f) * (n[i].ring == 0 ? 1.f : n[i].ring == 1 ? 0.85f : 0.7f);
		}
	}
	static const int MAX_WN = 20;
	float W[MAX_NOTES][MAX_NOTES];
	int wn[MAX_NOTES];
	int wj[MAX_NOTES][MAX_WN];
	float wv[MAX_NOTES][MAX_WN];

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
	float rad = 0.f;    // how well this mode radiates
	float invMw = 0.f;  // state -> physical displacement, 1/(M w)
	float sqMw = 0.f, invSqMw = 0.f;  // for energy-symmetric coupling
	float fr = 0.f, fi = 0.f;         // free-running prediction this sample
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
	Contact contacts[MAX_CONTACTS];
	float eps[MAX_NOTES];      // quadratic shell coupling of each area
	float aRef[MAX_NOTES];     // fundamental displacement of a full-force hit
	float u[MAX_NOTES][NUM_MODES];
	float ut[MAX_NOTES];
	Rng srng{4242u};
	int blockPos = 0;
	// body / skirt
	float bzr[NUM_BODY], bzi[NUM_BODY], bpr[NUM_BODY], bpi[NUM_BODY], bg[NUM_BODY];
	// stick click noise
	float clickEnv = 0.f, clickSx = 0.f, clickHp = 0.f, clickLast = 0.f;
	Rng noise{12345u};
	float outGain = 6.5f;
	static constexpr float IREF = STICK_MASS * 1.5f;  // impulse of a full-force hit
	static constexpr float BLOOM_MAX = 0.03f;
	bool fullDirty = true;

	PanEngine() { reset(); }

	void reset() {
		std::memset(m, 0, sizeof(m));
		for (int i = 0; i < MAX_NOTES; i++) { y[i] = 0.f; energy[i] = 0.f; bend[i] = 1.f; active[i] = false; }
		for (int p = 0; p < MAX_PULSES; p++) pulses[p].note = -1;
		for (int p = 0; p < MAX_CONTACTS; p++) contacts[p].note = -1;
		for (int i = 0; i < MAX_NOTES; i++) { eps[i] = 0.f; aRef[i] = 1e-3f; ut[i] = 0.f; for (int k = 0; k < NUM_MODES; k++) u[i][k] = 0.f; }
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
			for (int p = 0; p < MAX_CONTACTS; p++) contacts[p].note = -1;
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
			float amp2 = (m[i][0].zr * m[i][0].zr + m[i][0].zi * m[i][0].zi) * (1.f / (IREF * IREF));
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
				float wr = w * fs;  // rad/s
				md.invMw = 1.f / (a.M * wr);
				md.sqMw = std::sqrt(a.M * wr);
				md.invSqMw = 1.f / md.sqMw;
			}
			// the shell's quadratic coupling, eps = eta M w0^2 / A, A = Iref/(M w0)
			{
				float w0 = TWO_PI_F * f0;
				eps[i] = c.bloom * BLOOM_MAX * a.M * a.M * w0 * w0 * w0 / IREF;
				aRef[i] = IREF / (a.M * w0);
			}
			// radiation: the fundamental pumps the most air; the rest less so.
			// Big outer areas radiate better (area), which cancels their mass.
			{
				float tw = T.twelfthAmp * (a.ring == 0 ? 1.f : a.ring == 1 ? 0.6f : 0.35f);
				const float rad[NUM_MODES] = {1.f, 0.5f, 0.75f * tw, 0.2f, 0.3f, 0.25f};
				for (int k = 0; k < NUM_MODES; k++) m[i][k].rad = rad[k];
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
		// the stick: contact time at unit speed on rigid steel (a softer rubber
		// tip is longer); the hit's own speed and the give of the note do the rest
		// small high notes are hit with a lighter, quicker flick (and are stiffer),
		// so the contact shortens up the range
		float fLow = 440.f * std::pow(2.f, (T.lowMidi - 69) / 12.f);
		float f0 = 440.f * std::pow(2.f, (a.midi - 69) / 12.f);
		float ms = T.stickMs * (1.9f - 1.55f * hard) * 0.9f * std::pow(f0 / fLow, -0.4f);
		int len = std::max(2, (int) (ms * 0.001f * fs * (1.15f - 0.3f * std::min(force, 1.f))));
		if (!active[i]) updateNote(i);
		int slot = -1;
		for (int k = 0; k < MAX_CONTACTS; k++)
			if (contacts[k].note < 0) { slot = k; break; }
		if (slot < 0) {  // all busy: take the oldest
			int best = 0;
			for (int k = 1; k < MAX_CONTACTS; k++)
				if (contacts[k].life > contacts[best].life) best = k;
			slot = best;
		}
		Contact& C = contacts[slot];
		// where it lands: r from the middle of the note towards its rim, at a
		// small angle off the long axis (players aim along it, but not exactly).
		// The octave's nodal line crosses the long axis at the middle and the
		// twelfth's runs along it, so a dead-centre hit is nearly a pure tone
		float r = 0.05f + 0.9f * p;
		float th = 0.3f + 0.25f * (srng.uni() * 2.f - 1.f);
		float sr1 = std::sin(3.14159265f * std::min(r, 0.95f));
		C.phi[0] = std::cos(0.5f * 3.14159265f * r);
		C.phi[1] = sr1 * std::cos(th);
		C.phi[2] = sr1 * std::sin(th) + 0.15f * r;
		C.phi[3] = 0.15f + 0.6f * std::fabs(std::sin(2.f * 3.14159265f * r));
		C.phi[4] = 0.3f + 0.7f * r;   // rim and skirt modes: more towards the edge
		C.phi[5] = 0.2f + 0.8f * r;
		float uc = 0.f;
		for (int k = 0; k < NUM_MODES; k++) uc += C.phi[k] * m[i][k].zi * m[i][k].invMw;
		float lam = 0.15f + 0.6f * (1.f - hard);  // soft rubber loses more
		// players hit the small top notes a little harder to keep them level
		float v = force * std::pow(f0 / fLow, 0.2f);
		C.start(i, uc, v, ms * 0.001f, lam, 25.f + 20.f * hard, fs);
		active[i] = true;
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
		const float dt = 1.f / fs;
		const int N = lay.count;
		// 1) where is every sounding mode going this sample (before any input)?
		for (int i = 0; i < N; i++) {
			ut[i] = 0.f;
			if (!active[i]) { for (int q = 0; q < NUM_MODES; q++) u[i][q] = 0.f; continue; }
			Mode* mm = m[i];
			for (int q = 0; q < NUM_MODES; q++) {
				Mode& md = mm[q];
				if (!md.on) { md.fr = md.fi = 0.f; u[i][q] = 0.f; continue; }
				md.fr = md.pr * md.zr - md.pi * md.zi;
				md.fi = md.pi * md.zr + md.pr * md.zi;
				u[i][q] = md.fi * md.invMw;
				ut[i] += md.cpl * md.fi * md.invSqMw;
			}
		}
		// 2) forces into each mode, as impulses this sample
		float x[MAX_NOTES][NUM_MODES];
		std::memset(x, 0, sizeof(x));
		// the stick on the steel
		for (int k = 0; k < MAX_CONTACTS; k++) {
			Contact& C = contacts[k];
			if (C.note < 0) continue;
			int i = C.note;
			float uc = 0.f;
			for (int q = 0; q < NUM_MODES; q++) uc += C.phi[q] * u[i][q];
			float F = C.step(uc, dt) * dt;
			for (int q = 0; q < NUM_MODES; q++) x[i][q] += C.phi[q] * F;
			active[i] = true;
		}
		// knocks from an edge hit next door (short half-sine pushes)
		for (int k = 0; k < MAX_PULSES; k++) {
			Pulse& P = pulses[k];
			if (P.note < 0) continue;
			float sp = P.force * std::sin(3.14159265f * (P.pos + 0.5f) / P.len) * (1.5708f / P.len);
			for (int q = 0; q < NUM_MODES; q++) x[P.note][q] += P.amp[q] * sp * IREF;
			if (++P.pos >= P.len) P.note = -1;
		}
		// neighbours: a symmetric spring through the steel between note areas,
		// at a rate of up to ~2.5 Hz (how fast energy swaps between two notes
		// tuned to the same partial)
		const float kc = 2.f * (TWO_PI_F * 2.5f) * c.neighbours * dt;
		float L = 0.f, R = 0.f;
		float halfW = c.width;
		for (int i = 0; i < N; i++) {
			const NoteArea& a = lay.n[i];
			float in = 0.f;
			for (int k = 0; k < lay.wn[i]; k++) in += lay.wv[i][k] * ut[lay.wj[i][k]];
			in *= kc;
			if (!active[i]) {
				if (std::fabs(in) < 1e-9f) { y[i] = 0.f; continue; }
				// woken by its neighbours: start from rest
				for (int q = 0; q < NUM_MODES; q++) m[i][q].fr = m[i][q].fi = 0.f;
				updateNote(i);
			}
			Mode* mm = m[i];
			// shell nonlinearity, H = e (u0^2 u1 + 0.8 u0 u1 u2), softly limited
			// at a few times a full hit (a cubic potential has no floor)
			{
				float lim = 1.f / (3.f * aRef[i]);
				float u0 = u[i][0] / (1.f + std::fabs(u[i][0]) * lim);
				float u1 = u[i][1] / (1.f + std::fabs(u[i][1]) * lim);
				float u2 = u[i][2] / (1.f + std::fabs(u[i][2]) * lim);
				float e = eps[i] * dt;
				x[i][0] -= e * (2.f * u0 * u1 + 0.8f * u1 * u2);
				x[i][1] -= e * (u0 * u0 + 0.8f * u0 * u2);
				x[i][2] -= e * 0.8f * u0 * u1;
			}
			float out = 0.f, en = 0.f;
			for (int q = 0; q < NUM_MODES; q++) {
				Mode& md = mm[q];
				if (!md.on) continue;
				md.zr = md.fr + x[i][q] + in * md.cpl * md.sqMw;
				md.zi = md.fi;
				out += md.rad * md.zi;
				en += md.zr * md.zr + md.zi * md.zi;
			}
			out *= 1.f / IREF;
			en *= 1.f / (IREF * IREF);
			y[i] = out;
			energy[i] = en;
			if (en < 1e-12f) {
				bool pending = false;
				for (int k = 0; k < MAX_PULSES; k++)
					if (pulses[k].note == i) pending = true;
				for (int k = 0; k < MAX_CONTACTS; k++)
					if (contacts[k].note == i) pending = true;
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
