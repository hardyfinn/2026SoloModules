// Soloist's line engine: writes one phrase at a time over a chart, as a horn player would, and the head
// melody of the tune. Pure and deterministic: the same seed, chart and settings give the same notes.
// No Rack includes. Beats are absolute (the band's clock, phi); bar lines fall on multiples of 4.
//
// A phrase is built in four passes:
//   rhythm  one-beat cells from the groove's library, chosen by density (or a remembered motif's rhythm)
//   targets strong onsets (beats 1 and 3, the first note on a new chord, the last note) land on chord tones,
//           3rd and 7th first, along a contour that rises to a peak and falls back
//   fill    the notes between targets walk by scale step, arpeggio or chromatic approach, with enclosures
//   outside side-slips, altered tensions and tritone substitutes, then (past half) free intervals and
//           crushed rhythm, and at the top free playing that leaves the chart
// then articulation: bebop tonguing, slurs, scoops, falls, vibrato, growl and breath.
#pragma once
#include "Engine.hpp"
#include "Horn.hpp"
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace solo {
using comper::Chord; using comper::Rng; using comper::pmod;

enum Groove { G_SWING = 0, G_BOSSA, G_BALLAD, G_SOUL, G_HOUSE, G_REGGAE };
enum Genre { GEN_JAZZ = 0, GEN_HOUSE, GEN_GOSPEL, GEN_BLUES, GEN_DISCO, GEN_SOUL, GEN_FUNK, GEN_REGGAE, NUM_GENRES };
static const char* const GENRE_NAMES[NUM_GENRES] = {"Jazz", "House", "Gospel", "Blues", "Disco", "Soul", "Funk", "Reggae"};
// the family of a progression, from its name ("House: ...", "Jazz blues in F", ...)
static inline int genreOf(const char* name) {
	if (!name) return GEN_JAZZ;
	struct P { const char* pre; int g; };
	static const P PS[] = {{"House", GEN_HOUSE}, {"Gospel", GEN_GOSPEL}, {"Blues", GEN_BLUES}, {"Disco", GEN_DISCO},
		{"Soul", GEN_SOUL}, {"Funk", GEN_FUNK}, {"Reggae", GEN_REGGAE}};
	for (const P& p : PS) if (!std::strncmp(name, p.pre, std::strlen(p.pre))) return p.g;
	if (std::strstr(name, "blues") || std::strstr(name, "Blues")) return GEN_BLUES;
	return GEN_JAZZ;
}

enum Part { P_IDLE = 0, P_HEAD, P_SOLO, P_TRADE, P_OUT, P_END, P_CUE, NUM_PARTS };
static const char* const PART_NAMES[NUM_PARTS] = {"-", "HEAD", "SOLO", "TRADE", "HEAD OUT", "END", "CUE"};

struct SNote {
	double beat = 0, len = 1;      // absolute beats (unswung grid), length in beats
	int pitch = 60;                // concert MIDI
	double vel = 0.6;
	bool slur = false, gliss = false, accent = false, last = false, first = false;
	double tongue = 0.5, scoop = 0, fall = 0, vib = 0, growl = 0, air = 0;
};

struct SParams {
	int kind = horn::TENOR;
	int groove = G_SWING, genre = GEN_JAZZ;
	double density = 0.5, space = 0.4, motif = 0.4, outside = 0.1;
	double energy = 0.5;           // now (the arc already applied)
	double reg = 0.5;              // 0 low .. 1 high in the horn's range
	double length = 1.0;           // note length multiplier (0.5 .. 1.6)
};

static inline bool sixteenths(int groove, int genre) {
	if (genre == GEN_FUNK || genre == GEN_DISCO || genre == GEN_HOUSE) return groove != G_BALLAD;
	return groove == G_SOUL || groove == G_HOUSE || groove == G_REGGAE;
}
static inline bool isDom(int q) { return q == comper::DOM || q == comper::ALT || q == comper::SUS; }

// chord tones and scale of a chord, as pitch classes relative to C
struct Harm {
	int root = 0, q = 0;
	int ct[5]; int nct = 0;        // root, 3rd, 5th, 7th (or 6th), 9th if it belongs
	int sc[8]; int nsc = 0;
	int g3 = 4, g7 = 10;
	void set(const Chord& c) {
		root = pmod(c.root, 12); q = std::max(0, std::min(comper::NUM_Q - 1, c.q));
		g3 = comper::GUIDES[q][0]; g7 = comper::GUIDES[q][1];
		nsc = comper::SCALE_LEN[q]; for (int i = 0; i < nsc; i++) sc[i] = comper::SCALES[q][i];
		nct = 0; ct[nct++] = 0; ct[nct++] = g3;
		int fifth = (q == comper::HDIM || q == comper::DIM) ? 6 : (q == comper::ALT ? 8 : 7);
		ct[nct++] = fifth;
		if (g7 != 7) ct[nct++] = g7;
		bool nine = q != comper::ALT && q != comper::HDIM && q != comper::DIM && q != comper::TRI && q != comper::MIN;
		if (nine) ct[nct++] = 2;
	}
	bool inScale(int pc) const { int r = pmod(pc - root, 12); for (int i = 0; i < nsc; i++) if (sc[i] == r) return true; return false; }
	bool isCT(int pc) const { int r = pmod(pc - root, 12); for (int i = 0; i < nct; i++) if (ct[i] == r) return true; return false; }
};

static inline int nearestPC(int pc, double around) {       // the pitch of class pc nearest to around
	int base = (int)std::floor(around / 12.0) * 12 + pmod(pc, 12);
	int best = base; double bd = 1e9;
	for (int k = -1; k <= 1; k++) { int c = base + 12 * k; double d = std::fabs(c - around); if (d < bd) { bd = d; best = c; } }
	return best;
}

struct Motif { std::vector<double> on, len; std::vector<int> iv; double frac = 0; };

struct Line {
	Rng r, hr;                     // r: the line; hr: the head (its own stream, so the head never moves)
	std::function<Chord(double)> chordAt;
	std::vector<Motif> mem;
	int lastPitch = -1;
	int seed = 1;
	// what the host can show and send
	double lastPhraseStart = 0, lastPhraseEnd = -1;
	int lastRhythmN = 0; float lastRhythm[16] = {};

	void reset(int s) { seed = s; r = Rng((int64_t)s * 97 + 13); hr = Rng((int64_t)s * 31 + 7); mem.clear(); lastPitch = -1; lastPhraseEnd = -1; lastRhythmN = 0; }

	Harm harmAt(double beat) const { Harm h; h.set(chordAt ? chordAt(beat) : Chord(0, comper::MAJ)); return h; }

	// ---------------------------------------------------------------- ranges
	static void rangeOf(int kind, double reg, double energy, int& lo, int& hi, double& center) {
		int L = horn::RANGE_LO[kind], H = horn::RANGE_HI[kind], T = horn::RANGE_TOP[kind];
		center = L + 5 + (H - L - 10) * std::max(0.0, std::min(1.0, reg)) + 5.0 * (energy - 0.5);
		lo = L; hi = H + (int)std::lround((T - H) * std::max(0.0, (energy - 0.7) / 0.3));
	}
	static int fold(int p, int lo, int hi) { while (p > hi) p -= 12; while (p < lo) p += 12; if (p > hi) p = hi; return p; }

	// ---------------------------------------------------------------- rhythm
	// the onsets of one beat (beats from the beat), from the groove's cells
	void beatCell(const SParams& p, double d, bool sixteen, std::vector<double>& out) {
		out.clear();
		double u = r.u();
		if (p.groove == G_BALLAD && !sixteen) {
			// ballad: long notes, eighths when busy
			double pOn = 0.35 + 0.45 * d, p8 = 0.08 + 0.35 * d * d;
			if (u < pOn) { out.push_back(0.0); if (r.u() < p8) out.push_back(0.5); }
			else if (r.u() < 0.25 + 0.3 * d) out.push_back(0.5);
			return;
		}
		if (!sixteen) {
			bool trip = (p.genre == GEN_JAZZ || p.genre == GEN_BLUES || p.genre == GEN_GOSPEL) && p.groove != G_BOSSA;
			double w[5] = {0.36 - 0.26 * d, 0.18 + 0.62 * d, 0.16, 0.14 * (1 - d), trip ? 0.03 + 0.12 * d : 0.0};
			double tot = 0; for (double x : w) tot += x; u *= tot;
			int k = 0; while (k < 4 && u > w[k]) { u -= w[k]; k++; }
			switch (k) {
				case 0: out.push_back(0.0); break;
				case 1: out.push_back(0.0); out.push_back(0.5); break;
				case 2: out.push_back(0.5); break;
				case 3: break;
				default: out.push_back(0.0); out.push_back(1.0 / 3); out.push_back(2.0 / 3); break;
			}
			return;
		}
		// sixteenth cells, as bit masks (bit 3 = the beat, bit 0 = the last sixteenth)
		static const int CELLS[] = {0x8, 0xA, 0xB, 0xD, 0x2, 0x3, 0x6, 0xF, 0x5, 0x9, 0xE, 0x0};
		const int NC = (int)(sizeof CELLS / sizeof CELLS[0]);
		double target = 0.12 + 0.62 * d; if (p.groove == G_REGGAE || p.genre == GEN_REGGAE) target *= 0.6;
		double w[16], tot = 0;
		for (int i = 0; i < NC; i++) {
			int c = CELLS[i], n = __builtin_popcount(c);
			double x = n / 4.0 - target; w[i] = std::exp(-x * x / 0.05);
			bool off = (c & 0x8) == 0 && c != 0;
			if (off) w[i] *= (p.groove == G_REGGAE || p.genre == GEN_REGGAE) ? 2.2 : (p.genre == GEN_FUNK ? 1.5 : (p.groove == G_HOUSE || p.genre == GEN_HOUSE || p.genre == GEN_DISCO) ? 1.3 : 0.8);
			if (c == 0x2 && (p.groove == G_HOUSE || p.genre == GEN_HOUSE || p.genre == GEN_DISCO || p.groove == G_REGGAE)) w[i] *= 1.8;   // the offbeat
			if (c == 0) w[i] = 0.10 * (1 - d);
			tot += w[i];
		}
		u *= tot; int k = 0; while (k < NC - 1 && u > w[k]) { u -= w[k]; k++; }
		for (int b = 0; b < 4; b++) if (CELLS[k] & (8 >> b)) out.push_back(0.25 * b);
	}

	// ---------------------------------------------------------------- one phrase
	// Writes the phrase that starts at beat b0 and must end before limit. Returns the notes; next = where
	// the following phrase may start (after the rest).
	std::vector<SNote> phrase(double b0, const SParams& p, double spb, double limit, double& next) {
		std::vector<SNote> ns;
		const double d = std::max(0.0, std::min(1.0, 0.12 + 0.62 * p.density + 0.38 * p.energy));
		const bool six = sixteenths(p.groove, p.genre);
		const double o = std::max(0.0, std::min(1.0, p.outside));
		const bool freePlay = o >= 0.92;
		int lo, hi; double center; rangeOf(p.kind, p.reg, p.energy, lo, hi, center);
		if (lastPitch < 0) lastPitch = (int)std::lround(center);

		// length: 1 to 4 bars, never longer than a breath
		double lens[] = {2, 3, 4, 6, 8, 12, 16};
		double want = 4.0 + 10.0 * (0.55 * d + 0.45 * p.energy) + 3.0 * r.g();
		double best = lens[0]; for (double L : lens) if (std::fabs(L - want) < std::fabs(best - want)) best = L;
		double breathS = 7.5 - 3.0 * p.energy;
		double L = std::min(best, std::max(2.0, breathS / spb));
		if (p.groove == G_BALLAD) L = std::min(L, 8.0);
		double end = std::min(b0 + L, limit);
		if (end - b0 < 0.75) { next = limit; return ns; }

		// --- rhythm: a remembered motif, or fresh cells
		std::vector<double> on, ln;
		bool useMotif = !mem.empty() && r.u() < p.motif * 0.85 && !freePlay;
		int motifShape = 0; const Motif* mo = nullptr;
		if (useMotif) {
			mo = &mem[mem.size() - 1 - (r.u() < 0.6 ? 0 : r.n((int)mem.size()))];
			double u = r.u(); motifShape = u < 0.35 ? 0 : u < 0.55 ? 1 : u < 0.8 ? 2 : u < 0.9 ? 3 : 4;   // same, inverted, sequenced, backwards, cut
			// the motif keeps its place in the beat
			double st = std::floor(b0) + mo->frac; if (st < b0 - 1e-9) st += 1.0;
			double span = mo->on.empty() ? 0 : mo->on.back() + mo->len.back();
			int reps = (motifShape == 2 || motifShape == 4) ? 2 : 1;
			double hop = std::ceil(span + 0.5);
			for (int rp = 0; rp < reps; rp++) {
				size_t i0 = motifShape == 4 ? std::max<size_t>(0, mo->on.size() > 3 ? mo->on.size() - 3 : 0) : 0;
				double base = st + rp * (motifShape == 4 ? std::ceil(span - (mo->on.empty() ? 0 : mo->on[i0]) + 0.5) : hop) - (motifShape == 4 ? mo->on[i0] : 0.0);
				for (size_t i = i0; i < mo->on.size(); i++) { double b = base + mo->on[i]; if (b < end - 0.2) { on.push_back(b); ln.push_back(mo->len[i]); } }
			}
			if (on.size() < 2) { useMotif = false; on.clear(); ln.clear(); }
		}
		if (!useMotif) {
			std::vector<double> cell;
			for (double bt = std::floor(b0); bt < end - 1e-9; bt += 1.0) {
				if (p.groove == G_BALLAD && !six && pmod((int)std::floor(bt), 2) == 1 && r.u() < 0.5 - 0.3 * d) continue;   // ballad: let notes ring over two beats
				beatCell(p, d, six, cell);
				if (o > 0.5 && r.u() < 0.3 * (o - 0.5) * 2) {   // crushed: five or six to the beat
					int n = r.u() < 0.5 ? 5 : 6; cell.clear(); for (int i = 0; i < n; i++) cell.push_back(i / (double)n);
				}
				for (double c : cell) { double b = bt + c; if (b >= b0 - 1e-9 && b < end - 0.24) on.push_back(b); }
			}
			if (on.empty()) on.push_back(b0);
			if (freePlay) {   // free: bursts and held cries
				on.clear();
				double b = b0;
				while (b < end - 0.25) {
					if (r.u() < 0.45 + 0.3 * p.energy) {   // a burst
						int n = 3 + r.n(6 + (int)(6 * p.energy)); double step = 1.0 / (4 + r.n(4));
						for (int i = 0; i < n && b < end - 0.2; i++) { on.push_back(b); b += step * (0.8 + 0.4 * r.u()); }
					} else { on.push_back(b); b += 1.0 + 2.0 * r.u(); }
					b += r.u() < 0.4 ? 0.5 + r.u() : 0.0;
				}
				if (on.empty()) on.push_back(b0);
			}
			for (size_t i = 0; i < on.size(); i++) ln.push_back((i + 1 < on.size() ? on[i + 1] : end) - on[i]);
		}
		// past half on OUTSIDE the grid goes soft
		if (o > 0.5 && !freePlay) for (size_t i = 1; i < on.size(); i++) on[i] += (o - 0.5) * 0.2 * r.g() * 0.5;
		std::sort(on.begin(), on.end());
		const int N = (int)on.size();
		std::vector<double> len(N);
		for (int i = 0; i < N; i++) len[i] = (i + 1 < N ? on[i + 1] : end) - on[i];
		// the last note is held: a phrase ends on a long note
		{
			double want = six ? 0.75 : 1.0; if (p.groove == G_BALLAD) want = 2.0;
			len[N - 1] = std::max(len[N - 1], std::min(want, limit - on[N - 1] - 0.05));
			if (len[N - 1] < 0.2) len[N - 1] = 0.2;
		}

		// --- pitches
		std::vector<int> pitch(N, lastPitch);
		std::vector<bool> strong(N, false);
		for (int i = 0; i < N; i++) {
			double b = on[i], fr = b - std::floor(b);
			int beatInBar = pmod((int)std::floor(b + 1e-9), 4);
			bool onBeat = fr < 0.02;
			Chord c0 = chordAt ? chordAt(b) : Chord(), cp = chordAt ? chordAt(b - 0.5) : Chord();
			strong[i] = (onBeat && (beatInBar == 0 || beatInBar == 2)) || (onBeat && c0 != cp) || i == 0 || i == N - 1 || len[i] >= 1.5;
		}
		const double peakAt = 0.45 + 0.25 * r.u();
		const double rise = 3.0 + 9.0 * p.energy + 2.0 * r.g();
		auto contour = [&](int i) {
			double x = N > 1 ? i / (double)(N - 1) : 0.0;
			double y = x < peakAt ? x / peakAt : 1.0 - (x - peakAt) / (1.0 - peakAt) * 1.2;
			return center + (lastPitch - center) * 0.5 * (1 - x) + rise * y - 0.5 * rise * 0.4;
		};
		const bool penta = p.genre == GEN_HOUSE || p.genre == GEN_SOUL || p.genre == GEN_GOSPEL || p.genre == GEN_FUNK || p.genre == GEN_REGGAE || p.genre == GEN_DISCO || p.genre == GEN_BLUES;
		// the scale a fill walks on at beat b (genre colours, tritone substitutes)
		auto walkScale = [&](double b, std::vector<int>& sc) {
			Harm h = harmAt(b); sc.clear();
			int root = h.root;
			if (o > 0.15 && isDom(h.q) && r.u() < 0.5 * o) root = pmod(root + 6, 12);   // tritone substitute
			if (penta && r.u() < 0.7) {
				bool minorish = comper::minorFamily(h.q) || h.q == comper::DOM || p.genre == GEN_BLUES;
				static const int MP[5] = {0, 3, 5, 7, 10}, MJ[5] = {0, 2, 4, 7, 9}, BL[6] = {0, 3, 5, 6, 7, 10};
				if (p.genre == GEN_BLUES && minorish) for (int x : BL) sc.push_back(pmod(root + x, 12));
				else if (minorish) for (int x : MP) sc.push_back(pmod(root + x, 12));
				else for (int x : MJ) sc.push_back(pmod(root + x, 12));
			} else for (int i = 0; i < h.nsc; i++) sc.push_back(pmod(root + h.sc[i], 12));
		};
		auto scaleStep = [&](int from, int dir, const std::vector<int>& sc) {
			for (int k = 1; k <= 4; k++) { int c = from + dir * k; for (int x : sc) if (pmod(c, 12) == x) return c; }
			return from + dir * 2;
		};
		// pick a target at strong onset i
		auto pickTarget = [&](int i, int prev) {
			double b = on[i]; Harm h = harmAt(b);
			double around = contour(i) * 0.6 + prev * 0.4 + 2.0 * r.g();
			if (freePlay) {
				static const int IV[] = {1, 2, 5, 6, 7, 10, 11, 13};
				int s = IV[r.n(8)] * (r.u() < 0.5 ? -1 : 1); int q = prev + s;
				if (std::fabs(q - around) > 9) q = prev - s;
				return q;
			}
			if (o > 0.5 && r.u() < (o - 0.5) * 2 * 0.8) {   // past half: any note, by an open interval
				static const int IV[] = {5, 6, 7, 10, 11};
				int s = IV[r.n(5)] * (around > prev ? 1 : -1); return prev + s;
			}
			int rel;
			double u = r.u();
			if (isDom(h.q) && o > 0.05 && r.u() < 0.8 * o) { static const int ALT[4] = {1, 3, 8, 6}; rel = ALT[r.n(4)]; }
			else if (u < 0.30) rel = h.g3;
			else if (u < 0.58) rel = h.g7;
			else if (u < 0.74) rel = h.ct[2];
			else if (u < 0.86) rel = 0;
			else rel = h.nct > 4 ? 2 : h.g3;
			if (penta && p.genre == GEN_BLUES && r.u() < 0.25) rel = (h.q == comper::DOM || h.q == comper::TRI || h.q == comper::MAJ || h.q == comper::MAJ6) ? 3 : rel;   // the blue third (scooped up below)
			return nearestPC(h.root + rel, around);
		};
		int prev = lastPitch;
		std::vector<int> strongIdx; for (int i = 0; i < N; i++) if (strong[i]) strongIdx.push_back(i);
		std::vector<int> sc;
		for (size_t si = 0; si < strongIdx.size(); si++) {
			int i = strongIdx[si];
			int T = useMotif ? prev : pickTarget(i, prev);
			if (!useMotif) {
				T = fold(T, lo, hi);
				// keep leaps singable: a target more than a sixth away is moved an octave towards the line
				if (!freePlay) { while (T - prev > 9 && T - 12 >= lo) T -= 12; while (prev - T > 9 && T + 12 <= hi) T += 12; }
				pitch[i] = T;
			}
			// fill up to the next target
			int k = si + 1 < strongIdx.size() ? strongIdx[si + 1] : N;
			if (useMotif) continue;
			int n = k - i - 1; if (n <= 0) { prev = pitch[i]; continue; }
			int Tn = k < N ? fold(pickTarget(k, pitch[i]), lo, hi) : pitch[i];
			if (k < N) {
				if (!freePlay) { while (Tn - pitch[i] > 9 && Tn - 12 >= lo) Tn -= 12; while (pitch[i] - Tn > 9 && Tn + 12 <= hi) Tn += 12; }
				pitch[k] = Tn;
			}
			walkScale(on[i + 1], sc);
			int cur = pitch[i];
			bool enclose = k < N && n >= 2 && r.u() < (p.genre == GEN_JAZZ ? 0.35 : 0.15) && !freePlay;
			int walkN = enclose ? n - 2 : n;
			int dest = enclose ? scaleStep(Tn, 1, sc) : Tn;
			// time to spare: go the other way first (an arpeggio out, a scale back), never trill on the spot
			const int needSteps = (int)std::ceil(std::abs(dest - cur) / 1.7);
			const int outMax = std::max(0, walkN - needSteps) / 2;
			const int detour = (dest > cur) ? -1 : (dest < cur ? 1 : (contour(i) > cur ? 1 : -1));
			const bool arpOut = outMax >= 2 && r.u() < 0.5;
			int outDone = 0;
			for (int j = 0; j < walkN; j++) {
				int idx = i + 1 + j;
				if (freePlay) { static const int IV[] = {1, 2, 3, 5, 6, 11}; cur += IV[r.n(6)] * (r.u() < 0.5 ? -1 : 1); }
				else if (o > 0.5 && r.u() < (o - 0.5) * 1.2) { cur += (r.u() < 0.5 ? 5 : 6) * (dest >= cur ? 1 : -1); }   // fourths and tritones
				else {
					int remaining = walkN - j;
					int dist = dest - cur;
					int dir = dist > 0 ? 1 : (dist < 0 ? -1 : (r.u() < 0.5 ? 1 : -1));
					// far to go: arpeggiate; nowhere to go: turn around
					auto arp = [&](int d) {
						Harm h = harmAt(on[idx]); int c2 = cur + d;
						for (int s = 2; s <= 6; s++) { int c = cur + d * s; if (h.isCT(c)) { c2 = c; break; } }
						return c2;
					};
					if (std::abs(dist) > 2 * remaining + 2) cur = arp(dir);
					else if (outDone < outMax) { cur = arpOut ? arp(detour) : scaleStep(cur, detour, sc); outDone++; }
					else if (dist == 0) cur = scaleStep(cur, detour, sc);
					else cur = scaleStep(cur, dir, sc);
					// chromatic approach into the next target
					if (remaining == 1 && k < N && !enclose && r.u() < (p.genre == GEN_JAZZ ? 0.3 : 0.12)) cur = Tn + (cur > Tn ? 1 : -1);
				}
				cur = fold(cur, lo, hi);
				pitch[idx] = cur;
			}
			if (enclose) { pitch[k - 2] = fold(scaleStep(Tn, 1, sc), lo, hi); pitch[k - 1] = fold(Tn - 1, lo, hi); }
			prev = cur;
		}
		if (useMotif && mo) {
			// the motif's intervals, from a new chord tone near the last note
			int start = fold(pickTarget(0, lastPitch), lo, hi);
			std::vector<int> iv = mo->iv;
			if (motifShape == 1) for (int& x : iv) x = -x;
			if (motifShape == 3) std::reverse(iv.begin(), iv.end());
			int cur = start; int seqShift = 0;
			size_t m = std::max<size_t>(1, mo->on.size());
			for (int i = 0; i < N; i++) {
				size_t j = i % m;
				if (i > 0 && j == 0) { seqShift = (motifShape == 2) ? (r.u() < 0.5 ? 2 : -2) : 0; cur = pitch[i - 1] + seqShift - (iv.empty() ? 0 : 0); }
				if (j > 0 && j - 1 < iv.size()) cur += iv[j - 1];
				else if (i > 0 && j == 0) cur = fold(nearestPC(harmAt(on[i]).root + harmAt(on[i]).g3, cur + seqShift), lo, hi);
				// strong notes snap to the nearest chord tone, weak notes to the scale (unless outside)
				Harm h = harmAt(on[i]);
				if (o < 0.5) {
					if (strong[i] && !h.isCT(cur)) { for (int s = 1; s <= 2; s++) { if (h.isCT(cur + s)) { cur += s; break; } if (h.isCT(cur - s)) { cur -= s; break; } } }
					else if (!h.inScale(cur) && r.u() < 0.6) cur += h.inScale(cur + 1) ? 1 : -1;
				}
				cur = fold(cur, lo, hi);
				pitch[i] = cur;
			}
		}
		// side-slips: a span a semitone up or down, resolved by the next strong note
		if (!freePlay && o > 0.05 && N > 3 && r.u() < 0.7 * o) {
			double spanB = o < 0.5 ? 1.0 : 1.0 + (o - 0.5) * 2 * 7.0;
			int a = 1 + r.n(N - 2); int dir = r.u() < 0.6 ? 1 : -1;
			for (int i = a; i < N - 1 && on[i] < on[a] + spanB; i++) pitch[i] = fold(pitch[i] + dir, lo, hi);
		}
		// no more than two of the same note in a row (unless it is a motif's riff)
		if (!useMotif) for (int i = 2; i < N; i++) if (pitch[i] == pitch[i - 1] && pitch[i] == pitch[i - 2]) {
			Harm h = harmAt(on[i]); int d = (i + 1 < N && pitch[i + 1] < pitch[i]) ? -1 : 1;
			int c = pitch[i] + d; for (int s = 1; s <= 4; s++) { c = pitch[i] + d * s; if (strong[i] ? h.isCT(c) : h.inScale(c)) break; }
			pitch[i] = fold(c, lo, hi);
		}
		// a leap of more than a fourth is followed by a step back (the weak notes only)
		if (!freePlay) for (int i = 2; i < N; i++) {
			int lp = pitch[i - 1] - pitch[i - 2];
			if (std::abs(lp) > 5 && !strong[i]) { int dir = lp > 0 ? -1 : 1; if ((pitch[i] - pitch[i - 1]) * dir <= 0) pitch[i] = fold(pitch[i - 1] + dir * (r.u() < 0.6 ? 2 : 1), lo, hi); }
		}

		// --- articulation
		int peakI = 0; for (int i = 0; i < N; i++) if (pitch[i] > pitch[peakI]) peakI = i;
		const bool jazzy = !six && p.groove != G_BALLAD;
		double legato = six ? (p.genre == GEN_FUNK ? 0.55 : (p.groove == G_REGGAE || p.genre == GEN_REGGAE) ? 0.6 : 0.78) : (p.groove == G_BALLAD ? 0.98 : 0.94);
		for (int i = 0; i < N; i++) {
			SNote n; n.beat = on[i]; n.pitch = pitch[i];
			double gap = i + 1 < N ? on[i + 1] - on[i] : len[i];
			n.len = std::max(0.08, std::min(gap * legato * p.length, i + 1 < N ? gap - 0.02 : len[i] * p.length));
			if (i == N - 1) n.len = std::min(std::max(0.2, len[i] * std::max(0.8, p.length)), limit - on[i] - 0.02);
			double fr = on[i] - std::floor(on[i]);
			bool offEighth = std::fabs(fr - 0.5) < 0.02;
			double prevGap = i > 0 ? on[i] - on[i - 1] : 99;
			// tonguing: bebop tongues the off-beat eighths and slurs into the beat
			if (jazzy) {
				n.slur = i > 0 && fr < 0.02 && prevGap <= 0.75 && !freePlay;
				n.tongue = offEighth ? 0.6 : 0.35;
				if (offEighth && prevGap <= 0.75) n.accent = true;
			} else if (six) {
				n.slur = i > 0 && prevGap <= 0.3 && r.u() < (p.genre == GEN_FUNK ? 0.15 : 0.35);
				n.tongue = p.genre == GEN_FUNK ? 0.85 : 0.6;
				n.accent = (p.genre == GEN_FUNK && fr > 0.01) || (std::fabs(fr - 0.5) < 0.02 && (p.groove == G_HOUSE || p.groove == G_REGGAE));
			} else {   // ballad
				n.slur = i > 0 && prevGap <= 1.6 && r.u() < 0.7;
				n.tongue = 0.2;
			}
			if (i == 0) { n.slur = false; n.first = true; n.tongue = std::min(1.0, 0.45 + 0.4 * p.energy); }
			// dynamics: the phrase swells to its peak and eases at the end
			double x = N > 1 ? i / (double)(N - 1) : 0;
			double arc = x < peakAt ? 0.08 * x / peakAt : 0.08 - 0.16 * (x - peakAt) / (1 - peakAt);
			n.vel = 0.30 + 0.52 * p.energy + arc + (n.accent ? 0.10 : 0.0) + (strong[i] ? 0.03 : -0.02) + 0.03 * r.g();
			n.vel = std::max(0.08, std::min(1.0, n.vel));
			double secs = n.len * spb;
			// vibrato on long notes, late
			if (secs > 0.45) n.vib = std::min(1.0, 0.35 + 0.5 * (secs - 0.45) + (p.groove == G_BALLAD ? 0.2 : 0.0));
			// scoops into peaks and long notes; the blue third gets its scoop from below
			if ((i == peakI || secs > 0.5) && r.u() < 0.3 + ((p.genre == GEN_BLUES || p.genre == GEN_GOSPEL || p.genre == GEN_SOUL) ? 0.3 : 0.0)) n.scoop = 0.7 + 1.3 * r.u();
			// growl: tenor when the energy is high, everyone when far out
			double gE = p.kind == horn::TENOR ? (p.energy - 0.72) / 0.28 : (p.energy - 0.85) / 0.15 * 0.6;
			if ((secs > 0.3 || i == peakI) && gE > 0) n.growl = std::min(1.0, gE * (0.6 + 0.4 * r.u()));
			if (o > 0.8) n.growl = std::max(n.growl, (o - 0.8) / 0.2 * (secs > 0.25 ? 0.9 : 0.4));
			// breath
			if (p.groove == G_BALLAD) n.air = p.kind == horn::TENOR ? 0.55 : 0.25;
			if (p.energy < 0.4) n.air = std::max(n.air, (0.4 - p.energy) * (p.kind == horn::TENOR ? 1.2 : 0.5));
			// trombone slides; blues smears
			if (n.slur && i > 0) {
				int iv = std::abs(pitch[i] - pitch[i - 1]);
				double pg = p.kind == horn::TROMBONE ? (p.genre == GEN_BLUES ? 0.6 : 0.3) : (p.genre == GEN_BLUES || p.genre == GEN_GOSPEL ? 0.2 : 0.0);
				if (iv >= 2 && iv <= 7 && r.u() < pg) n.gliss = true;
			}
			ns.push_back(n);
		}
		// the end of the phrase: held, with a fall or a fade
		{
			SNote& e = ns.back(); e.last = true;
			double pf = 0.25 + ((p.genre == GEN_BLUES || p.genre == GEN_GOSPEL || p.genre == GEN_SOUL) ? 0.3 : 0.0) + 0.2 * p.energy;
			if (e.len * spb > 0.3 && r.u() < pf) e.fall = 2.0 + 4.0 * r.u();
			if (freePlay && r.u() < 0.5) { e.scoop = 4.0 + 3.0 * r.u(); e.growl = 1.0; }   // a cry
		}
		// remember it
		lastPitch = pitch[N - 1];
		lastPhraseStart = on[0]; lastPhraseEnd = on[N - 1];
		{
			double bar0 = std::floor(on[0] / 4.0) * 4.0; lastRhythmN = 0;
			for (int i = 0; i < N && lastRhythmN < 16; i++) { double x = on[i] - bar0; if (x < 4.0) lastRhythm[lastRhythmN++] = (float)x; }
		}
		if (!useMotif && N >= 3 && !freePlay) {
			Motif m; m.frac = on[0] - std::floor(on[0]);
			int take = std::min(N, 3 + r.n(5));
			for (int i = 0; i < take; i++) { m.on.push_back(on[i] - on[0]); m.len.push_back(len[i]); if (i > 0) m.iv.push_back(pitch[i] - pitch[i - 1]); }
			mem.push_back(m); if (mem.size() > 4) mem.erase(mem.begin());
		}
		// the rest
		double rest = 0.5 + p.space * 5.5 * (0.4 + 0.8 * r.u()) * (1.15 - 0.5 * p.energy);
		if (p.groove == G_BALLAD) rest += 1.0;
		double nx = on[N - 1] + std::max(len[N - 1], 0.5) + rest;
		// start on an eighth (sixteenth for the sixteenth grooves), sometimes a pickup before the bar
		double grid = six ? 0.25 : 0.5;
		nx = std::ceil(nx / grid - 1e-9) * grid;
		if (r.u() < 0.3) { double bar = std::ceil(nx / 4.0) * 4.0; if (bar - nx < 2.0 && bar - nx > 0) nx = bar - (r.u() < 0.5 ? 0.5 : 1.0); }
		next = nx;
		return ns;
	}

	// ---------------------------------------------------------------- the head
	// The tune's melody over `beats` beats from beat 0 of the chart (chordAt must cover them): long notes on
	// guide tones that lead smoothly from one chord to the next, one rhythm motif per two bars repeated
	// with a closing rhythm at the end of each eight, and the last note on the tonic held into the end.
	std::vector<SNote> head(double beats, const SParams& p, int finalRoot, int finalQual) {
		std::vector<SNote> ns;
		Rng& g = hr;
		const bool six = sixteenths(p.groove, p.genre);
		struct RN { double b, l; };
		// two-bar rhythms (beats, lengths)
		static const RN R8[][7] = {
			{{0, 1.5}, {1.5, 0.5}, {2, 1}, {3, 1}, {4, 3.5}, {-1, 0}},
			{{0.5, 1}, {1.5, 0.5}, {2, 2}, {4, 1}, {5, 1}, {6, 1.5}, {-1, 0}},
			{{0, 2}, {2, 1}, {3, 1}, {4, 4}, {-1, 0}},
			{{0, 1}, {1, 1}, {2, 0.5}, {2.5, 1.5}, {4, 3}, {-1, 0}},
			{{0, 0.5}, {0.5, 0.5}, {1, 1.5}, {2.5, 1.5}, {4, 2}, {6, 1.5}, {-1, 0}},
		};
		static const RN R16[][7] = {
			{{0, 0.75}, {0.75, 0.75}, {1.5, 0.5}, {2, 1.5}, {4, 0.5}, {4.5, 1}, {5.5, 2.5}},
			{{0.5, 0.5}, {1, 0.5}, {1.5, 1}, {2.5, 1.5}, {4.5, 0.5}, {5, 1}, {6, 2}},
			{{0, 1.5}, {1.5, 0.5}, {2, 0.5}, {2.5, 1.5}, {4, 3.5}, {-1, 0}},
		};
		static const RN RB[][7] = {   // ballad
			{{0, 3}, {3, 1}, {4, 4}, {-1, 0}},
			{{0, 2}, {2, 1.5}, {3.5, 0.5}, {4, 4}, {-1, 0}},
		};
		static const RN CLOSE[] = {{0, 1}, {1, 1}, {2, 6}, {-1, 0}};
		const RN* motif;
		if (p.groove == G_BALLAD) motif = RB[g.n(2)];
		else if (six) motif = R16[g.n(3)];
		else motif = R8[g.n(5)];
		int lo, hi; double center; rangeOf(p.kind, p.reg, 0.45, lo, hi, center);
		int prev = (int)std::lround(center);
		int nTwo = (int)std::ceil(beats / 8.0 - 1e-9);
		for (int t = 0; t < nTwo; t++) {
			double base = t * 8.0;
			bool closing = (t % 4 == 3) || t == nTwo - 1;
			const RN* rr = closing ? CLOSE : motif;
			for (int i = 0; i < 7 && rr[i].b >= 0; i++) {
				double b = base + rr[i].b; if (b >= beats - 0.01) break;
				double l = std::min(rr[i].l, beats - b);
				Harm h = harmAt(b);
				int pc;
				bool longish = l >= 1.0 || std::fabs(b - std::floor(b)) < 0.01;
				if (longish) pc = h.root + (g.u() < 0.55 ? h.g3 : (g.u() < 0.6 ? h.g7 : h.ct[2]));
				else { int d = g.u() < 0.5 ? 1 : -1; pc = prev + d * (g.u() < 0.6 ? 2 : 1); while (!h.inScale(pc)) pc += d; }
				int q = longish ? nearestPC(pc, prev * 0.7 + center * 0.3) : pc;
				q = fold(q, lo, std::min(hi, (int)center + 9));
				SNote n; n.beat = b; n.len = std::max(0.25, l * 0.95); n.pitch = q;
				n.vel = 0.5 + 0.1 * (longish ? 1 : 0) + 0.03 * g.g();
				n.slur = !six && i > 0 && rr[i].b - rr[i - 1].b <= 1.0; n.tongue = six ? 0.6 : 0.4; n.first = i == 0;
				if (n.len * 1.0 >= 1.5) n.vib = 0.6;
				if (g.u() < 0.2 && longish) n.scoop = 1.0;
				ns.push_back(n); prev = q;
			}
		}
		// the last note: the tonic chord's third or root, held
		{
			Harm h; h.set(Chord(finalRoot, finalQual));
			SNote n; n.beat = beats; n.len = 4.0; n.pitch = fold(nearestPC(h.root + (g.u() < 0.5 ? h.g3 : 0), prev), lo, hi);
			n.vel = 0.62; n.vib = 0.8; n.tongue = 0.5; n.last = true;
			ns.push_back(n);
		}
		return ns;
	}
};

// The energy arc over a solo: low, up to a peak about two thirds through, and back down.
static inline double arcShape(double x) {
	x = std::max(0.0, std::min(1.0, x));
	if (x < 0.67) { double y = x / 0.67; return y * y * (3 - 2 * y); }
	double y = (x - 0.67) / 0.33; return 1.0 - 0.7 * y * y;
}

} // namespace solo
