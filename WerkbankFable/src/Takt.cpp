#include "plugin.hpp"
#include "dsp/Common.hpp"
#include "dsp/Takt.hpp"

using namespace wbui;
namespace L = layout::Takt;

static const char* NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const int RANGES[3] = {1, 2, 4};

static std::string noteName(int semis) {
	int o = (int)std::floor(semis / 12.0);
	return string::f("%s%d", NOTE_NAMES[((semis % 12) + 12) % 12], 4 + o);
}

struct Takt : Module {
	enum ParamIds {
		PATTERN_PARAM, GATE_PARAM, LAST_A_PARAM, LAST_B_PARAM, RUN_PARAM, RESET_PARAM, RANGE_PARAM, CHAIN_PARAM, WHEN_PARAM, KEYS_PARAM,
		ENUMS(KEY_PARAM, 12),
		ENUMS(SLIDER_PARAM, 32), ENUMS(MODE_PARAM, 32), ENUMS(ACC_PARAM, 32),
		NUM_PARAMS
	};
	enum InputIds { CLOCK_INPUT, RESET_INPUT, RUN_INPUT, TRANS_INPUT, GATECV_INPUT, PATCV_INPUT, NUM_INPUTS };
	enum OutputIds { A_VOCT_OUTPUT, A_GATE_OUTPUT, A_ACC_OUTPUT, B_VOCT_OUTPUT, B_GATE_OUTPUT, B_ACC_OUTPUT, END_OUTPUT, NUM_OUTPUTS };
	enum LightIds { RUN_LIGHT, ENUMS(KEY_LIGHT, 12), ENUMS(STEP_LIGHT, 32), ENUMS(ACC_LIGHT, 32), NUM_LIGHTS };

	wb::TaktCore core;
	wb::Schmitt clockTrig, resetTrig;
	dsp::BooleanTrigger resetBtn, keyBtn[12];
	uint16_t scale = 0xFFF;
	int keyTrans = 0;
	dsp::ClockDivider lightDiv;
	static wb::TaktPattern clipboard;
	static bool haveClipboard;

	struct StepQ : ParamQuantity {
		std::string getDisplayValueString() override {
			Takt* t = dynamic_cast<Takt*>(module);
			if (!t) return ParamQuantity::getDisplayValueString();
			int range = RANGES[clamp((int)std::round(t->params[RANGE_PARAM].getValue()), 0, 2)];
			int s = wb::taktSemis(getValue(), range, t->scale);
			return noteName(s + t->core.applied);
		}
	};
	struct PatternQ : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%d", (int)std::round(getValue()) + 1); }
	};

	Takt() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<PatternQ>(PATTERN_PARAM, 0.f, 7.f, 0.f, "Pattern")->description = "Eight patterns. A change takes effect when row A next plays step 1 (at once when stopped).";
		getParamQuantity(PATTERN_PARAM)->snapEnabled = true;
		configParam(GATE_PARAM, 0.05f, 1.f, 0.5f, "Gate length", "%", 0.f, 100.f);
		configParam(LAST_A_PARAM, 1.f, 16.f, 16.f, "Last step, row A")->snapEnabled = true;
		configParam(LAST_B_PARAM, 1.f, 16.f, 16.f, "Last step, row B (in SERIES: where the 32-step loop ends)")->snapEnabled = true;
		configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
		configButton(RESET_PARAM, "Reset");
		configSwitch(RANGE_PARAM, 0.f, 2.f, 1.f, "Slider range", {"1 octave", "2 octaves", "4 octaves"});
		configSwitch(CHAIN_PARAM, 0.f, 1.f, 0.f, "Chain", {"Parallel: two 16-step rows", "Series: one 32-step row"});
		configSwitch(WHEN_PARAM, 0.f, 1.f, 0.f, "Transpose takes effect", {"Now", "At the bar (row A step 1)"});
		configSwitch(KEYS_PARAM, 0.f, 1.f, 1.f, "Keys", {"Scale: keys choose the allowed notes", "Transpose: keys choose the transposition"});
		for (int n = 0; n < 12; n++) {
			configButton(KEY_PARAM + n, std::string("Key ") + NOTE_NAMES[n]);
			configLight(KEY_LIGHT + n, std::string("Key ") + NOTE_NAMES[n]);
		}
		for (int r = 0; r < 2; r++) {
			for (int i = 0; i < 16; i++) {
				std::string nm = string::f("%c%d", r == 0 ? 'A' : 'B', i + 1);
				int k = r * 16 + i;
				configParam<StepQ>(SLIDER_PARAM + k, 0.f, 1.f, 0.f, "Step " + nm + " pitch");
				configSwitch(MODE_PARAM + k, 0.f, 2.f, 2.f, "Step " + nm, {"Rest", "Tie", "Play"});
				configSwitch(ACC_PARAM + k, 0.f, 1.f, 0.f, "Step " + nm + " accent", {"Off", "On"});
				configLight(STEP_LIGHT + k, "Step " + nm);
			}
		}
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run gate (replaces the button when patched)");
		configInput(TRANS_INPUT, "Transpose (V/OCT, rounded to semitones, added to the keys)");
		configInput(GATECV_INPUT, "Gate length CV (5 V sweeps the knob range)");
		configInput(PATCV_INPUT, "Pattern CV (1 V per pattern, added to the knob)");
		configOutput(A_VOCT_OUTPUT, "Row A V/OCT");
		configOutput(A_GATE_OUTPUT, "Row A gate");
		configOutput(A_ACC_OUTPUT, "Row A accent");
		configOutput(B_VOCT_OUTPUT, "Row B V/OCT (row A's in SERIES)");
		configOutput(B_GATE_OUTPUT, "Row B gate");
		configOutput(B_ACC_OUTPUT, "Row B accent");
		configOutput(END_OUTPUT, "End of row A");
		lightDiv.setDivision(64);
		loadPanel();
	}

	// pattern -> panel
	void loadPanel() {
		const wb::TaktPattern& P = core.pat[core.cur];
		for (int r = 0; r < 2; r++) {
			for (int i = 0; i < 16; i++) {
				int k = r * 16 + i;
				params[SLIDER_PARAM + k].setValue(P.slider[r][i]);
				params[MODE_PARAM + k].setValue((float)(2 - P.mode[r][i]));
				params[ACC_PARAM + k].setValue(P.accent[r][i] ? 1.f : 0.f);
			}
		}
		params[LAST_A_PARAM].setValue((float)P.last[0]);
		params[LAST_B_PARAM].setValue((float)P.last[1]);
	}
	// panel -> pattern
	void storePanel() {
		wb::TaktPattern& P = core.pat[core.cur];
		for (int r = 0; r < 2; r++) {
			for (int i = 0; i < 16; i++) {
				int k = r * 16 + i;
				P.slider[r][i] = params[SLIDER_PARAM + k].getValue();
				P.mode[r][i] = 2 - clamp((int)std::round(params[MODE_PARAM + k].getValue()), 0, 2);
				P.accent[r][i] = params[ACC_PARAM + k].getValue() > 0.5f;
			}
		}
		P.last[0] = clamp((int)std::round(params[LAST_A_PARAM].getValue()), 1, 16);
		P.last[1] = clamp((int)std::round(params[LAST_B_PARAM].getValue()), 1, 16);
	}

	void onReset() override {
		core = wb::TaktCore();
		scale = 0xFFF; keyTrans = 0;
		loadPanel();
	}

	void process(const ProcessArgs& args) override {
		storePanel();
		wb::TaktSettings s;
		s.range = RANGES[clamp((int)std::round(params[RANGE_PARAM].getValue()), 0, 2)];
		s.series = params[CHAIN_PARAM].getValue() > 0.5f;
		s.atBar = params[WHEN_PARAM].getValue() > 0.5f;
		const bool transposeKeys = params[KEYS_PARAM].getValue() > 0.5f;
		for (int n = 0; n < 12; n++) {
			if (keyBtn[n].process(params[KEY_PARAM + n].getValue() > 0.f)) {
				if (transposeKeys) keyTrans = n;
				else {
					uint16_t m = scale ^ (uint16_t)(1 << n);
					if (m & 0xFFF) scale = m;     // at least one note stays on
				}
			}
		}
		s.scale = scale;
		s.keyTranspose = keyTrans;
		s.cvTranspose = inputs[TRANS_INPUT].isConnected() ? (int)std::lround(inputs[TRANS_INPUT].getVoltage() * 12.f) : 0;
		s.gate = wb::clampd(params[GATE_PARAM].getValue() + inputs[GATECV_INPUT].getVoltage() / 5.0 * 0.95, 0.05, 1.0);
		core.requested = clamp((int)std::round(params[PATTERN_PARAM].getValue() + inputs[PATCV_INPUT].getVoltage()), 0, 7);

		const bool running = inputs[RUN_INPUT].isConnected() ? inputs[RUN_INPUT].getVoltage() >= 1.f : params[RUN_PARAM].getValue() > 0.5f;
		bool clock = clockTrig.process(inputs[CLOCK_INPUT].getVoltage());
		bool rs = resetTrig.process(inputs[RESET_INPUT].getVoltage());
		if (resetBtn.process(params[RESET_PARAM].getValue() > 0.f)) rs = true;

		wb::TaktRowOut out[2];
		bool end = false;
		core.process(args.sampleTime, clock, rs, running, s, out, end);
		if (core.switched) loadPanel();

		const int ids[2][3] = {{A_VOCT_OUTPUT, A_GATE_OUTPUT, A_ACC_OUTPUT}, {B_VOCT_OUTPUT, B_GATE_OUTPUT, B_ACC_OUTPUT}};
		for (int r = 0; r < 2; r++) {
			outputs[ids[r][0]].setVoltage(out[r].voct);
			outputs[ids[r][1]].setVoltage(out[r].gate ? 10.f : 0.f);
			outputs[ids[r][2]].setVoltage(out[r].accent ? 10.f : 0.f);
		}
		outputs[END_OUTPUT].setVoltage(end ? 10.f : 0.f);

		if (lightDiv.process()) {
			lights[RUN_LIGHT].setBrightness(running ? 1.f : 0.f);
			for (int n = 0; n < 12; n++)
				lights[KEY_LIGHT + n].setBrightness(transposeKeys ? (n == keyTrans ? 1.f : 0.f) : ((scale >> n) & 1 ? 1.f : 0.f));
			for (int k = 0; k < 32; k++) {
				lights[STEP_LIGHT + k].setBrightness(0.f);
				lights[ACC_LIGHT + k].setBrightness(params[ACC_PARAM + k].getValue() > 0.5f ? 1.f : 0.f);
			}
			if (s.series) {
				if (core.spos >= 0) lights[STEP_LIGHT + core.spos].setBrightness(out[0].gate ? 1.f : 0.4f);
			} else {
				for (int r = 0; r < 2; r++)
					if (core.pos[r] >= 0) lights[STEP_LIGHT + r * 16 + core.pos[r]].setBrightness(out[r].gate ? 1.f : 0.4f);
			}
		}
	}

	// context menu actions (UI thread): edit through the panel, which is copied into the pattern every sample
	void clearPattern() {
		for (int k = 0; k < 32; k++) { params[SLIDER_PARAM + k].setValue(0.f); params[MODE_PARAM + k].setValue(2.f); params[ACC_PARAM + k].setValue(0.f); }
		params[LAST_A_PARAM].setValue(16.f); params[LAST_B_PARAM].setValue(16.f);
	}
	void randomisePitches() {
		for (int k = 0; k < 32; k++) params[SLIDER_PARAM + k].setValue(random::uniform());
	}
	void copyPattern() { storePanel(); clipboard = core.pat[core.cur]; haveClipboard = true; }
	void pastePattern() {
		if (!haveClipboard) return;
		const wb::TaktPattern& P = clipboard;
		for (int r = 0; r < 2; r++)
			for (int i = 0; i < 16; i++) {
				int k = r * 16 + i;
				params[SLIDER_PARAM + k].setValue(P.slider[r][i]);
				params[MODE_PARAM + k].setValue((float)(2 - P.mode[r][i]));
				params[ACC_PARAM + k].setValue(P.accent[r][i] ? 1.f : 0.f);
			}
		params[LAST_A_PARAM].setValue((float)P.last[0]);
		params[LAST_B_PARAM].setValue((float)P.last[1]);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* pats = json_array();
		for (int p = 0; p < 8; p++) {
			const wb::TaktPattern& P = core.pat[p];
			json_t* jp = json_object();
			json_t* sl = json_array(); json_t* md = json_array(); json_t* ac = json_array();
			for (int r = 0; r < 2; r++)
				for (int i = 0; i < 16; i++) {
					json_array_append_new(sl, json_real(P.slider[r][i]));
					json_array_append_new(md, json_integer(P.mode[r][i]));
					json_array_append_new(ac, json_boolean(P.accent[r][i]));
				}
			json_object_set_new(jp, "slider", sl);
			json_object_set_new(jp, "mode", md);
			json_object_set_new(jp, "accent", ac);
			json_object_set_new(jp, "lastA", json_integer(P.last[0]));
			json_object_set_new(jp, "lastB", json_integer(P.last[1]));
			json_array_append_new(pats, jp);
		}
		json_object_set_new(root, "patterns", pats);
		json_object_set_new(root, "current", json_integer(core.cur));
		json_object_set_new(root, "scale", json_integer(scale));
		json_object_set_new(root, "keyTranspose", json_integer(keyTrans));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* pats = json_object_get(root, "patterns");
		if (pats && json_is_array(pats)) {
			for (int p = 0; p < 8 && p < (int)json_array_size(pats); p++) {
				json_t* jp = json_array_get(pats, p);
				wb::TaktPattern& P = core.pat[p];
				P.clear();
				json_t* sl = json_object_get(jp, "slider"); json_t* md = json_object_get(jp, "mode"); json_t* ac = json_object_get(jp, "accent");
				for (int k = 0; k < 32; k++) {
					int r = k / 16, i = k % 16;
					if (sl && k < (int)json_array_size(sl)) P.slider[r][i] = clamp((float)json_number_value(json_array_get(sl, k)), 0.f, 1.f);
					if (md && k < (int)json_array_size(md)) P.mode[r][i] = clamp((int)json_integer_value(json_array_get(md, k)), 0, 2);
					if (ac && k < (int)json_array_size(ac)) P.accent[r][i] = json_boolean_value(json_array_get(ac, k));
				}
				json_t* la = json_object_get(jp, "lastA"); json_t* lb = json_object_get(jp, "lastB");
				if (la) P.last[0] = clamp((int)json_integer_value(la), 1, 16);
				if (lb) P.last[1] = clamp((int)json_integer_value(lb), 1, 16);
			}
		}
		json_t* c = json_object_get(root, "current");
		if (c) { core.cur = core.requested = clamp((int)json_integer_value(c), 0, 7); }
		json_t* sc = json_object_get(root, "scale");
		if (sc) { scale = (uint16_t)(json_integer_value(sc) & 0xFFF); if (!scale) scale = 0xFFF; }
		json_t* kt = json_object_get(root, "keyTranspose");
		if (kt) keyTrans = clamp((int)json_integer_value(kt), 0, 11);
		loadPanel();
	}
};

wb::TaktPattern Takt::clipboard;
bool Takt::haveClipboard = false;

struct TaktWidget : ModuleWidget {
	TaktWidget(Takt* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Takt.svg")));
		addScrews(this);
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::PATTERN), module, Takt::PATTERN_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::GATE), module, Takt::GATE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::LAST_A), module, Takt::LAST_A_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::LAST_B), module, Takt::LAST_B_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm(L::RUN), module, Takt::RUN_PARAM, Takt::RUN_LIGHT));
		addParam(createParamCentered<VCVButton>(mm(L::RESET), module, Takt::RESET_PARAM));
		addParam(createParamCentered<CKSSThree>(mm(L::RANGE), module, Takt::RANGE_PARAM));
		addParam(createParamCentered<CKSS>(mm(L::CHAIN), module, Takt::CHAIN_PARAM));
		addParam(createParamCentered<CKSS>(mm(L::WHEN), module, Takt::WHEN_PARAM));
		addParam(createParamCentered<CKSS>(mm(L::KEYS), module, Takt::KEYS_PARAM));
		const layout::P keys[12] = {L::KEY0, L::KEY1, L::KEY2, L::KEY3, L::KEY4, L::KEY5, L::KEY6, L::KEY7, L::KEY8, L::KEY9, L::KEY10, L::KEY11};
		for (int n = 0; n < 12; n++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<OrangeLight>>>(mm(keys[n]), module, Takt::KEY_PARAM + n, Takt::KEY_LIGHT + n));
		const layout::P lt[32] = {L::A_LIGHT0, L::A_LIGHT1, L::A_LIGHT2, L::A_LIGHT3, L::A_LIGHT4, L::A_LIGHT5, L::A_LIGHT6, L::A_LIGHT7,
			L::A_LIGHT8, L::A_LIGHT9, L::A_LIGHT10, L::A_LIGHT11, L::A_LIGHT12, L::A_LIGHT13, L::A_LIGHT14, L::A_LIGHT15,
			L::B_LIGHT0, L::B_LIGHT1, L::B_LIGHT2, L::B_LIGHT3, L::B_LIGHT4, L::B_LIGHT5, L::B_LIGHT6, L::B_LIGHT7,
			L::B_LIGHT8, L::B_LIGHT9, L::B_LIGHT10, L::B_LIGHT11, L::B_LIGHT12, L::B_LIGHT13, L::B_LIGHT14, L::B_LIGHT15};
		const layout::P sl[32] = {L::A_SLIDER0, L::A_SLIDER1, L::A_SLIDER2, L::A_SLIDER3, L::A_SLIDER4, L::A_SLIDER5, L::A_SLIDER6, L::A_SLIDER7,
			L::A_SLIDER8, L::A_SLIDER9, L::A_SLIDER10, L::A_SLIDER11, L::A_SLIDER12, L::A_SLIDER13, L::A_SLIDER14, L::A_SLIDER15,
			L::B_SLIDER0, L::B_SLIDER1, L::B_SLIDER2, L::B_SLIDER3, L::B_SLIDER4, L::B_SLIDER5, L::B_SLIDER6, L::B_SLIDER7,
			L::B_SLIDER8, L::B_SLIDER9, L::B_SLIDER10, L::B_SLIDER11, L::B_SLIDER12, L::B_SLIDER13, L::B_SLIDER14, L::B_SLIDER15};
		const layout::P md[32] = {L::A_MODE0, L::A_MODE1, L::A_MODE2, L::A_MODE3, L::A_MODE4, L::A_MODE5, L::A_MODE6, L::A_MODE7,
			L::A_MODE8, L::A_MODE9, L::A_MODE10, L::A_MODE11, L::A_MODE12, L::A_MODE13, L::A_MODE14, L::A_MODE15,
			L::B_MODE0, L::B_MODE1, L::B_MODE2, L::B_MODE3, L::B_MODE4, L::B_MODE5, L::B_MODE6, L::B_MODE7,
			L::B_MODE8, L::B_MODE9, L::B_MODE10, L::B_MODE11, L::B_MODE12, L::B_MODE13, L::B_MODE14, L::B_MODE15};
		const layout::P ac[32] = {L::A_ACC0, L::A_ACC1, L::A_ACC2, L::A_ACC3, L::A_ACC4, L::A_ACC5, L::A_ACC6, L::A_ACC7,
			L::A_ACC8, L::A_ACC9, L::A_ACC10, L::A_ACC11, L::A_ACC12, L::A_ACC13, L::A_ACC14, L::A_ACC15,
			L::B_ACC0, L::B_ACC1, L::B_ACC2, L::B_ACC3, L::B_ACC4, L::B_ACC5, L::B_ACC6, L::B_ACC7,
			L::B_ACC8, L::B_ACC9, L::B_ACC10, L::B_ACC11, L::B_ACC12, L::B_ACC13, L::B_ACC14, L::B_ACC15};
		for (int k = 0; k < 32; k++) {
			addChild(createLightCentered<SmallLight<OrangeLight>>(mm(lt[k]), module, Takt::STEP_LIGHT + k));
			addParam(createParamCentered<VCVSlider>(mm(sl[k]), module, Takt::SLIDER_PARAM + k));
			addParam(createParamCentered<CKSSThree>(mm(md[k]), module, Takt::MODE_PARAM + k));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm(ac[k]), module, Takt::ACC_PARAM + k, Takt::ACC_LIGHT + k));
		}
		const layout::P ip[6] = {L::CLOCK_IN, L::RESET_IN, L::RUN_IN, L::TRANS_IN, L::GATECV_IN, L::PATCV_IN};
		for (int i = 0; i < 6; i++) addInput(createInputCentered<PJ301MPort>(mm(ip[i]), module, Takt::CLOCK_INPUT + i));
		const layout::P op[7] = {L::A_VOCT, L::A_GATE, L::A_ACC, L::B_VOCT, L::B_GATE, L::B_ACC, L::END_OUT};
		for (int i = 0; i < 7; i++) addOutput(createOutputCentered<PJ301MPort>(mm(op[i]), module, Takt::A_VOCT_OUTPUT + i));
	}
	void appendContextMenu(Menu* menu) override {
		Takt* m = dynamic_cast<Takt*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Pattern %d", m->core.cur + 1)));
		menu->addChild(createMenuItem("Copy pattern", "", [=]() { m->copyPattern(); }));
		menu->addChild(createMenuItem("Paste pattern", "", [=]() { m->pastePattern(); }, !Takt::haveClipboard));
		menu->addChild(createMenuItem("Clear pattern", "", [=]() { m->clearPattern(); }));
		menu->addChild(createMenuItem("Randomise pitches in scale", "", [=]() { m->randomisePitches(); }));
	}
};

Model* modelTakt = createModel<Takt, TaktWidget>("Takt");
