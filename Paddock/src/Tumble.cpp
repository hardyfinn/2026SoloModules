// Tumble: five rhythm cores ("rolz") that charge, flip and charge again at rhythm rates.
// A TOUCH knob sits between each pair of neighbours (in a ring: 5 touches 1). Turned
// right it pulls them together, left it pushes them apart, like a finger bridging two
// brass nodes. Every rising flip strikes that rolz's gong. In the spirit of Peter
// Blasser's Rollz; our own design and name.
#include "plugin.hpp"

using namespace paddock;

// ---- LAYOUT Tumble (same numbers in res/make_panel.py) ----
static const float TU_X[5] = {11.6f, 31.2f, 50.8f, 70.4f, 90.0f};
static const float TU_RATE_Y = 28.5f;
static const float TU_TOUCH_Y = 49.0f;
static const float TU_TCV_Y = 62.5f;
static const float TU_GATE_Y = 78.0f;
static const float TU_KNOB_Y = 96.0f;
static const float TU_JACK_Y = 115.0f;

struct Tumble : PaddockModule {
	enum ParamIds {
		ENUMS(RATE_PARAM, 5), ENUMS(TOUCH_PARAM, 5),
		TEMPO_PARAM, PITCH_PARAM, SPREAD_PARAM, TONE_PARAM, DECAY_PARAM,
		NUM_PARAMS
	};
	enum InputIds { ENUMS(TOUCH_INPUT, 5), SYNC_INPUT, TEMPO_INPUT, PITCH_INPUT, NUM_INPUTS };
	enum OutputIds { ENUMS(GATE_OUTPUT, 5), OUTL_OUTPUT, OUTR_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(RATE_LIGHT, 5), NUM_LIGHTS };

	TumbleEngine eng;
	dsp::SchmittTrigger syncTrig;
	dsp::ClockDivider lightDiv;
	float flash[5] = {};

	Tumble() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float defRate[5] = {0.42f, 0.52f, 0.36f, 0.47f, 0.60f};
		for (int i = 0; i < 5; i++) {
			configParam(RATE_PARAM + i, 0.f, 1.f, defRate[i], string::f("Rolz %d rate", i + 1), " Hz", 80.f, 0.25f);
			configParam(TOUCH_PARAM + i, -1.f, 1.f, 0.f,
			            string::f("Touch %d-%d (right: pull together, left: push apart)", i + 1, (i + 1) % 5 + 1), "%", 0.f, 100.f);
			configInput(TOUCH_INPUT + i, string::f("Touch %d-%d CV (adds; 10 V = full right, a gate works like a finger)", i + 1, (i + 1) % 5 + 1));
			configOutput(GATE_OUTPUT + i, string::f("Rolz %d gate", i + 1));
		}
		configParam(TEMPO_PARAM, -2.f, 2.f, 0.f, "Tempo (all rolz)", "x", 2.f);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.4f, "Gong pitch (rolz 1)", " Hz", 32.f, 40.f);
		configParam(SPREAD_PARAM, 0.f, 12.f, 3.f, "Gong spread (each rolz this many semitones above the last)", " st");
		configParam(TONE_PARAM, 0.f, 1.f, 0.4f, "Gong tone (wood to metal)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.45f, "Gong decay", " s", 100.f, 0.03f);
		configInput(SYNC_INPUT, "Sync (restarts all five together; e.g. one pulse per bar)");
		configInput(TEMPO_INPUT, "Tempo CV (1 V = twice as fast)");
		configInput(PITCH_INPUT, "Gong pitch (1 V/oct)");
		configOutput(OUTL_OUTPUT, "Gongs left");
		configOutput(OUTR_OUTPUT, "Gongs right");
		lightDiv.setDivision(64);
		newUnit();
	}

	void applySeed() override {
		eng.rng.seed(seed);
		eng.roll();
	}

	void process(const ProcessArgs& args) override {
		if (syncTrig.process(inputs[SYNC_INPUT].getVoltage(), 0.1f, 1.f)) eng.sync();
		float tempo = dsp::exp2_taylor5(clamp(params[TEMPO_PARAM].getValue() + inputs[TEMPO_INPUT].getVoltage(), -6.f, 6.f));
		float hz[5], touch[5];
		for (int i = 0; i < 5; i++) {
			hz[i] = clamp(0.25f * std::pow(80.f, params[RATE_PARAM + i].getValue()) * tempo, 0.01f, 200.f);
			touch[i] = clamp(params[TOUCH_PARAM + i].getValue() + inputs[TOUCH_INPUT + i].getVoltage() * 0.1f, -1.f, 1.f);
		}
		float pitch = 40.f * dsp::exp2_taylor5(clamp(params[PITCH_PARAM].getValue() * 5.f + inputs[PITCH_INPUT].getVoltage(), -4.f, 9.f));
		float decay = 0.03f * std::pow(100.f, params[DECAY_PARAM].getValue());
		eng.process(hz, touch, pitch, params[SPREAD_PARAM].getValue(), params[TONE_PARAM].getValue(), decay, age(), args.sampleRate);

		for (int i = 0; i < 5; i++) {
			outputs[GATE_OUTPUT + i].setVoltage(eng.r[i].s > 0 ? 10.f : 0.f);
			if (eng.fired[i]) flash[i] = 1.f;
		}
		outputs[OUTL_OUTPUT].setVoltage(eng.outL);
		outputs[OUTR_OUTPUT].setVoltage(eng.outR);

		if (lightDiv.process()) {
			float lt = args.sampleTime * 64.f;
			for (int i = 0; i < 5; i++) {
				lights[RATE_LIGHT + i].setBrightness(flash[i]);
				flash[i] = std::max(0.f, flash[i] - lt * 8.f);
			}
		}
	}
};

struct TumbleWidget : ModuleWidget {
	TumbleWidget(Tumble* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Tumble.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int i = 0; i < 5; i++) {
			float x = TU_X[i];
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, TU_RATE_Y)), module, Tumble::RATE_PARAM + i));
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(x + 6.6f, TU_RATE_Y - 5.4f)), module, Tumble::RATE_LIGHT + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x, TU_TOUCH_Y)), module, Tumble::TOUCH_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, TU_TCV_Y)), module, Tumble::TOUCH_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, TU_GATE_Y)), module, Tumble::GATE_OUTPUT + i));
		}
		const int knobs[5] = {Tumble::TEMPO_PARAM, Tumble::PITCH_PARAM, Tumble::SPREAD_PARAM, Tumble::TONE_PARAM, Tumble::DECAY_PARAM};
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(TU_X[i], TU_KNOB_Y)), module, knobs[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TU_X[0], TU_JACK_Y)), module, Tumble::SYNC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TU_X[1], TU_JACK_Y)), module, Tumble::TEMPO_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TU_X[2], TU_JACK_Y)), module, Tumble::PITCH_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TU_X[3], TU_JACK_Y)), module, Tumble::OUTL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TU_X[4], TU_JACK_Y)), module, Tumble::OUTR_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		if (Tumble* m = dynamic_cast<Tumble*>(module)) appendCharacterMenu(menu, m);
	}
};

Model* modelTumble = createModel<Tumble, TumbleWidget>("Tumble");
