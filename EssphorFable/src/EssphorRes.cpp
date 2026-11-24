// Essphor Res: morphing 48-band tuned filter bank and resonator. Part B of docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/Res.hpp"
#include "layout.hpp"
#include <atomic>

using namespace essphor;

static const float RVOLT = 0.2f;   // internal 1.0 = 5 V

struct EssphorRes : Module {
	enum ParamIds { CUTOFF_PARAM, RES_PARAM, DECAY_PARAM, PITCH_PARAM, SLOPE_PARAM, TONE_PARAM, SCALE_PARAM, WET_PARAM, AGE_PARAM,
		DECAY_MODE_PARAM, WET_MODE_PARAM, NUM_PARAMS };
	enum InputIds { CUTOFF_CV_INPUT, RES_CV_INPUT, DECAY_CV_INPUT, PITCH_CV_INPUT, SLOPE_CV_INPUT, TONE_CV_INPUT, WET_CV_INPUT, AGE_CV_INPUT,
		VOCT_INPUT, IN_L_INPUT, IN_R_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_L_OUTPUT, OUT_R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	ResEngine eng;
	uint64_t seedL = 1, seedR = 2;
	std::atomic<bool> reqInit{true};

	EssphorRes() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CUTOFF_PARAM, 0.f, 1.f, 1.f, "Cutoff", " Hz", 1000.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0.f, 100.f)->description = "Peak of the filter curve drawn across the bands.";
		configParam(DECAY_PARAM, 0.f, 1.f, 0.f, "Decay", "%", 0.f, 100.f)->description = "Ring time of every band: 0.05 s to 20 s at middle C, longer below, shorter above.";
		auto* pitch = configParam(PITCH_PARAM, -24.f, 24.f, 0.f, "Pitch", " semitones");
		pitch->snapEnabled = true;
		pitch->description = "Moves the whole bank. CV and V/OCT are not snapped.";
		auto* slope = configParam<FnQuantity>(SLOPE_PARAM, -1.f, 1.f, -1.f, "Slope");
		slope->fn = [](float v) {
			if (v <= -0.995f) return std::string("low-pass");
			if (v >= 0.995f) return std::string("high-pass");
			if (std::fabs(v) < 0.005f) return std::string("band-pass");
			return v < 0 ? string::f("%.0f%% low-pass, %.0f%% band-pass", -v * 100, (1 + v) * 100)
			             : string::f("%.0f%% high-pass, %.0f%% band-pass", v * 100, (1 - v) * 100);
		};
		configParam(TONE_PARAM, -1.f, 1.f, 0.f, "Tone")->description = "Tilts the bands: dark to bright, pivoting at 1 kHz.";
		std::vector<std::string> scales(SCALE_NAMES, SCALE_NAMES + NUM_SCALES);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES - 1, 0.f, "Scale", scales);
		configParam(WET_PARAM, 0.f, 1.f, 1.f, "Wet", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.f, "Age", "%", 0.f, 100.f)->description =
			"An old, drifting bank: mistuned bands, uneven ring and level, drive and hiss. Left and right age differently.";
		configSwitch(DECAY_MODE_PARAM, 0.f, 1.f, 0.f, "Decay mode", {"SUSTAIN (bands ring on when the filter moves away)", "CHOKE (closing the filter silences them)"});
		configSwitch(WET_MODE_PARAM, 0.f, 1.f, 0.f, "Wet mode", {"MIX", "SEND"});
		getParamQuantity(DECAY_MODE_PARAM)->randomizeEnabled = false;
		getParamQuantity(WET_MODE_PARAM)->randomizeEnabled = false;

		configInput(CUTOFF_CV_INPUT, "Cutoff CV (1 V per octave)");
		const char* cv[7] = {"Resonance", "Decay", "Pitch", "Slope", "Tone", "Wet", "Age"};
		for (int i = 0; i < 7; i++) configInput(RES_CV_INPUT + i, string::f("%s CV (±5 V sweeps the knob)", cv[i]));
		configInput(VOCT_INPUT, "V/OCT (moves the whole bank)");
		configInput(IN_L_INPUT, "Left");
		configInput(IN_R_INPUT, "Right (follows left when unpatched)");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);

		newUnit();
	}

	void newUnit() { seedL = random::u64() | 1; seedR = random::u64() | 1; reqInit = true; }

	void onSampleRateChange(const SampleRateChangeEvent& e) override { reqInit = true; }

	double knobCv(int param, int input, double lo, double hi) {
		double v = params[param].getValue();
		if (inputs[input].isConnected()) v += inputs[input].getVoltage() * 0.1 * (hi - lo);
		return std::max(lo, std::min(hi, v));
	}

	void process(const ProcessArgs& args) override {
		if (reqInit.exchange(false) || eng.fs != args.sampleRate) {
			eng.seeds[0] = seedL; eng.seeds[1] = seedR;
			eng.init(args.sampleRate);
		}
		ResParams& p = eng.p;
		double cut = 20.0 * std::pow(1000.0, (double)params[CUTOFF_PARAM].getValue());
		if (inputs[CUTOFF_CV_INPUT].isConnected()) cut *= std::pow(2.0, (double)inputs[CUTOFF_CV_INPUT].getVoltage());
		p.cutoff = std::max(20.0, std::min(20000.0, cut));
		p.res = knobCv(RES_PARAM, RES_CV_INPUT, 0, 1);
		p.decay = knobCv(DECAY_PARAM, DECAY_CV_INPUT, 0, 1);
		p.choke = params[DECAY_MODE_PARAM].getValue() > 0.5f;
		p.pitch = knobCv(PITCH_PARAM, PITCH_CV_INPUT, -24, 24) + inputs[VOCT_INPUT].getVoltage() * 12.0;
		p.slope = knobCv(SLOPE_PARAM, SLOPE_CV_INPUT, -1, 1);
		p.tone = knobCv(TONE_PARAM, TONE_CV_INPUT, -1, 1);
		p.scale = (int)params[SCALE_PARAM].getValue();
		p.wet = knobCv(WET_PARAM, WET_CV_INPUT, 0, 1);
		p.send = params[WET_MODE_PARAM].getValue() > 0.5f;
		p.age = knobCv(AGE_PARAM, AGE_CV_INPUT, 0, 1);

		const bool lOn = inputs[IN_L_INPUT].isConnected(), rOn = inputs[IN_R_INPUT].isConnected();
		const float l = lOn ? inputs[IN_L_INPUT].getVoltage() : (rOn ? inputs[IN_R_INPUT].getVoltage() : 0.f);
		const float r = rOn ? inputs[IN_R_INPUT].getVoltage() : l;
		double oL, oR;
		eng.process(l * RVOLT, r * RVOLT, oL, oR);
		outputs[OUT_L_OUTPUT].setVoltage((float)oL / RVOLT);
		outputs[OUT_R_OUTPUT].setVoltage((float)oR / RVOLT);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seedL", json_string(string::f("%016llx", (unsigned long long)seedL).c_str()));
		json_object_set_new(root, "seedR", json_string(string::f("%016llx", (unsigned long long)seedR).c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* a = json_object_get(root, "seedL");
		json_t* b = json_object_get(root, "seedR");
		if (a && json_is_string(a)) seedL = std::strtoull(json_string_value(a), nullptr, 16) | 1;
		if (b && json_is_string(b)) seedR = std::strtoull(json_string_value(b), nullptr, 16) | 1;
		reqInit = true;
	}
};

struct EssphorResWidget : ModuleWidget {
	EssphorResWidget(EssphorRes* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/EssphorRes.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		namespace L = layout::EssphorRes;
		using R = EssphorRes;
		auto knob = [&](float x, float y, int id) { addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, y)), module, id)); };
		auto in = [&](float x, float y, int id) { addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
		auto out = [&](float x, float y, int id) { addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(L::CUTOFF_X, L::CUTOFF_Y)), module, R::CUTOFF_PARAM));
		knob(L::RES_X, L::RES_Y, R::RES_PARAM); knob(L::DECAY_X, L::DECAY_Y, R::DECAY_PARAM);
		knob(L::PITCH_X, L::PITCH_Y, R::PITCH_PARAM); knob(L::SLOPE_X, L::SLOPE_Y, R::SLOPE_PARAM); knob(L::TONE_X, L::TONE_Y, R::TONE_PARAM);
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(L::SCALE_X, L::SCALE_Y)), module, R::SCALE_PARAM));
		knob(L::WET_X, L::WET_Y, R::WET_PARAM); knob(L::AGE_X, L::AGE_Y, R::AGE_PARAM);
		addParam(createParamCentered<CycleButton>(mm2px(Vec(L::DECAY_MODE_X, L::DECAY_MODE_Y)), module, R::DECAY_MODE_PARAM));
		addParam(createParamCentered<CycleButton>(mm2px(Vec(L::WET_MODE_X, L::WET_MODE_Y)), module, R::WET_MODE_PARAM));
		addChild(makeDisplay(L::DECAY_MODE_DISP_X, L::DECAY_MODE_DISP_Y, L::DECAY_MODE_DISP_W, L::DECAY_MODE_DISP_H,
			[module]() { return std::string(module && module->params[R::DECAY_MODE_PARAM].getValue() > 0.5f ? "CHOKE" : "SUSTAIN"); }));
		addChild(makeDisplay(L::WET_MODE_DISP_X, L::WET_MODE_DISP_Y, L::WET_MODE_DISP_W, L::WET_MODE_DISP_H,
			[module]() { return std::string(module && module->params[R::WET_MODE_PARAM].getValue() > 0.5f ? "SEND" : "MIX"); }));
		addChild(makeDisplay(L::SCALE_DISP_X, L::SCALE_DISP_Y, L::SCALE_DISP_W, L::SCALE_DISP_H,
			[module]() {
				if (!module) return std::string("CHROMATIC 48");
				int s = clamp((int)module->params[R::SCALE_PARAM].getValue(), 0, NUM_SCALES - 1);
				double tmp[NB];
				return string::f("%s %d", SCALE_NAMES[s], bandNotes(s, tmp));
			}, 9.f));

		const float cx[8] = {L::CUTOFF_CV_X, L::RES_CV_X, L::DECAY_CV_X, L::PITCH_CV_X, L::SLOPE_CV_X, L::TONE_CV_X, L::WET_CV_X, L::AGE_CV_X};
		const float cy[8] = {L::CUTOFF_CV_Y, L::RES_CV_Y, L::DECAY_CV_Y, L::PITCH_CV_Y, L::SLOPE_CV_Y, L::TONE_CV_Y, L::WET_CV_Y, L::AGE_CV_Y};
		for (int i = 0; i < 8; i++) in(cx[i], cy[i], R::CUTOFF_CV_INPUT + i);
		in(L::VOCT_X, L::VOCT_Y, R::VOCT_INPUT);
		in(L::IN_L_X, L::IN_L_Y, R::IN_L_INPUT); in(L::IN_R_X, L::IN_R_Y, R::IN_R_INPUT);
		out(L::OUT_L_X, L::OUT_L_Y, R::OUT_L_OUTPUT); out(L::OUT_R_X, L::OUT_R_Y, R::OUT_R_OUTPUT);
	}

	void appendContextMenu(Menu* menu) override {
		EssphorRes* m = dynamic_cast<EssphorRes*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Unit %08llx", (unsigned long long)(m->seedL >> 32))));
		menu->addChild(createMenuItem("New unit (new AGE errors)", "", [=]() { m->newUnit(); }));
	}
};

Model* modelEssphorRes = createModel<EssphorRes, EssphorResWidget>("EssphorRes");
