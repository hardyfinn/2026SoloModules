// Cymbolism Extended: the Cymbolism cymbal with the laws of physics on knobs.
// Same engine as Cymbolism (CyModes, CyEngine, CyRunner); the UNREAL section reaches into the
// model through cy::Unreal (CyUnreal.hpp). This file is the panel, the jacks and the screen.
#include "plugin.hpp"
#include "CyRunner.hpp"

using namespace cy;

// ---- LAYOUT (same numbers in res/make_panel_extended.py) ----
static const float DISP_X = 5.0f, DISP_Y = 13.0f, DISP_W = 104.0f, DISP_H = 38.0f;
static const float TYPE_X = 122.0f, TYPE_Y = 24.0f;
static const float TYPEB_X = 122.0f, TYPEB_Y = 44.0f;
static const float NEW_X = 135.0f, NEW_Y = 44.0f;
static const float POS_X = 150.0f, FORCE_X = 174.0f, BIG_Y = 24.0f, BIGCV_Y = 44.0f;
static const float BTN_X = 193.0f, STRIKE_Y = 19.0f, CHOKE_Y = 31.5f, FREEZE_Y = 44.0f;
static const float IO_X0 = 210.0f, IO_DX = 12.2f, IO1_Y = 24.0f, IO2_Y = 44.0f;
static const float ROW_X0 = 14.0f, ROW_DX = 22.0f;
static const float ROW1_Y = 63.0f, ROW2_Y = 85.5f, ROW3_Y = 108.0f, CV_DY = 10.0f;

static const char* X_ALLOY_SHORT[NUM_ALLOYS_EXT] = {"B20", "B12", "B8", "BRASS", "NICKEL SILVER", "STEEL", "ALUMINIUM", "GLASS", "WOOD"};

namespace {

struct BigSnapKnob : RoundBigBlackKnob {
	BigSnapKnob() { snap = true; }
};
struct LatchButton : VCVButton {
	LatchButton() { momentary = false; latch = true; }
};

// SIZE shows the diameter in inches for the current type
struct XSizeQuantity : ParamQuantity {
	float typeInches() {
		int t = module ? (int)std::round(module->params[0].getValue()) : 1;
		return (float)CY_TYPES[clamp(t, 0, NUM_TYPES_EXT - 1)].sizeIn;
	}
	float getDisplayValue() override { return getValue() * typeInches(); }
	void setDisplayValue(float v) override { setValue(v / typeInches()); }
	std::string getUnit() override { return "\""; }
};
struct XWeightQuantity : ParamQuantity {
	float typeMm() {
		int t = module ? (int)std::round(module->params[0].getValue()) : 1;
		return (float)CY_TYPES[clamp(t, 0, NUM_TYPES_EXT - 1)].edgeMm;
	}
	float getDisplayValue() override { return getValue() * typeMm(); }
	void setDisplayValue(float v) override { setValue(v / typeMm()); }
	std::string getUnit() override { return " mm edge"; }
};

} // namespace

struct CymbolismExtended : Module {
	enum ParamId {
		TYPE_PARAM,   // must stay 0 (the quantities above read it)
		TYPEB_PARAM, NEW_PARAM,
		POSITION_PARAM, FORCE_PARAM, STRIKE_PARAM, CHOKE_PARAM, FREEZE_PARAM,
		// row 1: CYMBAL
		SIZE_PARAM, WEIGHT_PARAM, TAPER_PARAM, PROFILE_PARAM, BELL_PARAM, FLANGE_PARAM, HOLE_PARAM, ALLOY_PARAM, HAMMER_PARAM, TENSION_PARAM,
		// row 2: UNREAL
		MORPH_PARAM, WARP_PARAM, PIVOT_PARAM, TUNE_PARAM, SCALE_PARAM, ROOT_PARAM, TILT_PARAM, SUSTAIN_PARAM, SWELL_PARAM, COLOUR_PARAM,
		FLOW_PARAM, LOAD_PARAM, LOADPOS_PARAM,
		// row 3: SOUND and PLAY
		DECAY_PARAM, BLOOM_PARAM, DAMP_PARAM, MOUNT_PARAM, SIZZLE_PARAM, MIC_PARAM, PICKUP_PARAM, WIDTH_PARAM,
		PLAY_PARAM, HARDNESS_PARAM, RATE_PARAM, BOUNCE_PARAM, EXC_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		STRIKE_INPUT, VEL_INPUT, CHOKE_INPUT, ROLL_INPUT, HOLD_INPUT, FREEZE_INPUT, EXCITE_INPUT, PUMP_INPUT,
		VOCT_INPUT, CHORD_INPUT, POSITION_INPUT, FORCE_INPUT,
		SIZE_INPUT, WEIGHT_INPUT, TAPER_INPUT, PROFILE_INPUT, BELL_INPUT, HAMMER_INPUT, TENSION_INPUT,
		MORPH_INPUT, WARP_INPUT, PIVOT_INPUT, TUNE_INPUT, ROOT_INPUT, TILT_INPUT, SUSTAIN_INPUT, SWELL_INPUT, COLOUR_INPUT,
		FLOW_INPUT, LOAD_INPUT, LOADPOS_INPUT,
		DECAY_INPUT, BLOOM_INPUT, DAMP_INPUT, MOUNT_INPUT, SIZZLE_INPUT, MIC_INPUT, PICKUP_INPUT, WIDTH_INPUT,
		PLAY_INPUT, HARDNESS_INPUT, RATE_INPUT, BOUNCE_INPUT, EXC_INPUT,
		INPUTS_LEN
	};
	enum OutputId { L_OUTPUT, R_OUTPUT, CORE_OUTPUT, WASH_OUTPUT, ENV_OUTPUT, OUTPUTS_LEN };
	enum LightId { BUILD_LIGHT, FREEZE_LIGHT, LIGHTS_LEN };

	Runner runner;
	dsp::SchmittTrigger strikeTrig, strikeBtn, newBtn;
	dsp::ClockDivider ctlDiv;
	uint64_t seed = 1;
	int quality = 1;
	bool keepGeometry = false;
	bool rebuildOnCv = true;
	bool voctAtStrikes = false;
	float trimDb = 0.f;
	int lastType = -1;
	float sampleRate = 0.f;
	GeomParams lastGeom;
	int lastTypeB = -1;
	bool geomPending = true;
	float voctSlew = 0.f, voctHeld = 0.f;

	// for the screen
	static const int SCOPE = 2048;
	float scope[SCOPE] = {};
	std::atomic<int> scopePos{0};
	float dispLam = 1.f, dispPos = 0.9f, dispMic = 1.f;
	int dispPlay = SHOULDER, dispRivets = 0;
	Unreal dispU;
	float strikeFlash = 0.f;

	CymbolismExtended() {
		runner.extended = true;
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> types;
		for (int i = 0; i < NUM_TYPES_EXT; i++) types.push_back(CY_TYPES[i].name);
		configSwitch(TYPE_PARAM, 0.f, NUM_TYPES_EXT - 1, 1.f, "Type", types);
		configSwitch(TYPEB_PARAM, 0.f, NUM_TYPES_EXT - 1, 7.f, "Type B (MORPH target)", types);
		configButton(NEW_PARAM, "New cymbal (re-roll the hammering pattern)");
		configParam(POSITION_PARAM, 0.f, 1.f, 0.95f, "Strike position: bell (0) to edge (1)", "%", 0.f, 100.f);
		configParam(FORCE_PARAM, 0.f, 1.f, 0.661f, "Force: strike speed (VEL patched: speed at 10 V); bow pressure; EXCITE gain", " m/s", 60.f, 0.2f);
		configButton(STRIKE_PARAM, "Strike");
		configButton(CHOKE_PARAM, "Choke (grab the edge while held)");
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze (hold the energy where it is)", {"Off", "On"});

		configParam<XSizeQuantity>(SIZE_PARAM, 0.2f, 3.f, 1.f, "Size (diameter; CV is 1 V/oct: +1 V halves the size)");
		configParam<XWeightQuantity>(WEIGHT_PARAM, 0.2f, 5.f, 1.f, "Weight (thickness)");
		configParam(TAPER_PARAM, 0.4f, 3.f, 1.7f, "Taper (centre / edge thickness; below 1 the edge is thicker)", "x");
		configParam(PROFILE_PARAM, 0.f, 1.f, 0.07f, "Profile (rise; up to a deep bowl)", "% of radius", 0.f, 100.f);
		configParam(BELL_PARAM, 0.f, 3.f, 1.f, "Bell size", "x");
		configParam(FLANGE_PARAM, -80.f, 80.f, 0.f, "Flange (edge turned down / up)", " deg");
		configParam(HOLE_PARAM, 0.f, 0.8f, 0.f, "Hole (0 = the usual 6.5 mm; up to a ring)", "% of radius", 0.f, 100.f);
		configSwitch(ALLOY_PARAM, 0.f, NUM_ALLOYS_EXT - 1, 0.f, "Alloy",
		             {"B20", "B12", "B8", "Brass", "Nickel silver", "Steel", "Aluminium", "Glass", "Wood"});
		configParam(HAMMER_PARAM, 0.f, 1.f, 0.3f, "Hammering: machine spun (0) to hand hammered (1)", "%", 0.f, 100.f);
		configParam(TENSION_PARAM, -1.f, 1.f, 0.f, "Tension (residual stress, live)", "%", 0.f, 100.f);

		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph towards TYPE B's pitches", "%", 0.f, 100.f);
		configParam(WARP_PARAM, -1.f, 2.5f, 1.f, "Warp (frequency law: 1 real, 0 unison, below 0 mirrored)");
		configParam(PIVOT_PARAM, 0.f, 1.f, 0.4974f, "Pivot (the pitch WARP bends around)", " Hz", 50.f, 100.f);
		configParam(TUNE_PARAM, 0.f, 1.f, 0.f, "Tune (pull every mode onto the scale)", "%", 0.f, 100.f);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES - 1, SC_PENTMIN, "Scale",
		             {"Chromatic", "Major", "Minor", "Dorian", "Pentatonic major", "Pentatonic minor", "Blues", "Whole tone", "Fifths",
		              "Octaves", "Harmonic series", "Chord (CHORD input)"});
		configSwitch(ROOT_PARAM, 0.f, 11.f, 2.f, "Root", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
		configParam(TILT_PARAM, -1.f, 1.f, 0.f, "Tilt (negative: highs outlast lows)", "%", 0.f, 100.f);
		configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.f, "Sustain (the cymbal sings on its own; HOLD gates it)", "%", 0.f, 100.f);
		configParam(SWELL_PARAM, 0.f, 1.f, 0.607f, "Swell (how fast SUSTAIN grows the ring)", " /s", 60.f, 0.5f);
		configParam(COLOUR_PARAM, 0.f, 1.f, 0.55f, "Colour (where SUSTAIN feeds energy)", " Hz", 166.67f, 60.f);
		configParam(FLOW_PARAM, -1.f, 1.f, 0.f, "Flow (bloom speed: slow motion to instant)");
		configParam(LOAD_PARAM, 0.f, 0.5f, 0.f, "Load (a weight stuck on the cymbal)", " g", 0.f, 1000.f);
		configParam(LOADPOS_PARAM, 0.f, 1.f, 0.6f, "Load position: bell (0) to edge (1)", "%", 0.f, 100.f);

		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay", "x", 16.f, 0.25f);
		configParam(BLOOM_PARAM, -2.f, 2.f, 1.f, "Bloom (negative reverses the pitch glide)", "x");
		configParam(DAMP_PARAM, 0.f, 1.f, 0.f, "Damp (tape on the bow)", "%", 0.f, 100.f);
		configParam(MOUNT_PARAM, 0.f, 1.f, 0.5f, "Mount: tight felt (0) to loose and swinging (1)", "%", 0.f, 100.f);
		configParam(SIZZLE_PARAM, 0.f, 12.f, 0.f, "Sizzle (rivets)");
		paramQuantities[SIZZLE_PARAM]->snapEnabled = true;
		configParam(MIC_PARAM, 0.f, 4.f, 1.f, "Mic: 0 bell, 1 bow, 2 edge, 3 overhead, 4 room");
		configParam(PICKUP_PARAM, 0.f, 1.f, 0.f, "Pickup: air mic (0) to contact piezo (1)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 90.f, 20.f, "Width (stereo mics apart round the cymbal)", " deg");
		configSwitch(PLAY_PARAM, 0.f, NUM_PLAYS_EXT - 1, 1.f, "Play", {"Tip", "Shoulder", "Mallet", "Rods", "Brush", "Bow", "Rain"});
		configParam(HARDNESS_PARAM, 0.f, 1.f, 0.5f, "Hardness (beater stiffness; brush and rain: brightness)", "%", 0.f, 100.f);
		configParam(RATE_PARAM, 0.f, 1.f, 0.45f, "Rate: roll speed, sweep speed, bow speed, rain density", " Hz", 15.f, 2.f);
		configParam(BOUNCE_PARAM, 0.f, 1.f, 0.f, "Bounce (the beater is held against the cymbal and ricochets)", "%", 0.f, 100.f);
		configParam(EXC_PARAM, 0.f, 1.f, 0.f, "Excite level (EXCITE input)", "%", 0.f, 100.f);

		configInput(STRIKE_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Velocity 0-10 V (sampled at the strike)");
		configInput(CHOKE_INPUT, "Choke gate");
		configInput(ROLL_INPUT, "Roll gate (roll; brush sweep; bow; rain)");
		configInput(HOLD_INPUT, "Hold gate (SUSTAIN while high: a reverse cymbal)");
		configInput(FREEZE_INPUT, "Freeze gate");
		configInput(EXCITE_INPUT, "Excite (audio pushed into the cymbal at the strike point)");
		configInput(PUMP_INPUT, "Pump (audio-rate tension: parametric excitation)");
		configInput(VOCT_INPUT, "V/oct (transposes every mode)");
		configInput(CHORD_INPUT, "Chord (polyphonic V/oct, for SCALE CHORD)");
		configInput(POSITION_INPUT, "Position CV");
		configInput(FORCE_INPUT, "Force CV");
		const char* cvNames[] = {"Size (1 V/oct)", "Weight", "Taper", "Profile", "Bell", "Hammer", "Tension",
		                         "Morph", "Warp", "Pivot", "Tune", "Root (1 V/oct)", "Tilt", "Sustain", "Swell", "Colour", "Flow", "Load", "Load position",
		                         "Decay", "Bloom", "Damp", "Mount", "Sizzle", "Mic", "Pickup", "Width", "Play (0-10 V across the seven)", "Hardness", "Rate",
		                         "Bounce", "Excite level"};
		for (int i = SIZE_INPUT; i <= EXC_INPUT; i++) configInput(i, std::string(cvNames[i - SIZE_INPUT]) + " CV");
		configOutput(L_OUTPUT, "Left (mono when R is unpatched)");
		configOutput(R_OUTPUT, "Right");
		configOutput(CORE_OUTPUT, "Core (the coupled low modes)");
		configOutput(WASH_OUTPUT, "Wash (everything above the core)");
		configOutput(ENV_OUTPUT, "Envelope (core energy, 0-10 V)");
		configLight(BUILD_LIGHT, "Building the cymbal");
		ctlDiv.setDivision(32);
		seed = random::u32() | 1;
	}

	float cv(int id, float range) { return inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f * range : 0.f; }
	float knob(int p, int in, float lo, float hi) { return clamp(params[p].getValue() + cv(in, hi - lo), lo, hi); }
	int typeNow() { return clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES_EXT - 1); }

	void applyTypeDefaults(int t) {
		const CymbalType& ct = CY_TYPES[t];
		params[WEIGHT_PARAM].setValue(1.f);
		params[TAPER_PARAM].setValue((float)ct.taper);
		params[PROFILE_PARAM].setValue((float)ct.rise);
		params[BELL_PARAM].setValue(1.f);
		params[FLANGE_PARAM].setValue((float)ct.flangeDeg);
		params[HOLE_PARAM].setValue((float)ct.hole);
	}
	void loadPlayDefaults() {
		const CymbalType& ct = CY_TYPES[typeNow()];
		params[PLAY_PARAM].setValue((float)ct.play);
		params[POSITION_PARAM].setValue((float)ct.position);
		params[SIZZLE_PARAM].setValue((float)ct.rivets);
	}
	void resetUnreal() {
		for (int p = MORPH_PARAM; p <= LOADPOS_PARAM; p++) params[p].setValue(paramQuantities[p]->getDefaultValue());
		params[TENSION_PARAM].setValue(0.f);
		params[FREEZE_PARAM].setValue(0.f);
		params[BLOOM_PARAM].setValue(1.f);
		params[PICKUP_PARAM].setValue(0.f);
		params[BOUNCE_PARAM].setValue(0.f);
	}

	GeomParams readGeometry(bool withCv) {
		GeomParams g;
		g.type = typeNow();
		auto P = [&](int p, int in, float lo, float hi) {
			float v = params[p].getValue() + (withCv && in >= 0 ? cv(in, hi - lo) : 0.f);
			return clamp(v, lo, hi);
		};
		// quantised so small CV wobble doesn't rebuild constantly
		g.weight = std::round(P(WEIGHT_PARAM, WEIGHT_INPUT, 0.2f, 5.f) * 40.f) / 40.f;
		g.taper = std::round(P(TAPER_PARAM, TAPER_INPUT, 0.4f, 3.f) * 40.f) / 40.f;
		g.rise = std::round(P(PROFILE_PARAM, PROFILE_INPUT, 0.f, 1.f) * 250.f) / 250.f;
		g.bell = std::round(P(BELL_PARAM, BELL_INPUT, 0.f, 3.f) * 40.f) / 40.f;
		g.flangeDeg = std::round(params[FLANGE_PARAM].getValue());
		g.hole = std::round(params[HOLE_PARAM].getValue() * 100.f) / 100.f;
		g.alloy = clamp((int)std::round(params[ALLOY_PARAM].getValue()), 0, NUM_ALLOYS_EXT - 1);
		g.tension = 0.0;   // TENSION is live here
		g.quality = quality;
		g.sampleRate = sampleRate > 0 ? sampleRate : 48000.f;
		// a SIZE knob above 1.5x rebuilds with a wider band so the top octave stays full
		// (capped so the model stays near 3500 modes)
		float sz = params[SIZE_PARAM].getValue();
		if (sz > 1.5f) {
			float want = std::round(sz * 4.f) / 4.f;
			int nNow = runner.nModes.load();
			double bandNow = lastGeom.band > 0 ? lastGeom.band : CY_SIZE_MAX;
			double nBase = nNow > 0 ? nNow * CY_SIZE_MAX / bandNow : 1500.0;
			double cap = std::max((double)CY_SIZE_MAX, CY_SIZE_MAX * 3500.0 / std::max(1.0, nBase));
			g.band = std::max((double)CY_SIZE_MAX, std::min((double)want, std::floor(cap * 4.0) / 4.0));
		}
		return g;
	}

	void readControls(Controls& c) {
		float lam = params[SIZE_PARAM].getValue();
		if (inputs[SIZE_INPUT].isConnected()) lam *= std::pow(2.f, -inputs[SIZE_INPUT].getVoltage());
		c.size = clamp(lam, 0.2f, 3.5f);
		float d = knob(DECAY_PARAM, DECAY_INPUT, 0.f, 1.f);
		c.decay = 0.25 * std::pow(16.0, d);
		c.bloom = knob(BLOOM_PARAM, BLOOM_INPUT, -2.f, 2.f);
		c.damp = knob(DAMP_PARAM, DAMP_INPUT, 0.f, 1.f);
		c.mount = knob(MOUNT_PARAM, MOUNT_INPUT, 0.f, 1.f);
		c.hammer = knob(HAMMER_PARAM, HAMMER_INPUT, 0.f, 1.f);
		c.mic = knob(MIC_PARAM, MIC_INPUT, 0.f, 4.f);
		c.sizzle = std::round(knob(SIZZLE_PARAM, SIZZLE_INPUT, 0.f, 12.f)) / 12.0;
		c.position = knob(POSITION_PARAM, POSITION_INPUT, 0.f, 1.f);
		c.hardness = knob(HARDNESS_PARAM, HARDNESS_INPUT, 0.f, 1.f);
		float f = knob(FORCE_PARAM, FORCE_INPUT, 0.f, 1.f);
		c.force = 0.2 * std::pow(60.0, f);
		if (inputs[VEL_INPUT].isConnected()) c.force *= clamp(inputs[VEL_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		float r = knob(RATE_PARAM, RATE_INPUT, 0.f, 1.f);
		c.rate = 2.0 * std::pow(15.0, r);
		c.excite = knob(EXC_PARAM, EXC_INPUT, 0.f, 1.f);
		c.roll = inputs[ROLL_INPUT].getVoltage() >= 1.f;
		c.choke = params[CHOKE_PARAM].getValue() > 0.5f || inputs[CHOKE_INPUT].getVoltage() >= 1.f;
		c.play = playNow();
		c.seed = seed;

		Unreal& u = c.u;
		u.morph = knob(MORPH_PARAM, MORPH_INPUT, 0.f, 1.f);
		u.load = knob(LOAD_PARAM, LOAD_INPUT, 0.f, 0.5f);
		u.loadPos = knob(LOADPOS_PARAM, LOADPOS_INPUT, 0.f, 1.f);
		u.tension = knob(TENSION_PARAM, TENSION_INPUT, -1.f, 1.f);
		// WARP: a wide dead zone at 1 (real)
		float w = params[WARP_PARAM].getValue() + cv(WARP_INPUT, 3.5f);
		if (!inputs[WARP_INPUT].isConnected() && std::fabs(w - 1.f) < 0.06f) w = 1.f;
		u.warp = clamp(w, -1.f, 2.5f);
		u.pivot = 100.0 * std::pow(50.0, knob(PIVOT_PARAM, PIVOT_INPUT, 0.f, 1.f));
		u.tune = knob(TUNE_PARAM, TUNE_INPUT, 0.f, 1.f);
		u.scale = clamp((int)std::round(params[SCALE_PARAM].getValue()), 0, NUM_SCALES - 1);
		u.root = params[ROOT_PARAM].getValue() + (inputs[ROOT_INPUT].isConnected() ? 12.f * inputs[ROOT_INPUT].getVoltage() : 0.f);
		u.root = u.root - 12.0 * std::floor(u.root / 12.0);
		// V/OCT: slewed about 5 ms (this runs every 32 samples), or held at strikes
		float vo = inputs[VOCT_INPUT].isConnected() ? inputs[VOCT_INPUT].getVoltage() : 0.f;
		float target = voctAtStrikes ? voctHeld : vo;
		float a = 1.f - std::exp(-32.f / (0.005f * std::max(1000.f, sampleRate)));
		voctSlew += (target - voctSlew) * a;
		if (std::fabs(voctSlew - target) < 1e-5f) voctSlew = target;
		u.voct = clamp(voctSlew, -5.f, 5.f);
		u.nChord = 0;
		if (inputs[CHORD_INPUT].isConnected()) {
			int ch = std::min(16, inputs[CHORD_INPUT].getChannels());
			for (int i = 0; i < ch; i++) u.chord[i] = inputs[CHORD_INPUT].getVoltage(i);
			u.nChord = ch;
		}
		u.tilt = knob(TILT_PARAM, TILT_INPUT, -1.f, 1.f);
		float sus = knob(SUSTAIN_PARAM, SUSTAIN_INPUT, 0.f, 1.f);
		if (inputs[HOLD_INPUT].isConnected()) sus = inputs[HOLD_INPUT].getVoltage() >= 1.f ? (sus > 0.f ? sus : 0.7f) : 0.f;
		u.sustain = sus;
		u.swell = 0.5 * std::pow(60.0, knob(SWELL_PARAM, SWELL_INPUT, 0.f, 1.f));
		u.colour = 60.0 * std::pow(166.67, knob(COLOUR_PARAM, COLOUR_INPUT, 0.f, 1.f));
		u.freeze = params[FREEZE_PARAM].getValue() > 0.5f || inputs[FREEZE_INPUT].getVoltage() >= 1.f;
		float fl = params[FLOW_PARAM].getValue() + cv(FLOW_INPUT, 2.f);
		if (!inputs[FLOW_INPUT].isConnected() && std::fabs(fl) < 0.04f) fl = 0.f;
		fl = clamp(fl, -1.f, 1.f);
		u.flow = fl < 0 ? std::pow(20.0, fl) : std::pow(10.0, fl);
		u.bounce = knob(BOUNCE_PARAM, BOUNCE_INPUT, 0.f, 1.f);
		u.width = knob(WIDTH_PARAM, WIDTH_INPUT, 0.f, 90.f) * (M_PI / 180.0);
		if (!outputs[R_OUTPUT].isConnected()) u.width = 0.0;   // mono: one mic, as Cymbolism
		u.pickup = knob(PICKUP_PARAM, PICKUP_INPUT, 0.f, 1.f);
	}

	int playNow() {
		float p = params[PLAY_PARAM].getValue();
		if (inputs[PLAY_INPUT].isConnected()) p += clamp(inputs[PLAY_INPUT].getVoltage(), 0.f, 10.f) * 0.1f * (NUM_PLAYS_EXT - 1);
		return clamp((int)std::round(p), 0, NUM_PLAYS_EXT - 1);
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
		if (strikeNow && inputs[VOCT_INPUT].isConnected()) voctHeld = inputs[VOCT_INPUT].getVoltage();

		if (ctlDiv.process()) {
			int t = typeNow();
			if (t != lastType) {
				if (lastType >= 0 && !keepGeometry) applyTypeDefaults(t);
				lastType = t;
				geomPending = true;
			}
			int tb = clamp((int)std::round(params[TYPEB_PARAM].getValue()), 0, NUM_TYPES_EXT - 1);
			readControls(runner.ctl);
			// TYPE B is only built while MORPH is in use
			int wantB = runner.ctl.u.morph > 0 ? tb : -1;
			if (wantB != lastTypeB && (wantB >= 0 || lastTypeB >= 0)) { lastTypeB = wantB; geomPending = true; }
			GeomParams g = readGeometry(rebuildOnCv);
			if (g.key() != lastGeom.key()) { lastGeom = g; geomPending = true; }
			if (geomPending && runner.requestBuild(lastGeom, runner.ctl, lastTypeB)) geomPending = false;
			lights[BUILD_LIGHT].setBrightness(runner.building ? 1.f : 0.f);
			lights[FREEZE_LIGHT].setBrightness(runner.ctl.u.freeze ? 1.f : 0.f);
			dispLam = (float)runner.ctl.size;
			dispPos = (float)runner.ctl.position;
			dispMic = (float)runner.ctl.mic;
			dispPlay = runner.ctl.play;
			dispRivets = (int)std::round(runner.ctl.sizzle * 12);
			dispU = runner.ctl.u;
		}
		float vel = 0.f;
		if (strikeNow) {
			if (!rebuildOnCv) {
				GeomParams g = readGeometry(true);
				if (g.key() != lastGeom.key()) { lastGeom = g; geomPending = true; }
			}
			runner.ctl.play = playNow();
			runner.ctl.position = knob(POSITION_PARAM, POSITION_INPUT, 0.f, 1.f);
			float f = knob(FORCE_PARAM, FORCE_INPUT, 0.f, 1.f);
			vel = 0.2f * std::pow(60.f, f);
			if (inputs[VEL_INPUT].isConnected()) vel *= clamp(inputs[VEL_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
			if (vel < 0.02f) vel = 0.f;
			strikeFlash = 1.f;
		}
		float exc = inputs[EXCITE_INPUT].getVoltage() * 0.2f;
		float pump = inputs[PUMP_INPUT].getVoltage();
		float ext[4];
		float y = runner.tick(exc, vel, pump, ext);
		float g = std::pow(10.f, trimDb / 20.f);
		y *= g;
		outputs[L_OUTPUT].setVoltage(y);
		outputs[R_OUTPUT].setVoltage(ext[0] * g);
		outputs[CORE_OUTPUT].setVoltage(ext[1] * g);
		outputs[WASH_OUTPUT].setVoltage(ext[2] * g);
		outputs[ENV_OUTPUT].setVoltage(ext[3]);
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
		json_object_set_new(root, "voctAtStrikes", json_boolean(voctAtStrikes));
		json_object_set_new(root, "trimDb", json_real(trimDb));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "seed"))) seed = (uint64_t)json_integer_value(j);
		if ((j = json_object_get(root, "quality"))) quality = clamp((int)json_integer_value(j), 0, 3);
		if ((j = json_object_get(root, "threaded"))) runner.threaded = json_boolean_value(j);
		if ((j = json_object_get(root, "keepGeometry"))) keepGeometry = json_boolean_value(j);
		if ((j = json_object_get(root, "rebuildOnCv"))) rebuildOnCv = json_boolean_value(j);
		if ((j = json_object_get(root, "voctAtStrikes"))) voctAtStrikes = json_boolean_value(j);
		if ((j = json_object_get(root, "trimDb"))) trimDb = (float)json_number_value(j);
		lastType = typeNow();
		geomPending = true;
	}
};

// ---- the screen: the cymbal from above, the strike point, the load and the mics, a spectrum,
//      the scale line and a strip that draws the bent frequency law

struct CymbolismExtendedDisplay : Widget {
	CymbolismExtended* module = nullptr;
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
		typedef CymbolismExtended X;

		int type = module ? module->typeNow() : 1;
		const CymbalType& ct = CY_TYPES[type];
		float lam = module ? module->dispLam : 1.f;
		float weight = module ? module->params[X::WEIGHT_PARAM].getValue() : 1.f;
		float bellX = module ? module->params[X::BELL_PARAM].getValue() : 1.f;
		float flange = module ? module->params[X::FLANGE_PARAM].getValue() : 0.f;
		float hole = module ? module->params[X::HOLE_PARAM].getValue() : 0.f;
		int alloy = module ? clamp((int)std::round(module->params[X::ALLOY_PARAM].getValue()), 0, NUM_ALLOYS_EXT - 1) : 0;
		int play = module ? module->dispPlay : ct.play;
		float pos = module ? module->dispPos : (float)ct.position;
		float mic = module ? module->dispMic : 1.f;
		int rivets = module ? module->dispRivets : 0;
		float swing = module ? module->runner.swing.load() : 0.f;
		Unreal u;
		if (module) u = module->dispU;
		else { u.tune = 0.7; u.warp = 0.8; }

		// ---- the cymbal from above (left)
		float cx = mm2px(20.f), cy = h * 0.5f + mm2px(1.f);
		float Rmax = mm2px(17.f);
		float inches = (float)ct.sizeIn * lam;
		float R = Rmax * clamp(0.40f + 0.60f * (inches - 4.f) / 40.f, 0.3f, 1.f);
		nvgSave(vg);
		nvgTranslate(vg, cx, cy);
		nvgRotate(vg, swing * 3.f);
		nvgBeginPath(vg);
		nvgCircle(vg, 0, 0, R);
		NVGcolor c0 = nvgRGB(176, 136, 70), c1 = nvgRGB(70, 50, 22);
		if (alloy == STEEL || alloy == ALUMINIUM) { c0 = nvgRGB(165, 165, 168); c1 = nvgRGB(60, 60, 62); }
		else if (alloy == NICKEL) { c0 = nvgRGB(175, 170, 150); }
		else if (alloy == GLASS) { c0 = nvgRGBA(190, 225, 220, 200); c1 = nvgRGBA(40, 70, 70, 200); }
		else if (alloy == WOOD) { c0 = nvgRGB(150, 105, 60); c1 = nvgRGB(60, 38, 20); }
		nvgFillPaint(vg, nvgRadialGradient(vg, -R * 0.25f, -R * 0.3f, R * 0.05f, R * 1.1f, c0, c1));
		nvgFill(vg);
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
		float holeR = std::max(std::max(1.5f, R * 0.04f), R * std::max(hole, hole > 0 ? 0.f : (float)ct.hole));
		nvgBeginPath(vg);
		nvgCircle(vg, 0, 0, holeR);
		nvgFillColor(vg, nvgRGB(10, 8, 5));
		nvgFill(vg);
		for (int i = 0; i < rivets; i++) {
			float a = 2.f * M_PI * i / std::max(1, rivets);
			nvgBeginPath(vg);
			nvgCircle(vg, 0.82f * R * std::cos(a), 0.82f * R * std::sin(a), 1.1f);
			nvgFillColor(vg, cream);
			nvgFill(vg);
		}
		// the load: 90 degrees round from the strike point
		if (u.load > 0) {
			float lr = R * (0.04f + 0.96f * (float)u.loadPos);
			nvgBeginPath(vg);
			nvgCircle(vg, 0, lr, 1.2f + 3.f * std::sqrt((float)u.load / 0.5f));
			nvgFillColor(vg, nvgRGBA(127, 165, 143, 220));
			nvgFill(vg);
		}
		nvgRestore(vg);
		float sr = R * (0.04f + 0.96f * pos);
		float glow = module ? module->strikeFlash : 0.f;
		nvgBeginPath(vg);
		nvgCircle(vg, cx + sr, cy, 2.0f + 2.5f * glow);
		nvgFillColor(vg, nvgRGBAf(1.f, 0.85f, 0.5f, 0.5f + 0.5f * glow));
		nvgFill(vg);
		// mics (two when WIDTH is open)
		float mr = mic <= 2.f ? R * (0.05f + 0.465f * mic) : R * (0.98f + 0.08f * (mic - 2.f));
		for (int sgn : {-1, 1}) {
			float a = (float)u.width * sgn;
			float mx = cx - mr * std::cos(a), my = cy - mr * std::sin(a) - (mic <= 2.f ? 0.f : mm2px(2.5f) * (mic - 2.f));
			nvgBeginPath(vg);
			nvgRect(vg, mx - 1.5f, my - 3.f, 3.f, 6.f);
			nvgFillColor(vg, u.pickup > 0.5 ? bronze : patina);
			nvgFill(vg);
			if (u.width == 0) break;
		}

		if (!font) { Widget::drawLayer(args, layer); return; }
		nvgFontFaceId(vg, font->handle);

		// ---- text (right)
		float x0 = mm2px(41.f), right = w - mm2px(2.f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(4.0f));
		nvgFillColor(vg, gold);
		nvgText(vg, x0, mm2px(5.4f), ct.name, NULL);
		nvgFontSize(vg, mm2px(2.5f));
		nvgFillColor(vg, cream);
		std::string l1 = string::f("%.1f\"  %.2f mm  %s", inches, ct.edgeMm * weight * lam, X_ALLOY_SHORT[alloy]);
		nvgText(vg, x0, mm2px(9.2f), l1.c_str(), NULL);
		nvgFillColor(vg, bronze);
		static const char* micNames[5] = {"BELL", "BOW", "EDGE", "OVERHEAD", "ROOM"};
		std::string l2 = string::f("%s  %s %s", CY_PLAY_NAMES_EXT[clamp(play, 0, NUM_PLAYS_EXT - 1)], u.pickup > 0.5 ? "piezo" : "mic",
		                           micNames[clamp((int)std::round(mic), 0, 4)]);
		nvgText(vg, x0, mm2px(12.6f), l2.c_str(), NULL);
		// the scale line
		nvgFillColor(vg, patina);
		std::string l3;
		if (u.tune > 0) {
			int root = (int)std::round(u.root) % 12;
			if (u.scale == SC_CHORD && u.nChord > 0) l3 = string::f("TUNE %d%%  CHORD %d notes", (int)std::round(u.tune * 100), u.nChord);
			else l3 = string::f("TUNE %d%%  %s %s", (int)std::round(u.tune * 100), CY_NOTE_NAMES[(root + 12) % 12], CY_SCALE_NAMES[u.scale]);
		}
		else l3 = "untuned";
		if (u.voct != 0) l3 += string::f("  %+.2f oct", u.voct);
		if (u.freeze) l3 += "  FROZEN";
		else if (u.sustain > 0) l3 += "  SUSTAIN";
		nvgText(vg, x0, mm2px(16.0f), l3.c_str(), NULL);

		// ---- spectrum
		float sx0 = x0, sx1 = right, sy0 = mm2px(18.0f), sy1 = h - mm2px(8.4f);
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
				in[i] = module->scope[(p - 1024 + i + X::SCOPE) % X::SCOPE] * win;
			}
			fft.rfft(in, out);
			float srt = module->sampleRate > 0 ? module->sampleRate : 48000.f;
			for (int b = 0; b < 64; b++) {
				float f0 = 40.f * std::pow(500.f, b / 64.f), f1 = 40.f * std::pow(500.f, (b + 1) / 64.f);
				int k0 = clamp((int)(f0 / srt * 1024), 1, 511), k1 = clamp((int)(f1 / srt * 1024), k0 + 1, 512);
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
		// ---- the frequency law strip: built pitch (x) against the pitch you hear (y), 40 Hz .. 20 kHz
		float ly0 = sy1 + mm2px(1.0f), ly1 = h - mm2px(3.4f);
		nvgBeginPath(vg);
		nvgRect(vg, sx0, ly0, sx1 - sx0, ly1 - ly0);
		nvgFillColor(vg, nvgRGBA(127, 165, 143, 22));
		nvgFill(vg);
		nvgBeginPath(vg);
		for (int i = 0; i <= 48; i++) {
			double fi = 40.0 * std::pow(500.0, i / 48.0);
			double fo = u.warp != 1.0 ? u.pivot * std::pow(fi / u.pivot, u.warp) : fi;
			fo *= std::pow(2.0, u.voct);
			float yv = clamp((float)(std::log(fo / 40.0) / std::log(500.0)), -0.1f, 1.1f);
			float xx = sx0 + (sx1 - sx0) * i / 48.f, yy = ly1 - (ly1 - ly0) * clamp(yv, 0.f, 1.f);
			if (i == 0) nvgMoveTo(vg, xx, yy);
			else nvgLineTo(vg, xx, yy);
		}
		nvgStrokeColor(vg, patina);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		// ---- status line
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(2.2f));
		std::string st;
		if (module) {
			if (module->runner.building) st = "BUILDING...";
			else if (!module->runner.hasModel) st = "waiting for the first model";
			else st = string::f("%d modes  core %d  cpu %d%%", module->runner.nModes.load(), module->runner.nCore.load(),
			                    (int)std::round(module->runner.cpu.load() * 100.f));
		}
		else st = "2215 modes  core 63";
		nvgFillColor(vg, module && module->runner.building ? gold : soft);
		nvgText(vg, mm2px(1.4f), h - mm2px(1.2f), st.c_str(), NULL);
		Widget::drawLayer(args, layer);
	}
};

struct CymbolismExtendedWidget : ModuleWidget {
	CymbolismExtendedWidget(CymbolismExtended* module) {
		typedef CymbolismExtended X;
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/CymbolismExtended.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		CymbolismExtendedDisplay* d = createWidget<CymbolismExtendedDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<BigSnapKnob>(mm2px(Vec(TYPE_X, TYPE_Y)), module, X::TYPE_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(TYPEB_X, TYPEB_Y)), module, X::TYPEB_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(NEW_X, NEW_Y)), module, X::NEW_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(NEW_X + 5.f, NEW_Y - 4.f)), module, X::BUILD_LIGHT));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(POS_X, BIG_Y)), module, X::POSITION_PARAM));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(FORCE_X, BIG_Y)), module, X::FORCE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(POS_X, BIGCV_Y)), module, X::POSITION_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(FORCE_X, BIGCV_Y)), module, X::FORCE_INPUT));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(BTN_X, STRIKE_Y)), module, X::STRIKE_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(BTN_X, CHOKE_Y)), module, X::CHOKE_PARAM));
		addParam(createParamCentered<LatchButton>(mm2px(Vec(BTN_X, FREEZE_Y)), module, X::FREEZE_PARAM));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(BTN_X + 5.f, FREEZE_Y - 4.f)), module, X::FREEZE_LIGHT));

		const int io1[8] = {X::STRIKE_INPUT, X::VEL_INPUT, X::CHOKE_INPUT, X::ROLL_INPUT, X::HOLD_INPUT, X::FREEZE_INPUT, X::EXCITE_INPUT, X::PUMP_INPUT};
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IO_X0 + IO_DX * i, IO1_Y)), module, io1[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IO_X0, IO2_Y)), module, X::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IO_X0 + IO_DX, IO2_Y)), module, X::CHORD_INPUT));
		for (int i = 0; i < 5; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IO_X0 + IO_DX * (3 + i), IO2_Y)), module, X::L_OUTPUT + i));

		// knob rows; -1 = no CV jack
		struct K { int p, in; bool snap; };
		const K row1[10] = {{X::SIZE_PARAM, X::SIZE_INPUT, false}, {X::WEIGHT_PARAM, X::WEIGHT_INPUT, false}, {X::TAPER_PARAM, X::TAPER_INPUT, false},
		                    {X::PROFILE_PARAM, X::PROFILE_INPUT, false}, {X::BELL_PARAM, X::BELL_INPUT, false}, {X::FLANGE_PARAM, -1, false},
		                    {X::HOLE_PARAM, -1, false}, {X::ALLOY_PARAM, -1, true}, {X::HAMMER_PARAM, X::HAMMER_INPUT, false},
		                    {X::TENSION_PARAM, X::TENSION_INPUT, false}};
		const K row2[13] = {{X::MORPH_PARAM, X::MORPH_INPUT, false}, {X::WARP_PARAM, X::WARP_INPUT, false}, {X::PIVOT_PARAM, X::PIVOT_INPUT, false},
		                    {X::TUNE_PARAM, X::TUNE_INPUT, false}, {X::SCALE_PARAM, -1, true}, {X::ROOT_PARAM, X::ROOT_INPUT, true},
		                    {X::TILT_PARAM, X::TILT_INPUT, false}, {X::SUSTAIN_PARAM, X::SUSTAIN_INPUT, false}, {X::SWELL_PARAM, X::SWELL_INPUT, false},
		                    {X::COLOUR_PARAM, X::COLOUR_INPUT, false}, {X::FLOW_PARAM, X::FLOW_INPUT, false}, {X::LOAD_PARAM, X::LOAD_INPUT, false},
		                    {X::LOADPOS_PARAM, X::LOADPOS_INPUT, false}};
		const K row3[13] = {{X::DECAY_PARAM, X::DECAY_INPUT, false}, {X::BLOOM_PARAM, X::BLOOM_INPUT, false}, {X::DAMP_PARAM, X::DAMP_INPUT, false},
		                    {X::MOUNT_PARAM, X::MOUNT_INPUT, false}, {X::SIZZLE_PARAM, X::SIZZLE_INPUT, true}, {X::MIC_PARAM, X::MIC_INPUT, false},
		                    {X::PICKUP_PARAM, X::PICKUP_INPUT, false}, {X::WIDTH_PARAM, X::WIDTH_INPUT, false}, {X::PLAY_PARAM, X::PLAY_INPUT, true},
		                    {X::HARDNESS_PARAM, X::HARDNESS_INPUT, false}, {X::RATE_PARAM, X::RATE_INPUT, false}, {X::BOUNCE_PARAM, X::BOUNCE_INPUT, false},
		                    {X::EXC_PARAM, X::EXC_INPUT, false}};
		auto place = [&](const K* r, int n, float y) {
			for (int i = 0; i < n; i++) {
				Vec p = mm2px(Vec(ROW_X0 + ROW_DX * i, y));
				if (r[i].snap) addParam(createParamCentered<RoundBlackSnapKnob>(p, module, r[i].p));
				else addParam(createParamCentered<RoundBlackKnob>(p, module, r[i].p));
				if (r[i].in >= 0) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ROW_X0 + ROW_DX * i, y + CV_DY)), module, r[i].in));
			}
		};
		place(row1, 10, ROW1_Y);
		place(row2, 13, ROW2_Y);
		place(row3, 13, ROW3_Y);
	}

	void appendContextMenu(Menu* menu) override {
		CymbolismExtended* m = getModule<CymbolismExtended>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		// menu order ECO, MID, HIGH, ULTRA; the saved codes are 0, 3, 1, 2 (MID came later)
		menu->addChild(createIndexSubmenuItem("Quality", {"ECO (lightest)", "MID", "HIGH (default)", "ULTRA (bigger nonlinear core, heavy)"},
			[=]() { static const int pos[4] = {0, 2, 3, 1}; return pos[clamp(m->quality, 0, 3)]; },
			[=](int i) { static const int code[4] = {0, 3, 1, 2}; m->quality = code[clamp(i, 0, 3)]; m->geomPending = true; m->lastGeom = GeomParams(); }));
		menu->addChild(createBoolMenuItem("Run on its own thread", "",
			[=]() { return (bool)m->runner.threaded; },
			[=](bool b) { m->runner.threaded = b; }));
		menu->addChild(createMenuLabel(string::f("Latency %.1f ms", m->runner.latency() * 64 * 1000.f / std::max(1.f, m->sampleRate))));
		menu->addChild(createBoolPtrMenuItem("Keep geometry when TYPE changes", "", &m->keepGeometry));
		menu->addChild(createBoolPtrMenuItem("Rebuild on geometry CV changes (off: read at strikes)", "", &m->rebuildOnCv));
		menu->addChild(createBoolPtrMenuItem("Tuning follows V/OCT only at strikes", "", &m->voctAtStrikes));
		menu->addChild(createMenuItem("Load the type's playing defaults", "", [=]() { m->loadPlayDefaults(); }));
		menu->addChild(createMenuItem("Reset the UNREAL section", "", [=]() { m->resetUnreal(); }));
		menu->addChild(createIndexSubmenuItem("Output trim", {"-12 dB", "-6 dB", "0 dB", "+6 dB", "+12 dB"},
			[=]() { return clamp((int)std::round(m->trimDb / 6.f) + 2, 0, 4); },
			[=](int i) { m->trimDb = 6.f * (i - 2); }));
		menu->addChild(createMenuLabel(string::f("This cymbal: #%llu", (unsigned long long)m->seed)));
	}
};

rack::plugin::Model* modelCymbolismExtended = createModel<CymbolismExtended, CymbolismExtendedWidget>("CymbolismExtended");
