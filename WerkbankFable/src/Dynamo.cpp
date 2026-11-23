#include "plugin.hpp"
#include "dsp/Dynamo.hpp"

using namespace wbui;
namespace L = layout::Dynamo;

static const int MAXV = 16;

struct Dynamo : Module {
	enum ParamIds {
		OCTAVE_PARAM, WAVE1_PARAM, WAVE2_PARAM, DETUNE_PARAM, WIDTH_PARAM, MIX_PARAM, SUB_PARAM, NOISE_PARAM,
		CUTOFF_PARAM, RES_PARAM, ENV_PARAM, KEY_PARAM, FA_PARAM, FD_PARAM, FS_PARAM,
		AA_PARAM, AD_PARAM, AS_PARAM, GLIDE_PARAM, DRIVE_PARAM, AGE_PARAM, LEVEL_PARAM, CUT_ATT_PARAM,
		NUM_PARAMS
	};
	enum InputIds { VOCT_INPUT, GATE_INPUT, ACCENT_INPUT, CUT_INPUT, WIDTH_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_OUTPUT, NUM_OUTPUTS };

	wb::DynamoVoice voices[MAXV];
	Smooth sm[NUM_PARAMS];
	uint64_t seed = 1;
	bool needInit = true, sumMono = false;
	double rate = 0;

	Dynamo() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		configSwitch(OCTAVE_PARAM, 0.f, 2.f, 1.f, "Octave", {"-2", "-1", "0"});
		configSwitch(WAVE1_PARAM, 0.f, 1.f, 1.f, "Oscillator 1 wave", {"Pulse", "Saw"});
		configSwitch(WAVE2_PARAM, 0.f, 1.f, 1.f, "Oscillator 2 wave", {"Pulse", "Saw"});
		configParam(DETUNE_PARAM, -12.f, 12.f, 0.1f, "Oscillator 2 detune", " semitones");
		configParam(WIDTH_PARAM, 0.05f, 0.95f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Oscillator mix (1 to 2)", "%", 0.f, 100.f);
		configParam(SUB_PARAM, 0.f, 1.f, 0.f, "Sub oscillator", "%", 0.f, 100.f)->description = "A square one octave below oscillator 1.";
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f);
		configParam(CUTOFF_PARAM, 0.f, 1.f, 0.45f, "Cutoff", " Hz", 1000.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.25f, "Resonance", "%", 0.f, 100.f);
		configParam(ENV_PARAM, 0.f, 7.f, 3.15f, "Filter envelope amount", " oct");
		configParam(KEY_PARAM, 0.f, 1.f, 0.5f, "Keyboard tracking", "%", 0.f, 100.f);
		// times: attack 0.001 * 5000^k, decay 0.005 * 2000^k
		configParam<ExpTimeQuantity<1000, 5000000>>(FA_PARAM, 0.f, 1.f, 0.0814f, "Filter attack");
		configParam<ExpTimeQuantity<5000, 10000000>>(FD_PARAM, 0.f, 1.f, 0.5147f, "Filter decay");
		configParam(FS_PARAM, 0.f, 1.f, 0.1f, "Filter sustain", "%", 0.f, 100.f);
		configParam<ExpTimeQuantity<1000, 5000000>>(AA_PARAM, 0.f, 1.f, 0.0814f, "Amp attack");
		configParam<ExpTimeQuantity<5000, 10000000>>(AD_PARAM, 0.f, 1.f, 0.5765f, "Amp decay");
		configParam(AS_PARAM, 0.f, 1.f, 0.7f, "Amp sustain", "%", 0.f, 100.f);
		struct GlideQ : ParamQuantity {
			std::string getDisplayValueString() override { double k = getValue(); double t = 2.0 * k * k; return t < 1e-4 ? "off" : timeString(t); }
		};
		configParam<GlideQ>(GLIDE_PARAM, 0.f, 1.f, 0.f, "Glide");
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.3f, "Drive", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"From factory-fresh to tired: oscillator tuning and tracking error, drift, pulse-width and cutoff offsets, bent saw shapes, loose envelopes. Above 60% hiss and VCA bleed.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(CUT_ATT_PARAM, -1.f, 1.f, 1.f, "Cutoff CV amount (octaves per volt at full)", "%", 0.f, 100.f);
		configInput(VOCT_INPUT, "V/OCT (poly)");
		configInput(GATE_INPUT, "Gate (poly)");
		configInput(ACCENT_INPUT, "Accent (read when the gate rises, 5 V = full)");
		configInput(CUT_INPUT, "Cutoff CV");
		configInput(WIDTH_INPUT, "Pulse width CV (0.09 per volt)");
		configOutput(OUT_OUTPUT, "Audio");
		seed = random::u64();
	}

	void initVoices(double sr) {
		rate = sr;
		for (int c = 0; c < MAXV; c++) voices[c].init(sr, seed + (uint64_t)c, ((uint32_t)(seed + c) & 0x7FFFFFFF) | 1u);
		needInit = false;
	}
	void onReset() override { sumMono = false; needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) initVoices(args.sampleRate);
		const double a = smoothCoef(args.sampleTime);
		double k[NUM_PARAMS];
		for (int i = 0; i < NUM_PARAMS; i++) k[i] = sm[i].process(params[i].getValue(), a);
		double P[wb::D_NP];
		P[wb::D_W1] = params[WAVE1_PARAM].getValue() > 0.5f ? 0.0 : 1.0;
		P[wb::D_W2] = params[WAVE2_PARAM].getValue() > 0.5f ? 0.0 : 1.0;
		P[wb::D_DET] = k[DETUNE_PARAM];
		P[wb::D_MIX] = k[MIX_PARAM];
		P[wb::D_SUB] = k[SUB_PARAM];
		P[wb::D_NOISE] = k[NOISE_PARAM];
		P[wb::D_CUT] = k[CUTOFF_PARAM];
		P[wb::D_RES] = k[RES_PARAM];
		P[wb::D_ENV] = k[ENV_PARAM] / 7.0;
		P[wb::D_KEY] = k[KEY_PARAM];
		P[wb::D_FA] = 0.001 * std::pow(5000.0, k[FA_PARAM]);
		P[wb::D_FD] = 0.005 * std::pow(2000.0, k[FD_PARAM]);
		P[wb::D_FS] = k[FS_PARAM];
		P[wb::D_AA] = 0.001 * std::pow(5000.0, k[AA_PARAM]);
		P[wb::D_AD] = 0.005 * std::pow(2000.0, k[AD_PARAM]);
		P[wb::D_AS] = k[AS_PARAM];
		P[wb::D_GLIDE] = 2.0 * k[GLIDE_PARAM] * k[GLIDE_PARAM];
		P[wb::D_DRIVE] = k[DRIVE_PARAM];
		P[wb::D_AGE] = k[AGE_PARAM];
		const double oct = std::round(params[OCTAVE_PARAM].getValue()) - 2.0;
		const double level = k[LEVEL_PARAM] * 5.0;
		const double catt = k[CUT_ATT_PARAM];

		const int ch = std::max({1, inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels()});
		double mix = 0;
		for (int c = 0; c < ch; c++) {
			double pitch = wb::clampd(inputs[VOCT_INPUT].getPolyVoltage(c) + oct, -8.0, 8.0);
			double gate = inputs[GATE_INPUT].getPolyVoltage(c) >= 1.f ? 1.0 : 0.0;
			double acc = wb::clampd(inputs[ACCENT_INPUT].getPolyVoltage(c) / 5.0, 0.0, 1.0);
			double cut = catt * inputs[CUT_INPUT].getPolyVoltage(c);
			P[wb::D_PW] = wb::clampd(k[WIDTH_PARAM] + 0.09 * inputs[WIDTH_INPUT].getPolyVoltage(c), 0.05, 0.95);
			double y = voices[c].process(pitch, gate, acc, cut, P) * level;
			if (sumMono) mix += y;
			else outputs[OUT_OUTPUT].setVoltage(outClamp(y), c);
		}
		if (sumMono) { outputs[OUT_OUTPUT].setChannels(1); outputs[OUT_OUTPUT].setVoltage(outClamp(mix)); }
		else outputs[OUT_OUTPUT].setChannels(ch);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", seedToJson(seed));
		json_object_set_new(root, "sumMono", json_boolean(sumMono));
		return root;
	}
	void dataFromJson(json_t* root) override {
		seedFromJson(root, seed);
		json_t* s = json_object_get(root, "sumMono");
		if (s) sumMono = json_boolean_value(s);
		needInit = true;
	}
};

struct DynamoWidget : ModuleWidget {
	DynamoWidget(Dynamo* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Dynamo.svg")));
		addScrews(this);
		addParam(createParamCentered<CKSSThree>(mm(L::OCTAVE), module, Dynamo::OCTAVE_PARAM));
		addParam(createParamCentered<CKSS>(mm(L::WAVE1), module, Dynamo::WAVE1_PARAM));
		addParam(createParamCentered<CKSS>(mm(L::WAVE2), module, Dynamo::WAVE2_PARAM));
		const layout::P sp[] = {L::DETUNE, L::WIDTH, L::MIX, L::SUB, L::NOISE};
		for (int i = 0; i < 5; i++) addParam(createParamCentered<RoundSmallBlackKnob>(mm(sp[i]), module, Dynamo::DETUNE_PARAM + i));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm(L::CUTOFF), module, Dynamo::CUTOFF_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm(L::RES), module, Dynamo::RES_PARAM));
		const layout::P fp[] = {L::ENV, L::KEY, L::FA, L::FD, L::FS, L::AA, L::AD, L::AS, L::GLIDE, L::DRIVE, L::AGE, L::LEVEL};
		for (int i = 0; i < 12; i++) addParam(createParamCentered<RoundSmallBlackKnob>(mm(fp[i]), module, Dynamo::ENV_PARAM + i));
		addParam(createParamCentered<Trimpot>(mm(L::CUT_ATT), module, Dynamo::CUT_ATT_PARAM));
		const layout::P ip[] = {L::VOCT_IN, L::GATE_IN, L::ACCENT_IN, L::CUT_IN, L::WIDTH_IN};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm(ip[i]), module, Dynamo::VOCT_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::OUT), module, Dynamo::OUT_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		Dynamo* m = dynamic_cast<Dynamo*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Sum voices to mono", "", &m->sumMono));
		appendUnitMenu(menu, m->seed, [=]() { m->seed = random::u64(); m->needInit = true; });
	}
};

Model* modelDynamo = createModel<Dynamo, DynamoWidget>("Dynamo");
