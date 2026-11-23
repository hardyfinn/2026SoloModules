// House Keys: polyphonic voice playing 100 keyboard sounds of late-80s Detroit and Chicago
// records through models of the five machines they came from. See docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "PanelLayout.hpp"
#include "dsp/Core.hpp"
#include <osdialog.h>
#include <atomic>
#include <fstream>
#include <iterator>

using namespace hk;

static const char* STAB_NAMES[10] = {"Off", "5th", "Octave", "Minor", "Major", "Minor 7", "Major 7", "Dominant 7", "Sus 4", "Minor 9"};
static const char* ENGINE_LONG[NUM_ENGINES] = {"4-OPERATOR FM", "ANALOG POLY", "PHASE DIST", "WAVE", "SAMPLER"};

struct SoundQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int s = clamp((int) std::round(getValue()), 0, NUM_SOUNDS - 1);
		return string::f("%03d %s", s + 1, soundName(s));
	}
};

struct HouseKeys : Module {
	enum ParamId {
		SOUND_PARAM, PREV_PARAM, NEXT_PARAM,
		ENGINE_PARAM, // 5 buttons
		OCTAVE_PARAM = ENGINE_PARAM + NUM_ENGINES,
		TONE_PARAM, ATTACK_PARAM, DECAY_PARAM, VELOCITY_PARAM, MOD_PARAM,
		TWIN_PARAM, CHORUS_PARAM, STAB_PARAM, GRIT_PARAM, TAPE_PARAM, LEVEL_PARAM,
		PARAMS_LEN
	};
	enum InputId { VOCT_INPUT, GATE_INPUT, VEL_INPUT, SOUND_INPUT, TONE_INPUT, DECAY_INPUT, MOD_INPUT, TWIN_INPUT, INPUTS_LEN };
	enum OutputId { L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { ENGINE_LIGHT, LIGHTS_LEN = ENGINE_LIGHT + NUM_ENGINES };

	Core core;
	CoreParams cp;
	dsp::BooleanTrigger prevTrig, nextTrig, engTrig[NUM_ENGINES];
	dsp::ClockDivider paramDiv, lightDiv;
	int polyLimit = 8;
	int fixedVel = -1;

	// a loaded 4-operator bank; the chosen voice is handed to the audio thread
	uint8_t bank[32][128] = {};
	std::string bankNames[32];
	int bankSize = 0;
	std::string bankPath;
	uint8_t userVoice[128] = {};
	std::string userVoiceName;
	bool userVoiceOpz = false;
	bool userHave = false;          // restore on load
	std::atomic<bool> userRequest{false};
	int userSoundAtLoad = -1;       // SOUND when the voice was chosen; moving SOUND drops it

	// for the display (written on the audio thread)
	std::atomic<int> shownSound{0}, shownEngine{0};
	std::atomic<bool> shownLoading{false}, shownUser{false};

	HouseKeys() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<SoundQuantity>(SOUND_PARAM, 0.f, NUM_SOUNDS - 1, 0.f, "Sound");
		getParamQuantity(SOUND_PARAM)->snapEnabled = true;
		configButton(PREV_PARAM, "Previous sound");
		configButton(NEXT_PARAM, "Next sound");
		const char* en[NUM_ENGINES] = {"FM (sounds 1 to 30)", "POLY (31 to 54)", "PD (55 to 66)", "WAVE (67 to 88)", "SAMPLER (89 to 100)"};
		for (int i = 0; i < NUM_ENGINES; i++) configButton(ENGINE_PARAM + i, en[i]);
		configParam(OCTAVE_PARAM, -2.f, 2.f, 0.f, "Octave");
		getParamQuantity(OCTAVE_PARAM)->snapEnabled = true;
		configParam(TONE_PARAM, -1.f, 1.f, 0.f, "Tone", "%", 0.f, 100.f);
		configParam(ATTACK_PARAM, -1.f, 1.f, 0.f, "Attack time", "x", 8.f);
		configParam(DECAY_PARAM, -1.f, 1.f, 0.f, "Decay and release time", "x", 8.f);
		configParam(VELOCITY_PARAM, 0.f, 2.f, 1.f, "Velocity amount", "%", 0.f, 100.f);
		configParam(MOD_PARAM, 0.f, 1.f, 0.f, "Mod wheel", "%", 0.f, 100.f);
		configParam(TWIN_PARAM, 0.f, 1.f, 0.f, "Twin (second unit, detuned and panned)", "%", 0.f, 100.f);
		configSwitch(CHORUS_PARAM, 0.f, 3.f, 0.f, "Chorus", {"As the sound", "Off", "I", "II"});
		configSwitch(STAB_PARAM, 0.f, 9.f, 0.f, "Stab chord", {STAB_NAMES, STAB_NAMES + 10});
		configParam(GRIT_PARAM, 0.f, 1.f, 0.5f, "Grit (converter and age; 50% is the real machine)", "%", 0.f, 100.f);
		configParam(TAPE_PARAM, 0.f, 1.f, 0.f, "Tape", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.5f, "Level", " dB", -10.f, 40.f, 18.0618f)->randomizeEnabled = false;
		configInput(VOCT_INPUT, "V/OCT (polyphonic)");
		configInput(GATE_INPUT, "Gate (polyphonic)");
		configInput(VEL_INPUT, "Velocity 0 to 10 V (polyphonic)");
		configInput(SOUND_INPUT, "Sound CV (0 to 10 V spans 1 to 100)");
		configInput(TONE_INPUT, "Tone CV (±5 V)");
		configInput(DECAY_INPUT, "Decay CV (±5 V)");
		configInput(MOD_INPUT, "Mod CV (0 to 10 V)");
		configInput(TWIN_INPUT, "Twin CV (0 to 10 V)");
		configOutput(L_OUTPUT, "Left (mono sum when R is unpatched)");
		configOutput(R_OUTPUT, "Right");
		paramDiv.setDivision(16);
		lightDiv.setDivision(256);
		core.init(APP->engine->getSampleRate());
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		core.init(e.sampleRate);
		if (userHave) userRequest = true;
	}

	void onReset() override {
		polyLimit = 8; fixedVel = -1;
		userHave = false; userSoundAtLoad = -1;
		core.panic();
	}

	int currentSoundParam() {
		return clamp((int) std::round(params[SOUND_PARAM].getValue()), 0, NUM_SOUNDS - 1);
	}

	void updateParams() {
		int s = currentSoundParam();
		if (inputs[SOUND_INPUT].isConnected())
			s = clamp(s + (int) std::round(clamp(inputs[SOUND_INPUT].getVoltage(), 0.f, 10.f) / 10.f * 99.f), 0, NUM_SOUNDS - 1);
		cp.sound = s;
		cp.octave = (int) std::round(params[OCTAVE_PARAM].getValue());
		cp.tone = clamp(params[TONE_PARAM].getValue() + inputs[TONE_INPUT].getVoltage() * 0.4f, -1.f, 1.f);
		cp.attack = params[ATTACK_PARAM].getValue();
		cp.decay = clamp(params[DECAY_PARAM].getValue() + inputs[DECAY_INPUT].getVoltage() * 0.4f, -1.f, 1.f);
		cp.velocity = params[VELOCITY_PARAM].getValue();
		cp.mod = clamp(params[MOD_PARAM].getValue() + inputs[MOD_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		cp.twin = clamp(params[TWIN_PARAM].getValue() + inputs[TWIN_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		cp.grit = params[GRIT_PARAM].getValue();
		cp.tape = params[TAPE_PARAM].getValue();
		cp.level = params[LEVEL_PARAM].getValue();
		cp.chorus = (int) std::round(params[CHORUS_PARAM].getValue());
		cp.stab = (int) std::round(params[STAB_PARAM].getValue());
		cp.polyLimit = polyLimit;
		cp.fixedVel = fixedVel;
	}

	void process(const ProcessArgs& args) override {
		// buttons move the SOUND knob
		if (prevTrig.process(params[PREV_PARAM].getValue() > 0.f))
			params[SOUND_PARAM].setValue((float) ((currentSoundParam() + NUM_SOUNDS - 1) % NUM_SOUNDS));
		if (nextTrig.process(params[NEXT_PARAM].getValue() > 0.f))
			params[SOUND_PARAM].setValue((float) ((currentSoundParam() + 1) % NUM_SOUNDS));
		for (int i = 0; i < NUM_ENGINES; i++)
			if (engTrig[i].process(params[ENGINE_PARAM + i].getValue() > 0.f))
				params[SOUND_PARAM].setValue((float) engineFirstSound(i));

		if (paramDiv.process()) {
			updateParams();
			if (userRequest.exchange(false)) {
				core.setUserVoice(userVoice, userVoiceOpz, userVoiceName.c_str());
				userSoundAtLoad = cp.sound;
			}
			if (userHave && userSoundAtLoad >= 0 && cp.sound != userSoundAtLoad) { userHave = false; userSoundAtLoad = -1; }
		}

		// notes
		int nch = std::max(inputs[GATE_INPUT].getChannels(), inputs[VOCT_INPUT].getChannels());
		nch = std::min(nch, MAX_CH);
		float voct[MAX_CH], gate[MAX_CH], vel[MAX_CH];
		for (int c = 0; c < nch; c++) {
			voct[c] = inputs[VOCT_INPUT].getPolyVoltage(c);
			gate[c] = inputs[GATE_INPUT].getPolyVoltage(c);
			vel[c] = inputs[VEL_INPUT].getPolyVoltage(c);
		}
		core.setInputs(nch, voct, gate, inputs[VEL_INPUT].isConnected() ? vel : nullptr);

		float l, r;
		core.process(&l, &r, 1, cp);
		if (outputs[R_OUTPUT].isConnected()) {
			outputs[L_OUTPUT].setVoltage(l);
			outputs[R_OUTPUT].setVoltage(r);
		}
		else outputs[L_OUTPUT].setVoltage(0.5f * (l + r));

		if (lightDiv.process()) {
			int e = core.currentEngine();
			for (int i = 0; i < NUM_ENGINES; i++) lights[ENGINE_LIGHT + i].setBrightness(i == e ? 1.f : 0.f);
			shownSound = core.currentSound();
			shownEngine = e;
			shownLoading = core.loading();
			shownUser = core.userActive();
		}
	}

	// ---- 4-operator banks
	bool loadBank(const std::string& path) {
		std::ifstream f(path, std::ios::binary);
		if (!f) return false;
		std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		uint8_t tmp[32][128];
		int n = parseVmemBank(data, tmp);
		if (n <= 0) return false;
		std::memcpy(bank, tmp, sizeof(bank));
		bankSize = n;
		for (int i = 0; i < n; i++) {
			char nm[11];
			for (int k = 0; k < 10; k++) { char c = (char) bank[i][57 + k]; nm[k] = (c >= 32 && c < 127) ? c : ' '; }
			nm[10] = 0;
			std::string s(nm);
			while (!s.empty() && s.back() == ' ') s.pop_back();
			bankNames[i] = s.empty() ? string::f("Voice %d", i + 1) : s;
		}
		bankPath = path;
		return true;
	}

	void chooseVoice(int i) {
		if (i < 0 || i >= bankSize) return;
		std::memcpy(userVoice, bank[i], 128);
		userVoiceName = bankNames[i];
		// TX81Z extension bytes in use means the TX81Z chip (extra waveforms, fixed frequencies)
		userVoiceOpz = false;
		for (int k = 73; k < 81; k++) if (bank[i][k]) userVoiceOpz = true;
		userHave = true;
		userRequest = true;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "polyLimit", json_integer(polyLimit));
		json_object_set_new(root, "fixedVel", json_integer(fixedVel));
		if (userHave) {
			std::string hex;
			for (int i = 0; i < 128; i++) hex += string::f("%02x", userVoice[i]);
			json_object_set_new(root, "userVoice", json_string(hex.c_str()));
			json_object_set_new(root, "userName", json_string(userVoiceName.c_str()));
			json_object_set_new(root, "userOpz", json_boolean(userVoiceOpz));
		}
		if (!bankPath.empty()) json_object_set_new(root, "bankPath", json_string(bankPath.c_str()));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "polyLimit")) polyLimit = (int) json_integer_value(j);
		if (polyLimit != 4 && polyLimit != 6 && polyLimit != 16) polyLimit = 8;
		if (json_t* j = json_object_get(root, "fixedVel")) fixedVel = (int) json_integer_value(j);
		if (json_t* j = json_object_get(root, "bankPath")) loadBank(json_string_value(j));
		json_t* uv = json_object_get(root, "userVoice");
		if (uv && json_is_string(uv) && std::strlen(json_string_value(uv)) == 256) {
			const char* h = json_string_value(uv);
			for (int i = 0; i < 128; i++) { unsigned v = 0; std::sscanf(h + 2 * i, "%2x", &v); userVoice[i] = (uint8_t) v; }
			if (json_t* j = json_object_get(root, "userName")) userVoiceName = json_string_value(j);
			if (json_t* j = json_object_get(root, "userOpz")) userVoiceOpz = json_boolean_value(j);
			userHave = true;
			userRequest = true;
		}
	}
};

struct HouseKeysDisplay : Widget {
	HouseKeys* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		float w = box.size.x, h = box.size.y;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		int s = module ? module->shownSound.load() : 0;
		int e = module ? module->shownEngine.load() : 0;
		bool loading = module && module->shownLoading.load();
		bool user = module && module->shownUser.load();
		std::string l1 = user ? "USR " + module->userVoiceName : string::f("%03d %s", s + 1, soundName(s));
		std::string l2 = ENGINE_LONG[clamp(e, 0, NUM_ENGINES - 1)];
		NVGcolor amber = nvgRGB(0xFF, 0xB0, 0x4A), dim = nvgRGB(0x9A, 0x86, 0x60);
		nvgFontFaceId(args.vg, font->handle);
		float pad = mm2px(1.6f);
		nvgFontSize(args.vg, mm2px(4.0f));
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(args.vg, amber);
		nvgText(args.vg, pad, mm2px(6.2f), l1.c_str(), NULL);
		nvgFontSize(args.vg, mm2px(3.0f));
		nvgFillColor(args.vg, dim);
		nvgText(args.vg, pad, h - mm2px(2.2f), l2.c_str(), NULL);
		if (loading) {
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, amber);
			nvgText(args.vg, w - pad, h - mm2px(2.2f), "LOADING", NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct HouseKeysWidget : ModuleWidget {
	HouseKeysWidget(HouseKeys* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/HouseKeys.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		HouseKeysDisplay* d = createWidget<HouseKeysDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(SOUND_X, SOUND_Y)), module, HouseKeys::SOUND_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(PREV_X, PN_Y)), module, HouseKeys::PREV_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(NEXT_X, PN_Y)), module, HouseKeys::NEXT_PARAM));
		for (int i = 0; i < NUM_ENGINES; i++)
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(ENG_X0 + ENG_DX * i, ENG_Y)), module,
				HouseKeys::ENGINE_PARAM + i, HouseKeys::ENGINE_LIGHT + i));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_PLAY, ROW1)), module, HouseKeys::OCTAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_PLAY, ROW2)), module, HouseKeys::STAB_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_PLAY, ROW3)), module, HouseKeys::VELOCITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_PLAY, ROW4)), module, HouseKeys::MOD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_SHAPE, ROW1)), module, HouseKeys::TONE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_SHAPE, ROW2)), module, HouseKeys::ATTACK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_SHAPE, ROW3)), module, HouseKeys::DECAY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_M1, ROW1)), module, HouseKeys::TWIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_M2, ROW1)), module, HouseKeys::CHORUS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_M1, ROW2)), module, HouseKeys::GRIT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_M2, ROW2)), module, HouseKeys::TAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec((COL_M1 + COL_M2) / 2, ROW3)), module, HouseKeys::LEVEL_PARAM));

		for (int i = 0; i < 8; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + JACK_DX * i, JACK_Y)), module, HouseKeys::VOCT_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + JACK_DX * 8, JACK_Y)), module, HouseKeys::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + JACK_DX * 9, JACK_Y)), module, HouseKeys::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		HouseKeys* m = dynamic_cast<HouseKeys*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Polyphony limit", {"4", "6", "8", "16"},
			[=]() { return m->polyLimit == 4 ? 0 : m->polyLimit == 6 ? 1 : m->polyLimit == 16 ? 3 : 2; },
			[=](int i) { const int v[4] = {4, 6, 8, 16}; m->polyLimit = v[i]; }));
		menu->addChild(createIndexSubmenuItem("Fixed velocity when VEL is unpatched", {"Default (100, FM sounds 64)", "64", "100", "127"},
			[=]() { return m->fixedVel == 64 ? 1 : m->fixedVel == 100 ? 2 : m->fixedVel == 127 ? 3 : 0; },
			[=](int i) { const int v[4] = {-1, 64, 100, 127}; m->fixedVel = v[i]; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Load 4-operator voice bank (.syx)…", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("SysEx:syx,SYX");
			char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
			osdialog_filters_free(filters);
			if (!path) return;
			bool ok = m->loadBank(path);
			std::free(path);
			if (!ok) osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, "No 32-voice 4-operator bank (VMEM) found in that file.");
		}));
		if (m->bankSize > 0) {
			menu->addChild(createSubmenuItem("Play a voice from the bank", "", [=](Menu* sub) {
				for (int i = 0; i < m->bankSize; i++)
					sub->addChild(createMenuItem(string::f("%02d %s", i + 1, m->bankNames[i].c_str()), "", [=]() { m->chooseVoice(i); }));
			}));
		}
		if (m->userHave)
			menu->addChild(createMenuLabel("Turn SOUND to leave the loaded voice"));
	}
};

Model* modelHouseKeys = createModel<HouseKeys, HouseKeysWidget>("HouseKeys");
