#include "plugin.hpp"
#include "UprightEngine.hpp"
#include <thread>
#include <mutex>

static const float CX[2] = {16.0f, 45.0f};
static const float ROW1 = 39.0f, ROW2 = 62.0f, IO1 = 88.0f, IO2 = 108.0f;
static const float IOX[3] = {11.0f, 30.5f, 50.0f};
static const float READY_X = 52.5f, READY_Y = 10.6f;

// One copy of the samples for every Upright in the patch, decoded in the
// background the first time a module is created (about 150 MB, a second or two).
static upright::Bank gBank;
static std::mutex gBankMutex;

static void loadBankOnce() {
	std::lock_guard<std::mutex> lock(gBankMutex);
	if (gBank.state.load() != 0) return;
	gBank.state = 1;
	std::string dir = asset::plugin(pluginInstance, "res/samples");
	std::thread([dir]() {
		bool ok = gBank.loadAll(dir);
		gBank.state = ok ? 2 : 3;
	}).detach();
}

struct Upright : Module {
	enum ParamIds { TONE_PARAM, RELEASE_PARAM, PEDAL_PARAM, VOLUME_PARAM, NUM_PARAMS };
	enum InputIds { VOCT_INPUT, GATE_INPUT, VEL_INPUT, SUS_INPUT, NUM_INPUTS };
	enum OutputIds { L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { PEDAL_LIGHT, READY_LIGHT, NUM_LIGHTS };

	upright::Engine engine;
	upright::Controls ctl;
	bool gateHigh[16] = {};
	float pitchOf[16] = {};
	dsp::ClockDivider lightDivider;
	float blink = 0.f;

	struct ReleaseQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.2f s", releaseSeconds(getValue())); }
	};
	static float releaseSeconds(float v) { return 0.06f * std::pow(50.f, v); }   // 0.06 .. 3 s

	Upright() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TONE_PARAM, -1.f, 1.f, 0.f, "Tone (dark <-> bright)", "%", 0.f, 100.f);
		configParam<ReleaseQuantity>(RELEASE_PARAM, 0.f, 1.f, 0.45f, "Release (how fast the dampers stop a note)");
		configSwitch(PEDAL_PARAM, 0.f, 1.f, 0.f, "Sustain pedal", {"up", "down"});
		configParam(VOLUME_PARAM, 0.f, 2.f, 1.f, "Volume", "%", 0.f, 100.f)->randomizeEnabled = false;
		configInput(VOCT_INPUT, "V/OCT (poly, 0 V = middle C)");
		configInput(GATE_INPUT, "Gate (poly: key down while high)");
		configInput(VEL_INPUT, "Velocity (poly, 0-10 V, read at each key press; unpatched = 6.5 V)");
		configInput(SUS_INPUT, "Sustain pedal gate (high = pedal down; adds to the button)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		lightDivider.setDivision(512);
		engine.bank = &gBank;
		loadBankOnce();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine.reset();
		for (int c = 0; c < 16; c++) gateHigh[c] = false;
	}

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);
		ctl.tone = params[TONE_PARAM].getValue();
		ctl.release = releaseSeconds(params[RELEASE_PARAM].getValue());
		ctl.volume = params[VOLUME_PARAM].getValue() * 5.f;
		bool pedal = params[PEDAL_PARAM].getValue() > 0.5f || (inputs[SUS_INPUT].isConnected() && inputs[SUS_INPUT].getVoltage() > 1.f);
		ctl.pedal = pedal;
		engine.setPedal(pedal, ctl);

		int channels = std::max(inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels());
		for (int c = 0; c < 16; c++) {
			bool high = c < channels && inputs[GATE_INPUT].getPolyVoltage(c) > 1.f;
			if (high && !gateHigh[c]) {
				float pitch = 60.f + 12.f * inputs[VOCT_INPUT].getPolyVoltage(c);
				float vel = 0.65f;
				if (inputs[VEL_INPUT].isConnected())
					vel = clamp(inputs[VEL_INPUT].getPolyVoltage(c) * 0.1f, 0.f, 1.f);
				engine.noteOn(c, clamp(pitch, 21.f, 108.f), vel, ctl);
				pitchOf[c] = pitch;
			}
			else if (!high && gateHigh[c]) {
				engine.noteOff(c, ctl);
			}
			gateHigh[c] = high;
		}

		engine.process(ctl);
		outputs[L_OUTPUT].setVoltage(engine.outL);
		outputs[R_OUTPUT].setVoltage(engine.outR);

		if (lightDivider.process()) {
			lights[PEDAL_LIGHT].setBrightness(pedal ? 1.f : 0.f);
			int st = gBank.state.load();
			blink += 512.f * args.sampleTime;
			float b = st == 2 ? 1.f : st == 1 ? (std::fmod(blink, 0.5f) < 0.25f ? 1.f : 0.f) : 0.f;
			lights[READY_LIGHT].setBrightness(b);
		}
	}
};

struct UprightWidget : ModuleWidget {
	UprightWidget(Upright* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Upright.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Upright U;
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(READY_X, READY_Y)), module, U::READY_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(CX[0], ROW1)), module, U::TONE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(CX[1], ROW1)), module, U::RELEASE_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(CX[0], ROW2)), module, U::PEDAL_PARAM, U::PEDAL_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(CX[1], ROW2)), module, U::VOLUME_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[0], IO1)), module, U::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[1], IO1)), module, U::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[2], IO1)), module, U::VEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[0], IO2)), module, U::SUS_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[1], IO2)), module, U::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[2], IO2)), module, U::R_OUTPUT));
	}
};

Model* modelUpright = createModel<Upright, UprightWidget>("Upright");
