// Swerve: one bounce oscillator (in the spirit of Peter Blasser's "horse" circuits).
// It rises at the UP slope to HIGH, turns, falls at the DOWN slope to LOW, turns again.
// Move the bounds and the pitch follows the gap; cross them and it goes into paradox.
#include "plugin.hpp"

using namespace paddock;

// ---- LAYOUT Swerve (same numbers in res/make_panel.py) ----
static const float SW_KX[2] = {16.0f, 44.96f};
static const float SW_K1_Y = 29.0f;
static const float SW_K2_Y = 48.5f;
static const float SW_SW_Y = 69.0f;
static const float SW_RANGE_X = 9.0f, SW_CYCLE_X = 31.0f, SW_LIGHT_X = 52.0f;
static const float SW_TX[3] = {11.0f, 30.48f, 49.96f};
static const float SW_TRIM_Y = 84.0f;
static const float SW_CVJ_Y = 92.6f;
static const float SW_JX[4] = {8.5f, 23.2f, 37.8f, 52.46f};
static const float SW_IN_Y = 105.6f;
static const float SW_OUT_Y = 117.6f;

struct Swerve : PaddockModule {
	enum ParamIds {
		UP_PARAM, DOWN_PARAM, HI_PARAM, LO_PARAM, RANGE_PARAM, MODE_PARAM,
		UP_ATT, BOTH_ATT, DOWN_ATT,
		NUM_PARAMS
	};
	enum InputIds { UP_INPUT, BOTH_INPUT, DOWN_INPUT, HI_INPUT, LO_INPUT, TRIGUP_INPUT, TRIGDOWN_INPUT, NUM_INPUTS };
	enum OutputIds { TRI_OUTPUT, SQR_OUTPUT, TOP_OUTPUT, BOTTOM_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(DIR_LIGHT, 2), PARADOX_LIGHT, NUM_LIGHTS };

	SwerveEngine eng;
	Dec decTri, decSqr;
	dsp::SchmittTrigger upTrig, downTrig;
	dsp::PulseGenerator topPulse, bottomPulse;
	dsp::ClockDivider lightDiv;

	Swerve() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(UP_PARAM, 0.f, 1.f, 0.45f, "Up slope (how fast it rises)");
		configParam(DOWN_PARAM, 0.f, 1.f, 0.45f, "Down slope (how fast it falls)");
		configParam(HI_PARAM, -5.f, 5.f, 5.f, "High bound (added to HIGH in)", " V");
		configParam(LO_PARAM, -5.f, 5.f, -5.f, "Low bound (added to LOW in)", " V");
		configSwitch(RANGE_PARAM, 0.f, 2.f, 2.f, "Range", {"CV (0.005-20 Hz)", "Low (0.5-128 Hz)", "Audio (16 Hz-8 kHz)"});
		configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Mode", {"Once (waits at LOW for a trigger)", "Cycle"});
		configParam(UP_ATT, -1.f, 1.f, 0.f, "Up CV amount (1 V/oct at full)", "%", 0.f, 100.f);
		configParam(BOTH_ATT, -1.f, 1.f, 1.f, "Both CV amount (1 V/oct at full)", "%", 0.f, 100.f);
		configParam(DOWN_ATT, -1.f, 1.f, 0.f, "Down CV amount (1 V/oct at full)", "%", 0.f, 100.f);
		configInput(UP_INPUT, "Up slope CV");
		configInput(BOTH_INPUT, "Both slopes CV (1 V/oct)");
		configInput(DOWN_INPUT, "Down slope CV");
		configInput(HI_INPUT, "High bound");
		configInput(LO_INPUT, "Low bound");
		configInput(TRIGUP_INPUT, "Swoop up (trigger: turns it upwards now)");
		configInput(TRIGDOWN_INPUT, "Swoop down (trigger: turns it downwards now)");
		configOutput(TRI_OUTPUT, "Position (triangle, between the bounds)");
		configOutput(SQR_OUTPUT, "Direction (square, +5 V rising, -5 V falling)");
		configOutput(TOP_OUTPUT, "Top (10 ms pulse when it turns at HIGH)");
		configOutput(BOTTOM_OUTPUT, "Bottom (10 ms pulse when it turns at LOW, end of a one-shot)");
		lightDiv.setDivision(64);
		newUnit();
	}

	void applySeed() override {
		eng.rng.seed(seed);
		eng.h.roll(eng.rng);
	}

	void process(const ProcessArgs& args) override {
		int range = (int) params[RANGE_PARAM].getValue();
		bool once = params[MODE_PARAM].getValue() < 0.5f;
		float both = inputs[BOTH_INPUT].getVoltage() * params[BOTH_ATT].getValue();
		float upOct = both + inputs[UP_INPUT].getVoltage() * params[UP_ATT].getValue();
		float downOct = both + inputs[DOWN_INPUT].getVoltage() * params[DOWN_ATT].getValue();
		float maxHz = std::min(20000.f, args.sampleRate * 0.45f);
		float upHz = clamp(rangeHz(range, params[UP_PARAM].getValue()) * dsp::exp2_taylor5(clamp(upOct, -12.f, 12.f)), 1e-4f, maxHz);
		float downHz = clamp(rangeHz(range, params[DOWN_PARAM].getValue()) * dsp::exp2_taylor5(clamp(downOct, -12.f, 12.f)), 1e-4f, maxHz);
		float hi = params[HI_PARAM].getValue() + inputs[HI_INPUT].getVoltage();
		float lo = params[LO_PARAM].getValue() + inputs[LO_INPUT].getVoltage();

		if (upTrig.process(inputs[TRIGUP_INPUT].getVoltage(), 0.1f, 1.f)) eng.trigUp();
		if (downTrig.process(inputs[TRIGDOWN_INPUT].getVoltage(), 0.1f, 1.f)) eng.trigDown();
		if (!once && eng.h.dir == 0) eng.h.dir = 1;   // switched back to cycle

		eng.process(upHz, downHz, lo, hi, once, age(), args.sampleTime);
		if (eng.events & 1) topPulse.trigger(0.01f);
		if (eng.events & 2) bottomPulse.trigger(0.01f);

		outputs[TRI_OUTPUT].setVoltage(decTri.process(eng.tri));
		outputs[SQR_OUTPUT].setVoltage(decSqr.process(eng.sqr));
		outputs[TOP_OUTPUT].setVoltage(topPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[BOTTOM_OUTPUT].setVoltage(bottomPulse.process(args.sampleTime) ? 10.f : 0.f);

		if (lightDiv.process()) {
			float lt = args.sampleTime * 64.f;
			lights[DIR_LIGHT + 0].setBrightnessSmooth(eng.h.dir > 0 ? 1.f : 0.f, lt);
			lights[DIR_LIGHT + 1].setBrightnessSmooth(eng.h.dir < 0 ? 1.f : 0.f, lt);
			lights[PARADOX_LIGHT].setBrightnessSmooth(eng.h.paradox ? 1.f : 0.f, lt);
		}
	}
};

struct SwerveWidget : ModuleWidget {
	SwerveWidget(Swerve* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Swerve.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(SW_KX[0], SW_K1_Y)), module, Swerve::UP_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(SW_KX[1], SW_K1_Y)), module, Swerve::DOWN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(SW_KX[0], SW_K2_Y)), module, Swerve::HI_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(SW_KX[1], SW_K2_Y)), module, Swerve::LO_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(SW_RANGE_X, SW_SW_Y)), module, Swerve::RANGE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(SW_CYCLE_X, SW_SW_Y)), module, Swerve::MODE_PARAM));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(SW_LIGHT_X, SW_SW_Y - 1.0f)), module, Swerve::DIR_LIGHT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(SW_LIGHT_X, SW_SW_Y + 4.0f)), module, Swerve::PARADOX_LIGHT));

		const int atts[3] = {Swerve::UP_ATT, Swerve::BOTH_ATT, Swerve::DOWN_ATT};
		const int cvs[3] = {Swerve::UP_INPUT, Swerve::BOTH_INPUT, Swerve::DOWN_INPUT};
		for (int i = 0; i < 3; i++) {
			addParam(createParamCentered<Trimpot>(mm2px(Vec(SW_TX[i], SW_TRIM_Y)), module, atts[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(SW_TX[i], SW_CVJ_Y)), module, cvs[i]));
		}
		const int ins[4] = {Swerve::HI_INPUT, Swerve::LO_INPUT, Swerve::TRIGUP_INPUT, Swerve::TRIGDOWN_INPUT};
		const int outs[4] = {Swerve::TRI_OUTPUT, Swerve::SQR_OUTPUT, Swerve::TOP_OUTPUT, Swerve::BOTTOM_OUTPUT};
		for (int i = 0; i < 4; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(SW_JX[i], SW_IN_Y)), module, ins[i]));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(SW_JX[i], SW_OUT_Y)), module, outs[i]));
		}
	}
	void appendContextMenu(Menu* menu) override {
		if (Swerve* m = dynamic_cast<Swerve*>(module)) appendCharacterMenu(menu, m);
	}
};

Model* modelSwerve = createModel<Swerve, SwerveWidget>("Swerve");
