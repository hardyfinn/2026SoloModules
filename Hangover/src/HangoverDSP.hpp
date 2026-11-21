// Hangover: a handpan (hang drum) with its dirt left in.
// Pure C++ engine, no Rack dependency, so test/render.cpp can run it offline.
//
// One physical instrument: a ding in the middle plus up to 8 tone fields.
// Every field is always "there" and ringing; a strike adds energy to it.
//   - each field has 6 partials (fundamental, octave, compound fifth + 3
//     higher shell-ish modes), each split into a slowly beating pair
//   - the shell has a few knock modes, and the air inside is a Helmholtz
//     resonator that the gu (bottom port) lets out: the deep bass "boom"
//   - physical excitation: the finger is a small mass on a soft, lossy
//     (Hunt-Crossley) spring that lands on the moving surface, so a harder
//     hit is shorter and brighter by itself, a soft pad lingers and damps,
//     and striking a field that is already swinging feels (and sounds)
//     different. Where it lands sets how much of each mode it reaches:
//     the octave and the compound fifth have nodal lines through the
//     centre, so a dead-centre hit is almost a pure fundamental
//   - the shell is nonlinear: a quadratic coupling between fundamental,
//     octave and fifth (the 1:2:3 internal resonances the tuner sets up)
//     pumps energy upwards after the hit, so the octave blooms in, more for
//     harder hits and less when the octave is out of tune. The coupling is
//     conservative (energy moves, it is not made), so it cannot run away
//   - the cavity air is a Helmholtz resonator coupled both ways to every
//     field's fundamental (the only modes that change the volume), so the
//     ding pumps the gu and the gu pushes back on the ding
//   - fields couple to each other through the shell with a symmetric,
//     energy-correct spring coupling (sympathetic ringing)
//   - the dirt: tuning errors, beating, a rattler + a loose field that buzz,
//     fields bleeding into each other, air through the port, skin noise,
//     sloppy hands (late, uneven, bounced, grazed strikes), a small room
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace hangover {

static const float HG_PI = 3.14159265f;
static const int MAXN = 9;          // ding + 8 tone fields
static const int NP = 6;            // partials per field
static const int NM = NP * 2;       // each partial is a beating pair
static const int NBODY = 5;         // shell knock modes
static const int NQ = 64;           // pending strikes (slop delays, bounces)
static const int NPULSE = 3;        // overlapping finger pulses per target

struct HgRng {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v ? v : 0x1234567u; for (int i = 0; i < 8; i++) next(); }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

struct HgScale {
	const char* name;
	int n;            // notes including the ding
	int iv[MAXN];     // semitones above the ding, ascending
};

static const int NUM_SCALES = 9;
static const HgScale HG_SCALES[NUM_SCALES] = {
	{"KURD", 9, {0, 7, 8, 10, 12, 14, 15, 17, 19}},
	{"INTEGRAL", 8, {0, 7, 8, 10, 12, 14, 15, 19, 0}},
	{"CELTIC", 9, {0, 7, 10, 12, 14, 15, 17, 19, 22}},
	{"AMARA", 8, {0, 7, 10, 12, 14, 15, 19, 22, 0}},
	{"HIJAZ", 9, {0, 7, 8, 11, 12, 14, 15, 17, 19}},
	{"PYGMY", 9, {0, 3, 5, 7, 10, 12, 14, 15, 19}},
	{"EQUINOX", 9, {0, 3, 7, 8, 10, 12, 14, 15, 19}},
	{"SABYE", 9, {0, 4, 5, 7, 9, 11, 12, 14, 16}},
	{"AEGEAN", 9, {0, 4, 7, 11, 12, 16, 18, 19, 23}},
};

static const char* HG_NOTE_NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

// partial layout of one tone field
// Measured on a real D Kurd hang (see docs): a tone field is three partials
// and almost nothing else (the next modes sit 40 dB down). The ding's dome
// has more: partials near 4, 6 and 8 times its note are part of its voice.
static const float HG_RATIO[NP] = {1.f, 2.f, 3.f, 4.1f, 5.43f, 6.8f};
static const float HG_RATIO_DING[NP] = {1.f, 2.f, 2.98f, 3.97f, 5.9f, 7.83f};
// how strongly each mode radiates (the fundamental is a monopole and pumps
// the most air; the octave and fifth are dipoles; the upper shell modes less)
static const float HG_AMP[NP] = {1.f, 0.6f, 0.38f, 0.12f, 0.08f, 0.05f};
// how well the upper shell modes radiate (the three tuned partials follow a
// frequency law instead, see updateCoefs)
static const float HG_RAD[NP] = {1.f, 0.7f, 0.45f, 0.06f, 0.05f, 0.04f};
static const float HG_RAD_DING[NP] = {0.3f, 1.f, 0.42f, 0.35f, 0.08f, 1.0f};
// how strongly each mode is tied to the rest of the shell (sympathetic bleed)
static const float HG_CPL[NP] = {1.f, 0.8f, 0.6f, 0.3f, 0.2f, 0.15f};
static const float HG_FINGER_MASS = 0.6f;   // relative to a 220 Hz field
static const float HG_NL = 0.004f;          // strength of the 1:2:3 shell coupling
// The hang's long tail is its octave and fifth: measured, they ring about
// 1.5 and 1.8 times as long as the fundamental, which leaves first.
static const float HG_DECAYF[NP] = {1.f, 1.5f, 1.8f, 0.2f, 0.14f, 0.1f};
static const float HG_DECAYF_DING[NP] = {1.f, 1.1f, 2.2f, 1.6f, 0.5f, 1.2f};

struct HgParams {
	int scale = 0;
	int root = 2;          // 0 = C ... 11 = B (2 = D)
	float decay = 4.f;     // seconds for a fundamental to die away (T60)
	float force = 0.7f;    // velocity for triggers without VEL
	float touch = 0.3f;    // 0 soft finger pad .. 1 hard slap
	float strike = 0.3f;   // 0 centre of the field .. 1 its edge
	float mute = 0.f;      // palm damping 0..1 (knob or gate, whichever is more)
	float gu = 0.5f;       // Helmholtz bass level
	float roll = 0.5f;     // thumb roll speed 0..1 -> 6..24 hits/s
	// the dirt, each 0..1 (already scaled by the DIRT master)
	float tuning = 0.3f, beat = 0.3f, rattle = 0.2f, bleed = 0.3f;
	float air = 0.3f, skin = 0.3f, slop = 0.2f, room = 0.25f;
};

// complex one-pole resonator: z = p*z + x, output Im(z)
struct HgMode {
	float re = 0.f, im = 0.f;
	float c = 0.f, s = 0.f;
	float gain = 1.f;   // output gain
	float inW = 0.f;    // weight of the current finger pulse
	float cw = 0.f;     // weight of sympathetic input
	float fr = 0.f, fi = 0.f;   // free-running prediction for this sample
	inline void prep() { fr = c * re - s * im; fi = s * re + c * im; }
	inline float commit(float x) { re = fr + x; im = fi; return im; }
	inline float tick(float x) {
		float nr = c * re - s * im + x;
		float ni = s * re + c * im;
		re = nr; im = ni;
		return gain * ni;
	}
	void setCoef(float freq, float t60, float sr) {
		if (freq <= 0.f || freq >= 0.45f * sr || t60 <= 0.f) { c = s = 0.f; return; }
		float r = std::exp(-6.9078f / (t60 * sr));
		float w = 2.f * HG_PI * freq / sr;
		c = r * std::cos(w);
		s = r * std::sin(w);
	}
};

// half-sine force pulse (a finger or palm in contact)
struct HgPulse {
	int len = 0, pos = 0;
	float amp = 0.f;
	void start(int L, float a) { len = std::max(2, L); pos = 0; amp = a * HG_PI / (2.f * len); }
	inline float next() {
		if (pos >= len) return 0.f;
		float v = amp * std::sin(HG_PI * (pos + 0.5f) / len);
		pos++;
		return v;
	}
	bool active() const { return pos < len; }
};

// integral used to size a power-law contact spring: int_0^1 (1-x^(a+1))^-1/2 dx
static inline float hgContactIntegral(float alpha) {
	const int N = 64;
	float n = alpha + 1.f, sum = 0.f;
	for (int i = 0; i < N; i++) {
		float y = (i + 0.5f) / N;  // x = 1 - y^2
		float d = 1.f - std::pow(1.f - y * y, n);
		sum += 2.f * y / std::sqrt(std::max(d, 1e-12f));
	}
	return sum / N;
}

// a finger (or palm) landing on the steel: a mass on a power-law spring with
// Hunt-Crossley damping, F = K d^a (1 + lam d'), pushing against the surface
// displacement at the contact point. Runs until the finger has lifted away.
struct HgContact {
	bool on = false;
	float xs = 0.f, vs = 0.f;     // finger position (into the steel) and velocity
	float K = 0.f, alpha = 2.f, lam = 0.f, ms = 0.6f, ret = 50.f, dRef = 1e-3f;
	float ucPrev = 0.f;
	int sepN = 0, life = 0, sepMax = 100, lifeMax = 2000;
	float phi[NP];                // how much of each mode this spot reaches
	// size the spring so a hit at unit speed on rigid steel lasts T seconds
	void start(float uc, float v, float T, float a, float lambda, float mass, float retract, float sr) {
		alpha = a; lam = lambda; ms = mass; ret = retract;
		static float I2 = hgContactIntegral(2.f);
		float Ia = (std::fabs(a - 2.f) < 1e-3f) ? I2 : hgContactIntegral(a);
		dRef = T / (2.f * Ia);
		K = (a + 1.f) * ms / (2.f * std::pow(dRef, a + 1.f));
		xs = uc; vs = v; ucPrev = uc;
		sepN = 0; life = 0;
		sepMax = (int) (0.004f * sr);
		lifeMax = (int) (0.06f * sr);
		on = true;
	}
	inline float step(float uc, float dt) {
		float d = xs - uc;
		float F = 0.f;
		if (d > 0.f) {
			float dd = vs - (uc - ucPrev) / dt;
			F = K * std::pow(d, alpha) * (1.f + lam * dd);
			if (F < 0.f) F = 0.f;
			sepN = 0;
		}
		else sepN++;
		ucPrev = uc;
		vs -= F / ms * dt;
		if (vs < 0.f) vs -= ret * dt;  // the hand lifts away
		xs += vs * dt;
		if ((sepN > sepMax && d < -0.3f * dRef) || ++life > lifeMax) on = false;
		return F;
	}
};

// TPT state-variable filter (bandpass / lowpass / highpass)
struct HgSvf {
	float g = 0.f, k = 1.f, ic1 = 0.f, ic2 = 0.f, bp = 0.f, lp = 0.f, hp = 0.f;
	void set(float fc, float q, float sr) {
		fc = std::min(fc, 0.45f * sr);
		g = std::tan(HG_PI * fc / sr);
		k = 1.f / q;
	}
	inline void tick(float x) {
		float v3 = x - ic2;
		float v1 = (g * v3 + ic1) / (1.f + g * (g + k));
		float v2 = ic2 + g * v1;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		bp = v1; lp = v2; hp = x - k * v1 - v2;
	}
};

struct HgNote {
	HgMode m[NM];
	HgPulse pulse[NPULSE];
	int pulseNext = 0;
	float pulseW[NPULSE][NP];
	float freq = 220.f;
	int midi = 57;
	float px = 0.f, py = 0.f;     // position on the top view (unit circle)
	float panL = 0.707f, panR = 0.707f;
	float env = 0.f;              // smoothed fundamental level (display, glide)
	float flash = 0.f;            // strike flash for the display
	float last = 0.f;             // last output (drives the neighbours)
	bool idle = true;
	float rFund = 0.f;            // pole radius of the fundamental
	HgContact con[NPULSE];        // fingers on this field
	int conNext = 0;
	float M = 1.f;                // modal mass (big low fields are heavy)
	float wk[NP];                 // nominal angular frequency of each partial (rad/s)
	float invMw[NP];              // state -> physical displacement
	float sqMw[NP], invSqMw[NP];  // for energy-symmetric coupling
	float u[NP];                  // predicted displacement of each partial this sample
	float ut = 0.f;               // normalised displacement seen by the neighbours
	float eps = 0.f;              // quadratic shell nonlinearity
	float aRef = 1e-3f;           // fundamental displacement of a full-force hit
	// this instrument's imperfections (seeded)
	float tuneErr[NP];            // -1..1
	float beatHz[NP];             // pair split in Hz at BEAT = 1
	float pairAmp[NP];            // level of the second mode of each pair
	float ratioJit[NP];           // fixed inharmonic jitter of the upper modes
	float radVar[NP];             // this field's own voicing: how loud each partial speaks
	float decVar[NP];             // and how long each rings
	float rad[NP];                // output gain of each partial
};

struct HgQueued {
	bool on = false;
	int delay = 0;
	int note = 0;
	float vel = 0.f, pos = 0.f;
	bool gu = false, quiet = false;
};

struct HangoverEngine {
	float sr = 48000.f;
	HgParams p;
	HgRng rng, noise;
	uint32_t seed = 1;

	int nNotes = 9;
	HgNote note[MAXN];
	float adj[MAXN][MAXN];
	float adjN[MAXN][MAXN];
	HgQueued queue[NQ];

	HgMode body[NBODY];
	float bodyFreq[NBODY], bodyT60[NBODY];
	HgPulse bodyPulse[NPULSE];
	int bodyNext = 0;

	HgMode helm;
	static constexpr float HELM_M = 1.f;
	float helmSqMw = 1.f;
	float helmFreq = 75.f;
	HgPulse helmPulse[NPULSE];
	int helmNext = 0;

	// skin / nail / air
	HgSvf skinF, airF, hissF, floorF;
	float skinEnv = 0.f, skinDec = 0.99f;
	float nailEnv = 0.f;
	float airEnv = 0.f;
	float palmEnv = 0.f;
	HgSvf palmF;

	// rattle: a loose part (a ring, a washer, the stand) that clatters
	float rp = 0.f, rv = 0.f, rattleFreq = 180.f;
	HgMode click[2];
	float clickFreq[2];
	float clickNoise = 0.f;
	int looseNote = 3;
	float looseHp = 0.f, looseX = 0.f;

	// room
	std::vector<float> erBuf, fdnBuf[4];
	int erPos = 0, fdnPos[4] = {0, 0, 0, 0};
	int fdnLen[4] = {1, 1, 1, 1};
	float fdnLp[4] = {0, 0, 0, 0};
	float fdnG[4] = {0, 0, 0, 0};

	// roll
	bool rollOn = false;
	float rollTimer = 0.f;
	bool rollAlt = false;
	int lastNote = 0;

	int blockCount = 0;
	float muteGate = 0.f;  // 0..1 from the MUTE gate
	float outPeak = 0.f;
	int rattleHits = 0;     // count of rattler impacts (display / tests)

	HangoverEngine() { setSampleRate(48000.f); reseed(1); }

	void setSampleRate(float s) {
		sr = s;
		int erN = (int) (0.03f * sr) + 4;
		erBuf.assign(erN, 0.f);
		erPos = 0;
		const float ms[4] = {23.1f, 29.9f, 35.3f, 41.7f};
		for (int i = 0; i < 4; i++) {
			fdnLen[i] = (int) (ms[i] * 0.001f * sr);
			fdnBuf[i].assign(fdnLen[i] + 2, 0.f);
			fdnPos[i] = 0;
		}
		layout();
		updateCoefs();
	}

	// a new instrument: every imperfection re-rolled
	void reseed(uint32_t s) {
		seed = s ? s : 1;
		HgRng r;
		r.seed(seed * 2654435761u + 17u);
		for (int i = 0; i < MAXN; i++) {
			HgNote& n = note[i];
			for (int k = 0; k < NP; k++) {
				n.tuneErr[k] = r.bi();
				n.beatHz[k] = (0.25f + 2.4f * r.uni()) * (k == 0 ? 0.6f : 1.f + 0.4f * k);
				n.pairAmp[k] = 0.3f + 0.6f * r.uni();
				n.ratioJit[k] = k < 3 ? 0.f : 0.03f * r.bi();
				// measured fields differ a lot: octave anywhere from 12 dB under the
				// fundamental to 10 dB over it. Each field gets its own voicing.
				n.radVar[k] = k == 0 ? 1.f : std::pow(2.f, 0.9f * r.bi());
				n.decVar[k] = k == 0 ? 1.f : std::pow(1.3f, r.bi());
			}
		}
		const float bf[NBODY] = {165.f, 272.f, 418.f, 633.f, 905.f};
		for (int b = 0; b < NBODY; b++) {
			bodyFreq[b] = bf[b] * (1.f + 0.12f * r.bi());
			bodyT60[b] = 0.18f + 0.35f * r.uni();
		}
		helmFreq = 0.5f + 0.12f * r.bi();  // stored as a ratio of the ding until layout()
		rattleFreq = 140.f + 140.f * r.uni();
		clickFreq[0] = 1900.f + 1200.f * r.uni();
		clickFreq[1] = 4200.f + 2600.f * r.uni();
		looseNote = 1 + (int) (r.uni() * 7.99f);
		helmRatio = helmFreq;
		layout();
		updateCoefs();
	}
	float helmRatio = 0.5f;

	static int dingMidi(int root) { return root <= 7 ? 48 + root : 36 + root; }

	// note pitches and where each field sits on the shell
	void layout() {
		const HgScale& sc = HG_SCALES[std::max(0, std::min(NUM_SCALES - 1, p.scale))];
		nNotes = sc.n;
		int dm = dingMidi(p.root);
		int nf = nNotes - 1;
		float step = 2.f * HG_PI / nf;
		for (int i = 0; i < MAXN; i++) {
			HgNote& n = note[i];
			int iv = i < nNotes ? sc.iv[i] : 0;
			n.midi = dm + iv;
			n.freq = 440.f * std::pow(2.f, (n.midi - 69) / 12.f);
			if (i == 0) { n.px = 0.f; n.py = 0.f; }
			else {
				// zigzag up both sides from the bottom (nearest the player)
				int k = i - 1;
				int side = (k % 2 == 1) ? 1 : -1;
				int steps = (k + 1) / 2;
				float a = HG_PI / 2.f + side * steps * step;
				n.px = 0.7f * std::cos(a);
				n.py = 0.7f * std::sin(a);
			}
			// bigger, lower fields carry more steel; the ding is the biggest
			n.M = std::max(0.4f, std::min(2.5f, std::pow(220.f / n.freq, 0.6f))) * (i == 0 ? 1.5f : 1.f);
			float pan = 0.6f * n.px;
			float ang = (pan + 1.f) * HG_PI / 4.f;
			n.panL = std::cos(ang);
			n.panR = std::sin(ang);
		}
		for (int i = 0; i < MAXN; i++)
			for (int j = 0; j < MAXN; j++) {
				if (i == j || i >= nNotes || j >= nNotes) { adj[i][j] = 0.f; continue; }
				if (i == 0 || j == 0) { adj[i][j] = 0.55f; continue; }
				float dx = note[i].px - note[j].px, dy = note[i].py - note[j].py;
				adj[i][j] = 0.15f + 0.85f * std::exp(-(dx * dx + dy * dy) / 0.3f);
			}
		// symmetric copy for the energy-correct shell coupling, scaled so no
		// row sums past 1 (the coupling rate is then set by BLEED alone)
		float maxRow = 1e-6f;
		for (int i = 0; i < nNotes; i++) {
			float r = 0.f;
			for (int j = 0; j < nNotes; j++) r += adj[i][j];
			maxRow = std::max(maxRow, r);
		}
		for (int i = 0; i < MAXN; i++)
			for (int j = 0; j < MAXN; j++) adjN[i][j] = adj[i][j] / maxRow;
		helmFreq = std::max(55.f, std::min(115.f, note[0].freq * helmRatio));
		if (looseNote >= nNotes) looseNote = nNotes - 1;
	}

	int lastScale = -1, lastRoot = -1;

	void updateCoefs() {
		if (p.scale != lastScale || p.root != lastRoot) {
			lastScale = p.scale; lastRoot = p.root;
			layout();
		}
		float muteAmt = std::max(p.mute, muteGate);
		for (int i = 0; i < nNotes; i++) {
			HgNote& n = note[i];
			float pitchF = std::pow(220.f / n.freq, 0.15f);
			pitchF = std::max(0.7f, std::min(1.3f, pitchF));
			const float* ratio = i == 0 ? HG_RATIO_DING : HG_RATIO;
			const float* decf = i == 0 ? HG_DECAYF_DING : HG_DECAYF;
			float glide = (0.0015f + 0.004f * p.tuning) * std::min(n.env, 1.5f);
			float base = n.freq * std::pow(2.f, n.tuneErr[0] * 8.f * p.tuning / 1200.f) * (1.f + glide);
			for (int k = 0; k < NP; k++) {
				float cents = k == 0 ? 0.f : n.tuneErr[k] * (k == 2 ? 28.f : 20.f) * p.tuning;
				float f = base * ratio[k] * (1.f + n.ratioJit[k]) * std::pow(2.f, cents / 1200.f);
				float split = n.beatHz[k] * p.beat * 0.5f;
				float t60 = p.decay * decf[k] * n.decVar[k] * pitchF * (i == 0 ? 1.15f : 1.f);
				float inv = 1.f / t60 + muteAmt * (12.f + 4.f * k);
				t60 = 1.f / inv;
				n.m[2 * k].setCoef(f - split, t60, sr);
				n.m[2 * k + 1].setCoef(f + split, t60 * 0.92f, sr);
				n.m[2 * k].gain = 1.f;
				n.m[2 * k + 1].gain = n.pairAmp[k] * p.beat;
				float a = HG_AMP[k] * (i == 0 && k == 1 ? 0.8f : 1.f);
				n.m[2 * k].cw = a;
				n.m[2 * k + 1].cw = a;
				float w = 2.f * HG_PI * f;
				n.wk[k] = w;
				n.invMw[k] = 1.f / (n.M * w);
				n.sqMw[k] = std::sqrt(n.M * w);
				n.invSqMw[k] = 1.f / n.sqMw[k];
			}
			// quadratic shell term, sized so a full-force hit sets off a clear bloom:
			// eps = eta M w0^2 / A with A the displacement of a unit-impulse fundamental
			{
				float w0 = n.wk[0];
				float Iref = HG_FINGER_MASS * 1.5f;
				// (the ding's big dome is the most nonlinear: its octave swells in)
				n.eps = HG_NL * (i == 0 ? 3.f : 1.f) * n.M * n.M * w0 * w0 * w0 / Iref;
				// radiation: a field is small beside the wavelength of its own note,
				// so its octave and fifth leave the steel more easily than its
				// fundamental (pressure rises with frequency, up to about 900 Hz)
				const float fc = 900.f;
				float fb = std::min(n.wk[0] / (2.f * HG_PI), fc);
				for (int k = 0; k < NP; k++) {
					float fk = std::min(n.wk[k] / (2.f * HG_PI), fc);
					n.rad[k] = (k < 3 ? fk / fb : 1.f) * (i == 0 ? HG_RAD_DING[k] : HG_RAD[k]) * n.radVar[k];
				}
				n.aRef = Iref / (n.M * w0);
			}
			n.rFund = std::sqrt(n.m[0].c * n.m[0].c + n.m[0].s * n.m[0].s);
		}
		for (int b = 0; b < NBODY; b++) {
			float t = bodyT60[b] / (1.f + muteAmt * 2.f);
			body[b].setCoef(bodyFreq[b], t, sr);
			body[b].gain = 1.f / (1.f + b * 0.6f);
		}
		helm.setCoef(helmFreq, 0.42f / (1.f + muteAmt), sr);
		helmSqMw = std::sqrt(HELM_M * 2.f * HG_PI * helmFreq);
		click[0].setCoef(clickFreq[0], 0.03f, sr);
		click[1].setCoef(clickFreq[1], 0.018f, sr);
		click[0].gain = 1.f; click[1].gain = 0.6f;
		hissF.set(1800.f, 0.7f, sr);
		floorF.set(500.f, 0.5f, sr);
		airF.set(helmFreq * 1.5f, 1.2f, sr);
	}

	// ---- playing --------------------------------------------------------

	void push(int delay, int n, float vel, float pos, bool gu, bool quiet) {
		for (int q = 0; q < NQ; q++) {
			if (queue[q].on) continue;
			queue[q].on = true;
			queue[q].delay = delay;
			queue[q].note = n;
			queue[q].vel = vel;
			queue[q].pos = pos;
			queue[q].gu = gu;
			queue[q].quiet = quiet;
			return;
		}
	}

	// a hand hits field n (0 = ding) with vel 0..1; SLOP makes the hand human
	void strike(int n, float vel, bool fromRoll = false) {
		if (n < 0 || n >= nNotes || vel <= 0.f) return;
		float sl = p.slop;
		int delay = (int) (rng.uni() * sl * 0.012f * sr);
		float v = std::max(0.02f, std::min(1.2f, vel * (1.f + sl * 0.35f * rng.bi())));
		float pos = std::max(0.f, std::min(1.f, p.strike + sl * 0.3f * rng.bi()));
		push(delay, n, v, pos, false, false);
		lastNote = n;
		if (fromRoll) return;
		// the finger bounces back down
		if (rng.uni() < sl * 0.22f) {
			int d2 = delay + (int) ((0.006f + 0.018f * rng.uni()) * sr);
			push(d2, n, v * (0.12f + 0.2f * rng.uni()), std::min(1.f, pos + 0.1f), false, true);
		}
		// the hand grazes a neighbouring field on the way
		if (rng.uni() < sl * 0.08f && nNotes > 2) {
			int best = -1; float bestA = 0.f;
			for (int j = 1; j < nNotes; j++) {
				float a = adj[n][j] * (0.6f + 0.4f * rng.uni());
				if (j != n && a > bestA) { bestA = a; best = j; }
			}
			if (best >= 0) push(delay + (int) (rng.uni() * 0.004f * sr), best, v * 0.22f, 0.9f, false, true);
		}
	}

	// a palm on the shell beside the gu: the deep bass boom
	void strikeGu(float vel) {
		float sl = p.slop;
		int delay = (int) (rng.uni() * sl * 0.01f * sr);
		float v = std::max(0.02f, std::min(1.2f, vel * (1.f + sl * 0.3f * rng.bi())));
		push(delay, 0, v, 0.f, true, false);
	}

	// the nearest field to a pitch (V/OCT, 0 V = C4)
	int nearest(float voct) const {
		float target = 60.f + 12.f * voct;
		int best = 0; float bd = 1e9f;
		for (int i = 0; i < nNotes; i++) {
			float d = std::fabs(note[i].midi - target);
			if (d < bd - 1e-4f) { bd = d; best = i; }
		}
		return best;
	}

	void palm() { palmEnv = std::max(palmEnv, 0.6f); }

	void fire(const HgQueued& q) {
		float v = q.vel;
		float touch = p.touch;
		if (q.gu) {
			helmPulse[helmNext].start((int) (0.009f * sr), v * 1.4f);
			helmNext = (helmNext + 1) % NPULSE;
			bodyPulse[bodyNext].start((int) (0.004f * sr), v * 0.9f);
			bodyNext = (bodyNext + 1) % NPULSE;
			// a palm, so a low, wide skin sound
			skinF.set(450.f + 500.f * touch, 0.8f, sr);
			skinEnv = std::max(skinEnv, v * p.skin * 0.8f);
			skinDec = std::exp(-1.f / (sr * 0.012f));
			airEnv += v * 1.0f;
			for (int i = 0; i < nNotes; i++) note[i].flash = std::max(note[i].flash, 0.25f * v);
			return;
		}
		HgNote& n = note[q.note];
		// the finger: soft pad (long, lossy contact) .. hard slap (short, springy)
		// (real finger strikes on a handpan last about 0.5 to 2.5 ms; any longer
		// and the force pulse itself cancels the upper fields' fundamentals)
		float contact = 0.0016f - 0.0012f * touch;   // at unit speed on rigid steel
		int L = std::max(3, (int) (contact * (1.15f - 0.3f * std::min(v, 1.f)) * sr));
		int slot = n.conNext;
		n.conNext = (n.conNext + 1) % NPULSE;
		HgContact& c = n.con[slot];
		// where it lands: r from the centre of the field towards its rim, and an
		// angle from the field's long axis. The octave's nodal line runs across
		// the centre one way and the fifth's the other, so both vanish at r = 0
		float r = 0.08f + 0.86f * q.pos;
		float th = (0.6f + (0.15f + 0.35f * p.slop) * rng.bi());
		float sr1 = std::sin(HG_PI * std::min(r, 0.95f));
		c.phi[0] = std::cos(0.5f * HG_PI * r);
		c.phi[1] = sr1 * std::fabs(std::cos(th)) * (q.note == 0 ? 0.8f : 1.f);
		c.phi[2] = sr1 * std::fabs(std::sin(th));
		c.phi[3] = 0.25f + 0.75f * std::fabs(std::sin(1.6f * HG_PI * r + 0.4f));
		c.phi[4] = 0.2f + 0.8f * std::fabs(std::sin(2.3f * HG_PI * r + 0.9f));
		c.phi[5] = 0.2f + 0.8f * std::fabs(std::sin(3.1f * HG_PI * r + 1.7f));
		float uc = 0.f;
		for (int k = 0; k < NP; k++) uc += c.phi[k] * (n.m[2 * k].im + n.m[2 * k + 1].im) * n.invMw[k];
		float alpha = 1.5f + 0.5f * touch;             // flesh stiffens as it compresses
		float lam = 0.35f + 1.6f * (1.f - touch);      // a soft pad soaks up energy
		float mass = HG_FINGER_MASS * (0.8f + 0.5f * (1.f - touch));
		c.start(uc, v, contact, alpha, lam, mass, 40.f + 60.f * touch, sr);
		n.idle = false;
		n.flash = std::max(n.flash, std::min(1.f, v));

		bodyPulse[bodyNext].start(L, v * (0.12f + 0.3f * touch));
		bodyNext = (bodyNext + 1) % NPULSE;
		// the shell flexes and pumps a little air through the gu
		helmPulse[helmNext].start((int) (0.006f * sr), v * 0.06f);
		helmNext = (helmNext + 1) % NPULSE;
		airEnv += v * (0.3f + 0.5f * touch);

		if (!q.quiet || touch > 0.6f) {
			float fc = 700.f + 2600.f * touch + 300.f * rng.uni();
			skinF.set(fc, 0.9f, sr);
			skinEnv = std::max(skinEnv, v * p.skin * (0.25f + 0.75f * touch) * (q.quiet ? 0.4f : 1.f));
			skinDec = std::exp(-1.f / (sr * (0.002f + 0.006f * (1.f - touch))));
			// a fingernail catches the steel now and then
			if (rng.uni() < p.skin * (0.08f + 0.3f * p.slop)) nailEnv = std::max(nailEnv, v * p.skin * 0.5f);
		}
		// the shell carries a knock to the neighbours straight away
		if (p.bleed > 0.f) {
			for (int j = 0; j < nNotes; j++) {
				if (j == q.note || adj[q.note][j] < 0.2f) continue;
				HgNote& m = note[j];
				int s2 = m.pulseNext;
				m.pulseNext = (m.pulseNext + 1) % NPULSE;
				float a = adj[q.note][j];
				m.pulse[s2].start((int) (0.0025f * sr), v * p.bleed * 0.5f * a);
				for (int k = 0; k < NP; k++) m.pulseW[s2][k] = HG_AMP[k] * (k == 0 ? 1.f : 0.3f);
			}
		}
	}

	void setRoll(bool on) {
		if (on && !rollOn) { rollTimer = 0.f; rollAlt = false; }
		rollOn = on;
	}

	// ---- audio ----------------------------------------------------------

	void process(float& outL, float& outR) {
		// pending strikes
		for (int q = 0; q < NQ; q++) {
			if (!queue[q].on) continue;
			if (--queue[q].delay <= 0) {
				queue[q].on = false;
				fire(queue[q]);
			}
		}
		// thumb roll: thumb and finger alternate on the last field played
		if (rollOn) {
			rollTimer -= 1.f / sr;
			if (rollTimer <= 0.f) {
				float rate = 6.f * std::pow(4.f, p.roll);
				float jit = 1.f + (0.06f + 0.2f * p.slop) * rng.bi();
				rollTimer += jit / rate;
				float v = p.force * (rollAlt ? 0.55f : 0.42f);
				rollAlt = !rollAlt;
				strike(lastNote, v, true);
			}
		}
		if (++blockCount >= 32) { blockCount = 0; updateCoefs(); }

		// pulses into the body and the air
		float bodyIn = 0.f, helmIn = 0.f;
		for (int k = 0; k < NPULSE; k++) { bodyIn += bodyPulse[k].next(); helmIn += helmPulse[k].next(); }

		float wn = noise.bi();

		// tone fields, in two passes so every coupling sees the same instant:
		// 1) predict where every mode is going this sample (no input yet)
		const float dt = 1.f / sr;
		float dryL = 0.f, dryR = 0.f, mono = 0.f;
		float bleed = p.bleed;
		float dingOut = 0.f;
		helm.prep();
		float uH = helm.fi / helmSqMw;  // cavity, normalised
		bool live[MAXN];
		for (int i = 0; i < nNotes; i++) {
			HgNote& n = note[i];
			bool con = false, pul = false;
			for (int s = 0; s < NPULSE; s++) { con = con || n.con[s].on; pul = pul || n.pulse[s].active(); }
			live[i] = !n.idle || con || pul;
			n.ut = 0.f;
			if (!live[i]) { for (int k = 0; k < NP; k++) n.u[k] = 0.f; continue; }
			for (int k = 0; k < NM; k++) n.m[k].prep();
			for (int k = 0; k < NP; k++) {
				float st = n.m[2 * k].fi + n.m[2 * k + 1].fi;
				n.u[k] = st * n.invMw[k];
				n.ut += HG_CPL[k] * st * n.invSqMw[k];
			}
		}
		// 2) forces: fingers, the shell's own nonlinearity, the neighbours, the air
		const float bleedRate = 2.f * (2.f * HG_PI * 2.2f) * bleed * dt;   // coupling rate up to 2.2 Hz
		float helmDrive = 0.f;
		for (int i = 0; i < nNotes; i++) {
			HgNote& n = note[i];
			float sym = 0.f;
			if (bleed > 0.f)
				for (int j = 0; j < nNotes; j++) sym += adjN[i][j] * note[j].ut;
			sym *= bleedRate;
			if (!live[i]) {
				if (std::fabs(sym) < 1e-10f) { n.last = 0.f; continue; }
				for (int k = 0; k < NM; k++) n.m[k].prep();  // all zero, wakes up
				n.idle = false;
			}
			float F[NP];
			for (int k = 0; k < NP; k++) F[k] = 0.f;
			// fingers in contact
			for (int s = 0; s < NPULSE; s++) {
				HgContact& c = n.con[s];
				if (!c.on) continue;
				float uc = 0.f;
				for (int k = 0; k < NP; k++) uc += c.phi[k] * n.u[k];
				float f = c.step(uc, dt);
				for (int k = 0; k < NP; k++) F[k] += c.phi[k] * f;
			}
			// 1:2:3 internal resonance, from H = eps (u0^2 u1 + 0.7 u0 u1 u2)
			{
				// (displacements are softly limited at a few times a full-force hit:
				// a pure cubic potential has no floor, and a pile of slaps could
				// otherwise throw the shell over the top of it)
				float lim = 1.f / (3.f * n.aRef);
				// (each force is the exact gradient of the limited energy, so the
				// coupling only moves energy between the three partials)
				float d0 = 1.f / (1.f + std::fabs(n.u[0]) * lim);
				float d1 = 1.f / (1.f + std::fabs(n.u[1]) * lim);
				float d2 = 1.f / (1.f + std::fabs(n.u[2]) * lim);
				float u0 = n.u[0] * d0, u1 = n.u[1] * d1, u2 = n.u[2] * d2;
				float e = n.eps;
				F[0] -= e * (2.f * u0 * u1 + 0.7f * u1 * u2) * d0 * d0;
				F[1] -= e * (u0 * u0 + 0.7f * u0 * u2) * d1 * d1;
				F[2] -= e * 0.7f * u0 * u1 * d2 * d2;
			}
			float x[NP];
			for (int k = 0; k < NP; k++) x[k] = F[k] * dt + sym * HG_CPL[k] * n.sqMw[k];
			// knocks carried through the shell from a strike next door
			for (int s = 0; s < NPULSE; s++) {
				float pv = n.pulse[s].next();
				if (pv != 0.f) for (int k = 0; k < NP; k++) x[k] += pv * n.pulseW[s][k];
			}
			// the cavity: fundamentals change the volume, the pressure pushes back
			float hc = 2.f * (2.f * HG_PI * (i == 0 ? 5.f : 1.5f)) * dt;
			x[0] -= hc * n.sqMw[0] * uH;
			helmDrive -= hc * helmSqMw * (n.m[0].fi + n.m[1].fi) * n.invSqMw[0];
			float y = 0.f;
			for (int k = 0; k < NP; k++) {
				float st = n.m[2 * k].commit(x[k]) + n.m[2 * k + 1].commit(x[k] * n.m[2 * k + 1].gain);
				y += n.rad[k] * st;
			}
			// radiated ~ surface velocity x area; the heavy fields are the big ones,
			// so the 1/M of the velocity and the area cancel to first order
			// the loose field buzzes against itself when it swings hard
			if (i == looseNote && p.rattle > 0.f) {
				float th = 0.45f * (1.f - 0.85f * p.rattle);
				float over = y - std::max(-th, std::min(th, y));
				float hpIn = over;
				looseHp = 0.93f * (looseHp + hpIn - looseX);
				looseX = hpIn;
				y += looseHp * p.rattle * 1.6f;
			}
			n.last = y;
			bool con = false;
			for (int s = 0; s < NPULSE; s++) con = con || n.con[s].on || n.pulse[s].active();
			float a = std::fabs(n.m[0].im) + std::fabs(n.m[0].re) + std::fabs(n.m[1].im) + std::fabs(n.m[1].re);
			n.idle = !con && a < 1e-6f && std::fabs(y) < 1e-6f;
			if (n.idle) for (int k = 0; k < NM; k++) n.m[k].re = n.m[k].im = 0.f;
			n.env += (a - n.env) * 0.002f;
			n.flash *= 0.9995f;
			dryL += y * n.panL;
			dryR += y * n.panR;
			mono += y;
			if (i == 0) dingOut = y;
		}
		(void) dingOut;

		// shell knock
		float bodyOut = 0.f;
		for (int b = 0; b < NBODY; b++) bodyOut += body[b].tick(bodyIn);
		bodyOut *= 0.5f;

		// air: breath through the gu after strikes, turbulence hiss, room tone
		airEnv *= 1.f - 1.f / (0.07f * sr);
		float air = p.air;
		airF.tick(wn);
		hissF.tick(wn);
		floorF.tick(wn);
		helmIn += airF.bp * air * airEnv * 0.08f;
		float helmOut = helm.commit(helmIn + helmDrive) * p.gu * 1.4f;
		float airOut = hissF.bp * air * airEnv * 0.14f + floorF.lp * air * air * 0.0015f;

		// skin and nail
		skinF.tick(wn);
		float skinOut = skinF.bp * skinEnv * 2.4f;
		skinEnv *= skinDec;
		float nailOut = 0.f;
		if (nailEnv > 1e-5f) {
			nailOut = (wn - clickNoise) * nailEnv;
			nailEnv *= 0.92f;
		}
		clickNoise = wn;
		// palm landing on the shell (MUTE gate)
		palmF.set(220.f, 0.7f, sr);
		palmF.tick(wn);
		float palmOut = palmF.lp * palmEnv * 0.5f * (0.3f + p.skin);
		palmEnv *= 1.f - 1.f / (0.025f * sr);

		// rattler: a loose mass on a spring, driven by the shell, hitting stops
		float rattleOut = 0.f;
		if (p.rattle > 0.f) {
			float w0 = 2.f * HG_PI * rattleFreq;
			float dt = 1.f / sr;
			float acc = w0 * w0 * ((mono + bodyOut) * 0.4f - rp) - 2.f * 0.12f * w0 * rv;
			rv += acc * dt;
			rp += rv * dt;
			float gap = 0.62f * (1.f - 0.92f * p.rattle);
			float imp = 0.f;
			if (std::fabs(rp) > gap) {
				imp = std::fabs(rv) / w0;
				rattleHits++;
				rp = rp > 0.f ? gap : -gap;
				rv = -0.4f * rv;
			}
			float cin = imp * 0.6f;
			rattleOut = (click[0].tick(cin) + click[1].tick(cin)) * p.rattle * 1.2f;
		}
		else { rp = rv = 0.f; }

		float extra = bodyOut + helmOut + airOut + skinOut + nailOut * 0.6f + palmOut + rattleOut;
		dryL += extra * 0.85f;
		dryR += extra * 0.85f;
		mono += extra;

		// small room: early reflections + a 4-line FDN
		float wetL = 0.f, wetR = 0.f;
		if (p.room > 0.001f) {
			int N = (int) erBuf.size();
			erBuf[erPos] = mono;
			auto tap = [&](float ms) {
				int d = (int) (ms * 0.001f * sr);
				int i = erPos - d;
				while (i < 0) i += N;
				return erBuf[i];
			};
			float e1 = tap(3.1f), e2 = tap(7.3f), e3 = tap(11.9f), e4 = tap(16.8f), e5 = tap(23.5f);
			erPos = (erPos + 1) % N;
			wetL += 0.5f * e1 + 0.33f * e3 + 0.22f * e5;
			wetR += 0.42f * e2 + 0.27f * e4;
			float rt = 0.35f + 0.7f * p.room;
			float d[4];
			for (int k = 0; k < 4; k++) {
				int rd = fdnPos[k];
				d[k] = fdnBuf[k][rd];
				fdnG[k] = std::pow(10.f, -3.f * (fdnLen[k] / sr) / rt);
				fdnLp[k] += (d[k] - fdnLp[k]) * 0.45f;
				d[k] = fdnLp[k] * fdnG[k];
			}
			float h0 = 0.5f * (d[0] + d[1] + d[2] + d[3]);
			float h1 = 0.5f * (d[0] - d[1] + d[2] - d[3]);
			float h2 = 0.5f * (d[0] + d[1] - d[2] - d[3]);
			float h3 = 0.5f * (d[0] - d[1] - d[2] + d[3]);
			float in = (e2 + e4) * 0.35f;
			float hs[4] = {h0 + in, h1 + in, h2 - in, h3 - in};
			for (int k = 0; k < 4; k++) {
				fdnBuf[k][fdnPos[k]] = hs[k];
				fdnPos[k] = (fdnPos[k] + 1) % fdnLen[k];
			}
			wetL += 0.6f * (d[0] + d[2]);
			wetR += 0.6f * (d[1] + d[3]);
		}
		float wet = p.room * 0.9f;
		float dry = 1.f - 0.25f * p.room;
		float L = (dryL * dry + wetL * wet) * 4.5f;
		float R = (dryR * dry + wetR * wet) * 4.5f;
		outL = 10.f * std::tanh(L * 0.1f);
		outR = 10.f * std::tanh(R * 0.1f);
		outPeak = std::max(std::fabs(outL), std::fabs(outR));
	}

	void silence() {
		for (int i = 0; i < MAXN; i++) {
			for (int k = 0; k < NM; k++) note[i].m[k].re = note[i].m[k].im = 0.f;
			note[i].last = note[i].env = 0.f;
		}
	}
};

}  // namespace hangover
