#include "WaveEngine.hpp"
#include "../generated/Sounds.hpp"

namespace hk {

static inline float sqTime(int T) { return T <= 0 ? 0.f : 0.002f * std::exp2(T / 5.f); }

void WaveEngine::init(float sampleRate) {
	fs = sampleRate; fs2 = 2.f * fs;
	waveTables();
	for (int i = 0; i < MAX_SLOTS; i++) v[i].nz.seed(300 + i);
	setSound(0);
	allOff();
}

void WaveEngine::setSound(int idx) {
	s = &WAVE_SOUNDS[std::max(0, std::min(21, idx))];
	usesWheel = s->m1Src == SRC_WHEEL || s->m2Src == SRC_WHEEL;
	for (int k = 0; k < 3; k++) usesWheel |= s->o[k].pmSrc == SRC_WHEEL || s->o[k].amSrc == SRC_WHEEL || s->lfo[k][5] == SRC_WHEEL;
}

void WaveEngine::allOff() {
	for (auto& vc : v) { vc.gate = false; for (auto& e : vc.env) { e.stage = 0; e.v = 0.f; } vc.lp.reset(); }
	decL.reset(); decR.reset();
}

void WaveEngine::noteOn(int slot, float note, float vel, float pan) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	Voice& vc = v[slot];
	bool legato = s->mono && vc.gate;
	vc.target = note;
	if (!legato && !(s->mono && vc.env[3].active() && s->glide > 0)) vc.note = note;
	vc.vel = vel; vc.pan = pan; vc.gate = true;
	if (legato) return;
	float vn = vel / 127.f;
	float as = curM.attackScale(), ds = curM.decayScale();
	for (int e = 0; e < 4; e++) {
		const int* d = s->env[e];
		float lv = 1.f - (d[7] / 63.f) * (1.f - vn);
		float tk = std::exp2(-(d[9] / 63.f) * (note - 60.f) / 24.f);
		for (int k = 0; k < 3; k++) vc.env[e].L[k] = d[k] / 63.f * lv;
		vc.env[e].T[0] = sqTime(d[3]) * (1.f + d[8] / 63.f * (1.f - vn) * 3.f) * tk * as;
		vc.env[e].T[1] = sqTime(d[4]) * tk * ds;
		vc.env[e].T[2] = sqTime(d[5]) * tk * ds;
		vc.env[e].T[3] = sqTime(d[6]) * tk * ds;
		vc.env[e].on(fs2);
	}
	for (int k = 0; k < 3; k++) { vc.o[k].ph = 0.0; vc.o[k].done = false; vc.lfoPh[k] = hash01(slot, k, 55); }
	vc.lfoAge = 0.f;
}

void WaveEngine::setPitch(int slot, float note) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	v[slot].target = note;
	if (!(s->mono && s->glide > 0)) v[slot].note = note;
}

void WaveEngine::noteOff(int slot) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	v[slot].gate = false;
	for (auto& e : v[slot].env) e.off(fs2);
}

// reads one oscillator for this output sample; returns the value for the current GRIT
float readOsc(const Wave& W, int band, double ph, double inc, float grit, bool& done) {
	const WaveBand& B = W.band[band];
	int len = W.len;
	if (W.kind == WK_ONESHOT && ph >= len) { done = true; return 0.f; }
	auto stairAvg = [&](const std::vector<int8_t>& q, const std::vector<int32_t>& c) -> float {
		auto integ = [&](double x) -> double {
			if (W.kind == WK_ONESHOT) {
				if (x >= len) return (double) c[len];
				int i = (int) x; return c[i] + (x - i) * q[i];
			}
			double w = std::floor(x / len);
			double r = x - w * len;
			int i = (int) r; if (i >= len) i = len - 1;
			return w * c[len] + c[i] + (r - i) * q[i];
		};
		if (inc < 1e-9) return q[(int) ph % len] / 128.f;
		return (float) ((integ(ph + inc) - integ(ph)) / inc / 128.0);
	};
	auto lin = [&]() -> float {
		int i = (int) ph; double f = ph - i;
		int i1 = i + 1;
		if (i1 >= len) i1 = (W.kind == WK_ONESHOT) ? i : 0;
		if (i >= len) i = len - 1;
		return (float) (B.f16[i] + f * (B.f16[i1] - B.f16[i]));
	};
	if (grit <= 0.f) return lin();
	if (grit < 0.5f) { float a = lin(), b = stairAvg(B.q8, B.c8); return a + (b - a) * grit * 2.f; }
	if (grit == 0.5f) return stairAvg(B.q8, B.c8);
	float a = stairAvg(B.q8, B.c8), b = stairAvg(B.q6, B.c6);
	return a + (b - a) * (grit - 0.5f) * 2.f;
}

float WaveEngine::renderVoice(Voice& vc, float vib) {
	const WaveSound& p = *s;
	const Macros& m = curM;
	const auto& tabs = waveTables();
	if (p.mono && p.glide > 0) {
		float t = 0.005f * std::exp2(p.glide / 6.f);
		vc.note += (vc.target - vc.note) * (1.f - std::exp(-1.f / (t * fs2)));
	}
	float src[NUM_SRC] = {};
	for (int e = 0; e < 4; e++) src[SRC_ENV1 + e] = vc.env[e].next(fs2);
	float vn = vc.vel / 127.f;
	src[SRC_VEL] = vn; src[SRC_VEL2] = vn * vn;
	src[SRC_KBD] = (vc.note - 60.f) / 36.f;
	src[SRC_KBD2] = vc.note > 60.f ? (vc.note - 60.f) / 36.f : 0.f;
	src[SRC_WHEEL] = m.mod;
	vc.lfoAge += 1.f / fs2;
	for (int l = 0; l < 3; l++) {
		const int* d = p.lfo[l];
		if (d[2] == 0 && d[4] == 0 && d[5] == SRC_OFF) { src[SRC_LFO1 + l] = 0.f; continue; }
		double f = 0.05 * std::exp2(d[0] / 7.0);
		double ph = vc.lfoPh[l] + f / fs2;
		if (ph >= 1.0) { ph -= 1.0; vc.lfoNoise[l] = vc.nz.next(); }
		vc.lfoPh[l] = ph;
		float w;
		switch (d[1]) {
			case 1: w = (float) (2.0 * ph - 1.0); break;
			case 2: w = ph < 0.5 ? 1.f : -1.f; break;
			case 3: w = vc.lfoNoise[l]; break;
			default: w = (float) (4.0 * std::fabs(ph - 0.5) - 1.0); break;
		}
		float td = d[3] <= 0 ? 0.f : 0.02f * std::exp2(d[3] / 8.f);
		float fr = td > 0.f ? clampf(vc.lfoAge / td, 0.f, 1.f) : 1.f;
		float depth = d[2] + (d[4] - d[2]) * fr;
		if (d[5] != SRC_OFF) depth += 63.f * src[d[5]];
		src[SRC_LFO1 + l] = w * clampf(depth, 0.f, 63.f) / 63.f;
	}
	float vibCents = usesWheel ? 0.f : 30.f * m.mod * vib;
	float o[3] = {0, 0, 0};
	double ph1Before = vc.o[0].ph;
	bool wrapped1 = false;
	for (int k = 0; k < 3; k++) {
		const WaveOsc& os = p.o[k];
		if (!os.on) continue;
		const Wave& W = tabs[os.wave];
		float cents = (os.pmSrc != SRC_OFF ? os.pmDepth / 63.f * 1200.f * src[os.pmSrc] : 0.f) + vibCents;
		float nn = vc.note + 12.f * os.oct + os.semi + os.fine * 0.03125f + cents / 100.f;
		float hz = 440.f * std::exp2((nn - 69.f) / 12.f);
		double inc = hz * 256.0 / fs2;
		Osc& oc = vc.o[k];
		float val = oc.done ? 0.f : readOsc(W, waveBandFor(hz), oc.ph, inc, m.grit, oc.done);
		oc.ph += inc;
		if (k == 0 && std::floor(oc.ph / 256.0) != std::floor(ph1Before / 256.0)) wrapped1 = true;
		if (W.kind != WK_ONESHOT && oc.ph >= W.len) oc.ph -= std::floor(oc.ph / W.len) * W.len;
		if (k == 1 && p.sync && wrapped1) oc.ph = 0.0;
		float lvl = clampf(os.level + (os.amSrc != SRC_OFF ? os.amDepth * src[os.amSrc] : 0.f), 0.f, 63.f) / 63.f;
		o[k] = val * lvl;
	}
	if (p.am && p.o[1].on) o[0] *= o[1];
	float x = (o[0] + o[1] + o[2]) * (2.f / 3.f);
	x = ftanh(1.2f * x) / 1.2f;
	// GRIT above 0.5: the oscillator mix is also held at a 15 kHz clock
	if (m.grit > 0.5f) {
		float inc = 15000.f / fs2;
		vc.holdAcc += inc;
		float held;
		if (vc.holdAcc >= 1.f) { vc.holdAcc -= 1.f; float f = vc.holdAcc / inc; held = vc.hold * (1.f - f) + x * f; vc.hold = x; }
		else held = vc.hold;
		float g = (m.grit - 0.5f) * 2.f;
		x = x + (held - x) * g;
	}
	if (vc.filtCounter-- <= 0) {
		vc.filtCounter = 3;
		float units = p.cut + p.m1Depth * src[p.m1Src] * (p.m1Src != SRC_OFF) + p.m2Depth * src[p.m2Src] * (p.m2Src != SRC_OFF)
			+ (p.kbd / 63.f) * (vc.note - 60.f) * 12.7f / 12.f + m.tone * 40.f;
		float fc = clampf(20.f * std::exp2(units / 12.7f), 16.f, std::min(20000.f, 0.45f * fs2));
		vc.lp.setCutoff(fc, fs2);
	}
	float y = vc.lp.process(x, 3.6f * p.res / 31.f);
	return y * src[SRC_ENV4];
}

void WaveEngine::process(float* L, float* R, int n, const Macros& m) {
	curM = m;
	float soundPan = (s->pan - 8) / 7.5f;
	float gl[MAX_SLOTS], gr[MAX_SLOTS];
	for (int i = 0; i < MAX_SLOTS; i++) panGains(clampf(v[i].pan + soundPan, -1.f, 1.f), gl[i], gr[i]);
	double vibInc = 5.5 / fs2;
	for (int i = 0; i < n; i++) {
		float os[2][2];
		for (int k = 0; k < 2; k++) {
			vibPh += vibInc; if (vibPh >= 1.0) vibPh -= 1.0;
			float vib = (float) (4.0 * std::fabs(vibPh - 0.5) - 1.0);
			float sl = 0.f, sr = 0.f;
			for (int vi = 0; vi < MAX_SLOTS; vi++) {
				Voice& vc = v[vi];
				if (!vc.env[3].active()) continue;
				float y = renderVoice(vc, vib);
				sl += y * gl[vi]; sr += y * gr[vi];
			}
			os[k][0] = sl; os[k][1] = sr;
		}
		L[i] += decL.process(os[0][0], os[1][0]);
		R[i] += decR.process(os[0][1], os[1][1]);
	}
}

} // namespace hk
