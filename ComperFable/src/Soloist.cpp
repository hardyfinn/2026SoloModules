#include "plugin.hpp"
#include "dsp/SoloHost.hpp"
#include "LinkPort.hpp"
#include "Progressions.hpp"

using namespace solo;

// ---- LAYOUT (same numbers in res/make_soloist_panel.py) ----
static const float S_DISP_X = 4.0f, S_DISP_Y = 12.5f, S_DISP_W = 144.4f, S_DISP_H = 13.0f;   // 30 HP
static const float S_COL_X0 = 10.5f, S_COL_DX = 18.77f;
static const float S_Y_FORM = 42.0f, S_Y_LINE = 66.0f, S_Y_HORN = 89.0f, S_Y_IN1 = 106.0f, S_Y_IN2 = 118.0f;
static float scolX(int i) { return S_COL_X0 + i * S_COL_DX; }

static const int S_PPB_CHOICES[4] = {1, 2, 4, 24};

struct Soloist : Module {
	enum ParamIds {
		FORM_PARAM, CHORUSES_PARAM, TRADE_PARAM, WITH_PARAM, SECTION_PARAM, SEED_PARAM, NEW_PARAM,
		DENSITY_PARAM, SPACE_PARAM, MOTIF_PARAM, OUTSIDE_PARAM, ENERGY_PARAM, LOOSE_PARAM, LENGTH_PARAM,
		HORN_PARAM, REGISTER_PARAM, OCTAVE_PARAM, ROOM_PARAM, LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds { CLOCK_INPUT, RESET_INPUT, CHORD_INPUT, PLAY_INPUT, TRADE_INPUT, ENERGY_INPUT, DENSITY_INPUT, OUTSIDE_INPUT, NUM_INPUTS };
	enum OutputIds { L_OUTPUT, R_OUTPUT, VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, BREATH_OUTPUT, PHRASE_OUTPUT, FORM_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NOTE_LIGHT, LINK_LIGHT, NUM_LIGHTS };

	SoloHost host;
	SIn in;
	SOut out;
	int ppbIndex = 0;
	dsp::BooleanTrigger newTrig;
	dsp::ClockDivider lightDiv;
	LinkPort port;
	bool linked = false, follower = false;

	Soloist() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(FORM_PARAM, 0.f, 2.f, 0.f, "Form", {"Tune (head, solos, trading, head out, end)", "Solo (blows forever)", "Cue (plays while PLAY is high)"});
		configParam(CHORUSES_PARAM, 1.f, 8.f, 2.f, "Solo choruses (in Tune)")->snapEnabled = true;
		configSwitch(TRADE_PARAM, 0.f, 3.f, 2.f, "Trade", {"Off", "Twos", "Fours", "Eights"});
		configSwitch(WITH_PARAM, 0.f, 2.f, 2.f, "Trade with", {"Drums", "Piano", "Drums and piano in turn"});
		configSwitch(SECTION_PARAM, 0.f, 1.f, 0.f, "Two Soloists", {"Horns (the second harmonises the head)", "Turns (they take turns and trade)"});
		configParam(SEED_PARAM, 0.f, 999.f, 1.f, "Seed (a new take)")->snapEnabled = true;
		configButton(NEW_PARAM, "New seed");
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density (how many notes)", "%", 0.f, 100.f);
		configParam(SPACE_PARAM, 0.f, 1.f, 0.4f, "Space (rests between phrases)", "%", 0.f, 100.f);
		configParam(MOTIF_PARAM, 0.f, 1.f, 0.4f, "Motif (develop an idea: repeat, invert, sequence)", "%", 0.f, 100.f);
		configParam(OUTSIDE_PARAM, 0.f, 1.f, 0.1f, "Outside", "%", 0.f, 100.f)->description =
			"0 inside the changes. Up: altered tensions, side-slips, tritone subs; past half, open intervals and a loose grid; at the top, free.";
		configParam(ENERGY_PARAM, 0.f, 1.f, 0.5f, "Energy (how far the solo builds)", "%", 0.f, 100.f);
		configParam(LOOSE_PARAM, 0.f, 2.f, 1.f, "Loose (0 machine, 1 a good player, 2 lazy)", "%", 0.f, 100.f);
		configParam(LENGTH_PARAM, 0.5f, 1.6f, 1.f, "Length (every note shorter or longer)", "x");
		configSwitch(HORN_PARAM, 0.f, 3.f, 0.f, "Horn", {"Tenor sax", "Trumpet", "Trombone", "Off (notes only)"});
		configParam(REGISTER_PARAM, 0.f, 1.f, 0.5f, "Register (low to high in the horn's range)", "%", 0.f, 100.f);
		struct OctQ : ParamQuantity {
			std::string getDisplayValueString() override { int o = (int)std::round(getValue()); return o == 0 ? "as played" : (o > 0 ? "+" : "") + std::to_string(o); }
		};
		configParam<OctQ>(OCTAVE_PARAM, -2.f, 2.f, 0.f, "Octave (V/OCT out only)", " oct")->snapEnabled = true;
		configParam(ROOM_PARAM, 0.f, 1.f, 0.35f, "Room (dry to a club)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f);
		for (int id : {(int)SEED_PARAM, (int)NEW_PARAM, (int)LEVEL_PARAM, (int)HORN_PARAM, (int)FORM_PARAM}) getParamQuantity(id)->randomizeEnabled = false;

		configInput(CLOCK_INPUT, "Clock (without Comper; pulses per beat in the right-click menu)");
		configInput(RESET_INPUT, "Reset (without Comper)");
		configInput(CHORD_INPUT, "Chord notes (poly V/oct, without Comper)");
		configInput(PLAY_INPUT, "Play (gate: plays while high; in Cue the solo starts when it rises)");
		configInput(TRADE_INPUT, "Trade (trigger: start trading at the next bar line)");
		configInput(ENERGY_INPUT, "ENERGY CV (0.1 per volt)");
		configInput(DENSITY_INPUT, "DENSITY CV (0.1 per volt)");
		configInput(OUTSIDE_INPUT, "OUTSIDE CV (0.1 per volt)");
		configOutput(L_OUTPUT, "Horn left");
		configOutput(R_OUTPUT, "Horn right");
		configOutput(VOCT_OUTPUT, "Notes (V/oct, 0 V = middle C, with scoops and vibrato)");
		configOutput(GATE_OUTPUT, "Gate (stays high through slurs)");
		configOutput(VEL_OUTPUT, "Velocity (0 to 10 V)");
		configOutput(BREATH_OUTPUT, "Breath (0 to 10 V envelope)");
		configOutput(PHRASE_OUTPUT, "Phrase (trigger at each phrase's first note)");
		configOutput(FORM_OUTPUT, "Form (trigger when the part changes: head, solo, trade, head out, end)");
		configLight(NOTE_LIGHT, "Note");
		configLight(LINK_LIGHT, "Band in the row");
		lightDiv.setDivision(64);

		port.attach(this);
		host.setSampleRate(APP->engine->getSampleRate());
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { host.setSampleRate(e.sampleRate); }
	void onReset() override { ppbIndex = 0; double sr = host.sr; host = SoloHost(); host.setSampleRate(sr); }

	float cv(int id) { return inputs[id].isConnected() ? 0.1f * inputs[id].getVoltage() : 0.f; }

	void process(const ProcessArgs& args) override {
		if (newTrig.process(params[NEW_PARAM].getValue() > 0.f))
			params[SEED_PARAM].setValue((float)(random::u32() % 1000));

		port.read(this);
		CompLink heard = port.merged();
		linked = heard.hasComp;
		// a Soloist further left leads; this one follows its form
		follower = port.ok[0] && port.got[0].sol[0].on;
		in.linkOk = port.ok[0] || port.ok[1];
		in.link = heard;
		in.follower = follower;

		in.clockConnected = inputs[CLOCK_INPUT].isConnected(); in.clock = inputs[CLOCK_INPUT].getVoltage();
		in.ppb = S_PPB_CHOICES[clamp(ppbIndex, 0, 3)];
		in.reset = inputs[RESET_INPUT].getVoltage();
		in.chordConnected = inputs[CHORD_INPUT].isConnected();
		in.chordChannels = inputs[CHORD_INPUT].getChannels();
		for (int c = 0; c < in.chordChannels; c++) in.chordV[c] = inputs[CHORD_INPUT].getVoltage(c);
		in.playConnected = inputs[PLAY_INPUT].isConnected(); in.play = inputs[PLAY_INPUT].getVoltage();
		in.tradeGate = inputs[TRADE_INPUT].getVoltage();

		in.form = clamp((int)std::round(params[FORM_PARAM].getValue()), 0, 2);
		in.choruses = clamp((int)std::round(params[CHORUSES_PARAM].getValue()), 1, 8);
		in.trade = clamp((int)std::round(params[TRADE_PARAM].getValue()), 0, 3);
		in.tradeWith = clamp((int)std::round(params[WITH_PARAM].getValue()), 0, 2);
		in.section = clamp((int)std::round(params[SECTION_PARAM].getValue()), 0, 1);
		in.seed = (int)params[SEED_PARAM].getValue();
		in.density = clamp(params[DENSITY_PARAM].getValue() + cv(DENSITY_INPUT), 0.f, 1.f);
		in.space = params[SPACE_PARAM].getValue();
		in.motif = params[MOTIF_PARAM].getValue();
		in.outside = clamp(params[OUTSIDE_PARAM].getValue() + cv(OUTSIDE_INPUT), 0.f, 1.f);
		in.energy = clamp(params[ENERGY_PARAM].getValue() + cv(ENERGY_INPUT), 0.f, 1.f);
		in.loose = params[LOOSE_PARAM].getValue();
		in.length = params[LENGTH_PARAM].getValue();
		in.kind = clamp((int)std::round(params[HORN_PARAM].getValue()), 0, KIND_OFF);
		in.reg = params[REGISTER_PARAM].getValue();
		in.octave = clamp((int)std::round(params[OCTAVE_PARAM].getValue()), -2, 2);
		in.room = params[ROOM_PARAM].getValue();
		in.level = params[LEVEL_PARAM].getValue();

		host.process(in, out, args.sampleTime);
		{	// tell the band what the horn is doing
			CompLink m; host.fillLink(in, m);
			port.send(this, m);
		}

		outputs[L_OUTPUT].setVoltage(out.L);
		outputs[R_OUTPUT].setVoltage(outputs[R_OUTPUT].isConnected() && !outputs[L_OUTPUT].isConnected() ? out.L : out.R);
		outputs[VOCT_OUTPUT].setVoltage(out.voct);
		outputs[GATE_OUTPUT].setVoltage(out.gate);
		outputs[VEL_OUTPUT].setVoltage(out.vel);
		outputs[BREATH_OUTPUT].setVoltage(out.breath);
		outputs[PHRASE_OUTPUT].setVoltage(out.phrase);
		outputs[FORM_OUTPUT].setVoltage(out.form);

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[NOTE_LIGHT].setBrightnessSmooth(out.gate > 1.f ? 1.f : 0.f, dt);
			lights[LINK_LIGHT].setBrightness(in.linkOk ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppb", json_integer(ppbIndex));
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppb")) ppbIndex = clamp((int)json_integer_value(j), 0, 3);
	}
};

// ---------------------------------------------------------------- display
static NVGcolor S_AMBER() { return nvgRGB(0xF2, 0xC2, 0x5C); }
static NVGcolor S_DIM() { return nvgRGB(0x8A, 0x7A, 0x55); }

struct SoloistDisplay : TransparentWidget {
	Soloist* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		std::string horn = "TENOR SAX", part = "SOLO 2 OF 3", who = "LEADER", seed = "1";
		double energy = 0.6, phi = 0;
		std::vector<SoloHost::Dot> dots;
		if (module) {
			SoloHost& h = module->host;
			const SIn& in = module->in;
			horn = in.kind >= KIND_OFF ? "NOTES ONLY" : horn::KIND_NAMES[in.kind];
			for (auto& c : horn) c = (char)std::toupper((unsigned char)c);
			int pn = std::max(0, std::min(NUM_PARTS - 1, h.partNow));
			part = PART_NAMES[pn];
			if (pn == P_SOLO && h.plan.mode == F_TUNE) {
				int u = (int)std::floor(h.phi / h.plan.U + 1e-9);
				int k = u - 1 + 1;
				if (h.plan.nSoloists > 1) part = h.plan.soloistAt(h.phi) == (module->follower ? 1 : 0) ? "SOLO (MINE)" : "SOLO (THEIRS)";
				else part = "SOLO " + std::to_string(std::max(1, std::min(h.plan.soloUnits, k))) + " OF " + std::to_string(h.plan.soloUnits);
			}
			if (pn == P_TRADE) {
				int t = turnAt(h.plan.asLink(), h.phi);
				static const char* const TN[5] = {"LEADER", "SECOND", "DRUMS", "PIANO", "END"};
				part = std::string("TRADE ") + std::to_string(h.plan.tradeSeg) + "s: " + (t >= 0 && t < 5 ? TN[t] : "");
			}
			if (pn == P_IDLE) part = module->in.linkOk ? "WAITING" : "NEXT TO COMPER, OR PATCH CLOCK";
			who = module->follower ? (h.plan.section == 0 ? "SECOND: HORNS" : "SECOND: TURNS") : "LEADER";
			seed = std::to_string(h.seedNow);
			energy = h.energyNow; phi = h.phi;
			dots.assign(h.dots.begin(), h.dots.end());
		}
		float hgt = box.size.y, w = box.size.x;
		nvgFillColor(args.vg, S_AMBER());
		nvgFontSize(args.vg, 15.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, 6.f, hgt * 0.45f, horn.c_str(), NULL);
		nvgFillColor(args.vg, S_DIM());
		nvgFontSize(args.vg, 11.f);
		nvgText(args.vg, 6.f, hgt * 0.84f, part.c_str(), NULL);
		// the last bars of the line: one dot per note, height by pitch
		float x0 = w * 0.42f, x1 = w * 0.78f, y0 = hgt * 0.15f, y1 = hgt * 0.85f;
		nvgFillColor(args.vg, S_AMBER());
		for (const auto& d : dots) {
			double age = phi - d.beat; if (age < 0 || age > 8.0) continue;
			float x = x1 - (float)(age / 8.0) * (x1 - x0);
			float y = y1 - (float)std::max(0.0, std::min(1.0, (d.pitch - 40) / 50.0)) * (y1 - y0);
			nvgBeginPath(args.vg); nvgCircle(args.vg, x, y, 1.6f); nvgFill(args.vg);
		}
		// energy
		float ex = w * 0.80f, ew = w * 0.06f;
		nvgBeginPath(args.vg); nvgRect(args.vg, ex, hgt * 0.62f, ew, hgt * 0.16f); nvgFillColor(args.vg, nvgRGB(0x2A, 0x26, 0x1C)); nvgFill(args.vg);
		nvgBeginPath(args.vg); nvgRect(args.vg, ex, hgt * 0.62f, ew * (float)std::max(0.0, std::min(1.0, energy)), hgt * 0.16f); nvgFillColor(args.vg, S_AMBER()); nvgFill(args.vg);
		nvgFillColor(args.vg, S_DIM());
		nvgFontSize(args.vg, 9.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, ex, hgt * 0.45f, "ENERGY", NULL);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, w - 5.f, hgt * 0.45f, who.c_str(), NULL);
		std::string sd = "SEED " + seed;
		nvgText(args.vg, w - 5.f, hgt * 0.84f, sd.c_str(), NULL);
	}
};

struct SAmberLight : GrayModuleLightWidget { SAmberLight() { addBaseColor(nvgRGB(0xF2, 0xC2, 0x5C)); } };

struct SoloistWidget : ModuleWidget {
	SoloistWidget(Soloist* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Soloist.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SoloistDisplay* d = createWidget<SoloistDisplay>(mm2px(Vec(S_DISP_X, S_DISP_Y)));
		d->box.size = mm2px(Vec(S_DISP_W, S_DISP_H)); d->module = module; addChild(d);

		// FORM
		const int form[6] = {Soloist::FORM_PARAM, Soloist::CHORUSES_PARAM, Soloist::TRADE_PARAM, Soloist::WITH_PARAM, Soloist::SECTION_PARAM, Soloist::SEED_PARAM};
		for (int i = 0; i < 6; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(scolX(i), S_Y_FORM)), module, form[i]));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(scolX(6), S_Y_FORM)), module, Soloist::NEW_PARAM));
		// LINE
		for (int i = 0; i < 7; i++) {
			int id = Soloist::DENSITY_PARAM + i;
			if (id == Soloist::OUTSIDE_PARAM) addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(scolX(i), S_Y_LINE)), module, id));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(scolX(i), S_Y_LINE)), module, id));
		}
		// HORN
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(scolX(0), S_Y_HORN)), module, Soloist::HORN_PARAM));
		for (int i = 1; i < 5; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(scolX(i), S_Y_HORN)), module, Soloist::HORN_PARAM + i));
		addChild(createLightCentered<MediumLight<SAmberLight>>(mm2px(Vec(scolX(6), S_Y_HORN)), module, Soloist::NOTE_LIGHT));
		addChild(createLightCentered<MediumLight<SAmberLight>>(mm2px(Vec(scolX(7), S_Y_HORN)), module, Soloist::LINK_LIGHT));
		// jacks
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(scolX(i), S_Y_IN1)), module, Soloist::CLOCK_INPUT + i));
		for (int i = 0; i < 8; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(scolX(i), S_Y_IN2)), module, Soloist::L_OUTPUT + i));
	}

	void appendContextMenu(Menu* menu) override {
		Soloist* m = dynamic_cast<Soloist*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Cables (without Comper)"));
		menu->addChild(createIndexSubmenuItem("Clock pulses per beat", {"1", "2", "4", "24 (MIDI clock)"},
			[=]() { return m->ppbIndex; }, [=](size_t i) { m->ppbIndex = (int)i; }));
	}
};

Model* modelSoloist = createModel<Soloist, SoloistWidget>("Soloist");
