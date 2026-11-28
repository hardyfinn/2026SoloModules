// Juke: footwork / juke drum machine. 16-step grid of nine lanes you can edit on the
// panel, 14 presets, a generator that writes new bars in the style (GEN), mutates them
// (MUTATE), fills phrase endings (FILL) and evolves phrases (EVOLVE). Internal clock at
// 160 BPM, or send 16ths to CLOCK. Engine, presets and generator: JukeDSP.hpp.
#include "plugin.hpp"
#include "JukeDSP.hpp"

using namespace juke;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float KNOB_Y = 19.5f, CV_Y = 30.0f;
static const float KX[6] = {10.5f, 24.0f, 39.5f, 54.0f, 67.5f, 81.5f};   // TEMPO PRESET DENSITY VARY SWING LEVEL
static const float BX[3] = {95.0f, 108.0f, 121.0f};                       // GEN MUTATE FILL
static const float FEEL_X = 134.0f, EVOLVE_X = 154.0f;
static const float ROW_Y0 = 38.5f, ROW_DY = 9.0f;
static const float MUTE_X = 20.5f;
static const float GRID_X0 = 26.0f, CELL = 6.0f, GAP = 1.5f;
static const float TRIG_X = 134.5f, AUDIO_X = 148.5f;
static const float IO_Y = 123.0f;
static const float IOX[7] = {15.0f, 27.5f, 40.0f, 52.5f, 65.0f, 78.0f, 90.0f};  // RUN CLOCK RESET CLKOUT ACCENT MIXL MIXR
static const float DISP_X = 99.0f, DISP_Y = 115.6f, DISP_W = 29.0f, DISP_H = 11.4f;
static const float GRID_W = 16 * CELL + 3 * GAP;

struct PresetQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int i = clamp((int) std::round(getValue()), 0, NUM_PRESETS - 1);
		return string::f("%d %s", i + 1, PRESETS[i].name);
	}
};

enum Status { ST_PRESET, ST_GEN, ST_MUTATED, ST_EDITED };

struct Juke : Module {
	enum ParamIds {
		TEMPO_PARAM, PRESET_PARAM, DENSITY_PARAM, VARY_PARAM, SWING_PARAM, LEVEL_PARAM,
		GEN_PARAM, MUTATE_PARAM, FILL_PARAM, FEEL_PARAM, EVOLVE_PARAM, RUN_PARAM,
		MUTE_PARAM,
		NUM_PARAMS = MUTE_PARAM + NUM_LANES
	};
	enum InputIds {
		PRESET_INPUT, DENSITY_INPUT, VARY_INPUT, SWING_INPUT,
		GEN_INPUT, MUTATE_INPUT, FILL_INPUT, CLOCK_INPUT, RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		AUDIO_OUTPUT = TRIG_OUTPUT + NUM_LANES,
		CLOCK_OUTPUT = AUDIO_OUTPUT + NUM_LANES,
		ACCENT_OUTPUT, MIXL_OUTPUT, MIXR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		MUTE_LIGHT,
		RUN_LIGHT = MUTE_LIGHT + NUM_LANES,
		GEN_LIGHT, MUTATE_LIGHT, FILL_LIGHT,
		NUM_LIGHTS
	};

	DrumEngine drums;
	Seq seq;
	dsp::SchmittTrigger clockTrig, resetTrig, genTrig, mutTrig, fillTrig, genBtn, mutBtn, fillBtn;
	dsp::PulseGenerator trigPulse[NUM_LANES], clockPulse;
	float trigVolts[NUM_LANES] = {};
	float phase = 0.f, sinceClock = 0.f, period = 0.f;
	float pendDelay[NUM_LANES];
	uint8_t pendLevel[NUM_LANES];
	float accent = 0.f, genFlash = 0.f, mutFlash = 0.f;
	int lastPreset = 0;
	float ghostVolts = 5.f;
	// display state (read by the widgets)
	int status = ST_PRESET, dispStep = -1;
	float flash[NUM_LANES] = {};
	bool extClock = false;

	Juke() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TEMPO_PARAM, 100.f, 200.f, 160.f, "Tempo (internal clock, unused when CLOCK is patched)", " BPM");
		configParam<PresetQuantity>(PRESET_PARAM, 0.f, NUM_PRESETS - 1, 0.f, "Preset")->snapEnabled = true;
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.55f, "Density (for GEN)", "%", 0.f, 100.f);
		configParam(VARY_PARAM, 0.f, 1.f, 0.35f, "Variation (how far MUTATE and EVOLVE move)", "%", 0.f, 100.f);
		configParam(SWING_PARAM, 0.f, 1.f, 0.f, "Swing", "%", 0.f, 25.f, 50.f);
		configParam(LEVEL_PARAM, 0.f, 1.2f, 0.8f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configButton(GEN_PARAM, "Generate a new bar");
		configButton(MUTATE_PARAM, "Mutate the bar");
		configButton(FILL_PARAM, "Fill (second half of this bar, or the next)");
		configSwitch(FEEL_PARAM, 0.f, 2.f, 1.f, "Feel (for GEN)", {"Double-time", "Auto", "Half-time"});
		configSwitch(EVOLVE_PARAM, 0.f, 1.f, 0.f, "Evolve (vary each bar, fill the last bar of each phrase)", {"Off", "On"});
		configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
		paramQuantities[RUN_PARAM]->randomizeEnabled = false;
		for (int i = 0; i < NUM_LANES; i++) {
			configSwitch(MUTE_PARAM + i, 0.f, 1.f, 0.f, std::string(LANE_NAMES[i]) + " mute", {"Playing", "Muted"});
			paramQuantities[MUTE_PARAM + i]->randomizeEnabled = false;
			configOutput(TRIG_OUTPUT + i, std::string(LANE_NAMES[i]) + " trigger (10 V hit, ghost level in the menu)");
			configOutput(AUDIO_OUTPUT + i, std::string(LANE_NAMES[i]) + " audio (removes it from the mix)");
		}
		configInput(PRESET_INPUT, "Preset CV (0-10 V across all presets, added to the knob)");
		configInput(DENSITY_INPUT, "Density CV (10 V = 100%)");
		configInput(VARY_INPUT, "Variation CV (10 V = 100%)");
		configInput(SWING_INPUT, "Swing CV (10 V = full range)");
		configInput(GEN_INPUT, "Generate trigger");
		configInput(MUTATE_INPUT, "Mutate trigger");
		configInput(FILL_INPUT, "Fill trigger");
		configInput(CLOCK_INPUT, "Clock (16ths, overrides the internal clock)");
		configInput(RESET_INPUT, "Reset");
		configOutput(CLOCK_OUTPUT, "Clock (16ths)");
		configOutput(ACCENT_OUTPUT, "Accent CV (how busy each step is, 0-10 V)");
		configOutput(MIXL_OUTPUT, "Mix left");
		configOutput(MIXR_OUTPUT, "Mix right");
		onReset();
	}

	void onReset() override {
		seq.reset();
		lastPreset = 0;
		loadPreset(0, seq.base);
		seq.play = seq.base;
		status = ST_PRESET;
		for (int i = 0; i < NUM_LANES; i++) { pendDelay[i] = -1.f; pendLevel[i] = 0; flash[i] = 0.f; }
		phase = 0.f; dispStep = -1;
	}
	void onSampleRateChange() override { drums.setSampleRate(APP->engine->getSampleRate()); }

	float knobCv(int param, int input) {
		return clamp(params[param].getValue() + inputs[input].getVoltage() / 10.f, 0.f, 1.f);
	}

	void fire(int lane, uint8_t level) {
		drums.hit(lane, level);
		trigPulse[lane].trigger(1e-3f);
		trigVolts[lane] = level == 1 ? 10.f : ghostVolts;
		flash[lane] = 1.f;
	}

	void tick(const ProcessArgs& args, float stepSamples) {
		clockPulse.trigger(1e-3f);
		seq.vary = knobCv(VARY_PARAM, VARY_INPUT);
		seq.evolve = params[EVOLVE_PARAM].getValue() > 0.5f;
		int played = seq.step;
		uint8_t lv[NUM_LANES];
		seq.next(lv);
		dispStep = played;
		float swing = knobCv(SWING_PARAM, SWING_INPUT);
		float delay = (played & 1) ? swing * 0.5f * stepSamples : 0.f;
		float busy = 0.f;
		for (int l = 0; l < NUM_LANES; l++) {
			if (!lv[l] || params[MUTE_PARAM + l].getValue() > 0.5f) continue;
			busy += lv[l] == 1 ? 1.f : 0.4f;
			if (delay < 1.f) fire(l, lv[l]);
			else { pendDelay[l] = delay; pendLevel[l] = lv[l]; }
		}
		accent = std::min(10.f, busy * 2.5f);
		(void) args;
	}

	void process(const ProcessArgs& args) override {
		drums.sr = args.sampleRate;
		sinceClock += 1.f;

		// preset: knob + CV; only a change loads it, so edits survive
		int pi = (int) std::round(params[PRESET_PARAM].getValue());
		if (inputs[PRESET_INPUT].isConnected()) pi += (int) std::floor(inputs[PRESET_INPUT].getVoltage() / 10.f * NUM_PRESETS);
		pi = clamp(pi, 0, NUM_PRESETS - 1);
		if (pi != lastPreset) {
			lastPreset = pi;
			loadPreset(pi, seq.base);
			seq.play = seq.base;
			status = ST_PRESET;
		}

		bool g = genBtn.process(params[GEN_PARAM].getValue());
		g = genTrig.process(inputs[GEN_INPUT].getVoltage(), 0.1f, 2.f) || g;
		if (g) {
			static const int FEELS[3] = {FEEL_DOUBLE, FEEL_AUTO, FEEL_HALF};
			int feel = FEELS[clamp((int) std::round(params[FEEL_PARAM].getValue()), 0, 2)];
			seq.doGenerate(random::u32() % 9999u + 1u, knobCv(DENSITY_PARAM, DENSITY_INPUT), feel);
			status = ST_GEN; genFlash = 1.f;
		}
		bool m = mutBtn.process(params[MUTATE_PARAM].getValue());
		m = mutTrig.process(inputs[MUTATE_INPUT].getVoltage(), 0.1f, 2.f) || m;
		if (m) {
			seq.vary = knobCv(VARY_PARAM, VARY_INPUT);
			seq.doMutate();
			status = ST_MUTATED; mutFlash = 1.f;
		}
		bool f = fillBtn.process(params[FILL_PARAM].getValue());
		if (fillTrig.process(inputs[FILL_INPUT].getVoltage(), 0.1f, 2.f) || f) seq.fillArmed = true;

		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
			seq.step = 0; seq.bar = 0;
			for (int l = 0; l < NUM_LANES; l++) pendDelay[l] = -1.f;
			phase = 1.f;   // internal clock plays step 1 right away
			dispStep = -1;
		}

		// clock
		bool running = params[RUN_PARAM].getValue() > 0.5f;
		extClock = inputs[CLOCK_INPUT].isConnected();
		float intPeriod = args.sampleRate * 60.f / params[TEMPO_PARAM].getValue() / 4.f;
		if (extClock) {
			if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				if (sinceClock > 8.f && sinceClock < args.sampleRate * 2.f) period = period > 0.f ? 0.7f * period + 0.3f * sinceClock : sinceClock;
				sinceClock = 0.f;
				if (running) tick(args, period > 0.f ? period : intPeriod);
			}
		}
		else if (running) {
			phase += 1.f / intPeriod;
			if (phase >= 1.f) { phase -= std::floor(phase); tick(args, intPeriod); }
		}

		// swung hits waiting for their moment
		for (int l = 0; l < NUM_LANES; l++) {
			if (pendDelay[l] >= 0.f) {
				pendDelay[l] -= 1.f;
				if (pendDelay[l] < 0.f) fire(l, pendLevel[l]);
			}
			flash[l] = std::max(0.f, flash[l] - args.sampleTime * 6.f);
			outputs[TRIG_OUTPUT + l].setVoltage(trigPulse[l].process(args.sampleTime) ? trigVolts[l] : 0.f);
			lights[MUTE_LIGHT + l].setBrightness(params[MUTE_PARAM + l].getValue());
		}
		outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[ACCENT_OUTPUT].setVoltage(accent);

		// audio
		float v[NUM_LANES];
		drums.process(v);
		static const float pan[NUM_LANES] = {0.f, 0.f, 0.1f, 0.3f, 0.35f, -0.3f, -0.15f, -0.4f, 0.2f};
		float L = 0.f, R = 0.f, lvl = params[LEVEL_PARAM].getValue();
		for (int i = 0; i < NUM_LANES; i++) {
			if (outputs[AUDIO_OUTPUT + i].isConnected()) outputs[AUDIO_OUTPUT + i].setVoltage(5.f * v[i]);
			else {
				float a = (pan[i] + 1.f) * 0.25f * PI_F;
				L += v[i] * std::cos(a) * 1.414f; R += v[i] * std::sin(a) * 1.414f;
			}
		}
		outputs[MIXL_OUTPUT].setVoltage(5.f * std::tanh(L * lvl * 0.6f) / 0.6f);
		outputs[MIXR_OUTPUT].setVoltage(5.f * std::tanh(R * lvl * 0.6f) / 0.6f);

		genFlash = std::max(0.f, genFlash - args.sampleTime * 4.f);
		mutFlash = std::max(0.f, mutFlash - args.sampleTime * 4.f);
		lights[GEN_LIGHT].setBrightness(genFlash);
		lights[MUTATE_LIGHT].setBrightness(mutFlash);
		lights[FILL_LIGHT].setSmoothBrightness(seq.fillArmed || seq.barKind == 2 ? 1.f : 0.f, args.sampleTime);
		lights[RUN_LIGHT].setBrightness(running ? 1.f : 0.f);
	}

	void editCell(int lane, int step) {
		seq.edit(lane, step);
		status = ST_EDITED;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_t* p = json_array();
		for (int l = 0; l < NUM_LANES; l++) { char t[17]; toText(seq.base, l, t); json_array_append_new(p, json_string(t)); }
		json_object_set_new(r, "pattern", p);
		json_object_set_new(r, "preset", json_integer(lastPreset));
		json_object_set_new(r, "status", json_integer(status));
		json_object_set_new(r, "seed", json_integer(seq.seed));
		json_object_set_new(r, "phrase", json_integer(seq.phrase));
		json_object_set_new(r, "ghostVolts", json_real(ghostVolts));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* p = json_object_get(r, "pattern");
		if (p && json_is_array(p) && (int) json_array_size(p) == NUM_LANES) {
			for (int l = 0; l < NUM_LANES; l++) fromText(json_string_value(json_array_get(p, l)), seq.base, l);
			seq.play = seq.base;
		}
		if (json_t* x = json_object_get(r, "preset")) lastPreset = clamp((int) json_integer_value(x), 0, NUM_PRESETS - 1);
		if (json_t* x = json_object_get(r, "status")) status = clamp((int) json_integer_value(x), 0, 3);
		if (json_t* x = json_object_get(r, "seed")) seq.seed = (uint32_t) json_integer_value(x);
		if (json_t* x = json_object_get(r, "phrase")) seq.phrase = clamp((int) json_integer_value(x), 2, 16);
		if (json_t* x = json_object_get(r, "ghostVolts")) ghostVolts = (float) json_number_value(x);
	}
};

// ---- the step grid: draws the bar being played; click a cell to cycle off / hit / ghost ----
struct StepGrid : OpaqueWidget {
	Juke* module = nullptr;
	Pattern preview;
	StepGrid() { loadPreset(0, preview); }

	static float cellX(int s) { return mm2px(CELL * s + GAP * (s / 4) + CELL * 0.5f); }
	static float cellY(int l) { return mm2px(ROW_DY * l + ROW_DY * 0.5f); }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		const Pattern& p = module ? module->seq.play : preview;
		int now = module ? module->dispStep : -1;
		const NVGcolor pink = nvgRGB(255, 79, 154), ghost = nvgRGB(130, 52, 92), cyan = nvgRGB(89, 195, 232);
		float w = mm2px(CELL - 1.0f), h = mm2px(ROW_DY - 2.4f), rr = mm2px(0.6f);
		for (int l = 0; l < NUM_LANES; l++) {
			bool muted = module && module->params[Juke::MUTE_PARAM + l].getValue() > 0.5f;
			for (int s = 0; s < STEPS; s++) {
				int v = p.s[l][s];
				float x = cellX(s) - w / 2, y = cellY(l) - h / 2;
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, x, y, w, h, rr);
				NVGcolor c = v == 1 ? pink : (v == 2 ? ghost : (s % 4 == 0 ? nvgRGB(52, 46, 62) : nvgRGB(38, 34, 46)));
				if (muted && v) c = nvgLerpRGBA(c, nvgRGB(38, 34, 46), 0.65f);
				nvgFillColor(args.vg, c);
				nvgFill(args.vg);
				if (s == now) {
					nvgStrokeColor(args.vg, cyan);
					nvgStrokeWidth(args.vg, mm2px(0.35f));
					nvgStroke(args.vg);
				}
			}
			if (module && module->flash[l] > 0.f) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, cellX(0) - w / 2, cellY(l) + h / 2 + mm2px(0.25f), (cellX(15) - cellX(0) + w) * module->flash[l], mm2px(0.35f));
				nvgFillColor(args.vg, nvgRGBAf(0.35f, 0.76f, 0.91f, 0.8f * module->flash[l]));
				nvgFill(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			int l = (int) (e.pos.y / mm2px(ROW_DY));
			float xmm = e.pos.x / mm2px(1.f);
			int group = clamp((int) (xmm / (4 * CELL + GAP)), 0, 3);
			float inGroup = xmm - group * (4 * CELL + GAP);
			int s = group * 4 + (int) (inGroup / CELL);
			if (l >= 0 && l < NUM_LANES && inGroup < 4 * CELL && s >= 0 && s < STEPS) {
				module->editCell(l, s);
				e.consume(this);
				return;
			}
		}
		Widget::onButton(e);   // right-click still opens the module menu
	}
};

struct JukeDisplay : Widget {
	Juke* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			std::string l1 = "01 Basic", l2 = "160 BPM";
			if (module) {
				const Seq& q = module->seq;
				switch (module->status) {
				case ST_GEN: l1 = string::f("GEN  seed %u", q.seed); break;
				case ST_MUTATED: l1 = "MUTATED"; break;
				case ST_EDITED: l1 = "EDITED"; break;
				default: l1 = string::f("%02d %s", module->lastPreset + 1, PRESETS[module->lastPreset].name); break;
				}
				l2 = module->extClock ? "EXT CLK" : string::f("%.0f BPM", module->params[Juke::TEMPO_PARAM].getValue());
				if (q.evolve) l2 += string::f(" %d/%d %s", q.barInPhrase + 1, q.phrase, q.barKind == 2 ? "FILL" : (q.barKind == 1 ? "VAR" : "BASE"));
				else if (q.fillArmed || q.barKind == 2) l2 += "  FILL";
			}
			nvgFontFaceId(args.vg, font->handle);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFontSize(args.vg, mm2px(2.7f));
			nvgFillColor(args.vg, nvgRGB(255, 79, 154));
			nvgText(args.vg, mm2px(1.2f), mm2px(4.4f), l1.c_str(), NULL);
			nvgFillColor(args.vg, nvgRGB(89, 195, 232));
			nvgText(args.vg, mm2px(1.2f), mm2px(9.0f), l2.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct JukeWidget : ModuleWidget {
	JukeWidget(Juke* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Juke.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// top row
		const int knobs[6] = {Juke::TEMPO_PARAM, Juke::PRESET_PARAM, Juke::DENSITY_PARAM, Juke::VARY_PARAM, Juke::SWING_PARAM, Juke::LEVEL_PARAM};
		for (int i = 0; i < 6; i++) {
			if (i == 1) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(KX[i], KNOB_Y)), module, knobs[i]));
			else addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(KX[i], KNOB_Y)), module, knobs[i]));
		}
		const int cvs[4] = {Juke::PRESET_INPUT, Juke::DENSITY_INPUT, Juke::VARY_INPUT, Juke::SWING_INPUT};
		for (int i = 0; i < 4; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(KX[i + 1], CV_Y)), module, cvs[i]));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(BX[0], KNOB_Y)), module, Juke::GEN_PARAM, Juke::GEN_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(BX[1], KNOB_Y)), module, Juke::MUTATE_PARAM, Juke::MUTATE_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(BX[2], KNOB_Y)), module, Juke::FILL_PARAM, Juke::FILL_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BX[0], CV_Y)), module, Juke::GEN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BX[1], CV_Y)), module, Juke::MUTATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BX[2], CV_Y)), module, Juke::FILL_INPUT));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(FEEL_X, KNOB_Y + 1.0f)), module, Juke::FEEL_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(EVOLVE_X, KNOB_Y)), module, Juke::EVOLVE_PARAM));

		// grid rows
		StepGrid* g = createWidget<StepGrid>(mm2px(Vec(GRID_X0, ROW_Y0 - ROW_DY * 0.5f)));
		g->box.size = mm2px(Vec(GRID_W, ROW_DY * NUM_LANES));
		g->module = module;
		addChild(g);
		for (int l = 0; l < NUM_LANES; l++) {
			float y = ROW_Y0 + ROW_DY * l;
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(MUTE_X, y)), module, Juke::MUTE_PARAM + l, Juke::MUTE_LIGHT + l));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TRIG_X, y)), module, Juke::TRIG_OUTPUT + l));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(AUDIO_X, y)), module, Juke::AUDIO_OUTPUT + l));
		}

		// bottom row
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(IOX[0], IO_Y)), module, Juke::RUN_PARAM, Juke::RUN_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[1], IO_Y)), module, Juke::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[2], IO_Y)), module, Juke::RESET_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[3], IO_Y)), module, Juke::CLOCK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[4], IO_Y)), module, Juke::ACCENT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[5], IO_Y)), module, Juke::MIXL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[6], IO_Y)), module, Juke::MIXR_OUTPUT));

		JukeDisplay* d = createWidget<JukeDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
	}

	void appendContextMenu(Menu* menu) override {
		Juke* m = dynamic_cast<Juke*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Evolve phrase length", string::f("%d bars", m->seq.phrase), [=](Menu* sub) {
			const int opts[3] = {4, 8, 16};
			for (int o : opts)
				sub->addChild(createCheckMenuItem(string::f("%d bars", o), "", [=]() { return m->seq.phrase == o; }, [=]() { m->seq.phrase = o; }));
		}));
		menu->addChild(createSubmenuItem("Ghost note trigger level", string::f("%.0f V", m->ghostVolts), [=](Menu* sub) {
			const float opts[3] = {0.f, 5.f, 10.f};
			for (float o : opts)
				sub->addChild(createCheckMenuItem(o == 0.f ? "Off (hits only)" : string::f("%.0f V", o), "", [=]() { return m->ghostVolts == o; }, [=]() { m->ghostVolts = o; }));
		}));
		menu->addChild(createMenuItem("Reload preset", "", [=]() { loadPreset(m->lastPreset, m->seq.base); m->seq.play = m->seq.base; m->status = ST_PRESET; }));
		menu->addChild(createMenuItem("Clear pattern", "", [=]() { m->seq.base.clear(); m->seq.play = m->seq.base; m->status = ST_EDITED; }));
	}
};

Model* modelJuke = createModel<Juke, JukeWidget>("Juke");
