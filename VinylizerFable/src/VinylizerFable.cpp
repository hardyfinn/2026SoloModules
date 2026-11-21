#include "plugin.hpp"
#include "VinylDSP.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COLX0 = 13.f, COLDX = 19.f;
static const float SW_Y = 33.f, K1_Y = 50.f, K2_Y = 66.f, K3_Y = 82.f, ON_Y = 95.5f, CV_Y = 111.f;
static const float DISP_X = 197.f, DISP_Y = 13.5f, DISP_W = 53.f, DISP_H = 25.f;
static const float FMT_X = 205.f, DISC_X = 222.f, SPEED_X = 241.f, SW2_Y = 49.f;
static const float NEEDLE_X = 214.f, NEEDLE_Y = 70.f, PLAY_X = 241.f, PLAY_Y = 70.f;
static const float MIX_X = 208.f, OUT_X = 238.f, MIXOUT_Y = 88.5f;
static const float JR1_Y = 104.f, NCV_X = 211.f, PTRIG_X = 237.f;
static const float IO_Y = 117.f;
static const float IOX[4] = {205.f, 218.f, 231.f, 244.f};

static float colX(int i) { return COLX0 + COLDX * i; }

struct Format {
	const char* name;
	int disc, speed;
	float side, level;
	int medium;
};
static const Format FORMATS[5] = {
	{"7\" 45", 0, 1, 4.f, 3.f, 0},
	{"12\" 45", 2, 1, 7.f, 6.f, 0},
	{"EP 33", 2, 0, 12.f, 3.f, 0},
	{"LP 33", 2, 0, 20.f, 0.f, 0},
	{"DUB PLATE", 1, 1, 6.f, 6.f, 2},
};

struct VinylizerFable : Module {
	enum ParamId {
		ELL_FREQ, ELL_AMT, ELL_SLOPE,
		HFL_THRESH, HFL_FREQ, HFL_REL,
		CUT_LEVEL, CUT_SIDE, CUT_HEAD,
		MED_TYPE, MED_ECHO, MED_WEAR,
		TRC_STYLUS, TRC_AMT,
		TRK_ALIGN, TRK_FORCE, TRK_BAL,
		CART_MODEL, CART_LOAD, CART_SEP, CART_AZ,
		ARM_RES, ARM_DAMP, ARM_WARP,
		PLT_DRIVE, PLT_ECC, PLT_WOW, PLT_FLUT,
		PHO_TYPE, PHO_GAIN, PHO_TOL, PHO_SUB,
		ENUMS(ON_PARAM, 10),
		DISC_PARAM, SPEED_PARAM, FORMAT_PARAM, NEEDLE_PARAM, PLAY_PARAM, MIX_PARAM, OUT_PARAM,
		PARAMS_LEN
	};
	enum InputId { IN_L, IN_R, NEEDLE_INPUT, PLAY_INPUT, ENUMS(CV_INPUT, 10), INPUTS_LEN };
	enum OutputId { OUT_L, OUT_R, OUTPUTS_LEN };
	enum LightId { ENUMS(ON_LIGHT, 10), HFL_LIGHT, EXC_LIGHT, TRK_LIGHT, PHO_LIGHT, PLAY_LIGHT, LIGHTS_LEN };

	vinyl::Engine engine;
	dsp::ClockDivider div;
	dsp::SchmittTrigger playTrig, playBtn, fmtBtn;
	bool playing = false, loopSide = false;
	int formatIndex = 3;

	// CV targets: one per process column
	const int cvTarget[10] = {ELL_AMT, HFL_THRESH, CUT_LEVEL, MED_WEAR, TRC_AMT, TRK_FORCE, CART_LOAD, ARM_WARP, PLT_WOW, PHO_GAIN};

	VinylizerFable() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(ELL_FREQ, 40.f, 300.f, 150.f, "Elliptic crossover", " Hz");
		configParam(ELL_AMT, 0.f, 1.f, 1.f, "Elliptic amount (bass stereo removed)", "%", 0.f, 100.f);
		configSwitch(ELL_SLOPE, 0.f, 2.f, 1.f, "Elliptic slope", {"6 dB/oct", "12 dB/oct", "24 dB/oct"});
		configParam(HFL_THRESH, -24.f, 12.f, 0.f, "HF limiter threshold", " dB");
		configParam(HFL_FREQ, 3000.f, 12000.f, 6000.f, "HF limiter band starts at", " Hz");
		configParam(HFL_REL, 0.005f, 0.3f, 0.05f, "HF limiter release", " ms", 0.f, 1000.f);
		configParam(CUT_LEVEL, -12.f, 9.f, 0.f, "Cut level (how hot the groove is)", " dB");
		configParam(CUT_SIDE, 4.f, 25.f, 20.f, "Side length", " min");
		configParam(CUT_HEAD, 0.f, 1.f, 0.5f, "Cutter head (clean to classic)", "%", 0.f, 100.f);
		configSwitch(MED_TYPE, 0.f, 2.f, 0.f, "Medium", {"Lacquer pressing", "DMM pressing", "Acetate dub plate"});
		configParam(MED_ECHO, 0.f, 1.f, 0.3f, "Groove echo", "%", 0.f, 100.f);
		configParam(MED_WEAR, 0.f, 100.f, 0.f, "Wear", " plays");
		configSwitch(TRC_STYLUS, 0.f, 2.f, 0.f, "Stylus", {"Spherical 0.7 mil", "Elliptical 0.3 x 0.7 mil", "Fine line"});
		configParam(TRC_AMT, 0.f, 2.f, 1.f, "Tracing distortion", "%", 0.f, 100.f);
		configSwitch(TRK_ALIGN, 0.f, 1.f, 0.f, "Arm alignment", {"Baerwald", "Stevenson"});
		configParam(TRK_FORCE, 0.75f, 5.f, 2.5f, "Tracking force", " g");
		configParam(TRK_BAL, -1.f, 1.f, 0.f, "Skating balance (left / right wall force)", "%", 0.f, 100.f);
		configSwitch(CART_MODEL, 0.f, 2.f, 0.f, "Cartridge", {"DJ moving magnet", "Hi-fi moving magnet", "Moving coil"});
		configParam(CART_LOAD, 50.f, 600.f, 200.f, "Load capacitance (cable + input)", " pF");
		configParam(CART_SEP, 10.f, 40.f, 25.f, "Channel separation", " dB");
		configParam(CART_AZ, -1.f, 1.f, 0.f, "Azimuth error", "%", 0.f, 100.f);
		configParam(ARM_RES, 5.f, 20.f, 10.f, "Arm resonance", " Hz");
		configParam(ARM_DAMP, 0.f, 1.f, 0.5f, "Arm damping", "%", 0.f, 100.f);
		configParam(ARM_WARP, 0.f, 1.5f, 0.f, "Warp", " mm");
		configSwitch(PLT_DRIVE, 0.f, 2.f, 0.f, "Drive", {"Direct drive", "Belt drive", "Cheap deck"});
		configParam(PLT_ECC, 0.f, 1.f, 0.15f, "Off-centre hole", " mm");
		configParam(PLT_WOW, 0.f, 1.f, 0.2f, "Wow", "%", 0.f, 100.f);
		configParam(PLT_FLUT, 0.f, 1.f, 0.2f, "Flutter", "%", 0.f, 100.f);
		configSwitch(PHO_TYPE, 0.f, 2.f, 1.f, "Phono stage", {"Tube", "Op-amp", "DJ mixer"});
		configParam(PHO_GAIN, -6.f, 24.f, 0.f, "Phono input drive", " dB");
		configParam(PHO_TOL, -1.f, 1.f, 0.f, "RIAA part tolerance (bass tilt)", "%", 0.f, 5.f);
		configSwitch(PHO_SUB, 0.f, 1.f, 1.f, "Subsonic filter", {"Off", "On"});
		const char* names[10] = {"Elliptic", "HF limit", "Cut", "Medium", "Tracing", "Tracking", "Cartridge", "Arm", "Platter", "Phono"};
		for (int i = 0; i < 10; i++) {
			configSwitch(ON_PARAM + i, 0.f, 1.f, 1.f, std::string(names[i]) + " on", {"Bypassed", "On"});
			configInput(CV_INPUT + i, std::string(names[i]) + " CV (" + paramQuantities[cvTarget[i]]->name + ")");
		}
		configSwitch(DISC_PARAM, 0.f, 2.f, 2.f, "Disc", {"7 inch", "10 inch", "12 inch"});
		configSwitch(SPEED_PARAM, 0.f, 1.f, 0.f, "Speed", {"33 1/3 rpm", "45 rpm"});
		configButton(FORMAT_PARAM, "Next format preset");
		configParam(NEEDLE_PARAM, 0.f, 1.f, 0.f, "Needle position (edge to end of side)", "%", 0.f, 100.f);
		configButton(PLAY_PARAM, "Play side (needle travels across the side in real time)");
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix", "%", 0.f, 100.f);
		configParam(OUT_PARAM, 0.f, 2.f, 1.f, "Output level", " dB", -10.f, 20.f);
		configInput(IN_L, "Left / mono");
		configInput(IN_R, "Right");
		configInput(NEEDLE_INPUT, "Needle position CV (0-10 V)");
		configInput(PLAY_INPUT, "Play side trigger");
		configOutput(OUT_L, "Left");
		configOutput(OUT_R, "Right");
		configBypass(IN_L, OUT_L);
		configBypass(IN_R, OUT_R);
		div.setDivision(32);
		engine.init(APP ? APP->engine->getSampleRate() : 48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.init(e.sampleRate); }
	void onReset() override { playing = false; formatIndex = 3; engine.init(APP->engine->getSampleRate()); }

	float val(int p, int cvIndex = -1) {
		float v = params[p].getValue();
		if (cvIndex >= 0 && inputs[CV_INPUT + cvIndex].isConnected()) {
			ParamQuantity* q = paramQuantities[p];
			v += inputs[CV_INPUT + cvIndex].getVoltage() / 10.f * (q->getMaxValue() - q->getMinValue());
			v = clamp(v, q->getMinValue(), q->getMaxValue());
		}
		return v;
	}

	void applyFormat(int i) {
		formatIndex = i;
		const Format& f = FORMATS[i];
		params[DISC_PARAM].setValue(f.disc);
		params[SPEED_PARAM].setValue(f.speed);
		params[CUT_SIDE].setValue(f.side);
		params[CUT_LEVEL].setValue(f.level);
		params[MED_TYPE].setValue(f.medium);
		params[NEEDLE_PARAM].setValue(0.f);
	}

	bool formatMatches() {
		const Format& f = FORMATS[formatIndex];
		return (int) params[DISC_PARAM].getValue() == f.disc && (int) params[SPEED_PARAM].getValue() == f.speed
			&& std::fabs(params[CUT_SIDE].getValue() - f.side) < 0.05f && std::fabs(params[CUT_LEVEL].getValue() - f.level) < 0.05f
			&& (int) params[MED_TYPE].getValue() == f.medium;
	}

	void readSettings() {
		vinyl::Settings& s = engine.s;
		auto on = [&](int i) { return params[ON_PARAM + i].getValue() > 0.5f; };
		s.ellOn = on(0); s.ellFreq = val(ELL_FREQ); s.ellAmount = val(ELL_AMT, 0); s.ellSlope = (int) val(ELL_SLOPE);
		s.hflOn = on(1); s.hflThreshDb = val(HFL_THRESH, 1); s.hflFreq = val(HFL_FREQ); s.hflRelease = val(HFL_REL);
		s.cutOn = on(2); s.cutLevelDb = val(CUT_LEVEL, 2); s.sideMin = val(CUT_SIDE); s.head = val(CUT_HEAD);
		s.medOn = on(3); s.medType = (int) val(MED_TYPE); s.echo = val(MED_ECHO); s.wear = val(MED_WEAR, 3);
		s.trcOn = on(4); s.stylus = (int) val(TRC_STYLUS); s.trcAmount = val(TRC_AMT, 4);
		s.trkOn = on(5); s.align = (int) val(TRK_ALIGN); s.force = val(TRK_FORCE, 5); s.balance = val(TRK_BAL);
		s.cartOn = on(6); s.cartModel = (int) val(CART_MODEL); s.loadPf = val(CART_LOAD, 6); s.sepDb = val(CART_SEP); s.azimuth = val(CART_AZ);
		s.armOn = on(7); s.armRes = val(ARM_RES); s.armDamp = val(ARM_DAMP); s.warpMm = val(ARM_WARP, 7);
		s.pltOn = on(8); s.drive = (int) val(PLT_DRIVE); s.eccMm = val(PLT_ECC); s.wow = val(PLT_WOW, 8); s.flutter = val(PLT_FLUT);
		s.phoOn = on(9); s.phoType = (int) val(PHO_TYPE); s.phoGainDb = val(PHO_GAIN, 9); s.tol = val(PHO_TOL); s.subsonic = val(PHO_SUB) > 0.5f;
		s.disc = (int) params[DISC_PARAM].getValue();
		s.speed = (int) params[SPEED_PARAM].getValue();
		float n = params[NEEDLE_PARAM].getValue();
		if (inputs[NEEDLE_INPUT].isConnected()) n += inputs[NEEDLE_INPUT].getVoltage() / 10.f;
		s.needle = clamp(n, 0.f, 1.f);
	}

	void process(const ProcessArgs& args) override {
		if (fmtBtn.process(params[FORMAT_PARAM].getValue() > 0.5f)) applyFormat((formatIndex + 1) % 5);
		bool trig = playBtn.process(params[PLAY_PARAM].getValue() > 0.5f) | playTrig.process(inputs[PLAY_INPUT].getVoltage(), 0.1f, 1.f);
		if (trig) {
			playing = !playing;
			if (playing && params[NEEDLE_PARAM].getValue() >= 0.999f) params[NEEDLE_PARAM].setValue(0.f);
		}
		if (div.process()) {
			if (playing) {
				float side = std::max(params[CUT_SIDE].getValue(), 1.f) * 60.f;
				float n = params[NEEDLE_PARAM].getValue() + 32.f * args.sampleTime / side;
				if (n >= 1.f) {
					if (loopSide) n = 0.f;
					else { n = 1.f; playing = false; }
				}
				params[NEEDLE_PARAM].setValue(n);
			}
			readSettings();
			float dt = 32.f * args.sampleTime;
			for (int i = 0; i < 10; i++) lights[ON_LIGHT + i].setBrightness(params[ON_PARAM + i].getValue());
			lights[HFL_LIGHT].setBrightnessSmooth(clamp(-engine.m.hflGR / 6.f, 0.f, 1.f), dt);
			lights[EXC_LIGHT].setBrightnessSmooth(clamp(-engine.m.excGR / 6.f, 0.f, 1.f), dt);
			lights[TRK_LIGHT].setBrightnessSmooth(engine.m.mistrack, dt);
			lights[PHO_LIGHT].setBrightnessSmooth(engine.m.overload, dt);
			lights[PLAY_LIGHT].setBrightness(playing ? 1.f : 0.f);
		}
		float inL = inputs[IN_L].getVoltage();
		float inR = inputs[IN_R].isConnected() ? inputs[IN_R].getVoltage() : inL;
		float wl, wr, dl, dr;
		engine.process(inL, inR, wl, wr, dl, dr);
		float mix = params[MIX_PARAM].getValue(), g = params[OUT_PARAM].getValue();
		outputs[OUT_L].setVoltage(clamp((dl + (wl - dl) * mix) * g, -12.f, 12.f));
		outputs[OUT_R].setVoltage(clamp((dr + (wr - dr) * mix) * g, -12.f, 12.f));
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "format", json_integer(formatIndex));
		json_object_set_new(r, "truePreEcho", json_boolean(engine.s.truePreEcho));
		json_object_set_new(r, "loopSide", json_boolean(loopSide));
		json_object_set_new(r, "playing", json_boolean(playing));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "format")) formatIndex = clamp((int) json_integer_value(j), 0, 4);
		if (json_t* j = json_object_get(r, "truePreEcho")) engine.s.truePreEcho = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "loopSide")) loopSide = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "playing")) playing = json_boolean_value(j);
	}
};

struct VinylDisplay : Widget {
	VinylizerFable* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		NVGcontext* vg = args.vg;
		float w = box.size.x, h = box.size.y;
		nvgBeginPath(vg); nvgRoundedRect(vg, 0, 0, w, h, 3); nvgFillColor(vg, nvgRGB(0x0B, 0x0A, 0x0D)); nvgFill(vg);

		vinyl::Meters m;
		const char* fmt = "LP 33";
		bool custom = false, playing = false;
		if (module) {
			m = module->engine.m;
			fmt = FORMATS[module->formatIndex].name;
			custom = !module->formatMatches();
			playing = module->playing;
		}
		else { m.radius = 0.11f; m.grooveSpeed = 0.38f; m.rEnd = 0.07f; }

		// the disc from above: outer edge, end of the music, label, and the stylus
		float cx = h * 0.5f + 2, cy = h * 0.5f, R = h * 0.5f - 3;
		float sc = R / 0.150f;
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, m.rOut * sc); nvgFillColor(vg, nvgRGB(0x24, 0x22, 0x28)); nvgFill(vg);
		for (float rr = m.rEnd; rr < m.rOut; rr += (m.rOut - m.rEnd) / 7.f) {
			nvgBeginPath(vg); nvgCircle(vg, cx, cy, rr * sc);
			nvgStrokeColor(vg, nvgRGBA(0x90, 0x8A, 0x9A, 0x50)); nvgStrokeWidth(vg, 0.5f); nvgStroke(vg);
		}
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, m.rMin * sc * 0.85f); nvgFillColor(vg, nvgRGB(0xD9, 0x8B, 0x4A)); nvgFill(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, 1.0f); nvgFillColor(vg, nvgRGB(0x0B, 0x0A, 0x0D)); nvgFill(vg);
		float sx = cx + m.radius * sc * 0.7071f, sy = cy - m.radius * sc * 0.7071f;
		nvgBeginPath(vg); nvgMoveTo(vg, w * 0.47f, 2); nvgLineTo(vg, sx, sy);
		nvgStrokeColor(vg, nvgRGB(0xF2, 0xEA, 0xDA)); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
		nvgBeginPath(vg); nvgCircle(vg, sx, sy, 1.6f); nvgFillColor(vg, nvgRGB(0xF2, 0xEA, 0xDA)); nvgFill(vg);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(vg, font->handle);
		float tx = h + 4;
		nvgFontSize(vg, 11);
		nvgFillColor(vg, nvgRGB(0xD9, 0x8B, 0x4A));
		nvgText(vg, tx, 11, custom ? "CUSTOM" : fmt, NULL);
		nvgFontSize(vg, 8.5f);
		nvgFillColor(vg, nvgRGB(0xF2, 0xEA, 0xDA));
		char buf[64];
		snprintf(buf, sizeof(buf), "r %3.0f mm  %s", m.radius * 1000.f, playing ? "PLAY" : "");
		nvgText(vg, tx, 22, buf, NULL);
		snprintf(buf, sizeof(buf), "groove %.2f m/s", m.grooveSpeed);
		nvgText(vg, tx, 31, buf, NULL);
		snprintf(buf, sizeof(buf), "HF %5.1f  EXC %4.1f", m.hflGR, m.excGR);
		nvgText(vg, tx, 40, buf, NULL);
		snprintf(buf, sizeof(buf), "track %+.1f deg", m.trackErrDeg);
		nvgText(vg, tx, 49, buf, NULL);
		snprintf(buf, sizeof(buf), "pitch %.3f mm", m.pitch * 1000.f);
		nvgText(vg, tx, 58, buf, NULL);
		nvgFontSize(vg, 7.5f);
		nvgFillColor(vg, nvgRGBA(0xF2, 0xEA, 0xDA, 0x90));
		snprintf(buf, sizeof(buf), "side cut %+.1f dB", m.sideDb);
		nvgText(vg, tx, 67, buf, NULL);
		Widget::drawLayer(args, layer);
	}
};

struct VinylizerFableWidget : ModuleWidget {
	VinylizerFableWidget(VinylizerFable* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VinylizerFable.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		using M = VinylizerFable;
		auto knob = [&](int col, float y, int id) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(col), y)), module, id));
		};
		auto sw3 = [&](int col, int id) {
			addParam(createParamCentered<CKSSThree>(mm2px(Vec(colX(col) - 4.f, SW_Y)), module, id));
		};
		auto sw2 = [&](int col, float y, int id) {
			addParam(createParamCentered<CKSS>(mm2px(Vec(colX(col) - 4.f, y)), module, id));
		};
		// 1 ELLIPTIC
		sw3(0, M::ELL_SLOPE); knob(0, K1_Y, M::ELL_FREQ); knob(0, K2_Y, M::ELL_AMT);
		// 2 HF LIMIT
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(colX(1), SW_Y + 1.f)), module, M::HFL_LIGHT));
		knob(1, K1_Y, M::HFL_THRESH); knob(1, K2_Y, M::HFL_FREQ); knob(1, K3_Y, M::HFL_REL);
		// 3 CUT
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(colX(2), SW_Y + 1.f)), module, M::EXC_LIGHT));
		knob(2, K1_Y, M::CUT_LEVEL); knob(2, K2_Y, M::CUT_SIDE); knob(2, K3_Y, M::CUT_HEAD);
		// 4 MEDIUM
		sw3(3, M::MED_TYPE); knob(3, K1_Y, M::MED_ECHO); knob(3, K2_Y, M::MED_WEAR);
		// 5 TRACING
		sw3(4, M::TRC_STYLUS); knob(4, K1_Y, M::TRC_AMT);
		// 6 TRACKING
		sw2(5, SW_Y, M::TRK_ALIGN);
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(colX(5), K3_Y)), module, M::TRK_LIGHT));
		knob(5, K1_Y, M::TRK_FORCE); knob(5, K2_Y, M::TRK_BAL);
		// 7 CARTRIDGE
		sw3(6, M::CART_MODEL); knob(6, K1_Y, M::CART_LOAD); knob(6, K2_Y, M::CART_SEP); knob(6, K3_Y, M::CART_AZ);
		// 8 ARM
		knob(7, K1_Y, M::ARM_RES); knob(7, K2_Y, M::ARM_DAMP); knob(7, K3_Y, M::ARM_WARP);
		// 9 PLATTER
		sw3(8, M::PLT_DRIVE); knob(8, K1_Y, M::PLT_ECC); knob(8, K2_Y, M::PLT_WOW); knob(8, K3_Y, M::PLT_FLUT);
		// 10 PHONO
		sw3(9, M::PHO_TYPE);
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(colX(9) + 5.8f, K3_Y + 4.f)), module, M::PHO_LIGHT));
		knob(9, K1_Y, M::PHO_GAIN); knob(9, K2_Y, M::PHO_TOL); sw2(9, K3_Y, M::PHO_SUB);

		for (int i = 0; i < 10; i++) {
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(colX(i), ON_Y)), module, M::ON_PARAM + i, M::ON_LIGHT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), CV_Y)), module, M::CV_INPUT + i));
		}

		VinylDisplay* d = createWidget<VinylDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addParam(createParamCentered<VCVBezel>(mm2px(Vec(FMT_X, SW2_Y)), module, M::FORMAT_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(DISC_X, SW2_Y)), module, M::DISC_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(SPEED_X, SW2_Y)), module, M::SPEED_PARAM));
		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(NEEDLE_X, NEEDLE_Y)), module, M::NEEDLE_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(PLAY_X, PLAY_Y)), module, M::PLAY_PARAM, M::PLAY_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(MIX_X, MIXOUT_Y)), module, M::MIX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(OUT_X, MIXOUT_Y)), module, M::OUT_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(NCV_X, JR1_Y)), module, M::NEEDLE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(PTRIG_X, JR1_Y)), module, M::PLAY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[0], IO_Y)), module, M::IN_L));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IOX[1], IO_Y)), module, M::IN_R));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[2], IO_Y)), module, M::OUT_L));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IOX[3], IO_Y)), module, M::OUT_R));
	}

	void appendContextMenu(Menu* menu) override {
		VinylizerFable* m = getModule<VinylizerFable>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Format preset", FORMATS[m->formatIndex].name, [=](Menu* sub) {
			for (int i = 0; i < 5; i++)
				sub->addChild(createCheckMenuItem(FORMATS[i].name, "", [=]() { return m->formatIndex == i; }, [=]() { m->applyFormat(i); }));
		}));
		menu->addChild(createBoolPtrMenuItem("True pre-echo (delays the output by one turn)", "", &m->engine.s.truePreEcho));
		menu->addChild(createBoolPtrMenuItem("PLAY SIDE loops back to the edge", "", &m->loopSide));
	}
};

Model* modelVinylizerFable = createModel<VinylizerFable, VinylizerFableWidget>("VinylizerFable");
