// Test Pattern: the four Ikeda voices for Datamatic. The sound is in
// TestPatternDSP.hpp; this file is knobs, jacks and the LINK cable.
// Each GATE input overrides its LINK channel when patched; CUT likewise.
#include "plugin.hpp"
#include "TestPatternDSP.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float VX[4] = {13.4f, 38.4f, 63.4f, 88.4f};
static const float HEAD_Y = 19.0f;
static const float ZONE_Y = 24.8f;
static const float K1_Y = 38.5f, K2_Y = 53.5f, K3_Y = 68.5f, LEV_Y = 81.5f;
static const float GATE_Y = 94.0f, OUT_Y = 107.0f, IO_Y = 120.0f;
static const float JX0 = 9.3f, JDX = 16.6f;

struct TestPattern : Module {
	enum ParamIds {
		SUB_PITCH_PARAM, SUB_SHAPE_PARAM, SUB_LEN_PARAM, SUB_LEVEL_PARAM,
		TONE_PITCH_PARAM, TONE_BEAT_PARAM, TONE_SCAN_PARAM, TONE_LEVEL_PARAM,
		CLICK_WIDTH_PARAM, CLICK_COLOUR_PARAM, CLICK_SPREAD_PARAM, CLICK_LEVEL_PARAM,
		NOISE_LEN_PARAM, NOISE_COLOUR_PARAM, NOISE_SPREAD_PARAM, NOISE_LEVEL_PARAM,
		TONE_ZONE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SUB_GATE_INPUT, TONE_GATE_INPUT, CLICK_GATE_INPUT, NOISE_GATE_INPUT,
		LINK_INPUT, SUB_VOCT_INPUT, TONE_VOCT_INPUT, CUT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SUB_OUTPUT, TONE_OUTPUT, CLICK_OUTPUT, NOISE_OUTPUT, L_OUTPUT, R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		SUB_LIGHT, TONE_LIGHT, CLICK_LIGHT, NOISE_LIGHT,
		NUM_LIGHTS
	};

	testpattern::Engine eng;
	testpattern::Params p;
	float flash[4] = {0.f, 0.f, 0.f, 0.f};

	struct HzQuantity : ParamQuantity {
		int which = 0;   // 0 SUB, 1 TONE
		float getDisplayValue() override {
			Module* m = module;
			if (!m) return getValue();
			if (which == 0) return testpattern::subHz(getValue());
			return testpattern::toneHz((int) m->params[TONE_ZONE_PARAM].getValue(), getValue());
		}
		void setDisplayValue(float v) override {
			if (which == 0) setValue(clamp(std::log2(std::max(1.f, v) / 25.f) / std::log2(160.f / 25.f), 0.f, 1.f));
		}
	};

	TestPattern() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		HzQuantity* q = configParam<HzQuantity>(SUB_PITCH_PARAM, 0.f, 1.f, 0.4f, "SUB pitch", " Hz");
		q->which = 0;
		configParam(SUB_SHAPE_PARAM, 0.f, 1.f, 0.f, "SUB shape (sine to square)", "%", 0.f, 100.f);
		configParam(SUB_LEN_PARAM, 0.f, 1.f, 1.f, "SUB length (full = as long as the gate)");
		configParam(SUB_LEVEL_PARAM, 0.f, 1.f, 0.8f, "SUB level", "%", 0.f, 100.f);
		q = configParam<HzQuantity>(TONE_PITCH_PARAM, 0.f, 1.f, 0.5f, "TONE pitch", " Hz");
		q->which = 1;
		configParam(TONE_BEAT_PARAM, 0.f, 1.f, 0.15f, "TONE beat (right side detune, up to 20 Hz)", "%", 0.f, 100.f);
		configParam(TONE_SCAN_PARAM, 0.f, 1.f, 0.f, "TONE scan (DATA picks a harmonic)", "%", 0.f, 100.f);
		configParam(TONE_LEVEL_PARAM, 0.f, 1.f, 0.45f, "TONE level", "%", 0.f, 100.f);
		configParam(CLICK_WIDTH_PARAM, 0.f, 1.f, 0.f, "CLICK width", " samples", 32.f, 1.f);
		configParam(CLICK_COLOUR_PARAM, 0.f, 1.f, 0.f, "CLICK colour (sharp to dull)", "%", 0.f, 100.f);
		configParam(CLICK_SPREAD_PARAM, 0.f, 1.f, 0.7f, "CLICK spread (alternate left/right)", "%", 0.f, 100.f);
		configParam(CLICK_LEVEL_PARAM, 0.f, 1.f, 0.8f, "CLICK level", "%", 0.f, 100.f);
		configParam(NOISE_LEN_PARAM, 0.f, 1.f, 0.35f, "NOISE length", " ms", 300.f, 1.f);
		configParam(NOISE_COLOUR_PARAM, 0.f, 1.f, 0.3f, "NOISE colour (white to hiss)", "%", 0.f, 100.f);
		configParam(NOISE_SPREAD_PARAM, 0.f, 1.f, 1.f, "NOISE spread (mono to wide)", "%", 0.f, 100.f);
		configParam(NOISE_LEVEL_PARAM, 0.f, 1.f, 0.35f, "NOISE level", "%", 0.f, 100.f);
		configSwitch(TONE_ZONE_PARAM, 0.f, 2.f, 2.f, "TONE zone", {"LO 100-1000 Hz", "MID 250-4000 Hz (1 kHz at noon)", "HI 4-18 kHz"});
		configInput(SUB_GATE_INPUT, "SUB gate (overrides LINK)");
		configInput(TONE_GATE_INPUT, "TONE gate (overrides LINK)");
		configInput(CLICK_GATE_INPUT, "CLICK trigger (overrides LINK)");
		configInput(NOISE_GATE_INPUT, "NOISE trigger (overrides LINK)");
		configInput(LINK_INPUT, "LINK from Datamatic");
		configInput(SUB_VOCT_INPUT, "SUB V/OCT");
		configInput(TONE_VOCT_INPUT, "TONE V/OCT");
		configInput(CUT_INPUT, "CUT (silence while high, overrides LINK)");
		configOutput(SUB_OUTPUT, "SUB");
		configOutput(TONE_OUTPUT, "TONE");
		configOutput(CLICK_OUTPUT, "CLICK");
		configOutput(NOISE_OUTPUT, "NOISE");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
	}

	void process(const ProcessArgs& args) override {
		p.subPitch = params[SUB_PITCH_PARAM].getValue();
		p.subShape = params[SUB_SHAPE_PARAM].getValue();
		p.subLen = params[SUB_LEN_PARAM].getValue();
		p.subLevel = params[SUB_LEVEL_PARAM].getValue();
		p.toneZone = (int) params[TONE_ZONE_PARAM].getValue();
		p.tonePitch = params[TONE_PITCH_PARAM].getValue();
		p.toneBeat = params[TONE_BEAT_PARAM].getValue();
		p.toneScan = params[TONE_SCAN_PARAM].getValue();
		p.toneLevel = params[TONE_LEVEL_PARAM].getValue();
		p.clickWidth = params[CLICK_WIDTH_PARAM].getValue();
		p.clickColour = params[CLICK_COLOUR_PARAM].getValue();
		p.clickSpread = params[CLICK_SPREAD_PARAM].getValue();
		p.clickLevel = params[CLICK_LEVEL_PARAM].getValue();
		p.noiseLen = params[NOISE_LEN_PARAM].getValue();
		p.noiseColour = params[NOISE_COLOUR_PARAM].getValue();
		p.noiseSpread = params[NOISE_SPREAD_PARAM].getValue();
		p.noiseLevel = params[NOISE_LEVEL_PARAM].getValue();

		Input& link = inputs[LINK_INPUT];
		int lc = link.isConnected() ? link.getChannels() : 0;
		bool gate[4];
		for (int v = 0; v < 4; v++) {
			Input& in = inputs[SUB_GATE_INPUT + v];
			float g = in.isConnected() ? in.getVoltage() : (lc > v ? link.getVoltage(v) : 0.f);
			gate[v] = g >= 1.f;
		}
		float data = lc > 4 ? link.getVoltage(4) : 0.f;
		float cutV = inputs[CUT_INPUT].isConnected() ? inputs[CUT_INPUT].getVoltage() : (lc > 5 ? link.getVoltage(5) : 0.f);

		testpattern::Frame f = eng.process(args.sampleRate, gate, cutV >= 1.f, data,
		                                   inputs[SUB_VOCT_INPUT].getVoltage(), inputs[TONE_VOCT_INPUT].getVoltage(), p);
		for (int v = 0; v < 4; v++) {
			outputs[SUB_OUTPUT + v].setVoltage(f.voice[v]);
			if (f.active[v]) flash[v] = 1.f;
			else flash[v] = std::max(0.f, flash[v] - args.sampleTime * 12.f);
			lights[SUB_LIGHT + v].setBrightness(flash[v]);
		}
		outputs[L_OUTPUT].setVoltage(clamp(f.l, -11.f, 11.f));
		outputs[R_OUTPUT].setVoltage(clamp(f.r, -11.f, 11.f));
	}
};

struct TestPatternWidget : ModuleWidget {
	TestPatternWidget(TestPattern* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/TestPattern.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int v = 0; v < 4; v++) {
			addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(VX[v] + 10.6f, HEAD_Y - 1.2f)), module, TestPattern::SUB_LIGHT + v));
			int base = TestPattern::SUB_PITCH_PARAM + 4 * v;
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(VX[v], K1_Y)), module, base + 0));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(VX[v], K2_Y)), module, base + 1));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(VX[v], K3_Y)), module, base + 2));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(VX[v], LEV_Y)), module, base + 3));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(VX[v], GATE_Y)), module, TestPattern::SUB_GATE_INPUT + v));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(VX[v], OUT_Y)), module, TestPattern::SUB_OUTPUT + v));
		}
		addParam(createParamCentered<CKSSThreeHorizontal>(mm2px(Vec(VX[1], ZONE_Y)), module, TestPattern::TONE_ZONE_PARAM));

		const int io[4] = {TestPattern::LINK_INPUT, TestPattern::SUB_VOCT_INPUT, TestPattern::TONE_VOCT_INPUT, TestPattern::CUT_INPUT};
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, IO_Y)), module, io[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * 4, IO_Y)), module, TestPattern::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * 5, IO_Y)), module, TestPattern::R_OUTPUT));
	}
};

Model* modelTestPattern = createModel<TestPattern, TestPatternWidget>("TestPattern");
