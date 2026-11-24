// Plate Driver engine: a thin metal plate held in a feedback loop by a pickup and a driver.
// No Rack code here, so test/ can run it offline. Ported from tools/proto/plate_driver_ref.c,
// which defines the sound; see docs/BUILD-INSTRUCTIONS.md section 3 for the maths.
//
// In plain words:
//  * The plate is 192 ringing modes (exact two-pole resonators).
//  * A pickup reads the plate's velocity at one point; the loop (focus filter, delay, limiter
//    "pump", amplifier, driver) turns it back into a force at the driver point. Velocity fed
//    back as force at the same point is negative damping, so the plate's own modes grow.
//  * The plate is thin, so large vibration stretches it. Six "stretch channels" tie every
//    mode to every other through a potential 0.5 * sum S_c^2 that is never negative: the
//    stretching moves energy between modes but cannot create it. That is what makes one pure
//    tone split into multiphonics and then roar, with the pitch rising as the plate tightens.
//  * Everything is in double precision: at 96 kHz a 30 Hz two-pole resonator in float is
//    detuned by about 2 percent by coefficient rounding alone.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace plate {

static const int N = 192;      // modes
static const int C = 6;        // stretch channels
static const int BLK = 32;     // control block (engine samples)
static const double PI = 3.14159265358979323846;

enum Type { SHEET = 0, TAMTAM, BOSSED, BOWL, NTYPES };

// xorshift64, the same generator and draw order as the reference
struct Rng {
	uint64_t s = 0x2545F4914F6CDD1DULL;
	void seed(uint64_t v) { s = v ? v : 0x2545F4914F6CDD1DULL; }
	double uni() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (double)(s >> 11) * (1.0 / 9007199254740992.0); }
	double bi() { return 2.0 * uni() - 1.0; }
};

// Physical controls, already mapped from knobs (the module and the tests fill these).
struct Controls {
	double drive = 0.0;      // force scale (0 = loop off). Reference units: 150 .. 90000
	double fc = 262.0;       // focus filter centre, Hz
	double focus = 0.5;      // 0 .. 1
	double delay = 0.0;      // loop delay, seconds (0 .. 0.004)
	double curve = 0.6;      // dish (quadratic stretch term)
	double eta = 1.0;        // cross-mode stretch term
	double tune = 1.0;       // all mode frequencies multiplied by this
	double decay = 1.0;      // T60 multiplier
	double damp = 0.0;       // hand / grab
	double posD = 0.31;      // driver position 0..1
	double posP = 0.31;      // loop pickup position 0..1
	double bloom = 1.0;      // stretch strength (THICK)
	double gain = 40.0;      // loop gain
	double pick = 0.0;       // pickup bend 0..1
	double amp = 0.0;        // amplifier saturation 0..1
	double drv = 0.0;        // driver saturation 0..1
	double rat = 0.0;        // rattle 0..1
	double pump = 0.0;       // 0..1
	double rate = 0.3;       // pump rate 0..1
	double nu = 4e-5;        // loss on the rate of stretching
	double stretch = 1.0;    // pitch-rise scale 0..2
	double bright = 0.0;     // output pickup brightness 0..1
	double bleed = 0.0;      // driver bleed into the output 0..1
	double out = 0.0;        // output clipping 0..1
	double inLevel = 0.0;    // audio-in gain into the loop (already includes 600 / 5 V scaling)
	double noise = 1e-4;     // seed noise in the loop
	double mallet = 900.0;   // strike brightness (Hz)
	double hum = 0.0;        // hum amplitude added to the pickup signal (AGE)
	double ampBias = 0.0;    // extra amplifier bias (AGE)
	double ratDamp = 2.0;    // rattle head damping to ground (the reference used 12; see README)
};

struct Engine {
	// ---- mode table (built per TYPE / CAST)
	double r[N] = {};          // frequency ratios (r[0] = lowest, = 1 for SHEET)
	double f1 = 65.4;          // base frequency: f0_j = f1 * r_j
	double kw[N] = {}, ph[N] = {};
	double oLs[N] = {}, oRs[N] = {};
	double A[C][N] = {}, Lm[C][N] = {}, Sg[C][N] = {};
	int type = SHEET, cast = 0;
	Rng rng;                   // after the build it carries on as the loop noise source

	// ---- per-block coefficients
	double a1[N] = {}, a2[N] = {}, bb[N] = {}, w[N] = {}, ow[N] = {}, sw[N] = {}, f0[N] = {};
	double dW[N] = {}, pW[N] = {};
	double oLw[N] = {}, oRw[N] = {};   // output pickup * brightness
	double P[C][N] = {}, Lw[C][N] = {}, Gs[C][N] = {};   // A*w, Lm*w, Sg*sqrt(w)
	double detune[N] = {};     // AGE dents (multipliers, 1 = none)

	// ---- state
	double q[N] = {}, qp[N] = {};
	double Sprev[C] = {}, Sbar[C] = {};
	double svl = 0, svb = 0;
	static const int DL = 2048;
	double dl[DL] = {};
	int dp = 0;
	double env = 0, env2 = 0, pk = 0, pg = 1;
	int trip = 0;
	double dx = 0, dv = 0;
	double Eacc[N] = {};
	double strikeV = 0; bool strikePending = false;
	double humPh = 0;

	// ---- outputs and meters
	double outL = 0, outR = 0;
	double ypick = 0;          // loop pickup signal, last sample (for calibration)
	double energy = 0;         // total mode energy, last sample
	double epot = 0;           // stretch potential 0.5 * bloom * sum S^2, last sample
	double on[N];
	double capHold = 0;
	double kroom = 1e30, gs2[C] = {0}, lwMax[C] = {0}, kscale = 1;
	double g2s = 0;          // largest static |dS/dq|^2 over channels (diagnostics)
	double smaxSeen = 0;       // largest |S| seen (diagnostics)
	double guardE = 1e9;       // energy guard threshold
	double smax = 1e30;        // stretch cap (set in coefs)
	double stiffSeen = 0;      // largest added stiffness / w0^2 seen (diagnostics)
	double pr = 1;             // participation ratio over the last meter window
	double emean = 0;          // mean energy over the last meter window
	long guards = 0;
	long kclipN = 0;
	double tripT = 0;
	double kmul = 2.0;       // the stiffness bound is a sum of upper bounds; 2x still never blows up in the fuzz test
	long softN = 0;          // samples spent on the energy ceiling (diagnostics)
	bool idle = false;

	double fe = 96000, T = 1.0 / 96000;
	int blk = 0;
	int meterN = 0, meterLen = 4800;
	double sbarK = 0.001;
	Controls c;

	void setRate(double rate) {
		fe = rate; T = 1.0 / rate;
		meterLen = std::max(1, (int)std::lround(0.05 * rate));
		sbarK = 1.0 - std::exp(-1.0 / (0.0104 * rate));   // 0.001 at 96 kHz as in the reference
		blk = 0;
	}

	// ------------------------------------------------------------ mode tables
	static int plateRatios(double* out, int nmax, double p, Rng& g) {
		struct Cd { double r; int m; };
		static Cd cd[4096];
		int nc = 0;
		for (int m = 0; m <= 70; m++)
			for (int n = 0; n <= 35; n++) {
				int qq = m + 2 * n;
				if (qq < 2) continue;
				double ratio = std::pow(qq * 0.5, p);
				if (m == 0) ratio *= 1.45;
				if (m == 1) ratio *= 1.18;
				ratio *= 1.0 + 0.025 * g.bi();
				if (ratio > 2000.0 || nc >= 4096) continue;
				cd[nc++] = {ratio, m};
			}
		std::sort(cd, cd + nc, [](const Cd& a, const Cd& b) { return a.r < b.r; });
		int k = 0;
		for (int i = 0; i < nc && k < nmax; i++) {
			out[k++] = cd[i].r;
			if (cd[i].m > 0 && k < nmax) out[k++] = cd[i].r * (1.0 + 0.003 * (0.1 + 0.9 * g.uni()));   // beating twin
		}
		return k;
	}

	// Builds the mode table. CAST 0 of SHEET reproduces the reference exactly (same draws, same order).
	void build(int type_, int cast_) {
		type = type_; cast = cast_;
		rng.seed(cast == 0 ? 0x2545F4914F6CDD1DULL : 0x2545F4914F6CDD1DULL ^ (0x9E3779B97F4A7C15ULL * (uint64_t)cast));
		if (type == SHEET) {
			for (int j = 0; j < N; j++) {
				r[j] = 1.0 + 0.6 * (j + (j ? 0.45 * rng.bi() : 0.0));
				kw[j] = 3.1 * std::sqrt(r[j]);
				ph[j] = 2 * PI * rng.uni();
				oLs[j] = std::cos(kw[j] * 0.13 + ph[j]);
				oRs[j] = std::cos(kw[j] * 0.88 + ph[j]);
			}
		} else {
			Rng g; g.seed(0xA5A5F00DULL + 7919ULL * (uint64_t)cast + 104729ULL * (uint64_t)type);
			int k = 0;
			if (type == TAMTAM) {
				k = plateRatios(r, N, 2.0, g);
			} else if (type == BOSSED) {
				static const double R[] = {1.0, 2.005, 2.76, 2.995, 4.07, 5.43, 6.1, 8.2};
				static const bool TW[] = {true, true, false, false, false, false, false, false};
				for (int i = 0; i < 8 && k < N; i++) {
					double rr = R[i] * (1.0 + 0.004 * g.bi() * (i > 0));
					r[k++] = rr;
					if (TW[i]) r[k++] = rr * (1.0 + 0.006 * (0.6 + 0.4 * g.uni()));
				}
				double tmp[N];
				int np = plateRatios(tmp, N - k, 1.8, g);
				for (int i = 0; i < np && k < N; i++) r[k++] = tmp[i] * 2.3;
			} else {   // BOWL
				static const double R[] = {1.0, 2.76, 5.10, 7.90, 11.1, 14.6, 18.4};
				for (int i = 0; i < 7; i++) {
					double rr = R[i] * (1.0 + 0.003 * g.bi() * (i > 0));
					r[k++] = rr;
					r[k++] = rr + (0.3 + 0.9 * g.uni()) / 200.0 * rr;
				}
				int i = 0;
				while (k < N) { r[k++] = 20.0 + 0.6 * (i + 0.45 * g.bi()); i++; }
			}
			for (; k < N; k++) r[k] = r[k - 1] * 1.01;
			std::sort(r, r + N);
			double r0 = r[0];
			for (int j = 0; j < N; j++) r[j] /= r0;
			for (int j = 0; j < N; j++) {
				kw[j] = 3.1 * std::sqrt(r[j]);
				ph[j] = 2 * PI * rng.uni();
				oLs[j] = std::cos(kw[j] * 0.13 + ph[j]);
				oRs[j] = std::cos(kw[j] * 0.88 + ph[j]);
			}
		}
		for (int cc = 0; cc < C; cc++)
			for (int j = 0; j < N; j++) {
				A[cc][j] = (rng.uni() < 0.5 ? -1.0 : 1.0) / std::sqrt((double)C * N);
				Lm[cc][j] = (0.3 + rng.uni()) / std::sqrt((double)C) * 0.8;
				Sg[cc][j] = (rng.uni() < 0.5 ? -1.0 : 1.0) / std::sqrt((double)C);
			}
		for (int j = 0; j < N; j++) detune[j] = 1.0;
		blk = 0; coefDirty = true;
	}

	void reset() {
		std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp);
		std::memset(Sprev, 0, sizeof Sprev); std::memset(Sbar, 0, sizeof Sbar);
		std::memset(dl, 0, sizeof dl); std::memset(Eacc, 0, sizeof Eacc);
		dp = 0; svl = svb = 0; env = env2 = pk = 0; pg = 1; trip = 0; dx = dv = 0;
		strikePending = false; meterN = 0; pr = 1; emean = 0; energy = 0; idle = false; blk = 0; capHold = 0;
	}

	void strike(double v) { strikeV = v; strikePending = true; idle = false; }

	// The anchor mode for TRACK = PLATE: the mode whose ratio is nearest to target.
	// Among the modes within about a minor third of target, the one the loop couples to best at these
	// driver and pickup positions (so it is the one that sings, and the sung note lands on the pitch).
	int anchor(double target, double xD, double xP) const {
		int best = 0; double bw = -1, bd = 1e30; int nearest = 0;
		for (int j = 0; j < N; j++) {
			double d = std::fabs(std::log(r[j] / target));
			if (d < bd) { bd = d; nearest = j; }
			if (d > 0.18) continue;
			double wgt = std::cos(kw[j] * xD + ph[j]) * std::cos(kw[j] * xP + ph[j]) * (1.0 - 2.0 * d);
			if (wgt > bw) { bw = wgt; best = j; }
		}
		return bw > 0 ? best : nearest;
	}

	double ck[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
	bool coefDirty = true;
	void coefs() {
		const double key[8] = {c.tune, c.decay, c.damp, c.bright, f1, c.posD, c.posP, fe};
		bool same = !coefDirty;
		for (int i = 0; i < 8 && same; i++) same = key[i] == ck[i];
		if (same) return;
		for (int i = 0; i < 8; i++) ck[i] = key[i];
		coefDirty = false;
		for (int j = 0; j < N; j++) {
			f0[j] = f1 * r[j];
			double f = f0[j] * c.tune * detune[j];
			// Modes above 0.25 fe cannot be integrated stably once stretched, and clamping piles them onto one
			// frequency. They are muted instead (at 2x oversampling that is above 22 kHz).
			bool mute = f > 0.25 * fe;
			f = std::min(f, 0.25 * fe);
			w[j] = 2 * PI * f;
			double t60 = c.decay * 9.0 / (1 + f / 700.0);
			double al = 6.908 / t60 * (1 + c.damp * (8 + f / 40.0));
			double e = std::exp(-al * T);
			a1[j] = 2 * e * std::cos(w[j] * T);
			a2[j] = e * e;
			bb[j] = e * T * std::sin(w[j] * T) / w[j];
			ow[j] = 1.0 / (1 + f / (1500.0 * (1 + 4 * c.bright)));
			sw[j] = std::sqrt(w[j]);
			oLw[j] = oLs[j] * ow[j]; oRw[j] = oRs[j] * ow[j];
			dW[j] = std::cos(kw[j] * c.posD + ph[j]);
			pW[j] = std::cos(kw[j] * c.posP + ph[j]);
			on[j] = mute ? 0.0 : 1.0;
			if (mute) { a1[j] = a2[j] = bb[j] = 0; dW[j] = pW[j] = 0; oLw[j] = oRw[j] = 0; }
		}
		// Stability: the explicit stretch force adds up to 3.5 * w_j * |S| of stiffness to mode j. Keep the
		// stiffest mode below w T = 1.5 by capping |S| (soft). The approved sketches stay 20x below the cap.
		double wmax = 1; for (int j = 0; j < N; j++) if (on[j] > 0) wmax = std::max(wmax, w[j]);
		double room = (1.5 * fe) * (1.5 * fe) - wmax * wmax;
		smax = std::max(room, 0.1 * wmax * wmax) / (3.5 * wmax);
		// Headroom for the stiffness the stretching adds (all terms, see process): keep w_eff T below 1.9.
		kroom = kmul * std::max((1.9 * fe) * (1.9 * fe) - wmax * wmax, 0.05 * fe * fe);
		g2s = 0;
		for (int cc = 0; cc < C; cc++) {
			double g2 = 0;
			for (int j = 0; j < N; j++) g2 += (A[cc][j] * w[j] * on[j]) * (A[cc][j] * w[j] * on[j]);
			g2s = std::max(g2s, g2);
		}
		for (int cc = 0; cc < C; cc++)
			for (int j = 0; j < N; j++) {
				P[cc][j] = A[cc][j] * w[j] * on[j];
				Lw[cc][j] = Lm[cc][j] * w[j] * on[j];
				Gs[cc][j] = Sg[cc][j] * sw[j] * on[j];
			}
		for (int cc = 0; cc < C; cc++) {
			double b2 = 0, lm = 0;
			for (int j = 0; j < N; j++) { b2 += Gs[cc][j] * Gs[cc][j]; lm = std::max(lm, std::fabs(Lw[cc][j])); }
			gs2[cc] = b2; lwMax[cc] = lm;
		}
	}

	// One engine sample. `in` is the audio input (already scaled by the caller). Returns outL/outR.
	void process(double in) {
		if (blk == 0) coefs();
		if (++blk >= BLK) blk = 0;

		const bool active = c.drive > 0.0 || strikePending || std::fabs(in) > 1e-12 || energy > 1e-9;
		if (!active) {
			if (!idle) {
				std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp);
				std::memset(Sprev, 0, sizeof Sprev); std::memset(Sbar, 0, sizeof Sbar);
				svl = svb = 0; std::memset(dl, 0, sizeof dl); dx = dv = 0; env = env2 = 0;
				idle = true;
			}
			outL = outR = 0; energy = 0; pr = 1; emean = 0;
			return;
		}
		idle = false;

		// ---- pickup
		double y = 0, up = 0, uL = 0, uR = 0;
		for (int j = 0; j < N; j++) {
			y += pW[j] * (q[j] - qp[j]);
			up += pW[j] * q[j];
			uL += oLs[j] * q[j];
			uR += oRs[j] * q[j];
		}
		y *= fe;
		{ double g = 1 + 0.85 * c.pick * std::tanh(up * 0.6); y /= g * g; }
		if (c.hum > 0) {
			humPh += 50.0 * T; if (humPh >= 1) humPh -= 1;
			double a = 2 * PI * humPh;
			y += c.hum * (std::sin(a) + 0.5 * std::sin(2 * a) + 0.3 * std::sin(3 * a));
		}
		ypick = y;
		y += in;

		// ---- focus (Chamberlin state variable)
		double g = 2 * std::sin(PI * std::min(c.fc, std::min(20000.0, 0.2 * fe)) / fe), Q = 1.0 + 8 * c.focus;
		svl += g * svb; double hp = y - svl - svb / Q; svb += g * hp; double bp = svb / Q;
		double yf = (1 - c.focus) * y + c.focus * bp * 2.0;

		// ---- phase (delay, linear interpolation)
		dl[dp] = yf;
		double yd = yf;
		if (c.delay > 0) {
			double ds = std::min(c.delay * fe, (double)(DL - 2));
			int di = (int)ds; double fr = ds - di;
			double s0 = dl[(dp - di + DL) & (DL - 1)], s1 = dl[(dp - di - 1 + DL) & (DL - 1)];
			yd = s0 + (s1 - s0) * fr;
		}
		dp = (dp + 1) & (DL - 1);

		// ---- normalise, pump, amplifier, driver
		double x = c.gain * yd / std::max(c.drive, 1.0);
		{
			double ax = std::fabs(x);
			const double tau = 0.6 * std::pow(0.02, c.rate);
			double k = 1 - std::exp(-T / tau);
			env += (ax - env) * k; env2 += (env - env2) * k;
			pk = std::max(pk * (1 - T / 8.0), env2);
			// The breaker also resets after a hold time (twice the follower time): without it, a loop that
			// is pumped from the start can settle into a second tone while tripped and never recover.
			if (!trip && env2 > 0.85 * pk && env2 > 0.3) { trip = 1; tripT = 0; }   // 0.3: well below any singing loop (1 to 15)
			else if (trip) {
				tripT += T;
				if (env2 < 0.85 * pk * (1 - 0.9 * c.pump) || (c.pump > 0 && tripT > 1.2 * tau)) trip = 0;
			}
			double tg = (trip && c.pump > 0) ? 1 - 1.6 * c.pump : 1;
			pg += (tg - pg) * (1 - std::exp(-T / 0.003));
			x *= pg;
		}
		{ double ga = 1 + 29 * c.amp * c.amp, b = 0.45 * c.amp + c.ampBias; x = std::tanh(ga * x + b) - std::tanh(b); }
		{
			double h = x * (1 + 12 * c.drv * c.drv); h = h > 1 ? 1 : (h < -1 ? -1 : h);
			double h2 = h * h;
			x = (1 - 0.5 * c.drv) * h + 0.5 * c.drv * h2 * h2 * h + 0.7 * c.drv * (std::fabs(h) - 0.6);
		}
		double F = 0.0;
		if (c.drive > 0) F = c.drive * x + c.noise * rng.bi();
		if (c.rat > 0 && c.drive > 0) {
			double Pp = c.drive * (2 * (1 - c.rat) + 0.03), kc = 1.1e6, md = 0.003, d = dx - up;
			double Fc = d > 0 ? kc * d : 0;
			dv += T * (F + Pp - Fc - c.ratDamp * dv) / md; dx += T * dv; F = Fc - Pp;
		}

		if (strikePending) {
			for (int j = 0; j < N; j++) qp[j] -= strikeV * dW[j] * T * std::exp(-f0[j] / c.mallet);
			strikePending = false;
		}

		// ---- stretching
		double S[C], Gq[C], Sa[C], Sl[C], Sgq[C], grad2[C], ktot = 0;
		double pot = 0;
		for (int cc = 0; cc < C; cc++) {
			const double* Pc = P[cc]; const double* Lc = Lw[cc]; const double* Gc = Gs[cc];
			double s = 0, s2 = 0, gq = 0, saa = 0, sab = 0;
			const double cv = c.curve;
			for (int j = 0; j < N; j++) {
				s += Pc[j] * q[j]; s2 += Lc[j] * q[j] * q[j]; gq += Gc[j] * q[j];
				double a = cv * Pc[j] + Lc[j] * q[j]; saa += a * a; sab += a * Gc[j];
			}
			Gq[cc] = c.eta * c.eta * gq;
			grad2[cc] = saa + 2 * Gq[cc] * sab + Gq[cc] * Gq[cc] * gs2[cc];
			double Sr = c.curve * s + 0.5 * s2 + 0.5 * Gq[cc] * gq;
			pot += Sr * Sr;
			double sd = (Sr - Sprev[cc]) * fe; Sprev[cc] = Sr;
			Sbar[cc] += (Sr - Sbar[cc]) * sbarK;
			S[cc] = Sr - (1 - c.stretch) * Sbar[cc] + c.nu * sd;
			{ double sb = smax / std::max(c.bloom, 0.05); S[cc] = sb * std::tanh(S[cc] / sb); }
			smaxSeen = std::max(smaxSeen, std::fabs(S[cc]));
			ktot += c.bloom * (grad2[cc] + std::fabs(S[cc]) * (lwMax[cc] + c.eta * c.eta * gs2[cc]));
		}
		// Stability: the stretching stiffens the plate with amplitude (that is the physics), but an explicit
		// step cannot follow a stiffness beyond the sample rate. Past the headroom, the stretch forces are
		// scaled back for that sample, so the hardest-driven settings saturate instead of blowing up.
		kscale = ktot > kroom ? kroom / ktot : 1.0;
		if (kscale < 1.0) kclipN++;
		for (int cc = 0; cc < C; cc++) {
			double sk = S[cc] * kscale;
			Sa[cc] = sk * c.curve; Sl[cc] = sk; Sgq[cc] = sk * Gq[cc];
		}

		// ---- modes (force terms first, channel-outer so the loops vectorise)
		double fa[N], fl[N];
		for (int j = 0; j < N; j++) { fa[j] = 0; fl[j] = 0; }
		for (int cc = 0; cc < C; cc++) {
			const double sa = Sa[cc], sg = Sgq[cc], sl = Sl[cc];
			const double* Pc = P[cc]; const double* Gc = Gs[cc]; const double* Lc = Lw[cc];
			for (int j = 0; j < N; j++) { fa[j] += sa * Pc[j] + sg * Gc[j]; fl[j] += sl * Lc[j]; }
		}
		double L = 0, R = 0, E = 0;
		const double bl = c.bloom;
		for (int j = 0; j < N; j++) {
			double fn = fa[j] + fl[j] * q[j];
			double qn = a1[j] * q[j] - a2[j] * qp[j] + bb[j] * (dW[j] * F - bl * fn);
			double v = (qn - q[j]) * fe;
			double e = 0.5 * v * v + 0.5 * w[j] * w[j] * qn * qn;
			E += e; Eacc[j] += e;
			L += oLw[j] * v; R += oRw[j] * v;
			qp[j] = q[j]; q[j] = qn;
		}
		// Energy ceiling. Above it the plate loses amplitude smoothly (a soft limiter on the state, like a
		// plate shedding energy into the air much faster when hammered). The settings where the model breaks
		// up sit on this ceiling and stay musical; a hard reset is kept only for numbers that are no longer
		// finite or for growth the soft loss cannot hold (100x the ceiling).
		{
			// The ceiling follows the energy the drive can put in; it falls slowly after the drive is cut so a
			// ringing plate is not clamped.
			double capNow = guardE * std::max(1.0, (c.drive / 90000.0) * (c.drive / 90000.0)) * std::max(1.0, c.tune * c.tune);
			capHold = std::max(capNow, capHold * (1 - T / 1.5));
			double cap = capHold;
			if (!(E < 100 * cap)) {
				guards++;
				for (int j = 0; j < N; j++) {
					if (!std::isfinite(q[j]) || !std::isfinite(qp[j])) { q[j] = qp[j] = 0; }
					q[j] *= 0.1; qp[j] *= 0.1;
				}
				for (int cc = 0; cc < C; cc++) { Sprev[cc] = 0; Sbar[cc] = 0; }
				E *= 0.01;
				if (!std::isfinite(E)) E = 0;
				if (!std::isfinite(L) || !std::isfinite(R)) L = R = 0;
			} else if (E > cap) {
				double r = 1.0 - 0.01 * (1.0 - cap / E);
				for (int j = 0; j < N; j++) { q[j] *= r; qp[j] *= r; }
				softN++;
			}
		}
		energy = E;
		epot = 0.5 * c.bloom * pot;
		{ double gl = 1 + 0.85 * c.pick * std::tanh(uL * 0.6), gr = 1 + 0.85 * c.pick * std::tanh(uR * 0.6); L /= gl * gl; R /= gr * gr; }
		L += c.bleed * 0.06 * F; R += c.bleed * 0.06 * F;
		{ double go = 1 + 14 * c.out * c.out, rr = 2500; double sg = 1.0 / std::sqrt(go); L = rr * std::tanh(go * L / rr) * sg; R = rr * std::tanh(go * R / rr) * sg; }
		outL = L; outR = R;

		if (++meterN >= meterLen) {
			double s = 0, s2 = 0;
			for (int j = 0; j < N; j++) { s += Eacc[j]; s2 += Eacc[j] * Eacc[j]; Eacc[j] = 0; }
			pr = s * s / (s2 + 1e-30);
			emean = s / meterN;
			meterN = 0;
		}
	}
};

// Half-band decimator (2x -> 1x), 31-tap windowed sinc.
struct Decimator {
	static const int NT = 31;
	double h[NT];
	double buf[2][64] = {};
	int pos = 0;
	Decimator() {
		const int M = NT - 1;
		double sum = 0;
		for (int i = 0; i < NT; i++) {
			double n = i - M / 2.0;
			double s = n == 0 ? 0.5 : std::sin(PI * 0.5 * n) / (PI * n);
			double wv = 0.42 - 0.5 * std::cos(2 * PI * i / M) + 0.08 * std::cos(4 * PI * i / M);
			h[i] = s * wv; sum += h[i];
		}
		for (int i = 0; i < NT; i++) h[i] /= sum;
	}
	void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
	// push two engine samples per channel, get one host sample
	void process(const double* l2, const double* r2, double& L, double& R) {
		for (int k = 0; k < 2; k++) {
			buf[0][pos] = buf[0][pos + 32] = l2[k];
			buf[1][pos] = buf[1][pos + 32] = r2[k];
			pos = (pos + 1) & 31;
		}
		double sl = 0, sr = 0;
		for (int i = 0; i < NT; i++) { int idx = pos + 32 - 1 - i; sl += h[i] * buf[0][idx]; sr += h[i] * buf[1][idx]; }
		L = sl; R = sr;
	}
};

// Slow 2:1 level rider (matches what the approved sketches went through), then nothing else.
struct Leveller {
	double ms = 0, k = 0, ref = 2500;
	bool on = true;
	void setRate(double rate) { k = 1 - std::exp(-1.0 / (0.075 * rate)); }
	void reset() { ms = 0; }
	double gain(double L, double R) {
		ms += ((L * L + R * R) * 0.5 - ms) * k;
		if (!on) return 1.0;
		double rms = std::sqrt(ms);
		return std::pow(std::max(rms, 0.01 * ref) / ref, -0.5);
	}
};

} // namespace plate
