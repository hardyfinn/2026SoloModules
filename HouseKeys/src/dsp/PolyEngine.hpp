// POLY engine: analog polysynths (Juno-60/106, Jupiter-6, JX-8P, Poly-800 characters).
// Spec section 5. Runs at 2x the host rate.
#pragma once
#include "Engine.hpp"
#include "SoundTypes.hpp"

namespace hk {

struct PolyEngine : Engine {
	struct Voice {
		bool gate = false;
		float note = 60.f, target = 60.f, vel = 100.f, pan = 0.f;
		double ph1 = 0.0, ph2 = 0.0;
		float subSign = 1.f;
		float drift1 = 0.f, drift2 = 0.f, dn1 = 0.f, dn2 = 0.f; // slow drift state
		float off1 = 0.f, off2 = 0.f, cutOff = 1.f;
		float lfoAge = 0.f;
		Adsr fenv, aenv;
		Ladder lp;
		Noise nz;
		int filtCounter = 0;
		float lastFc = -1.f;
		float sweepEnv = 0.f;
		float blepPend1 = 0.f, blepPend2 = 0.f;
	};
	Voice v[MAX_SLOTS];
	const PolySound* s = nullptr;
	int soundIdx = 0;
	float fs2 = 96000.f;
	Decimator2 decL, decR;
	double lfoPh = 0.0, vibPh = 0.0;
	Biquad hpfL, hpfR;
	int hpfMode = -1;
	// Poly-800: one filter and one filter envelope for the instrument
	Ladder sharedLp, sharedLpR;
	Adsr sharedEnv;
	Noise floorNoise;

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
	bool slotActive(int slot) const override { return v[slot].gate || v[slot].aenv.active(); }
private:
	Macros curM;
	float renderVoice(Voice& vc, int slot, float lfo, float vib, float* sharedIn);
};

} // namespace hk
