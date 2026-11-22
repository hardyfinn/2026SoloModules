// Riddim: dancehall riddim drum machine. 24 classic riddims, each with its own
// machine flavour (Casio, 808, Linn, Simmons, sampler...), ten synthesised voices.
// Clock it with 16ths. Sound engine and patterns: RiddimDSP.hpp, RiddimPatterns.hpp.
#include "plugin.hpp"
#include "RiddimDSP.hpp"

using namespace riddim;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 14.0f, DISP_W = 142.4f, DISP_H = 21.0f;
static const float COLX[6] = {13.0f, 37.5f, 62.0f, 86.5f, 111.0f, 135.5f};
static const float ROW1_Y = 51.0f;
static const float ROW2_Y = 69.5f;
static const float VX[5] = {17.0f, 47.0f, 77.0f, 107.0f, 137.0f};
static const float VKNOB_DX = -6.5f, VJACK_DX = 6.5f;
static const float VA_Y = 88.6f, VB_Y = 105.0f;
static const float IO_Y = 122.6f;
static const float IO_X0 = 17.0f, IO_DX = 19.6f;

struct NameQuantity : ParamQuantity {
	int kind = 0;   // 0 riddim, 1 kit
	std::string getDisplayValueString() override {
		int i = (int) std::round(getValue());
		if (kind == 0) return string::f("%d %s", i + 1, RIDDIMS[clamp(i, 0, NUM_RIDDIMS - 1)].name);
		return KIT_NAMES[clamp(i, 0, 8)];
	}
};

struct Riddim : Module {
	enum ParamIds {
		RIDDIM_PARAM, KIT_PARAM, TUNE_PARAM, DECAY_PARAM, TONE_PARAM, LEVEL_PARAM,
		SWING_PARAM, MUTATE_PARAM, CRUSH_PARAM, DRIVE_PARAM, VAR_PARAM, FILL_PARAM,
		VOICE_LEVEL_PARAM,
		NUM_PARAMS = VOICE_LEVEL_PARAM + 10
	};
	enum InputIds { CLOCK_INPUT, RESET_INPUT, FILL_INPUT, VAR_INPUT, RIDDIM_INPUT, NUM_INPUTS };
	enum OutputIds { MIXL_OUTPUT, MIXR_OUTPUT, VOICE_OUTPUT, NUM_OUTPUTS = VOICE_OUTPUT + 10 };
	enum LightIds { FILL_LIGHT, NUM_LIGHTS };

	DrumEngine drums;
	Seq seq;
	dsp::SchmittTrigger clockTrig, resetTrig, fillTrig, fillBtnTrig;
	float sinceClock = 0.f, period = 0.125f * 48000.f;
	float pendDelay[10];
	uint8_t pendLevel[10];
	// display state
	int dispRiddim = 0, dispStep = -1, dispKit = 0;
	bool dispB = false;
	float flash[10] = {};

	Riddim() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<NameQuantity>(RIDDIM_PARAM, 0.f, NUM_RIDDIMS - 1, 0.f, "Riddim")->snapEnabled = true;
		((NameQuantity*) paramQuantities[RIDDIM_PARAM])->kind = 0;
		configParam<NameQuantity>(KIT_PARAM, 0.f, 8.f, 0.f, "Kit (0 = the riddim's own machine)")->snapEnabled = true;
		((NameQuantity*) paramQuantities[KIT_PARAM])->kind = 1;
		configParam(TUNE_PARAM, -12.f, 12.f, 0.f, "Tune", " st");
		configParam(DECAY_PARAM, 0.5f, 2.f, 1.f, "Decay", "x");
		configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.2f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(SWING_PARAM, 0.f, 1.f, 0.f, "Swing", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate", "%", 0.f, 100.f);
		configParam(CRUSH_PARAM, 0.f, 1.f, 0.f, "Crush", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configSwitch(VAR_PARAM, 0.f, 2.f, 0.f, "Variation", {"A", "Auto (B every 4th bar)", "B"});
		configButton(FILL_PARAM, "Fill (last beat of this bar)");
		for (int i = 0; i < 10; i++) {
			configParam(VOICE_LEVEL_PARAM + i, 0.f, 1.f, 0.8f, std::string(TRACK_NAMES[i]) + " level", "%", 0.f, 100.f);
			configOutput(VOICE_OUTPUT + i, std::string(TRACK_NAMES[i]) + " out (removes it from the mix)");
		}
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(FILL_INPUT, "Fill");
		configInput(VAR_INPUT, "Variation B (gate)");
		configInput(RIDDIM_INPUT, "Riddim CV (0-10 V = 1-24)");
		configOutput(MIXL_OUTPUT, "Mix left");
		configOutput(MIXR_OUTPUT, "Mix right");
		onReset();
	}

	void onReset() override {
		seq.reset();
		for (int i = 0; i < 10; i++) { pendDelay[i] = -1.f; pendLevel[i] = 0; flash[i] = 0.f; }
		dispStep = -1;
	}
	void onSampleRateChange() override { drums.setSampleRate(APP->engine->getSampleRate()); }

	void process(const ProcessArgs& args) override {
		drums.sr = args.sampleRate;
		sinceClock += 1.f;

		int idx = (int) std::round(params[RIDDIM_PARAM].getValue());
		if (inputs[RIDDIM_INPUT].isConnected())
			idx += (int) std::round(inputs[RIDDIM_INPUT].getVoltage() / 10.f * NUM_RIDDIMS);
		idx = ((idx % NUM_RIDDIMS) + NUM_RIDDIMS) % NUM_RIDDIMS;
		dispRiddim = idx;

		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) { seq.step = 0; seq.bar = 0; dispStep = -1; }
		bool fillBtn = fillBtnTrig.process(params[FILL_PARAM].getValue());
		if (fillTrig.process(inputs[FILL_INPUT].getVoltage(), 0.1f, 2.f) || fillBtn) seq.fillArmed = true;
		lights[FILL_LIGHT].setSmoothBrightness(seq.fillArmed ? 1.f : 0.f, args.sampleTime);

		int kit = (int) std::round(params[KIT_PARAM].getValue());
		int kitSel = kit == 0 ? RIDDIMS[idx].kit : kit - 1;
		dispKit = kitSel;
		Ctl c;
		c.tune = std::pow(2.f, params[TUNE_PARAM].getValue() / 12.f);
		c.decay = params[DECAY_PARAM].getValue();
		c.tone = params[TONE_PARAM].getValue();
		c.crush = params[CRUSH_PARAM].getValue();
		c.drive = params[DRIVE_PARAM].getValue();
		c.kit = kitSel;

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
			if (sinceClock > 8.f && sinceClock < args.sampleRate * 2.f) period = 0.7f * period + 0.3f * sinceClock;
			sinceClock = 0.f;
			int played = seq.step;
			uint8_t lv[10];
			int var = (int) std::round(params[VAR_PARAM].getValue());
			bool gate = inputs[VAR_INPUT].getVoltage() > 1.f;
			dispB = var == 2 || gate || (var == 1 && (seq.bar % 4) == 3);
			seq.next(idx, var, gate, params[MUTATE_PARAM].getValue(), false, lv);
			dispStep = played;
			float delay = (played & 1) ? params[SWING_PARAM].getValue() * 0.6f * period : 0.f;
			for (int t = 0; t < 10; t++) {
				if (!lv[t]) continue;
				if (delay < 1.f) { drums.hit(t, lv[t], c); flash[t] = 1.f; }
				else { pendDelay[t] = delay; pendLevel[t] = lv[t]; }
			}
		}
		for (int t = 0; t < 10; t++) {
			if (pendDelay[t] >= 0.f) {
				pendDelay[t] -= 1.f;
				if (pendDelay[t] < 0.f) { drums.hit(t, pendLevel[t], c); flash[t] = 1.f; }
			}
			flash[t] = std::max(0.f, flash[t] - args.sampleTime * 6.f);
		}

		float v[10];
		drums.process(c, v);
		static const float pan[10] = {0.f, -0.05f, 0.25f, -0.2f, 0.3f, 0.35f, 0.3f, -0.35f, 0.4f, -0.3f};
		float L = 0.f, R = 0.f;
		for (int i = 0; i < 10; i++) {
			float s = v[i] * params[VOICE_LEVEL_PARAM + i].getValue();
			if (outputs[VOICE_OUTPUT + i].isConnected()) outputs[VOICE_OUTPUT + i].setVoltage(5.f * s);
			else {
				float a = (pan[i] + 1.f) * 0.25f * PI_F;
				L += s * std::cos(a) * 1.414f; R += s * std::sin(a) * 1.414f;
			}
		}
		float lvl = params[LEVEL_PARAM].getValue();
		outputs[MIXL_OUTPUT].setVoltage(5.f * std::tanh(L * lvl * 0.7f) / 0.7f);
		outputs[MIXR_OUTPUT].setVoltage(5.f * std::tanh(R * lvl * 0.7f) / 0.7f);
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "autoFill", json_integer(seq.autoFill));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* a = json_object_get(r, "autoFill");
		if (a) seq.autoFill = (int) json_integer_value(a);
	}
};

struct RiddimDisplay : Widget {
	Riddim* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		int ri = 0, step = -1, kit = 1;
		bool b = false; float flash[10] = {};
		if (module) {
			ri = module->dispRiddim; step = module->dispStep; kit = module->dispKit; b = module->dispB;
			for (int i = 0; i < 10; i++) flash[i] = module->flash[i];
		}
		const RiddimDef& r = RIDDIMS[ri];
		const NVGcolor gold = nvgRGB(255, 200, 60), green = nvgRGB(60, 200, 90), red = nvgRGB(235, 70, 60), dim = nvgRGB(120, 125, 110);
		float h = box.size.y;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			float x = mm2px(1.6f);
			nvgFontSize(args.vg, mm2px(3.4f));
			nvgFillColor(args.vg, gold);
			std::string t1 = string::f("%02d %s", ri + 1, r.name);
			nvgText(args.vg, x, mm2px(4.6f), t1.c_str(), NULL);
			nvgFontSize(args.vg, mm2px(3.0f));
			nvgFillColor(args.vg, green);
			nvgText(args.vg, x, mm2px(9.2f), (std::string(r.year) + "  " + r.producer).c_str(), NULL);
			nvgFillColor(args.vg, r.confirmed ? gold : red);
			nvgText(args.vg, x, mm2px(13.8f), r.machine, NULL);
			nvgFillColor(args.vg, dim);
			std::string k = std::string("KIT ") + KIT_NAMES[kit + 1] + (b ? "   B" : "   A");
			nvgText(args.vg, x, mm2px(18.4f), k.c_str(), NULL);
		}
		// step grid: 10 tracks x 16 steps
		float gx = mm2px(64.f), gy = mm2px(1.2f);
		float cw = mm2px(4.5f), rh = mm2px(1.85f);
		const uint8_t (*g)[16] = b ? r.b : r.a;
		for (int t = 0; t < 10; t++) {
			for (int s = 0; s < 16; s++) {
				int lv = g[t][s];
				float cx = gx + s * cw + cw * 0.5f, cy = gy + t * rh + rh * 0.5f;
				bool now = s == step;
				nvgBeginPath(args.vg);
				if (lv) nvgRect(args.vg, cx - cw * 0.34f, cy - rh * 0.36f, cw * 0.68f, rh * 0.72f);
				else nvgRect(args.vg, cx - cw * 0.12f, cy - rh * 0.12f, cw * 0.24f, rh * 0.24f);
				if (lv) {
					NVGcolor col = lv == 3 ? red : (lv == 2 ? gold : nvgRGB(160, 130, 50));
					if (now) col = nvgRGB(255, 255, 255);
					nvgFillColor(args.vg, col);
				}
				else nvgFillColor(args.vg, now ? nvgRGB(170, 170, 150) : nvgRGBA(120, 125, 110, 90));
				nvgFill(args.vg);
			}
		}
		for (int t = 0; t < 10; t++) if (flash[t] > 0.f) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, gx - mm2px(1.6f), gy + t * rh + rh * 0.5f, mm2px(0.7f));
			nvgFillColor(args.vg, nvgRGBAf(0.25f, 0.9f, 0.4f, flash[t]));
			nvgFill(args.vg);
		}
		(void) h;
		Widget::drawLayer(args, layer);
	}
};

struct RiddimWidget : ModuleWidget {
	RiddimWidget(Riddim* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Riddim.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		RiddimDisplay* d = createWidget<RiddimDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COLX[0], ROW1_Y)), module, Riddim::RIDDIM_PARAM));
		const int r1[5] = {Riddim::KIT_PARAM, Riddim::TUNE_PARAM, Riddim::DECAY_PARAM, Riddim::TONE_PARAM, Riddim::LEVEL_PARAM};
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[i + 1], ROW1_Y)), module, r1[i]));
		const int r2[4] = {Riddim::SWING_PARAM, Riddim::MUTATE_PARAM, Riddim::CRUSH_PARAM, Riddim::DRIVE_PARAM};
		for (int i = 0; i < 4; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[i], ROW2_Y)), module, r2[i]));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(COLX[4] - 3.f, ROW2_Y)), module, Riddim::VAR_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(COLX[5], ROW2_Y)), module, Riddim::FILL_PARAM, Riddim::FILL_LIGHT));

		for (int i = 0; i < 10; i++) {
			float cx = VX[i % 5], y = i < 5 ? VA_Y : VB_Y;
			addParam(createParamCentered<Trimpot>(mm2px(Vec(cx + VKNOB_DX, y)), module, Riddim::VOICE_LEVEL_PARAM + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(cx + VJACK_DX, y)), module, Riddim::VOICE_OUTPUT + i));
		}
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IO_X0 + IO_DX * i, IO_Y)), module, Riddim::CLOCK_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IO_X0 + IO_DX * 5, IO_Y)), module, Riddim::MIXL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IO_X0 + IO_DX * 6, IO_Y)), module, Riddim::MIXR_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Riddim* m = dynamic_cast<Riddim*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Auto fill", m->seq.autoFill ? string::f("every %d bars", m->seq.autoFill) : "off", [=](Menu* sub) {
			const int opts[4] = {0, 2, 4, 8};
			for (int o : opts)
				sub->addChild(createCheckMenuItem(o ? string::f("every %d bars", o) : "off", "",
					[=]() { return m->seq.autoFill == o; }, [=]() { m->seq.autoFill = o; }));
		}));
	}
};

Model* modelRiddim = createModel<Riddim, RiddimWidget>("Riddim");
