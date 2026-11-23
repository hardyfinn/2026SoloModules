// Rimbaud: a physical model of a snare drum with its rim.
// The sound engine is in RimbaudEngine.hpp, with notes on how it works.
#include "plugin.hpp"
#include "RimbaudEngine.hpp"
#include "RimbaudPresets.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COL[6] = {16.2f, 40.6f, 65.0f, 89.4f, 113.8f, 138.2f};
static const float ROW_DRUM = 31.0f, ROW_STROKE = 53.0f, ROW_SNARE = 75.0f, ROW_CV = 96.0f, ROW_IO = 115.0f;
static const float IOX[9] = {11.0f, 26.8f, 42.6f, 58.4f, 74.2f, 90.0f, 109.0f, 124.8f, 140.6f};
static const float HIT_X = 138.2f, HIT_Y = 10.0f;

struct Rimbaud : Module {
	enum ParamIds {
		SIZE_PARAM, DEPTH_PARAM, TUNE_PARAM, RESO_PARAM, DAMP_PARAM, SHELL_PARAM,
		POSITION_PARAM, STICK_PARAM, BUZZ_PARAM, RIM_PARAM, HOOP_PARAM, XSTICK_PARAM,
		SNARES_PARAM, TENSION_PARAM, WIRES_PARAM, SYMP_PARAM, MIC_PARAM, LEVEL_PARAM,
		HIT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, POSITION_INPUT, STICK_INPUT, BUZZ_INPUT, TENSION_INPUT, DAMP_INPUT,
		HIT_INPUT, RIM_INPUT, XSTICK_INPUT, VEL_INPUT, SNARES_INPUT, EXT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { TOP_OUTPUT, BOTTOM_OUTPUT, MIX_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HIT_LIGHT, NUM_LIGHTS };

	rimbaud::Engine engine;
	rimbaud::Controls ctl;
	dsp::SchmittTrigger hitIn, rimIn, xIn, hitButton;
	dsp::ClockDivider lightDivider;

	struct PositionQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			const char* w = v < 0.15f ? "centre" : v < 0.45f ? "between centre and edge" : v < 0.8f ? "towards the edge" : "edge";
			return string::f("%.0f %% (%s)", v * 100.f, w);
		}
	};

	Rimbaud() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SIZE_PARAM, 10.f, 15.f, 14.f, "Size (head diameter)", "\"");
		configParam(DEPTH_PARAM, 3.f, 8.f, 6.5f, "Depth (shell)", "\"");
		configParam(TUNE_PARAM, std::log2(90.f), std::log2(520.f), std::log2(240.f), "Tune (batter head tension, as its own pitch)", " Hz", 2.f);
		configParam(RESO_PARAM, 0.8f, 2.2f, 1.5f, "Reso (snare-side head pitch, relative to the batter)", "x");
		configParam(DAMP_PARAM, 0.f, 1.f, 0.15f, "Damp (gel / muffling near the edge)", "%", 0.f, 100.f);
		configParam(SHELL_PARAM, 0.f, 1.f, 0.2f, "Shell (wood <-> metal)", "%", 0.f, 100.f);
		configParam<PositionQuantity>(POSITION_PARAM, 0.f, 1.f, 0.3f, "Position (where the stick lands)");
		configParam(STICK_PARAM, 0.f, 1.f, 0.15f, "Stick (hard tip <-> felt mallet)", "%", 0.f, 100.f);
		configParam(BUZZ_PARAM, 0.f, 1.f, 0.f, "Buzz (hand presses the stick into the head: press / buzz stroke)", "%", 0.f, 100.f);
		configParam(RIM_PARAM, 0.f, 1.f, 0.5f, "Rim (rimshot: share of the stick on the hoop)", "%", 0.f, 100.f);
		configParam(HOOP_PARAM, 0.f, 1.f, 0.3f, "Hoop (triple-flanged steel <-> die-cast)", "%", 0.f, 100.f);
		configParam(XSTICK_PARAM, 0.f, 1.f, 0.5f, "Cross-stick (short overhang: click <-> long: knock)", "%", 0.f, 100.f);
		configSwitch(SNARES_PARAM, 0.f, 1.f, 1.f, "Snares", {"off", "on"});
		configParam(TENSION_PARAM, 0.f, 1.f, 0.5f, "Tension (snare strainer: loose and long <-> tight and crisp)", "%", 0.f, 100.f);
		configParam(WIRES_PARAM, 0.f, 1.f, 0.5f, "Wires (12 <-> 42 strands)", "%", 0.f, 100.f);
		configParam(SYMP_PARAM, 0.f, 1.f, 0.5f, "Sympathy (how hard EXT pushes on the heads)", "%", 0.f, 100.f);
		configParam(MIC_PARAM, 0.f, 1.f, 0.3f, "Mic (MIX output: top mic <-> bottom mic)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configButton(HIT_PARAM, "Hit");

		configInput(VOCT_INPUT, "V/OCT (tune)");
		configInput(POSITION_INPUT, "Position CV (+-5 V = full range, read at each hit)");
		configInput(STICK_INPUT, "Stick CV (+-5 V = full range, read at each hit)");
		configInput(BUZZ_INPUT, "Buzz CV (+-5 V = full range, read at each hit)");
		configInput(TENSION_INPUT, "Snare tension CV (+-5 V = full range)");
		configInput(DAMP_INPUT, "Damp CV (+-5 V = full range)");
		configInput(HIT_INPUT, "Hit (stick on the head)");
		configInput(RIM_INPUT, "Rimshot (tip on the head and shaft on the hoop together)");
		configInput(XSTICK_INPUT, "Cross-stick (stick lying on the head, butt end on the hoop)");
		configInput(VEL_INPUT, "Velocity (0-10 V, read at each trigger; unpatched = 7.5 V)");
		configInput(SNARES_INPUT, "Snares gate (when patched: high = snares on, low = off)");
		configInput(EXT_INPUT, "EXT (outside sound pushing on the heads: sympathetic snare buzz)");
		configOutput(TOP_OUTPUT, "Top mic (over the batter head)");
		configOutput(BOTTOM_OUTPUT, "Bottom mic (under the snares)");
		configOutput(MIX_OUTPUT, "Mix (top/bottom by MIC, plus the drum in the room)");

		lightDivider.setDivision(256);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine.reset();
	}

	static float cv5(Input& in) { return in.getVoltage() * 0.1f; }

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);

		ctl.size = params[SIZE_PARAM].getValue();
		ctl.depth = params[DEPTH_PARAM].getValue();
		float tune = params[TUNE_PARAM].getValue() + inputs[VOCT_INPUT].getVoltage();
		ctl.tuneHz = clamp(std::exp2(tune), 30.f, 2000.f);
		ctl.reso = params[RESO_PARAM].getValue();
		ctl.damp = clamp(params[DAMP_PARAM].getValue() + cv5(inputs[DAMP_INPUT]), 0.f, 1.f);
		ctl.shell = params[SHELL_PARAM].getValue();
		ctl.position = clamp(params[POSITION_PARAM].getValue() + cv5(inputs[POSITION_INPUT]), 0.f, 1.f);
		ctl.stick = clamp(params[STICK_PARAM].getValue() + cv5(inputs[STICK_INPUT]), 0.f, 1.f);
		ctl.buzz = clamp(params[BUZZ_PARAM].getValue() + cv5(inputs[BUZZ_INPUT]), 0.f, 1.f);
		ctl.rim = params[RIM_PARAM].getValue();
		ctl.hoop = params[HOOP_PARAM].getValue();
		ctl.xstick = params[XSTICK_PARAM].getValue();
		if (inputs[SNARES_INPUT].isConnected())
			ctl.snares = inputs[SNARES_INPUT].getVoltage() > 1.f;
		else
			ctl.snares = params[SNARES_PARAM].getValue() > 0.5f;
		ctl.tension = clamp(params[TENSION_PARAM].getValue() + cv5(inputs[TENSION_INPUT]), 0.f, 1.f);
		ctl.wires = params[WIRES_PARAM].getValue();
		ctl.symp = params[SYMP_PARAM].getValue();
		ctl.mic = params[MIC_PARAM].getValue();
		ctl.level = params[LEVEL_PARAM].getValue();

		float vel = 0.75f;
		if (inputs[VEL_INPUT].isConnected())
			vel = clamp(inputs[VEL_INPUT].getVoltage(), 0.f, 12.f) * 0.1f;
		bool button = hitButton.process(params[HIT_PARAM].getValue());
		if (hitIn.process(inputs[HIT_INPUT].getVoltage(), 0.1f, 1.f))
			engine.hit(vel, rimbaud::HEAD, ctl);
		else if (button)
			engine.hit(0.75f, rimbaud::HEAD, ctl);
		if (rimIn.process(inputs[RIM_INPUT].getVoltage(), 0.1f, 1.f))
			engine.hit(vel, rimbaud::RIMSHOT, ctl);
		if (xIn.process(inputs[XSTICK_INPUT].getVoltage(), 0.1f, 1.f))
			engine.hit(vel, rimbaud::CROSS, ctl);

		// EXT: +-5 V of outside sound -> up to ~120 Pa on the heads (SYMP squared for a gentle start)
		float ext = 0.f;
		if (inputs[EXT_INPUT].isConnected())
			ext = inputs[EXT_INPUT].getVoltage() * 24.f * ctl.symp * ctl.symp;

		engine.process(ctl, ext);
		outputs[TOP_OUTPUT].setVoltage(engine.outTop);
		outputs[BOTTOM_OUTPUT].setVoltage(engine.outBot);
		outputs[MIX_OUTPUT].setVoltage(engine.outMix);

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[HIT_LIGHT].setBrightnessSmooth(engine.meter, dt, 30.f);
		}
	}

	// right-click presets (SYMP and LEVEL are left alone)
	void loadPreset(const rimbaud::Preset& p) {
		params[SIZE_PARAM].setValue(p.size);
		params[DEPTH_PARAM].setValue(p.depth);
		params[TUNE_PARAM].setValue(std::log2(p.tuneHz));
		params[RESO_PARAM].setValue(p.reso);
		params[DAMP_PARAM].setValue(p.damp);
		params[SHELL_PARAM].setValue(p.shell);
		params[POSITION_PARAM].setValue(p.position);
		params[STICK_PARAM].setValue(p.stick);
		params[BUZZ_PARAM].setValue(p.buzz);
		params[RIM_PARAM].setValue(p.rim);
		params[HOOP_PARAM].setValue(p.hoop);
		params[XSTICK_PARAM].setValue(p.xstick);
		params[SNARES_PARAM].setValue(p.snares);
		params[TENSION_PARAM].setValue(p.tension);
		params[WIRES_PARAM].setValue(p.wires);
		params[MIC_PARAM].setValue(p.mic);
	}
};

struct RimbaudWidget : ModuleWidget {
	RimbaudWidget(Rimbaud* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rimbaud.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Rimbaud R;
		const int drum[6] = {R::SIZE_PARAM, R::DEPTH_PARAM, R::TUNE_PARAM, R::RESO_PARAM, R::DAMP_PARAM, R::SHELL_PARAM};
		const int stroke[6] = {R::POSITION_PARAM, R::STICK_PARAM, R::BUZZ_PARAM, R::RIM_PARAM, R::HOOP_PARAM, R::XSTICK_PARAM};
		const int snare[5] = {R::TENSION_PARAM, R::WIRES_PARAM, R::SYMP_PARAM, R::MIC_PARAM, R::LEVEL_PARAM};
		for (int i = 0; i < 6; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_DRUM)), module, drum[i]));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_STROKE)), module, stroke[i]));
		}
		addParam(createParamCentered<CKSS>(mm2px(Vec(COL[0], ROW_SNARE)), module, R::SNARES_PARAM));
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i + 1], ROW_SNARE)), module, snare[i]));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(HIT_X, HIT_Y)), module, R::HIT_PARAM, R::HIT_LIGHT));

		const int cv[6] = {R::VOCT_INPUT, R::POSITION_INPUT, R::STICK_INPUT, R::BUZZ_INPUT, R::TENSION_INPUT, R::DAMP_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_CV)), module, cv[i]));
		const int io[6] = {R::HIT_INPUT, R::RIM_INPUT, R::XSTICK_INPUT, R::VEL_INPUT, R::SNARES_INPUT, R::EXT_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[i], ROW_IO)), module, io[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[6], ROW_IO)), module, R::TOP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[7], ROW_IO)), module, R::BOTTOM_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[8], ROW_IO)), module, R::MIX_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Rimbaud* m = dynamic_cast<Rimbaud*>(this->module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Presets (20)", "", [=](Menu* sub) {
			for (int i = 0; i < rimbaud::NUM_PRESETS; i++) {
				const rimbaud::Preset& p = rimbaud::PRESETS[i];
				sub->addChild(createMenuItem(string::f("%d. %s", i + 1, p.name), "", [=]() { m->loadPreset(p); }));
			}
		}));
	}
};

Model* modelRimbaud = createModel<Rimbaud, RimbaudWidget>("Rimbaud");
