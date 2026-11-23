#include "PolyEngine.hpp"
#include "../generated/Sounds.hpp"

namespace hk {

static inline float polyBlep(double t, double dt) {
	if (t < dt) { double x = t / dt; return (float) (x + x - x * x - 1.0); }
	if (t > 1.0 - dt) { double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
	return 0.f;
}
static inline float sawB(double ph, double dt) { return (float) (2.0 * ph - 1.0) - polyBlep(ph, dt); }
static inline float pulseB(double ph, double dt, float pw) {
	float y = ph < pw ? 1.f : -1.f;
	y += polyBlep(ph, dt);
	double t2 = ph + 1.0 - pw; if (t2 >= 1.0) t2 -= 1.0;
	y -= polyBlep(t2, dt);
	return y;
}
static inline float triN(double ph) { return (float) (4.0 * std::fabs(ph - 0.5) - 1.0); }
// Poly-800 "saw": sum of squares at f, 2f, 4f, 8f with halving levels
static inline float stairSaw(double ph, double dt) {
	float y = 0.f, g = 1.f;
	double p = ph, d = dt;
	for (int k = 0; k < 4; k++) {
		y += g * pulseB(p, d, 0.5f);
		p = p * 2.0; p -= std::floor(p); d *= 2.0; g *= 0.5f;
	}
	return y * (1.f / 1.875f);
}

void PolyEngine::init(float sampleRate) {
	fs = sampleRate; fs2 = 2.f * fs;
	decL.reset(); decR.reset();
	floorNoise.seed(91);
	setSound(0);
	allOff();
}

void PolyEngine::setSound(int idx) {
	soundIdx = std::max(0, std::min(23, idx));
	s = &POLY_SOUNDS[soundIdx];
	hpfMode = -1;
	for (int i = 0; i < MAX_SLOTS; i++) {
		Voice& vc = v[i];
		vc.off1 = hashBi(soundIdx, i, 1) * 2.5f / 100.f;
		vc.off2 = hashBi(soundIdx, i, 2) * 2.5f / 100.f;
		vc.cutOff = std::exp2(hashBi(soundIdx, i, 3) * 0.0426f); // +-3%
		vc.nz.seed(1000 + i);
		vc.ph1 = hash01(i, 7); vc.ph2 = hash01(i, 8);
	}
}

void PolyEngine::allOff() {
	for (auto& vc : v) { vc.gate = false; vc.aenv.stage = Adsr::IDLE; vc.aenv.v = 0.f; vc.fenv.stage = Adsr::IDLE; vc.fenv.v = 0.f; vc.lp.reset(); }
	sharedLp.reset(); sharedLpR.reset(); sharedEnv.stage = Adsr::IDLE; sharedEnv.v = 0.f;
	decL.reset(); decR.reset();
}

void PolyEngine::noteOn(int slot, float note, float vel, float pan) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	Voice& vc = v[slot];
	bool legato = s->mono && vc.gate;
	vc.target = note + 12.f * s->oct;
	if (!legato && !(s->mono && vc.aenv.active() && s->glide > 0.f)) vc.note = vc.target;
	vc.vel = vel; vc.pan = pan;
	vc.gate = true;
	if (!legato) {
		float as = curM.attackScale(), ds = curM.decayScale();
		vc.fenv.set(s->fenv, false, fs2, as, ds);
		vc.aenv.set(s->aenv, s->aenvGate, fs2, as, ds);
		vc.fenv.gateOn(); vc.aenv.gateOn();
		vc.lfoAge = 0.f;
		if (s->model == PM_P800) {
			sharedEnv.set(s->fenv, false, fs2, as, ds);
			sharedEnv.gateOn();
		}
	}
}

void PolyEngine::setPitch(int slot, float note) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	v[slot].target = note + 12.f * s->oct;
	if (!(s->mono && s->glide > 0.f)) v[slot].note = v[slot].target;
}

void PolyEngine::noteOff(int slot) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	Voice& vc = v[slot];
	vc.gate = false;
	vc.fenv.gateOff(); vc.aenv.gateOff();
	if (s->model == PM_P800) {
		bool any = false;
		for (auto& o : v) any |= o.gate;
		if (!any) sharedEnv.gateOff();
	}
}

float PolyEngine::renderVoice(Voice& vc, int slot, float lfo, float vib, float* sharedIn) {
	const PolySound& p = *s;
	const Macros& m = curM;
	float grit = m.grit;
	float gf = grit < 0.5f ? grit * 2.f : 1.f + (grit - 0.5f) * 4.f;
	// glide
	if (p.mono && p.glide > 0.f) vc.note += (vc.target - vc.note) * (1.f - std::exp(-1.f / (p.glide * fs2)));
	// pitch
	float fe = vc.fenv.next();
	float ae = vc.aenv.next();
	vc.lfoAge += 1.f / fs2;
	float lfoFade = p.lfoDelay > 0.f ? clampf(vc.lfoAge / p.lfoDelay, 0.f, 1.f) : 1.f;
	float cents = p.lfoPitch * lfo * lfoFade + 30.f * m.mod * vib;
	float drift1 = 0.f, drift2 = 0.f;
	if (p.model == PM_JUP) {
		// slow drift: band-limited noise below ~0.3 Hz
		float a = 1.f - std::exp(-2.f * PI_F * 0.3f / fs2);
		vc.dn1 += (vc.nz.next() - vc.dn1) * a; vc.drift1 += (vc.dn1 * 60.f - vc.drift1) * a;
		vc.dn2 += (vc.nz.next() - vc.dn2) * a; vc.drift2 += (vc.dn2 * 60.f - vc.drift2) * a;
		drift1 = (vc.off1 + clampf(vc.drift1, -1.5f, 1.5f) / 100.f) * gf;
		drift2 = (vc.off2 + clampf(vc.drift2, -1.5f, 1.5f) / 100.f) * gf;
	}
	float n1 = vc.note + cents / 100.f + drift1;
	double f1 = 440.0 * std::exp2((n1 - 69.f) / 12.0);
	double dt1 = std::min(f1 / fs2, 0.45);
	// oscillator 1
	double ph = vc.ph1 + dt1;
	bool wrapped = ph >= 1.0;
	if (wrapped) { ph -= 1.0; vc.subSign = -vc.subSign; }
	vc.ph1 = ph;
	float pw = clampf(p.pw + p.pwm * lfo, 0.05f, 0.95f);
	float o = 0.f;
	if (p.saw > 0.f) o += p.saw * (p.model == PM_P800 ? stairSaw(ph, dt1) : sawB(ph, dt1));
	if (p.pulse > 0.f) o += p.pulse * pulseB(ph, dt1, pw);
	if (p.tri > 0.f) o += p.tri * triN(ph);
	if (p.sub > 0.f) {
		float sb = vc.subSign;
		float pb = polyBlep(ph, dt1);
		float subv = (ph < dt1) ? sb * (1.f + pb) : (ph > 1.0 - dt1 ? sb * (1.f - pb) : sb);
		o += p.sub * subv;
	}
	if (p.noise > 0.f) o += p.noise * vc.nz.next();
	// oscillator 2
	if (p.lvl2 > 0.f) {
		float n2 = vc.note + 12.f * p.oct2 + p.semi2 + p.fine2 / 100.f + cents / 100.f + drift2 + p.sweep2 * fe;
		double f2 = 440.0 * std::exp2((n2 - 69.f) / 12.0);
		double dt2 = std::min(f2 / fs2, 0.45);
		double ph2 = vc.ph2 + dt2;
		if (ph2 >= 1.0) ph2 -= 1.0;
		if (p.sync && wrapped) ph2 = ph * (dt2 / dt1); // hard sync (no BLEP on the reset)
		if (ph2 >= 1.0) ph2 -= std::floor(ph2);
		vc.ph2 = ph2;
		float y2;
		switch (p.wave2) {
			case W2_PULSE: y2 = pulseB(ph2, dt2, pw); break;
			case W2_TRI: y2 = triN(ph2); break;
			case W2_SQ: y2 = pulseB(ph2, dt2, 0.5f); break;
			default: y2 = (p.model == PM_P800) ? stairSaw(ph2, dt2) : sawB(ph2, dt2); break;
		}
		o += p.lvl2 * y2;
	}
	o *= 0.5f;
	float velN = vc.vel / 127.f;
	float velGain = 1.f - p.vel * (1.f - velN);
	if (p.model == PM_P800) {
		*sharedIn += o * ae * velGain;
		return 0.f;
	}
	// filter (coefficients every 4 samples)
	if (vc.filtCounter-- <= 0) {
		vc.filtCounter = 3;
		float oct = p.env * fe + p.kbd * (vc.note - 60.f) / 12.f + p.velcut * velN + m.tone * 3.f;
		float fc = clampf(p.cut * std::exp2(oct) * (1.f + (vc.cutOff - 1.f) * gf), 16.f, std::min(20000.f, 0.45f * fs2));
		vc.lp.setCutoff(fc, fs2);
	}
	float k = 4.1f * std::pow(p.res, 1.2f);
	float out2 = 0.f;
	float y = vc.lp.process(o, k, &out2);
	if (p.poles == 2) y = out2;
	return y * ae * velGain;
}

void PolyEngine::process(float* L, float* R, int n, const Macros& m) {
	curM = m;
	const PolySound& p = *s;
	if (hpfMode != p.hpf) {
		hpfMode = p.hpf;
		if (p.hpf == 0 && p.model == PM_JUNO) { hpfL.lowShelf(150.f, 3.f, fs); }
		else if (p.hpf == 2) { hpfL.highpass(240.f, 0.5f, fs); }
		else if (p.hpf == 3) { hpfL.highpass(720.f, 0.5f, fs); }
		else hpfL.set(1, 0, 0, 1, 0, 0);
		hpfR = hpfL;
	}
	float gl[MAX_SLOTS], gr[MAX_SLOTS];
	for (int i = 0; i < MAX_SLOTS; i++) panGains(v[i].pan, gl[i], gr[i]);
	float floorAmp = dbToGain(-78.f) * (m.grit / 0.5f);
	double lfoInc = p.lfoRate / fs2, vibInc = 5.5 / fs2;
	for (int i = 0; i < n; i++) {
		float os[2][2];
		for (int k = 0; k < 2; k++) {
			lfoPh += lfoInc; if (lfoPh >= 1.0) lfoPh -= 1.0;
			vibPh += vibInc; if (vibPh >= 1.0) vibPh -= 1.0;
			float lfo = triN(lfoPh);
			float vib = p.lfoRate > 0.f ? lfo : triN(vibPh);
			float sl = 0.f, sr = 0.f, shared = 0.f, sharedL = 0.f, sharedR = 0.f;
			for (int vi = 0; vi < MAX_SLOTS; vi++) {
				Voice& vc = v[vi];
				if (!vc.aenv.active()) continue;
				float before = shared;
				float y = renderVoice(vc, vi, lfo, vib, &shared);
				sl += y * gl[vi]; sr += y * gr[vi];
				float d = shared - before;
				sharedL += d * gl[vi]; sharedR += d * gr[vi];
			}
			if (p.model == PM_P800) {
				float fe = sharedEnv.next();
				float lowest = 200.f;
				for (auto& vc : v) if (vc.gate) lowest = std::min(lowest, vc.note);
				if (lowest > 199.f) lowest = 60.f;
				float oct = p.env * fe + p.kbd * (lowest - 60.f) / 12.f + m.tone * 3.f;
				float fc = clampf(p.cut * std::exp2(oct), 16.f, std::min(20000.f, 0.45f * fs2));
				sharedLp.setCutoff(fc, fs2); sharedLpR.setCutoff(fc, fs2);
				float kk = 4.1f * std::pow(p.res, 1.2f);
				// one filter for the instrument (one copy per side so TWIN keeps its spread)
				sl += sharedLp.process(sharedL, kk);
				sr += sharedLpR.process(sharedR, kk);
			}
			os[k][0] = sl; os[k][1] = sr;
		}
		float l = decL.process(os[0][0], os[1][0]);
		float r = decR.process(os[0][1], os[1][1]);
		float nf = floorNoise.next() * floorAmp;
		L[i] += hpfL.process(l) + nf;
		R[i] += hpfR.process(r) + nf;
	}
}

} // namespace hk
