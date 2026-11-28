// Bulgaria: harmonises one melody the way Bulgarian village choirs do. A drone and up to seven more voices that sit
// a deliberately mistuned second from the lead; rub in cents or as a beat rate in Hz; village tunings; slides,
// throat shakes and the phrase-end whoop. Pitch, gate and push CV out; makes no sound itself.
// Engine in src/dsp/BulgariaEngine.hpp (shared with the offline tests). Spec: docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/BulgariaEngine.hpp"
#include "defaults.hpp"

using namespace bulgaria;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float K1 = 23.0f, K2 = 39.0f, K3 = 57.0f, K4 = 83.5f;
static const float ROW1_X[4] = {13.0f, 38.0f, 63.5f, 88.5f};
static const float ROW2_X[2] = {13.0f, 33.0f};
static float lightx(int i) { return 47.0f + 6.6f * i; }
static const float RUB_X = 16.0f;
static const float SW_X[3] = {36.0f, 52.0f, 85.0f};
static const float CHANCE_X = 70.0f;
static const float ROW4_X[4] = {13.0f, 33.0f, 53.0f, 85.0f};
static const float TRIG_Y = 94.5f;
static const float J1 = 106.5f, J2 = 118.5f;
static float jackx(int i) { return 9.0f + 13.9f * i; }

struct Bulgaria;

// RUB tooltip in cents or Hz, following the UNIT switch.
struct RubQ : ParamQuantity {
	bool hz() const;
	std::string getDisplayValueString() override {
		double x = getValue();
		return hz() ? string::f("%.1f Hz", rubHz(x)) : string::f("%.0f cents", rubCents(x));
	}
	void setDisplayValueString(std::string s) override {
		double v = std::atof(s.c_str());
		if (hz()) setValue((float)clamp01(rubHzInv(std::max(1.0, std::min(80.0, v)))));
		else setValue((float)clamp01(rubCentsInv(std::max(25.0, std::min(250.0, v)))));
	}
};

struct PctQ : ParamQuantity {
	std::string getDisplayValueString() override { return string::f("%.0f%%", 100.0 * getValue()); }
};

struct Bulgaria : Module {
	enum ParamIds { ROOT_P, OCT_P, MODE_P, TUNE_P, VOICES_P, SPREAD_P, RUB_P, UNIT_P, WHEN_P, CHANCE_P, CADENCE_P,
		SLIDE_P, SHAKE_P, WHOOP_P, LOOSE_P, NUM_PARAMS };
	enum InputIds { VOCT_INPUT, GATE_INPUT, DRONE_INPUT, PHRASE_INPUT, RUB_INPUT, TUNE_INPUT, VOICES_INPUT,
		SLIDE_INPUT, SHAKE_INPUT, WHOOP_INPUT, LOOSE_INPUT, NUM_INPUTS };
	enum OutputIds { LEAD_VOCT_OUTPUT, LEAD_GATE_OUTPUT, DRONE_VOCT_OUTPUT, DRONE_GATE_OUTPUT, POLY_VOCT_OUTPUT,
		POLY_GATE_OUTPUT, POLY_PUSH_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(VOICE_LIGHT, NV * 2), NUM_LIGHTS };

	Engine e;
	uint64_t seed = 1;
	double rate = 0;
	bool needInit = true;
	dsp::ClockDivider lightDiv;

	Bulgaria() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(ROOT_P, 0.f, 11.f, BG_DEFAULTS[ROOT_P], "Root (drone note)",
			{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
		configSwitch(OCT_P, -2.f, 2.f, BG_DEFAULTS[OCT_P], "Octave of the drone", {"-2", "-1", "0", "+1", "+2"});
		configSwitch(MODE_P, 0.f, 6.f, BG_DEFAULTS[MODE_P], "Mode",
			{"Shope", "Aeolian", "Dorian", "Phrygian", "Hijaz", "Mixolydian", "Major"});
		configParam<PctQ>(TUNE_P, 0.f, 1.f, BG_DEFAULTS[TUNE_P], "Tune (equal-tempered to village tuning)")->description =
			"At 0 the melody passes through untouched, chromatic notes included. At 100% it is snapped to the mode and village-tuned.";
		configSwitch(VOICES_P, 1.f, 8.f, BG_DEFAULTS[VOICES_P], "Voices", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configParam<PctQ>(SPREAD_P, 0.f, 1.f, BG_DEFAULTS[SPREAD_P], "Spread (open octaves and fifths to close cluster)");
		RubQ* rq = configParam<RubQ>(RUB_P, 0.f, 1.f, BG_DEFAULTS[RUB_P], "Rub");
		rq->description = "How far the rubbing voices sit from the lead: a width in cents (25 to 250) or a beat rate in Hz (1 to 80), set by UNIT.";
		configSwitch(UNIT_P, 0.f, 1.f, BG_DEFAULTS[UNIT_P], "Rub unit", {"Cents", "Hz"});
		configSwitch(WHEN_P, 0.f, 1.f, BG_DEFAULTS[WHEN_P], "When to rub", {"Rule", "Chance"});
		configParam<PctQ>(CHANCE_P, 0.f, 1.f, BG_DEFAULTS[CHANCE_P], "Chance (how often voice 2 rubs, in CHANCE)");
		configSwitch(CADENCE_P, 0.f, 1.f, BG_DEFAULTS[CADENCE_P], "Cadence", {"Tight", "Resolve"});
		configParam<PctQ>(SLIDE_P, 0.f, 1.f, BG_DEFAULTS[SLIDE_P], "Slide (scoop into notes)");
		configParam<PctQ>(SHAKE_P, 0.f, 1.f, BG_DEFAULTS[SHAKE_P], "Shake (tresene, the throat shake on held notes)");
		configParam<PctQ>(WHOOP_P, 0.f, 1.f, BG_DEFAULTS[WHOOP_P], "Whoop (izvik, the leap up at phrase ends)");
		configParam<PctQ>(LOOSE_P, 0.f, 1.f, BG_DEFAULTS[LOOSE_P], "Loose (human singers)")->description =
			"Pitch drift, late entries, rub width wander and uneven ornaments.";

		configInput(VOCT_INPUT, "Melody V/OCT");
		configInput(GATE_INPUT, "Melody gate (unpatched: always on, every pitch change is a note)");
		configInput(DRONE_INPUT, "Drone V/OCT (overrides ROOT and OCT)");
		configInput(PHRASE_INPUT, "Phrase end (trigger on the last note of a phrase)");
		configInput(RUB_INPUT, "Rub CV (+0.1 per volt)");
		configInput(TUNE_INPUT, "Tune CV (+0.1 per volt)");
		configInput(VOICES_INPUT, "Voices CV (0 to 10 V adds 0 to 7 voices)");
		configInput(SLIDE_INPUT, "Slide trigger");
		configInput(SHAKE_INPUT, "Shake trigger");
		configInput(WHOOP_INPUT, "Whoop trigger");
		configInput(LOOSE_INPUT, "Loose CV (+0.1 per volt)");
		configOutput(LEAD_VOCT_OUTPUT, "Lead V/OCT (the retuned melody)");
		configOutput(LEAD_GATE_OUTPUT, "Lead gate");
		configOutput(DRONE_VOCT_OUTPUT, "Drone V/OCT (voice 3)");
		configOutput(DRONE_GATE_OUTPUT, "Drone gate");
		configOutput(POLY_VOCT_OUTPUT, "Poly V/OCT (all voices)");
		configOutput(POLY_GATE_OUTPUT, "Poly gate (all voices)");
		configOutput(POLY_PUSH_OUTPUT, "Poly push (0 to 10 V per voice: patch to cutoff or level)");
		for (int i = 0; i < NV; i++) configLight(VOICE_LIGHT + 2 * i, string::f("Voice %d (green sounding, red rubbing)", i + 1));
		lightDiv.setDivision(64);
		seed = random::u64();
	}

	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	float cv(int in) { return inputs[in].isConnected() ? 0.1f * inputs[in].getVoltage() : 0.f; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) { rate = args.sampleRate; e.init(rate, seed); needInit = false; }
		Params p;
		p.root = (int)std::lround(params[ROOT_P].getValue());
		p.oct = (int)std::lround(params[OCT_P].getValue());
		p.mode = clamp((int)std::lround(params[MODE_P].getValue()), 0, NUM_MODES - 1);
		p.tune = clamp(params[TUNE_P].getValue() + cv(TUNE_INPUT), 0.f, 1.f);
		float vc = inputs[VOICES_INPUT].isConnected() ? 0.7f * inputs[VOICES_INPUT].getVoltage() : 0.f;
		p.voices = clamp((int)std::lround(params[VOICES_P].getValue() + vc), 1, NV);
		p.spread = params[SPREAD_P].getValue();
		p.rub = clamp(params[RUB_P].getValue() + cv(RUB_INPUT), 0.f, 1.f);
		p.hz = params[UNIT_P].getValue() > 0.5f;
		p.chanceMode = params[WHEN_P].getValue() > 0.5f;
		p.chance = params[CHANCE_P].getValue();
		p.resolve = params[CADENCE_P].getValue() > 0.5f;
		p.slide = params[SLIDE_P].getValue();
		p.shake = params[SHAKE_P].getValue();
		p.whoop = params[WHOOP_P].getValue();
		p.loose = clamp(params[LOOSE_P].getValue() + cv(LOOSE_INPUT), 0.f, 1.f);

		Inputs in;
		in.voct = inputs[VOCT_INPUT].getVoltage();
		in.gate = inputs[GATE_INPUT].getVoltage();
		in.gatePatched = inputs[GATE_INPUT].isConnected();
		in.drone = inputs[DRONE_INPUT].getVoltage();
		in.dronePatched = inputs[DRONE_INPUT].isConnected();
		in.phrase = inputs[PHRASE_INPUT].getVoltage();
		in.slideTrig = inputs[SLIDE_INPUT].getVoltage();
		in.shakeTrig = inputs[SHAKE_INPUT].getVoltage();
		in.whoopTrig = inputs[WHOOP_INPUT].getVoltage();
		e.process(p, in);

		int n = p.voices;
		outputs[POLY_VOCT_OUTPUT].setChannels(n);
		outputs[POLY_GATE_OUTPUT].setChannels(n);
		outputs[POLY_PUSH_OUTPUT].setChannels(n);
		for (int c = 0; c < n; c++) {
			outputs[POLY_VOCT_OUTPUT].setVoltage((float)e.volts(c), c);
			outputs[POLY_GATE_OUTPUT].setVoltage((float)e.gateV(c), c);
			outputs[POLY_PUSH_OUTPUT].setVoltage((float)e.pushV(c), c);
		}
		outputs[LEAD_VOCT_OUTPUT].setVoltage((float)e.volts(0));
		outputs[LEAD_GATE_OUTPUT].setVoltage((float)e.gateV(0));
		outputs[DRONE_VOCT_OUTPUT].setVoltage((float)e.volts(2));
		outputs[DRONE_GATE_OUTPUT].setVoltage((float)e.gateV(2));

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			for (int i = 0; i < NV; i++) {
				bool on = i < n && e.v[i].gate;
				float b = on ? 0.35f + 0.065f * (float)e.pushV(i) : 0.f;
				bool rub = on && e.v[i].rubbing;
				lights[VOICE_LIGHT + 2 * i].setBrightnessSmooth(rub ? 0.f : b, dt);
				lights[VOICE_LIGHT + 2 * i + 1].setBrightnessSmooth(rub ? b : 0.f, dt);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(string::f("%016llx", (unsigned long long)seed).c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) seed = std::strtoull(json_string_value(s), nullptr, 16);
		needInit = true;
	}
};

bool RubQ::hz() const {
	Bulgaria* m = dynamic_cast<Bulgaria*>(module);
	return m && m->params[Bulgaria::UNIT_P].getValue() > 0.5f;
}

struct BulgariaWidget : ModuleWidget {
	BulgariaWidget(Bulgaria* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bulgaria.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		typedef Bulgaria M;

		const int r1[4] = {M::ROOT_P, M::OCT_P, M::MODE_P, M::TUNE_P};
		for (int i = 0; i < 4; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW1_X[i], K1)), module, r1[i]));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[0], K2)), module, M::VOICES_P));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[1], K2)), module, M::SPREAD_P));
		for (int i = 0; i < NV; i++)
			addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(lightx(i), K2)), module, M::VOICE_LIGHT + 2 * i));

		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(RUB_X, K3)), module, M::RUB_P));
		addParam(createParamCentered<CKSS>(mm2px(Vec(SW_X[0], K3)), module, M::UNIT_P));
		addParam(createParamCentered<CKSS>(mm2px(Vec(SW_X[1], K3)), module, M::WHEN_P));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(CHANCE_X, K3)), module, M::CHANCE_P));
		addParam(createParamCentered<CKSS>(mm2px(Vec(SW_X[2], K3)), module, M::CADENCE_P));

		const int r4[4] = {M::SLIDE_P, M::SHAKE_P, M::WHOOP_P, M::LOOSE_P};
		const int t4[4] = {M::SLIDE_INPUT, M::SHAKE_INPUT, M::WHOOP_INPUT, M::LOOSE_INPUT};
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW4_X[i], K4)), module, r4[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ROW4_X[i], TRIG_Y)), module, t4[i]));
		}

		const int in1[7] = {M::VOCT_INPUT, M::GATE_INPUT, M::DRONE_INPUT, M::PHRASE_INPUT, M::RUB_INPUT, M::TUNE_INPUT, M::VOICES_INPUT};
		for (int i = 0; i < 7; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackx(i), J1)), module, in1[i]));
		const int o2[7] = {M::LEAD_VOCT_OUTPUT, M::LEAD_GATE_OUTPUT, M::DRONE_VOCT_OUTPUT, M::DRONE_GATE_OUTPUT,
			M::POLY_VOCT_OUTPUT, M::POLY_GATE_OUTPUT, M::POLY_PUSH_OUTPUT};
		for (int i = 0; i < 7; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackx(i), J2)), module, o2[i]));
	}
};

Model* modelBulgaria = createModel<Bulgaria, BulgariaWidget>("Bulgaria");
