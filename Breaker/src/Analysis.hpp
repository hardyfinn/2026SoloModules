// Breaker - break analysis (no Rack dependencies, so the test program can use it too).
//
// 1. Split the break into four bands with simple filters: SUB (<120 Hz), BODY
//    (120-500 Hz), MID (500-3000 Hz) and AIR (>5 kHz).
// 2. Find hits: every ~3 ms, how much louder each band got compared with the last
//    few frames ("spectral flux"). Peaks that stand out from the local median are hits.
// 3. Sort each hit by its first 60 ms: lots of SUB = kick, lots of AIR and little
//    SUB = hat (or "other" when it rings on like a cymbal), MID + AIR noise = snare,
//    anything else (toms, percussion, stabs) = other.
// 4. Guess how many bars the loop is (assumes 4/4 and a loop of whole bars) and
//    the tempo, then place every hit on a 16th-note grid.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace breaker {

enum Lane { KICK = 0, SNARE, HAT, OTHER, NUM_LANES };
static const int MAX_BARS = 4;          // pattern length
static const int STEPS = 16;            // per bar
static const int MAX_LOOP_BARS = 8;     // break length we place on the grid

struct Audio {
	std::vector<float> L, R;
	float rate = 44100.f;
	size_t size() const { return L.size(); }
};

struct Hit {
	int64_t start = 0, end = 0;   // frames
	int type = OTHER;
	int autoType = OTHER;         // what the analyser said (before your clicks)
	float loud = 0.f;             // peak level in the first 60 ms
	float rel = 1.f;              // loudness relative to the loudest hit of its type
	int step = 0;                 // 16th-note position in the loop (0 .. loopBars*16-1)
	bool ghost = false;           // quiet compared with others of its type
	float sub = 0, body = 0, mid = 0, air = 0, ring = 0;   // features (for the test print-out)
};

struct BreakData {
	std::shared_ptr<const Audio> audio;
	std::string name;
	std::vector<Hit> hits;
	std::vector<int> byType[NUM_LANES];   // hit indices, loudest first
	int loopBars = 1;
	bool barsGuessed = true;
	float bpm = 120.f;
	// the break's own pattern: hit index at [loopBar][step][lane], or -1
	int orig[MAX_LOOP_BARS][STEPS][NUM_LANES];
	// waveform overview for the display
	static const int PEAKS = 600;
	float peakMax[PEAKS], peakMin[PEAKS];
};

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void set(bool highpass, double f, double fs, double q = 0.7071) {
		double w = 2 * M_PI * f / fs, c = std::cos(w), s = std::sin(w), al = s / (2 * q), a0 = 1 + al;
		if (highpass) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
		else { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
		b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
	}
	float step(float x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return (float) y;
	}
};

// filter a whole signal through a chain of biquads (24 dB/oct when two are used)
inline std::vector<float> filterChain(const std::vector<float>& x, std::vector<Biquad> chain) {
	std::vector<float> y(x.size());
	for (size_t i = 0; i < x.size(); i++) {
		float v = x[i];
		for (auto& b : chain) v = b.step(v);
		y[i] = v;
	}
	return y;
}

// A reclassification you made by clicking a tag: kept across re-analysis by position.
struct Override {
	int64_t frame;
	int type;
};

// sens 0..1 (more = more hits). barsOverride 0 = guess.
// bySound: for samples that aren't drum breaks (voices, synths, field recordings).
// Instead of looking for kick/snare/hat sounds, it ranks the hits against each
// other: the quietest fifth become HAT, then of the rest the darkest third KICK,
// the brightest third SNARE and the middle third OTHER. Every lane gets sounds.
inline BreakData* analyse(std::shared_ptr<const Audio> audio, const std::string& name, float sens,
		int barsOverride, const std::vector<Override>& overrides, bool bySound = false) {
	BreakData* d = new BreakData();
	d->audio = audio;
	d->name = name;
	for (auto& a : d->orig) for (auto& b : a) for (int& c : b) c = -1;
	const Audio& A = *audio;
	const size_t N = A.size();
	const double fs = A.rate;
	std::vector<float> mono(N);
	for (size_t i = 0; i < N; i++) mono[i] = 0.5f * (A.L[i] + A.R[i]);

	// overview peaks
	for (int p = 0; p < BreakData::PEAKS; p++) {
		size_t a = N * p / BreakData::PEAKS, b = std::max(a + 1, N * (p + 1) / BreakData::PEAKS);
		float mx = 0, mn = 0;
		for (size_t i = a; i < b && i < N; i++) { mx = std::max(mx, mono[i]); mn = std::min(mn, mono[i]); }
		d->peakMax[p] = mx; d->peakMin[p] = mn;
	}
	if (N < (size_t) (fs * 0.1)) return d;

	// ---- bands
	auto bq = [&](bool hp, double f) { Biquad b; b.set(hp, std::min(f, fs * 0.45), fs); return b; };
	std::vector<float> sub = filterChain(mono, {bq(false, 120), bq(false, 120)});
	std::vector<float> body = filterChain(mono, {bq(true, 120), bq(true, 120), bq(false, 500), bq(false, 500)});
	std::vector<float> mid = filterChain(mono, {bq(true, 500), bq(true, 500), bq(false, 3000), bq(false, 3000)});
	std::vector<float> air = filterChain(mono, {bq(true, 5000), bq(true, 5000)});
	const std::vector<float>* bands[4] = {&sub, &body, &mid, &air};

	// ---- onset detection function (multi-band flux, in dB)
	const int hop = std::max(32, (int) std::lround(fs * 0.0029));
	const int F = (int) (N / hop);
	std::vector<float> db[5];
	for (int b = 0; b < 5; b++) db[b].assign(F, -120.f);
	float maxDb[5] = {-120, -120, -120, -120, -120};
	for (int f = 0; f < F; f++) {
		for (int b = 0; b < 5; b++) {
			const std::vector<float>& s = b < 4 ? *bands[b] : mono;
			double e = 0;
			for (int i = 0; i < hop; i++) { float v = s[f * hop + i]; e += v * v; }
			float v = 10.f * std::log10((float) (e / hop) + 1e-12f);
			db[b][f] = v;
			maxDb[b] = std::max(maxDb[b], v);
		}
	}
	for (int b = 0; b < 5; b++)
		for (float& v : db[b]) v = std::max(v, maxDb[b] - 60.f);
	std::vector<float> odf(F, 0.f);
	for (int f = 3; f < F; f++) {
		float sum = 0;
		for (int b = 0; b < 5; b++) {
			float prev = std::max(db[b][f - 1], std::max(db[b][f - 2], db[b][f - 3]));
			sum += std::max(0.f, db[b][f] - prev);
		}
		odf[f] = sum;
	}
	// ---- peak picking
	sens = std::min(1.f, std::max(0.f, sens));
	const float delta = 22.f - 17.f * sens;                         // dB above the local median
	const float floorDb = maxDb[4] - (28.f + 26.f * sens);          // ignore very quiet stuff
	const int minGap = std::max(1, (int) (fs * (0.075 - 0.045 * sens) / hop));
	const int medW = std::max(3, (int) (fs * 0.12 / hop));
	std::vector<int> onsets;
	for (int f = 3; f < F; f++) {
		bool peak = true;
		for (int k = -3; k <= 3 && peak; k++)
			if (k && f + k >= 0 && f + k < F && odf[f + k] > odf[f]) peak = false;
		if (!peak) continue;
		int a = std::max(0, f - medW), b = std::min(F, f + medW);
		std::vector<float> w(odf.begin() + a, odf.begin() + b);
		std::nth_element(w.begin(), w.begin() + w.size() / 2, w.end());
		if (odf[f] < w[w.size() / 2] + delta) continue;
		float lvl = -200;
		for (int k = 0; k < 6 && f + k < F; k++) lvl = std::max(lvl, db[4][f + k]);
		if (lvl < floorDb) continue;
		if (!onsets.empty() && f - onsets.back() < minGap) {
			if (odf[f] > odf[onsets.back()]) onsets.back() = f;
			continue;
		}
		onsets.push_back(f);
	}
	// a loop nearly always starts with a hit: make sure frame 0 counts if it's loud
	if ((onsets.empty() || onsets[0] * hop > fs * 0.03)) {
		float lvl = -200;
		for (int k = 0; k < 8 && k < F; k++) lvl = std::max(lvl, db[4][k]);
		if (lvl > floorDb) onsets.insert(onsets.begin(), 0);
	}

	// ---- refine each onset to the sample where the attack starts
	std::vector<int64_t> starts;
	for (int f : onsets) {
		int64_t a = std::max<int64_t>(0, (int64_t) (f - 2) * hop), b = std::min<int64_t>(N, (int64_t) (f + 3) * hop);
		float pk = 0;
		int64_t pe = std::min<int64_t>(N, a + (int64_t) (fs * 0.012));
		for (int64_t i = a; i < pe; i++) pk = std::max(pk, std::fabs(mono[i]));
		int64_t s = a;
		for (int64_t i = a; i < b; i++)
			if (std::fabs(mono[i]) > 0.1f * pk) { s = i; break; }
		s = std::max<int64_t>(0, s - (int64_t) (fs * 0.0015));
		if (!starts.empty() && s <= starts.back() + (int64_t) (fs * 0.01)) continue;
		starts.push_back(s);
	}
	if (!starts.empty() && starts[0] < (int64_t) (fs * 0.004)) starts[0] = 0;

	// ---- features + classification
	for (size_t h = 0; h < starts.size(); h++) {
		Hit hit;
		hit.start = starts[h];
		hit.end = h + 1 < starts.size() ? starts[h + 1] : (int64_t) N;
		int64_t w0 = hit.start, w1 = std::min<int64_t>(hit.end, w0 + (int64_t) (fs * 0.06));
		// energy per band in the first 60 ms, minus what was already sounding just
		// before (the tail of the previous hit), so a snare on a kick's tail is a snare
		double e[4] = {0, 0, 0, 0}, pre[4] = {0, 0, 0, 0};
		float pk = 0;
		for (int64_t i = w0; i < w1; i++) {
			for (int b = 0; b < 4; b++) e[b] += (*bands[b])[i] * (*bands[b])[i];
			pk = std::max(pk, std::fabs(mono[i]));
		}
		int64_t p0 = std::max<int64_t>(0, w0 - (int64_t) (fs * 0.012));
		for (int64_t i = p0; i < w0; i++)
			for (int b = 0; b < 4; b++) pre[b] += (*bands[b])[i] * (*bands[b])[i];
		for (int b = 0; b < 4; b++) {
			double perPre = w0 > p0 ? pre[b] / (w0 - p0) : 0;
			e[b] = std::max(0.0, e[b] - perPre * (w1 - w0) * 0.8);
		}
		// how long the AIR band rings on (cymbals vs hats)
		double late = 0;
		int64_t l0 = w0 + (int64_t) (fs * 0.12), l1 = std::min<int64_t>(hit.end, w0 + (int64_t) (fs * 0.24));
		for (int64_t i = l0; i < l1; i++) late += air[i] * air[i];
		double tot = e[0] + e[1] + e[2] + e[3] + 1e-12;
		hit.sub = (float) (e[0] / tot); hit.body = (float) (e[1] / tot);
		hit.mid = (float) (e[2] / tot); hit.air = (float) (e[3] / tot);
		double early = e[3] / std::max<double>(1, w1 - w0), lateM = l1 > l0 ? late / (l1 - l0) : 0;
		hit.ring = (float) (lateM / (early + 1e-12));
		hit.loud = pk;
		int t;
		if (hit.air > 0.40f && hit.sub < 0.12f) t = (hit.ring > 0.20f && l1 - l0 > fs * 0.06) ? OTHER : HAT;
		else if (hit.sub > 0.38f && hit.sub > hit.body * 0.9f) t = KICK;
		else if (hit.mid + hit.air > 0.30f && hit.air > 0.04f) t = SNARE;
		else if (hit.sub > 0.55f) t = KICK;
		else t = OTHER;
		hit.type = hit.autoType = t;
		d->hits.push_back(hit);
	}
	// nothing called a kick or snare? take the most kick-like / snare-like hit
	auto have = [&](int t) { for (auto& h : d->hits) if (h.type == t) return true; return false; };
	if (!have(KICK)) {
		int best = -1;
		for (size_t i = 0; i < d->hits.size(); i++)
			if (d->hits[i].sub > 0.2f && (best < 0 || d->hits[i].sub > d->hits[best].sub)) best = (int) i;
		if (best >= 0) d->hits[best].type = d->hits[best].autoType = KICK;
	}
	if (!have(SNARE)) {
		int best = -1;
		float bs = 0;
		for (size_t i = 0; i < d->hits.size(); i++) {
			const Hit& h = d->hits[i];
			float s = (h.mid + h.air) * h.loud;
			if (h.type != KICK && s > bs) { bs = s; best = (int) i; }
		}
		if (best >= 0) d->hits[best].type = d->hits[best].autoType = SNARE;
	}
	if (bySound && !d->hits.empty()) {
		std::vector<int> idx(d->hits.size());
		for (size_t i = 0; i < idx.size(); i++) idx[i] = (int) i;
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return d->hits[a].loud < d->hits[b].loud; });
		size_t quiet = idx.size() >= 5 ? idx.size() / 5 : 0;
		std::vector<int> rest(idx.begin() + quiet, idx.end());
		for (size_t i = 0; i < quiet; i++) d->hits[idx[i]].type = d->hits[idx[i]].autoType = HAT;
		auto bright = [&](const Hit& h) { return h.mid + 2.f * h.air - h.body - 1.5f * h.sub; };
		std::sort(rest.begin(), rest.end(), [&](int a, int b) { return bright(d->hits[a]) < bright(d->hits[b]); });
		size_t n = rest.size();
		for (size_t i = 0; i < n; i++) {
			int t = i < n / 3 ? KICK : i >= n - n / 3 ? SNARE : OTHER;
			if (n < 3) t = i == 0 ? KICK : SNARE;
			d->hits[rest[i]].type = d->hits[rest[i]].autoType = t;
		}
	}
	// your clicks win
	for (auto& o : overrides)
		for (auto& h : d->hits)
			if (std::llabs(h.start - o.frame) < (int64_t) (fs * 0.015)) h.type = o.type;

	// ---- bars + tempo (4/4, whole bars). Pick the bar count that puts the tempo
	// nearest 125 BPM (in ratio terms) inside 70..190.
	double dur = N / fs;
	int bars = 1;
	if (barsOverride > 0) { bars = barsOverride; d->barsGuessed = false; }
	else {
		double best = 1e9;
		for (int b : {1, 2, 4, 8}) {
			double bpm = 240.0 * b / dur;
			if (bpm < 70 || bpm > 190) continue;
			double dist = std::fabs(std::log(bpm / 125.0));
			if (dist < best) { best = dist; bars = b; }
		}
	}
	d->loopBars = bars;
	d->bpm = (float) (240.0 * bars / dur);
	const int loopSteps = bars * STEPS;
	const double stepLen = (double) N / loopSteps;

	// ---- grid + per-type lists
	for (size_t i = 0; i < d->hits.size(); i++) {
		Hit& h = d->hits[i];
		h.step = (int) std::lround(h.start / stepLen) % loopSteps;
		d->byType[h.type].push_back((int) i);
	}
	for (int t = 0; t < NUM_LANES; t++) {
		auto& v = d->byType[t];
		std::sort(v.begin(), v.end(), [&](int a, int b) { return d->hits[a].loud > d->hits[b].loud; });
		float top = v.empty() ? 1.f : std::max(1e-6f, d->hits[v[0]].loud);
		for (int i : v) {
			d->hits[i].rel = d->hits[i].loud / top;
			d->hits[i].ghost = (t == SNARE || t == HAT) && d->hits[i].rel < 0.42f;
		}
	}
	for (size_t i = 0; i < d->hits.size(); i++) {
		const Hit& h = d->hits[i];
		if (bars > MAX_LOOP_BARS) continue;
		int& slot = d->orig[h.step / STEPS][h.step % STEPS][h.type];
		if (slot < 0 || d->hits[slot].loud < h.loud) slot = (int) i;
	}
	return d;
}

} // namespace breaker
