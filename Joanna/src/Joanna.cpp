// Joanna: an upright home piano, physically modelled, and the room it stands in.
// The sound engine is in JoannaEngine.hpp, with notes on how it works.
#include "plugin.hpp"
#include "JoannaEngine.hpp"
#include "JoannaPresets.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COL[6] = {16.0f, 44.0f, 72.0f, 100.0f, 128.0f, 156.0f};
static const float ROW_PIANO = 32.0f, ROW_LIFE = 54.0f, ROW_HOUSE = 76.0f, ROW_PEDAL = 96.0f, ROW_IO = 115.0f;
static const float IOX[9] = {10.0f, 27.0f, 44.0f, 61.0f, 78.0f, 95.0f, 122.0f, 140.0f, 158.0f};
static const float PLATE_X = 128.0f, PLATE_Y = 6.2f, PLATE_W = 36.0f, PLATE_H = 9.0f;

struct Joanna : Module {
	enum ParamIds {
		SIZE_PARAM, HAMMER_PARAM, TOUCH_PARAM, SUSTAIN_PARAM, SYMP_PARAM, NOISE_PARAM,
		TUNING_PARAM, UNISON_PARAM, WEAR_PARAM, AGE_PARAM, FINE_PARAM, ANOTHER_PARAM,
		LID_PARAM, WALL_PARAM, ROOM_PARAM, FURNISH_PARAM, DISTANCE_PARAM, LEVEL_PARAM,
		PEDAL_PARAM, SOFT_PARAM, FELT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, SUS_INPUT, SOFT_INPUT, EXT_INPUT,
		HAMMER_INPUT, TUNING_INPUT, DIST_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { L_OUTPUT, R_OUTPUT, DRY_OUTPUT, NUM_OUTPUTS };
	enum LightIds { PEDAL_LIGHT, SOFT_LIGHT, FELT_LIGHT, ANOTHER_LIGHT, NUM_LIGHTS };

	joanna::Engine engine;
	joanna::Controls ctl;
	uint32_t serial = 4127;
	int pitchRef = 0;          // 0: A440, 1: A435 (old pitch), 2: A432
	bool freePitch = false;
	dsp::SchmittTrigger gateIn[16], anotherButton;
	bool gateHigh[16] = {};
	int keyOf[16];
	dsp::ClockDivider lightDivider;

	struct YearsQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			float y = 20.f * v * v;
			if (y < 0.08f) return "tuned today";
			if (y < 1.f) return string::f("%.0f months since the tuner came", y * 12.f);
			return string::f("%.1f years since the tuner came", y);
		}
	};
	struct LidQuantity : SwitchQuantity {};
	struct DistanceQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			const char* w = v < 0.12f ? "inside the piano" : v < 0.28f ? "over the player's shoulder" : v < 0.42f ? "the player's seat" :
			                v < 0.62f ? "across the room" : v < 0.8f ? "far side of the room" : "the next room";
			return string::f("%.0f %% (%s)", v * 100.f, w);
		}
	};

	Joanna() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SIZE_PARAM, 0.f, 1.f, 0.5f, "Size (spinet <-> tall old upright: string length, bass, soundboard)", "%", 0.f, 100.f);
		configParam(HAMMER_PARAM, 0.f, 1.f, 0.4f, "Hammer felt (soft and voiced <-> hard; tack piano at the top)", "%", 0.f, 100.f);
		configParam(TOUCH_PARAM, 0.f, 1.f, 0.5f, "Touch (heavy action <-> light action: velocity curve)", "%", 0.f, 100.f);
		configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.5f, "Sustain (how long the strings ring)", "%", 0.f, 100.f);
		configParam(SYMP_PARAM, 0.f, 1.f, 0.5f, "Sympathy (strings answering each other through the bridge; 50 % = real)", "%", 0.f, 100.f);
		configParam(NOISE_PARAM, 0.f, 1.f, 0.4f, "Noise (key thump, dampers, pedals, rattles)", "%", 0.f, 100.f);
		configParam<YearsQuantity>(TUNING_PARAM, 0.f, 1.f, 0.25f, "Tuning (time since the tuner came)");
		configParam(UNISON_PARAM, 0.f, 25.f, 0.f, "Unison (extra spread between the strings of a note: honky-tonk)", " cents");
		configParam(WEAR_PARAM, 0.f, 1.f, 0.3f, "Wear (how much it has been played: hard flat hammers, uneven keys, loose action)", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age (dead bass strings, tired soundboard, leaky dampers)", "%", 0.f, 100.f);
		configParam(FINE_PARAM, -50.f, 50.f, 0.f, "Fine tune", " cents");
		configButton(ANOTHER_PARAM, "Another piano (new serial number: every quirk of every key changes)");
		configSwitch(LID_PARAM, 0.f, 2.f, 1.f, "Lid", {"shut", "top open", "front panel off"});
		configParam(WALL_PARAM, 0.f, 1.f, 0.3f, "Wall (back of the piano to the wall)", " cm", 0.f, 57.f, 3.f);
		configParam(ROOM_PARAM, 0.f, 1.f, 0.35f, "Room (box room <-> front room <-> hall)", "%", 0.f, 100.f);
		configParam(FURNISH_PARAM, 0.f, 1.f, 0.35f, "Furnish (carpet, curtains and sofa <-> bare boards)", "%", 0.f, 100.f);
		configParam<DistanceQuantity>(DISTANCE_PARAM, 0.f, 1.f, 0.4f, "Distance (where you listen)");
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f);
		configSwitch(PEDAL_PARAM, 0.f, 1.f, 0.f, "Sustain pedal", {"up", "down"});
		configSwitch(SOFT_PARAM, 0.f, 1.f, 0.f, "Soft pedal (hammers move closer to the strings)", {"off", "on"});
		configSwitch(FELT_PARAM, 0.f, 1.f, 0.f, "Felt (practice felt between hammers and strings)", {"off", "on"});

		configInput(VOCT_INPUT, "V/OCT (poly, 0 V = middle C; snaps to the nearest key)");
		configInput(GATE_INPUT, "Gate (poly: key down while high)");
		configInput(VEL_INPUT, "Velocity (poly, 0-10 V, read at each key press; 0 V = press the key silently; unpatched = 6 V)");
		configInput(SUS_INPUT, "Sustain pedal CV (0-10 V, half pedal in between; adds to the button)");
		configInput(SOFT_INPUT, "Soft pedal gate (when patched: high = on)");
		configInput(EXT_INPUT, "EXT (outside sound shaking the bridge: sing into the piano with the pedal down)");
		configInput(HAMMER_INPUT, "Hammer CV (+-5 V = full range, read at each key press)");
		configInput(TUNING_INPUT, "Tuning CV (+-5 V = full range)");
		configInput(DIST_INPUT, "Distance CV (+-5 V = full range)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		configOutput(DRY_OUTPUT, "Dry (soundboard and cabinet, no room; mono)");

		for (int c = 0; c < 16; c++) keyOf[c] = -1;
		lightDivider.setDivision(256);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	void releaseAll() {
		for (int c = 0; c < 16; c++) {
			if (gateHigh[c] && keyOf[c] >= 0) engine.noteOff(keyOf[c]);
			gateHigh[c] = false;
			keyOf[c] = -1;
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		releaseAll();
		engine.reset();
		serial = 4127;
		pitchRef = 0;
		freePitch = false;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "serial", json_integer(serial));
		json_object_set_new(root, "pitchRef", json_integer(pitchRef));
		json_object_set_new(root, "freePitch", json_boolean(freePitch));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "serial")) serial = (uint32_t)json_integer_value(j);
		if (json_t* j = json_object_get(root, "pitchRef")) pitchRef = (int)json_integer_value(j);
		if (json_t* j = json_object_get(root, "freePitch")) freePitch = json_boolean_value(j);
	}

	static float cv5(Input& in) { return in.getVoltage() * 0.1f; }

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);

		if (anotherButton.process(params[ANOTHER_PARAM].getValue()))
			serial = 1000 + random::u32() % 9000;

		ctl.size = params[SIZE_PARAM].getValue();
		ctl.hammer = clamp(params[HAMMER_PARAM].getValue() + cv5(inputs[HAMMER_INPUT]), 0.f, 1.f);
		ctl.touch = params[TOUCH_PARAM].getValue();
		ctl.sustain = params[SUSTAIN_PARAM].getValue();
		ctl.symp = params[SYMP_PARAM].getValue();
		ctl.noise = params[NOISE_PARAM].getValue();
		ctl.tuning = clamp(params[TUNING_PARAM].getValue() + cv5(inputs[TUNING_INPUT]), 0.f, 1.f);
		ctl.unison = params[UNISON_PARAM].getValue() / 25.f;
		ctl.wear = params[WEAR_PARAM].getValue();
		ctl.age = params[AGE_PARAM].getValue();
		ctl.fine = params[FINE_PARAM].getValue();
		ctl.lid = (int)std::round(params[LID_PARAM].getValue());
		ctl.wall = params[WALL_PARAM].getValue();
		ctl.room = params[ROOM_PARAM].getValue();
		ctl.furnish = params[FURNISH_PARAM].getValue();
		ctl.distance = clamp(params[DISTANCE_PARAM].getValue() + cv5(inputs[DIST_INPUT]), 0.f, 1.f);
		ctl.level = params[LEVEL_PARAM].getValue();
		float pedal = params[PEDAL_PARAM].getValue();
		if (inputs[SUS_INPUT].isConnected())
			pedal = std::max(pedal, clamp(inputs[SUS_INPUT].getVoltage() * 0.1f, 0.f, 1.f));
		ctl.pedal = pedal;
		if (inputs[SOFT_INPUT].isConnected())
			ctl.soft = inputs[SOFT_INPUT].getVoltage() > 1.f;
		else
			ctl.soft = params[SOFT_PARAM].getValue() > 0.5f;
		ctl.felt = params[FELT_PARAM].getValue() > 0.5f;
		ctl.pitchRef = pitchRef == 1 ? 435.f : pitchRef == 2 ? 432.f : 440.f;
		ctl.freePitch = freePitch;
		ctl.serial = serial;

		// keys: one per poly channel
		int channels = std::max(inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels());
		for (int c = 0; c < 16; c++) {
			bool high = c < channels && inputs[GATE_INPUT].getPolyVoltage(c) > 1.f;
			if (high) {
				float v = inputs[VOCT_INPUT].getPolyVoltage(c);
				float semis = v * 12.f;
				int key = clamp((int)std::round(semis) + joanna::KEY_C4, 0, joanna::NKEYS - 1);
				float frac = semis - std::round(semis);
				if (!gateHigh[c] || key != keyOf[c]) {
					if (gateHigh[c] && keyOf[c] >= 0) engine.noteOff(keyOf[c]);
					float vel = 0.6f;
					if (inputs[VEL_INPUT].isConnected())
						vel = clamp(inputs[VEL_INPUT].getPolyVoltage(c) * 0.1f, 0.f, 1.f);
					engine.noteOn(key, vel, frac);
					keyOf[c] = key;
				}
			} else if (gateHigh[c]) {
				if (keyOf[c] >= 0) engine.noteOff(keyOf[c]);
				keyOf[c] = -1;
			}
			gateHigh[c] = high;
		}

		float ext = inputs[EXT_INPUT].isConnected() ? inputs[EXT_INPUT].getVoltage() * 2.f : 0.f;
		engine.process(ctl, ext);
		outputs[L_OUTPUT].setVoltage(engine.outL);
		outputs[R_OUTPUT].setVoltage(engine.outR);
		outputs[DRY_OUTPUT].setVoltage(engine.outDry);

		if (lightDivider.process()) {
			lights[PEDAL_LIGHT].setBrightness(pedal);
			lights[SOFT_LIGHT].setBrightness(ctl.soft ? 1.f : 0.f);
			lights[FELT_LIGHT].setBrightness(ctl.felt ? 1.f : 0.f);
			lights[ANOTHER_LIGHT].setBrightness(params[ANOTHER_PARAM].getValue());
		}
	}

	// right-click presets: one piano in one place (TOUCH, FINE, LEVEL, sustain and soft pedals are left alone)
	void loadPreset(const joanna::Preset& p) {
		params[SIZE_PARAM].setValue(p.size);
		params[HAMMER_PARAM].setValue(p.hammer);
		params[SUSTAIN_PARAM].setValue(p.sustain);
		params[SYMP_PARAM].setValue(p.symp);
		params[NOISE_PARAM].setValue(p.noise);
		params[TUNING_PARAM].setValue(p.tuning);
		params[UNISON_PARAM].setValue(p.unison * 25.f);
		params[WEAR_PARAM].setValue(p.wear);
		params[AGE_PARAM].setValue(p.age);
		params[LID_PARAM].setValue((float)p.lid);
		params[WALL_PARAM].setValue(p.wall);
		params[ROOM_PARAM].setValue(p.room);
		params[FURNISH_PARAM].setValue(p.furnish);
		params[DISTANCE_PARAM].setValue(p.distance);
		params[FELT_PARAM].setValue(p.felt ? 1.f : 0.f);
		serial = p.serial;
	}
};

// the brass maker's plate shows the piano's serial number
struct SerialPlate : TransparentWidget {
	Joanna* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
			if (!font) return;
			uint32_t s = module ? module->serial : 4127;
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 13.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0x4a, 0x24, 0x16));
			nvgText(args.vg, box.size.x / 2, box.size.y / 2 + 0.5f, string::f("No. %u", (unsigned)s).c_str(), NULL);
		}
		TransparentWidget::drawLayer(args, layer);
	}
};

struct JoannaWidget : ModuleWidget {
	JoannaWidget(Joanna* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Joanna.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SerialPlate* plate = createWidget<SerialPlate>(mm2px(Vec(PLATE_X, PLATE_Y)));
		plate->box.size = mm2px(Vec(PLATE_W, PLATE_H));
		plate->module = module;
		addChild(plate);

		typedef Joanna J;
		const int piano[6] = {J::SIZE_PARAM, J::HAMMER_PARAM, J::TOUCH_PARAM, J::SUSTAIN_PARAM, J::SYMP_PARAM, J::NOISE_PARAM};
		const int life[5] = {J::TUNING_PARAM, J::UNISON_PARAM, J::WEAR_PARAM, J::AGE_PARAM, J::FINE_PARAM};
		const int house[5] = {J::WALL_PARAM, J::ROOM_PARAM, J::FURNISH_PARAM, J::DISTANCE_PARAM, J::LEVEL_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_PIANO)), module, piano[i]));
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_LIFE)), module, life[i]));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(COL[5], ROW_LIFE)), module, J::ANOTHER_PARAM, J::ANOTHER_LIGHT));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(COL[0], ROW_HOUSE)), module, J::LID_PARAM));
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i + 1], ROW_HOUSE)), module, house[i]));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COL[0], ROW_PEDAL)), module, J::PEDAL_PARAM, J::PEDAL_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COL[1], ROW_PEDAL)), module, J::SOFT_PARAM, J::SOFT_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COL[2], ROW_PEDAL)), module, J::FELT_PARAM, J::FELT_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[3], ROW_PEDAL)), module, J::HAMMER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[4], ROW_PEDAL)), module, J::TUNING_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[5], ROW_PEDAL)), module, J::DIST_INPUT));

		const int io[6] = {J::VOCT_INPUT, J::GATE_INPUT, J::VEL_INPUT, J::SUS_INPUT, J::SOFT_INPUT, J::EXT_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[i], ROW_IO)), module, io[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[6], ROW_IO)), module, J::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[7], ROW_IO)), module, J::R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[8], ROW_IO)), module, J::DRY_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Joanna* m = dynamic_cast<Joanna*>(this->module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Presets (20)", "", [=](Menu* sub) {
			for (int i = 0; i < joanna::NUM_PRESETS; i++) {
				const joanna::Preset& p = joanna::PRESETS[i];
				sub->addChild(createMenuItem(string::f("%d. %s", i + 1, p.name), "", [=]() { m->loadPreset(p); }));
			}
		}));
		menu->addChild(createIndexPtrSubmenuItem("Concert pitch", {"A = 440 Hz", "A = 435 Hz (old pitch)", "A = 432 Hz"}, &m->pitchRef));
		menu->addChild(createBoolPtrMenuItem("Free pitch (V/OCT between keys bends that key)", "", &m->freePitch));
		menu->addChild(createMenuItem("Another piano (new serial number)", "", [=]() { m->serial = 1000 + random::u32() % 9000; }));
	}
};

Model* modelJoanna = createModel<Joanna, JoannaWidget>("Joanna");
