// Heathrow: stereo phaser / flanger.
//   Engine A: MORPH phaser<->flanger, bipolar feedback, through-zero, an LFO
//             that can step and lock to a clock, envelope follower, SWEEP in,
//             CV with attenuverters over the main controls.
//   Engine B: a second engine (Bi-Phase idea) that can be switched out, run
//             in SERIES after A, in PARALLEL with A, or SPLIT (A left, B right).
//             Its sweep can be its own LFO, A's LFO, or A's LFO upside down.
//   Memory:   6 slots holding every setting; recall glides over TIME; SLOT CV
//             picks a slot.
// The sound engine is in HeathrowDSP.hpp.
#include "plugin.hpp"
#include "HeathrowDSP.hpp"

using namespace heathrow;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 7.0f, DISP_Y = 15.0f, DISP_W = 128.24f, DISP_H = 17.0f;
static const float COLX[5] = {15.12f, 43.12f, 71.12f, 99.12f, 127.12f};
static const float ROW1_Y = 46.0f;
static const float ROW2_Y = 67.0f;
static const float ROW3_Y = 84.5f;
static const float CV_X0 = 8.995f, CV_DX = 17.75f;
static const float TRIM_Y = 97.6f;
static const float CVJ_Y = 105.4f;
static const float IO_Y = 118.0f;
// engine B + memory block (right of x = 142.24)
static const float BX[4] = {151.235f, 168.985f, 186.735f, 204.485f};
static const float BSW_Y = 25.0f;
static const float BK1_Y = 44.0f;
static const float BK2_Y = 61.5f;
static const float MEM_Y = 79.0f;
static const float MEMX0 = 151.235f, MEMDX = 10.65f;
static const float MEM2_Y = 90.5f;

static const int NUM_SLOTS = 6;

struct Heathrow : Module {
	enum ParamIds {
		MORPH_PARAM, CENTER_PARAM, DEPTH_PARAM, RATE_PARAM, FEEDBACK_PARAM,
		STAGES_PARAM, SPREAD_PARAM, SHAPE_PARAM, STEPS_PARAM, MIX_PARAM,
		GLIDE_PARAM, ENV_PARAM, RES_PARAM, TZ_PARAM, CLOCKMODE_PARAM,
		MORPH_ATT, CENTER_ATT, DEPTH_ATT, RATE_ATT, FEEDBACK_ATT, STAGES_ATT, SPREAD_ATT, MIX_ATT,
		B_ON_PARAM, B_ROUTE_PARAM, B_LFO_PARAM,
		B_MORPH_PARAM, B_CENTER_PARAM, B_DEPTH_PARAM, B_RATE_PARAM, B_FEEDBACK_PARAM, B_STAGES_PARAM, B_MIX_PARAM,
		NUM_STORED,                       // everything above is saved in memory slots
		MEM_PARAM = NUM_STORED,
		SAVE_PARAM = MEM_PARAM + NUM_SLOTS,
		TIME_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		MORPH_INPUT, CENTER_INPUT, DEPTH_INPUT, RATE_INPUT, FEEDBACK_INPUT, STAGES_INPUT, SPREAD_INPUT, MIX_INPUT,
		IN_L_INPUT, IN_R_INPUT, CLOCK_INPUT, RESET_INPUT, SWEEP_INPUT,
		B_MORPH_INPUT, B_CENTER_INPUT, B_DEPTH_INPUT, B_RATE_INPUT, B_FEEDBACK_INPUT, B_STAGES_INPUT, B_MIX_INPUT,
		SLOT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LFO_OUTPUT, OUT_L_OUTPUT, OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLOCK_LIGHT,
		MEM_LIGHT,
		SAVE_LIGHT = MEM_LIGHT + NUM_SLOTS,
		NUM_LIGHTS
	};
	enum Route { SERIES = 0, PARALLEL = 1, SPLIT = 2 };   // B_ROUTE_PARAM; CKSSThree 0 = down
	enum BLfo { B_OWN = 0, B_FOLLOW = 1, B_INVERT = 2 };  // B_LFO_PARAM

	Channel chA[2], chB[2];
	Lfo lfoA, lfoB;
	Follower follower;
	dsp::SchmittTrigger clockTrig, resetTrig, saveTrig, memTrig[NUM_SLOTS];
	float sweepA[2] = {0.5f, 0.5f}, sweepB[2] = {0.5f, 0.5f};
	int64_t sampleCount = 0, lastClock = -1;
	float clockPeriod = 0.f;
	int clockCountA = 0, clockCountB = 0;
	float clockLight = 0.f;

	// memory
	bool slotUsed[NUM_SLOTS] = {};
	float slotVal[NUM_SLOTS][NUM_STORED] = {};
	int currentSlot = -1;
	bool saveArmed = false;
	float blink = 0.f;
	int lastCvSlot = -1;
	bool morphing = false;
	float morphFrom[NUM_STORED] = {}, morphTo[NUM_STORED] = {};
	float morphPos = 0.f, morphSecs = 0.f;
	int memDiv = 0;

	// read by the screen
	Settings dispA, dispB;
	float dispSweepA = 0.5f, dispSweepB = 0.5f, dispRateHz = 0.5f, dispSpread = 0.f, dispShape = 0.f;
	int dispRatio = 9, dispSteps = 0, dispRoute = 0;
	bool dispClock = false, dispExt = false, dispBOn = false, dispMorphing = false;

	static bool isSwitch(int p) {
		return p == STEPS_PARAM || p == TZ_PARAM || p == CLOCKMODE_PARAM
		       || p == B_ON_PARAM || p == B_ROUTE_PARAM || p == B_LFO_PARAM;
	}

	Heathrow() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		// ---- engine A
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "A morph (phaser to flanger)", "%", 0.f, 100.f);
		configParam(CENTER_PARAM, 0.f, 1.f, 0.5f, "A center (where the sweep sits)", "%", 0.f, 100.f);
		configParam(DEPTH_PARAM, 0.f, 1.f, 0.6f, "A depth (how far it sweeps)", "%", 0.f, 100.f);
		configParam(RATE_PARAM, 0.f, 1.f, 0.3f, "A rate (free: 0.01-20 Hz; clocked: cycles per clock)");
		configParam(FEEDBACK_PARAM, -1.f, 1.f, 0.f, "A feedback (negative = hollow, positive = ringing)", "%", 0.f, 97.f);
		configParam(STAGES_PARAM, 1.f, (float) MAX_SECTIONS, 4.f, "A phaser stages", "", 0.f, 2.f);
		getParamQuantity(STAGES_PARAM)->snapEnabled = true;
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.25f, "Stereo spread (right channel LFO offset)", "°", 0.f, 180.f);
		configParam(SHAPE_PARAM, 0.f, (float) (NUM_SHAPES - 1), 0.f,
		            "LFO shape (sine, triangle, ramp, square, drift, S&H; in-between blends)");
		{
			std::vector<std::string> names = {"Smooth"};
			for (int i = 2; i <= 16; i++) names.push_back(string::f("%d steps", i));
			configSwitch(STEPS_PARAM, 0.f, 15.f, 0.f, "Steps (sweep moves in jumps)", names);
		}
		configParam(MIX_PARAM, -1.f, 1.f, 0.5f, "A mix (centre dry, half-way deepest, ends wet only; negative flips the wet)", "%", 0.f, 100.f);
		configParam(GLIDE_PARAM, 0.f, 1.f, 0.f, "Glide (smooths the sweep, 0-1 s)", "%", 0.f, 100.f);
		configParam(ENV_PARAM, -1.f, 1.f, 0.f, "Envelope (input level pushes the sweep up or down)", "%", 0.f, 100.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.f, "Resonance (sharper phaser notches)", "%", 0.f, 100.f);
		configSwitch(TZ_PARAM, 0.f, 1.f, 0.f, "Through-zero flanging", {"Off", "On"});
		configSwitch(CLOCKMODE_PARAM, 0.f, 1.f, 0.f, "LFO", {"Free", "Clocked"});
		const char* attNames[8] = {"Morph", "Center", "Depth", "Rate", "Feedback", "Stages", "Spread", "Mix"};
		for (int i = 0; i < 8; i++) {
			configParam(MORPH_ATT + i, -1.f, 1.f, 1.f, string::f("A %s CV amount", attNames[i]), "%", 0.f, 100.f);
			configInput(MORPH_INPUT + i, string::f("A %s CV", attNames[i]));
		}
		configInput(RATE_INPUT, "A rate CV (1 V/oct in free mode, up to audio rate)");
		configInput(IN_L_INPUT, "Left (or mono)");
		configInput(IN_R_INPUT, "Right (normalled to left)");
		configInput(CLOCK_INPUT, "Clock (used when LFO is set to CLK)");
		configInput(RESET_INPUT, "Reset (LFOs back to the start of their cycle)");
		configInput(SWEEP_INPUT, "Sweep (replaces A's LFO, +-5 V; 2-channel poly = separate right)");
		configOutput(LFO_OUTPUT, "A LFO (+-5 V, before depth)");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);

		// ---- engine B
		configSwitch(B_ON_PARAM, 0.f, 1.f, 0.f, "Engine B", {"Off", "On"});
		configSwitch(B_ROUTE_PARAM, 0.f, 2.f, 0.f, "B routing", {"Series (A then B)", "Parallel (A + B)", "Split (A left, B right)"});
		configSwitch(B_LFO_PARAM, 0.f, 2.f, 0.f, "B sweep", {"Own LFO (B rate)", "Follows A", "A upside down"});
		configParam(B_MORPH_PARAM, 0.f, 1.f, 0.f, "B morph (phaser to flanger)", "%", 0.f, 100.f);
		configParam(B_CENTER_PARAM, 0.f, 1.f, 0.5f, "B center", "%", 0.f, 100.f);
		configParam(B_DEPTH_PARAM, 0.f, 1.f, 0.6f, "B depth", "%", 0.f, 100.f);
		configParam(B_RATE_PARAM, 0.f, 1.f, 0.4f, "B rate (own LFO; free: 0.01-20 Hz; clocked: cycles per clock)");
		configParam(B_FEEDBACK_PARAM, -1.f, 1.f, 0.f, "B feedback", "%", 0.f, 97.f);
		configParam(B_STAGES_PARAM, 1.f, (float) MAX_SECTIONS, 3.f, "B phaser stages", "", 0.f, 2.f);
		getParamQuantity(B_STAGES_PARAM)->snapEnabled = true;
		configParam(B_MIX_PARAM, -1.f, 1.f, 0.5f, "B mix", "%", 0.f, 100.f);
		configInput(B_MORPH_INPUT, "B morph CV (10 V = full)");
		configInput(B_CENTER_INPUT, "B center CV (10 V = full)");
		configInput(B_DEPTH_INPUT, "B depth CV (10 V = full)");
		configInput(B_RATE_INPUT, "B rate CV (1 V/oct)");
		configInput(B_FEEDBACK_INPUT, "B feedback CV (+-5 V = full)");
		configInput(B_STAGES_INPUT, "B stages CV (10 V = all)");
		configInput(B_MIX_INPUT, "B mix CV (+-5 V = full)");

		// ---- memory
		for (int i = 0; i < NUM_SLOTS; i++)
			configButton(MEM_PARAM + i, string::f("Memory %d (tap to recall; after SAVE, tap to store)", i + 1));
		configButton(SAVE_PARAM, "Save (then tap a slot; tap SAVE again to cancel)");
		configParam(TIME_PARAM, 0.f, 1.f, 0.f, "Recall time (0 = jump, then 10 ms to 20 s)", "%", 0.f, 100.f);
		configInput(SLOT_INPUT, "Slot CV (0-10 V picks slot 1-6; a change recalls it)");
	}

	static float recallSeconds(float t) {
		if (t <= 0.001f) return 0.f;
		return 0.01f * std::pow(2000.f, t);   // 10 ms .. 20 s
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int c = 0; c < 2; c++) { chA[c].reset(); chB[c].reset(); }
		lfoA.reset(); lfoB.reset();
		clockCountA = clockCountB = 0;
		for (int i = 0; i < NUM_SLOTS; i++) slotUsed[i] = false;
		currentSlot = -1;
		saveArmed = morphing = false;
	}

	float cv(int input, int att, float scale) {
		return inputs[input].getVoltage() * scale * params[att].getValue();
	}

	static float softLimit(float v) {
		float a = std::fabs(v);
		if (a < 8.f) return v;
		float y = 8.f + 3.f * std::tanh((a - 8.f) / 3.f);
		return v < 0.f ? -y : y;
	}

	void store(int slot) {
		for (int p = 0; p < NUM_STORED; p++) slotVal[slot][p] = params[p].getValue();
		slotUsed[slot] = true;
		currentSlot = slot;
	}

	void recall(int slot) {
		if (slot < 0 || slot >= NUM_SLOTS || !slotUsed[slot]) return;
		currentSlot = slot;
		float secs = recallSeconds(params[TIME_PARAM].getValue());
		for (int p = 0; p < NUM_STORED; p++) {
			morphFrom[p] = params[p].getValue();
			morphTo[p] = slotVal[slot][p];
			if (isSwitch(p) || secs <= 0.f) params[p].setValue(morphTo[p]);
		}
		morphing = secs > 0.f;
		morphPos = 0.f;
		morphSecs = secs;
	}

	void processMemory(const ProcessArgs& args) {
		// buttons
		bool save = saveTrig.process(params[SAVE_PARAM].getValue());
		if (save) saveArmed = !saveArmed;
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (memTrig[i].process(params[MEM_PARAM + i].getValue())) {
				if (saveArmed) { store(i); saveArmed = false; }
				else recall(i);
			}
		}
		// slot CV: a change of slot recalls it
		if (inputs[SLOT_INPUT].isConnected()) {
			int s = clamp((int) (inputs[SLOT_INPUT].getVoltage() * 0.6f), 0, NUM_SLOTS - 1);
			if (s != lastCvSlot) {
				lastCvSlot = s;
				recall(s);
			}
		}
		else lastCvSlot = -1;

		// glide between settings (switches already jumped)
		if (morphing) {
			morphPos += args.sampleTime * 16.f / morphSecs;
			float t = std::min(morphPos, 1.f);
			float e = t * t * (3.f - 2.f * t);   // ease in/out
			for (int p = 0; p < NUM_STORED; p++)
				if (!isSwitch(p)) params[p].setValue(morphFrom[p] + (morphTo[p] - morphFrom[p]) * e);
			if (t >= 1.f) morphing = false;
		}

		// lights
		blink += args.sampleTime * 16.f;
		if (blink > 1.f) blink -= 1.f;
		for (int i = 0; i < NUM_SLOTS; i++) {
			float b = i == currentSlot ? 1.f : (slotUsed[i] ? 0.2f : 0.f);
			if (saveArmed) b = blink < 0.5f ? 0.8f : 0.05f;
			lights[MEM_LIGHT + i].setBrightness(b);
		}
		lights[SAVE_LIGHT].setBrightness(saveArmed ? 1.f : 0.f);
	}

	// LFO speed for one engine; handles clock lock
	float lfoRate(float rateKnob, float rateCv, bool clocked, bool clockEdge, Lfo& lfo, int& count, int& ratio, float sr) {
		if (clocked) {
			float r = clamp(rateKnob + rateCv * 0.1f, 0.f, 1.f);
			ratio = clamp((int) std::round(r * (NUM_RATIOS - 1)), 0, NUM_RATIOS - 1);
			if (clockEdge) {
				count++;
				if (count >= RATIO_CLK[ratio]) {
					count = 0;
					lfo.phase = 0.0;
				}
			}
			return clockPeriod > 0.f ? RATIO_CYC[ratio] / (RATIO_CLK[ratio] * clockPeriod) : 0.f;
		}
		float oct = rateKnob * std::log2(2000.f) + rateCv;
		return clamp(0.01f * std::pow(2.f, oct), 0.001f, 2000.f);
	}

	void process(const ProcessArgs& args) override {
		float sr = args.sampleRate;
		sampleCount++;
		if (++memDiv >= 16) {
			memDiv = 0;
			processMemory(args);
		}

		// ---- engine A settings
		Settings st;
		st.morph = clamp(params[MORPH_PARAM].getValue() + cv(MORPH_INPUT, MORPH_ATT, 0.1f), 0.f, 1.f);
		st.feedback = 0.97f * clamp(params[FEEDBACK_PARAM].getValue() + cv(FEEDBACK_INPUT, FEEDBACK_ATT, 0.2f), -1.f, 1.f);
		st.sections = clamp((int) std::round(params[STAGES_PARAM].getValue() + cv(STAGES_INPUT, STAGES_ATT, 1.1f)), 1, MAX_SECTIONS);
		st.q = resToQ(params[RES_PARAM].getValue());
		st.mix = clamp(params[MIX_PARAM].getValue() + cv(MIX_INPUT, MIX_ATT, 0.2f), -1.f, 1.f);
		st.tz = params[TZ_PARAM].getValue() > 0.5f;
		float center = params[CENTER_PARAM].getValue() + cv(CENTER_INPUT, CENTER_ATT, 0.1f);
		float depth = clamp(params[DEPTH_PARAM].getValue() + cv(DEPTH_INPUT, DEPTH_ATT, 0.1f), 0.f, 1.f);
		float spread = clamp(params[SPREAD_PARAM].getValue() + cv(SPREAD_INPUT, SPREAD_ATT, 0.1f), 0.f, 1.f);
		float shape = params[SHAPE_PARAM].getValue();
		int stepsIdx = (int) params[STEPS_PARAM].getValue();
		int steps = stepsIdx == 0 ? 0 : stepsIdx + 1;
		bool clocked = params[CLOCKMODE_PARAM].getValue() > 0.5f;

		// ---- engine B settings (shares RES, TZ, SHAPE, STEPS, SPREAD, GLIDE, ENV with A)
		bool bOn = params[B_ON_PARAM].getValue() > 0.5f;
		int route = (int) std::round(params[B_ROUTE_PARAM].getValue());
		int bLfoMode = (int) std::round(params[B_LFO_PARAM].getValue());
		Settings sb;
		sb.morph = clamp(params[B_MORPH_PARAM].getValue() + inputs[B_MORPH_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		sb.feedback = 0.97f * clamp(params[B_FEEDBACK_PARAM].getValue() + inputs[B_FEEDBACK_INPUT].getVoltage() * 0.2f, -1.f, 1.f);
		sb.sections = clamp((int) std::round(params[B_STAGES_PARAM].getValue() + inputs[B_STAGES_INPUT].getVoltage() * 1.1f), 1, MAX_SECTIONS);
		sb.q = st.q;
		sb.mix = clamp(params[B_MIX_PARAM].getValue() + inputs[B_MIX_INPUT].getVoltage() * 0.2f, -1.f, 1.f);
		sb.tz = st.tz;
		float centerB = params[B_CENTER_PARAM].getValue() + inputs[B_CENTER_INPUT].getVoltage() * 0.1f;
		float depthB = clamp(params[B_DEPTH_PARAM].getValue() + inputs[B_DEPTH_INPUT].getVoltage() * 0.1f, 0.f, 1.f);

		// ---- clock + reset
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			lfoA.reset(); lfoB.reset();
			clockCountA = clockCountB = 0;
		}
		bool clockEdge = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		if (clockEdge) {
			if (lastClock >= 0) {
				float p = (sampleCount - lastClock) / sr;
				if (p > 0.002f && p < 20.f) clockPeriod = p;
			}
			lastClock = sampleCount;
			clockLight = 1.f;
		}
		clockLight = std::max(0.f, clockLight - args.sampleTime * 10.f);

		// ---- LFOs
		int ratioA = 9, ratioB = 9;
		float rateA = lfoRate(params[RATE_PARAM].getValue(), cv(RATE_INPUT, RATE_ATT, 1.f), clocked, clockEdge, lfoA, clockCountA, ratioA, sr);
		lfoA.advance(rateA / sr);
		if (bOn && bLfoMode == B_OWN) {
			float rateB = lfoRate(params[B_RATE_PARAM].getValue(), inputs[B_RATE_INPUT].getVoltage(), clocked, clockEdge, lfoB, clockCountB, ratioB, sr);
			lfoB.advance(rateB / sr);
		}

		// ---- sweep sources
		float modA[2], modB[2];
		bool ext = inputs[SWEEP_INPUT].isConnected();
		if (ext) {
			float a = inputs[SWEEP_INPUT].getVoltage(0) * 0.2f;
			float b = inputs[SWEEP_INPUT].getChannels() > 1 ? inputs[SWEEP_INPUT].getVoltage(1) * 0.2f
			                                                : a * (1.f - 2.f * spread);
			if (steps > 0) {
				float q = steps * 0.5f;
				a = std::round(a * q) / q;
				b = std::round(b * q) / q;
			}
			modA[0] = a; modA[1] = b;
		}
		else {
			modA[0] = lfoA.value(0.f, shape, steps);
			modA[1] = lfoA.value(spread * 0.5f, shape, steps);
		}
		if (bLfoMode == B_OWN) {
			modB[0] = lfoB.value(0.f, shape, steps);
			modB[1] = lfoB.value(spread * 0.5f, shape, steps);
		}
		else {
			float sgn = bLfoMode == B_INVERT ? -1.f : 1.f;
			modB[0] = sgn * modA[0];
			modB[1] = sgn * modA[1];
		}
		outputs[LFO_OUTPUT].setVoltage(5.f * lfoA.value(0.f, shape, steps));

		// ---- audio
		float inL = inputs[IN_L_INPUT].getVoltage();
		float inR = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() : inL;
		float env = follower.process(0.5f * (inL + inR), sr) * params[ENV_PARAM].getValue();

		float g = params[GLIDE_PARAM].getValue();
		float coef = g <= 0.001f ? 1.f : 1.f - std::exp(-args.sampleTime / (0.001f * std::pow(1000.f, g)));
		for (int c = 0; c < 2; c++) {
			sweepA[c] += (clamp(center + 0.5f * depth * modA[c] + env, 0.f, 1.f) - sweepA[c]) * coef;
			sweepB[c] += (clamp(centerB + 0.5f * depthB * modB[c] + env, 0.f, 1.f) - sweepB[c]) * coef;
		}

		float out[2];
		float in[2] = {inL, inR};
		if (!bOn) {
			for (int c = 0; c < 2; c++) out[c] = chA[c].process(in[c], sweepA[c], st, sr);
		}
		else if (route == SERIES) {
			for (int c = 0; c < 2; c++) out[c] = chB[c].process(chA[c].process(in[c], sweepA[c], st, sr), sweepB[c], sb, sr);
		}
		else if (route == PARALLEL) {
			for (int c = 0; c < 2; c++)
				out[c] = 0.5f * (chA[c].process(in[c], sweepA[c], st, sr) + chB[c].process(in[c], sweepB[c], sb, sr));
		}
		else {
			// split: A on the left, B on the right (mono in -> Bi-Phase stereo)
			out[0] = chA[0].process(inL, sweepA[0], st, sr);
			out[1] = chB[1].process(inR, sweepB[1], sb, sr);
		}
		outputs[OUT_L_OUTPUT].setVoltage(softLimit(out[0]));
		outputs[OUT_R_OUTPUT].setVoltage(softLimit(out[1]));
		lights[CLOCK_LIGHT].setBrightness(clocked ? clockLight : 0.f);

		dispA = st; dispB = sb;
		dispSweepA = sweepA[0];
		dispSweepB = route == SPLIT ? sweepB[1] : sweepB[0];
		dispRateHz = rateA;
		dispRatio = ratioA;
		dispClock = clocked;
		dispExt = ext;
		dispSteps = steps;
		dispShape = shape;
		dispSpread = spread;
		dispBOn = bOn;
		dispRoute = route;
		dispMorphing = morphing;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* slots = json_array();
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (!slotUsed[i]) { json_array_append_new(slots, json_null()); continue; }
			json_t* o = json_object();
			for (int p = 0; p < NUM_STORED; p++)
				json_object_set_new(o, string::f("p%d", p).c_str(), json_real(slotVal[i][p]));
			json_array_append_new(slots, o);
		}
		json_object_set_new(root, "slots", slots);
		json_object_set_new(root, "currentSlot", json_integer(currentSlot));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* slots = json_object_get(root, "slots");
		if (slots) {
			for (int i = 0; i < NUM_SLOTS && i < (int) json_array_size(slots); i++) {
				json_t* o = json_array_get(slots, i);
				slotUsed[i] = json_is_object(o);
				if (!slotUsed[i]) continue;
				for (int p = 0; p < NUM_STORED; p++) {
					json_t* v = json_object_get(o, string::f("p%d", p).c_str());
					slotVal[i][p] = v ? (float) json_number_value(v) : params[p].getValue();
				}
			}
		}
		json_t* cs = json_object_get(root, "currentSlot");
		if (cs) currentSlot = (int) json_integer_value(cs);
	}
};

// The screen: live response of A (cyan) and B (amber), sweep markers, status.
struct HeathrowDisplay : Widget {
	Heathrow* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		Settings sa, sb;
		float s = 0.55f, s2 = 0.35f, rate = 0.25f, spread = 0.25f, shape = 0.f;
		int ratio = 9, steps = 0, route = 0;
		bool clk = false, ext = false, bOn = false, morphing = false;
		float sr = 48000.f;
		if (module) {
			sa = module->dispA; sb = module->dispB; s = module->dispSweepA; s2 = module->dispSweepB;
			rate = module->dispRateHz; ratio = module->dispRatio; clk = module->dispClock; ext = module->dispExt;
			steps = module->dispSteps; shape = module->dispShape; spread = module->dispSpread;
			bOn = module->dispBOn; route = module->dispRoute; morphing = module->dispMorphing;
			sr = APP->engine->getSampleRate();
		}
		else {
			sa.sections = 4; sa.feedback = -0.6f; sa.mix = 0.5f;
		}
		float w = box.size.x, h = box.size.y;
		float pad = mm2px(1.6f);
		float top = pad, bot = h - mm2px(4.6f);
		const NVGcolor cyan = nvgRGB(111, 211, 255), amber = nvgRGB(255, 154, 60);
		auto xOf = [&](float hz) { return pad + (w - 2 * pad) * std::log(clamp(hz, 20.f, 20000.f) / 20.f) / std::log(1000.f); };
		auto dbY = [&](float db) { return top + (bot - top) * (12.f - clamp(db, -36.f, 12.f)) / 48.f; };

		nvgStrokeWidth(args.vg, 0.6f);
		nvgStrokeColor(args.vg, nvgRGBA(111, 211, 255, 28));
		for (float hz = 31.25f; hz < 20000.f; hz *= 2.f) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, xOf(hz), top);
			nvgLineTo(args.vg, xOf(hz), bot);
			nvgStroke(args.vg);
		}
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, pad, dbY(0.f));
		nvgLineTo(args.vg, w - pad, dbY(0.f));
		nvgStroke(args.vg);

		auto curve = [&](const Settings& st, float sw, NVGcolor col, float width) {
			const int N = 160;
			nvgBeginPath(args.vg);
			for (int i = 0; i < N; i++) {
				float f = (float) i / (N - 1);
				float hz = 20.f * std::pow(1000.f, f);
				float y = dbY(responseDb(hz, sw, st, sr));
				float x = pad + (w - 2 * pad) * f;
				if (i == 0) nvgMoveTo(args.vg, x, y);
				else nvgLineTo(args.vg, x, y);
			}
			nvgStrokeColor(args.vg, col);
			nvgStrokeWidth(args.vg, width);
			nvgLineJoin(args.vg, NVG_ROUND);
			nvgStroke(args.vg);
			float markHz = st.morph < 0.5f ? phaserHz(sw) : 1000.f / (2.f * std::max(0.02f, st.tz ? flangerTzMs(sw) : flangerMs(sw)));
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, xOf(markHz), top);
			nvgLineTo(args.vg, xOf(markHz), bot);
			nvgStrokeColor(args.vg, nvgTransRGBA(col, 110));
			nvgStrokeWidth(args.vg, 1.0f);
			nvgStroke(args.vg);
		};
		if (bOn) curve(sb, s2, amber, 1.1f);
		curve(sa, s, cyan, 1.4f);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.9f));
			std::string left;
			if (sa.morph < 0.02f) left = "PHASER";
			else if (sa.morph > 0.98f) left = "FLANGER";
			else left = string::f("MORPH %d%%", (int) std::round(sa.morph * 100.f));
			if (sa.morph < 0.98f) left += string::f(" %d STG", sa.sections * 2);
			left += string::f(" FB %+d%%", (int) std::round(sa.feedback / 0.97f * 100.f));
			if (sa.tz) left += " TZ";
			if (bOn) {
				const char* rn[3] = {"B SERIES", "B PARALLEL", "B SPLIT"};
				left += std::string("  ") + rn[clamp(route, 0, 2)];
			}
			std::string right;
			if (morphing) right = "RECALLING ";
			if (ext) right += "EXT SWEEP";
			else {
				int si = clamp((int) std::round(shape), 0, NUM_SHAPES - 1);
				right += std::string(SHAPE_NAMES[si]) + " ";
				if (clk) {
					if (RATIO_CLK[ratio] == 1) right += string::f("CLK %dx", (int) RATIO_CYC[ratio]);
					else right += string::f("CLK 1/%d", RATIO_CLK[ratio]);
				}
				else if (rate < 1.f) right += string::f("%.2f Hz", rate);
				else if (rate < 100.f) right += string::f("%.1f Hz", rate);
				else right += string::f("%d Hz", (int) rate);
			}
			if (steps > 0) right += string::f(" %d STEPS", steps);
			right += string::f(" %d°", (int) std::round(spread * 180.f));
			float by = h - mm2px(1.3f);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, cyan);
			nvgText(args.vg, pad, by, left.c_str(), NULL);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, amber);
			nvgText(args.vg, w - pad, by, right.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct HeathrowWidget : ModuleWidget {
	HeathrowWidget(Heathrow* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Heathrow.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		HeathrowDisplay* d = createWidget<HeathrowDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		// ---- engine A
		const int row1[5] = {Heathrow::MORPH_PARAM, Heathrow::CENTER_PARAM, Heathrow::DEPTH_PARAM, Heathrow::RATE_PARAM, Heathrow::FEEDBACK_PARAM};
		const int row2[5] = {Heathrow::STAGES_PARAM, Heathrow::SPREAD_PARAM, Heathrow::SHAPE_PARAM, Heathrow::STEPS_PARAM, Heathrow::MIX_PARAM};
		for (int i = 0; i < 5; i++) {
			addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COLX[i], ROW1_Y)), module, row1[i]));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[i], ROW2_Y)), module, row2[i]));
		}
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[0], ROW3_Y)), module, Heathrow::GLIDE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[1], ROW3_Y)), module, Heathrow::ENV_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[2], ROW3_Y)), module, Heathrow::RES_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(COLX[3] - 3.f, ROW3_Y)), module, Heathrow::TZ_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(COLX[4] - 3.f, ROW3_Y)), module, Heathrow::CLOCKMODE_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(COLX[4] + 8.5f, ROW3_Y - 3.f)), module, Heathrow::CLOCK_LIGHT));

		for (int i = 0; i < 8; i++) {
			float x = CV_X0 + CV_DX * i;
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, TRIM_Y)), module, Heathrow::MORPH_ATT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, CVJ_Y)), module, Heathrow::MORPH_INPUT + i));
		}
		const int ins[5] = {Heathrow::IN_L_INPUT, Heathrow::IN_R_INPUT, Heathrow::CLOCK_INPUT, Heathrow::RESET_INPUT, Heathrow::SWEEP_INPUT};
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CV_X0 + CV_DX * i, IO_Y)), module, ins[i]));
		const int outs[3] = {Heathrow::LFO_OUTPUT, Heathrow::OUT_L_OUTPUT, Heathrow::OUT_R_OUTPUT};
		for (int i = 0; i < 3; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(CV_X0 + CV_DX * (5 + i), IO_Y)), module, outs[i]));

		// ---- engine B
		addParam(createParamCentered<CKSS>(mm2px(Vec(BX[0] - 2.f, BSW_Y)), module, Heathrow::B_ON_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(BX[1] - 4.f, BSW_Y)), module, Heathrow::B_ROUTE_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(BX[2] + 2.f, BSW_Y)), module, Heathrow::B_LFO_PARAM));
		const int bk1[4] = {Heathrow::B_MORPH_PARAM, Heathrow::B_CENTER_PARAM, Heathrow::B_DEPTH_PARAM, Heathrow::B_RATE_PARAM};
		const int bk2[3] = {Heathrow::B_FEEDBACK_PARAM, Heathrow::B_STAGES_PARAM, Heathrow::B_MIX_PARAM};
		for (int i = 0; i < 4; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(BX[i], BK1_Y)), module, bk1[i]));
		for (int i = 0; i < 3; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(BX[i], BK2_Y)), module, bk2[i]));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(BX[1], MEM2_Y)), module, Heathrow::TIME_PARAM));
		const int bcv[4] = {Heathrow::B_MORPH_INPUT, Heathrow::B_CENTER_INPUT, Heathrow::B_DEPTH_INPUT, Heathrow::B_RATE_INPUT};
		const int bcv2[4] = {Heathrow::B_FEEDBACK_INPUT, Heathrow::B_STAGES_INPUT, Heathrow::B_MIX_INPUT, Heathrow::SLOT_INPUT};
		for (int i = 0; i < 4; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BX[i], CVJ_Y)), module, bcv[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(BX[i], IO_Y)), module, bcv2[i]));
		}

		// ---- memory
		for (int i = 0; i < NUM_SLOTS; i++)
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(MEMX0 + MEMDX * i, MEM_Y)), module,
			                                                          Heathrow::MEM_PARAM + i, Heathrow::MEM_LIGHT + i));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(BX[0], MEM2_Y)), module, Heathrow::SAVE_PARAM, Heathrow::SAVE_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Heathrow* module = dynamic_cast<Heathrow*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Memory"));
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (!module->slotUsed[i]) continue;
			menu->addChild(createMenuItem(string::f("Clear slot %d", i + 1), "", [=]() {
				module->slotUsed[i] = false;
				if (module->currentSlot == i) module->currentSlot = -1;
			}));
		}
		menu->addChild(createMenuItem("Clear all slots", "", [=]() {
			for (int i = 0; i < NUM_SLOTS; i++) module->slotUsed[i] = false;
			module->currentSlot = -1;
		}));
	}
};

Model* modelHeathrow = createModel<Heathrow, HeathrowWidget>("Heathrow");
