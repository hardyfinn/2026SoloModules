// PD engine: phase distortion after the Casio CZ. Spec section 6. Runs at 4x.
#pragma once
#include "Engine.hpp"
#include "SoundTypes.hpp"

namespace hk {

// linear multi-segment envelopes as on the CZ
struct LinEnv {
	enum { IDLE, ATT, DEC, SUS, REL };
	int stage = IDLE;
	float v = 0.f, start = 0.f, peak = 1.f, sus = 1.f;
	float ra = 1.f, rd = 1.f, rr = 1.f; // per-sample increments
	void setDcw(const float* d, float fs, float as, float ds) {
		start = d[0]; peak = d[2]; sus = d[4];
		ra = std::fabs(peak - start) / std::max(d[1] * as * fs, 1.f);
		rd = std::fabs(sus - peak) / std::max(d[3] * ds * fs, 1.f);
		rr = 1.f / std::max(d[5] * ds * fs, 1.f);
	}
	void setAmp(const float* a, bool gate, float fs, float as, float ds) {
		start = 0.f; peak = 1.f;
		if (gate) { sus = 1.f; ra = 1.f / (0.002f * fs); rd = 1.f; rr = 1.f / (0.006f * fs); return; }
		sus = a[2];
		ra = 1.f / std::max(a[0] * as * fs, 1.f);
		rd = std::fabs(1.f - sus) / std::max(a[1] * ds * fs, 1.f);
		rr = 1.f / std::max(a[3] * ds * fs, 1.f);
	}
	void on() { v = start; stage = ATT; }
	void off() { if (stage != IDLE) stage = REL; }
	float next() {
		switch (stage) {
			case ATT: if (peak >= start) { v += ra; if (v >= peak) { v = peak; stage = DEC; } } else { v -= ra; if (v <= peak) { v = peak; stage = DEC; } } break;
			case DEC: if (sus <= peak) { v -= rd; if (v <= sus) { v = sus; stage = SUS; } } else { v += rd; if (v >= sus) { v = sus; stage = SUS; } } break;
			case SUS: v = sus; break;
			case REL: v -= rr; if (v <= 0.f) { v = 0.f; stage = IDLE; } break;
			default: break;
		}
		return v;
	}
	bool active() const { return stage != IDLE; }
};

struct PdEngine : Engine {
	struct Line { double ph = 0.0, dt = 0.0; int parity = 0; LinEnv dcw, amp; };
	struct Voice {
		bool gate = false;
		float note = 60.f, target = 60.f, vel = 100.f, pan = 0.f, age = 0.f;
		Line l[2];
	};
	Voice v[MAX_SLOTS];
	const PdSound* s = nullptr;
	float fs4 = 192000.f;
	Decimator2 dL1, dL2, dR1, dR2;
	double vibPh = 0.0, tremPh = 0.0;
	float holdL = 0.f, holdR = 0.f, holdAcc = 0.f;
	void init(float sampleRate) override;
	void setSound(int idx) override;
	void noteOn(int slot, float note, float vel, float pan) override;
	void setPitch(int slot, float note) override;
	void setPan(int slot, float pan) override { if (slot >= 0 && slot < MAX_SLOTS) v[slot].pan = pan; }
	void noteOff(int slot) override;
	void allOff() override;
	void process(float* L, float* R, int n, const Macros& m) override;
	bool isMono() const override { return s && s->mono; }
	int chorusDefault() const override { return s ? s->chorus : 0; }
	bool slotActive(int slot) const override { return v[slot].gate || v[slot].l[0].amp.active() || v[slot].l[1].amp.active(); }
private:
	Macros curM;
};

float pdWave(int w, double ph, float a);

} // namespace hk
