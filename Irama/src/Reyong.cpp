// Reyong: two kettle-gong players (Java: bonang barung + panerus; Bali: reyong)
// plus kenong and kethuk. Open, damped (byot) and rim (cek) strokes.
// Engine: ReyongEngine in Instruments.hpp. Design: docs/instruments-design.md
#include "plugin.hpp"
#include "Instruments.hpp"

using namespace bronze;

// ---- LAYOUT (same numbers in res/make_instrument_panels.py) ----
static const float R_COLX[4] = {15.f, 41.5f, 68.f, 94.5f};
static const float R_ROW_Y[3] = {33.f, 53.f, 73.f};
static const float R_IN_Y = 95.f, R_IN_X0 = 9.5f, R_IN_DX = 15.46f;
static const float R_BOT_Y = 116.f, R_BOT_X0 = 11.f, R_BOT_DX = 17.95f;

struct Reyong : Module {
	enum ParamIds {
		STYLE_PARAM, STRIKE_PARAM, MALLET_PARAM, RING_PARAM,
		DAMP_PARAM, BYOT_PARAM, CEK_PARAM, SYMPATHY_PARAM,
		DAMPMODE_PARAM, SET_PARAM, WIDTH_PARAM, KRING_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AVOCT_INPUT, AGATE_INPUT, BVOCT_INPUT, BGATE_INPUT, CEK_INPUT, BYOT_INPUT, VEL_INPUT,
		KENONG_INPUT, KVOCT_INPUT, KETHUK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { L_OUTPUT, R_OUTPUT, COLO_OUTPUT, NUM_OUTPUTS };
	enum LightIds { A_LIGHT, B_LIGHT, NUM_LIGHTS };

	ReyongEngine eng;
	dsp::SchmittTrigger aT, bT, cekT, kenT, kethT;
	bool aHigh = false, bHigh = false;
	uint32_t seed = 777u;
	float kethukHz = 220.f;
	float light[2] = {};

	Reyong() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(STYLE_PARAM, 0.f, 1.f, 0.f, "Style", {"Java (bonang: padded sticks, B = panerus an octave up)", "Bali (reyong: cord sticks, bright)"});
		configParam(STRIKE_PARAM, 0.f, 1.f, 0.2f, "Strike (boss to shoulder)");
		configParam(MALLET_PARAM, 0.f, 1.f, 0.5f, "Mallet hardness");
		configParam(RING_PARAM, 0.2f, 5.f, 1.5f, "Ring (open decay)", " s");
		configParam(DAMP_PARAM, 0.f, 1.f, 0.5f, "Damp (how hard the stick chokes)");
		configParam(BYOT_PARAM, 0.f, 1.f, 0.f, "Byot (chance of a damped stroke)", "%", 0.f, 100.f);
		configParam(CEK_PARAM, 0.f, 1.f, 0.5f, "Cek (rim click level)");
		configParam(SYMPATHY_PARAM, 0.f, 1.f, 0.3f, "Sympathy (shared frame)");
		configSwitch(DAMPMODE_PARAM, 0.f, 1.f, 0.f, "Damp on", {"Next note (as played)", "Gate fall"});
		configParam(SET_PARAM, 0.f, 1.f, 0.3f, "Set (tuning errors)");
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.6f, "Width (A left, B right)");
		configParam(KRING_PARAM, 0.5f, 6.f, 3.f, "Kenong ring", " s");
		configInput(AVOCT_INPUT, "A V/oct (e.g. Irama BONANG)");
		configInput(AGATE_INPUT, "A gate");
		configInput(BVOCT_INPUT, "B V/oct (Java: unpatched = A an octave up)");
		configInput(BGATE_INPUT, "B gate (Java: unpatched = A's gate)");
		configInput(CEK_INPUT, "Cek trigger (rim click)");
		configInput(BYOT_INPUT, "Byot gate (every stroke damped while high)");
		configInput(VEL_INPUT, "Velocity 0-10 V");
		configInput(KENONG_INPUT, "Kenong trigger");
		configInput(KVOCT_INPUT, "Kenong V/oct (unpatched: A an octave down)");
		configInput(KETHUK_INPUT, "Kethuk trigger");
		configOutput(L_OUTPUT, "Left (mono mix if R unpatched)");
		configOutput(R_OUTPUT, "Right");
		configOutput(COLO_OUTPUT, "Kenong + kethuk alone");
		seed = random::u32();
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setSampleRate(e.sampleRate); }
	void onAdd(const AddEvent& e) override { eng.setSampleRate(APP->engine->getSampleRate()); }

	ReyongParams P() {
		ReyongParams q;
		q.bali = params[STYLE_PARAM].getValue() > 0.5f;
		q.strike = params[STRIKE_PARAM].getValue();
		q.mallet = params[MALLET_PARAM].getValue();
		q.ring = params[RING_PARAM].getValue();
		q.damp = params[DAMP_PARAM].getValue();
		q.byot = params[BYOT_PARAM].getValue();
		q.cek = params[CEK_PARAM].getValue();
		q.sympathy = params[SYMPATHY_PARAM].getValue();
		q.dampOnGate = params[DAMPMODE_PARAM].getValue() > 0.5f;
		q.set = params[SET_PARAM].getValue();
		q.width = params[WIDTH_PARAM].getValue();
		q.kenongRing = params[KRING_PARAM].getValue();
		q.kethukHz = kethukHz;
		q.seed = seed;
		return q;
	}

	static float hzOf(float v) { return dsp::FREQ_C4 * std::pow(2.f, clamp(v, -5.f, 5.f)); }

	void process(const ProcessArgs& args) override {
		float vel = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 0.8f;
		bool byot = inputs[BYOT_INPUT].getVoltage() >= 1.f;
		float ga = inputs[AGATE_INPUT].getVoltage();
		bool aRise = aT.process(ga, 0.1f, 1.f);
		bool aFall = aHigh && ga < 0.1f;
		if (aRise) aHigh = true;
		if (aFall) aHigh = false;
		bool bPatched = inputs[BGATE_INPUT].isConnected();
		float gb = inputs[BGATE_INPUT].getVoltage();
		bool bRise = bPatched && bT.process(gb, 0.1f, 1.f);
		bool bFall = bPatched && bHigh && gb < 0.1f;
		if (bRise) bHigh = true;
		if (bFall) bHigh = false;
		bool cek = cekT.process(inputs[CEK_INPUT].getVoltage(), 0.1f, 1.f);
		bool ken = kenT.process(inputs[KENONG_INPUT].getVoltage(), 0.1f, 1.f);
		bool keth = kethT.process(inputs[KETHUK_INPUT].getVoltage(), 0.1f, 1.f);

		if (aRise || aFall || bRise || bFall || cek || ken || keth) {
			ReyongParams q = P();
			float aHz = hzOf(inputs[AVOCT_INPUT].getVoltage());
			if (aRise) {
				eng.strike(0, aHz, vel, q, byot); light[0] = 1.f;
				if (!bPatched && !q.bali) {   // Java: one cable plays barung and panerus
					float bHz = inputs[BVOCT_INPUT].isConnected() ? hzOf(inputs[BVOCT_INPUT].getVoltage()) : aHz * 2.f;
					eng.strike(1, bHz, vel * 0.8f, q, byot); light[1] = 1.f;
				}
			}
			if (aFall) { eng.release(0, q); if (!bPatched && !q.bali) eng.release(1, q); }
			if (bRise) {
				float bHz = inputs[BVOCT_INPUT].isConnected() ? hzOf(inputs[BVOCT_INPUT].getVoltage()) : aHz * (q.bali ? 1.f : 2.f);
				eng.strike(1, bHz, vel, q, byot); light[1] = 1.f;
			}
			if (bFall) eng.release(1, q);
			if (cek) eng.cekHit(vel, q);
			if (ken) {
				float kHz = inputs[KVOCT_INPUT].isConnected() ? hzOf(inputs[KVOCT_INPUT].getVoltage()) : eng.lastHz * 0.5f;
				eng.kenongHit(kHz, vel, q);
			}
			if (keth) eng.kethukHit(vel, q);
		}
		float L, R, C;
		eng.process(L, R, C);
		bool stereo = outputs[R_OUTPUT].isConnected();
		outputs[L_OUTPUT].setVoltage(stereo ? L : 0.5f * (L + R));
		outputs[R_OUTPUT].setVoltage(R);
		outputs[COLO_OUTPUT].setVoltage(C);
		for (int i = 0; i < 2; i++) {
			lights[A_LIGHT + i].setBrightness(light[i]);
			light[i] = std::max(0.f, light[i] - args.sampleTime * 8.f);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "seed", json_integer(seed));
		json_object_set_new(r, "kethukHz", json_real(kethukHz));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* s = json_object_get(r, "seed");
		if (s) seed = (uint32_t)json_integer_value(s);
		json_t* k = json_object_get(r, "kethukHz");
		if (k) kethukHz = (float)json_number_value(k);
	}
};

struct ReyongWidget : ModuleWidget {
	ReyongWidget(Reyong* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Reyong.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		for (int i = 0; i < 12; i++) {
			Vec p = mm2px(Vec(R_COLX[i % 4], R_ROW_Y[i / 4]));
			if (i == Reyong::STYLE_PARAM || i == Reyong::DAMPMODE_PARAM) addParam(createParamCentered<CKSS>(p, module, i));
			else addParam(createParamCentered<RoundBlackKnob>(p, module, i));
		}
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(R_COLX[0] + 6.f, R_ROW_Y[2] - 6.f)), module, Reyong::A_LIGHT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(R_COLX[0] + 6.f, R_ROW_Y[2] + 6.f)), module, Reyong::B_LIGHT));
		for (int i = 0; i < 7; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(R_IN_X0 + R_IN_DX * i, R_IN_Y)), module, Reyong::AVOCT_INPUT + i));
		for (int i = 0; i < 3; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(R_BOT_X0 + R_BOT_DX * i, R_BOT_Y)), module, Reyong::KENONG_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(R_BOT_X0 + R_BOT_DX * (3 + i), R_BOT_Y)), module, Reyong::L_OUTPUT + i));
		}
	}

	void appendContextMenu(Menu* menu) override {
		Reyong* m = dynamic_cast<Reyong*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("New set (re-roll tuning errors)", "", [=]() { m->seed = random::u32(); }));
		menu->addChild(createMenuLabel("Kethuk pitch"));
		const float hz[4] = {196.f, 220.f, 247.f, 294.f};
		const char* nm[4] = {"G3", "A3", "B3", "D4"};
		for (int i = 0; i < 4; i++) {
			float h = hz[i];
			menu->addChild(createCheckMenuItem(nm[i], "", [=]() { return std::fabs(m->kethukHz - h) < 0.5f; }, [=]() { m->kethukHz = h; }));
		}
	}
};

Model* modelReyong = createModel<Reyong, ReyongWidget>("Reyong");
