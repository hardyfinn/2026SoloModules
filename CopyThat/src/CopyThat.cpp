// Copy That: an FM radio link that loses the signal (Roger That-style effect).
// The engine is in CopyThatDSP.hpp; this file is the Rack module and panel.
#include "plugin.hpp"
#include "CopyThatDSP.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float W_MM = 71.12f;
static const float TUNE_X = 35.56f, TUNE_Y = 29.6f, TATT_X = 61.0f;
static const float METER_Y = 42.0f, LOCK_Y = 36.0f, METER_X0 = 26.06f, METER_DX = 4.75f;
static const float ROW2_Y = 54.0f, L_X = 17.0f, R_X = 54.12f, ROW3_Y = 73.6f;
static const float SW_Y = 87.4f, SW_X[3] = {12.0f, 33.0f, 54.0f};
static const float CV_Y = 102.5f, IO_Y = 115.5f;
static const float CV_X[4] = {10.0f, 27.0f, 44.0f, 61.0f};
static const float OUT_X[3] = {29.0f, 44.0f, 59.0f};

struct CopyThat : Module {
	enum ParamId { TUNE_PARAM, TATT_PARAM, SHIFT_PARAM, DRIVE_PARAM, WET_PARAM, DRY_PARAM,
		RANGE_PARAM, VOICE_PARAM, PHASE_PARAM, PARAMS_LEN };
	enum InputId { TUNE_INPUT, SHIFT_INPUT, DRIVE_INPUT, FADE_INPUT, IN_INPUT, INPUTS_LEN };
	enum OutputId { OUT_OUTPUT, SIGNAL_OUTPUT, LOCK_OUTPUT, OUTPUTS_LEN };
	enum LightId { ENUMS(METER_LIGHT, 5), LOCK_LIGHT, LIGHTS_LEN };

	copythat::Channel ch[16];
	bool mains60 = false;
	float lastRate = 0.f;
	dsp::ClockDivider lightDiv;

	CopyThat() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TUNE_PARAM, 0.f, 1.f, 0.f, "Tune (drift off the station)", "%", 0.f, 100.f);
		configParam(TATT_PARAM, -1.f, 1.f, 1.f, "Tune CV amount", "%", 0.f, 100.f);
		configParam(SHIFT_PARAM, 0.f, 1.f, 0.25f, "Shift (receiver tracking: accurate to lost)", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.3f, "Drive", " dB", 0.f, 30.f);
		configParam(WET_PARAM, 0.f, 1.f, 1.f, "Wet level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configParam(DRY_PARAM, 0.f, 1.f, 0.f, "Dry level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configSwitch(RANGE_PARAM, 0.f, 1.f, 0.f, "Range", {"1: normal", "2: extended (reaches the neighbouring station)"});
		configSwitch(VOICE_PARAM, 0.f, 1.f, 0.f, "Voice filter on the wet (300 Hz to 3 kHz)", {"Off", "On"});
		configSwitch(PHASE_PARAM, 0.f, 1.f, 0.f, "Wet phase", {"180° (dry cancels the clean part)", "0° (in phase)"});
		configInput(TUNE_INPUT, "Tune CV (±5 V sweeps the dial, scaled by T CV)");
		configInput(SHIFT_INPUT, "Shift CV (0-10 V)");
		configInput(DRIVE_INPUT, "Drive CV (0-10 V)");
		configInput(FADE_INPUT, "Fade: signal strength (0 V = gone, 10 V = full; unpatched = full)");
		configInput(IN_INPUT, "Audio");
		configOutput(OUT_OUTPUT, "Audio");
		configOutput(SIGNAL_OUTPUT, "Signal strength (0-10 V)");
		configOutput(LOCK_OUTPUT, "Lock gate (high while the receiver holds the station)");
		configBypass(IN_INPUT, OUT_OUTPUT);
		lightDiv.setDivision(256);
	}

	void setupRate(float sr) {
		for (int c = 0; c < 16; c++) ch[c].setRate(sr, 0x51A7u + 977u * c + (uint32_t) (random::u32() & 0xFFFF));
		lastRate = sr;
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { setupRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); for (auto& c : ch) c.reset(); }

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != lastRate) setupRate(args.sampleRate);
		int n = std::max(1, inputs[IN_INPUT].getChannels());
		float tune = params[TUNE_PARAM].getValue(), tatt = params[TATT_PARAM].getValue();
		float shift = params[SHIFT_PARAM].getValue(), drive = params[DRIVE_PARAM].getValue();
		float wet = params[WET_PARAM].getValue(), dry = params[DRY_PARAM].getValue();
		bool ext = params[RANGE_PARAM].getValue() > 0.5f;
		bool voice = params[VOICE_PARAM].getValue() > 0.5f;
		float wsign = params[PHASE_PARAM].getValue() > 0.5f ? 1.f : -1.f;
		bool fadeOn = inputs[FADE_INPUT].isConnected();

		for (int c = 0; c < n; c++) {
			copythat::Settings st;
			st.tune = clamp(tune + tatt * inputs[TUNE_INPUT].getPolyVoltage(c) / 5.f, 0.f, 1.f);
			st.shift = clamp(shift + inputs[SHIFT_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
			st.drive = clamp(drive + inputs[DRIVE_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
			if (fadeOn) {
				// 0..10 V -> -40..0 dB of signal strength, so fades sound like a car driving away
				float v = clamp(inputs[FADE_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
				st.fade = std::pow(10.f, (v - 1.f) * 2.f);
			}
			st.extended = ext;
			st.mainsHz = mains60 ? 60.f : 50.f;
			copythat::Frame f = ch[c].process(inputs[IN_INPUT].getVoltage(c), st, voice);
			float y = wet * wsign * f.wet + dry * f.dry;
			float a = std::fabs(y);
			if (a > 8.f) y = std::copysign(8.f + 3.f * std::tanh((a - 8.f) / 3.f), y);  // soft ceiling ~11 V
			outputs[OUT_OUTPUT].setVoltage(y, c);
			outputs[SIGNAL_OUTPUT].setVoltage(10.f * f.signal, c);
			outputs[LOCK_OUTPUT].setVoltage(f.lock ? 10.f : 0.f, c);
		}
		outputs[OUT_OUTPUT].setChannels(n);
		outputs[SIGNAL_OUTPUT].setChannels(n);
		outputs[LOCK_OUTPUT].setChannels(n);

		if (lightDiv.process()) {
			float s = ch[0].sig;
			float dt = args.sampleTime * lightDiv.getDivision();
			for (int i = 0; i < 5; i++)
				lights[METER_LIGHT + i].setBrightnessSmooth(clamp(s * 5.f - i, 0.f, 1.f), dt);
			lights[LOCK_LIGHT].setBrightnessSmooth(ch[0].locked ? 1.f : 0.f, dt);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "mains60", json_boolean(mains60));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* j = json_object_get(r, "mains60");
		if (j) mains60 = json_boolean_value(j);
	}
};

struct CopyThatWidget : ModuleWidget {
	CopyThatWidget(CopyThat* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/CopyThat.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(TUNE_X, TUNE_Y)), module, CopyThat::TUNE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(TATT_X, TUNE_Y)), module, CopyThat::TATT_PARAM));
		for (int i = 0; i < 5; i++)
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(METER_X0 + METER_DX * i, METER_Y)), module, CopyThat::METER_LIGHT + i));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(L_X, ROW2_Y)), module, CopyThat::SHIFT_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(R_X, ROW2_Y)), module, CopyThat::DRIVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(L_X, ROW3_Y)), module, CopyThat::WET_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R_X, ROW3_Y)), module, CopyThat::DRY_PARAM));
		const int sw[3] = {CopyThat::RANGE_PARAM, CopyThat::VOICE_PARAM, CopyThat::PHASE_PARAM};
		for (int i = 0; i < 3; i++)
			addParam(createParamCentered<CKSS>(mm2px(Vec(SW_X[i], SW_Y + 1.3f)), module, sw[i]));
		const int cv[4] = {CopyThat::TUNE_INPUT, CopyThat::SHIFT_INPUT, CopyThat::DRIVE_INPUT, CopyThat::FADE_INPUT};
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CV_X[i], CV_Y)), module, cv[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CV_X[0], IO_Y)), module, CopyThat::IN_INPUT));
		const int outs[3] = {CopyThat::OUT_OUTPUT, CopyThat::SIGNAL_OUTPUT, CopyThat::LOCK_OUTPUT};
		for (int i = 0; i < 3; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_X[i], IO_Y + 0.6f)), module, outs[i]));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(TATT_X, LOCK_Y)), module, CopyThat::LOCK_LIGHT));
		(void) W_MM;
	}

	void appendContextMenu(Menu* menu) override {
		CopyThat* m = getModule<CopyThat>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Mains hum at 60 Hz (Americas)", "", &m->mains60));
	}
};

Model* modelCopyThat = createModel<CopyThat, CopyThatWidget>("CopyThat");
