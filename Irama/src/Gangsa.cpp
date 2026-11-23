// Gangsa: two players (polos + sangsih), every note an umbang + isep pair,
// kantilan doubling an octave up, hand damping.
// Engine: GangsaEngine in Instruments.hpp. Design: docs/instruments-design.md
#include "plugin.hpp"
#include "Instruments.hpp"

using namespace bronze;

// ---- LAYOUT (same numbers in res/make_instrument_panels.py) ----
static const float G2_COLX[3] = {19.f, 50.8f, 82.6f};
static const float G2_ROW_Y[3] = {33.f, 56.f, 76.f};
static const float G2_ROW4_Y = 89.5f;
static const float G2_IN_Y = 102.f, G2_IN_X0 = 11.f, G2_IN_DX = 19.9f;
static const float G2_BOT_Y = 117.f, G2_BOT_X0 = 10.f, G2_BOT_DX = 16.32f;

struct Gangsa : Module {
	enum ParamIds {
		OMBAK_PARAM, KANTILAN_PARAM, MALLET_PARAM, DAMP_PARAM, RING_PARAM, BRIGHT_PARAM,
		TUBE_PARAM, SET_PARAM, WIDTH_PARAM, DAMPMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds { PVOCT_INPUT, PGATE_INPUT, SVOCT_INPUT, SGATE_INPUT, OMBAK_INPUT, VEL_INPUT, DAMP_INPUT, NUM_INPUTS };
	enum OutputIds { L_OUTPUT, R_OUTPUT, POLOS_OUTPUT, SANGSIH_OUTPUT, NUM_OUTPUTS };
	enum LightIds { POLOS_LIGHT, SANGSIH_LIGHT, NUM_LIGHTS };

	GangsaEngine eng;
	dsp::SchmittTrigger gateT[2][16];
	bool gateHigh[2][16] = {};
	uint32_t seed = 12345u;
	bool chord = false;
	float light[2] = {};

	Gangsa() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(OMBAK_PARAM, 0.f, 12.f, 6.5f, "Ombak (umbang / isep beating)", " Hz");
		configParam(KANTILAN_PARAM, 0.f, 1.f, 0.4f, "Kantilan (octave-up pair) level");
		configParam(MALLET_PARAM, 0.f, 1.f, 0.7f, "Mallet hardness");
		configParam(DAMP_PARAM, 0.f, 1.f, 0.75f, "Damp (how hard the hand chokes)");
		configParam(RING_PARAM, 0.3f, 6.f, 1.8f, "Ring (undamped decay)", " s");
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.6f, "Bright (upper partials)");
		configParam(TUBE_PARAM, 0.f, 1.f, 0.5f, "Tube (bamboo resonator)");
		configParam(SET_PARAM, 0.f, 1.f, 0.3f, "Set (tuning errors up to 12 cents)");
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.6f, "Width (polos left, sangsih right)");
		configSwitch(DAMPMODE_PARAM, 0.f, 1.f, 0.f, "Damp on", {"Next note (as played)", "Gate fall"});
		configInput(PVOCT_INPUT, "Polos V/oct (2 channels = umbang + isep from Irama)");
		configInput(PGATE_INPUT, "Polos gate");
		configInput(SVOCT_INPUT, "Sangsih V/oct (2 channels = umbang + isep)");
		configInput(SGATE_INPUT, "Sangsih gate");
		configInput(OMBAK_INPUT, "Ombak CV (1 Hz per volt)");
		configInput(VEL_INPUT, "Velocity 0-10 V (2 channels: polos, sangsih)");
		configInput(DAMP_INPUT, "Damp CV (0.1 per volt)");
		configOutput(L_OUTPUT, "Left (mono mix if R unpatched)");
		configOutput(R_OUTPUT, "Right");
		configOutput(POLOS_OUTPUT, "Polos alone");
		configOutput(SANGSIH_OUTPUT, "Sangsih alone");
		seed = random::u32();
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setSampleRate(e.sampleRate); }
	void onAdd(const AddEvent& e) override { eng.setSampleRate(APP->engine->getSampleRate()); }

	GangsaParams P() {
		GangsaParams q;
		q.ombak = clamp(params[OMBAK_PARAM].getValue() + inputs[OMBAK_INPUT].getVoltage(), 0.f, 20.f);
		q.kantilan = params[KANTILAN_PARAM].getValue();
		q.mallet = params[MALLET_PARAM].getValue();
		q.damp = clamp(params[DAMP_PARAM].getValue() + inputs[DAMP_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		q.ring = params[RING_PARAM].getValue();
		q.bright = params[BRIGHT_PARAM].getValue();
		q.tube = params[TUBE_PARAM].getValue();
		q.set = params[SET_PARAM].getValue();
		q.width = params[WIDTH_PARAM].getValue();
		q.dampOnGate = params[DAMPMODE_PARAM].getValue() > 0.5f;
		q.seed = seed;
		return q;
	}

	void part(int p, int voctIn, int gateIn, GangsaParams& q, bool& haveQ) {
		Input& gi = inputs[gateIn];
		Input& vi = inputs[voctIn];
		if (!gi.isConnected()) return;
		int chans = chord ? std::max(gi.getChannels(), 1) : 1;
		float vel = 0.8f;
		if (inputs[VEL_INPUT].isConnected()) {
			int vc = inputs[VEL_INPUT].getChannels();
			vel = clamp(inputs[VEL_INPUT].getVoltage(vc >= 2 ? p : 0) / 10.f, 0.f, 1.f);
		}
		for (int c = 0; c < chans; c++) {
			float g = gi.getVoltage(c);
			if (gateT[p][c].process(g, 0.1f, 1.f)) {
				if (!haveQ) { q = P(); haveQ = true; }
				float v = vi.getVoltage(chord ? c : 0);
				float hz = dsp::FREQ_C4 * std::pow(2.f, clamp(v, -5.f, 5.f));
				float isep = -1.f;
				if (!chord && vi.getChannels() >= 2) isep = dsp::FREQ_C4 * std::pow(2.f, clamp(vi.getVoltage(1), -5.f, 5.f));
				eng.strike(p, hz, isep, vel, q);
				light[p] = 1.f;
				gateHigh[p][c] = true;
			}
			else if (gateHigh[p][c] && g < 0.1f) {
				gateHigh[p][c] = false;
				if (!haveQ) { q = P(); haveQ = true; }
				eng.release(p, q);
			}
		}
	}

	void process(const ProcessArgs& args) override {
		GangsaParams q;
		bool haveQ = false;
		part(0, PVOCT_INPUT, PGATE_INPUT, q, haveQ);
		part(1, SVOCT_INPUT, SGATE_INPUT, q, haveQ);
		float L, R, Po, Sa;
		eng.process(L, R, Po, Sa);
		bool stereo = outputs[R_OUTPUT].isConnected();
		outputs[L_OUTPUT].setVoltage(stereo ? L : 0.5f * (L + R));
		outputs[R_OUTPUT].setVoltage(R);
		outputs[POLOS_OUTPUT].setVoltage(Po);
		outputs[SANGSIH_OUTPUT].setVoltage(Sa);
		for (int i = 0; i < 2; i++) {
			lights[POLOS_LIGHT + i].setBrightness(light[i]);
			light[i] = std::max(0.f, light[i] - args.sampleTime * 8.f);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "seed", json_integer(seed));
		json_object_set_new(r, "chord", json_boolean(chord));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* s = json_object_get(r, "seed");
		if (s) seed = (uint32_t)json_integer_value(s);
		json_t* c = json_object_get(r, "chord");
		if (c) chord = json_boolean_value(c);
	}
};

struct GangsaWidget : ModuleWidget {
	GangsaWidget(Gangsa* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gangsa.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		for (int i = 0; i < 9; i++) {
			Vec p = mm2px(Vec(G2_COLX[i % 3], G2_ROW_Y[i / 3]));
			if (i == 0 || i == 3) addParam(createParamCentered<RoundLargeBlackKnob>(p, module, Gangsa::OMBAK_PARAM + i));
			else addParam(createParamCentered<RoundBlackKnob>(p, module, Gangsa::OMBAK_PARAM + i));
		}
		addParam(createParamCentered<CKSS>(mm2px(Vec(G2_COLX[0], G2_ROW4_Y)), module, Gangsa::DAMPMODE_PARAM));
		addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(G2_COLX[1], G2_ROW4_Y)), module, Gangsa::POLOS_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(G2_COLX[2], G2_ROW4_Y)), module, Gangsa::SANGSIH_LIGHT));
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G2_IN_X0 + G2_IN_DX * i, G2_IN_Y)), module, Gangsa::PVOCT_INPUT + i));
		for (int i = 0; i < 2; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G2_BOT_X0 + G2_BOT_DX * i, G2_BOT_Y)), module, Gangsa::VEL_INPUT + i));
		for (int i = 0; i < 4; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(G2_BOT_X0 + G2_BOT_DX * (2 + i), G2_BOT_Y)), module, Gangsa::L_OUTPUT + i));
	}

	void appendContextMenu(Menu* menu) override {
		Gangsa* m = dynamic_cast<Gangsa*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("New set (re-roll tuning errors)", "", [=]() { m->seed = random::u32(); }));
		menu->addChild(createBoolPtrMenuItem("Poly = chord (each channel its own note)", "", &m->chord));
	}
};

Model* modelGangsa = createModel<Gangsa, GangsaWidget>("Gangsa");
