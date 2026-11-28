// Seal Legs: stereo character delay. Two delay lines, TAPE / BBD / DIGI models, MOD, loop filters,
// drive, noise, plate fog, freeze, plus reverse, half / double speed, tape stop, duck,
// glide or jump time changes and MUTATE + LOCK.
// Spec: vcv-modules/seal-legs.md. All sound is in SealLegsDSP.hpp; this file is Rack glue.
#include "plugin.hpp"
#include "SealLegsDSP.hpp"

// ---- LAYOUT (mm, same numbers in res/make_panel.py) ----
static const float SX[8] = {9.5f, 21.0f, 32.5f, 44.0f, 55.5f, 67.0f, 78.5f, 90.0f};
static const float SLIDER_Y = 43.0f, TRIM_Y = 63.0f, CV_Y = 73.5f;
static const float J1_Y = 96.0f, J2_Y = 114.0f;
static const float RX[6] = {107.5f, 123.0f, 138.5f, 154.0f, 169.5f, 185.0f};
static const float R1_Y = 28.0f, R2_Y = 47.5f, R3_Y = 66.0f, R4_Y = 84.5f, R5_Y = 103.0f;
static const float SW_DX = -2.6f;   // 3-position switches sit left of their column, labels to the right

namespace sl = seallegs;

struct SealLegs;

struct TimeQuantity : ParamQuantity {
	bool isWidth = false;
	std::string getDisplayValueString() override;
};
struct ShapeQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float k = getValue();
		int n = (int)std::round(k);
		if (std::fabs(k - n) < 0.1f || k < 3.f) {
			int i = k < 0.5f ? 0 : (k < 1.5f ? 1 : (k < 2.5f ? 2 : n));
			return sl::MOD_SHAPE_NAMES[clamp(i, 0, 9)];
		}
		int a = (int)std::floor(k);
		return string::f("%s > %s", sl::MOD_SHAPE_NAMES[clamp(a, 0, 9)], sl::MOD_SHAPE_NAMES[clamp(a + 1, 0, 9)]);
	}
};
struct RateQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return string::f("%.2f Hz", 0.1 * std::pow(400.0, (double)getValue()));
	}
};
struct HzQuantity : ParamQuantity {
	bool isLp = false;
	std::string getDisplayValueString() override {
		float s = getValue();
		if (!isLp && s < 0.01f) return "Off";
		if (isLp && s > 0.99f) return "Off";
		double hz = isLp ? 80.0 * std::pow(250.0, (double)s) : 10.0 * std::pow(800.0, (double)s);
		return hz >= 1000 ? string::f("%.2f kHz", hz / 1000) : string::f("%.0f Hz", hz);
	}
};

// 3-position switch helpers: labels are given top to bottom; value 0 is the bottom position.
static int topIndex(float v) { return 2 - clamp((int)std::round(v), 0, 2); }

struct SealLegs : Module {
	enum ParamId {
		DRIVE_PARAM, NOISE_PARAM, NOISE_MODE_PARAM, FOG_PARAM, FOG_DECAY_PARAM, MIX_PARAM, DUCK_PARAM,
		TIME_PARAM, WIDTH_PARAM, FB_PARAM, MOD_RATE_PARAM, MOD_DEPTH_PARAM, COLOR_PARAM, HP_PARAM, LP_PARAM,
		ENUMS(ATT_PARAM, 8), AUX_ATT_PARAM, MOD_SHAPE_PARAM,
		RANGE_PARAM, STEREO_PARAM, MODEL_PARAM, FOG_POS_PARAM, DRIVE_POS_PARAM, AUX_DEST_PARAM,
		REVERSE_PARAM, SPEED_PARAM, CHANGE_PARAM, TAPE_STOP_PARAM,
		FREEZE_PARAM, DSYNC_PARAM, MSYNC_PARAM, FBX_PARAM, MPHASE_PARAM, TAP_PARAM, CLEAR_PARAM,
		MUTATE_PARAM, MUT_PARAM, LOCK_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		IN_L_INPUT, IN_R_INPUT, FREEZE_INPUT, CLK_INPUT, TAP_INPUT, CLEAR_INPUT, REV_INPUT, MUT_INPUT, AUX_INPUT,
		ENUMS(CV_INPUT, 8),
		INPUTS_LEN
	};
	enum OutputId { OUT_L_OUTPUT, OUT_R_OUTPUT, MOD_OUTPUT, OUTPUTS_LEN };
	enum LightId {
		FREEZE_LIGHT, DSYNC_LIGHT, MSYNC_LIGHT, FBX_LIGHT, MPHASE_LIGHT, TAP_LIGHT, LOCK_LIGHT,
		ENUMS(DRIVE_LIGHT, 2), ENUMS(MOD_LIGHT, 2),
		LIGHTS_LEN
	};
	// slider order for the attenuverters and CV jacks
	static constexpr int SLIDERS[8] = {TIME_PARAM, WIDTH_PARAM, FB_PARAM, MOD_RATE_PARAM, MOD_DEPTH_PARAM, COLOR_PARAM, HP_PARAM, LP_PARAM};

	sl::Engine engine;
	dsp::SchmittTrigger clkIn, tapIn, clearIn, mutIn, tapBtn, clearBtn, freezeIn, revIn;
	float tapFlash = 0.f;
	int lightDiv = 0;
	bool mutBtnPrev = false, mutCleared = false;
	float mutHeld = 0.f;

	// menu settings
	int trimSetting = 1;        // -6 dB, 0 dB, +8 dB
	bool sendReturn = false, clkQuarter = false, modOut10 = false, snapShape = false;

	// mutation offsets (spec 12)
	float colorMut = 0.f, rateMut = 0.f, depthMut = 0.f, hpMut = 0.f, lpMut = 0.f, noiseMut = 0.f, fogMut = 0.f;
	int shapeMut = -1, modelMut = -1, speedMut = -1;
	bool revMut = false;

	SealLegs() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.5f, "Drive (noon = unity)", "%", 0.f, 100.f);
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f);
		configParam(NOISE_MODE_PARAM, 0.f, 1.f, 0.f, "Noise mode (pre-delay hiss > crackle > post-delay hiss)", "%", 0.f, 100.f);
		configParam(FOG_PARAM, 0.f, 1.f, 0.f, "Fog (plate reverb)", "%", 0.f, 100.f);
		configParam(FOG_DECAY_PARAM, 0.f, 1.f, 0.5f, "Fog decay", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Mix", "%", 0.f, 100.f);
		configParam(DUCK_PARAM, 0.f, 1.f, 0.f, "Duck (input pushes the echoes down)", "%", 0.f, 100.f);
		configParam<TimeQuantity>(TIME_PARAM, 0.f, 1.f, 0.5f, "Time");
		configParam<TimeQuantity>(WIDTH_PARAM, 0.f, 1.f, 0.f, "Width (right-line offset, or right time in L/R)");
		dynamic_cast<TimeQuantity*>(paramQuantities[WIDTH_PARAM])->isWidth = true;
		configParam(FB_PARAM, 0.f, 1.f, 0.4f, "Feedback", "%", 0.f, 100.f);
		configParam<RateQuantity>(MOD_RATE_PARAM, 0.f, 1.f, 0.3f, "Mod rate");
		configParam(MOD_DEPTH_PARAM, 0.f, 1.f, 0.f, "Mod depth", "%", 0.f, 100.f);
		configParam(COLOR_PARAM, 0.f, 1.f, 0.f, "Color (model artefacts)", "%", 0.f, 100.f);
		configParam<HzQuantity>(HP_PARAM, 0.f, 1.f, 0.f, "Loop highpass");
		configParam<HzQuantity>(LP_PARAM, 0.f, 1.f, 1.f, "Loop lowpass");
		dynamic_cast<HzQuantity*>(paramQuantities[LP_PARAM])->isLp = true;
		const char* attNames[8] = {"Time", "Width", "Feedback", "Mod rate", "Mod depth", "Color", "Highpass", "Lowpass"};
		for (int i = 0; i < 8; i++) {
			configParam(ATT_PARAM + i, -1.f, 1.f, 0.f, string::f("%s CV amount", attNames[i]), "%", 0.f, 100.f);
			configInput(CV_INPUT + i, string::f("%s CV", attNames[i]));
		}
		configParam(AUX_ATT_PARAM, -1.f, 1.f, 0.f, "AUX CV amount", "%", 0.f, 100.f);
		configParam<ShapeQuantity>(MOD_SHAPE_PARAM, 0.f, 9.f, 0.f, "Mod shape");
		configSwitch(RANGE_PARAM, 0.f, 2.f, 1.f, "Range", {"Short (3-94 ms)", "Medium (50 ms-1.5 s)", "Long (0.2-6 s)"});
		configSwitch(STEREO_PARAM, 0.f, 2.f, 1.f, "Stereo", {"L/R (independent)", "T:W (parallel)", "Ping-pong"});
		configSwitch(MODEL_PARAM, 0.f, 2.f, 2.f, "Model", {"Digital", "Bucket brigade", "Tape"});
		configSwitch(FOG_POS_PARAM, 0.f, 3.f, 1.f, "Fog position", {"Input", "Delay", "Mix", "Loop (inside the feedback)"});
		configSwitch(DRIVE_POS_PARAM, 0.f, 2.f, 2.f, "Drive position", {"Mix (output)", "Delay (send only)", "Input"});
		configSwitch(AUX_DEST_PARAM, 0.f, 4.f, 4.f, "AUX destination", {"Drive", "Noise", "Mod shape", "Fog", "Mix"});
		configSwitch(REVERSE_PARAM, 0.f, 2.f, 2.f, "Reverse", {"Mix (left forward, right reversed)", "Reverse", "Off"});
		configSwitch(SPEED_PARAM, 0.f, 2.f, 1.f, "Speed", {"Half (octave down)", "Normal", "Double (octave up)"});
		configSwitch(CHANGE_PARAM, 0.f, 1.f, 1.f, "Time change", {"Jump (crossfade, no pitch)", "Glide (pitch bends)"});
		configSwitch(TAPE_STOP_PARAM, 0.f, 1.f, 0.f, "Tape stop on freeze (TAPE model)", {"Off", "On"});
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze", {"Off", "On"});
		configSwitch(DSYNC_PARAM, 0.f, 1.f, 0.f, "Delay sync", {"Off", "On"});
		configSwitch(MSYNC_PARAM, 0.f, 1.f, 0.f, "Mod sync", {"Off", "On"});
		configSwitch(FBX_PARAM, 0.f, 1.f, 0.f, "Cross feedback", {"Off", "On"});
		configSwitch(MPHASE_PARAM, 0.f, 1.f, 0.f, "Mod phase (right line inverted)", {"Off", "On"});
		configButton(TAP_PARAM, "Tap tempo");
		configButton(CLEAR_PARAM, "Clear buffers");
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.3f, "Mutate depth", "%", 0.f, 100.f);
		configButton(MUT_PARAM, "Mutate (hold 1 s to clear)");
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock mutation", {"Off", "On"});
		paramQuantities[TIME_PARAM]->randomizeEnabled = false;
		paramQuantities[WIDTH_PARAM]->randomizeEnabled = false;
		paramQuantities[FB_PARAM]->randomizeEnabled = false;
		paramQuantities[MIX_PARAM]->randomizeEnabled = false;
		paramQuantities[DRIVE_PARAM]->randomizeEnabled = false;

		configInput(IN_L_INPUT, "Left audio");
		configInput(IN_R_INPUT, "Right audio (normalled to left)");
		configInput(FREEZE_INPUT, "Freeze gate");
		configInput(CLK_INPUT, "Clock");
		configInput(TAP_INPUT, "Tap trigger");
		configInput(CLEAR_INPUT, "Clear trigger");
		configInput(REV_INPUT, "Reverse gate (inverts the switch while high)");
		configInput(MUT_INPUT, "Mutate trigger");
		configInput(AUX_INPUT, "AUX CV");
		configOutput(OUT_L_OUTPUT, "Left audio");
		configOutput(OUT_R_OUTPUT, "Right audio");
		configOutput(MOD_OUTPUT, "Modulator");
		configLight(DRIVE_LIGHT, "Drive (red = clipping)");
		configLight(MOD_LIGHT, "Modulator");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
	}

	int range() { return clamp((int)std::round(params[RANGE_PARAM].getValue()), 0, 2); }

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		double pc = engine.Pc;
		engine.setSampleRate(e.sampleRate);
		engine.Pc = pc;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		clearMutation();
		engine.Pc = 0.125;
	}

	// ---- mutation (spec 12) ----
	void mutate() {
		if (params[LOCK_PARAM].getValue() > 0.5f) return;
		float m = params[MUTATE_PARAM].getValue();
		auto bi = [] { return random::uniform() * 2.f - 1.f; };
		colorMut = bi() * 0.5f * m;
		rateMut = bi() * 0.3f * m;
		depthMut = bi() * 0.3f * m;
		shapeMut = random::uniform() < 0.5f * m ? (int)(random::u32() % 10) : -1;
		hpMut = random::uniform() * 0.35f * m;
		lpMut = -random::uniform() * 0.5f * m;
		noiseMut = random::uniform() * 0.3f * m;
		fogMut = bi() * 0.3f * m;
		modelMut = random::uniform() < 0.25f * m ? (int)(random::u32() % 3) : -1;
		revMut = random::uniform() < 0.2f * m;
		speedMut = random::uniform() < 0.1f * m ? (random::uniform() < 0.5f ? 0 : 2) : -1;
	}
	void clearMutation() {
		colorMut = rateMut = depthMut = hpMut = lpMut = noiseMut = fogMut = 0.f;
		shapeMut = modelMut = speedMut = -1;
		revMut = false;
	}

	float slider(int i) {
		int p = SLIDERS[i];
		return params[p].getValue() + params[ATT_PARAM + i].getValue() * inputs[CV_INPUT + i].getVoltage() / 10.f;
	}

	void process(const ProcessArgs& args) override {
		sl::Frame f;
		// audio in (polyphonic cables summed)
		f.inL = inputs[IN_L_INPUT].getVoltageSum();
		f.inR = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltageSum() : f.inL;
		const float trims[3] = {0.5012f, 1.f, 2.5119f};
		f.trim = trims[clamp(trimSetting, 0, 2)];

		// mutation triggers
		if (mutIn.process(inputs[MUT_INPUT].getVoltage(), 0.1f, 1.f)) mutate();
		bool mb = params[MUT_PARAM].getValue() > 0.5f;
		if (mb) {
			mutHeld += args.sampleTime;
			if (mutHeld >= 1.f && !mutCleared) { clearMutation(); mutCleared = true; }
		}
		else if (mutBtnPrev) {
			if (!mutCleared) mutate();
			mutHeld = 0.f;
			mutCleared = false;
		}
		mutBtnPrev = mb;

		// AUX
		float aux = params[AUX_ATT_PARAM].getValue() * inputs[AUX_INPUT].getVoltage() / 10.f;
		int auxDest = clamp((int)std::round(params[AUX_DEST_PARAM].getValue()), 0, 4);
		auto withAux = [&](float v, int dest) { return auxDest == dest ? v + aux : v; };

		f.drive = clamp(withAux(params[DRIVE_PARAM].getValue(), sl::AUX_DRIVE), 0.f, 1.f);
		f.noise = clamp(withAux(params[NOISE_PARAM].getValue(), sl::AUX_NOISE) + noiseMut, 0.f, 1.f);
		f.noiseMode = params[NOISE_MODE_PARAM].getValue();
		f.fog = clamp(withAux(params[FOG_PARAM].getValue(), sl::AUX_FOG) + fogMut, 0.f, 1.f);
		f.fogDecay = params[FOG_DECAY_PARAM].getValue();
		f.mix = clamp(withAux(params[MIX_PARAM].getValue(), sl::AUX_MIX), 0.f, 1.f);
		f.duck = params[DUCK_PARAM].getValue();

		f.time = clamp(slider(0), 0.f, 1.f);
		f.width = clamp(slider(1), 0.f, 1.f);
		f.fb = clamp(slider(2), 0.f, 1.f);
		f.modRate = clamp(slider(3) + rateMut, 0.f, 1.f);
		float depth = slider(4);
		f.modDepth = clamp(depth + depthMut, 0.f, depthMut > 0.f ? std::max(clamp(depth, 0.f, 1.f), 0.7f) : 1.f);
		f.color = clamp(slider(5) + colorMut, 0.f, 1.f);
		f.hp = clamp(slider(6) + hpMut, 0.f, 1.f);
		f.lp = clamp(slider(7) + lpMut, 0.f, 1.f);
		float k = params[MOD_SHAPE_PARAM].getValue();
		if (snapShape) k = std::round(k);
		if (shapeMut >= 0) k = (float)shapeMut;
		if (auxDest == sl::AUX_MOD) k += 9.f * aux;
		f.modShape = clamp(k, 0.f, 9.f);

		f.range = range();
		f.stereo = topIndex(params[STEREO_PARAM].getValue());
		f.model = modelMut >= 0 ? modelMut : topIndex(params[MODEL_PARAM].getValue());
		f.fogPos = clamp((int)std::round(params[FOG_POS_PARAM].getValue()), 0, 3);
		f.drivePos = topIndex(params[DRIVE_POS_PARAM].getValue());
		int rev = topIndex(params[REVERSE_PARAM].getValue());
		if (revMut) rev = rev == sl::REV_OFF ? sl::REV_ON : (rev == sl::REV_ON ? sl::REV_OFF : rev);
		revIn.process(inputs[REV_INPUT].getVoltage(), 0.1f, 1.f);
		if (revIn.isHigh()) rev = rev == sl::REV_OFF ? sl::REV_ON : sl::REV_OFF;
		f.reverse = rev;
		const double speeds[3] = {0.5, 1.0, 2.0};
		int sp = clamp((int)std::round(params[SPEED_PARAM].getValue()), 0, 2);
		if (speedMut >= 0) sp = speedMut;
		f.speed = speeds[sp];
		f.jump = params[CHANGE_PARAM].getValue() < 0.5f;
		f.tapeStop = params[TAPE_STOP_PARAM].getValue() > 0.5f;

		freezeIn.process(inputs[FREEZE_INPUT].getVoltage(), 0.1f, 1.f);
		f.freeze = params[FREEZE_PARAM].getValue() > 0.5f || (inputs[FREEZE_INPUT].isConnected() && freezeIn.isHigh());
		f.delaySync = params[DSYNC_PARAM].getValue() > 0.5f;
		f.modSync = params[MSYNC_PARAM].getValue() > 0.5f;
		f.fbCross = params[FBX_PARAM].getValue() > 0.5f;
		f.modPhase = params[MPHASE_PARAM].getValue() > 0.5f;
		bool tapT = tapIn.process(inputs[TAP_INPUT].getVoltage(), 0.1f, 1.f);
		bool tapB = tapBtn.process(params[TAP_PARAM].getValue());
		f.tap = tapT || tapB;
		bool clrT = clearIn.process(inputs[CLEAR_INPUT].getVoltage(), 0.1f, 1.f);
		bool clrB = clearBtn.process(params[CLEAR_PARAM].getValue());
		f.clear = clrT || clrB;
		f.clkConnected = inputs[CLK_INPUT].isConnected();
		f.clkEdge = clkIn.process(inputs[CLK_INPUT].getVoltage(), 0.1f, 1.f);
		f.clkQuarter = clkQuarter;
		f.sendReturn = sendReturn;
		f.modOut10 = modOut10;

		sl::Output o = engine.process(f);
		outputs[OUT_L_OUTPUT].setVoltage((float)o.l);
		outputs[OUT_R_OUTPUT].setVoltage((float)o.r);
		outputs[MOD_OUTPUT].setVoltage((float)o.mod);

		if (o.beat) tapFlash = 0.06f;
		if (++lightDiv >= 32) {
			float dt = args.sampleTime * lightDiv;
			lightDiv = 0;
			tapFlash = std::max(0.f, tapFlash - dt);
			lights[FREEZE_LIGHT].setBrightness(f.freeze ? 1.f : 0.f);
			lights[DSYNC_LIGHT].setBrightness(f.delaySync ? 1.f : 0.f);
			lights[MSYNC_LIGHT].setBrightness(f.modSync ? 1.f : 0.f);
			lights[FBX_LIGHT].setBrightness(f.fbCross ? 1.f : 0.f);
			lights[MPHASE_LIGHT].setBrightness(f.modPhase ? 1.f : 0.f);
			lights[TAP_LIGHT].setBrightness(tapFlash > 0.f ? 1.f : 0.f);
			lights[LOCK_LIGHT].setBrightness(params[LOCK_PARAM].getValue() > 0.5f ? 1.f : 0.f);
			lights[DRIVE_LIGHT + 0].setBrightnessSmooth((float)o.driveGreen, dt);
			lights[DRIVE_LIGHT + 1].setBrightnessSmooth((float)o.driveRed, dt);
			lights[MOD_LIGHT + 0].setBrightness((float)o.modPos);
			lights[MOD_LIGHT + 1].setBrightness((float)o.modNeg);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "trim", json_integer(trimSetting));
		json_object_set_new(root, "sendReturn", json_boolean(sendReturn));
		json_object_set_new(root, "clkQuarter", json_boolean(clkQuarter));
		json_object_set_new(root, "modOut10", json_boolean(modOut10));
		json_object_set_new(root, "snapShape", json_boolean(snapShape));
		json_object_set_new(root, "tempo16th", json_real(engine.Pc));
		json_t* mu = json_object();
		json_object_set_new(mu, "color", json_real(colorMut));
		json_object_set_new(mu, "rate", json_real(rateMut));
		json_object_set_new(mu, "depth", json_real(depthMut));
		json_object_set_new(mu, "hp", json_real(hpMut));
		json_object_set_new(mu, "lp", json_real(lpMut));
		json_object_set_new(mu, "noise", json_real(noiseMut));
		json_object_set_new(mu, "fog", json_real(fogMut));
		json_object_set_new(mu, "shape", json_integer(shapeMut));
		json_object_set_new(mu, "model", json_integer(modelMut));
		json_object_set_new(mu, "speed", json_integer(speedMut));
		json_object_set_new(mu, "rev", json_boolean(revMut));
		json_object_set_new(root, "mutation", mu);
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "trim")) trimSetting = clamp((int)json_integer_value(j), 0, 2);
		if (json_t* j = json_object_get(root, "sendReturn")) sendReturn = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "clkQuarter")) clkQuarter = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "modOut10")) modOut10 = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "snapShape")) snapShape = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "tempo16th")) engine.Pc = std::min(std::max((double)json_number_value(j), 0.002), 4.0);
		if (json_t* mu = json_object_get(root, "mutation")) {
			auto num = [&](const char* k, float& v) { if (json_t* j = json_object_get(mu, k)) v = (float)json_number_value(j); };
			auto in = [&](const char* k, int& v) { if (json_t* j = json_object_get(mu, k)) v = (int)json_integer_value(j); };
			num("color", colorMut); num("rate", rateMut); num("depth", depthMut); num("hp", hpMut); num("lp", lpMut);
			num("noise", noiseMut); num("fog", fogMut);
			in("shape", shapeMut); in("model", modelMut); in("speed", speedMut);
			shapeMut = clamp(shapeMut, -1, 9); modelMut = clamp(modelMut, -1, 2);
			speedMut = speedMut == 0 || speedMut == 2 ? speedMut : -1;
			if (json_t* j = json_object_get(mu, "rev")) revMut = json_boolean_value(j);
		}
	}
};

constexpr int SealLegs::SLIDERS[8];

std::string TimeQuantity::getDisplayValueString() {
	SealLegs* m = dynamic_cast<SealLegs*>(module);
	int rg = m ? m->range() : sl::MED;
	int st = m ? topIndex(m->params[SealLegs::STEREO_PARAM].getValue()) : sl::TW;
	bool sync = m && m->params[SealLegs::DSYNC_PARAM].getValue() > 0.5f;
	double s = getValue();
	if (isWidth && st != sl::LR) return string::f("+%.1f %%", 33.0 * s);
	if (sync) {
		static const char* names[3][7] = {
			{"1/64", "1/32T", "1/32D", "1/32", "1/16T", "1/16D", "1/16"},
			{"1/16", "1/8T", "1/8D", "1/8", "1/4T", "1/4D", "1/4"},
			{"1/4", "1/2T", "1/2D", "1/2", "1T", "1D", "1 bar"},
		};
		return names[rg][(int)std::round(s * 6)];
	}
	double d = sl::timeFree(rg, s);
	return d < 1 ? string::f("%.1f ms", d * 1000) : string::f("%.3f s", d);
}

// ---- widget ------------------------------------------------------------------

struct SealLegsWidget : ModuleWidget {
	SealLegsWidget(SealLegs* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SealLegs.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// sliders, attenuverters, CV jacks
		for (int i = 0; i < 8; i++) {
			addParam(createParamCentered<VCVSlider>(mm2px(Vec(SX[i], SLIDER_Y)), module, SealLegs::SLIDERS[i]));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(SX[i], TRIM_Y)), module, SealLegs::ATT_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(SX[i], CV_Y)), module, SealLegs::CV_INPUT + i));
		}
		// jack row 1
		const int j1[8] = {SealLegs::IN_L_INPUT, SealLegs::IN_R_INPUT, SealLegs::FREEZE_INPUT, SealLegs::REV_INPUT,
			SealLegs::CLK_INPUT, SealLegs::TAP_INPUT, SealLegs::CLEAR_INPUT, SealLegs::MUT_INPUT};
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(SX[i], J1_Y)), module, j1[i]));
		// jack row 2: AUX group and outputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(SX[0], J2_Y)), module, SealLegs::AUX_INPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(SX[1], J2_Y)), module, SealLegs::AUX_ATT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(SX[2] + 5.0f, J2_Y)), module, SealLegs::AUX_DEST_PARAM));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(SX[5], J2_Y)), module, SealLegs::MOD_OUTPUT));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(SX[5] - 5.2f, J2_Y - 5.4f)), module, SealLegs::MOD_LIGHT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(SX[6], J2_Y)), module, SealLegs::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(SX[7], J2_Y)), module, SealLegs::OUT_R_OUTPUT));

		// R1: switches
		const int r1[5] = {SealLegs::MODEL_PARAM, SealLegs::RANGE_PARAM, SealLegs::STEREO_PARAM, SealLegs::SPEED_PARAM, SealLegs::REVERSE_PARAM};
		for (int i = 0; i < 5; i++) addParam(createParamCentered<CKSSThree>(mm2px(Vec(RX[i] + SW_DX, R1_Y)), module, r1[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(RX[5] + SW_DX, R1_Y)), module, SealLegs::CHANGE_PARAM));
		// R2: knobs
		const int r2[6] = {SealLegs::DRIVE_PARAM, SealLegs::NOISE_PARAM, SealLegs::NOISE_MODE_PARAM, SealLegs::FOG_PARAM, SealLegs::FOG_DECAY_PARAM, SealLegs::MIX_PARAM};
		for (int i = 0; i < 6; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RX[i], R2_Y)), module, r2[i]));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(RX[0] + 5.4f, R2_Y + 5.4f)), module, SealLegs::DRIVE_LIGHT));
		// R3
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(RX[0] + SW_DX, R3_Y)), module, SealLegs::DRIVE_POS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(RX[1], R3_Y)), module, SealLegs::FOG_POS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RX[2], R3_Y)), module, SealLegs::DUCK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RX[3], R3_Y)), module, SealLegs::MOD_SHAPE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(RX[4] + SW_DX, R3_Y)), module, SealLegs::TAPE_STOP_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(RX[5], R3_Y)), module, SealLegs::CLEAR_PARAM));
		// R4: latching buttons and tap
		const int r4[5] = {SealLegs::FREEZE_PARAM, SealLegs::DSYNC_PARAM, SealLegs::MSYNC_PARAM, SealLegs::FBX_PARAM, SealLegs::MPHASE_PARAM};
		const int r4l[5] = {SealLegs::FREEZE_LIGHT, SealLegs::DSYNC_LIGHT, SealLegs::MSYNC_LIGHT, SealLegs::FBX_LIGHT, SealLegs::MPHASE_LIGHT};
		for (int i = 0; i < 5; i++)
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(RX[i], R4_Y)), module, r4[i], r4l[i]));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(RX[5], R4_Y)), module, SealLegs::TAP_PARAM, SealLegs::TAP_LIGHT));
		// R5: mutation
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RX[2], R5_Y)), module, SealLegs::MUTATE_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(RX[3], R5_Y)), module, SealLegs::MUT_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(RX[4] + SW_DX, R5_Y)), module, SealLegs::LOCK_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(RX[4] + 2.2f, R5_Y + 3.0f)), module, SealLegs::LOCK_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		SealLegs* m = getModule<SealLegs>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Input trim", {"-6 dB", "0 dB", "+8 dB"}, &m->trimSetting));
		menu->addChild(createBoolPtrMenuItem("Send / return (no dry, MIX = wet level)", "", &m->sendReturn));
		menu->addChild(createIndexSubmenuItem("CLK input is", {"16th notes", "Quarter notes"},
			[=]() { return (size_t)(m->clkQuarter ? 1 : 0); }, [=](size_t i) { m->clkQuarter = i == 1; }));
		menu->addChild(createIndexSubmenuItem("MOD OUT range", {"±5 V", "±10 V"},
			[=]() { return (size_t)(m->modOut10 ? 1 : 0); }, [=](size_t i) { m->modOut10 = i == 1; }));
		menu->addChild(createBoolPtrMenuItem("Snap MOD shape", "", &m->snapShape));
		menu->addChild(createMenuItem("Clear mutation", "", [=]() { m->clearMutation(); }));
		menu->addChild(createMenuLabel(string::f("Tempo: %.1f BPM", 60.0 / (4.0 * m->engine.Pc))));
	}
};

Model* modelSealLegs = createModel<SealLegs, SealLegsWidget>("SealLegs");
