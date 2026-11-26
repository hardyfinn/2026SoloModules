// Rider's kit: the real-time form of tools/proto/rider_kit_ref.py (the definition). Fixed 48 kHz.
// Every voice is partials (decaying sine modes, here rotating phasors), a noise wash in 26 bands whose levels are
// energy stores that strokes top up, and a stick click (two very fast stores). The kit works in 16-sample ticks;
// before making the 16 samples of block k it runs tick k + 1, so it can run the band gains linearly across the block.
// Its output is the reference's sample for sample (test/render_rider_kit.cpp). No Rack includes.
#pragma once
#include "Engine.hpp"
#include "RiderKitData.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

namespace riderkit {

using comper::Rng;
static const int SR = 48000;
static const int CTRL = 16;
enum Fam { CYM, HAT, DRUM };
enum KVoice { RIDE, BELL, CRASH, HATF, HATC, HATO, SNARE, STICK, KICK, TOM_HI, TOM_LO, BR_TAP, BR_SWEEP, BR_HATF, SNARE_EDGE };
static const int NS = NRATES + NCLICK + NRATES * NTAUS * 2;   // 58 stores per band
static inline int swellIndex(int ri, int ti) { return NRATES + NCLICK + (ri * NTAUS + ti) * 2; }

struct KitParams {
	double level, tune, decay, bright, width, age;
	double mix[5];   // RIDE HATS SNARE KICK TOMS
	KitParams() : level(1.0), tune(0.5), decay(0.5), bright(0.5), width(1.0), age(0.0) { for (int i = 0; i < 5; i++) mix[i] = 1.0; }
};

static inline double velDb(int v) { return 32.0 * std::log10(std::max(v, 1) / 96.0); }

// one stroke's worth of partials on one voice: phasors that ring on; later strokes with the same tuning add in
struct PartSet {
	bool used;
	int voice;
	double chL, chR;
	std::vector<double> f, rate;          // the tuning this set rings at (frequency, dB per second after DECAY/AGE)
	std::vector<double> rr, ri;           // per-sample rotation (complex)
	std::vector<double> zr, zi;           // settled
	std::vector<double> nr, ni;           // the stroke still fading in
	std::vector<int> kr;                  // its fade-in length per partial
	int rampPos; bool ramping;
	long long lastStart;
	PartSet() : used(false), voice(-1), chL(0), chR(0), rampPos(0), ramping(false), lastStart(0) {}
};

struct PendingPart {          // applied at a given sample: start a stroke's partials, or choke the open hat
	long long at; int kind;   // 0 start, 1 choke
	int voice; double chL, chR;
	std::vector<double> f, rate, g, ph;
};

struct Kit {
	KitParams kp;
	int seed;
	double store[3][2][NB][NS];
	double dec[3][NS];
	double decFor[2];                    // the DECAY and AGE that dec was made for
	// wash
	Rng noise[2];
	double tapBuf[2][2048]; int tapPos;
	double bz[2][NB][2][2];              // biquad states: channel, band, section, (z0 z1)
	double gPrev[2][NB], gNext[2][NB];
	// sweeps
	struct Sweep { long long a, m; int vel; };
	std::vector<Sweep> sweeps;
	double sweepDrift; Rng rd;
	// strokes waiting for their tick (tick, voice, velocity), in the order given
	struct Q { long long tick; int voice, vel; };
	std::deque<Q> queue;
	long long strokeIdx;
	// partials
	std::vector<PartSet> sets;
	std::vector<PendingPart> pending;
	// time
	long long sample;                    // reference sample about to be made
	long long tickRun;                   // the last tick that has been run
	bool started;

	Kit() { init(1); }

	void init(int seed_) {
		seed = seed_;
		for (int f = 0; f < 3; f++) for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) for (int k = 0; k < NS; k++) store[f][c][i][k] = 0.0;
		decFor[0] = decFor[1] = -1.0; makeDec();
		noise[0] = Rng((int64_t)seed * 2 + 11); noise[1] = Rng((int64_t)seed * 2 + 12);
		for (int c = 0; c < 2; c++) for (int j = 0; j < 2048; j++) tapBuf[c][j] = 0.0;
		tapPos = 0;
		for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) for (int s = 0; s < 2; s++) bz[c][i][s][0] = bz[c][i][s][1] = 0.0;
		for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) gPrev[c][i] = gNext[c][i] = 0.0;
		sweeps.clear(); sweepDrift = 0.0; rd = Rng((int64_t)seed * 7 + 3);
		queue.clear(); strokeIdx = 0;
		sets.clear(); sets.resize(48); pending.clear();
		sample = 0; tickRun = -1; started = false;
	}

	// ---- what the knobs do
	double rateMult(int fam) const {
		if (fam == DRUM) return 1.0 + 0.8 * kp.age;
		return std::pow(2.0, (0.5 - kp.decay) * 2.0) * (1.0 + 0.6 * kp.age);
	}
	double tiltDb(int i) const {
		double fc = std::sqrt(EDGES[i] * EDGES[i + 1]);
		return std::max(-9.0, std::min(9.0, ((kp.bright - 0.5) * 12.0 - 6.0 * kp.age) * std::log2(fc / 1000.0) / 4.0));
	}
	void makeDec() {
		if (decFor[0] == kp.decay && decFor[1] == kp.age) return;
		decFor[0] = kp.decay; decFor[1] = kp.age;
		const double dt = (double)CTRL / SR;
		for (int f = 0; f < 3; f++) {
			double m = rateMult(f);
			for (int k = 0; k < NRATES; k++) dec[f][k] = std::pow(10.0, -RATES[k] * m * dt / 10.0);
			for (int k = 0; k < NCLICK; k++) dec[f][NRATES + k] = std::pow(10.0, -CLICK[k] * dt / 10.0);
			for (int ri = 0; ri < NRATES; ri++)
				for (int ti = 0; ti < NTAUS; ti++) {
					double kk = 10.0 / std::log(10.0) / TAUS[ti]; int j = swellIndex(ri, ti);
					dec[f][j] = std::pow(10.0, -(RATES[ri] * m + kk) * dt / 10.0);
					dec[f][j + 1] = std::pow(10.0, -(RATES[ri] * m + 2 * kk) * dt / 10.0);
				}
		}
	}
	void curve(const Voice& d, const double (*T)[NB], double level, double* out) const {
		const double* x = d.lx; int n = d.nl;
		if (level <= x[0]) { for (int i = 0; i < NB; i++) out[i] = T[0][i] + (T[1][i] - T[0][i]) / (x[1] - x[0]) * (level - x[0]); return; }
		if (level >= x[n - 1]) { for (int i = 0; i < NB; i++) out[i] = T[n - 1][i] + (T[n - 1][i] - T[n - 2][i]) / (x[n - 1] - x[n - 2]) * (level - x[n - 1]); return; }
		int k = 0;
		while (level > x[k + 1]) k++;
		double u = (level - x[k]) / (x[k + 1] - x[k]);
		for (int i = 0; i < NB; i++) out[i] = T[k][i] + (T[k + 1][i] - T[k][i]) * u;
	}

	// ---- input
	// a stroke at a given tick (the test gives ceil(t * 48000 / 16)); a tick already run becomes the next one
	void strokeAtTick(long long tick, int voice, int vel) {
		if (tick <= tickRun) tick = tickRun + 1;
		Q q; q.tick = tick; q.voice = voice; q.vel = vel;
		auto it = queue.end();
		while (it != queue.begin() && (it - 1)->tick > tick) --it;    // stable: after every stroke with the same tick
		queue.insert(it, q);
	}
	void strokeNow(int voice, int vel) { strokeAtTick(tickRun + 1, voice, vel); }
	void sweepAt(long long a, long long m, int vel) { Sweep s; s.a = a; s.m = std::max(1LL, m); s.vel = vel; sweeps.push_back(s); }
	void sweepNow(double durS, int vel) { sweepAt((tickRun + 1) * CTRL, (long long)std::nearbyint(durS * 1.08 * SR), vel); }

	// ---- one tick: strokes, band gains, decay
	void applyStroke(long long tick, int voice, int vel) {
		Rng rs((int64_t)seed * 7919 + 31 * strokeIdx + 5); strokeIdx++;
		if (voice == SNARE && vel < 42) voice = SNARE_EDGE;
		const Voice& d = VOICES[voice]; int fam = d.family;
		const double* x = d.lx;
		double db = std::max(std::min(x[0], -12.0) - 6.0, std::min(std::max(x[d.nl - 1], 4.0) + 3.0, velDb(vel)));
		db += rs.g() * 1.5 * kp.age;
		double tl[NB], cl[NB]; curve(d, d.ltail, db, tl); curve(d, d.lclick, db, cl);
		double lin = kp.level * kp.mix[d.mix]; double pan = d.pan * kp.width;
		double ch[2] = {std::sqrt(0.5 * (1 - pan)), std::sqrt(0.5 * (1 + pan))};
		long long at = tick * CTRL;
		if (d.closes) {
			for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) for (int k = 0; k < NS; k++) store[HAT][c][i][k] *= 0.03;
			PendingPart pp; pp.at = at; pp.kind = 1; pp.voice = HATO; pp.chL = pp.chR = 0; pending.push_back(pp);
		}
		double tilt[NB]; for (int i = 0; i < NB; i++) tilt[i] = tiltDb(i);
		// wash
		int wi = 0;
		for (int i = 0; i < NB; i++) {
			double w = lin * lin * d.gain[i] * d.gain[i] * std::pow(10.0, (tl[i] + tilt[i]) / 10.0) / BANDPOWER[i];
			for (; wi < d.nwash && d.wash[wi].band == i; wi++) {
				const WashC& wc = d.wash[wi]; double e = wc.c * w;
				for (int c2 = 0; c2 < 2; c2++) {
					double ec = e * ch[c2] * ch[c2];
					store[fam][c2][i][wc.rate] += ec;
					if (wc.tau >= 0) { int j = swellIndex(wc.rate, wc.tau); store[fam][c2][i][j] -= 2 * ec; store[fam][c2][i][j + 1] += ec; }
				}
			}
			if (EDGES[i + 1] > 180) {
				double w2 = lin * lin * d.cgain[i] * d.cgain[i] * std::pow(10.0, (cl[i] + tilt[i]) / 10.0) / BANDPOWER[i];
				for (int ci = 0; ci < NCLICK; ci++)
					for (int c2 = 0; c2 < 2; c2++) store[fam][c2][i][NRATES + ci] += d.click[i][ci] * w2 * ch[c2] * ch[c2];
			}
		}
		// partials
		if (d.npart > 0) {
			PendingPart pp; pp.at = at; pp.kind = 0; pp.voice = voice; pp.chL = ch[0]; pp.chR = ch[1];
			double semis = d.tuned ? ((kp.tune - 0.5) * 10.0 - 0.7 * kp.age) : 0.0;
			double rm = rateMult(fam);
			for (int q = 0; q < d.npart; q++) {
				const Partial& P = d.part[q];
				double ph = rs.u() * 2 * M_PI; double vary = rs.g() * 1.5;
				double cents = d.tuned ? rs.g() * 12.0 * kp.age : 0.0;
				double f = P.f * std::pow(2.0, semis / 12.0 + cents / 1200.0);
				int bi = (int)(std::lower_bound(EDGES, EDGES + NB + 1, P.f) - EDGES) - 1; bi = std::max(0, std::min(NB - 1, bi));
				double g = lin * P.a * std::pow(10.0, (tl[bi] + vary) / 20.0);
				pp.f.push_back(f); pp.rate.push_back(P.r * rm); pp.g.push_back(g); pp.ph.push_back(ph);
			}
			pending.push_back(pp);
		}
	}

	void runTick(long long k) {
		makeDec();
		while (!queue.empty() && queue.front().tick <= k) { Q q = queue.front(); queue.pop_front(); applyStroke(k, q.voice, q.vel); }
		double tot[2][NB];
		for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) {
			double s = 0.0;
			for (int f = 0; f < 3; f++) { const double* st = store[f][c][i]; for (int j = 0; j < NS; j++) s += st[j]; }
			tot[c][i] = s;
		}
		// the sweeping hand
		double sw = 0.0;
		for (size_t q = 0; q < sweeps.size(); q++) {
			const Sweep& s = sweeps[q];
			long long k0 = (long long)std::ceil(s.a / (double)CTRL), k1 = (s.a + s.m) / CTRL;
			if (k < k0 || k > k1) continue;
			double u = (k * CTRL - s.a) / (double)s.m;
			if (u < 0 || u > 1) continue;
			double sn = std::sin(M_PI * std::pow(u, 0.7));
			double g = (0.42 + 0.58 * sn * sn) * std::min(1.0, std::min(u, 1 - u) / 0.06) * std::pow(10.0, (SWEEP_LX[NSWEEP - 1] - 3 + 0.5 * velDb(s.vel)) / 20.0);
			sw = std::max(sw, g);
		}
		for (size_t q = 0; q < sweeps.size();) { if ((sweeps[q].a + sweeps[q].m) / CTRL < k) sweeps.erase(sweeps.begin() + q); else q++; }
		if (sw > 0) {
			sweepDrift = 0.995 * sweepDrift + rd.g() * 0.12;
			double lv = 20 * std::log10(sw) + sweepDrift;
			double inside = std::max(SWEEP_LX[0], std::min(SWEEP_LX[NSWEEP - 1], lv));
			int j = 0; while (j < NSWEEP - 2 && inside >= SWEEP_LX[j + 1]) j++;
			const Voice& v = VOICES[BR_SWEEP];
			double pan = v.pan * kp.width, lin = kp.level * kp.mix[v.mix];
			for (int i = 0; i < NB; i++) {
				if (EDGES[i + 1] <= 130) continue;
				double fp;
				if (inside >= SWEEP_LX[NSWEEP - 1]) fp = SWEEP_BANDS[NSWEEP - 1][i];
				else fp = (SWEEP_BANDS[j + 1][i] - SWEEP_BANDS[j][i]) / (SWEEP_LX[j + 1] - SWEEP_LX[j]) * (inside - SWEEP_LX[j]) + SWEEP_BANDS[j][i];
				double dbb = fp + (lv - inside) + tiltDb(i);
				double pw = lin * lin * std::pow(10.0, dbb / 10.0) / BANDPOWER[i];
				tot[0][i] += pw * 0.5 * (1 - pan); tot[1][i] += pw * 0.5 * (1 + pan);
			}
		}
		for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) gNext[c][i] = std::sqrt(std::max(tot[c][i], 0.0));
		for (int f = 0; f < 3; f++) for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) { double* st = store[f][c][i]; for (int j = 0; j < NS; j++) st[j] *= dec[f][j]; }
		tickRun = k;
	}

	// ---- partials
	static bool sameTuning(const PartSet& s, const PendingPart& p) {
		if (s.chL != p.chL || s.chR != p.chR || s.f.size() != p.f.size()) return false;
		for (size_t q = 0; q < p.f.size(); q++) if (s.f[q] != p.f[q] || s.rate[q] != p.rate[q]) return false;
		return true;
	}
	static double level(const PartSet& s) {
		double m = 0;
		for (size_t q = 0; q < s.zr.size(); q++) m = std::max(m, std::max(std::fabs(s.zr[q]) + std::fabs(s.zi[q]), std::fabs(s.nr[q]) + std::fabs(s.ni[q])));
		return m;
	}
	void settle(PartSet& s) {
		// fold the stroke that is fading in into the settled part, at its present fade
		for (size_t q = 0; q < s.f.size(); q++) {
			double w = s.rampPos < s.kr[q] ? 0.5 - 0.5 * std::cos(M_PI * s.rampPos / s.kr[q]) : 1.0;
			s.zr[q] += w * s.nr[q]; s.zi[q] += w * s.ni[q]; s.nr[q] = s.ni[q] = 0.0;
		}
		s.ramping = false;
	}
	void applyPending(const PendingPart& p) {
		if (p.kind == 1) {
			for (auto& s : sets) if (s.used && s.voice == p.voice)
				for (size_t q = 0; q < s.f.size(); q++) { s.zr[q] *= 0.1; s.zi[q] *= 0.1; s.nr[q] *= 0.1; s.ni[q] *= 0.1; }
			return;
		}
		PartSet* t = nullptr;
		// the voice's newest set, if it rings at the same tuning and is not fading a stroke in
		PartSet* newest = nullptr;
		for (auto& s : sets) if (s.used && s.voice == p.voice && (!newest || s.lastStart > newest->lastStart)) newest = &s;
		if (newest && sameTuning(*newest, p)) {
			t = newest;
			if (t->ramping) settle(*t);
		}
		if (!t) for (auto& s : sets) if (!s.used) { t = &s; break; }
		if (!t) {   // all in use: take the quietest
			double best = 1e30;
			for (auto& s : sets) { double l = level(s); if (l < best) { best = l; t = &s; } }
		}
		PartSet& s = *t;
		size_t n = p.f.size();
		if (!s.used || !sameTuning(s, p)) {
			s.used = true; s.voice = p.voice; s.chL = p.chL; s.chR = p.chR; s.f = p.f; s.rate = p.rate;
			s.rr.resize(n); s.ri.resize(n); s.zr.assign(n, 0.0); s.zi.assign(n, 0.0); s.nr.assign(n, 0.0); s.ni.assign(n, 0.0); s.kr.resize(n);
			for (size_t q = 0; q < n; q++) {
				double lam = p.rate[q] * std::log(10.0) / 20.0;
				double a = std::exp(-lam / SR), w = 2 * M_PI * p.f[q] / SR;
				s.rr[q] = a * std::cos(w); s.ri[q] = a * std::sin(w);
				s.kr[q] = std::max(1, (int)(std::min(0.006, 1.0 / p.f[q]) * SR));
			}
		}
		for (size_t q = 0; q < n; q++) { s.nr[q] = p.g[q] * std::cos(p.ph[q]); s.ni[q] = p.g[q] * std::sin(p.ph[q]); }   // g e^(i ph); Im() is the output
		s.rampPos = 0; s.ramping = true; s.lastStart = p.at;
	}

	// ---- one reference sample, stereo
	void process(double& outL, double& outR) {
		if (!started) { runTick(0); for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) gPrev[c][i] = gNext[c][i]; started = true; }
		int j = (int)(sample % CTRL);
		if (j == 0) {
			if (sample > 0) for (int c = 0; c < 2; c++) for (int i = 0; i < NB; i++) gPrev[c][i] = gNext[c][i];
			runTick(sample / CTRL + 1);
			// partials of strokes that start in this block's first sample
			std::vector<PendingPart> later;
			for (auto& p : pending) { if (p.at <= sample) applyPending(p); else later.push_back(p); }
			pending.swap(later);
			if (sample % (CTRL * 64) == 0)
				for (auto& s : sets) if (s.used && !s.ramping && level(s) < 1e-9) s.used = false;
		}
		// wash
		double fr = j / (double)CTRL;
		double out[2] = {0.0, 0.0};
		for (int c = 0; c < 2; c++) {
			double x = (noise[c].u() - 0.5) * std::sqrt(12.0);
			tapBuf[c][tapPos] = x;
			double col = 0.0;
			for (int t = 0; t < NTAPS; t++) col += TAP_GAIN[t] * tapBuf[c][(tapPos - TAP_DELAY[t]) & 2047];
			double acc = 0.0;
			for (int i = 0; i < NB; i++) {
				double y = col;
				for (int s = 0; s < 2; s++) {
					const double* q = SOS[i][s]; double* z = bz[c][i][s];
					double o = q[0] * y + z[0];
					z[0] = q[1] * y - q[3] * o + z[1];
					z[1] = q[2] * y - q[4] * o;
					y = o;
				}
				acc += y * (gPrev[c][i] + (gNext[c][i] - gPrev[c][i]) * fr);
			}
			out[c] = acc;
		}
		tapPos = (tapPos + 1) & 2047;
		// partials
		for (auto& s : sets) {
			if (!s.used) continue;
			double sum = 0.0; size_t n = s.f.size();
			if (s.ramping) {
				bool any = false;
				for (size_t q = 0; q < n; q++) {
					double w;
					if (s.rampPos < s.kr[q]) { w = 0.5 - 0.5 * std::cos(M_PI * s.rampPos / s.kr[q]); any = true; }
					else w = 1.0;
					sum += s.zi[q] + w * s.ni[q];
					double a = s.rr[q], b = s.ri[q];
					double zr = s.zr[q] * a - s.zi[q] * b; s.zi[q] = s.zr[q] * b + s.zi[q] * a; s.zr[q] = zr;
					double nr = s.nr[q] * a - s.ni[q] * b; s.ni[q] = s.nr[q] * b + s.ni[q] * a; s.nr[q] = nr;
				}
				s.rampPos++;
				if (!any) { for (size_t q = 0; q < n; q++) { s.zr[q] += s.nr[q]; s.zi[q] += s.ni[q]; s.nr[q] = s.ni[q] = 0.0; } s.ramping = false; }
			} else {
				for (size_t q = 0; q < n; q++) {
					sum += s.zi[q];
					double a = s.rr[q], b = s.ri[q];
					double zr = s.zr[q] * a - s.zi[q] * b; s.zi[q] = s.zr[q] * b + s.zi[q] * a; s.zr[q] = zr;
				}
			}
			out[0] += sum * s.chL; out[1] += sum * s.chR;
		}
		outL = out[0]; outR = out[1];
		sample++;
	}
};

} // namespace riderkit
