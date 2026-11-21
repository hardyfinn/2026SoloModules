// Cymbolism Fable: a physical cymbal for VCV Rack.
// The physics is in CyModes (geometry -> shell modes and couplings), CyEngine (the
// nonlinear modal shell, beaters, rivets, pickup) and CyRunner (engine thread and
// background rebuilds). This file is the panel, the jacks and the screen.
#include "plugin.hpp"
#include "CyRunner.hpp"

using namespace cy;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 14.0f, DISP_W = 98.0f, DISP_H = 42.0f;
static const float TYPE_X = 116.0f, TYPE_Y = 27.0f;
static const float NEW_X = 116.0f, NEW_Y = 48.0f;
static const float POS_X = 142.0f, FORCE_X = 168.0f, BIG_Y = 27.0f;
static const float STRIKE_X = 192.0f, STRIKE_Y = 23.0f, CHOKE_Y = 43.0f;
static const float ROWA_Y = 69.0f, ROWA_X0 = 13.0f, ROWA_DX = 22.0f;
static const float ROWB_Y = 89.0f, ROWB_X0 = 14.0f, ROWB_DX = 19.6f;
static const float JACK1_Y = 106.5f, JACK2_Y = 119.0f, JACK_X0 = 10.0f, JACK_DX = 16.6f;

static const char* QUALITY_NAMES[3] = {"ECO", "HIGH", "ULTRA"};
static const char* ALLOY_SHORT[NUM_ALLOYS] = {"B20", "B12", "B8", "BRASS", "NICKEL SILVER", "STEEL"};

struct Cymbolism;

struct RoundBigBlackSnapKnob : RoundBigBlackKnob {
	RoundBigBlackSnapKnob() { snap = true; }
};

// SIZE shows the diameter in inches for the current type
struct SizeQuantity : ParamQuantity {
	float typeInches() {
		int t = module ? (int)std::round(module->params[0].getValue()) : 1;
		return (float)CY_TYPES[clamp(t, 0, NUM_TYPES - 1)].sizeIn;
	}
	float getDisplayValue() override { return getValue() * typeInches(); }
	void setDisplayValue(float v) override { setValue(v / typeInches()); }
	std::string getUnit() override { return "\""; }
};

// WEIGHT shows the edge thickness in mm for the current type
struct WeightQuantity : ParamQuantity {
	float typeMm() {
		int t = module ? (int)std::round(module->params[0].getValue()) : 1;
		return (float)CY_TYPES[clamp(t, 0, NUM_TYPES - 1)].edgeMm;
	}
	float getDisplayValue() override { return getValue() * typeMm(); }
	void setDisplayValue(float v) override { setValue(v / typeMm()); }
	std::string getUnit() override { return " mm edge"; }
};

struct Cymbolism : Module {
	enum ParamId {
		TYPE_PARAM,   // must stay 0 (the quantities above read it)
		NEW_PARAM,
		POSITION_PARAM, FORCE_PARAM, STRIKE_PARAM, CHOKE_PARAM,
		SIZE_PARAM, WEIGHT_PARAM, TAPER_PARAM, PROFILE_PARAM, BELL_PARAM, FLANGE_PARAM, ALLOY_PARAM, HAMMER_PARAM, TENSION_PARAM,
		DECAY_PARAM, BLOOM_PARAM, DAMP_PARAM, MOUNT_PARAM, SIZZLE_PARAM, MIC_PARAM,
		PLAY_PARAM, HARDNESS_PARAM, RATE_PARAM, EXC_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		STRIKE_INPUT, VEL_INPUT, CHOKE_INPUT, ROLL_INPUT, EXCITE_INPUT, PLAY_INPUT,
		SIZE_INPUT, WEIGHT_INPUT, TAPER_INPUT, PROFILE_INPUT, BELL_INPUT, HAMMER_INPUT,
		TENSION_INPUT, DECAY_INPUT, BLOOM_INPUT, DAMP_INPUT, MOUNT_INPUT, SIZZLE_INPUT, MIC_INPUT,
		POSITION_INPUT, FORCE_INPUT, HARDNESS_INPUT, RATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId { OUT_OUTPUT, OUTPUTS_LEN };
	enum LightId { BUILD_LIGHT, LIGHTS_LEN };

	Runner runner;
	dsp::SchmittTrigger strikeTrig, strikeBtn, newBtn;
	dsp::ClockDivider ctlDiv;
	uint64_t seed = 1;
	int quality = 1;
	bool keepGeometry = false;
	bool rebuildOnCv = true;
	float trimDb = 0.f;
	int lastType = -1;
	float sampleRate = 0.f;
	GeomParams lastGeom;
	bool geomPending = true;
	bool firstStrikeAfterLoad = true;

	// for the screen
	static const int SCOPE = 2048;
	float scope[SCOPE] = {};
	std::atomic<int> scopePos{0};
	float dispLam = 1.f, dispPos = 0.9f, dispMic = 1.f;
	int dispPlay = SHOULDER, dispRivets = 0;
	float strikeFlash = 0.f;

	Cymbolism() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> types;
		for (int i = 0; i < NUM_TYPES; i++) types.push_back(CY_TYPES[i].name);
		configSwitch(TYPE_PARAM, 0.f, NUM_TYPES - 1, 1.f, "Type", types);
		configButton(NEW_PARAM, "New cymbal (re-roll the hammering pattern)");
		configParam(POSITION_PARAM, 0.f, 1.f, 0.95f, "Strike position: bell (0) to edge (1)", "%", 0.f, 100.f);
		configParam(FORCE_PARAM, 0.f, 1.f, 0.661f, "Force: strike speed (VEL patched: speed at 10 V); bow pressure; EXCITE gain", " m/s", 60.f, 0.2f);
		configButton(STRIKE_PARAM, "Strike");
		configButton(CHOKE_PARAM, "Choke (grab the edge while held)");
		configParam<SizeQuantity>(SIZE_PARAM, 0.6f, 1.5f, 1.f, "Size (diameter; CV is 1 V/oct: +1 V halves the size)");
		configParam<WeightQuantity>(WEIGHT_PARAM, 0.5f, 2.5f, 1.f, "Weight (thickness)");
		configParam(TAPER_PARAM, 1.f, 2.5f, 1.7f, "Taper (centre / edge thickness)", "x");
		configParam(PROFILE_PARAM, 0.f, 0.14f, 0.07f, "Profile (bow rise)", "% of radius", 0.f, 100.f);
		configParam(BELL_PARAM, 0.f, 2.f, 1.f, "Bell size", "x");
		configParam(FLANGE_PARAM, -20.f, 25.f, 0.f, "Flange (edge turned down / up, china)", " deg");
		configSwitch(ALLOY_PARAM, 0.f, NUM_ALLOYS - 1, 0.f, "Alloy", {"B20", "B12", "B8", "Brass", "Nickel silver", "Steel"});
		configParam(HAMMER_PARAM, 0.f, 1.f, 0.3f, "Hammering: machine spun (0) to hand hammered (1)", "%", 0.f, 100.f);
		configParam(TENSION_PARAM, -1.f, 1.f, 0.f, "Tension (residual hammering stress)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay", "x", 16.f, 0.25f);
		configParam(BLOOM_PARAM, 0.f, 2.f, 1.f, "Bloom (nonlinearity: 0 linear, 1 physical, 2 exaggerated)", "x");
		configParam(DAMP_PARAM, 0.f, 1.f, 0.f, "Damp (tape on the bow)", "%", 0.f, 100.f);
		configParam(MOUNT_PARAM, 0.f, 1.f, 0.5f, "Mount: tight felt (0) to loose and swinging (1)", "%", 0.f, 100.f);
		configParam(SIZZLE_PARAM, 0.f, 12.f, 0.f, "Sizzle (rivets)");
		paramQuantities[SIZZLE_PARAM]->snapEnabled = true;
		configParam(MIC_PARAM, 0.f, 4.f, 1.f, "Mic: 0 bell, 1 bow, 2 edge, 3 overhead, 4 room");
		configSwitch(PLAY_PARAM, 0.f, NUM_PLAYS - 1, 1.f, "Play", {"Tip", "Shoulder", "Mallet", "Rods", "Brush", "Bow"});
		configParam(HARDNESS_PARAM, 0.f, 1.f, 0.5f, "Hardness (beater stiffness; brush: wire stiffness)", "%", 0.f, 100.f);
		configParam(RATE_PARAM, 0.f, 1.f, 0.45f, "Rate: roll speed, brush sweep speed, bow speed", " Hz", 15.f, 2.f);
		configParam(EXC_PARAM, 0.f, 1.f, 0.f, "Excite level (EXCITE input)", "%", 0.f, 100.f);

		configInput(STRIKE_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Velocity 0-10 V (sampled at the strike)");
		configInput(CHOKE_INPUT, "Choke gate");
		configInput(ROLL_INPUT, "Roll gate (roll; brush sweep; bow)");
		configInput(EXCITE_INPUT, "Excite (audio pushed into the cymbal at the strike point)");
		configInput(PLAY_INPUT, "Play CV (0-10 V across the six beaters, added to the switch, read at each strike)");
		const char* cvNames[] = {"Size (1 V/oct)", "Weight", "Taper", "Profile", "Bell", "Hammer", "Tension", "Decay", "Bloom",
		                         "Damp", "Mount", "Sizzle", "Mic", "Position", "Force", "Hardness", "Rate"};
		for (int i = SIZE_INPUT; i <= RATE_INPUT; i++) configInput(i, std::string(cvNames[i - SIZE_INPUT]) + " CV");
		configOutput(OUT_OUTPUT, "Cymbal");
		configLight(BUILD_LIGHT, "Building the cymbal");
		ctlDiv.setDivision(32);
		seed = random::u32() | 1;
	}

	float cv(int id, float range) { return inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f * range : 0.f; }

	void applyTypeDefaults(int t) {
		const CymbalType& ct = CY_TYPES[t];
		params[WEIGHT_PARAM].setValue(1.f);
		params[TAPER_PARAM].setValue((float)ct.taper);
		params[PROFILE_PARAM].setValue((float)ct.rise);
		params[BELL_PARAM].setValue(1.f);
		params[FLANGE_PARAM].setValue((float)ct.flangeDeg);
	}
	void loadPlayDefaults() {
		int t = clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES - 1);
		const CymbalType& ct = CY_TYPES[t];
		params[PLAY_PARAM].setValue((float)ct.play);
		params[POSITION_PARAM].setValue((float)ct.position);
		params[SIZZLE_PARAM].setValue((float)ct.rivets);
	}

	GeomParams readGeometry(bool withCv) {
		GeomParams g;
		int t = clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES - 1);
		g.type = t;
		auto P = [&](int p, int in, float lo, float hi) {
			float v = params[p].getValue() + (withCv ? cv(in, hi - lo) : 0.f);
			return clamp(v, lo, hi);
		};
		// quantised so small CV wobble doesn't rebuild constantly
		g.weight = std::round(P(WEIGHT_PARAM, WEIGHT_INPUT, 0.5f, 2.5f) * 50.f) / 50.f;
		g.taper = std::round(P(TAPER_PARAM, TAPER_INPUT, 1.f, 2.5f) * 40.f) / 40.f;
		g.rise = std::round(P(PROFILE_PARAM, PROFILE_INPUT, 0.f, 0.14f) * 500.f) / 500.f;
		g.bell = std::round(P(BELL_PARAM, BELL_INPUT, 0.f, 2.f) * 40.f) / 40.f;
		g.flangeDeg = std::round(params[FLANGE_PARAM].getValue());
		g.alloy = clamp((int)std::round(params[ALLOY_PARAM].getValue()), 0, NUM_ALLOYS - 1);
		g.tension = std::round(P(TENSION_PARAM, TENSION_INPUT, -1.f, 1.f) * 20.f) / 20.f;
		g.quality = quality;
		g.sampleRate = sampleRate > 0 ? sampleRate : 48000.f;
		return g;
	}

	void readControls(Controls& c) {
		float lam = params[SIZE_PARAM].getValue();
		if (inputs[SIZE_INPUT].isConnected()) lam *= std::pow(2.f, -inputs[SIZE_INPUT].getVoltage());
		c.size = clamp(lam, 0.35f, 2.0f);
		float d = clamp(params[DECAY_PARAM].getValue() + cv(DECAY_INPUT, 1.f), 0.f, 1.f);
		c.decay = 0.25 * std::pow(16.0, d);
		c.bloom = clamp(params[BLOOM_PARAM].getValue() + cv(BLOOM_INPUT, 2.f), 0.f, 2.f);
		c.damp = clamp(params[DAMP_PARAM].getValue() + cv(DAMP_INPUT, 1.f), 0.f, 1.f);
		c.mount = clamp(params[MOUNT_PARAM].getValue() + cv(MOUNT_INPUT, 1.f), 0.f, 1.f);
		c.hammer = clamp(params[HAMMER_PARAM].getValue() + cv(HAMMER_INPUT, 1.f), 0.f, 1.f);
		c.mic = clamp(params[MIC_PARAM].getValue() + cv(MIC_INPUT, 4.f), 0.f, 4.f);
		c.sizzle = std::round(clamp(params[SIZZLE_PARAM].getValue() + cv(SIZZLE_INPUT, 12.f), 0.f, 12.f)) / 12.0;
		c.position = clamp(params[POSITION_PARAM].getValue() + cv(POSITION_INPUT, 1.f), 0.f, 1.f);
		c.hardness = clamp(params[HARDNESS_PARAM].getValue() + cv(HARDNESS_INPUT, 1.f), 0.f, 1.f);
		float f = clamp(params[FORCE_PARAM].getValue() + cv(FORCE_INPUT, 1.f), 0.f, 1.f);
		c.force = 0.2 * std::pow(60.0, f);
		if (inputs[VEL_INPUT].isConnected()) c.force *= clamp(inputs[VEL_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		float r = clamp(params[RATE_PARAM].getValue() + cv(RATE_INPUT, 1.f), 0.f, 1.f);
		c.rate = 2.0 * std::pow(15.0, r);
		c.excite = params[EXC_PARAM].getValue();
		c.roll = inputs[ROLL_INPUT].getVoltage() >= 1.f;
		c.choke = params[CHOKE_PARAM].getValue() > 0.5f || inputs[CHOKE_INPUT].getVoltage() >= 1.f;
		c.play = playNow();
		c.seed = seed;
	}

	int playNow() {
		float p = params[PLAY_PARAM].getValue();
		if (inputs[PLAY_INPUT].isConnected()) p += clamp(inputs[PLAY_INPUT].getVoltage(), 0.f, 10.f) * 0.1f * (NUM_PLAYS - 1);
		return clamp((int)std::round(p), 0, NUM_PLAYS - 1);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		geomPending = true;
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		sampleRate = e.sampleRate;
		geomPending = true;
	}

	void process(const ProcessArgs& args) override {
		if (sampleRate != args.sampleRate) { sampleRate = args.sampleRate; geomPending = true; }
		bool strikeNow = false;
		if (strikeTrig.process(inputs[STRIKE_INPUT].getVoltage(), 0.1f, 1.f)) strikeNow = true;
		if (strikeBtn.process(params[STRIKE_PARAM].getValue())) strikeNow = true;
		if (newBtn.process(params[NEW_PARAM].getValue())) seed = random::u32() | 1;

		if (ctlDiv.process()) {
			int t = clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES - 1);
			if (t != lastType) {
				if (lastType >= 0 && !keepGeometry) applyTypeDefaults(t);
				lastType = t;
				geomPending = true;
			}
			readControls(runner.ctl);
			GeomParams g = readGeometry(rebuildOnCv);
			if (g.key() != lastGeom.key()) { lastGeom = g; geomPending = true; }
			if (geomPending && runner.requestBuild(lastGeom, runner.ctl)) geomPending = false;
			lights[BUILD_LIGHT].setBrightness(runner.building ? 1.f : 0.f);
			dispLam = (float)runner.ctl.size;
			dispPos = (float)runner.ctl.position;
			dispMic = (float)runner.ctl.mic;
			dispPlay = runner.ctl.play;
			dispRivets = (int)std::round(runner.ctl.sizzle * 12);
		}
		float vel = 0.f;
		if (strikeNow) {
			if (!rebuildOnCv) {
				GeomParams g = readGeometry(true);
				if (g.key() != lastGeom.key()) { lastGeom = g; geomPending = true; }
			}
			runner.ctl.play = playNow();
			runner.ctl.position = clamp(params[POSITION_PARAM].getValue() + cv(POSITION_INPUT, 1.f), 0.f, 1.f);
			float f = clamp(params[FORCE_PARAM].getValue() + cv(FORCE_INPUT, 1.f), 0.f, 1.f);
			vel = 0.2f * std::pow(60.f, f);
			if (inputs[VEL_INPUT].isConnected()) vel *= clamp(inputs[VEL_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
			if (vel < 0.02f) vel = 0.f;
			strikeFlash = 1.f;
		}
		float exc = inputs[EXCITE_INPUT].getVoltage() * 0.2f;
		float y = runner.tick(exc, vel);
		y *= std::pow(10.f, trimDb / 20.f);
		outputs[OUT_OUTPUT].setVoltage(y);
		int sp = scopePos.load(std::memory_order_relaxed);
		scope[sp] = y;
		scopePos.store((sp + 1) % SCOPE, std::memory_order_relaxed);
		strikeFlash *= 0.9995f;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer((json_int_t)seed));
		json_object_set_new(root, "quality", json_integer(quality));
		json_object_set_new(root, "threaded", json_boolean(runner.threaded));
		json_object_set_new(root, "keepGeometry", json_boolean(keepGeometry));
		json_object_set_new(root, "rebuildOnCv", json_boolean(rebuildOnCv));
		json_object_set_new(root, "trimDb", json_real(trimDb));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "seed"))) seed = (uint64_t)json_integer_value(j);
		if ((j = json_object_get(root, "quality"))) quality = clamp((int)json_integer_value(j), 0, 2);
		if ((j = json_object_get(root, "threaded"))) runner.threaded = json_boolean_value(j);
		if ((j = json_object_get(root, "keepGeometry"))) keepGeometry = json_boolean_value(j);
		if ((j = json_object_get(root, "rebuildOnCv"))) rebuildOnCv = json_boolean_value(j);
		if ((j = json_object_get(root, "trimDb"))) trimDb = (float)json_number_value(j);
		// the TYPE knob was restored with the patch: don't overwrite the saved geometry
		lastType = clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES - 1);
		geomPending = true;
	}
};

// ---- the screen: the cymbal from above, the strike point, rivets and mic, and a spectrum

struct CymbolismDisplay : Widget {
	Cymbolism* module = nullptr;
	dsp::RealFFT fft{1024};
	float spec[64] = {};

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		const NVGcolor gold = nvgRGB(226, 194, 122), cream = nvgRGB(241, 231, 207), soft = nvgRGB(183, 168, 136);
		const NVGcolor bronze = nvgRGB(185, 142, 82), patina = nvgRGB(127, 165, 143);
		float w = box.size.x, h = box.size.y;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));

		int type = module ? clamp((int)std::round(module->params[Cymbolism::TYPE_PARAM].getValue()), 0, NUM_TYPES - 1) : 1;
		const CymbalType& ct = CY_TYPES[type];
		float lam = module ? module->dispLam : 1.f;
		float weight = module ? module->params[Cymbolism::WEIGHT_PARAM].getValue() : 1.f;
		float bellX = module ? module->params[Cymbolism::BELL_PARAM].getValue() : 1.f;
		float flange = module ? module->params[Cymbolism::FLANGE_PARAM].getValue() : 0.f;
		int alloy = module ? clamp((int)std::round(module->params[Cymbolism::ALLOY_PARAM].getValue()), 0, NUM_ALLOYS - 1) : 0;
		int play = module ? module->dispPlay : ct.play;
		float pos = module ? module->dispPos : (float)ct.position;
		float mic = module ? module->dispMic : 1.f;
		int rivets = module ? module->dispRivets : 0;
		float swing = module ? module->runner.swing.load() : 0.f;

		// ---- the cymbal from above (left)
		float cx = mm2px(23.f), cy = h * 0.5f + mm2px(1.5f);
		float Rmax = mm2px(19.f);
		float inches = (float)ct.sizeIn * lam;
		float R = Rmax * clamp(0.45f + 0.55f * (inches - 6.f) / 24.f, 0.35f, 1.f);
		nvgSave(vg);
		nvgTranslate(vg, cx, cy);
		nvgRotate(vg, swing * 3.f);
		nvgBeginPath(vg);
		nvgCircle(vg, 0, 0, R);
		NVGcolor c0 = alloy == STEEL ? nvgRGB(150, 150, 150) : (alloy == NICKEL ? nvgRGB(175, 170, 150) : nvgRGB(176, 136, 70));
		NVGcolor c1 = alloy == STEEL ? nvgRGB(60, 60, 62) : nvgRGB(70, 50, 22);
		nvgFillPaint(vg, nvgRadialGradient(vg, -R * 0.25f, -R * 0.3f, R * 0.05f, R * 1.1f, c0, c1));
		nvgFill(vg);
		// lathe rings
		for (int i = 1; i < 9; i++) {
			nvgBeginPath(vg);
			nvgCircle(vg, 0, 0, R * (0.25f + 0.09f * i));
			nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 28));
			nvgStrokeWidth(vg, 0.5f);
			nvgStroke(vg);
		}
		if (flange != 0.f) {
			nvgBeginPath(vg);
			nvgCircle(vg, 0, 0, R * 0.93f);
			nvgStrokeColor(vg, nvgRGBA(255, 230, 180, 50));
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		}
		float rb = R * clamp((float)ct.bellDia * bellX, 0.f, 0.75f);
		if (rb > R * 0.05f) {
			nvgBeginPath(vg);
			nvgCircle(vg, 0, 0, rb);
			nvgFillPaint(vg, nvgRadialGradient(vg, -rb * 0.3f, -rb * 0.3f, rb * 0.1f, rb, nvgRGB(210, 170, 95), nvgRGB(110, 80, 35)));
			nvgFill(vg);
		}
		nvgBeginPath(vg);
		nvgCircle(vg, 0, 0, std::max(1.5f, R * 0.04f));
		nvgFillColor(vg, nvgRGB(10, 8, 5));
		nvgFill(vg);
		// rivets
		for (int i = 0; i < rivets; i++) {
			float a = 2.f * M_PI * i / std::max(1, rivets);
			nvgBeginPath(vg);
			nvgCircle(vg, 0.82f * R * std::cos(a), 0.82f * R * std::sin(a), 1.1f);
			nvgFillColor(vg, cream);
			nvgFill(vg);
		}
		nvgRestore(vg);
		// strike point (fixed in the room: the cymbal turns under it)
		float sr = R * (0.04f + 0.96f * pos);
		float glow = module ? module->strikeFlash : 0.f;
		nvgBeginPath(vg);
		nvgCircle(vg, cx + sr, cy, 2.0f + 2.5f * glow);
		nvgFillColor(vg, nvgRGBAf(1.f, 0.85f, 0.5f, 0.5f + 0.5f * glow));
		nvgFill(vg);
		// mic
		float mr = mic <= 2.f ? R * (0.05f + 0.465f * mic) : R * (0.98f + 0.08f * (mic - 2.f));
		float my = cy - (mic <= 2.f ? 0.f : mm2px(2.5f) * (mic - 2.f));
		nvgBeginPath(vg);
		nvgRect(vg, cx - mr - 1.5f, my - 4.5f, 3.f, 6.f);
		nvgFillColor(vg, patina);
		nvgFill(vg);

		if (!font) { Widget::drawLayer(args, layer); return; }
		nvgFontFaceId(vg, font->handle);

		// ---- text (right)
		float x0 = mm2px(47.f), right = w - mm2px(2.f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(4.2f));
		nvgFillColor(vg, gold);
		nvgText(vg, x0, mm2px(5.8f), ct.name, NULL);
		nvgFontSize(vg, mm2px(2.6f));
		nvgFillColor(vg, cream);
		std::string l1 = string::f("%.1f\"  %.2f mm  %s", inches, ct.edgeMm * weight * lam, ALLOY_SHORT[alloy]);
		nvgText(vg, x0, mm2px(10.0f), l1.c_str(), NULL);
		nvgFillColor(vg, bronze);
		static const char* micNames[5] = {"BELL", "BOW", "EDGE", "OVERHEAD", "ROOM"};
		std::string l2 = string::f("%s  mic %s", CY_PLAY_NAMES[clamp(play, 0, NUM_PLAYS - 1)], micNames[clamp((int)std::round(mic), 0, 4)]);
		nvgText(vg, x0, mm2px(13.8f), l2.c_str(), NULL);

		// ---- spectrum
		float sx0 = x0, sx1 = right, sy0 = mm2px(17.0f), sy1 = h - mm2px(6.0f);
		nvgBeginPath(vg);
		nvgRect(vg, sx0, sy0, sx1 - sx0, sy1 - sy0);
		nvgFillColor(vg, nvgRGBA(255, 255, 255, 8));
		nvgFill(vg);
		if (module) {
			alignas(16) float in[1024];
			alignas(16) float out[2048];
			int p = module->scopePos.load(std::memory_order_relaxed);
			for (int i = 0; i < 1024; i++) {
				float win = 0.5f - 0.5f * std::cos(2.f * M_PI * i / 1023.f);
				in[i] = module->scope[(p - 1024 + i + Cymbolism::SCOPE) % Cymbolism::SCOPE] * win;
			}
			fft.rfft(in, out);
			float sr = module->sampleRate > 0 ? module->sampleRate : 48000.f;
			for (int b = 0; b < 64; b++) {
				float f0 = 40.f * std::pow(500.f, b / 64.f), f1 = 40.f * std::pow(500.f, (b + 1) / 64.f);
				int k0 = clamp((int)(f0 / sr * 1024), 1, 511), k1 = clamp((int)(f1 / sr * 1024), k0 + 1, 512);
				float e = 0;
				for (int k = k0; k < k1; k++) e += out[2 * k] * out[2 * k] + out[2 * k + 1] * out[2 * k + 1];
				float db = 10.f * std::log10(e / (k1 - k0) + 1e-12f);
				float v = clamp((db + 10.f) / 60.f, 0.f, 1.f);
				spec[b] = std::max(v, spec[b] * 0.93f);
			}
		}
		else for (int b = 0; b < 64; b++) spec[b] = 0.55f * std::exp(-std::pow((b - 30) / 22.f, 2)) + 0.05f;
		float bw = (sx1 - sx0) / 64.f;
		for (int b = 0; b < 64; b++) {
			float bh = spec[b] * (sy1 - sy0);
			nvgBeginPath(vg);
			nvgRect(vg, sx0 + b * bw + 0.3f, sy1 - bh, bw - 0.6f, bh);
			nvgFillColor(vg, nvgLerpRGBA(bronze, gold, b / 64.f));
			nvgFill(vg);
		}
		nvgFontSize(vg, mm2px(2.0f));
		nvgFillColor(vg, soft);
		nvgText(vg, sx0, h - mm2px(2.6f), "40 Hz", NULL);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(vg, sx1, h - mm2px(2.6f), "20 kHz", NULL);

		// ---- status line
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(2.3f));
		std::string st;
		if (module) {
			if (module->runner.building) st = "BUILDING...";
			else if (!module->runner.hasModel) st = "waiting for the first model";
			else st = string::f("%d modes  core %d  cpu %d%%", module->runner.nModes.load(), module->runner.nCore.load(),
			                    (int)std::round(module->runner.cpu.load() * 100.f));
		}
		else st = "1215 modes  core 63";
		nvgFillColor(vg, module && module->runner.building ? gold : soft);
		nvgText(vg, mm2px(1.4f), h - mm2px(1.4f), st.c_str(), NULL);
		Widget::drawLayer(args, layer);
	}
};

struct CymbolismWidget : ModuleWidget {
	CymbolismWidget(Cymbolism* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Cymbolism.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		CymbolismDisplay* d = createWidget<CymbolismDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundBigBlackSnapKnob>(mm2px(Vec(TYPE_X, TYPE_Y)), module, Cymbolism::TYPE_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(NEW_X, NEW_Y)), module, Cymbolism::NEW_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(NEW_X + 6.f, NEW_Y - 4.f)), module, Cymbolism::BUILD_LIGHT));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(POS_X, BIG_Y)), module, Cymbolism::POSITION_PARAM));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(FORCE_X, BIG_Y)), module, Cymbolism::FORCE_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(STRIKE_X, STRIKE_Y)), module, Cymbolism::STRIKE_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(STRIKE_X, CHOKE_Y)), module, Cymbolism::CHOKE_PARAM));

		const int rowA[9] = {Cymbolism::SIZE_PARAM, Cymbolism::WEIGHT_PARAM, Cymbolism::TAPER_PARAM, Cymbolism::PROFILE_PARAM,
		                     Cymbolism::BELL_PARAM, Cymbolism::FLANGE_PARAM, Cymbolism::ALLOY_PARAM, Cymbolism::HAMMER_PARAM,
		                     Cymbolism::TENSION_PARAM};
		for (int i = 0; i < 9; i++) {
			Vec p = mm2px(Vec(ROWA_X0 + ROWA_DX * i, ROWA_Y));
			if (rowA[i] == Cymbolism::ALLOY_PARAM) addParam(createParamCentered<RoundBlackSnapKnob>(p, module, rowA[i]));
			else addParam(createParamCentered<RoundBlackKnob>(p, module, rowA[i]));
		}
		const int rowB[10] = {Cymbolism::DECAY_PARAM, Cymbolism::BLOOM_PARAM, Cymbolism::DAMP_PARAM, Cymbolism::MOUNT_PARAM,
		                      Cymbolism::SIZZLE_PARAM, Cymbolism::MIC_PARAM, Cymbolism::PLAY_PARAM, Cymbolism::HARDNESS_PARAM,
		                      Cymbolism::RATE_PARAM, Cymbolism::EXC_PARAM};
		for (int i = 0; i < 10; i++) {
			Vec p = mm2px(Vec(ROWB_X0 + ROWB_DX * i, ROWB_Y));
			if (rowB[i] == Cymbolism::PLAY_PARAM || rowB[i] == Cymbolism::SIZZLE_PARAM)
				addParam(createParamCentered<RoundBlackSnapKnob>(p, module, rowB[i]));
			else addParam(createParamCentered<RoundBlackKnob>(p, module, rowB[i]));
		}
		// jacks: row 1 = STRIKE .. HAMMER (inputs 0..11), row 2 = TENSION .. RATE (12..22) + OUT
		for (int i = 0; i < 12; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + JACK_DX * i, JACK1_Y)), module, Cymbolism::STRIKE_INPUT + i));
		for (int i = 0; i < 11; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + JACK_DX * i, JACK2_Y)), module, Cymbolism::TENSION_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + JACK_DX * 11, JACK2_Y)), module, Cymbolism::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Cymbolism* m = getModule<Cymbolism>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Quality", {"ECO (lightest)", "HIGH (default)", "ULTRA (bigger nonlinear core, heavy)"},
			[=]() { return m->quality; },
			[=](int q) { m->quality = q; m->geomPending = true; m->lastGeom = GeomParams(); }));
		menu->addChild(createBoolMenuItem("Run on its own thread", "",
			[=]() { return (bool)m->runner.threaded; },
			[=](bool b) { m->runner.threaded = b; }));
		menu->addChild(createMenuLabel(string::f("Latency %.1f ms", m->runner.latency() * 64 * 1000.f / std::max(1.f, m->sampleRate))));
		menu->addChild(createBoolPtrMenuItem("Keep geometry when TYPE changes", "", &m->keepGeometry));
		menu->addChild(createBoolPtrMenuItem("Rebuild on geometry CV changes (off: read at strikes)", "", &m->rebuildOnCv));
		menu->addChild(createMenuItem("Load the type's playing defaults", "", [=]() { m->loadPlayDefaults(); }));
		menu->addChild(createIndexSubmenuItem("OUT trim", {"-12 dB", "-6 dB", "0 dB", "+6 dB", "+12 dB"},
			[=]() { return clamp((int)std::round(m->trimDb / 6.f) + 2, 0, 4); },
			[=](int i) { m->trimDb = 6.f * (i - 2); }));
		menu->addChild(createMenuLabel(string::f("This cymbal: #%llu", (unsigned long long)m->seed)));
	}
};

rack::plugin::Model* modelCymbolism = createModel<Cymbolism, CymbolismWidget>("Cymbolism");
