#include "plugin.hpp"
#include "dsp/Zap.hpp"

using namespace wbui;
namespace L = layout::Zap;

static const int MAXV = 16;

struct Zap : Module {
	enum ParamIds { START_PARAM, END_PARAM, SWEEP_PARAM, RES_PARAM, DECAY_PARAM, NOISE_PARAM, CLICK_PARAM, DRIVE_PARAM, AGE_PARAM, LEVEL_PARAM, HIT_PARAM, NUM_PARAMS };
	enum InputIds { TRIG_INPUT, VOCT_INPUT, ACCENT_INPUT, START_INPUT, END_INPUT, SWEEP_INPUT, RES_INPUT, DECAY_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HIT_LIGHT, NUM_LIGHTS };

	wb::ZapVoice voices[MAXV];
	wb::Schmitt trig[MAXV];
	dsp::BooleanTrigger hitButton;
	Smooth sm[9];
	uint64_t seed = 1;
	bool needInit = true, sumMono = true;
	double rate = 0;
	float hitLight = 0.f;

	Zap() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		// exponential knobs: display = 20 * 800^k Hz
		configParam(START_PARAM, 0.f, 1.f, 0.7926f, "Start pitch", " Hz", 800.f, 20.f)->description = "Pitch at the moment of the hit.";
		configParam(END_PARAM, 0.f, 1.f, 0.2408f, "End pitch", " Hz", 800.f, 20.f)->description = "Pitch the ring settles to. May be above START for a rising zap.";
		configParam(SWEEP_PARAM, 0.f, 1.f, 0.4924f, "Sweep time", " ms", 1000.f, 1.f)->description = "Time constant of the pitch fall from START to END.";
		configParam(RES_PARAM, 0.f, 1.f, 0.8f, "Resonance", "%", 0.f, 100.f)->description = "Ring length. Above 90% the filter oscillates by itself.";
		struct DecayQ : ParamQuantity {
			std::string getDisplayValueString() override {
				float k = getValue();
				if (k >= 0.999f) return "HOLD";
				return timeString(expMap(k, 0.005, 8.0));
			}
		};
		configParam<DecayQ>(DECAY_PARAM, 0.f, 1.f, 0.594f, "Decay")->description = "VCA time to -60 dB. Fully clockwise is HOLD: the VCA stays open.";
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f)->description = "A noise burst into the filter, for snares.";
		configParam(CLICK_PARAM, 0.f, 1.f, 1.f, "Click", "%", 0.f, 100.f)->description = "Strength of the kick that pings the filter.";
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"From factory-fresh to tired: tuning error, hit-to-hit jitter in pitch and level, drift. Above 60% a little hiss.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configButton(HIT_PARAM, "Hit");
		configInput(TRIG_INPUT, "Trigger (poly)");
		configInput(VOCT_INPUT, "V/OCT (moves START and END together, poly)");
		configInput(ACCENT_INPUT, "Accent (high at the trigger = hit 1.6 times harder)");
		configInput(START_INPUT, "START CV (1 V/oct)");
		configInput(END_INPUT, "END CV (1 V/oct)");
		configInput(SWEEP_INPUT, "SWEEP CV (time doubles per volt)");
		configInput(RES_INPUT, "RESONANCE CV (0.1 per volt)");
		configInput(DECAY_INPUT, "DECAY CV (time doubles per volt)");
		configOutput(OUT_OUTPUT, "Audio");
		configLight(HIT_LIGHT, "Hit");
		seed = random::u64();
	}

	void initVoices(double sr) {
		rate = sr;
		for (int c = 0; c < MAXV; c++) voices[c].init(sr, seed, (uint32_t)(seed & 0x7FFFFFFF) + 7919u * (uint32_t)c);
		needInit = false;
	}
	void onReset() override { sumMono = true; needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) initVoices(args.sampleRate);
		const double a = smoothCoef(args.sampleTime);
		double k[9];
		for (int i = 0; i < 9; i++) k[i] = sm[i].process(params[START_PARAM + i].getValue(), a);
		const double level = params[LEVEL_PARAM].getValue();
		const bool hold = params[DECAY_PARAM].getValue() >= 0.999f;
		const bool hit = hitButton.process(params[HIT_PARAM].getValue() > 0.f);

		const int ch = std::max({1, inputs[TRIG_INPUT].getChannels(), inputs[VOCT_INPUT].getChannels()});
		double mix = 0;
		bool any = false;
		for (int c = 0; c < ch; c++) {
			bool t = trig[c].process(inputs[TRIG_INPUT].getPolyVoltage(c)) || hit;
			double tv = 0.0;
			if (t) { tv = inputs[ACCENT_INPUT].getPolyVoltage(c) >= 1.f ? 1.6 : 1.0; any = true; }
			wb::ZapParams p;
			p.startHz = wb::clampd(20.0 * std::pow(800.0, k[0]) * std::exp2((double)inputs[START_INPUT].getPolyVoltage(c)), 10.0, 22000.0);
			p.endHz = wb::clampd(20.0 * std::pow(800.0, k[1]) * std::exp2((double)inputs[END_INPUT].getPolyVoltage(c)), 10.0, 22000.0);
			p.sweep = wb::clampd(expMap(k[2], 0.001, 1.0) * std::exp2((double)inputs[SWEEP_INPUT].getPolyVoltage(c)), 1e-4, 10.0);
			p.res = wb::clampd(k[3] + 0.1 * inputs[RES_INPUT].getPolyVoltage(c), 0.0, 1.0);
			p.decay = hold ? 8.0 : wb::clampd(expMap(k[4], 0.005, 8.0) * std::exp2((double)inputs[DECAY_INPUT].getPolyVoltage(c)), 0.002, 7.99);
			p.noise = k[5]; p.click = k[6]; p.drive = k[7]; p.age = k[8];
			double y = voices[c].process(tv, wb::clampd((double)inputs[VOCT_INPUT].getPolyVoltage(c), -10.0, 10.0), p);
			double v = y * 5.0 * level;
			if (sumMono) mix += v;
			else outputs[OUT_OUTPUT].setVoltage(outClamp(v), c);
		}
		if (sumMono) { outputs[OUT_OUTPUT].setChannels(1); outputs[OUT_OUTPUT].setVoltage(outClamp(mix)); }
		else outputs[OUT_OUTPUT].setChannels(ch);
		if (any) hitLight = 1.f;
		hitLight *= std::exp(-args.sampleTime / 0.06f);
		lights[HIT_LIGHT].setBrightness(hitLight);
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

struct ZapWidget : ModuleWidget {
	ZapWidget(Zap* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Zap.svg")));
		addScrews(this);
		const layout::P kp[9] = {L::START, L::END, L::SWEEP, L::RES, L::DECAY, L::NOISE, L::CLICK, L::DRIVE, L::AGE};
		for (int i = 0; i < 9; i++) addParam(createParamCentered<RoundBlackKnob>(mm(kp[i]), module, Zap::START_PARAM + i));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::LEVEL), module, Zap::LEVEL_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<OrangeLight>>(mm(L::HIT), module, Zap::HIT_PARAM, Zap::HIT_LIGHT));
		const layout::P ip[8] = {L::TRIG_IN, L::VOCT_IN, L::ACCENT_IN, L::START_IN, L::END_IN, L::SWEEP_IN, L::RES_IN, L::DECAY_IN};
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm(ip[i]), module, Zap::TRIG_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::OUT), module, Zap::OUT_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		Zap* m = dynamic_cast<Zap*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Sum voices to mono", "", &m->sumMono));
		appendUnitMenu(menu, m->seed, [=]() { m->seed = random::u64(); m->needInit = true; });
	}
};

Model* modelZap = createModel<Zap, ZapWidget>("Zap");
