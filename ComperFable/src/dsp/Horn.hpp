// Soloist's horns: tenor sax, trumpet and trombone as physical models. No Rack includes.
//
// One player, one horn (monophonic, as the real things are). The engine runs at 2x the host rate below 50 kHz.
//
// TENOR SAX  A self-oscillating single reed on a truncated cone. The reed is a pressure-controlled valve
//            (flow = opening * sqrt(pressure difference), the opening closing as the mouth pressure rises and
//            curling shut softly on the lay). The cone is a travelling-wave line whose far end reflects the lows
//            (the tone-hole lattice) and lets the highs out; the missing tip of the cone is the inertance that
//            makes a cone play all harmonics. Nothing forces the pitch: the note is whatever the reed and the
//            bore agree on; the player's embouchure leans on the note meant (a little of the reed's give
//            moves at the note's frequency), which keeps the horn in its low register and in tune.
// TRUMPET,   The player's lips are a valve that opens and closes at the note's frequency (the lips are driven:
// TROMBONE   their motion is given, the air flow through them is solved against the mouthpiece pressure each
//            sample). The bore is a travelling-wave line with a mouthpiece resonance, losses, a bell that
//            reflects the lows and radiates the highs, and the wave steepening of a long narrow tube that turns
//            a loud note brassy. Because the lips and the bore are separate, lipping a note away from its slot
//            goes stuffy, as it does on the horn; a trumpet scoop or fall is played half-valve, a trombone's on
//            the slide.
//
// The caller (the Soloist line engine) plays it like a player would: noteOn (tongued or slurred, with a scoop),
// noteOff (clean or with a fall), and the continuous controls breath, bend, vibrato, growl and air.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace horn {

enum Kind { TENOR = 0, TRUMPET, TROMBONE, NUM_KINDS };
static const char* const KIND_NAMES[NUM_KINDS] = {"Tenor sax", "Trumpet", "Trombone"};
// sounding (concert) range of each horn, MIDI notes: comfortable low, comfortable high, extreme high
static const int RANGE_LO[NUM_KINDS] = {44, 52, 40};
static const int RANGE_HI[NUM_KINDS] = {76, 84, 72};
static const int RANGE_TOP[NUM_KINDS] = {81, 89, 77};

struct Note {
	double pitch = 60;      // MIDI note, fractions allowed
	double vel = 0.7;       // 0..1 how hard the note is blown
	bool slur = false;      // true: no tongue, the breath carries on from the last note
	double tongue = 0.5;    // 0 breath attack ("hoo") .. 1 hard tongue ("ta")
	double scoop = 0;       // semitones below the note that it starts from (0 = none)
	bool gliss = false;     // slurs only: slide the whole way (trombone slide, sax/trumpet smear)
};

static inline double hz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }
static inline double clampd(double x, double a, double b) { return x < a ? a : (x > b ? b : x); }
static inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

struct Noise {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v ? v : 1u; }
	inline double uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double)s * (1.0 / 4294967296.0); }
	inline double bi() { return 2.0 * uni() - 1.0; }
};

struct OnePole {            // y += k (x - y)
	double k = 1, y = 0;
	void setHz(double f, double sr) { k = 1.0 - std::exp(-2.0 * M_PI * f / sr); }
	void setTime(double sec, double sr) { k = sec <= 0 ? 1.0 : 1.0 - std::exp(-1.0 / (sec * sr)); }
	inline double lp(double x) { y += k * (x - y); return y; }
};

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	void set(double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
	void lp(double f, double q, double sr) {
		double w = 2 * M_PI * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void hp(double f, double q, double sr) {
		double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void peak(double f, double q, double gdb, double sr) {
		double A = std::pow(10.0, gdb / 40), w = 2 * M_PI * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2 * q);
		set(1 + al * A, -2 * std::cos(w), 1 - al * A, 1 + al / A, -2 * std::cos(w), 1 - al / A);
	}
	void hishelf(double f, double gdb, double sr) {
		double A = std::pow(10.0, gdb / 40), w = 2 * M_PI * std::min(f, 0.45 * sr) / sr, c = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0), sA = 2 * std::sqrt(A) * al;
		set(A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
		    (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
	}
	inline double process(double x) {
		double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1; x1 = x; y2 = y1; y1 = y; return y;
	}
	inline double hist() const { return b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; }   // y = b0 x + hist()
	// phase delay in samples at w (rad/sample)
	double phaseDelay(double w) const {
		double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
		double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2), dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
		return -(std::atan2(ni, nr) - std::atan2(di, dr)) / w;
	}
	double mag(double w) const {
		double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
		double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2), dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
		return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
	}
	void clear() { x1 = x2 = y1 = y2 = 0; }
};

struct Line {               // travelling-wave delay with 4-point Lagrange reads
	static const int N = 8192;
	float buf[N]; int w = 0;
	void clear() { std::memset(buf, 0, sizeof buf); w = 0; }
	inline void write(double x) { buf[w] = (float)x; w = (w + 1) & (N - 1); }
	inline double read(double d) const {        // d >= 2 samples back from the next write
		if (d < 2.0) d = 2.0; else if (d > N - 4) d = N - 4;
		int di = (int)d; double f = d - di;
		int i = w - di;
		double xm = buf[(i + 1) & (N - 1)], x0 = buf[i & (N - 1)], x1 = buf[(i - 1) & (N - 1)], x2 = buf[(i - 2) & (N - 1)];
		// Lagrange through delays di-1, di, di+1, di+2 at di+f
		double c0 = -f * (f - 1) * (f - 2) / 6.0, c1 = (f + 1) * (f - 1) * (f - 2) / 2.0, c2 = -(f + 1) * f * (f - 2) / 2.0, c3 = (f + 1) * f * (f - 1) / 6.0;
		return c0 * xm + c1 * x0 + c2 * x1 + c3 * x2;
	}
};

// Everything that makes one horn itself.
struct Voicing {
	// shared
	double reflHz, reflQ;       // far end / bell: lows reflect below this, highs get out
	double reflKey;             // how much the reflection corner follows the note (0 fixed .. 1 tracks)
	double loopGain;            // reflection strength at the bottom of the range
	double loopGainHi;          // ... at the top
	double noise;               // breath noise in the tone
	double vibCents, vibHz;
	double outGain;
	double eq[3][3];            // body peaks {Hz, Q, dB}
	double airHz;               // top of the radiated sound
	// brass
	double mpHz, mpQ, mpDb;     // mouthpiece resonance
	double lipArea;             // how much air the lips pass
	double brassy;              // wave steepening
	// sax
	double zeta, zetaHi;        // reed flow (embouchure) low / high notes
	double betaLo, betaHi;      // cone truncation ratio low / high notes
	double reedSoft;
};
static const Voicing VOICINGS[NUM_KINDS] = {
	// TENOR
	{1150, 0.62, 0.35, 0.955, 0.90, 0.055, 32, 5.3, 1.0, {{520, 1.4, 2.5}, {1350, 1.6, 3.0}, {3000, 1.0, 4.5}}, 9000,
	 0, 1, 0, 0, 0, 0.50, 0.42, 0.16, 0.30, 0.10},
	// TRUMPET
	{2000, 0.75, 0.25, 0.90, 0.80, 0.020, 24, 5.6, 1.0, {{1200, 1.2, 2.0}, {2400, 1.5, 2.5}, {3600, 2.0, 1.0}}, 11000,
	 1100, 0.9, 6.0, 0.03, 0.55, 0, 0, 0, 0, 0},
	// TROMBONE
	{1000, 0.75, 0.25, 0.90, 0.80, 0.022, 28, 5.0, 1.0, {{520, 1.2, 2.0}, {1100, 1.5, 2.0}, {2500, 1.5, 1.0}}, 8500,
	 600, 0.9, 6.0, 0.033, 0.80, 0, 0, 0, 0, 0},
};

struct Horn {
	// ---- performance controls (set any time; smoothed inside)
	double breath = 1.0;     // 0..1 phrase dynamic, multiplies each note's vel
	double bend = 0.0;       // semitones
	double vibrato = 0.0;    // 0..1 depth (comes in after the start of a note, as players do)
	double growl = 0.0;      // 0..1
	double air = 0.0;        // 0..1 breathiness (sax: subtone)

	Kind kind = TENOR;
	double sr = 48000, fs = 96000; int os = 2;
	Voicing V = VOICINGS[TENOR];
	Noise rng;

	// ---- note state
	bool gate = false; bool everPlayed = false;
	double pitch = 60, pitchFrom = 60;          // target note, and where the glide started
	double glide = 1.0, glideRate = 0;          // 0..1 progress of a slur glide
	double vel = 0.7, tongue = 0.5;
	double scoopAmt = 0, scoopT = 0, scoopLen = 0.12;
	double fallAmt = 0, fallT = 0, fallLen = 0.3; bool falling = false;
	double age = 0;                             // seconds since note-on
	double tongueGap = 0;                       // seconds of closed tongue still to come
	double env = 0;                             // mouth pressure envelope 0..1
	double over = 0;                            // tongue overshoot
	double valve = 0;                           // trumpet valve / sax key transition (dips the bore)
	double silence = 1e9;                       // seconds of no output
	// slow things
	double vibPh = 0, vibHzNow = 5.3, drift = 0, driftTarget = 0, humPh = 0, flutPh = 0, shim = 0;
	OnePole dynLP, bendLP, growlLP, airLP, vibLP, noiseLP, noiseHP, pmLP;
	// ---- the bore
	Line line, tip; Biquad refl, dcHP, mp; OnePole lossLP, tipLP;
	double D = 100, Dtarget = 100, Dstep = 0;   // loop delay
	double D2 = 20, D2target = 20, D2step = 0;  // sax: the short side of the cone (mouthpiece to tip)
	double gLoop = 0.9;
	double lipPh = 0, lipInc = 0, lipIncT = 0;
	double p_prev = 0, wInt = 0, dPrev = 0.3;   // sax junction
	double zetaNow = 0.4, gammaNow = 0, betaNow = 0.2, gi = 0.01, softNow = 0.1;
	double lipA = 0, lipH = 0, pmNow = 0, steep = 0, noiseAmt = 0, lipSoft = 0.05;
	double halfValve = 0;                       // 0 slotted .. 1 bore follows the lips, weak slot
	double boreSemis = 0;                       // where the bore (valves/slide) currently is, relative to pitch target
	// ---- output
	Biquad eq[3], airLPF, outHP; double outGainNow = 1;
	double fir[32]; double hbuf[64]; int hpos = 0;
	int ctl = 0;
	static const int CTL = 16;

	Horn() { setSampleRate(48000); }

	void setSampleRate(double s) {
		sr = s; os = s < 50000 ? 2 : 1; fs = sr * os;
		// half-band decimator (Kaiser windowed sinc, 31 taps)
		const int M = 31; double beta = 8.0, sum = 0;
		auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
		for (int i = 0; i < M; i++) {
			double n = i - (M - 1) / 2.0, sinc = n == 0 ? 0.5 : std::sin(M_PI * n * 0.5) / (M_PI * n);
			double r = 2.0 * i / (M - 1) - 1.0; fir[i] = sinc * i0(beta * std::sqrt(1 - r * r)) / i0(beta); sum += fir[i];
		}
		for (int i = 0; i < M; i++) fir[i] /= sum;
		fir[31] = 0; std::memset(hbuf, 0, sizeof hbuf); hpos = 0;
		dynLP.setTime(0.018, fs / CTL); bendLP.setTime(0.012, fs / CTL); growlLP.setTime(0.03, fs / CTL); airLP.setTime(0.05, fs / CTL); vibLP.setTime(0.08, fs / CTL);
		setKind(kind);
	}
	void setKind(Kind k) {
		kind = k; V = VOICINGS[k];
		line.clear(); tip.clear(); tipLP.y = 0; tipLP.setHz(5000, fs); refl.clear(); dcHP.clear(); mp.clear(); lossLP.y = 0;
		for (int i = 0; i < 3; i++) { eq[i].peak(V.eq[i][0], V.eq[i][1], V.eq[i][2], fs); eq[i].clear(); }
		airLPF.lp(V.airHz, 0.7, fs); airLPF.clear(); outHP.hp(k == TRUMPET ? 110 : 55, 0.7, fs); outHP.clear();
		dcLP.setHz(4.0, fs); dcLP.y = 0; noiseLP.setHz(k == TENOR ? 5500 : 4200, fs); noiseHP.setHz(350, fs);
		mp.peak(V.mpHz > 0 ? V.mpHz : 1000, V.mpQ, V.mpDb, fs);
		pmLP.setHz(900, fs); radHP.y = 0; dSlew = 1.0 - std::exp(-1.0 / (0.0012 * fs));
		env = over = 0; gate = false; p_prev = wInt = 0; dPrev = 0.3; silence = 1e9; everPlayed = false; glide = 1; valve = 0; halfValve = 0;
		control(true);
	}
	bool sounding() const { return gate || silence < 0.25; }

	// ---------------------------------------------------------------- playing
	void noteOn(const Note& n) {
		bool wasOn = env > 0.05;                    // still sounding (even if just released): the tongue stops it first
		bool slur = n.slur && gate && wasOn;
		double from = currentPitch();
		vel = clampd(n.vel, 0.02, 1.0); tongue = clampd(n.tongue, 0, 1);
		if (slur) {
			pitchFrom = from; pitch = n.pitch; glide = 0;
			double iv = std::fabs(pitch - pitchFrom), t;
			if (kind == TROMBONE) t = n.gliss ? 0.035 + 0.022 * iv : 0.03;
			else if (kind == TRUMPET) t = n.gliss ? 0.05 + 0.02 * iv : 0.012;
			else t = n.gliss ? 0.04 + 0.018 * iv : 0.007;
			glideRate = 1.0 / std::max(0.002, t);
			if (!n.gliss) {
				if (kind == TROMBONE) { tongueGap = 0.0; over = -0.45; }       // legato tongue: a dip, not a stop
				else { valve = 1.0; if (kind == TENOR && iv > 4.5) { valve = 1.8; over = -0.45; } }   // valve / key change; a wide sax slur gets a breath of tongue so the low register speaks
			} else if (kind != TROMBONE) halfValve = 1.0;
		} else {
			pitch = pitchFrom = n.pitch; glide = 1;
			tongueGap = wasOn ? 0.012 + 0.010 * tongue : 0.0;
			if (!wasOn) { env = 0; }
			over = 0.15 + 0.45 * tongue;
			age = 0; vibPh = rng.uni() * 0.3; vibHzNow = V.vibHz * (1.0 + 0.06 * rng.bi());
			lipPh = 0;
		}
		if (slur) age = std::min(age, 0.12);       // vibrato eases off through a slur
		scoopAmt = clampd(n.scoop, 0, 7); scoopT = 0; scoopLen = 0.10 + 0.06 * scoopAmt;
		falling = false; fallAmt = 0; fallT = 0;
		gate = true; everPlayed = true; silence = 0; kick = 1.0;
	}
	// fall: semitones the note drops (or rises, if negative: a "doit") as it dies away. 0 = clean release.
	void noteOff(double fall = 0) {
		if (!gate) return;
		if (fall != 0) { falling = true; fallAmt = fall; fallT = 0; fallLen = 0.16 + 0.035 * std::fabs(fall); }
		gate = false;
	}
	void allOff() { gate = false; falling = false; }

	double currentPitch() const { double g = glide * glide * (3 - 2 * glide); return pitchFrom + (pitch - pitchFrom) * g; }

	// ---------------------------------------------------------------- control rate
	void control(bool init = false) {
		double dt = CTL / fs;
		double dyn = dynLP.lp(clampd(vel * clampd(breath, 0, 1), 0.0, 1.0));
		double bnd = bendLP.lp(bend), grw = growlLP.lp(clampd(growl, 0, 1)), ar = airLP.lp(clampd(air, 0, 1));
		if (init) { dynLP.y = dyn = 0.5; }
		age += dt;
		// --- breath envelope
		if (gate) {
			if (tongueGap > 0) { tongueGap -= dt; env += (0 - env) * (1 - std::exp(-dt / 0.003)); }
			else {
				double rise = kind == TENOR ? lerp(0.030, 0.006, tongue) : lerp(0.035, 0.008, tongue);
				if (kind == TENOR) rise = std::max(rise, 3.0 / hz(pitch));      // a low reed note takes a few periods to speak
				env += (1 - env) * (1 - std::exp(-dt / rise));
			}
			over *= std::exp(-dt / (over < 0 ? 0.035 : 0.030));
		} else {
			double rel = falling ? fallLen * 0.75 : (kind == TENOR ? 0.035 : 0.028);
			env *= std::exp(-dt / rel); over *= std::exp(-dt / 0.02);
			if (falling) fallT += dt;
		}
		// --- pitch
		if (glide < 1) { glide = std::min(1.0, glide + glideRate * dt); if (glide >= 1) halfValve = std::min(halfValve, 0.999); }
		double sc = 0; if (scoopAmt > 0) { scoopT += dt; double x = clampd(scoopT / scoopLen, 0, 1); sc = -scoopAmt * (1 - x * x * (3 - 2 * x)); if (x >= 1) scoopAmt = 0; }
		double fl = 0; if (falling) { double x = clampd(fallT / fallLen, 0, 1.5); fl = -fallAmt * x * x; }
		// vibrato: comes in after the note has started
		double vdepth = vibLP.lp(clampd(vibrato, 0, 1) * clampd((age - 0.16) / 0.35, 0, 1));
		vibPh += vibHzNow * (1.0 + 0.25 * vdepth) * dt; if (vibPh > 1) vibPh -= 1;
		double vs = std::sin(2 * M_PI * vibPh);
		double vib = vdepth * V.vibCents * 0.01 * (vs + 0.12 * std::sin(4 * M_PI * vibPh + 0.6));
		// drift: the wander of a held note
		if (rng.uni() < dt / 0.25) driftTarget = 0.045 * rng.bi();
		drift += (driftTarget - drift) * (1 - std::exp(-dt / 0.35));
		double blip = 0;
		if (kind != TENOR && age < 0.12) blip = -(0.10 + 0.22 * tongue) * std::exp(-age / 0.014);
		double lipBendTotal = bnd + sc + fl;                     // what the player does away from the fingered note
		double pLip = currentPitch() + lipBendTotal + vib + drift + blip;
		// where the bore is: sax and trombone follow the bend; the trumpet stays in its slot for small bends
		double hvT = 0;
		if (kind == TRUMPET) hvT = clampd((std::fabs(lipBendTotal) - 0.6) / 0.8, 0, 1);
		if (glide < 1 && halfValve > 0) hvT = 1; else if (glide >= 1 && halfValve > 0 && hvT < halfValve) halfValve = std::max(hvT, halfValve - dt / 0.03);
		if (hvT > halfValve) halfValve = std::min(hvT, halfValve + dt / 0.02); else if (kind == TRUMPET && glide >= 1) halfValve = std::max(hvT, halfValve - dt / 0.03);
		if (kind == TENOR && glide >= 1) halfValve = std::max(0.0, halfValve - dt / 0.03);
		double pBore;
		if (kind == TRUMPET) pBore = lerp(currentPitch() + 0.3 * vib, pLip, halfValve);
		else pBore = pLip;
		valve *= std::exp(-dt / (kind == TRUMPET ? 0.009 : 0.005));
		double f0 = hz(pLip), fb = hz(pBore);
		f0 = clampd(f0, 30, 0.2 * fs); fb = clampd(fb, 30, 0.2 * fs);
		double key = clampd((pBore - RANGE_LO[kind]) / (double)(RANGE_TOP[kind] - RANGE_LO[kind]), 0, 1);
		// --- growl + flutter
		double hum = 0, flut = 0;
		if (grw > 0.001) {
			double fh = clampd(f0 * 0.4307, 85, 230);
			humPh += fh * dt; if (humPh > 1) humPh -= 1;         // advanced at control rate: a rough tone is the point
			flutPh += 27.0 * dt; if (flutPh > 1) flutPh -= 1;
			hum = grw; flut = std::max(0.0, grw - 0.55) / 0.45;
		}
		humAmt = hum; flutAmt = flut; humInc = clampd(f0 * 0.4307, 85, 230) / fs;
		// --- voicing of this moment
		double e = clampd(env * (1 + over), 0, 1.6);
		double w0 = 2 * M_PI * fb / fs;
		double rHz = V.reflHz * std::pow(fb / hz(RANGE_LO[kind] + 12), V.reflKey);
		gLoop = lerp(V.loopGain, V.loopGainHi, key);
		if (kind == TENOR) {
			rHz *= (1.0 - 0.30 * ar) * (0.80 + 0.35 * dyn);
			refl.lp(rHz, V.reflQ, fs);
			{	// cone truncation by fingered note, kept off the values that would silence one harmonic outright
				double kf = clampd((std::floor(pitch + 0.5) - RANGE_LO[kind]) / (double)(RANGE_TOP[kind] - RANGE_LO[kind]), 0, 1);
				double xb = 1.0 / lerp(V.betaLo, V.betaHi, kf), fl = std::floor(xb);
				betaNow = 1.0 / (fl + 0.5 + 0.5 * (xb - fl - 0.5));
			}
			double z = lerp(V.zeta, V.zetaHi, key) * (1.0 + 0.25 * ar);
			zetaNow = z * clampd(e * 3.0, 0, 1);                    // the tongue lets the reed go
			double es = clampd(env * (1 + (over < 0 ? 1.2 : 0.35) * over), 0, 1.25);
			gammaNow = es * (0.30 + 0.07 * key + 0.40 * dyn * (0.60 + 0.40 * key)) * (1.0 - 0.08 * ar);
			kick *= std::exp(-dt / 0.07);
			lockAmt = 0.09 + 0.30 * (1 - key) * (0.3 + dyn) + 0.22 * kick;
			lipIncT = f0 / fs; saxLevel = 0.25 + 0.75 * std::pow(dyn, 1.2);
			softNow = V.reedSoft * (1.0 + 2.2 * ar) * (1.25 - 0.5 * dyn);
			noiseAmt = V.noise * (1.0 + 5.0 * ar) * (0.6 + 0.8 * (1 - dyn));
			gLoop *= (1.0 - 0.45 * std::min(valve, 1.9)) * (1.0 - 0.25 * halfValve);
			double tune = tuneCents(pBore, dyn);
			double T = (fs / fb) * std::pow(2.0, tune / 1200.0);
			D2target = std::max(2.5, betaNow * T - tipDelay(w0));
			Dtarget = T - (D2target + tipDelay(w0)) - refl.phaseDelay(w0) - lossDelay(w0);
		} else {
			rHz *= (1.0 - 0.2 * ar);
			refl.lp(rHz, V.reflQ, fs);
			mp.peak(V.mpHz, V.mpQ, V.mpDb, fs); radHP.setHz(kind == TRUMPET ? 1250.0 : 620.0, fs);
			// lips: louder = wider swing and a tighter rest position, so they shut for more of each cycle
			double b = dyn;
			lipA = V.lipArea * (0.35 + 1.25 * b) * clampd(e * 1.5, 0, 1.15);
			lipH = V.lipArea * (0.20 - 1.20 * std::pow(b, 0.8) + 0.14 * ar);
			lipSoft = V.lipArea * (0.03 + 0.10 * ar + 0.08 * (1 - b));
			pmNow = e * (0.16 + 0.84 * std::pow(b, 1.35));
			steep = V.brassy * std::pow(b, 2.6) * (1.0 - 0.5 * halfValve) * 13.0 / std::pow(w0, 0.75); steepMax = 0.16 * fs / fb;
			noiseAmt = V.noise * (1.0 + 6.0 * ar) * (0.5 + 1.0 * (1 - b)) + 0.045 * halfValve * (kind == TRUMPET ? 1 : 0);
			gLoop *= (1.0 - 0.55 * halfValve) * (1.0 - 0.5 * valve);
			lipIncT = f0 / fs; brassLevel = 0.30 + 0.70 * b;
			Dtarget = fs / fb - refl.phaseDelay(w0) - lossDelay(w0);   // (the DC block's small lead is left alone: correcting it at the fundamental would detune every harmonic above)
		}
		Dtarget = clampd(Dtarget, 4.0, Line::N - 8.0);
		if (init || !everPlayed) { D = Dtarget; D2 = D2target; lipInc = lipIncT; }

		lipIncStep = (lipIncT - lipInc) / CTL;
		// loudness: even across the range
		outGainNow = V.outGain * levelComp(pBore);
		{ double c = (kind == TENOR ? 750.0 : (kind == TRUMPET ? 1100.0 : 650.0)) * std::pow(2.0, 4.6 * dyn * clampd(e, 0.3, 1.2)) * (1.0 - 0.4 * ar) * (0.7 + 0.6 * key);
		  dynLPF.setHz(std::min(c, 0.4 * fs), fs); dynLPF2.k = dynLPF.k; }
	}
	double humAmt = 0, flutAmt = 0, humInc = 0, humPhA = 0, lipIncStep = 0;
	double lossK = 0.35;
	double lossDelay(double w) const {     // phase delay of the loss one-pole y += k (x - y)
		double k = lossKNow(); double a = 1 - k;
		return std::atan2(a * std::sin(w), 1 - a * std::cos(w)) / w;
	}
	double tipDelay(double w) const { double a = 1 - tipLP.k; return std::atan2(a * std::sin(w), 1 - a * std::cos(w)) / w; }
	double lossKNow() const { return kind == TENOR ? 0.80 : (kind == TRUMPET ? 0.75 : 0.62); }
	double levelComp(double p) const {
		static const double LEVEL_DB[NUM_KINDS][3] = {{-1.5, 0, 0}, {0, -2, 0}, {-1, -2, 0}};   // a local table: one copy, no link-time symbol
		double key = clampd((p - RANGE_LO[kind]) / (double)(RANGE_TOP[kind] - RANGE_LO[kind]), 0, 1);
		return std::pow(10.0, LEVEL_DB[kind][0] / 20.0 + (LEVEL_DB[kind][1] / 20.0) * key + (LEVEL_DB[kind][2] / 20.0) * key * key);
	}
	double tuneCents(double p, double) const { double x = clampd((p - 68.0) / 12.0, 0, 1.2); return 8.0 * x; }   // the top of the sax sits sharp; pull the bore down

	// ---------------------------------------------------------------- audio rate
	static inline double softpos(double x, double s) { return 0.5 * (x + std::sqrt(x * x + s * s)); }

	inline double tickSax() {
		// a cone played at its small end behaves as two tubes side by side at the reed: the long one down to the
		// open tone holes, and a short one standing in for the missing tip of the cone
		double x = line.read(D);
		double low = refl.process(x);
		double r1 = -gLoop * lossLP.lp(low);                            // back from the tone holes
		double r2 = -0.93 * tipLP.lp(tip.read(D2));                    // back from the tip
		double nz = noiseLP.lp(rng.bi()); nz -= noiseHP.lp(nz);
		humPhA += humInc; if (humPhA > 1) humPhA -= 1;
		double hm = humAmt > 0 ? std::sin(2 * M_PI * humPhA) + 0.5 * std::sin(4 * M_PI * humPhA + 1.0) : 0;
		double gam = gammaNow * (1.0 + 0.30 * humAmt * hm) + shimmer();
		lipPh += lipInc; if (lipPh >= 1) lipPh -= 1;
		// the player's embouchure and throat favour the note they mean: a little of the reed's give moves with it
		double zt = zetaNow * (1.0 + noiseAmt * 9.0 * nz) * (1.0 + 0.25 * humAmt * hm) * (1.0 + lockAmt * std::cos(2 * M_PI * lipPh));
		if (zt < 0) zt = 0;
		// reed against the mouthpiece: d + u(d) = rhs, d = mouth pressure - mouthpiece pressure
		double rhs = gam - r1 - r2;
		double d = dPrev, s2 = softNow * softNow;
		for (int it = 0; it < 4; it++) {
			double o = 1.0 - d, sq = std::sqrt(o * o + s2), op = 0.5 * (o + sq), dop = -0.5 * (1.0 + o / sq);
			double m = std::sqrt(std::sqrt(d * d + 1e-4)), sg = d / m, dsg = (1.0 - 0.5 * d * d / (d * d + 1e-4)) / m;
			double F = d + zt * op * sg - rhs, dF = 1.0 + zt * (dop * sg + op * dsg);
			if (dF < 0.2) dF = 0.2;
			double step = F / dF; if (step > 0.5) step = 0.5; else if (step < -0.5) step = -0.5;
			d -= step;
		}
		dPrev = d;
		double p = gam - d;
		line.write(p - r1); tip.write(p - r2);
		// what gets out: the highs the tone holes do not reflect, plus some of everything from the open holes
		return (x - 0.86 * low) * 0.9 * saxLevel;
	}
	inline double shimmer() { shim += 0.002 * (rng.bi() * 0.02 - shim); return shim; }

	inline double tickBrass() {
		double x = line.read(D);
		// loud waves steepen on the way down the tube; what reaches the bell and gets out carries that edge
		double xr = steep > 0 ? line.read(D - clampd(steep * x, -steepMax, steepMax)) : x;
		double lowpart = refl.process(x);
		double r = lossLP.lp(lowpart); r = gLoop * (r - dcLP.lp(r));             // reflected by the bell, back at the lips
		lipPh += lipInc; if (lipPh >= 1) lipPh -= 1;
		humPhA += humInc; if (humPhA > 1) humPhA -= 1;
		double hm = humAmt > 0 ? std::sin(2 * M_PI * humPhA) + 0.5 * std::sin(4 * M_PI * humPhA + 1.0) : 0;
		double fl = flutAmt > 0 ? (0.5 + 0.5 * std::sin(2 * M_PI * flutPh)) : 1.0;
		double nz = noiseLP.lp(rng.bi()); nz -= noiseHP.lp(nz);
		double S = softpos(lipH + lipA * std::sin(2 * M_PI * lipPh + 0.8 * std::sin(2 * M_PI * lipPh)) + 0.35 * humAmt * lipA * hm, lipSoft);
		S *= (1.0 + noiseAmt * 9.0 * nz);
		if (S < 0) S = 0;
		double pm = pmNow * (1.0 + 0.25 * humAmt * hm) * (1.0 - 0.7 * flutAmt * (1 - fl)) + shimmer();
		// lips against the mouthpiece: u = S sqrt(pm - p), p = 2 r + hist + b0 u
		double h = mp.hist(), dd = pm - 2.0 * r - h, S2 = S * S, b0 = mp.b0;
		double u = 0.5 * (-S2 * b0 + std::sqrt(S2 * S2 * b0 * b0 + 4.0 * S2 * std::fabs(dd)));
		if (dd < 0) u = -u;
		double zu = mp.process(u);
		line.write(r + zu);
		dbg[0] = u; dbg[1] = x; dbg[2] = S; dbg[3] = r;
		double t = xr - radHP.lp(xr);
		return t * 9.0 * brassLevel;                                     // the bell lets out what it does not reflect
	}
	double dbg[4] = {0, 0, 0, 0};
	double brassLevel = 1.0;
	double kick = 0, dSlew = 0.01;
	double steepScale = 1.0, steepMax = 1.0, saxLevel = 1.0, lockAmt = 0.08;
	OnePole radHP, dynLPF, dynLPF2, dcLP;

	inline double tick() {
		if (ctl <= 0) { control(); ctl = CTL; steepScale = (fs / 96000.0) * 60.0; }
		ctl--;
		D += (Dtarget - D) * dSlew; D2 += (D2target - D2) * dSlew; lipInc += lipIncStep;   // the bore never jumps: fingers and valves take a moment
		double y = kind == TENOR ? tickSax() : tickBrass();
		for (int i = 0; i < 3; i++) y = eq[i].process(y);
		y = outHP.process(airLPF.process(dynLPF2.lp(dynLPF.lp(y))));
		return y * outGainNow;
	}

	double process() {
		if (!sounding()) return 0.0;
		double y;
		if (os == 2) {
			hbuf[hpos] = hbuf[hpos + 32] = tick(); hpos = (hpos + 1) & 31;
			hbuf[hpos] = hbuf[hpos + 32] = tick(); hpos = (hpos + 1) & 31;
			const double* h = hbuf + hpos; y = 0;                      // h[0] oldest .. h[31] newest
			for (int i = 0; i < 31; i++) y += fir[i] * h[i + 1];
		} else y = tick();
		if (!(y == y) || std::fabs(y) > 50.0) { setKind(kind); y = 0; }  // never let a blown-up bore out
		if (!gate) { if (std::fabs(y) < 1e-4) silence += 1.0 / sr; else silence = 0; }
		return y;
	}
};

// A small club room around the horn: a few early reflections and a short tail, mono in, stereo out.
struct Room {
	static const int N = 16384;
	std::vector<float> in; int w = 0; double sr = 48000;
	struct Tap { int d; float gl, gr; }; Tap taps[8];
	static const int NL = 4; std::vector<float> dl[NL]; int dn[NL], dp[NL]; double lpz[NL], fb = 0.7, damp = 0.35;
	OnePole inLP;
	Room() { setSampleRate(48000); }
	void setSampleRate(double s) {
		sr = s; in.assign(N, 0.f); w = 0;
		const double ms[8] = {7.3, 11.9, 17.1, 23.6, 31.3, 39.8, 47.2, 58.9}, g[8] = {0.50, 0.42, 0.36, 0.30, 0.25, 0.21, 0.17, 0.14}, pan[8] = {0.25, 0.8, 0.4, 0.1, 0.7, 0.95, 0.3, 0.6};
		for (int i = 0; i < 8; i++) { taps[i].d = std::min(N - 1, (int)(ms[i] * 0.001 * sr)); taps[i].gl = (float)(g[i] * std::cos(pan[i] * M_PI / 2)); taps[i].gr = (float)(g[i] * std::sin(pan[i] * M_PI / 2)); }
		const double lm[NL] = {29.7, 37.1, 41.9, 53.3};
		for (int i = 0; i < NL; i++) { dn[i] = (int)(lm[i] * 0.001 * sr) | 1; dl[i].assign(dn[i], 0.f); dp[i] = 0; lpz[i] = 0; }
		inLP.setHz(6500, sr); inLP.y = 0; setSize(0.5);
	}
	void setSize(double z) { fb = 0.55 + 0.30 * clampd(z, 0, 1); }       // tail from about 0.4 s to 1.2 s
	// amount 0..1: how much room is heard
	inline void process(double x, double amount, double& L, double& R) {
		in[w] = (float)x;
		double el = 0, er = 0;
		for (int i = 0; i < 8; i++) { float v = in[(w - taps[i].d) & (N - 1)]; el += taps[i].gl * v; er += taps[i].gr * v; }
		w = (w + 1) & (N - 1);
		double o[NL], sum = 0;
		for (int i = 0; i < NL; i++) { o[i] = dl[i][dp[i]]; lpz[i] += (1 - damp) * (o[i] - lpz[i]); o[i] = lpz[i]; sum += o[i]; }
		double feed = inLP.lp(x + 0.6 * (el + er)) * 0.5;
		for (int i = 0; i < NL; i++) { double v = feed + fb * (o[i] - 0.5 * sum); dl[i][dp[i]] = (float)v; if (++dp[i] >= dn[i]) dp[i] = 0; }   // Householder mix
		double tl = o[0] - o[2] + 0.6 * o[1], tr = o[1] - o[3] + 0.6 * o[2];
		double a = clampd(amount, 0, 1);
		L = x * (1 - 0.25 * a) + a * (0.55 * el + 0.38 * tl);
		R = x * (1 - 0.25 * a) + a * (0.55 * er + 0.38 * tr);
	}
};

} // namespace horn
