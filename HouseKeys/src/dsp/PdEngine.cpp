#include "PdEngine.hpp"
#include "../generated/Sounds.hpp"

namespace hk {

// cosine table, 4096 points with linear interpolation (error below -130 dB)
struct CosTable {
	float t[4097];
	CosTable() { for (int i = 0; i <= 4096; i++) t[i] = (float) std::cos(2.0 * PI_D * i / 4096.0); }
};
static const CosTable COS_TAB;
// cos(2 pi p) for any p
static inline float fcos(double p) {
	p -= std::floor(p);
	double x = p * 4096.0;
	int i = (int) x; float f = (float) (x - i);
	return COS_TAB.t[i] + f * (COS_TAB.t[i + 1] - COS_TAB.t[i]);
}
static inline float ncos(double p) { return -fcos(p); }

static inline double sawMap(double ph, float a) {
	double d = 0.5 - 0.49 * a;
	return ph < d ? ph / (2.0 * d) : 0.5 + (ph - d) / (2.0 * (1.0 - d));
}

float pdWave(int w, double ph, float a) {
	a = clampf(a, 0.f, 1.f);
	switch (w) {
		case PDW_SAW: return ncos(sawMap(ph, a));
		case PDW_SQUARE: {
			double t = 2.0 * ph, fl = std::floor(t), h = t - fl;
			return ncos((fl + std::min(1.0, h / (1.0 - 0.98 * a))) * 0.5);
		}
		case PDW_PULSE: return ncos(std::min(1.0, ph / (1.0 - 0.98 * a)));
		case PDW_DSINE: { double p = ph * (1.0 + a); return ncos(p - std::floor(p)); }
		case PDW_SAWPULSE: {
			if (ph < 0.5) return ncos(0.5 * sawMap(2.0 * ph, a));
			return ncos(0.5 + 0.5 * std::min(1.0, (ph - 0.5) / (0.5 * (1.0 - 0.98 * a))));
		}
		case PDW_RES1: case PDW_RES2: case PDW_RES3: {
			double win = (w == PDW_RES1) ? 1.0 - ph : (w == PDW_RES2 ? 1.0 - std::fabs(2.0 * ph - 1.0) : std::min(1.0, 2.0 * (1.0 - ph)));
			return (float) win * fcos(ph * (1.0 + 15.0 * a) - 0.25);
		}
	}
	return 0.f;
}

// CZ-style stepped amplifier: 6 dB steps with a 4-bit fine part
static inline float ampQuant(float g) {
	if (g <= 1e-6f) return 0.f;
	// keep 4 fraction bits of the mantissa, rounded (same as frexp, round(m*32)/32, ldexp)
	uint32_t b; std::memcpy(&b, &g, 4);
	b = (b + (1u << 18)) & ~((1u << 19) - 1u);
	float r; std::memcpy(&r, &b, 4);
	return r;
}
static inline float qbits(float x, float levels) { return std::round(clampf(x, -1.f, 1.f) * levels) / levels; }

void PdEngine::init(float sampleRate) {
	fs = sampleRate; fs4 = 4.f * fs;
	setSound(0);
	allOff();
}

void PdEngine::setSound(int idx) { s = &PD_SOUNDS[std::max(0, std::min(11, idx))]; }

void PdEngine::allOff() {
	for (auto& vc : v) { vc.gate = false; for (auto& l : vc.l) { l.amp.stage = LinEnv::IDLE; l.amp.v = 0.f; l.dcw.stage = LinEnv::IDLE; } }
	dL1.reset(); dL2.reset(); dR1.reset(); dR2.reset();
}

void PdEngine::noteOn(int slot, float note, float vel, float pan) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	Voice& vc = v[slot];
	bool legato = s->mono && vc.gate;
	vc.target = note + 12.f * s->oct;
	if (!legato && !(s->mono && vc.l[0].amp.active() && s->glide > 0.f)) vc.note = vc.target;
	vc.vel = vel; vc.pan = pan; vc.gate = true;
	if (legato) return;
	float as = curM.attackScale(), ds = curM.decayScale();
	const PdLine* ld[2] = {&s->l1, &s->l2};
	for (int k = 0; k < 2; k++) {
		vc.l[k].dcw.setDcw(ld[k]->dcw, fs4, as, ds);
		vc.l[k].amp.setAmp(ld[k]->amp, ld[k]->ampGate, fs4, as, ds);
		vc.l[k].dcw.on(); vc.l[k].amp.on();
		vc.l[k].parity = 0;
	}
	vc.age = 0.f;
}

void PdEngine::setPitch(int slot, float note) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	v[slot].target = note + 12.f * s->oct;
	if (!(s->mono && s->glide > 0.f)) v[slot].note = v[slot].target;
}

void PdEngine::noteOff(int slot) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	v[slot].gate = false;
	for (auto& l : v[slot].l) { l.dcw.off(); l.amp.off(); }
}

void PdEngine::process(float* L, float* R, int n, const Macros& m) {
	curM = m;
	const PdSound& p = *s;
	float gl[MAX_SLOTS], gr[MAX_SLOTS];
	for (int i = 0; i < MAX_SLOTS; i++) panGains(v[i].pan, gl[i], gr[i]);
	float grit = m.grit;
	float detSemis = 12.f * p.det[0] + p.det[1] + p.det[2] / 100.f;
	double vibInc = (p.vibRate > 0.f ? p.vibRate : 5.5f) / fs4, tremInc = p.tremRate / fs4;
	float holdInc = 20000.f / fs4;
	for (int i = 0; i < n; i++) {
		float os[4][2];
		for (int k = 0; k < 4; k++) {
			vibPh += vibInc; if (vibPh >= 1.0) vibPh -= 1.0;
			tremPh += tremInc; if (tremPh >= 1.0) tremPh -= 1.0;
			float vib = fcos(vibPh - 0.25);
			float trem = 1.f - p.tremDepth * (0.5f + 0.5f * fcos(tremPh - 0.25));
			float aL = 0.f, aR = 0.f, bL = 0.f, bR = 0.f; // a = lower-grit path, b = higher-grit path
			for (int vi = 0; vi < MAX_SLOTS; vi++) {
				Voice& vc = v[vi];
				if (!vc.l[0].amp.active() && !vc.l[1].amp.active()) continue;
				if (p.mono && p.glide > 0.f) vc.note += (vc.target - vc.note) * (1.f - std::exp(-1.f / (p.glide * fs4)));
				vc.age += 1.f / fs4;
				float vfade = p.vibDelay > 0.f ? clampf(vc.age / p.vibDelay, 0.f, 1.f) : 1.f;
				float cents = (p.vibRate > 0.f ? p.vibCents * vib * vfade : 0.f) + 30.f * m.mod * vib;
				float velN = vc.vel / 127.f;
				float velLoud = 1.f - p.vel * (1.f - velN);
				float velA = 1.f - 0.5f * p.vel * (1.f - velN);
				float keyA = 1.f - p.dcwkey * clampf((vc.note - 48.f) / 48.f, 0.f, 1.f);
				int nl = p.mode == 0 ? 1 : 2;
				float cleanOut[2] = {0, 0}, q8Out[2] = {0, 0}, q7Out[2] = {0, 0};
				for (int li = 0; li < nl; li++) {
					Line& ln = vc.l[li];
					const PdLine& pl = li == 0 ? p.l1 : p.l2;
					// pitch increment once per host sample (vibrato and glide are slow)
					if (k == 0) {
						float noteL = vc.note + (li == 1 ? detSemis : 0.f) + cents / 100.f;
						ln.dt = 440.0 * std::exp2((noteL - 69.f) / 12.0) / fs4;
					}
					ln.ph += ln.dt;
					if (ln.ph >= 1.0) { ln.ph -= std::floor(ln.ph); ln.parity ^= 1; }
					float a = clampf(ln.dcw.next() * keyA * velA + 0.35f * m.tone, 0.f, 1.f);
					int w = pl.nw == 2 ? pl.w[ln.parity] : pl.w[0];
					float wv = pdWave(w, ln.ph, a);
					float amp = ln.amp.next() * velLoud * trem;
					cleanOut[li] = wv * amp;
					if (grit > 0.f) {
						float aq = ampQuant(amp);
						q8Out[li] = qbits(wv, 127.f) * aq;
						if (grit > 0.5f) q7Out[li] = qbits(wv, 63.f) * aq;
					}
				}
				auto combine = [&](const float* o) {
					if (nl == 1) return o[0];
					if (p.ring) return o[0] * o[1] * p.lvl2;
					return o[0] + p.lvl2 * o[1];
				};
				float c = combine(cleanOut), q8 = combine(q8Out), q7 = combine(q7Out);
				float lo, hi;
				if (grit <= 0.5f) { lo = c + (q8 - c) * (grit * 2.f); hi = 0.f; }
				else { lo = q8 * (1.f - (grit - 0.5f) * 2.f); hi = q7 * ((grit - 0.5f) * 2.f); }
				aL += lo * gl[vi]; aR += lo * gr[vi];
				bL += hi * gl[vi]; bR += hi * gr[vi];
			}
			// 20 kHz zero-order hold on the grittiest path, box-integrated
			if (grit > 0.5f) {
				holdAcc += holdInc;
				if (holdAcc >= 1.f) {
					holdAcc -= 1.f;
					float f = holdAcc / holdInc; // part of this sample after the update
					float nl = holdL, nr = holdR;
					holdL = bL; holdR = bR;
					bL = nl * (1.f - f) + holdL * f; bR = nr * (1.f - f) + holdR * f;
				}
				else { bL = holdL; bR = holdR; }
			}
			os[k][0] = aL + bL; os[k][1] = aR + bR;
		}
		float l1 = dL1.process(os[0][0], os[1][0]), l2 = dL1.process(os[2][0], os[3][0]);
		float r1 = dR1.process(os[0][1], os[1][1]), r2 = dR1.process(os[2][1], os[3][1]);
		L[i] += dL2.process(l1, l2);
		R[i] += dR2.process(r1, r2);
	}
}

} // namespace hk
