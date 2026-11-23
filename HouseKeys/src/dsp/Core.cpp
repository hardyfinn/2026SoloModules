#include "Core.hpp"
#include "../generated/Sounds.hpp"
#include "../generated/Trims.hpp"
#include <cstdio>

namespace hk {

static const int ENGINE_FIRST[NUM_ENGINES + 1] = {0, 30, 54, 66, 88, 100};
static const char* ENGINE_NAMES[NUM_ENGINES] = {"FM", "POLY", "PD", "WAVE", "SMPL"};

// STAB chords (spec 3.4)
static const int STAB_N[10] = {1, 2, 2, 3, 3, 4, 4, 4, 3, 5};
static const int STAB_OFS[10][5] = {
	{0}, {0, 7}, {0, 12}, {0, 3, 7}, {0, 4, 7}, {0, 3, 7, 10}, {0, 4, 7, 11}, {0, 4, 7, 10}, {0, 5, 7}, {0, 3, 7, 10, 14}};

void soundLocate(int sound, int& engine, int& idx) {
	sound = std::max(0, std::min(NUM_SOUNDS - 1, sound));
	engine = 0;
	while (sound >= ENGINE_FIRST[engine + 1]) engine++;
	idx = sound - ENGINE_FIRST[engine];
}

const char* soundName(int sound) {
	int e, i; soundLocate(sound, e, i);
	switch (e) {
		case ENG_FM: return FM_SOUNDS[i].name;
		case ENG_POLY: return POLY_SOUNDS[i].name;
		case ENG_PD: return PD_SOUNDS[i].name;
		case ENG_WAVE: return WAVE_SOUNDS[i].name;
		default: return SMP_SOUNDS[i].name;
	}
}

int soundOctave(int sound) {
	int e, i; soundLocate(sound, e, i);
	switch (e) {
		case ENG_FM: {
			FmVoiceData d = decodeVmem(FM_SOUNDS[i].vmem, FM_SOUNDS[i].opz);
			return (int) std::floor((d.trps - 24) / 12.0);
		}
		case ENG_POLY: return POLY_SOUNDS[i].oct;
		case ENG_PD: return PD_SOUNDS[i].oct;
		case ENG_WAVE: {
			const WaveSound& w = WAVE_SOUNDS[i];
			for (int k = 0; k < 3; k++) if (w.o[k].on) return w.o[k].oct;
			return 0;
		}
		default: return 0;
	}
}

int engineFirstSound(int engine) { return ENGINE_FIRST[std::max(0, std::min(NUM_ENGINES - 1, engine))]; }
const char* engineName(int engine) { return ENGINE_NAMES[std::max(0, std::min(NUM_ENGINES - 1, engine))]; }

Core::Core() {
	eng[ENG_FM] = &fm; eng[ENG_POLY] = &poly; eng[ENG_PD] = &pd; eng[ENG_WAVE] = &wave; eng[ENG_SMP] = &smp;
	for (int i = 0; i < NUM_SOUNDS; i++) trimDb[i] = SOUND_TRIM_DB[i];
	for (int s = 0; s < 16; s++) slotOwner[s] = -1;
}

Core::~Core() {}

void Core::init(float sampleRate, bool synchronousSampler) {
	fs = sampleRate;
	smp.synchronous = synchronousSampler;
	for (auto* e : eng) e->init(fs);
	chorus.init(fs);
	tape.init(fs);
	dcL.setLP(5.f, fs); dcR.setLP(5.f, fs);
	curSound = -1; curEngine = -1;
	blockPos = BLOCK;
	fadeGain = 1.f; fadeState = 0;
}

void Core::setInputs(int channels, const float* voct, const float* gate, const float* vel) {
	nch = std::max(0, std::min(MAX_CH, channels));
	velPatched = vel != nullptr;
	for (int c = 0; c < MAX_CH; c++) {
		if (c >= nch) { inGate[c] = false; continue; }
		inNote[c] = 60.f + 12.f * voct[c];
		// Schmitt trigger: on above 1 V, off below 0.5 V
		if (!inGate[c] && gate[c] > 1.f) inGate[c] = true;
		else if (inGate[c] && gate[c] < 0.5f) inGate[c] = false;
		inVel[c] = vel ? clampf(vel[c] / 10.f, 0.f, 1.f) * 127.f : -1.f;
	}
}

void Core::setUserVoice(const uint8_t* vmem, bool opz, const char* name) {
	std::memcpy(userVmem, vmem, 128);
	userOpz = opz;
	std::snprintf(userNameBuf, sizeof(userNameBuf), "%s", name);
	userPending = true;
}

void Core::panic() {
	if (curEngine >= 0) eng[curEngine]->allOff();
	for (int s = 0; s < 16; s++) { slotOwner[s] = -1; twinPlaying[s] = false; }
	for (auto& c : ch) c.nslots = 0;
	monoGate = false;
}

int Core::chordSize() const { return std::min(STAB_N[stabCur], std::min(5, cur.polyLimit)); }
const int* Core::chordOffsets() const { return STAB_OFS[stabCur]; }

float Core::effVel(float v) const {
	return clampf(64.f + (v - 64.f) * cur.velocity, 1.f, 127.f);
}

void Core::slotOn(int s, float note, float vel) {
	Engine* e = eng[curEngine];
	slotNote[s] = note;
	float det = twinDet();
	e->noteOn(s, note - det, vel, -0.6f * twinCur);
	if (twinCur > 0.f) { e->noteOn(s + 16, note + det, vel, 0.6f * twinCur); twinPlaying[s] = true; }
}

void Core::slotOff(int s) {
	Engine* e = eng[curEngine];
	e->noteOff(s);
	if (twinPlaying[s]) { e->noteOff(s + 16); twinPlaying[s] = false; }
	slotOwner[s] = -1;
	slotFreed[s] = ++ageCounter;
}

void Core::slotPitch(int s, float note) {
	Engine* e = eng[curEngine];
	slotNote[s] = note;
	float det = twinDet();
	e->setPitch(s, note - det);
	if (twinPlaying[s]) e->setPitch(s + 16, note + det);
}

void Core::chOn(int c) {
	Ch& h = ch[c];
	int P = std::max(1, std::min(16, cur.polyLimit));
	int k = std::min(chordSize(), P);
	const int* ofs = chordOffsets();
	float base = h.note + 12.f * cur.octave;
	Engine* e = eng[curEngine];
	h.nslots = 0;
	for (int j = 0; j < k; j++) {
		int best = -1;
		for (;;) {
			// free slot: prefer silent ones, then the one released longest ago
			uint32_t bestAge = 0xffffffffu; bool bestSilent = false;
			for (int s = 0; s < P; s++) {
				if (slotOwner[s] >= 0) continue;
				bool silent = !e->slotActive(s) && !e->slotActive(s + 16);
				if (best < 0 || (silent && !bestSilent) || (silent == bestSilent && slotFreed[s] < bestAge)) {
					best = s; bestAge = slotFreed[s]; bestSilent = silent;
				}
			}
			if (best >= 0) break;
			// steal the oldest note's chord
			int victim = -1; uint32_t va = 0xffffffffu;
			for (int o = 0; o < MAX_CH; o++)
				if (o != c && ch[o].nslots > 0 && ch[o].age < va) { va = ch[o].age; victim = o; }
			if (victim < 0) break;
			for (int q = 0; q < ch[victim].nslots; q++) slotOff(ch[victim].slots[q]);
			ch[victim].nslots = 0;
		}
		if (best < 0) break;
		slotOwner[best] = c;
		h.slots[h.nslots++] = best;
		slotOn(best, base + ofs[j], h.vel);
	}
}

void Core::chOff(int c) {
	Ch& h = ch[c];
	for (int q = 0; q < h.nslots; q++) slotOff(h.slots[q]);
	h.nslots = 0;
}

void Core::chPitch(int c) {
	Ch& h = ch[c];
	const int* ofs = chordOffsets();
	float base = h.note + 12.f * cur.octave;
	for (int q = 0; q < h.nslots; q++) slotPitch(h.slots[q], base + ofs[q]);
}

// mono: last-note priority, legato (the engine glides when its slot is already gated)
void Core::monoUpdate(bool, int) {
	if (monoN == 0) {
		for (int s = 0; s < 16; s++) if (slotOwner[s] >= 0) slotOff(s);
		monoGate = false;
		return;
	}
	int top = monoStack[monoN - 1];
	const Ch& h = ch[top];
	int k = std::min(chordSize(), std::max(1, cur.polyLimit));
	const int* ofs = chordOffsets();
	float base = h.note + 12.f * cur.octave;
	for (int j = 0; j < k; j++) { slotOwner[j] = top; slotOn(j, base + ofs[j], h.vel); }
	monoChord = k;
	monoGate = true;
}

void Core::releaseAll() {
	if (curEngine < 0) return;
	for (int s = 0; s < 16; s++) if (slotOwner[s] >= 0 || twinPlaying[s]) slotOff(s);
	for (auto& c : ch) c.nslots = 0;
	monoGate = false;
}

void Core::retriggerHeld() {
	// held channels in the order they were pressed
	int order[MAX_CH]; int n = 0;
	for (int c = 0; c < MAX_CH; c++) if (ch[c].gate) order[n++] = c;
	std::sort(order, order + n, [&](int a, int b) { return ch[a].age < ch[b].age; });
	if (monoMode) {
		monoN = 0;
		for (int i = 0; i < n; i++) monoStack[monoN++] = order[i];
		monoGate = false;
		if (monoN) monoUpdate(true, monoStack[monoN - 1]);
	} else {
		for (int i = 0; i < n; i++) chOn(order[i]);
	}
}

void Core::switchSound(int sound, bool user) {
	if (curEngine >= 0) eng[curEngine]->allOff();
	for (int s = 0; s < 16; s++) { slotOwner[s] = -1; twinPlaying[s] = false; }
	for (auto& c : ch) c.nslots = 0;
	monoGate = false;
	int e, idx; soundLocate(sound, e, idx);
	if (user) {
		e = ENG_FM;
		fm.setVoice(userVmem, userOpz);
	} else {
		eng[e]->setSound(idx);
	}
	curEngine = e; curSound = sound; userOn = user;
	monoMode = eng[e]->isMono();
	fm.setUnitsNeeded(cur.polyLimit, twinCur > 0.f);
	chorus.reset();
	wasReady = eng[e]->ready();
	if (wasReady) retriggerHeld();
}

void Core::renderBlock() {
	const CoreParams& p = cur;
	// --- sound selection: fade 20 ms, switch, retrigger held gates
	bool wantUser = userPending;
	bool soundMoved = p.sound != curSound;
	if (curEngine < 0) {
		twinCur = p.twin; stabCur = p.stab;
		switchSound(p.sound, false);
		userPending = false;
	} else if (wantUser || soundMoved) {
		if (fadeState == 0) fadeState = 1;
	}
	float fadeStart = fadeGain, fadeEnd = fadeGain;
	if (fadeState == 1) {
		fadeEnd = std::max(0.f, fadeGain - BLOCK / (0.02f * fs));
	}

	// --- TWIN, STAB, polyphony changes
	if (curEngine >= 0) {
		Engine* e = eng[curEngine];
		bool monoNow = e->isMono();
		float tw = clampf(p.twin, 0.f, 1.f);
		if ((tw > 0.f) != (twinCur > 0.f)) {
			if (tw <= 0.f) {
				for (int s = 0; s < 16; s++) if (twinPlaying[s]) { e->noteOff(s + 16); twinPlaying[s] = false; }
			}
			fm.setUnitsNeeded(p.polyLimit, tw > 0.f);
		}
		if (std::fabs(tw - twinCur) > 1e-4f) {
			twinCur = tw;
			for (int s = 0; s < 16; s++) if (slotOwner[s] >= 0) {
				slotPitch(s, slotNote[s]);
				e->setPan(s, -0.6f * twinCur);
				if (twinPlaying[s]) e->setPan(s + 16, 0.6f * twinCur);
			}
		}
		if (p.stab != stabCur || monoNow != monoMode) {
			// re-voice held notes with the new chord
			releaseAll();
			stabCur = std::max(0, std::min(9, p.stab));
			monoMode = monoNow;
			if (e->ready()) retriggerHeld();
		}
	}

	// --- gates and pitch
	if (curEngine >= 0) {
		Engine* e = eng[curEngine];
		bool ready = e->ready();
		if (ready && !wasReady) { wasReady = true; retriggerHeld(); }
		wasReady = ready;
		float fixedV = p.fixedVel > 0 ? (float) p.fixedVel : (curEngine == ENG_FM ? 64.f : 100.f);
		for (int c = 0; c < MAX_CH; c++) {
			Ch& h = ch[c];
			bool g = inGate[c];
			if (g && !h.gate) {
				h.gate = true;
				h.note = inNote[c];
				h.vel = effVel(velPatched && inVel[c] >= 0.f ? inVel[c] : fixedV);
				h.age = ++ageCounter;
				if (monoMode) {
					monoStack[monoN++] = c;
					if (ready) monoUpdate(true, c);
				} else if (ready) chOn(c);
			} else if (!g && h.gate) {
				h.gate = false;
				if (monoMode) {
					int w = 0;
					for (int i = 0; i < monoN; i++) if (monoStack[i] != c) monoStack[w++] = monoStack[i];
					bool wasTop = monoN > 0 && monoStack[monoN - 1] == c;
					monoN = w;
					if (ready && (wasTop || monoN == 0)) monoUpdate(false, -1);
				} else chOff(c);
			} else if (g && std::fabs(inNote[c] - h.note) > 1e-4f) {
				h.note = inNote[c];
				if (monoMode) {
					if (monoN && monoStack[monoN - 1] == c && monoGate) {
						float base = h.note + 12.f * p.octave;
						for (int j = 0; j < monoChord; j++) slotPitch(j, base + chordOffsets()[j]);
					}
				} else chPitch(c);
			}
		}
	}

	// --- render the engine
	std::memset(bufL, 0, sizeof(bufL)); std::memset(bufR, 0, sizeof(bufR));
	Macros m;
	m.tone = clampf(p.tone, -1.f, 1.f); m.attack = clampf(p.attack, -1.f, 1.f); m.decay = clampf(p.decay, -1.f, 1.f);
	m.mod = clampf(p.mod, 0.f, 1.f); m.twin = twinCur; m.grit = clampf(p.grit, 0.f, 1.f);
	Engine* e = eng[curEngine];
	e->process(bufL, bufR, BLOCK, m);

	// trim, stab level, fade
	float trim = 0.f;
	if (applyTrims) {
		if (userOn) { for (int i = 0; i < 30; i++) trim += trimDb[i]; trim /= 30.f; }
		else trim = trimDb[curSound];
	}
	float g = dbToGain(trim);
	float stabTarget = 1.f / std::sqrt((float) std::min(STAB_N[stabCur], std::max(1, p.polyLimit)));
	float sg0 = stabGain;
	stabGain += (stabTarget - stabGain) * std::min(1.f, BLOCK / (0.01f * fs));
	for (int i = 0; i < BLOCK; i++) {
		float t = (i + 1) / (float) BLOCK;
		float gi = g * (sg0 + (stabGain - sg0) * t) * (fadeStart + (fadeEnd - fadeStart) * t);
		bufL[i] = dcL.hp(bufL[i] * gi); bufR[i] = dcR.hp(bufR[i] * gi);
	}
	fadeGain = fadeEnd;
	if (fadeState == 1 && fadeGain <= 0.f) {
		bool user = userPending;
		userPending = false;
		switchSound(p.sound, user);
		fadeGain = 1.f; fadeState = 0;
	}

	// chorus
	int cm = p.chorus == 0 ? (userOn ? 0 : e->chorusDefault()) : p.chorus - 1;
	chorus.process(bufL, bufR, BLOCK, cm, m.grit);
	// tape
	tape.process(bufL, bufR, BLOCK, clampf(p.tape, 0.f, 1.f));
	// level and soft limit (0 dBFS = 10 V)
	float lv = clampf(p.level, 0.f, 1.f);
	float og = 2.f * (lv / 0.5f) * (lv / 0.5f);
	float og0 = outGain;
	for (int i = 0; i < BLOCK; i++) {
		float gi = 10.f * (og0 + (og - og0) * (i + 1) / (float) BLOCK);
		outL[i] = 12.f * std::tanh(bufL[i] * gi / 12.f);
		outR[i] = 12.f * std::tanh(bufR[i] * gi / 12.f);
	}
	outGain = og;
}

void Core::process(float* L, float* R, int n, const CoreParams& p) {
	for (int i = 0; i < n; i++) {
		if (blockPos >= BLOCK) {
			int oldPoly = cur.polyLimit;
			cur = p;
			cur.polyLimit = (p.polyLimit == 4 || p.polyLimit == 6 || p.polyLimit == 16) ? p.polyLimit : 8;
			if (curEngine >= 0 && cur.polyLimit != oldPoly) {
				releaseAll();
				fm.setUnitsNeeded(cur.polyLimit, twinCur > 0.f);
				if (eng[curEngine]->ready()) retriggerHeld();
			}
			renderBlock();
			blockPos = 0;
		}
		L[i] = outL[blockPos]; R[i] = outR[blockPos];
		blockPos++;
	}
}

} // namespace hk
