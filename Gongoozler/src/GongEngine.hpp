// Gongoozler: a gong and tam-tam model (no Rack code here, so test/ can run it offline).
//
// How it works, in plain words:
//  * A gong is a big bronze plate that rings at hundreds of unrelated
//    frequencies ("modes"). We model up to 256 of them as tiny ringing filters.
//    Where the modes sit depends on the TYPE:
//      - TAM-TAM and WIND GONG are flat plates: the modes follow the flat-plate
//        law (frequency grows with the square of the number of nodal lines), so
//        they are sparse at the bottom and crowd together higher up. Every
//        non-round mode comes as a near-identical pair that beats slowly (BEAT).
//      - OPERA GONG has a flat, tuned centre: one strong pitched mode plus a few
//        partials, then the rim's plate modes above.
//      - BOSSED GONG (gong ageng, kempul, Thai gong) has a raised boss that gives a
//        clear, nearly harmonic note (1, 2, 3 ...). The note and its octave come in
//        slightly detuned pairs, the slow "ombak" beating of a gamelan gong.
//  * The mallet is a short raised-cosine push. A hard stick is short (bright),
//    a big soft beater is long (only the low modes get going).
//  * BLOOM is the famous tam-tam effect: a big, hard hit starts as a low "bwong",
//    then over the next second the energy climbs up the spectrum into a roaring,
//    shimmering wash. Physically, the plate is so thin that big vibrations
//    stretch it, which couples the modes and passes energy from low to high
//    ("wave turbulence", Chaigne, Touze, Ducceschi et al.). We model it as an
//    energy cascade through 7 octave bands: each band hands energy to the band
//    above at a rate that grows with its own energy, so soft hits stay clean and
//    hard hits bloom, and nothing is created out of nothing (what goes up is
//    taken from below). BUILD sets how slowly it climbs.
//  * ROAR is the coherent part of the same nonlinearity: a bent plate is also
//    stretched, and the stretch (mostly from the big low modes) stiffens every
//    mode at once (Berger / von Karman tension). Hard hits growl, smear and sweep
//    downwards as they settle. It is written as an energy-conserving force, so it
//    can never run away.
//  * GLIDE is the pitch bend of opera gongs: the pitch at a hard hit is shifted
//    by up to GLIDE semitones and slides back as the vibration dies
//    (shift proportional to amplitude squared, as in a real hardening/softening
//    plate). The big opera gong falls (+ setting), the small one rises (- setting).
//  * MUTE is a towel on the gong, HAND is a hand grabbing it.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace gong {

static const int N = 256;        // mode slots
static const int NDENSE = 200;   // modes taken from the real mode list, the rest are sparse high modes
static const int NBAND = 7;      // cascade bands: <200, 200-400, ... 3.2-6.4k, >6.4k Hz
static const int BLOCK = 16;     // coefficients are refreshed every 16 samples
static const float PI = 3.14159265358979f;

enum Type { TAMTAM = 0, WIND, OPERA, BOSSED, NTYPES };

struct Controls {
	int type = TAMTAM;
	float size = 90.f;       // diameter in cm, 20..150
	float thick = 0.5f;      // 0 thin .. 1 thick
	float beat = 0.35f;      // 0 perfectly round .. 1 strong beating
	float glide = 0.f;       // semitones at a full hit, -7..+7 (+ = falls back down)
	int cast = 0;            // which individual gong, 0..15
	float position = 0.35f;  // 0 centre .. 1 edge
	float mallet = 0.65f;    // 0 hard stick .. 1 big soft beater
	float decay = 0.6f;      // 0..1
	float damp = 0.4f;       // 0 bright .. 1 dark
	float bloom = 0.6f;      // 0..1
	float build = 0.5f;      // 0 fast .. 1 slow
	float roar = 0.3f;       // 0..1
	float tune = 0.f;        // octaves (knob + V/OCT)
	bool followPitch = true; // opera / bossed: fundamental = C4 * 2^tune
	float mute = 0.f;        // 0..1
	float width = 0.7f;
	float level = 1.f;
	bool hand = false;
};

static inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
static inline float smoothstep(float a, float b, float x) {
	float t = clampf((x - a) / (b - a), 0.f, 1.f);
	return t * t * (3.f - 2.f * t);
}

struct Rng {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v * 2654435761u + 0x6D2B79F5u; if (!s) s = 1; for (int i = 0; i < 4; i++) next(); }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }  // 0..1
	float bi() { return uni() * 2.f - 1.f; }                      // -1..1
};

// Mode kinds
enum Kind { PLATE = 0, PARTIAL = 1, HIGH = 2 };

struct ModeDef {
	float ratio;   // to the type's reference frequency (PLATE/PARTIAL)
	float u;       // HIGH: 0..1 position between the last dense mode and 16 kHz
	float tw;      // 0..1 random amount for twin splitting (0 = not a twin)
	int twinOf;    // index of the mode this one is the twin of, or -1
	int m, n;      // nodal diameters / circles (PLATE)
	int kind;
	float w;       // base strength
	float hz;      // PARTIAL twins: split in Hz per unit of BEAT
};

struct Engine {
	// resonator state and coefficients (SIMD-friendly arrays)
	alignas(16) float x[N] = {}, v[N] = {};
	alignas(16) float e[N] = {}, r[N] = {};
	alignas(16) float a[N] = {};    // strike gain per mode (set at each hit)
	alignas(16) float gn[N] = {};   // cascade noise gain per mode (per block)
	alignas(16) float b[N] = {};    // ROAR: random per-mode coupling strength
	alignas(16) float cp[N] = {}, cpp[N] = {};  // ROAR stretch weights
	alignas(16) float oL[N] = {}, oR[N] = {};
	float f[N] = {};                // current frequencies (Hz)
	int band[N] = {};

	ModeDef md[N];
	int M = 0;                      // modes in use
	int nDense = 0;
	float phi[N], sgnL[N], sgnR[N], magL[N], magR[N];
	int typeNow = -1, castNow = -1;

	float fs = 48000.f;
	int blockPos = 0;
	Rng rng, rngHit;

	// strike pulse
	int pulseLen = 0, pulsePos = 0;
	float pulseAmp = 0.f;

	// cascade
	float bandE[NBAND] = {}, flux[NBAND] = {}, drain[NBAND] = {};
	float topFlux = 0.f, airAmp = 0.f;
	// glide
	float glideE = 0.f, pitchMul = 1.f;
	// roar
	float roarG = 0.f;
	// hand / mute
	int handHold = 0;
	float handS = 0.f;
	// air and output filters
	float airL = 0.f, airR = 0.f, airPrevL = 0.f, airPrevR = 0.f;
	float dcL = 0.f, dcR = 0.f;

	// level calibration (see test/render.cpp)
	static constexpr float ETH = 0.0012f;     // cascade threshold: mean energy per mode in a band
	static constexpr float EGLIDE = 2.0f;     // energy that gives the full GLIDE
	static constexpr float OUT_GAIN = 6.0f;
	static constexpr float SMAX = 50.f;
	static constexpr float ROAR_G = 1500.f;

	float outL = 0.f, outR = 0.f;
	float meter = 0.f;

	void setSampleRate(float sr) { fs = sr; }

	// ---------------------------------------------------------------- mode lists
	// Flat plate modes (m nodal diameters, n nodal circles), Chladni's law
	// f ~ (m + 2n)^p. Non-axisymmetric modes (m > 0) come in pairs.
	int addPlate(int k0, int kmax, float p, float start, float w, Rng& g) {
		struct C { float r; int m, n; };
		static C c[4096];
		int nc = 0;
		for (int m = 0; m <= 70; m++)
			for (int n = 0; n <= 35; n++) {
				int q = m + 2 * n;
				if (q < 2) continue;
				float ratio = std::pow(q * 0.5f, p);
				if (m == 0) ratio *= 1.45f;        // axisymmetric modes sit a bit higher
				if (m == 1) ratio *= 1.18f;
				ratio *= start * (1.f + 0.025f * g.bi());
				if (ratio > 2000.f || nc >= 4096) continue;
				c[nc++] = {ratio, m, n};
			}
		std::sort(c, c + nc, [](const C& A, const C& B) { return A.r < B.r; });
		int k = k0;
		for (int i = 0; i < nc && k < kmax; i++) {
			ModeDef d = {};
			d.ratio = c[i].r; d.m = c[i].m; d.n = c[i].n; d.kind = PLATE; d.w = w; d.twinOf = -1;
			md[k++] = d;
			if (c[i].m > 0 && k < kmax) {
				ModeDef t = d;
				t.tw = 0.1f + 0.9f * g.uni();
				t.twinOf = k - 1;
				md[k++] = t;
			}
		}
		return k;
	}

	int addPartials(int k0, const float* ratios, const float* weights, const float* twinHz, int np, Rng& g) {
		int k = k0;
		for (int i = 0; i < np; i++) {
			ModeDef d = {};
			d.ratio = ratios[i] * (1.f + 0.004f * g.bi() * (i > 0));
			d.kind = PARTIAL; d.w = weights[i]; d.twinOf = -1; d.m = 0; d.n = i;
			md[k++] = d;
			if (twinHz[i] > 0.f) {
				ModeDef t = d;
				t.twinOf = k - 1;
				t.tw = 0.6f + 0.4f * g.uni();
				t.hz = twinHz[i];
				t.w = d.w * 0.8f;
				md[k++] = t;
			}
		}
		return k;
	}

	void makeModes(int type, int cast) {
		typeNow = type; castNow = cast;
		Rng g;
		g.seed(7000u + (uint32_t) cast * 7919u + (uint32_t) type * 104729u);
		int k = 0;
		if (type == TAMTAM) {
			k = addPlate(0, NDENSE, 2.0f, 1.f, 1.f, g);
		} else if (type == WIND) {
			k = addPlate(0, NDENSE, 1.82f, 1.f, 1.f, g);
		} else if (type == OPERA) {
			// flat tuned centre: strong fundamental + a few axisymmetric partials
			static const float R[] = {1.f, 1.47f, 2.08f, 2.63f, 3.37f, 4.12f, 5.08f};
			static const float W[] = {1.6f, 0.55f, 0.6f, 0.4f, 0.35f, 0.25f, 0.2f};
			static const float T[] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
			k = addPartials(0, R, W, T, 7, g);
			k = addPlate(k, NDENSE, 1.75f, 1.6f, 0.45f, g);
		} else {
			// bossed: nearly harmonic boss modes, the first two as beating pairs
			static const float R[] = {1.f, 2.005f, 2.76f, 2.995f, 4.07f, 5.43f, 6.1f, 8.2f};
			static const float W[] = {1.8f, 0.8f, 0.3f, 0.45f, 0.25f, 0.18f, 0.15f, 0.1f};
			static const float T[] = {1.f, 1.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
			k = addPartials(0, R, W, T, 8, g);
			k = addPlate(k, NDENSE, 1.8f, 2.3f, 0.3f, g);
		}
		nDense = k;
		// sparse high modes: stand-ins for the thousands of modes above the dense list
		int nh = N - k;
		for (int i = 0; i < nh; i++) {
			ModeDef d = {};
			d.kind = HIGH; d.twinOf = -1; d.w = (type == BOSSED ? 0.3f : type == OPERA ? 0.45f : 1.f);
			d.u = (i + 0.5f + 0.8f * g.bi() * 0.5f) / nh;
			md[k++] = d;
		}
		M = k;
		for (int i = 0; i < M; i++) {
			phi[i] = g.uni() * PI;
			sgnL[i] = (g.next() & 1) ? 1.f : -1.f;
			sgnR[i] = (g.next() & 1) ? 1.f : -1.f;
			magL[i] = 0.55f + 0.45f * g.uni();
			magR[i] = 0.55f + 0.45f * g.uni();
			b[i] = ((g.next() & 1) ? 1.f : -1.f) * (0.4f + 0.6f * g.uni());
			// twins share the strike angle a quarter turn apart (cos / sin pair)
			if (md[i].twinOf >= 0 && md[i].kind == PLATE) phi[i] = phi[md[i].twinOf] + PI * 0.5f;
		}
		for (int i = M; i < N; i++) { x[i] = v[i] = 0.f; e[i] = r[i] = a[i] = gn[i] = 0.f; }
	}

	// ---------------------------------------------------------------- the strike
	float strikeWeight(int k, float rr) const {
		const ModeDef& d = md[k];
		if (d.kind == PARTIAL) {
			// centre (boss) excites the note; off-centre less of the fundamental
			float c = 1.f - smoothstep(0.08f, 0.7f, rr);
			float s = d.n == 0 ? (0.25f + 0.75f * c) : (0.45f + 0.55f * (0.5f + 0.5f * std::cos(PI * (d.n * 0.7f + 0.3f) * rr)));
			return d.w * s;
		}
		if (d.kind == HIGH) {
			return d.w * (0.5f + 0.5f * smoothstep(0.05f, 0.4f, rr)) * (0.4f + 0.6f * std::fabs(std::sin(phi[k] * 7.f + rr * 9.f)));
		}
		// plate mode: ~J_m(k r) cos(m theta). Rises like r^m near the centre,
		// then oscillates with n + m/2 half waves; every mode moves at a free edge.
		float m = (float) d.m;
		float z = rr * (0.5f * m + d.n + 0.75f);
		float knee = 0.5f * m + 0.6f;
		float rise = d.m == 0 ? 1.f : std::pow(std::min(1.f, z / knee), std::min(m, 6.f));
		float osc = 0.2f + 0.8f * std::fabs(std::cos(PI * (z - 0.25f * m) + 0.3f * phi[k]));
		float ang = d.m == 0 ? 1.f : std::fabs(std::cos(phi[k]));
		float edge = 1.f + 0.5f * smoothstep(0.85f, 1.f, rr);
		float s = rise * osc * (0.35f + 0.65f * ang) * edge;
		if (typeNow == OPERA || typeNow == BOSSED) s *= 0.25f + 0.75f * smoothstep(0.1f, 0.5f, rr);  // the boss is stiff
		return d.w * s;
	}

	void trigger(float vel, const Controls& c) { hit(vel, c.position, c.mallet, c); }

	void hit(float vel, float pos, float mallet, const Controls& c) {
		if (typeNow != c.type || castNow != c.cast) makeModes(c.type, c.cast);
		if (f[0] == 0.f) updateBlock(c);
		float rr = 0.03f + 0.95f * clampf(pos, 0.f, 1.f);
		for (int k = 0; k < M; k++) a[k] = strikeWeight(k, rr);
		vel = clampf(vel, 0.f, 1.5f);
		// contact time: hard stick 0.15 ms .. big soft beater 20 ms; harder hits
		// squash the felt and make the contact shorter (Hertz contact law)
		float tau = 0.00015f * std::pow(130.f, clampf(mallet, 0.f, 1.f));
		tau *= std::pow(std::max(vel, 0.05f) / 0.75f, -0.2f);
		pulseLen = std::max(2, (int) (tau * fs));
		pulsePos = 0;
		pulseAmp = vel * 2.f / pulseLen;  // raised cosine with area = vel
	}

	void handGrab() { handHold = (int) (0.2f * fs); }

	// ---------------------------------------------------------------- per block
	void updateBlock(const Controls& c) {
		if (typeNow != c.type || castNow != c.cast) makeModes(c.type, c.cast);
		const float D = clampf(c.size, 15.f, 160.f);
		const float th = c.thick;
		const float dt = 1.f / fs;
		const float bdt = BLOCK * dt;
		const bool tuned = c.type == OPERA || c.type == BOSSED;

		// ---- reference frequency
		float f1;
		if (c.type == TAMTAM) f1 = 34.f * std::pow(90.f / D, 2.f) * (0.55f + 0.9f * th);
		else if (c.type == WIND) f1 = 52.f * std::pow(90.f / D, 2.f) * (0.55f + 0.9f * th);
		else if (c.followPitch) f1 = 261.63f;
		else if (c.type == OPERA) f1 = 330.f * std::pow(30.f / D, 1.5f) * (0.7f + 0.6f * th);
		else f1 = 62.f * std::pow(90.f / D, 1.5f) * (0.7f + 0.6f * th);
		f1 *= std::exp2(c.tune);

		// ---- band energies -> cascade (energy-conserving: drained below, injected above)
		for (int i = 0; i < NBAND; i++) bandE[i] = 0.f;
		int cnt[NBAND] = {};
		float gsum[NBAND] = {};
		float Eglide = 0.f;
		for (int k = 0; k < M; k++) {
			float E = x[k] * x[k] + v[k] * v[k];
			if (E < 1e-20f) { x[k] = 0.f; v[k] = 0.f; E = 0.f; }  // silent: no denormal crawl
			bandE[band[k]] += E;
			cnt[band[k]]++;
			if (md[k].kind != HIGH && k < 24) Eglide += E;
		}
		// thicker gongs and tuned gongs need harder hits before they bloom
		float eth = ETH * (0.4f + 1.6f * th * th) * (tuned ? 3.f : 1.f) * (c.type == WIND ? 0.6f : 1.f);
		float Tb = 0.06f * std::pow(40.f, c.build);  // 60 ms .. 2.4 s for the lowest band
		float bl = c.bloom * (tuned ? 0.6f : 1.f);
		for (int i = 0; i < NBAND; i++) {
			float s = bandE[i] / (std::max(cnt[i], 1) * eth);
			float s15 = s * std::sqrt(s);
			float rate = bl / (Tb * std::pow(0.72f, (float) i)) * s15 / (1.f + s15);
			flux[i] = rate * bandE[i];          // energy per second leaving band i
			drain[i] = 0.5f * rate;             // amplitude damping that removes it
		}

		// ---- glide: pitch shift proportional to amplitude^2 of the low modes
		glideE += (Eglide - glideE) * std::min(1.f, bdt / 0.015f);
		float gy = glideE / EGLIDE;
		float gx = std::min(1.25f * gy / (1.f + 0.25f * gy), 2.f);  // ~amplitude^2, rounded off for huge hits
		pitchMul = std::exp2(c.glide / 12.f * gx);

		// ---- frequencies
		float fLast = 0.f;
		for (int k = 0; k < nDense; k++) {
			const ModeDef& d = md[k];
			float fk;
			if (d.twinOf >= 0) {
				float base = f[d.twinOf];
				if (d.kind == PARTIAL) fk = base + (0.25f + 6.f * c.beat) * d.hz * d.tw;
				else fk = base * (1.f + (0.0006f + 0.012f * c.beat * c.beat) * d.tw);
			} else {
				fk = f1 * d.ratio * pitchMul;
			}
			f[k] = fk;
			fLast = std::max(fLast, fk);
		}
		float fTop = 16000.f;
		for (int k = nDense; k < M; k++) {
			if (fLast < fTop) f[k] = fLast * std::pow(fTop / fLast, md[k].u);
			else f[k] = 30000.f;
		}

		// ---- decay times
		float Tlow = 1.2f * std::pow(45.f, c.decay) * std::pow(D / 90.f, 0.5f) * (0.75f + 0.5f * th);
		if (c.type == OPERA) Tlow *= 0.6f;
		float fd = 16000.f * std::pow(700.f / 16000.f, c.damp);
		float lamMute = 14.f * c.mute * c.mute;
		float handT = (c.hand || handHold > 0) ? 1.f : 0.f;
		handS += (handT - handS) * std::min(1.f, bdt / 0.03f);
		if (handHold > 0) handHold -= BLOCK;
		float lamHand = 40.f * handS;

		// ---- noise gains for the cascade: band i+1 receives what band i loses
		int nb[N];
		for (int k = 0; k < M; k++) {
			float fk = f[k];
			int bi = fk < 200.f ? 0 : std::min(NBAND - 1, 1 + (int) std::floor(std::log2(fk / 200.f)));
			nb[k] = bi;
		}
		for (int k = 0; k < M; k++) {
			float hw = md[k].kind == PARTIAL ? 0.3f : 1.f;
			gsum[nb[k]] += hw * hw;
		}
		float sig[NBAND];
		sig[0] = 0.f;
		for (int i = 1; i < NBAND; i++) {
			// unit-variance noise into gain g per mode adds g^2 per sample to x^2+v^2
			float inflow = flux[i - 1];
			sig[i] = gsum[i] > 0.f ? std::sqrt(inflow / (fs * gsum[i])) : 0.f;
		}
		// whatever leaves the top band (and anything with nowhere to go) becomes air
		topFlux = flux[NBAND - 1];
		for (int i = 0; i < NBAND - 1; i++) if (gsum[i + 1] == 0.f) topFlux += flux[i];
		airAmp = std::sqrt(topFlux);

		for (int k = 0; k < M; k++) {
			float fk = f[k];
			band[k] = nb[k];
			const ModeDef& d = md[k];
			float T60 = Tlow / (1.f + fk / fd);
			if (d.kind == PARTIAL) T60 *= (c.type == BOSSED ? 1.6f : 1.2f);
			else if (tuned) T60 *= 0.45f;
			T60 = std::max(T60, 0.03f);
			float lam = 6.91f / T60 + drain[nb[k]] + lamMute * (0.6f + 0.4f * std::min(fk / 2000.f, 1.f)) + lamHand * (1.f + std::min(fk / 3000.f, 1.f));
			if (fk < 0.45f * fs) {
				e[k] = 2.f * std::sin(PI * fk * dt);
				r[k] = std::exp(-lam * dt);
				float hw = d.kind == PARTIAL ? 0.3f : 1.f;
				gn[k] = hw * sig[nb[k]];
				{ float pr = std::min(300.f / fk, 4.f); cp[k] = std::fabs(b[k]) * pr; cpp[k] = cp[k] * pr; }
			} else {
				e[k] = 0.f; r[k] = 0.f; gn[k] = 0.f; cp[k] = cpp[k] = 0.f; x[k] = 0.f; v[k] = 0.f;
			}
			// radiation: the lowest modes of a big plate radiate weakly
			float tilt = fk < 160.f ? std::sqrt(fk / 160.f) : 1.f;
			oL[k] = sgnL[k] * magL[k] * tilt;
			oR[k] = oL[k] + (sgnR[k] * magR[k] * tilt - oL[k]) * c.width;
		}

		// ---- roar: quadratic coupling strength (bounded, see process())
		roarG = ROAR_G / fs * c.roar * c.roar * (tuned ? 0.5f : 1.f);

		float Etot = 0.f;
		for (int i = 0; i < NBAND; i++) Etot += bandE[i];
		meter = std::min(1.f, std::sqrt(Etot) * 0.5f);
	}

	// ---------------------------------------------------------------- per sample
	void process(const Controls& c) {
		if (blockPos == 0) updateBlock(c);
		if (++blockPos >= BLOCK) blockPos = 0;

		float sA = 0.f;
		if (pulsePos < pulseLen) {
			float ph = (pulsePos + 0.5f) / pulseLen;
			sA = pulseAmp * 0.5f * (1.f - std::cos(2.f * PI * ph));
			pulsePos++;
		}
		// cascade noise (one unit-variance white noise drives all the receiving modes;
		// each mode only keeps its own frequency, so they ring independently)
		float n = rng.bi() * 1.7320508f;

		// roar: the plate stretches when it bends (Berger / von Karman tension).
		// S = total stretch, made mostly of the big low modes; it stiffens every
		// mode a little, which smears and couples the spectrum into a roar and
		// bends the pitch up while the gong is loud. Potential ~ S^2 >= 0, so it is
		// energy-conserving and can't run away. (x is velocity-like: displacement = x / omega.)
		float S = 0.f;
		for (int k = 0; k < M; k++) S += cpp[k] * x[k] * x[k];
		float nl = roarG * S / (1.f + S * (1.f / SMAX));
		float L = 0.f, R = 0.f;
		for (int k = 0; k < M; k++) {
			float vk = r[k] * v[k] - e[k] * x[k] + sA * a[k] + n * gn[k] - nl * cp[k] * x[k];
			float xk = r[k] * x[k] + e[k] * vk;
			v[k] = vk;
			x[k] = xk;
			L += xk * oL[k];
			R += xk * oR[k];
		}

		// air: the hiss above the last modes, fed by what leaves the top band
		float w1 = n, w2 = rng.bi() * 1.7320508f;
		float hl = w1 - airPrevL, hr = w2 - airPrevR;
		airPrevL = w1; airPrevR = w2;
		airL += (hl - airL) * 0.6f;
		airR += (hr - airR) * 0.6f;
		float ag = airAmp * 0.004f;
		L += airL * ag;
		R += (airL + (airR - airL) * c.width) * ag;

		dcL += (L - dcL) * 0.0015f; L -= dcL;
		dcR += (R - dcR) * 0.0015f; R -= dcR;
		float g = OUT_GAIN * c.level;
		outL = 10.f * std::tanh(L * g * 0.1f);
		outR = 10.f * std::tanh(R * g * 0.1f);
	}
};

}  // namespace gong
