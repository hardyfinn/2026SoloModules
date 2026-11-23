// Gonzo: the Gongoozler engine pushed past real gongs.
// Same engine (GongEngine.hpp); the extra controls are the "Gonzo" block in its Controls.
// At the knob marks it is a Gongoozler tam-tam; everything else is turning knobs away from them.
#include "plugin.hpp"
#include "GongEngine.hpp"

// ---- LAYOUT (same numbers in res/make_panel_gonzo.py) ----
static const float GCOL0 = 11.5f, GCOLD = 22.8f;
static const float GROW_GONG = 30.0f, GROW_STRIKE = 53.0f, GROW_CASC = 75.0f, GROW_CV = 96.0f, GROW_IO = 114.0f;
static float gcol(int i) { return GCOL0 + GCOLD * i; }

static const char* GZ_TYPE_NAMES[4] = {"Tam-tam", "Wind gong", "Opera gong", "Bossed gong"};

struct Gonzo : Module {
	enum ParamIds {
		TYPE_PARAM, SIZE_PARAM, THICK_PARAM, LAW_PARAM, ORDER_PARAM, DENSITY_PARAM, BEAT_PARAM, CAST_PARAM,
		POSITION_PARAM, MALLET_PARAM, BOW_PARAM, DECAY_PARAM, TILT_PARAM, GLIDE_PARAM, ROAR_PARAM, HIT_PARAM,
		BLOOM_PARAM, BUILD_PARAM, THRESH_PARAM, TUNE_PARAM, MUTE_PARAM, WIDTH_PARAM, LEVEL_PARAM, PITCHED_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, SIZE_INPUT, LAW_INPUT, ORDER_INPUT, POSITION_INPUT, MALLET_INPUT, BOW_INPUT, DECAY_INPUT,
		BLOOM_INPUT, GLIDE_INPUT, TRIG_INPUT, VEL_INPUT, HAND_INPUT, EXCITE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { LEFT_OUTPUT, RIGHT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HIT_LIGHT, NUM_LIGHTS };

	gong::Engine engine;
	gong::Controls ctl;
	dsp::SchmittTrigger trigIn, handIn, hitButton;
	dsp::ClockDivider lightDivider;

	struct TypeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int t = clamp((int) std::round(getValue()) - 1, 0, 3);
			return string::f("%d: %s", t + 1, GZ_TYPE_NAMES[t]);
		}
	};
	struct SizeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float cm = 2.f * std::pow(1000.f, getValue());
			if (cm < 100.f) return string::f("%.0f cm", cm);
			return string::f("%.1f m", cm / 100.f);
		}
	};
	struct LawQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			return string::f("x%.2f (1 = real plate)", std::exp2(getValue()));
		}
	};
	struct OrderQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			if (std::fabs(v) < 0.01f) return "0 (real)";
			return v < 0.f ? string::f("%.0f %% pulled onto harmonics", -v * 100.f) : string::f("%.0f %% scattered", v * 100.f);
		}
	};
	struct DecayQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			if (v >= 1.49f) return "FREEZE (rings forever)";
			return string::f("%.2f (0-1 = real gongs)", v);
		}
	};
	struct ThreshQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			return string::f("x%.2f (left blooms at a whisper, right never)", std::pow(10.f, getValue()));
		}
	};
	struct CastQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("Gong no. %d", (int) std::round(getValue())); }
	};

	Gonzo() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<TypeQuantity>(TYPE_PARAM, 1.f, 4.f, 1.f, "Type (mode family)");
		getParamQuantity(TYPE_PARAM)->snapEnabled = true;
		configParam<SizeQuantity>(SIZE_PARAM, 0.f, 1.f, 0.5511f, "Size (2 cm .. 20 m; real gongs 20-150 cm)");
		configParam(THICK_PARAM, 0.f, 1.f, 0.45f, "Thickness", "%", 0.f, 100.f);
		configParam<LawQuantity>(LAW_PARAM, -2.f, 1.f, 0.f, "Law (mode spacing: squeezed <-> stretched)");
		configParam<OrderQuantity>(ORDER_PARAM, -1.f, 1.f, 0.f, "Order (harmonic <-> scattered)");
		configParam(DENSITY_PARAM, 0.03f, 1.f, 1.f, "Density (how many modes exist)", "%", 0.f, 100.f);
		configParam(BEAT_PARAM, 0.f, 3.f, 0.4f, "Beat (past 1: chorus and clusters)", "%", 0.f, 100.f);
		configParam<CastQuantity>(CAST_PARAM, 1.f, 16.f, 1.f, "Cast");
		getParamQuantity(CAST_PARAM)->snapEnabled = true;

		configParam(POSITION_PARAM, 0.f, 1.f, 0.4f, "Strike position (centre <-> edge)", "%", 0.f, 100.f);
		configParam(MALLET_PARAM, -0.5f, 1.5f, 0.7f, "Mallet (click <-> stick <-> soft beater <-> slow shove)");
		configParam(BOW_PARAM, 0.f, 1.f, 0.f, "Bow (rub the plate continuously, no hits needed)", "%", 0.f, 100.f);
		configParam<DecayQuantity>(DECAY_PARAM, -0.4f, 1.5f, 0.65f, "Decay (fully right = freeze)");
		configParam(TILT_PARAM, -1.f, 1.f, 0.35f, "Tilt (left: lows die first; right: highs die first, like a real gong)");
		configParam(GLIDE_PARAM, -36.f, 36.f, 0.f, "Glide (pitch bend at a hard hit)", " semitones");
		configParam(ROAR_PARAM, 0.f, 2.f, 0.35f, "Roar (stretch; past 1 is beyond any real plate)", "%", 0.f, 100.f);
		configButton(HIT_PARAM, "Hit");

		configParam(BLOOM_PARAM, -1.f, 1.f, 0.7f, "Bloom (right: energy climbs; left: energy sinks into the lows)", "%", 0.f, 100.f);
		configParam(BUILD_PARAM, -0.7f, 1.6f, 0.5f, "Build (how slowly the bloom moves; 0-1 = real gongs)");
		configParam<ThreshQuantity>(THRESH_PARAM, -2.f, 2.f, 0.f, "Threshold (how hard a hit must be to bloom)");
		configParam(TUNE_PARAM, -48.f, 48.f, 0.f, "Tune", " semitones");
		configParam(MUTE_PARAM, 0.f, 1.f, 0.f, "Mute (towel)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.7f, "Stereo width", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configSwitch(PITCHED_PARAM, 0.f, 1.f, 0.f, "Pitch", {"Size sets the pitch (tam-tam, wind)", "V/OCT sets the pitch for every type (0 V = C4)"});

		configInput(VOCT_INPUT, "V/OCT");
		configInput(SIZE_INPUT, "Size CV (+-5 V = full range)");
		configInput(LAW_INPUT, "Law CV (+-5 V = full range)");
		configInput(ORDER_INPUT, "Order CV (+-5 V = full range)");
		configInput(POSITION_INPUT, "Position CV (+-5 V = full range)");
		configInput(MALLET_INPUT, "Mallet CV (+-5 V = full range, read at each hit)");
		configInput(BOW_INPUT, "Bow CV (0-10 V, added to the knob)");
		configInput(DECAY_INPUT, "Decay CV (+-5 V = full range)");
		configInput(BLOOM_INPUT, "Bloom CV (+-5 V = full range, can flip it to sinking)");
		configInput(GLIDE_INPUT, "Glide CV (1 V = 5 semitones)");
		configInput(TRIG_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Velocity (0-10 V; unpatched = 7.5 V)");
		configInput(HAND_INPUT, "Hand (a trigger grabs for 200 ms; a gate holds)");
		configInput(EXCITE_INPUT, "Excite (any audio drives the plate at the strike position)");
		configOutput(LEFT_OUTPUT, "Left (mono when RIGHT is unpatched)");
		configOutput(RIGHT_OUTPUT, "Right");

		ctl.beyond = true;
		lightDivider.setDivision(256);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setSampleRate(e.sampleRate); }

	float knobCv(int p, int in, float lo, float hi) {
		return clamp(params[p].getValue() + inputs[in].getVoltage() * 0.1f * (hi - lo), lo, hi);
	}

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);
		ctl.type = clamp((int) std::round(params[TYPE_PARAM].getValue()) - 1, 0, 3);
		ctl.size = 2.f * std::pow(1000.f, knobCv(SIZE_PARAM, SIZE_INPUT, 0.f, 1.f));
		ctl.thick = params[THICK_PARAM].getValue();
		ctl.law = std::exp2(knobCv(LAW_PARAM, LAW_INPUT, -2.f, 1.f));
		ctl.order = knobCv(ORDER_PARAM, ORDER_INPUT, -1.f, 1.f);
		ctl.density = params[DENSITY_PARAM].getValue();
		ctl.beat = params[BEAT_PARAM].getValue();
		ctl.cast = clamp((int) std::round(params[CAST_PARAM].getValue()) - 1, 0, 15);
		ctl.position = knobCv(POSITION_PARAM, POSITION_INPUT, 0.f, 1.f);
		ctl.mallet = knobCv(MALLET_PARAM, MALLET_INPUT, -0.5f, 1.5f);
		ctl.bow = clamp(params[BOW_PARAM].getValue() + inputs[BOW_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		float dec = knobCv(DECAY_PARAM, DECAY_INPUT, -0.4f, 1.5f);
		ctl.freeze = dec >= 1.49f;
		ctl.decay = dec;
		ctl.damp = params[TILT_PARAM].getValue();
		ctl.glide = clamp(params[GLIDE_PARAM].getValue() + inputs[GLIDE_INPUT].getVoltage() * 5.f, -48.f, 48.f);
		ctl.roar = params[ROAR_PARAM].getValue();
		ctl.bloom = knobCv(BLOOM_PARAM, BLOOM_INPUT, -1.f, 1.f);
		ctl.build = params[BUILD_PARAM].getValue();
		ctl.threshMul = std::pow(10.f, params[THRESH_PARAM].getValue());
		ctl.pitchAll = params[PITCHED_PARAM].getValue() > 0.5f;
		ctl.followPitch = true;
		ctl.tune = params[TUNE_PARAM].getValue() / 12.f + clamp(inputs[VOCT_INPUT].getVoltage(), -10.f, 10.f);
		ctl.mute = params[MUTE_PARAM].getValue();
		ctl.width = params[WIDTH_PARAM].getValue();
		ctl.level = params[LEVEL_PARAM].getValue();
		ctl.hand = inputs[HAND_INPUT].getVoltage() > 1.f;
		ctl.excite = inputs[EXCITE_INPUT].isConnected() ? clamp(inputs[EXCITE_INPUT].getVoltage(), -12.f, 12.f) * 0.1f : 0.f;

		bool hit = hitButton.process(params[HIT_PARAM].getValue());
		if (trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f) || hit) {
			float vel = 0.75f;
			if (inputs[VEL_INPUT].isConnected() && !hit)
				vel = clamp(inputs[VEL_INPUT].getVoltage(), 0.f, 10.f) * 0.1f;
			engine.trigger(vel, ctl);
		}
		if (handIn.process(inputs[HAND_INPUT].getVoltage(), 0.1f, 1.f))
			engine.handGrab();

		engine.process(ctl);
		if (outputs[RIGHT_OUTPUT].isConnected()) {
			outputs[LEFT_OUTPUT].setVoltage(engine.outL);
			outputs[RIGHT_OUTPUT].setVoltage(engine.outR);
		} else {
			outputs[LEFT_OUTPUT].setVoltage(0.5f * (engine.outL + engine.outR));
		}
		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[HIT_LIGHT].setBrightnessSmooth(engine.meter, dt, 30.f);
		}
	}

	void setAll(std::initializer_list<std::pair<int, float>> v) {
		// start from the Gongoozler tam-tam marks, then apply the changes
		const std::pair<int, float> base[] = {
			{TYPE_PARAM, 1.f}, {SIZE_PARAM, 0.5511f}, {THICK_PARAM, 0.45f}, {LAW_PARAM, 0.f}, {ORDER_PARAM, 0.f},
			{DENSITY_PARAM, 1.f}, {BEAT_PARAM, 0.4f}, {POSITION_PARAM, 0.4f}, {MALLET_PARAM, 0.7f}, {BOW_PARAM, 0.f},
			{DECAY_PARAM, 0.65f}, {TILT_PARAM, 0.35f}, {GLIDE_PARAM, 0.f}, {ROAR_PARAM, 0.35f}, {BLOOM_PARAM, 0.7f},
			{BUILD_PARAM, 0.5f}, {THRESH_PARAM, 0.f}, {TUNE_PARAM, 0.f}, {MUTE_PARAM, 0.f}, {PITCHED_PARAM, 0.f}};
		for (auto& p : base) params[p.first].setValue(p.second);
		for (auto& p : v) params[p.first].setValue(p.second);
	}
};

struct GonzoWidget : ModuleWidget {
	GonzoWidget(Gonzo* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gonzo.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Gonzo G;
		const int r1[8] = {G::TYPE_PARAM, G::SIZE_PARAM, G::THICK_PARAM, G::LAW_PARAM, G::ORDER_PARAM, G::DENSITY_PARAM, G::BEAT_PARAM, G::CAST_PARAM};
		for (int i = 0; i < 8; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(gcol(i), GROW_GONG)), module, r1[i]));
		const int r2[7] = {G::POSITION_PARAM, G::MALLET_PARAM, G::BOW_PARAM, G::DECAY_PARAM, G::TILT_PARAM, G::GLIDE_PARAM, G::ROAR_PARAM};
		for (int i = 0; i < 7; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(gcol(i), GROW_STRIKE)), module, r2[i]));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(gcol(7), GROW_STRIKE)), module, G::HIT_PARAM, G::HIT_LIGHT));
		const int r3[7] = {G::BLOOM_PARAM, G::BUILD_PARAM, G::THRESH_PARAM, G::TUNE_PARAM, G::MUTE_PARAM, G::WIDTH_PARAM, G::LEVEL_PARAM};
		for (int i = 0; i < 7; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(gcol(i), GROW_CASC)), module, r3[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(gcol(7), GROW_CASC)), module, G::PITCHED_PARAM));

		const int cv[8] = {G::VOCT_INPUT, G::SIZE_INPUT, G::LAW_INPUT, G::ORDER_INPUT, G::POSITION_INPUT, G::MALLET_INPUT, G::BOW_INPUT, G::DECAY_INPUT};
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(gcol(i), GROW_CV)), module, cv[i]));
		const int io[6] = {G::BLOOM_INPUT, G::GLIDE_INPUT, G::TRIG_INPUT, G::VEL_INPUT, G::HAND_INPUT, G::EXCITE_INPUT};
		for (int i = 0; i < 6; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(gcol(i), GROW_IO)), module, io[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gcol(6), GROW_IO)), module, G::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gcol(7), GROW_IO)), module, G::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Gonzo* m = dynamic_cast<Gonzo*>(this->module);
		if (!m) return;
		typedef Gonzo G;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Starting points"));
		menu->addChild(createMenuItem("Gongoozler tam-tam (every knob on its mark)", "", [=]() { m->setAll({}); }));
		menu->addChild(createMenuItem("Sinking sun (bright hit collapses into sub)", "", [=]() {
			m->setAll({{G::MALLET_PARAM, 0.05f}, {G::BLOOM_PARAM, -0.85f}, {G::BUILD_PARAM, 0.7f}, {G::THRESH_PARAM, -1.f}, {G::DECAY_PARAM, 0.9f}, {G::TILT_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Glass organ (harmonic, play it with V/OCT)", "", [=]() {
			m->setAll({{G::ORDER_PARAM, -1.f}, {G::PITCHED_PARAM, 1.f}, {G::DENSITY_PARAM, 0.4f}, {G::DECAY_PARAM, 1.2f}, {G::TILT_PARAM, -0.3f},
			           {G::MALLET_PARAM, 0.3f}, {G::BLOOM_PARAM, 0.2f}, {G::ROAR_PARAM, 0.1f}, {G::BEAT_PARAM, 0.15f}});
		}));
		menu->addChild(createMenuItem("Bowed sheet, 8 m (drones without hits)", "", [=]() {
			m->setAll({{G::SIZE_PARAM, 0.867f}, {G::BOW_PARAM, 0.5f}, {G::DECAY_PARAM, 1.1f}, {G::ROAR_PARAM, 1.2f}, {G::BLOOM_PARAM, 0.6f}, {G::BUILD_PARAM, 1.0f}});
		}));
		menu->addChild(createMenuItem("Laser gong (opera, huge glide)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 3.f}, {G::GLIDE_PARAM, 30.f}, {G::MALLET_PARAM, -0.3f}, {G::DECAY_PARAM, 0.25f}, {G::POSITION_PARAM, 0.05f},
			           {G::BLOOM_PARAM, 0.3f}, {G::ROAR_PARAM, 0.2f}, {G::TUNE_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Crotale rain (6 cm, few modes, glassy)", "", [=]() {
			m->setAll({{G::SIZE_PARAM, 0.159f}, {G::DENSITY_PARAM, 0.15f}, {G::LAW_PARAM, 0.68f}, {G::TILT_PARAM, -0.5f}, {G::MALLET_PARAM, 0.1f},
			           {G::DECAY_PARAM, 0.9f}, {G::BLOOM_PARAM, 0.2f}});
		}));
		menu->addChild(createMenuItem("Whale (15 m, squeezed modes, bowed, sinking)", "", [=]() {
			m->setAll({{G::SIZE_PARAM, 0.958f}, {G::LAW_PARAM, -1.f}, {G::BOW_PARAM, 0.3f}, {G::BLOOM_PARAM, -0.5f}, {G::DECAY_PARAM, 1.1f}, {G::ROAR_PARAM, 0.9f}});
		}));
		menu->addChild(createMenuItem("Resonator (patch drums into EXCITE)", "", [=]() {
			m->setAll({{G::DECAY_PARAM, 0.45f}, {G::BLOOM_PARAM, 0.3f}, {G::POSITION_PARAM, 0.6f}, {G::ROAR_PARAM, 0.2f}, {G::TILT_PARAM, 0.5f}});
		}));
	}
};

Model* modelGonzo = createModel<Gonzo, GonzoWidget>("Gonzo");
