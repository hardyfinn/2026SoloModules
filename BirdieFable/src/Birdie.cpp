#include "plugin.hpp"
#include "dsp/BirdieCore.hpp"
#include "dsp/presets.h"

using namespace birdie;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 32.0f, DISP_Y = 4.4f, DISP_W = 72.0f, DISP_H = 8.2f;
static const float LIGHT_X = 108.5f, LIGHT_Y = 8.5f;
static const float COLS[8] = {10.6f, 28.8f, 47.0f, 65.2f, 83.4f, 101.6f, 119.8f, 138.0f};
static const float ROW_Y[4] = {30.0f, 48.5f, 67.0f, 85.5f};
static const float RX[2] = {157.4f, 173.4f};
static const float RY[3] = {30.0f, 48.5f, 67.0f};
static const float JX0 = 7.62f, JDX = 15.24f;
static const float JA_Y = 103.0f, JB_Y = 116.5f;

struct BirdieModule : Module {
	enum LightIds { PRESS_LIGHT, HOLD_LIGHT, NUM_LIGHTS };

	Core core;
	float kv[NUM_P] = {};
	float inV[NUM_I] = {};
	bool inC[NUM_I] = {};
	dsp::ClockDivider lightDiv;

	// display copies (written on the audio thread, read on the UI thread)
	int dispPattern = PT_MOTIF, dispIdx = 0, dispN = 0;
	bool dispPlaying = false, dispLock = false, dispClock = false;
	uint32_t dispSeed = 0;

	BirdieModule() {
		config(NUM_P, NUM_I, NUM_O, NUM_LIGHTS);
		BdParams d = bd_default();
		float def[NUM_P];
		paramsToKnobs(d, def);

		// --- voice
		struct SizeQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double s = mapSize(getValue());
				return string::f("x%.2f (whistle near %.0f Hz)", s, 4250.0 / s);
			}
		};
		configParam<SizeQ>(SIZE_P, 0.f, 1.f, def[SIZE_P], "Size")->description =
			"How big the bird is. Pitch follows exactly: twice the size is an octave lower, and everything else in the body scales with it. Small is a wren, large is a goose, the top end is bigger than any bird.";
		configParam(TENSION_P, 0.f, 1.f, def[TENSION_P], "Tension", " semitones", 0.f, 43.f, -36.f)->description =
			"Pitch and timbre together. High tension is a pure whistle. Lower it and the pitch drops and the tone turns spiky and rich, down to crows, ducks and buzzing pulses at the bottom.";
		configParam(PRESSURE_P, 0.f, 1.f, def[PRESSURE_P], "Pressure", "%", 0.f, 100.f)->description =
			"Air pressure from the lungs at the peak of each note: louder, brighter and a touch sharper.";
		configParam(BREATH_P, 0.f, 1.f, def[BREATH_P], "Breath", "%", 0.f, 100.f)->description =
			"Air noise riding on each note.";
		configParam(ROUGH_P, 0.f, 1.f, def[ROUGH_P], "Rough", "%", 0.f, 100.f)->description =
			"Turbulence in the air flow: shakes pressure and tension randomly for raspy, harsh calls.";
		configParam(V2MIX_P, 0.f, 1.f, def[V2MIX_P], "2nd voice mix", "%", 0.f, 100.f)->description =
			"Brings in the other side of the syrinx, a second voice that sings at the same time. Birds really do this.";
		configParam(V2INT_P, 0.f, 1.f, def[V2INT_P], "2nd voice interval", " semitones", 0.f, 24.f, -12.f)->description =
			"Pitch of the second voice relative to the first. Near zero they beat against each other.";
		configParam(V2SLIDE_P, 0.f, 1.f, def[V2SLIDE_P], "2nd voice slide", "%", 0.f, 200.f, -100.f)->description =
			"How the second voice follows the note's slide: +100% moves with the first voice, -100% mirrors it, 0 holds still.";

		// --- note
		struct LengthQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double s = mapLength(getValue());
				return s < 1.0 ? string::f("%.0f ms", s * 1000.0) : string::f("%.2f s", s);
			}
		};
		configParam<LengthQ>(LENGTH_P, 0.f, 1.f, def[LENGTH_P], "Length")->description =
			"How long one note lasts. In a phrase a note never runs past 92% of its slot.";
		configParam(SLIDE_P, 0.f, 1.f, def[SLIDE_P], "Slide", " semitones", 0.f, 48.f, -24.f)->description =
			"How far the pitch moves during the note, following SHAPE. Negative falls, positive rises.";
		configSwitch(SHAPE_P, 0.f, (float)(SH_COUNT - 1), def[SHAPE_P], "Shape",
			{"LINE: straight slide", "FALL: fast then settling", "SWOOP: slow then fast", "ARCH: out and back", "DOUBLE: two arches"});
		configParam(ATTACK_P, 0.f, 1.f, def[ATTACK_P], "Attack", "% of the note", 0.f, 58.f, 2.f);
		configParam(RELEASE_P, 0.f, 1.f, def[RELEASE_P], "Release", "% of the note", 0.f, 58.f, 2.f);
		configParam(BUZZRATE_P, 0.f, 1.f, def[BUZZRATE_P], "Buzz rate", " Hz", 80.f, 5.f)->description =
			"Speed of the buzz: fast wobble of tension and pressure.";
		configParam(BUZZ_P, 0.f, 1.f, def[BUZZ_P], "Buzz depth", "%", 0.f, 50.f)->description =
			"How far the buzz wobbles the tension.";
		configParam(CHOP_P, 0.f, 1.f, def[CHOP_P], "Chop", "%", 0.f, 100.f)->description =
			"How hard the buzz chops the air pressure, from a smooth tremolo to separate clicks.";

		// --- body
		configParam(TRACHEA_P, 0.f, 1.f, def[TRACHEA_P], "Trachea length", " cm", 20.f, 0.5f)->description =
			"Length of the windpipe (at size 1; it grows with SIZE). Adds hollow, tube-like resonances when TUBE is up.";
		configParam(TUBE_P, 0.f, 1.f, def[TUBE_P], "Tube", "%", 0.f, 100.f)->description =
			"How strongly the windpipe resonates.";
		configParam(THROAT_P, 0.f, 1.f, def[THROAT_P], "Throat", "%", 0.f, 100.f)->description =
			"How much the throat cavity filters the sound. With FOLLOW on it tracks the pitch, which is how real songbirds whistle so cleanly.";
		struct TuneQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = getValue();
				bool follow = !module || module->params[FOLLOW_P].getValue() > 0.5f;
				if (follow) return string::f("%.2f x pitch", std::pow(2.0, k * 3.0 - 1.0));
				double size = module ? mapSize(module->params[SIZE_P].getValue()) : 1.0;
				return string::f("%.0f Hz", 300.0 * std::pow(20.0, k) / std::sqrt(size));
			}
		};
		configParam<TuneQ>(TUNE_P, 0.f, 1.f, def[TUNE_P], "Throat tune")->description =
			"Where the throat resonance sits: a multiple of the pitch with FOLLOW on, a fixed formant with it off.";
		configSwitch(FOLLOW_P, 0.f, 1.f, def[FOLLOW_P], "Throat follows pitch", {"Off: fixed formant", "On: tracks the pitch"});
		struct BeakQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double size = module ? mapSize(module->params[SIZE_P].getValue()) : 1.0;
				return string::f("%.0f Hz", mapBeak(getValue()) / std::sqrt(size));
			}
		};
		configParam<BeakQ>(BEAK_P, 0.f, 1.f, def[BEAK_P], "Beak")->description =
			"How wide the beak opens: closed is muffled, like a dove; open is bright.";
		struct TimeQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("x%.2f slower", mapTime(getValue())); }
		};
		configParam<TimeQ>(TIME_P, 0.f, 1.f, def[TIME_P], "Time stretch")->description =
			"Slows every gesture, note, buzz and phrase down, up to 16 times, without changing pitch.";
		configButton(HOLD_P, "Hold (latching)")->description =
			"The voice sounds continuously, as an oscillator. Notes and phrases still play their slides on top. The HOLD jack does the same while its gate is high.";

		// --- phrase
		configParam(NOTES_P, 1.f, 32.f, def[NOTES_P], "Notes per phrase")->snapEnabled = true;
		struct RateQ : ParamQuantity {
			std::string getDisplayValueString() override {
				bool lock = module && module->params[CLOCK_P].getValue() > 0.5f;
				if (lock) return string::f("%s notes per clock", LOCK_NAMES[lockZone(getValue())]);
				return string::f("%.2f notes per second", mapRate(getValue()));
			}
		};
		configParam<RateQ>(RATE_P, 0.f, 1.f, def[RATE_P], "Rate")->description =
			"Speed of the phrase. With CLOCK at LOCK it picks how many notes fit in one clock: 1/4 to 8.";
		configSwitch(PATTERN_P, 0.f, (float)(PT_COUNT - 1), def[PATTERN_P], "Pattern",
			{"REPEAT: A A A A", "PAIR: A B A B", "MOTIF: A A B", "TAIL: A A A ... then B", "RUN: A, B, C, D in blocks", "CALL: A B C D", "SHUFFLE: picked by the seed"});
		configParam(VARY_P, 0.f, 1.f, def[VARY_P], "Vary", "%", 0.f, 100.f)->description =
			"How different the other note types (B, C, D) are from the note on the panel. At 0 every note is the same.";
		configParam(DRIFT_P, 0.f, 1.f, def[DRIFT_P], "Drift", " semitones", 0.f, 24.f, -12.f)->description =
			"Pitch movement across the whole phrase.";
		configParam(ACCEL_P, 0.f, 1.f, def[ACCEL_P], "Accel", " octaves", 0.f, 3.f, -1.5f)->description =
			"Speeds the phrase up (positive) or slows it down (negative) by the end.";
		configParam(SWELL_P, 0.f, 1.f, def[SWELL_P], "Swell", "%", 0.f, 200.f, -100.f)->description =
			"Positive grows louder across the phrase, negative fades away.";
		struct GapQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%.2f s", mapGap(getValue())); }
		};
		configParam<GapQ>(GAP_P, 0.f, 1.f, def[GAP_P], "Gap (AUTO)")->description =
			"Rest between phrases when AUTO is on (stretched by TIME).";

		// --- play
		configSwitch(CLOCK_P, 0.f, 1.f, 0.f, "Clock", {"FREE: phrases run at RATE", "LOCK: notes fit the clock input"});
		configSwitch(AUTO_P, 0.f, 1.f, 0.f, "Auto", {"Off", "On: sings phrase after phrase with GAP between"});
		configButton(NEW_P, "New seed")->description = "Picks a new phrase. It takes effect at the next phrase start.";
		configButton(NOTE_P, "Note")->description = "Sings one note from the panel settings.";
		configButton(PHRASE_P, "Phrase")->description = "Sings one phrase.";

		configInput(SIZE_I, "Size CV (+1 V = one octave up, 1 V/oct)");
		configInput(TENSION_I, "Tension CV (12 semitones per volt)");
		configInput(PRESSURE_I, "Pressure CV (10% per volt)");
		configInput(ROUGH_I, "Rough CV (10% per volt)");
		configInput(V2MIX_I, "2nd voice mix CV (10% per volt)");
		configInput(LENGTH_I, "Length CV (+1 V doubles)");
		configInput(SLIDE_I, "Slide CV (4.8 semitones per volt)");
		configInput(BUZZRATE_I, "Buzz rate CV (1 V/oct)");
		configInput(BEAK_I, "Beak CV (1 V/oct)");
		configInput(TIME_I, "Time stretch CV (+1 V doubles)");
		configInput(RATE_I, "Rate CV (1 V/oct; in LOCK one step per volt)");
		configInput(VARY_I, "Vary CV (10% per volt)");
		configInput(NOTE_I, "Note trigger");
		configInput(PHRASE_I, "Phrase trigger");
		configInput(NEW_I, "New seed trigger");
		configInput(CLOCK_I, "Clock (sets the note rate when CLOCK is at LOCK)");
		configInput(HOLD_I, "Hold gate");
		configOutput(END_O, "End of phrase trigger");
		configOutput(GATE_O, "Gate (high while a note sounds)");
		configOutput(PRESS_O, "Pressure envelope (0 to 10 V)");
		configOutput(PITCH_O, "Pitch estimate (V/oct, 0 V = C4)");
		configOutput(OUT_O, "Audio");

		lightDiv.setDivision(64);
		core.seedRng = random::u32() | 1u;
		core.seed = core.freshSeed();
		core.setSampleRate(48000.0);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { core.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		core.newEachPhrase = false;
		core.setSampleRate(APP->engine->getSampleRate());
	}

	void process(const ProcessArgs& args) override {
		for (int i = 0; i < NUM_P; i++) kv[i] = params[i].getValue();
		for (int i = 0; i < NUM_I; i++) {
			inC[i] = inputs[i].isConnected();
			inV[i] = inC[i] ? inputs[i].getVoltage() : 0.f;
		}
		core.process(kv, inV, inC);
		for (int o = 0; o < NUM_O; o++) outputs[o].setVoltage(core.out[o]);

		if (lightDiv.process()) {
			lights[PRESS_LIGHT].setBrightness(core.out[PRESS_O] * 0.1f);
			lights[HOLD_LIGHT].setBrightness(core.P.hold ? 1.f : 0.f);
			dispPattern = core.dispPattern; dispIdx = core.dispIdx; dispN = core.dispN;
			dispPlaying = core.dispPlaying; dispSeed = core.seed;
			dispLock = kv[CLOCK_P] > 0.5f; dispClock = core.clockPeriod > 0.0;
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer((json_int_t)core.seed));
		json_object_set_new(root, "newEachPhrase", json_boolean(core.newEachPhrase));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s) core.seed = (uint32_t)json_integer_value(s);
		json_t* n = json_object_get(root, "newEachPhrase");
		if (n) core.newEachPhrase = json_boolean_value(n);
	}
};

struct BirdieDisplay : Widget {
	BirdieModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		int pat = module ? module->dispPattern : PT_MOTIF;
		std::string big;
		if (module && module->dispPlaying) big = string::f("%s %d/%d", PATTERN_NAMES[clamp(pat, 0, PT_COUNT - 1)], module->dispIdx, module->dispN);
		else big = PATTERN_NAMES[clamp(pat, 0, PT_COUNT - 1)];
		std::string sub = string::f("%s  SEED %04u", module && module->dispLock ? (module->dispClock ? "LOCK" : "LOCK?") : "FREE",
		                            module ? (unsigned)(module->dispSeed % 10000u) : 421u);
		NVGcolor orange = nvgRGB(255, 138, 61), ink = nvgRGB(239, 233, 210);
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(5.0f));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, orange);
			nvgText(args.vg, mm2px(1.8f), box.size.y / 2, big.c_str(), NULL);
			nvgFontSize(args.vg, mm2px(2.8f));
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, ink);
			nvgText(args.vg, box.size.x - mm2px(1.6f), box.size.y / 2, sub.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct BirdieWidget : ModuleWidget {
	BirdieWidget(BirdieModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Birdie.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		BirdieDisplay* d = createWidget<BirdieDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(LIGHT_X, LIGHT_Y)), module, BirdieModule::PRESS_LIGHT));

		const int rows[4][8] = {
			{SIZE_P, TENSION_P, PRESSURE_P, BREATH_P, ROUGH_P, V2MIX_P, V2INT_P, V2SLIDE_P},
			{LENGTH_P, SLIDE_P, SHAPE_P, ATTACK_P, RELEASE_P, BUZZRATE_P, BUZZ_P, CHOP_P},
			{TRACHEA_P, TUBE_P, THROAT_P, TUNE_P, -1, BEAK_P, TIME_P, -1},
			{NOTES_P, RATE_P, PATTERN_P, VARY_P, DRIFT_P, ACCEL_P, SWELL_P, GAP_P}};
		for (int r = 0; r < 4; r++)
			for (int i = 0; i < 8; i++)
				if (rows[r][i] >= 0) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLS[i], ROW_Y[r])), module, rows[r][i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(COLS[4] - 2.0f, ROW_Y[2])), module, FOLLOW_P));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COLS[7], ROW_Y[2])), module, HOLD_P, BirdieModule::HOLD_LIGHT));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(RX[0], RY[0])), module, NOTE_P));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(RX[1], RY[0])), module, PHRASE_P));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(RX[0], RY[1])), module, NEW_P));
		addParam(createParamCentered<CKSS>(mm2px(Vec(RX[1] - 2.0f, RY[1])), module, AUTO_P));
		addParam(createParamCentered<CKSS>(mm2px(Vec(RX[0] - 2.0f, RY[2])), module, CLOCK_P));

		const int ja[12] = {SIZE_I, TENSION_I, PRESSURE_I, ROUGH_I, V2MIX_I, LENGTH_I, SLIDE_I, BUZZRATE_I, BEAK_I, TIME_I, RATE_I, VARY_I};
		for (int i = 0; i < 12; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JA_Y)), module, ja[i]));
		const int jb[5] = {NOTE_I, PHRASE_I, NEW_I, CLOCK_I, HOLD_I};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JB_Y)), module, jb[i]));
		const int jo[5] = {END_O, GATE_O, PRESS_O, PITCH_O, OUT_O};
		for (int i = 0; i < 5; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * (7 + i), JB_Y)), module, jo[i]));
	}

	void appendContextMenu(Menu* menu) override {
		BirdieModule* m = dynamic_cast<BirdieModule*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("New seed every phrase", "", &m->core.newEachPhrase));
		menu->addChild(createSubmenuItem("Seed", string::f("%u", (unsigned)m->core.seed), [=](Menu* sub) {
			struct SeedField : ui::TextField {
				BirdieModule* m;
				void onSelectKey(const SelectKeyEvent& e) override {
					if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
						try { m->core.seed = (uint32_t)std::stoul(text); } catch (...) {}
						ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
						if (overlay) overlay->requestDelete();
						e.consume(this);
					}
					if (!e.getTarget()) ui::TextField::onSelectKey(e);
				}
			};
			SeedField* f = new SeedField;
			f->m = m;
			f->box.size.x = 120;
			f->text = string::f("%u", (unsigned)m->core.seed);
			sub->addChild(createMenuLabel("Type a seed, then Enter"));
			sub->addChild(f);
		}));
	}
};

Model* modelBirdie = createModel<BirdieModule, BirdieWidget>("Birdie");
