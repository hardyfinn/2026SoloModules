// SPRECHER: speech from typed words. Port of tools/proto/sprecher_ref.py.
// Text front end (parse, letter-to-sound), frame builder (word_targets), a live scheduler,
// and the FORMANT and CHIP engines. Tables come from SprecherTables.hpp (generated).
#pragma once
#include "Common.hpp"
#include "SprecherTables.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <cstring>

namespace wb {

static const double SP_CTRL = 0.005;

struct SpPh { int idx; bool stress; };
struct SpWordP { std::string label; std::vector<SpPh> ph; };
typedef std::array<double, 12> SpFrame;   // F1 F2 F3 AV AH AF FC FB NZ quick hold stress

// ---------------------------------------------------------------- text front end
struct SpText {
	std::unordered_map<std::string, int> phIndex;
	std::unordered_map<std::string, std::string> dict[7];
	SpText() {
		for (int i = 0; i < SP_NPHON; i++) phIndex[SP_PHON[i].name] = i;
		for (int l = 0; l < 7; l++)
			for (int i = 0; i < SP_DICT_N[l]; i++) dict[l][SP_DICT[l][i].word] = SP_DICT[l][i].ph;
	}
	static SpText& get() { static SpText t; return t; }

	// "'AY N S" -> phonemes. Unknown names are dropped.
	std::vector<SpPh> phonemes(const std::string& s, bool upper = false) const {
		std::vector<SpPh> out;
		size_t i = 0;
		while (i < s.size()) {
			while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
			size_t j = i;
			while (j < s.size() && !std::isspace((unsigned char)s[j])) j++;
			if (j > i) {
				std::string t = s.substr(i, j - i);
				if (upper) for (auto& c : t) c = (char)std::toupper((unsigned char)c);
				bool st = false;
				size_t a = 0;
				while (a < t.size() && t[a] == '\'') { st = true; a++; }
				auto it = phIndex.find(t.substr(a));
				if (it != phIndex.end()) out.push_back({it->second, st});
			}
			i = j;
		}
		return out;
	}
	static std::string lower(const std::string& s) {
		std::string o;
		for (size_t i = 0; i < s.size(); i++) {
			unsigned char c = (unsigned char)s[i];
			if (c == 0xC3 && i + 1 < s.size()) {
				unsigned char d = (unsigned char)s[i + 1];
				if (d == 0x84 || d == 0x96 || d == 0x9C) d += 0x20;    // Ä Ö Ü
				o += (char)c; o += (char)d; i++; continue;
			}
			o += (char)std::tolower(c);
		}
		return o;
	}
	std::vector<SpPh> lts(const std::string& w, int lang) const {
		const SpRule* rules = SP_LTS[lang]; int nr = SP_LTS_N[lang];
		std::vector<SpPh> out;
		size_t i = 0;
		while (i < w.size()) {
			int best = -1; size_t bestLen = 0;
			for (int r = 0; r < nr; r++) {
				std::string p = rules[r].pat;
				bool start = !p.empty() && p.front() == '^', end = !p.empty() && p.back() == '$';
				std::string core = p.substr(start ? 1 : 0);
				if (end) core.pop_back();
				if (start && i != 0) continue;
				if (w.compare(i, core.size(), core) != 0) continue;
				if (end && i + core.size() != w.size()) continue;
				if (best < 0 || core.size() > bestLen) { best = r; bestLen = core.size(); }
			}
			if (best < 0) { i++; continue; }
			auto ph = phonemes(rules[best].ph);
			out.insert(out.end(), ph.begin(), ph.end());
			i += bestLen;
		}
		for (auto& p : out) if (SP_PHON[p.idx].vowel) { p.stress = true; break; }
		return out;
	}
	static bool isSep(unsigned char c) { return std::isspace(c) || c == ',' || c == '.' || c == ';' || c == ':' || c == '!' || c == '?'; }
	std::vector<SpWordP> parse(const std::string& text, int lang, size_t maxWords = 256) const {
		std::vector<SpWordP> out;
		size_t i = 0, n = text.size();
		while (i < n && out.size() < maxWords) {
			if (isSep((unsigned char)text[i])) { i++; continue; }
			std::string tok;
			size_t close = text[i] == '[' ? text.find(']', i + 1) : std::string::npos;
			if (close != std::string::npos) { tok = text.substr(i, close - i + 1); i = close + 1; }
			else {
				size_t j = i;
				while (j < n && !isSep((unsigned char)text[j])) j++;
				tok = text.substr(i, j - i); i = j;
			}
			if (tok.size() >= 2 && tok.front() == '[' && tok.back() == ']') {
				out.push_back({tok, phonemes(tok.substr(1, tok.size() - 2), true)});
				continue;
			}
			std::string t = lower(tok);
			bool digits = !t.empty();
			for (char c : t) if (c < '0' || c > '9') digits = false;
			if (digits) {
				auto it = dict[lang].find(t);
				if (it != dict[lang].end()) out.push_back({t, phonemes(it->second)});
				else for (char c : t) {
					if (out.size() >= maxWords) break;
					std::string ch(1, c);
					out.push_back({ch, phonemes(dict[lang].at(ch))});
				}
				continue;
			}
			int ls[3] = {lang, 0, 1};
			bool found = false;
			for (int l : ls) {
				auto it = dict[l].find(t);
				if (it != dict[l].end()) { out.push_back({t, phonemes(it->second)}); found = true; break; }
			}
			if (!found) out.push_back({t, lts(t, lang)});
		}
		return out;
	}
	static std::string phString(const std::vector<SpPh>& ph) {
		std::string s;
		for (size_t i = 0; i < ph.size(); i++) {
			if (i) s += ' ';
			if (ph[i].stress) s += '\'';
			s += SP_PHON[ph[i].idx].name;
		}
		return s;
	}
};

// ---------------------------------------------------------------- frames (word_targets)
inline std::vector<SpFrame> spWordTargets(const std::vector<SpPh>& phs, double rate) {
	struct Seg { double ms, f[3], av, ah, af, fc, fb, nz; int flags; bool st; };
	std::vector<Seg> segs;
	for (auto& p : phs) {
		const SpPhon& P = SP_PHON[p.idx];
		for (int s = 0; s < P.count; s++) {
			const SpSeg& g = SP_SEGS[P.first + s];
			double ms = g.ms;
			bool v = g.flags & 1;
			if (p.stress && v) ms *= 1.2;
			segs.push_back({ms / rate, {g.f1, g.f2, g.f3}, g.av, g.ah, g.af, g.fc, g.fb, g.nz, g.flags, p.stress && v});
		}
	}
	if (segs.empty()) return {};
	for (size_t i = 0; i < segs.size(); i++) {
		Seg& s = segs[i];
		if ((s.flags & 4) || s.f[0] == 0) {
			const Seg* src = nullptr;
			for (size_t j = i + 1; j < segs.size(); j++) if (!(segs[j].flags & 4) && !(segs[j].flags & 2)) { src = &segs[j]; break; }
			if (!src) for (int j = (int)i - 1; j >= 0; j--) if (!(segs[j].flags & 4)) { src = &segs[j]; break; }
			if (src) { s.f[0] = src->f[0]; s.f[1] = src->f[1]; s.f[2] = src->f[2]; }
			else { s.f[0] = 500; s.f[1] = 1500; s.f[2] = 2500; }
		}
	}
	int hold = -1;
	for (size_t i = 0; i < segs.size(); i++) if (segs[i].st) { hold = (int)i; break; }
	if (hold < 0) for (size_t i = 0; i < segs.size(); i++) if (segs[i].flags & 1) { hold = (int)i; break; }
	std::vector<SpFrame> rows;
	for (size_t i = 0; i < segs.size(); i++) {
		const Seg& s = segs[i];
		int n = std::max(1, (int)std::nearbyint(s.ms / 1000.0 / SP_CTRL));
		SpFrame r = {s.f[0], s.f[1], s.f[2], s.av, s.ah, s.af, s.fc != 0 ? s.fc : 3000.0, s.fb != 0 ? s.fb : 2000.0, s.nz,
			(s.flags & 2) ? 1.0 : 0.0, (int)i == hold ? 1.0 : 0.0, s.st ? 1.0 : 0.0};
		for (int k = 0; k < n; k++) rows.push_back(r);
	}
	SpFrame last = rows.back();
	rows.push_back({last[0], last[1], last[2], 0, 0, 0, 3000, 2000, 0, 0, 0, 0});
	return rows;
}

// ---------------------------------------------------------------- live scheduler
struct SpScheduler {
	std::vector<SpFrame> frames;
	size_t k = 0;
	bool active = false;
	SpFrame row = {500, 1500, 2500, 0, 0, 0, 3000, 2000, 0, 0, 0, 0};
	int hop = 240, cnt = 0;
	bool pending = false, finished = false;
	std::vector<SpFrame> next;
	void init(double fs) { hop = (int)std::nearbyint(SP_CTRL * fs); cnt = 0; active = false; pending = false; k = 0;
		row = {500, 1500, 2500, 0, 0, 0, 3000, 2000, 0, 0, 0, 0}; }
	void start(std::vector<SpFrame>&& f) { next = std::move(f); pending = true; }
	double progress() const { return frames.empty() ? 0.0 : std::min(1.0, (double)k / frames.size()); }
	// call every engine sample. Returns true on a frame boundary (row may have changed).
	bool tick(bool gateHigh, bool holdOn) {
		finished = false;
		if (cnt == 0) {
			if (pending) { frames.swap(next); next.clear(); pending = false; k = 0; active = !frames.empty(); }
			if (active) {
				if (k < frames.size()) {
					row = frames[k];
					bool lastHold = frames[k][10] > 0 && (k + 1 >= frames.size() || frames[k + 1][10] == 0);
					if (!(holdOn && lastHold && gateHigh)) k++;
				} else {
					SpFrame c = frames.back();
					row = {c[0], c[1], c[2], 0, 0, 0, 3000, 2000, 0, 0, 0, 0};
					active = false; finished = true;
				}
			}
		}
		bool b = cnt == 0;
		if (++cnt >= hop) cnt = 0;
		return b;
	}
	bool busy() const { return active || pending; }
};

inline void spReson(double f, double bw, double fs, double& A, double& B, double& C) {
	f = std::min(f, 0.45 * fs);
	C = -std::exp(-2.0 * PI * bw / fs); B = 2.0 * std::exp(-PI * bw / fs) * std::cos(2.0 * PI * f / fs);
	A = 1.0 - B - C;
}

struct SpVoiceParams { double size = 1.0, whisper = 0.0, age = 0.3; };

// ---------------------------------------------------------------- FORMANT engine
struct SpFormant {
	double fs = 48000;
	double U[4] = {};
	double cur[9];
	double y1[6], y2[6], cf[6][3];
	double fy1 = 0, fy2 = 0, fA = 0, fB = 0, fC = 0;
	double ph = 0, lp = 0, hpz = 0, nlp = 0, sh = 0;
	XS32 rng;
	Drift drift;
	uint64_t n = 0;
	double f1out = 500;
	void init(double sampleRate, uint64_t unitSeed, uint32_t noiseSeed) {
		fs = sampleRate; unitValues(unitSeed, U, 4); rng.seed(noiseSeed); drift.init(fs);
		for (int j = 0; j < 9; j++) cur[j] = 0;
		cur[0] = 500; cur[1] = 1500; cur[2] = 2500; cur[6] = 3000; cur[7] = 2000;
		for (int j = 0; j < 6; j++) { y1[j] = y2[j] = 0; cf[j][0] = cf[j][1] = cf[j][2] = 0; }
		fy1 = fy2 = fA = fB = fC = 0; ph = lp = hpz = nlp = sh = 0; n = 0;
	}
	double process(const SpFrame& t, double f0, const SpVoiceParams& p) {
		static const double BW[5] = {70.0, 100.0, 160.0, 250.0, 300.0};
		const double age = p.age, size = p.size, whisper = p.whisper;
		const double kf = 1.0 - std::exp(-1.0 / (0.022 * fs)), ka = 1.0 - std::exp(-1.0 / (0.007 * fs)), kq = 1.0 - std::exp(-1.0 / (0.0015 * fs));
		const double clk = 1.0 + age * 0.02 * U[0];
		const double hiss = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 1.5e-4;
		double r = rng.next();
		double dr = drift.tick(r), ck = clk * (1.0 + age * 0.006 * dr);
		const double ns = std::sqrt(fs / 48000.0), cn = 1.0 - std::exp(-2.0 * PI * 5295.0 / fs);
		r *= ns;
		for (int j = 0; j < 3; j++) cur[j] += kf * (t[j] - cur[j]);
		double kk = t[9] > 0.5 ? kq : ka;
		for (int j = 3; j < 6; j++) cur[j] += kk * (t[j] - cur[j]);
		cur[6] = t[6]; cur[7] = t[7];
		cur[8] += ka * (t[8] - cur[8]);
		if (n % 16 == 0) {
			double sc = ck / size;
			for (int j = 0; j < 3; j++) {
				double bw = BW[j] * (j == 0 ? (1.0 + 1.0 * cur[8]) : 1.0);
				spReson(cur[j] * sc, bw, fs, cf[j][0], cf[j][1], cf[j][2]);
			}
			spReson(3500.0 * sc, BW[3], fs, cf[3][0], cf[3][1], cf[3][2]);
			spReson(4500.0 * sc, BW[4], fs, cf[4][0], cf[4][1], cf[4][2]);
			spReson(270.0 * sc, 100.0, fs, cf[5][0], cf[5][1], cf[5][2]);
			spReson(cur[6] * sc, cur[7], fs, fA, fB, fC);
		}
		double dt = std::min(f0 * ck / fs, 0.45);
		ph += dt; if (ph >= 1.0) ph -= 1.0;
		double saw = 2.0 * ph - 1.0 - blep(ph, dt);
		lp += (1.0 - std::exp(-2.0 * PI * 4000.0 / fs)) * (saw - lp);
		double av = cur[3] * (1.0 - whisper), ah = cur[4] + cur[3] * whisper * 1.5;
		nlp += cn * (r - nlp);
		double x = 0.5 * av * lp + 0.25 * ah * nlp;
		double y = x;
		for (int j = 0; j < 5; j++) { double yy = cf[j][0] * y + cf[j][1] * y1[j] + cf[j][2] * y2[j]; y2[j] = y1[j]; y1[j] = yy; y = yy; }
		double nn = cf[5][0] * x + cf[5][1] * y1[5] + cf[5][2] * y2[5]; y2[5] = y1[5]; y1[5] = nn;
		y = y * (1.0 - 0.4 * cur[8]) + 0.8 * cur[8] * nn;
		double ff = fA * r + fB * fy1 + fC * fy2; fy2 = fy1; fy1 = ff;
		y += 0.45 * cur[5] * ff;
		hpz += (1.0 - std::exp(-2.0 * PI * 60.0 / fs)) * (y - hpz); y -= hpz;
		sh += (1.0 - std::exp(-2.0 * PI * 1500.0 / fs)) * (y - sh); y += 1.0 * (y - sh);
		double r2 = rng.next();
		f1out = cur[0] / size;
		n++;
		return std::tanh(1.1 * y) + hiss * r2 * ns;
	}
};

// ---------------------------------------------------------------- CHIP engine
struct SpChip {
	double fs = 48000;
	double U[4] = {};
	static constexpr double CH_SR = 8000.0;
	double F1T[32], F2T[32], F3T[16], ENT[16];
	double clk = 8000;
	double y1[4], y2[4], cf[4][3];
	double a[6], b[6], cur[6];
	double acc = 0, smp = 0, per = 0;
	int fcount = 0, fN = 200;
	uint32_t lf = 0x1ACE;
	double l1 = 0, l2 = 0;
	bool fric = false;
	XS32 rng;
	Drift drift;
	double f1out = 500;
	SpChip() {
		for (int i = 0; i < 32; i++) { F1T[i] = 200.0 * std::pow(1000.0 / 200.0, i / 31.0); F2T[i] = 600.0 * std::pow(2800.0 / 600.0, i / 31.0); }
		for (int i = 0; i < 16; i++) F3T[i] = 1500.0 * std::pow(3500.0 / 1500.0, i / 15.0);
		ENT[0] = 0.0;
		for (int i = 0; i < 15; i++) ENT[i + 1] = std::pow(10.0, (-28.0 + 2.0 * i) / 20.0);
	}
	static double q(const double* tab, int n, double x) {
		int bi = 0; double e = 1e30;
		for (int i = 0; i < n; i++) { double d = std::fabs(tab[i] - x); if (d < e) { e = d; bi = i; } }
		return tab[bi];
	}
	void init(double sampleRate, uint64_t unitSeed, uint32_t noiseSeed) {
		fs = sampleRate; unitValues(unitSeed, U, 4); rng.seed(noiseSeed); drift.init(fs);
		for (int j = 0; j < 4; j++) { y1[j] = y2[j] = 0; cf[j][0] = cf[j][1] = cf[j][2] = 0; }
		for (int j = 0; j < 6; j++) a[j] = b[j] = cur[j] = 0;
		a[0] = 500; a[1] = 1500; a[2] = 2500; a[4] = 80;
		for (int j = 0; j < 6; j++) b[j] = a[j];
		acc = smp = per = 0; fcount = 0; fN = (int)(0.025 * CH_SR); lf = 0x1ACE; l1 = l2 = 0; fric = false;
	}
	double process(const SpFrame& t, double f0, const SpVoiceParams& p) {
		const double age = p.age, size = p.size, whisper = p.whisper;
		clk = CH_SR * (1.0 + age * 0.03 * U[0]);
		double r = rng.next(), dr = drift.tick(r);
		acc += clk * (1.0 + age * 0.01 * dr) / fs;
		while (acc >= 1.0) {
			acc -= 1.0;
			if (fcount == 0) {
				for (int j = 0; j < 6; j++) a[j] = b[j];
				double av = t[3] * (1.0 - whisper), nz = std::max(t[4], t[5]) + t[3] * whisper * 0.6;
				bool voiced = av > 0.3;
				fric = (!voiced) && nz > 0.05;
				if (fric && t[5] > t[4]) {
					double fcn = std::min(t[6] / size, 3600.0);
					b[0] = q(F1T, 32, 0.25 * fcn); b[1] = q(F2T, 32, 0.55 * fcn); b[2] = q(F3T, 16, fcn);
				} else {
					b[0] = q(F1T, 32, t[0] / size); b[1] = q(F2T, 32, t[1] / size); b[2] = q(F3T, 16, std::min(t[2] / size, 3500.0));
				}
				double e = voiced ? av : 0.5 * nz;
				b[3] = q(ENT, 16, e);
				double pidx = std::min(std::max(std::nearbyint(CH_SR / std::max(f0, 20.0)), 15.0), 159.0);
				b[4] = pidx; b[5] = voiced ? 1.0 : 0.0;
				if (b[5] != a[5]) { for (int j = 0; j < 5; j++) a[j] = b[j]; a[5] = b[5]; }
			}
			int sub = (fcount * 8) / fN;
			double w = (sub + 1) / 8.0;
			if (fcount % (fN / 8) == 0) {
				for (int j = 0; j < 5; j++) cur[j] = a[j] + w * (b[j] - a[j]);
				cur[5] = b[5];
				bool wide = fric && cur[5] < 0.5;
				spReson(cur[0], wide ? 600.0 : 80.0, CH_SR, cf[0][0], cf[0][1], cf[0][2]);
				spReson(cur[1], wide ? 800.0 : 100.0, CH_SR, cf[1][0], cf[1][1], cf[1][2]);
				spReson(cur[2], wide ? 500.0 : 140.0, CH_SR, cf[2][0], cf[2][1], cf[2][2]);
				spReson(3300.0, 200.0, CH_SR, cf[3][0], cf[3][1], cf[3][2]);
			}
			double x;
			if (cur[5] > 0.5) {
				per += 1.0;
				if (per >= cur[4]) { per = 0.0; x = cur[3] * 0.35 * std::sqrt(cur[4]); }
				else x = 0.0;
			} else {
				lf = ((lf >> 1) ^ ((lf & 1) ? 0xB400u : 0u)) & 0xFFFFu;
				x = cur[3] * 0.7 * ((lf & 1) ? 1.0 : -1.0);
			}
			double y = x;
			for (int j = 0; j < 4; j++) {
				if (j == 3 && cur[5] < 0.5) continue;
				double yy = cf[j][0] * y + cf[j][1] * y1[j] + cf[j][2] * y2[j]; y2[j] = y1[j]; y1[j] = yy; y = yy;
			}
			y = clampd(y * 1.2, -1.0, 1.0);
			smp = std::nearbyint(y * 127.0) / 127.0;
			fcount++;
			if (fcount >= fN) fcount = 0;
		}
		double g = std::tan(PI * std::min(4500.0, 0.45 * fs) / fs), k = 1.2;
		double a1 = 1.0 / (1.0 + g * (g + k));
		double v3 = smp - l2, v1 = a1 * l1 + g * a1 * v3, v2 = l2 + g * a1 * l1 + g * g * a1 * v3;
		l1 = 2.0 * v1 - l1; l2 = 2.0 * v2 - l2;
		f1out = cur[0];
		return v2;
	}
};

}  // namespace wb
