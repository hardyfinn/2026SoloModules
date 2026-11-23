#include "plugin.hpp"
#include "dsp/Pond.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 14.0f, DISP_W = 101.76f, DISP_H = 17.0f;
static const float R1_Y = 46.0f;
static const float R1_X[6] = {12.0f, 29.0f, 46.0f, 63.0f, 82.0f, 99.5f};   // SPECIES A, B, MIX, FROGS, HUSH, CALL
static const float R2_Y = 66.0f;
static const float R2_X[6] = {12.0f, 29.5f, 47.0f, 64.5f, 82.0f, 99.5f};   // PITCH PULSE LENGTH ROUGH VARIETY SPACE
static const float R3_Y = 87.5f;
static const float R3_X[5] = {13.0f, 32.0f, 50.0f, 70.0f, 97.0f};          // TEMP BUSY BOUTS TALK RETURN
static const float JX0 = 8.4f, JDX = 11.9f;
static const float JA_Y = 104.0f, JB_Y = 117.0f;

struct PondModule : Module {
	enum ParamIds {
		SPA_PARAM, SPB_PARAM, MIX_PARAM, FROGS_PARAM, HUSH_PARAM, CALL_PARAM,
		PITCH_PARAM, PULSE_PARAM, LENGTH_PARAM, ROUGH_PARAM, VARIETY_PARAM, SPACE_PARAM,
		TEMP_PARAM, BUSY_PARAM, BOUTS_PARAM, TALK_PARAM, RETURN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, PULSE_INPUT, LENGTH_INPUT, TEMP_INPUT, BUSY_INPUT, BOUTS_INPUT, TALK_INPUT, MIX_INPUT, SPACE_INPUT,
		CLOCK_INPUT, CALL_INPUT, HUSH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { TRIG_OUTPUT, GATE_OUTPUT, PITCH_OUTPUT, ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HUSH_LIGHT, CALL_LIGHT, NUM_LIGHTS };

	pond::Pond engine;
	uint32_t seed = 1;
	dsp::SchmittTrigger clockTrig, hushTrig, hushBtnTrig, callBtnTrig;
	dsp::SchmittTrigger callTrig[pond::MAX_FROGS];
	dsp::PulseGenerator trigGen[pond::MAX_FROGS];
	dsp::ClockDivider lightDiv;
	float callFlash = 0.f;

	// for the display (written on the audio thread, read by the UI)
	int dispA = 0, dispB = 1, dispN = 8, dispCalling = 0;
	float dispMix = 0.f, dispTemp = 20.f, dispFear = 0.f;
	float dispX[pond::MAX_FROGS] = {}, dispY[pond::MAX_FROGS] = {}, dispLevel[pond::MAX_FROGS] = {};
	bool dispB_[pond::MAX_FROGS] = {};

	PondModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		std::vector<std::string> names;
		for (int i = 0; i < pond::NUM_SPECIES; i++) {
			std::string s = pond::SPECIES[i].name;
			names.push_back(s);
		}
		configSwitch(SPA_PARAM, 0.f, pond::NUM_SPECIES - 1, 0.f, "Species A", names)->description =
			"The species most of the pond is.";
		configSwitch(SPB_PARAM, 0.f, pond::NUM_SPECIES - 1, 1.f, "Species B", names)->description =
			"A second species. MIX sets how many frogs are this one.";
		configParam(MIX_PARAM, 0.f, 1.f, 0.f, "Mix (share of species B)", "%", 0.f, 100.f)->description =
			"Turn up and frogs change to species B one by one, spread over the pond. A frog changes between its calls.";
		configParam(FROGS_PARAM, 1.f, (float)pond::MAX_FROGS, 8.f, "Frogs")->snapEnabled = true;
		configButton(HUSH_PARAM, "Hush (a predator: the pond falls silent while held)");
		configButton(CALL_PARAM, "Call (the keenest quiet frog calls now)");

		configParam(PITCH_PARAM, -12.f, 12.f, 0.f, "Pitch", " semitones");
		struct MulQuantity : ParamQuantity {
			int kind = 0;
			std::string getDisplayValueString() override {
				double k = getValue();
				return string::f("x%.2f", kind == 0 ? pond::mapPulse(k) : pond::mapLength(k));
			}
		};
		MulQuantity* q = configParam<MulQuantity>(PULSE_PARAM, 0.f, 1.f, 0.5f, "Pulse rate");
		q->kind = 0; q->description = "Speed of the pulses inside a note (the trill rate). Species that whistle without pulses ignore it.";
		q = configParam<MulQuantity>(LENGTH_PARAM, 0.f, 1.f, 0.5f, "Note length");
		q->kind = 1;
		configParam(ROUGH_PARAM, 0.f, 1.f, 0.5f, "Rough", "%", 0.f, 100.f)->description =
			"Centre = the species' own voice. Down: cleaner and steadier. Up: raspy, wobbly, breathy.";
		configParam(VARIETY_PARAM, 0.f, 1.f, 0.5f, "Variety", "%", 0.f, 100.f)->description =
			"How different the frogs are from each other: size (pitch), tempo, loudness.";
		configParam(SPACE_PARAM, 0.f, 1.f, 0.4f, "Space", "%", 0.f, 100.f)->description =
			"How wide and far away the pond is: stereo spread, distance and night air.";

		struct TempQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double c = pond::mapTempC(getValue());
				return string::f("%.1f °C (speed x%.2f)", c, pond::tempSpeed(c));
			}
		};
		configParam<TempQuantity>(TEMP_PARAM, 0.f, 1.f, 0.6f, "Temperature")->description =
			"Warmer frogs pulse and call faster. Below about 10 °C they go quiet.";
		configParam(BUSY_PARAM, 0.f, 1.f, 0.7f, "Busy", "%", 0.f, 100.f)->description =
			"How keen the pond is to sing. 0 = silent (CALL still works).";
		configParam(BOUTS_PARAM, 0.f, 1.f, 0.4f, "Bouts", "%", 0.f, 100.f)->description =
			"Excitement spreads from frog to frog, then they tire: the pond swells and dies away in waves.";
		struct TalkQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = getValue();
				if (k < -0.05) return string::f("Synchronise %d%%", (int)std::round(-k * 100));
				if (k > 0.05) return string::f("Call and answer %d%%", (int)std::round(k * 100));
				return "Independent";
			}
		};
		configParam<TalkQuantity>(TALK_PARAM, -1.f, 1.f, 0.7f, "Talk")->description =
			"Left: frogs pull each other into step and call together. Right: they wait for a gap and answer each other.";
		struct ReturnQuantity : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%.1f s", pond::mapReturn(getValue())); }
		};
		configParam<ReturnQuantity>(RETURN_PARAM, 0.f, 1.f, 0.53f, "Return")->description =
			"After a HUSH: how long the frogs stay frightened. The bravest come back first, tentatively; each call heard gives the others courage.";

		configInput(VOCT_INPUT, "V/OCT (polyphonic: one frog per channel)");
		configInput(PULSE_INPUT, "Pulse rate CV");
		configInput(LENGTH_INPUT, "Note length CV");
		configInput(TEMP_INPUT, "Temperature CV");
		configInput(BUSY_INPUT, "Busy CV");
		configInput(BOUTS_INPUT, "Bouts CV");
		configInput(TALK_INPUT, "Talk CV (±5 V = full sweep)");
		configInput(MIX_INPUT, "Mix CV");
		configInput(SPACE_INPUT, "Space CV");
		configInput(CLOCK_INPUT, "Clock: frogs that are ready call on the next edge");
		configInput(CALL_INPUT, "Call trigger (mono: the keenest frog; polyphonic: channel n = frog n)");
		configInput(HUSH_INPUT, "Hush: a trigger scares the pond silent; a held gate keeps it silent");
		configOutput(TRIG_OUTPUT, "Call trigger (polyphonic, one per frog)");
		configOutput(GATE_OUTPUT, "Calling gate (polyphonic, one per frog)");
		configOutput(PITCH_OUTPUT, "Frog pitch, V/OCT from C4 (polyphonic)");
		configOutput(ENV_OUTPUT, "Pond loudness 0-10 V");
		configOutput(L_OUTPUT, "Left (mono sum when R is not patched)");
		configOutput(R_OUTPUT, "Right");

		lightDiv.setDivision(64);
		seed = random::u32() % 100000u + 1u;
		engine.reseed(seed);
		engine.setSampleRate(48000.0);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); engine.reseed(seed); }
	void newPond() { seed = random::u32() % 100000u + 1u; engine.reseed(seed); }

	float cv(int p, int in, float scale, float lo, float hi) {
		float v = params[p].getValue();
		if (inputs[in].isConnected()) v += inputs[in].getVoltage() * scale;
		return clamp(v, lo, hi);
	}

	void process(const ProcessArgs& args) override {
		pond::Params p;
		p.spA = (int)std::round(params[SPA_PARAM].getValue());
		p.spB = (int)std::round(params[SPB_PARAM].getValue());
		p.mix = cv(MIX_PARAM, MIX_INPUT, 0.1f, 0.f, 1.f);
		p.frogs = pond::mapFrogs(params[FROGS_PARAM].getValue());
		p.pitch = params[PITCH_PARAM].getValue();
		p.pulse = pond::mapPulse(cv(PULSE_PARAM, PULSE_INPUT, 0.1f, 0.f, 1.f));
		p.length = pond::mapLength(cv(LENGTH_PARAM, LENGTH_INPUT, 0.1f, 0.f, 1.f));
		p.rough = params[ROUGH_PARAM].getValue();
		p.variety = params[VARIETY_PARAM].getValue();
		p.space = cv(SPACE_PARAM, SPACE_INPUT, 0.1f, 0.f, 1.f);
		p.tempC = pond::mapTempC(cv(TEMP_PARAM, TEMP_INPUT, 0.1f, 0.f, 1.f));
		p.busy = cv(BUSY_PARAM, BUSY_INPUT, 0.1f, 0.f, 1.f);
		p.bouts = cv(BOUTS_PARAM, BOUTS_INPUT, 0.1f, 0.f, 1.f);
		p.talk = cv(TALK_PARAM, TALK_INPUT, 0.2f, -1.f, 1.f);
		p.returnS = pond::mapReturn(params[RETURN_PARAM].getValue());

		// hush: button or input. A rising edge scares them; while held they stay silent.
		bool hushIn = inputs[HUSH_INPUT].getVoltage() >= 1.f;
		bool hushBtn = params[HUSH_PARAM].getValue() > 0.5f;
		if (hushTrig.process(inputs[HUSH_INPUT].getVoltage(), 0.1f, 1.f)) engine.hush();
		if (hushBtnTrig.process(hushBtn ? 1.f : 0.f)) engine.hush();
		bool held = hushIn || hushBtn;

		// call: button or input
		if (callBtnTrig.process(params[CALL_PARAM].getValue())) engine.callNow(-1, p);
		int cc = inputs[CALL_INPUT].getChannels();
		if (cc == 1) {
			if (callTrig[0].process(inputs[CALL_INPUT].getVoltage(), 0.1f, 1.f)) engine.callNow(-1, p);
		}
		else {
			for (int c = 0; c < cc && c < pond::MAX_FROGS; c++)
				if (callTrig[c].process(inputs[CALL_INPUT].getVoltage(c), 0.1f, 1.f)) engine.callNow(c, p);
		}

		engine.clocked = inputs[CLOCK_INPUT].isConnected();
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) engine.clockTick();

		double voct[pond::MAX_FROGS];
		for (int i = 0; i < pond::MAX_FROGS; i++) voct[i] = inputs[VOCT_INPUT].getPolyVoltage(i);

		double L, R;
		engine.process(p, voct, held, L, R);
		float outL = (float)pond::softClip(L * pond::OUT_GAIN), outR = (float)pond::softClip(R * pond::OUT_GAIN);
		if (outputs[R_OUTPUT].isConnected()) {
			outputs[L_OUTPUT].setVoltage(outL);
			outputs[R_OUTPUT].setVoltage(outR);
		}
		else {
			outputs[L_OUTPUT].setVoltage((outL + outR) * 0.70710678f);
			outputs[R_OUTPUT].setVoltage(0.f);
		}

		int n = engine.n;
		bool anyCall = false;
		for (int i = 0; i < n; i++) {
			pond::Frog& f = engine.frogs[i];
			if (f.trigOut) { trigGen[i].trigger(1e-3f); f.trigOut = false; anyCall = true; }
			outputs[TRIG_OUTPUT].setVoltage(trigGen[i].process(args.sampleTime) ? 10.f : 0.f, i);
			outputs[GATE_OUTPUT].setVoltage(f.calling ? 10.f : 0.f, i);
			outputs[PITCH_OUTPUT].setVoltage((float)f.pitchV, i);
		}
		outputs[TRIG_OUTPUT].setChannels(n);
		outputs[GATE_OUTPUT].setChannels(n);
		outputs[PITCH_OUTPUT].setChannels(n);
		outputs[ENV_OUTPUT].setVoltage(clamp((float)(engine.env * pond::OUT_GAIN * 2.0), 0.f, 10.f));
		if (anyCall) callFlash = 1.f;

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[HUSH_LIGHT].setBrightness(held ? 1.f : (float)engine.meanFear);
			lights[CALL_LIGHT].setBrightness(callFlash);
			callFlash = std::max(0.f, callFlash - dt / 0.12f);
			dispA = p.spA; dispB = p.spB; dispN = n; dispMix = (float)p.mix;
			dispTemp = (float)p.tempC; dispFear = held ? 1.f : (float)engine.meanFear;
			dispCalling = engine.callingCount;
			for (int i = 0; i < pond::MAX_FROGS; i++) {
				pond::Frog& f = engine.frogs[i];
				dispX[i] = (float)f.x; dispY[i] = (float)f.y;
				dispLevel[i] = i < n ? (float)std::min(1.0, f.level) : 0.f;
				dispB_[i] = f.species == p.spB && p.mix > 0.f && f.mixRank < p.mix;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer(seed));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s) { seed = (uint32_t)json_integer_value(s); engine.reseed(seed); }
	}
};

struct PondDisplay : Widget {
	PondModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		NVGcolor green = nvgRGB(140, 198, 63), amber = nvgRGB(242, 179, 61), ink = nvgRGB(234, 240, 224), red = nvgRGB(230, 80, 60);
		int a = module ? module->dispA : 0, b = module ? module->dispB : 1;
		float mix = module ? module->dispMix : 0.f, temp = module ? module->dispTemp : 20.f, fear = module ? module->dispFear : 0.f;
		int n = module ? module->dispN : 8, calling = module ? module->dispCalling : 3;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		float mapW = mm2px(36.f), mapX = box.size.x - mapW - mm2px(1.2f);
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFontSize(args.vg, mm2px(4.0f));
			nvgFillColor(args.vg, green);
			std::string la = std::string("A ") + pond::SPECIES[clamp(a, 0, pond::NUM_SPECIES - 1)].name;
			nvgText(args.vg, mm2px(1.8f), mm2px(3.4f), la.c_str(), NULL);
			nvgFillColor(args.vg, mix > 0.f ? amber : nvgRGBA(242, 179, 61, 90));
			std::string lb = std::string("B ") + pond::SPECIES[clamp(b, 0, pond::NUM_SPECIES - 1)].name + string::f(" %d%%", (int)std::round(mix * 100));
			nvgText(args.vg, mm2px(1.8f), mm2px(8.4f), lb.c_str(), NULL);
			nvgFontSize(args.vg, mm2px(3.2f));
			std::string st;
			if (fear > 0.7f) { nvgFillColor(args.vg, red); st = string::f("HUSHED   %.0fC", temp); }
			else { nvgFillColor(args.vg, ink); st = string::f("%d/%d CALLING  %.0fC", calling, n, temp); }
			nvgText(args.vg, mm2px(1.8f), mm2px(13.6f), st.c_str(), NULL);
		}
		// the pond map: where each frog sits, lit when it calls
		float mapY = mm2px(1.2f), mapH = box.size.y - mm2px(2.4f);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, mapX, mapY, mapW, mapH, mm2px(1.5f));
		nvgFillColor(args.vg, nvgRGB(12, 30, 34));
		nvgFill(args.vg);
		for (int i = 0; i < n; i++) {
			float fx = module ? module->dispX[i] : (float)(std::fmod(0.5 + i * 0.618, 1.0) * 2.0 - 1.0);
			float fy = module ? module->dispY[i] : (float)std::fmod(0.3 + i * 0.37, 1.0);
			float lv = module ? module->dispLevel[i] : (i % 3 == 0 ? 0.8f : 0.f);
			bool isB = module ? module->dispB_[i] : false;
			float px = mapX + mm2px(2.f) + (fx + 1.f) * 0.5f * (mapW - mm2px(4.f));
			float py = mapY + mapH - mm2px(2.f) - fy * (mapH - mm2px(4.f));   // near = bottom
			NVGcolor c = isB ? amber : green;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, px, py, mm2px(0.7f + 0.9f * lv));
			c.a = 0.25f + 0.75f * lv;
			nvgFillColor(args.vg, c);
			nvgFill(args.vg);
		}
		Widget::drawLayer(args, layer);
	}
};

struct PondWidget : ModuleWidget {
	PondWidget(PondModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pond.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PondDisplay* d = createWidget<PondDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R1_X[0], R1_Y)), module, PondModule::SPA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R1_X[1], R1_Y)), module, PondModule::SPB_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R1_X[2], R1_Y)), module, PondModule::MIX_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(R1_X[3], R1_Y)), module, PondModule::FROGS_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(R1_X[4], R1_Y)), module, PondModule::HUSH_PARAM, PondModule::HUSH_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(R1_X[5], R1_Y)), module, PondModule::CALL_PARAM, PondModule::CALL_LIGHT));

		const int r2[6] = {PondModule::PITCH_PARAM, PondModule::PULSE_PARAM, PondModule::LENGTH_PARAM,
		                   PondModule::ROUGH_PARAM, PondModule::VARIETY_PARAM, PondModule::SPACE_PARAM};
		for (int i = 0; i < 6; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R2_X[i], R2_Y)), module, r2[i]));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(R3_X[0], R3_Y)), module, PondModule::TEMP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R3_X[1], R3_Y)), module, PondModule::BUSY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R3_X[2], R3_Y)), module, PondModule::BOUTS_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(R3_X[3], R3_Y)), module, PondModule::TALK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R3_X[4], R3_Y)), module, PondModule::RETURN_PARAM));

		const int ja[9] = {PondModule::VOCT_INPUT, PondModule::PULSE_INPUT, PondModule::LENGTH_INPUT, PondModule::TEMP_INPUT,
		                   PondModule::BUSY_INPUT, PondModule::BOUTS_INPUT, PondModule::TALK_INPUT, PondModule::MIX_INPUT, PondModule::SPACE_INPUT};
		for (int i = 0; i < 9; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JA_Y)), module, ja[i]));
		const int jbIn[3] = {PondModule::CLOCK_INPUT, PondModule::CALL_INPUT, PondModule::HUSH_INPUT};
		for (int i = 0; i < 3; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JB_Y)), module, jbIn[i]));
		const int jbOut[6] = {PondModule::TRIG_OUTPUT, PondModule::GATE_OUTPUT, PondModule::PITCH_OUTPUT,
		                      PondModule::ENV_OUTPUT, PondModule::L_OUTPUT, PondModule::R_OUTPUT};
		for (int i = 0; i < 6; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * (3 + i), JB_Y)), module, jbOut[i]));
	}

	void appendContextMenu(Menu* menu) override {
		PondModule* m = dynamic_cast<PondModule*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("New pond (move the frogs, new individuals)", "", [=]() { m->newPond(); }));
	}
};

Model* modelPond = createModel<PondModule, PondWidget>("Pond");
