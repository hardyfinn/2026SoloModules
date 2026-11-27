// Perun: one thunder drum voice in the Perkons style (all models our own).
// The sound engine is in PerunEngine.hpp; factory memories in PerunPresets.hpp.
#include "plugin.hpp"
#include "PerunEngine.hpp"
#include "PerunPresets.hpp"
#include <osdialog.h>
#include <atomic>
#include <fstream>

// ---- LAYOUT (same numbers in res/make_panel.py) ---- 26 HP
static const float COL[7] = {12.f, 29.8f, 47.6f, 65.4f, 83.2f, 101.f, 118.8f};
static const float ROW[4] = {31.f, 50.5f, 70.f, 89.5f};
static const float TRIM_DX = -2.5f, LEARN_DX = 5.0f, LEARN_DY = 4.0f;
static const float IO_TOP = 106.f, IO_BOT = 118.6f;
static const float MEMX[4] = {96.5f, 103.5f, 110.5f, 117.5f};
static const float MEMY[2] = {9.5f, 16.8f};
static const float STORE_X = 125.6f, STORE_Y = 12.8f;
static const float SCREEN_X = 27.5f, SCREEN_Y = 3.f, SCREEN_W = 62.5f, SCREEN_H = 16.5f;

// what CV A / CV B can be learned onto (the 24 memory values + LEVEL)
static const int NUM_TARGETS = 25;
static const char* const TARGET_NAMES[NUM_TARGETS] = {
	"ALGO A", "ALGO B", "MORPH", "MODE", "TUNE", "BEND", "SWEEP", "CLICK", "DECAY", "P1", "P2", "NOISE",
	"CUTOFF", "RES", "ENV", "VCF", "DRIVE", "TONE", "SEND", "TIME", "FDBK", "ACCENT", "WEAR", "MUTATE", "LEVEL"};

static const char* const ALGO_NAMES[perun::NUM_ALGOS] = {
	"Fold BD", "Wavetable BD", "Resonant BD", "Complex (FM)", "Res Snare", "Slap",
	"Tom", "Noise Hat", "Pulse Hat", "Karplus", "Sample", "Metal", "Particles", "Circuit 909", "Chaos"};

// Minimal WAV reader: PCM 8/16/24/32 bit or float, any channel count (mixed to mono), up to 20 s.
static bool readWav(const std::string& path, perun::Sample& out, std::string& err) {
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
	size_t dataPos = 0, dataLen = 0;
	size_t p = 12;
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
		p += 8 + len + (len & 1);
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
	size_t frames = dataLen / (bps * ch);
	frames = std::min(frames, (size_t) rate * 20);
	out.data.assign(frames, 0.f);
	out.sr = (float) rate;
	for (size_t i = 0; i < frames; i++) {
		float s = 0.f;
		for (int c = 0; c < ch; c++) {
			size_t q = dataPos + (i * ch + c) * bps;
			float v = 0.f;
			if (fmt == 3) {
				uint32_t w = u32(q);
				std::memcpy(&v, &w, 4);
			}
			else if (bits == 8)
				v = (b[q] - 128) / 128.f;
			else if (bits == 16)
				v = (int16_t) u16(q) / 32768.f;
			else if (bits == 24) {
				int32_t w = (int32_t) ((b[q] << 8) | (b[q + 1] << 16) | (b[q + 2] << 24)) >> 8;
				v = w / 8388608.f;
			}
			else if (bits == 32)
				v = (int32_t) u32(q) / 2147483648.f;
			s += v;
		}
		out.data[i] = s / ch;
	}
	return true;
}

struct Perun : Module {
	enum ParamIds {
		// the 24 values a memory holds, in PerunPresets.hpp order
		ALGOA_PARAM, ALGOB_PARAM, MORPH_PARAM, MODE_PARAM, TUNE_PARAM, BEND_PARAM, SWEEP_PARAM, CLICK_PARAM,
		DECAY_PARAM, P1_PARAM, P2_PARAM, NOISE_PARAM, CUTOFF_PARAM, RES_PARAM, FENV_PARAM, VCF_PARAM,
		DRIVE_PARAM, TONE_PARAM, SEND_PARAM, TIME_PARAM, FDBK_PARAM, ACCENT_PARAM, WEAR_PARAM, MUTATE_PARAM,
		LEVEL_PARAM, LOCK_PARAM, HIT_PARAM, STORE_PARAM,
		ENUMS(MEM_PARAM, 8),
		CVA_AMT_PARAM, CVB_AMT_PARAM, LEARNA_PARAM, LEARNB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT, VOCT_INPUT, ACC_INPUT, CHOKE_INPUT, PATCH_INPUT, CVA_INPUT, CVB_INPUT, LOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { ENV_OUTPUT, SEND_OUTPUT, OUT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { LOCK_LIGHT, HIT_LIGHT, STORE_LIGHT, ENUMS(MEM_LIGHT, 8), LEARNA_LIGHT, LEARNB_LIGHT, NUM_LIGHTS };

	perun::Engine engine;
	perun::Controls ctl;
	dsp::SchmittTrigger trigIn, chokeIn, hitButton, storeButton, memButton[8], learnButton[2];
	// CV A / CV B: learned target (-1 = none); LEARN arms, the next knob you move becomes the target
	int cvTarget[2] = {12, 2};
	bool learnArmed[2] = {false, false};
	float learnSnap[NUM_TARGETS];
	int learnCheck = 0;
	dsp::ClockDivider lightDivider;
	float mem[8][perun::NUM_MEM_VALUES];
	std::string memName[8];
	int currentMem = 0;
	int lastPatch = -1;
	bool storeArmed = false;
	float blink = 0.f;
	float hitFlash = 0.f;
	// samples (loaded on the UI thread, swapped in by process())
	std::string samplePath[3];
	// 0 = empty, 1 = UI thread writing, 2 = ready for the audio thread
	perun::Sample pending[3];
	std::atomic<int> pendingState[3];
	int hitDelay = 0; // fire 2 samples after the trigger edge so ACC and CVs sent alongside are in

	Perun() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		std::vector<std::string> algos(ALGO_NAMES, ALGO_NAMES + perun::NUM_ALGOS);
		configSwitch(ALGOA_PARAM, 0.f, 14.f, 0.f, "Algorithm A", algos);
		configSwitch(ALGOB_PARAM, 0.f, 14.f, 2.f, "Algorithm B", algos);
		configParam(MORPH_PARAM, 0.f, 1.f, 0.15f, "Morph (A <-> B)", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Mode (variant of each algorithm)", {"A", "B", "C"});
		configParam(TUNE_PARAM, 0.f, 1.f, 0.189f, "Tune", " Hz", 128.f, 20.f);
		configParam(BEND_PARAM, 0.f, 1.f, 0.45f, "Bend (pitch envelope amount, up to 4 octaves)", "%", 0.f, 100.f);
		configParam(SWEEP_PARAM, 0.f, 1.f, 0.3f, "Sweep (pitch envelope time)", " ms", 100.f, 4.f);
		configParam(CLICK_PARAM, 0.f, 1.f, 0.35f, "Click (attack transient)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.56f, "Decay (fully up = drone)", " s", 400.f, 0.012f);
		configParam(P1_PARAM, 0.f, 1.f, 0.35f, "Parameter 1 (depends on the algorithm, see the screen)", "%", 0.f, 100.f);
		configParam(P2_PARAM, 0.f, 1.f, 0.5f, "Parameter 2 (depends on the algorithm, see the screen)", "%", 0.f, 100.f);
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f);
		configParam(CUTOFF_PARAM, 0.f, 1.f, 1.f, "Cutoff (LP opens clockwise, HP opens anticlockwise)", " Hz", 1000.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.1f, "Resonance", "%", 0.f, 100.f);
		configParam(FENV_PARAM, -1.f, 1.f, 0.f, "Filter envelope (+-6 octaves)", "%", 0.f, 100.f);
		configSwitch(VCF_PARAM, 0.f, 2.f, 2.f, "Filter type", {"High-pass", "Band-pass", "Low-pass"});
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.35f, "Drive (dry/wet of the overdrive)", "%", 0.f, 100.f);
		configParam(TONE_PARAM, 0.f, 1.f, 0.7f, "Tone (after the drive)", "%", 0.f, 100.f);
		configParam(SEND_PARAM, 0.f, 1.f, 0.f, "FX send (built-in delay, or the SEND output when patched)", "%", 0.f, 100.f);
		configParam(TIME_PARAM, 0.f, 1.f, 0.45f, "Delay time", " ms", 50.f, 20.f);
		configParam(FDBK_PARAM, 0.f, 1.f, 0.3f, "Delay feedback", "%", 0.f, 100.f);
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "Accent amount (level, brightness, drive)", "%", 0.f, 100.f);
		configParam(WEAR_PARAM, 0.f, 1.f, 0.1f, "Wear (every hit slightly different, like a real drum)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (P1, P2, morph and mode wander from hit to hit)", "%", 0.f, 100.f)->randomizeEnabled = false;
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock (freeze what MUTATE has changed)", {"off", "on"});
		configButton(HIT_PARAM, "Hit");
		configButton(STORE_PARAM, "Store (then press a memory)");
		for (int i = 0; i < 8; i++) configButton(MEM_PARAM + i, string::f("Memory %d", i + 1));

		configParam(CVA_AMT_PARAM, -1.f, 1.f, 1.f, "CV A amount (attenuverter)", "%", 0.f, 100.f);
		configParam(CVB_AMT_PARAM, -1.f, 1.f, 1.f, "CV B amount (attenuverter)", "%", 0.f, 100.f);
		configButton(LEARNA_PARAM, "Learn CV A (press, then move the knob it should control)");
		configButton(LEARNB_PARAM, "Learn CV B (press, then move the knob it should control)");
		configInput(VOCT_INPUT, "V/OCT (tune)");
		configInput(CVA_INPUT, "CV A (+-5 V = full range of the learned knob)");
		configInput(CVB_INPUT, "CV B (+-5 V = full range of the learned knob)");
		configInput(LOCK_INPUT, "Lock gate (high = locked)");
		configInput(TRIG_INPUT, "Trigger");
		configInput(ACC_INPUT, "Accent (read at each trigger, 5 V = full accent)");
		configInput(CHOKE_INPUT, "Choke (silences the ringing hits)");
		configInput(PATCH_INPUT, "Patch CV (0-10 V picks memory 1-8)");
		configOutput(ENV_OUTPUT, "Envelope (0-10 V)");
		configOutput(SEND_OUTPUT, "FX send (patching it bypasses the built-in delay)");
		configOutput(OUT_OUTPUT, "Audio");

		for (int i = 0; i < 3; i++) pendingState[i] = 0;
		lightDivider.setDivision(256);
		restoreFactory();
	}

	void restoreFactory() {
		for (int m = 0; m < 8; m++) {
			const perun::Preset& p = perun::factoryPreset(m);
			for (int i = 0; i < perun::NUM_MEM_VALUES; i++) mem[m][i] = p.v[i];
			memName[m] = p.name;
		}
	}

	void recall(int m) {
		for (int i = 0; i < perun::NUM_MEM_VALUES; i++) params[i].setValue(mem[m][i]);
		currentMem = m;
		engine.clearMutation();
	}

	void store(int m) {
		for (int i = 0; i < perun::NUM_MEM_VALUES; i++) mem[m][i] = params[i].getValue();
		memName[m] = string::f("Stored %d", m + 1);
		currentMem = m;
		storeArmed = false;
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine.reset();
		restoreFactory();
		currentMem = 0;
		storeArmed = false;
	}

	void loadSample(int slot, const std::string& path) {
		perun::Sample s;
		std::string err;
		if (!readWav(path, s, err)) {
			WARN("Perun: %s: %s", path.c_str(), err.c_str());
			return;
		}
		int expected = 0;
		if (!pendingState[slot].compare_exchange_strong(expected, 1)) return; // a load is already waiting
		std::swap(pending[slot], s);
		samplePath[slot] = path;
		pendingState[slot] = 2;
	}

	void resetSample(int slot) {
		int expected = 0;
		if (!pendingState[slot].compare_exchange_strong(expected, 1)) return;
		perun::makeDefaultSample(slot, pending[slot]);
		samplePath[slot] = "";
		pendingState[slot] = 2;
	}

	float targetValue(int t) { return params[t == 24 ? (int) LEVEL_PARAM : t].getValue(); }

	void armLearn(int k) {
		learnArmed[k] = !learnArmed[k];
		learnArmed[1 - k] = false;
		for (int t = 0; t < NUM_TARGETS; t++) learnSnap[t] = targetValue(t);
	}

	// add a CV (+-5 V = full range) to one knob value, with each control's own scaling
	static void applyCv(float* v, float& level, int t, float cv) {
		if (t < 0) return;
		if (t == 24) level = clamp(level + cv * 0.1f, 0.f, 1.f);
		else if (t == ALGOA_PARAM || t == ALGOB_PARAM) v[t] = clamp(v[t] + std::round(cv * 1.4f), 0.f, 14.f);
		else if (t == MODE_PARAM || t == VCF_PARAM) v[t] = clamp(v[t] + std::round(cv * 0.3f), 0.f, 2.f);
		else if (t == TUNE_PARAM) v[t] = v[t] + cv / 7.f;
		else if (t == FENV_PARAM) v[t] = clamp(v[t] + cv * 0.2f, -1.f, 1.f);
		else v[t] = clamp(v[t] + cv * 0.1f, 0.f, 1.f);
	}

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);
		for (int i = 0; i < 3; i++)
			if (pendingState[i] == 2) {
				std::swap(engine.samples[i], pending[i]);
				pendingState[i] = 0;
			}

		// memories: STORE arms, a slot button then stores; otherwise slot buttons recall
		if (storeButton.process(params[STORE_PARAM].getValue())) storeArmed = !storeArmed;
		for (int m = 0; m < 8; m++)
			if (memButton[m].process(params[MEM_PARAM + m].getValue())) {
				if (storeArmed) store(m);
				else recall(m);
			}
		if (inputs[PATCH_INPUT].isConnected()) {
			int m = clamp((int) (inputs[PATCH_INPUT].getVoltage() * 0.8f), 0, 7);
			if (m != lastPatch) recall(m);
			lastPatch = m;
		}
		else
			lastPatch = -1;

		// LEARN: the first knob that moves becomes the target
		for (int k = 0; k < 2; k++)
			if (learnButton[k].process(params[LEARNA_PARAM + k].getValue())) armLearn(k);
		if ((learnArmed[0] || learnArmed[1]) && ++learnCheck >= 32) {
			learnCheck = 0;
			int k = learnArmed[0] ? 0 : 1;
			for (int t = 0; t < NUM_TARGETS; t++)
				if (std::fabs(targetValue(t) - learnSnap[t]) > 1e-3f) {
					cvTarget[k] = t;
					learnArmed[k] = false;
					break;
				}
		}

		float v[perun::NUM_MEM_VALUES];
		for (int i = 0; i < perun::NUM_MEM_VALUES; i++) v[i] = params[i].getValue();
		float level = params[LEVEL_PARAM].getValue();
		if (inputs[VOCT_INPUT].isConnected()) v[TUNE_PARAM] += inputs[VOCT_INPUT].getVoltage() / 7.f;
		for (int k = 0; k < 2; k++)
			if (inputs[CVA_INPUT + k].isConnected())
				applyCv(v, level, cvTarget[k], inputs[CVA_INPUT + k].getVoltage() * params[CVA_AMT_PARAM + k].getValue());
		perun::valuesToControls(v, ctl);
		ctl.level = level;
		ctl.lock = params[LOCK_PARAM].getValue() > 0.5f || inputs[LOCK_INPUT].getVoltage() > 1.f;
		ctl.sendPatched = outputs[SEND_OUTPUT].isConnected();

		if (chokeIn.process(inputs[CHOKE_INPUT].getVoltage(), 0.1f, 1.f)) engine.choke();
		bool trig = trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		bool button = hitButton.process(params[HIT_PARAM].getValue());
		if (trig) hitDelay = 2;
		if (button) {
			engine.hit(0.f, ctl);
			hitFlash = 1.f;
		}
		if (hitDelay > 0 && --hitDelay == 0) {
			engine.hit(clamp(inputs[ACC_INPUT].getVoltage() * 0.2f, 0.f, 1.f), ctl);
			hitFlash = 1.f;
		}

		engine.process(ctl);
		outputs[OUT_OUTPUT].setVoltage(engine.out);
		outputs[ENV_OUTPUT].setVoltage(engine.env * 10.f);
		outputs[SEND_OUTPUT].setVoltage(engine.sendOut);

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			blink += dt * 3.f;
			if (blink >= 1.f) blink -= 1.f;
			hitFlash = std::max(0.f, hitFlash - dt * 8.f);
			lights[HIT_LIGHT].setBrightness(hitFlash);
			lights[LOCK_LIGHT].setBrightness(ctl.lock ? 1.f : 0.f);
			lights[STORE_LIGHT].setBrightness(storeArmed ? (blink < 0.5f ? 1.f : 0.2f) : 0.f);
			for (int k = 0; k < 2; k++) lights[LEARNA_LIGHT + k].setBrightness(learnArmed[k] ? (blink < 0.5f ? 1.f : 0.15f) : 0.f);
			for (int m = 0; m < 8; m++) {
				float b = m == currentMem ? 1.f : 0.f;
				if (storeArmed) b = blink < 0.5f ? 0.5f : 0.1f;
				lights[MEM_LIGHT + m].setBrightness(b);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* mems = json_array();
		for (int m = 0; m < 8; m++) {
			json_t* o = json_object();
			json_object_set_new(o, "name", json_string(memName[m].c_str()));
			json_t* vals = json_array();
			for (int i = 0; i < perun::NUM_MEM_VALUES; i++) json_array_append_new(vals, json_real(mem[m][i]));
			json_object_set_new(o, "values", vals);
			json_array_append_new(mems, o);
		}
		json_object_set_new(root, "memories", mems);
		json_object_set_new(root, "currentMem", json_integer(currentMem));
		json_object_set_new(root, "cvTargetA", json_integer(cvTarget[0]));
		json_object_set_new(root, "cvTargetB", json_integer(cvTarget[1]));
		json_t* smp = json_array();
		for (int i = 0; i < 3; i++) json_array_append_new(smp, json_string(samplePath[i].c_str()));
		json_object_set_new(root, "samples", smp);
		json_object_set_new(root, "mutP1", json_real(engine.mutP1));
		json_object_set_new(root, "mutP2", json_real(engine.mutP2));
		json_object_set_new(root, "mutMorph", json_real(engine.mutMorph));
		json_object_set_new(root, "mutMode", json_integer(engine.mutMode));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* mems = json_object_get(root, "memories");
		if (mems)
			for (int m = 0; m < 8 && m < (int) json_array_size(mems); m++) {
				json_t* o = json_array_get(mems, m);
				json_t* n = json_object_get(o, "name");
				if (n) memName[m] = json_string_value(n);
				json_t* vals = json_object_get(o, "values");
				for (int i = 0; vals && i < perun::NUM_MEM_VALUES && i < (int) json_array_size(vals); i++)
					mem[m][i] = json_number_value(json_array_get(vals, i));
			}
		json_t* ta = json_object_get(root, "cvTargetA");
		if (ta) cvTarget[0] = clamp((int) json_integer_value(ta), -1, NUM_TARGETS - 1);
		json_t* tb = json_object_get(root, "cvTargetB");
		if (tb) cvTarget[1] = clamp((int) json_integer_value(tb), -1, NUM_TARGETS - 1);
		json_t* c = json_object_get(root, "currentMem");
		if (c) currentMem = clamp((int) json_integer_value(c), 0, 7);
		json_t* smp = json_object_get(root, "samples");
		for (int i = 0; smp && i < 3 && i < (int) json_array_size(smp); i++) {
			std::string p = json_string_value(json_array_get(smp, i));
			if (!p.empty()) loadSample(i, p);
		}
		json_t* j;
		if ((j = json_object_get(root, "mutP1"))) engine.mutP1 = json_number_value(j);
		if ((j = json_object_get(root, "mutP2"))) engine.mutP2 = json_number_value(j);
		if ((j = json_object_get(root, "mutMorph"))) engine.mutMorph = json_number_value(j);
		if ((j = json_object_get(root, "mutMode"))) engine.mutMode = clamp((int) json_integer_value(j), 0, 2);
	}
};

// ---- the screen ----
struct PerunScreen : TransparentWidget {
	Perun* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x0c, 0x1a, 0x12));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x33, 0x36, 0x3a));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		std::string l1, l2, l3;
		if (!module) {
			l1 = "A 01 FOLD BD > 15% > B 03 RESONANT BD";
			l2 = "MODE A sine | P1 fold | P2 symmetry";
			l3 = "MEM 1 Thunder Kick | A>CUTOFF";
		}
		else {
			Perun* m = module;
			perun::Controls& c = m->ctl;
			perun::Engine& e = m->engine;
			float morph = e.effMorph(c);
			int mode = e.effMode(c);
			const perun::AlgoInfo& a = perun::algoInfo(c.algoA);
			const perun::AlgoInfo& b = perun::algoInfo(c.algoB);
			const perun::AlgoInfo& cur = morph < 0.5f ? a : b;
			if (morph < 0.005f) l1 = string::f("A %02d %s", c.algoA + 1, a.name);
			else if (morph > 0.995f) l1 = string::f("B %02d %s", c.algoB + 1, b.name);
			else l1 = string::f("A %02d %s > %d%% > B %02d %s", c.algoA + 1, a.name, (int) std::round(morph * 100.f), c.algoB + 1, b.name);
			l2 = string::f("MODE %c %s | P1 %s %d | P2 %s %d", 'A' + mode, cur.mode[mode], cur.p1, (int) std::round(e.effP1(c) * 100.f),
			               cur.p2, (int) std::round(e.effP2(c) * 100.f));
			if (m->learnArmed[0] || m->learnArmed[1]) l3 = string::f("LEARN CV %c: move a knob", m->learnArmed[0] ? 'A' : 'B');
			else if (m->storeArmed) l3 = "STORE: press a memory 1-8";
			else {
				l3 = string::f("MEM %d %s", m->currentMem + 1, m->memName[m->currentMem].c_str());
				if (perun::isDrone(c.decay)) l3 += " | DRONE";
				if (c.wear > 0.005f) l3 += string::f(" | WEAR %d", (int) std::round(c.wear * 100.f));
				if (c.mutate > 0.005f) l3 += string::f(" | MUT %d", (int) std::round(c.mutate * 100.f));
				if (c.lock) l3 += " | LOCK";
				for (int k = 0; k < 2; k++)
					if (m->inputs[Perun::CVA_INPUT + k].isConnected() && m->cvTarget[k] >= 0)
						l3 += string::f(" | %c>%s", 'A' + k, TARGET_NAMES[m->cvTarget[k]]);
			}
		}
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0.f);
		float size = 10.5f;
		// shrink line 1 if it would overflow
		nvgFontSize(args.vg, size);
		nvgFillColor(args.vg, nvgRGB(0x66, 0xff, 0x77));
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		const std::string* lines[3] = {&l1, &l2, &l3};
		for (int i = 0; i < 3; i++) {
			float bounds[4];
			nvgFontSize(args.vg, size);
			nvgTextBounds(args.vg, 0, 0, lines[i]->c_str(), nullptr, bounds);
			float w = bounds[2] - bounds[0];
			float maxW = box.size.x - 10.f;
			nvgFontSize(args.vg, w > maxW ? size * maxW / w : size);
			nvgText(args.vg, 5.f, 6.f + i * 15.f, lines[i]->c_str(), nullptr);
		}
	}
};

struct PerunWidget : ModuleWidget {
	PerunWidget(Perun* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Perun.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PerunScreen* screen = createWidget<PerunScreen>(mm2px(Vec(SCREEN_X, SCREEN_Y)));
		screen->box.size = mm2px(Vec(SCREEN_W, SCREEN_H));
		screen->module = module;
		addChild(screen);

		typedef Perun P;
		const int rows[4][7] = {
			{P::ALGOA_PARAM, P::ALGOB_PARAM, P::MORPH_PARAM, P::MODE_PARAM, P::TUNE_PARAM, P::BEND_PARAM, P::SWEEP_PARAM},
			{P::DECAY_PARAM, P::P1_PARAM, P::P2_PARAM, P::CLICK_PARAM, P::NOISE_PARAM, P::WEAR_PARAM, P::MUTATE_PARAM},
			{P::CUTOFF_PARAM, P::RES_PARAM, P::FENV_PARAM, P::VCF_PARAM, P::DRIVE_PARAM, P::TONE_PARAM, P::LOCK_PARAM},
			{P::SEND_PARAM, P::TIME_PARAM, P::FDBK_PARAM, P::ACCENT_PARAM, P::LEVEL_PARAM, P::CVA_AMT_PARAM, P::CVB_AMT_PARAM}};
		for (int r = 0; r < 4; r++)
			for (int c = 0; c < 7; c++) {
				int id = rows[r][c];
				Vec pos = mm2px(Vec(COL[c], ROW[r]));
				if (id == P::MODE_PARAM || id == P::VCF_PARAM) addParam(createParamCentered<CKSSThree>(pos, module, id));
				else if (id == P::ALGOA_PARAM || id == P::ALGOB_PARAM) addParam(createParamCentered<RoundBlackSnapKnob>(pos, module, id));
				else if (id == P::LOCK_PARAM) addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(pos, module, P::LOCK_PARAM, P::LOCK_LIGHT));
				else if (id == P::CVA_AMT_PARAM || id == P::CVB_AMT_PARAM) addParam(createParamCentered<Trimpot>(mm2px(Vec(COL[c] + TRIM_DX, ROW[r])), module, id));
				else addParam(createParamCentered<RoundBlackKnob>(pos, module, id));
			}
		for (int k = 0; k < 2; k++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(COL[5 + k] + LEARN_DX, ROW[3] + LEARN_DY)), module, P::LEARNA_PARAM + k, P::LEARNA_LIGHT + k));
		// trigger + output block
		const int ins[7] = {P::TRIG_INPUT, P::VOCT_INPUT, P::ACC_INPUT, P::CHOKE_INPUT, P::PATCH_INPUT, P::CVA_INPUT, P::CVB_INPUT};
		for (int i = 0; i < 7; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], IO_TOP)), module, ins[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[2], IO_BOT)), module, P::LOCK_INPUT));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(COL[3], IO_BOT)), module, P::HIT_PARAM, P::HIT_LIGHT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[4], IO_BOT)), module, P::ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], IO_BOT)), module, P::SEND_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[6], IO_BOT)), module, P::OUT_OUTPUT));
		// memories
		for (int m = 0; m < 8; m++)
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(MEMX[m % 4], MEMY[m / 4])), module, P::MEM_PARAM + m, P::MEM_LIGHT + m));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(STORE_X, STORE_Y)), module, P::STORE_PARAM, P::STORE_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Perun* m = dynamic_cast<Perun*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Memories"));
		menu->addChild(createMenuItem("Restore the 8 factory memories", "", [=]() {
			m->restoreFactory();
			m->recall(m->currentMem);
		}));
		menu->addChild(createMenuItem("Clear mutation (back to the knobs)", "", [=]() { m->engine.clearMutation(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("CV inputs (or press LEARN and move a knob)"));
		for (int k = 0; k < 2; k++) {
			std::string cur = m->cvTarget[k] < 0 ? "none" : TARGET_NAMES[m->cvTarget[k]];
			menu->addChild(createSubmenuItem(string::f("CV %c controls", 'A' + k), cur, [=](Menu* sub) {
				sub->addChild(createCheckMenuItem("Nothing", "", [=]() { return m->cvTarget[k] < 0; }, [=]() { m->cvTarget[k] = -1; }));
				for (int t = 0; t < NUM_TARGETS; t++)
					sub->addChild(createCheckMenuItem(TARGET_NAMES[t], "", [=]() { return m->cvTarget[k] == t; }, [=]() { m->cvTarget[k] = t; }));
			}));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("SAMPLE algorithm (MODE picks the slot)"));
		for (int s = 0; s < 3; s++) {
			std::string cur = m->samplePath[s].empty() ? "built-in" : system::getFilename(m->samplePath[s]);
			menu->addChild(createMenuItem(string::f("Load WAV into slot %d...", s + 1), cur, [=]() {
				osdialog_filters* filters = osdialog_filters_parse("WAV:wav,WAV");
				char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
				osdialog_filters_free(filters);
				if (path) {
					m->loadSample(s, path);
					std::free(path);
				}
			}));
		}
		menu->addChild(createSubmenuItem("Back to built-in sound", "", [=](Menu* sub) {
			const char* names[3] = {"Slot 1: rim", "Slot 2: wood block", "Slot 3: shaker"};
			for (int s = 0; s < 3; s++) sub->addChild(createMenuItem(names[s], "", [=]() { m->resetSample(s); }));
		}));
	}
};

Model* modelPerun = createModel<Perun, PerunWidget>("Perun");
