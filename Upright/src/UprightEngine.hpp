// Upright: a sampled upright piano (VS Upright No. 1 by Versilian Studios,
// CC0, from the VSCO 2 Community Edition). No Rack dependency, so the same
// code runs in the offline tests.
//
// The samples: a C and a G in every octave from C1 to G7 (14 notes), each
// at three dynamics (pp, mf, f) and mostly two takes (round robins).
//   key -> nearest sampled note, pitch-shifted (at most 3.5 semitones)
//   velocity -> dynamic layer, crossfaded at the borders, plus a level
//               curve inside each layer so loudness rises smoothly
//   gate off -> damper (RELEASE), unless the sustain pedal is down;
//               the top keys (F6 up) have no dampers, as on a real piano
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>

#define STB_VORBIS_HEADER_ONLY
#include "dep/stb_vorbis.c"

namespace upright {

static const int NNOTES = 14;     // C1 G1 C2 G2 ... C7 G7
static const int NLAYERS = 3;     // pp mf f
static const int NRR = 2;
static const char* LAYER_NAMES[NLAYERS] = {"pp", "mf", "f"};

static inline int sampledKey(int i) { return 24 + 12 * (i / 2) + ((i & 1) ? 7 : 0); }   // MIDI note
static inline std::string sampledName(int i) { return std::string((i & 1) ? "G" : "C") + std::to_string(1 + i / 2); }

struct Sample {
	std::vector<int16_t> data;   // interleaved stereo
	int frames = 0;
	float rate = 44100.f;
	float db = 0.f;              // loudness of the first 0.3 s
	float norm = 1.f;            // gain that brings this take to its layer's level
	bool ok() const { return frames > 8; }
};

// All samples, decoded once and shared by every Upright in the patch.
struct Bank {
	Sample s[NNOTES][NLAYERS][NRR];
	float layerDb[NLAYERS] = {-41.8f, -22.6f, -13.1f};   // measured loudness of each layer
	std::atomic<int> state{0};    // 0 not started, 1 loading, 2 ready, 3 failed
	int loaded = 0;

	bool loadAll(const std::string& dir) {
		loaded = 0;
		double sum[NLAYERS] = {}; int cnt[NLAYERS] = {};
		for (int n = 0; n < NNOTES; n++)
			for (int l = 0; l < NLAYERS; l++)
				for (int r = 0; r < NRR; r++) {
					std::string path = dir + "/" + sampledName(n) + "_" + LAYER_NAMES[l] + "_" + std::to_string(r + 1) + ".ogg";
					int ch = 0, sr = 0;
					short* out = nullptr;
					int frames = stb_vorbis_decode_filename(path.c_str(), &ch, &sr, &out);
					if (frames <= 0 || !out) continue;
					Sample& S = s[n][l][r];
					S.frames = frames;
					S.rate = (float)sr;
					S.data.resize((size_t)frames * 2);
					for (int i = 0; i < frames; i++) {
						S.data[2 * i] = out[i * ch];
						S.data[2 * i + 1] = out[i * ch + (ch > 1 ? 1 : 0)];
					}
					free(out);
					loaded++;
					// loudness of the first 0.3 s
					int m = std::min(frames, (int)(0.3f * sr));
					double e = 0;
					for (int i = 0; i < 2 * m; i++) { double v = S.data[i] / 32768.0; e += v * v; }
					S.db = (float)(10.0 * std::log10(e / (2 * m) + 1e-20));
					sum[l] += S.db; cnt[l]++;
				}
		for (int l = 0; l < NLAYERS; l++) if (cnt[l]) layerDb[l] = (float)(sum[l] / cnt[l]);
		// Each take is levelled to its layer, keeping only the note's own
		// overall loudness (averaged over its takes), so velocity is smooth
		// and the keyboard keeps its natural balance.
		for (int n = 0; n < NNOTES; n++) {
			double off = 0; int k = 0;
			for (int l = 0; l < NLAYERS; l++) for (int r = 0; r < NRR; r++) if (s[n][l][r].ok()) { off += s[n][l][r].db - layerDb[l]; k++; }
			if (k) off /= k;
			for (int l = 0; l < NLAYERS; l++) for (int r = 0; r < NRR; r++) {
				Sample& S = s[n][l][r];
				if (S.ok()) S.norm = std::pow(10.f, (layerDb[l] + (float)off - S.db) / 20.f);
			}
		}
		return loaded > 0;
	}
	// a take that exists, preferring the requested round robin, then the
	// neighbouring dynamic
	const Sample* pick(int n, int l, int rr) const {
		for (int dl : {0, -1, 1, -2, 2}) {
			int ll = l + dl;
			if (ll < 0 || ll >= NLAYERS) continue;
			if (s[n][ll][rr].ok()) return &s[n][ll][rr];
			if (s[n][ll][rr ^ 1].ok()) return &s[n][ll][rr ^ 1];
		}
		return nullptr;
	}
};

struct Controls {
	float tone = 0.f;       // -1 dark .. +1 bright
	float release = 0.35f;  // damper time, seconds to -60 dB
	float volume = 1.f;
	bool pedal = false;
};

// Loudness (dB) for a velocity 0..1: smooth through the three recorded
// dynamics, so each layer plays at its natural level in the middle of its range.
static inline float velDb(float v) {
	static const float xs[] = {0.f, 0.24f, 0.62f, 0.92f, 1.f};
	static const float ys[] = {-58.f, -41.8f, -22.6f, -13.1f, -11.5f};
	if (v <= xs[0]) return ys[0];
	for (int i = 1; i < 5; i++)
		if (v <= xs[i]) return ys[i - 1] + (v - xs[i - 1]) / (xs[i] - xs[i - 1]) * (ys[i] - ys[i - 1]);
	return ys[4];
}
static const float LAYER_EDGE[2] = {0.43f, 0.78f};   // pp|mf and mf|f
// No crossfade between layers: two takes of the same note sum with random
// phase and partials cancel. One take per strike; the level curve keeps the
// loudness continuous across the layer borders.
static const float XFADE = 0.f;

struct Voice {
	const Sample* smp = nullptr;
	double pos = 0.0, step = 1.0;
	float gain = 0.f;
	float env = 1.f, envMul = 1.f;    // release envelope
	bool active = false, held = false, releasing = false;
	int key = 0, channel = -1;
	uint32_t born = 0;
};

struct Engine {
	static const int MAXV = 48;
	const Bank* bank = nullptr;
	Voice voices[MAXV];
	float sr = 48000.f;
	uint32_t clock = 0;
	int rrNext[NNOTES] = {};
	bool pedal = false;
	float lpL = 0.f, lpR = 0.f, lpA = 0.f;
	float outL = 0.f, outR = 0.f;

	void setSampleRate(float s) {
		if (s == sr && lpA != 0.f) return;
		sr = s;
		lpA = 1.f - std::exp(-2.f * 3.14159265f * 1200.f / sr);
	}
	void reset() { for (auto& v : voices) v.active = false; pedal = false; }

	static int nearestNote(int key) {
		int best = 0, bd = 1000;
		for (int i = 0; i < NNOTES; i++) {
			int d = std::abs(key - sampledKey(i));
			if (d < bd || (d == bd && sampledKey(i) > key)) { bd = d; best = i; }   // ties: shift down (sounds more natural)
		}
		return best;
	}

	Voice* freeVoice() {
		Voice* best = nullptr;
		for (auto& v : voices) if (!v.active) return &v;
		// steal: the quietest releasing voice, else the oldest
		float q = 1e9f;
		for (auto& v : voices) if (v.releasing && v.env * v.gain < q) { q = v.env * v.gain; best = &v; }
		if (best) return best;
		uint32_t age = 0;
		for (auto& v : voices) if (clock - v.born >= age) { age = clock - v.born; best = &v; }
		return best;
	}

	void startRelease(Voice& v, float seconds) {
		v.releasing = true;
		v.envMul = std::exp(-6.9078f / (std::max(0.01f, seconds) * sr));
	}

	// key: MIDI note number (may be fractional for in-between pitches)
	void noteOn(int ch, float pitch, float vel, const Controls& c) {
		if (!bank || bank->state.load() != 2) return;
		int key = (int)std::lround(pitch);
		int n = nearestNote(key);
		// a key struck again: what was ringing fades quickly
		for (auto& v : voices)
			if (v.active && v.key == key && !v.releasing) startRelease(v, pedal ? 0.6f : 0.12f);
		int rr = rrNext[n]; rrNext[n] ^= 1;
		vel = std::min(std::max(vel, 0.f), 1.f);
		float db = velDb(vel);
		// which layers sound, and how much of each
		float w[NLAYERS] = {0.f, 0.f, 0.f};
		int layer = vel < LAYER_EDGE[0] ? 0 : (vel < LAYER_EDGE[1] ? 1 : 2);
		w[layer] = 1.f;
		for (int e = 0; e < 2; e++) {
			float d = XFADE > 0.f ? (vel - LAYER_EDGE[e]) / XFADE : 2.f;    // -1..1 inside the crossfade
			if (XFADE > 0.f && d > -1.f && d < 1.f) {
				float t = 0.5f * (d + 1.f);              // 0 = lower layer, 1 = upper layer
				w[e] = std::cos(0.5f * 3.14159265f * t);
				w[e + 1] = std::sin(0.5f * 3.14159265f * t);
			}
		}
		for (int l = 0; l < NLAYERS; l++) {
			if (w[l] < 1e-3f) continue;
			const Sample* s = bank->pick(n, l, rr);
			if (!s) continue;
			Voice* v = freeVoice();
			if (!v) return;
			v->smp = s;
			v->pos = 0.0;
			v->step = std::pow(2.0, (pitch - sampledKey(n)) / 12.0) * s->rate / sr;
			// the take found may be from a neighbouring layer: level it as that layer
			int sl = (int)((s - &bank->s[0][0][0]) / NRR) % NLAYERS;
			v->gain = w[l] * s->norm * std::pow(10.f, (db - bank->layerDb[sl]) / 20.f);
			v->env = 1.f; v->envMul = 1.f;
			v->active = true; v->held = true; v->releasing = false;
			v->key = key; v->channel = ch; v->born = clock;
		}
	}

	void noteOff(int ch, const Controls& c) {
		for (auto& v : voices)
			if (v.active && v.held && v.channel == ch) {
				v.held = false;
				if (!pedal && v.key < 89 && !v.releasing) startRelease(v, c.release);
			}
	}

	void setPedal(bool p, const Controls& c) {
		if (p == pedal) return;
		pedal = p;
		if (!p)
			for (auto& v : voices)
				if (v.active && !v.held && !v.releasing && v.key < 89) startRelease(v, c.release);
	}

	static inline float hermite(float xm1, float x0, float x1, float x2, float t) {
		float c1 = 0.5f * (x1 - xm1);
		float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * t + c2) * t + c1) * t + x0;
	}

	void process(const Controls& c) {
		clock++;
		float L = 0.f, R = 0.f;
		for (auto& v : voices) {
			if (!v.active) continue;
			const Sample& s = *v.smp;
			int i = (int)v.pos;
			if (i + 2 >= s.frames || v.env < 1e-4f) { v.active = false; continue; }
			float t = (float)(v.pos - i);
			const int16_t* d = s.data.data();
			int im1 = i > 0 ? i - 1 : 0;
			const float k = 1.f / 32768.f;
			float l = hermite(d[2 * im1], d[2 * i], d[2 * i + 2], d[2 * i + 4], t);
			float r = hermite(d[2 * im1 + 1], d[2 * i + 1], d[2 * i + 3], d[2 * i + 5], t);
			float g = v.gain * v.env * k;
			L += l * g; R += r * g;
			v.pos += v.step;
			if (v.releasing) v.env *= v.envMul;
		}
		// TONE: tilt around 1.2 kHz (dark: highs down; bright: highs up 6 dB)
		lpL += lpA * (L - lpL);
		lpR += lpA * (R - lpR);
		float hiGain = c.tone < 0.f ? 1.f + 0.85f * c.tone : 1.f + c.tone;
		L = lpL + hiGain * (L - lpL);
		R = lpR + hiGain * (R - lpR);
		outL = L * c.volume;
		outR = R * c.volume;
	}

	int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active; return n; }
};

} // namespace upright
