// Peal ringing engine: rows, place notation, methods, calls, open handstroke gap.
// Plain C++ with no Rack code, so test/render.cpp can check it offline.
//
// Words used here (the guide explains them):
//   row     one order of the bells, every bell strikes once ("a change")
//   place   a position in the row, 1 = first to strike
//   place notation  which places stay put between two rows; every other bell
//           swaps with its neighbour. "x" means everybody swaps.
//   lead    the repeating block of changes a method is built from
//   call    BOB or SINGLE: swaps one change at a lead end for another so the
//           bells go a different way (that's how a conductor steers a peal)
#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace peal {

static const int MAXB = 12;
static const char* BELL_CHARS = "1234567890ET";

enum Method { HUNT, PLAIN_BOB, LITTLE_BOB, GRANDSIRE, STEDMAN, CAMBRIDGE, WANDER, NUM_METHODS };
static const char* METHOD_NAMES[NUM_METHODS] = {
	"PLAIN HUNT", "PLAIN BOB", "LITTLE BOB", "GRANDSIRE", "STEDMAN", "CAMBRIDGE S", "WANDER"};
static const char* STAGE_NAMES[13] = {
	"", "", "", "SINGLES", "MINIMUS", "DOUBLES", "MINOR", "TRIPLES", "MAJOR", "CATERS", "ROYAL", "CINQUES", "MAXIMUS"};

inline int placeChar(char c) {
	if (c >= '1' && c <= '9') return c - '1';
	if (c == '0') return 9;
	if (c == 'E' || c == 'e') return 10;
	if (c == 'T' || c == 't') return 11;
	return -1;
}

// Place notation text -> list of changes. Each change is a bit mask of the
// places that stay (0 = "x", all swap). "a,b" is the usual shorthand for a
// lead that runs a forwards, then a backwards without its last change, then b.
inline std::vector<uint16_t> parseNotation(const char* s) {
	std::vector<uint16_t> a, b;
	std::vector<uint16_t>* cur = &a;
	bool comma = false, have = false;
	uint16_t acc = 0;
	auto flush = [&]() { if (have) cur->push_back(acc); acc = 0; have = false; };
	for (const char* p = s; *p; p++) {
		char c = *p;
		if (c == 'x' || c == 'X' || c == '-') { flush(); cur->push_back(0); }
		else if (c == '.') flush();
		else if (c == ',') { flush(); comma = true; cur = &b; }
		else { int k = placeChar(c); if (k >= 0) { acc |= (uint16_t)(1 << k); have = true; } }
	}
	flush();
	if (!comma) return a;
	std::vector<uint16_t> out = a;
	for (int i = (int) a.size() - 2; i >= 0; i--) out.push_back(a[i]);
	for (uint16_t v : b) out.push_back(v);
	return out;
}

inline uint16_t one(const char* s) {
	std::vector<uint16_t> v = parseNotation(s);
	return v.empty() ? 0 : v[0];
}

// Apply one change to the first w places of a row.
inline void applyChange(uint8_t* row, int w, uint16_t places) {
	int i = 0;
	while (i < w) {
		if (places & (1 << i)) { i++; continue; }
		if (i + 1 < w && !(places & (1 << (i + 1)))) {
			uint8_t t = row[i]; row[i] = row[i + 1]; row[i + 1] = t;
			i += 2;
		}
		else i++;
	}
}

struct CallSlot {
	int at = 0;          // change index in the lead where the call starts
	int len = 1;         // how many changes it replaces
	uint16_t bob[2] = {0, 0};
	uint16_t single[2] = {0, 0};
};

struct Lead {
	std::vector<uint16_t> ch;
	std::vector<CallSlot> slots;
};

inline char pc(int place) { return BELL_CHARS[place - 1]; }  // 1-based place -> char

// Build the lead of a method on w working bells.
inline Lead buildLead(int m, int w) {
	Lead L;
	char W = pc(w);
	char buf[64];
	bool even = (w % 2) == 0;
	auto huntLead = [&]() {
		std::vector<uint16_t> v;
		for (int i = 0; i < w; i++) {
			if (even) { v.push_back(0); snprintf(buf, sizeof buf, "1%c", W); v.push_back(one(buf)); }
			else { snprintf(buf, sizeof buf, "%c", W); v.push_back(one(buf)); v.push_back(one("1")); }
		}
		return v;
	};
	auto bobSlotAtEnd = [&]() {
		CallSlot s;
		s.at = (int) L.ch.size() - 1;
		s.len = 1;
		if (even) { s.bob[0] = one("14"); s.single[0] = one("1234"); }
		else { snprintf(buf, sizeof buf, "14%c", W); s.bob[0] = one(buf); s.single[0] = one("123"); }
		if (w < 4) { s.bob[0] = one("1"); s.single[0] = one("3"); }
		L.slots.push_back(s);
	};
	switch (m) {
		case HUNT:
		case WANDER:
			L.ch = huntLead();
			bobSlotAtEnd();
			break;
		case PLAIN_BOB:
			L.ch = huntLead();
			if (even) L.ch.back() = one("12");
			else { snprintf(buf, sizeof buf, "12%c", W); L.ch.back() = one(buf); }
			bobSlotAtEnd();
			break;
		case LITTLE_BOB:  // even w
			snprintf(buf, sizeof buf, "x1%cx14,12", W);
			L.ch = parseNotation(buf);
			bobSlotAtEnd();
			break;
		case GRANDSIRE: {  // odd w >= 5
			L.ch.push_back(one("3"));
			L.ch.push_back(one("1"));
			snprintf(buf, sizeof buf, "%c", W);
			for (int i = 0; i < w - 1; i++) { L.ch.push_back(one(buf)); L.ch.push_back(one("1")); }
			CallSlot s;
			s.at = (int) L.ch.size() - 2;
			s.len = 2;
			s.bob[0] = one("3"); s.bob[1] = one("1");
			s.single[0] = one("3"); s.single[1] = one("123");
			L.slots.push_back(s);
			break;
		}
		case STEDMAN: {  // odd w >= 5
			snprintf(buf, sizeof buf, "3.1.%c.3.1.3.1.3.%c.1.3.1", W, W);
			L.ch = parseNotation(buf);
			char b1[8], s1[8];
			snprintf(b1, sizeof b1, "%c", pc(w - 2));
			snprintf(s1, sizeof s1, "%c%c%c", pc(w - 2), pc(w - 1), W);
			for (int at : {2, 8}) {
				CallSlot s;
				s.at = at; s.len = 1;
				s.bob[0] = one(b1); s.single[0] = one(s1);
				L.slots.push_back(s);
			}
			break;
		}
		case CAMBRIDGE: {  // even w >= 6
			const char* pn = "x36x14x12x36x14x56,12";
			if (w == 8) pn = "x38x14x1258x36x14x58x16x78,12";
			else if (w == 10) pn = "x30x14x1250x36x1470x58x16x70x18x90,12";
			else if (w == 12) pn = "x3Tx14x125Tx36x147Tx58x169Tx70x18x9Tx10xET,12";
			L.ch = parseNotation(pn);
			bobSlotAtEnd();
			break;
		}
	}
	return L;
}

struct RingParams {
	int stage = 8;
	int method = PLAIN_BOB;
	float chance = 0.f;   // auto calls (or mutation, for WANDER)
	int gap = 1;          // rest pulses after each backstroke row
	bool going = true;    // false = ring rounds
};

struct Strike {
	bool struck = false;  // a bell sounded on this pulse
	bool gap = false;     // the pulse was the handstroke rest
	int bell = 0;         // 0 = treble
	int place = 0;        // 0 = lead
	bool hand = true;     // stroke of the row being rung
	bool rowStart = false, leadEnd = false, rounds = false, go = false, thatsAll = false;
	int call = 0;         // 1 bob, 2 single (on the row it starts)
};

struct HistRow {
	uint8_t b[MAXB];
	int n = 0;
	int call = 0;
	bool leadHead = false;
};

static const int HIST = 16;

struct Ringer {
	int stage = 8;        // bells in the row
	int w = 8;            // bells working (stage, or stage-1 with the tenor covering)
	bool cover = false;
	int methodSel = PLAIN_BOB, method = PLAIN_BOB;
	Lead lead;
	uint8_t row[MAXB];
	int pos = 0, changeIdx = 0, gapLeft = 0, leads = 0;
	bool hand = true, inMethod = false;
	int pendingCall = 0, activeCall = 0, activeSlot = -1;
	uint32_t rng = 0x9E3779B9u;
	Strike up;            // flags that land on the next row's first blow

	HistRow hist[HIST];
	int histHead = 0, histCount = 0;
	int lastCall = 0;

	Ringer() { setRounds(); build(PLAIN_BOB); }

	float rnd() {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
		return (rng >> 8) * (1.f / 16777216.f);
	}

	void setRounds() { for (int i = 0; i < MAXB; i++) row[i] = (uint8_t) i; }

	bool isRounds() const {
		for (int i = 0; i < stage; i++) if (row[i] != i) return false;
		return true;
	}

	// Pick working bells for a method, with a covering tenor if the parity is
	// wrong, or fall back to Plain Bob when there aren't enough bells.
	void build(int m) {
		methodSel = m;
		method = m;
		w = stage; cover = false;
		bool needOdd = (m == GRANDSIRE || m == STEDMAN);
		bool needEven = (m == LITTLE_BOB || m == CAMBRIDGE);
		if (needOdd && (w % 2) == 0) { w--; cover = true; }
		if (needEven && (w % 2) == 1) { w--; cover = true; }
		if ((needOdd && w < 5) || (m == CAMBRIDGE && w < 6)) { method = PLAIN_BOB; w = stage; cover = false; }
		lead = buildLead(method, w);
		changeIdx = 0;
		activeSlot = -1; activeCall = 0;
	}

	void reset() {
		setRounds();
		pos = 0; changeIdx = 0; gapLeft = 0; hand = true; inMethod = false;
		activeSlot = -1; activeCall = 0; leads = 0;
		up = Strike();
		histCount = 0;
	}

	void call(int c) { pendingCall = c; }

	// The Wander method rewrites one change of its lead now and then.
	void mutate(float chance) {
		if (lead.ch.size() < 2 || rnd() >= chance) return;
		int idx = (int) (rnd() * (lead.ch.size() - 1));
		for (int tries = 0; tries < 8; tries++) {
			uint16_t places = 0;
			int i = 0, swaps = 0;
			while (i < w) {
				if (i + 1 < w && rnd() < 0.75f) { i += 2; swaps++; }
				else { places |= (uint16_t)(1 << i); i++; }
			}
			if (swaps > 0) { lead.ch[idx] = places; return; }
		}
	}

	void pushHist() {
		HistRow& h = hist[histHead];
		memcpy(h.b, row, MAXB);
		h.n = stage;
		h.call = up.call;
		h.leadHead = up.leadEnd;
		histHead = (histHead + 1) % HIST;
		if (histCount < HIST) histCount++;
	}

	// One clock pulse.
	Strike tick(const RingParams& p) {
		Strike s;
		s.hand = hand;
		if (gapLeft > 0) {
			gapLeft--;
			s.gap = true;
			return s;
		}
		if (pos == 0) {
			s.rowStart = true;
			s.leadEnd = up.leadEnd; s.rounds = up.rounds; s.call = up.call;
			s.go = up.go; s.thatsAll = up.thatsAll;
			pushHist();
			up = Strike();
		}
		s.struck = true;
		s.place = pos;
		s.bell = row[pos];
		pos++;
		if (pos >= stage) {
			pos = 0;
			endRow(p);
		}
		return s;
	}

	void endRow(const RingParams& p) {
		bool wasBack = !hand;
		hand = !hand;
		if (wasBack) gapLeft = p.gap;

		if (p.stage != stage) {
			stage = p.stage < 3 ? 3 : (p.stage > MAXB ? MAXB : p.stage);
			setRounds();
			if (inMethod) up.thatsAll = true;
			inMethod = false;
			histCount = 0;
		}
		if (!p.going) {
			if (inMethod) { inMethod = false; setRounds(); up.thatsAll = true; }
			return;
		}
		if (!inMethod) {
			if (!hand) return;  // "Go" always starts on a handstroke
			inMethod = true;
			leads = 0;
			build(p.method);
			up.go = true;
		}
		if (changeIdx == 0 && p.method != methodSel) build(p.method);

		for (int i = 0; i < (int) lead.slots.size(); i++) {
			if (lead.slots[i].at != changeIdx) continue;
			int c = 0;
			if (pendingCall) { c = pendingCall; pendingCall = 0; }
			else if (method != WANDER && p.chance > 0.f && rnd() < p.chance) c = (rnd() < 0.25f) ? 2 : 1;
			activeSlot = c ? i : -1;
			activeCall = c;
			if (c) { up.call = c; lastCall = c; }
		}

		uint16_t ch = lead.ch[changeIdx];
		if (activeSlot >= 0) {
			const CallSlot& sl = lead.slots[activeSlot];
			int k = changeIdx - sl.at;
			if (k >= 0 && k < sl.len) ch = (activeCall == 1) ? sl.bob[k] : sl.single[k];
			if (k >= sl.len - 1) { activeSlot = -1; activeCall = 0; }
		}
		applyChange(row, w, ch);
		changeIdx++;
		if (changeIdx >= (int) lead.ch.size()) {
			changeIdx = 0;
			leads++;
			up.leadEnd = true;
			if (method == WANDER) mutate(p.chance);
		}
		if (isRounds()) up.rounds = true;
	}
};

}  // namespace peal
