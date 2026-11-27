// Signal: the melody player of the Belleville trio. Follows Strata's chart through the row, no cables.
#include "Common.hpp"
#include "dsp/SignalEngine.hpp"

struct Signal : Module {
	enum ParamIds { STYLE_PARAM, RANGE_PARAM, DENSITY_PARAM, MOTIF_PARAM, ANSWER_PARAM, OUTSIDE_PARAM, MUTATE_PARAM, FREEZE_PARAM, NEW_PARAM, NUM_PARAMS };
	enum InputIds { MUTATE_INPUT, FREEZE_INPUT, RANGE_INPUT, NUM_INPUTS };
	enum OutputIds { VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, MOD_OUTPUT, NUM_OUTPUTS };
	enum LightIds { LINK_LIGHT, FREEZE_LIGHT, NEW_LIGHT, NUM_LIGHTS };

	bv::SignalEngine eng;
	LinkPort link;
	NotePlayer out;
	bv::TrioLink L;
	dsp::SchmittTrigger newTrig;
	dsp::PulseGenerator newFlash;
	int64_t lastStep = -2;
	uint32_t lastReset = 0;
	float modOut = 0.f;
	int ctl = 0;

	Signal() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configNamed(this, STYLE_PARAM, 0, bv::SS_COUNT - 1, 0, "Style", [](int v) { return std::string(bv::sigStyleName(v)); });
		configParam(RANGE_PARAM, 0.f, 1.f, 0.5f, "Range", "%", 0.f, 100.f);
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density", "%", 0.f, 100.f);
		configNamed(this, MOTIF_PARAM, 0, 11, 7, "Motif length", [](int v) { return std::to_string(bv::motifLen(v)) + " sixteenths"; });
		configParam(ANSWER_PARAM, 0.f, 1.f, 0.6f, "Answer (play in the gaps, land on the chord)", "%", 0.f, 100.f);
		configParam(OUTSIDE_PARAM, 0.f, 1.f, 0.1f, "Outside (chromatic notes)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.3f, "Mutate (the motif evolves each time round)", "%", 0.f, 100.f);
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze (keep this version)", {"Off", "On"});
		configButton(NEW_PARAM, "New motif");
		configInput(MUTATE_INPUT, "Mutate CV (+/-10 V)");
		configInput(FREEZE_INPUT, "Freeze gate");
		configInput(RANGE_INPUT, "Range CV (+/-10 V)");
		configOutput(VOCT_OUTPUT, "Melody V/OCT");
		configOutput(GATE_OUTPUT, "Melody gate");
		configOutput(VEL_OUTPUT, "Velocity (0-10 V)");
		configOutput(MOD_OUTPUT, "Contour (0-10 V, high notes high)");
		link.attach(this);
	}
	void onRandomize() override { eng.newMotif(random::u32()); }
	void onReset() override { eng.registerOct = 0; }

	void process(const ProcessArgs& args) override {
		link.read(this);
		L = link.merged();
		if (++ctl >= 32) {
			ctl = 0;
			int st = (int)params[STYLE_PARAM].getValue();
			if (st != eng.style) { eng.style = st; }
			eng.range = params[RANGE_PARAM].getValue();
			eng.density = params[DENSITY_PARAM].getValue();
			eng.motifSel = (int)params[MOTIF_PARAM].getValue();
			eng.answer = params[ANSWER_PARAM].getValue();
			eng.outside = params[OUTSIDE_PARAM].getValue();
			eng.mutate = clamp(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			eng.freeze = params[FREEZE_PARAM].getValue() > 0.5f || inputs[FREEZE_INPUT].getVoltage() > 1.f;
		}
		if (newTrig.process(params[NEW_PARAM].getValue())) { eng.newMotif(random::u32()); newFlash.trigger(0.15f); }
		if (L.hasStrata) {
			if (L.resetCount != lastReset) { lastReset = L.resetCount; lastStep = -2; out.reset(); }
			if (L.step != lastStep && L.step >= 0) {
				lastStep = L.step;
				bv::NoteEvent e = eng.tick(L.chart, L.step, L.energy, L.stabMask, L.hasBass ? L.bassMask : 0, inputs[RANGE_INPUT].getVoltage() / 10.f);
				if (e.hit) out.schedule(e, bv::swingDelaySteps(L.step, L.swing) * L.stepSamples, L.stepSamples);
			}
		}
		out.process(args.sampleRate);
		bool live = L.hasStrata && L.running;
		modOut += (out.cur.mod * 10.f - modOut) * 0.002f;
		outputs[VOCT_OUTPUT].setVoltage(out.voct[0]);
		outputs[GATE_OUTPUT].setVoltage(live && out.gate() ? 10.f : 0.f);
		outputs[VEL_OUTPUT].setVoltage(out.cur.vel[0] * 10.f);
		outputs[MOD_OUTPUT].setVoltage(modOut);

		bv::TrioLink own;
		own.hasSig = true; own.sigMask = eng.barMask; own.sigNote = eng.lastNote;
		link.send(this, own);
		lights[LINK_LIGHT].setBrightness(L.hasStrata ? 1.f : 0.f);
		lights[FREEZE_LIGHT].setBrightness(eng.freeze ? 1.f : 0.f);
		lights[NEW_LIGHT].setBrightness(newFlash.process(args.sampleTime) ? 1.f : 0.f);
	}

	void screen(ScreenInfo& s) {
		if (!L.hasStrata) { s.lines = 2; s.line[0] = "put me next to Strata"; s.line[1] = "(any order, touching)"; return; }
		s.line[0] = std::string(bv::sigStyleName(eng.style)) + " - " + std::to_string(bv::motifLen(eng.motifSel)) + "-step motif";
		s.line[1] = "gen " + std::to_string(eng.gen) + (eng.freeze ? " (frozen)" : "") + " - " + bv::pcName(eng.lastNote) + std::to_string(eng.lastNote / 12 - 1);
		s.lines = 2;
		s.grid = true; s.mask = eng.barMask; s.pos = L.step >= 0 ? (int)(L.step % 16) : -1;
	}

	static json_t* motifJson(const bv::MotifStep* m) {
		json_t* a = json_array();
		for (int i = 0; i < 64; i++) {
			json_array_append_new(a, json_real(m[i].uOn)); json_array_append_new(a, json_integer(m[i].move));
			json_array_append_new(a, json_real(m[i].uLen)); json_array_append_new(a, json_real(m[i].uVel)); json_array_append_new(a, json_real(m[i].uOut));
		}
		return a;
	}
	static void motifFrom(json_t* a, bv::MotifStep* m) {
		if (!a || json_array_size(a) < 320) return;
		for (int i = 0; i < 64; i++) {
			m[i].uOn = json_number_value(json_array_get(a, i * 5)); m[i].move = json_integer_value(json_array_get(a, i * 5 + 1));
			m[i].uLen = json_number_value(json_array_get(a, i * 5 + 2)); m[i].uVel = json_number_value(json_array_get(a, i * 5 + 3));
			m[i].uOut = json_number_value(json_array_get(a, i * 5 + 4));
		}
	}
	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "register", json_integer(eng.registerOct));
		json_object_set_new(r, "rng", json_integer(eng.rng.s));
		json_object_set_new(r, "gen", json_integer(eng.gen));
		json_object_set_new(r, "motif", motifJson(eng.motif));
		json_object_set_new(r, "home", motifJson(eng.home));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "register")) eng.registerOct = json_integer_value(j);
		if (json_t* j = json_object_get(r, "rng")) eng.rng.seed((uint32_t)json_integer_value(j));
		if (json_t* j = json_object_get(r, "gen")) eng.gen = json_integer_value(j);
		motifFrom(json_object_get(r, "motif"), eng.motif);
		motifFrom(json_object_get(r, "home"), eng.home);
	}
};

struct SignalWidget : ModuleWidget {
	SignalWidget(Signal* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Signal.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		using namespace lay;
		TrioScreen<Signal>* d = createWidget<TrioScreen<Signal>>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(T_W - 2 * DISP_X, T_DISP_H));
		d->module = module;
		addChild(d);
		for (int i = 0; i < 3; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(T_COLS[i], T_ROWS[0])), module, Signal::STYLE_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(T_COLS[i], T_ROWS[1])), module, Signal::MOTIF_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(T_COLS[i], Y_IN)), module, Signal::MUTATE_INPUT + i));
		}
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(T_COLS[0], T_ROWS[2])), module, Signal::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightBezelLatch<WhiteLight>>(mm2px(Vec(T_COLS[1], T_ROWS[2])), module, Signal::FREEZE_PARAM, Signal::FREEZE_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(T_COLS[2], T_ROWS[2])), module, Signal::NEW_PARAM, Signal::NEW_LIGHT));
		for (int i = 0; i < 4; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(T_JCOLS[i], Y_OUT)), module, Signal::VOCT_OUTPUT + i));
		addChild(createLightCentered<SmallLight<LinkLight>>(mm2px(Vec(T_W - LINK_X_FROM_RIGHT, LINK_Y)), module, Signal::LINK_LIGHT));
	}
	void appendContextMenu(Menu* menu) override {
		Signal* m = getModule<Signal>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Melody register", {"-2 octaves", "-1 octave", "Normal", "+1 octave", "+2 octaves"},
			[=]() { return m->eng.registerOct + 2; },
			[=](int i) { m->eng.registerOct = i - 2; }));
	}
};

Model* modelSignal = createModel<Signal, SignalWidget>("Signal");
