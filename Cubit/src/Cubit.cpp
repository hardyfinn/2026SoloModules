// Cubit: 8-bit wavetable VCO, modulator and pattern source (spec vcv-modules/cubit.md).
// The DSP lives in CubitDSP.hpp (shared with test/render.cpp); this file is Rack glue,
// mutation, ROM loading and persistence.
#include "plugin.hpp"
#include "CubitDSP.hpp"
#include "CubitRom.hpp"
#include <osdialog.h>
#include <array>
#include <atomic>
#include <memory>

// ---- LAYOUT (mm, same numbers in res/make_panel.py) ----
static const float ROW1_Y = 27.0f;
static const float FREQ_X = 13.0f, FINE_X = 30.0f, RANGE_X = 43.0f, EXPO_X = 57.0f, LIN_X = 70.0f, DRIFT_X = 83.0f;
static const float MUTATE_X = 98.0f, MUTBTN_X = 112.0f, LOCK_X = 123.0f;
static const float DIG_Y = 48.0f;
static const float DIG_X[3] = {33.0f, 66.0f, 99.0f};
static const float DIG_W = 15.0f, DIG_H = 17.0f;
static const float TC_Y = 65.5f, TC_DX = 6.5f;
static const float RESET_X = 12.0f, MUTIN_X = 120.0f;
static const float BEND_Y = 84.0f;
static const float BEND_X[6] = {16.0f, 36.0f, 56.0f, 76.0f, 96.0f, 116.0f};
static const float IN_Y = 101.0f;
static const float OUT_Y = 115.0f;
static float inX(int i) { return 10.0f + 14.0f * i; }
static float outX(int i) { return 10.0f + 16.0f * i; }

static const int MAX_VOICES = 16;
using Rom = std::array<uint8_t, 32768>;

struct Cubit : Module {
	enum ParamId {
		FREQ_PARAM, FINE_PARAM, RANGE_PARAM, EXPO_PARAM, LIN_PARAM,
		A_PARAM, B_PARAM, C_PARAM,
		SPLICE_PARAM, WITH_PARAM, BIT_PARAM, STUCK_PARAM, RES_PARAM, DEPTH_PARAM,
		DRIFT_PARAM, MUTATE_PARAM, MUT_BTN_PARAM, LOCK_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT, EXPO_INPUT, LIN_INPUT, SYNC_INPUT, CLK_INPUT, VCA_INPUT,
		TRIG_A_INPUT, TRIG_B_INPUT, TRIG_C_INPUT, RESET_INPUT,
		CV_A_INPUT, CV_B_INPUT, CV_C_INPUT,
		SPLICE_INPUT, RES_INPUT, DEPTH_INPUT, MUT_INPUT,
		INPUTS_LEN
	};
	enum OutputId { OUT_OUTPUT, UNI_OUTPUT, SUB_OUTPUT, TAP1_OUTPUT, TAP2_OUTPUT, TAP3_OUTPUT, TAP4_OUTPUT, CLK_OUTPUT, OUTPUTS_LEN };
	enum LightId { TAP_LIGHT, LOCK_LIGHT = TAP_LIGHT + 4, MUT_LIGHT, LIGHTS_LEN };

	cubit::DecimFilter filt;
	cubit::Voice voices[MAX_VOICES];
	cubit::EdgeDetect syncEd[MAX_VOICES], clkEd[MAX_VOICES];
	float acLp[MAX_VOICES] = {};
	dsp::SchmittTrigger trigIn[3], resetIn, mutIn;
	cubit::Mutation mut;

	// ROM: the audio thread reads through rom; loads swap the pointer between samples
	std::atomic<const uint8_t*> rom{cubit::FACTORY_ROM};
	std::vector<std::unique_ptr<Rom>> romStore;   // custom ROMs stay alive until the module goes
	std::string romName;                          // empty = factory

	// menu
	int frameSize = 64;
	int normMode = cubit::NORM_FILE;
	bool vca10 = false;
	bool acCoupled = false;
	bool throughZero = false;

	// UI
	std::atomic<int> digit[3];
	float tapLight[4] = {};
	float mutHeld = 0.f;
	bool mutBtnPrev = false, mutCleared = false;

	Cubit() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FREQ_PARAM, -3.71f, 2.93f, 0.f, "Frequency", " Hz", 2.f, (float)cubit::C4_HZ);
		configParam(FINE_PARAM, -4.f, 4.f, 0.f, "Fine", " semitones");
		configSwitch(RANGE_PARAM, 0.f, 1.f, 1.f, "Range", {"LO (clock / 256: steps 5 to 500 per second)", "HI (audio)"});
		configParam(EXPO_PARAM, 0.f, 1.f, 0.f, "Expo FM", "%", 0.f, 100.f);
		configParam(LIN_PARAM, 0.f, 1.f, 0.f, "Lin FM", "%", 0.f, 100.f);
		const char* axis[3] = {"A (wave within the row)", "B (row)", "C (bank)"};
		for (int i = 0; i < 3; i++) {
			configParam(A_PARAM + i, 0.f, 7.f, 0.f, axis[i]);
			paramQuantities[A_PARAM + i]->snapEnabled = true;
		}
		configSwitch(SPLICE_PARAM, 0.f, 6.f, 0.f, "Splice", {"Off", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"});
		configSwitch(WITH_PARAM, 0.f, 8.f, 0.f, "Splice with", {"A1", "A2", "A4", "B1", "B2", "B4", "C1", "C2", "C4"});
		configSwitch(BIT_PARAM, 0.f, 7.f, 7.f, "Stuck bit", {"0", "1", "2", "3", "4", "5", "6", "7"});
		configSwitch(STUCK_PARAM, 0.f, 2.f, 1.f, "Stuck", {"LO (bit forced to 0)", "Off", "HI (bit forced to 1)"});
		configSwitch(RES_PARAM, 0.f, 4.f, 0.f, "Resolution", {"64 steps", "32 steps", "16 steps", "8 steps", "4 steps"});
		configSwitch(DEPTH_PARAM, 0.f, 7.f, 0.f, "Depth", {"8 bits", "7 bits", "6 bits", "5 bits", "4 bits", "3 bits", "2 bits", "1 bit"});
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.1f, "Drift", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.3f, "Mutate depth", "%", 0.f, 100.f);
		configButton(MUT_BTN_PARAM, "Mutate (hold 1 s to clear)");
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock mutation", {"Off", "On"});

		configInput(VOCT_INPUT, "V/Oct");
		configInput(EXPO_INPUT, "Expo FM");
		configInput(LIN_INPUT, "Lin FM");
		configInput(SYNC_INPUT, "Sync (rising edge resets the cycle)");
		configInput(CLK_INPUT, "Clock (each rising edge = one step; replaces the internal clock)");
		configInput(VCA_INPUT, "VCA");
		configInput(TRIG_A_INPUT, "Step A");
		configInput(TRIG_B_INPUT, "Step B");
		configInput(TRIG_C_INPUT, "Step C");
		configInput(RESET_INPUT, "Reset A to 0");
		configInput(CV_A_INPUT, "A CV (1.25 V per wave)");
		configInput(CV_B_INPUT, "B CV (1.25 V per row)");
		configInput(CV_C_INPUT, "C CV (1.25 V per bank)");
		configInput(SPLICE_INPUT, "Splice CV");
		configInput(RES_INPUT, "Resolution CV");
		configInput(DEPTH_INPUT, "Depth CV");
		configInput(MUT_INPUT, "Mutate trigger");

		configOutput(OUT_OUTPUT, "Bipolar (about +-7 V)");
		configOutput(UNI_OUTPUT, "Unipolar (byte / 48 V: 4 LSB = 1 semitone)");
		configOutput(SUB_OUTPUT, "Sub square (an octave down)");
		configOutput(TAP1_OUTPUT, "Tap 1 (bit 0)");
		configOutput(TAP2_OUTPUT, "Tap 2 (bit 3)");
		configOutput(TAP3_OUTPUT, "Tap 3 (bit 5)");
		configOutput(TAP4_OUTPUT, "Tap 4 (bit 7)");
		configOutput(CLK_OUTPUT, "Step clock (64 x the note)");

		for (int i = 0; i < 3; i++) digit[i] = 0;
		for (int c = 0; c < MAX_VOICES; c++) voices[c].setSeed(0xC0B17ull + 7919ull * c + random::u32());
		setRate(APP->engine->getSampleRate());
	}

	void setRate(float fs) {
		for (int c = 0; c < MAX_VOICES; c++) voices[c].setSampleRate(fs, &filt);
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { setRate(e.sampleRate); }

	void onReset() override {
		mut = cubit::Mutation();
		for (int c = 0; c < MAX_VOICES; c++) voices[c].resetVoice();
	}

	// ---- mutation (spec 9) --------------------------------------------------------

	int cBefore() {
		return ((int)std::round(params[C_PARAM].getValue()) + cubit::cubeIndex(inputs[CV_C_INPUT].getVoltage(0)) % 8 + 16) % 8;
	}
	void mutate() {
		if (params[LOCK_PARAM].getValue() > 0.5f) return;
		mut.draw(params[MUTATE_PARAM].getValue(), cBefore(), []() { return (double)random::uniform(); });
	}

	// ---- ROM -------------------------------------------------------------------------

	void setRom(std::unique_ptr<Rom> r, const std::string& name) {
		rom = r->data();
		romStore.push_back(std::move(r));
		romName = name;
	}
	void factoryRom() {
		rom = cubit::FACTORY_ROM;
		romName.clear();
	}
	bool loadRomFile(const std::string& path) {
		std::unique_ptr<Rom> r(new Rom);
		std::memcpy(r->data(), rom.load(), 32768);
		if (!cubit::loadRom(path, frameSize, normMode, r->data())) return false;
		setRom(std::move(r), system::getFilename(path));
		return true;
	}

	// ---- audio -----------------------------------------------------------------------

	void process(const ProcessArgs& args) override {
		// stepping triggers: like a button press, wrap 7 -> 0
		for (int i = 0; i < 3; i++)
			if (trigIn[i].process(inputs[TRIG_A_INPUT + i].getVoltage(), 0.1f, 1.f))
				params[A_PARAM + i].setValue((float)(((int)std::round(params[A_PARAM + i].getValue()) + 1) % 8));
		if (resetIn.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) params[A_PARAM].setValue(0.f);

		// mutation
		if (mutIn.process(inputs[MUT_INPUT].getVoltage(), 0.1f, 1.f)) mutate();
		bool mb = params[MUT_BTN_PARAM].getValue() > 0.5f;
		if (mb) {
			mutHeld += args.sampleTime;
			if (mutHeld >= 1.f && !mutCleared) { mut = cubit::Mutation(); mutCleared = true; }
		}
		else if (mutBtnPrev) {
			if (!mutCleared) mutate();
			mutHeld = 0.f;
			mutCleared = false;
		}
		mutBtnPrev = mb;

		// controls
		cubit::Controls k;
		k.freq = params[FREQ_PARAM].getValue();
		k.fine = params[FINE_PARAM].getValue();
		k.rangeLo = params[RANGE_PARAM].getValue() < 0.5f;
		k.expo = params[EXPO_PARAM].getValue();
		k.lin = params[LIN_PARAM].getValue();
		k.drift = params[DRIFT_PARAM].getValue();
		k.throughZero = throughZero;
		k.A = (int)std::round(params[A_PARAM].getValue());
		k.B = (int)std::round(params[B_PARAM].getValue());
		k.C = (int)std::round(params[C_PARAM].getValue());
		k.splice = (int)std::round(params[SPLICE_PARAM].getValue());
		k.with = (int)std::round(params[WITH_PARAM].getValue());
		k.bit = (int)std::round(params[BIT_PARAM].getValue());
		k.stuck = (int)std::round(params[STUCK_PARAM].getValue());
		k.res = (int)std::round(params[RES_PARAM].getValue());
		k.depth = (int)std::round(params[DEPTH_PARAM].getValue());
		k.spliceCv = inputs[SPLICE_INPUT].getVoltage(0);
		k.resCv = inputs[RES_INPUT].getVoltage(0);
		k.depthCv = inputs[DEPTH_INPUT].getVoltage(0);
		k.mut = mut;

		int n = 1;
		for (int id : {VOCT_INPUT, CV_A_INPUT, CV_B_INPUT, CV_C_INPUT, CLK_INPUT})
			n = std::max(n, inputs[id].getChannels());
		n = std::min(n, MAX_VOICES);

		const bool ext = inputs[CLK_INPUT].isConnected();
		const bool sync = inputs[SYNC_INPUT].isConnected();
		const bool vca = inputs[VCA_INPUT].isConnected();
		const float vcaRange = vca10 ? 10.f : 5.f;
		const bool dacOut = outputs[OUT_OUTPUT].isConnected() || outputs[UNI_OUTPUT].isConnected();
		unsigned mask = 0;
		if (dacOut) mask |= 1u << cubit::CH_DAC;
		if (outputs[SUB_OUTPUT].isConnected()) mask |= 1u << cubit::CH_SUB;
		for (int i = 0; i < 4; i++)
			if (outputs[TAP1_OUTPUT + i].isConnected()) mask |= 1u << (cubit::CH_T1 + i);
		if (outputs[CLK_OUTPUT].isConnected() && !ext) mask |= 1u << cubit::CH_CLK;

		const uint8_t* r = rom.load();
		const float acA = 1.f - std::exp(-2.f * (float)M_PI * 5.f * args.sampleTime);
		cubit::Path path0;
		for (int c = 0; c < n; c++) {
			cubit::VoiceCv cv;
			cv.voct = inputs[VOCT_INPUT].getPolyVoltage(c);
			cv.expoFm = inputs[EXPO_INPUT].getPolyVoltage(c);
			cv.linFm = inputs[LIN_INPUT].getPolyVoltage(c);
			cv.cvA = inputs[CV_A_INPUT].getPolyVoltage(c);
			cv.cvB = inputs[CV_B_INPUT].getPolyVoltage(c);
			cv.cvC = inputs[CV_C_INPUT].getPolyVoltage(c);
			cubit::Frame f;
			f.path = cubit::makePath(k, cv);
			f.rate = cubit::clockRate(k, cv);
			f.drift = k.drift;
			f.mask = mask;
			f.syncT = sync ? syncEd[c].process(inputs[SYNC_INPUT].getPolyVoltage(c)) : -1.0;
			f.ext = ext;
			if (ext) {
				f.extT = clkEd[c].process(inputs[CLK_INPUT].getPolyVoltage(c));
				f.extHigh = clkEd[c].high;
			}
			cubit::Result res = voices[c].process(r, f);
			float g = vca ? clamp(inputs[VCA_INPUT].getPolyVoltage(c) / vcaRange, 0.f, 1.f) : 1.f;
			if (dacOut) {
				float o = (float)cubit::outVolts(res.v[cubit::CH_DAC], f.path) * g;
				if (acCoupled) { acLp[c] += acA * (o - acLp[c]); o -= acLp[c]; }
				outputs[OUT_OUTPUT].setVoltage(o, c);
				outputs[UNI_OUTPUT].setVoltage((float)cubit::uniVolts(res.v[cubit::CH_DAC]) * g, c);
			}
			outputs[SUB_OUTPUT].setVoltage((float)res.v[cubit::CH_SUB], c);
			for (int i = 0; i < 4; i++) outputs[TAP1_OUTPUT + i].setVoltage((float)res.v[cubit::CH_T1 + i], c);
			outputs[CLK_OUTPUT].setVoltage((float)res.v[cubit::CH_CLK], c);
			if (c == 0) {
				path0 = f.path;
				int a, b, cc;
				cubit::effectiveDigits(k, cv, a, b, cc);
				digit[0] = a; digit[1] = b; digit[2] = cc;
			}
		}
		for (int o = 0; o < OUTPUTS_LEN; o++) outputs[o].setChannels(n);

		// lights: voice 1's current byte, 30 ms smoothing
		int byte = path0.byteAt(r, voices[0].step());
		float la = 1.f - std::exp(-args.sampleTime / 0.03f);
		for (int i = 0; i < 4; i++) {
			tapLight[i] += la * ((((byte >> cubit::TAP_BITS[i]) & 1) ? 1.f : 0.f) - tapLight[i]);
			lights[TAP_LIGHT + i].setBrightness(tapLight[i]);
		}
		lights[LOCK_LIGHT].setBrightness(params[LOCK_PARAM].getValue() > 0.5f ? 1.f : 0.f);
		lights[MUT_LIGHT].setBrightness(mut.any() ? 1.f : 0.f);
	}

	// ---- persistence ----------------------------------------------------------------

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "frameSize", json_integer(frameSize));
		json_object_set_new(root, "normalise", json_integer(normMode));
		json_object_set_new(root, "vca10", json_boolean(vca10));
		json_object_set_new(root, "acCoupled", json_boolean(acCoupled));
		json_object_set_new(root, "throughZero", json_boolean(throughZero));
		json_t* m = json_object();
		json_object_set_new(m, "a", json_integer(mut.a));
		json_object_set_new(m, "b", json_integer(mut.b));
		json_object_set_new(m, "c", json_integer(mut.c));
		json_object_set_new(m, "splice", json_boolean(mut.splice));
		json_object_set_new(m, "splicePos", json_integer(mut.splicePos));
		json_object_set_new(m, "withPos", json_integer(mut.withPos));
		json_object_set_new(m, "stuck", json_boolean(mut.stuck));
		json_object_set_new(m, "stuckMode", json_integer(mut.stuckMode));
		json_object_set_new(m, "stuckBit", json_integer(mut.stuckBit));
		json_object_set_new(m, "res", json_integer(mut.res));
		json_object_set_new(m, "depth", json_integer(mut.depth));
		json_object_set_new(root, "mutation", m);
		const uint8_t* r = rom.load();
		if (r != cubit::FACTORY_ROM) {
			json_object_set_new(root, "rom", json_string(string::toBase64(r, 32768).c_str()));
			json_object_set_new(root, "romName", json_string(romName.c_str()));
		}
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "frameSize")) {
			int v = (int)json_integer_value(j);
			frameSize = (v == 256 || v == 2048) ? v : 64;
		}
		if (json_t* j = json_object_get(root, "normalise")) normMode = clamp((int)json_integer_value(j), 0, 2);
		if (json_t* j = json_object_get(root, "vca10")) vca10 = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "acCoupled")) acCoupled = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "throughZero")) throughZero = json_boolean_value(j);
		if (json_t* m = json_object_get(root, "mutation")) {
			cubit::Mutation x;
			auto gi = [&](const char* key, int def) { json_t* j = json_object_get(m, key); return j ? (int)json_integer_value(j) : def; };
			auto gb = [&](const char* key) { json_t* j = json_object_get(m, key); return j ? json_boolean_value(j) : false; };
			x.a = gi("a", 0); x.b = clamp(gi("b", 0), 0, 7); x.c = clamp(gi("c", 0), 0, 7);
			x.splice = gb("splice"); x.splicePos = clamp(gi("splicePos", 1), 1, 6); x.withPos = clamp(gi("withPos", 0), 0, 8);
			x.stuck = gb("stuck"); x.stuckMode = clamp(gi("stuckMode", 1), 1, 2); x.stuckBit = clamp(gi("stuckBit", 7), 0, 7);
			x.res = clamp(gi("res", 0), 0, 4); x.depth = clamp(gi("depth", 0), 0, 7);
			mut = x;
		}
		json_t* jr = json_object_get(root, "rom");
		if (jr && json_is_string(jr)) {
			std::vector<uint8_t> d = string::fromBase64(json_string_value(jr));
			if (d.size() == 32768) {
				std::unique_ptr<Rom> r(new Rom);
				std::memcpy(r->data(), d.data(), 32768);
				json_t* jn = json_object_get(root, "romName");
				setRom(std::move(r), jn ? json_string_value(jn) : "Custom ROM");
			}
		}
		else factoryRom();
	}
};

// ---- widgets ------------------------------------------------------------------------

// Big push button with a 7-segment digit. A click adds 1 (wraps 7 -> 0); right-click is the
// usual param menu, so the digit can be typed, initialised and randomised.
struct DigitButton : app::ParamWidget {
	Cubit* mod = nullptr;
	int axis = 0;
	bool pressed = false;

	DigitButton() { box.size = mm2px(Vec(DIG_W, DIG_H)); }

	void onButton(const ButtonEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
			if (e.action == GLFW_PRESS) {
				pressed = true;
				ParamQuantity* pq = getParamQuantity();
				if (pq && module) {
					float old = pq->getValue();
					float nv = (float)(((int)std::round(old) + 1) % 8);
					pq->setValue(nv);
					history::ParamChange* h = new history::ParamChange;
					h->name = "step " + pq->getLabel();
					h->moduleId = module->id;
					h->paramId = paramId;
					h->oldValue = old;
					h->newValue = nv;
					APP->history->push(h);
				}
			}
			else if (e.action == GLFW_RELEASE) pressed = false;
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onDragEnd(const DragEndEvent& e) override { pressed = false; ParamWidget::onDragEnd(e); }

	int shown() {
		if (mod) return mod->digit[axis].load();
		ParamQuantity* pq = getParamQuantity();
		return pq ? (int)std::round(pq->getValue()) & 7 : 0;
	}

	void draw(const DrawArgs& args) override {
		float w = box.size.x, h = box.size.y;
		float d = pressed ? 0.8f : 0.f;
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, w, h, 4.f);
		nvgFillColor(args.vg, nvgRGB(0x3A, 0x41, 0x52));
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 2.f + d, 2.f + d, w - 4.f, h - 4.f, 3.f);
		nvgFillColor(args.vg, pressed ? nvgRGB(0x0A, 0x0D, 0x12) : nvgRGB(0x0E, 0x12, 0x1A));
		nvgFill(args.vg);
		drawSegments(args, nvgRGBA(0x7B, 0xE3, 0xA2, 0x1C), 0x7F);   // unlit segments
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) drawSegments(args, nvgRGB(0x7B, 0xE3, 0xA2), segMask(shown()));
		ParamWidget::drawLayer(args, layer);
	}

	static int segMask(int n) {
		static const int m[8] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07};   // gfedcba
		return m[n & 7];
	}

	void drawSegments(const DrawArgs& args, NVGcolor col, int mask) {
		float w = box.size.x, h = box.size.y;
		float d = pressed ? 0.8f : 0.f;
		float x0 = w * 0.27f + d, x1 = w * 0.73f + d, y0 = h * 0.18f + d, y1 = h * 0.5f + d, y2 = h * 0.82f + d;
		float t = w * 0.085f, g = t * 0.25f;
		auto hseg = [&](float y) { nvgRoundedRect(args.vg, x0 + g, y - t / 2, x1 - x0 - 2 * g, t, t / 3); };
		auto vseg = [&](float x, float ya, float yb) { nvgRoundedRect(args.vg, x - t / 2, ya + g, t, yb - ya - 2 * g, t / 3); };
		nvgBeginPath(args.vg);
		if (mask & 0x01) hseg(y0);
		if (mask & 0x02) vseg(x1, y0, y1);
		if (mask & 0x04) vseg(x1, y1, y2);
		if (mask & 0x08) hseg(y2);
		if (mask & 0x10) vseg(x0, y1, y2);
		if (mask & 0x20) vseg(x0, y0, y1);
		if (mask & 0x40) hseg(y1);
		nvgFillColor(args.vg, col);
		nvgFill(args.vg);
	}
};

struct CubitWidget : ModuleWidget {
	CubitWidget(Cubit* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Cubit.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// row 1
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(FREQ_X, ROW1_Y)), module, Cubit::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(FINE_X, ROW1_Y)), module, Cubit::FINE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(RANGE_X, ROW1_Y + 1.0f)), module, Cubit::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(EXPO_X, ROW1_Y)), module, Cubit::EXPO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(LIN_X, ROW1_Y)), module, Cubit::LIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(DRIFT_X, ROW1_Y)), module, Cubit::DRIFT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(MUTATE_X, ROW1_Y)), module, Cubit::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenLight>>>(mm2px(Vec(MUTBTN_X, ROW1_Y)), module, Cubit::MUT_BTN_PARAM, Cubit::MUT_LIGHT));
		addParam(createParamCentered<CKSS>(mm2px(Vec(LOCK_X, ROW1_Y)), module, Cubit::LOCK_PARAM));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(LOCK_X + 4.2f, ROW1_Y - 3.0f)), module, Cubit::LOCK_LIGHT));

		// the cube
		for (int i = 0; i < 3; i++) {
			DigitButton* d = createParamCentered<DigitButton>(mm2px(Vec(DIG_X[i], DIG_Y)), module, Cubit::A_PARAM + i);
			d->mod = module;
			d->axis = i;
			addParam(d);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(DIG_X[i] - TC_DX, TC_Y)), module, Cubit::TRIG_A_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(DIG_X[i] + TC_DX, TC_Y)), module, Cubit::CV_A_INPUT + i));
		}
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RESET_X, TC_Y)), module, Cubit::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(MUTIN_X, TC_Y)), module, Cubit::MUT_INPUT));

		// bends
		const int bend[6] = {Cubit::SPLICE_PARAM, Cubit::WITH_PARAM, Cubit::BIT_PARAM, Cubit::STUCK_PARAM, Cubit::RES_PARAM, Cubit::DEPTH_PARAM};
		for (int i = 0; i < 6; i++) {
			if (bend[i] == Cubit::STUCK_PARAM)
				addParam(createParamCentered<CKSSThree>(mm2px(Vec(BEND_X[i], BEND_Y)), module, bend[i]));
			else
				addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(BEND_X[i], BEND_Y)), module, bend[i]));
		}

		// jacks
		const int ins[9] = {Cubit::VOCT_INPUT, Cubit::EXPO_INPUT, Cubit::LIN_INPUT, Cubit::SYNC_INPUT, Cubit::CLK_INPUT,
		                    Cubit::VCA_INPUT, Cubit::SPLICE_INPUT, Cubit::RES_INPUT, Cubit::DEPTH_INPUT};
		for (int i = 0; i < 9; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(inX(i), IN_Y)), module, ins[i]));
		for (int i = 0; i < 8; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outX(i), OUT_Y)), module, Cubit::OUT_OUTPUT + i));
		for (int i = 0; i < 4; i++)
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(outX(3 + i) + 6.3f, OUT_Y - 6.5f)), module, Cubit::TAP_LIGHT + i));
	}

	void appendContextMenu(Menu* menu) override {
		Cubit* m = getModule<Cubit>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(m->romName.empty() ? "ROM: factory" : "ROM: " + m->romName));
		menu->addChild(createMenuItem("Load ROM…", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("ROM or WAV:bin,rom,wav,BIN,ROM,WAV");
			char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
			osdialog_filters_free(filters);
			if (!path) return;
			std::string p = path;
			std::free(path);
			if (!m->loadRomFile(p))
				osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK,
					"Could not load that file. Cubit loads a .bin / .rom of exactly 32768 bytes, or a PCM or float WAV.");
		}));
		menu->addChild(createMenuItem("Export ROM…", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("ROM:bin");
			char* path = osdialog_file(OSDIALOG_SAVE, NULL, "cubit.bin", filters);
			osdialog_filters_free(filters);
			if (!path) return;
			std::string p = path;
			std::free(path);
			if (system::getExtension(p).empty()) p += ".bin";
			if (!cubit::saveRom(p, m->rom.load()))
				osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, "Could not write that file.");
		}));
		menu->addChild(createMenuItem("Factory ROM", m->romName.empty() ? CHECKMARK_STRING : "", [=]() { m->factoryRom(); }));
		menu->addChild(createIndexSubmenuItem("WAV frame size", {"64", "256", "2048"},
			[=]() { return (size_t)(m->frameSize == 2048 ? 2 : m->frameSize == 256 ? 1 : 0); },
			[=](size_t i) { m->frameSize = i == 2 ? 2048 : i == 1 ? 256 : 64; }));
		menu->addChild(createIndexSubmenuItem("Normalise on load", {"Off", "Whole file", "Each wave"},
			[=]() { return (size_t)m->normMode; },
			[=](size_t i) { m->normMode = (int)i; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("VCA CV range", {"5 V", "10 V"},
			[=]() { return (size_t)(m->vca10 ? 1 : 0); },
			[=](size_t i) { m->vca10 = i == 1; }));
		menu->addChild(createIndexSubmenuItem("±BI output", {"DC-coupled", "AC-coupled"},
			[=]() { return (size_t)(m->acCoupled ? 1 : 0); },
			[=](size_t i) { m->acCoupled = i == 1; }));
		menu->addChild(createBoolPtrMenuItem("Lin FM through zero", "", &m->throughZero));
	}
};

Model* modelCubit = createModel<Cubit, CubitWidget>("Cubit");
