// Facet: a four-tone digital layer synth in the JD-800 tradition (all waves and presets our own).
// Sound engine: FacetEngine.hpp + FacetFx.hpp. Waves: FacetWaves.hpp. Parameters: FacetParams.hpp.
//
// Every patch parameter (about 350) is a real Rack parameter, so Ctrl+R, MIDI-MAP and the
// right-click menu reach all of them. The panel sliders are "proxies": they edit whichever
// tone (A-D), envelope (PITCH/TVF/TVA) or LFO (1/2) is selected, and follow it when it changes.
#include "plugin.hpp"
#include "FacetEngine.hpp"
#include "FacetPresets.hpp"
#include <osdialog.h>
#include <atomic>
#include <fstream>

using namespace facet;

// ---- LAYOUT (same numbers in res/make_panel.py) ---- 56 HP
static const float TSEL_X[4] = {12.f, 25.f, 38.f, 51.f};
static const float TSEL_Y = 25.f, TON_Y = 34.f;
static const float LINK_X = 22.f, SOLO_X = 50.f, LINKSOLO_Y = 45.f;
static const float SCR_X = 62.f, SCR_Y = 12.5f, SCR_W = 100.f, SCR_H = 31.f;
static const float NAV_Y = 49.f, PREV_X = 68.f, NEXT_X = 80.f, PAGE_X = 98.f, DATA_X = 152.f;
static const float PRESET_X[2] = {171.f, 181.f};
static const float MEM_X0 = 193.f, MEM_DX = 10.6f, MEM_Y = 25.f, SAVE_X = 277.8f;
static const float MOV_Y = 40.f, RAMT_X = 176.f, RAND_X = 194.f, MUT_X = 216.f, LOCK_X = 234.f, SEL_X = 252.f;
static const float SL_Y = 78.f;
static const float WAVE_X = 14.f, WAVE_Y = 74.f, COARSE_X = 30.f, FINE_X = 43.f;
static const float FM_X = 64.f, FM_Y[3] = {69.f, 77.f, 85.f};
static const float CUT_X = 77.f, RES_X = 91.f, FENV_X = 105.f;
static const float LEV_X = 127.f, VELO_X = 141.f;
static const float ES_X = 166.f, ENV_X[5] = {180.f, 192.f, 204.f, 216.f, 228.f};
static const float LS_X[2] = {244.f, 252.f}, LS_Y = 70.f, LRATE_X = 248.f, LRATE_Y = 84.f;
static const float LDEPTH_X = 274.f, LDEPTH_Y[3] = {69.f, 80.f, 91.f};
static const float FX_X0 = 5.f, FX_DX = 20.2f, FX_Y = 114.f, FX_LATCH_DX = 4.f, FX_KNOB_DX = 13.f;
static const float JACK_X0 = 152.f, JACK_DX = 10.5f, JACK_Y = 117.f;
static const float LEARN_Y = 103.5f, DAC_X = 262.f, DAC_Y = 103.5f;
static const float OUTL_X = 252.f, OUTR_X = 263.f, OLEV_X = 275.f;

enum Proxy { PX_WAVE, PX_COARSE, PX_FINE, PX_CUT, PX_RES, PX_FENV, PX_LEVEL, PX_VELO, PX_E1, PX_E2, PX_E3, PX_E4, PX_E5,
	PX_LRATE, PX_LPITCH, PX_LTVF, PX_LTVA, PX_DATA, NUM_PX };

static const int FX_ON[7] = {C_DS_ON, C_PH_ON, C_SP_ON, C_EH_ON, C_CH_ON, C_DL_ON, C_RV_ON};
static const int FX_KNOB[7] = {C_DS_DRIVE, C_PH_MIX, C_SP_WIDTH, C_EH_MIX, C_CH_LEVEL, C_DL_CLEV, C_RV_LEVEL};

// ---- screen pages
struct Page {
	const char* title;
	bool tone;
	std::vector<int> e; // tone-relative or common-relative parameter indices
};
static const std::vector<Page>& pages() {
	static std::vector<Page> P;
	if (P.empty()) {
		P.push_back({"WG", true, {T_WAVE, T_COARSE, T_FINE, T_PRAND, T_PKF, T_BENDSW, T_ATBEND, T_ON}});
		P.push_back({"PITCH ENV", true, {T_PE_DEPTH, T_PE_VELO, T_PE_TVELO, T_PE_TKF, T_PE_L0, T_PE_T1, T_PE_L1, T_PE_T2, T_PE_T3, T_PE_L2}});
		P.push_back({"TVF", true, {T_F_MODE, T_F_CUT, T_F_RES, T_F_KF, T_F_ENV, T_F_AT, T_MODCUT}});
		P.push_back({"TVF ENV", true, {T_FE_VELO, T_FE_TVELO, T_FE_TKF, T_FE_T1, T_FE_L1, T_FE_T2, T_FE_L2, T_FE_T3, T_FE_SUS, T_FE_T4, T_FE_L4}});
		P.push_back({"TVA", true, {T_LEVEL, T_VCURVE, T_VSENS, T_BIASDIR, T_BIASPT, T_BIASLV, T_A_AT, T_PAN}});
		P.push_back({"TVA ENV", true, {T_AE_TVELO, T_AE_TKF, T_AE_T1, T_AE_L1, T_AE_T2, T_AE_L2, T_AE_T3, T_AE_SUS, T_AE_T4}});
		std::vector<int> l1, l2;
		for (int k = 0; k < 9; k++) {
			l1.push_back(T_L1_RATE + k);
			l2.push_back(T_L2_RATE + k);
		}
		P.push_back({"LFO 1", true, l1});
		P.push_back({"LFO 2", true, l2});
		P.push_back({"CONTROL", true, {T_KEYLO, T_KEYHI, T_MODLFO, T_MODCUT}});
		P.push_back({"FX: GROUP A + DIST", false, {C_ORDER, C_DS_ON, C_DS_TYPE, C_DS_DRIVE, C_DS_LEVEL}});
		P.push_back({"FX: PHASER", false, {C_PH_ON, C_PH_MAN, C_PH_RATE, C_PH_DEPTH, C_PH_RESO, C_PH_MIX}});
		P.push_back({"FX: SPECTRUM", false, {C_SP_ON, C_SP_B1, C_SP_B2, C_SP_B3, C_SP_B4, C_SP_B5, C_SP_B6, C_SP_WIDTH}});
		P.push_back({"FX: ENHANCER", false, {C_EH_ON, C_EH_SENS, C_EH_MIX}});
		P.push_back({"FX: CHORUS", false, {C_CH_ON, C_CH_RATE, C_CH_DEPTH, C_CH_DELAY, C_CH_FB, C_CH_LEVEL}});
		P.push_back({"FX: DELAY", false, {C_DL_ON, C_DL_CTIME, C_DL_CLEV, C_DL_LTIME, C_DL_LLEV, C_DL_RTIME, C_DL_RLEV, C_DL_FB}});
		P.push_back({"FX: REVERB", false, {C_RV_ON, C_RV_TYPE, C_RV_PRE, C_RV_EARLY, C_RV_HFD, C_RV_TIME, C_RV_LEVEL}});
		P.push_back({"FX: EQ", false, {C_EQ_LOF, C_EQ_LOG, C_EQ_MIDF, C_EQ_MIDQ, C_EQ_MIDG, C_EQ_HIF, C_EQ_HIG}});
		P.push_back({"COMMON", false, {C_LEVEL, C_OCT, C_BENDR, C_SOLO, C_LEGATO, C_PORTA, C_PTIME, C_DAC90}});
	}
	return P;
}

// Minimal WAV reader: PCM 8/16/24/32 bit or float, any channel count (mixed to mono), up to 20 s.
static bool readWav(const std::string& path, Wave& out, std::string& err) {
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		err = "could not open file";
		return false;
	}
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	auto u16 = [&](size_t i) { return (uint32_t) b[i] | ((uint32_t) b[i + 1] << 8); };
	auto u32 = [&](size_t i) { return u16(i) | (u16(i + 2) << 16); };
	if (b.size() < 44 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) {
		err = "not a WAV file";
		return false;
	}
	int fmt = 0, ch = 0, bits = 0;
	uint32_t rate = 0;
	size_t dataPos = 0, dataLen = 0, p = 12;
	while (p + 8 <= b.size()) {
		uint32_t len = u32(p + 4);
		if (!std::memcmp(&b[p], "fmt ", 4) && p + 24 <= b.size()) {
			fmt = u16(p + 8);
			ch = u16(p + 10);
			rate = u32(p + 12);
			bits = u16(p + 22);
			if (fmt == 0xFFFE && p + 34 <= b.size()) fmt = u16(p + 32);
		}
		else if (!std::memcmp(&b[p], "data", 4)) {
			dataPos = p + 8;
			dataLen = std::min((size_t) len, b.size() - dataPos);
		}
		p += 8 + (size_t) len + (len & 1);
	}
	if (!dataPos || ch < 1 || rate < 1000 || !(fmt == 1 || fmt == 3)) {
		err = "unsupported WAV format";
		return false;
	}
	int bps = bits / 8;
	if (bps < 1 || (fmt == 3 && bits != 32)) {
		err = "unsupported bit depth";
		return false;
	}
	size_t frames = std::min(dataLen / (bps * ch), (size_t) rate * 20);
	out.smp.assign(frames, 0.f);
	out.smpRate = (float) rate;
	float pk = 0.f;
	for (size_t i = 0; i < frames; i++) {
		float s = 0.f;
		for (int c = 0; c < ch; c++) {
			size_t q = dataPos + (i * ch + c) * bps;
			float v = 0.f;
			if (fmt == 3) {
				uint32_t w = u32(q);
				std::memcpy(&v, &w, 4);
			}
			else if (bits == 8) v = (b[q] - 128) / 128.f;
			else if (bits == 16) v = (int16_t) u16(q) / 32768.f;
			else if (bits == 24) v = ((int32_t) ((b[q] << 8) | (b[q + 1] << 16) | (b[q + 2] << 24)) >> 8) / 8388608.f;
			else if (bits == 32) v = (int32_t) u32(q) / 2147483648.f;
			s += v;
		}
		out.smp[i] = s / ch;
		pk = std::max(pk, std::fabs(out.smp[i]));
	}
	if (pk > 0.f)
		for (float& x : out.smp) x *= 0.9f / pk;
	return true;
}

struct Facet;

// A panel slider/knob that edits whichever tone/envelope/LFO/screen entry is selected.
struct ProxyQuantity : ParamQuantity {
	int px = 0;
	Facet* fm();
	int target();
	ParamQuantity* tq() {
		int t = target();
		return t >= 0 ? module->paramQuantities[t] : nullptr;
	}
	std::string getLabel() override;
	std::string getDisplayValueString() override {
		ParamQuantity* q = tq();
		return q ? q->getDisplayValueString() : "";
	}
	std::string getUnit() override {
		ParamQuantity* q = tq();
		return q ? q->getUnit() : "";
	}
	void setDisplayValueString(std::string s) override;
	void reset() override;
};

struct Facet : Module {
	enum ParamIds {
		// 0 .. NPV-1 are the patch parameters
		PX_PARAM = NPV,
		TSEL_PARAM = PX_PARAM + NUM_PX,
		FMODE_PARAM = TSEL_PARAM + 4,
		ENVSEL_PARAM = FMODE_PARAM + 3,
		LFOSEL_PARAM = ENVSEL_PARAM + 3,
		PREV_PARAM = LFOSEL_PARAM + 2,
		NEXT_PARAM, PAGE_PARAM, PRESET_DN_PARAM, PRESET_UP_PARAM,
		MEM_PARAM,
		SAVE_PARAM = MEM_PARAM + 8,
		RAND_PARAM, SCOPE_PARAM, LEARN1_PARAM, LEARN2_PARAM,
		LINK_PARAM, SOLO_PARAM, LOCK_PARAM, RANDAMT_PARAM, MUTATE_PARAM, CV1AMT_PARAM, CV2AMT_PARAM,
		NUM_PARAMS
	};
	enum InputIds { VOCT_INPUT, GATE_INPUT, VEL_INPUT, PRESS_INPUT, MOD_INPUT, BEND_INPUT, CV1_INPUT, CV2_INPUT, LOCK_INPUT, NUM_INPUTS };
	enum OutputIds { L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds {
		TSEL_LIGHT, TON_LIGHT = TSEL_LIGHT + 4, FMODE_LIGHT = TON_LIGHT + 4, ENVSEL_LIGHT = FMODE_LIGHT + 3, LFOSEL_LIGHT = ENVSEL_LIGHT + 3,
		MEM_LIGHT = LFOSEL_LIGHT + 2, SAVE_LIGHT = MEM_LIGHT + 8, RAND_LIGHT, LEARN1_LIGHT, LEARN2_LIGHT, LINK_LIGHT, SOLO_LIGHT, LOCK_LIGHT,
		FX_LIGHT, DAC_LIGHT = FX_LIGHT + 7, NUM_LIGHTS
	};

	facet::Engine engine;
	In in;
	// selection state
	int selTone = 0, envSel = 1, lfoSel = 0, page = 0, cursor = 0, scope = SC_ALL;
	bool wild = false;
	// proxies
	float pxLast[NUM_PX];
	int touched = -1;
	float touchedTime = 0.f;
	// memories and presets
	float mem[8][NPV];
	std::string memName[8];
	int currentMem = -1, presetIdx = 0;
	std::string patchName = "Init";
	bool saveArmed = false;
	std::string flash;
	float flashTime = 0.f;
	// mutation
	float mut[NPV];
	Rng rng{(uint32_t) random::u32()};
	int lastNoteOns = 0;
	// CV learn
	int cvTarget[2] = {-1, -1};
	bool learnArmed[2] = {false, false};
	float learnSnap[NPV];
	// user waves (loaded on the UI thread, swapped in by process())
	std::string userPath[NUM_USER];
	bool userLoop[NUM_USER] = {};
	Wave pendingWave[NUM_USER];
	std::atomic<int> pendingState[NUM_USER];
	// misc
	dsp::SchmittTrigger btn[NUM_PARAMS - TSEL_PARAM];
	dsp::ClockDivider ctl, lightDiv;
	float randFlash = 0.f, blink = 0.f;
	bool loadingPreset = false;

	Facet() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const Rom& rom = Rom::get();
		std::vector<std::string> waveNames;
		for (const Wave& w : rom.waves) waveNames.push_back(w.name);
		for (int i = 0; i < NPV; i++) {
			Def d = defOf(i);
			std::string name = isTone(i) ? string::f("Tone %c %s", 'A' + i / NT, d.longName) : std::string(d.longName);
			name[isTone(i) ? 7 : 0] = std::toupper(name[isTone(i) ? 7 : 0]);
			ParamQuantity* q;
			if (isTone(i) && i % NT == T_WAVE) q = configSwitch(i, 0.f, (float) (rom.size() - 1), 0.f, name, waveNames);
			else if (d.choices >= 0) {
				int n;
				const char* const* L = choiceList(d.choices, n);
				q = configSwitch(i, d.min, d.max, d.def, name, std::vector<std::string>(L, L + n));
			}
			else {
				q = configParam(i, d.min, d.max, paramDefault(i), name);
				q->snapEnabled = d.snap;
			}
			q->defaultValue = paramDefault(i);
			// RAND / Ctrl+R are handled by randomizePatch() so the rules below always apply
			q->randomizeEnabled = false;
		}
		static const char* const PXN[NUM_PX] = {"Wave", "Pitch coarse", "Pitch fine", "Cutoff", "Resonance", "Filter env depth",
			"Tone level", "Velocity sens", "Env slider 1", "Env slider 2", "Env slider 3", "Env slider 4", "Env slider 5",
			"LFO rate", "LFO -> pitch", "LFO -> cutoff", "LFO -> level", "Data"};
		for (int p = 0; p < NUM_PX; p++) {
			ProxyQuantity* q = configParam<ProxyQuantity>(PX_PARAM + p, 0.f, 1.f, 0.5f, PXN[p]);
			q->px = p;
			q->randomizeEnabled = false;
			pxLast[p] = -1.f;
		}
		for (int t = 0; t < 4; t++) configButton(TSEL_PARAM + t, string::f("Edit tone %c", 'A' + t));
		const char* fm[3] = {"LPF", "BPF", "HPF"};
		for (int k = 0; k < 3; k++) configButton(FMODE_PARAM + k, string::f("Filter mode %s (selected tone)", fm[k]));
		const char* es[3] = {"PITCH", "TVF", "TVA"};
		for (int k = 0; k < 3; k++) configButton(ENVSEL_PARAM + k, string::f("Envelope sliders edit the %s envelope", es[k]));
		for (int k = 0; k < 2; k++) configButton(LFOSEL_PARAM + k, string::f("LFO knobs edit LFO %d", k + 1));
		configButton(PREV_PARAM, "Screen: previous entry");
		configButton(NEXT_PARAM, "Screen: next entry");
		configButton(PAGE_PARAM, "Screen: next page");
		configButton(PRESET_DN_PARAM, "Previous factory preset");
		configButton(PRESET_UP_PARAM, "Next factory preset");
		for (int m = 0; m < 8; m++) configButton(MEM_PARAM + m, string::f("Memory %d", m + 1));
		configButton(SAVE_PARAM, "Save (then press a memory)");
		configButton(RAND_PARAM, "Randomise (by RAND AMT, in the SEL group)");
		configButton(SCOPE_PARAM, "SEL: what RAND and MUTATE touch (all, waves, filter, envelopes, LFOs, effects)");
		configButton(LEARN1_PARAM, "Learn CV 1 (press, then move the control it should drive)");
		configButton(LEARN2_PARAM, "Learn CV 2 (press, then move the control it should drive)");
		configSwitch(LINK_PARAM, 0.f, 1.f, 0.f, "Link all tones (sliders move all four tones together)", {"off", "on"})->randomizeEnabled = false;
		configSwitch(SOLO_PARAM, 0.f, 1.f, 0.f, "Solo tone (hear only the tone you are editing)", {"off", "on"})->randomizeEnabled = false;
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock (freeze what MUTATE has changed)", {"off", "on"})->randomizeEnabled = false;
		configParam(RANDAMT_PARAM, 0.f, 1.f, 0.25f, "Rand amount (small = nudge the patch, full = new patch)", "%", 0.f, 100.f)->randomizeEnabled = false;
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (chance per note that the SEL group drifts)", "%", 0.f, 100.f)->randomizeEnabled = false;
		configParam(CV1AMT_PARAM, -1.f, 1.f, 1.f, "CV 1 amount", "%", 0.f, 100.f)->randomizeEnabled = false;
		configParam(CV2AMT_PARAM, -1.f, 1.f, 1.f, "CV 2 amount", "%", 0.f, 100.f)->randomizeEnabled = false;
		configInput(VOCT_INPUT, "V/OCT (polyphonic, up to 16 voices)");
		configInput(GATE_INPUT, "Gate (polyphonic)");
		configInput(VEL_INPUT, "Velocity (0-10 V, polyphonic)");
		configInput(PRESS_INPUT, "Pressure / aftertouch (0-10 V, polyphonic)");
		configInput(MOD_INPUT, "Mod wheel (0-10 V)");
		configInput(BEND_INPUT, "Pitch bend (+-5 V)");
		configInput(CV1_INPUT, "CV 1 (+-5 V = full range of the learned control)");
		configInput(CV2_INPUT, "CV 2 (+-5 V = full range of the learned control)");
		configInput(LOCK_INPUT, "Lock gate (high = locked)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		for (int i = 0; i < NUM_USER; i++) pendingState[i] = 0;
		std::memset(mut, 0, sizeof(mut));
		ctl.setDivision(32);
		lightDiv.setDivision(256);
		restoreFactoryMemories();
		loadPreset(0);
	}

	// ------------------------------------------------ patch helpers
	void writePatch(const float* v) {
		for (int i = 0; i < NPV; i++) params[i].setValue(v[i]);
		std::memset(mut, 0, sizeof(mut));
	}
	void readPatch(float* v) {
		for (int i = 0; i < NPV; i++) v[i] = params[i].getValue();
	}
	void restoreFactoryMemories() {
		const auto& pr = factoryPresets();
		for (int m = 0; m < 8; m++) {
			std::copy(pr[DEFAULT_MEMS[m]].v.begin(), pr[DEFAULT_MEMS[m]].v.end(), mem[m]);
			memName[m] = pr[DEFAULT_MEMS[m]].name;
		}
	}
	void loadPreset(int i) {
		const auto& pr = factoryPresets();
		int n = (int) pr.size();
		presetIdx = ((i % n) + n) % n;
		writePatch(pr[presetIdx].v.data());
		patchName = pr[presetIdx].name;
		currentMem = -1;
		showFlash(string::f("P%02d %s  [%s]", presetIdx + 1, pr[presetIdx].name.c_str(), pr[presetIdx].cat.c_str()));
	}
	void recall(int m) {
		writePatch(mem[m]);
		patchName = memName[m];
		currentMem = m;
		showFlash(string::f("MEMORY %d  %s", m + 1, memName[m].c_str()));
	}
	void store(int m) {
		readPatch(mem[m]);
		// keep what MUTATE has done
		for (int i = 0; i < NPV; i++) mem[m][i] = clampTo(i, mem[m][i] + mut[i]);
		memName[m] = patchName;
		currentMem = m;
		saveArmed = false;
		showFlash(string::f("SAVED TO MEMORY %d", m + 1));
	}
	void showFlash(const std::string& s) {
		flash = s;
		flashTime = 2.5f;
	}
	void randomizeNow() {
		float v[NPV];
		readPatch(v);
		randomizePatch(v, params[RANDAMT_PARAM].getValue(), scope, wild, rng);
		writePatch(v);
		if (patchName.empty() || patchName.back() != '~') patchName += " ~";
		randFlash = 1.f;
		showFlash(string::f("RAND %d%% %s", (int) std::round(params[RANDAMT_PARAM].getValue() * 100.f), SCOPE_NAMES[scope]));
	}
	void keepMutation() {
		for (int i = 0; i < NPV; i++) params[i].setValue(clampTo(i, params[i].getValue() + mut[i]));
		std::memset(mut, 0, sizeof(mut));
	}

	void onRandomize(const RandomizeEvent& e) override { randomizeNow(); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		restoreFactoryMemories();
		engine.allOff();
		loadPreset(0);
		saveArmed = false;
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setSampleRate(e.sampleRate); }

	// ------------------------------------------------ proxies
	const Page& curPage() { return pages()[page]; }
	int pageEntry(int c) {
		const Page& pg = curPage();
		if (pg.e.empty()) return -1;
		int e = pg.e[std::max(0, std::min((int) pg.e.size() - 1, c))];
		return pg.tone ? tp(selTone, e) : cp(e);
	}
	int proxyTarget(int px) {
		static const int ENV[3][5] = {{T_PE_T1, T_PE_T2, T_PE_T3, T_PE_L1, T_PE_L0},
		                              {T_FE_T1, T_FE_T2, T_FE_T3, T_FE_SUS, T_FE_T4},
		                              {T_AE_T1, T_AE_T2, T_AE_T3, T_AE_SUS, T_AE_T4}};
		int L = lfoSel * LFO_STRIDE;
		switch (px) {
			case PX_WAVE: return tp(selTone, T_WAVE);
			case PX_COARSE: return tp(selTone, T_COARSE);
			case PX_FINE: return tp(selTone, T_FINE);
			case PX_CUT: return tp(selTone, T_F_CUT);
			case PX_RES: return tp(selTone, T_F_RES);
			case PX_FENV: return tp(selTone, T_F_ENV);
			case PX_LEVEL: return tp(selTone, T_LEVEL);
			case PX_VELO: return tp(selTone, T_VSENS);
			case PX_E1: case PX_E2: case PX_E3: case PX_E4: case PX_E5: return tp(selTone, ENV[envSel][px - PX_E1]);
			case PX_LRATE: return tp(selTone, T_L1_RATE + L);
			case PX_LPITCH: return tp(selTone, T_L1_PITCH + L);
			case PX_LTVF: return tp(selTone, T_L1_TVF + L);
			case PX_LTVA: return tp(selTone, T_L1_TVA + L);
			case PX_DATA: return pageEntry(cursor);
		}
		return -1;
	}
	float norm(int i, float x) {
		Def d = defOf(i);
		return (x - d.min) / (d.max - d.min);
	}
	float denorm(int i, float n) {
		Def d = defOf(i);
		return clampTo(i, d.min + n * (d.max - d.min));
	}
	// sets one patch parameter from the panel, on all four tones when LINK is on
	void setFromPanel(int i, float value, float normDelta) {
		params[i].setValue(clampTo(i, value));
		if (isTone(i) && params[LINK_PARAM].getValue() > 0.5f) {
			int p = i % NT;
			for (int t = 0; t < 4; t++) {
				int j = tp(t, p);
				if (j == i) continue;
				params[j].setValue(denorm(j, norm(j, params[j].getValue()) + normDelta));
			}
		}
		touched = i;
		touchedTime = 2.f;
	}
	void syncProxies() {
		for (int p = 0; p < NUM_PX; p++) {
			int t = proxyTarget(p);
			float pv = params[PX_PARAM + p].getValue();
			if (t < 0) continue;
			if (pxLast[p] >= 0.f && pv != pxLast[p]) {
				setFromPanel(t, denorm(t, pv), pv - pxLast[p]);
				pxLast[p] = pv;
			}
			else if (denorm(t, pv) != params[t].getValue() || pxLast[p] < 0.f) {
				float n = norm(t, params[t].getValue());
				params[PX_PARAM + p].setValue(n);
				pxLast[p] = n;
			}
		}
	}

	// ------------------------------------------------ user waves
	void loadUser(int slot, const std::string& path) {
		Wave w;
		std::string err;
		if (!readWav(path, w, err)) {
			WARN("Facet: %s: %s", path.c_str(), err.c_str());
			return;
		}
		w.kind = WK_USER;
		w.loop = userLoop[slot];
		w.name = system::getStem(path);
		int expected = 0;
		if (!pendingState[slot].compare_exchange_strong(expected, 1)) return;
		std::swap(pendingWave[slot], w);
		userPath[slot] = path;
		pendingState[slot] = 2;
	}
	void clearUser(int slot) {
		int expected = 0;
		if (!pendingState[slot].compare_exchange_strong(expected, 1)) return;
		pendingWave[slot] = Wave();
		pendingWave[slot].kind = WK_USER;
		userPath[slot] = "";
		pendingState[slot] = 2;
	}

	// ------------------------------------------------ process
	bool pressed(int id) { return btn[id - TSEL_PARAM].process(params[id].getValue()); }

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);
		for (int i = 0; i < NUM_USER; i++)
			if (pendingState[i] == 2) {
				std::swap(engine.user[i], pendingWave[i]);
				engine.user[i].loop = userLoop[i];
				pendingState[i] = 0;
			}

		if (ctl.process()) control(args.sampleTime * ctl.getDivision());

		// inputs
		int nch = std::max(inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels());
		in.channels = std::min(MAXV, nch);
		in.velConnected = inputs[VEL_INPUT].isConnected();
		for (int c = 0; c < in.channels; c++) {
			in.voct[c] = inputs[VOCT_INPUT].getPolyVoltage(c);
			in.gate[c] = inputs[GATE_INPUT].getPolyVoltage(c);
			in.vel[c] = inputs[VEL_INPUT].getPolyVoltage(c);
			in.press[c] = inputs[PRESS_INPUT].getPolyVoltage(c);
		}
		in.mod = clamp(inputs[MOD_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		in.bend = clamp(inputs[BEND_INPUT].getVoltage() * 0.2f, -1.f, 1.f);
		in.solo = params[SOLO_PARAM].getValue() > 0.5f ? selTone : -1;

		engine.process(in);
		float l = engine.outL * 5.f, r = engine.outR * 5.f;
		// soft ceiling at about +-10 V
		auto soft = [](float x) { return std::fabs(x) < 8.f ? x : (x > 0.f ? 1.f : -1.f) * (8.f + 2.f * std::tanh((std::fabs(x) - 8.f) * 0.5f)); };
		if (outputs[R_OUTPUT].isConnected()) {
			outputs[L_OUTPUT].setVoltage(soft(l));
			outputs[R_OUTPUT].setVoltage(soft(r));
		}
		else
			outputs[L_OUTPUT].setVoltage(soft(0.5f * (l + r)));

		if (lightDiv.process()) lightsUpdate(args.sampleTime * lightDiv.getDivision());
	}

	void control(float dt) {
		// ---- buttons
		for (int t = 0; t < 4; t++)
			if (pressed(TSEL_PARAM + t)) selTone = t;
		for (int k = 0; k < 3; k++)
			if (pressed(FMODE_PARAM + k)) setFromPanel(tp(selTone, T_F_MODE), (float) k, 0.f), linkSet(T_F_MODE, (float) k);
		for (int k = 0; k < 3; k++)
			if (pressed(ENVSEL_PARAM + k)) envSel = k;
		for (int k = 0; k < 2; k++)
			if (pressed(LFOSEL_PARAM + k)) lfoSel = k;
		int npe = (int) curPage().e.size();
		if (pressed(PREV_PARAM)) cursor = (cursor + npe - 1) % npe;
		if (pressed(NEXT_PARAM)) cursor = (cursor + 1) % npe;
		if (pressed(PAGE_PARAM)) {
			page = (page + 1) % (int) pages().size();
			cursor = 0;
		}
		if (pressed(PRESET_DN_PARAM)) loadPreset(presetIdx - 1);
		if (pressed(PRESET_UP_PARAM)) loadPreset(presetIdx + 1);
		if (pressed(SAVE_PARAM)) saveArmed = !saveArmed;
		for (int m = 0; m < 8; m++)
			if (pressed(MEM_PARAM + m)) {
				if (saveArmed) store(m);
				else recall(m);
			}
		if (pressed(RAND_PARAM)) randomizeNow();
		if (pressed(SCOPE_PARAM)) {
			scope = (scope + 1) % NUM_SCOPES;
			showFlash(string::f("SEL: RAND + MUTATE touch %s", SCOPE_NAMES[scope]));
		}
		for (int k = 0; k < 2; k++)
			if (pressed(LEARN1_PARAM + k)) {
				learnArmed[k] = !learnArmed[k];
				learnArmed[1 - k] = false;
				readPatch(learnSnap);
			}

		// ---- panel proxies (after buttons so a new selection is followed at once)
		syncProxies();

		// ---- CV learn: the first patch parameter that moves becomes the target
		if (learnArmed[0] || learnArmed[1]) {
			int k = learnArmed[0] ? 0 : 1;
			for (int i = 0; i < NPV; i++)
				if (params[i].getValue() != learnSnap[i]) {
					cvTarget[k] = i;
					learnArmed[k] = false;
					// put the knob back where it was: the CV now drives it
					showFlash(string::f("CV %d -> %s", k + 1, paramScreenName(i).c_str()));
					break;
				}
		}

		// ---- MUTATE on each new note
		bool lock = params[LOCK_PARAM].getValue() > 0.5f || inputs[LOCK_INPUT].getVoltage() > 1.f;
		if (engine.noteOns != lastNoteOns) {
			float mu = params[MUTATE_PARAM].getValue();
			float base[NPV];
			readPatch(base);
			if (mu > 0.f && !lock) mutateStep(base, mut, mu * mu, scope, rng);
			lastNoteOns = engine.noteOns;
		}

		// ---- effective patch for the engine: knobs + mutation + learned CVs
		for (int i = 0; i < NPV; i++) {
			float x = params[i].getValue() + mut[i];
			engine.v[i] = x;
		}
		for (int k = 0; k < 2; k++) {
			int t = cvTarget[k];
			if (t < 0 || !inputs[CV1_INPUT + k].isConnected()) continue;
			Def d = defOf(t);
			engine.v[t] += inputs[CV1_INPUT + k].getVoltage() * 0.1f * params[CV1AMT_PARAM + k].getValue() * (d.max - d.min);
		}
		for (int i = 0; i < NPV; i++)
			if (mut[i] != 0.f || i == cvTarget[0] || i == cvTarget[1]) engine.v[i] = clampTo(i, engine.v[i]);

		touchedTime -= dt;
		flashTime -= dt;
	}

	void linkSet(int p, float value) {
		if (params[LINK_PARAM].getValue() < 0.5f) return;
		for (int t = 0; t < 4; t++) params[tp(t, p)].setValue(value);
	}

	void lightsUpdate(float dt) {
		blink += dt * 3.f;
		if (blink >= 1.f) blink -= 1.f;
		randFlash = std::max(0.f, randFlash - dt * 4.f);
		for (int t = 0; t < 4; t++) {
			lights[TSEL_LIGHT + t].setBrightness(t == selTone ? 1.f : 0.f);
			lights[TON_LIGHT + t].setBrightness(params[tp(t, T_ON)].getValue() > 0.5f ? 1.f : 0.f);
		}
		int fmode = (int) params[tp(selTone, T_F_MODE)].getValue();
		for (int k = 0; k < 3; k++) lights[FMODE_LIGHT + k].setBrightness(k == fmode ? 1.f : 0.f);
		for (int k = 0; k < 3; k++) lights[ENVSEL_LIGHT + k].setBrightness(k == envSel ? 1.f : 0.f);
		for (int k = 0; k < 2; k++) lights[LFOSEL_LIGHT + k].setBrightness(k == lfoSel ? 1.f : 0.f);
		for (int m = 0; m < 8; m++) {
			float b = m == currentMem ? 1.f : 0.f;
			if (saveArmed) b = blink < 0.5f ? 0.6f : 0.1f;
			lights[MEM_LIGHT + m].setBrightness(b);
		}
		lights[SAVE_LIGHT].setBrightness(saveArmed ? (blink < 0.5f ? 1.f : 0.2f) : 0.f);
		lights[RAND_LIGHT].setBrightness(randFlash);
		for (int k = 0; k < 2; k++) {
			float b = learnArmed[k] ? (blink < 0.5f ? 1.f : 0.15f) : (cvTarget[k] >= 0 ? 0.3f : 0.f);
			lights[LEARN1_LIGHT + k].setBrightness(b);
		}
		lights[LINK_LIGHT].setBrightness(params[LINK_PARAM].getValue());
		lights[SOLO_LIGHT].setBrightness(params[SOLO_PARAM].getValue());
		bool lock = params[LOCK_PARAM].getValue() > 0.5f || inputs[LOCK_INPUT].getVoltage() > 1.f;
		lights[LOCK_LIGHT].setBrightness(lock ? 1.f : 0.f);
		for (int k = 0; k < 7; k++) lights[FX_LIGHT + k].setBrightness(params[cp(FX_ON[k])].getValue() > 0.5f ? 1.f : 0.f);
		lights[DAC_LIGHT].setBrightness(params[cp(C_DAC90)].getValue() > 0.5f ? 1.f : 0.f);
	}

	std::string paramScreenName(int i) {
		Def d = defOf(i);
		if (isTone(i)) return string::f("%c %s", 'A' + i / NT, d.shortName);
		return d.shortName;
	}
	std::string valueString(int i, bool withMutation = false) {
		ParamQuantity* q = paramQuantities[i];
		float keep = params[i].getValue();
		std::string s = q->getDisplayValueString();
		std::string u = q->getUnit();
		(void) keep;
		(void) withMutation;
		return s + u;
	}

	// ------------------------------------------------ JSON
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* mems = json_array();
		for (int m = 0; m < 8; m++) {
			json_t* o = json_object();
			json_object_set_new(o, "name", json_string(memName[m].c_str()));
			json_t* vals = json_array();
			for (int i = 0; i < NPV; i++) json_array_append_new(vals, json_real(mem[m][i]));
			json_object_set_new(o, "values", vals);
			json_array_append_new(mems, o);
		}
		json_object_set_new(root, "memories", mems);
		json_object_set_new(root, "currentMem", json_integer(currentMem));
		json_object_set_new(root, "presetIdx", json_integer(presetIdx));
		json_object_set_new(root, "patchName", json_string(patchName.c_str()));
		json_object_set_new(root, "selTone", json_integer(selTone));
		json_object_set_new(root, "envSel", json_integer(envSel));
		json_object_set_new(root, "lfoSel", json_integer(lfoSel));
		json_object_set_new(root, "page", json_integer(page));
		json_object_set_new(root, "cursor", json_integer(cursor));
		json_object_set_new(root, "scope", json_integer(scope));
		json_object_set_new(root, "wild", json_boolean(wild));
		json_object_set_new(root, "cvTarget1", json_integer(cvTarget[0]));
		json_object_set_new(root, "cvTarget2", json_integer(cvTarget[1]));
		json_t* users = json_array();
		for (int i = 0; i < NUM_USER; i++) {
			json_t* o = json_object();
			json_object_set_new(o, "path", json_string(userPath[i].c_str()));
			json_object_set_new(o, "loop", json_boolean(userLoop[i]));
			json_array_append_new(users, o);
		}
		json_object_set_new(root, "userWaves", users);
		json_t* mu = json_array();
		for (int i = 0; i < NPV; i++) json_array_append_new(mu, json_real(mut[i]));
		json_object_set_new(root, "mutation", mu);
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "memories")))
			for (int m = 0; m < 8 && m < (int) json_array_size(j); m++) {
				json_t* o = json_array_get(j, m);
				json_t* n = json_object_get(o, "name");
				if (n) memName[m] = json_string_value(n);
				json_t* vals = json_object_get(o, "values");
				for (int i = 0; vals && i < NPV && i < (int) json_array_size(vals); i++) mem[m][i] = json_number_value(json_array_get(vals, i));
			}
		auto geti = [&](const char* k, int& dst, int lo, int hi) {
			json_t* x = json_object_get(root, k);
			if (x) dst = clamp((int) json_integer_value(x), lo, hi);
		};
		geti("currentMem", currentMem, -1, 7);
		geti("presetIdx", presetIdx, 0, (int) factoryPresets().size() - 1);
		geti("selTone", selTone, 0, 3);
		geti("envSel", envSel, 0, 2);
		geti("lfoSel", lfoSel, 0, 1);
		geti("page", page, 0, (int) pages().size() - 1);
		geti("cursor", cursor, 0, (int) pages()[page].e.size() - 1);
		geti("scope", scope, 0, NUM_SCOPES - 1);
		geti("cvTarget1", cvTarget[0], -1, NPV - 1);
		geti("cvTarget2", cvTarget[1], -1, NPV - 1);
		if ((j = json_object_get(root, "patchName"))) patchName = json_string_value(j);
		if ((j = json_object_get(root, "wild"))) wild = json_boolean_value(j);
		if ((j = json_object_get(root, "userWaves")))
			for (int i = 0; i < NUM_USER && i < (int) json_array_size(j); i++) {
				json_t* o = json_array_get(j, i);
				json_t* lp = json_object_get(o, "loop");
				if (lp) userLoop[i] = json_boolean_value(lp);
				json_t* p = json_object_get(o, "path");
				std::string path = p ? json_string_value(p) : "";
				if (!path.empty()) loadUser(i, path);
			}
		if ((j = json_object_get(root, "mutation")))
			for (int i = 0; i < NPV && i < (int) json_array_size(j); i++) mut[i] = json_number_value(json_array_get(j, i));
		for (int p = 0; p < NUM_PX; p++) pxLast[p] = -1.f;
	}
};

// ---- ProxyQuantity (needs Facet defined)
Facet* ProxyQuantity::fm() { return dynamic_cast<Facet*>(module); }
int ProxyQuantity::target() {
	Facet* f = fm();
	return f ? f->proxyTarget(px) : -1;
}
std::string ProxyQuantity::getLabel() {
	Facet* f = fm();
	int t = target();
	if (!f || t < 0) return ParamQuantity::getLabel();
	return f->paramQuantities[t]->getLabel();
}
void ProxyQuantity::setDisplayValueString(std::string s) {
	ParamQuantity* q = tq();
	Facet* f = fm();
	if (!q || !f) return;
	int t = target();
	float before = f->norm(t, q->getValue());
	q->setDisplayValueString(s);
	f->setFromPanel(t, q->getValue(), f->norm(t, q->getValue()) - before);
}
void ProxyQuantity::reset() {
	Facet* f = fm();
	int t = target();
	if (!f || t < 0) return;
	float before = f->norm(t, f->params[t].getValue());
	float d = paramDefault(t);
	f->setFromPanel(t, d, f->norm(t, d) - before);
	float n = f->norm(t, d);
	setValue(n);
	f->pxLast[px] = n;
}

// ---- the screen
struct FacetScreen : TransparentWidget {
	Facet* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x17, 0x15, 0x0f));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x55, 0x50, 0x3f));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}

	void text(const DrawArgs& args, float x, float y, const std::string& s, NVGcolor c, float maxW, float size = 10.f) {
		nvgFontSize(args.vg, size);
		float b[4];
		nvgTextBounds(args.vg, 0, 0, s.c_str(), nullptr, b);
		float w = b[2] - b[0];
		if (w > maxW) nvgFontSize(args.vg, size * maxW / w);
		nvgFillColor(args.vg, c);
		nvgText(args.vg, x, y, s.c_str(), nullptr);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		NVGcolor amber = nvgRGB(0xf3, 0xb0, 0x4a), ink = nvgRGB(0xec, 0xe6, 0xd8), soft = nvgRGB(0x9a, 0x96, 0x8c), dark = nvgRGB(0x17, 0x15, 0x0f);
		float W = box.size.x, x0 = 6.f, lh = 12.3f;
		if (!module) {
			text(args, x0, 4, "TONE A  WG                       1/18", amber, W - 12);
			text(args, x0, 4 + lh, "WAVE   Glass Pad      COARSE   0", ink, W - 12);
			text(args, x0, 4 + 2 * lh, "FINE   0              P RANDOM 0", ink, W - 12);
			text(args, x0, 4 + 5 * lh, "P01 Glass Cathedral | SEL ALL", soft, W - 12);
			return;
		}
		Facet* m = module;
		const Page& pg = m->curPage();
		int np = (int) pages().size();
		std::string title = pg.tone ? string::f("TONE %c  %s", 'A' + m->selTone, pg.title) : std::string(pg.title);
		text(args, x0, 4, title, amber, W * 0.7f);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		text(args, W - x0, 4, string::f("%d/%d", m->page + 1, np), soft, W * 0.2f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		// entries: two columns, four rows, scrolling by row
		int n = (int) pg.e.size();
		int firstRow = std::max(0, m->cursor / 2 - 3);
		float colW = (W - 2 * x0) / 2.f;
		for (int r = 0; r < 4; r++)
			for (int c = 0; c < 2; c++) {
				int k = (firstRow + r) * 2 + c;
				if (k >= n) continue;
				int id = m->pageEntry(k);
				float x = x0 + c * colW, y = 4 + (r + 1) * lh;
				bool sel = k == m->cursor;
				if (sel) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x - 2, y - 1, colW - 4, lh - 1);
					nvgFillColor(args.vg, amber);
					nvgFill(args.vg);
				}
				Def d = defOf(id);
				std::string val = m->valueString(id);
				if (m->mut[id] != 0.f) val += "~";
				if (id == m->cvTarget[0] || id == m->cvTarget[1]) val += "*";
				text(args, x, y, d.shortName, sel ? dark : soft, colW * 0.5f, 9.5f);
				text(args, x + colW * 0.5f, y, val, sel ? dark : ink, colW * 0.48f, 9.5f);
			}
		// line 6: what you just touched, a flash message, or the CV learn prompt
		std::string l6;
		NVGcolor c6 = ink;
		if (m->learnArmed[0] || m->learnArmed[1]) {
			l6 = string::f("LEARN CV %d: move any control", m->learnArmed[0] ? 1 : 2);
			c6 = amber;
		}
		else if (m->saveArmed) {
			l6 = "SAVE: press a memory 1-8";
			c6 = amber;
		}
		else if (m->flashTime > 0.f) {
			l6 = m->flash;
			c6 = amber;
		}
		else if (m->touchedTime > 0.f && m->touched >= 0)
			l6 = "> " + m->paramScreenName(m->touched) + "  " + m->valueString(m->touched);
		text(args, x0, 4 + 5 * lh, l6, c6, W - 2 * x0);
		// line 7: status
		std::string st = m->currentMem >= 0 ? string::f("M%d ", m->currentMem + 1) : string::f("P%02d ", m->presetIdx + 1);
		st += m->patchName;
		st += string::f(" | SEL %s", SCOPE_NAMES[m->scope]);
		float mu = m->params[Facet::MUTATE_PARAM].getValue();
		if (mu > 0.005f) st += string::f(" | MUT %d", (int) std::round(mu * 100.f));
		if (m->params[Facet::LOCK_PARAM].getValue() > 0.5f || m->inputs[Facet::LOCK_INPUT].getVoltage() > 1.f) st += " LOCK";
		if (m->params[Facet::LINK_PARAM].getValue() > 0.5f) st += " | LINK";
		if (m->wild) st += " | WILD";
		text(args, x0, 4 + 6 * lh, st, soft, W - 2 * x0);
	}
};

// small tone-coloured light for the tone select buttons
template <int R, int G, int B>
struct ToneLight : GrayModuleLightWidget {
	ToneLight() { addBaseColor(nvgRGB(R, G, B)); }
};

struct FacetWidget : ModuleWidget {
	FacetWidget(Facet* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Facet.svg")));
		typedef Facet F;

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FacetScreen* screen = createWidget<FacetScreen>(mm2px(Vec(SCR_X, SCR_Y)));
		screen->box.size = mm2px(Vec(SCR_W, SCR_H));
		screen->module = module;
		addChild(screen);

		// tones
		addParam(createLightParamCentered<VCVLightBezel<ToneLight<0xe0, 0x53, 0x3d>>>(mm2px(Vec(TSEL_X[0], TSEL_Y)), module, F::TSEL_PARAM + 0, F::TSEL_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezel<ToneLight<0xf3, 0xb0, 0x4a>>>(mm2px(Vec(TSEL_X[1], TSEL_Y)), module, F::TSEL_PARAM + 1, F::TSEL_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezel<ToneLight<0x4f, 0xb3, 0xa9>>>(mm2px(Vec(TSEL_X[2], TSEL_Y)), module, F::TSEL_PARAM + 2, F::TSEL_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezel<ToneLight<0x7b, 0x8c, 0xde>>>(mm2px(Vec(TSEL_X[3], TSEL_Y)), module, F::TSEL_PARAM + 3, F::TSEL_LIGHT + 3));
		for (int t = 0; t < 4; t++)
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(TSEL_X[t], TON_Y)), module, tp(t, T_ON), F::TON_LIGHT + t));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(LINK_X, LINKSOLO_Y)), module, F::LINK_PARAM, F::LINK_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(SOLO_X, LINKSOLO_Y)), module, F::SOLO_PARAM, F::SOLO_LIGHT));

		// screen navigation
		addParam(createParamCentered<VCVButton>(mm2px(Vec(PREV_X, NAV_Y)), module, F::PREV_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(NEXT_X, NAV_Y)), module, F::NEXT_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(PAGE_X, NAV_Y)), module, F::PAGE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(DATA_X, NAV_Y)), module, F::PX_PARAM + PX_DATA));

		// presets, memories, movement
		addParam(createParamCentered<VCVButton>(mm2px(Vec(PRESET_X[0], MEM_Y)), module, F::PRESET_DN_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(PRESET_X[1], MEM_Y)), module, F::PRESET_UP_PARAM));
		for (int m = 0; m < 8; m++)
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(MEM_X0 + m * MEM_DX, MEM_Y)), module, F::MEM_PARAM + m, F::MEM_LIGHT + m));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(SAVE_X, MEM_Y)), module, F::SAVE_PARAM, F::SAVE_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RAMT_X, MOV_Y)), module, F::RANDAMT_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(RAND_X, MOV_Y)), module, F::RAND_PARAM, F::RAND_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(MUT_X, MOV_Y)), module, F::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(LOCK_X, MOV_Y)), module, F::LOCK_PARAM, F::LOCK_LIGHT));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(SEL_X, MOV_Y)), module, F::SCOPE_PARAM));

		// WG / TVF / TVA / ENV / LFO (edit the selected tone)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(WAVE_X, WAVE_Y)), module, F::PX_PARAM + PX_WAVE));
		auto slider = [&](float x, int px) { addParam(createParamCentered<VCVSlider>(mm2px(Vec(x, SL_Y)), module, F::PX_PARAM + px)); };
		slider(COARSE_X, PX_COARSE);
		slider(FINE_X, PX_FINE);
		for (int k = 0; k < 3; k++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(FM_X, FM_Y[k])), module, F::FMODE_PARAM + k, F::FMODE_LIGHT + k));
		slider(CUT_X, PX_CUT);
		slider(RES_X, PX_RES);
		slider(FENV_X, PX_FENV);
		slider(LEV_X, PX_LEVEL);
		slider(VELO_X, PX_VELO);
		for (int k = 0; k < 3; k++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(ES_X, FM_Y[k])), module, F::ENVSEL_PARAM + k, F::ENVSEL_LIGHT + k));
		for (int k = 0; k < 5; k++) slider(ENV_X[k], PX_E1 + k);
		for (int k = 0; k < 2; k++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(LS_X[k], LS_Y)), module, F::LFOSEL_PARAM + k, F::LFOSEL_LIGHT + k));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(LRATE_X, LRATE_Y)), module, F::PX_PARAM + PX_LRATE));
		for (int k = 0; k < 3; k++) addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(LDEPTH_X, LDEPTH_Y[k])), module, F::PX_PARAM + PX_LPITCH + k));

		// effects: on/off latch + main knob
		for (int k = 0; k < 7; k++) {
			float x0 = FX_X0 + k * FX_DX;
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(x0 + FX_LATCH_DX, FX_Y)), module, cp(FX_ON[k]), F::FX_LIGHT + k));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x0 + FX_KNOB_DX, FX_Y)), module, cp(FX_KNOB[k])));
		}

		// jacks
		const int ins[9] = {F::VOCT_INPUT, F::GATE_INPUT, F::VEL_INPUT, F::PRESS_INPUT, F::MOD_INPUT, F::BEND_INPUT, F::CV1_INPUT, F::CV2_INPUT, F::LOCK_INPUT};
		for (int i = 0; i < 9; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + i * JACK_DX, JACK_Y)), module, ins[i]));
		for (int k = 0; k < 2; k++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(JACK_X0 + (6 + k) * JACK_DX, LEARN_Y)), module, F::LEARN1_PARAM + k, F::LEARN1_LIGHT + k));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(DAC_X, DAC_Y)), module, cp(C_DAC90), F::DAC_LIGHT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUTL_X, JACK_Y)), module, F::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUTR_X, JACK_Y)), module, F::R_OUTPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(OLEV_X, JACK_Y)), module, cp(C_LEVEL)));
	}

	static void addSlider(Menu* menu, ParamQuantity* q) {
		ui::Slider* s = new ui::Slider;
		s->quantity = q;
		s->box.size.x = 240.f;
		menu->addChild(s);
	}

	void appendContextMenu(Menu* menu) override {
		Facet* m = dynamic_cast<Facet*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		// factory presets, by group
		menu->addChild(createSubmenuItem("Factory presets", "", [=](Menu* sub) {
			const auto& pr = factoryPresets();
			std::vector<std::string> cats;
			for (const Preset& p : pr)
				if (std::find(cats.begin(), cats.end(), p.cat) == cats.end()) cats.push_back(p.cat);
			for (const std::string& c : cats)
				sub->addChild(createSubmenuItem(c, "", [=](Menu* sub2) {
					for (int i = 0; i < (int) pr.size(); i++)
						if (pr[i].cat == c)
							sub2->addChild(createMenuItem(string::f("%02d %s", i + 1, pr[i].name.c_str()), "", [=]() { m->loadPreset(i); }));
				}));
		}));
		menu->addChild(createMenuItem("Restore the 8 factory memories", "", [=]() { m->restoreFactoryMemories(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("RAND and MUTATE"));
		menu->addChild(createBoolPtrMenuItem("Allow wild pitch (lifts the in-tune limits)", "", &m->wild));
		menu->addChild(createSubmenuItem("SEL group", SCOPE_NAMES[m->scope], [=](Menu* sub) {
			for (int s = 0; s < NUM_SCOPES; s++)
				sub->addChild(createCheckMenuItem(SCOPE_NAMES[s], "", [=]() { return m->scope == s; }, [=]() { m->scope = s; }));
		}));
		menu->addChild(createMenuItem("Keep mutation (write it into the patch)", "", [=]() { m->keepMutation(); }));
		menu->addChild(createMenuItem("Clear mutation (back to the patch)", "", [=]() { std::memset(m->mut, 0, sizeof(m->mut)); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("CV 1 / CV 2 (press LEARN and move a control)"));
		for (int k = 0; k < 2; k++) {
			std::string cur = m->cvTarget[k] < 0 ? "nothing" : m->paramScreenName(m->cvTarget[k]);
			menu->addChild(createMenuItem(string::f("CV %d drives: %s (click to clear)", k + 1, cur.c_str()), "", [=]() { m->cvTarget[k] = -1; }));
			addSlider(menu, m->paramQuantities[Facet::CV1AMT_PARAM + k]);
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("User waves (the last 4 waves on the WAVE knob)"));
		for (int s = 0; s < NUM_USER; s++) {
			std::string cur = m->userPath[s].empty() ? "empty" : system::getFilename(m->userPath[s]);
			menu->addChild(createSubmenuItem(string::f("User %d", s + 1), cur, [=](Menu* sub) {
				sub->addChild(createMenuItem("Load WAV...", "", [=]() {
					osdialog_filters* filters = osdialog_filters_parse("WAV:wav,WAV");
					char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
					osdialog_filters_free(filters);
					if (path) {
						m->loadUser(s, path);
						std::free(path);
					}
				}));
				sub->addChild(createBoolMenuItem("Loop (off = one-shot)", "", [=]() { return m->userLoop[s]; }, [=](bool b) {
					m->userLoop[s] = b;
					m->engine.user[s].loop = b;
				}));
				sub->addChild(createMenuItem("Clear", "", [=]() { m->clearUser(s); }));
			}));
		}
		menu->addChild(new MenuSeparator);
		// every parameter, by tone and page
		menu->addChild(createSubmenuItem("All parameters", "", [=](Menu* sub) {
			for (int t = 0; t < 4; t++)
				sub->addChild(createSubmenuItem(string::f("Tone %c", 'A' + t), "", [=](Menu* s2) {
					for (const Page& pg : pages())
						if (pg.tone)
							s2->addChild(createSubmenuItem(pg.title, "", [=](Menu* s3) {
								for (int e : pg.e) addSlider(s3, m->paramQuantities[tp(t, e)]);
							}));
				}));
			for (const Page& pg : pages())
				if (!pg.tone)
					sub->addChild(createSubmenuItem(pg.title, "", [=](Menu* s3) {
						for (int e : pg.e) addSlider(s3, m->paramQuantities[cp(e)]);
					}));
		}));
	}
};

Model* modelFacet = createModel<Facet, FacetWidget>("Facet");
