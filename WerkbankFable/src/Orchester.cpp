#include "plugin.hpp"
#include "dsp/Orchester.hpp"

using namespace wbui;
namespace L = layout::Orchester;

static const int MAXV = 16;
static const std::vector<std::string> DISC_NAMES = {"Choir", "Strings", "Flute", "Organ", "Cello", "Horn", "Vox", "Reed"};

struct Orchester : Module {
	enum ParamIds { DISC_PARAM, WOBBLE_PARAM, TONE_PARAM, ATTACK_PARAM, RELEASE_PARAM, OCTAVE_PARAM, AGE_PARAM, LEVEL_PARAM, NUM_PARAMS };
	enum InputIds { VOCT_INPUT, GATE_INPUT, DISC_INPUT, WOBBLE_INPUT, TONE_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_OUTPUT, NUM_OUTPUTS };

	wb::OrchVoice voices[MAXV];
	wb::OrchDisc disc;
	bool gateWas[MAXV] = {};
	wb::XS32 hissRng;
	double humPh = 0;
	Smooth sm[6];
	uint64_t seed = 1;
	bool needInit = true;
	double rate = 0;

	Orchester() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		configSwitch(DISC_PARAM, 0.f, 7.f, 0.f, "Disc", DISC_NAMES)->description =
			"Which optical disc is in the machine. A change takes effect on each key's next press, as if the disc had been swapped.";
		configParam(WOBBLE_PARAM, 0.f, 1.f, 0.5f, "Wobble", "%", 0.f, 100.f)->description =
			"How much the disc's eccentricity bends pitch and level once per revolution. All keys wobble together.";
		configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", "%", 0.f, 100.f);
		configParam<ExpTimeQuantity<5000, 2000000>>(ATTACK_PARAM, 0.f, 1.f, 0.f, "Attack");
		configParam<ExpTimeQuantity<30000, 4000000>>(RELEASE_PARAM, 0.f, 1.f, 0.f, "Release");
		configSwitch(OCTAVE_PARAM, 0.f, 2.f, 1.f, "Octave", {"-1", "0", "+1"});
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"From a fresh disc to a worn one: dust, darker photocells, more splice thump. Above 60% lamp hum.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configInput(VOCT_INPUT, "V/OCT (poly)");
		configInput(GATE_INPUT, "Gate (poly)");
		configInput(DISC_INPUT, "Disc CV (1.25 V per disc, added to the knob)");
		configInput(WOBBLE_INPUT, "Wobble CV (10 V = full range)");
		configInput(TONE_INPUT, "Tone CV (10 V = full range)");
		configOutput(OUT_OUTPUT, "Audio");
		seed = random::u64();
	}

	void initVoices(double sr) {
		rate = sr;
		disc.init(sr);
		for (int c = 0; c < MAXV; c++) { voices[c].init(sr, (uint32_t)(seed & 0x7FFFFFFF) + 31u * (uint32_t)c + 777u); gateWas[c] = false; }
		hissRng.seed((uint32_t)(seed >> 35) | 1u);
		needInit = false;
	}
	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) initVoices(args.sampleRate);
		const double a = smoothCoef(args.sampleTime);
		wb::OrchParams P;
		P.wobble = wb::clampd(sm[0].process(params[WOBBLE_PARAM].getValue(), a) + 0.1 * inputs[WOBBLE_INPUT].getVoltage(), 0.0, 1.0);
		P.tone = wb::clampd(sm[1].process(params[TONE_PARAM].getValue(), a) + 0.1 * inputs[TONE_INPUT].getVoltage(), 0.0, 1.0);
		P.attack = expMap(sm[2].process(params[ATTACK_PARAM].getValue(), a), 0.005, 2.0);
		P.release = expMap(sm[3].process(params[RELEASE_PARAM].getValue(), a), 0.03, 4.0);
		P.age = sm[4].process(params[AGE_PARAM].getValue(), a);
		P.octave = std::round(params[OCTAVE_PARAM].getValue()) - 1.0;
		const double level = sm[5].process(params[LEVEL_PARAM].getValue(), a);
		const int discNow = clamp((int)std::round(params[DISC_PARAM].getValue() + inputs[DISC_INPUT].getVoltage() / 1.25f), 0, 7);

		double th0, th1;
		disc.advance(th0, th1);
		const int ch = std::max({1, inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels()});
		double sum = 0, envMax = 0;
		for (int c = 0; c < ch; c++) {
			double pitch = wb::clampd(inputs[VOCT_INPUT].getPolyVoltage(c), -6.0, 6.0);
			double gate = inputs[GATE_INPUT].getPolyVoltage(c) >= 1.f ? 1.0 : 0.0;
			bool g = gate > 0.5;
			if (g && !gateWas[c]) voices[c].reset(discNow, (int)std::lround((pitch + P.octave) * 12.0));
			gateWas[c] = g;
			if (!g && voices[c].silent()) continue;
			sum += voices[c].process(pitch, gate, th0, th1, P);
			envMax = std::max(envMax, voices[c].env);
		}
		for (int c = ch; c < MAXV; c++) gateWas[c] = false;

		// shared photocell hiss and lamp hum, while any key sounds
		const double age = P.age;
		const double hiss = std::pow(10.0, (-66.0 + 20.0 * age) / 20.0);
		const double hum = age <= 0.6 ? 0.0 : (age - 0.6) / 0.4 * 4.0e-4;
		humPh += 100.0 * args.sampleTime; if (humPh >= 1.0) humPh -= 1.0;
		double r = hissRng.next();
		sum += std::min(envMax, 1.0) * (hiss * r * 1.73 * std::sqrt(args.sampleRate / 48000.0) + hum * std::sin(2.0 * M_PI * humPh));
		outputs[OUT_OUTPUT].setVoltage(outClamp(sum * 5.0 * level));
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", seedToJson(seed));
		return root;
	}
	void dataFromJson(json_t* root) override { seedFromJson(root, seed); needInit = true; }
};

struct OrchesterWidget : ModuleWidget {
	OrchesterWidget(Orchester* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Orchester.svg")));
		addScrews(this);
		auto* d = createParamCentered<RoundLargeBlackKnob>(mm(L::DISC), module, Orchester::DISC_PARAM);
		d->snap = true;
		addParam(d);
		addParam(createParamCentered<RoundBlackKnob>(mm(L::WOBBLE), module, Orchester::WOBBLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm(L::TONE), module, Orchester::TONE_PARAM));
		addParam(createParamCentered<CKSSThree>(mm(L::OCTAVE), module, Orchester::OCTAVE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::ATTACK), module, Orchester::ATTACK_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::RELEASE), module, Orchester::RELEASE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::AGE), module, Orchester::AGE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm(L::LEVEL), module, Orchester::LEVEL_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm(L::VOCT_IN), module, Orchester::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::GATE_IN), module, Orchester::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::DISC_IN), module, Orchester::DISC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::WOBBLE_IN), module, Orchester::WOBBLE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm(L::TONE_IN), module, Orchester::TONE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm(L::OUT), module, Orchester::OUT_OUTPUT));
	}
	void appendContextMenu(Menu* menu) override {
		Orchester* m = dynamic_cast<Orchester*>(module);
		if (!m) return;
		appendUnitMenu(menu, m->seed, [=]() { m->seed = random::u64(); m->needInit = true; });
	}
};

Model* modelOrchester = createModel<Orchester, OrchesterWidget>("Orchester");
