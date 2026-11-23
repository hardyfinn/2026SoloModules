#include "plugin.hpp"
#include "dsp/Vokoder.hpp"

using namespace wbui;
namespace L = layout::Vokoder;

static const int MAXV = 16;

struct Vokoder : Module {
	enum ParamIds {
		ENUMS(BAND_PARAM, wb::VK_NB),
		MODGAIN_PARAM, ATTACK_PARAM, RELEASE_PARAM, FORMANT_PARAM, SIB_PARAM, VOICING_PARAM, AGE_PARAM, LEVEL_PARAM,
		SHAPE_PARAM, WIDTH_PARAM, TUNE_PARAM, CHORD_PARAM, FREEZE_PARAM,
		NUM_PARAMS
	};
	enum InputIds { MOD_INPUT, CAR_INPUT, VOCT_INPUT, FORMANT_INPUT, FREEZE_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_OUTPUT, VOICED_OUTPUT, ENV_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(BAND_LIGHT, wb::VK_NB), FREEZE_LIGHT, VOICED_LIGHT, NUM_LIGHTS };

	wb::VokoderEngine engine;
	wb::VokCarrier carriers[MAXV];
	wb::VokParams P;
	Smooth sm[9];
	uint64_t seed = 1;
	bool needInit = true;
	double rate = 0;
	dsp::ClockDivider ctrlDiv, lightDiv;

	struct BandQ : ParamQuantity {
		std::string getDisplayValueString() override {
			double s = 2.0 * getValue();
			return s <= 0.001 ? "off" : string::f("%+.1f dB", 40.0 * std::log10(s));
		}
	};

	Vokoder() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < wb::VK_NB; i++) {
			double c = wb::vkCentre(i);
			std::string name = c < 1000 ? string::f("Band %d (%.0f Hz)", i + 1, c) : string::f("Band %d (%.1f kHz)", i + 1, c / 1000.0);
			configParam<BandQ>(BAND_PARAM + i, 0.f, 1.f, 0.5f, name)->description = "Gain of this band. Centre = unity.";
			configLight(BAND_LIGHT + i, name);
		}
		configParam(MODGAIN_PARAM, 0.f, 4.f, 1.f, "Modulator gain", "x");
		configParam<ExpTimeQuantity<1000, 30000>>(ATTACK_PARAM, 0.f, 1.f, 0.4076f, "Attack");
		configParam<ExpTimeQuantity<10000, 600000>>(RELEASE_PARAM, 0.f, 1.f, 0.3386f, "Release");
		configParam(FORMANT_PARAM, -5.f, 5.f, 0.f, "Formant shift", " bands")->description = "Moves the modulator's spectrum across the bank: up for a small voice, down for a large one.";
		configParam(SIB_PARAM, 0.f, 1.f, 0.7f, "Sibilance", "%", 0.f, 100.f)->description = "Level of the noise that replaces the carrier on consonants.";
		configParam(VOICING_PARAM, 0.f, 1.f, 0.5f, "Voicing", "%", 0.f, 100.f)->description = "Lower treats more of the modulator as consonant.";
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"From factory-fresh to tired: mismatched band centres, Q and gain between analysis and synthesis, slower envelopes. Above 60% carrier bleed and hiss.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(SHAPE_PARAM, 0.f, 1.f, 0.f, "Carrier shape (saw to pulse)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.05f, 0.5f, 0.3f, "Carrier pulse width", "%", 0.f, 100.f);
		configParam(TUNE_PARAM, -2.f, 2.f, -1.f, "Carrier tune", " oct");
		configSwitch(CHORD_PARAM, 0.f, 3.f, 0.f, "Carrier chord", {"Off", "Fifth", "Minor", "Major"});
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze", {"Off", "On"});
		configInput(MOD_INPUT, "Modulator (voice)");
		configInput(CAR_INPUT, "Carrier (replaces the internal carrier)");
		configInput(VOCT_INPUT, "Carrier V/OCT (poly: one note per channel)");
		configInput(FORMANT_INPUT, "Formant shift CV (1 band per volt)");
		configInput(FREEZE_INPUT, "Freeze gate");
		configOutput(OUT_OUTPUT, "Audio");
		configOutput(VOICED_OUTPUT, "Voiced gate (10 V while speech is voiced)");
		configOutput(ENV_OUTPUT, "Speech envelope (0 to 10 V)");
		configLight(FREEZE_LIGHT, "Freeze");
		configLight(VOICED_LIGHT, "Voiced");
		ctrlDiv.setDivision(32);
		lightDiv.setDivision(64);
		seed = random::u64();
	}

	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) {
			rate = args.sampleRate;
			engine.init(rate, seed, (uint32_t)(seed >> 33) | 1u);
			for (auto& c : carriers) c.reset();
			needInit = false;
		}
		if (ctrlDiv.process()) {
			// AGE and the sliders change filter coefficients: read them at control rate, unsmoothed
			P.age = params[AGE_PARAM].getValue();
			for (int i = 0; i < wb::VK_NB; i++) { double s = 2.0 * params[BAND_PARAM + i].getValue(); P.sliders[i] = s * s; }
		}
		const double a = smoothCoef(args.sampleTime);
		const double modGain = sm[0].process(params[MODGAIN_PARAM].getValue(), a);
		P.attack = expMap(sm[1].process(params[ATTACK_PARAM].getValue(), a), 0.001, 0.030);
		P.release = expMap(sm[2].process(params[RELEASE_PARAM].getValue(), a), 0.010, 0.600);
		P.shift = wb::clampd(sm[3].process(params[FORMANT_PARAM].getValue(), a) + inputs[FORMANT_INPUT].getVoltage(), -5.0, 5.0);
		P.sib = sm[4].process(params[SIB_PARAM].getValue(), a);
		P.thresh = sm[5].process(params[VOICING_PARAM].getValue(), a);
		const double level = sm[6].process(params[LEVEL_PARAM].getValue(), a);
		const double shape = sm[7].process(params[SHAPE_PARAM].getValue(), a);
		const double pw = sm[8].process(params[WIDTH_PARAM].getValue(), a);
		const double tune = params[TUNE_PARAM].getValue();
		const int chord = clamp((int)std::round(params[CHORD_PARAM].getValue()), 0, 3);
		const bool freeze = params[FREEZE_PARAM].getValue() > 0.5f || inputs[FREEZE_INPUT].getVoltage() >= 1.f;

		double car;
		if (inputs[CAR_INPUT].isConnected()) car = inputs[CAR_INPUT].getVoltage() / 5.0;
		else {
			int ch = std::max(1, inputs[VOCT_INPUT].getChannels());
			car = 0;
			for (int c = 0; c < ch; c++)
				car += carriers[c].process(tune + wb::clampd(inputs[VOCT_INPUT].getVoltage(c), -6.0, 6.0), shape, pw, chord, args.sampleRate);
			car /= std::sqrt((double)ch);
		}
		double m = inputs[MOD_INPUT].getVoltage() / 5.0 * modGain;
		double y = engine.process(m, car, freeze, P);
		outputs[OUT_OUTPUT].setVoltage(outClamp(y * 5.0 * level));
		const bool voiced = engine.voiced > 0.5 && engine.speechEnv > 0.02;
		outputs[VOICED_OUTPUT].setVoltage(voiced ? 10.f : 0.f);
		outputs[ENV_OUTPUT].setVoltage((float)(engine.speechEnv * 10.0));

		if (lightDiv.process()) {
			for (int i = 0; i < wb::VK_NB; i++) lights[BAND_LIGHT + i].setBrightness((float)std::min(1.0, 6.0 * engine.bandEnv(i)));
			lights[FREEZE_LIGHT].setBrightness(freeze ? 1.f : 0.f);
			lights[VOICED_LIGHT].setBrightness(voiced ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", seedToJson(seed));
		return root;
	}
	void dataFromJson(json_t* root) override { seedFromJson(root, seed); needInit = true; }
};

struct VokoderWidget : ModuleWidget {
	VokoderWidget(Vokoder* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Vokoder.svg")));
		addScrews(this);
		const layout::P bands[wb::VK_NB] = {L::BAND0, L::BAND1, L::BAND2, L::BAND3, L::BAND4, L::BAND5, L::BAND6, L::BAND7, L::BAND8, L::BAND9,
			L::BAND10, L::BAND11, L::BAND12, L::BAND13, L::BAND14, L::BAND15, L::BAND16, L::BAND17, L::BAND18, L::BAND19};
		for (int i = 0; i < wb::VK_NB; i++)
			addParam(createLightParamCentered<VCVLightSlider<OrangeLight>>(mm(bands[i]), module, Vokoder::BAND_PARAM + i, Vokoder::BAND_LIGHT + i));
		const layout::P kp[] = {L::MODGAIN, L::ATTACK, L::RELEASE, L::FORMANT, L::SIB, L::VOICING, L::AGE, L::LEVEL, L::SHAPE, L::WIDTH, L::TUNE};
		for (int i = 0; i < 11; i++) addParam(createParamCentered<RoundSmallBlackKnob>(mm(kp[i]), module, Vokoder::MODGAIN_PARAM + i));
		auto* ch = createParamCentered<RoundSmallBlackKnob>(mm(L::CHORD), module, Vokoder::CHORD_PARAM);
		ch->snap = true;
		ch->minAngle = -0.5f * M_PI; ch->maxAngle = 0.5f * M_PI;
		addParam(ch);
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(mm(L::FREEZE), module, Vokoder::FREEZE_PARAM, Vokoder::FREEZE_LIGHT));
		addChild(createLightCentered<MediumLight<OrangeLight>>(mm(L::VOICED_LIGHT), module, Vokoder::VOICED_LIGHT));
		const layout::P ip[] = {L::MOD_IN, L::CAR_IN, L::VOCT_IN, L::FORMANT_IN, L::FREEZE_IN};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm(ip[i]), module, Vokoder::MOD_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::OUT), module, Vokoder::OUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::VOICED_OUT), module, Vokoder::VOICED_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::ENV_OUT), module, Vokoder::ENV_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		Vokoder* m = dynamic_cast<Vokoder*>(module);
		if (!m) return;
		appendUnitMenu(menu, m->seed, [=]() { m->seed = random::u64(); m->needInit = true; });
	}
};

Model* modelVokoder = createModel<Vokoder, VokoderWidget>("Vokoder");
