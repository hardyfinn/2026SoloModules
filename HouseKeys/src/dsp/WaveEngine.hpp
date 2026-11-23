// WAVE engine: three 8-bit wavetable oscillators, analog 4-pole low-pass, four
// envelopes, three LFOs, after the Ensoniq SQ-80. Spec section 7. Runs at 2x.
#pragma once
#include "Engine.hpp"
#include "SoundTypes.hpp"
#include "WaveTables.hpp"

namespace hk {

// SQ-style envelope: 0 -> L1 (T1) -> L2 (T2) -> L3 (T3), hold; release to 0 (T4). Linear.
struct SqEnv {
	int stage = 0; // 0 idle, 1..3 segments, 4 hold, 5 release
	float v = 0.f, from = 0.f, to = 0.f, pos = 0.f, step = 0.f;
	float L[3] = {0, 0, 0}, T[4] = {0, 0, 0, 0};
	void begin(float target, float time, float fs) {
		from = v; to = target; pos = 0.f;
		step = time <= 0.f ? 1.f : 1.f / (time * fs);
	}
	void on(float fs) { v = 0.f; stage = 1; begin(L[0], T[0], fs); }
	void off(float fs) { if (stage != 0) { stage = 5; begin(0.f, T[3], fs); } }
	float next(float fs) {
		if (stage == 0 || stage == 4) return v;
		pos += step;
		if (pos >= 1.f) {
			v = to;
			if (stage == 5) { stage = 0; return v; }
			stage++;
			if (stage <= 3) begin(L[stage - 1], T[stage - 1], fs);
			return v;
		}
		v = from + (to - from) * pos;
		return v;
	}
	bool active() const { return stage != 0; }
};

// one oscillator read at phase ph (samples) with increment inc; GRIT blend (exposed for tests)
float readOsc(const Wave& W, int band, double ph, double inc, float grit, bool& done);

struct WaveEngine : Engine {
	struct Osc { double ph = 0.0; bool done = false; };
	struct Voice {
		bool gate = false;
		float note = 60.f, target = 60.f, vel = 100.f, pan = 0.f;
		Osc o[3];
		SqEnv env[4];
		double lfoPh[3] = {0, 0, 0};
		float lfoAge = 0.f, lfoNoise[3] = {0, 0, 0};
		Ladder lp;
		int filtCounter = 0;
		float hold = 0.f, holdAcc = 0.f;
		Noise nz;
	};
	Voice v[MAX_SLOTS];
	const WaveSound* s = nullptr;
	float fs2 = 96000.f;
	Decimator2 decL, decR;
	bool usesWheel = false;
	double vibPh = 0.0;

	void init(float sampleRate) override;
	void setSound(int idx) override;
	void noteOn(int slot, float note, float vel, float pan) override;
	void setPitch(int slot, float note) override;
	void setPan(int slot, float pan) override { if (slot >= 0 && slot < MAX_SLOTS) v[slot].pan = pan; }
	void noteOff(int slot) override;
	void allOff() override;
	void process(float* L, float* R, int n, const Macros& m) override;
	bool isMono() const override { return s && s->mono; }
	bool slotActive(int slot) const override { return v[slot].gate || v[slot].env[3].active(); }
private:
	Macros curM;
	float renderVoice(Voice& vc, float vib);
};

} // namespace hk
