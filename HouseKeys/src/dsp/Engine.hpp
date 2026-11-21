// Engine interface shared by the five sound engines.
#pragma once
#include "Dsp.hpp"

namespace hk {

static constexpr int MAX_SLOTS = 32; // 0..15 main notes, 16..31 their TWIN copies

struct Macros {
	float tone = 0.f;    // -1..1
	float attack = 0.f;  // -1..1 (+ = longer)
	float decay = 0.f;   // -1..1 (+ = longer)
	float mod = 0.f;     // 0..1
	float twin = 0.f;    // 0..1
	float grit = 0.5f;   // 0..1
	float attackScale() const { return std::exp2(3.f * attack); } // x1/8 .. x8
	float decayScale() const { return std::exp2(3.f * decay); }
};

// pan law: p = -1 (left) .. 1 (right); unity gain on both sides at centre
inline void panGains(float p, float& gl, float& gr) {
	float a = (clampf(p, -1.f, 1.f) + 1.f) * 0.25f * PI_F;
	gl = std::cos(a) * 1.41421356f;
	gr = std::sin(a) * 1.41421356f;
}

struct Engine {
	float fs = 48000.f;
	virtual ~Engine() {}
	virtual void init(float sampleRate) = 0;
	// index within this engine's own list
	virtual void setSound(int idx) = 0;
	// note is a MIDI note number (fractional), already including OCTAVE and TWIN detune;
	// vel is the effective velocity 1..127; pan -1..1
	virtual void noteOn(int slot, float note, float vel, float pan) = 0;
	virtual void setPitch(int slot, float note) = 0;
	virtual void setPan(int slot, float pan) = 0;
	virtual void noteOff(int slot) = 0;
	virtual void allOff() = 0;  // hard stop, silence now
	// adds into L and R
	virtual void process(float* L, float* R, int n, const Macros& m) = 0;
	virtual bool isMono() const { return false; }
	virtual int chorusDefault() const { return 0; }
	virtual bool ready() const { return true; } // false while a SAMPLER sound renders
	virtual bool slotActive(int slot) const = 0;
};

} // namespace hk
