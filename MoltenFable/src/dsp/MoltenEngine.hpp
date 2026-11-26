// Molten engine: Plate Driver's singing sheet with a temperature map on top.
// No Rack code here, so test/ can run it offline. Ported from tools/proto/molten_ref.c, which defines the
// sound; docs/BUILD-INSTRUCTIONS.md section 3 has the maths.
//
// In plain words:
//  * The sheet is 192 ringing modes (exact two-pole resonators), held in a feedback loop by a pickup and a
//    driver at the same point, exactly as in Plate Driver. Six "stretch channels" let the modes feed each
//    other, which is what makes one pure tone split into chords and roar.
//  * Over the sheet lies a strip of 16 thermal zones. A flame heats them, heat spreads between them, and a
//    zone must take in extra (latent) heat at the melting point before it turns liquid.
//  * Each mode knows how much of itself lives in each zone. Heat sinks its pitch and shortens its ring by
//    the temperature of those zones; it loosens the metal (the stretching sees the motion larger), so the
//    loop splits at a drive where cold metal sings one tone; a molten zone swaps plate stiffness for
//    surface tension, so the modes slide down into a dense low cluster.
//  * When the liquid sets, the shape it holds at that instant becomes a new tuning of the solid (casting).
//  * Double precision throughout, as Plate Driver (low two-pole resonators in float drift by percents).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace molten {

static const int N = 192;      // modes
static const int C = 6;        // stretch channels
static const int K = 16;       // thermal zones
static const int BLK = 32;     // control block (engine samples)
static const double PI = 3.14159265358979323846;

// xorshift64, the same generator and draw order as the reference
struct Rng {
	uint64_t s = 0x2545F4914F6CDD1DULL;
	void seed(uint64_t v) { s = v ? v : 0x2545F4914F6CDD1DULL; }
	double uni() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (double)(s >> 11) * (1.0 / 9007199254740992.0); }
	double bi() { return 2.0 * uni() - 1.0; }
};

// Physical controls, already mapped from knobs (the module and the tests fill these).
struct Controls {
	// ---- the singing sheet (as Plate Driver)
	double drive = 0.0;      // force scale (0 = loop off)
	double fc = 262.0;       // focus filter centre, Hz
	double focus = 0.5;      // 0 .. 1
	double curve = 0.6;      // dish (quadratic stretch term)
	double eta = 1.0;        // cross-mode stretch term
	double tune = 1.0;       // all mode frequencies multiplied by this (pitch)
	double decay = 1.0;      // T60 multiplier
	double damp = 0.0;       // hand / grab
	double pos = 0.31;       // driver and pickup position 0..1
	double strikeX = 0.23;   // where the mallet lands
	double bloom = 1.0;      // stretch strength (THICK)
	double gain = 40.0;      // loop gain
	double pick = 0.0;       // pickup bend 0..1
	double amp = 0.0;        // amplifier saturation 0..1
	double ampBias = 0.0;    // extra amplifier bias (AGE)
	double nu = 4e-5;        // loss on the rate of stretching
	double bright = 0.0;     // output pickup brightness 0..1
	double out = 0.0;        // output clipping 0..1
	double inLevel = 0.0;    // audio-in gain into the loop
	double noise = 1e-4;     // seed noise in the loop
	double mallet = 900.0;   // strike brightness (Hz)
	double hum = 0.0;        // hum added to the pickup signal (AGE)
	// ---- heat
	double heat = 0.0;       // flame temperature: 0 cold, 1 melting point, 2 white-hot
	double rate = 3.0;       // how fast the zones follow the flame, 1/s
	double xf = 0.3;         // flame position 0..1
	double wf = 2.0;         // flame width (2 = the whole body)
	double cond = 4.0;       // heat conduction between zones
	double air = 0.0;        // cooling to the air
	double quench = 0.0;     // 0..1, water
	double latent = 0.5;     // latent heat (how long it lingers at the melting point)
	double uc = 0.0;         // supercooling: how far below the melting point liquid can go
	double sag = 0.6;        // how far heat sinks the solid's pitch
	double tens = 1.0;       // liquid pitch relative to the solid
	double loose = 5.0;      // looseness of hot solid
	double liqnl = 400.0;    // looseness of liquid
	double cast = 1.0;       // casting depth
	double ping = 0.5;       // stress pings
	double self = 0.0;       // self-heating from the vibration (UNTESTED in the reference)
	double slag = 0.0;       // AGE: zones that deaden the modes living in them
};

struct Engine {
	// ---- mode table (built per BODY)
	double r[N] = {}, r075[N] = {};
	double f1 = 65.4;                 // lowest mode of the cold, uncast body
	double kw[N] = {}, ph[N] = {};
	double oLs[N] = {}, oRs[N] = {};
	double A[C][N] = {}, Lm[C][N] = {}, Sg[C][N] = {};
	double zw[N][K] = {};             // how much of mode j lives in zone k (sums to 1 over k)
	double slagW[N] = {};             // AGE slag weight per mode
	int body = 0;
	Rng rng;                          // after the build it carries on as the noise / ping / casting source

	// ---- per-block coefficients
	double a1[N] = {}, a2[N] = {}, bb[N] = {}, w[N] = {}, ow[N] = {}, sw[N] = {}, f0[N] = {};
	double dW[N] = {}, pW[N] = {}, sX[N] = {};
	double oLw[N] = {}, oRw[N] = {};
	double P[C][N] = {}, Lw[C][N] = {}, Gs[C][N] = {};
	double gs2[C] = {}, lwMax[C] = {};
	double on[N] = {};
	double detune[N] = {};            // AGE dents (multipliers, 1 = none)
	double cast[N] = {}, castT[N] = {};
	double smax = 1e30, kroom = 1e30;
	double kmul = 2.0;

	// ---- thermal state
	double h[K] = {}, T[K] = {}, phi[K] = {}, Tp[K] = {}, phip[K] = {};
	int liq[K] = {}, nuc[K] = {};
	double zMelt[K] = {}, zRate[K] = {};   // AGE: per-zone melting point offset and heating-rate scale
	double M = 0, Tsol = 0, Tliq = 1, Mprev = 0, Tmean = 0;
	double lam = 1, curveEff = 0.6;
	long casts = 0, pings = 0;
	double selfT = 0;                 // self-heating target (0 = none)
	bool pinged = false;

	// ---- state
	double q[N] = {}, qp[N] = {};
	double Sprev[C] = {};
	double svl = 0, svb = 0;
	double Eacc[N] = {};
	double strikeV = 0; bool strikePending = false;
	double humPh = 0;
	double tunePrev = -1;

	// ---- outputs and meters
	double outL = 0, outR = 0;
	double energy = 0, ekin = 0, epot = 0;
	double capHold = 0;
	double pr = 1, emean = 0;
	int top = 0;                      // loudest mode over the last meter window
	long guards = 0, kclipN = 0, clampN = 0, softN = 0;
	bool idle = false;
	bool allowIdle = true;            // the reference-match test turns this off (the reference never idles)
	double guardE = 1e9;

	double fe = 96000, T1 = 1.0 / 96000;
	int blk = 0;
	int meterN = 0, meterLen = 4800;
	double castK = 0.02, pingScale = 1.0;
	Controls c;

	Engine() { for (int j = 0; j < N; j++) { cast[j] = castT[j] = detune[j] = 1.0; } for (int k = 0; k < K; k++) zRate[k] = 1.0; }

	void setRate(double rate) {
		fe = rate; T1 = 1.0 / rate;
		meterLen = std::max(1, (int)std::lround(0.05 * rate));
		double dt = BLK / fe;
		castK = 1.0 - std::exp(-dt / 0.0165);          // 0.02 per block at 96 kHz, as the reference
		pingScale = dt / (BLK / 96000.0);              // pings were tuned per 32-sample block at 96 kHz
		blk = 0;
	}

	// Builds the mode table. BODY 0 reproduces the reference exactly (same draws, same order).
	void build(int body_) {
		body = body_;
		rng.seed(body == 0 ? 0x2545F4914F6CDD1DULL : 0x2545F4914F6CDD1DULL ^ (0x9E3779B97F4A7C15ULL * (uint64_t)body));
		for (int j = 0; j < N; j++) {
			r[j] = 1.0 + 0.6 * (j + (j ? 0.45 * rng.bi() : 0.0));
			r075[j] = std::pow(r[j], 0.75);
			kw[j] = 3.1 * std::sqrt(r[j]);
			ph[j] = 2 * PI * rng.uni();
			oLs[j] = std::cos(kw[j] * 0.13 + ph[j]);
			oRs[j] = std::cos(kw[j] * 0.88 + ph[j]);
			double tot = 0;
			for (int k = 0; k < K; k++) {
				double s = 0;
				for (int i = 0; i < 8; i++) { double x = (k + (i + 0.5) / 8) / K, cc = std::cos(kw[j] * x + ph[j]); s += cc * cc; }
				zw[j][k] = s / 8 + 0.1; tot += zw[j][k];
			}
			for (int k = 0; k < K; k++) zw[j][k] /= tot;
		}
		for (int cc = 0; cc < C; cc++)
			for (int j = 0; j < N; j++) {
				A[cc][j] = (rng.uni() < 0.5 ? -1.0 : 1.0) / std::sqrt((double)C * N);
				Lm[cc][j] = (0.3 + rng.uni()) / std::sqrt((double)C) * 0.8;
				Sg[cc][j] = (rng.uni() < 0.5 ? -1.0 : 1.0) / std::sqrt((double)C);
			}
		for (int j = 0; j < N; j++) { detune[j] = 1.0; slagW[j] = 0.0; }
		blk = 0;
	}

	// AGE per-zone values (u, v, s in -1..1, 0..1) and the slag weight of every mode.
	void setZones(const double* u, const double* v, const double* s, double age) {
		for (int k = 0; k < K; k++) { zMelt[k] = 0.12 * age * u[k]; zRate[k] = 1.0 + 0.3 * age * v[k]; }
		for (int j = 0; j < N; j++) { double a = 0; for (int k = 0; k < K; k++) a += zw[j][k] * s[k] * s[k]; slagW[j] = a; }
	}

	void reset() {
		std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp);
		std::memset(Sprev, 0, sizeof Sprev); std::memset(Eacc, 0, sizeof Eacc);
		svl = svb = 0; strikePending = false; meterN = 0; pr = 1; emean = 0; energy = 0; idle = false; blk = 0; capHold = 0; selfT = 0;
		tunePrev = -1;
		for (int j = 0; j < N; j++) w[j] = 0;
	}

	// Thermal state as if the body had sat at flame temperature `heat` for ever (the reference's start).
	void setThermal(double heat) {
		for (int k = 0; k < K; k++) {
			h[k] = heat < 1 ? heat : heat + c.latent; liq[k] = heat > 1; nuc[k] = -1;
			T[k] = Tp[k] = heat; phi[k] = phip[k] = heat > 1 ? 1.0 : 0.0;
		}
		Mprev = heat > 1 ? 1 : 0;
		aggregates();
		coefs(false);
	}

	void strike(double v) { strikeV = v; strikePending = true; idle = false; }

	// Every mode's tuning target back to the original (context menu "Forget casting").
	void forgetCasting() { for (int j = 0; j < N; j++) castT[j] = 1.0; }

	// The anchor mode for TRACK = PLATE (as Plate Driver, collocated pickup).
	int anchor(double target, double x) const {
		int best = 0; double bw = -1, bd = 1e30; int nearest = 0;
		for (int j = 0; j < N; j++) {
			double d = std::fabs(std::log(r[j] / target));
			if (d < bd) { bd = d; nearest = j; }
			if (d > 0.18) continue;
			double cc = std::cos(kw[j] * x + ph[j]);
			double wgt = cc * cc * (1.0 - 2.0 * d);
			if (wgt > bw) { bw = wgt; best = j; }
		}
		return bw > 0 ? best : nearest;
	}

	void aggregates() {
		double ms = 0, tm = 0; M = 0; Tsol = 0; Tliq = 0;
		for (int k = 0; k < K; k++) { M += phi[k] / K; Tsol += T[k] * (1 - phi[k]); ms += 1 - phi[k]; Tliq += T[k] * phi[k]; tm += T[k]; }
		Tsol = ms > 1e-6 ? Tsol / ms : 1; Tliq = M > 1e-6 ? Tliq / (M * K) : 1;
		Tmean = tm / K;
	}

	// ------------------------------------------------------------ heat (once per block)
	void thermal() {
		const double dt = BLK * T1;
		double lap[K];
		for (int k = 0; k < K; k++) { double tl = T[k > 0 ? k - 1 : k], tr = T[k < K - 1 ? k + 1 : k]; lap[k] = tl - 2 * T[k] + tr; }
		// Self-heating (spec 3.6, reworked): loud playing raises a slow "vibration temperature" that the whole
		// body is pulled towards on top of the flame, so a held roar at SELF 1 melts the body in about 5 to 10 s.
		// (The spec's additive dh could never beat the flame's own pull back to its temperature.)
		selfT += (std::min(3.0 * c.self * c.self * std::tanh(energy / 5e5), std::max(0.0, 2.2 - c.heat)) - selfT) * (dt / 8.0);
		if (selfT < 1e-12) selfT = 0;
		const double selfH = 0.6 * c.rate * selfT;
		for (int k = 0; k < K; k++) {
			double x = (k + 0.5) / K, d = (x - c.xf) / c.wf, g = std::exp(-d * d);
			double rt = c.rate * zRate[k];
			double dh = c.heat > T[k] ? rt * g * (c.heat - T[k]) : 0.6 * rt * (c.heat - T[k]);
			dh += -c.air * T[k] + c.cond * lap[k] + c.quench * 2.5 * (0.5 + 1.5 * std::fabs(2 * x - 1)) * (0 - T[k]) + selfH;
			h[k] += dh * dt; if (h[k] < 0) h[k] = 0;
		}
		pinged = false;
		for (int k = 0; k < K; k++) {
			const double m = 1.0 + zMelt[k];             // this zone's melting point
			if (h[k] >= m + c.latent) liq[k] = 1;
			if (liq[k] && h[k] < m + c.latent) {           // supercooled
				double Tl = h[k] - c.latent;
				int seed = Tl < m - c.uc;
				int nb = (k > 0 && !liq[k - 1] && phi[k - 1] < 0.999) || (k < K - 1 && !liq[k + 1] && phi[k + 1] < 0.999);
				if (seed) { liq[k] = 0; nuc[k] = -1; }
				else if (nb) {
					if (nuc[k] < 0) nuc[k] = (int)((0.004 + 0.03 * std::max(0.0, 1 - 3 * (m - Tl))) / dt);
					else if (--nuc[k] <= 0) { liq[k] = 0; nuc[k] = -1; }
				}
			} else nuc[k] = -1;
			Tp[k] = T[k]; phip[k] = phi[k];
			if (liq[k]) { T[k] = h[k] - c.latent; phi[k] = 1; }
			else if (h[k] < m) { T[k] = h[k]; phi[k] = 0; }
			else { T[k] = m; phi[k] = (h[k] - m) / c.latent; }
			// stress pings: thermal shock in the solid, and each zone as it freezes
			double p = pingScale * c.ping * (0.6 * std::fabs(T[k] - Tp[k]) * (1 - phi[k]) + 2.0 * std::max(0.0, phip[k] - phi[k]));
			if (rng.uni() < p) {
				double xx = (k + rng.uni()) / K, amp = (60 + 250 * rng.uni() * rng.uni()) * (rng.uni() < 0.5 ? -1 : 1);
				for (int j = 0; j < N; j++) {
					double f = w[j] / (2 * PI);
					qp[j] -= amp * std::cos(kw[j] * xx + ph[j]) * T1 * f * f / (f * f + 1500.0 * 1500.0) * std::exp(-f / 6000.0) * on[j];
				}
				pings++; pinged = true; idle = false;
			}
		}
		aggregates();
		// casting: the shape the liquid holds as it sets becomes the new tuning of the solid
		if (Mprev >= 0.5 && M < 0.5) {
			double rr = 0;
			for (int j = 0; j < N; j++) rr += w[j] * w[j] * q[j] * q[j];
			rr = std::sqrt(rr / N) + 1e-9;
			for (int j = 0; j < N; j++) castT[j] = 1 + 0.14 * c.cast * std::tanh(1.5 * w[j] * q[j] / rr) * (0.4 + 0.6 * rng.uni());
			casts++;
		}
		Mprev = M;
		lam = std::sqrt(std::exp(c.loose * std::min(Tsol, 1.0) * (1 - M)) * (1 + c.liqnl * M));
		curveEff = c.curve * (1 + 0.5 * M);
	}

	// ------------------------------------------------------------ per-block coefficients
	void coefs(bool slew = true) {
		// A change of pitch keeps each mode's energy (scale the state by old/new frequency); changes from
		// heat, melting and casting do not, so a mode whose pitch rises as it freezes blooms (spec 3.2).
		if (tunePrev > 0 && c.tune != tunePrev) { double rr = tunePrev / c.tune; for (int j = 0; j < N; j++) { q[j] *= rr; qp[j] *= rr; } }
		tunePrev = c.tune;
		const double sig = std::max(0.2, 1 - 0.5 * (Tliq - 1)), visc = std::exp(-2.2 * (Tliq - 1));
		const double fl0 = f1 * c.tune * c.tens * std::sqrt(sig);
		const double flRef = f1 * c.tune * c.tens;
		double Tz[K];
		for (int k = 0; k < K; k++) Tz[k] = std::min(T[k], 1.0);
		double wmax = 1;
		for (int j = 0; j < N; j++) {
			double Tb = 0, pb = 0;
			const double* z = zw[j];
			for (int k = 0; k < K; k++) { Tb += z[k] * Tz[k]; pb += z[k] * phi[k]; }
			if (slew) cast[j] += (castT[j] - cast[j]) * castK;
			f0[j] = f1 * r[j];
			double fs = f0[j] * c.tune * cast[j] * detune[j], fl = fl0 * r075[j] * detune[j];
			double f = std::sqrt((1 - pb) * (1 - c.sag * Tb * Tb) * fs * fs + pb * fl * fl);
			if (!(f >= 4)) f = 4;
			bool mute = f > 0.25 * fe;
			if (mute) f = 0.25 * fe;
			double als = 6.908 / (c.decay * 9.0 / (1 + fs / 700.0)) * (1 + 2 * Tb * Tb * Tb);
			double all = 6.908 / (c.decay * 4.0) * std::sqrt(fl / flRef) * visc;
			const double wNew = 2 * PI * f;
			w[j] = wNew;
			double al = ((1 - pb) * als + pb * all) * (1 + 8 * pb * (1 - pb)) * (1 + c.damp * (8 + f / 40.0)) * (1 + 2 * c.slag * slagW[j]);
			double e = std::exp(-al * T1);
			a1[j] = 2 * e * std::cos(w[j] * T1);
			a2[j] = e * e;
			bb[j] = e * T1 * std::sin(w[j] * T1) / w[j];
			ow[j] = 1.0 / (1 + f / (1500.0 * (1 + 4 * c.bright)));
			sw[j] = std::sqrt(w[j]);
			oLw[j] = oLs[j] * ow[j]; oRw[j] = oRs[j] * ow[j];
			dW[j] = std::cos(kw[j] * c.pos + ph[j]);
			pW[j] = dW[j];
			sX[j] = std::cos(kw[j] * c.strikeX + ph[j]);
			on[j] = mute ? 0.0 : 1.0;
			if (mute) { a1[j] = a2[j] = bb[j] = 0; dW[j] = pW[j] = 0; oLw[j] = oRw[j] = 0; }
			wmax = std::max(wmax, w[j]);
		}
		// Stability as Plate Driver: cap |S| softly, and scale the stretch forces back for a sample when the
		// stiffness they add would exceed what the explicit step can follow.
		double room = (1.5 * fe) * (1.5 * fe) - wmax * wmax;
		smax = std::max(room, 0.1 * wmax * wmax) / (3.5 * wmax);
		kroom = kmul * std::max((1.9 * fe) * (1.9 * fe) - wmax * wmax, 0.05 * fe * fe);
		for (int cc = 0; cc < C; cc++) {
			double b2 = 0, lm = 0;
			for (int j = 0; j < N; j++) {
				P[cc][j] = A[cc][j] * w[j] * on[j];
				Lw[cc][j] = Lm[cc][j] * w[j] * on[j];
				Gs[cc][j] = Sg[cc][j] * sw[j] * on[j];
				b2 += Gs[cc][j] * Gs[cc][j]; lm = std::max(lm, Lw[cc][j]);
			}
			gs2[cc] = b2; lwMax[cc] = lm;
		}
	}

	// One engine sample. `in` is the audio input (already scaled by the caller).
	void process(double in) {
		if (blk == 0) { thermal(); coefs(); }
		if (++blk >= BLK) blk = 0;

		const bool active = !allowIdle || c.drive > 0.0 || strikePending || std::fabs(in) > 1e-12 || energy > 1e-7 || pinged;
		pinged = false;
		if (!active) {
			if (!idle) {
				std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp);
				std::memset(Sprev, 0, sizeof Sprev);
				svl = svb = 0;
				idle = true;
			}
			outL = outR = 0; energy = 0; ekin = 0; pr = 1; emean = 0;
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
			humPh += 50.0 * T1; if (humPh >= 1) humPh -= 1;
			double a = 2 * PI * humPh;
			y += c.hum * (std::sin(a) + 0.5 * std::sin(2 * a) + 0.3 * std::sin(3 * a));
		}
		y += in;

		// ---- focus (Chamberlin state variable)
		double g = 2 * std::sin(PI * std::min(c.fc, std::min(20000.0, 0.2 * fe)) / fe), Q = 1.0 + 8 * c.focus;
		svl += g * svb; double hp = y - svl - svb / Q; svb += g * hp; double bp = svb / Q;
		double yf = (1 - c.focus) * y + c.focus * bp * 2.0;

		// ---- normalise, amplifier, force
		double x = c.gain * yf / std::max(c.drive, 1.0);
		{ double ga = 1 + 29 * c.amp * c.amp, b = 0.45 * c.amp + c.ampBias; x = std::tanh(ga * x + b) - std::tanh(b); }
		double F = c.drive * x + c.noise * rng.bi();

		if (strikePending) {
			for (int j = 0; j < N; j++) qp[j] -= strikeV * sX[j] * T1 * std::exp(-f0[j] / c.mallet) * on[j];
			// a hard strike seeds freezing in supercooled liquid
			int k = (int)(c.strikeX * K); k = std::max(0, std::min(K - 1, k));
			if (strikeV > 250 && liq[k] && h[k] < 1.0 + zMelt[k] + c.latent) liq[k] = 0;
			strikePending = false;
		}

		// ---- stretching, in the loosened coordinate lam*q (force divided by lam again)
		const double lm = lam, cv = curveEff;
		double S[C], Gq[C], ktot = 0, pot = 0;
		for (int cc = 0; cc < C; cc++) {
			const double* Pc = P[cc]; const double* Lc = Lw[cc]; const double* Gc = Gs[cc];
			double s = 0, s2 = 0, gq = 0, saa = 0, sab = 0;
			for (int j = 0; j < N; j++) {
				double qs = lm * q[j];
				s += Pc[j] * qs; s2 += Lc[j] * qs * qs; gq += Gc[j] * qs;
				double a = cv * Pc[j] + Lc[j] * qs; saa += a * a; sab += a * Gc[j];
			}
			Gq[cc] = c.eta * c.eta * gq;
			double g2 = saa + 2 * Gq[cc] * sab + Gq[cc] * Gq[cc] * gs2[cc];
			double Sr = cv * s + 0.5 * s2 + 0.5 * Gq[cc] * gq;
			pot += Sr * Sr;
			double sd = (Sr - Sprev[cc]) * fe; Sprev[cc] = Sr;
			double Sv = Sr + c.nu * sd;
			{ double sb = smax / std::max(c.bloom, 0.05); Sv = sb * std::tanh(Sv / sb); }
			S[cc] = Sv;
			ktot += c.bloom * (g2 + std::fabs(Sv) * (lwMax[cc] + c.eta * c.eta * gs2[cc]));
		}
		double kscale = ktot > kroom ? kroom / ktot : 1.0;
		if (kscale < 1.0) kclipN++;
		double fa[N], fl[N];
		for (int j = 0; j < N; j++) { fa[j] = 0; fl[j] = 0; }
		for (int cc = 0; cc < C; cc++) {
			const double sk = S[cc] * kscale / lm;
			const double sa = sk * cv, sg = sk * Gq[cc], sl = sk * lm;
			const double* Pc = P[cc]; const double* Gc = Gs[cc]; const double* Lc = Lw[cc];
			for (int j = 0; j < N; j++) { fa[j] += sa * Pc[j] + sg * Gc[j]; fl[j] += sl * Lc[j]; }
		}

		// ---- modes
		double L = 0, R = 0, E = 0, Ek = 0;
		const double bl = c.bloom;
		for (int j = 0; j < N; j++) {
			double fn = fa[j] + fl[j] * q[j];
			double qn = a1[j] * q[j] - a2[j] * qp[j] + bb[j] * (dW[j] * F - bl * fn);
			double v = (qn - q[j]) * fe;
			double ek = 0.5 * v * v, e = ek + 0.5 * w[j] * w[j] * qn * qn;
			E += e; Ek += ek; Eacc[j] += e;
			L += oLw[j] * v; R += oRw[j] * v;
			qp[j] = q[j]; q[j] = qn;
		}
		// A bent-in (buckled) rest shape holds energy without moving; with the drive off, let it relax.
		if (c.drive <= 0 && Ek < 1e-3 * E) {
			for (int j = 0; j < N; j++) { double m = 0.5 * (q[j] + qp[j]); q[j] -= 2e-4 * m; qp[j] -= 2e-4 * m; }
		}
		// Energy ceiling as Plate Driver: soft above the cap, a one-step clamp past 10x, reset only on NaN.
		{
			double capNow = guardE * std::max(1.0, (c.drive / 90000.0) * (c.drive / 90000.0)) * std::max(1.0, c.tune * c.tune);
			capHold = std::max(capNow, capHold * (1 - T1 / 1.5));
			double cap = capHold;
			if (!std::isfinite(E)) {
				guards++;
				std::memset(q, 0, sizeof q); std::memset(qp, 0, sizeof qp);
				std::memset(Sprev, 0, sizeof Sprev);
				E = 0; Ek = 0; L = R = 0;
			} else if (E > 10 * cap) {
				clampN++;
				double rr = std::sqrt(cap / E);
				for (int j = 0; j < N; j++) { q[j] *= rr; qp[j] *= rr; }
				for (int cc = 0; cc < C; cc++) Sprev[cc] *= rr;
				E = cap;
			} else if (E > cap) {
				double rr = 1.0 - 0.05 * (1.0 - cap / E);
				for (int j = 0; j < N; j++) { q[j] *= rr; qp[j] *= rr; }
				softN++;
			}
		}
		energy = E; ekin = Ek;
		epot = 0.5 * c.bloom * pot / (lm * lm);
		{ double gl = 1 + 0.85 * c.pick * std::tanh(uL * 0.6), gr = 1 + 0.85 * c.pick * std::tanh(uR * 0.6); L /= gl * gl; R /= gr * gr; }
		{ double go = 1 + 14 * c.out * c.out, rr = 2500; double sg = 1.0 / std::sqrt(go); L = rr * std::tanh(go * L / rr) * sg; R = rr * std::tanh(go * R / rr) * sg; }
		if (!std::isfinite(L) || !std::isfinite(R)) { L = R = 0; }
		outL = L; outR = R;

		if (++meterN >= meterLen) {
			double s = 0, s2 = 0, mx = 0; int mj = 0;
			for (int j = 0; j < N; j++) { s += Eacc[j]; s2 += Eacc[j] * Eacc[j]; if (Eacc[j] > mx) { mx = Eacc[j]; mj = j; } Eacc[j] = 0; }
			pr = s * s / (s2 + 1e-30);
			emean = s / meterN;
			top = mj;
			meterN = 0;
		}
	}
};

// Half-band decimator (2x -> 1x), 31-tap windowed sinc.
struct Decimator {
	static const int NT = 31;
	double hh[NT];
	double buf[2][64] = {};
	int pos = 0;
	Decimator() {
		const int Mm = NT - 1;
		double sum = 0;
		for (int i = 0; i < NT; i++) {
			double n = i - Mm / 2.0;
			double s = n == 0 ? 0.5 : std::sin(PI * 0.5 * n) / (PI * n);
			double wv = 0.42 - 0.5 * std::cos(2 * PI * i / Mm) + 0.08 * std::cos(4 * PI * i / Mm);
			hh[i] = s * wv; sum += hh[i];
		}
		for (int i = 0; i < NT; i++) hh[i] /= sum;
	}
	void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
	void process(const double* l2, const double* r2, double& L, double& R) {
		for (int k = 0; k < 2; k++) {
			buf[0][pos] = buf[0][pos + 32] = l2[k];
			buf[1][pos] = buf[1][pos + 32] = r2[k];
			pos = (pos + 1) & 31;
		}
		double sl = 0, sr = 0;
		for (int i = 0; i < NT; i++) { int idx = pos + 32 - 1 - i; sl += hh[i] * buf[0][idx]; sr += hh[i] * buf[1][idx]; }
		L = sl; R = sr;
	}
};

// Slow 2:1 level rider (what the approved sketches went through).
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

} // namespace molten
