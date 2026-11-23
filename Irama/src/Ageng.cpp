// Ageng: the gong pair (gong ageng + second gong) and kempul.
// Engine: AgengEngine in Instruments.hpp. Design: docs/instruments-design.md
#include "plugin.hpp"
#include "Instruments.hpp"

using namespace bronze;

// ---- LAYOUT (same numbers in res/make_instrument_panels.py) ----
static const float A_COLX[3] = {16.f, 45.72f, 75.44f};
static const float A_ROW_Y[3] = {33.f, 54.f, 75.f};
static const float A_ROW4_Y = 89.f;
static const float A_IN_Y = 102.f, A_IN_X0 = 11.f, A_IN_DX = 23.15f;
static const float A_BOT_Y = 117.f, A_BOT_X0 = 9.f, A_BOT_DX = 14.69f;

struct Ageng : Module {
	enum ParamIds {
		TUNE_PARAM, INTERVAL_PARAM, SIZE_PARAM, BLOOM_PARAM, OMBAK_PARAM, MALLET_PARAM,
		BRIGHT_PARAM, KTUNE_PARAM, KRING_PARAM, ALT_PARAM, GLEVEL_PARAM, KLEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds { GONG_INPUT, GONG2_INPUT, KEMPUL_INPUT, KVOCT_INPUT, VEL_INPUT, DAMP_INPUT, TUNE_INPUT, NUM_INPUTS };
	enum OutputIds { L_OUTPUT, R_OUTPUT, SUB_OUTPUT, NUM_OUTPUTS };
	enum LightIds { GONG_LIGHT, GONG2_LIGHT, NUM_LIGHTS };

	AgengEngine eng;
	dsp::SchmittTrigger gongT, gong2T, kempT, dampT;
	int alt = 0;
	float light[2] = {};

	Ageng() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TUNE_PARAM, std::log2(32.f), std::log2(98.f), std::log2(49.f), "Gong pitch", " Hz", 2.f);
		configParam(INTERVAL_PARAM, 0.f, 12.f, 3.f, "Second gong above the first", " semitones");
		getParamQuantity(INTERVAL_PARAM)->snapEnabled = true;
		configParam(SIZE_PARAM, 0.f, 1.f, 0.5f, "Size (decay 6-30 s)");
		configParam(BLOOM_PARAM, 0.f, 1.f, 0.4f, "Bloom (swell time)");
		configParam(OMBAK_PARAM, 0.f, 6.f, 1.6f, "Ombak (the gong's own beating)", " Hz");
		configParam(MALLET_PARAM, 0.f, 1.f, 0.35f, "Mallet (soft to hard)");
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.5f, "Bright (upper partials)");
		configParam(KTUNE_PARAM, -12.f, 12.f, 0.f, "Kempul tune", " semitones");
		configParam(KRING_PARAM, 0.5f, 8.f, 2.5f, "Kempul ring", " s");
		configSwitch(ALT_PARAM, 0.f, 1.f, 0.f, "Alternate", {"Off: GONG plays the first gong", "On: GONG alternates the two gongs (wadon / lanang)"});
		configParam(GLEVEL_PARAM, 0.f, 1.f, 1.f, "Gong level")->randomizeEnabled = false;
		configParam(KLEVEL_PARAM, 0.f, 1.f, 0.8f, "Kempul level")->randomizeEnabled = false;
		configInput(GONG_INPUT, "Gong trigger");
		configInput(GONG2_INPUT, "Second gong trigger");
		configInput(KEMPUL_INPUT, "Kempul trigger");
		configInput(KVOCT_INPUT, "Kempul V/oct (e.g. Irama SARON; unpatched: 2 octaves above the gong)");
		configInput(VEL_INPUT, "Velocity (0-10 V)");
		configInput(DAMP_INPUT, "Damp (hand on the gongs)");
		configInput(TUNE_INPUT, "Gong tune V/oct");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		configOutput(SUB_OUTPUT, "Sub (gong fundamentals only)");
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setSampleRate(e.sampleRate); }
	void onAdd(const AddEvent& e) override { eng.setSampleRate(APP->engine->getSampleRate()); }

	AgengParams P() {
		AgengParams q;
		q.tuneHz = std::pow(2.f, params[TUNE_PARAM].getValue() + inputs[TUNE_INPUT].getVoltage());
		q.tuneHz = clamp(q.tuneHz, 16.f, 400.f);
		q.interval = params[INTERVAL_PARAM].getValue();
		q.size = params[SIZE_PARAM].getValue();
		q.bloom = params[BLOOM_PARAM].getValue();
		q.ombak = params[OMBAK_PARAM].getValue();
		q.mallet = params[MALLET_PARAM].getValue();
		q.bright = params[BRIGHT_PARAM].getValue();
		q.kempulRing = params[KRING_PARAM].getValue();
		q.gongLevel = params[GLEVEL_PARAM].getValue();
		q.kempulLevel = params[KLEVEL_PARAM].getValue();
		return q;
	}

	void process(const ProcessArgs& args) override {
		float vel = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 0.8f;
		bool g1 = gongT.process(inputs[GONG_INPUT].getVoltage(), 0.1f, 1.f);
		bool g2 = gong2T.process(inputs[GONG2_INPUT].getVoltage(), 0.1f, 1.f);
		bool k = kempT.process(inputs[KEMPUL_INPUT].getVoltage(), 0.1f, 1.f);
		if (g1 || g2 || k) {
			AgengParams q = P();
			if (g1) {
				int w = 0;
				if (params[ALT_PARAM].getValue() > 0.5f) { w = alt; alt ^= 1; }
				eng.strikeGong(w, vel, q); light[w] = 1.f;
			}
			if (g2) { eng.strikeGong(1, vel, q); light[1] = 1.f; }
			if (k) {
				float hz = inputs[KVOCT_INPUT].isConnected() ? dsp::FREQ_C4 * std::pow(2.f, inputs[KVOCT_INPUT].getVoltage()) : q.tuneHz * 4.f;
				hz *= std::pow(2.f, params[KTUNE_PARAM].getValue() / 12.f);
				eng.strikeKempul(clamp(hz, 20.f, 4000.f), vel, q);
			}
		}
		if (dampT.process(inputs[DAMP_INPUT].getVoltage(), 0.1f, 1.f)) eng.damp();
		float L, R, S;
		eng.process(L, R, S);
		bool stereo = outputs[R_OUTPUT].isConnected();
		outputs[L_OUTPUT].setVoltage(stereo ? L : 0.5f * (L + R));   // L alone = mono mix
		outputs[R_OUTPUT].setVoltage(R);
		outputs[SUB_OUTPUT].setVoltage(S);
		for (int i = 0; i < 2; i++) {
			lights[GONG_LIGHT + i].setBrightness(light[i]);
			light[i] = std::max(0.f, light[i] - args.sampleTime * 2.f);
		}
	}
};

struct AgengWidget : ModuleWidget {
	AgengWidget(Ageng* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ageng.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		const int knobs[9] = {Ageng::TUNE_PARAM, Ageng::INTERVAL_PARAM, Ageng::SIZE_PARAM, Ageng::BLOOM_PARAM, Ageng::OMBAK_PARAM,
		                      Ageng::MALLET_PARAM, Ageng::BRIGHT_PARAM, Ageng::KTUNE_PARAM, Ageng::KRING_PARAM};
		for (int i = 0; i < 9; i++) {
			Vec p = mm2px(Vec(A_COLX[i % 3], A_ROW_Y[i / 3]));
			if (i == 0) addParam(createParamCentered<RoundLargeBlackKnob>(p, module, knobs[i]));
			else addParam(createParamCentered<RoundBlackKnob>(p, module, knobs[i]));
		}
		addParam(createParamCentered<CKSS>(mm2px(Vec(A_COLX[0], A_ROW4_Y)), module, Ageng::ALT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(A_COLX[1], A_ROW4_Y)), module, Ageng::GLEVEL_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(A_COLX[2], A_ROW4_Y)), module, Ageng::KLEVEL_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(A_COLX[0] + 7.f, A_ROW4_Y - 3.f)), module, Ageng::GONG_LIGHT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(A_COLX[0] + 7.f, A_ROW4_Y + 3.f)), module, Ageng::GONG2_LIGHT));
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(A_IN_X0 + A_IN_DX * i, A_IN_Y)), module, Ageng::GONG_INPUT + i));
		for (int i = 0; i < 3; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(A_BOT_X0 + A_BOT_DX * i, A_BOT_Y)), module, Ageng::VEL_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(A_BOT_X0 + A_BOT_DX * (3 + i), A_BOT_Y)), module, Ageng::L_OUTPUT + i));
		}
	}
};

Model* modelAgeng = createModel<Ageng, AgengWidget>("Ageng");
