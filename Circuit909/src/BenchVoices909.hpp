// 909 Circuit Bench: the eleven voices of the kit behind one interface, and the
// circuit-diagram layout the screen draws for each. Rack-free (see test/bench.cpp).
//
// Every voice is one of the 909 Circuits engines (Engines909.hpp) with its knob positions
// k[] (0..1) and a stock position each, exactly as on the single modules, so a kit sounds
// the same as the separate modules. The toms' RANGE and the cymbals' TYPE are fixed per
// voice (LT/MT/HT, CH/OH = HAT, CR = CRASH, RD = RIDE) and not shown on the screen.
#pragma once
#include "Engines909.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace bench909 {

using namespace c909;

static const int NV = 11, MAXK = 16, PAN = 100;   // PAN: a bench-only parameter (mix position)
enum VoiceId { BD, SD, LT, MT, HT, RS, CP, CH, OH, CR, RD };
enum Kind { K_KICK, K_SNARE, K_TOM, K_RIM, K_CLAP, K_CH, K_CYM, NKINDS };

static inline const char* code(int v) {
	static const char* c[NV] = {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH", "CR", "RD"};
	return c[v];
}
static inline const char* longName(int v) {
	static const char* n[NV] = {"BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HI TOM", "RIM SHOT", "HAND CLAP",
		"CLOSED HAT", "OPEN HAT", "CRASH", "RIDE"};
	return n[v];
}
// button colours, in groups along the kit (909 orange to cream)
static inline uint32_t colour(int v) {
	static const uint32_t c[NV] = {0xE5631F, 0xE5631F, 0xEE8A3C, 0xEE8A3C, 0xEE8A3C, 0xF2B04A, 0xF2B04A,
		0xF4F1EA, 0xF4F1EA, 0xF4F1EA, 0xF4F1EA};
	return c[v];
}
static inline int kindOf(int v) {
	static const int k[NV] = {K_KICK, K_SNARE, K_TOM, K_TOM, K_TOM, K_RIM, K_CLAP, K_CH, K_CYM, K_CYM, K_CYM};
	return k[v];
}
// the parameter a voice holds fixed (and hides), or -1
static inline int fixedParam(int v) {
	int k = kindOf(v);
	if (k == K_TOM) return tom::RANGE;
	if (k == K_CH || k == K_CYM) return cymbals::TYPE;
	return -1;
}
// the screen hides parameters that do nothing on this voice (CH has no open DECAY,
// OH/CR/RD have no CH DEC); a hidden parameter still loads and saves
static inline bool hidden(int v, int p) {
	if (p == fixedParam(v)) return true;
	if (kindOf(v) == K_CH && p == cymbals::DECAY) return true;
	if (kindOf(v) == K_CYM && p == cymbals::CHDECAY) return true;
	return false;
}

// value as the screen shows it (a short string)
static inline void format(char* buf, int n, int v, const KnobDef& d, float k) {
	if (d.kind != 0) {   // switch: position names, '|' separated
		int want = (int)std::round(k), i = 0;
		const char* s = d.unit;
		while (i < want && *s) { if (*s == '|') i++; s++; }
		int len = 0; while (s[len] && s[len] != '|') len++;
		snprintf(buf, n, "%.*s", len, s);
		return;
	}
	float x = knobValue(d, k);
	// cymbal DECAY is a multiple of the type's own length: show the length itself
	if ((kindOf(v) == K_CYM) && !std::strcmp(d.id, "DECAY")) {
		x *= CymbalsEngine::natural(v == OH ? 0 : v == CR ? 1 : 2);
		snprintf(buf, n, "%.2fs", x);
		return;
	}
	if (d.disp == DISP_PCT) { snprintf(buf, n, "%.0f%%", 100.f * x); return; }
	const char* u = d.unit;
	while (*u == ' ') u++;
	float a = std::fabs(x);
	if (!std::strcmp(u, "Hz") && a >= 1000.f) { snprintf(buf, n, "%.1fk", x / 1000.f); return; }
	if (!std::strcmp(u, "ms") && a >= 1000.f) { snprintf(buf, n, "%.2fs", x / 1000.f); return; }
	if (d.disp == DISP_INT) snprintf(buf, n, "%.0f%s", x, u);
	else if (a >= 99.95f) snprintf(buf, n, "%.0f%s", x, u);
	else if (a >= 9.995f) snprintf(buf, n, "%.1f%s", x, u);
	else snprintf(buf, n, "%.2f%s", x, u);
}

// ---------------------------------------------------------------- the circuit diagrams
// A block sits in a column (0..4) and row (0 top, 1 bottom, 0.5 middle) of the screen and
// holds up to four parameters (indices into k[], or PAN). Arrows join blocks.
struct Block { const char* name; float col, row; int n; int p[4]; };
struct Arrow { int from, to; };
struct Layout { int nBlocks; const Block* blocks; int nArrows; const Arrow* arrows; };

static inline const Layout& layout(int kind) {
	static const Block kb[] = {
		{"TRIG", 0, 0.5f, 2, {kick::ACCENT, kick::ATTACK, 0, 0}},
		{"PITCH ENV", 1, 0, 3, {kick::TUNE, kick::SWEEP, kick::BEND, 0}},
		{"CLICK", 1, 1, 4, {kick::PULSE, kick::NOISE, kick::NOISELP, kick::SNAP}},
		{"VCO + SHAPER", 2, 0, 2, {kick::BASE, kick::SHAPE, 0, 0}},
		{"VCA", 3, 0.5f, 1, {kick::DECAY, 0, 0, 0}},
		{"OUT", 4, 0.5f, 4, {kick::DRIVE, kick::LEVEL, kick::AGE, PAN}},
	};
	static const Arrow ka[] = {{0, 1}, {0, 2}, {1, 3}, {3, 4}, {2, 4}, {4, 5}};
	static const Block sb[] = {
		{"TRIG", 0, 0, 1, {snare::ACCENT, 0, 0, 0}},
		{"PITCH ENV", 0, 1, 2, {snare::SWEEP, snare::BEND, 0, 0}},
		{"2 VCOs", 1, 0, 4, {snare::TUNE, snare::RATIO, snare::BALANCE, snare::SHAPE}},
		{"NOISE", 1, 1, 2, {snare::NOISECOL, snare::TONE, 0, 0}},
		{"VCA", 2, 0, 1, {snare::DECAY, 0, 0, 0}},
		{"SNAPPY VCA", 2, 1, 2, {snare::SNAPPY, snare::SNAPDECAY, 0, 0}},
		{"OUT", 4, 0.5f, 4, {snare::DRIVE, snare::LEVEL, snare::AGE, PAN}},
	};
	static const Arrow sa[] = {{0, 2}, {1, 2}, {2, 4}, {3, 5}, {4, 6}, {5, 6}};
	static const Block tb[] = {
		{"TRIG", 0, 0.5f, 1, {tom::ACCENT, 0, 0, 0}},
		{"PITCH ENV", 1, 0, 2, {tom::SWEEP, tom::BEND, 0, 0}},
		{"NOISE", 2, 1, 1, {tom::NOISE, 0, 0, 0}},
		{"VCO + SHAPER", 2, 0, 3, {tom::TUNE, tom::SHAPE, tom::DOUBLE, 0}},
		{"VCA + LOW-PASS", 3, 0.5f, 3, {tom::DECAY, tom::MUTE, tom::TONE, 0}},
		{"OUT", 4, 0.5f, 4, {tom::DRIVE, tom::LEVEL, tom::AGE, PAN}},
	};
	static const Arrow ta[] = {{0, 1}, {0, 2}, {1, 3}, {3, 4}, {2, 4}, {4, 5}};
	static const Block rb[] = {
		{"TRIG", 0, 0.5f, 2, {rim::ACCENT, rim::STRIKE, 0, 0}},
		{"3 RESONATORS", 1, 0.5f, 3, {rim::TUNE, rim::RATIO, rim::RING, 0}},
		{"VCA", 2, 0.5f, 1, {rim::GATE, 0, 0, 0}},
		{"HIGH-PASS", 3, 0.5f, 1, {rim::SNAP, 0, 0, 0}},
		{"OUT", 4, 0.5f, 4, {rim::DRIVE, rim::LEVEL, rim::AGE, PAN}},
	};
	static const Arrow ra[] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}};
	static const Block pb[] = {
		{"TRIG", 0, 0.5f, 1, {clap::ACCENT, 0, 0, 0}},
		{"BURSTS", 1, 0, 3, {clap::BURSTS, clap::SPREAD, clap::SNAP, 0}},
		{"TAIL", 1, 1, 2, {clap::TAIL, clap::MIX, 0, 0}},
		{"BAND-PASS", 2, 0.5f, 2, {clap::BAND, clap::RES, 0, 0}},
		{"LOW-PASS", 3, 0.5f, 1, {clap::TONE, 0, 0, 0}},
		{"OUT", 4, 0.5f, 4, {clap::DRIVE, clap::LEVEL, clap::AGE, PAN}},
	};
	static const Arrow pa[] = {{0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {4, 5}};
	// cymbals: the source ("ROM"), the clocked 6-bit DAC with its reconstruction filter,
	// the VCA that does the fading, the output high-pass
	static const Block hb[] = {
		{"SOURCE", 0, 0.5f, 2, {cymbals::METAL, cymbals::NOISE, 0, 0}},
		{"TRIG", 1, 1, 1, {cymbals::ACCENT, 0, 0, 0}},
		{"6-BIT DAC", 1, 0, 3, {cymbals::TUNE, cymbals::BITS, cymbals::RECON, 0}},
		{"VCA", 2, 0.5f, 1, {cymbals::CHDECAY, 0, 0, 0}},
		{"HIGH-PASS", 3, 0.5f, 1, {cymbals::CUT, 0, 0, 0}},
		{"OUT", 4, 0.5f, 4, {cymbals::DRIVE, cymbals::LEVEL, cymbals::AGE, PAN}},
	};
	static const Block yb[] = {
		{"SOURCE", 0, 0.5f, 2, {cymbals::METAL, cymbals::NOISE, 0, 0}},
		{"TRIG", 1, 1, 1, {cymbals::ACCENT, 0, 0, 0}},
		{"6-BIT DAC", 1, 0, 3, {cymbals::TUNE, cymbals::BITS, cymbals::RECON, 0}},
		{"VCA", 2, 0.5f, 1, {cymbals::DECAY, 0, 0, 0}},
		{"HIGH-PASS", 3, 0.5f, 1, {cymbals::CUT, 0, 0, 0}},
		{"OUT", 4, 0.5f, 4, {cymbals::DRIVE, cymbals::LEVEL, cymbals::AGE, PAN}},
	};
	static const Arrow ya[] = {{0, 2}, {2, 3}, {1, 3}, {3, 4}, {4, 5}};
#define L(b, a) {(int)(sizeof(b) / sizeof(b[0])), b, (int)(sizeof(a) / sizeof(a[0])), a}
	static const Layout lays[NKINDS] = {L(kb, ka), L(sb, sa), L(tb, ta), L(rb, ra), L(pb, pa), L(hb, ya), L(yb, ya)};
#undef L
	return lays[kind];
}

// ---------------------------------------------------------------- the voices
struct Voice {
	int id = 0, kind = 0, fixIdx = -1;
	float fixVal = 0.f;
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
	float stockOf(int i) const { return i == PAN ? 0.5f : def(i).stock; }
	float get(int i) const { return i == PAN ? pan : k[i]; }
	void fix() { if (fixIdx >= 0) k[fixIdx] = fixVal; }
	void set(int i, float v) {
		if (!std::isfinite(v)) return;
		v = std::min(std::max(v, 0.f), 1.f);
		if (i == PAN) pan = v;
		else if (i >= 0 && i < nK()) k[i] = v;
		fix();
	}
	void toStock() { for (int i = 0; i < nK(); i++) k[i] = def(i).stock; pan = 0.5f; fix(); }
};

template <class E>
struct CircuitVoice : Voice {
	E e;
	int nK() const override { return E::spec().nKnobs; }
	const KnobDef& def(int i) const override { return E::spec().knobs[i]; }
	void setSampleRate(float fs) override { e.setSampleRate(fs); }
	void setSerial(uint32_t s) override { e.age.setSerial(s); }
	void hit(float acc) override { e.hit(acc, k); }
	void process() override { if (e.sleeping()) { out = 0.f; e.env = 0.f; return; } e.process(k, 0.f); out = e.out; }
	bool sleeping() const override { return e.sleeping(); }
	void reset() override { e.reset(); }
};

// CH plays the hat closed, OH open; CR and RD are open by their TYPE
struct CymbalVoice : CircuitVoice<CymbalsEngine> {
	bool openHat = false;
	void hit(float acc) override { if (openHat) e.hit2(acc, k); else e.hit(acc, k); }
	void choke() override { e.choke(); }
};

// a kit: one voice per id
struct Kit {
	Voice* v[NV] = {};
	Kit() {
		v[BD] = new CircuitVoice<KickEngine>;
		v[SD] = new CircuitVoice<SnareEngine>;
		for (int t = 0; t < 3; t++) v[LT + t] = new CircuitVoice<TomEngine>;
		v[RS] = new CircuitVoice<RimEngine>;
		v[CP] = new CircuitVoice<ClapEngine>;
		for (int c = 0; c < 4; c++) { CymbalVoice* cv = new CymbalVoice; cv->openHat = (CH + c) == OH; v[CH + c] = cv; }
		for (int i = 0; i < NV; i++) {
			v[i]->id = i; v[i]->kind = kindOf(i);
			v[i]->fixIdx = fixedParam(i);
			v[i]->fixVal = kindOf(i) == K_TOM ? (float)(i - LT) : i == CR ? 1.f : i == RD ? 2.f : 0.f;
			v[i]->toStock();
		}
	}
	~Kit() { for (auto* x : v) delete x; }
	Kit(const Kit&) = delete;
	Kit& operator=(const Kit&) = delete;
	void setSampleRate(float fs) { for (auto* x : v) x->setSampleRate(fs); }
	void setSerial(uint32_t s) { for (int i = 0; i < NV; i++) v[i]->setSerial(s * 2654435761u + 977u * (i + 1)); }
	CymbalsEngine& cymbal(int i) { return static_cast<CymbalVoice*>(v[i])->e; }
	// the 909's closed hat chokes the open hat
	void hit(int i, float acc) { if (i == CH) v[OH]->choke(); v[i]->hit(acc); }
};

// ---------------------------------------------------------------- the 16 knob slots
// Every shown parameter of a voice on one knob, in signal-flow order (blocks by column,
// then row). The hidden ones (RANGE, TYPE, the other hat's decay) are in no block.
static const int NSLOT = 16;
struct Slots { int n; int p[NSLOT]; int blk[NSLOT]; };
static inline const Slots& slots(int kind) {
	static const std::vector<Slots> all = [] {
		std::vector<Slots> out;
		for (int kd = 0; kd < NKINDS; kd++) {
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
struct RollRng {
	uint32_t s = 0x909909u;
	float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
};
// LEVEL, AGE (as on the single modules), PAN and the hidden parameters are never rolled.
static inline bool rollable(const Voice* v, int p) { return p != PAN && !v->def(p).norand && !hidden(v->id, p); }
// wild: any value, equally likely. Nudge: values near `home` (the loaded kit setting, or
// stock) are the most likely: home + a bell-shaped offset (sd ~0.12, at most 0.36 of the
// knob's travel), folded back at the ends (as the 808 Circuit Bench).
// home: MAXK knob positions + pan, as Bench909 keeps its snapshot.
static inline void roll(Voice* v, const float* home, bool wild, RollRng& r) {
	for (int p = 0; p < v->nK(); p++) {
		if (!rollable(v, p)) continue;
		float k;
		if (wild) k = r.uni();
		else {
			k = home[p] + 0.24f * (r.uni() + r.uni() + r.uni() - 1.5f);
			if (k < 0.f) k = -k;
			if (k > 1.f) k = 2.f - k;
		}
		v->set(p, k);
	}
}

// ---------------------------------------------------------------- factory kits
// Each kit starts from the stock 909 and changes knob positions (0..1):
// "VOICES: ID=value ID=value; ..." where VOICES is one or more codes, or ALL.
struct FactoryKit { const char* name; const char* text; };
static inline const std::vector<FactoryKit>& factoryKits() {
	static const std::vector<FactoryKit> k = {
		{"Stock 909", ""},
		{"Chicago Jack",
			"BD: DECAY=0.42 ATTACK=0.62 DRIVE=0.12;"
			"SD: TONE=0.5 SNAPPY=0.62 SNAPDECAY=0.45;"
			"CP: TAIL=0.46 PAN=0.45; RS: PAN=0.4;"
			"CH: CHDECAY=0.44 PAN=0.6; OH: DECAY=0.5 PAN=0.6; RD: PAN=0.4"},
		{"Detroit (deep, long kick)",
			"BD: TUNE=0.62 DECAY=0.66 BASE=0.36 ATTACK=0.4;"
			"SD: SNAPPY=0.4 TONE=0.45; CP: TAIL=0.6 MIX=0.62;"
			"LT MT HT: DECAY=0.64; RS: RING=0.45 PAN=0.38;"
			"CH: CHDECAY=0.4 PAN=0.62; OH: DECAY=0.56 PAN=0.62; RD: DECAY=0.66 PAN=0.38"},
		{"Hard Techno (driven)",
			"BD: DECAY=0.48 ATTACK=0.78 DRIVE=0.62 SHAPE=0.35;"
			"SD: DRIVE=0.5 SNAPPY=0.7; CP: DRIVE=0.45 BURSTS=0.57;"
			"RS: DRIVE=0.5; LT MT HT: DRIVE=0.35;"
			"CH: DRIVE=0.4 CHDECAY=0.38; OH: DRIVE=0.4; CR: DRIVE=0.4; RD: DRIVE=0.4"},
		{"Gabber Kick",
			"BD: TUNE=0.62 DECAY=0.58 SWEEP=0.62 DRIVE=0.95 SHAPE=0.55 ATTACK=0.85 BASE=0.5;"
			"SD: DRIVE=0.7; CP: DRIVE=0.6"},
		{"Deep House (soft, round)",
			"BD: ATTACK=0.3 PULSE=0.35 DECAY=0.46 SHAPE=0.05;"
			"SD: TONE=0.4 SNAPPY=0.38 BALANCE=0.4;"
			"CP: TONE=0.6 TAIL=0.6 MIX=0.6; RS: SNAP=0.5 PAN=0.4;"
			"LT MT HT: TONE=0.6 DECAY=0.6;"
			"CH: RECON=0.55 CHDECAY=0.46 PAN=0.6; OH: RECON=0.55 DECAY=0.52 PAN=0.6; RD: RECON=0.55 PAN=0.4"},
		{"Garage Swing",
			"BD: DECAY=0.4 ATTACK=0.58; SD: TUNE=0.55 SNAPPY=0.55 SNAPDECAY=0.4;"
			"RS: TUNE=0.55 RING=0.45 PAN=0.42; CP: SPREAD=0.38 TAIL=0.42;"
			"CH: CHDECAY=0.36 PAN=0.62; OH: DECAY=0.42 PAN=0.62"},
		{"Crunchy 4-bit",
			"CH OH CR RD: BITS=0.2 RECON=0.85 TUNE=0.45;"
			"BD: DRIVE=0.25; SD: DRIVE=0.25"},
		{"Tuned Toms",
			"LT: TUNE=0.42 DECAY=0.7 PAN=0.34; MT: TUNE=0.5 DECAY=0.68; HT: TUNE=0.58 DECAY=0.66 PAN=0.66;"
			"LT MT HT: SWEEP=0.12 NOISE=0.15 DOUBLE=0.25"},
		{"Worn Unit (aged, drifting)",
			"ALL: AGE=0.72"},
		{"Circuit Bent",
			"BD: SWEEP=0.95 BEND=0.8 SHAPE=0.9 SNAP=0.85;"
			"SD: RATIO=0.85 SWEEP=0.6 SHAPE=0.8 NOISECOL=0.9;"
			"LT MT HT: BEND=0.85 SWEEP=0.8 DOUBLE=0.8;"
			"RS: RATIO=0.9 RING=0.8; CP: BURSTS=1 SPREAD=0.8 RES=0.8;"
			"CH OH CR RD: BITS=0.05 TUNE=0.25 METAL=0.9 NOISE=0.1"},
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

}  // namespace bench909
