// Groover engine: everything that doesn't need Rack, so it can be tested offline.
//   - Groove: a groove as a list of notes (beats, velocity), plus per-step tables
//   - loaders for Ableton .agr (plain or gzipped XML) and Standard MIDI files
//   - built-in grooves
//   - Clock: follows an incoming clock and predicts where the beat is, so hits
//     can be fired a little EARLY as well as late
//   - Scheduler: works out when each grid step (or incoming trigger) should land
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace groover {

// Live's Groove Pool "Base" choices, as fractions of a beat (quarter note).
static const int NUM_BASES = 6;
static const double BASE_BEATS[NUM_BASES] = {1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125};
static const char* const BASE_NAMES[NUM_BASES] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};

struct Note {
	double t;   // beats from the start of the groove
	float vel;  // 1..127
};

struct Groove {
	std::string name;
	double length = 4.0;  // beats
	int base = 3;         // index into BASE_BEATS (default 1/16)
	// the groove's own Groove Pool settings, percent (velocity -100..100)
	float quantize = 0.f, timing = 100.f, random = 0.f, velocity = 0.f;
	std::vector<Note> notes;
	// per-step tables for every base: offset in steps (-0.5..0.5), velocity 0..1
	std::vector<float> off[NUM_BASES];
	std::vector<float> vel[NUM_BASES];

	int steps(int b) const { return (int) off[b].size(); }

	void build() {
		if (length < 0.25) length = 0.25;
		float vmax = 1.f;
		for (const Note& n : notes) vmax = std::max(vmax, n.vel);
		for (int b = 0; b < NUM_BASES; b++) {
			double g = BASE_BEATS[b];
			int n = std::max(1, (int) std::lround(length / g));
			if (n > 1024) n = 1024;
			off[b].assign(n, 0.f);
			vel[b].assign(n, 1.f);
			for (int k = 0; k < n; k++) {
				double t = k * g;
				double best = 1e9;
				const Note* bn = nullptr;
				for (const Note& no : notes) {
					double d = no.t - t;
					// wrap round the loop
					d -= length * std::floor(d / length + 0.5);
					if (std::fabs(d) < std::fabs(best)) { best = d; bn = &no; }
				}
				if (bn && std::fabs(best) <= g * 0.5 + 1e-9) {
					off[b][k] = (float) std::max(-0.5, std::min(0.5, best / g));
					vel[b][k] = std::max(0.f, std::min(1.f, bn->vel / vmax));
				}
			}
		}
	}
};

// ---------------------------------------------------------------- built-ins
// Swing as on an MPC: the second 16th of each pair lands at swing% of the 8th.
inline void addSwingBar(Groove& g, double bar, double sw, float vOn, float vOff) {
	for (int p = 0; p < 8; p++) {
		double t = bar * 4.0 + p * 0.5;
		g.notes.push_back({t, vOn});
		g.notes.push_back({t + 0.5 * sw, vOff});
	}
}

inline std::vector<Groove> builtinGrooves() {
	std::vector<Groove> v;
	auto push = [&](Groove g) { g.build(); v.push_back(g); };
	{
		Groove g; g.name = "Straight";
		for (int k = 0; k < 16; k++) g.notes.push_back({k * 0.25, 100.f});
		push(g);
	}
	const double sw[4] = {0.54, 0.58, 0.62, 2.0 / 3.0};
	const char* swn[4] = {"Swing 16 54%", "Swing 16 58%", "Swing 16 62%", "Swing 16 66% (triplet)"};
	for (int i = 0; i < 4; i++) {
		Groove g; g.name = swn[i];
		addSwingBar(g, 0, sw[i], 110.f, 85.f);
		push(g);
	}
	{
		Groove g; g.name = "Swing 8 60%"; g.base = 1;
		for (int b = 0; b < 4; b++) { g.notes.push_back({b * 1.0, 110.f}); g.notes.push_back({b + 0.6, 85.f}); }
		push(g);
	}
	{
		// three velocity levels from the groove field guide, with 57% swing
		Groove g; g.name = "Accent ladder 57%";
		const float lv[16] = {45, 30, 92, 40, 45, 30, 122, 40, 45, 30, 90, 40, 48, 84, 118, 55};
		for (int k = 0; k < 16; k++) {
			double t = (k / 2) * 0.5 + ((k & 1) ? 0.5 * 0.57 : 0.0);
			g.notes.push_back({t, lv[k]});
		}
		push(g);
	}
	{
		// laid back: off-16ths and the backbeat sit late
		Groove g; g.name = "Laid back";
		for (int k = 0; k < 16; k++) {
			double o = 0.0;
			if (k & 1) o = 0.09;
			if (k == 4 || k == 12) o = 0.06;
			float vv = (k % 4 == 0) ? 110.f : ((k & 1) ? 70.f : 90.f);
			g.notes.push_back({k * 0.25 + o * 0.25, vv});
		}
		push(g);
	}
	{
		// pushing: off-16ths ahead of the grid, urgent
		Groove g; g.name = "Pushing";
		for (int k = 0; k < 16; k++) {
			double o = (k & 1) ? -0.07 : ((k % 4 == 2) ? -0.03 : 0.0);
			float vv = (k % 4 == 2) ? 115.f : ((k & 1) ? 75.f : 95.f);
			g.notes.push_back({k * 0.25 + o * 0.25, vv});
		}
		push(g);
	}
	{
		// lopsided, unquantised-MPC feel: every 16th has its own lean
		Groove g; g.name = "Wonky";
		const double o[16] = {0.0, 0.24, -0.06, 0.31, 0.03, 0.19, -0.11, 0.27, 0.0, 0.22, -0.04, 0.34, 0.06, 0.15, -0.09, 0.29};
		const float vv[16] = {112, 64, 88, 58, 104, 70, 96, 52, 110, 60, 84, 66, 100, 74, 92, 57};
		for (int k = 0; k < 16; k++) g.notes.push_back({k * 0.25 + o[k] * 0.25, vv[k]});
		push(g);
	}
	{
		// tresillo accents (3+3+2) with a small push on the accents
		Groove g; g.name = "Tresillo";
		for (int k = 0; k < 16; k++) {
			int m = k % 8;
			bool acc = (m == 0 || m == 3 || m == 6);
			double o = acc && m != 0 ? -0.05 : ((k & 1) ? 0.04 : 0.0);
			g.notes.push_back({k * 0.25 + o * 0.25, acc ? 120.f : 72.f});
		}
		push(g);
	}
	{
		// two bars that never sit still: a drummer slightly past it
		Groove g; g.name = "Drunk 2 bars"; g.length = 8.0;
		uint32_t s = 0x9E3779B9u;
		for (int k = 0; k < 32; k++) {
			s = s * 1664525u + 1013904223u;
			double r = ((s >> 8) & 0xFFFF) / 65535.0 - 0.5;
			double o = 0.22 * r + ((k & 1) ? 0.1 : 0.0);
			s = s * 1664525u + 1013904223u;
			float vv = 70.f + 50.f * (((s >> 8) & 0xFF) / 255.f) + ((k % 4 == 0) ? 10.f : 0.f);
			g.notes.push_back({k * 0.25 + o * 0.25, std::min(127.f, vv)});
		}
		push(g);
	}
	{
		// four bars: swing opens from straight to 64% and closes again
		Groove g; g.name = "Shuffle tide 4 bars"; g.length = 16.0;
		const double s4[4] = {0.50, 0.57, 0.64, 0.57};
		for (int b = 0; b < 4; b++) addSwingBar(g, b, s4[b], 108.f, 80.f + b * 6.f);
		push(g);
	}
	{
		// triplet 16ths base for 6/8-ish shuffles: middle note of each 3 pushed
		Groove g; g.name = "Triplet roll"; g.base = 4;
		for (int k = 0; k < 24; k++) {
			double o = (k % 3 == 1) ? 0.12 : ((k % 3 == 2) ? -0.05 : 0.0);
			float vv = (k % 3 == 0) ? 115.f : ((k % 3 == 1) ? 60.f : 85.f);
			g.notes.push_back({k / 6.0 + o / 6.0, vv});
		}
		push(g);
	}
	return v;
}

// ---------------------------------------------------------------- gzip inflate
// A small DEFLATE decoder (RFC 1951), enough for gzipped .agr files.
struct Inflate {
	const uint8_t* src; size_t n, pos = 0; uint32_t bitbuf = 0; int bitcnt = 0;
	std::vector<uint8_t> out;
	bool err = false;
	struct Huff { uint16_t count[16]; uint16_t sym[320]; };

	int bit() {
		if (bitcnt == 0) {
			if (pos >= n) { err = true; return 0; }
			bitbuf = src[pos++]; bitcnt = 8;
		}
		int b = bitbuf & 1; bitbuf >>= 1; bitcnt--; return b;
	}
	int bits(int k) { int v = 0; for (int i = 0; i < k; i++) v |= bit() << i; return v; }
	void build(Huff& h, const uint8_t* len, int num) {
		std::memset(h.count, 0, sizeof(h.count));
		for (int i = 0; i < num; i++) h.count[len[i]]++;
		h.count[0] = 0;
		uint16_t offs[16]; offs[1] = 0;
		for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h.count[i];
		for (int i = 0; i < num; i++) if (len[i]) h.sym[offs[len[i]]++] = (uint16_t) i;
	}
	int decode(const Huff& h) {
		int code = 0, first = 0, index = 0;
		for (int l = 1; l < 16; l++) {
			code |= bit();
			int c = h.count[l];
			if (code - c < first) return h.sym[index + (code - first)];
			index += c; first += c; first <<= 1; code <<= 1;
			if (err) return -1;
		}
		err = true; return -1;
	}
	void codes(const Huff& lit, const Huff& dist) {
		static const uint16_t lbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
		static const uint8_t lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
		static const uint16_t dbase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
		static const uint8_t dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
		while (!err) {
			int s = decode(lit);
			if (s < 0) return;
			if (s < 256) { out.push_back((uint8_t) s); continue; }
			if (s == 256) return;
			s -= 257;
			if (s >= 29) { err = true; return; }
			int len = lbase[s] + bits(lext[s]);
			int ds = decode(dist);
			if (ds < 0 || ds >= 30) { err = true; return; }
			size_t d = dbase[ds] + bits(dext[ds]);
			if (d > out.size()) { err = true; return; }
			size_t from = out.size() - d;
			for (int i = 0; i < len; i++) out.push_back(out[from + i]);
			if (out.size() > (64u << 20)) { err = true; return; }
		}
	}
	bool run() {
		int last;
		do {
			last = bit();
			int type = bits(2);
			if (err) return false;
			if (type == 0) {
				bitbuf = 0; bitcnt = 0;
				if (pos + 4 > n) return false;
				unsigned len = src[pos] | (src[pos + 1] << 8); pos += 4;
				if (pos + len > n) return false;
				out.insert(out.end(), src + pos, src + pos + len); pos += len;
			} else if (type == 1) {
				uint8_t l[288 + 30];
				for (int i = 0; i < 144; i++) l[i] = 8;
				for (int i = 144; i < 256; i++) l[i] = 9;
				for (int i = 256; i < 280; i++) l[i] = 7;
				for (int i = 280; i < 288; i++) l[i] = 8;
				Huff lit, dist; build(lit, l, 288);
				for (int i = 0; i < 30; i++) l[i] = 5;
				build(dist, l, 30);
				codes(lit, dist);
			} else if (type == 2) {
				int nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
				static const uint8_t ord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
				uint8_t l[320] = {0};
				for (int i = 0; i < ncode; i++) l[ord[i]] = (uint8_t) bits(3);
				Huff lc; build(lc, l, 19);
				int i = 0;
				uint8_t L[320] = {0};
				while (i < nlen + ndist && !err) {
					int s = decode(lc);
					if (s < 0) return false;
					if (s < 16) L[i++] = (uint8_t) s;
					else {
						int rep = 0; uint8_t val = 0;
						if (s == 16) { if (i == 0) return false; val = L[i - 1]; rep = 3 + bits(2); }
						else if (s == 17) rep = 3 + bits(3);
						else rep = 11 + bits(7);
						if (i + rep > nlen + ndist) return false;
						while (rep--) L[i++] = val;
					}
				}
				Huff lit, dist; build(lit, L, nlen); build(dist, L + nlen, ndist);
				codes(lit, dist);
			} else return false;
			if (err) return false;
		} while (!last);
		return true;
	}
};

inline bool gunzip(const std::string& in, std::string& out) {
	const uint8_t* p = (const uint8_t*) in.data();
	size_t n = in.size();
	if (n < 18 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8) return false;
	int flg = p[3];
	size_t pos = 10;
	if (flg & 4) { if (pos + 2 > n) return false; pos += 2 + (p[pos] | (p[pos + 1] << 8)); }
	if (flg & 8) { while (pos < n && p[pos]) pos++; pos++; }
	if (flg & 16) { while (pos < n && p[pos]) pos++; pos++; }
	if (flg & 2) pos += 2;
	if (pos >= n) return false;
	Inflate inf; inf.src = p + pos; inf.n = n - pos;
	if (!inf.run()) return false;
	out.assign((const char*) inf.out.data(), inf.out.size());
	return true;
}

// ---------------------------------------------------------------- .agr loader
inline bool attr(const std::string& tag, const char* name, std::string& val) {
	std::string key = std::string(" ") + name + "=\"";
	size_t a = tag.find(key);
	if (a == std::string::npos) return false;
	a += key.size();
	size_t b = tag.find('"', a);
	if (b == std::string::npos) return false;
	val = tag.substr(a, b - a);
	return true;
}

// Value="..." of the first <Elem Value="..."/> at or after `from`.
inline bool elemValue(const std::string& x, const char* elem, double& v, size_t from = 0) {
	std::string key = std::string("<") + elem + " ";
	size_t a = x.find(key, from);
	if (a == std::string::npos) return false;
	size_t e = x.find('>', a);
	if (e == std::string::npos) return false;
	std::string val;
	if (!attr(x.substr(a, e - a), "Value", val)) return false;
	v = std::atof(val.c_str());
	return true;
}

inline std::string stemOf(const std::string& path) {
	size_t s = path.find_last_of("/\\");
	std::string f = (s == std::string::npos) ? path : path.substr(s + 1);
	size_t d = f.find_last_of('.');
	return d == std::string::npos ? f : f.substr(0, d);
}

inline bool parseAgr(const std::string& raw, Groove& g, std::string& why) {
	std::string x;
	if (raw.size() > 2 && (uint8_t) raw[0] == 0x1f && (uint8_t) raw[1] == 0x8b) {
		if (!gunzip(raw, x)) { why = "couldn't unpack the gzipped file"; return false; }
	} else x = raw;
	if (x.find("<Groove") == std::string::npos) { why = "no <Groove> in the file"; return false; }
	double loopStart = 0, loopEnd = 0;
	size_t lp = x.find("<Loop>");
	if (lp != std::string::npos) {
		elemValue(x, "LoopStart", loopStart, lp);
		elemValue(x, "LoopEnd", loopEnd, lp);
	}
	if (loopEnd <= loopStart) {
		double ce = 0;
		if (elemValue(x, "CurrentEnd", ce)) loopEnd = ce;
	}
	std::vector<Note> notes;
	size_t p = 0;
	double last = 0;
	while ((p = x.find("<MidiNoteEvent ", p)) != std::string::npos) {
		size_t e = x.find('>', p);
		if (e == std::string::npos) break;
		std::string tag = x.substr(p, e - p);
		p = e;
		std::string tv, vv, en;
		if (!attr(tag, "Time", tv)) continue;
		if (attr(tag, "IsEnabled", en) && en == "false") continue;
		float vel = attr(tag, "Velocity", vv) ? (float) std::atof(vv.c_str()) : 100.f;
		double t = std::atof(tv.c_str()) - loopStart;
		notes.push_back({t, std::max(1.f, std::min(127.f, vel))});
		last = std::max(last, t);
	}
	if (notes.empty()) { why = "the groove has no notes"; return false; }
	double len = loopEnd - loopStart;
	if (len <= 0) len = std::max(1.0, std::ceil((last + 1e-6) / 4.0) * 4.0);
	g.notes.clear();
	for (const Note& n : notes) if (n.t >= -0.5 && n.t < len + 0.5) g.notes.push_back(n);
	g.length = len;
	// Groove Pool settings live after the clip
	size_t clipEnd = x.find("</Clip>");
	size_t from = clipEnd == std::string::npos ? 0 : clipEnd;
	double v;
	if (elemValue(x, "Grid", v, from)) g.base = std::max(0, std::min(NUM_BASES - 1, (int) v));
	if (elemValue(x, "QuantizationAmount", v, from)) g.quantize = (float) v;
	if (elemValue(x, "TimingAmount", v, from)) g.timing = (float) v;
	if (elemValue(x, "RandomAmount", v, from)) g.random = (float) v;
	if (elemValue(x, "VelocityAmount", v, from)) g.velocity = (float) v;
	g.build();
	return true;
}

// ---------------------------------------------------------------- .agr writer
// Writes a groove as an Ableton groove file (Live 11+ layout, plain XML) so
// grooves made or chosen in VCV can go back into Live's Groove Pool.
inline std::string xmlEscape(const std::string& s) {
	std::string o;
	for (char c : s) {
		if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c;
	}
	return o;
}

inline std::string writeAgr(const Groove& g) {
	std::ostringstream x;
	x.precision(10);
	std::string nm = xmlEscape(g.name);
	double L = g.length;
	x << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	  << "<Ableton MajorVersion=\"5\" MinorVersion=\"11.0_11300\" SchemaChangeCount=\"3\" Creator=\"Ableton Live 11.3.12\" Revision=\"\">\n"
	  << "\t<Groove>\n\t\t<LomId Value=\"0\" />\n\t\t<Name Value=\"" << nm << "\" />\n\t\t<Clip>\n\t\t\t<Value>\n"
	  << "\t\t\t\t<MidiClip Id=\"0\" Time=\"0\">\n\t\t\t\t\t<LomId Value=\"0\" />\n\t\t\t\t\t<LomIdView Value=\"0\" />\n"
	  << "\t\t\t\t\t<CurrentStart Value=\"0\" />\n\t\t\t\t\t<CurrentEnd Value=\"" << L << "\" />\n"
	  << "\t\t\t\t\t<Loop>\n\t\t\t\t\t\t<LoopStart Value=\"0\" />\n\t\t\t\t\t\t<LoopEnd Value=\"" << L << "\" />\n"
	  << "\t\t\t\t\t\t<StartRelative Value=\"0\" />\n\t\t\t\t\t\t<LoopOn Value=\"true\" />\n\t\t\t\t\t\t<OutMarker Value=\"" << L << "\" />\n"
	  << "\t\t\t\t\t\t<HiddenLoopStart Value=\"0\" />\n\t\t\t\t\t\t<HiddenLoopEnd Value=\"" << L << "\" />\n\t\t\t\t\t</Loop>\n"
	  << "\t\t\t\t\t<Name Value=\"" << nm << "\" />\n\t\t\t\t\t<Annotation Value=\"\" />\n\t\t\t\t\t<Color Value=\"19\" />\n"
	  << "\t\t\t\t\t<LaunchMode Value=\"0\" />\n\t\t\t\t\t<LaunchQuantisation Value=\"0\" />\n"
	  << "\t\t\t\t\t<TimeSignature>\n\t\t\t\t\t\t<TimeSignatures>\n\t\t\t\t\t\t\t<RemoteableTimeSignature Id=\"0\">\n"
	  << "\t\t\t\t\t\t\t\t<Numerator Value=\"4\" />\n\t\t\t\t\t\t\t\t<Denominator Value=\"4\" />\n\t\t\t\t\t\t\t\t<Time Value=\"0\" />\n"
	  << "\t\t\t\t\t\t\t</RemoteableTimeSignature>\n\t\t\t\t\t\t</TimeSignatures>\n\t\t\t\t\t</TimeSignature>\n"
	  << "\t\t\t\t\t<Envelopes>\n\t\t\t\t\t\t<Envelopes />\n\t\t\t\t\t</Envelopes>\n"
	  << "\t\t\t\t\t<ScrollerTimePreserver>\n\t\t\t\t\t\t<LeftTime Value=\"0\" />\n\t\t\t\t\t\t<RightTime Value=\"0\" />\n\t\t\t\t\t</ScrollerTimePreserver>\n"
	  << "\t\t\t\t\t<TimeSelection>\n\t\t\t\t\t\t<AnchorTime Value=\"0\" />\n\t\t\t\t\t\t<OtherTime Value=\"0\" />\n\t\t\t\t\t</TimeSelection>\n"
	  << "\t\t\t\t\t<Legato Value=\"false\" />\n\t\t\t\t\t<Ram Value=\"false\" />\n"
	  << "\t\t\t\t\t<GrooveSettings>\n\t\t\t\t\t\t<GrooveId Value=\"-1\" />\n\t\t\t\t\t</GrooveSettings>\n"
	  << "\t\t\t\t\t<Disabled Value=\"false\" />\n\t\t\t\t\t<VelocityAmount Value=\"0\" />\n"
	  << "\t\t\t\t\t<FollowAction>\n\t\t\t\t\t\t<FollowTime Value=\"4\" />\n\t\t\t\t\t\t<IsLinked Value=\"true\" />\n\t\t\t\t\t\t<LoopIterations Value=\"1\" />\n"
	  << "\t\t\t\t\t\t<FollowActionA Value=\"4\" />\n\t\t\t\t\t\t<FollowActionB Value=\"0\" />\n\t\t\t\t\t\t<FollowChanceA Value=\"100\" />\n"
	  << "\t\t\t\t\t\t<FollowChanceB Value=\"0\" />\n\t\t\t\t\t\t<JumpIndexA Value=\"1\" />\n\t\t\t\t\t\t<JumpIndexB Value=\"1\" />\n"
	  << "\t\t\t\t\t\t<FollowActionEnabled Value=\"false\" />\n\t\t\t\t\t</FollowAction>\n"
	  << "\t\t\t\t\t<Grid>\n\t\t\t\t\t\t<FixedNumerator Value=\"1\" />\n\t\t\t\t\t\t<FixedDenominator Value=\"16\" />\n\t\t\t\t\t\t<GridIntervalPixel Value=\"20\" />\n"
	  << "\t\t\t\t\t\t<Ntoles Value=\"2\" />\n\t\t\t\t\t\t<SnapToGrid Value=\"true\" />\n\t\t\t\t\t\t<Fixed Value=\"true\" />\n\t\t\t\t\t</Grid>\n"
	  << "\t\t\t\t\t<FreezeStart Value=\"0\" />\n\t\t\t\t\t<FreezeEnd Value=\"0\" />\n\t\t\t\t\t<IsWarped Value=\"true\" />\n\t\t\t\t\t<TakeId Value=\"-1\" />\n"
	  << "\t\t\t\t\t<Notes>\n\t\t\t\t\t\t<KeyTracks>\n\t\t\t\t\t\t\t<KeyTrack Id=\"0\">\n\t\t\t\t\t\t\t\t<Notes>\n";
	std::vector<Note> ns = g.notes;
	std::sort(ns.begin(), ns.end(), [](const Note& a, const Note& b) { return a.t < b.t; });
	int id = 1;
	for (const Note& n : ns) {
		double t = std::max(0.0, n.t);
		x << "\t\t\t\t\t\t\t\t\t<MidiNoteEvent Time=\"" << t << "\" Duration=\"0.0625\" Velocity=\"" << (int) std::lround(n.vel)
		  << "\" VelocityDeviation=\"0\" OffVelocity=\"64\" Probability=\"1\" IsEnabled=\"true\" NoteId=\"" << id++ << "\" />\n";
	}
	x << "\t\t\t\t\t\t\t\t</Notes>\n\t\t\t\t\t\t\t\t<MidiKey Value=\"36\" />\n\t\t\t\t\t\t\t</KeyTrack>\n\t\t\t\t\t\t</KeyTracks>\n"
	  << "\t\t\t\t\t\t<PerNoteEventStore>\n\t\t\t\t\t\t\t<EventLists />\n\t\t\t\t\t\t</PerNoteEventStore>\n"
	  << "\t\t\t\t\t\t<NoteIdGenerator>\n\t\t\t\t\t\t\t<NextId Value=\"" << id << "\" />\n\t\t\t\t\t\t</NoteIdGenerator>\n\t\t\t\t\t</Notes>\n"
	  << "\t\t\t\t\t<BankSelectCoarse Value=\"-1\" />\n\t\t\t\t\t<BankSelectFine Value=\"-1\" />\n\t\t\t\t\t<ProgramChange Value=\"-1\" />\n"
	  << "\t\t\t\t\t<NoteEditorFoldInZoom Value=\"-1\" />\n\t\t\t\t\t<NoteEditorFoldInScroll Value=\"0\" />\n"
	  << "\t\t\t\t\t<NoteEditorFoldOutZoom Value=\"-1\" />\n\t\t\t\t\t<NoteEditorFoldOutScroll Value=\"0\" />\n"
	  << "\t\t\t\t\t<NoteEditorFoldScaleZoom Value=\"-1\" />\n\t\t\t\t\t<NoteEditorFoldScaleScroll Value=\"0\" />\n"
	  << "\t\t\t\t\t<ScaleInformation>\n\t\t\t\t\t\t<RootNote Value=\"0\" />\n\t\t\t\t\t\t<Name Value=\"Major\" />\n\t\t\t\t\t</ScaleInformation>\n"
	  << "\t\t\t\t\t<IsInKey Value=\"true\" />\n\t\t\t\t\t<NoteSpellingPreference Value=\"3\" />\n\t\t\t\t\t<PreferFlatRootNote Value=\"false\" />\n"
	  << "\t\t\t\t\t<ExpressionGrid>\n\t\t\t\t\t\t<FixedNumerator Value=\"1\" />\n\t\t\t\t\t\t<FixedDenominator Value=\"16\" />\n"
	  << "\t\t\t\t\t\t<GridIntervalPixel Value=\"20\" />\n\t\t\t\t\t\t<Ntoles Value=\"2\" />\n\t\t\t\t\t\t<SnapToGrid Value=\"false\" />\n"
	  << "\t\t\t\t\t\t<Fixed Value=\"false\" />\n\t\t\t\t\t</ExpressionGrid>\n\t\t\t\t</MidiClip>\n\t\t\t</Value>\n\t\t</Clip>\n"
	  << "\t\t<Grid Value=\"" << g.base << "\" />\n\t\t<QuantizationAmount Value=\"" << g.quantize << "\" />\n"
	  << "\t\t<TimingAmount Value=\"" << g.timing << "\" />\n\t\t<RandomAmount Value=\"" << g.random << "\" />\n"
	  << "\t\t<VelocityAmount Value=\"" << g.velocity << "\" />\n\t\t<Annotation Value=\"\" />\n\t\t<Selection Value=\"true\" />\n"
	  << "\t\t<SourceContext />\n\t</Groove>\n</Ableton>\n";
	return x.str();
}

// ---------------------------------------------------------------- .mid loader
inline bool parseMidi(const std::string& d, Groove& g, std::string& why) {
	const uint8_t* p = (const uint8_t*) d.data();
	size_t n = d.size();
	auto u32 = [&](size_t i) { return (uint32_t) p[i] << 24 | (uint32_t) p[i + 1] << 16 | (uint32_t) p[i + 2] << 8 | p[i + 3]; };
	if (n < 14 || std::memcmp(p, "MThd", 4) != 0) { why = "not a MIDI file"; return false; }
	uint32_t hl = u32(4);
	int ntr = p[10] << 8 | p[11];
	int div = p[12] << 8 | p[13];
	if (div & 0x8000) { why = "SMPTE-timed MIDI files aren't supported"; return false; }
	if (div == 0) div = 96;
	size_t pos = 8 + hl;
	std::vector<Note> notes;
	for (int t = 0; t < ntr && pos + 8 <= n; t++) {
		if (std::memcmp(p + pos, "MTrk", 4) != 0) break;
		uint32_t len = u32(pos + 4);
		size_t i = pos + 8, end = std::min(n, (size_t) (pos + 8 + len));
		pos = end;
		uint64_t tick = 0;
		uint8_t run = 0;
		while (i < end) {
			uint32_t dt = 0;
			for (int k = 0; k < 4 && i < end; k++) { uint8_t b = p[i++]; dt = dt << 7 | (b & 0x7f); if (!(b & 0x80)) break; }
			tick += dt;
			if (i >= end) break;
			uint8_t st = p[i];
			if (st & 0x80) i++; else st = run;
			if (st == 0xFF) {
				if (i >= end) break;
				i++;  // type
				uint32_t l = 0;
				for (int k = 0; k < 4 && i < end; k++) { uint8_t b = p[i++]; l = l << 7 | (b & 0x7f); if (!(b & 0x80)) break; }
				i += l;
				continue;
			}
			if (st == 0xF0 || st == 0xF7) {
				uint32_t l = 0;
				for (int k = 0; k < 4 && i < end; k++) { uint8_t b = p[i++]; l = l << 7 | (b & 0x7f); if (!(b & 0x80)) break; }
				i += l;
				continue;
			}
			run = st;
			int hi = st & 0xF0;
			int nb = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
			if (i + nb > end) break;
			if (hi == 0x90 && p[i + 1] > 0) notes.push_back({(double) tick / div, (float) p[i + 1]});
			i += nb;
		}
	}
	if (notes.empty()) { why = "no notes in the MIDI file"; return false; }
	std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) { return a.t < b.t; });
	// merge notes struck together (a whole drum kit on one beat): keep the loudest
	std::vector<Note> m;
	for (const Note& no : notes) {
		if (!m.empty() && no.t - m.back().t < 1.0 / 128.0) m.back().vel = std::max(m.back().vel, no.vel);
		else m.push_back(no);
	}
	double first = std::floor(m.front().t / 4.0) * 4.0;  // start at the bar of the first note
	for (Note& no : m) no.t -= first;
	double len = std::max(4.0, std::ceil((m.back().t + 0.125) / 4.0) * 4.0);
	g.notes = m;
	g.length = std::min(len, 64.0);
	g.base = 3;
	g.quantize = 0.f; g.timing = 100.f; g.random = 0.f; g.velocity = 0.f;
	g.build();
	return true;
}

inline bool loadGrooveFile(const std::string& path, Groove& g, std::string& why) {
	std::ifstream f(path, std::ios::binary);
	if (!f) { why = "can't open the file"; return false; }
	std::stringstream ss; ss << f.rdbuf();
	std::string d = ss.str();
	g = Groove();
	g.name = stemOf(path);
	bool ok;
	if (d.size() >= 4 && std::memcmp(d.data(), "MThd", 4) == 0) ok = parseMidi(d, g, why);
	else ok = parseAgr(d, g, why);
	g.name = stemOf(path);
	return ok;
}

// ---------------------------------------------------------------- clock follower
// Counts incoming clock pulses (PPQN per beat) and, between pulses, estimates
// how far through the beat we are from the last pulse period. The estimate
// stops just short of the next pulse, so on-grid hits still wait for the real
// clock and nothing runs on when the clock stops.
struct Clock {
	int ppqn = 4;
	double pulseBeat = -0.25;  // beat position of the last pulse
	double period = 0.125;     // seconds between pulses
	double since = 0.0;
	bool running = false;

	void reset() { pulseBeat = -1.0 / ppqn; since = 0.0; }
	void tick(double dt) { since += dt; if (since > 2.0) running = false; }
	void pulse() {
		if (running && since > 1e-4) period = since;
		running = true;
		since = 0.0;
		pulseBeat += 1.0 / ppqn;
	}
	double pos() const {
		double f = running ? std::min(since / period, 0.999) : 0.0;
		return pulseBeat + f / ppqn;
	}
	double beatSeconds() const { return period * ppqn; }
	double bpm() const { return 60.0 / beatSeconds(); }
};

// ---------------------------------------------------------------- scheduler
struct Settings {
	int base = 3;
	float amount = 1.f;    // 0..1.3, scales timing
	float timing = 1.f;    // 0..1
	float velocity = 0.f;  // -1..1
	float random = 0.f;    // 0..1
	float quantize = 0.f;  // 0..1 (lanes only)
};

struct Rng {
	uint32_t s = 0x1234567u;
	float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
};

// Offset of step k in beats, given the groove and settings, plus a random part.
inline double stepOffsetBeats(const Groove& g, const Settings& st, long k, float rnd) {
	int n = g.steps(st.base);
	if (n <= 0) return 0.0;
	long i = ((k % n) + n) % n;
	double gb = BASE_BEATS[st.base];
	double o = g.off[st.base][i] * st.timing * st.amount + rnd * st.random * 0.25f;
	o = std::max(-0.5, std::min(0.75, o));
	return o * gb;
}

// Accent 0..1 for step k: Live-style velocity amount (negative flips it).
inline float stepAccent(const Groove& g, const Settings& st, long k) {
	int n = g.steps(st.base);
	if (n <= 0) return 1.f;
	long i = ((k % n) + n) % n;
	float v = g.vel[st.base][i];
	float a = std::fabs(st.velocity) * std::min(1.f, st.amount);
	return st.velocity >= 0.f ? 1.f - a * (1.f - v) : 1.f - a * v;
}

// Grooved clock: one pulse per grid step, at grid + groove offset.
struct GroovedClock {
	long next = 0;     // next step to fire
	float rnd = 0.f;   // random part for `next`
	Rng rng;

	void reset() { next = 0; rnd = rng.uni() * 2.f - 1.f; }
	// Returns the step fired, or -1.
	long process(const Groove& g, const Settings& st, double pos) {
		double gb = BASE_BEATS[st.base];
		// out of step (base changed, clock jumped): re-align
		if (next * gb + 0.75 * gb < pos - gb || next * gb - 0.5 * gb > pos + 2.0 * gb) {
			next = (long) std::ceil(pos / gb - 0.5);
			if (next < 0) next = 0;
			rnd = rng.uni() * 2.f - 1.f;
		}
		double t = next * gb + stepOffsetBeats(g, st, next, rnd);
		if (pos >= t) {
			long fired = next;
			next++;
			rnd = rng.uni() * 2.f - 1.f;
			return fired;
		}
		return -1;
	}
};

// Trigger lane: incoming edges are delayed so they land on the groove.
struct Lane {
	struct Ev { double t; bool high; };
	Ev q[32];
	int count = 0;
	bool out = false;
	double lastDelay = 0.0;
	double lastOn = -1.0;

	void push(double t, bool high) {
		if (count < 32) {
			int i = count++;
			while (i > 0 && q[i - 1].t > t) { q[i] = q[i - 1]; i--; }
			q[i] = {t, high};
		}
	}
	void clear() { count = 0; out = false; }
	// returns true on a rising output edge
	bool process(double now) {
		bool rose = false;
		while (count > 0 && q[0].t <= now) {
			bool h = q[0].high;
			if (h && !out) rose = true;
			out = h;
			for (int i = 1; i < count; i++) q[i - 1] = q[i];
			count--;
		}
		return rose;
	}
};

// Work out how long to delay a trigger that arrives at beat position `pos`.
// `lookahead` (seconds) lets hits land early: every lane is delayed by it.
// Returns the delay in seconds (>= 0) and the step it belongs to.
inline double laneDelay(const Groove& g, const Settings& st, double pos, double beatSec, float rnd,
                        double lookahead, long& stepOut) {
	double gb = BASE_BEATS[st.base];
	long k = (long) std::floor(pos / gb + 0.5);
	double grid = k * gb;
	double qpos = pos + (grid - pos) * st.quantize;
	double target = qpos + stepOffsetBeats(g, st, k, rnd);
	stepOut = k;
	double d = (target - pos) * beatSec + lookahead;
	return d < 0.0 ? 0.0 : d;
}

}  // namespace groover
