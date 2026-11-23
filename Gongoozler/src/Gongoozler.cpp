// Gongoozler: a gong and tam-tam model.
// The sound engine is in GongEngine.hpp, with notes on how it works.
#include "plugin.hpp"
#include "GongEngine.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COL[6] = {12.0f, 35.6f, 59.2f, 82.8f, 106.4f, 130.0f};
static const float ROW_GONG = 30.0f, ROW_STRIKE = 53.0f, ROW_OUT = 75.0f, ROW_CV = 96.0f, ROW_IO = 114.0f;

static const char* TYPE_NAMES[4] = {"Tam-tam", "Wind gong", "Opera gong", "Bossed gong"};

struct Gongoozler : Module {
	enum ParamIds {
		TYPE_PARAM,
		SIZE_PARAM,
		THICK_PARAM,
		BEAT_PARAM,
		GLIDE_PARAM,
		CAST_PARAM,
		POSITION_PARAM,
		MALLET_PARAM,
		DECAY_PARAM,
		DAMP_PARAM,
		BLOOM_PARAM,
		BUILD_PARAM,
		TUNE_PARAM,
		ROAR_PARAM,
		MUTE_PARAM,
		WIDTH_PARAM,
		LEVEL_PARAM,
		HIT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT,
		POSITION_INPUT,
		MALLET_INPUT,
		DECAY_INPUT,
		BLOOM_INPUT,
		GLIDE_INPUT,
		TRIG_INPUT,
		VEL_INPUT,
		HAND_INPUT,
		MUTE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		HIT_LIGHT,
		NUM_LIGHTS
	};

	gong::Engine engine;
	gong::Controls ctl;
	dsp::SchmittTrigger trigIn, handIn, hitButton;
	dsp::ClockDivider lightDivider;
	bool followPitch = true;

	struct TypeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int t = clamp((int) std::round(getValue()) - 1, 0, 3);
			return string::f("%d: %s", t + 1, TYPE_NAMES[t]);
		}
	};
	struct CastQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			return string::f("Gong no. %d", (int) std::round(getValue()));
		}
	};
	struct PositionQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			const char* w = v < 0.15f ? "centre / boss" : v < 0.6f ? "middle" : v < 0.85f ? "outer" : "edge";
			return string::f("%.0f %% (%s)", v * 100.f, w);
		}
	};

	Gongoozler() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<TypeQuantity>(TYPE_PARAM, 1.f, 4.f, 1.f, "Type");
		getParamQuantity(TYPE_PARAM)->snapEnabled = true;
		configParam(SIZE_PARAM, 20.f, 150.f, 90.f, "Size (diameter)", " cm");
		configParam(THICK_PARAM, 0.f, 1.f, 0.45f, "Thickness (thin: blooms easily; thick: needs a hard hit)", "%", 0.f, 100.f);
		configParam(BEAT_PARAM, 0.f, 1.f, 0.4f, "Beat (slow beating of paired tones; ombak on bossed gongs)", "%", 0.f, 100.f);
		configParam(GLIDE_PARAM, -7.f, 7.f, 0.f, "Glide (pitch bend at a hard hit; + falls back, - rises back)", " semitones");
		configParam<CastQuantity>(CAST_PARAM, 1.f, 16.f, 1.f, "Cast (which individual gong)");
		getParamQuantity(CAST_PARAM)->snapEnabled = true;
		configParam<PositionQuantity>(POSITION_PARAM, 0.f, 1.f, 0.4f, "Strike position (centre <-> edge)");
		configParam(MALLET_PARAM, 0.f, 1.f, 0.7f, "Mallet (hard stick <-> big soft beater)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.65f, "Decay", "%", 0.f, 100.f);
		configParam(DAMP_PARAM, 0.f, 1.f, 0.35f, "Damp (bright ring <-> dark)", "%", 0.f, 100.f);
		configParam(BLOOM_PARAM, 0.f, 1.f, 0.7f, "Bloom (hard hits climb into a shimmering wash)", "%", 0.f, 100.f);
		configParam(BUILD_PARAM, 0.f, 1.f, 0.5f, "Build (how slowly the bloom climbs)", "%", 0.f, 100.f);
		configParam(TUNE_PARAM, -24.f, 24.f, 0.f, "Tune", " semitones");
		configParam(ROAR_PARAM, 0.f, 1.f, 0.35f, "Roar (the plate stretching: growl and downward sweep on hard hits)", "%", 0.f, 100.f);
		configParam(MUTE_PARAM, 0.f, 1.f, 0.f, "Mute (a towel on the gong)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.7f, "Stereo width", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f);
		configButton(HIT_PARAM, "Hit");

		configInput(VOCT_INPUT, "V/OCT (tune; on tuned gongs 0 V = C4, see right-click menu)");
		configInput(POSITION_INPUT, "Position CV (+-5 V = full range, read at each hit)");
		configInput(MALLET_INPUT, "Mallet CV (+-5 V = full range, read at each hit)");
		configInput(DECAY_INPUT, "Decay CV (+-5 V = full range)");
		configInput(BLOOM_INPUT, "Bloom CV (+-5 V = full range)");
		configInput(GLIDE_INPUT, "Glide CV (1 V = 1 semitone, added to the knob)");
		configInput(TRIG_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Velocity (0-10 V, read at each trigger; unpatched = 7.5 V)");
		configInput(HAND_INPUT, "Hand (a trigger grabs the gong for 200 ms; a held gate keeps holding it)");
		configInput(MUTE_INPUT, "Mute CV (0-10 V, added to the MUTE knob)");
		configOutput(LEFT_OUTPUT, "Left (mono when RIGHT is unpatched)");
		configOutput(RIGHT_OUTPUT, "Right");

		lightDivider.setDivision(256);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "followPitch", json_boolean(followPitch));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* j = json_object_get(root, "followPitch");
		if (j) followPitch = json_boolean_value(j);
	}

	static float cv5(Input& in) { return in.getVoltage() * 0.1f; }

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);

		ctl.type = clamp((int) std::round(params[TYPE_PARAM].getValue()) - 1, 0, 3);
		ctl.size = params[SIZE_PARAM].getValue();
		ctl.thick = params[THICK_PARAM].getValue();
		ctl.beat = params[BEAT_PARAM].getValue();
		ctl.glide = clamp(params[GLIDE_PARAM].getValue() + inputs[GLIDE_INPUT].getVoltage(), -12.f, 12.f);
		ctl.cast = clamp((int) std::round(params[CAST_PARAM].getValue()) - 1, 0, 15);
		ctl.position = clamp(params[POSITION_PARAM].getValue() + cv5(inputs[POSITION_INPUT]), 0.f, 1.f);
		ctl.mallet = clamp(params[MALLET_PARAM].getValue() + cv5(inputs[MALLET_INPUT]), 0.f, 1.f);
		ctl.decay = clamp(params[DECAY_PARAM].getValue() + cv5(inputs[DECAY_INPUT]), 0.f, 1.f);
		ctl.damp = params[DAMP_PARAM].getValue();
		ctl.bloom = clamp(params[BLOOM_PARAM].getValue() + cv5(inputs[BLOOM_INPUT]), 0.f, 1.f);
		ctl.build = params[BUILD_PARAM].getValue();
		ctl.tune = params[TUNE_PARAM].getValue() / 12.f + clamp(inputs[VOCT_INPUT].getVoltage(), -5.f, 5.f);
		ctl.followPitch = followPitch;
		ctl.roar = params[ROAR_PARAM].getValue();
		ctl.mute = clamp(params[MUTE_PARAM].getValue() + inputs[MUTE_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		ctl.width = params[WIDTH_PARAM].getValue();
		ctl.level = params[LEVEL_PARAM].getValue();
		ctl.hand = inputs[HAND_INPUT].getVoltage() > 1.f;

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

	// right-click starting points
	void setAll(std::initializer_list<std::pair<int, float>> v) {
		for (auto& p : v) params[p.first].setValue(p.second);
	}
};

struct GongoozlerWidget : ModuleWidget {
	GongoozlerWidget(Gongoozler* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gongoozler.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Gongoozler G;
		const int gongRow[6] = {G::TYPE_PARAM, G::SIZE_PARAM, G::THICK_PARAM, G::BEAT_PARAM, G::GLIDE_PARAM, G::CAST_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_GONG)), module, gongRow[i]));

		const int strike[6] = {G::POSITION_PARAM, G::MALLET_PARAM, G::DECAY_PARAM, G::DAMP_PARAM, G::BLOOM_PARAM, G::BUILD_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_STRIKE)), module, strike[i]));

		const int outRow[5] = {G::TUNE_PARAM, G::ROAR_PARAM, G::MUTE_PARAM, G::WIDTH_PARAM, G::LEVEL_PARAM};
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_OUT)), module, outRow[i]));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(COL[5], ROW_OUT)), module, G::HIT_PARAM, G::HIT_LIGHT));

		const int cvIns[6] = {G::VOCT_INPUT, G::POSITION_INPUT, G::MALLET_INPUT, G::DECAY_INPUT, G::BLOOM_INPUT, G::GLIDE_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_CV)), module, cvIns[i]));

		const int ioIns[4] = {G::TRIG_INPUT, G::VEL_INPUT, G::HAND_INPUT, G::MUTE_INPUT};
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_IO)), module, ioIns[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[4], ROW_IO)), module, G::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], ROW_IO)), module, G::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Gongoozler* m = dynamic_cast<Gongoozler*>(this->module);
		if (!m)
			return;
		typedef Gongoozler G;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Opera and bossed gongs follow V/OCT (0 V = C4)", "", &m->followPitch));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Starting points"));
		menu->addChild(createMenuItem("Tam-tam, 36\" (soft beater, big bloom)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 1.f}, {G::SIZE_PARAM, 90.f}, {G::THICK_PARAM, 0.45f}, {G::BEAT_PARAM, 0.4f}, {G::GLIDE_PARAM, 0.f},
			           {G::POSITION_PARAM, 0.4f}, {G::MALLET_PARAM, 0.75f}, {G::DECAY_PARAM, 0.7f}, {G::DAMP_PARAM, 0.35f},
			           {G::BLOOM_PARAM, 0.7f}, {G::BUILD_PARAM, 0.5f}, {G::TUNE_PARAM, 0.f}, {G::ROAR_PARAM, 0.35f}, {G::MUTE_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Wind gong, 24\" (fast roaring wash)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 2.f}, {G::SIZE_PARAM, 60.f}, {G::THICK_PARAM, 0.3f}, {G::BEAT_PARAM, 0.4f}, {G::GLIDE_PARAM, 0.f},
			           {G::POSITION_PARAM, 0.55f}, {G::MALLET_PARAM, 0.55f}, {G::DECAY_PARAM, 0.55f}, {G::DAMP_PARAM, 0.3f},
			           {G::BLOOM_PARAM, 0.85f}, {G::BUILD_PARAM, 0.3f}, {G::TUNE_PARAM, 0.f}, {G::ROAR_PARAM, 0.45f}, {G::MUTE_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Opera gong, large (pitch falls)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 3.f}, {G::SIZE_PARAM, 32.f}, {G::THICK_PARAM, 0.5f}, {G::BEAT_PARAM, 0.2f}, {G::GLIDE_PARAM, 2.5f},
			           {G::POSITION_PARAM, 0.1f}, {G::MALLET_PARAM, 0.4f}, {G::DECAY_PARAM, 0.4f}, {G::DAMP_PARAM, 0.45f},
			           {G::BLOOM_PARAM, 0.3f}, {G::BUILD_PARAM, 0.3f}, {G::TUNE_PARAM, -7.f}, {G::ROAR_PARAM, 0.2f}, {G::MUTE_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Opera gong, small (pitch rises)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 3.f}, {G::SIZE_PARAM, 22.f}, {G::THICK_PARAM, 0.5f}, {G::BEAT_PARAM, 0.2f}, {G::GLIDE_PARAM, -2.5f},
			           {G::POSITION_PARAM, 0.1f}, {G::MALLET_PARAM, 0.2f}, {G::DECAY_PARAM, 0.3f}, {G::DAMP_PARAM, 0.4f},
			           {G::BLOOM_PARAM, 0.2f}, {G::BUILD_PARAM, 0.3f}, {G::TUNE_PARAM, 7.f}, {G::ROAR_PARAM, 0.15f}, {G::MUTE_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Gong ageng (big gamelan gong, slow beating)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 4.f}, {G::SIZE_PARAM, 90.f}, {G::THICK_PARAM, 0.7f}, {G::BEAT_PARAM, 0.45f}, {G::GLIDE_PARAM, 0.3f},
			           {G::POSITION_PARAM, 0.f}, {G::MALLET_PARAM, 0.85f}, {G::DECAY_PARAM, 0.8f}, {G::DAMP_PARAM, 0.55f},
			           {G::BLOOM_PARAM, 0.2f}, {G::BUILD_PARAM, 0.6f}, {G::TUNE_PARAM, -24.f}, {G::ROAR_PARAM, 0.15f}, {G::MUTE_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Kempul / Thai gong (small bossed gong)", "", [=]() {
			m->setAll({{G::TYPE_PARAM, 4.f}, {G::SIZE_PARAM, 50.f}, {G::THICK_PARAM, 0.6f}, {G::BEAT_PARAM, 0.3f}, {G::GLIDE_PARAM, 0.f},
			           {G::POSITION_PARAM, 0.f}, {G::MALLET_PARAM, 0.7f}, {G::DECAY_PARAM, 0.6f}, {G::DAMP_PARAM, 0.5f},
			           {G::BLOOM_PARAM, 0.15f}, {G::BUILD_PARAM, 0.5f}, {G::TUNE_PARAM, -12.f}, {G::ROAR_PARAM, 0.1f}, {G::MUTE_PARAM, 0.f}});
		}));
	}
};

Model* modelGongoozler = createModel<Gongoozler, GongoozlerWidget>("Gongoozler");
