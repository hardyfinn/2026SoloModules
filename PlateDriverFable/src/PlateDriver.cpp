// Plate Driver: a metal plate held in a feedback loop (pickup -> amplifier -> driver), so it sings,
// splits into multiphonics and roars. Engine in src/dsp/PlateEngine.hpp, knob mapping in src/dsp/Mapping.hpp
// (shared with the offline tests). Spec: docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/PlateEngine.hpp"
#include "dsp/Mapping.hpp"

using namespace plate;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float W_MM = 203.2f;
static float col10(int i) { return 13.9f + 19.5f * i; }    // rows 1, 2 and 4
static float col11(int i) { return 12.6f + 17.8f * i; }    // rows 3 and 5
static const float Y1 = 26.5f, Y2 = 50.5f, Y3 = 78.0f, Y4 = 98.0f, Y5 = 115.0f;
static const float RING_R = 11.0f;   // ROAR ring around DRIVE
static const float DRIVE_X = 23.65f;   // between columns 0 and 1

static const char* TYPE_NAMES[4] = {"Sheet", "Tam-tam", "Bossed", "Bowl"};

struct PlateDriver : Module {
	enum ParamIds {
		TYPE_PARAM, CAST_PARAM, SIZE_PARAM, THICK_PARAM, CURVE_PARAM, STRETCH_PARAM, DECAY_PARAM,
		BRIGHT_PARAM, AGE_PARAM, LEVEL_PARAM,
		DRIVE_PARAM, GAIN_PARAM, FOCUS_PARAM, PITCH_PARAM, PHASE_PARAM, TRACK_PARAM,
		DRIVER_PARAM, SPREAD_PARAM, GRAB_PARAM,
		PICKUP_PARAM, AMP_PARAM, DRVSAT_PARAM, RATTLE_PARAM, BLEED_PARAM, OUT_PARAM,
		PUMP_PARAM, RATE_PARAM, MALLET_PARAM, INLEVEL_PARAM, RELEASE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, GATE_INPUT, DRIVE_INPUT, FOCUS_INPUT, PHASE_INPUT, SIZE_INPUT, THICK_INPUT, CURVE_INPUT,
		DRIVER_INPUT, SPREAD_INPUT,
		GRAB_INPUT, RATTLE_INPUT, PUMP_INPUT, RATE_INPUT, STRIKE_INPUT, VEL_INPUT, IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { ROAR_OUTPUT, ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	plate::Engine* e = nullptr;
	plate::Age age;
	plate::Decimator dec;
	plate::Leveller lev;
	uint64_t seed = 1;
	double rate = 0;
	bool os = true;
	bool needInit = true;
	int ctr = 0;
	double gateS = 1.0;
	double roarS = 0, envS = 0;
	float ringRoar = 0.f, ringEnv = 0.f;   // read by the DRIVE ring light
	dsp::SchmittTrigger strikeTrig, gateTrig;
	plate::Knobs k;

	PlateDriver() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(TYPE_PARAM, 0.f, 3.f, 0.f, "Plate type", {"Sheet", "Tam-tam", "Bossed", "Bowl"})->description =
			"Which plate the loop drives. Sheet is the approved sound: an even spread of modes.";
		configParam(CAST_PARAM, 0.f, 15.f, 0.f, "Cast", "")->snapEnabled = true;
		getParamQuantity(CAST_PARAM)->description = "Which plate out of sixteen of the same type: the fine detail of its modes and couplings.";
		configParam(SIZE_PARAM, 0.f, 1.f, std::log10(65.4f / 20.f), "Size (lowest mode at C4)", " Hz", 10.f, 20.f)->description =
			"In TRACK PLATE, a small value is a big plate singing a high mode with close neighbours, so it splits sooner.";
		configParam(THICK_PARAM, 0.f, 1.f, 0.5f, "Thickness", "%", 0.f, 100.f)->description = "Thin plates stretch and bloom readily; thick ones stay pure longer.";
		configParam(CURVE_PARAM, 0.f, 1.f, 0.6f, "Curvature", "%", 0.f, 100.f)->description = "A curved plate couples its modes through stretching: the source of the splits.";
		configParam(STRETCH_PARAM, 0.f, 2.f, 1.f, "Stretch (pitch rise with amplitude)", "x");
		struct DecayQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%.2f s", 9.0 * getValue()); }
		};
		configParam<DecayQ>(DECAY_PARAM, 0.15f, 4.f, 1.f, "Decay (lowest mode, T60)");
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.5f, "Bright", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"Analog looseness: loop gain drift, hiss, mains hum, dented modes, a loose driver head, warm-up and a tired amplifier.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.7f, "Level", "%", 0.f, 100.f)->description = "At 70% a pure tone is about 5 V peak.";
		getParamQuantity(LEVEL_PARAM)->randomizeEnabled = false;

		struct DriveQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double v = getValue();
				return v <= 0 ? "off" : string::f("%.0f%%  (force %.0f at C4)", v * 100.0, driveForce(v));
			}
		};
		configParam<DriveQ>(DRIVE_PARAM, 0.f, 1.f, 0.35f, "Drive")->description =
			"How hard the driver pushes the plate. Low: one pure tone. Middle: the tone splits into chords. High: roar.";
		configParam(GAIN_PARAM, 0.f, 1.f, std::log(3.f) / std::log(30.f), "Loop gain", "", 30.f, 20.f)->description =
			"Low gain starts slowly and organically; high gain starts at once and hits hard.";
		configParam(FOCUS_PARAM, 0.f, 1.f, 0.5f, "Focus", "%", 0.f, 100.f)->description = "A band-pass in the loop at the played pitch. High focus holds one tone.";
		configParam(PITCH_PARAM, -3.f, 3.f, 0.f, "Pitch", " Hz", 2.f, (float)C4);
		configParam(PHASE_PARAM, 0.f, 1.f, 0.f, "Phase (loop delay)", " ms", 0.f, 4.f)->description = "Delay in the loop: changes which modes the loop can keep alive.";
		configSwitch(TRACK_PARAM, 0.f, 1.f, 1.f, "Track", {"Focus (overblowing: only the loop filter follows pitch)", "Plate (the whole plate follows pitch)"});
		configParam(DRIVER_PARAM, 0.05f, 0.95f, 0.31f, "Driver position", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, 0.f, 0.5f, 0.f, "Pickup spread (distance from the driver)", "%", 0.f, 100.f)->description =
			"At zero the pickup sits on the driver and the loop always sings. Away from zero some modes are fed and some fought.";
		configParam(GRAB_PARAM, 0.f, 1.f, 0.f, "Grab (hand on the plate)", "%", 0.f, 100.f);
		configParam(PICKUP_PARAM, 0.f, 1.f, 0.2f, "Pickup saturation", "%", 0.f, 100.f);
		configParam(AMP_PARAM, 0.f, 1.f, 0.2f, "Amplifier saturation", "%", 0.f, 100.f);
		configParam(DRVSAT_PARAM, 0.f, 1.f, 0.1f, "Driver saturation (towards square)", "%", 0.f, 100.f);
		configParam(RATTLE_PARAM, 0.f, 1.f, 0.f, "Rattle (driver head loses contact)", "%", 0.f, 100.f);
		configParam(BLEED_PARAM, 0.f, 1.f, 0.1f, "Bleed (driver signal into the output)", "%", 0.f, 100.f);
		configParam(OUT_PARAM, 0.f, 1.f, 0.f, "Output saturation", "%", 0.f, 100.f);
		configParam(PUMP_PARAM, 0.f, 1.f, 0.f, "Pump (amplifier trips and recovers)", "%", 0.f, 100.f);
		configParam(RATE_PARAM, 0.f, 1.f, 0.3f, "Pump rate", "%", 0.f, 100.f);
		configParam(MALLET_PARAM, 0.f, 1.f, 0.5f, "Mallet (strike brightness)", " Hz", 10.f, 300.f);
		configParam(INLEVEL_PARAM, 0.f, 1.f, 0.5f, "Input level", "%", 0.f, 100.f);
		configParam(RELEASE_PARAM, 0.f, 1.f, 0.f, "Release damping (while the gate is low)", "%", 0.f, 100.f);

		configInput(VOCT_INPUT, "V/OCT");
		configInput(GATE_INPUT, "Gate (drive on while high; on when unpatched)");
		configInput(DRIVE_INPUT, "DRIVE CV");
		configInput(FOCUS_INPUT, "FOCUS CV");
		configInput(PHASE_INPUT, "PHASE CV");
		configInput(SIZE_INPUT, "SIZE CV (0.1 decade per volt)");
		configInput(THICK_INPUT, "THICK CV");
		configInput(CURVE_INPUT, "CURVE CV");
		configInput(DRIVER_INPUT, "DRIVER POS CV");
		configInput(SPREAD_INPUT, "SPREAD CV");
		configInput(GRAB_INPUT, "GRAB CV (10 V = full)");
		configInput(RATTLE_INPUT, "RATTLE CV");
		configInput(PUMP_INPUT, "PUMP CV");
		configInput(RATE_INPUT, "RATE CV");
		configInput(STRIKE_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Strike velocity (0 to 10 V; 8 V when unpatched)");
		configInput(IN_INPUT, "Audio into the loop");
		configOutput(ROAR_OUTPUT, "Roar (0 V one tone, 10 V full roar)");
		configOutput(ENV_OUTPUT, "Plate energy envelope (0 to 10 V)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		e = new plate::Engine();
		seed = random::u64();
	}
	~PlateDriver() { delete e; }

	void init(double sr) {
		rate = sr;
		os = sr < 88200.0;
		e->setRate(os ? 2 * sr : sr);
		e->build(plate::SHEET, 0);
		e->reset();
		age.init(seed);
		lev.setRate(sr); lev.reset(); dec.reset();
		ctr = 0;
		needInit = false;
	}
	void newUnit() { seed = random::u64(); age.init(seed); }

	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	double cv(int id) { return inputs[id].getVoltage(); }

	void readKnobs() {
		auto p = [&](int id) { return (double)params[id].getValue(); };
		k.type = clamp((int)std::round(p(TYPE_PARAM)), 0, 3);
		k.cast = clamp((int)std::round(p(CAST_PARAM)), 0, 15);
		k.size = 20.0 * std::pow(10.0, clampd(p(SIZE_PARAM) + 0.1 * cv(SIZE_INPUT), 0, 1));
		k.thick = clampd(p(THICK_PARAM) + 0.1 * cv(THICK_INPUT), 0, 1);
		k.curve = clampd(p(CURVE_PARAM) + 0.1 * cv(CURVE_INPUT), 0, 1);
		k.stretch = p(STRETCH_PARAM);
		k.decay = p(DECAY_PARAM);
		k.drive = clampd(p(DRIVE_PARAM) + 0.1 * cv(DRIVE_INPUT), 0, 1);
		k.gain = 20.0 * std::pow(30.0, clampd(p(GAIN_PARAM), 0, 1));
		k.focus = clampd(p(FOCUS_PARAM) + 0.1 * cv(FOCUS_INPUT), 0, 1);
		k.pitch = p(PITCH_PARAM) + inputs[VOCT_INPUT].getVoltage();
		k.trackPlate = p(TRACK_PARAM) > 0.5;
		k.phase = clampd(p(PHASE_PARAM) + 0.1 * cv(PHASE_INPUT), 0, 1);
		k.driverPos = clampd(p(DRIVER_PARAM) + 0.1 * cv(DRIVER_INPUT), 0.05, 0.95);
		k.spread = clampd(p(SPREAD_PARAM) + 0.05 * cv(SPREAD_INPUT), 0, 0.5);
		k.grab = clampd(p(GRAB_PARAM) + 0.1 * cv(GRAB_INPUT), 0, 1);
		k.release = p(RELEASE_PARAM);
		k.pick = p(PICKUP_PARAM); k.amp = p(AMP_PARAM); k.drv = p(DRVSAT_PARAM);
		k.rat = clampd(p(RATTLE_PARAM) + 0.1 * cv(RATTLE_INPUT), 0, 1);
		k.bleed = p(BLEED_PARAM); k.out = p(OUT_PARAM);
		k.pump = clampd(p(PUMP_PARAM) + 0.1 * cv(PUMP_INPUT), 0, 1);
		k.rate = clampd(p(RATE_PARAM) + 0.1 * cv(RATE_INPUT), 0, 1);
		k.mallet = p(MALLET_PARAM);
		k.inLevel = p(INLEVEL_PARAM);
		k.bright = p(BRIGHT_PARAM);
		k.age = p(AGE_PARAM);
		k.gate = gateS;
	}

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) init(args.sampleRate);

		// gate: on when unpatched, 4 ms slew
		bool gateHigh = true;
		if (inputs[GATE_INPUT].isConnected()) { gateTrig.process(inputs[GATE_INPUT].getVoltage(), 0.1f, 1.f); gateHigh = gateTrig.isHigh(); }
		gateS += ((gateHigh ? 1.0 : 0.0) - gateS) * (1.0 - std::exp(-args.sampleTime / 0.004));

		const int ctrlLen = plate::BLK / (os ? 2 : 1);
		if (ctr == 0) {
			readKnobs();
			age.update(plate::BLK / e->fe, e->emean);
			mapKnobs(k, *e, &age);
		}
		if (++ctr >= ctrlLen) ctr = 0;

		if (strikeTrig.process(inputs[STRIKE_INPUT].getVoltage(), 0.1f, 1.f)) {
			double vel = inputs[VEL_INPUT].isConnected() ? clampd(inputs[VEL_INPUT].getVoltage() / 10.0, 0, 1) : 0.8;
			if (vel > 0) e->strike(420.0 * vel);
		}

		double inS = inputs[IN_INPUT].getVoltage() * e->c.inLevel;
		double l2[2], r2[2];
		const int nsub = os ? 2 : 1;
		for (int s = 0; s < nsub; s++) { e->process(inS); l2[s] = e->outL; r2[s] = e->outR; }
		double l, r;
		if (os) dec.process(l2, r2, l, r); else { l = l2[0]; r = r2[0]; }
		double g = lev.gain(l, r);
		double sc = params[LEVEL_PARAM].getValue() * OUT_VOLTS;
		outputs[L_OUTPUT].setVoltage((float)(11.0 * std::tanh(l * g * sc / 11.0)));
		outputs[R_OUTPUT].setVoltage((float)(11.0 * std::tanh(r * g * sc / 11.0)));

		double roar = clampd(std::log2(std::max(e->pr, 1.0)) / std::log2(24.0), 0, 1);
		roarS += (roar - roarS) * (1.0 - std::exp(-args.sampleTime / 0.1));
		double en = clampd(std::sqrt(std::max(e->energy, 0.0) / 3e6), 0, 1);
		envS += (en - envS) * (1.0 - std::exp(-args.sampleTime / 0.02));
		outputs[ROAR_OUTPUT].setVoltage((float)(10.0 * roarS));
		outputs[ENV_OUTPUT].setVoltage((float)(10.0 * envS));
		ringRoar = (float)roarS; ringEnv = (float)envS;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(string::f("%016llx", (unsigned long long)seed).c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) seed = std::strtoull(json_string_value(s), nullptr, 16);
		needInit = true;
	}
};

// Ring around DRIVE: dim blue for one pure tone, white for multiphonics, red for roar; brightness follows the plate energy.
struct RoarRing : TransparentWidget {
	PlateDriver* module = nullptr;
	float radius = 0;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		float roar = module ? module->ringRoar : 0.f, en = module ? module->ringEnv : 0.f;
		NVGcolor c;
		if (roar < 0.5f) c = nvgLerpRGBA(nvgRGB(60, 110, 255), nvgRGB(240, 240, 240), roar * 2.f);
		else c = nvgLerpRGBA(nvgRGB(240, 240, 240), nvgRGB(255, 40, 30), (roar - 0.5f) * 2.f);
		float a = module ? 0.15f + 0.85f * std::min(1.f, en * 1.5f) : 0.f;
		if (a <= 0.f) return;
		c.a = a;
		Vec ctr = box.size.div(2);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, ctr.x, ctr.y, radius);
		nvgStrokeColor(args.vg, c);
		nvgStrokeWidth(args.vg, 2.2f);
		nvgStroke(args.vg);
		// soft halo
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, ctr.x, ctr.y, radius);
		NVGcolor h = c; h.a = a * 0.25f;
		nvgStrokeColor(args.vg, h);
		nvgStrokeWidth(args.vg, 6.f);
		nvgStroke(args.vg);
	}
};

struct PlateDriverWidget : ModuleWidget {
	PlateDriverWidget(PlateDriver* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PlateDriver.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef PlateDriver M;
		// row 1
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(col10(0), Y1)), module, M::TYPE_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(col10(1), Y1)), module, M::CAST_PARAM));
		const int r1[8] = {M::SIZE_PARAM, M::THICK_PARAM, M::CURVE_PARAM, M::STRETCH_PARAM, M::DECAY_PARAM, M::BRIGHT_PARAM, M::AGE_PARAM, M::LEVEL_PARAM};
		for (int i = 0; i < 8; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col10(i + 2), Y1)), module, r1[i]));
		// row 2
		{
			RoarRing* ring = new RoarRing;
			ring->module = module;
			float rpx = mm2px(RING_R);
			ring->radius = rpx;
			ring->box.size = Vec(2 * rpx + 8, 2 * rpx + 8);
			ring->box.pos = mm2px(Vec(DRIVE_X, Y2)).minus(ring->box.size.div(2));
			addChild(ring);
		}
		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(DRIVE_X, Y2)), module, M::DRIVE_PARAM));
		const int r2[4] = {M::GAIN_PARAM, M::FOCUS_PARAM, M::PITCH_PARAM, M::PHASE_PARAM};
		for (int i = 0; i < 4; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col10(i + 2), Y2)), module, r2[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(col10(6), Y2)), module, M::TRACK_PARAM));
		const int r2b[3] = {M::DRIVER_PARAM, M::SPREAD_PARAM, M::GRAB_PARAM};
		for (int i = 0; i < 3; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col10(i + 7), Y2)), module, r2b[i]));
		// row 3
		const int r3[11] = {M::PICKUP_PARAM, M::AMP_PARAM, M::DRVSAT_PARAM, M::RATTLE_PARAM, M::BLEED_PARAM, M::OUT_PARAM,
			M::PUMP_PARAM, M::RATE_PARAM, M::MALLET_PARAM, M::INLEVEL_PARAM, M::RELEASE_PARAM};
		for (int i = 0; i < 11; i++) addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col11(i), Y3)), module, r3[i]));
		// row 4
		const int r4[10] = {M::VOCT_INPUT, M::GATE_INPUT, M::DRIVE_INPUT, M::FOCUS_INPUT, M::PHASE_INPUT, M::SIZE_INPUT, M::THICK_INPUT, M::CURVE_INPUT, M::DRIVER_INPUT, M::SPREAD_INPUT};
		for (int i = 0; i < 10; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col10(i), Y4)), module, r4[i]));
		// row 5
		const int r5[7] = {M::GRAB_INPUT, M::RATTLE_INPUT, M::PUMP_INPUT, M::RATE_INPUT, M::STRIKE_INPUT, M::VEL_INPUT, M::IN_INPUT};
		for (int i = 0; i < 7; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col11(i), Y5)), module, r5[i]));
		const int o5[4] = {M::ROAR_OUTPUT, M::ENV_OUTPUT, M::L_OUTPUT, M::R_OUTPUT};
		for (int i = 0; i < 4; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col11(i + 7), Y5)), module, o5[i]));
	}

	void appendContextMenu(Menu* menu) override {
		PlateDriver* m = dynamic_cast<PlateDriver*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Plate: %s, cast %d", TYPE_NAMES[clamp((int)m->params[PlateDriver::TYPE_PARAM].getValue(), 0, 3)], (int)m->params[PlateDriver::CAST_PARAM].getValue())));
		menu->addChild(createMenuLabel(string::f("AGE unit %08llx", (unsigned long long)(m->seed >> 32))));
		menu->addChild(createMenuItem("New AGE unit (other dents, other drift)", "", [=]() { m->newUnit(); }));
	}
};

Model* modelPlateDriver = createModel<PlateDriver, PlateDriverWidget>("PlateDriver");
