// Facet sound engine: up to 16 voices, each with four tones (A-D).
// Each tone: WG (wave + pitch) -> TVF (2-pole LPF/BPF/HPF) -> TVA (level), with a pitch envelope,
// a filter envelope, an amp envelope and two LFOs, laid out the way a JD-800 tone is.
// The four tones are summed, then run through the patch effects (FacetFx.hpp).
// No Rack headers here, so test/render.cpp can run it offline.
#pragma once
#include "FacetParams.hpp"
#include "FacetFx.hpp"

namespace facet {

static const int MAXV = 16;
static const int CTRL = 8; // control-rate block (samples)

inline float envSecs(float x) { return 0.001f * std::pow(30000.f, x * 0.01f); }
inline float lfoHz(float x) { return 0.05f * std::pow(600.f, x * 0.01f); }
inline float cutoffHz(float x) { return 20.f * std::pow(1000.f, x * 0.01f); }
inline float fastExp2(float x) { return std::exp2(x); }

inline float velCurve(int c, float v) {
	switch (c) {
		case 1: return v * v;
		case 2: return std::sqrt(v);
		case 3: return v * v * (3.f - 2.f * v);
		default: return v;
	}
}
// sensitivity s in -1..1: + = louder/bigger with velocity, - = the opposite
inline float velScale(float s, float cv) { return s >= 0.f ? 1.f - s * (1.f - cv) : 1.f + s * cv; }

// ---------------------------------------------------------------- envelope (JD style: times + levels)
struct Env {
	enum { DONE = 100, SUS = 50, REL = 60 };
	float v = 0.f;
	int stage = DONE;
	int nAtk = 0;
	float T[3] = {0, 0, 0}, L[3] = {0, 0, 0};
	float sus = 0.f, relT = 0.f, relL = 0.f;
	float inc = 0.f, tgt = 0.f;
	int left = 0;
	float rate = 48000.f; // ticks per second

	void segment(float secs, float target) {
		left = std::max(1, (int) (secs * rate));
		tgt = target;
		inc = (target - v) / left;
	}
	void start(float startV) {
		v = startV;
		stage = 0;
		if (nAtk > 0) segment(T[0], L[0]);
		else stage = SUS;
	}
	void release() {
		if (stage == DONE || stage == REL) return;
		stage = REL;
		segment(relT, relL);
	}
	float tick() {
		if (stage < nAtk || stage == REL) {
			v += inc;
			if (--left <= 0) {
				v = tgt;
				if (stage == REL) stage = DONE;
				else if (++stage < nAtk) segment(T[stage], L[stage]);
				else stage = SUS;
			}
		}
		else if (stage == SUS)
			v += (sus - v) * 0.02f; // follows the sustain slider live
		return v;
	}
};

// ---------------------------------------------------------------- LFO
struct Lfo {
	float ph = 0.f, delayLeft = 0.f, amp = 1.f, fadeInc = 0.f, sh = 0.f, ra = 0.f, rb = 0.f, out = 0.f;
	bool fadeOut = false;
	void start(float phase, float delaySecs, float fade, Rng& r) {
		ph = phase;
		delayLeft = delaySecs;
		float ft = 5.f * (fade / 50.f) * (fade / 50.f);
		if (fade > 0.5f) {
			amp = 0.f;
			fadeInc = 1.f / std::max(0.001f, ft);
			fadeOut = false;
		}
		else if (fade < -0.5f) {
			amp = 1.f;
			fadeInc = -1.f / std::max(0.001f, ft);
			fadeOut = true;
		}
		else {
			amp = 1.f;
			fadeInc = 0.f;
		}
		sh = r.bi();
		ra = r.bi();
		rb = r.bi();
	}
	float tick(float dt, float hz, int wave, int ofs, Rng& r) {
		if (delayLeft > 0.f) {
			delayLeft -= dt;
			out = 0.f;
			return out;
		}
		amp = std::max(0.f, std::min(1.f, amp + fadeInc * dt));
		ph += hz * dt;
		if (ph >= 1.f) {
			ph -= std::floor(ph);
			sh = r.bi();
			ra = rb;
			rb = r.bi();
		}
		float x;
		switch (wave) {
			case 0: x = ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph; break;
			case 1: x = 1.f - 2.f * ph; break;
			case 2: x = ph < 0.5f ? 1.f : -1.f; break;
			case 3: x = sh; break;
			default: {
				float s = ph * ph * (3.f - 2.f * ph);
				x = ra + (rb - ra) * s;
			}
		}
		if (ofs == 2) x = 0.5f * (x + 1.f);
		else if (ofs == 0) x = 0.5f * (x - 1.f);
		out = x * amp;
		return out;
	}
};

// ---------------------------------------------------------------- one tone of one voice
struct ToneV {
	bool active = false;
	int wave = 0;
	double phase = 0.0, spos = 0.0;
	float fpos = 0.f, fdir = 1.f;
	bool smpDone = false;
	float s1 = 0.f, s2 = 0.f; // filter state
	Env pe, fe, ae;
	Lfo lfo[2];
	float rnd = 0.f, velAmp = 1.f, feVel = 1.f, peVel = 1.f, bias = 1.f;
	// control-rate results
	float inc = 0.f, incT = 0.f, scan = 0.f;
	int m0 = 0, m1 = 0;
	float mf = 0.f;
	float g = 0.1f, k = 2.f;
	int fmode = 0;
	float amp = 0.f, ampT = 0.f, panL = 0.707f, panR = 0.707f;
	float lastX = 0.f, dck = 0.f; // sample waves: last raw sample, and the retrigger de-click offset
};

struct Voice {
	bool gate = false;
	float note = 60.f, cur = 60.f, vel = 0.8f, press = 0.f;
	bool glide = false;
	ToneV t[4];
	bool sounding() const { return t[0].active || t[1].active || t[2].active || t[3].active; }
};

struct In {
	int channels = 0;
	float voct[MAXV] = {}, gate[MAXV] = {}, vel[MAXV] = {}, press[MAXV] = {};
	bool velConnected = false;
	float mod = 0.f, bend = 0.f; // mod 0..1, bend -1..1
	int solo = -1;               // >=0: only this tone sounds (SOLO TONE monitor)
};

struct Engine {
	float sr = 48000.f, isr = 1.f / 48000.f;
	float v[NPV];
	Voice voices[MAXV];
	bool prevGate[MAXV] = {};
	float prevVoct[MAXV] = {};
	Wave user[NUM_USER];
	float freePh[4][2] = {};
	Fx fx;
	Rng rng{12345};
	int ctr = 0;
	int noteOns = 0;
	float outL = 0.f, outR = 0.f;
	float glideK = 1.f;
	float dckK = 0.993f; // decay per sample of the retrigger de-click offset (about 3 ms)
	// 90s converter
	float dacAcc = 0.f, dacL = 0.f, dacR = 0.f, dlp[4] = {0, 0, 0, 0};

	Engine() {
		patchDefaults(v);
		// empty user slots must be USER waves: a default Wave is a LOOP with no table to read
		for (int i = 0; i < NUM_USER; i++) user[i].kind = WK_USER;
		setSampleRate(48000.f);
	}

	void setSampleRate(float s) {
		if (s == sr && fx.sr == s) return;
		sr = s;
		isr = 1.f / s;
		dckK = std::exp(-1.f / (0.003f * s));
		fx.setSampleRate(s);
	}

	const Wave& wave(int i) const {
		const Rom& rom = Rom::get();
		if (i >= rom.firstUser) return user[std::min(NUM_USER - 1, i - rom.firstUser)];
		return rom.waves[std::max(0, i)];
	}

	float P(int tone, int p) const { return v[tp(tone, p)]; }
	float C(int p) const { return v[cp(p)]; }

	// ------------------------------------------------ notes
	void startTone(Voice& vo, int t, bool retrig) {
		ToneV& tv = vo.t[t];
		bool was = tv.active;
		tv.active = true;
		tv.wave = (int) P(t, T_WAVE);
		const Wave& w = wave(tv.wave);
		float note = vo.note;
		float vel = vo.vel;
		if (!was || !retrig) {
			tv.phase = 0.0;
			tv.s1 = tv.s2 = 0.f;
			tv.fpos = rng.uni() * (MOV_FRAMES - 1);
			tv.fdir = rng.uni() < 0.5f ? 1.f : -1.f;
			tv.ae.v = 0.f;
			tv.amp = 0.f;
			tv.inc = 0.f;
			tv.lastX = 0.f;
			tv.dck = 0.f;
		}
		bool isSmp = w.kind != WK_LOOP && w.kind != WK_MOVING;
		// a sample that restarts while the tone still sounds would jump from wherever it was to its
		// first value: carry the difference as an offset that dies away in a few ms (no click)
		if (was && isSmp) tv.dck = tv.lastX - (w.smp.empty() ? 0.f : w.smp[0]);
		tv.spos = 0.0;
		tv.smpDone = isSmp && w.smp.size() < 2;
		tv.rnd = rng.bi() * P(t, T_PRAND) * 0.012f;
		// velocity scaling
		float cv = velCurve((int) P(t, T_VCURVE), vel);
		tv.velAmp = velScale(P(t, T_VSENS) / 50.f, cv);
		tv.feVel = velScale(P(t, T_FE_VELO) / 50.f, vel);
		tv.peVel = velScale(P(t, T_PE_VELO) / 50.f, vel);
		// bias (level vs key)
		{
			float pt = P(t, T_BIASPT), lv = P(t, T_BIASLV);
			int dir = (int) P(t, T_BIASDIR);
			float oct = (note - pt) / 12.f, db = 0.f;
			if ((dir == 0 || dir == 2) && oct > 0.f) db = lv * 1.5f * oct;
			if ((dir == 1 || dir == 2) && oct < 0.f) db = lv * 1.5f * -oct;
			tv.bias = std::pow(10.f, std::max(-60.f, std::min(12.f, db)) / 20.f);
		}
		float keyOct = (note - 60.f) / 12.f;
		// pitch envelope (control rate): L0 -T1-> L1 -T2-> 0 ... release -T3-> L2
		{
			Env& e = tv.pe;
			e.rate = sr / CTRL;
			float ts = std::pow(2.f, -P(t, T_PE_TKF) / 10.f * keyOct) * std::pow(4.f, -(P(t, T_PE_TVELO) / 50.f) * (vel * 2.f - 1.f));
			e.nAtk = 2;
			e.T[0] = envSecs(P(t, T_PE_T1)) * ts;
			e.L[0] = P(t, T_PE_L1);
			e.T[1] = envSecs(P(t, T_PE_T2)) * ts;
			e.L[1] = 0.f;
			e.sus = 0.f;
			e.relT = envSecs(P(t, T_PE_T3)) * ts;
			e.relL = P(t, T_PE_L2);
			e.start(P(t, T_PE_L0));
		}
		// filter envelope: 0 -T1-> L1 -T2-> L2 -T3-> SUS ... -T4-> L4
		{
			Env& e = tv.fe;
			e.rate = sr / CTRL;
			float ts = std::pow(2.f, -P(t, T_FE_TKF) / 10.f * keyOct);
			float t1 = std::pow(4.f, -(P(t, T_FE_TVELO) / 50.f) * (vel * 2.f - 1.f));
			e.nAtk = 3;
			e.T[0] = envSecs(P(t, T_FE_T1)) * ts * t1;
			e.L[0] = P(t, T_FE_L1);
			e.T[1] = envSecs(P(t, T_FE_T2)) * ts;
			e.L[1] = P(t, T_FE_L2);
			e.T[2] = envSecs(P(t, T_FE_T3)) * ts;
			e.L[2] = P(t, T_FE_SUS);
			e.sus = P(t, T_FE_SUS);
			e.relT = envSecs(P(t, T_FE_T4)) * ts;
			e.relL = P(t, T_FE_L4);
			e.start(0.f);
		}
		// amp envelope (per sample), restarted from where it is so retriggers never click
		{
			Env& e = tv.ae;
			e.rate = sr;
			float ts = std::pow(2.f, -P(t, T_AE_TKF) / 10.f * keyOct);
			float t1 = std::pow(4.f, -(P(t, T_AE_TVELO) / 50.f) * (vel * 2.f - 1.f));
			e.nAtk = 3;
			e.T[0] = envSecs(P(t, T_AE_T1)) * ts * t1;
			e.L[0] = P(t, T_AE_L1) * 0.01f;
			e.T[1] = envSecs(P(t, T_AE_T2)) * ts;
			e.L[1] = P(t, T_AE_L2) * 0.01f;
			e.T[2] = envSecs(P(t, T_AE_T3)) * ts;
			e.L[2] = P(t, T_AE_SUS) * 0.01f;
			e.sus = e.L[2];
			e.relT = envSecs(P(t, T_AE_T4)) * ts;
			e.relL = 0.f;
			e.start(e.v);
		}
		for (int l = 0; l < 2; l++) {
			int b = T_L1_RATE + l * LFO_STRIDE;
			bool key = P(t, b + 5) > 0.5f;
			float d = P(t, b + 1) * 0.01f;
			tv.lfo[l].start(key ? 0.f : freePh[t][l], 10.f * d * d, P(t, b + 2), rng);
		}
		control(vo, tv, t); // first control values now, so the first samples are right
		tv.amp = was ? tv.amp : 0.f;
	}

	void noteOn(int ch, float note, float vel, bool legato) {
		Voice& vo = voices[ch];
		vo.vel = vel;
		bool porta = C(C_PORTA) > 0.5f;
		vo.glide = porta && vo.sounding();
		if (!vo.glide) vo.cur = note;
		vo.note = note;
		if (legato) return;
		noteOns++;
		for (int t = 0; t < 4; t++) {
			bool on = P(t, T_ON) > 0.5f && note >= P(t, T_KEYLO) && note <= P(t, T_KEYHI);
			if (on) startTone(vo, t, true);
			else if (vo.t[t].active) releaseTone(vo.t[t]);
		}
	}

	static void releaseTone(ToneV& tv) {
		tv.pe.release();
		tv.fe.release();
		tv.ae.release();
	}

	void noteOff(int ch) {
		for (int t = 0; t < 4; t++)
			if (voices[ch].t[t].active) releaseTone(voices[ch].t[t]);
	}

	void allOff() {
		for (int c = 0; c < MAXV; c++) {
			for (int t = 0; t < 4; t++) voices[c].t[t].active = false;
			prevGate[c] = false;
		}
	}

	// ------------------------------------------------ control rate (every CTRL samples)
	float curMod = 0.f, curBend = 0.f;

	void control(Voice& vo, ToneV& tv, int t) {
		float dt = CTRL * isr;
		float l1 = tv.lfo[0].tick(dt, lfoHz(P(t, T_L1_RATE)), (int) P(t, T_L1_WAVE), (int) P(t, T_L1_OFS), rng);
		float l2 = tv.lfo[1].tick(dt, lfoHz(P(t, T_L2_RATE)), (int) P(t, T_L2_WAVE), (int) P(t, T_L2_OFS), rng);
		float pe = tv.pe.tick();
		float fe = tv.fe.tick();
		const Wave& w = wave(tv.wave);

		// ---- pitch
		float note = vo.cur;
		float semis = (note - 60.f) * P(t, T_PKF) * 0.01f + C(C_OCT) * 12.f + P(t, T_COARSE) + P(t, T_FINE) * 0.01f + tv.rnd;
		if (P(t, T_BENDSW) > 0.5f) semis += curBend * C(C_BENDR);
		semis += P(t, T_ATBEND) * vo.press;
		semis += P(t, T_PE_DEPTH) * (pe / 50.f) * tv.peVel;
		auto lp = [](float d) { return 12.f * (d / 50.f) * std::fabs(d / 50.f); };
		semis += lp(P(t, T_L1_PITCH)) * l1 + lp(P(t, T_L2_PITCH)) * l2;
		semis += 0.6f * (P(t, T_MODLFO) * 0.01f) * curMod * tv.lfo[0].out; // mod wheel vibrato on LFO 1
		semis = std::max(-96.f, std::min(84.f, semis));
		float f = ROOT_HZ * fastExp2(semis / 12.f);
		float hfGain = 1.f;
		if (w.kind == WK_LOOP || w.kind == WK_MOVING) {
			// a fundamental above about 0.45 x sample rate can only alias: fade it out and hold the rate
			hfGain = std::max(0.f, std::min(1.f, (0.49f * sr - f) / (0.04f * sr)));
			f = std::min(f, 0.49f * sr);
			tv.incT = f * isr;
			float lv = std::log2(std::max(1e-6f, MAXH * f / (0.45f * sr)));
			lv = std::max(0.f, std::min((float) (NMIP - 1), lv));
			tv.m0 = (int) lv;
			tv.m1 = std::min(NMIP - 1, tv.m0 + 1);
			tv.mf = lv - tv.m0;
			tv.scan = w.scanHz * 2.f * (MOV_FRAMES - 1) * isr;
		}
		else
			tv.incT = (f / ROOT_HZ) * (w.smpRate * isr);
		if (tv.inc == 0.f) tv.inc = tv.incT;

		// ---- filter
		float oct = (note - 60.f) / 12.f * P(t, T_F_KF) * 0.01f;
		oct += (P(t, T_F_ENV) / 50.f) * 7.f * (fe / 50.f) * tv.feVel;
		oct += (P(t, T_L1_TVF) / 50.f) * 4.f * l1 + (P(t, T_L2_TVF) / 50.f) * 4.f * l2;
		oct += (P(t, T_F_AT) / 50.f) * 4.f * vo.press + (P(t, T_MODCUT) / 50.f) * 4.f * curMod;
		float fc = cutoffHz(P(t, T_F_CUT)) * fastExp2(oct);
		fc = std::max(16.f, std::min(0.46f * sr, fc));
		tv.g = std::tan(PI_F * fc * isr);
		float res = P(t, T_F_RES) * 0.01f;
		tv.k = 2.f * (1.f - 0.985f * res);
		tv.fmode = (int) P(t, T_F_MODE);

		// ---- amp
		float lvl = P(t, T_LEVEL) * 0.01f;
		float a = lvl * lvl * tv.velAmp * tv.bias * hfGain;
		a *= std::max(0.f, std::min(2.f, 1.f + (P(t, T_A_AT) / 50.f) * vo.press));
		a *= std::max(0.f, 1.f + (P(t, T_L1_TVA) / 50.f) * l1 * 0.5f);
		a *= std::max(0.f, 1.f + (P(t, T_L2_TVA) / 50.f) * l2 * 0.5f);
		tv.ampT = a;
		float pan = (P(t, T_PAN) / 50.f + 1.f) * 0.25f * PI_F;
		tv.panL = std::cos(pan);
		tv.panR = std::sin(pan);
		tv.ae.sus = P(t, T_AE_SUS) * 0.01f;
		tv.fe.sus = P(t, T_FE_SUS);
	}

	// ------------------------------------------------ one sample of one tone
	inline float renderTone(ToneV& tv) {
		const Wave& w = wave(tv.wave);
		float x = 0.f;
		tv.inc += (tv.incT - tv.inc) * 0.25f;
		if (w.kind == WK_LOOP || w.kind == WK_MOVING) {
			if (w.tbl.empty()) return 0.f;
			tv.phase += tv.inc;
			if (tv.phase >= 1.0) tv.phase -= std::floor(tv.phase);
			// in double: as a float a phase just under 1 rounds up to 1.0 and reads past the table
			double idx = tv.phase * TBL;
			int i = std::min(TBL - 1, (int) idx);
			float fr = (float) (idx - i);
			if (w.kind == WK_LOOP) {
				const float* a = w.table(0, tv.m0);
				const float* b = w.table(0, tv.m1);
				float xa = a[i] + (a[i + 1] - a[i]) * fr;
				float xb = b[i] + (b[i + 1] - b[i]) * fr;
				x = xa + (xb - xa) * tv.mf;
			}
			else {
				tv.fpos += tv.fdir * tv.scan;
				if (tv.fpos >= MOV_FRAMES - 1) {
					tv.fpos = 2.f * (MOV_FRAMES - 1) - tv.fpos;
					tv.fdir = -1.f;
				}
				else if (tv.fpos < 0.f) {
					tv.fpos = -tv.fpos;
					tv.fdir = 1.f;
				}
				int f0 = std::min(MOV_FRAMES - 2, (int) tv.fpos);
				float ff = tv.fpos - f0;
				float r[2];
				for (int k = 0; k < 2; k++) {
					const float* a = w.table(f0 + k, tv.m0);
					const float* b = w.table(f0 + k, tv.m1);
					float xa = a[i] + (a[i + 1] - a[i]) * fr;
					float xb = b[i] + (b[i + 1] - b[i]) * fr;
					r[k] = xa + (xb - xa) * tv.mf;
				}
				x = r[0] + (r[1] - r[0]) * ff;
			}
		}
		else {
			size_t n = w.smp.size();
			if (!tv.smpDone) {
				if (n < 2) tv.smpDone = true;
				else if (w.loop) {
					// the loop is n samples long and the last one runs on into the first
					if (tv.spos >= (double) n) tv.spos -= (double) n * std::floor(tv.spos / (double) n);
				}
				else if (tv.spos >= (double) (n - 1)) tv.smpDone = true;
				if (!tv.smpDone) {
					size_t i = std::min(n - 1, (size_t) tv.spos);
					float fr = (float) (tv.spos - (double) i);
					size_t j = i + 1 < n ? i + 1 : (w.loop ? 0 : n - 1);
					x = w.smp[i] + (w.smp[j] - w.smp[i]) * fr;
					tv.spos += tv.inc;
				}
			}
			if (tv.dck != 0.f) {
				x += tv.dck;
				tv.dck *= dckK;
				if (std::fabs(tv.dck) < 1e-6f) tv.dck = 0.f;
			}
			tv.lastX = x;
		}
		// TVF: trapezoidal state-variable filter
		float g = tv.g, k = tv.k;
		float a1 = 1.f / (1.f + g * (g + k));
		float v3 = x - tv.s2;
		float v1 = a1 * tv.s1 + g * a1 * v3;
		float v2 = tv.s2 + g * v1;
		tv.s1 = 2.f * v1 - tv.s1;
		tv.s2 = 2.f * v2 - tv.s2;
		// soft limit inside the loop so high resonance stays musical
		tv.s1 = tv.s1 / (1.f + 0.02f * tv.s1 * tv.s1);
		float y = tv.fmode == 0 ? v2 : tv.fmode == 1 ? v1 * k : x - k * v1 - v2;
		// TVA
		float e = tv.ae.tick();
		tv.amp += (tv.ampT - tv.amp) * 0.02f;
		if (tv.ae.stage == Env::DONE) tv.active = false;
		return y * tv.amp * e * e;
	}

	// ------------------------------------------------ one sample
	void process(const In& in) {
		bool solo = C(C_SOLO) > 0.5f;
		bool legato = C(C_LEGATO) > 0.5f;
		int nch = solo ? std::min(1, in.channels) : std::min(MAXV, in.channels);
		for (int c = 0; c < nch; c++) {
			bool g = in.gate[c] >= 1.f;
			float note = 60.f + in.voct[c] * 12.f;
			Voice& vo = voices[c];
			vo.press = std::max(0.f, std::min(1.f, in.press[c] * 0.1f));
			if (g && !prevGate[c]) {
				float vel = in.velConnected ? std::max(0.f, std::min(1.f, in.vel[c] * 0.1f)) : 0.8f;
				noteOn(c, note, vel, false);
			}
			else if (!g && prevGate[c])
				noteOff(c);
			else if (g && std::fabs(note - vo.note) > 0.01f) {
				// pitch moved while the gate is held (solo/mono keyboards, or V/OCT modulation)
				if (solo && !legato && std::fabs(note - vo.note) > 0.5f) noteOn(c, note, vo.vel, false);
				else {
					vo.note = note;
					if (C(C_PORTA) > 0.5f && std::fabs(note - vo.cur) > 0.5f) vo.glide = true;
					else if (!vo.glide) vo.cur = note;
				}
			}
			prevGate[c] = g;
			prevVoct[c] = note;
		}
		for (int c = nch; c < MAXV; c++)
			if (prevGate[c]) {
				noteOff(c);
				prevGate[c] = false;
			}

		if (++ctr >= CTRL) {
			ctr = 0;
			curMod = in.mod;
			curBend = in.bend;
			float dt = CTRL * isr;
			for (int t = 0; t < 4; t++)
				for (int l = 0; l < 2; l++) {
					freePh[t][l] += lfoHz(P(t, T_L1_RATE + l * LFO_STRIDE)) * dt;
					freePh[t][l] -= std::floor(freePh[t][l]);
				}
			float tau = 0.002f * std::pow(1000.f, C(C_PTIME) * 0.01f);
			glideK = 1.f - std::exp(-dt / tau);
			for (int c = 0; c < MAXV; c++) {
				Voice& vo = voices[c];
				if (!vo.sounding()) continue;
				if (vo.glide) {
					vo.cur += (vo.note - vo.cur) * glideK;
					if (std::fabs(vo.note - vo.cur) < 0.005f) {
						vo.cur = vo.note;
						vo.glide = false;
					}
				}
				else
					vo.cur = vo.note;
				for (int t = 0; t < 4; t++)
					if (vo.t[t].active) control(vo, vo.t[t], t);
			}
		}

		float L = 0.f, R = 0.f;
		for (int c = 0; c < MAXV; c++) {
			Voice& vo = voices[c];
			for (int t = 0; t < 4; t++) {
				ToneV& tv = vo.t[t];
				if (!tv.active) continue;
				float y = renderTone(tv);
				if (in.solo >= 0 && in.solo != t) continue;
				L += y * tv.panL;
				R += y * tv.panR;
			}
		}

		// 90s converter: 32 kHz zero-order hold (keeps the aliasing shimmer), 14-bit steps,
		// a touch of converter warmth, then a gentle 14 kHz smoothing filter
		if (C(C_DAC90) > 0.5f) {
			if (sr > 32000.f) {
				dacAcc += 32000.f * isr;
				if (dacAcc >= 1.f) {
					dacAcc -= 1.f;
					dacL = std::round(L * 4096.f) / 4096.f;
					dacR = std::round(R * 4096.f) / 4096.f;
				}
				L = dacL;
				R = dacR;
			}
			else {
				L = std::round(L * 4096.f) / 4096.f;
				R = std::round(R * 4096.f) / 4096.f;
			}
			L = std::tanh(1.1f * L) / 1.1f;
			R = std::tanh(1.1f * R) / 1.1f;
			float a = 1.f - std::exp(-2.f * PI_F * std::min(14000.f, 0.45f * sr) * isr);
			dlp[0] += a * (L - dlp[0]);
			dlp[1] += a * (dlp[0] - dlp[1]);
			dlp[2] += a * (R - dlp[2]);
			dlp[3] += a * (dlp[2] - dlp[3]);
			L = dlp[1];
			R = dlp[3];
		}

		fx.process(v + NT * 4, L, R);
		float lv = C(C_LEVEL) / 80.f;
		outL = L * lv * lv;
		outR = R * lv * lv;
	}

	int activeTones() const {
		int n = 0;
		for (int c = 0; c < MAXV; c++)
			for (int t = 0; t < 4; t++) n += voices[c].t[t].active;
		return n;
	}
};

} // namespace facet
