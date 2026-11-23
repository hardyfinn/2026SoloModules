#include "plugin.hpp"
#include "dsp/Blech.hpp"

using namespace wbui;
namespace L = layout::Blech;

static const char* VOICE_NAMES[6] = {"Bass", "Snare", "Hat", "Cymbal", "Tom", "Clave"};

struct Blech : Module {
	enum ParamIds {
		BOX_PARAM, DYN_PARAM, PITCH_PARAM, TONE_PARAM, AGE_PARAM, LEVEL_PARAM,
		ENUMS(PAD_PARAM, 6), ENUMS(TUNE_PARAM, 6), ENUMS(DECAY_PARAM, 6),
		NUM_PARAMS
	};
	enum InputIds { DYN_INPUT, PITCH_INPUT, TONE_INPUT, ACCENT_INPUT, ENUMS(TRIG_INPUT, 6), NUM_INPUTS };
	enum OutputIds { MIX_OUTPUT, ENUMS(VOICE_OUTPUT, 6), NUM_OUTPUTS };
	enum LightIds { ENUMS(HIT_LIGHT, 6), NUM_LIGHTS };

	wb::BlechEngine engine;
	wb::Schmitt trig[6];
	dsp::BooleanTrigger pad[6];
	Smooth sm[15];
	uint64_t seed = 1;
	bool needInit = true, velocityFromTrigger = false;
	double rate = 0;
	float flash[6] = {};
	dsp::ClockDivider lightDiv;

	Blech() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(BOX_PARAM, 0.f, 2.f, 2.f, "Box", {"3", "2", "1"})->description =
			"Three rhythm boxes with different component values. A change takes effect on each voice's next hit.";
		configParam(DYN_PARAM, 0.f, 1.f, 0.7f, "Dynamics pedal", "%", 0.f, 100.f)->description =
			"How hard every hit lands. A CV (0 to 10 V) replaces the knob, as a real pedal would.";
		configParam(PITCH_PARAM, -1.f, 1.f, 0.f, "Pitch pedal", " oct")->description = "Retunes all six voices together. CV adds 0.2 octave per volt.";
		configParam(TONE_PARAM, 0.f, 1.f, 1.f, "Tone pedal", "%", 0.f, 100.f)->description = "A low-pass on the whole box. CV adds, 10 V = full range.";
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"From factory-fresh to tired: tuning and decay mismatch between voices, drift, hit-to-hit level jitter. Above 60% a little hiss and mains hum.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		for (int v = 0; v < 6; v++) {
			configButton(PAD_PARAM + v, std::string(VOICE_NAMES[v]) + " pad");
			configParam(TUNE_PARAM + v, -1.f, 1.f, 0.f, std::string(VOICE_NAMES[v]) + " tune", " oct");
			configParam(DECAY_PARAM + v, -1.f, 1.f, 0.f, std::string(VOICE_NAMES[v]) + " decay", "x", 4.f);
			configInput(TRIG_INPUT + v, std::string(VOICE_NAMES[v]) + " trigger");
			configOutput(VOICE_OUTPUT + v, std::string(VOICE_NAMES[v]) + " (removed from MIX when patched)");
			configLight(HIT_LIGHT + v, std::string(VOICE_NAMES[v]) + " hit");
		}
		configInput(DYN_INPUT, "Dynamics pedal (0 to 10 V, replaces the knob)");
		configInput(PITCH_INPUT, "Pitch pedal (0.2 octave per volt)");
		configInput(TONE_INPUT, "Tone pedal (10 V = full range)");
		configInput(ACCENT_INPUT, "Accent (high at the trigger = hit 1.6 times harder)");
		configOutput(MIX_OUTPUT, "Mix");
		lightDiv.setDivision(32);
		seed = random::u64();
	}

	void onReset() override { velocityFromTrigger = false; needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) {
			rate = args.sampleRate;
			engine.init(rate, seed, (uint32_t)(seed >> 33) | 1u);
			needInit = false;
		}
		const double a = smoothCoef(args.sampleTime);
		wb::BlechParams P;
		P.box = 2 - (int)std::round(params[BOX_PARAM].getValue());
		double dynK = sm[0].process(params[DYN_PARAM].getValue(), a);
		P.dyn = inputs[DYN_INPUT].isConnected() ? wb::clampd(inputs[DYN_INPUT].getVoltage() / 10.0, 0.0, 1.0) : dynK;
		P.pedPitch = wb::clampd(sm[1].process(params[PITCH_PARAM].getValue(), a) + 0.2 * inputs[PITCH_INPUT].getVoltage(), -2.0, 2.0);
		P.tone = wb::clampd(sm[2].process(params[TONE_PARAM].getValue(), a) + 0.1 * inputs[TONE_INPUT].getVoltage(), 0.0, 1.0);
		P.age = sm[3].process(params[AGE_PARAM].getValue(), a);
		for (int v = 0; v < 6; v++) {
			P.tune[v] = sm[4 + v].process(params[TUNE_PARAM + v].getValue(), a);
			P.decay[v] = std::pow(4.0, (double)params[DECAY_PARAM + v].getValue());
		}
		const double level = sm[10].process(params[LEVEL_PARAM].getValue(), a);
		const bool accent = inputs[ACCENT_INPUT].getVoltage() >= 1.f;

		double t[6];
		for (int v = 0; v < 6; v++) {
			float tv = inputs[TRIG_INPUT + v].getVoltage();
			bool fromJack = trig[v].process(tv);
			bool fromPad = pad[v].process(params[PAD_PARAM + v].getValue() > 0.f);
			t[v] = 0.0;
			if (fromJack || fromPad) {
				double hit = accent ? 1.6 : 1.0;
				if (fromJack && velocityFromTrigger) hit = std::max(0.2, tv / 10.0 * 1.6);
				t[v] = hit;
				flash[v] = 1.f;
			}
		}
		double o[6];
		engine.process(t, P, o);
		double mix = 0;
		for (int v = 0; v < 6; v++) {
			double y = o[v] * 5.0 * level;
			if (outputs[VOICE_OUTPUT + v].isConnected()) outputs[VOICE_OUTPUT + v].setVoltage(outClamp(y));
			else mix += y;
		}
		outputs[MIX_OUTPUT].setVoltage(outClamp(mix));

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			for (int v = 0; v < 6; v++) {
				lights[HIT_LIGHT + v].setBrightness(flash[v]);
				flash[v] *= std::exp(-dt / 0.06f);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", seedToJson(seed));
		json_object_set_new(root, "velocityFromTrigger", json_boolean(velocityFromTrigger));
		return root;
	}
	void dataFromJson(json_t* root) override {
		seedFromJson(root, seed);
		json_t* s = json_object_get(root, "velocityFromTrigger");
		if (s) velocityFromTrigger = json_boolean_value(s);
		needInit = true;
	}
};

struct BlechWidget : ModuleWidget {
	BlechWidget(Blech* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Blech.svg")));
		addScrews(this);
		addParam(createParamCentered<CKSSThree>(mm(L::BOX), module, Blech::BOX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::AGE), module, Blech::AGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::DYN), module, Blech::DYN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::PITCH), module, Blech::PITCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::TONE), module, Blech::TONE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::LEVEL), module, Blech::LEVEL_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm(L::DYN_IN), module, Blech::DYN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::PITCH_IN), module, Blech::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::TONE_IN), module, Blech::TONE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::ACCENT_IN), module, Blech::ACCENT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::MIX), module, Blech::MIX_OUTPUT));
		const layout::P pads[6] = {L::BASS_PAD, L::SNARE_PAD, L::HAT_PAD, L::CYMBAL_PAD, L::TOM_PAD, L::CLAVE_PAD};
		const layout::P lts[6] = {L::BASS_LIGHT, L::SNARE_LIGHT, L::HAT_LIGHT, L::CYMBAL_LIGHT, L::TOM_LIGHT, L::CLAVE_LIGHT};
		const layout::P tunes[6] = {L::BASS_TUNE, L::SNARE_TUNE, L::HAT_TUNE, L::CYMBAL_TUNE, L::TOM_TUNE, L::CLAVE_TUNE};
		const layout::P decs[6] = {L::BASS_DECAY, L::SNARE_DECAY, L::HAT_DECAY, L::CYMBAL_DECAY, L::TOM_DECAY, L::CLAVE_DECAY};
		const layout::P trs[6] = {L::BASS_TRIG, L::SNARE_TRIG, L::HAT_TRIG, L::CYMBAL_TRIG, L::TOM_TRIG, L::CLAVE_TRIG};
		const layout::P outs[6] = {L::BASS_OUT, L::SNARE_OUT, L::HAT_OUT, L::CYMBAL_OUT, L::TOM_OUT, L::CLAVE_OUT};
		for (int v = 0; v < 6; v++) {
			addParam(createParamCentered<Pad>(mm(pads[v]), module, Blech::PAD_PARAM + v));
			addChild(createLightCentered<SmallLight<OrangeLight>>(mm(lts[v]), module, Blech::HIT_LIGHT + v));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm(tunes[v]), module, Blech::TUNE_PARAM + v));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm(decs[v]), module, Blech::DECAY_PARAM + v));
			addInput(createInputCentered<PJ301MPort>(mm(trs[v]), module, Blech::TRIG_INPUT + v));
			addOutput(createOutputCentered<PJ301MPort>(mm(outs[v]), module, Blech::VOICE_OUTPUT + v));
		}
	}
	void appendContextMenu(Menu* menu) override {
		Blech* m = dynamic_cast<Blech*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Trigger voltage sets level", "", &m->velocityFromTrigger));
		appendUnitMenu(menu, m->seed, [=]() { m->seed = random::u64(); m->needInit = true; });
	}
};

Model* modelBlech = createModel<Blech, BlechWidget>("Blech");
