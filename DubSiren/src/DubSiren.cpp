// Dub Siren: the classic UK sound-system transistor siren (NJD SE-1 circuit,
// as in the Rigsmith NS1), modelled part by part, plus an echo.
// See DubSirenDSP.hpp for what each control is in the circuit.
#include "plugin.hpp"
#include "DubSirenDSP.hpp"
#include "Layout.hpp"

using namespace dubsiren;
using namespace layout;

static float speedMult(float k) { return std::pow(4.f, 2.f * k - 1.f); }          // 0.25x .. 4x
static float releaseSec(float k) { return 0.02f * std::pow(150.f, k); }          // 20 ms .. 3 s
static float timeSec(float k) { return 0.04f * std::pow(25.f, k); }              // 40 ms .. 1 s
// Hardware mode: what the box has fixed inside (C10 hold-up, echo chip time)
static const float HW_RELEASE = 0.35f, HW_TIME = 0.33f;

struct SpeedQuantity : ParamQuantity {
	std::string getDisplayValueString() override { return string::f("%.2fx", speedMult(getValue())); }
};
struct ReleaseQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float s = releaseSec(getValue());
		return s < 1.f ? string::f("%d ms", (int) std::round(s * 1000.f)) : string::f("%.2f s", s);
	}
};
struct TimeQuantity : ParamQuantity {
	std::string getDisplayValueString() override { return string::f("%d ms", (int) std::round(timeSec(getValue()) * 1000.f)); }
};

struct DubSirenModule : Module {
	enum ParamIds { SPEED_PARAM, PITCH_PARAM, FEEDBACK_PARAM, MODE_PARAM, BEAT_PARAM, VOLUME_PARAM,
		TRIG_PARAM, SIREN_PARAM, TONE_PARAM, DEPTH_PARAM, RELEASE_PARAM, TIME_PARAM, ERA_PARAM, EXTRAS_PARAM, NUM_PARAMS };
	enum InputIds { VOCT_INPUT, SPEED_INPUT, FDBK_INPUT, CLOCK_INPUT, TRIG_INPUT, SIREN_INPUT, TONE_INPUT, IN_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ACTIVE_LIGHT, SIREN_LIGHT, TONE_LIGHT, NUM_LIGHTS };

	dubsiren::Engine eng;
	dubsiren::Settings st;
	dsp::SchmittTrigger clockTrig;

	DubSirenModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<SpeedQuantity>(SPEED_PARAM, 0.f, 1.f, 0.5f, "Speed (scales the BEAT rate)");
		configSwitch(PITCH_PARAM, 0.f, 3.f, 1.f, "Pitch", {"1 (470k, low)", "2 (220k)", "3 (100k)", "4 (68k, high)"});
		configParam(FEEDBACK_PARAM, 0.f, 1.15f, 0.f, "Echo feedback (left = no echo)", "%", 0.f, 100.f / 1.15f);
		configSwitch(MODE_PARAM, 0.f, 3.f, 3.f, "Mode", {"1: steady | shaped", "2: steady | square (stutter)", "3: square | square (two-tone)", "4: shaped | shaped (wail)"});
		configSwitch(BEAT_PARAM, 0.f, 3.f, 0.f, "Beat (sweep speed)", {"1 (slow)", "2", "3 (fast)", "Off"});
		configParam(VOLUME_PARAM, 0.f, 1.f, 0.8f, "Siren volume", "%", 0.f, 100.f);
		paramQuantities[VOLUME_PARAM]->randomizeEnabled = false;
		configSwitch(TRIG_PARAM, 0.f, 1.f, 0.f, "Trig (power)", {"Off", "On"});
		configButton(SIREN_PARAM, "Siren (charge the sweep, hold it high)");
		configButton(TONE_PARAM, "Tone (slow rise)");
		configParam(DEPTH_PARAM, 0.f, 1.5f, 1.f, "Depth (1 = the original circuit)", "%", 0.f, 100.f);
		configParam<ReleaseQuantity>(RELEASE_PARAM, 0.f, 1.f, 0.56f, "Release (how long the supply holds up)");
		configParam<TimeQuantity>(TIME_PARAM, 0.f, 1.f, 0.65f, "Echo time");
		configSwitch(ERA_PARAM, 0.f, 1.f, 1.f, "Echo era", {"90s chip echo", "80s tape echo"});
		configSwitch(EXTRAS_PARAM, 0.f, 1.f, 0.f, "Extras", {"Off: works exactly like the hardware box", "On: DEPTH, RELEASE, TIME, ERA and the CV/gate jacks"});
		configInput(VOCT_INPUT, "Pitch V/OCT");
		configInput(SPEED_INPUT, "Speed CV (1 V/oct)");
		configInput(FDBK_INPUT, "Feedback CV (+10 V = full)");
		configInput(CLOCK_INPUT, "Clock (restarts the sweep)");
		configInput(TRIG_INPUT, "Trig gate (power while high)");
		configInput(SIREN_INPUT, "Siren gate");
		configInput(TONE_INPUT, "Tone gate");
		configInput(IN_INPUT, "Audio into the echo");
		configOutput(OUT_OUTPUT, "Siren");
		configBypass(IN_INPUT, OUT_OUTPUT);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setRate(e.sampleRate); }
	void onAdd(const AddEvent& e) override { eng.setRate(APP->engine->getSampleRate()); }

	void process(const ProcessArgs& args) override {
		st.pitch = (int) std::round(params[PITCH_PARAM].getValue());
		st.mode = (int) std::round(params[MODE_PARAM].getValue());
		st.beat = (int) std::round(params[BEAT_PARAM].getValue());
		// EXTRAS off = the hardware box: only its own controls, fixed values for the rest
		bool ex = params[EXTRAS_PARAM].getValue() > 0.5f;
		auto in = [&](int i) { return ex ? inputs[i].getVoltage() : 0.f; };
		st.voct = in(VOCT_INPUT);
		st.speed = speedMult(params[SPEED_PARAM].getValue()) * std::exp2(clamp(in(SPEED_INPUT), -5.f, 5.f));
		st.depth = ex ? params[DEPTH_PARAM].getValue() : 1.f;
		st.release = ex ? releaseSec(params[RELEASE_PARAM].getValue()) : HW_RELEASE;
		st.feedback = clamp(params[FEEDBACK_PARAM].getValue() + in(FDBK_INPUT) * 0.115f, 0.f, 1.15f);
		st.time = ex ? timeSec(params[TIME_PARAM].getValue()) : HW_TIME;
		st.era80 = ex ? params[ERA_PARAM].getValue() > 0.5f : false;
		st.power = params[TRIG_PARAM].getValue() > 0.5f || in(TRIG_INPUT) >= 1.f;
		st.siren = params[SIREN_PARAM].getValue() > 0.5f || in(SIREN_INPUT) >= 1.f;
		st.tone = params[TONE_PARAM].getValue() > 0.5f || in(TONE_INPUT) >= 1.f;
		st.sync = clockTrig.process(in(CLOCK_INPUT), 0.1f, 1.f);
		float y = eng.process(st, in(IN_INPUT), args.sampleTime);
		outputs[OUT_OUTPUT].setVoltage(y * params[VOLUME_PARAM].getValue());
		lights[ACTIVE_LIGHT].setBrightnessSmooth(eng.led * clamp(eng.vcc / 9.f, 0.f, 1.f), args.sampleTime);
		lights[SIREN_LIGHT].setBrightness(st.siren ? 1.f : 0.f);
		lights[TONE_LIGHT].setBrightness(st.tone ? 1.f : 0.f);
	}
};

// Knobs with gold and white caps on the flag panel
struct CapKnob : app::SvgKnob {
	CapKnob() { minAngle = -0.83f * M_PI; maxAngle = 0.83f * M_PI; }
};
struct BigGoldKnob : CapKnob { BigGoldKnob() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob-gold-big.svg"))); } };
struct GoldKnob : CapKnob { GoldKnob() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob-gold.svg"))); } };
struct WhiteKnob : CapKnob { WhiteKnob() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob-white.svg"))); } };
struct TrimKnob : CapKnob { TrimKnob() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob-trim.svg"))); } };

struct DubSirenWidget : ModuleWidget {
	DubSirenWidget(DubSirenModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/DubSiren.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		typedef DubSirenModule M;
		addParam(createParamCentered<BigGoldKnob>(mm2px(Vec(K_SPEED_X, K_SPEED_Y)), module, M::SPEED_PARAM));
		addParam(createParamCentered<BigGoldKnob>(mm2px(Vec(K_PITCH_X, K_PITCH_Y)), module, M::PITCH_PARAM));
		addParam(createParamCentered<GoldKnob>(mm2px(Vec(K_FEEDBACK_X, K_FEEDBACK_Y)), module, M::FEEDBACK_PARAM));
		addParam(createParamCentered<WhiteKnob>(mm2px(Vec(K_MODE_X, K_MODE_Y)), module, M::MODE_PARAM));
		addParam(createParamCentered<WhiteKnob>(mm2px(Vec(K_BEAT_X, K_BEAT_Y)), module, M::BEAT_PARAM));
		addParam(createParamCentered<GoldKnob>(mm2px(Vec(K_VOLUME_X, K_VOLUME_Y)), module, M::VOLUME_PARAM));
		addParam(createParamCentered<TrimKnob>(mm2px(Vec(K_DEPTH_X, K_DEPTH_Y)), module, M::DEPTH_PARAM));
		addParam(createParamCentered<TrimKnob>(mm2px(Vec(K_RELEASE_X, K_RELEASE_Y)), module, M::RELEASE_PARAM));
		addParam(createParamCentered<TrimKnob>(mm2px(Vec(K_TIME_X, K_TIME_Y)), module, M::TIME_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_TRIG_X, S_TRIG_Y)), module, M::TRIG_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_ERA_X, S_ERA_Y)), module, M::ERA_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_EXTRAS_X, S_EXTRAS_Y)), module, M::EXTRAS_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(B_SIREN_X, B_SIREN_Y)), module, M::SIREN_PARAM, M::SIREN_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(B_TONE_X, B_TONE_Y)), module, M::TONE_PARAM, M::TONE_LIGHT));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(L_ACTIVE_X, L_ACTIVE_Y)), module, M::ACTIVE_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_VOCT_X, I_VOCT_Y)), module, M::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_SPEED_X, I_SPEED_Y)), module, M::SPEED_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_FDBK_X, I_FDBK_Y)), module, M::FDBK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_CLOCK_X, I_CLOCK_Y)), module, M::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_TRIG_X, I_TRIG_Y)), module, M::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_SIREN_X, I_SIREN_Y)), module, M::SIREN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_TONE_X, I_TONE_Y)), module, M::TONE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_IN_X, I_IN_Y)), module, M::IN_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_OUT_X, O_OUT_Y)), module, M::OUT_OUTPUT));
	}
};

Model* modelDubSiren = createModel<DubSirenModule, DubSirenWidget>("DubSiren");
