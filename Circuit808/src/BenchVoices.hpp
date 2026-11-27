// 808 Circuit Bench: the thirteen voices of the kit behind one interface, the circuit
// stages each voice's parameters are grouped in, the 16 knob slots they fill on the panel,
// the RAND rolls and the factory kits. Rack-free (see test/bench.cpp).
//
// BD = the 808 Kick engine; SD, LT/MT/HT, RS, CL, CP, MA, CB = the 808 Circuits voice
// engines; CY, OH, CH = the 808 Metal engine (src/metal/MetalEngine.hpp, GPL-3).
// Every voice keeps its settings as knob positions k[] (0..1) with a stock position each,
// exactly as on the single modules, so a kit sounds the same as the separate modules.
#pragma once
#include "VoiceEngines.hpp"
#include "KickEngine.hpp"
#include "metal/MetalEngine.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

namespace bench {

using c808::KnobDef;
using c808::DISP_PCT; using c808::DISP_EXP; using c808::DISP_LIN; using c808::DISP_INT; using c808::DISP_SW;

static const int NV = 13, MAXK = 16, PAN = 100;   // PAN: a bench-only parameter (mix position)
enum VoiceId { BD, SD, LT, MT, HT, RS, CL, CP, MA, CB, CY, OH, CH };
enum Kind { K_KICK, K_SNARE, K_TOM, K_RIM, K_CLAVE, K_CLAP, K_MARACAS, K_COWBELL, K_METAL };

static inline const char* code(int v) {
	static const char* c[NV] = {"BD", "SD", "LT", "MT", "HT", "RS", "CL", "CP", "MA", "CB", "CY", "OH", "CH"};
	return c[v];
}
static inline const char* longName(int v) {
	static const char* n[NV] = {"BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HI TOM", "RIM SHOT", "CLAVES",
		"HAND CLAP", "MARACAS", "COWBELL", "CYMBAL", "OPEN HAT", "CLOSED HAT"};
	return n[v];
}
// the 808's step-button colours, in groups along the kit
static inline uint32_t colour(int v) {
	static const uint32_t c[NV] = {0xE0533D, 0xE0533D, 0xE0533D, 0xE0533D, 0xF08A24, 0xF08A24, 0xF08A24, 0xF08A24,
		0xE8C547, 0xE8C547, 0xE8C547, 0xECE6D8, 0xECE6D8};
	return c[v];
}
static inline int kindOf(int v) {
	static const int k[NV] = {K_KICK, K_SNARE, K_TOM, K_TOM, K_TOM, K_RIM, K_CLAVE, K_CLAP, K_MARACAS, K_COWBELL, K_METAL, K_METAL, K_METAL};
	return k[v];
}

// ---------------------------------------------------------------- knob tables
// kick: same maps as the 808 Kick module (KickEngine.hpp)
namespace kick {
enum { TUNE, DECAY, TONE, ACCENT, LEVEL, AGE, SWEEP, BEND, STRIKE, CLICK, RETRIG, SIGH, TENSION, SHELF, DRIVE, N };
static inline const KnobDef* defs() {
	static const KnobDef d[N] = {
		{"TUNE", "TUNE", "", 0.37786f, DISP_EXP, 20.f, 220.f, " Hz", 0, 0, 0, false},
		{"DECAY", "DECAY", "", 0.45f, DISP_PCT, 0.f, 1.f, "", 0, 0, 0, false},
		{"TONE", "TONE", "", 0.5f, DISP_EXP, 150.f, 10000.f, " Hz", 0, 0, 0, false},
		{"ACCENT", "ACCENT", "", 0.6f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"LEVEL", "LEVEL", "", 1.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, true},
		{"AGE", "AGE", "", 0.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, true},
		{"SWEEP", "SWEEP", "", 0.45f, DISP_LIN, 0.f, 2.5f, " oct", 0, 0, 0, false},
		{"BEND", "BEND", "", 0.4376f, DISP_EXP, 1.f, 60.f, " ms", 0, 0, 0, false},
		{"STRIKE", "STRIKE", "", 0.5f, DISP_EXP, 0.1f, 10.f, " ms", 0, 0, 0, false},
		{"CLICK", "CLICK", "", 0.5f, DISP_LIN, 0.f, 2.f, "x", 0, 0, 0, false},
		{"RETRIG", "RETRIG", "", 0.2f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"SIGH", "SIGH", "", 0.25f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"TENSION", "TENSION", "", 0.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"SHELF", "SHELF", "", 0.33f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"DRIVE", "DRIVE", "", 0.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
	};
	return d;
}
}  // namespace kick

// metal: same ranges as the 808 Metal module, as knob positions
namespace metal808 {
enum { TUNE, TRIMA, TRIMB, SPREAD, WIDTH, DRIFT, COLOUR, RES, BAND, CUT, DECAY, TONE, DRIVE, LEVEL, ACCENT, N };
static inline const KnobDef* defs() {
	static const KnobDef d[N] = {
		{"TUNE", "TUNE", "", 0.5f, DISP_LIN, -24.f, 24.f, " st", 0, 0, 0, false},
		{"TRIMA", "TRIM A", "", 0.55737f, DISP_LIN, 359.4f, 1149.9f, " Hz", 0, 0, 0, false},
		{"TRIMB", "TRIM B", "", 0.76616f, DISP_LIN, 254.3f, 627.2f, " Hz", 0, 0, 0, false},
		{"SPREAD", "SPREAD", "", 0.5f, DISP_LIN, -1.f, 1.f, "", 0, 0, 0, false},
		{"WIDTH", "WIDTH", "", 0.47756f, DISP_LIN, 5.f, 95.f, "%", 0, 0, 0, false},
		{"DRIFT", "DRIFT", "", 0.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"COLOUR", "COLOUR", "", 0.5f, DISP_LIN, -1.f, 1.f, " oct", 0, 0, 0, false},
		{"RES", "RES", "", 0.5f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"BAND", "BAND", "", 0.5f, DISP_LIN, -1.f, 1.f, "", 0, 0, 0, false},
		{"CUT", "CUT", "", 0.66667f, DISP_LIN, -2.f, 1.f, " oct", 0, 0, 0, false},
		{"DECAY", "DECAY", "", 0.5f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"TONE", "TONE", "", 0.5f, DISP_LIN, -1.f, 1.f, "", 0, 0, 0, false},
		{"DRIVE", "DRIVE", "", 0.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
		{"LEVEL", "LEVEL", "", 1.f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, true},
		{"ACCENT", "ACCENT", "", 0.6f, DISP_PCT, 0.f, 100.f, "%", 0, 0, 0, false},
	};
	return d;
}
}  // namespace metal808

// value as the screen shows it (a short string)
static inline void format(char* buf, int n, int kind, const KnobDef& d, float k) {
	if (kind == K_KICK && !std::strcmp(d.id, "DECAY")) {
		if (k > 0.95f) snprintf(buf, n, "SUSTAIN");
		else snprintf(buf, n, "%.2fs", kick808::decayT60(k));
		return;
	}
	if (d.kind != 0) {   // switch: position names, bottom first, '|' separated
		int nPos = d.kind == 1 ? 3 : 2, want = (int)std::round(k * (nPos - 1)), i = 0;
		const char* s = d.unit;
		while (i < want && *s) { if (*s == '|') i++; s++; }
		int len = 0; while (s[len] && s[len] != '|') len++;
		snprintf(buf, n, "%.*s", len, s);
		return;
	}
	float v = c808::knobValue(d, k);
	if (d.disp == DISP_PCT) { snprintf(buf, n, "%.0f%%", (d.hi > 1.5f ? 1.f : 100.f) * v * (d.hi > 1.5f ? 100.f : 1.f)); return; }
	const char* u = d.unit;
	while (*u == ' ') u++;
	float a = std::fabs(v);
	if (!std::strcmp(u, "Hz") && a >= 1000.f) { snprintf(buf, n, "%.2gk", v / 1000.f); return; }
	if (!std::strcmp(u, "ms") && a >= 1000.f) { snprintf(buf, n, "%.2fs", v / 1000.f); return; }
	if (d.disp == DISP_INT) snprintf(buf, n, "%.0f%s", v, u);
	else if (a >= 99.95f) snprintf(buf, n, "%.0f%s", v, u);
	else if (a >= 9.995f) snprintf(buf, n, "%.1f%s", v, u);
	else snprintf(buf, n, "%.2f%s", v, u);
}

// ---------------------------------------------------------------- the circuit stages
// A block is one stage of the voice's circuit and holds up to four parameters (indices into
// k[], or PAN). col/row give the stage's place in the signal flow (row 0 top, 1 bottom,
// 0.5 middle); the knob slots follow that order. Arrows join blocks (signal flow).
struct Block { const char* name; float col, row; int n; int p[4]; };
struct Arrow { int from, to; };
struct Layout { int nBlocks; const Block* blocks; int nArrows; const Arrow* arrows; };

static inline const Layout& layout(int kind) {
	using namespace c808;
	static const Block kb[] = {
		{"TRIG", 0, 0.5f, 3, {kick::ACCENT, kick::STRIKE, kick::RETRIG, 0}},
		{"Q43 JUMP", 1, 0, 3, {kick::SWEEP, kick::BEND, kick::SIGH, 0}},
		{"BRIDGED-T", 2, 0.5f, 4, {kick::TUNE, kick::DECAY, kick::SHELF, kick::TENSION}},
		{"TONE", 3, 0, 2, {kick::TONE, kick::CLICK, 0, 0}},
		{"OUT", 4, 0.5f, 4, {kick::DRIVE, kick::LEVEL, kick::AGE, PAN}},
	};
	static const Arrow ka[] = {{0, 2}, {1, 2}, {2, 3}, {3, 4}};
	static const Block sb[] = {
		{"TRIG", 0, 0, 3, {snare::ACCENT, snare::STRIKE, snare::SWEEP, 0}},
		{"2 SHELLS", 1, 0, 4, {snare::TUNE, snare::RATIO, snare::DECAY, snare::TONE}},
		{"NOISE", 1, 1, 2, {snare::NOISECOL, snare::HISS, 0, 0}},
		{"VCA", 2, 1, 2, {snare::SNAPPY, snare::SNAPDECAY, 0, 0}},
		{"OUT", 4, 0.5f, 4, {snare::DRIVE, snare::LEVEL, snare::AGE, PAN}},
	};
	static const Arrow sa[] = {{0, 1}, {0, 2}, {2, 3}, {1, 4}, {3, 4}};
	static const Block tb[] = {
		{"TRIG", 0, 0, 2, {tom::ACCENT, tom::STRIKE, 0, 0}},
		{"BRIDGED-T", 1, 0, 4, {tom::TUNE, tom::DECAY, tom::BEND, tom::MODE}},
		{"HEAD", 2, 0, 2, {tom::MUTE, tom::DOUBLE, 0, 0}},
		{"PINK NOISE", 1, 1, 1, {tom::SLAP, 0, 0, 0}},
		{"TONE", 3, 0.5f, 1, {tom::TONE, 0, 0, 0}},
		{"OUT", 4, 0.5f, 4, {tom::DRIVE, tom::LEVEL, tom::AGE, PAN}},
	};
	static const Arrow ta[] = {{0, 1}, {0, 3}, {1, 2}, {2, 4}, {3, 4}, {4, 5}};
	static const Block rb[] = {
		{"TRIG", 0, 0.5f, 2, {rim::ACCENT, rim::STRIKE, 0, 0}},
		{"2x BRIDGED-T", 1, 0.5f, 3, {rim::TUNE, rim::RATIO, rim::RING, 0}},
		{"VCA", 2, 0.5f, 2, {rim::GATE, rim::DRIVE, 0, 0}},
		{"HIGH-PASS", 3, 0.5f, 1, {rim::SNAP, 0, 0, 0}},
		{"OUT", 4, 0.5f, 3, {rim::LEVEL, rim::AGE, PAN, 0}},
	};
	static const Arrow ra[] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}};
	static const Block cb_[] = {
		{"TRIG", 0, 0.5f, 2, {clave::ACCENT, clave::STRIKE, 0, 0}},
		{"BRIDGED-T", 1, 0, 3, {clave::TUNE, clave::RING, clave::BEND, 0}},
		{"OVERTONE", 1, 1, 2, {clave::OVERTONE, clave::RATIO, 0, 0}},
		{"OUT", 3, 0.5f, 4, {clave::DRIVE, clave::LEVEL, clave::AGE, PAN}},
	};
	static const Arrow ca[] = {{0, 1}, {0, 2}, {1, 3}, {2, 3}};
	static const Block pb[] = {
		{"TRIG", 0, 0.5f, 1, {clap::ACCENT, 0, 0, 0}},
		{"BURSTS", 1, 0, 3, {clap::BURSTS, clap::SPREAD, clap::SNAP, 0}},
		{"TAIL", 1, 1, 2, {clap::TAIL, clap::MIX, 0, 0}},
		{"NOISE VCA", 2, 0.5f, 3, {clap::BAND, clap::RES, clap::TONE, 0}},
		{"OUT", 4, 0.5f, 4, {clap::DRIVE, clap::LEVEL, clap::AGE, PAN}},
	};
	static const Arrow pa[] = {{0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}};
	static const Block mb[] = {
		{"TRIG", 0, 0, 2, {maracas::ACCENT, maracas::SHAKE, 0, 0}},
		{"NOISE", 0, 1, 1, {maracas::GRAINS, 0, 0, 0}},
		{"HIGH-PASS", 1, 1, 1, {maracas::CUT, 0, 0, 0}},
		{"VCA", 2, 0.5f, 2, {maracas::ATTACK, maracas::LENGTH, 0, 0}},
		{"OUT", 4, 0.5f, 4, {maracas::DRIVE, maracas::LEVEL, maracas::AGE, PAN}},
	};
	static const Arrow ma[] = {{0, 3}, {1, 2}, {2, 3}, {3, 4}};
	static const Block wb[] = {
		{"2 SQUARES", 0, 1, 3, {cowbell::TUNE, cowbell::INTERVAL, cowbell::WIDTH, 0}},
		{"TRIG", 0, 0, 1, {cowbell::ACCENT, 0, 0, 0}},
		{"VCA", 1, 0.5f, 2, {cowbell::SHAPE, cowbell::DECAY, 0, 0}},
		{"BAND-PASS", 2, 0.5f, 2, {cowbell::FILTER, cowbell::RES, 0, 0}},
		{"OUT", 4, 0.5f, 4, {cowbell::DRIVE, cowbell::LEVEL, cowbell::AGE, PAN}},
	};
	static const Arrow wa[] = {{0, 2}, {1, 2}, {2, 3}, {3, 4}};
	static const Block xb[] = {
		{"6 SQUARES", 0, 0, 4, {metal808::TUNE, metal808::TRIMA, metal808::TRIMB, metal808::SPREAD}},
		{"SHAPE", 0, 1, 2, {metal808::WIDTH, metal808::DRIFT, 0, 0}},
		{"2 BAND-PASS", 1, 0.5f, 3, {metal808::COLOUR, metal808::RES, metal808::BAND, 0}},
		{"TRIG", 2, 0, 1, {metal808::ACCENT, 0, 0, 0}},
		{"3 VCAs", 2, 1, 3, {metal808::DECAY, metal808::TONE, metal808::DRIVE, 0}},
		{"HIGH-PASS", 3, 0.5f, 1, {metal808::CUT, 0, 0, 0}},
		{"OUT", 4, 0.5f, 2, {metal808::LEVEL, PAN, 0, 0}},
	};
	static const Arrow xa[] = {{0, 2}, {1, 2}, {2, 4}, {3, 4}, {4, 5}, {5, 6}};
#define L(b, a) {(int)(sizeof(b) / sizeof(b[0])), b, (int)(sizeof(a) / sizeof(a[0])), a}
	static const Layout lays[] = {L(kb, ka), L(sb, sa), L(tb, ta), L(rb, ra), L(cb_, ca), L(pb, pa), L(mb, ma), L(wb, wa), L(xb, xa)};
#undef L
	return lays[kind];
}

// ---------------------------------------------------------------- the voices
struct Voice {
	int id = 0, kind = 0;
	float k[MAXK] = {};
	float pan = 0.5f;
	float out = 0.f;
	virtual ~Voice() {}
	virtual int nK() const = 0;
	virtual const KnobDef& def(int i) const = 0;
	virtual void setSampleRate(float fs) = 0;
	virtual void setSerial(uint32_t s) = 0;
	virtual void hit(float acc) = 0;          // acc 0..1, as the ACCENT input
	virtual void process() = 0;               // fills out
	virtual bool sleeping() const = 0;
	virtual void choke() {}
	virtual void reset() = 0;
	// fixed parameters (the tom's RANGE) are held whatever is loaded
	virtual void fix() {}
	float stockOf(int i) const { return i == PAN ? 0.5f : def(i).stock; }
	float get(int i) const { return i == PAN ? pan : k[i]; }
	void set(int i, float v) { v = std::min(std::max(v, 0.f), 1.f); if (i == PAN) pan = v; else if (i >= 0 && i < nK()) k[i] = v; fix(); }
	void toStock() { for (int i = 0; i < nK(); i++) k[i] = def(i).stock; pan = 0.5f; fix(); }
};

template <class E>
struct CircuitVoice : Voice {
	E e;
	int fixIdx = -1;
	float fixVal = 0.f;
	int nK() const override { return E::spec().nKnobs; }
	const KnobDef& def(int i) const override { return E::spec().knobs[i]; }
	void setSampleRate(float fs) override { e.setSampleRate(fs); }
	void setSerial(uint32_t s) override { e.age.setSerial(s); }
	void hit(float acc) override { e.hit(acc, k); }
	void process() override { if (e.sleeping()) { out = 0.f; return; } e.process(k, 0.f); out = e.out; }
	bool sleeping() const override { return e.sleeping(); }
	void reset() override { e.reset(); }
	void fix() override { if (fixIdx >= 0) k[fixIdx] = fixVal; }
};

struct KickVoice : Voice {
	kick808::Engine e;
	kick808::Controls c;
	int nK() const override { return kick::N; }
	const KnobDef& def(int i) const override { return kick::defs()[i]; }
	void setSampleRate(float fs) override { e.setSampleRate(fs); }
	void setSerial(uint32_t s) override { e.age.setSerial(s); }
	void controls() {
		c.tuneHz = c808::knobValue(def(kick::TUNE), k[kick::TUNE]);
		c.decay = k[kick::DECAY]; c.tone = k[kick::TONE]; c.accent = k[kick::ACCENT];
		c.level = k[kick::LEVEL]; c.age = k[kick::AGE]; c.sweep = k[kick::SWEEP]; c.bend = k[kick::BEND];
		c.strike = k[kick::STRIKE]; c.click = k[kick::CLICK]; c.retrig = k[kick::RETRIG]; c.sigh = k[kick::SIGH];
		c.tension = k[kick::TENSION]; c.shelf = k[kick::SHELF]; c.drive = k[kick::DRIVE];
	}
	void hit(float acc) override { controls(); e.hit(acc, c); }
	void process() override { if (e.sleeping()) { out = 0.f; return; } controls(); e.process(c); out = e.out; }
	bool sleeping() const override { return e.sleeping(); }
	void reset() override { e.reset(); }
};

struct MetalVoice : Voice {
	metal::Engine e;
	metal::Controls c;
	int mode = metal::MODE_CY;
	int nK() const override { return metal808::N; }
	const KnobDef& def(int i) const override { return metal808::defs()[i]; }
	void setSampleRate(float fs) override { e.setSampleRate(fs); }
	void setSerial(uint32_t) override {}
	void controls() {
		using namespace metal808;
		auto V = [&](int i) { return c808::knobValue(def(i), k[i]); };
		c.tune = V(TUNE) / 12.f; c.trimA = V(TRIMA); c.trimB = V(TRIMB); c.spread = V(SPREAD);
		c.pw = V(WIDTH) / 100.f; c.drift = k[DRIFT]; c.colour = V(COLOUR); c.res = k[RES]; c.band = V(BAND);
		c.cut = V(CUT); c.decay = k[DECAY]; c.mode = mode; c.tone = V(TONE); c.drive = k[DRIVE]; c.level = k[LEVEL];
	}
	void hit(float acc) override {
		// same law as the other voices, scaled to 808 Metal's own accent (+3 dB at 10 V)
		float law = c808::accentLaw(c808::triggerVolts(k[metal808::ACCENT], acc));
		e.trigger(1.f + 0.43f * (law - 1.f) / 0.78f);
	}
	bool quiet() const { return !e.attacking && e.env[0] < 1e-5f && e.env[1] < 1e-5f && e.env[2] < 1e-5f; }
	int idle = 0;
	float fs = 48000.f;
	void process() override {
		if (idle > (int)(0.1f * fs)) { out = 0.f; if (!quiet()) idle = 0; else return; }
		controls();
		out = e.process(c);
		idle = quiet() ? idle + 1 : 0;
	}
	bool sleeping() const override { return idle > (int)(0.1f * fs); }
	void choke() override { e.choke(); }
	void reset() override { for (int i = 0; i < 3; i++) e.env[i] = 0.f; e.attacking = false; }
};

// a kit: one voice per id
struct Kit {
	Voice* v[NV] = {};
	Kit() {
		v[BD] = new KickVoice;
		v[SD] = new CircuitVoice<c808::SnareEngine>;
		for (int t = 0; t < 3; t++) {
			auto* tv = new CircuitVoice<c808::TomEngine>;
			tv->fixIdx = c808::tom::RANGE; tv->fixVal = (float)t;
			v[LT + t] = tv;
		}
		v[RS] = new CircuitVoice<c808::RimEngine>;
		v[CL] = new CircuitVoice<c808::ClaveEngine>;
		v[CP] = new CircuitVoice<c808::ClapEngine>;
		v[MA] = new CircuitVoice<c808::MaracasEngine>;
		v[CB] = new CircuitVoice<c808::CowbellEngine>;
		const int modes[3] = {metal::MODE_CY, metal::MODE_OH, metal::MODE_CH};
		for (int m = 0; m < 3; m++) { auto* mv = new MetalVoice; mv->mode = modes[m]; v[CY + m] = mv; }
		for (int i = 0; i < NV; i++) { v[i]->id = i; v[i]->kind = kindOf(i); v[i]->toStock(); }
		// 808 Metal's own stock DECAY sits mid-range for every mode
	}
	~Kit() { for (auto* x : v) delete x; }
	void setSampleRate(float fs) {
		for (auto* x : v) x->setSampleRate(fs);
		for (int m = 0; m < 3; m++) static_cast<MetalVoice*>(v[CY + m])->fs = fs;
	}
	void setSerial(uint32_t s) { for (int i = 0; i < NV; i++) v[i]->setSerial(s * 2654435761u + 977u * (i + 1)); }
	// the 808's closed hat chokes the open hat
	void hit(int i, float acc) { if (i == CH) v[OH]->choke(); v[i]->hit(acc); }
};

// ---------------------------------------------------------------- the 16 knob slots
// Every parameter of a voice on one knob, in signal-flow order (blocks by column, then row).
static const int NSLOT = 16;
struct Slots { int n; int p[NSLOT]; int blk[NSLOT]; };
static inline const Slots& slots(int kind) {
	static const std::vector<Slots> all = [] {
		std::vector<Slots> out;
		for (int kd = 0; kd <= K_METAL; kd++) {
			const Layout& L = layout(kd);
			std::vector<int> order;
			for (int b = 0; b < L.nBlocks; b++) order.push_back(b);
			std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
				const Block &A = L.blocks[a], &B = L.blocks[b];
				return A.col != B.col ? A.col < B.col : A.row < B.row;
			});
			Slots s; s.n = 0;
			for (int b : order)
				for (int j = 0; j < L.blocks[b].n && s.n < NSLOT; j++) { s.p[s.n] = L.blocks[b].p[j]; s.blk[s.n] = b; s.n++; }
			out.push_back(s);
		}
		return out;
	}();
	return all[kind];
}

// ---------------------------------------------------------------- RAND
// A small generator so the rolls are reproducible in the tests.
struct Rng {
	uint32_t s = 0x808808u;
	float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
};
// LEVEL and PAN are never rolled (a roll must not make a voice loud, silent or move it).
static inline bool rollable(const Voice* v, int p) { return p != PAN && std::strcmp(v->def(p).id, "LEVEL") != 0; }
// wild: any value, equally likely. Nudge: values near `home` (the loaded kit setting, or
// stock) are the most likely: home + a bell-shaped offset (sd ~0.12, at most 0.36 of the
// knob's travel), folded back at the ends. Switches stay put on a nudge 4 times in 5.
// home: MAXK knob positions + pan, as Bench808 keeps its snapshot.
static inline void roll(Voice* v, const float* home, bool wild, Rng& r) {
	for (int p = 0; p < v->nK(); p++) {
		if (!rollable(v, p)) continue;
		const KnobDef& d = v->def(p);
		float k;
		if (d.kind != 0) {
			int nPos = d.kind == 1 ? 3 : 2;
			int pos = (int)std::round(home[p] * (nPos - 1));
			if (wild || r.uni() < 0.2f) pos = std::min(nPos - 1, (int)(r.uni() * nPos));
			k = (float)pos / (nPos - 1);
		}
		else if (wild) k = r.uni();
		else {
			k = home[p] + 0.24f * (r.uni() + r.uni() + r.uni() - 1.5f);
			if (k < 0.f) k = -k;
			if (k > 1.f) k = 2.f - k;
		}
		v->set(p, k);
	}
}

// ---------------------------------------------------------------- factory kits
// Each kit starts from the stock 808 and changes knob positions (0..1):
// "VOICES: ID=value ID=value; ..." where VOICES is one or more codes, or ALL.
struct FactoryKit { const char* name; const char* text; };
static inline const std::vector<FactoryKit>& factoryKits() {
	static const std::vector<FactoryKit> k = {
		{"Stock 808", ""},
		{"Boom (long tuned kick)",
			"BD: TUNE=0.30 DECAY=0.97 SWEEP=0.32 TONE=0.36 SIGH=0.45 DRIVE=0.15 LEVEL=0.92;"
			"SD: TUNE=0.42 DECAY=0.56 SNAPPY=0.42;"
			"LT MT HT: DECAY=0.72 BEND=0.12;"
			"CP: TAIL=0.56; CH: DECAY=0.38; OH: DECAY=0.6; CY: DECAY=0.62"},
		{"Miami Bass",
			"BD: TUNE=0.33 DECAY=0.99 SWEEP=0.58 TONE=0.6 CLICK=0.72 DRIVE=0.25 LEVEL=0.9;"
			"SD: TONE=0.35 SNAPPY=0.7 SNAPDECAY=0.45 TUNE=0.55;"
			"LT MT HT: TUNE=0.72 DECAY=0.75 BEND=0.25;"
			"CB: DECAY=0.66; CL: TUNE=0.76; CP: SPREAD=0.4 TAIL=0.4;"
			"CH: DECAY=0.3 PAN=0.6; OH: PAN=0.6"},
		{"Electro",
			"BD: TUNE=0.41 DECAY=0.55 SWEEP=0.62 CLICK=0.82 STRIKE=0.4;"
			"SD: SNAPPY=0.76 SNAPDECAY=0.36 DECAY=0.3 TONE=0.4 HISS=0.66;"
			"CH: DECAY=0.2 TONE=0.62; OH: DECAY=0.36; CP: SNAP=0.3 TAIL=0.36;"
			"RS: RING=0.36; CB: DECAY=0.4 PAN=0.38; CL: PAN=0.64"},
		{"Acid House",
			"BD: TUNE=0.36 DECAY=0.4 TONE=0.66 CLICK=0.66 DRIVE=0.2;"
			"SD: TONE=0.6 SNAPPY=0.6; CP: TAIL=0.56 MIX=0.62;"
			"CH: DECAY=0.3 TONE=0.66 CUT=0.75 PAN=0.58; OH: DECAY=0.46 PAN=0.42;"
			"RS: RING=0.5 PAN=0.4; MA: LENGTH=0.4 PAN=0.62"},
		{"Deep House (soft, round)",
			"BD: TUNE=0.35 DECAY=0.55 TONE=0.3 CLICK=0.3 STRIKE=0.66;"
			"SD: TONE=0.7 SNAPPY=0.35 HISS=0.35 STRIKE=0.66;"
			"CP: TONE=0.5 TAIL=0.62; RS: STRIKE=0.66 SNAP=0.45 PAN=0.4; CL: STRIKE=0.66 PAN=0.62;"
			"LT MT HT: DECAY=0.62 TONE=0.5; MA: CUT=0.6 PAN=0.64;"
			"CH: TONE=0.3 DECAY=0.36 PAN=0.6; OH: TONE=0.36 DECAY=0.5 PAN=0.42; CY: TONE=0.3"},
		{"Driven Techno",
			"BD: TUNE=0.4 DECAY=0.5 DRIVE=0.7 SHELF=0.5 TENSION=0.3;"
			"SD: DRIVE=0.6 SNAPPY=0.66; CP: DRIVE=0.5; RS: DRIVE=0.6; CB: DRIVE=0.5;"
			"LT MT HT: DRIVE=0.4; CH: DRIVE=0.5 DECAY=0.3; OH: DRIVE=0.5; CY: DRIVE=0.5 CUT=0.75"},
		{"Tuned Congas",
			"LT MT HT: MODE=1 DECAY=0.42 SLAP=0.56;"
			"LT: PAN=0.36; HT: PAN=0.64;"
			"BD: TUNE=0.45 DECAY=0.35; CL: TUNE=0.6 RING=0.45 PAN=0.6;"
			"RS: RING=0.56 PAN=0.42; CB: TUNE=0.4; MA: LENGTH=0.36 SHAKE=0.3 PAN=0.66"},
		{"Worn Unit (aged, drifting)",
			"ALL: AGE=0.72; BD: TENSION=0.2;"
			"CY OH CH: DRIFT=0.6 SPREAD=0.62"},
		{"Circuit Bent",
			"BD: SWEEP=0.95 BEND=0.8 RETRIG=0.8 SIGH=0.9 TENSION=0.7 CLICK=1;"
			"SD: RATIO=0.85 SWEEP=0.6 NOISECOL=0.9;"
			"LT MT HT: BEND=0.8 DOUBLE=0.8;"
			"RS: RATIO=0.9 RING=0.8; CL: BEND=0.7 OVERTONE=0.8 RATIO=0.8;"
			"CP: BURSTS=1 SPREAD=0.8 RES=0.8; MA: GRAINS=0.8 SHAKE=0.8; CB: INTERVAL=0.9 RES=0.8;"
			"CY OH CH: TRIMA=0.2 TRIMB=0.1 SPREAD=0.9 WIDTH=0.85 COLOUR=0.8 RES=0.8"},
	};
	return k;
}
static inline int paramByIdString(const Voice* v, const std::string& id) {
	if (id == "PAN") return PAN;
	for (int i = 0; i < v->nK(); i++) if (id == v->def(i).id) return i;
	return -1;
}
// the whole kit to stock, then the kit's changes (unknown ids are skipped)
static inline void applyKit(Kit& kit, const char* text) {
	for (auto* v : kit.v) v->toStock();
	std::string s(text);
	size_t pos = 0;
	while (pos < s.size()) {
		size_t end = s.find(';', pos);
		if (end == std::string::npos) end = s.size();
		std::string part = s.substr(pos, end - pos);
		pos = end + 1;
		size_t colon = part.find(':');
		if (colon == std::string::npos) continue;
		std::vector<std::string> who, sets;
		auto split = [](const std::string& x, std::vector<std::string>& out) {
			size_t i = 0;
			while (i < x.size()) {
				while (i < x.size() && x[i] == ' ') i++;
				size_t j = i;
				while (j < x.size() && x[j] != ' ') j++;
				if (j > i) out.push_back(x.substr(i, j - i));
				i = j;
			}
		};
		split(part.substr(0, colon), who);
		split(part.substr(colon + 1), sets);
		for (int vi = 0; vi < NV; vi++) {
			bool mine = false;
			for (auto& w : who) if (w == "ALL" || w == code(vi)) mine = true;
			if (!mine) continue;
			for (auto& a : sets) {
				size_t eq = a.find('=');
				if (eq == std::string::npos) continue;
				int p = paramByIdString(kit.v[vi], a.substr(0, eq));
				if (p >= 0) kit.v[vi]->set(p, (float)std::atof(a.c_str() + eq + 1));
			}
		}
	}
}

}  // namespace bench
