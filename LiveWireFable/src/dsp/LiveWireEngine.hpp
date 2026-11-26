// Live Wire engine: one electrical circuit (source, ballast + choke, spark gap with arc, saturating L-C tank,
// corona), simulated at OS x the host rate. A literal port of tools/proto/livewire_ref.c (the definition of the
// sound): same state, same order of operations, same RNG and the same number of draws per sample, so the offline
// test can compare the two sample for sample. See docs/BUILD-INSTRUCTIONS.md section 3. No Rack code here.
#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
#include "Mapping.hpp"

namespace livewire {

static const int MAXOS = 16;
static const int MAXNT = MAXOS * 24;
static const double PI_D = 3.14159265358979323846;

// Panel state. The 28 continuous controls are knob values 0..1 (after CV), smoothed inside the engine.
struct K {
	double volts = 0, mains = 0, freq = 0, current = 0, choke = 0;
	double gap = 0, heat = 0, cool = 0, quench = 0, memory = 0, restrike = 0, jitter = 0;
	double wind = 0, climb = 0;
	double tune = 0, ring = 0, load = 0, iron = 0;
	double corona = 0, hum = 0, fb = 0, dist = 0, age = 0;
	double arc = 0, field = 0, out = 0, draw = 0, in = 0;
	int track = 0;                // COOL follows TUNE
	double voct = 0, voctMains = 0;   // octaves
	int trig = 0, surge = 0;      // one-sample pulses
	int key = 0, keyPatched = 0;  // tank key gate
	int touch = 0;                // touch gate
};
static const int NKNOB = 28;
static double K::* const KNOBS[NKNOB] = {
	&K::volts, &K::mains, &K::freq, &K::current, &K::choke, &K::gap, &K::heat, &K::cool, &K::quench, &K::memory,
	&K::restrike, &K::jitter, &K::wind, &K::climb, &K::tune, &K::ring, &K::load, &K::iron, &K::corona, &K::hum,
	&K::fb, &K::dist, &K::age, &K::arc, &K::field, &K::out, &K::draw, &K::in};

struct Dec { double b[MAXNT]; int w; };
struct Res { double y1, y2, a1, a2, b0; };

static const double HF[6] = {2, 4, 6, 417, 733, 1180};   // first three are multiples of the mains frequency, the rest case modes in Hz
static const double HQ[6] = {30, 25, 20, 18, 14, 10};
static const double HGL[6] = {1.0, 0.8, 0.5, 0.62, 0.26, 0.36}, HGR[6] = {1.0, 0.72, 0.6, 0.40, 0.44, 0.24};

struct Engine {
	double sr = 48000; int os = 16, nt = 384; double fir[MAXNT];
	unsigned long long rs = 1;
	K s;
	int first = 1;
	double is = 0, vg = 0, it = 0, vct = 0, g = 1e-7, h = 0, c = 0, jz = 0, path = 1; int on = 0;
	double tf = 0, ts = 0, ts2 = 0, ts3 = 0, ph = 0;
	double gapEff = 0, surgeEnv = 0; int keyPrev = 0;
	long lag = -1;
	double looseR = 0; long looseN = 0;
	Dec dP, dIs, dIt;
	double pprev = 0, fprev = 0, hp1 = 0, hp2 = 0, dcL = 0, dcR = 0;
	Res hr[2][6]; double humf = 0;
	std::vector<double> fbbuf; int fbn = 0, fbw = 0; double fbv = 0, fblp = 0;
	double pan = 0.5;
	long strikes = 0, guard = 0;
	double dbgOn = 0;
	double vbCold = 1;            // for the CHARGE light

	double frand() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (double)(rs >> 11) * (1.0 / 9007199254740992.0); }
	double nrand() { return (frand() + frand() + frand() + frand() - 2.0) * 1.7320508; }
	static void rset(Res* r, double f, double q, double sr) { if (f > 0.45 * sr) f = 0.45 * sr; double w = 2 * PI_D * f / sr, R = std::exp(-w / (2 * q)); r->a1 = 2 * R * std::cos(w); r->a2 = -R * R; r->b0 = (1 - R * R) * 0.5; }
	static inline double rproc(Res* r, double x) { double y = r->b0 * x + r->a1 * r->y1 + r->a2 * r->y2; r->y2 = r->y1; r->y1 = y; return y; }
	inline void dpush(Dec* d, double x) { d->b[d->w] = x; if (++d->w == nt) d->w = 0; }
	inline double dout(Dec* d) { double sum = 0; int k = d->w; for (int i = 0; i < nt; i++) { sum += fir[i] * d->b[k]; if (++k == nt) k = 0; } return sum; }

	void init(double rate, unsigned long long seed) {
		sr = rate; os = sr <= 50000 ? 16 : (sr <= 100000 ? 8 : 4); nt = os * 24;
		double fc = (0.396 * sr < 19000 ? 0.396 * sr : 19000) / (sr * os), sum = 0;   // cutoff 19 kHz (0.396 x host rate below 48 k)
		for (int i = 0; i < nt; i++) { double x = i - (nt - 1) / 2.0; double w = 0.42 - 0.5 * std::cos(2 * PI_D * i / (nt - 1)) + 0.08 * std::cos(4 * PI_D * i / (nt - 1)); double hh = (x == 0) ? 2 * fc : std::sin(2 * PI_D * fc * x) / (PI_D * x); fir[i] = hh * w; sum += fir[i]; }
		for (int i = 0; i < nt; i++) fir[i] /= sum;
		rs = seed * 2654435761ULL + 88172645463325252ULL; for (int i = 0; i < 8; i++) frand();
		s = K(); first = 1;
		is = vg = it = vct = h = c = jz = 0; g = 1e-7; path = 1; on = 0;
		tf = ts = ts2 = ts3 = ph = 0; gapEff = surgeEnv = 0; keyPrev = 0; lag = -1; looseR = 0; looseN = 0;
		std::memset(&dP, 0, sizeof dP); std::memset(&dIs, 0, sizeof dIs); std::memset(&dIt, 0, sizeof dIt);
		pprev = fprev = hp1 = hp2 = dcL = dcR = 0; std::memset(hr, 0, sizeof hr); humf = 0;
		fbn = (int)(0.021 * sr) + 4; fbbuf.assign(fbn, 0.0); fbw = 0; fbv = fblp = 0;
		pan = 0.5; strikes = guard = 0; dbgOn = 0; vbCold = 1;
	}

	// One host sample. in: audio in, 1.0 = 5 V. Outputs are before the x5 V scaling.
	void process(const K& kk, double in, double* L, double* R, double* oArc, double* oField) {
		const K* k = &kk;
		int OS = os; double dt = 1.0 / (sr * OS);
		// ---- knob smoothing, 5 ms one-pole on the 0..1 values
		{ double a = first ? 1.0 : 1.0 - std::exp(-1.0 / (0.005 * sr)); for (int i = 0; i < NKNOB; i++) s.*KNOBS[i] += (kk.*KNOBS[i] - s.*KNOBS[i]) * a; }
		if (first) { gapEff = M_GAP(k->gap); first = 0; }
		const K* sp = &s; double age = sp->age;
		// ---- slow random processes (unit variance), once per host sample
		{ double a = 2 * PI_D * 900 / sr; if (a > 1) a = 1; tf += -tf * a + std::sqrt(a * (2 - a)) * nrand();
			a = 2 * PI_D * 4 / sr;    ts  += -ts  * a + std::sqrt(2 * a) * nrand();
			a = 2 * PI_D * 0.7 / sr;  ts2 += -ts2 * a + std::sqrt(2 * a) * nrand();
			a = 2 * PI_D * 0.15 / sr; ts3 += -ts3 * a + std::sqrt(2 * a) * nrand(); }
		// ---- mapping
		double E = M_VOLTS(sp->volts) * (1.0 + 0.05 * age * ts2);
		if (k->surge) surgeEnv = 1.0;
		surgeEnv *= std::exp(-1.0 / (0.06 * sr));
		E *= 1.0 + 2.0 * surgeEnv;
		double Edc = E * (1.0 - sp->mains), Eac = E * sp->mains;
		double fm = M_FREQ(sp->freq) * std::pow(2.0, k->voctMains) * (1.0 + 0.004 * age * ts3); fm = lw_clamp(fm, 5.0, 2000.0);
		double cur = M_CUR(sp->current) * (1.0 + surgeEnv), Rs = 1.0 / cur, Ls = Rs * M_CHOKE(sp->choke);
		double ft = lw_clamp(M_TUNE(sp->tune) * std::pow(2.0, k->voct) * (1.0 + 0.006 * age * ts3), 16.0, 0.4 * sr); if (ft > 18000) ft = 18000;
		double Zt = M_LOAD(sp->load), Qt = M_RING(sp->ring) * (1.0 - 0.5 * age);
		int keyOn = k->keyPatched ? k->key : 1; if (!keyOn) Qt = 1.2; if (Qt < 0.6) Qt = 0.6;
		if (k->keyPatched && k->key && !keyPrev) { vct = 0; it = 0; }   // a key swaps in a discharged capacitor
		keyPrev = k->key;
		double Lt = Zt / (2 * PI_D * ft), Ct = 1.0 / (2 * PI_D * ft * Zt), Rt = Zt / Qt;
		double iron = M_IRON(sp->iron); double ironMax = 0.5 / (2 * PI_D * ft * dt); ironMax = ironMax * ironMax; if (ironMax > 16) ironMax = 16;
		double fp = 17000.0 * (1.0 - 0.45 * age);
		double Lpar = Lt * Ls / (Lt + Ls), Cp = 1.0 / ((2 * PI_D * fp) * (2 * PI_D * fp) * Lpar), gp = 0.15 * std::sqrt(Cp / Lpar);
		double tau = M_COOL(sp->cool) * (k->track ? 261.6256 / ft : 1.0); if (tau < 4e-6) tau = 4e-6; if (tau > 0.02) tau = 0.02;
		double P0 = M_HEAT(sp->heat), goff = M_QUENCH(sp->quench), mem = M_MEM(sp->memory), restr = M_RESTR(sp->restrike);
		double jit = lw_clamp(M_JIT(sp->jitter) + 0.3 * age, 0, 0.9), wind = M_WIND(sp->wind) + 0.01 * age, climb = M_CLIMB(sp->climb);
		double corona = M_CORONA(sp->corona), fb = M_FB(sp->fb);
		// ---- touch and draw
		{ double gk = M_GAP(sp->gap); if (k->touch) gapEff += (0.012 - gapEff) * (1.0 - std::exp(-1.0 / (0.015 * sr)));
			else { gapEff += M_GAP(1.0) / (M_DRAW(sp->draw) * sr); } if (gapEff > gk) gapEff = gk; }
		double len = (gapEff + c) * path; double lt = len * std::exp(wind * (4.0 * ts + 0.97 * tf)); if (lt < 0.004) lt = 0.004;
		double Pw = P0 * lt, VbCold = lt * (1.0 + jit * jz); if (VbCold < 0.01) VbCold = 0.01;
		vbCold = VbCold;
		// ---- loose connection (AGE)
		if (looseN > 0) { if (--looseN == 0) looseR = 0; }
		else if (age > 0 && frand() < 12.0 * age * age / sr) { looseN = (long)((0.0002 - 0.002 * std::log(frand() + 1e-12)) * sr); looseR = Rs * (8.0 + 30.0 * frand()); }
		double Rtot = Rs + looseR;
		// ---- triggered gap: statistical lag and misfires scale with JITTER
		if (k->trig && !on) { double r = std::fabs(vg) / VbCold; if (r > 1) r = 1;
			if (frand() < 1.0 - 1.2 * jit * (1.0 - r)) lag = (long)(jit * (-std::log(frand() + 1e-12)) * 0.008 * (1.5 - r) / dt);
			else vg *= 0.85; }
		double corL = 0, corR = 0;
		if (lag > 0 && frand() < 900.0 * jit / sr) { double q = 0.12 * vg, pn = frand(); vg -= q; corL += 3.0 * q * std::sqrt(1 - pn); corR += 3.0 * q * std::sqrt(pn); }
		double sat = lw_clamp(0.1 + 0.45 * Eac, 0, 1) + 0.4 * age; if (sat > 1.4) sat = 1.4;
		for (int o = 0; o < OS; o++) {
			ph += fm * dt; if (ph >= 1) ph -= 1;
			double a = 2 * PI_D * ph, sn = std::sin(a), fl = -std::cos(a); double f3 = fl * fl * fl, f9 = f3 * f3 * f3;
			double ev = Edc + Eac * (sn - 0.25 * sat * f9 + age * (0.12 * std::sin(3 * a) + 0.07 * std::sin(5 * a + 1.0))) + fbv + M_IN(sp->in) * in;
			double Vb = VbCold * (1.0 - restr * h); if (Vb < 0.01) Vb = 0.01;
			if (corona > 0) { double av = std::fabs(vg), x = (av - 0.55 * lt) / (0.08 * lt); if (x > -6) { if (x > 3) x = 3; int neg = vg < 0; double rate = corona * (neg ? 9000.0 : 2500.0) * std::exp(x);
					if (frand() < rate * dt) { double q = (neg ? 0.04 : 0.16) * (0.4 + frand()) * av; vg -= (neg ? -1 : 1) * q; double cs = q * ((vg < 0) ? 1 : -1.6), pn = frand(); corL += cs * std::sqrt(1 - pn); corR += cs * std::sqrt(pn); } } }
			is = (is + dt / Ls * (ev - vg)) / (1.0 + dt * Rtot / Ls);
			{ double f = 1.0 + iron * it * it; if (f > ironMax) f = ironMax; it += dt / Lt * f * (vg - vct - Rt * it); } vct += dt / Ct * it;
			vg = (vg + dt / Cp * (is - it)) / (1.0 + dt * (g + gp) / Cp);
			double pw = vg * vg * g;
			if (!on) { int fire = std::fabs(vg) > Vb; if (lag >= 0) { if (lag == 0) fire = 1; lag--; }
				if (fire) { strikes++; on = 1; g = 0.4 + 0.4 * frand(); lag = -1; } }
			else { double sg = std::sqrt(g), tg = tau * sg; if (tg < 2 * dt) tg = 2 * dt; double ex = dt / tg * (pw / (Pw * sg) - 1.0); if (ex > 0.5) ex = 0.5; if (ex < -0.5) ex = -0.5;
				g *= std::exp(ex); if (g > 400) g = 400;
				if (g < goff) { on = 0; g = 1e-7; jz = nrand(); path = std::exp(0.6 * jit * nrand()); } }
			if (on) h += (1 - h) * dt / 0.002; else h -= h * dt / mem;
			if (h > 0.08) c += climb * h * dt; else c -= c * dt / 0.004;
			if (c > 12) c = 12;
			if (!(std::fabs(is) < 200)) { is = is > 0 ? 200 : -200; if (is != is) is = 0; guard++; }
			if (!(std::fabs(it) < 200)) { it = it > 0 ? 200 : -200; if (it != it) it = 0; guard++; }
			if (!(std::fabs(vg) < 200)) { vg = vg > 0 ? 200 : -200; if (vg != vg) vg = 0; guard++; }
			if (!(std::fabs(vct) < 200)) { vct = vct > 0 ? 200 : -200; if (vct != vct) vct = 0; guard++; }
			dpush(&dP, pw); dpush(&dIs, is); dpush(&dIt, it); dbgOn += on;
		}
		double pw = dout(&dP), isd = dout(&dIs), itd = dout(&dIt);
		double dp = (pw - pprev) * sr / 700.0; pprev = pw; hp1 += (pw - hp1) * (1.0 - std::exp(-2 * PI_D * 25.0 / sr)); double arc = dp + 0.1 * (pw - hp1);
		double fcur = isd + 0.6 * itd; double fld = (fcur - fprev) * sr / 2000.0; fprev = fcur; hp2 += (fcur - hp2) * (1.0 - std::exp(-2 * PI_D * 30.0 / sr)); fld += 0.15 * (fcur - hp2);
		if (fm != humf) { humf = fm; for (int ch = 0; ch < 2; ch++) for (int j = 0; j < 6; j++) rset(&hr[ch][j], j < 3 ? HF[j] * fm : HF[j], HQ[j], sr); }
		double hL = 0, hR = 0;
		{ double fl = -std::cos(2 * PI_D * ph), amp = Eac / 0.8; if (amp > 1) amp = 1; double sfl = std::tanh((1.0 + 2.5 * sat) * fl * amp);
			double drive = sfl * sfl + 0.4 * isd * isd; if (drive > 50) drive = 50; double th = 0.75 - 0.35 * age; double rat = drive > th ? (drive - th) * 3.0 * (0.5 + frand()) : 0;
			for (int j = 0; j < 6; j++) { hL += HGL[j] * rproc(&hr[0][j], drive + rat); hR += HGR[j] * rproc(&hr[1][j], drive + rat); } }
		double hum = M_HUM(sp->hum) * 0.6;
		{ double target = 0.5 + 0.22 * std::tanh(ts2) + 0.10 * std::tanh(ts); pan += (target - pan) * (1.0 - std::exp(-1.0 / (0.02 * sr))); }
		double gl = std::sqrt(2.0 * (1.0 - pan)), gr = std::sqrt(2.0 * pan);
		double al = M_ARC(sp->arc), flv = M_FIELD(sp->field);
		double yl = al * (arc * gl + 6.0 * corL * 1.4142) + flv * fld + hum * hL, yr = al * (arc * gr + 6.0 * corR * 1.4142) + flv * fld + hum * hR;
		{ double y = 0.5 * (yl + yr); fbbuf[fbw] = y; int d = (int)(M_DIST(sp->dist) * sr); if (d < 1) d = 1; if (d > fbn - 1) d = fbn - 1; int r = fbw - d; if (r < 0) r += fbn; if (++fbw == fbn) fbw = 0;
			fblp += (fbbuf[r] - fblp) * 0.35; fbv = fb * std::tanh(fblp * 0.5); }
		double og = M_OUT(sp->out) * 0.35; yl = 1.6 * std::tanh(og * yl / 1.6); yr = 1.6 * std::tanh(og * yr / 1.6);
		{ double a = 1.0 - std::exp(-2 * PI_D * 20.0 / sr); dcL += (yl - dcL) * a; dcR += (yr - dcR) * a; }
		*L = yl - dcL; *R = yr - dcR; *oArc = 1.6 * std::tanh(0.35 * arc / 1.6); *oField = 1.6 * std::tanh(0.35 * fld / 1.6);
	}

	// lights, 0..1
	double arcLight() const { return h; }
	double chargeLight() const { double v = std::fabs(vct) > std::fabs(vg) ? std::fabs(vct) : std::fabs(vg); v /= vbCold; return v > 1 ? 1 : v; }
};

}  // namespace livewire
