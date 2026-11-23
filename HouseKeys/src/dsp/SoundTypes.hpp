// Data layout of the 100 sounds. Field order matches tools/gen_sounds.py.
#pragma once
#include <cstdint>

namespace hk {

enum EngineId { ENG_FM = 0, ENG_POLY, ENG_PD, ENG_WAVE, ENG_SMP, NUM_ENGINES };

struct FmSound {
	const char* name;
	bool opz;          // TX81Z chip (extra waveforms)
	uint8_t vmem[128]; // one 4-operator VMEM voice record
};

enum PolyModel { PM_JUNO = 0, PM_JUP, PM_JX, PM_P800 };
enum PolyW2 { W2_SAW = 0, W2_PULSE, W2_TRI, W2_SQ };

struct PolySound {
	const char* name;
	int model, oct;
	float saw, pulse, tri, sub, noise, pw, pwm;
	float lvl2; int wave2, oct2; float semi2, fine2; int sync; float sweep2;
	int hpf; float cut, res, env, kbd, velcut; int poles;
	float fenv[4], aenv[4]; bool aenvGate;
	float lfoRate, lfoDelay, lfoPitch;
	float vel; bool mono; float glide; int chorus;
};

enum PdWave { PDW_SAW = 0, PDW_SQUARE, PDW_PULSE, PDW_DSINE, PDW_SAWPULSE, PDW_RES1, PDW_RES2, PDW_RES3 };

struct PdLine {
	int w[2]; int nw;
	float dcw[6]; // start, A, peak, D, sustain, R
	float amp[4]; bool ampGate;
};

struct PdSound {
	const char* name;
	int oct, mode; // mode 0 "1", 1 "1+1'", 2 "1+2'"
	PdLine l1, l2; float lvl2;
	float det[3]; int ring;
	float vibRate, vibDelay, vibCents, tremRate, tremDepth;
	float dcwkey; int chorus; float vel; bool mono; float glide;
};

enum ModSrc { SRC_OFF = 0, SRC_LFO1, SRC_LFO2, SRC_LFO3, SRC_ENV1, SRC_ENV2, SRC_ENV3, SRC_ENV4,
	SRC_VEL, SRC_VEL2, SRC_KBD, SRC_KBD2, SRC_WHEEL, SRC_PRESS, NUM_SRC };

struct WaveOsc {
	bool on; int wave, oct, semi, fine, level, pmSrc, pmDepth, amSrc, amDepth;
};

struct WaveSound {
	const char* name;
	WaveOsc o[3];
	int cut, res, kbd, m1Src, m1Depth, m2Src, m2Depth;
	int env[4][10]; // L1 L2 L3 T1 T2 T3 T4 LV T1V TK
	int lfo[3][6];  // freq wave L1 delay L2 modsrc
	int pan, sync, am; bool mono; int glide;
};

enum SmpGen { GEN_STRINGS = 0, GEN_PIANO, GEN_CHOIR, GEN_BRASS, GEN_ORCHHIT, GEN_UPRIGHT, GEN_VIBES, GEN_CHORD };

struct SmpSound {
	const char* name;
	int gen; float prm[6]; int notes[6]; int nnotes;
	int zones[8]; int nz; float sr, len;
	bool loop; float loopp[3]; // start, length, crossfade (s)
	float det, mix, cut, res, kbd, env;
	float fenv[5], aenv[5]; // attack, peak, decay, sustain, release
	float vel, velcut;
};

} // namespace hk
