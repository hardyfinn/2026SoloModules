// Strata: chords, and the harmony leader of the Belleville trio.
#include "Common.hpp"
#include "dsp/StrataEngine.hpp"

struct Strata : Module {
	enum ParamIds { STYLE_PARAM, KEY_PARAM, MODE_PARAM, PROG_PARAM, RHYTHM_PARAM, VOICING_PARAM, COLOUR_PARAM, SPREAD_PARAM,
		LENGTH_PARAM, DRIFT_PARAM, HOLD_PARAM, NEXT_PARAM, NUM_PARAMS };
	enum InputIds { CLOCK_INPUT, RESET_INPUT, CHORDS_INPUT, ENERGY_INPUT, NUM_INPUTS };
	enum OutputIds { VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, CHANGE_OUTPUT, NUM_OUTPUTS };
	enum LightIds { LINK_LIGHT, HOLD_LIGHT, NEXT_LIGHT, NUM_LIGHTS };

	bv::StrataEngine eng;
	LinkPort link;
	NotePlayer out;
	dsp::SchmittTrigger clockTrig, resetTrig, nextTrig;
	int ppqn = 4;                 // clock pulses per quarter note (4 = sixteenths)
	float swing = 0.5f;
	int pulses = 0;
	int64_t step = -1;
	float sinceStep = 0.f, sinceClock = 1e9f, stepSamples = 5000.f;
	uint32_t resetCount = 0;
	int ctl = 0;
	int extSig = 0;               // a fingerprint of the CHORDS notes, to spot changes

	Strata() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configNamed(this, STYLE_PARAM, 0, bv::ST_COUNT - 1, 0, "Style", [](int v) { return std::string(bv::styleName(v)); });
		configNamed(this, KEY_PARAM, 0, 11, 5, "Key", [](int v) { return std::string(bv::pcName(v)); });
		configNamed(this, MODE_PARAM, 0, bv::M_COUNT - 1, 0, "Mode (auto = the progression's own)", [](int v) { return std::string(bv::modeName(v)); });
		configNamed(this, PROG_PARAM, 0, 15, 0, "Progression", [this](int v) { return std::string(bv::progDef(bv::progIndex(eng.style, v)).name); });
		configNamed(this, RHYTHM_PARAM, 0, bv::RH_COUNT - 1, 2, "Rhythm", [](int v) { return std::string(bv::rhythmName(v)); });
		configNamed(this, VOICING_PARAM, 0, bv::VO_COUNT - 1, 0, "Voicing", [](int v) { return std::string(bv::voicingName(v)); });
		configParam(COLOUR_PARAM, 0.f, 1.f, 0.5f, "Colour (plain triads to rich extensions, as written at noon)", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.4f, "Spread", "%", 0.f, 100.f);
		configNamed(this, LENGTH_PARAM, 0, 3, 1, "Chord length", [](int v) { static const char* N[4] = {"half", "as written", "double", "x4"}; return std::string(N[std::max(0, std::min(3, v))]); });
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.25f, "Drift (chords wander once a loop)", "%", 0.f, 100.f);
		configSwitch(HOLD_PARAM, 0.f, 1.f, 0.f, "Hold (stop drifting)", {"Off", "On"});
		configButton(NEXT_PARAM, "Next progression (at the end of the loop)");
		configInput(CLOCK_INPUT, "Clock (sixteenths by default; right-click for 24 or 48 PPQN)");
		configInput(RESET_INPUT, "Reset");
		configInput(CHORDS_INPUT, "Chords (poly V/OCT, e.g. from Hapax; the trio follows them)");
		configInput(ENERGY_INPUT, "Energy (0-10 V)");
		configOutput(VOCT_OUTPUT, "Chord V/OCT (poly)");
		configOutput(GATE_OUTPUT, "Chord gates (poly)");
		configOutput(VEL_OUTPUT, "Velocity (poly, 0-10 V)");
		configOutput(CHANGE_OUTPUT, "Chord change trigger");
		link.attach(this);
		eng.checkReload();
	}

	void onReset() override { ppqn = 4; swing = 0.5f; eng.registerOct = 0; eng.loaded[0] = -1; eng.checkReload(); }
	void onRandomize() override { eng.newSeed(random::u32()); }

	void readKnobs() {
		eng.style = (int)params[STYLE_PARAM].getValue();
		eng.key = (int)params[KEY_PARAM].getValue();
		eng.mode = (int)params[MODE_PARAM].getValue();
		eng.progSel = (int)params[PROG_PARAM].getValue();
		eng.rhythm = (int)params[RHYTHM_PARAM].getValue();
		eng.voicing = (int)params[VOICING_PARAM].getValue();
		eng.colour = params[COLOUR_PARAM].getValue();
		eng.spread = params[SPREAD_PARAM].getValue();
		eng.lengthSel = (int)params[LENGTH_PARAM].getValue();
		eng.drift = params[DRIFT_PARAM].getValue();
		eng.hold = params[HOLD_PARAM].getValue() > 0.5f;
		eng.energy = inputs[ENERGY_INPUT].isConnected() ? clamp(inputs[ENERGY_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 0.5f;
	}
	void readChords() {
		int n = inputs[CHORDS_INPUT].isConnected() ? inputs[CHORDS_INPUT].getChannels() : 0;
		int notes[16], sig = n;
		for (int c = 0; c < n; c++) { notes[c] = (int)std::round(inputs[CHORDS_INPUT].getVoltage(c) * 12.f) + 60; sig = sig * 31 + notes[c]; }
		if (n >= 2 && sig != extSig) { eng.setExternal(n, notes); eng.markExternalChange(); eng.buildChart(); }
		if (n < 2 && eng.external) eng.setExternal(0, notes);
		extSig = n >= 2 ? sig : 0;
	}

	void process(const ProcessArgs& args) override {
		link.read(this);
		if (++ctl >= 32) {
			ctl = 0;
			readKnobs();
			readChords();
			eng.checkReload();
		}
		// NEXT: the progression after this one, from the end of the loop (the PROG knob moves with it)
		if (nextTrig.process(params[NEXT_PARAM].getValue())) eng.nextArmed = !eng.nextArmed;
		int prevSel = eng.progSel;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			step = -1; pulses = 0; resetCount++; eng.lastSlot = -1; out.reset();
		}
		sinceStep += 1.f; sinceClock += 1.f;
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			int per = std::max(1, ppqn / 4);
			if (pulses % per == 0) {
				if (step >= 0 && sinceStep < args.sampleRate * 4.f) stepSamples = sinceStep;
				sinceStep = 0.f;
				step++;
				bv::NoteEvent e = eng.tick(step);
				if (eng.progSel != prevSel) params[PROG_PARAM].setValue((float)eng.progSelWrapped());
				if (e.hit) out.schedule(e, bv::swingDelaySteps(step, swing) * stepSamples, stepSamples);
				else if (e.change) out.changePulse = 0.001f * args.sampleRate;
			}
			pulses++;
			sinceClock = 0.f;
		}
		out.process(args.sampleRate);
		bool running = sinceClock < args.sampleRate * 2.f;
		int ch = std::max(1, out.channels);
		outputs[VOCT_OUTPUT].setChannels(ch); outputs[GATE_OUTPUT].setChannels(ch); outputs[VEL_OUTPUT].setChannels(ch);
		for (int c = 0; c < ch; c++) {
			outputs[VOCT_OUTPUT].setVoltage(out.voct[c], c);
			outputs[GATE_OUTPUT].setVoltage(out.gate() ? 10.f : 0.f, c);
			outputs[VEL_OUTPUT].setVoltage(out.cur.vel[c] * 10.f, c);
		}
		outputs[CHANGE_OUTPUT].setVoltage(out.changePulse > 0.f ? 10.f : 0.f);

		bv::TrioLink own;
		own.hasStrata = true; own.running = running; own.resetCount = resetCount; own.step = step;
		own.stepSamples = stepSamples; own.swing = swing; own.energy = eng.energy; own.style = eng.style;
		own.chart = eng.chart; own.stabMask = eng.plannedMask();
		link.send(this, own);
		lights[LINK_LIGHT].setBrightness(link.ok[0] || link.ok[1] ? 1.f : 0.f);
		lights[HOLD_LIGHT].setBrightness(eng.hold ? 1.f : 0.f);
		lights[NEXT_LIGHT].setBrightness(eng.nextArmed ? 1.f : 0.f);
	}

	void screen(ScreenInfo& s) {
		const bv::Chart& c = eng.chart;
		int slot = step >= 0 ? c.slotAtStep(step) : 0;
		std::string l0;
		if (eng.external) {
			l0 = "CHORDS in: ";
			for (int i = 0; i < eng.extN && i < 6; i++) l0 += std::string(bv::pcName(eng.extNotes[i])) + " ";
		} else {
			for (int i = 0; i < c.n && i < 8; i++) {
				std::string nm = bv::chordName(c.root[i], c.qual[i]);
				l0 += (i == slot ? "[" + nm + "]" : nm) + " ";
			}
		}
		s.line[0] = l0;
		s.line[1] = std::string(bv::styleName(eng.style)) + " - " + bv::pcName(eng.key) + " " + bv::modeName(eng.effectiveMode()) + " - " + bv::progDef(std::max(0, eng.progId)).name;
		int bars = std::max(1, c.loopSteps / 16);
		int bar = step >= 0 ? (int)((step % c.loopSteps) / 16) + 1 : 1;
		s.line[2] = "bar " + std::to_string(bar) + "/" + std::to_string(bars) + " - " + bv::rhythmName(eng.rhythm) + " - " + bv::voicingName(eng.voicing);
		std::string l3;
		if (sinceClock > 96000.f) l3 = "waiting for CLOCK";
		else if (eng.nextArmed) l3 = std::string("NEXT > ") + bv::progDef(bv::progIndex(eng.style, eng.progSelWrapped() + 1)).name;
		else if (eng.hold) l3 = "HOLD - chords stay put";
		else l3 = "DRIFT " + std::to_string(eng.driftedCount) + "/" + std::to_string(eng.baseN) + " chords moved";
		s.line[3] = l3;
		s.lines = 4;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "ppqn", json_integer(ppqn));
		json_object_set_new(r, "swing", json_real(swing));
		json_object_set_new(r, "register", json_integer(eng.registerOct));
		json_object_set_new(r, "rng", json_integer(eng.rng.s));
		json_object_set_new(r, "progId", json_integer(eng.progId));
		json_t* d = json_array(); json_t* q = json_array();
		for (int i = 0; i < 16; i++) { json_array_append_new(d, json_integer(eng.curDeg[i])); json_array_append_new(q, json_integer(eng.curQ[i])); }
		json_object_set_new(r, "deg", d); json_object_set_new(r, "qual", q);
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "ppqn")) ppqn = json_integer_value(j);
		if (json_t* j = json_object_get(r, "swing")) swing = json_number_value(j);
		if (json_t* j = json_object_get(r, "register")) eng.registerOct = json_integer_value(j);
		if (json_t* j = json_object_get(r, "rng")) eng.rng.seed((uint32_t)json_integer_value(j));
		readKnobs();
		eng.checkReload();
		json_t* pid = json_object_get(r, "progId");
		json_t* d = json_object_get(r, "deg"); json_t* q = json_object_get(r, "qual");
		if (pid && d && q && json_integer_value(pid) == eng.progId) {
			for (int i = 0; i < 16; i++) {
				if (json_t* a = json_array_get(d, i)) eng.curDeg[i] = json_integer_value(a);
				if (json_t* b = json_array_get(q, i)) eng.curQ[i] = json_integer_value(b);
			}
			eng.driftedCount = 0;
			for (int i = 0; i < eng.baseN; i++) if (eng.curDeg[i] != eng.baseDeg[i] || eng.curQ[i] != eng.baseQ[i]) eng.driftedCount++;
			eng.buildChart();
		}
	}
};

struct StrataWidget : ModuleWidget {
	StrataWidget(Strata* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Strata.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		TrioScreen<Strata>* d = createWidget<TrioScreen<Strata>>(mm2px(Vec(lay::DISP_X, lay::DISP_Y)));
		d->box.size = mm2px(Vec(lay::S_W - 2 * lay::DISP_X, lay::S_DISP_H));
		d->module = module;
		addChild(d);
		using namespace lay;
		int row0[4] = {Strata::STYLE_PARAM, Strata::KEY_PARAM, Strata::MODE_PARAM, Strata::PROG_PARAM};
		int row1[4] = {Strata::RHYTHM_PARAM, Strata::VOICING_PARAM, Strata::COLOUR_PARAM, Strata::SPREAD_PARAM};
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(S_COLS[i], S_ROWS[0])), module, row0[i]));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(S_COLS[i], S_ROWS[1])), module, row1[i]));
		}
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(S_COLS[0], S_ROWS[2])), module, Strata::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(S_COLS[1], S_ROWS[2])), module, Strata::DRIFT_PARAM));
		addParam(createLightParamCentered<VCVLightBezelLatch<WhiteLight>>(mm2px(Vec(S_COLS[2], S_ROWS[2])), module, Strata::HOLD_PARAM, Strata::HOLD_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(S_COLS[3], S_ROWS[2])), module, Strata::NEXT_PARAM, Strata::NEXT_LIGHT));
		for (int i = 0; i < 4; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(S_COLS[i], Y_IN)), module, Strata::CLOCK_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(S_COLS[i], Y_OUT)), module, Strata::VOCT_OUTPUT + i));
		}
		addChild(createLightCentered<SmallLight<LinkLight>>(mm2px(Vec(S_W - LINK_X_FROM_RIGHT, LINK_Y)), module, Strata::LINK_LIGHT));
	}
	void appendContextMenu(Menu* menu) override {
		Strata* m = getModule<Strata>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock input", {"Sixteenths (4 PPQN)", "24 PPQN", "48 PPQN"},
			[=]() { return m->ppqn == 24 ? 1 : m->ppqn == 48 ? 2 : 0; },
			[=](int i) { m->ppqn = i == 1 ? 24 : i == 2 ? 48 : 4; m->pulses = 0; }));
		menu->addChild(createIndexSubmenuItem("Swing (the whole trio)", {"Straight", "54%", "57%", "60%", "63%"},
			[=]() { float s = m->swing; return s < 0.52f ? 0 : s < 0.555f ? 1 : s < 0.585f ? 2 : s < 0.615f ? 3 : 4; },
			[=](int i) { static const float S[5] = {0.5f, 0.54f, 0.57f, 0.6f, 0.63f}; m->swing = S[i]; }));
		menu->addChild(createIndexSubmenuItem("Chord register", {"-2 octaves", "-1 octave", "Normal", "+1 octave", "+2 octaves"},
			[=]() { return m->eng.registerOct + 2; },
			[=](int i) { m->eng.registerOct = i - 2; }));
	}
};

Model* modelStrata = createModel<Strata, StrataWidget>("Strata");
