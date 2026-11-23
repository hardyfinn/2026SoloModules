// House Keys core: note logic (spec 3), sound switching, TWIN, STAB, effects and output
// stage (spec 2, 9). No Rack dependency; used by the module and the host test program.
#pragma once
#include "Engine.hpp"
#include "FmEngine.hpp"
#include "PolyEngine.hpp"
#include "PdEngine.hpp"
#include "WaveEngine.hpp"
#include "SamplerEngine.hpp"
#include "Effects.hpp"

namespace hk {

static constexpr int NUM_SOUNDS = 100;
static constexpr int BLOCK = 16;
static constexpr int MAX_CH = 16;

// sound index (0..99) -> engine and index inside the engine
void soundLocate(int sound, int& engine, int& idx);
const char* soundName(int sound);
int soundOctave(int sound);   // the sound's own octave (for trims and tests)
int engineFirstSound(int engine);
const char* engineName(int engine);

struct CoreParams {
	int sound = 0;          // 0..99
	int octave = 0;         // -2..2
	float tone = 0.f, attack = 0.f, decay = 0.f; // -1..1
	float velocity = 1.f;   // 0..2
	float mod = 0.f, twin = 0.f; // 0..1
	float grit = 0.5f, tape = 0.f; // 0..1
	float level = 0.5f;     // 0..1, 0.5 = default
	int chorus = 0;         // 0 SOUND, 1 OFF, 2 I, 3 II
	int stab = 0;           // 0..9
	int polyLimit = 8;      // 4, 6, 8, 16
	int fixedVel = -1;      // -1 = default (100, FM 64)
};

struct Core {
	Core();
	~Core();
	void init(float sampleRate, bool synchronousSampler = false);

	// per-channel inputs in volts, sampled once per block
	// vel == nullptr when VEL is unpatched
	void setInputs(int channels, const float* voct, const float* gate, const float* vel);
	// renders n samples (any n) into L, R in volts
	void process(float* L, float* R, int n, const CoreParams& p);
	void panic();

	// user voice from a loaded .syx bank (plays through FM until SOUND changes)
	void setUserVoice(const uint8_t* vmem, bool opz, const char* name);
	bool userActive() const { return userOn; }
	const char* userName() const { return userNameBuf; }

	int currentSound() const { return curSound; }
	int currentEngine() const { return curEngine; }
	bool loading() const { return curEngine >= 0 && !eng[curEngine]->ready(); }

	// trims per sound in dB (generated); test hook to override
	float trimDb[NUM_SOUNDS];
	bool applyTrims = true;

	FmEngine fm; PolyEngine poly; PdEngine pd; WaveEngine wave; SamplerEngine smp;
	Engine* eng[NUM_ENGINES];

private:
	float fs = 48000.f;
	// inputs
	int nch = 0;
	float inNote[MAX_CH] = {}, inVel[MAX_CH] = {};
	bool inGate[MAX_CH] = {};
	bool velPatched = false;
	// channel state
	struct Ch {
		bool gate = false;
		float note = 60.f, vel = 100.f;
		int slots[5] = {-1, -1, -1, -1, -1};
		int nslots = 0;
		uint32_t age = 0;
	} ch[MAX_CH];
	uint32_t ageCounter = 0;
	int slotOwner[16];
	uint32_t slotFreed[16] = {};
	float slotNote[16] = {}; // last note sent to the main slot (without twin detune)
	bool twinPlaying[16] = {};
	// mono
	int monoStack[MAX_CH]; int monoN = 0;
	bool monoGate = false; int monoChord = 0;
	// switching
	int curSound = -1, curEngine = -1, pendingSound = -1;
	bool userOn = false, userPending = false;
	uint8_t userVmem[128] = {}; bool userOpz = false; char userNameBuf[16] = {};
	float fadeGain = 1.f; int fadeState = 0; // 0 none, 1 fading out
	bool wasReady = true;
	// current settings
	CoreParams cur;
	float twinCur = 0.f;
	int stabCur = 0;
	float stabGain = 1.f, outGain = 1.f;
	bool monoMode = false;
	Chorus chorus; Tape tape;
	OnePole dcL, dcR; // output coupling (every modelled machine is AC coupled)
	float bufL[BLOCK], bufR[BLOCK];
	int blockPos = BLOCK;
	float outL[BLOCK], outR[BLOCK];

	void renderBlock();
	void switchSound(int sound, bool user);
	void retriggerHeld();
	void chOn(int c);
	void chOff(int c);
	void chPitch(int c);
	float effVel(float v) const;
	float twinDet() const { return 0.12f * twinCur; }
	void slotOn(int slot, float note, float vel);
	void slotOff(int slot);
	void slotPitch(int slot, float note);
	void releaseAll();
	int chordSize() const;
	const int* chordOffsets() const;
	void monoUpdate(bool newNote, int velCh);
};

} // namespace hk
