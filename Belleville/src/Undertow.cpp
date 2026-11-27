// Undertow: the bass player of the Belleville trio. Follows Strata's chart through the row, no cables.
#include "Common.hpp"
#include "dsp/UndertowEngine.hpp"

struct Undertow : Module {
	enum ParamIds { STYLE_PARAM, DENSITY_PARAM, FOLLOW_PARAM, OCTAVE_PARAM, SLIDE_PARAM, ACCENT_PARAM, DRIFT_PARAM, SPACE_PARAM, NEW_PARAM, NUM_PARAMS };
	enum InputIds { KICK_INPUT, DRIFT_INPUT, OCT_INPUT, NUM_INPUTS };
	enum OutputIds { VOCT_OUTPUT, GATE_OUTPUT, ACC_OUTPUT, SLIDE_OUTPUT, NUM_OUTPUTS };
	enum LightIds { LINK_LIGHT, NEW_LIGHT, NUM_LIGHTS };

	bv::UndertowEngine eng;
	LinkPort link;
	NotePlayer out;
	bv::TrioLink L;
	dsp::SchmittTrigger newTrig, kickTrig;
	dsp::PulseGenerator newFlash;
	int64_t lastStep = -2;
	uint32_t lastReset = 0;
	float sinceStep = 0.f;
	uint16_t kickAcc = 0;
	float glideMs = 70.f;
	int ctl = 0;

	Undertow() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configNamed(this, STYLE_PARAM, 0, bv::BS_COUNT - 1, 0, "Style", [](int v) { return std::string(bv::bassStyleName(v)); });
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.55f, "Density", "%", 0.f, 100.f);
		configParam(FOLLOW_PARAM, 0.f, 1.f, 0.4f, "Follow (root only, chord tones, counter-line)", "%", 0.f, 100.f);
		configNamed(this, OCTAVE_PARAM, -2, 1, 0, "Octave", [](int v) { return v > 0 ? "+" + std::to_string(v) : std::to_string(v); });
		configParam(SLIDE_PARAM, 0.f, 1.f, 0.2f, "Slide", "%", 0.f, 100.f);
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.3f, "Accent", "%", 0.f, 100.f);
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.2f, "Drift (re-rolls a few steps every two bars)", "%", 0.f, 100.f);
		configParam(SPACE_PARAM, 0.f, 1.f, 0.6f, "Space (leave room for the kick and the chords)", "%", 0.f, 100.f);
		configButton(NEW_PARAM, "New bass line");
		configInput(KICK_INPUT, "Kick (gates; unpatched = four to the floor)");
		configInput(DRIFT_INPUT, "Drift CV (+/-10 V)");
		configInput(OCT_INPUT, "Octave CV (1 V per octave)");
		configOutput(VOCT_OUTPUT, "Bass V/OCT");
		configOutput(GATE_OUTPUT, "Bass gate");
		configOutput(ACC_OUTPUT, "Accent gate");
		configOutput(SLIDE_OUTPUT, "Slide gate (high on a note that slides into the next)");
		link.attach(this);
	}
	void onRandomize() override { eng.reroll(random::u32()); }

	void process(const ProcessArgs& args) override {
		link.read(this);
		L = link.merged();
		if (++ctl >= 32) {
			ctl = 0;
			eng.style = (int)params[STYLE_PARAM].getValue();
			eng.density = params[DENSITY_PARAM].getValue();
			eng.follow = params[FOLLOW_PARAM].getValue();
			eng.octave = clamp((int)params[OCTAVE_PARAM].getValue() + (int)std::round(inputs[OCT_INPUT].getVoltage()), -3, 2);
			eng.slide = params[SLIDE_PARAM].getValue();
			eng.accent = params[ACCENT_PARAM].getValue();
			eng.drift = clamp(params[DRIFT_PARAM].getValue() + inputs[DRIFT_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			eng.space = params[SPACE_PARAM].getValue();
			if (!inputs[KICK_INPUT].isConnected()) eng.kickMask = 0x1111;
		}
		if (newTrig.process(params[NEW_PARAM].getValue())) { eng.reroll(random::u32()); newFlash.trigger(0.15f); }
		sinceStep += 1.f;
		if (L.hasStrata) {
			if (L.resetCount != lastReset) { lastReset = L.resetCount; lastStep = -2; out.reset(); }
			// learn the kick: each rising edge goes to the nearest sixteenth of this bar
			if (inputs[KICK_INPUT].isConnected() && kickTrig.process(inputs[KICK_INPUT].getVoltage(), 0.1f, 1.f) && L.step >= 0) {
				int64_t at = L.step + (sinceStep > 0.5f * L.stepSamples ? 1 : 0);
				kickAcc |= 1 << (int)(at % 16);
			}
			if (L.step != lastStep && L.step >= 0) {
				lastStep = L.step;
				sinceStep = 0.f;
				if (L.step % 16 == 0 && inputs[KICK_INPUT].isConnected()) { eng.kickMask = kickAcc; kickAcc = 0; }
				bv::NoteEvent e = eng.tick(L.chart, L.step, L.energy, L.stabMask);
				if (e.hit) out.schedule(e, bv::swingDelaySteps(L.step, L.swing) * L.stepSamples, L.stepSamples);
			}
		}
		float coef = 1.f - std::exp(-1.f / (std::max(1.f, glideMs) * 0.001f * args.sampleRate / 3.f));
		out.process(args.sampleRate, coef);
		bool live = L.hasStrata && L.running;
		outputs[VOCT_OUTPUT].setVoltage(out.voct[0]);
		outputs[GATE_OUTPUT].setVoltage(live && out.gate() ? 10.f : 0.f);
		outputs[ACC_OUTPUT].setVoltage(live && out.gate() && out.cur.accent ? 10.f : 0.f);
		outputs[SLIDE_OUTPUT].setVoltage(live && out.gateLeft > 0.f && out.cur.slide ? 10.f : 0.f);

		bv::TrioLink own;
		own.hasBass = true; own.bassMask = eng.barMask; own.bassNote = eng.lastNote;
		link.send(this, own);
		lights[LINK_LIGHT].setBrightness(L.hasStrata ? 1.f : 0.f);
		lights[NEW_LIGHT].setBrightness(newFlash.process(args.sampleTime) ? 1.f : 0.f);
	}

	void screen(ScreenInfo& s) {
		if (!L.hasStrata) { s.lines = 2; s.line[0] = "put me next to Strata"; s.line[1] = "(any order, touching)"; return; }
		int slot = L.step >= 0 ? L.chart.slotAtStep(L.step) : 0;
		static const char* F[3] = {"root", "chord tones", "counter-line"};
		std::string kick = inputs[KICK_INPUT].isConnected() ? "kick in" : "kick 4/4";
		s.line[0] = std::string(bv::bassStyleName(eng.style)) + " - " + bv::chordName(L.chart.root[slot], L.chart.qual[slot]) + " - " + bv::pcName(eng.lastNote) + std::to_string(eng.lastNote / 12 - 1);
		s.line[1] = std::string(F[std::min(2, (int)(eng.follow * 3.f))]) + " - " + kick;
		s.lines = 2;
		s.grid = true; s.mask = eng.barMask; s.pos = L.step >= 0 ? (int)(L.step % 16) : -1;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "glideMs", json_real(glideMs));
		json_object_set_new(r, "rng", json_integer(eng.rng.s));
		const float* A[6] = {eng.uOn, eng.uRole, eng.uLen, eng.uAcc, eng.uSlide, eng.uOct};
		json_t* d = json_array();
		for (int k = 0; k < 6; k++) for (int i = 0; i < 32; i++) json_array_append_new(d, json_real(A[k][i]));
		json_object_set_new(r, "dice", d);
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "glideMs")) glideMs = json_number_value(j);
		if (json_t* j = json_object_get(r, "rng")) eng.rng.seed((uint32_t)json_integer_value(j));
		float* A[6] = {eng.uOn, eng.uRole, eng.uLen, eng.uAcc, eng.uSlide, eng.uOct};
		if (json_t* d = json_object_get(r, "dice"))
			for (int k = 0; k < 6; k++) for (int i = 0; i < 32; i++) if (json_t* v = json_array_get(d, k * 32 + i)) A[k][i] = json_number_value(v);
	}
};

struct UndertowWidget : ModuleWidget {
	UndertowWidget(Undertow* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Undertow.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		using namespace lay;
		TrioScreen<Undertow>* d = createWidget<TrioScreen<Undertow>>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(T_W - 2 * DISP_X, T_DISP_H));
		d->module = module;
		addChild(d);
		for (int i = 0; i < 3; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(T_COLS[i], T_ROWS[0])), module, Undertow::STYLE_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(T_COLS[i], T_ROWS[1])), module, Undertow::OCTAVE_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(T_COLS[i], Y_IN)), module, Undertow::KICK_INPUT + i));
		}
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(T_COLS[0], T_ROWS[2])), module, Undertow::DRIFT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(T_COLS[1], T_ROWS[2])), module, Undertow::SPACE_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(T_COLS[2], T_ROWS[2])), module, Undertow::NEW_PARAM, Undertow::NEW_LIGHT));
		for (int i = 0; i < 4; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(T_JCOLS[i], Y_OUT)), module, Undertow::VOCT_OUTPUT + i));
		addChild(createLightCentered<SmallLight<LinkLight>>(mm2px(Vec(T_W - LINK_X_FROM_RIGHT, LINK_Y)), module, Undertow::LINK_LIGHT));
	}
	void appendContextMenu(Menu* menu) override {
		Undertow* m = getModule<Undertow>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Slide time", {"30 ms", "70 ms", "150 ms"},
			[=]() { return m->glideMs < 50.f ? 0 : m->glideMs < 110.f ? 1 : 2; },
			[=](int i) { static const float T[3] = {30.f, 70.f, 150.f}; m->glideMs = T[i]; }));
	}
};

Model* modelUndertow = createModel<Undertow, UndertowWidget>("Undertow");
