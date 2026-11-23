#include "plugin.hpp"
#include "dsp/Voice.hpp"

using namespace sawstack;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COLS[6] = {11.5f, 29.25f, 47.0f, 64.75f, 82.5f, 100.25f};
static const float Y_BIG = 27.5f, Y_ATT = 41.5f, Y_CV = 53.0f, Y_MED = 74.0f, Y_FATT = 85.5f, Y_IN = 99.0f, Y_OUT = 114.0f;
static const float LIGHT_DX = 6.6f, LIGHT_DY = -5.2f;

static const int MAX_VOICES = 16;

static double knobTime(double k) { return 0.002 * std::pow(5000.0, k); }   // 2 ms .. 10 s

struct Sawstack : Module {
	enum ParamIds {
		FREQ_PARAM, SPREAD_PARAM, TIMBRE_PARAM, RICH_PARAM, AMP_PARAM, OUTPUT_PARAM,
		FREQ_ATT_PARAM, SPREAD_ATT_PARAM, TIMBRE_ATT_PARAM, RICH_ATT_PARAM, AMP_ATT_PARAM,
		RISE1_PARAM, FALL1_PARAM, RISE2_PARAM, FALL2_PARAM, FALL1_ATT_PARAM, FALL2_ATT_PARAM,
		AGE_PARAM, DRIVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREQ_INPUT, SPREAD_INPUT, TIMBRE_INPUT, RICH_INPUT, AMP_INPUT,
		VOCT_INPUT, GATE1_INPUT, FALL1_INPUT, GATE2_INPUT, FALL2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { ENV1_OUTPUT, END1_OUTPUT, ENV2_OUTPUT, END2_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENV1_LIGHT, ENV2_LIGHT, NUM_LIGHTS };

	Voice voices[MAX_VOICES];
	Envelope env1[MAX_VOICES], env2[MAX_VOICES];
	OutFilter outFilter;
	uint64_t seed = 1;
	int voicing = OCTAVES;
	bool snapFreq = false;
	double rate = 0;
	bool needInit = true;
	int channels = 1;

	// 5 ms knob smoothing
	double sm[6] = {};
	bool smInit = false;
	dsp::ClockDivider ctrlDiv, lightDiv;
	float light1 = 0.f, light2 = 0.f;

	Sawstack() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, -4.f, 4.f, 0.f, "Frequency", " Hz", 2.f, (float)F0)->description =
			"Pitch of the centre oscillator. V/OCT adds to it.";

		struct SpreadQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = clamp(getValue(), 0.f, 1.f);
				int v = module ? ((Sawstack*)module)->voicing : OCTAVES;
				double inner = k * k * k * std::fabs(voicingRatios(v)[3]) * 1200.0;
				if (inner < 100.0) return string::f("inner pair %.1f cents", inner);
				return string::f("inner pair %.2f semitones", inner / 100.0);
			}
		};
		configParam<SpreadQuantity>(SPREAD_PARAM, 0.f, 1.f, 0.25f, "Spread")->description =
			"Two oscillators tune down and two tune up. The bottom third is a fine detune; the top reaches the chosen STACK voicing (right-click).";
		configParam(TIMBRE_PARAM, 0.f, 1.f, 1.f, "Timbre (sine to saw)", "%", 0.f, 100.f);

		struct RichQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = clamp(getValue(), 0.f, 1.f);
				return string::f("saw x%.2f", 1.0 + 7.0 * k * k);
			}
		};
		configParam<RichQuantity>(RICH_PARAM, 0.f, 1.f, 0.f, "Richness")->description =
			"Multiplies the saw, like sweeping a hard-synced oscillator. Up to 8 teeth per cycle.";
		configParam(AMP_PARAM, 0.f, 1.f, 0.f, "Amplitude (VCA offset)", "%", 0.f, 100.f)->description =
			"Opens the VCA by itself. Leave at zero to play the VCA from envelope 2 (normalled to the AMP input).";
		configParam(OUTPUT_PARAM, 0.f, 1.f, 1.f, "Output level", "%", 0.f, 100.f)->description = "At full the output is 10 V peak to peak.";
		configParam(FREQ_ATT_PARAM, -1.f, 1.f, 0.f, "FREQ CV amount", "%", 0.f, 100.f);
		configParam(SPREAD_ATT_PARAM, -1.f, 1.f, 0.f, "SPREAD CV amount", "%", 0.f, 100.f);
		configParam(TIMBRE_ATT_PARAM, -1.f, 1.f, 0.f, "TIMBRE CV amount", "%", 0.f, 100.f);
		configParam(RICH_ATT_PARAM, -1.f, 1.f, 0.f, "RICHNESS CV amount (envelope 1 when unpatched)", "%", 0.f, 100.f);
		configParam(AMP_ATT_PARAM, -1.f, 1.f, 1.f, "AMP CV amount (envelope 2 when unpatched)", "%", 0.f, 100.f);

		struct TimeQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double t = knobTime(clamp(getValue(), 0.f, 1.f));
				return t < 1.0 ? string::f("%.0f ms", t * 1000.0) : string::f("%.2f s", t);
			}
		};
		configParam<TimeQuantity>(RISE1_PARAM, 0.f, 1.f, 0.f, "Envelope 1 rise");
		configParam<TimeQuantity>(FALL1_PARAM, 0.f, 1.f, 0.55f, "Envelope 1 fall");
		configParam<TimeQuantity>(RISE2_PARAM, 0.f, 1.f, 0.f, "Envelope 2 rise");
		configParam<TimeQuantity>(FALL2_PARAM, 0.f, 1.f, 0.6f, "Envelope 2 fall");
		configParam(FALL1_ATT_PARAM, -1.f, 1.f, 0.f, "FALL 1 CV amount (octaves per volt at full)", "%", 0.f, 100.f);
		configParam(FALL2_ATT_PARAM, -1.f, 1.f, 0.f, "FALL 2 CV amount (octaves per volt at full)", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.6f, "Age", "%", 0.f, 100.f)->description =
			"From factory-fresh to tired: component mismatch, drift, jitter, imperfect waveshapes, oscillators pulling into lock, loose envelopes. Above 60% a little hiss and VCA bleed come in.";
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.35f, "Drive", "%", 0.f, 100.f)->description =
			"Saturation in each oscillator and in the output stage. When the five line up in unison the mix compresses.";

		configInput(FREQ_INPUT, "FREQ CV (1 V/oct at full)");
		configInput(SPREAD_INPUT, "SPREAD CV");
		configInput(TIMBRE_INPUT, "TIMBRE CV (patch velocity here)");
		configInput(RICH_INPUT, "RICHNESS CV (envelope 1 when unpatched)");
		configInput(AMP_INPUT, "AMP CV (envelope 2 when unpatched)");
		configInput(VOCT_INPUT, "V/OCT");
		configInput(GATE1_INPUT, "Gate 1");
		configInput(FALL1_INPUT, "FALL 1 CV");
		configInput(GATE2_INPUT, "Gate 2 (follows gate 1 when unpatched)");
		configInput(FALL2_INPUT, "FALL 2 CV");
		configOutput(ENV1_OUTPUT, "Envelope 1 (0 to 10 V)");
		configOutput(END1_OUTPUT, "Envelope 1 end pulse");
		configOutput(ENV2_OUTPUT, "Envelope 2 (0 to 10 V)");
		configOutput(END2_OUTPUT, "Envelope 2 end pulse");
		configOutput(L_OUTPUT, "Left (mono mix when only one output is patched)");
		configOutput(R_OUTPUT, "Right (mono mix when only one output is patched)");
		configLight(ENV1_LIGHT, "Envelope 1");
		configLight(ENV2_LIGHT, "Envelope 2");

		ctrlDiv.setDivision(16);
		lightDiv.setDivision(64);
		seed = random::u64();
	}

	void initVoices(double sr) {
		rate = sr;
		for (int c = 0; c < MAX_VOICES; c++) {
			voices[c].init(sr, seed + (uint64_t)c * 0x9E3779B97F4A7C15ULL, params[AGE_PARAM].getValue(), params[DRIVE_PARAM].getValue());
			env1[c].init(seed * 3 + 1 + c * 7919);
			env2[c].init(seed * 3 + 2 + c * 7919);
		}
		outFilter.init(sr);
		needInit = false;
	}
	void newUnit() { seed = random::u64(); needInit = true; }

	void onReset() override { voicing = OCTAVES; snapFreq = false; needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent& e) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) initVoices(args.sampleRate);

		const int ids[6] = {FREQ_PARAM, SPREAD_PARAM, TIMBRE_PARAM, RICH_PARAM, AMP_PARAM, OUTPUT_PARAM};
		const double a = 1.0 - std::exp(-args.sampleTime / 0.005);
		for (int i = 0; i < 6; i++) {
			double v = params[ids[i]].getValue();
			if (i == 0 && snapFreq) v = std::round(v * 12.0) / 12.0;
			if (!smInit) sm[i] = v; else sm[i] += a * (v - sm[i]);
		}
		smInit = true;

		if (ctrlDiv.process()) {
			double age = params[AGE_PARAM].getValue(), drive = params[DRIVE_PARAM].getValue();
			for (int c = 0; c < MAX_VOICES; c++) voices[c].setAgeDrive(age, drive);
		}

		channels = std::max({1, inputs[VOCT_INPUT].getChannels(), inputs[GATE1_INPUT].getChannels(), inputs[GATE2_INPUT].getChannels()});

		const bool lOn = outputs[L_OUTPUT].isConnected(), rOn = outputs[R_OUTPUT].isConnected();
		const bool mono = lOn != rOn;
		const double age = params[AGE_PARAM].getValue();
		const double r1 = knobTime(params[RISE1_PARAM].getValue()), f1k = knobTime(params[FALL1_PARAM].getValue());
		const double r2 = knobTime(params[RISE2_PARAM].getValue()), f2k = knobTime(params[FALL2_PARAM].getValue());
		const double f1a = params[FALL1_ATT_PARAM].getValue(), f2a = params[FALL2_ATT_PARAM].getValue();
		const double fa = params[FREQ_ATT_PARAM].getValue(), sa = params[SPREAD_ATT_PARAM].getValue();
		const double ta = params[TIMBRE_ATT_PARAM].getValue(), ra = params[RICH_ATT_PARAM].getValue();
		const double aa = params[AMP_ATT_PARAM].getValue();
		const bool g1On = inputs[GATE1_INPUT].isConnected(), g2On = inputs[GATE2_INPUT].isConnected();
		const bool richOn = inputs[RICH_INPUT].isConnected(), ampOn = inputs[AMP_INPUT].isConnected();

		double l2[2] = {0, 0}, r2s[2] = {0, 0};
		float maxE1 = 0.f, maxE2 = 0.f;
		for (int c = 0; c < channels; c++) {
			double g1 = g1On ? inputs[GATE1_INPUT].getPolyVoltage(c) : 0.0;
			double g2 = g2On ? inputs[GATE2_INPUT].getPolyVoltage(c) : g1;
			double fall1 = std::min(std::max(f1k * std::exp2(f1a * inputs[FALL1_INPUT].getPolyVoltage(c)), 0.0005), 20.0);
			double fall2 = std::min(std::max(f2k * std::exp2(f2a * inputs[FALL2_INPUT].getPolyVoltage(c)), 0.0005), 20.0);
			bool end1, end2;
			double e1 = env1[c].process(g1, r1, fall1, age, args.sampleRate, end1);
			double e2 = env2[c].process(g2, r2, fall2, age, args.sampleRate, end2);
			outputs[ENV1_OUTPUT].setVoltage((float)(e1 * 10.0), c);
			outputs[ENV2_OUTPUT].setVoltage((float)(e2 * 10.0), c);
			outputs[END1_OUTPUT].setVoltage(end1 ? 10.f : 0.f, c);
			outputs[END2_OUTPUT].setVoltage(end2 ? 10.f : 0.f, c);
			maxE1 = std::max(maxE1, (float)e1); maxE2 = std::max(maxE2, (float)e2);

			double richCv = richOn ? inputs[RICH_INPUT].getPolyVoltage(c) : e1 * 10.0;
			double ampCv = ampOn ? inputs[AMP_INPUT].getPolyVoltage(c) : e2 * 10.0;
			double pitch = sm[0] + inputs[VOCT_INPUT].getPolyVoltage(c) + fa * inputs[FREQ_INPUT].getPolyVoltage(c);
			pitch = std::min(std::max(pitch, -10.0), 10.0);
			double spread = sm[1] + sa * inputs[SPREAD_INPUT].getPolyVoltage(c) * 0.1;
			double timbre = sm[2] + ta * inputs[TIMBRE_INPUT].getPolyVoltage(c) * 0.1;
			double rich = sm[3] + ra * richCv * 0.1;
			double amp = sm[4] + aa * ampCv * 0.1;
			voices[c].processOS(pitch, spread, timbre, rich, amp, voicing, mono, l2, r2s);
		}
		for (int id : {ENV1_OUTPUT, ENV2_OUTPUT, END1_OUTPUT, END2_OUTPUT}) outputs[id].setChannels(channels);

		double L, R;
		outFilter.process(l2, r2s, mono, L, R);
		const double out = sm[5] * 5.0;
		outputs[L_OUTPUT].setVoltage((float)(L * out));
		outputs[R_OUTPUT].setVoltage((float)(R * out));

		light1 = std::max(light1, maxE1); light2 = std::max(light2, maxE2);
		if (lightDiv.process()) {
			lights[ENV1_LIGHT].setBrightness(light1);
			lights[ENV2_LIGHT].setBrightness(light2);
			light1 = maxE1; light2 = maxE2;
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(string::f("%016llx", (unsigned long long)seed).c_str()));
		json_object_set_new(root, "voicing", json_integer(voicing));
		json_object_set_new(root, "snapFreq", json_boolean(snapFreq));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) seed = std::strtoull(json_string_value(s), nullptr, 16);
		json_t* v = json_object_get(root, "voicing");
		if (v) voicing = clamp((int)json_integer_value(v), 0, NUM_VOICINGS - 1);
		json_t* f = json_object_get(root, "snapFreq");
		if (f) snapFreq = json_boolean_value(f);
		needInit = true;
	}
};

struct OrangeLightBase : GrayModuleLightWidget {
	OrangeLightBase() { addBaseColor(nvgRGB(232, 115, 58)); }
};
using EnvLight = SmallLight<OrangeLightBase>;

struct SawstackWidget : ModuleWidget {
	SawstackWidget(Sawstack* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Sawstack.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const int big[6] = {Sawstack::FREQ_PARAM, Sawstack::SPREAD_PARAM, Sawstack::TIMBRE_PARAM, Sawstack::RICH_PARAM, Sawstack::AMP_PARAM, Sawstack::OUTPUT_PARAM};
		for (int i = 0; i < 6; i++) addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COLS[i], Y_BIG)), module, big[i]));
		const int att[5] = {Sawstack::FREQ_ATT_PARAM, Sawstack::SPREAD_ATT_PARAM, Sawstack::TIMBRE_ATT_PARAM, Sawstack::RICH_ATT_PARAM, Sawstack::AMP_ATT_PARAM};
		for (int i = 0; i < 5; i++) addParam(createParamCentered<Trimpot>(mm2px(Vec(COLS[i], Y_ATT)), module, att[i]));
		const int cv[5] = {Sawstack::FREQ_INPUT, Sawstack::SPREAD_INPUT, Sawstack::TIMBRE_INPUT, Sawstack::RICH_INPUT, Sawstack::AMP_INPUT};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[i], Y_CV)), module, cv[i]));

		const int med[6] = {Sawstack::RISE1_PARAM, Sawstack::FALL1_PARAM, Sawstack::RISE2_PARAM, Sawstack::FALL2_PARAM, Sawstack::AGE_PARAM, Sawstack::DRIVE_PARAM};
		for (int i = 0; i < 6; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLS[i], Y_MED)), module, med[i]));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(COLS[1], Y_FATT)), module, Sawstack::FALL1_ATT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(COLS[3], Y_FATT)), module, Sawstack::FALL2_ATT_PARAM));
		addChild(createLightCentered<EnvLight>(mm2px(Vec(COLS[0] + LIGHT_DX, Y_MED + LIGHT_DY)), module, Sawstack::ENV1_LIGHT));
		addChild(createLightCentered<EnvLight>(mm2px(Vec(COLS[2] + LIGHT_DX, Y_MED + LIGHT_DY)), module, Sawstack::ENV2_LIGHT));

		const int in[5] = {Sawstack::VOCT_INPUT, Sawstack::GATE1_INPUT, Sawstack::FALL1_INPUT, Sawstack::GATE2_INPUT, Sawstack::FALL2_INPUT};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[i], Y_IN)), module, in[i]));
		const int out[6] = {Sawstack::ENV1_OUTPUT, Sawstack::END1_OUTPUT, Sawstack::ENV2_OUTPUT, Sawstack::END2_OUTPUT, Sawstack::L_OUTPUT, Sawstack::R_OUTPUT};
		for (int i = 0; i < 6; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[i], Y_OUT)), module, out[i]));
	}

	void appendContextMenu(Menu* menu) override {
		Sawstack* m = dynamic_cast<Sawstack*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("STACK voicing (top of SPREAD)",
			{"Octaves: -2, -1, 0, +1, +2 octaves", "Fifths: octave, fifth, root, fifth, octave", "Harmonics: harmonics 1 to 5 (centre is the 3rd)"},
			[=]() { return m->voicing; }, [=](size_t i) { m->voicing = (int)i; }));
		menu->addChild(createBoolPtrMenuItem("FREQUENCY knob snaps to semitones", "", &m->snapFreq));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Unit %08llx", (unsigned long long)(m->seed >> 32))));
		menu->addChild(createMenuItem("New unit", "", [=]() { m->newUnit(); }));
	}
};

Model* modelSawstack = createModel<Sawstack, SawstackWidget>("Sawstack");
