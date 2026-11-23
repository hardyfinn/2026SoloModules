#include "FmEngine.hpp"
#include "../generated/Sounds.hpp"
#include "../third_party/ymfm/ymfm_opm.h"
#include "../third_party/ymfm/ymfm_opz.h"
// the chip classes are templates defined in the .ipp; our subclasses call clock()/output()
// directly, so instantiate them here as well
#include "../third_party/ymfm/ymfm_fm.ipp"

namespace hk {

static constexpr double FM_CLOCK = 3579545.0;

// ---------------------------------------------------------------- voice data

void crsToMulDt2(int crs, int& mul, int& dt2, float* ratio) {
	struct P { float r; int m, d; };
	static P tab[64];
	static bool built = false;
	if (!built) {
		const float dt2f[4] = {1.f, 1.414f, 1.570f, 1.731f};
		int k = 0;
		for (int m = 0; m < 16; m++)
			for (int d = 0; d < 4; d++)
				tab[k++] = {(m == 0 ? 0.5f : (float) m) * dt2f[d], m, d};
		std::stable_sort(tab, tab + 64, [](const P& a, const P& b) { return a.r < b.r; });
		built = true;
	}
	crs = std::max(0, std::min(63, crs));
	mul = tab[crs].m; dt2 = tab[crs].d;
	if (ratio) *ratio = tab[crs].r;
}

FmVoiceData decodeVmem(const uint8_t* v, bool opz) {
	FmVoiceData d{};
	for (int k = 0; k < 4; k++) {
		const uint8_t* o = v + 10 * k;
		auto& p = d.op[k];
		p.ar = o[0] & 31; p.d1r = o[1] & 31; p.d2r = o[2] & 31; p.rr = o[3] & 15; p.d1l = o[4] & 15;
		p.ls = std::min<int>(o[5], 99);
		p.ame = (o[6] >> 6) & 1; p.ebs = (o[6] >> 3) & 7; p.kvs = o[6] & 7;
		p.out = std::min<int>(o[7], 99); p.crs = o[8] & 63;
		p.rs = (o[9] >> 3) & 3; p.det = std::min(o[9] & 7, 6);
		const uint8_t* e = v + 73 + 2 * k;
		p.egs = opz ? (e[0] >> 4) & 3 : 0;
		p.fix = opz ? (e[0] >> 3) & 1 : 0;
		p.fixrg = opz ? e[0] & 7 : 0;
		p.osw = opz ? (e[1] >> 4) & 7 : 0;
		p.fine = opz ? e[1] & 15 : 0;
	}
	d.sync = (v[40] >> 6) & 1; d.fb = (v[40] >> 3) & 7; d.alg = v[40] & 7;
	d.lfs = std::min<int>(v[41], 99); d.lfd = std::min<int>(v[42], 99);
	d.pmd = std::min<int>(v[43], 99); d.amd = std::min<int>(v[44], 99);
	d.pms = (v[45] >> 4) & 7; d.ams = (v[45] >> 2) & 3; d.lfw = v[45] & 3;
	d.trps = std::min<int>(v[46], 48);
	d.mwPitch = std::min<int>(v[51], 99); d.mwAmp = std::min<int>(v[52], 99);
	d.opz = opz;
	for (int i = 0; i < 10; i++) { char c = (char) v[57 + i]; d.name[i] = (c >= 32 && c < 127) ? c : ' '; }
	d.name[10] = 0;
	return d;
}

int parseVmemBank(const std::vector<uint8_t>& f, uint8_t out[32][128]) {
	for (size_t i = 0; i + 6 + 4096 <= f.size(); i++) {
		if (f[i] == 0xF0 && f[i + 1] == 0x43 && (f[i + 2] & 0xF0) == 0x00 && f[i + 3] == 0x04 && f[i + 4] == 0x20 && f[i + 5] == 0x00) {
			for (int v = 0; v < 32; v++)
				for (int b = 0; b < 128; b++) out[v][b] = f[i + 6 + v * 128 + b] & 0x7F;
			return 32;
		}
	}
	return 0;
}

static int outToTl(int out) {
	static const int low[20] = {127, 122, 118, 114, 110, 107, 104, 102, 100, 98, 96, 94, 92, 90, 88, 86, 85, 84, 82, 81};
	return out >= 20 ? 99 - out : low[std::max(0, out)];
}

// carriers by algorithm, in register slot order M1 M2 C1 C2
static bool isCarrier(int alg, int slot) {
	switch (alg) {
		case 4: return slot == 2 || slot == 3;
		case 5: case 6: return slot != 0;
		case 7: return true;
		default: return slot == 3;
	}
}

// ---------------------------------------------------------------- chip wrappers

struct Intf : ymfm::ymfm_interface {};

struct Opp : ymfm::ym2164 {
	explicit Opp(ymfm::ymfm_interface& i) : ymfm::ym2164(i) {}
	int32_t gen() {
		m_fm.clock(fm_engine::ALL_CHANNELS);
		output_data o;
		m_fm.output(o.clear(), 0, 32767, fm_engine::ALL_CHANNELS);
		return o.data[0];
	}
};

struct Opz : ymfm::ym2414 {
	explicit Opz(ymfm::ymfm_interface& i) : ymfm::ym2414(i) {}
	int32_t gen() {
		m_fm.clock(fm_engine::ALL_CHANNELS);
		output_data o;
		m_fm.output(o.clear(), 0, 32767, fm_engine::ALL_CHANNELS);
		return o.data[1];
	}
};

// written-register cache so macro updates only write what changed
struct FmUnit {
	Intf intfA, intfB;
	Opp opp{intfA};
	Opz opz{intfB};
	bool isOpz = false;
	int16_t cache[0x200];
	// per channel state
	bool gate[8] = {};
	float vel[8] = {};
	float note[8] = {};
	float detune = 0.f; // semitones added to every channel
	float pan = 0.f;
	int idleSamples = 1 << 30;
	float lfoRamp = 1.f, lfoRampStep = 1.f;

	void w(int reg, int val) {
		if (cache[reg] == val) return;
		cache[reg] = (int16_t) val;
		if (isOpz) { opz.write(0, (uint8_t) reg); opz.write(1, (uint8_t) val); }
		else { opp.write(0, (uint8_t) reg); opp.write(1, (uint8_t) val); }
	}
	void wForce(int reg, int val) { cache[reg] = (int16_t) val; if (isOpz) { opz.write(0, (uint8_t) reg); opz.write(1, (uint8_t) val); } else { opp.write(0, (uint8_t) reg); opp.write(1, (uint8_t) val); } }
	int32_t gen() { return isOpz ? opz.gen() : opp.gen(); }

	void reset(bool z) {
		isOpz = z;
		opp.reset(); opz.reset();
		for (auto& c : cache) c = -1;
		for (int i = 0; i < 8; i++) gate[i] = false;
		idleSamples = 1 << 30;
	}
};

// ---------------------------------------------------------------- engine

FmEngine::FmEngine() {
	for (auto& u : units) u.reset(new FmUnit());
	voice = decodeVmem(FM_SOUNDS[0].vmem, FM_SOUNDS[0].opz);
}
FmEngine::~FmEngine() {}

void FmEngine::init(float sampleRate) {
	fs = sampleRate;
	chipFs = (float) (FM_CLOCK / 64.0);
	rsL.init(chipFs, fs); rsR.init(chipFs, fs);
	lpL.lowpass(16000.f, 0.7071f, chipFs); lpR = lpL;
	hpL.setLP(12.f, chipFs); hpR = hpL;
	noise.seed(77);
	setVoice(FM_SOUNDS[0].vmem, FM_SOUNDS[0].opz);
}

void FmEngine::setSound(int idx) {
	idx = std::max(0, std::min(29, idx));
	setVoice(FM_SOUNDS[idx].vmem, FM_SOUNDS[idx].opz);
}

// Writes every per-channel and per-operator register of the voice (the DX100's CPU
// does this on a voice change). Velocity, level scaling and macros come later.
static void writeVoiceStatic(FmUnit& u, const FmVoiceData& v, int ch) {
	int fbAlg = (v.fb << 3) | v.alg;
	if (u.isOpz) u.wForce(0x20 + ch, 0x80 | fbAlg);
	else u.wForce(0x20 + ch, 0xC0 | fbAlg);
	u.wForce(0x38 + ch, (v.pms << 4) | v.ams);
	for (int k = 0; k < 4; k++) {
		const auto& o = v.op[k];
		int r = ch + 8 * k;
		int mul, dt2; crsToMulDt2(o.crs, mul, dt2);
		static const int detMap[7] = {7, 6, 5, 0, 1, 2, 3};
		u.wForce(0x40 + r, (detMap[o.det] << 4) | mul);
		if (u.isOpz) u.wForce(0x40 + r, 0x80 | (o.osw << 4) | o.fine);
		u.wForce(0x60 + r, 127);
		u.wForce(0x80 + r, (o.rs << 6) | o.ar);
		u.wForce(0xA0 + r, (o.ame << 7) | o.d1r);
		u.wForce(0xC0 + r, (dt2 << 6) | o.d2r);
		if (u.isOpz) u.wForce(0xC0 + r, 0x20 | (o.egs << 6));
		u.wForce(0xE0 + r, ((15 - o.d1l) << 4) | std::max(1, o.rr));
	}
}

void FmEngine::setVoice(const uint8_t* vmem, bool opz) {
	if (vmem != curVmem) std::memcpy(curVmem, vmem, 128);
	curOpz = opz;
	voice = decodeVmem(vmem, opz);
	for (auto& up : units) {
		FmUnit& u = *up;
		u.reset(opz);
		u.wForce(0x01, 0x00);
		u.wForce(0x0F, 0x00);
		u.wForce(0x14, 0x00);
		u.wForce(0x18, (int) std::lround(voice.lfs * 255.0 / 99.0));
		u.wForce(0x1B, voice.lfw | (opz ? (voice.sync << 4) : 0));
		u.wForce(0x19, 0x00);
		u.wForce(0x19, 0x80);
		for (int ch = 0; ch < 8; ch++) writeVoiceStatic(u, voice, ch);
	}
	lastMacros = Macros();
	lastMacros.grit = -1.f;
	macroCounter = 0;
}

static void writePitch(FmUnit& u, const FmVoiceData& v, int ch, float note) {
	static const int code[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};
	double k = note + u.detune + (v.trps - 24) - 13.0;
	if (k < 0) k = 0;
	int ki = (int) std::floor(k);
	int kf = (int) std::floor((k - ki) * 64.0);
	int oct = ki / 12;
	if (oct > 7) { oct = 7; ki = 7 * 12 + 11; kf = 63; }
	u.w(0x28 + ch, (oct << 4) | code[ki % 12]);
	u.w(0x30 + ch, kf << 2);
}

// per-note levels and macro-dependent rates
static void writeDynamic(FmUnit& u, const FmVoiceData& v, int ch, const Macros& m) {
	float toneOff = m.tone > 0.f ? -24.f * m.tone : -32.f * m.tone;
	int arOff = (int) std::lround(-6.f * m.attack);
	int drOff = (int) std::lround(-8.f * m.decay);
	int rrOff = (int) std::lround(-4.f * m.decay);
	float vel = u.vel[ch];
	int note = (int) std::lround(u.note[ch]);
	for (int k = 0; k < 4; k++) {
		const auto& o = v.op[k];
		int r = ch + 8 * k;
		bool car = isCarrier(v.alg, k);
		float tl = (float) outToTl(o.out);
		tl += std::round(o.kvs * (127.f - vel) / 18.f);
		tl += std::round((o.ls / 99.f) * std::max(0, note - 24) * (8.f / 12.f));
		if (!car) tl += std::round(toneOff);
		int tli = (int) clampf(tl, 0.f, 127.f);
		u.w(0x60 + r, tli);
		int ar = o.ar;
		if (car && ar > 0) ar = std::max(1, std::min(31, ar + arOff));
		u.w(0x80 + r, (o.rs << 6) | ar);
		int d1r = o.d1r > 0 ? std::max(1, std::min(31, o.d1r + drOff)) : 0;
		u.w(0xA0 + r, (o.ame << 7) | d1r);
		int mul, dt2; crsToMulDt2(o.crs, mul, dt2);
		int d2r = o.d2r > 0 ? std::max(1, std::min(31, o.d2r + drOff)) : 0;
		u.w(0xC0 + r, (dt2 << 6) | d2r);
		int rr = std::max(1, std::min(15, std::max(1, o.rr) + rrOff));
		int e0 = ((15 - o.d1l) << 4) | rr;
		if (u.isOpz && u.cache[0xE0 + r] != e0) {
			// keep the chip's preset copy of E0 in step (see ymfm_opz.cpp)
			u.wForce(0x40 + r, 0x80 | (o.osw << 4) | o.fine);
			u.wForce(0xE0 + r, e0);
		}
		else u.w(0xE0 + r, e0);
	}
}

static void keyOn(FmUnit& u, int ch, bool on) {
	if (u.isOpz) {
		u.wForce(0x08, ch);
		int v = u.cache[0x20 + ch] & ~0x40;
		u.wForce(0x20 + ch, v | (on ? 0x40 : 0));
	}
	else u.wForce(0x08, (on ? 0x78 : 0x00) | ch);
}

void FmEngine::noteOn(int slot, float note, float vel, float pan) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	int ui = slot / 8, ch = slot % 8;
	// slots 0..15 -> units A1 A2, 16..31 -> B1 B2
	FmUnit& u = *units[ui];
	slotNote[slot] = note; slotPan[slot] = pan; u.pan = pan;
	if (u.gate[ch]) keyOn(u, ch, false);
	u.gate[ch] = true;
	u.vel[ch] = vel;
	u.note[ch] = note;
	u.idleSamples = 0;
	writePitch(u, voice, ch, note);
	writeDynamic(u, voice, ch, lastMacros);
	// LFO: sync resets it; the delay restarts from this note
	if (voice.sync && !u.isOpz) { u.wForce(0x01, 0x02); u.wForce(0x01, 0x00); }
	float delay = (voice.lfd / 99.f) * (voice.lfd / 99.f) * 6.f;
	u.lfoRamp = delay > 0.f ? 0.f : 1.f;
	u.lfoRampStep = delay > 0.f ? 16.f / (delay * fs) : 1.f;
	keyOn(u, ch, true);
}

void FmEngine::setPitch(int slot, float note) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	slotNote[slot] = note;
	FmUnit& u = *units[slot / 8];
	u.note[slot % 8] = note;
	writePitch(u, voice, slot % 8, note);
}

void FmEngine::setPan(int slot, float pan) { if (slot >= 0 && slot < MAX_SLOTS) { slotPan[slot] = pan; units[slot / 8]->pan = pan; } }

void FmEngine::noteOff(int slot) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	FmUnit& u = *units[slot / 8];
	int ch = slot % 8;
	if (!u.gate[ch]) return;
	u.gate[ch] = false;
	keyOn(u, ch, false);
}

void FmEngine::allOff() {
	setVoice(curVmem, curOpz);
}

bool FmEngine::slotActive(int slot) const {
	const FmUnit& u = *units[slot / 8];
	return u.gate[slot % 8] || u.idleSamples < (int) (4 * chipFs);
}

// floating-point DAC: 3-bit exponent, mantissa of `bits` bits (10 on the real chip)
static inline int32_t dacRound(int32_t value, int bits) {
	if (value < -32768) value = -32768;
	if (value > 32767) value = 32767;
	int32_t scan = value ^ (value >> 31);
	int lz = 0;
	uint32_t s = (uint32_t) scan << 17;
	while (lz < 15 && !(s & 0x80000000u)) { s <<= 1; lz++; }
	int exponent = std::max(7 - lz, 1) - 1;
	int shift = exponent + (10 - bits);
	int32_t mask = (1 << shift) - 1;
	return value & ~mask;
}

void FmEngine::chipSample(float& l, float& r, float grit) {
	float sumL = 0.f, sumR = 0.f;
	const float scale = 1.f / 16384.f;
	for (int ui = 0; ui < 4; ui++) {
		FmUnit& u = *units[ui];
		bool needed = (ui == 0) || (ui == 1 && polyLimit16) || (ui >= 2 && twinOn && (ui == 2 || polyLimit16));
		bool busy = false;
		for (int c = 0; c < 8; c++) busy |= u.gate[c];
		if (!busy) u.idleSamples++;
		if (!needed || u.idleSamples > (int) (4 * chipFs)) continue;
		int32_t raw = u.gen();
		float y;
		if (grit <= 0.5f) {
			float a = raw * scale, b = dacRound(raw, 10) * scale;
			y = a + (b - a) * (grit * 2.f);
		}
		else {
			float a = dacRound(raw, 10) * scale, b = dacRound(raw, 8) * scale;
			y = a + (b - a) * ((grit - 0.5f) * 2.f);
		}
		// pan: A units left, B units right by 0.6 * TWIN (set via slotPan of channel 0)
		float gl, gr; panGains(u.pan, gl, gr);
		sumL += y * gl; sumR += y * gr;
	}
	if (grit > 0.5f) {
		float n = noise.next() * dbToGain(-62.f) * ((grit - 0.5f) * 2.f);
		sumL += n; sumR += n;
	}
	l = hpL.hp(lpL.process(sumL));
	r = hpR.hp(lpR.process(sumR));
}

void FmEngine::process(float* L, float* R, int n, const Macros& m) {
	float grit = gritOverride >= 0.f ? gritOverride : m.grit;
	for (int i = 0; i < n; i++) {
		if (macroCounter-- <= 0) {
			macroCounter = 15;
			bool changed = m.tone != lastMacros.tone || m.attack != lastMacros.attack || m.decay != lastMacros.decay;
			for (int ui = 0; ui < 4; ui++) {
				FmUnit& u = *units[ui];
				if (u.idleSamples > (int) (4 * chipFs)) continue;
				if (changed)
					for (int ch = 0; ch < 8; ch++) if (u.gate[ch]) writeDynamic(u, voice, ch, m);
				// LFO depths (with the CPU's delay ramp and the mod wheel)
				if (u.lfoRamp < 1.f) u.lfoRamp = std::min(1.f, u.lfoRamp + u.lfoRampStep);
				int pmd = (int) std::lround(127.0 / 99.0 * (voice.pmd + m.mod * voice.mwPitch) * u.lfoRamp);
				int amd = (int) std::lround(127.0 / 99.0 * (voice.amd + m.mod * voice.mwAmp) * u.lfoRamp);
				int pmdv = 0x80 | std::min(127, pmd), amdv = std::min(127, amd);
				if (u.cache[0x100] != pmdv) { u.cache[0x100] = (int16_t) pmdv; u.wForce(0x19, pmdv); }
				if (u.cache[0x101] != amdv) { u.cache[0x101] = (int16_t) amdv; u.wForce(0x19, amdv); }
			}
			lastMacros = m;
		}
		while (rsL.needInput()) {
			float l, r;
			chipSample(l, r, grit);
			rsL.push(l); rsR.push(r);
		}
		L[i] += rsL.pull();
		R[i] += rsR.pull();
	}
}

} // namespace hk
