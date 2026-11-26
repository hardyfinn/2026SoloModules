// Live Wire: one live electrical circuit. A source (DC, mains AC or both) feeds a spark gap through a ballast and a
// choke; across the gap sit an arc, a saturating coil-and-capacitor tank and corona. Buzzing arcs, a Jacob's ladder,
// a singing arc that tracks V/OCT, sparks as drums, corona sizzle and transformer hum.
// Engine in src/dsp/LiveWireEngine.hpp, the module chain in src/dsp/Voice.hpp (shared with the offline tests).
// Spec: docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/Voice.hpp"
#include "defaults.hpp"

using namespace livewire;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float Y1 = 30.0f, Y2 = 55.0f, Y3 = 77.0f, Y4 = 100.0f, Y5 = 117.0f;
static const float ROW1_X[11] = {15.0f, 33.0f, 48.0f, 66.0f, 84.0f, 104.0f, 122.0f, 142.0f, 160.0f, 175.0f, 190.0f};
static float row2x(int i) { return 12.0f + 19.9f * i; }
static const float ROW3_KNOB_X[4] = {11.0f, 27.0f, 43.0f, 59.0f};
static const float ROW3_SW_X[2] = {75.0f, 89.0f};
static const float ROW3_BTN_X[3] = {104.0f, 117.0f, 130.0f};
static const float ROW3_LIGHT_X[2] = {142.0f, 151.0f};
static const float ROW3_OUT_X[3] = {163.0f, 177.5f, 192.0f};
static float jackx(int i) { return 10.5f + 16.6f * i; }

// Tooltip showing the physical value behind a 0..1 knob.
struct PhysQ : ParamQuantity {
	std::function<std::string(double)> show;
	std::string getDisplayValueString() override { return show ? show(getValue()) : ParamQuantity::getDisplayValueString(); }
};

struct LiveWire : Module {
	enum ParamIds { NUM_PARAMS = P_SURGE + 1 };
	enum InputIds {
		VOCT_INPUT, TRIG_INPUT, KEY_INPUT, TOUCH_INPUT, SURGE_INPUT, AUDIO_INPUT,
		VOLTS_INPUT, CURRENT_INPUT, GAP_INPUT, HEAT_INPUT, COOL_INPUT, WIND_INPUT,
		CLIMB_INPUT, RING_INPUT, LOAD_INPUT, IRON_INPUT, CORONA_INPUT, FB_INPUT, MAINS_INPUT, AGE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { ARC_OUTPUT, FIELD_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ARC_LIGHT, CHARGE_LIGHT, NUM_LIGHTS };
	// CV jack for each Cv index (Voice.hpp order)
	const int CV_JACK[NUM_CV] = {VOLTS_INPUT, CURRENT_INPUT, GAP_INPUT, HEAT_INPUT, COOL_INPUT, WIND_INPUT, CLIMB_INPUT,
		RING_INPUT, LOAD_INPUT, IRON_INPUT, CORONA_INPUT, FB_INPUT, MAINS_INPUT, AGE_INPUT};

	Voice v;
	double rate = 0;
	bool needInit = true;
	float pv[NUM_P] = {};
	dsp::ClockDivider lightDiv;

	void knob(int id, const char* name, std::function<std::string(double)> show, const char* desc) {
		PhysQ* q = configParam<PhysQ>(id, 0.f, 1.f, LW_DEFAULTS[id], name);
		q->show = show;
		if (desc) q->description = desc;
	}

	LiveWire() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		knob(P_VOLTS, "Volts", [](double k) { return string::f("%.2f x strike", M_VOLTS(k)); },
			"Source voltage. 1 strikes a gap of 1 by itself.");
		knob(P_MAINS, "Mains (DC to AC)", [](double k) { return string::f("%.0f%% AC", 100 * k); },
			"DC sings and sparks; AC buzzes and goes out at every zero crossing.");
		knob(P_FREQ, "Mains frequency", [](double k) { return string::f("%.1f Hz", M_FREQ(k)); }, "50 Hz is about 28%.");
		knob(P_CURRENT, "Current (ballast)", [](double k) { return string::f("%.3f", M_CUR(k)); },
			"How much current the supply lets through. High holds a quiet steady arc; lower makes it sing; starved tears it. Bends the singing pitch.");
		knob(P_CHOKE, "Choke", [](double k) { return string::f("%.2f ms", 1000 * M_CHOKE(k)); }, "Inductance in the supply: smooths and delays the current.");
		knob(P_GAP, "Gap", [](double k) { return string::f("%.3f", M_GAP(k)); }, "Electrode spacing. Longer gaps need more volts to strike and sputter more.");
		knob(P_HEAT, "Heat (arc power)", [](double k) { return string::f("%.4f", M_HEAT(k)); },
			"Power the arc needs per length. Low makes sparks ring; high makes a fat, steady arc.");
		knob(P_COOL, "Cool (arc time constant)", [](double k) { return string::f("%.0f us", 1e6 * M_COOL(k)); },
			"How fast the arc channel heats and cools. With TRACK on it follows pitch (value shown at C4).");
		knob(P_QUENCH, "Quench", [](double k) { return string::f("%.4f", M_QUENCH(k)); }, "How easily the arc goes out. High stops sparks becoming a steady arc.");
		knob(P_MEMORY, "Memory (ionisation)", [](double k) { return string::f("%.1f ms", 1000 * M_MEM(k)); }, "How long a hot gap stays easy to re-strike.");
		knob(P_RESTRIKE, "Restrike", [](double k) { return string::f("%.0f%%", 100 * M_RESTR(k)); }, "How much a hot gap lowers the strike voltage.");
		knob(P_JITTER, "Jitter", [](double k) { return string::f("%.2f", M_JIT(k)); },
			"Randomness of each strike. On triggered sparks: misfires, late strikes with leader ticks, ghost re-strikes.");
		knob(P_DRAW, "Draw (pull-apart time)", [](double k) { return string::f("%.2f s", M_DRAW(k)); },
			"After TOUCH is released, how long the electrodes take to pull apart to the full range.");
		knob(P_WIND, "Wind", [](double k) { return string::f("%.3f", M_WIND(k)); }, "Turbulence stretching the arc.");
		knob(P_CLIMB, "Climb", [](double k) { return string::f("%.2f /s", M_CLIMB(k)); }, "The arc lengthens while it burns, tears and re-strikes: Jacob's ladder.");
		knob(P_TUNE, "Tune (tank)", [](double k) { return string::f("%.1f Hz", M_TUNE(k)); }, "Pitch of the coil and capacitor. Adds to V/OCT.");
		knob(P_RING, "Ring (tank Q)", [](double k) { return string::f("Q %.1f", M_RING(k)); }, nullptr);
		knob(P_LOAD, "Load (tank impedance)", [](double k) { return string::f("Z %.2f", M_LOAD(k)); }, "More LOAD means bigger tank currents and a stronger pitch.");
		knob(P_IRON, "Iron (coil saturation)", [](double k) { return string::f("%.2f", M_IRON(k)); }, "Pitch rises with current: drops on every hit, growl when singing.");
		knob(P_CORONA, "Corona", [](double k) { return string::f("%.2f", M_CORONA(k)); }, "Sizzle of micro-discharges as the voltage nears breakdown.");
		knob(P_HUM, "Hum (transformer)", [](double k) { return string::f("%.0f%%", 100 * k); }, nullptr);
		knob(P_FB, "Feedback", [](double k) { return string::f("%.2f", M_FB(k)); }, "The arc hears itself: output fed back into the source.");
		knob(P_DIST, "Distance (feedback delay)", [](double k) { return string::f("%.2f ms", 1000 * M_DIST(k)); }, nullptr);
		knob(P_AGE, "Age", [](double k) { return string::f("%.0f%%", 100 * k); },
			"Worn electrodes, loose connections, dirty mains, drifting parts.");
		knob(P_IN, "Audio in level", [](double k) { return string::f("%.2f", M_IN(k)); }, "AUDIO is added to the source voltage: the arc speaks it.");
		knob(P_ARC, "Arc level", [](double k) { return string::f("%.2f", M_ARC(k)); }, "Sound of the air heated by the arc.");
		knob(P_FIELD, "Field level", [](double k) { return string::f("%.2f", M_FIELD(k)); }, "A pickup on the wires: cleaner, pitched.");
		knob(P_OUT, "Out (drive)", [](double k) { return string::f("x%.2f", M_OUT(k)); }, nullptr);
		configSwitch(P_TRACK, 0.f, 1.f, LW_DEFAULTS[P_TRACK], "Track (COOL follows pitch)", {"Off", "On"});
		configSwitch(P_VTARGET, 0.f, 2.f, LW_DEFAULTS[P_VTARGET], "V/OCT goes to", {"Tank", "Both", "Mains"});
		configButton(P_FIRE, "Fire (trigger the gap)");
		configButton(P_TOUCH, "Touch (close the electrodes)");
		configButton(P_SURGE, "Surge");

		configInput(VOCT_INPUT, "V/OCT");
		configInput(TRIG_INPUT, "Trigger (fires the gap)");
		configInput(KEY_INPUT, "Key gate (tank on while high; a new key starts the note)");
		configInput(TOUCH_INPUT, "Touch gate (electrodes close while high, pull apart on release)");
		configInput(SURGE_INPUT, "Surge trigger");
		configInput(AUDIO_INPUT, "Audio (added to the source)");
		const char* cvn[NUM_CV] = {"Volts", "Current", "Gap", "Heat", "Cool", "Wind", "Climb", "Ring", "Load", "Iron", "Corona", "Feedback", "Mains", "Age"};
		for (int c = 0; c < NUM_CV; c++) configInput(CV_JACK[c], string::f("%s CV (10 V = full range)", cvn[c]));
		configOutput(ARC_OUTPUT, "Arc (air pressure from the arc)");
		configOutput(FIELD_OUTPUT, "Field (pickup on the wires)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		configLight(ARC_LIGHT, "Arc burning");
		configLight(CHARGE_LIGHT, "Charge");
		lightDiv.setDivision(64);
		v.seed = random::u64();
	}

	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) { rate = args.sampleRate; v.init(rate); needInit = false; }
		for (int i = 0; i < NUM_P; i++) pv[i] = params[i].getValue();
		Jacks j;
		for (int c = 0; c < NUM_CV; c++) j.cv[c] = inputs[CV_JACK[c]].getVoltage();
		j.voct = inputs[VOCT_INPUT].getVoltage();
		j.trig = inputs[TRIG_INPUT].getVoltage();
		j.key = inputs[KEY_INPUT].getVoltage();
		j.keyPatched = inputs[KEY_INPUT].isConnected();
		j.touch = inputs[TOUCH_INPUT].getVoltage();
		j.surge = inputs[SURGE_INPUT].getVoltage();
		j.audio = inputs[AUDIO_INPUT].getVoltage();
		v.process(pv, j);
		outputs[L_OUTPUT].setVoltage((float)v.L);
		outputs[R_OUTPUT].setVoltage((float)v.R);
		outputs[ARC_OUTPUT].setVoltage((float)v.arc);
		outputs[FIELD_OUTPUT].setVoltage((float)v.field);
		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[ARC_LIGHT].setBrightnessSmooth((float)v.e.arcLight(), dt);
			lights[CHARGE_LIGHT].setBrightnessSmooth((float)v.e.chargeLight(), dt);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(string::f("%016llx", (unsigned long long)v.seed).c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) v.seed = std::strtoull(json_string_value(s), nullptr, 16);
		needInit = true;
	}
};

struct LiveWireWidget : ModuleWidget {
	LiveWireWidget(LiveWire* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LiveWire.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		typedef LiveWire M;

		const int r1[11] = {P_VOLTS, P_MAINS, P_FREQ, P_CURRENT, P_CHOKE, P_GAP, P_DRAW, P_TUNE, P_RING, P_LOAD, P_IRON};
		for (int i = 0; i < 11; i++) {
			bool big = (i == 0 || i == 3 || i == 5 || i == 7);
			if (big) addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(ROW1_X[i], Y1)), module, r1[i]));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW1_X[i], Y1)), module, r1[i]));
		}
		const int r2[10] = {P_HEAT, P_COOL, P_QUENCH, P_MEMORY, P_RESTRIKE, P_JITTER, P_WIND, P_CLIMB, P_CORONA, P_HUM};
		for (int i = 0; i < 10; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(row2x(i), Y2)), module, r2[i]));
		const int r3[4] = {P_FB, P_DIST, P_AGE, P_IN};
		for (int i = 0; i < 4; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW3_KNOB_X[i], Y3)), module, r3[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(ROW3_SW_X[0], Y3)), module, P_TRACK));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(ROW3_SW_X[1], Y3)), module, P_VTARGET));
		const int b3[3] = {P_FIRE, P_TOUCH, P_SURGE};
		for (int i = 0; i < 3; i++) addParam(createParamCentered<VCVButton>(mm2px(Vec(ROW3_BTN_X[i], Y3)), module, b3[i]));
		addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(ROW3_LIGHT_X[0], Y3 + 1.0f)), module, M::ARC_LIGHT));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(ROW3_LIGHT_X[1], Y3 + 1.0f)), module, M::CHARGE_LIGHT));
		const int m3[3] = {P_ARC, P_FIELD, P_OUT};
		for (int i = 0; i < 3; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW3_OUT_X[i], Y3)), module, m3[i]));

		const int in4[12] = {M::VOCT_INPUT, M::TRIG_INPUT, M::KEY_INPUT, M::TOUCH_INPUT, M::SURGE_INPUT, M::AUDIO_INPUT,
			M::VOLTS_INPUT, M::CURRENT_INPUT, M::GAP_INPUT, M::HEAT_INPUT, M::COOL_INPUT, M::WIND_INPUT};
		for (int i = 0; i < 12; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackx(i), Y4)), module, in4[i]));
		const int in5[8] = {M::CLIMB_INPUT, M::RING_INPUT, M::LOAD_INPUT, M::IRON_INPUT, M::CORONA_INPUT, M::FB_INPUT, M::MAINS_INPUT, M::AGE_INPUT};
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackx(i), Y5)), module, in5[i]));
		const int o5[4] = {M::ARC_OUTPUT, M::FIELD_OUTPUT, M::L_OUTPUT, M::R_OUTPUT};
		for (int i = 0; i < 4; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackx(8 + i), Y5)), module, o5[i]));
	}
};

Model* modelLiveWire = createModel<LiveWire, LiveWireWidget>("LiveWire");
