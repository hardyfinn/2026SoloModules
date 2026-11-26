#include "plugin.hpp"
#include "dsp/WalkerHost.hpp"
#include "LinkPort.hpp"
#include "Progressions.hpp"
#include <atomic>

using namespace walker;

// ---- LAYOUT (same numbers in res/make_walker_panel.py) ----
static const float DISP_X = 4.0f, DISP_Y = 12.5f, DISP_W = 174.88f, DISP_H = 13.0f;   // 36 HP since 2.4.0
static const float COL_X0 = 11.0f, COL_DX = 20.11f;
static const float SND_X0 = 11.0f, SND_DX = 17.875f;
static const float Y_PLAYER = 42.0f, Y_HANDS = 66.0f, Y_SOUND = 89.0f, Y_IN1 = 106.0f, Y_IN2 = 118.0f;
static const int OUT_COL0 = 7;
static float colX(int i) { return COL_X0 + i * COL_DX; }
static float sndX(int i) { return SND_X0 + i * SND_DX; }

static const int W_PPB_CHOICES[4] = {1, 2, 4, 24};
static const int TUNE_CHOICES[7] = {0, 4, 8, 12, 16, 24, 32};

struct Walker : Module {
	enum ParamIds {
		FEEL_PARAM, ACTIVITY_PARAM, REGISTER_PARAM, ROAM_PARAM, OUT_PARAM, REPEAT_PARAM, SEED_PARAM, NEW_PARAM,
		LOOSE_PARAM, AHEAD_PARAM, TIMING_PARAM, WEIGHT_PARAM, SLOP_PARAM, PITCH_PARAM,
		ATTACK_PARAM, PLUCK_PARAM, BRIGHT_PARAM, SUSTAIN_PARAM, GROWL_PARAM, BODY_PARAM, PICKUP_PARAM, THUMP_PARAM, AGE_PARAM, LEVEL_PARAM,
		OCTAVE_PARAM, LENGTH_PARAM, VARY_PARAM,   // 2.4.0 (added at the end so old patches keep their knobs)
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT, RESET_INPUT, ROOT_INPUT, QUAL_INPUT, CHORD_INPUT, LAYOUT_INPUT,
		VOCT_IN_INPUT, GATE_IN_INPUT, VEL_IN_INPUT,
		ACTIVITY_INPUT, REGISTER_INPUT, LOOSE_INPUT, ATTACK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { AUDIO_OUTPUT, VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, NUM_OUTPUTS };
	enum LightIds { PLUCK_LIGHT, LINK_LIGHT, NUM_LIGHTS };

	WalkerHost host;
	WIn in;
	WOut out;
	int ppbIndex = 0;
	int tuneBars = 0;
	bool voctSlides = true;
	dsp::BooleanTrigger newTrig;
	dsp::ClockDivider lightDiv;
	// the band's link (Comper, Rider in the row, any order)
	LinkPort port;
	bool linked = false;
	std::atomic<bool> forgetTune{false};

	Walker() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(FEEL_PARAM, 0.f, 6.f, 0.f, "Feel", {"Follow Comper's groove", "Walk (four to the bar)", "Two-feel", "Ballad", "Bossa", "Soul", "House"});
		configParam(ACTIVITY_PARAM, 0.f, 1.f, 0.5f, "Activity (skips, dead notes, triplet runs)", "%", 0.f, 100.f);
		struct RegQ : ParamQuantity {
			std::string getDisplayValueString() override { return comper::noteName((int)std::round(getValue())); }
		};
		configParam<RegQ>(REGISTER_PARAM, 34.f, 46.f, 40.f, "Register (the middle of the line)")->snapEnabled = true;
		configParam(ROAM_PARAM, 0.f, 1.f, 0.5f, "Roam (how far the line wanders from the register)", "%", 0.f, 100.f);
		configParam(OUT_PARAM, 0.f, 1.f, 0.2f, "Out (chromatic approaches and outside notes)", "%", 0.f, 100.f);
		configParam(REPEAT_PARAM, 0.f, 1.f, 0.7f, "Repeat (keep the figures of soul and house)", "%", 0.f, 100.f);
		configParam(SEED_PARAM, 0.f, 999.f, 1.f, "Seed (a new take at the next bar line)")->snapEnabled = true;
		configButton(NEW_PARAM, "New seed");
		configParam(LOOSE_PARAM, 0.f, 2.5f, 1.f, "Loose (0 machine, 1 a good player, 2.5 drunk)", "%", 0.f, 100.f)->description =
			"Scales everything human: timing, accents, lengths, intonation, slides and buzz.";
		configParam(AHEAD_PARAM, 0.f, 1.f, 0.5f, "Ahead (how far the bass leans in front of the beat)", "%", 0.f, 100.f);
		configParam(TIMING_PARAM, 0.f, 2.f, 1.f, "Timing trim (x LOOSE)", "%", 0.f, 100.f);
		configParam(WEIGHT_PARAM, 0.f, 2.f, 1.f, "Weight trim (x LOOSE): accents on 2 and 4", "%", 0.f, 100.f);
		configParam(SLOP_PARAM, 0.f, 2.f, 1.f, "Slop trim (x LOOSE): note lengths", "%", 0.f, 100.f);
		configParam(PITCH_PARAM, 0.f, 2.f, 1.f, "Pitch trim (x LOOSE): intonation, scoops and vibrato", "%", 0.f, 100.f);
		configParam(ATTACK_PARAM, 0.f, 2.5f, 1.f, "Attack", "%", 0.f, 100.f)->description =
			"The pluck: finger burst, finger noise and the knock of the body. 1 is as measured on a real bass.";
		configParam(PLUCK_PARAM, 0.08f, 0.30f, 0.17f, "Pluck position (near the end of the string to near the middle)");
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.5f, "Bright (gut and soft to steel and bright)", "%", 0.f, 100.f);
		configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.5f, "Sustain (how long notes ring)", "%", 0.f, 100.f);
		configParam(GROWL_PARAM, 0.f, 1.f, 0.35f, "Growl (the string slapping the fingerboard)", "%", 0.f, 100.f);
		configParam(BODY_PARAM, 0.f, 1.f, 0.6f, "Body (the wooden box: resonance and ring)", "%", 0.f, 100.f);
		configParam(PICKUP_PARAM, 0.f, 1.f, 0.25f, "Mic / pickup (0 all microphone, 1 all pickup)", "%", 0.f, 100.f);
		configParam(THUMP_PARAM, 0.f, 1.f, 0.5f, "Thump (finger noise)", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age (old strings: duller, shorter, buzzier)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f);
		struct OctQ : ParamQuantity {
			std::string getDisplayValueString() override { int o = (int)std::round(getValue()); return o == 0 ? "as written" : "+" + std::to_string(o); }
		};
		configParam<OctQ>(OCTAVE_PARAM, 0.f, 2.f, 0.f, "Octave (moves the bass up, audio and V/OCT)", " oct")->snapEnabled = true;
		configParam(LENGTH_PARAM, 0.3f, 3.f, 1.4f, "Length (every note shorter or longer)", "x")->description =
			"1 is how the reference player holds notes. Long notes never overlap: the next note ends them.";
		configParam(VARY_PARAM, 0.f, 1.f, 0.5f, "Vary (a mix of short and long notes)", "%", 0.f, 100.f)->description =
			"0 every note the same length. Up: more notes held long (mostly on the beat) and clipped short (mostly off it).";
		for (int id : {(int)SEED_PARAM, (int)NEW_PARAM, (int)LEVEL_PARAM}) getParamQuantity(id)->randomizeEnabled = false;

		configInput(CLOCK_INPUT, "Clock (pulses per beat in the right-click menu; used without Comper)");
		configInput(RESET_INPUT, "Reset (without Comper; two resets also mark the length of the tune)");
		configInput(ROOT_INPUT, "Chord root (V/oct, without Comper)");
		configInput(QUAL_INPUT, "Chord quality (1 V per step: maj7 6 m7 m6 mMaj7 7 7alt m7b5 dim7 7sus)");
		configInput(CHORD_INPUT, "Chord notes (poly V/oct, without Comper)");
		configInput(LAYOUT_INPUT, "Lay out (gate: the bass stops playing while high)");
		configInput(VOCT_IN_INPUT, "V/oct in (played as a voice)");
		configInput(GATE_IN_INPUT, "Gate in (patching it turns Walker into a playable upright bass)");
		configInput(VEL_IN_INPUT, "Velocity in (0 to 10 V)");
		configInput(ACTIVITY_INPUT, "ACTIVITY CV (0.1 per volt)");
		configInput(REGISTER_INPUT, "REGISTER CV (1.2 semitones per volt)");
		configInput(LOOSE_INPUT, "LOOSE CV (0.25 per volt)");
		configInput(ATTACK_INPUT, "ATTACK CV (0.25 per volt)");
		configOutput(AUDIO_OUTPUT, "Upright bass audio");
		configOutput(VOCT_OUTPUT, "Bass notes (V/oct, 0 V = middle C)");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(VEL_OUTPUT, "Velocity (0 to 10 V)");
		configLight(PLUCK_LIGHT, "Note played");
		configLight(LINK_LIGHT, "Comper in the row");
		lightDiv.setDivision(64);

		port.attach(this);
		host.setSampleRate(APP->engine->getSampleRate());
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { host.setSampleRate(e.sampleRate); }
	void onReset() override {
		ppbIndex = 0; tuneBars = 0; voctSlides = true;
		double sr = host.sr; host = WalkerHost(); host.setSampleRate(sr);
	}

	void process(const ProcessArgs& args) override {
		if (newTrig.process(params[NEW_PARAM].getValue() > 0.f))
			params[SEED_PARAM].setValue((float)(random::u32() % 1000));
		if (forgetTune.exchange(false)) { host.tuneLen = 0; host.learnedMode = false; host.resetLenFromReset = 0; for (auto& h : host.recHeard) h = false; }

		port.read(this);
		CompLink heard = port.merged();
		linked = heard.hasComp;
		in.linkOk = linked;
		if (linked) in.link = heard;
		float bandX = heard.hasDrum ? clamp(heard.bandX, -1.f, 1.f) : 0.f;

		in.clockConnected = inputs[CLOCK_INPUT].isConnected(); in.clock = inputs[CLOCK_INPUT].getVoltage();
		in.ppb = W_PPB_CHOICES[clamp(ppbIndex, 0, 3)];
		in.reset = inputs[RESET_INPUT].getVoltage();
		in.rootConnected = inputs[ROOT_INPUT].isConnected(); in.rootV = inputs[ROOT_INPUT].getVoltage();
		in.qualConnected = inputs[QUAL_INPUT].isConnected(); in.qualV = inputs[QUAL_INPUT].getVoltage();
		in.chordConnected = inputs[CHORD_INPUT].isConnected();
		in.chordChannels = inputs[CHORD_INPUT].getChannels();
		for (int c = 0; c < in.chordChannels; c++) in.chordV[c] = inputs[CHORD_INPUT].getVoltage(c);
		in.layout = inputs[LAYOUT_INPUT].getVoltage();
		// a Soloist in the row (2.5.0): the bass lays out while the drums take their turn and after the tune ends
		float soloE = 0.f, soloOut = 0.f;
		if (heard.hasSolo()) {
			int turn = turnAt(heard, host.phi + 0.15);
			if (turn == TURN_DRUMS || turn == TURN_END) in.layout = 10.f;
			soloE = soloEnergy(heard); soloOut = soloOutside(heard);
		}
		in.gateInConnected = inputs[GATE_IN_INPUT].isConnected(); in.gateIn = inputs[GATE_IN_INPUT].getVoltage();
		in.voctIn = inputs[VOCT_IN_INPUT].getVoltage();
		in.velInConnected = inputs[VEL_IN_INPUT].isConnected(); in.velIn = inputs[VEL_IN_INPUT].getVoltage();

		in.feelPos = clamp((int)std::round(params[FEEL_PARAM].getValue()), 0, 6);
		BP& p = in.bp;
		p.activity = clamp(params[ACTIVITY_PARAM].getValue() + (inputs[ACTIVITY_INPUT].isConnected() ? 0.1f * inputs[ACTIVITY_INPUT].getVoltage() : 0.f) + 0.35f * bandX - 0.5f * std::max(0.f, soloE - 0.7f), 0.f, 1.f);   // under a hot solo the bass keeps it simple
		p.reg = clamp(params[REGISTER_PARAM].getValue() + (inputs[REGISTER_INPUT].isConnected() ? 1.2f * inputs[REGISTER_INPUT].getVoltage() : 0.f), 34.f, 46.f);
		p.roam = params[ROAM_PARAM].getValue();
		p.out = clamp(params[OUT_PARAM].getValue() + 0.4f * soloOut, 0.f, 1.f);
		p.repeat = params[REPEAT_PARAM].getValue();
		p.loose = clamp(params[LOOSE_PARAM].getValue() + (inputs[LOOSE_INPUT].isConnected() ? 0.25f * inputs[LOOSE_INPUT].getVoltage() : 0.f), 0.f, 2.5f);
		p.ahead = params[AHEAD_PARAM].getValue();
		p.timing = params[TIMING_PARAM].getValue();
		p.weight = params[WEIGHT_PARAM].getValue();
		p.slop = params[SLOP_PARAM].getValue();
		p.pitch = params[PITCH_PARAM].getValue();
		p.swing = 0.0;
		p.length = params[LENGTH_PARAM].getValue();
		p.vary = params[VARY_PARAM].getValue();
		in.octave = clamp((int)std::round(params[OCTAVE_PARAM].getValue()), 0, 2);
		in.seed = (int)params[SEED_PARAM].getValue();
		upright::Tone& t = in.tone;
		t.attack = clamp(params[ATTACK_PARAM].getValue() + (inputs[ATTACK_INPUT].isConnected() ? 0.25f * inputs[ATTACK_INPUT].getVoltage() : 0.f), 0.f, 2.5f);
		t.pluck = params[PLUCK_PARAM].getValue();
		t.bright = params[BRIGHT_PARAM].getValue();
		t.sustain = params[SUSTAIN_PARAM].getValue();
		t.growl = params[GROWL_PARAM].getValue();
		t.body = params[BODY_PARAM].getValue();
		t.pickup = params[PICKUP_PARAM].getValue();
		t.thump = params[THUMP_PARAM].getValue();
		t.age = params[AGE_PARAM].getValue();
		in.level = params[LEVEL_PARAM].getValue();
		in.tuneBars = tuneBars;
		in.voctSlides = voctSlides;

		host.process(in, out, args.sampleTime);
		{	// tell the band where the bass runs
			CompLink m; m.hasBass = true; m.bassN = host.offBeats(m.bassBeat, 16);
			port.send(this, m);
		}

		float bandGain = std::pow(10.f, 3.f * bandX / 20.f);
		outputs[AUDIO_OUTPUT].setVoltage(out.audio * bandGain);
		outputs[VOCT_OUTPUT].setVoltage(out.voct);
		outputs[GATE_OUTPUT].setVoltage(out.gate);
		outputs[VEL_OUTPUT].setVoltage(std::min(10.f, out.vel * bandGain));

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[PLUCK_LIGHT].setBrightnessSmooth(host.t - host.pluckAt < 0.06 ? 1.f : 0.f, dt);
			lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppb", json_integer(ppbIndex));
		json_object_set_new(root, "tuneBars", json_integer(tuneBars));
		json_object_set_new(root, "voctSlides", json_boolean(voctSlides));
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppb")) ppbIndex = clamp((int)json_integer_value(j), 0, 3);
		if (json_t* j = json_object_get(root, "tuneBars")) {
			int v = (int)json_integer_value(j); tuneBars = 0;
			for (int c : TUNE_CHOICES) if (c == v) tuneBars = v;
		}
		if (json_t* j = json_object_get(root, "voctSlides")) voctSlides = json_boolean_value(j);
	}
};

// ---------------------------------------------------------------- display
static NVGcolor W_AMBER() { return nvgRGB(0xF2, 0xC2, 0x5C); }
static NVGcolor W_DIM() { return nvgRGB(0x8A, 0x7A, 0x55); }

struct WalkerDisplay : TransparentWidget {
	Walker* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		std::string note = "F2", feel = "WALK", src = "LINK", chord = "Dm7", seed = "1";
		if (module) {
			WalkerHost& h = module->host;
			note = h.lastPitch >= 0 ? comper::noteName(h.lastPitch) : "--";
			src = WSOURCE_NAMES[std::max(0, std::min(4, h.source))];
			if (h.source == WS_PLAY) feel = "VOICE";
			else if (h.source == WS_IDLE) feel = "";
			else {
				feel = FEEL_NAMES[h.wantFeel(module->in)];
				if (module->in.feelPos == 0 && h.source == WS_LINK) feel += " (FOLLOW)";
			}
			chord = (h.haveCur && h.source != WS_PLAY && h.source != WS_IDLE) ? comper::chordName(h.curChord) : "";
			seed = std::to_string(h.seedNow);
			if (h.source == WS_IDLE) { note = "--"; chord = "next to Comper, or patch CLOCK"; }
		}
		float hgt = box.size.y, w = box.size.x;
		nvgFillColor(args.vg, W_AMBER());
		nvgFontSize(args.vg, 24.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, 6.f, hgt * 0.72f, note.c_str(), NULL);
		float b[4]; nvgTextBounds(args.vg, 6.f, hgt * 0.72f, note.c_str(), NULL, b);
		nvgFontSize(args.vg, 13.f);
		float x = std::max(b[2] + 10.f, 56.f);
		nvgText(args.vg, x, hgt * 0.45f, feel.c_str(), NULL);
		nvgFillColor(args.vg, W_DIM());
		nvgText(args.vg, x, hgt * 0.84f, chord.c_str(), NULL);
		nvgFontSize(args.vg, 11.f);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, w - 6.f, hgt * 0.45f, src.c_str(), NULL);
		std::string sd = "SEED " + seed;
		nvgText(args.vg, w - 6.f, hgt * 0.84f, sd.c_str(), NULL);
	}
};

struct WAmberLight : GrayModuleLightWidget { WAmberLight() { addBaseColor(nvgRGB(0xF2, 0xC2, 0x5C)); } };

struct WalkerWidget : ModuleWidget {
	WalkerWidget(Walker* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Walker.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		WalkerDisplay* d = createWidget<WalkerDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H)); d->module = module; addChild(d);

		// PLAYER
		const int player[7] = {Walker::FEEL_PARAM, Walker::ACTIVITY_PARAM, Walker::REGISTER_PARAM, Walker::ROAM_PARAM,
			Walker::OUT_PARAM, Walker::REPEAT_PARAM, Walker::SEED_PARAM};
		for (int i = 0; i < 7; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(i), Y_PLAYER)), module, player[i]));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(colX(7), Y_PLAYER)), module, Walker::NEW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(8), Y_PLAYER)), module, Walker::OCTAVE_PARAM));
		// HANDS
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(colX(0), Y_HANDS)), module, Walker::LOOSE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(1), Y_HANDS)), module, Walker::AHEAD_PARAM));
		const int trims[4] = {Walker::TIMING_PARAM, Walker::WEIGHT_PARAM, Walker::SLOP_PARAM, Walker::PITCH_PARAM};
		for (int i = 0; i < 4; i++) addParam(createParamCentered<Trimpot>(mm2px(Vec(colX(2 + i), Y_HANDS)), module, trims[i]));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(6), Y_HANDS)), module, Walker::LENGTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(7), Y_HANDS)), module, Walker::VARY_PARAM));
		addChild(createLightCentered<MediumLight<WAmberLight>>(mm2px(Vec(colX(8), Y_HANDS - 2.2f)), module, Walker::PLUCK_LIGHT));
		addChild(createLightCentered<MediumLight<WAmberLight>>(mm2px(Vec(colX(8), Y_HANDS + 5.4f)), module, Walker::LINK_LIGHT));
		// SOUND
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(sndX(0), Y_SOUND)), module, Walker::ATTACK_PARAM));
		for (int i = 1; i < 10; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(sndX(i), Y_SOUND)), module, Walker::ATTACK_PARAM + i));   // ATTACK..LEVEL
		// jacks
		const int in1[6] = {Walker::CLOCK_INPUT, Walker::RESET_INPUT, Walker::ROOT_INPUT, Walker::QUAL_INPUT, Walker::CHORD_INPUT, Walker::LAYOUT_INPUT};
		for (int i = 0; i < 6; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), Y_IN1)), module, in1[i]));
		const int in2[7] = {Walker::VOCT_IN_INPUT, Walker::GATE_IN_INPUT, Walker::VEL_IN_INPUT, Walker::ACTIVITY_INPUT, Walker::REGISTER_INPUT, Walker::LOOSE_INPUT, Walker::ATTACK_INPUT};
		for (int i = 0; i < 7; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), Y_IN2)), module, in2[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(OUT_COL0), Y_IN1)), module, Walker::AUDIO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(OUT_COL0 + 1), Y_IN1)), module, Walker::VOCT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(OUT_COL0), Y_IN2)), module, Walker::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(OUT_COL0 + 1), Y_IN2)), module, Walker::VEL_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Walker* m = dynamic_cast<Walker*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Cables (without Comper)"));
		menu->addChild(createIndexSubmenuItem("Clock pulses per beat", {"1", "2", "4", "24 (MIDI clock)"},
			[=]() { return m->ppbIndex; }, [=](size_t i) { m->ppbIndex = (int)i; }));
		menu->addChild(createIndexSubmenuItem("Tune length", {"From RESET", "4 bars", "8 bars", "12 bars", "16 bars", "24 bars", "32 bars"},
			[=]() { for (int i = 0; i < 7; i++) if (TUNE_CHOICES[i] == m->tuneBars) return (size_t)i; return (size_t)0; },
			[=](size_t i) { m->tuneBars = TUNE_CHOICES[std::min((size_t)6, i)]; }));
		menu->addChild(createMenuItem("Forget the learned tune", "", [=]() { m->forgetTune = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("V/OCT carries slides", "", &m->voctSlides));
	}
};

Model* modelWalker = createModel<Walker, WalkerWidget>("Walker");
