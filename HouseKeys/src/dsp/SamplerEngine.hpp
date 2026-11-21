// SAMPLER engine: the Ensoniq Mirage chain (8-bit, small memory, no interpolation,
// analog 4-pole filter) playing instrument models "recorded" into that memory when
// the sound is chosen. Spec section 8. Runs at 2x.
#pragma once
#include "Engine.hpp"
#include "SoundTypes.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace hk {

struct SmpZone {
	int root = 60, len = 0;
	float sr = 20000.f;
	bool loop = false;
	int loopStart = 0, loopEnd = 0;
	std::vector<float> f16;
	std::vector<int8_t> q8; std::vector<int32_t> c8;
	std::vector<int8_t> q6; std::vector<int32_t> c6; // 6-bit, every other sample held (half rate)
};

struct SmpMem {
	int sound = -1;
	int nz = 0;
	SmpZone z[8];
	int totalSamples = 0;
	double renderSeconds = 0;
};

// renders one SAMPLER sound into memory (any thread)
std::unique_ptr<SmpMem> renderSmpSound(int idx);

struct SmpEnv {
	int stage = 0; // 0 idle 1 attack 2 decay/sustain 3 release
	float v = 0.f, peak = 1.f, sus = 0.f, ra = 0.f, cd = 0.f, cr = 0.f;
	void set(const float* e, float fs, float as, float ds) {
		peak = e[1]; sus = e[3] * e[1];
		ra = peak / std::max(e[0] * as * fs, 1.f);
		cd = 1.f - std::exp(-4.f / std::max(e[2] * ds * fs, 1.f));
		cr = 1.f - std::exp(-4.f / std::max(e[4] * ds * fs, 1.f));
	}
	void on() { stage = 1; v = 0.f; }
	void off() { if (stage) stage = 3; }
	float next() {
		switch (stage) {
			case 1: v += ra; if (v >= peak) { v = peak; stage = 2; } break;
			case 2: v += (sus - v) * cd; break;
			case 3: v -= v * cr; if (v < 1e-5f) { v = 0.f; stage = 0; } break;
		}
		return v;
	}
};

struct SamplerEngine : Engine {
	struct Voice {
		bool gate = false;
		float note = 60.f, vel = 100.f, pan = 0.f;
		int zone = 0;
		double pos[2] = {0, 0};
		bool done[2] = {false, false};
		SmpEnv fenv, aenv;
		Ladder lp;
		int filtCounter = 0;
		const SmpMem* mem = nullptr;
		// per-layer caches: increment for the current note, and the staircase integral at pos
		float incNote = -1000.f; double inc[2] = {0, 0};
		double i8[2] = {0, 0}, i6[2] = {0, 0}; bool v8[2] = {false, false}, v6[2] = {false, false};
	};
	Voice v[MAX_SLOTS];
	const SmpSound* s = nullptr;
	int soundIdx = 0;
	float fs2 = 96000.f;
	Decimator2 decL, decR;
	bool synchronous = false; // host tests: render inside setSound

	SamplerEngine();
	~SamplerEngine();
	void init(float sampleRate) override;
	void setSound(int idx) override;
	void noteOn(int slot, float note, float vel, float pan) override;
	void setPitch(int slot, float note) override;
	void setPan(int slot, float pan) override { if (slot >= 0 && slot < MAX_SLOTS) v[slot].pan = pan; }
	void noteOff(int slot) override;
	void allOff() override;
	void process(float* L, float* R, int n, const Macros& m) override;
	bool ready() const override;
	bool slotActive(int slot) const override { return v[slot].gate || v[slot].aenv.stage != 0; }
	const SmpMem* current() const { return active.load(); }

private:
	Macros curM;
	std::atomic<SmpMem*> active{nullptr};
	std::atomic<int> requested{-1};
	std::mutex cacheMutex;
	std::unique_ptr<SmpMem> cache[4];
	int cacheAge[4] = {0, 0, 0, 0};
	int ageCounter = 0;
	std::thread worker;
	std::atomic<bool> quit{false};
	void workerLoop();
	SmpMem* obtain(int idx); // find in cache or render (not on the audio thread)
	float renderVoice(Voice& vc);
};

} // namespace hk
