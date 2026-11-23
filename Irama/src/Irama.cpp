// Irama: gamelan sequencer (Java and Bali).
//   Gong cycle (lancaran, ketawang, ladrang, gendhing / gilak, bapang, tabuh)
//   -> colotomic triggers; balungan built backwards from pathet-weighted
//   landing tones, slowly mutating; elaborating parts at 5 irama levels;
//   kotekan / imbal pairs; angsel breaks, kebyar bursts, suwuk ending;
//   ombak turns every pitch output into an umbang / isep pair.
// The sequencing engine is in GamelanCore.hpp.
#include "plugin.hpp"
#include "GamelanCore.hpp"

using namespace irama;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 14.5f, DISP_W = 125.0f, DISP_H = 18.5f;
static const float COLX[6] = {13.f, 35.f, 57.f, 79.f, 101.f, 123.f};
static const float ROW1_Y = 46.0f, ROW2_Y = 66.0f, ROW3_Y = 82.0f;
static const float JX0 = 10.f, JDX = 16.5f;
static const float IN_Y = 99.5f, COL_Y = 116.0f;
static const float OUT_VX = 168.f, OUT_GX = 189.f;
static const float OUT_Y0 = 31.f, OUT_DY = 13.f;

// Knob whose names depend on the STYLE switch (Java / Bali)
struct StyleNameQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int v = clamp((int)std::round(getValue()), 0, 5);
		bool bali = module && module->params[0].getValue() > 0.5f;   // STYLE_PARAM = 0
		if (paramId == 1) return bali ? BALI_FORMS[v & 3] : JAVA_FORMS[v & 3];   // FORM_PARAM = 1
		int m = bali ? NUM_JAVA_MODES + v % NUM_BALI_MODES : v % NUM_JAVA_MODES;
		return MODES[m].name;
	}
};

struct Irama : Module {
	enum ParamIds {
		STYLE_PARAM, FORM_PARAM, PATHET_PARAM, ROOT_PARAM, IRAMA_PARAM, KOTEKAN_PARAM,
		MUTATE_PARAM, FOCUS_PARAM, OMBAK_PARAM, ANGSEL_PARAM, TEMPO_PARAM, LOCK_PARAM,
		NEW_BUTTON, ANGSEL_BUTTON, KEBYAR_BUTTON, SUWUK_BUTTON,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT, RESET_INPUT, IRAMA_INPUT, MUTATE_INPUT, ANGSEL_INPUT, KEBYAR_INPUT, SUWUK_INPUT, NEW_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GONG_OUTPUT, KENONG_OUTPUT, KEMPUL_OUTPUT, KETHUK_OUTPUT, KEMPYANG_OUTPUT, GATRA_OUTPUT, KENDANG_OUTPUT, ANGSEL_OUTPUT,
		ENUMS(VOCT_OUTPUT, NUM_VOICES),
		ENUMS(GATE_OUTPUT, NUM_VOICES),
		SELEH_VOCT_OUTPUT, SELEH_GATE_OUTPUT,
		POLY_VOCT_OUTPUT, POLY_GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NEW_LIGHT, ANGSEL_LIGHT, KEBYAR_LIGHT, SUWUK_LIGHT, CLOCK_LIGHT,
		NUM_LIGHTS
	};

	IramaCore core;
	dsp::SchmittTrigger clockTrig, resetTrig, newTrig, newBtn, angselTrig, angselBtn, kebyarBtn, suwukTrig, suwukBtn;
	dsp::PulseGenerator trigPulse[NUM_TRIGS];
	float gateLeft[NUM_VOICES + 1] = {};
	float gateGap[NUM_VOICES + 1] = {};
	float voltsHeld[NUM_VOICES + 1] = {};
	bool kebyarLatch = false;
	bool snap12 = false;
	float clockLight = 0.f, newLight = 0.f;

	Irama() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(STYLE_PARAM, 0.f, 1.f, 0.f, "Style", {"Java", "Bali (gong kebyar)"});
		configParam<StyleNameQuantity>(FORM_PARAM, 0.f, 3.f, 0.f, "Form");
		configParam<StyleNameQuantity>(PATHET_PARAM, 0.f, 5.f, 2.f, "Laras / pathet");
		getParamQuantity(FORM_PARAM)->snapEnabled = true;
		getParamQuantity(PATHET_PARAM)->snapEnabled = true;
		configParam(ROOT_PARAM, -12.f, 12.f, 0.f, "Root", " semitones");
		configSwitch(IRAMA_PARAM, 0.f, 4.f, 1.f, "Irama", {"Lancar (1 per beat)", "Tanggung (2)", "Dadi (4)", "Wilet (8)", "Rangkep (16)"});
		configSwitch(KOTEKAN_PARAM, 0.f, 4.f, 0.f, "Kotekan / imbal", {"Imbal", "Norot", "Nyog cag", "Telu", "Empat"});
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.3f, "Mutate (gatras rewritten per gong)", "%", 0.f, 100.f);
		configParam(FOCUS_PARAM, 0.f, 3.f, 1.f, "Focus (pull towards the strong tones)");
		configParam(OMBAK_PARAM, 0.f, 10.f, 0.f, "Ombak (umbang / isep beating)", " Hz");
		configParam(ANGSEL_PARAM, 0.f, 1.f, 0.f, "Angsel chance per gatra", "%", 0.f, 100.f);
		configSwitch(TEMPO_PARAM, 0.f, 1.f, 0.f, "Tempo", {"Traditional (beat stretches with irama)", "Fixed (only density changes)"});
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock balungan", {"Off", "On"});
		configButton(NEW_BUTTON, "New balungan");
		configButton(ANGSEL_BUTTON, "Angsel (break at the next gatra)");
		configButton(KEBYAR_BUTTON, "Kebyar (toggle bursts)");
		configButton(SUWUK_BUTTON, "Suwuk (end at the next gong / restart)");
		getParamQuantity(ROOT_PARAM)->snapEnabled = true;

		configInput(CLOCK_INPUT, "Clock (pulse)");
		configInput(RESET_INPUT, "Reset");
		configInput(IRAMA_INPUT, "Irama CV (2 V per level)");
		configInput(MUTATE_INPUT, "Mutate CV (0-10 V adds)");
		configInput(ANGSEL_INPUT, "Angsel trigger");
		configInput(KEBYAR_INPUT, "Kebyar gate");
		configInput(SUWUK_INPUT, "Suwuk trigger");
		configInput(NEW_INPUT, "New balungan trigger");

		configOutput(GONG_OUTPUT, "Gong");
		configOutput(KENONG_OUTPUT, "Kenong (Bali: kempur)");
		configOutput(KEMPUL_OUTPUT, "Kempul (Bali: kemong)");
		configOutput(KETHUK_OUTPUT, "Kethuk (Bali: kempli)");
		configOutput(KEMPYANG_OUTPUT, "Kempyang (Bali: kajar)");
		configOutput(GATRA_OUTPUT, "Gatra (landing beat)");
		configOutput(KENDANG_OUTPUT, "Kendang cue");
		configOutput(ANGSEL_OUTPUT, "Angsel gate (break in progress)");
		const char* vn[NUM_VOICES] = {"Saron (Bali: calung)", "Peking (Bali: kantilan)", "Bonang (Bali: reyong)", "Polos (imbal A)", "Sangsih (imbal B)"};
		for (int v = 0; v < NUM_VOICES; v++) {
			configOutput(VOCT_OUTPUT + v, string::f("%s V/oct (2 ch with ombak)", vn[v]));
			configOutput(GATE_OUTPUT + v, string::f("%s gate", vn[v]));
		}
		configOutput(SELEH_VOCT_OUTPUT, "Seleh (landing tone, bass octave) V/oct");
		configOutput(SELEH_GATE_OUTPUT, "Seleh gate");
		configOutput(POLY_VOCT_OUTPUT, "All five parts V/oct (poly)");
		configOutput(POLY_GATE_OUTPUT, "All five parts gates (poly)");

		core.rng.seed(random::u32());
		core.buildAll();
	}

	void onReset() override {
		core.reset();
		kebyarLatch = false;
		core.buildAll();
	}
	void onRandomize() override {
		core.buildAll();
	}

	float applyPitch(float v) {
		v += params[ROOT_PARAM].getValue() / 12.f;
		if (snap12) v = std::round(v * 12.f) / 12.f;
		return v;
	}
	static float isepOf(float v, float hz) {
		float f = dsp::FREQ_C4 * std::pow(2.f, v);
		return v + std::log2((f + hz) / f);
	}

	void process(const ProcessArgs& args) override {
		// ---- parameters into the core
		core.sampleRate = args.sampleRate;
		core.style = params[STYLE_PARAM].getValue() > 0.5f ? BALI : JAVA;
		core.form = clamp((int)std::round(params[FORM_PARAM].getValue()), 0, 3);
		core.mode = core.modeIndex(clamp((int)std::round(params[PATHET_PARAM].getValue()), 0, 5));
		int lv = (int)std::round(params[IRAMA_PARAM].getValue() + inputs[IRAMA_INPUT].getVoltage() / 2.f);
		core.levelTarget = clamp(lv, 0, 4);
		core.kotekan = clamp((int)std::round(params[KOTEKAN_PARAM].getValue()), 0, 4);
		core.mutate = clamp(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		core.focus = params[FOCUS_PARAM].getValue();
		core.angselChance = params[ANGSEL_PARAM].getValue();
		core.tempoFixed = params[TEMPO_PARAM].getValue() > 0.5f;
		core.lock = params[LOCK_PARAM].getValue() > 0.5f;
		core.checkBuild();

		if (kebyarBtn.process(params[KEBYAR_BUTTON].getValue())) kebyarLatch = !kebyarLatch;
		core.kebyar = kebyarLatch || inputs[KEBYAR_INPUT].getVoltage() >= 1.f;

		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) core.reset();
		if (newTrig.process(inputs[NEW_INPUT].getVoltage(), 0.1f, 1.f) | newBtn.process(params[NEW_BUTTON].getValue())) {
			core.buildAll(); newLight = 0.3f;
		}
		if (angselTrig.process(inputs[ANGSEL_INPUT].getVoltage(), 0.1f, 1.f) | angselBtn.process(params[ANGSEL_BUTTON].getValue()))
			core.triggerAngsel();
		if (suwukTrig.process(inputs[SUWUK_INPUT].getVoltage(), 0.1f, 1.f) | suwukBtn.process(params[SUWUK_BUTTON].getValue()))
			core.triggerSuwuk();

		bool rise = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		if (rise) clockLight = 0.08f;
		IramaFrame f;
		core.tick(rise, f);

		// ---- triggers
		for (int k = 0; k < T_END; k++) if (f.trig[k]) trigPulse[k].trigger(k == T_GONG ? 0.02f : 0.005f);
		const int trigOut[7] = {GONG_OUTPUT, KENONG_OUTPUT, KEMPUL_OUTPUT, KETHUK_OUTPUT, KEMPYANG_OUTPUT, GATRA_OUTPUT, KENDANG_OUTPUT};
		for (int k = 0; k < 7; k++) outputs[trigOut[k]].setVoltage(trigPulse[k].process(args.sampleTime) ? 10.f : 0.f);
		outputs[ANGSEL_OUTPUT].setVoltage(f.angsel || (core.angselGatra && core.beat % 4 >= 2) ? 10.f : 0.f);

		// ---- notes: hold pitch, gate = half the line's interval (max 0.5 s), 1 ms gap to retrigger
		for (int v = 0; v < NUM_VOICES; v++) {
			if (f.fire[v]) {
				voltsHeld[v] = applyPitch(f.volts[v]);
				gateGap[v] = gateLeft[v] > 0.f ? 0.001f : 0.f;
				gateLeft[v] = clamp(f.interval[v] * 0.5f, 0.004f, 0.5f) + gateGap[v];
			}
		}
		if (f.seleh) {
			voltsHeld[NUM_VOICES] = applyPitch(f.selehVolts);
			gateGap[NUM_VOICES] = gateLeft[NUM_VOICES] > 0.f ? 0.001f : 0.f;
			gateLeft[NUM_VOICES] = clamp(core.stepSeconds() * core.D * 2.f, 0.01f, 2.f) + gateGap[NUM_VOICES];
		}
		bool gateOn[NUM_VOICES + 1];
		for (int v = 0; v <= NUM_VOICES; v++) {
			gateOn[v] = gateLeft[v] > 0.f && gateGap[v] <= 0.f;
			if (gateLeft[v] > 0.f) gateLeft[v] -= args.sampleTime;
			if (gateGap[v] > 0.f) gateGap[v] -= args.sampleTime;
		}

		const float ombak = params[OMBAK_PARAM].getValue();
		const int ch = ombak > 0.01f ? 2 : 1;
		int pc = 0;
		for (int v = 0; v < NUM_VOICES; v++) {
			float u = voltsHeld[v];
			float i = ch == 2 ? isepOf(u, ombak) : u;
			outputs[VOCT_OUTPUT + v].setChannels(ch);
			outputs[GATE_OUTPUT + v].setChannels(ch);
			outputs[VOCT_OUTPUT + v].setVoltage(u, 0);
			outputs[GATE_OUTPUT + v].setVoltage(gateOn[v] ? 10.f : 0.f, 0);
			outputs[POLY_VOCT_OUTPUT].setVoltage(u, pc);
			outputs[POLY_GATE_OUTPUT].setVoltage(gateOn[v] ? 10.f : 0.f, pc);
			pc++;
			if (ch == 2) {
				outputs[VOCT_OUTPUT + v].setVoltage(i, 1);
				outputs[GATE_OUTPUT + v].setVoltage(gateOn[v] ? 10.f : 0.f, 1);
				outputs[POLY_VOCT_OUTPUT].setVoltage(i, pc);
				outputs[POLY_GATE_OUTPUT].setVoltage(gateOn[v] ? 10.f : 0.f, pc);
				pc++;
			}
		}
		outputs[POLY_VOCT_OUTPUT].setChannels(pc);
		outputs[POLY_GATE_OUTPUT].setChannels(pc);
		outputs[SELEH_VOCT_OUTPUT].setVoltage(voltsHeld[NUM_VOICES]);
		outputs[SELEH_GATE_OUTPUT].setVoltage(gateOn[NUM_VOICES] ? 10.f : 0.f);

		// ---- lights
		clockLight = std::max(0.f, clockLight - args.sampleTime);
		newLight = std::max(0.f, newLight - args.sampleTime);
		lights[CLOCK_LIGHT].setBrightness(clockLight > 0.f ? 1.f : 0.f);
		lights[NEW_LIGHT].setBrightness(newLight > 0.f ? 1.f : 0.f);
		lights[ANGSEL_LIGHT].setBrightness((core.angselPending || core.angselGatra) ? 1.f : 0.f);
		lights[KEBYAR_LIGHT].setBrightness(core.kebyar ? 1.f : 0.f);
		lights[SUWUK_LIGHT].setBrightness(core.suwukPending ? 0.5f : (core.stopped ? 1.f : 0.f));
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* p = json_array();
		json_t* g = json_array();
		for (int i = 0; i < MAX_BEATS; i++) {
			json_array_append_new(p, json_integer(core.balP[i]));
			json_array_append_new(g, json_integer(core.balG[i]));
		}
		json_object_set_new(root, "balP", p);
		json_object_set_new(root, "balG", g);
		json_object_set_new(root, "built", json_integer(core.builtStyle * 100 + core.builtForm * 10 + core.builtMode));
		json_object_set_new(root, "kebyar", json_boolean(kebyarLatch));
		json_object_set_new(root, "snap12", json_boolean(snap12));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* p = json_object_get(root, "balP");
		json_t* g = json_object_get(root, "balG");
		json_t* b = json_object_get(root, "built");
		if (p && g && b) {
			for (int i = 0; i < MAX_BEATS; i++) {
				json_t* a = json_array_get(p, i);
				json_t* c = json_array_get(g, i);
				if (a) core.balP[i] = clamp((int)json_integer_value(a), 0, 14);
				if (c) core.balG[i] = clamp((int)json_integer_value(c), -1, 6);
			}
			int bi = (int)json_integer_value(b);
			core.builtStyle = bi / 100; core.builtForm = (bi / 10) % 10; core.builtMode = bi % 10;
			core.style = core.builtStyle; core.form = core.builtForm; core.mode = core.builtMode;
			core.N = formBeats(core.style, core.form);
		}
		json_t* k = json_object_get(root, "kebyar");
		if (k) kebyarLatch = json_boolean_value(k);
		json_t* s = json_object_get(root, "snap12");
		if (s) snap12 = json_boolean_value(s);
	}
};

// ---------------------------------------------------------------- display
struct IramaDisplay : TransparentWidget {
	Irama* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		const float W = box.size.x, H = box.size.y;
		NVGcolor ink = nvgRGB(0xF3, 0xE6, 0xC8), gold = nvgRGB(0xE8, 0xB0, 0x40), dim = nvgRGBA(0xF3, 0xE6, 0xC8, 0x60), red = nvgRGB(0xE0, 0x5A, 0x3A);

		IramaCore demo;
		const IramaCore* c = module ? &module->core : nullptr;
		if (!c) { demo.rng.seed(4); demo.buildAll(); c = &demo; }

		// header
		nvgFontSize(args.vg, 10.5f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFillColor(args.vg, gold);
		const char* formName = c->style == BALI ? BALI_FORMS[c->form] : JAVA_FORMS[c->form];
		std::string head = string::f("%s  %s", formName, c->M().name);
		nvgText(args.vg, 5, 3, head.c_str(), NULL);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		std::string st;
		if (c->stopped) st = "SUWUK - ENDED";
		else if (c->kebyar) st = "KEBYAR!";
		else st = string::f("IRAMA %s", LEVEL_NAMES[c->level]);
		nvgFillColor(args.vg, c->kebyar ? red : ink);
		nvgText(args.vg, W - 5, 3, st.c_str(), NULL);

		// balungan in gatras (cipher notation); dots above / below mark octaves
		int G = c->N / 4;
		int perRow = G > 8 ? 8 : G;
		int rows = (G + perRow - 1) / perRow;
		float cellW = (W - 10) / perRow;
		float charW = cellW / 4.6f;
		float fs = std::min(11.f, charW * 1.55f);
		float y0 = rows == 1 ? H * 0.62f : H * 0.47f;
		float rowH = rows == 1 ? 0 : H * 0.34f;
		nvgFontSize(args.vg, fs);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		for (int i = 0; i < c->N; i++) {
			int g = i / 4, r = g / perRow, cg = g % perRow;
			float x = 5 + cg * cellW + (i % 4 + 0.6f) * charW;
			float y = y0 + r * rowH;
			int p = c->balP[i];
			const char* nm = toneName(c->M().laras, c->balG[i] >= 0 ? c->balG[i] : c->M().pool[modp(p, 5)]);
			bool cur = c->started && i == c->beat;
			int m = formMask(c->style, c->form, i + 1);
			if (m & C_GONG) {
				nvgBeginPath(args.vg); nvgCircle(args.vg, x, y, fs * 0.62f);
				nvgStrokeColor(args.vg, gold); nvgStrokeWidth(args.vg, 1.0f); nvgStroke(args.vg);
			}
			else if (m & C_KENONG) {
				nvgBeginPath(args.vg); nvgMoveTo(args.vg, x - fs * 0.5f, y - fs * 0.62f); nvgLineTo(args.vg, x + fs * 0.5f, y - fs * 0.62f);
				nvgStrokeColor(args.vg, dim); nvgStrokeWidth(args.vg, 1.0f); nvgStroke(args.vg);
			}
			if (cur) {
				nvgBeginPath(args.vg); nvgRect(args.vg, x - charW * 0.5f, y - fs * 0.55f, charW, fs * 1.1f);
				nvgFillColor(args.vg, gold); nvgFill(args.vg);
			}
			nvgFillColor(args.vg, cur ? nvgRGB(0x1a, 0x10, 0x08) : (c->balG[i] >= 0 ? red : ink));
			nvgText(args.vg, x, y, nm, NULL);
			int oct = floorDiv(p, 5) - 1;
			if (oct != 0) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, x, oct > 0 ? y - fs * 0.62f : y + fs * 0.62f, 0.9f);
				nvgFillColor(args.vg, cur ? gold : ink); nvgFill(args.vg);
			}
		}
	}
};

// ---------------------------------------------------------------- widget
struct IramaWidget : ModuleWidget {
	IramaWidget(Irama* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Irama.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		IramaDisplay* d = createWidget<IramaDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<CKSS>(mm2px(Vec(COLX[0], ROW1_Y)), module, Irama::STYLE_PARAM));
		const int r1[5] = {Irama::FORM_PARAM, Irama::PATHET_PARAM, Irama::ROOT_PARAM, Irama::IRAMA_PARAM, Irama::KOTEKAN_PARAM};
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[i + 1], ROW1_Y)), module, r1[i]));
		const int r2[4] = {Irama::MUTATE_PARAM, Irama::FOCUS_PARAM, Irama::OMBAK_PARAM, Irama::ANGSEL_PARAM};
		for (int i = 0; i < 4; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[i], ROW2_Y)), module, r2[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(COLX[4], ROW2_Y)), module, Irama::TEMPO_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(COLX[5], ROW2_Y)), module, Irama::LOCK_PARAM));

		const int btn[4] = {Irama::NEW_BUTTON, Irama::ANGSEL_BUTTON, Irama::KEBYAR_BUTTON, Irama::SUWUK_BUTTON};
		const int bl[4] = {Irama::NEW_LIGHT, Irama::ANGSEL_LIGHT, Irama::KEBYAR_LIGHT, Irama::SUWUK_LIGHT};
		for (int i = 0; i < 4; i++)
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(COLX[i], ROW3_Y)), module, btn[i], bl[i]));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(COLX[5], ROW3_Y)), module, Irama::CLOCK_LIGHT));

		for (int i = 0; i < 8; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, IN_Y)), module, Irama::CLOCK_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, COL_Y)), module, Irama::GONG_OUTPUT + i));
		}
		for (int v = 0; v < NUM_VOICES; v++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_VX, OUT_Y0 + OUT_DY * v)), module, Irama::VOCT_OUTPUT + v));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_GX, OUT_Y0 + OUT_DY * v)), module, Irama::GATE_OUTPUT + v));
		}
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_VX, OUT_Y0 + OUT_DY * 5)), module, Irama::SELEH_VOCT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_GX, OUT_Y0 + OUT_DY * 5)), module, Irama::SELEH_GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_VX, OUT_Y0 + OUT_DY * 6)), module, Irama::POLY_VOCT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_GX, OUT_Y0 + OUT_DY * 6)), module, Irama::POLY_GATE_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Irama* module = dynamic_cast<Irama*>(this->module);
		if (!module) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Snap to 12-TET (for Western gear)", "", &module->snap12));
		menu->addChild(createMenuItem("New balungan", "", [=]() { module->core.buildAll(); }));
		menu->addChild(createMenuLabel("Gamelan tunings are approximate: real sets differ."));
	}
};

Model* modelIrama = createModel<Irama, IramaWidget>("Irama");
