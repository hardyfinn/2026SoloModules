// Surprise: a follower that reacts to novelty, not loudness.
// NEW (0-10 V) jumps when the input does something it hasn't heard in the
// last MEMORY seconds and falls back over DECAY; a loop goes quiet on it once
// it has played through. TRIG fires when NEW passes THRESH. BORED climbs
// while nothing new happens (a loop-fatigue meter) and drains when something
// does. BANDS is an 8-channel poly out: how new each band is, low to high.
// FOCUS listens more to the lows or the highs. FREEZE stops learning, so the
// memory stays as it is; FORGET clears it.
#include "plugin.hpp"
#include "SurpriseDSP.hpp"
#include "Layout.hpp"

using namespace surprise;

static const float BORED_TIMES[5] = {15.f, 30.f, 60.f, 120.f, 240.f};

struct MemoryQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float s = 0.5f * std::pow(64.f, getValue());
		return s < 10.f ? string::f("%.1f s", s) : string::f("%.0f s", s);
	}
};
static float memorySeconds(float k) { return 0.5f * std::pow(64.f, clamp(k, 0.f, 1.f)); }
static float decaySeconds(float k) { return 0.02f * std::pow(200.f, clamp(k, 0.f, 1.f)); }
struct DecayQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float s = decaySeconds(getValue());
		return s < 1.f ? string::f("%d ms", (int) std::round(s * 1000.f)) : string::f("%.1f s", s);
	}
};

struct SurpriseModule : Module {
	enum ParamIds { MEMORY_PARAM, SENS_PARAM, FOCUS_PARAM, DECAY_PARAM, THRESH_PARAM, FREEZE_PARAM, FORGET_PARAM, NUM_PARAMS };
	enum InputIds { IN_L_INPUT, IN_R_INPUT, FREEZE_INPUT, FORGET_INPUT, NUM_INPUTS };
	enum OutputIds { NEW_OUTPUT, TRIG_OUTPUT, BORED_OUTPUT, BANDS_OUTPUT, NUM_OUTPUTS };
	enum LightIds { FREEZE_LIGHT, FORGET_LIGHT, NUM_LIGHTS };

	NoveltyEngine eng;
	NoveltySettings st;
	dsp::SchmittTrigger forgetTrig, forgetBtn;
	dsp::PulseGenerator trigPulse;
	float forgetFlash = 0.f;
	int boredIdx = 2;
	float bandSmooth[BANDS] = {};
	// history for the screen
	static const int HIST = 160;
	float histNew[HIST] = {}, histBored[HIST] = {};
	bool histTrig[HIST] = {};
	int histHead = 0;
	float histAcc = 0.f, histMax = 0.f;
	bool histHit = false;

	SurpriseModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<MemoryQuantity>(MEMORY_PARAM, 0.f, 1.f, 0.6f, "Memory (how far back it compares)");
		configParam(SENS_PARAM, 0.f, 2.f, 1.f, "Sensitivity", "%", 0.f, 100.f);
		configParam(FOCUS_PARAM, -1.f, 1.f, 0.f, "Focus (lows <-> highs)", "%", 0.f, 100.f);
		configParam<DecayQuantity>(DECAY_PARAM, 0.f, 1.f, 0.55f, "Decay");
		configParam(THRESH_PARAM, 0.05f, 0.95f, 0.4f, "Trigger threshold", " V", 0.f, 10.f);
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze (stop learning)", {"Learning", "Frozen"});
		configButton(FORGET_PARAM, "Forget (clear the memory)");
		configInput(IN_L_INPUT, "Audio left / mono");
		configInput(IN_R_INPUT, "Audio right");
		configInput(FREEZE_INPUT, "Freeze gate");
		configInput(FORGET_INPUT, "Forget trigger");
		configOutput(NEW_OUTPUT, "Surprise (0-10 V)");
		configOutput(TRIG_OUTPUT, "Something new (trigger)");
		configOutput(BORED_OUTPUT, "Bored (0-10 V, climbs while nothing is new)");
		configOutput(BANDS_OUTPUT, "Surprise per band (8 channels, low to high)");
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setRate(e.sampleRate); }
	void onReset() override { eng.forget(); eng.bored = 0.f; }

	void process(const ProcessArgs& args) override {
		st.memory = memorySeconds(params[MEMORY_PARAM].getValue());
		st.sens = params[SENS_PARAM].getValue();
		st.focus = params[FOCUS_PARAM].getValue();
		st.decay = decaySeconds(params[DECAY_PARAM].getValue());
		st.thresh = params[THRESH_PARAM].getValue();
		st.boredRise = BORED_TIMES[clamp(boredIdx, 0, 4)];
		st.freeze = params[FREEZE_PARAM].getValue() > 0.5f || inputs[FREEZE_INPUT].getVoltage() >= 1.f;
		bool f1 = forgetTrig.process(inputs[FORGET_INPUT].getVoltage(), 0.1f, 1.f);
		bool f2 = forgetBtn.process(params[FORGET_PARAM].getValue());
		if (f1 || f2) { eng.forget(); forgetFlash = 1.f; }
		float in = inputs[IN_L_INPUT].getVoltage();
		if (inputs[IN_R_INPUT].isConnected()) in = 0.5f * (in + inputs[IN_R_INPUT].getVoltage());
		eng.process(in, st, args.sampleTime);
		if (eng.trig) trigPulse.trigger(1e-3f);
		outputs[NEW_OUTPUT].setVoltage(10.f * eng.out);
		outputs[TRIG_OUTPUT].setVoltage(trigPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[BORED_OUTPUT].setVoltage(10.f * eng.bored);
		outputs[BANDS_OUTPUT].setChannels(BANDS);
		float k = 1.f - std::exp(-args.sampleTime / std::max(st.decay, 0.005f));
		for (int b = 0; b < BANDS; b++) {
			float t = eng.bandOut[b];
			bandSmooth[b] = t > bandSmooth[b] ? t : bandSmooth[b] + (t - bandSmooth[b]) * k;
			outputs[BANDS_OUTPUT].setVoltage(10.f * bandSmooth[b], b);
		}
		lights[FREEZE_LIGHT].setBrightness(st.freeze ? 1.f : 0.f);
		forgetFlash = std::max(0.f, forgetFlash - args.sampleTime * 4.f);
		lights[FORGET_LIGHT].setBrightness(forgetFlash);
		// screen history: one column per 50 ms, peak hold
		histMax = std::max(histMax, eng.out);
		histHit = histHit || eng.trig;
		histAcc += args.sampleTime;
		if (histAcc >= 0.05f) {
			histAcc = 0.f;
			histNew[histHead] = histMax;
			histBored[histHead] = eng.bored;
			histTrig[histHead] = histHit;
			histHead = (histHead + 1) % HIST;
			histMax = 0.f;
			histHit = false;
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "boredRise", json_integer(boredIdx));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* b = json_object_get(root, "boredRise");
		if (b) boredIdx = (int) json_integer_value(b);
	}
};

// Screen: the last 8 s of NEW (amber, ticks on triggers) and BORED (lilac),
// the threshold, and how new each band is right now.
struct SurpriseDisplay : Widget {
	SurpriseModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		const NVGcolor amber = nvgRGB(255, 179, 71), lilac = nvgRGB(195, 184, 217), cream = nvgRGB(244, 238, 223);
		float w = box.size.x, h = box.size.y, pad = mm2px(1.2f);
		float bw = mm2px(13.f);
		float gx0 = pad, gx1 = w - bw - 2 * pad, gy0 = mm2px(4.2f), gy1 = h - pad;
		const int N = SurpriseModule::HIST;
		auto val = [&](const float* a, int i) {
			if (!module) {
				float t = (float) i / N;
				return a == nullptr ? 0.f : (std::exp(-std::fabs(t - 0.35f) * 40.f) * 0.9f + std::exp(-std::fabs(t - 0.8f) * 30.f) * 0.6f);
			}
			return a[(module->histHead + i) % N];
		};
		float thr = module ? module->st.thresh : 0.4f;
		// threshold line
		float ty = gy1 - (gy1 - gy0) * thr;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, gx0, ty);
		nvgLineTo(args.vg, gx1, ty);
		nvgStrokeColor(args.vg, nvgRGBA(244, 238, 223, 50));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);
		// bored
		nvgBeginPath(args.vg);
		for (int i = 0; i < N; i++) {
			float v = module ? module->histBored[(module->histHead + i) % N] : 0.2f + 0.5f * i / N * (i < 0.35f * N ? 1.f : 0.2f);
			float x = gx0 + (gx1 - gx0) * i / (N - 1), y = gy1 - (gy1 - gy0) * v;
			if (i == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);
		}
		nvgStrokeColor(args.vg, lilac);
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);
		// surprise (filled)
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, gx0, gy1);
		for (int i = 0; i < N; i++) {
			float v = module ? module->histNew[(module->histHead + i) % N] : val((const float*) 1, i);
			nvgLineTo(args.vg, gx0 + (gx1 - gx0) * i / (N - 1), gy1 - (gy1 - gy0) * v);
		}
		nvgLineTo(args.vg, gx1, gy1);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgTransRGBA(amber, 150));
		nvgFill(args.vg);
		if (module)
			for (int i = 0; i < N; i++)
				if (module->histTrig[(module->histHead + i) % N]) {
					float x = gx0 + (gx1 - gx0) * i / (N - 1);
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, x, gy0);
					nvgLineTo(args.vg, x, gy0 + mm2px(2.f));
					nvgStrokeColor(args.vg, cream);
					nvgStrokeWidth(args.vg, 1.2f);
					nvgStroke(args.vg);
				}
		// band bars
		float bx0 = w - bw - pad;
		for (int b = 0; b < BANDS; b++) {
			float v = module ? module->bandSmooth[b] : 0.15f + 0.1f * (b % 3);
			float x = bx0 + bw * b / BANDS;
			float y = gy1 - (gy1 - gy0) * v;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x + 0.5f, y, bw / BANDS - 1.f, gy1 - y);
			nvgFillColor(args.vg, amber);
			nvgFill(args.vg);
		}
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.5f));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, amber);
			nvgText(args.vg, gx0, mm2px(3.0f), "NEW", NULL);
			nvgFillColor(args.vg, lilac);
			nvgText(args.vg, gx0 + mm2px(7.f), mm2px(3.0f), "BORED", NULL);
			std::string m = module ? (module->st.freeze ? "FROZEN" : string::f("%.1f s", module->st.memory)) : "8.0 s";
			nvgFillColor(args.vg, cream);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgText(args.vg, gx1, mm2px(3.0f), m.c_str(), NULL);
			nvgText(args.vg, w - pad, mm2px(3.0f), "BANDS", NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct SurpriseWidget : ModuleWidget {
	SurpriseWidget(SurpriseModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Surprise.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		SurpriseDisplay* d = createWidget<SurpriseDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_MEMORY_X, K_MEMORY_Y)), module, SurpriseModule::MEMORY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_SENS_X, K_SENS_Y)), module, SurpriseModule::SENS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_FOCUS_X, K_FOCUS_Y)), module, SurpriseModule::FOCUS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_DECAY_X, K_DECAY_Y)), module, SurpriseModule::DECAY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_THRESH_X, K_THRESH_Y)), module, SurpriseModule::THRESH_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(mm2px(Vec(B_FREEZE_X, B_FREEZE_Y)), module, SurpriseModule::FREEZE_PARAM, SurpriseModule::FREEZE_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(B_FORGET_X, B_FORGET_Y)), module, SurpriseModule::FORGET_PARAM, SurpriseModule::FORGET_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_IN_L_X, I_IN_L_Y)), module, SurpriseModule::IN_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_IN_R_X, I_IN_R_Y)), module, SurpriseModule::IN_R_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_FREEZE_X, I_FREEZE_Y)), module, SurpriseModule::FREEZE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_FORGET_X, I_FORGET_Y)), module, SurpriseModule::FORGET_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_NEW_X, O_NEW_Y)), module, SurpriseModule::NEW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_TRIG_X, O_TRIG_Y)), module, SurpriseModule::TRIG_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_BORED_X, O_BORED_Y)), module, SurpriseModule::BORED_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_BANDS_X, O_BANDS_Y)), module, SurpriseModule::BANDS_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		SurpriseModule* m = dynamic_cast<SurpriseModule*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("BORED reaches full after"));
		const char* names[5] = {"15 s", "30 s", "1 min", "2 min", "4 min"};
		for (int i = 0; i < 5; i++)
			menu->addChild(createCheckMenuItem(names[i], "", [=]() { return m->boredIdx == i; }, [=]() { m->boredIdx = i; }));
	}
};

Model* modelSurprise = createModel<SurpriseModule, SurpriseWidget>("Surprise");
