// Menagerie: sixteen sound engines behind one small set of controls.
//   MODEL     picks the engine (knob or 0.5 V per step on the MODEL input)
//   FREQ      +-4 octaves around C4; V/OCT adds to it (polyphonic)
//   SHAPE / TEXTURE / LIFE   the three macros; what each does per engine is
//             shown on the screen. LIFE is always "how much it changes by itself"
//   DECAY     envelope decay (or ring time for the struck engines)
//   TRIG      patched: triggers/gates play notes through a built-in envelope
//             (struck engines are hit instead). Unpatched: the voice drones
//             and struck engines strike on each new note.
//   LEVEL     0..8 V, normalled to 8 V: volume, and how hard notes are struck
//   MAIN/AUX  two outputs; AUX is a different side of each engine
// Engines live in MenagerieDSP.hpp (no Rack code, used by test/render.cpp).
#include "plugin.hpp"
#include "MenagerieDSP.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 15.5f, DISP_W = 81.44f, DISP_H = 17.0f;
static const float ROWA_Y = 47.0f;
static const float XA[3] = {18.0f, 45.72f, 73.44f};
static const float ROWB_Y = 70.0f;
static const float XB[3] = {16.0f, 45.72f, 75.44f};
static const float ROWC_Y = 85.5f;
static const float JX[5] = {10.5f, 28.1f, 45.72f, 63.3f, 81.0f};
static const float IN1_Y = 102.0f;
static const float IN2_Y = 116.5f;

struct Menagerie : Module {
	enum ParamIds {
		MODEL_PARAM, FREQ_PARAM, DECAY_PARAM,
		SHAPE_PARAM, TEXTURE_PARAM, LIFE_PARAM,
		SHAPE_ATT_PARAM, TEXTURE_ATT_PARAM, LIFE_ATT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, TRIG_INPUT, LEVEL_INPUT, MODEL_INPUT,
		SHAPE_INPUT, TEXTURE_INPUT, LIFE_INPUT, DECAY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { AUX_OUTPUT, MAIN_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	mng::Voice voices[16];
	dsp::SchmittTrigger trig[16];
	int shownModel = 0;
	float shown[3] = {};

	Menagerie() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		std::vector<std::string> names;
		for (int i = 0; i < mng::NUM_ENGINES; i++) names.push_back(string::f("%d %s", i + 1, mng::ENGINE_NAMES[i]));
		configSwitch(MODEL_PARAM, 0.f, (float) (mng::NUM_ENGINES - 1), 0.f, "Model", names);
		configParam(FREQ_PARAM, -4.f, 4.f, 0.f, "Frequency", " Hz", 2.f, dsp::FREQ_C4);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay", " s", 400.f, 0.02f);
		configParam(SHAPE_PARAM, 0.f, 1.f, 0.5f, "Shape", "%", 0.f, 100.f);
		configParam(TEXTURE_PARAM, 0.f, 1.f, 0.5f, "Texture", "%", 0.f, 100.f);
		configParam(LIFE_PARAM, 0.f, 1.f, 0.2f, "Life (how much it changes by itself)", "%", 0.f, 100.f);
		configParam(SHAPE_ATT_PARAM, -1.f, 1.f, 0.f, "Shape CV amount", "%", 0.f, 100.f);
		configParam(TEXTURE_ATT_PARAM, -1.f, 1.f, 0.f, "Texture CV amount", "%", 0.f, 100.f);
		configParam(LIFE_ATT_PARAM, -1.f, 1.f, 0.f, "Life CV amount", "%", 0.f, 100.f);
		configInput(VOCT_INPUT, "1 V/octave pitch (polyphonic)");
		configInput(TRIG_INPUT, "Trigger / gate (unpatched: drone)");
		configInput(LEVEL_INPUT, "Level and strike strength (0..8 V)");
		configInput(MODEL_INPUT, "Model (0.5 V per step)");
		configInput(SHAPE_INPUT, "Shape CV");
		configInput(TEXTURE_INPUT, "Texture CV");
		configInput(LIFE_INPUT, "Life CV");
		configInput(DECAY_INPUT, "Decay CV");
		configOutput(AUX_OUTPUT, "Aux");
		configOutput(MAIN_OUTPUT, "Main");
		for (int i = 0; i < 16; i++) voices[i].seed(0x51ED2701u + 7919u * (uint32_t) i);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int i = 0; i < 16; i++) voices[i].resetEngine(voices[i].model);
	}

	void process(const ProcessArgs& args) override {
		int ch = std::max(1, std::max(inputs[VOCT_INPUT].getChannels(), inputs[TRIG_INPUT].getChannels()));
		bool drone = !inputs[TRIG_INPUT].isConnected();
		int model = (int) std::round(params[MODEL_PARAM].getValue() + inputs[MODEL_INPUT].getVoltage() * 2.f);
		model = clamp(model, 0, mng::NUM_ENGINES - 1);

		float shapeK = params[SHAPE_PARAM].getValue(), texK = params[TEXTURE_PARAM].getValue();
		float lifeK = params[LIFE_PARAM].getValue(), decK = params[DECAY_PARAM].getValue();
		float shapeA = params[SHAPE_ATT_PARAM].getValue(), texA = params[TEXTURE_ATT_PARAM].getValue();
		float lifeA = params[LIFE_ATT_PARAM].getValue();
		float freqK = params[FREQ_PARAM].getValue();

		for (int c = 0; c < ch; c++) {
			mng::Ctl ctl;
			ctl.setRate(args.sampleRate);
			float pitch = freqK + inputs[VOCT_INPUT].getPolyVoltage(c);
			ctl.f = clamp(dsp::FREQ_C4 * std::exp2(pitch), 16.f, args.sampleRate * 0.4f);
			ctl.shape = clamp(shapeK + inputs[SHAPE_INPUT].getPolyVoltage(c) * 0.1f * shapeA, 0.f, 1.f);
			ctl.texture = clamp(texK + inputs[TEXTURE_INPUT].getPolyVoltage(c) * 0.1f * texA, 0.f, 1.f);
			ctl.life = clamp(lifeK + inputs[LIFE_INPUT].getPolyVoltage(c) * 0.1f * lifeA, 0.f, 1.f);
			ctl.decay = clamp(decK + inputs[DECAY_INPUT].getPolyVoltage(c) * 0.1f, 0.f, 1.f);
			ctl.drone = drone;
			float tv = inputs[TRIG_INPUT].getPolyVoltage(c);
			bool t = !drone && trig[c].process(tv, 0.1f, 1.f);
			bool gate = !drone && tv >= 1.f;
			float level = inputs[LEVEL_INPUT].isConnected() ? clamp(inputs[LEVEL_INPUT].getPolyVoltage(c) / 8.f, 0.f, 1.25f) : 1.f;
			float m, a;
			voices[c].process(model, ctl, t, gate, level, m, a);
			outputs[MAIN_OUTPUT].setVoltage(5.f * m, c);
			outputs[AUX_OUTPUT].setVoltage(5.f * a, c);
			if (c == 0) { shown[0] = ctl.shape; shown[1] = ctl.texture; shown[2] = ctl.life; }
		}
		outputs[MAIN_OUTPUT].setChannels(ch);
		outputs[AUX_OUTPUT].setChannels(ch);
		shownModel = model;
	}
};

// The screen: engine number and name, and what the three macros do in it,
// with a bar for each.
struct MenagerieDisplay : TransparentWidget {
	Menagerie* module = nullptr;
	std::string fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (!font) return;
		int m = module ? module->shownModel : 0;
		float v[3] = {0.5f, 0.5f, 0.2f};
		if (module) for (int i = 0; i < 3; i++) v[i] = module->shown[i];
		const NVGcolor green = nvgRGB(0x7F, 0xE0, 0xA0), dim = nvgRGB(0x4A, 0x7A, 0x5C);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(args.vg, 17.f);
		nvgFillColor(args.vg, green);
		std::string title = string::f("%02d %s", m + 1, mng::ENGINE_NAMES[m]);
		nvgText(args.vg, 7.f, 19.f, title.c_str(), NULL);
		float colW = (box.size.x - 14.f) / 3.f;
		nvgFontSize(args.vg, 10.5f);
		for (int i = 0; i < 3; i++) {
			float x = 7.f + i * colW;
			nvgFillColor(args.vg, dim);
			nvgText(args.vg, x, 33.f, mng::ENGINE_HINTS[m][i], NULL);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x, 38.f, (colW - 8.f), 3.f);
			nvgFillColor(args.vg, nvgRGB(0x22, 0x33, 0x28));
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x, 38.f, (colW - 8.f) * clamp(v[i], 0.f, 1.f), 3.f);
			nvgFillColor(args.vg, green);
			nvgFill(args.vg);
		}
	}
};

struct MenagerieWidget : ModuleWidget {
	MenagerieWidget(Menagerie* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Menagerie.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		MenagerieDisplay* d = createWidget<MenagerieDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XA[0], ROWA_Y)), module, Menagerie::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(XA[1], ROWA_Y)), module, Menagerie::DECAY_PARAM));
		RoundLargeBlackKnob* mk = createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XA[2], ROWA_Y)), module, Menagerie::MODEL_PARAM);
		mk->snap = true;
		addParam(mk);
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XB[0], ROWB_Y)), module, Menagerie::SHAPE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XB[1], ROWB_Y)), module, Menagerie::TEXTURE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XB[2], ROWB_Y)), module, Menagerie::LIFE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(XB[0], ROWC_Y)), module, Menagerie::SHAPE_ATT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(XB[1], ROWC_Y)), module, Menagerie::TEXTURE_ATT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(XB[2], ROWC_Y)), module, Menagerie::LIFE_ATT_PARAM));

		const int in1[4] = {Menagerie::VOCT_INPUT, Menagerie::TRIG_INPUT, Menagerie::LEVEL_INPUT, Menagerie::MODEL_INPUT};
		const int in2[4] = {Menagerie::SHAPE_INPUT, Menagerie::TEXTURE_INPUT, Menagerie::LIFE_INPUT, Menagerie::DECAY_INPUT};
		for (int i = 0; i < 4; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX[i], IN1_Y)), module, in1[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX[i], IN2_Y)), module, in2[i]));
		}
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX[4], IN1_Y)), module, Menagerie::AUX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX[4], IN2_Y)), module, Menagerie::MAIN_OUTPUT));
	}
};

Model* modelMenagerie = createModel<Menagerie, MenagerieWidget>("Menagerie");
