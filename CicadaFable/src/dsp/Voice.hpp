// Cicada: one singing insect.  A line-for-line port of voice() in tools/proto/cicada_ref.py.
// No Rack includes, so test/render.cpp can use it offline.
//
// Signal flow:
//   song (phrase gesture) -> chirp scheduler -> pulse -> mechanism (ribs / teeth / wingbeats = impulses)
//   -> two impulse-rung complex one-pole resonators A and B -> body band-pass (OPEN, BODY) -> pan -> L, R
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cicada {

enum Mech { TYMBAL = 0, FILE_MECH = 1, WING = 2 };
enum Song { STEADY = 0, SWELL, PULSATE, RISE, SLUR, TICKBUZZ, SINK, TALK, NUM_SONGS };
static const char* const SONG_NAMES[NUM_SONGS] = {"STEADY", "SWELL", "PULSATE", "RISE", "SLUR", "TICK-BUZZ", "SINK", "TALK"};
static const char* const MECH_NAMES[3] = {"TYMBAL", "FILE", "WING"};
static const double REST_FRAC[NUM_SONGS] = {0.35, 0.50, 0.50, 0.90, 0.28, 0.15, 0.35, 0.40};
static const int CTL = 16;                      // control rate: every 16 samples
static const double PI = 3.14159265358979323846;

// ---- knob mappings (k = knob + CV, clamped 0..1) ----
inline double mapPitch(double k) { return 100.0 * std::pow(160.0, k); }
inline double mapRate(double k) { return std::pow(1000.0, k); }
inline double mapQ(double k) { return 2.0 * std::pow(15.0, k); }
inline double mapLock(double k) { return std::min(1.0, std::max(0.0, (k - 0.45) / 0.30)); }
inline int mapChirp(double k) {                 // 0 = continuous trill
	if (k >= 0.96) return 0;
	return (int)std::floor(std::pow(32.0, k / 0.95) + 0.5);
}
inline double mapGap(double k) { return 0.02 * std::pow(200.0, k); }
inline double mapLength(double k) {             // negative = Hold
	if (k >= 0.96) return -1.0;
	return 0.5 * std::pow(60.0, k / 0.95);
}
inline double mapTempC(double k) { return 10.0 + 25.0 * k; }
inline double mapTemp(double k) { return (mapTempC(k) - 5.0) / 17.5; }

// xorshift32, identical to the reference so renders can be compared closely
struct Rng {
	uint32_t s = 1;
	void seed(uint32_t seed) {
		uint64_t v = ((uint64_t)seed * 2654435761ull) % 4294967296ull;
		s = v ? (uint32_t)v : 1u;
	}
	double uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0; }
	double gauss() { return (uni() + uni() + uni() + uni() - 2.0) * 1.7320508; }
};

inline double lerp3(double u, double a, double b, double c, double ub) {
	if (u < ub) return a + (b - a) * u / ub;
	return b + (c - b) * (std::min(u, 1.0) - ub) / (1.0 - ub);
}

// One poly channel's fixed differences from the "exact" animal.
struct Individual {
	double pitch = 1.0, rate = 1.0, gap = 1.0, pan = 0.0, delay = 0.0;
};

// Knob values after CV (0..1), switches as ints, V/OCT in volts.
struct Params {
	int mech = FILE_MECH;
	double pitch = 0.752, pure = 0.885, stroke = 0.589, body = 0.0, open = 0.5;
	double rate = 0.468, chirp = 0.441, gap = 0.409;
	int song = STEADY;
	double length = 1.0, temp = 0.5, loose = 0.4, voct = 0.0;
	bool track = false;                         // V/OCT switch at RATE
};

struct Cpx { double re = 0.0, im = 0.0; };

struct Voice {
	double sr = 48000.0, dt = 1.0 / 48000.0;
	Rng rng;
	Individual ind;

	// resonators
	Cpx zA, zB, cA, cB;
	double fA = 1000.0, fB = 1000.0;
	double magA = 0.0, angA = 0.0, magB = 0.0, angB = 0.0;   // polar form of cA, cB for coef^late
	// body filters
	double s1A = 0, s2A = 0, s1B = 0, s2B = 0;
	double gPrev = 0.0, gNow = 0.0;                          // body coefficient, interpolated across a block
	// song
	enum { BODYS = 0, REL = 1, REST = 2 };
	int sstate = BODYS;
	double tS = 0.0, restLen = 0.0;
	bool gatePrev = false, trigPrev = false, clockPrev = false;
	int syl = 0;
	double tSyl = 0.0, sylLen = 1.0, relLen = 0.05;
	// chirp scheduler
	int pulsesLeft = 0, pulseIdx = 0;
	double pulseTimer = 0.0, tChirp = 0.0, chirpDur = 0.1, pauseT = 0.0;
	int side = 0;
	// mechanism
	int ribsLeft = 0, ribK = 0, ribN = 1, ribSide = 0;
	double ribTimer = 0.0, outTimer = -1.0, pulseAmp = 0.0;
	bool toothOn = false;
	double toothTimer = 0.0, tPulse = 0.0, pulseDur = 1.0, upTimer = -1.0;
	double drift = 0.0, driftTarget = 0.0, driftT = 0.0;
	// control-rate values
	double f0 = 1000.0, q = 10.0, lock = 1.0, rate = 30.0, gap = 0.2, tf = 1.0, loose = 0.0;
	int nChirp = 0, mech = 0, song = 0;
	double ampMul = 1, pitchMul = 1, rateMul = 1, openOfs = 0, gapMul = 1, nMul = 1;
	bool cont = false, active = true, hold = true;
	double stroke = 0.5, bodyD = 0.0, Rbp = 1.0 / 3.5, drop = 0.0, decay = 0.999, norm = 1.0, tremPh = 0.0;
	double panLA = 0.7071, panRA = 0.7071, panLB = 0.7071, panRB = 0.7071;
	double Ls = 8.0, fc = 1000.0, S = 0.2;
	long counter = 0;
	bool gatePatchedPrev = false;

	// outputs of the last process() call
	bool pulseOut = false, chirpOut = false;
	double pulseLevel = 0.0;                    // level of the last pulse (before loudness normalisation)

	void setSampleRate(double s) { sr = s; dt = 1.0 / s; }

	void init(uint32_t seed, const Individual& in, bool gatePatched) {
		Voice fresh;
		double s = sr;
		*this = fresh;
		setSampleRate(s);
		rng.seed(seed);
		ind = in;
		sstate = (!gatePatched && ind.delay <= 0.0) ? BODYS : REST;
		restLen = ind.delay;
		gatePatchedPrev = gatePatched;
	}

	inline void setCoef(Cpx& c, double& mag, double& ang, double f) {
		ang = 2.0 * PI * f * dt; mag = decay;
		c.re = mag * std::cos(ang); c.im = mag * std::sin(ang);
	}
	// z += a * c^late
	static inline void inject(Cpx& z, double mag, double ang, double a, double late) {
		double m = a * std::pow(mag, late), t = ang * late;
		z.re += m * std::cos(t); z.im += m * std::sin(t);
	}

	void startPhrase() {
		sstate = BODYS; tS = 0.0; syl = 0; tSyl = 0.0; pulsesLeft = 0; pulseTimer = 0.0;
	}

	// gateV/trigV/clockV in volts; *Patched say whether those jacks are in use (they change the behaviour).
	void process(const Params& p, double gateV, double trigV, double clockV,
	             bool gatePatched, bool trigPatched, bool clockPatched, double& outL, double& outR) {
		pulseOut = false;
		if (gatePatched != gatePatchedPrev) {
			if (gatePatched) { sstate = REST; tS = 0.0; }
			else { sstate = REST; tS = 0.0; restLen = 0.0; }
			gatePatchedPrev = gatePatched;
		}

		// ================= control rate =================
		if (counter % CTL == 0) control(p, gateV, gatePatched);
		double frac = (double)(counter % CTL + 1) / CTL;
		counter++;

		// ================= triggers =================
		bool tg = trigV > (trigPrev ? 0.1 : 1.0);
		if (trigPatched && tg && !trigPrev) {
			pulsesLeft = std::max(nChirp, 1); pulseTimer = 0.0; pulseIdx = 0; tChirp = 0.0;
		}
		trigPrev = tg;
		bool ck = clockV > (clockPrev ? 0.1 : 1.0);
		if (clockPatched && ck && !clockPrev && nChirp > 0) {
			pulsesLeft = std::max(1, (int)(nChirp * nMul + 0.5)); pulseTimer = 0.0; pulseIdx = 0; tChirp = 0.0;
			chirpDur = pulsesLeft / (rate * rateMul);
		}
		clockPrev = ck;

		// ================= chirp scheduler =================
		tChirp += dt;
		pulseTimer -= 1.0;
		while (pulseTimer <= 0.0) {
			double period = sr / (rate * rateMul);
			bool fire = false;
			double amp = ampMul;
			if (trigPatched) {
				if (pulsesLeft > 0) { fire = true; amp = 1.0; pulsesLeft--; }
				else { pulseTimer = 1e12; break; }
			}
			else if (!active) {
				pulseTimer += period; pulsesLeft = 0; continue;
			}
			else if (cont) {
				fire = true;
				if (pauseT > 0) { fire = false; pauseT -= period * dt; }
				else if (nChirp == 0 && song == STEADY && mech != WING && rng.uni() < 0.15 * loose * period * dt)
					pauseT = 0.15 + 0.25 * rng.uni();
			}
			else {
				if (pulsesLeft == 0 && clockPatched) { pulseTimer = 1e12; break; }
				if (pulsesLeft == 0) {
					pulsesLeft = std::max(1, (int)(nChirp * nMul + 0.5)); pulseIdx = 0; tChirp = 0.0;
					chirpDur = pulsesLeft / (rate * rateMul);
					if (pulsesLeft >= 3 && rng.uni() < 0.4 * std::pow(loose, 1.5)) pulsesLeft--;
				}
				fire = true; pulsesLeft--;
			}
			if (fire) {
				double late = -pulseTimer;
				if (!cont && nChirp >= 3 && pulseIdx < 2) amp *= 0.45 + 0.30 * pulseIdx;
				amp *= std::max(0.0, 1.0 + 0.2 * loose * rng.gauss());
				pulseOut = true; pulseIdx++; pulseLevel = amp;
				pulseAmp = amp * norm;
				if (mech == TYMBAL) {
					ribN = 1 + (int)(stroke * 11 + 0.5); ribsLeft = ribN; ribK = 0; ribTimer = -late; ribSide = side;
					outTimer = std::max(0.48 * period, (ribN + 0.5) * sr / fc) - late; side = 1 - side;
				}
				else if (mech == FILE_MECH) {
					toothOn = true; toothTimer = -late; tPulse = 0.0;
					pulseDur = (0.05 + 0.9 * stroke) * period;
				}
				else {
					inject(zA, magA, angA, pulseAmp, late); upTimer = 0.45 * period - late;
				}
			}
			pulseTimer += period * std::max(0.2, 1.0 + 0.08 * loose * rng.gauss());
			if (!cont && !trigPatched && pulsesLeft == 0 && !clockPatched)
				pulseTimer += sr * gap * gapMul * std::max(0.2, 1.0 + 0.3 * loose * rng.gauss());
		}
		if (!cont) chirpOut = (pulsesLeft > 0 || toothOn || ribsLeft > 0);
		else chirpOut = active && !trigPatched;

		// ================= mechanism =================
		if (mech == TYMBAL) {
			if (ribsLeft > 0) {
				ribTimer -= 1.0;
				while (ribTimer <= 0.0 && ribsLeft > 0) {
					double late = -ribTimer;
					double fr = fc * (1.0 - drop * ribK / std::max(ribN - 1, 1));
					double a = pulseAmp * (1.0 - 0.5 * ribK / ribN) * std::max(0.0, 1.0 + 0.24 * loose * rng.gauss());
					if (ribSide == 0) { fA = fr * 0.988; setCoef(cA, magA, angA, fA); inject(zA, magA, angA, a, late); }
					else { fB = fr * 1.012; setCoef(cB, magB, angB, fB); inject(zB, magB, angB, a, late); }
					ribK++; ribsLeft--;
					ribTimer += (sr / fr) * std::max(0.3, 1.0 + 0.12 * loose * rng.gauss());
				}
			}
			if (outTimer > -0.5) {
				outTimer -= 1.0;
				if (outTimer <= 0.0) {
					double late = -outTimer; outTimer = -1.0;
					if (ribSide == 0) { fA = fc * 0.988 * 1.08; setCoef(cA, magA, angA, fA); inject(zA, magA, angA, 0.35 * pulseAmp, late); }
					else { fB = fc * 1.012 * 1.08; setCoef(cB, magB, angB, fB); inject(zB, magB, angB, 0.35 * pulseAmp, late); }
				}
			}
		}
		else if (mech == FILE_MECH) {
			if (toothOn) {
				toothTimer -= 1.0; tPulse += 1.0;
				while (toothTimer <= 0.0 && toothOn) {
					double late = -toothTimer, xx = (tPulse - late) / pulseDur;
					if (xx >= 1.0) { toothOn = false; break; }
					double ft = fc * (1.0 - 0.08 * lock * (xx - 0.5));
					double jit = 0.45 * (1.0 - lock);
					double a = pulseAmp * std::max(0.02, std::pow(std::sin(PI * std::max(xx, 0.0)), 0.7)) * (1.0 + jit * rng.gauss());
					fA = ft; setCoef(cA, magA, angA, fA); inject(zA, magA, angA, a, late);
					double tr = ft * (0.22 + 0.78 * lock);
					toothTimer += (sr / tr) * std::max(0.2, 1.0 + jit * rng.gauss());
				}
			}
		}
		else {
			if (upTimer > -0.5) {
				upTimer -= 1.0;
				if (upTimer <= 0.0) { inject(zB, magB, angB, stroke * pulseAmp, -upTimer); upTimer = -1.0; }
			}
		}

		// ================= resonators and body =================
		Cpx t;
		t.re = zA.re * cA.re - zA.im * cA.im; t.im = zA.re * cA.im + zA.im * cA.re; zA = t;
		t.re = zB.re * cB.re - zB.im * cB.im; t.im = zB.re * cB.im + zB.im * cB.re; zB = t;
		if (std::fabs(zA.re) < 1e-30) zA.re = 0.0;
		if (std::fabs(zA.im) < 1e-30) zA.im = 0.0;
		if (std::fabs(zB.re) < 1e-30) zB.re = 0.0;
		if (std::fabs(zB.im) < 1e-30) zB.im = 0.0;
		double a_ = zA.im, b_ = zB.im;
		if (bodyD > 0.0) {
			double g = gPrev + (gNow - gPrev) * frac;    // C++ differs: interpolated across the block
			double d1 = 1.0 / (1.0 + Rbp * g + g * g);
			double hp = (a_ - (Rbp + g) * s1A - s2A) * d1, bp = g * hp + s1A;
			s1A = g * hp + bp; s2A = g * bp + s2A + g * bp;
			a_ = (1.0 - bodyD) * a_ + bodyD * bp * Rbp;
			hp = (b_ - (Rbp + g) * s1B - s2B) * d1; bp = g * hp + s1B;
			s1B = g * hp + bp; s2B = g * bp + s2B + g * bp;
			b_ = (1.0 - bodyD) * b_ + bodyD * bp * Rbp;
			if (std::fabs(s1A) < 1e-30) s1A = 0.0;
			if (std::fabs(s2A) < 1e-30) s2A = 0.0;
			if (std::fabs(s1B) < 1e-30) s1B = 0.0;
			if (std::fabs(s2B) < 1e-30) s2B = 0.0;
		}
		double mk = 1.0 + bodyD;
		outL = mk * (a_ * panLA + b_ * panLB);
		outR = mk * (a_ * panRA + b_ * panRB);
	}

	void control(const Params& p, double gateV, bool gatePatched) {
		mech = p.mech; loose = p.loose; stroke = p.stroke;
		tf = mapTemp(p.temp); song = std::max(0, std::min(NUM_SONGS - 1, p.song));
		q = mapQ(p.pure); lock = mapLock(p.pure);
		double Lk = mapLength(p.length); hold = Lk < 0; Ls = hold ? 8.0 : Lk;
		nChirp = mapChirp(p.chirp); gap = mapGap(p.gap) * ind.gap / tf;
		// slow drift of the carrier
		driftT -= CTL * dt;
		if (driftT <= 0) { driftTarget = rng.gauss(); driftT = 0.4 + 0.8 * rng.uni(); }
		drift += (driftTarget - drift) * CTL * dt * 3.0;
		double wander = 1.0 + drift * loose * (mech == WING ? 0.03 : 0.006);
		if (p.track) {
			f0 = mapPitch(p.pitch);
			rate = 261.6256 * std::pow(2.0, p.voct + (p.rate - 0.5) * 8.0);
		}
		else {
			f0 = mapPitch(p.pitch) * std::pow(2.0, p.voct);
			rate = mapRate(p.rate);
		}
		f0 *= ind.pitch * std::pow(tf, 0.38) * wander; rate *= ind.rate * tf;
		rate = std::min(rate, 2000.0);
		rate = std::max(rate, 0.05);
		// ---- gate / phrase state machine ----
		if (gatePatched) {
			bool gate = gateV > (gatePrev ? 0.1 : 1.0);
			if (gate && !gatePrev) startPhrase();
			if (!gate && gatePrev && sstate == BODYS) { sstate = REL; tS = 0.0; tSyl = 0.0; }
			gatePrev = gate;
		}
		S = (std::max(nChirp, 1) / rate + gap);
		double x = (sstate == BODYS) ? std::min(1.0, tS / Ls) : 1.0;
		ampMul = 1; pitchMul = 1; rateMul = 1; openOfs = 0; gapMul = 1; nMul = 1;
		cont = nChirp == 0; active = sstate != REST;
		relLen = 0.03;
		if (song == SWELL || song == PULSATE) {
			ampMul = 0.03 + 0.97 * std::min(1.0, x / 0.45); relLen = 0.3 * Ls;
			if (song == PULSATE) {
				double depth = 0.45 * std::min(1.0, std::max(0.0, x - 0.1) / 0.45);
				tremPh += 6.3 * tf * CTL * dt;
				if (tremPh > 1e6) tremPh -= 1e6;
				double tr = std::sin(2 * PI * tremPh);
				ampMul *= 1.0 - depth * (0.5 + 0.5 * tr); openOfs = 0.12 * (depth / 0.45) * tr;
			}
		}
		else if (song == RISE) {
			ampMul = 0.25 + 0.75 * std::pow(x, 1.5);
		}
		else if (song == SLUR) {
			ampMul = (sstate == BODYS) ? 0.1 + 0.9 * std::min(1.0, tS / 0.4) : 1.0;
			relLen = std::min(1.2, std::max(0.15, 0.2 * Ls));
		}
		else if (song == TICKBUZZ) {
			relLen = 0.25 * Ls;
			if (x < 0.45 && sstate == BODYS) {
				cont = true; rateMul = 0.06 + 0.09 * x / 0.45; ampMul = 0.5 + 0.5 * x / 0.45;
			}
			else {
				cont = true; double u = (x - 0.45) / 0.55;
				pitchMul = 0.96 + 0.08 * std::min(1.0, u / 0.4); ampMul = 0.5 + 0.5 * std::min(1.0, u / 0.35);
			}
		}
		else if (song == SINK) {
			relLen = 0.15 * Ls;
			gapMul = 1.0 / (1.0 + 1.6 * x); nMul = 1.0 - 0.35 * x; pitchMul = 1.0 - 0.10 * std::pow(x, 1.5);
			ampMul = (x < 0.6) ? lerp3(x, 0.5, 1.0, 0.8, 0.15) : 0.8 - 0.45 * (x - 0.6) / 0.4;
			double u = std::min(1.0, tChirp / std::max(chirpDur, 1e-3));
			ampMul *= lerp3(u, 0.6, 1.0, 0.35, 0.15); pitchMul *= 1.0 - 0.05 * u;
			openOfs = -0.28 * std::min(1.0, tChirp / (1.6 * std::max(chirpDur, 1e-3)));
		}
		else if (song == TALK) {
			cont = true; relLen = 5.0 * S;
			sylLen = (sstate == BODYS) ? (syl == 0 ? 3.3 * S : S) : 5.0 * S;
			double up = std::min(1.0, tSyl / (0.25 * S));
			double dn = std::min(1.0, std::max(0.0, (sylLen - tSyl) / (0.35 * S)));
			double w;
			if (sstate == REL) {
				w = up; ampMul = 0.25 + 0.75 * up; pitchMul = 1.0 - 0.04 * tSyl / sylLen;
				ampMul *= std::min(1.0, (sylLen - tSyl) / (0.3 * sylLen));
			}
			else {
				w = std::min(up, dn); ampMul = 0.25 + 0.75 * std::min(up, dn);
			}
			openOfs = -0.43 * (1.0 - w);
			tSyl += CTL * dt;
			if (sstate == BODYS && tSyl >= sylLen) {
				tSyl = 0.0; syl++;
				if (tS >= Ls && !hold && !gatePatched) { sstate = REL; tS = 0.0; }
			}
		}
		if (sstate == REL) {
			double y = std::min(1.0, tS / relLen);
			if (song == STEADY || song == RISE || song == SINK) ampMul *= 1.0 - y;
			else if (song == SWELL || song == PULSATE) { ampMul *= 1.0 - y; pitchMul *= 1.0 - 0.07 * y; rateMul *= 1.0 - 0.25 * y; }
			else if (song == SLUR) { ampMul *= 1.0 - 0.85 * y; pitchMul *= 1.0 - 0.26 * y; }
			else if (song == TICKBUZZ) { ampMul *= 1.0 - 0.9 * y; pitchMul = 1.04 - 0.16 * y; }
			else if (song == TALK) ampMul = std::max(0.0, ampMul);
		}
		rateMul = std::max(rateMul, 0.01);
		// advance the phrase clock
		tS += CTL * dt;
		if (sstate == BODYS) {
			if (!gatePatched && !hold && tS >= Ls && song != TALK) { sstate = REL; tS = 0.0; }
		}
		else if (sstate == REL) {
			if (tS >= relLen) {
				sstate = REST; tS = 0.0;
				restLen = REST_FRAC[song] * Ls * std::max(0.2, 1.0 + 0.3 * loose * rng.gauss());
			}
		}
		else if (!gatePatched && tS >= restLen) {
			startPhrase();
		}
		// ---- derived mechanism values ----
		fc = std::min(f0 * pitchMul, 0.45 * sr);
		double bw = fc / q; decay = std::exp(-PI * bw * dt);
		drop = std::max(0.0, 0.12 * (1.0 - p.pure / 0.75));
		if (mech == FILE_MECH) {
			double tr_ = fc * (0.22 + 0.78 * lock);
			norm = lock * (PI / q) * 1.15 + (1.0 - lock) * 0.5 / std::sqrt(tr_ / (4 * PI * bw));
		}
		else if (mech == TYMBAL) {
			int nr = 1 + (int)(stroke * 11 + 0.5);
			norm = 1.1 / std::sqrt((double)nr) / std::sqrt(std::max(1.0, rate * rateMul / (PI * bw)));
		}
		else {
			norm = 0.9 / std::sqrt(std::max(1.0, rate * rateMul / (PI * bw)));
		}
		bodyD = 0.9 * p.body;
		double o = std::min(1.0, std::max(0.0, p.open + openOfs));
		double fb = std::min(fc * std::pow(2.0, 2.0 * (o - 0.5)), 0.45 * sr);
		gPrev = (counter == 0) ? std::tan(PI * fb * dt) : gNow;
		gNow = std::tan(PI * fb * dt);
		double pc = ind.pan, sp = (mech == TYMBAL) ? 0.15 : 0.0;
		double a = (std::min(1.0, std::max(-1.0, pc - sp)) + 1.0) * PI / 4; panLA = std::cos(a); panRA = std::sin(a);
		a = (std::min(1.0, std::max(-1.0, pc + sp)) + 1.0) * PI / 4; panLB = std::cos(a); panRB = std::sin(a);
		if (mech == WING) { fA = fc; fB = std::min(fc * 1.9, 0.45 * sr); }
		setCoef(cA, magA, angA, fA); setCoef(cB, magB, angB, fB);
	}
};

// ---- shared stage: air and leaves ----
struct Tap { double ms; int sign, dst, src; };
static const Tap TAPS[24] = {
	{76.51, 1, 1, 1}, {30.12, 1, 0, 0}, {105.33, 1, 0, 0}, {96.46, 1, 0, 0}, {39.15, -1, 0, 1}, {55.63, -1, 0, 1},
	{68.21, 1, 1, 1}, {76.17, 1, 0, 1}, {28.98, 1, 0, 1}, {9.10, 1, 0, 0}, {63.73, 1, 0, 1}, {76.99, 1, 0, 1},
	{61.64, -1, 0, 1}, {26.32, -1, 1, 1}, {27.27, 1, 0, 0}, {100.29, -1, 1, 0}, {35.04, 1, 1, 0}, {102.27, 1, 1, 1},
	{90.05, -1, 0, 0}, {62.90, 1, 1, 1}, {45.91, 1, 1, 0}, {48.97, -1, 1, 0}, {21.42, -1, 1, 0}, {117.53, -1, 0, 1},
};

// Bilinear one-pole low-pass (same as scipy butter(1, fc)).
struct OnePole {
	double b = 1, a1 = 0, x1 = 0, y1 = 0;
	void set(double fc, double sr) {
		double k = std::tan(PI * std::min(fc, 0.45 * sr) / sr);
		b = k / (1 + k); a1 = (k - 1) / (k + 1);
	}
	double process(double x) {
		double y = b * x + b * x1 - a1 * y1;
		x1 = x; y1 = (std::fabs(y) < 1e-30) ? 0.0 : y;
		return y;
	}
};

struct Distance {
	double sr = 48000.0;
	OnePole lp[2], wetLp[2];
	enum { MAXLEN = 32768 };                   // 125 ms at 192 kHz is 24000
	float buf[2][MAXLEN] = {};
	int w = 0, len = 6000;
	int tapD[24];
	double d = -1.0, tapG[24];

	void setSampleRate(double s) {
		sr = s;
		w = 0;
		for (int c = 0; c < 2; c++) for (int i = 0; i < MAXLEN; i++) buf[c][i] = 0.f;
		len = std::min((int)MAXLEN, (int)(0.125 * sr) + 2);
		for (int i = 0; i < 24; i++) tapD[i] = std::max(1, (int)(TAPS[i].ms * 0.001 * sr));
		wetLp[0].set(5000.0, sr); wetLp[1].set(5000.0, sr);
		double dd = d; d = -1.0; setDistance(dd < 0 ? 0.15 : dd);
	}
	void reset() {
		for (int c = 0; c < 2; c++) { lp[c] = OnePole(); wetLp[c] = OnePole(); }
		setSampleRate(sr);
	}
	void setDistance(double nd) {
		if (nd == d) return;
		d = nd;
		double fc = 18000.0 * (1 - d) * (1 - d) + 2500.0;
		lp[0].set(fc, sr); lp[1].set(fc, sr);
		for (int i = 0; i < 24; i++) tapG[i] = TAPS[i].sign * d * 0.5 * std::exp(-TAPS[i].ms * 0.001 / 0.05);
	}
	void process(double& L, double& R) {
		if (d <= 0.0) {
			// keep the line moving so switching DISTANCE on is clean
			buf[0][w] = 0.f; buf[1][w] = 0.f; w = (w + 1) % len;
			return;
		}
		double y[2] = {lp[0].process(L), lp[1].process(R)};
		buf[0][w] = (float)y[0]; buf[1][w] = (float)y[1];
		double wet[2] = {0.0, 0.0};
		for (int i = 0; i < 24; i++) {
			int r = w - tapD[i]; if (r < 0) r += len;
			wet[TAPS[i].dst] += tapG[i] * buf[TAPS[i].src][r];
		}
		w = (w + 1) % len;
		L = y[0] + wetLp[0].process(wet[0]);
		R = y[1] + wetLp[1].process(wet[1]);
	}
};

static const double OUT_GAIN = 5.0;
inline double softClip(double x) { return 10.0 * std::tanh(x / 10.0); }

// Poly channel v of n: voice 0 exact; the others fixed offsets from a per-channel seed.
inline Individual individual(int v, int n) {
	Individual in;
	if (n <= 1) return in;
	in.pan = -0.8 + 1.6 * ((v * 7) % n) / (double)(n - 1);
	if (v == 0) return in;
	Rng r; r.seed(1000u + (uint32_t)v);
	auto u = [&]() { return r.uni() * 2.0 - 1.0; };
	in.pitch = std::pow(2.0, u() * 0.035);
	in.rate = 1.0 + u() * 0.04;
	in.gap = 1.0 + u() * 0.15;
	in.delay = 0.2 + 3.8 * r.uni();
	return in;
}

} // namespace cicada
