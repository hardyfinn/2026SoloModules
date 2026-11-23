// Bore Room: the inside of an MRI scanner's tube as a reverb.
//   SIZE      head coil (34 cm) .. standard 60 cm bore .. 70 cm wide bore;
//             sets the wall flutter, the cross and length modes and the tail
//   DECAY     0.1 .. 1.5 s;  DAMP  hard plastic .. padded
//   POSITION  head at the centre of the tube .. near the opening, where the
//             room outside takes over
//   EARS      open, earplugs or headphones; BODY the low end you feel
//             through the table
//   MIX       dry .. wet
// The engine is BoreRoom in BoreDSP.hpp (shared with Scanner).
#include "plugin.hpp"
#include "BoreDSP.hpp"

using namespace bore;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float BX[2] = {17.0f, 44.0f};
static const float BR1_Y = 29.0f;
static const float BR2_Y = 52.0f;
static const float BR3_Y = 73.0f;
static const float EARS_Y = 88.0f;
static const float BJX[4] = {9.0f, 23.3f, 37.6f, 52.0f};
static const float BCV_Y = 103.0f;
static const float BIO_Y = 117.0f;

struct BoreRoomModule : Module {
	enum ParamIds { SIZE_PARAM, DECAY_PARAM, DAMP_PARAM, POSITION_PARAM, BODY_PARAM, MIX_PARAM, EARS_PARAM, NUM_PARAMS };
	enum InputIds { SIZE_INPUT, DECAY_INPUT, POSITION_INPUT, MIX_INPUT, IN_L_INPUT, IN_R_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_L_OUTPUT, OUT_R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	BoreRoom room;
	BoreParams bp;
	float mix = 0.5f;
	int div = 0;

	BoreRoomModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SIZE_PARAM, 0.f, 1.f, 0.5f, "Size: head coil, 60 cm bore, 70 cm wide bore");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.45f, "Decay", " s", 12.5f, 0.12f);
		configParam(DAMP_PARAM, 0.f, 1.f, 0.35f, "Damping: hard plastic to padded", "%", 0.f, 100.f);
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Position: centre of the tube to the opening", "%", 0.f, 100.f);
		configParam(BODY_PARAM, 0.f, 1.f, 0.3f, "Body: sound through the table and skull", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 0.6f, "Mix", "%", 0.f, 100.f);
		configSwitch(EARS_PARAM, 0.f, 2.f, 0.f, "Ears", {"Open", "Earplugs", "Headphones"});
		configInput(SIZE_INPUT, "Size CV");
		configInput(DECAY_INPUT, "Decay CV");
		configInput(POSITION_INPUT, "Position CV");
		configInput(MIX_INPUT, "Mix CV");
		configInput(IN_L_INPUT, "Left (mono)");
		configInput(IN_R_INPUT, "Right");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
		room.init(48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { room.init(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); room.reset(); }

	float cv(int in) { return inputs[in].isConnected() ? inputs[in].getVoltage() * 0.1f : 0.f; }

	void process(const ProcessArgs& args) override {
		if (div <= 0) {
			div = 16;
			bp.size = clamp(params[SIZE_PARAM].getValue() + cv(SIZE_INPUT), 0.f, 1.f);
			bp.decay = clamp(params[DECAY_PARAM].getValue() + cv(DECAY_INPUT), 0.f, 1.f);
			bp.damp = params[DAMP_PARAM].getValue();
			bp.position = clamp(params[POSITION_PARAM].getValue() + cv(POSITION_INPUT), 0.f, 1.f);
			bp.body = params[BODY_PARAM].getValue();
			bp.ears = (int) std::round(params[EARS_PARAM].getValue());
			mix = clamp(params[MIX_PARAM].getValue() + cv(MIX_INPUT), 0.f, 1.f);
		}
		div--;
		float inL = inputs[IN_L_INPUT].getVoltage() * 0.2f;
		float inR = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() * 0.2f : inL;
		float wl, wr;
		room.process(inL, inR, bp, wl, wr);
		// equal-power dry/wet
		float gd = std::cos(mix * 0.5f * PI_F), gw = std::sin(mix * 0.5f * PI_F);
		outputs[OUT_L_OUTPUT].setVoltage(5.f * std::tanh(inL * gd + wl * gw));
		outputs[OUT_R_OUTPUT].setVoltage(5.f * std::tanh(inR * gd + wr * gw));
	}
};

struct BoreRoomWidget : ModuleWidget {
	BoreRoomWidget(BoreRoomModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BoreRoom.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(BX[0], BR1_Y)), module, BoreRoomModule::SIZE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(BX[1], BR1_Y)), module, BoreRoomModule::DECAY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(BX[0], BR2_Y)), module, BoreRoomModule::DAMP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(BX[1], BR2_Y)), module, BoreRoomModule::POSITION_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(BX[0], BR3_Y)), module, BoreRoomModule::BODY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(BX[1], BR3_Y)), module, BoreRoomModule::MIX_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(30.0f, EARS_Y + 1.0f)), module, BoreRoomModule::EARS_PARAM));
		const int cvs[4] = {BoreRoomModule::SIZE_INPUT, BoreRoomModule::DECAY_INPUT, BoreRoomModule::POSITION_INPUT, BoreRoomModule::MIX_INPUT};
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BJX[i], BCV_Y)), module, cvs[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BJX[0], BIO_Y)), module, BoreRoomModule::IN_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BJX[1], BIO_Y)), module, BoreRoomModule::IN_R_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(BJX[2], BIO_Y)), module, BoreRoomModule::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(BJX[3], BIO_Y)), module, BoreRoomModule::OUT_R_OUTPUT));
	}
};

Model* modelBoreRoom = createModel<BoreRoomModule, BoreRoomWidget>("BoreRoom");
