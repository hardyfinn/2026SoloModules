// 808 Kick: a circuit model of the TR-808 bass drum with its inner parts on the panel.
// The sound engine is in KickEngine.hpp, with notes on the circuit.
#include "plugin.hpp"
#include "KickEngine.hpp"

// ---- LAYOUT (same numbers in res/make_kick_panel.py) ----
static const float COL[7] = {11.f, 31.04f, 51.08f, 71.12f, 91.16f, 111.2f, 131.24f};
static const float ROW_STOCK = 30.f, ROW_ATK = 52.f, ROW_BODY = 74.f, ROW_CV = 96.f, ROW_IO = 115.f;

using kick808::Stock;

struct Kick808 : Module {
	enum ParamIds {
		TUNE_PARAM, DECAY_PARAM, TONE_PARAM, ACCENT_PARAM, LEVEL_PARAM, AGE_PARAM, HIT_PARAM,
		SWEEP_PARAM, BEND_PARAM, STRIKE_PARAM, CLICK_PARAM, RETRIG_PARAM, SIGH_PARAM, TENSION_PARAM,
		SHELF_PARAM, DRIVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, DECAY_INPUT, TONE_INPUT, SWEEP_INPUT, SIGH_INPUT, TENSION_INPUT, DRIVE_INPUT,
		TRIG_INPUT, ACCENT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { ENV_OUTPUT, OUT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HIT_LIGHT, NUM_LIGHTS };

	kick808::Engine engine;
	kick808::Controls ctl;
	dsp::SchmittTrigger trigIn, hitButton;
	dsp::ClockDivider lightDivider;
	uint32_t serial = 808;

	struct DecayQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			if (v > 0.95f) return string::f("sustain (loop gain %.0f %% over one)", (v - 0.95f) / 0.05f * 100.f);
			float t = kick808::decayT60(v);
			return string::f("%.2f s ring%s", t, v > 0.8f ? " (past the stock range)" : "");
		}
	};
	struct MapQuantity : ParamQuantity {
		float (*fn)(float) = nullptr;
		const char* unit = "";
		float mul = 1.f;
		std::string getDisplayValueString() override {
			return fn ? string::f("%.3g%s", fn(getValue()) * mul, unit) : ParamQuantity::getDisplayValueString();
		}
	};

	template <class Q = ParamQuantity>
	Q* knob(int id, float def, const char* name, const char* unit = "%", float mul = 100.f) {
		return configParam<Q>(id, 0.f, 1.f, def, name, unit, 0.f, mul);
	}
	void mapped(int id, float def, const char* name, float (*fn)(float), const char* unit, float mul = 1.f) {
		MapQuantity* q = configParam<MapQuantity>(id, 0.f, 1.f, def, name);
		q->fn = fn; q->unit = unit; q->mul = mul;
	}

	Kick808() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TUNE_PARAM, std::log2(20.f), std::log2(220.f), std::log2(Stock::TUNE_HZ), "Tune (bridged-T centre; the 808 sits at 49.5 Hz)", " Hz", 2.f);
		configParam<DecayQuantity>(DECAY_PARAM, 0.f, 1.f, Stock::DECAY, "Decay (feedback buffer; past 3 o'clock it rings on, at the top it sustains)");
		mapped(TONE_PARAM, Stock::TONE, "Tone (output low-pass)", kick808::toneHz, " Hz");
		knob(ACCENT_PARAM, Stock::ACCENT, "Accent level (how far an accented hit goes from 4 V towards 14 V)");
		knob(LEVEL_PARAM, 1.f, "Level")->randomizeEnabled = false;
		knob(AGE_PARAM, Stock::AGE, "Age (0 = new unit on spec; up = this unit's part tolerances, hit-to-hit drift, worn caps at the top)")->randomizeEnabled = false;
		configButton(HIT_PARAM, "Hit");
		mapped(SWEEP_PARAM, Stock::SWEEP, "Sweep (attack jump: Q43 shorts part of the bridged-T)", kick808::sweepOct, " oct");
		mapped(BEND_PARAM, Stock::BEND, "Bend (attack jump time)", kick808::bendMs, " ms");
		mapped(STRIKE_PARAM, Stock::STRIKE, "Strike (trigger pulse width)", kick808::strikeMs, " ms");
		mapped(CLICK_PARAM, Stock::CLICK, "Click (shaped pulse straight through the bridged-T)", kick808::clickGain, "x");
		knob(RETRIG_PARAM, Stock::RETRIG, "Retrig (second pulse: small at stock, up = louder and later, a flam)");
		knob(SIGH_PARAM, Stock::SIGH, "Sigh (Q43 leak: loud cycles run sharp, then settle)");
		knob(TENSION_PARAM, Stock::TENSION, "Tension (louder = higher, like a skin)");
		knob(SHELF_PARAM, Stock::SHELF, "Shelf (feedback high shelf: up = raised pitches die faster, down = they ring)");
		knob(DRIVE_PARAM, Stock::DRIVE, "Drive (output op-amp into its rails)");

		configInput(VOCT_INPUT, "V/OCT (tune)");
		configInput(DECAY_INPUT, "Decay CV (+-5 V = full range)");
		configInput(TONE_INPUT, "Tone CV (+-5 V = full range)");
		configInput(SWEEP_INPUT, "Sweep CV (+-5 V = full range)");
		configInput(SIGH_INPUT, "Sigh CV (+-5 V = full range)");
		configInput(TENSION_INPUT, "Tension CV (+-5 V = full range)");
		configInput(DRIVE_INPUT, "Drive CV (+-5 V = full range)");
		configInput(TRIG_INPUT, "Trigger");
		configInput(ACCENT_INPUT, "Accent (gate = accented; 0-5 V = how much, read at each trigger)");
		configOutput(ENV_OUTPUT, "Envelope (ring amplitude, 0-10 V)");
		configOutput(OUT_OUTPUT, "Kick");

		lightDivider.setDivision(256);
		serial = random::u32();
		engine.age.setSerial(serial);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); engine.reset(); }

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "serial", json_integer(serial));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "serial");
		if (s) { serial = (uint32_t)json_integer_value(s); engine.age.setSerial(serial); }
	}

	float knobCv(int p, int in) {
		return clamp(params[p].getValue() + inputs[in].getVoltage() * 0.1f, 0.f, 1.f);
	}

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);
		ctl.tuneHz = std::exp2(params[TUNE_PARAM].getValue() + inputs[VOCT_INPUT].getVoltage());
		ctl.decay = knobCv(DECAY_PARAM, DECAY_INPUT);
		ctl.tone = knobCv(TONE_PARAM, TONE_INPUT);
		ctl.accent = params[ACCENT_PARAM].getValue();
		ctl.level = params[LEVEL_PARAM].getValue();
		ctl.age = params[AGE_PARAM].getValue();
		ctl.sweep = knobCv(SWEEP_PARAM, SWEEP_INPUT);
		ctl.bend = params[BEND_PARAM].getValue();
		ctl.strike = params[STRIKE_PARAM].getValue();
		ctl.click = params[CLICK_PARAM].getValue();
		ctl.retrig = params[RETRIG_PARAM].getValue();
		ctl.sigh = knobCv(SIGH_PARAM, SIGH_INPUT);
		ctl.tension = knobCv(TENSION_PARAM, TENSION_INPUT);
		ctl.shelf = params[SHELF_PARAM].getValue();
		ctl.drive = knobCv(DRIVE_PARAM, DRIVE_INPUT);

		bool hit = trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		hit |= hitButton.process(params[HIT_PARAM].getValue());
		if (hit) {
			float acc = inputs[ACCENT_INPUT].isConnected() ? clamp(inputs[ACCENT_INPUT].getVoltage() / 5.f, 0.f, 1.f) : 0.f;
			engine.hit(acc, ctl);
		}
		engine.process(ctl);
		outputs[OUT_OUTPUT].setVoltage(engine.out);
		outputs[ENV_OUTPUT].setVoltage(engine.env);

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[HIT_LIGHT].setBrightnessSmooth(engine.env * 0.1f, dt, 30.f);
		}
	}

	// every knob back to the stock 808 (LEVEL left alone)
	void resetToStock() {
		params[TUNE_PARAM].setValue(std::log2(Stock::TUNE_HZ));
		params[DECAY_PARAM].setValue(Stock::DECAY);
		params[TONE_PARAM].setValue(Stock::TONE);
		params[ACCENT_PARAM].setValue(Stock::ACCENT);
		params[AGE_PARAM].setValue(Stock::AGE);
		params[SWEEP_PARAM].setValue(Stock::SWEEP);
		params[BEND_PARAM].setValue(Stock::BEND);
		params[STRIKE_PARAM].setValue(Stock::STRIKE);
		params[CLICK_PARAM].setValue(Stock::CLICK);
		params[RETRIG_PARAM].setValue(Stock::RETRIG);
		params[SIGH_PARAM].setValue(Stock::SIGH);
		params[TENSION_PARAM].setValue(Stock::TENSION);
		params[SHELF_PARAM].setValue(Stock::SHELF);
		params[DRIVE_PARAM].setValue(Stock::DRIVE);
	}
};

struct Kick808Widget : ModuleWidget {
	Kick808Widget(Kick808* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kick808.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Kick808 K;
		const int stock[6] = {K::TUNE_PARAM, K::DECAY_PARAM, K::TONE_PARAM, K::ACCENT_PARAM, K::LEVEL_PARAM, K::AGE_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_STOCK)), module, stock[i]));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(COL[6], ROW_STOCK)), module, K::HIT_PARAM, K::HIT_LIGHT));
		const int atk[7] = {K::SWEEP_PARAM, K::BEND_PARAM, K::STRIKE_PARAM, K::CLICK_PARAM, K::RETRIG_PARAM, K::SIGH_PARAM, K::TENSION_PARAM};
		for (int i = 0; i < 7; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_ATK)), module, atk[i]));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[0], ROW_BODY)), module, K::SHELF_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[1], ROW_BODY)), module, K::DRIVE_PARAM));

		const int cv[7] = {K::VOCT_INPUT, K::DECAY_INPUT, K::TONE_INPUT, K::SWEEP_INPUT, K::SIGH_INPUT, K::TENSION_INPUT, K::DRIVE_INPUT};
		for (int i = 0; i < 7; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_CV)), module, cv[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[0], ROW_IO)), module, K::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[1], ROW_IO)), module, K::ACCENT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], ROW_IO)), module, K::ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[6], ROW_IO)), module, K::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Kick808* m = dynamic_cast<Kick808*>(this->module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Reset to stock 808", "", [=]() { m->resetToStock(); }));
		menu->addChild(createMenuLabel(string::f("Serial number %u (sets this unit's AGE character)", (unsigned)m->serial)));
		menu->addChild(createMenuItem("New serial number (a different old unit)", "", [=]() {
			m->serial = random::u32();
			m->engine.age.setSerial(m->serial);
		}));
	}
};

Model* modelKick808 = createModel<Kick808, Kick808Widget>("Kick808");
