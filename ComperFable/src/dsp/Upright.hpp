// Walker's upright bass: the real-time form of tools/proto/upright_ref.py (the definition).
// A voice is a bank of sines (two motions per harmonic, plus the burst of the pluck) whose amplitudes are
// closed-form functions of time; they are evaluated exactly for the first 2 ms of a note and then at the ends
// of short segments (at most 16 samples) with linear ramps, and the sines run as rotating phasors re-anchored
// to the exact phase at every segment end. One shared stage follows: fingerboard buzz, body EQ, the ringing
// body (resonators equal to body_ir()), the pickup path and the mix. Checked by make test against
// test/upright_cases.json with tools/check_upright.py. No Rack includes.
#pragma once
#include "Engine.hpp"
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>

namespace upright {
using comper::Rng;

struct Tone {
	double pluck = 0.17, bright = 0.5, sustain = 0.5, growl = 0.35, body = 0.6, pickup = 0.25, age = 0.3, thump = 0.5, attack = 1.0;
};
struct NoteSpec {
	int pitch = 40; double vel = 0.75; int string = 1, fret = 3; bool dead = false; double scoop = 0, cents = 0, vib = 0;
};

// ------------------------------------------------------------------ constants (upright_ref.py)
static const double FC_LO = 240.0, FC_HI = 600.0, FC_EXP = 1.4, FC_KEY = 0.9, HP_F = 45.0;
static const double FAST_M = 0.6, FAST_DB = 80.0, RISE_K = 0.08;
static const double BURST_FC = 3000.0, BURST_LVL = 0.36;
static const double BEAT_HZ = 1.3, BEAT_BODY = 2.8;
static const double NOISE_LVL = 0.5, BURST_SNAP = 1.6;
static const double RING_DECAY = 1.0, RING_ADD = 120.0, SYMP = 0.08, KNOCK = 0.8, KNOCK_TILT = 1.0;
static const double STRING_B[4] = {6e-5, 5e-5, 4e-5, 3e-5};
static const double BURST_FAST = 260.0, BURST_FAST_F = 0.03, BURST_SLOW = 35.0, BURST_SLOW_F = 0.006, BURST_KEEP = 0.10;
static const double BODY_LP = 7000.0, BEND_CENTS = 200.0, BEND_CYCLES = 0.9;
static const int OPEN[4] = {28, 33, 38, 43};
struct Peak { double f, q, g; };
static const Peak BODY[8] = {{65, 9, 4}, {104, 9, 3}, {150, 8, 2.5}, {200, 8, 2.5}, {226, 9, 2}, {268, 8, 2}, {300, 8, 1.5}, {440, 5, 2}};
static const Peak SHAPE[3] = {{66, 1.6, 9.0}, {138, 1.5, -5.0}, {850, 1.2, 3.0}};
struct Mode { double f, db, decay; };
static const Mode MODES[40] = {{65.185546875,8.685890352395823e-09,54.272059440700495},{102.5390625,-2.1696857488451062,79.91412633027932},{146.484375,-6.346673075002517,60.71606019511431},{202.880859375,-6.311335796784515,188.92858480471503},{222.65625,-12.056892507538555,92.3331324282973},{248.291015625,-16.417340003835456,214.42873010230085},{320.068359375,-21.049982221455288,213.8901257884177},{357.421875,-25.996034760983818,153.49288458169434},{394.04296875,-18.606178765620268,237.81103248753897},{417.48046875,-18.291144563328793,187.06094683395645},{448.974609375,-20.19918088555253,238.30504066553843},{500.9765625,-24.50950579282527,128.01396752049286},{544.921875,-26.258973281203204,253.24442815343855},{588.134765625,-26.31535102663465,195.34750955206457},{630.615234375,-30.62963272584971,258.69185782753084},{656.982421875,-29.429561508887087,185.76328459027172},{731.689453125,-26.83662689797503,278.6080160346869},{799.072265625,-30.638886619395077,229.0120103021838},{837.158203125,-30.876193260458233,207.74465217390934},{863.525390625,-28.632460286328968,194.9776369864298},{917.724609375,-34.636289370542855,147.0656995068048},{960.9375,-33.173782340291744,200.19646662677687},{995.361328125,-37.54277919970416,224.71134014230967},{1026.85546875,-38.803141507073484,181.85679512124398},{1056.15234375,-34.88472940481833,227.71747731995538},{1092.7734375,-33.405745206350105,196.4911164586488},{1149.169921875,-30.788389167271234,301.2533781154593},{1196.77734375,-33.65764856581598,176.54699336340437},{1246.58203125,-35.52390805000173,289.3621402459639},{1305.17578125,-36.051425858400954,287.7591076569216},{1343.26171875,-38.69178973602383,271.41875181951434},{1417.236328125,-37.011273228034646,219.98185748566513},{1434.814453125,-38.37309857275133,278.6406568515002},{1462.646484375,-35.74686323953897,262.8416101151871},{1546.142578125,-38.643803922765436,203.5978883310912},{1565.185546875,-37.39049798168984,222.38648549438452},{1645.01953125,-38.860911580773305,289.9632308154358},{1751.953125,-36.86883241670363,304.4936189303944},{1781.25,-37.39842240445932,228.57421356170713},{2029.541015625,-38.17410826342516,243.07348347921604}};

static inline double hz(double m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
static const double LN10_20 = 2.302585092994046 / 20.0;     // dB -> natural log units
static inline double dbs(double dbPerS) { return dbPerS * LN10_20; }

// ------------------------------------------------------------------ biquads (as upright_ref.py builds them)
struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	void set(double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
	void peak(double f, double q, double gdb, double sr) {
		double A = std::pow(10.0, gdb / 40), w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q);
		set(1 + al * A, -2 * std::cos(w), 1 - al * A, 1 + al / A, -2 * std::cos(w), 1 - al / A);
	}
	void hp(double f, double q, double sr) {
		double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void lp(double f, double q, double sr) {
		double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set((1 - c) / 2, (1 - c), (1 - c) / 2, 1 + al, -2 * c, 1 - al);
	}
	double process(double x) {
		double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1; x1 = x; y2 = y1; y1 = y; return y;
	}
	void clear() { x1 = x2 = y1 = y2 = 0; }
};

// ------------------------------------------------------------------ one plucked note
struct Voice {
	static const int KMAX = 40, KBMAX = 220, SEG = 16, EXACT = 96;
	bool active = false; int id = -1; long long age = 0;
	double sr = 48000, dt = 1.0 / 48000;
	long long n = 0;                       // samples since note-on
	long long endAt = -1;                  // sample index at which the voice stops (0.6 s after release)
	double f0 = 110, vel = 0.75, scoop = 0, vib = 0, bendC = 0;
	double ph = 0;                         // running phase of the fundamental (cumsum of 2 pi f / sr)
	bool dead = false;
	// release
	bool released = false; double dur = 1e9; bool stopped = false;
	double relRate[KMAX] = {}, relBurst = 0;                        // natural-log per second
	// sustained partials
	int K = 0; double ck[KMAX], phk[KMAX], A[KMAX], sigS[KMAX], sigF[KMAX], mf[KMAX], tauR[KMAX], dk[KMAX];
	// burst partials
	int KB = 0; bool burstOn = false; double ckb[KBMAX], phb[KBMAX], Ab[KBMAX], lamF[KBMAX], lamS[KBMAX];
	double burstGain = 0, lamSnap = 0, bFloor[KBMAX];   // bFloor: a burst partial below it is dropped
	// phasors: components 0..K-1 slow, K..2K-1 fast, 2K..2K+KB-1 burst
	static const int CMAX = 2 * KMAX + KBMAX;
	int C = 0; double zr[CMAX], zi[CMAX], wr[CMAX], wi[CMAX], amp[CMAX], damp[CMAX];
	double ampScale = 1;                   // vel^1.35 * 2.2
	// thump
	std::vector<double> noise; int nNoise = 0; std::vector<double> tick; long long tickAt = -1; double thumpGain = 0;
	Rng rng;

	double pitchCents(double t) const {
		double pc = -scoop * std::exp(-t / 0.035);
		pc -= bendC * std::exp(-t * f0 / BEND_CYCLES);
		if (vib > 0) pc += vib * std::min(1.0, std::max(0.0, (t - 0.25) / 0.3)) * std::sin(2 * M_PI * 5.2 * t);
		return pc;
	}
	void start(const NoteSpec& ns, const Tone& tone, int64_t rngSeed, double sampleRate, int noteId, long long ageStamp) {
		active = true; id = noteId; age = ageStamp; sr = sampleRate; dt = 1.0 / sr; n = 0; endAt = -1;
		released = false; dur = 1e9; stopped = false; tick.clear(); tickAt = -1;
		rng = Rng(rngSeed);
		vel = ns.vel; dead = ns.dead; scoop = ns.scoop; vib = ns.vib;
		f0 = hz(ns.pitch) * std::pow(2.0, ns.cents / 1200.0);
		bendC = BEND_CENTS * (0.45 + 0.55 * vel);
		ph = 0;
		K = (int)std::min(40.0, 5000.0 / f0);
		int fret = ns.fret, string = std::max(0, std::min(3, ns.string));
		double Bs = STRING_B[string] * std::pow(2.0, fret / 6.0);
		bool open = fret == 0; double age_ = tone.age;
		double fc = FC_LO * std::pow(FC_HI / FC_LO, std::pow(vel, FC_EXP)) * std::pow(f0 / 65.0, FC_KEY);
		fc *= (0.6 + 0.9 * tone.bright) * (1 - 0.45 * age_) * (open ? 1.25 : 1.0);
		double beta = tone.pluck * (1 + 0.012 * fret);
		for (int i = 0; i < K; i++) {
			int k = i + 1; double fk = k * f0; double s = std::sin(k * M_PI * beta);
			A[i] = std::sqrt(s * s + 0.06) / k * std::exp(-fk / fc);
		}
		for (int i = 0; i < K; i++) { double g = std::max(-2.0, std::min(2.0, rng.g())); A[i] *= 1 + 0.25 * age_ * g; }
		double sustF = std::pow(2.0, 1.6 * (0.5 - tone.sustain));
		for (int i = 0; i < K; i++) {
			int k = i + 1; double fk = k * f0;
			double sig = 5.0 + fk * (0.03 + 0.05 * std::min(1.0, std::max(0.0, (f0 - 40.0) / 110.0))) + 0.4 * fret;
			sig *= (open ? 0.7 : 1.0) * sustF * (1 + 1.2 * age_ * fk / 1000.0);
			for (int b = 0; b < 3; b++) { double bf = BODY[b].f, bq = BODY[b].q; double z = (fk - bf) / (bf / bq * 1.5); sig += 9.0 * tone.body * std::exp(-0.5 * z * z); }
			if (dead) { sig = 260.0 + 0.5 * fk; A[i] *= std::exp(-fk / 500.0); }
			sigS[i] = dbs(sig); sigF[i] = dbs(sig + FAST_DB);
			mf[i] = FAST_M + 0.2 * std::min(1.0, fk / 1000.0);
			tauR[i] = 0.0008 + RISE_K / fk;
			ck[i] = k * std::sqrt(1 + Bs * k * k); phk[i] = 0.5 * (k % 2);
			relRate[i] = dbs(170.0 + 0.25 * fk);
		}
		for (int i = 0; i < K; i++) {
			int k = i + 1; double fk = k * f0;
			double wob = std::max(-1.5, std::min(1.5, rng.g()));
			double z = (fk - 125.0) / 70.0;
			double d = (BEAT_HZ + BEAT_BODY * tone.body / 0.6 * std::exp(-0.5 * z * z)) * (1 + 0.35 * wob) * (1 + 0.004 * fk / 10.0);
			dk[i] = std::min(d, 7.0);
		}
		KB = (int)std::min(220.0, 11000.0 / f0);
		double fcb = BURST_FC * (0.5 + 0.8 * vel) * (0.6 + 0.8 * tone.bright) * (1 - 0.4 * age_);
		double slowF = 1.6 - 1.2 * tone.sustain;
		for (int i = 0; i < KB; i++) {
			int k = i + 1; double fb = k * f0; double s = std::sin(k * M_PI * beta);
			Ab[i] = std::sqrt(s * s + 0.1) / k * std::exp(-fb / fcb);
			Ab[i] *= (1 - std::exp(-fb / 400.0));
		}
		for (int i = 0; i < KB; i++) { double g = std::max(-2.0, std::min(2.0, rng.g())); Ab[i] *= 1 + 0.3 * g; }
		for (int i = 0; i < KB; i++) {
			int k = i + 1; double fb = k * f0;
			ckb[i] = k * std::sqrt(1 + Bs * k * k); phb[i] = 0.5 * (k % 2);
			lamF[i] = dbs(BURST_FAST + BURST_FAST_F * fb); lamS[i] = dbs((BURST_SLOW + BURST_SLOW_F * fb) * slowF);
		}
		lamSnap = dbs(900.0);
		relBurst = relRate[0];
		burstOn = !dead;
		burstGain = BURST_LVL * tone.attack * tone.thump / 0.5;
		// burst partials above 0.45 of the sample rate are dropped (no aliasing); at 48 kHz none are
		while (KB > 0 && KB * f0 >= 0.45 * sr) KB--;
		ampScale = std::pow(vel, 1.35) * 2.2;
		// thump: finger leaving the string
		nNoise = (int)(0.03 * sr); noise.resize(nNoise);
		for (int i = 0; i < nNoise; i++) {
			double g = rng.g();
			noise[i] = g * (std::exp(-i / (0.003 * sr)) + 0.25 * std::exp(-i / (0.012 * sr)));
		}
		thumpGain = NOISE_LVL * tone.attack * tone.thump * vel;
		C = 2 * K + (burstOn ? KB : 0);
		// CPU economy: a burst partial is dropped (from the top down) once it is 100 dB under its own start
		for (int i = 0; i < KB; i++) bFloor[i] = 1e-5 * std::fabs(Ab[i]) * (BURST_SNAP + 1) * burstGain;
	}
	// the note will be released d seconds after note-on (may be called again to move it, until it happens)
	void setRelease(double d, bool stop) {
		if (released && n * dt >= dur) return;           // already happened
		double now = n * dt; if (d < now) d = now;
		released = true; dur = d; stopped = stop;
		for (int i = 0; i < K; i++) { double fk = (i + 1) * f0; relRate[i] = dbs(stop ? 700.0 + 1.0 * fk : 170.0 + 0.25 * fk); }
		relBurst = relRate[0];
		endAt = (long long)((dur + 0.6) * sr);
		if (stop) {
			// the finger landing on the moving string: a soft tick (random draws after the pluck's)
			tickAt = (long long)(dur * sr); int m2 = (int)(0.012 * sr); tick.resize(m2);
			for (int i = 0; i < m2; i++) tick[i] = 0.35 * rng.g() * std::exp(-i / (0.0025 * sr));
		}
	}
	// amplitude of component c at time t (seconds since note-on), without ampScale
	inline double relAt(double t, double rate) const { return (released && t > dur) ? std::exp(-rate * (t - dur)) : 1.0; }
	void amps(double t, double* out) const {
		for (int i = 0; i < K; i++) {
			double rise = 1 - std::exp(-t / tauR[i]); double rl = relAt(t, relRate[i]);
			out[i] = A[i] * rise * (1 - mf[i]) * std::exp(-sigS[i] * t) * rl;
			out[K + i] = A[i] * rise * mf[i] * std::exp(-sigF[i] * t) * rl;
		}
		if (burstOn) {
			double* o = out + 2 * K;
			if (t >= 0.6) { for (int i = 0; i < KB; i++) o[i] = 0; return; }
			double r0 = (1 - std::exp(-t / 0.0005)) * relAt(t, relBurst) * burstGain;
			double snap = BURST_SNAP * std::exp(-lamSnap * t);
			for (int i = 0; i < KB; i++)
				o[i] = Ab[i] * (snap + (1 - BURST_KEEP) * std::exp(-lamF[i] * t) + BURST_KEEP * std::exp(-lamS[i] * t)) * r0;
		}
	}
	void phases(double phase, double t, double* out) const {
		for (int i = 0; i < K; i++) { out[i] = ck[i] * phase + phk[i]; out[K + i] = out[i] + 2 * M_PI * dk[i] * t; }
		if (burstOn) for (int i = 0; i < KB; i++) out[2 * K + i] = ckb[i] * phase + phb[i];
	}
	// adds len samples of the string (s) and the raw thump (th, already times its gain) into the buffers
	void render(double* s, double* th, int len) {
		double a1[CMAX], p1[CMAX];
		int i = 0;
		while (i < len && active) {
			if (n < EXACT) {
				// exact, sample by sample
				double t = n * dt;
				double f = f0 * std::pow(2.0, pitchCents(t) / 1200.0);
				ph += 2 * M_PI * f / sr;
				amps(t, a1); phases(ph, t, p1);
				double acc = 0;
				for (int c = 0; c < C; c++) acc += a1[c] * std::sin(p1[c]);
				s[i] += acc * ampScale;
				th[i] += thumpSample();
				n++; i++;
				if (n == EXACT) {
					// prime the phasors and amplitudes at the last exact sample
					for (int c = 0; c < C; c++) { amp[c] = a1[c]; zr[c] = std::cos(p1[c]); zi[c] = std::sin(p1[c]); }
				}
				continue;
			}
			int L = std::min(SEG, len - i);
			// phase at the end of the segment: run the fundamental's phase sample by sample
			double phStart = ph, tStart = (n - 1) * dt;  // phases/amps at the previous sample (already rendered)
			double phs[SEG + 1]; phs[0] = phStart;
			for (int j = 0; j < L; j++) {
				double t = (n + j) * dt;
				double f = f0 * std::pow(2.0, pitchCents(t) / 1200.0);
				ph += 2 * M_PI * f / sr; phs[j + 1] = ph;
			}
			double tEnd = (n + L - 1) * dt;
			amps(tEnd, a1);
			// the phasors carry on from the previous segment (CPU economy: one sincos per component per segment)
			double p0[CMAX]; phases(phStart, tStart, p0); phases(ph, tEnd, p1);
			for (int c = 0; c < C; c++) {
				double dp = (p1[c] - p0[c]) / L;
				wr[c] = std::cos(dp); wi[c] = std::sin(dp);
				damp[c] = (a1[c] - amp[c]) / L;
			}
			for (int j = 0; j < L; j++) {
				double acc = 0;
				for (int c = 0; c < C; c++) {
					double r = zr[c] * wr[c] - zi[c] * wi[c]; double im = zr[c] * wi[c] + zi[c] * wr[c];
					zr[c] = r; zi[c] = im; amp[c] += damp[c];
					acc += amp[c] * im;
				}
				s[i + j] += acc * ampScale;
				th[i + j] += thumpSample();
				n++;
			}
			(void)phs;
			for (int c = 0; c < C; c++) amp[c] = a1[c];
			i += L;
			// burst finished: drop its components
			if (burstOn) {
				while (KB > 0 && std::fabs(amp[2 * K + KB - 1]) < bFloor[KB - 1]) KB--;
				if (KB == 0 || n * dt >= 0.6) { burstOn = false; KB = 0; }
				C = 2 * K + KB;
			}
			// renormalise the phasors now and then (rounding drift)
			if ((n & 1023) < L) for (int c = 0; c < C; c++) { double g = 1.0 / std::sqrt(zr[c] * zr[c] + zi[c] * zi[c]); zr[c] *= g; zi[c] *= g; }
			if (endAt >= 0 && n >= endAt) active = false;
		}
	}
	inline double thumpSample() {
		double v = 0;
		if (n < nNoise) v += noise[n];
		if (tickAt >= 0 && n >= tickAt && n < tickAt + (long long)tick.size()) v += tick[n - tickAt];
		return v * thumpGain;
	}
};

// ------------------------------------------------------------------ the instrument
struct Resonator {      // impulse response a * r^n * sin(w n + phi)
	double b0 = 0, b1 = 0, c1 = 0, c2 = 0, x1 = 0, y1 = 0, y2 = 0;
	void set(double a, double r, double w, double phi) { b0 = a * std::sin(phi); b1 = a * r * std::sin(w - phi); c1 = 2 * r * std::cos(w); c2 = -r * r; }
	inline double process(double x) { double y = b0 * x + b1 * x1 + c1 * y1 + c2 * y2; x1 = x; y2 = y1; y1 = y; return y; }
	void clear() { x1 = y1 = y2 = 0; }
};

struct Bass {
	static const int NV = 6, BLOCK = 16;
	double sr = 48000;
	Tone tone;
	Voice voices[NV];
	long long ageCounter = 0;
	// shared stage
	Biquad hp[3], peaks[8], shapes[3], lpBody, thLp, puHp, puLp;
	double lastBody = -1, lastBright = -1;
	Resonator modes[40], sympA[32], sympB[32];
	double modeNorm = 1, sympNorm = 1;
	// output block
	double out[BLOCK]; int pos = BLOCK;
	long long sampleNow = 0;          // absolute index of the next sample process() returns
	struct Pending { long long at; NoteSpec ns; int64_t seed; int id; };
	std::vector<Pending> pending;
	struct PendingRel { int id; double dur; bool stop; };

	Bass() { setSampleRate(48000); }
	void setSampleRate(double s) {
		sr = s;
		for (int i = 0; i < 3; i++) hp[i].hp(HP_F, i == 0 ? 0.52 : i == 1 ? 0.71 : 1.93, sr);
		lpBody.lp(BODY_LP, 0.6, sr); puHp.hp(28, 0.7, sr); puLp.lp(6500, 0.7, sr);
		lastBody = lastBright = -1;
		// the ringing body: 40 modes and 32 sympathetic-string modes, normalised as body_ir() does
		{
			int n = (int)(0.45 * sr); double e = 0;
			for (int i = 0; i < 40; i++) {
				const Mode& m = MODES[i];
				double a = std::pow(10.0, (m.db + KNOCK_TILT * std::max(0.0, std::log2(m.f / 200.0))) / 20.0) * (m.f / 65.0);
				double r = std::exp(-dbs(RING_DECAY * m.decay + RING_ADD) / sr);
				modes[i].set(a, r, 2 * M_PI * m.f / sr, 1.7 * i); modes[i].clear();
			}
			for (int k = 0; k < n; k++) { double v = 0; for (int i = 0; i < 40; i++) v += modes[i].process(k == 0 ? 1.0 : 0.0); e += v * v; }
			modeNorm = 1.0 / std::sqrt(e);
			int n2 = (int)(1.6 * sr); double e2 = 0; int j = 0;
			for (int o = 0; o < 4; o++) for (int kk = 1; kk <= 8; kk++) {
				double f = hz(OPEN[o]) * kk; j++;
				double r = std::exp(-dbs(12.0 + 0.02 * f) / sr), r2 = r * std::exp(-1.0 / (0.02 * sr));
				sympA[j - 1].set(1.0 / kk, r, 2 * M_PI * f / sr, 2.3 * j); sympB[j - 1].set(1.0 / kk, r2, 2 * M_PI * f / sr, 2.3 * j);
				sympA[j - 1].clear(); sympB[j - 1].clear();
			}
			for (int k = 0; k < n2; k++) { double v = 0; for (int i = 0; i < 32; i++) v += sympA[i].process(k == 0 ? 1.0 : 0.0) - sympB[i].process(k == 0 ? 1.0 : 0.0); e2 += v * v; }
			sympNorm = SYMP / std::sqrt(e2);
			for (auto& m : modes) m.clear();
			for (auto& m : sympA) m.clear();
			for (auto& m : sympB) m.clear();
		}
		for (auto& b : hp) b.clear();
		for (auto& b : peaks) b.clear();
		for (auto& b : shapes) b.clear();
		lpBody.clear(); thLp.clear(); puHp.clear(); puLp.clear();
	}
	void updateFilters() {
		if (tone.body != lastBody) {
			for (int i = 0; i < 8; i++) peaks[i].peak(BODY[i].f, BODY[i].q, BODY[i].g * tone.body / 0.6, sr);
			for (int i = 0; i < 3; i++) shapes[i].peak(SHAPE[i].f, SHAPE[i].q, SHAPE[i].g * std::min(1.3, tone.body / 0.6), sr);
			lastBody = tone.body;
		}
		if (tone.bright != lastBright) { thLp.lp(900 + 1500 * tone.bright, 0.7, sr); lastBright = tone.bright; }
	}
	// a note starting at absolute sample 'at' (>= the next sample to be rendered, or it starts as soon as possible)
	void noteOn(const NoteSpec& ns, int64_t seed, int id, long long at) { pending.push_back({at, ns, seed, id}); }
	Voice* find(int id) { for (auto& v : voices) if (v.active && v.id == id) return &v; return nullptr; }
	// release note 'id' dur seconds after its note-on; stop: the plucking finger stops it
	void release(int id, double dur, bool stop) {
		for (auto& p : pending) if (p.id == id) { pendRel.push_back({id, dur, stop}); return; }
		if (Voice* v = find(id)) v->setRelease(dur, stop);
	}
	std::vector<PendingRel> pendRel;
	void releaseNow(int id, bool stop) { if (Voice* v = find(id)) v->setRelease(v->n * v->dt, stop); }
	void allOff() { for (auto& v : voices) if (v.active) v.setRelease(v.n * v.dt, true); pending.clear(); pendRel.clear(); }
	void startVoice(const Pending& p) {
		Voice* v = nullptr;
		for (auto& x : voices) if (!x.active) { v = &x; break; }
		if (!v) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; steals++; }
		v->start(p.ns, tone, p.seed, sr, p.id, ageCounter++);
		for (size_t i = 0; i < pendRel.size();) {
			if (pendRel[i].id == p.id) { v->setRelease(pendRel[i].dur, pendRel[i].stop); pendRel.erase(pendRel.begin() + i); }
			else i++;
		}
	}
	int steals = 0;
	void renderBlock() {
		updateFilters();
		double s[BLOCK] = {}, th[BLOCK] = {};
		long long b0 = sampleNow;
		// voices already sounding
		for (auto& v : voices) if (v.active) v.render(s, th, BLOCK);
		// notes starting in this block (late ones start at its first sample)
		std::stable_sort(pending.begin(), pending.end(), [](const Pending& a, const Pending& b) { return a.at < b.at; });
		size_t k = 0;
		for (; k < pending.size() && pending[k].at < b0 + BLOCK; k++) {
			int off = (int)std::max(0LL, pending[k].at - b0);
			startVoice(pending[k]);
			Voice* v = find(pending[k].id);
			if (v) v->render(s + off, th + off, BLOCK - off);
		}
		pending.erase(pending.begin(), pending.begin() + k);
		// shared stage
		double g = tone.growl + 0.5 * tone.age, thr = 0.9 - 0.75 * std::min(1.0, g);
		double ringGain = KNOCK * (0.4 + 0.6 * tone.attack) * tone.body / 0.6, pu = tone.pickup;
		for (int i = 0; i < BLOCK; i++) {
			double x = s[i];
			if (g > 0) { double over = x - thr; if (over > 0) x -= over * 0.85; }
			double t = thLp.process(th[i]);
			double y = x + t;
			for (auto& b : hp) y = b.process(y);
			for (auto& b : peaks) y = b.process(y);
			for (auto& b : shapes) y = b.process(y);
			y = lpBody.process(y);
			double rin = x + 2.0 * t, ring = 0, sy = 0;
			for (auto& m : modes) ring += m.process(rin);
			for (int j = 0; j < 32; j++) sy += sympA[j].process(rin) - sympB[j].process(rin);
			y += ringGain * (ring * modeNorm + sy * sympNorm);
			double pp = puLp.process(puHp.process(x + 0.3 * t));
			out[i] = ((1 - pu) * y + pu * pp * 0.9) * (1 + 3.5 * pu);
		}
	}
	// next output sample (reference units: a loud note peaks around 5 to 10)
	double process() {
		if (pos >= BLOCK) { renderBlock(); pos = 0; }
		sampleNow++;
		return out[pos++];
	}
	int activeVoices() const { int c = 0; for (auto& v : voices) c += v.active; return c; }
};

} // namespace upright
