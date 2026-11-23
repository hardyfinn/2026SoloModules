// Bell Foundry: twelve cast bells to ring from Peal (or anything with gates).
// Patch Peal's GATES and TUNES poly outputs to GATES and V/OCT here.
// The sound engine is in FoundryDSP.hpp.
#include "plugin.hpp"
#include "FoundryDSP.hpp"

using namespace foundry;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float F_DISP_X = 6.0f, F_DISP_Y = 14.5f, F_DISP_W = 109.92f, F_DISP_H = 17.5f;
static const float F_SW_Y = 44.0f;
static const float F_SW_X[2] = {17.5f, 55.0f};
static const float F_RECAST_X = 104.5f;
static const float F_KX[4] = {17.5f, 46.5f, 75.5f, 104.5f};
static const float F_K1_Y = 61.5f, F_K2_Y = 82.0f;
static const float F_J_X0 = 12.0f, F_J_DX = 19.584f;
static const float F_CV_Y = 101.0f, F_IO_Y = 117.0f;

struct BellFoundry : Module {
	enum ParamIds {
		PROFILE_PARAM, METAL_PARAM, THICK_PARAM, SIZE_PARAM,
		CRACK_PARAM, CLAPPER_PARAM, DECAY_PARAM, SPREAD_PARAM,
		TUNING_PARAM, TOWER_PARAM, RECAST_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PROFILE_INPUT, METAL_INPUT, THICK_INPUT, CRACK_INPUT, CLAPPER_INPUT, DECAY_INPUT,
		GATES_INPUT, VOCT_INPUT, VEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { LEFT_OUTPUT, RIGHT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { RECAST_LIGHT, NUM_LIGHTS };

	Bell bells[NB];
	Reverb verb;
	Rng castRng;
	uint32_t castSeed = 1234567u;
	dsp::SchmittTrigger gateTrig[NB];
	dsp::BooleanTrigger recastBtn;
	dsp::PulseGenerator recastFlash;
	float lpL = 0.f, lpR = 0.f;
	float sr = 48000.f;
	// screen
	Settings dispSt;
	float dispEnv[NB] = {};

	BellFoundry() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PROFILE_PARAM, 0.f, 2.f, 0.f, "Profile (church, handbell, gamelan)");
		configParam(METAL_PARAM, 0.f, 1.f, 0.5f, "Metal (brass, bell bronze, steel)");
		configParam(THICK_PARAM, 0.f, 1.f, 0.5f, "Thickness");
		configParam(SIZE_PARAM, -2.f, 2.f, 0.f, "Size", " oct");
		configParam(CRACK_PARAM, 0.f, 1.f, 0.f, "Crack", "%", 0.f, 100.f);
		configParam(CLAPPER_PARAM, 0.f, 1.f, 0.55f, "Clapper (leather to iron)");
		configParam(DECAY_PARAM, 0.15f, 3.f, 1.f, "Ring time", "x");
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.7f, "Stereo spread", "%", 0.f, 100.f);
		configSwitch(TUNING_PARAM, 0.f, 2.f, 2.f, "Tuning", {"Ombak (gamelan beating)", "Old bells", "True-harmonic"});
		configSwitch(TOWER_PARAM, 0.f, 2.f, 1.f, "Listen from", {"Across town", "Churchyard", "Ringing chamber"});
		configButton(RECAST_PARAM, "Recast (new set of bells)");
		configInput(PROFILE_INPUT, "Profile CV (1 V per profile)");
		configInput(METAL_INPUT, "Metal CV (10 V = full)");
		configInput(THICK_INPUT, "Thickness CV (10 V = full)");
		configInput(CRACK_INPUT, "Crack CV (10 V = full)");
		configInput(CLAPPER_INPUT, "Clapper CV (10 V = full)");
		configInput(DECAY_INPUT, "Ring time CV (1 V adds 0.3x)");
		configInput(GATES_INPUT, "Gates (poly, channel 1 = treble)");
		configInput(VOCT_INPUT, "V/OCT (poly, one per bell)");
		configInput(VEL_INPUT, "Velocity (poly, 10 V = full)");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		recast();
		onSampleRateChange();
	}

	void recast() {
		castRng.s = castSeed | 1u;
		for (int i = 0; i < NB; i++) bells[i].recast(castRng);
	}

	void onSampleRateChange() override {
		sr = APP->engine->getSampleRate();
		verb.init(sr);
	}

	void onReset() override {
		castSeed = 1234567u;
		recast();
		for (int i = 0; i < NB; i++) bells[i].silence();
	}

	float knob(int param, int input, float perVolt, float lo, float hi) {
		float v = params[param].getValue();
		if (inputs[input].isConnected()) v += inputs[input].getVoltage() * perVolt;
		return clamp(v, lo, hi);
	}

	void process(const ProcessArgs& args) override {
		Settings st;
		st.profile = knob(PROFILE_PARAM, PROFILE_INPUT, 1.f, 0.f, 2.f);
		st.metal = knob(METAL_PARAM, METAL_INPUT, 0.1f, 0.f, 1.f);
		st.thickness = knob(THICK_PARAM, THICK_INPUT, 0.1f, 0.f, 1.f);
		st.size = params[SIZE_PARAM].getValue();
		st.crack = knob(CRACK_PARAM, CRACK_INPUT, 0.1f, 0.f, 1.f);
		st.clapper = knob(CLAPPER_PARAM, CLAPPER_INPUT, 0.1f, 0.f, 1.f);
		st.decay = knob(DECAY_PARAM, DECAY_INPUT, 0.3f, 0.15f, 4.f);
		int sw = (int) std::round(params[TUNING_PARAM].getValue());
		st.tuning = sw == 2 ? TRUE_HARMONIC : (sw == 1 ? OLD_BELLS : OMBAK);
		int tower = 2 - (int) std::round(params[TOWER_PARAM].getValue());   // 0 chamber, 1 churchyard, 2 town
		float spread = params[SPREAD_PARAM].getValue();

		if (recastBtn.process(params[RECAST_PARAM].getValue() > 0.f)) {
			castSeed = (uint32_t) (random::u32() | 1u);
			recast();
			recastFlash.trigger(0.2f);
		}

		int n = std::min(inputs[GATES_INPUT].getChannels(), NB);
		int pc = inputs[VOCT_INPUT].getChannels();
		int vc = inputs[VEL_INPUT].getChannels();
		for (int i = 0; i < n; i++) {
			if (gateTrig[i].process(inputs[GATES_INPUT].getVoltage(i), 0.1f, 1.f)) {
				float v;
				if (pc > 0) v = inputs[VOCT_INPUT].getVoltage(std::min(i, pc - 1)) + (pc == 1 ? 0.f : 0.f);
				else {
					// no pitch cable: a major scale down from the treble, tenor at C3
					static const int maj[7] = {0, 2, 4, 5, 7, 9, 11};
					int d = n - 1 - i;
					v = (maj[d % 7] + 12 * (d / 7)) / 12.f - 1.f;
				}
				float vel = vc > 0 ? clamp(inputs[VEL_INPUT].getVoltage(std::min(i, vc - 1)) * 0.1f, 0.f, 1.f) : 0.8f;
				bells[i].strike(dsp::FREQ_C4 * std::pow(2.f, v), vel, st, sr);
			}
		}

		float L = 0.f, R = 0.f;
		int nPan = std::max(n, 2);
		for (int i = 0; i < NB; i++) {
			float s = bells[i].process();
			// bells hang in a ring: treble to tenor goes round the circle
			float pan = 0.5f + 0.5f * spread * std::sin(TWO_PI * (i + 0.5f) / nPan);
			L += s * std::sqrt(1.f - pan);
			R += s * std::sqrt(pan);
			dispEnv[i] = bells[i].env;
		}
		// tower: louvres and distance darken it, and more of it is room sound
		static const float LP_HZ[3] = {16000.f, 6000.f, 2200.f};
		static const float WET[3] = {0.12f, 0.3f, 0.55f};
		static const float DRY[3] = {1.f, 0.8f, 0.45f};
		static const float FB[3] = {0.72f, 0.8f, 0.86f};
		static const float PRE[3] = {0.004f, 0.02f, 0.09f};
		float a = 1.f - std::exp(-TWO_PI * LP_HZ[tower] / sr);
		lpL += a * (L - lpL);
		lpR += a * (R - lpR);
		float wl, wr;
		verb.process(lpL, lpR, wl, wr, (int) (PRE[tower] * sr), FB[tower], 0.35f);
		float outL = (lpL * DRY[tower] + wl * WET[tower]) * 4.0f;
		float outR = (lpR * DRY[tower] + wr * WET[tower]) * 4.0f;
		// gentle limit to +-10 V
		outputs[LEFT_OUTPUT].setVoltage(10.f * std::tanh(outL * 0.5f));
		outputs[RIGHT_OUTPUT].setVoltage(10.f * std::tanh(outR * 0.5f));
		lights[RECAST_LIGHT].setBrightnessSmooth(recastFlash.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
		dispSt = st;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "castSeed", json_integer(castSeed));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "castSeed");
		if (s) { castSeed = (uint32_t) json_integer_value(s); recast(); }
	}
};

// The screen: the partials of the current casting (ratio to the strike note
// across, level up) and twelve bars that light as each bell rings.
struct FoundryDisplay : Widget {
	BellFoundry* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		NVGcontext* vg = args.vg;
		const float w = box.size.x, h = box.size.y;
		const NVGcolor bronze = nvgRGB(232, 178, 98), cream = nvgRGB(240, 230, 210);
		Settings st;
		float env[NB] = {};
		if (module) {
			st = module->dispSt;
			for (int i = 0; i < NB; i++) env[i] = module->dispEnv[i];
		}
		else for (int i = 0; i < NB; i++) env[i] = (i % 3 == 0) ? 0.3f : 0.05f;
		const float pad = mm2px(1.5f);
		const float sw = w * 0.62f;
		// partial lines
		int a = (int) std::floor(std::min(st.profile, 1.999f));
		float f = st.profile - a;
		auto xOf = [&](float r) { return pad + (sw - 2 * pad) * std::log2(r / 0.4f) / std::log2(10.f / 0.4f); };
		float steel = std::max(0.f, (st.metal - 0.5f) * 2.f);
		nvgStrokeColor(vg, nvgRGBA(232, 178, 98, 40));
		nvgStrokeWidth(vg, 0.6f);
		for (float r : {0.5f, 1.f, 2.f, 4.f, 8.f}) {
			nvgBeginPath(vg); nvgMoveTo(vg, xOf(r), pad); nvgLineTo(vg, xOf(r), h - pad); nvgStroke(vg);
		}
		for (int k = 0; k < NP; k++) {
			float ratio = RATIO[a][k] + (RATIO[a + 1][k] - RATIO[a][k]) * f;
			float level = LEVEL[a][k] + (LEVEL[a + 1][k] - LEVEL[a][k]) * f;
			if (ratio > 1.f) ratio = std::pow(ratio, 1.f + 0.08f * (st.thickness - 0.5f) + 0.035f * steel);
			level *= std::pow(std::max(ratio, 0.25f), (st.clapper - 0.6f) * 1.4f);
			float x = xOf(ratio), y0 = h - pad, y1 = y0 - (h - 2 * pad) * clamp(level, 0.f, 1.2f) / 1.2f;
			nvgBeginPath(vg); nvgMoveTo(vg, x, y0); nvgLineTo(vg, x, y1);
			nvgStrokeColor(vg, k == 1 ? cream : bronze);
			nvgStrokeWidth(vg, 2.0f);
			nvgStroke(vg);
			if (st.tuning != TRUE_HARMONIC || st.crack > 0.05f) {
				nvgBeginPath(vg); nvgMoveTo(vg, x + 2.5f, y0); nvgLineTo(vg, x + 2.5f, y0 + (y1 - y0) * 0.6f);
				nvgStrokeColor(vg, nvgRGBA(255, 90, 80, 160)); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
			}
		}
		// ringing bars
		float bx0 = sw + pad, bw = (w - bx0 - pad) / NB;
		for (int i = 0; i < NB; i++) {
			float v = clamp(std::sqrt(env[i]) * 1.4f, 0.f, 1.f);
			float x = bx0 + bw * i + bw * 0.15f;
			nvgBeginPath(vg);
			nvgRect(vg, x, h - pad - (h - 2 * pad) * v, bw * 0.7f, (h - 2 * pad) * v);
			nvgFillColor(vg, i == 0 ? nvgRGB(255, 90, 80) : bronze);
			nvgFill(vg);
		}
	}
};

struct BellFoundryWidget : ModuleWidget {
	BellFoundryWidget(BellFoundry* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BellFoundry.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FoundryDisplay* d = createWidget<FoundryDisplay>(mm2px(Vec(F_DISP_X, F_DISP_Y)));
		d->box.size = mm2px(Vec(F_DISP_W, F_DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<CKSSThree>(mm2px(Vec(F_SW_X[0], F_SW_Y)), module, BellFoundry::TUNING_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(F_SW_X[1], F_SW_Y)), module, BellFoundry::TOWER_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(F_RECAST_X, F_SW_Y)), module,
		                                                          BellFoundry::RECAST_PARAM, BellFoundry::RECAST_LIGHT));
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(F_KX[i], F_K1_Y)), module, BellFoundry::PROFILE_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(F_KX[i], F_K2_Y)), module, BellFoundry::CRACK_PARAM + i));
		}
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(F_J_X0 + F_J_DX * i, F_CV_Y)), module, BellFoundry::PROFILE_INPUT + i));
		for (int i = 0; i < 3; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(F_J_X0 + F_J_DX * i, F_IO_Y)), module, BellFoundry::GATES_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(F_J_X0 + F_J_DX * 4, F_IO_Y)), module, BellFoundry::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(F_J_X0 + F_J_DX * 5, F_IO_Y)), module, BellFoundry::RIGHT_OUTPUT));
	}
};

Model* modelBellFoundry = createModel<BellFoundry, BellFoundryWidget>("BellFoundry");
