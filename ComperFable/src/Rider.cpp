#include "plugin.hpp"
#include "dsp/RiderHost.hpp"
#include "LinkPort.hpp"

using namespace rider;

// ---- LAYOUT (same numbers in res/make_rider_panel.py) ----
static const float R_DISP_X = 4.0f, R_DISP_Y = 12.5f, R_DISP_W = 144.4f, R_DISP_H = 13.0f;
static const float R_COL_X0 = 9.0f, R_COL_DX = 14.9f;
static const float R_SND_X0 = 14.0f, R_SND_DX = 24.88f;
static const float R_IN_X0 = 9.0f, R_IN_DX = 22.35f;
static const float R_Y_PLAYER = 41.5f, R_Y_HANDS = 62.0f, R_Y_SOUND = 83.0f, R_Y_IN1 = 101.0f, R_Y_IN2 = 114.5f;
static float rcol(int i) { return R_COL_X0 + i * R_COL_DX; }
static float rscol(int i) { return R_SND_X0 + i * R_SND_DX; }
static float ricol(int i) { return R_IN_X0 + i * R_IN_DX; }

static const int R_PPB_CHOICES[4] = {1, 2, 4, 24};
static const int R_CHORUS_CHOICES[4] = {8, 12, 16, 32};

struct RiderModule : Module {
	enum ParamIds {
		FEEL_PARAM, ENERGY_PARAM, COMP_PARAM, LISTEN_PARAM, FILLS_PARAM, FEATHER_PARAM, REPEAT_PARAM, BAND_PARAM, SEED_PARAM, NEW_PARAM,
		LOOSE_PARAM, AHEAD_PARAM, RIDE_PARAM, HATS_PARAM, SNARE_PARAM, KICK_PARAM, TOMS_PARAM,
		TUNE_PARAM, DECAY_PARAM, BRIGHT_PARAM, WIDTH_PARAM, AGE_PARAM, LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds { CLOCK_INPUT, RESET_INPUT, ENERGY_INPUT, COMP_INPUT, FILLS_INPUT, FILL_INPUT, LAYOUT_INPUT, NUM_INPUTS };
	enum OutputIds { LEFT_OUTPUT, RIGHT_OUTPUT, KICK_OUTPUT, SNARE_OUTPUT, HAT_OUTPUT, RIDE_OUTPUT, CRASH_OUTPUT, TOM_OUTPUT, VEL_OUTPUT, ENERGY_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HIT_LIGHT, LINK_LIGHT, FILL_LIGHT, NUM_LIGHTS };

	RiderHost host;
	RIn in;
	ROut out;
	int ppbIndex = 0;
	int chorusBars = 12;
	dsp::BooleanTrigger newTrig;
	dsp::ClockDivider lightDiv;
	LinkPort port;
	bool linked = false;

	RiderModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(FEEL_PARAM, 0.f, 5.f, 0.f, "Feel", {"Follow Comper's groove", "Swing", "Bossa", "Ballad (brushes)", "Soul", "House"});
		configParam(ENERGY_PARAM, 0.f, 1.f, 0.5f, "Energy (how hard the band is playing: level, busyness, bombs, crashes)", "%", 0.f, 100.f);
		configParam(COMP_PARAM, 0.f, 1.f, 0.5f, "Comp (snare and bass drum conversation)", "%", 0.f, 100.f);
		configParam(LISTEN_PARAM, 0.f, 1.f, 1.f, "Listen (catch the pianist's pushes, leave room, answer in the gaps)", "%", 0.f, 100.f);
		configParam(FILLS_PARAM, 0.f, 1.f, 0.5f, "Fills (set-ups at the ends of phrases and into the top of the chorus)", "%", 0.f, 100.f);
		configParam(FEATHER_PARAM, 0.f, 1.f, 0.5f, "Feather (quiet bass drum on all four beats)", "%", 0.f, 100.f);
		configParam(REPEAT_PARAM, 0.f, 1.f, 0.6f, "Repeat (keep the two-bar comping figure)", "%", 0.f, 100.f);
		configParam(BAND_PARAM, 0.f, 1.f, 0.f, "Band (how far Comper and Walker follow ENERGY)", "%", 0.f, 100.f);
		configParam(SEED_PARAM, 0.f, 999.f, 1.f, "Seed (a new take at the next bar line)")->snapEnabled = true;
		configButton(NEW_PARAM, "New seed");
		configParam(LOOSE_PARAM, 0.f, 2.5f, 1.f, "Loose (0 machine, 1 a good drummer, 2.5 drunk)", "%", 0.f, 100.f);
		configParam(AHEAD_PARAM, 0.f, 1.f, 0.5f, "Ahead (0 lays back, 1 plays on top of the beat)", "%", 0.f, 100.f);
		configParam(RIDE_PARAM, 0.f, 2.f, 1.f, "Level of the cymbals", "%", 0.f, 100.f);
		configParam(HATS_PARAM, 0.f, 2.f, 1.f, "Level of the hi-hats", "%", 0.f, 100.f);
		configParam(SNARE_PARAM, 0.f, 2.f, 1.f, "Level of the snare and brushes", "%", 0.f, 100.f);
		configParam(KICK_PARAM, 0.f, 2.f, 1.f, "Level of the bass drum", "%", 0.f, 100.f);
		configParam(TOMS_PARAM, 0.f, 2.f, 1.f, "Level of the toms", "%", 0.f, 100.f);
		configParam(TUNE_PARAM, 0.f, 1.f, 0.5f, "Tune (drums, five semitones either way)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay (how long cymbals and hi-hats ring)", "%", 0.f, 100.f);
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.5f, "Bright (dark to bright)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, 1.f, "Width (mono to the full kit)", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.f, "Age (old kit: darker cymbals, dead heads, slack tuning, uneven strokes)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f);
		for (int id : {(int)SEED_PARAM, (int)NEW_PARAM, (int)LEVEL_PARAM}) getParamQuantity(id)->randomizeEnabled = false;

		configInput(CLOCK_INPUT, "Clock (pulses per beat in the right-click menu; used without Comper)");
		configInput(RESET_INPUT, "Reset (without Comper)");
		configInput(ENERGY_INPUT, "ENERGY CV (0.1 per volt)");
		configInput(COMP_INPUT, "COMP CV (0.1 per volt)");
		configInput(FILLS_INPUT, "FILLS CV (0.1 per volt)");
		configInput(FILL_INPUT, "Fill (trigger: a set-up in the next bar)");
		configInput(LAYOUT_INPUT, "Lay out (gate: the drummer stops playing while high)");
		configOutput(LEFT_OUTPUT, "Kit left");
		configOutput(RIGHT_OUTPUT, "Kit right");
		configOutput(KICK_OUTPUT, "Bass drum trigger");
		configOutput(SNARE_OUTPUT, "Snare trigger (snare, cross-stick, brush taps)");
		configOutput(HAT_OUTPUT, "Hi-hat trigger (foot, closed, open)");
		configOutput(RIDE_OUTPUT, "Ride trigger (ride and bell)");
		configOutput(CRASH_OUTPUT, "Crash trigger");
		configOutput(TOM_OUTPUT, "Tom trigger");
		configOutput(VEL_OUTPUT, "Velocities (poly, 6 channels: kick, snare, hat, ride, crash, tom; 0 to 10 V)");
		configOutput(ENERGY_OUTPUT, "Energy in use (0 to 10 V)");
		configLight(HIT_LIGHT, "Stroke played");
		configLight(LINK_LIGHT, "Comper in the row");
		configLight(FILL_LIGHT, "Playing a set-up");
		lightDiv.setDivision(64);

		port.attach(this);
		host.setSampleRate(APP->engine->getSampleRate());
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { host.setSampleRate(e.sampleRate); }
	void onReset() override {
		ppbIndex = 0; chorusBars = 12;
		double sr = host.sr; host = RiderHost(); host.setSampleRate(sr);
	}

	float cv(int id) { return inputs[id].isConnected() ? 0.1f * inputs[id].getVoltage() : 0.f; }

	void process(const ProcessArgs& args) override {
		if (newTrig.process(params[NEW_PARAM].getValue() > 0.f))
			params[SEED_PARAM].setValue((float)(random::u32() % 1000));

		port.read(this);
		in.link = port.merged();
		linked = in.link.hasComp;

		in.clockConnected = inputs[CLOCK_INPUT].isConnected(); in.clock = inputs[CLOCK_INPUT].getVoltage();
		in.ppb = R_PPB_CHOICES[clamp(ppbIndex, 0, 3)];
		in.reset = inputs[RESET_INPUT].getVoltage();
		in.fill = inputs[FILL_INPUT].getVoltage();
		in.layout = inputs[LAYOUT_INPUT].getVoltage();
		in.chorusBars = chorusBars;

		in.feelPos = clamp((int)std::round(params[FEEL_PARAM].getValue()), 0, 5);
		RP& p = in.rp;
		p.energy = clamp(params[ENERGY_PARAM].getValue() + cv(ENERGY_INPUT), 0.f, 1.f);
		p.comp = clamp(params[COMP_PARAM].getValue() + cv(COMP_INPUT), 0.f, 1.f);
		p.listen = params[LISTEN_PARAM].getValue();
		p.fills = clamp(params[FILLS_PARAM].getValue() + cv(FILLS_INPUT), 0.f, 1.f);
		p.feather = params[FEATHER_PARAM].getValue();
		p.repeat = params[REPEAT_PARAM].getValue();
		p.loose = params[LOOSE_PARAM].getValue();
		p.ahead = params[AHEAD_PARAM].getValue();
		in.seed = (int)params[SEED_PARAM].getValue();
		riderkit::KitParams& k = in.kit;
		for (int i = 0; i < 5; i++) k.mix[i] = params[RIDE_PARAM + i].getValue();
		k.tune = params[TUNE_PARAM].getValue();
		k.decay = params[DECAY_PARAM].getValue();
		k.bright = params[BRIGHT_PARAM].getValue();
		k.width = params[WIDTH_PARAM].getValue();
		k.age = params[AGE_PARAM].getValue();
		k.level = params[LEVEL_PARAM].getValue();

		host.process(in, out, args.sampleTime);
		{	// tell the band how hard we are playing
			CompLink m; m.hasDrum = true;
			m.bandX = clamp(params[BAND_PARAM].getValue() * 2.f * ((float)p.energy - 0.5f), -1.f, 1.f);
			port.send(this, m);
		}

		outputs[LEFT_OUTPUT].setVoltage(out.L);
		outputs[RIGHT_OUTPUT].setVoltage(out.R);
		for (int g = 0; g < NUM_TG; g++) outputs[KICK_OUTPUT + g].setVoltage(out.trig[g]);
		outputs[VEL_OUTPUT].setChannels(NUM_TG);
		for (int g = 0; g < NUM_TG; g++) outputs[VEL_OUTPUT].setVoltage(out.vel[g], g);
		outputs[ENERGY_OUTPUT].setVoltage(out.energy);

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[HIT_LIGHT].setBrightnessSmooth(host.t - host.hitAt < 0.05 ? 1.f : 0.f, dt);
			lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
			lights[FILL_LIGHT].setBrightnessSmooth(host.fillNow() ? 1.f : 0.f, dt);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppb", json_integer(ppbIndex));
		json_object_set_new(root, "chorusBars", json_integer(chorusBars));
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppb")) ppbIndex = clamp((int)json_integer_value(j), 0, 3);
		if (json_t* j = json_object_get(root, "chorusBars")) {
			int v = (int)json_integer_value(j); chorusBars = 12;
			for (int c : R_CHORUS_CHOICES) if (c == v) chorusBars = v;
		}
	}
};

// ---------------------------------------------------------------- display
static NVGcolor R_AMBER() { return nvgRGB(0xF2, 0xC2, 0x5C); }
static NVGcolor R_DIM() { return nvgRGB(0x8A, 0x7A, 0x55); }

struct RiderDisplay : TransparentWidget {
	RiderModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		std::string feel = "SWING", bar = "BAR 7/12", ev = "CAUGHT", src = "LINK", seed = "1";
		if (module) {
			RiderHost& h = module->host;
			src = RSOURCE_NAMES[std::max(0, std::min(2, h.source))];
			feel = FEEL_NAMES[h.wantFeel(module->in)];
			if (module->in.feelPos == 0 && h.source == RS_LINK) feel += "*";
			if (h.source == RS_IDLE) { bar = "next to Comper,"; ev = "or patch CLOCK"; }
			else {
				bar = h.running ? "BAR " + std::to_string(h.barInChorus()) + "/" + std::to_string(h.chorusNow) : "STOPPED";
				ev = DISP_EVENT_NAMES[std::max(0, std::min(4, h.eventNow()))];
				if (module->in.layout > 1.f) ev = "LAYING OUT";
			}
			seed = std::to_string(h.seedNow);
		}
		float hgt = box.size.y, w = box.size.x;
		nvgFillColor(args.vg, R_AMBER());
		nvgFontSize(args.vg, 24.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, 6.f, hgt * 0.72f, feel.c_str(), NULL);
		float b[4]; nvgTextBounds(args.vg, 6.f, hgt * 0.72f, feel.c_str(), NULL, b);
		nvgFontSize(args.vg, 13.f);
		float x = std::max(b[2] + 14.f, 110.f);
		nvgText(args.vg, x, hgt * 0.45f, bar.c_str(), NULL);
		nvgFillColor(args.vg, R_DIM());
		nvgText(args.vg, x, hgt * 0.84f, ev.c_str(), NULL);
		nvgFontSize(args.vg, 11.f);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, w - 6.f, hgt * 0.45f, src.c_str(), NULL);
		std::string sd = "SEED " + seed;
		nvgText(args.vg, w - 6.f, hgt * 0.84f, sd.c_str(), NULL);
	}
};

struct RAmberLight : GrayModuleLightWidget { RAmberLight() { addBaseColor(nvgRGB(0xF2, 0xC2, 0x5C)); } };

struct RiderWidget : ModuleWidget {
	RiderWidget(RiderModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rider.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		RiderDisplay* d = createWidget<RiderDisplay>(mm2px(Vec(R_DISP_X, R_DISP_Y)));
		d->box.size = mm2px(Vec(R_DISP_W, R_DISP_H)); d->module = module; addChild(d);

		// PLAYER
		for (int i = 0; i < 9; i++) {
			int id = RiderModule::FEEL_PARAM + i;
			if (i == 1) addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(rcol(i), R_Y_PLAYER)), module, id));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(rcol(i), R_Y_PLAYER)), module, id));
		}
		addParam(createParamCentered<VCVButton>(mm2px(Vec(rcol(9), R_Y_PLAYER)), module, RiderModule::NEW_PARAM));
		// HANDS and the kit's levels
		for (int i = 0; i < 7; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(rcol(i), R_Y_HANDS)), module, RiderModule::LOOSE_PARAM + i));
		for (int i = 0; i < 3; i++) addChild(createLightCentered<MediumLight<RAmberLight>>(mm2px(Vec(rcol(7 + i), R_Y_HANDS)), module, RiderModule::HIT_LIGHT + i));
		// KIT
		for (int i = 0; i < 6; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(rscol(i), R_Y_SOUND)), module, RiderModule::TUNE_PARAM + i));
		// jacks
		for (int i = 0; i < 7; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ricol(i), R_Y_IN1)), module, RiderModule::CLOCK_INPUT + i));
		for (int i = 0; i < 10; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(rcol(i), R_Y_IN2)), module, RiderModule::LEFT_OUTPUT + i));
	}

	void appendContextMenu(Menu* menu) override {
		RiderModule* m = dynamic_cast<RiderModule*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Cables (without Comper)"));
		menu->addChild(createIndexSubmenuItem("Clock pulses per beat", {"1", "2", "4", "24 (MIDI clock)"},
			[=]() { return m->ppbIndex; }, [=](size_t i) { m->ppbIndex = (int)i; }));
		menu->addChild(createIndexSubmenuItem("Chorus length", {"8 bars", "12 bars", "16 bars", "32 bars"},
			[=]() { for (int i = 0; i < 4; i++) if (R_CHORUS_CHOICES[i] == m->chorusBars) return (size_t)i; return (size_t)1; },
			[=](size_t i) { m->chorusBars = R_CHORUS_CHOICES[std::min((size_t)3, i)]; }));
	}
};

Model* modelRider = createModel<RiderModule, RiderWidget>("Rider");
