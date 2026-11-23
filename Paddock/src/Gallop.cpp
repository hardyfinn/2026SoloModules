// Gallop: four bounce oscillators ("horses") stacked so each one's bounds are its
// neighbours. Horse 1 bounces between +RAIL and horse 2, horse 2 between horse 1 and
// horse 3, ... horse 4 between horse 3 and -RAIL. They push each other around, and
// when a horse's lower neighbour rises above its upper one it goes into paradox.
// In the spirit of Peter Blasser's Fourses; our own design and name.
#include "plugin.hpp"

using namespace paddock;

// ---- LAYOUT Gallop (same numbers in res/make_panel.py) ----
static const float GA_DISP_X = 5.0f, GA_DISP_Y = 13.6f, GA_DISP_W = 111.92f, GA_DISP_H = 14.4f;
static const float GA_NUM_X = 5.6f;
static const float GA_RATE_X = 17.0f, GA_TILT_X = 32.0f, GA_TRIM_X = 44.5f, GA_CV_X = 55.5f;
static const float GA_HI_X = 68.5f, GA_LO_X = 80.5f;
static const float GA_TRI_X = 96.0f, GA_SQR_X = 110.0f;
static const float GA_ROWS[4] = {42.0f, 58.0f, 74.0f, 90.0f};
static const float GA_BOT_Y = 113.0f;
static const float GA_RANGE_X = 10.0f, GA_RAIL_X = 32.0f, GA_FM_X = 55.5f, GA_FMATT_X = 44.5f;
static const float GA_MIX_X = 96.0f, GA_POLY_X = 110.0f;

static const int SCOPE_N = 256;

struct Gallop : PaddockModule {
	enum ParamIds {
		ENUMS(RATE_PARAM, 4), ENUMS(TILT_PARAM, 4), ENUMS(ATT_PARAM, 4),
		RANGE_PARAM, RAIL_PARAM, FM_ATT,
		NUM_PARAMS
	};
	enum InputIds { ENUMS(CV_INPUT, 4), ENUMS(HI_INPUT, 4), ENUMS(LO_INPUT, 4), FM_INPUT, NUM_INPUTS };
	enum OutputIds { ENUMS(TRI_OUTPUT, 4), ENUMS(SQR_OUTPUT, 4), MIX_OUTPUT, POLY_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(DIR_LIGHT, 8), ENUMS(PARADOX_LIGHT, 4), NUM_LIGHTS };

	GallopEngine eng;
	Dec decTri[4], decSqr[4];
	dsp::ClockDivider lightDiv;

	// scope (read by the display)
	float scope[4][SCOPE_N] = {};
	int scopePos = 0, scopeCount = 0;
	float scopeRail = 5.f;

	Gallop() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float defRate[4] = {0.33f, 0.41f, 0.37f, 0.46f};
		for (int i = 0; i < 4; i++) {
			configParam(RATE_PARAM + i, 0.f, 1.f, defRate[i], string::f("Horse %d rate", i + 1));
			configParam(TILT_PARAM + i, -1.f, 1.f, 0.f, string::f("Horse %d tilt (left: slow rise, fast fall; right: the reverse)", i + 1), "%", 0.f, 100.f);
			configParam(ATT_PARAM + i, -1.f, 1.f, 1.f, string::f("Horse %d CV amount (moves rate 1 V/oct and tilt)", i + 1), "%", 0.f, 100.f);
			configInput(CV_INPUT + i, string::f("Horse %d CV", i + 1));
			configInput(HI_INPUT + i, string::f("Horse %d high bound (patched: replaces %s)", i + 1, i == 0 ? "+RAIL" : string::f("horse %d", i).c_str()));
			configInput(LO_INPUT + i, string::f("Horse %d low bound (patched: replaces %s)", i + 1, i == 3 ? "-RAIL" : string::f("horse %d", i + 2).c_str()));
			configOutput(TRI_OUTPUT + i, string::f("Horse %d position", i + 1));
			configOutput(SQR_OUTPUT + i, string::f("Horse %d direction (+5 V rising)", i + 1));
		}
		configSwitch(RANGE_PARAM, 0.f, 2.f, 2.f, "Range", {"CV (slow, for modulation)", "Low", "Audio"});
		configParam(RAIL_PARAM, 1.f, 8.f, 5.f, "Rail (top and bottom bound of the stack)", " V");
		configParam(FM_ATT, -1.f, 1.f, 1.f, "FM amount (all four, 1 V/oct at full)", "%", 0.f, 100.f);
		configInput(FM_INPUT, "FM / pitch for all four (1 V/oct)");
		configOutput(MIX_OUTPUT, "Mix of all four positions");
		configOutput(POLY_OUTPUT, "All four positions (4-channel poly)");
		lightDiv.setDivision(64);
		newUnit();
	}

	void applySeed() override {
		eng.rng.seed(seed);
		for (int i = 0; i < 4; i++) eng.h[i].roll(eng.rng);
	}

	void process(const ProcessArgs& args) override {
		int range = (int) params[RANGE_PARAM].getValue();
		float rail = params[RAIL_PARAM].getValue();
		float fm = inputs[FM_INPUT].getVoltage() * params[FM_ATT].getValue();
		float maxHz = std::min(20000.f, args.sampleRate * 0.45f);
		float hz[4], tilt[4], hiIn[4], loIn[4];
		for (int i = 0; i < 4; i++) {
			float cv = inputs[CV_INPUT + i].getVoltage() * params[ATT_PARAM + i].getValue();
			float oct = clamp(fm + cv, -12.f, 12.f);
			hz[i] = clamp(rangeHz(range, params[RATE_PARAM + i].getValue()) * dsp::exp2_taylor5(oct), 1e-4f, maxHz);
			tilt[i] = clamp(params[TILT_PARAM + i].getValue() + 0.1f * cv, -1.f, 1.f);
			hiIn[i] = inputs[HI_INPUT + i].isConnected() ? inputs[HI_INPUT + i].getVoltage() : NAN;
			loIn[i] = inputs[LO_INPUT + i].isConnected() ? inputs[LO_INPUT + i].getVoltage() : NAN;
		}
		eng.process(hz, tilt, hiIn, loIn, rail, age(), args.sampleTime);

		float mix = 0.f;
		outputs[POLY_OUTPUT].setChannels(4);
		for (int i = 0; i < 4; i++) {
			float t = decTri[i].process(eng.tri[i]);
			float s = decSqr[i].process(eng.sqr[i]);
			outputs[TRI_OUTPUT + i].setVoltage(t);
			outputs[SQR_OUTPUT + i].setVoltage(s);
			outputs[POLY_OUTPUT].setVoltage(t, i);
			mix += t;
		}
		outputs[MIX_OUTPUT].setVoltage(mix * 0.5f);

		// scope: about 2 s across in CV range, 0.2 s in Low, 20 ms in Audio
		int every = range == RANGE_CV ? 512 : (range == RANGE_LOW ? 48 : 4);
		if (++scopeCount >= every) {
			scopeCount = 0;
			for (int i = 0; i < 4; i++) scope[i][scopePos] = eng.h[i].v;
			scopePos = (scopePos + 1) % SCOPE_N;
			scopeRail = rail;
		}

		if (lightDiv.process()) {
			float lt = args.sampleTime * 64.f;
			for (int i = 0; i < 4; i++) {
				lights[DIR_LIGHT + 2 * i + 0].setBrightnessSmooth(eng.h[i].dir > 0 ? 1.f : 0.f, lt);
				lights[DIR_LIGHT + 2 * i + 1].setBrightnessSmooth(eng.h[i].dir < 0 ? 1.f : 0.f, lt);
				lights[PARADOX_LIGHT + i].setBrightnessSmooth(eng.paradox[i] ? 1.f : 0.f, lt);
			}
		}
	}
};

struct GallopScope : Widget {
	Gallop* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			static const NVGcolor cols[4] = {nvgRGB(0xD9, 0xA4, 0x41), nvgRGB(0xF2, 0xE6, 0xCF), nvgRGB(0xE0, 0x6A, 0x3B), nvgRGB(0x8F, 0xC9, 0xB8)};
			float w = box.size.x, h = box.size.y, rail = std::max(1.f, module->scopeRail);
			for (int i = 3; i >= 0; i--) {
				nvgBeginPath(args.vg);
				for (int k = 0; k < SCOPE_N; k++) {
					float v = module->scope[i][(module->scopePos + k) % SCOPE_N];
					float x = w * k / (SCOPE_N - 1);
					float y = h * 0.5f - (h * 0.45f) * clamp(v / rail, -1.1f, 1.1f);
					if (k == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);
				}
				nvgStrokeColor(args.vg, cols[i]);
				nvgStrokeWidth(args.vg, 1.1f);
				nvgStroke(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct GallopWidget : ModuleWidget {
	GallopWidget(Gallop* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gallop.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		GallopScope* sc = new GallopScope;
		sc->module = module;
		sc->box.pos = mm2px(Vec(GA_DISP_X, GA_DISP_Y));
		sc->box.size = mm2px(Vec(GA_DISP_W, GA_DISP_H));
		addChild(sc);

		for (int i = 0; i < 4; i++) {
			float y = GA_ROWS[i];
			addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(GA_NUM_X, y + 4.4f)), module, Gallop::DIR_LIGHT + 2 * i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(GA_RATE_X, y)), module, Gallop::RATE_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(GA_TILT_X, y)), module, Gallop::TILT_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(GA_TRIM_X, y)), module, Gallop::ATT_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GA_CV_X, y)), module, Gallop::CV_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GA_HI_X, y)), module, Gallop::HI_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GA_LO_X, y)), module, Gallop::LO_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(GA_TRI_X, y)), module, Gallop::TRI_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(GA_SQR_X, y)), module, Gallop::SQR_OUTPUT + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(66.0f + 4.0f * i, GA_BOT_Y - 1.0f)), module, Gallop::PARADOX_LIGHT + i));
		}
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(GA_RANGE_X - 2.0f, GA_BOT_Y)), module, Gallop::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(GA_RAIL_X, GA_BOT_Y)), module, Gallop::RAIL_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(GA_FMATT_X, GA_BOT_Y)), module, Gallop::FM_ATT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GA_FM_X, GA_BOT_Y)), module, Gallop::FM_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(GA_MIX_X, GA_BOT_Y)), module, Gallop::MIX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(GA_POLY_X, GA_BOT_Y)), module, Gallop::POLY_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		if (Gallop* m = dynamic_cast<Gallop*>(module)) appendCharacterMenu(menu, m);
	}
};

Model* modelGallop = createModel<Gallop, GallopWidget>("Gallop");
