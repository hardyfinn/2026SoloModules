// FM engine: a DX100 (YM2164 + floating-point DAC) or TX81Z (YM2414) playing real
// 4-operator voice data. Spec section 4.
#pragma once
#include "Engine.hpp"
#include "SoundTypes.hpp"
#include <memory>

namespace hk {

struct FmUnit; // one chip ("one synth"): 8 channels

struct FmVoiceData {
	// decoded VMEM voice (operators in register order M1 M2 C1 C2 = OP4 OP2 OP3 OP1)
	struct Op { int ar, d1r, d2r, rr, d1l, ls, ame, ebs, kvs, out, crs, rs, det, osw, fine, egs, fix, fixrg; } op[4];
	int alg, fb, sync, lfs, lfd, pmd, amd, pms, ams, lfw, trps, mwPitch, mwAmp;
	bool opz;
	char name[11];
};

// decode one 128-byte VMEM record
FmVoiceData decodeVmem(const uint8_t* v, bool opz);
// find a 32-voice 4-operator bank in a .syx file; returns number of voices (0 on failure)
int parseVmemBank(const std::vector<uint8_t>& file, uint8_t out[32][128]);

// MUL, DT2 pair for a CRS value (0..63) and its ratio
void crsToMulDt2(int crs, int& mul, int& dt2, float* ratio = nullptr);

struct FmEngine : Engine {
	FmEngine();
	~FmEngine();
	void init(float sampleRate) override;
	void setSound(int idx) override;
	void setVoice(const uint8_t* vmem, bool opz); // any VMEM record (user banks)
	void noteOn(int slot, float note, float vel, float pan) override;
	void setPitch(int slot, float note) override;
	void setPan(int slot, float pan) override;
	void noteOff(int slot) override;
	void allOff() override;
	void process(float* L, float* R, int n, const Macros& m) override;
	bool slotActive(int slot) const override;
	void setUnitsNeeded(int polyLimit, bool twin) { polyLimit16 = polyLimit > 8; twinOn = twin; }

	// test hooks
	float chipRate() const { return chipFs; }
	FmVoiceData voice;
	float gritOverride = -1.f;

private:
	std::unique_ptr<FmUnit> units[4]; // A1 A2 B1 B2
	float chipFs = 55930.f;
	bool polyLimit16 = false, twinOn = false;
	Resampler rsL, rsR;
	Biquad lpL, lpR;
	OnePole hpL, hpR;
	Noise noise;
	uint8_t curVmem[128] = {};
	bool curOpz = false;
	float slotNote[MAX_SLOTS] = {};
	float slotPan[MAX_SLOTS] = {};
	int macroCounter = 0;
	Macros lastMacros;
	void chipSample(float& l, float& r, float grit);
};

} // namespace hk
