// Bent Sheet Metal: a ringing spring-steel sheet you bend, S-bend, flex, snap, dip in water and bow
// (musical saw, flexatone, wobble board, water gong, thunder sheet). Engine in src/dsp/SheetEngine.hpp,
// knob mapping and the whole module chain in src/dsp/Mapping.hpp (shared with the offline tests).
// Spec: docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/SheetEngine.hpp"
#include "dsp/Mapping.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static float col11(int i) { return 12.6f + 17.8f * i; }   // rows 1 and 5
static float col10(int i) { return 13.9f + 19.5f * i; }   // row 4
static const float Y1 = 27.0f, Y2 = 52.0f, Y3 = 79.0f, Y4 = 100.0f, Y5 = 116.0f;
static const float ROW2_X[9] = {21.0f, 44.0f, 61.0f, 78.0f, 95.0f, 112.0f, 139.0f, 162.0f, 182.0f};
static const float DISP_X = 6.0f, DISP_Y = 72.0f, DISP_W = 72.0f, DISP_H = 14.0f;
static const float ROW3_KNOBS[3] = {92.0f, 108.0f, 124.0f};
static const float ROW3_OUTS[3] = {152.0f, 168.0f, 184.0f};

struct BentSheetMetal : Module {
	// Param order is the preset order (tools/make_presets.py).
	enum ParamIds {
		PITCH_PARAM, TAPER_PARAM, GRIP_PARAM, DECAY_PARAM, DIP_PARAM, DEPTH_PARAM, STRETCH_PARAM, PICKUP_PARAM,
		OUT_PARAM, AGE_PARAM, LEVEL_PARAM,
		BEND_PARAM, SHAPE_PARAM, SPOT_PARAM, HOLD_PARAM, SNAP_PARAM, CRACK_PARAM, BOW_PARAM, SPEED_PARAM, BOWPOS_PARAM,
		HARD_PARAM, ROLL_PARAM, INLEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, BENDVOCT_INPUT, BEND_INPUT, SHAPE_INPUT, SPOT_INPUT, HOLD_INPUT, SNAP_INPUT, TAPER_INPUT, GRIP_INPUT, DIP_INPUT,
		BOW_INPUT, SPEED_INPUT, BOWPOS_INPUT, BOWGATE_INPUT, STRIKE_INPUT, VEL_INPUT, ROLL_INPUT, STRETCH_INPUT, IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { SNAP_OUTPUT, BEND_OUTPUT, ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	bent::Voice v;
	bent::Knobs k;
	double rate = 0;
	bool needInit = true;
	double gateS = 1.0;
	dsp::SchmittTrigger strikeTrig;
	dsp::PulseGenerator snapPulse;

	// read by the display (written once per control block; a torn read only costs one odd frame)
	float dispK[bent::G] = {};
	float dispDip = 0.f, dispBowX = 0.5f, dispBow = 0.f, dispEnv = 0.f;
	int dispCtr = 0;

	BentSheetMetal() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, -3.f, 3.f, 0.f, "Pitch (sheet size)", " Hz", 2.f, (float)bent::C3)->description =
			"Resizes the whole sheet. Adds to V/OCT.";
		configParam(TAPER_PARAM, 0.f, 1.f, 0.7f, "Taper", "%", 0.f, 100.f)->description =
			"0 is a rectangle, 1 a saw blade three times wider at the handle.";
		configParam(GRIP_PARAM, 0.f, 1.f, 0.7f, "Grip", "%", 0.f, 100.f)->description =
			"How hard the ends are held. 0 hangs free and rings everywhere; 1 leaves only the trapped notes ringing.";
		configParam(DECAY_PARAM, 0.25f, 4.f, 1.f, "Decay", "x");
		configParam(DIP_PARAM, 0.f, 1.f, 0.f, "Dip (how far the sheet is in the water)", "%", 0.f, 100.f)->description =
			"Water loads the sheet and drags the pitch down; it bends the sound as the sheet goes in and out.";
		struct DepthQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double x = getValue();
				if (x < -0.01) return string::f("%.2f  (lighter than air: pitch rises)", x);
				return string::f("%.2f  (x%.2f the weight of water)", x, bent::depthFromKnob(x) / 1.0);
			}
		};
		configParam<DepthQ>(DEPTH_PARAM, -1.f, 1.f, 0.5f, "Depth (how heavy the liquid is)")->description =
			"0.5 is water, 1 far heavier (mercury). Below 0 the liquid is lighter than air: the pitch rises as it dips.";
		configParam(STRETCH_PARAM, 0.f, 3.f, 1.f, "Stretch (pitch rises with loudness)", "x")->description =
			"0 is a linear sheet. Higher, loud notes bend upwards, by more than half an octave at 3.";
		configParam(PICKUP_PARAM, 0.f, 1.f, 0.2f, "Pickup saturation", "%", 0.f, 100.f);
		configParam(OUT_PARAM, 0.f, 1.f, 0.f, "Output saturation", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"A used sheet: dents, detuned modes, extra loss, a tiring bowing hand, a drifting rosin, metal fatigue and warm-up.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.7f, "Level", "%", 0.f, 100.f)->description = "At 70% the Init sound is about 5 V peak.";
		getParamQuantity(LEVEL_PARAM)->randomizeEnabled = false;

		configParam(BEND_PARAM, -1.f, 1.f, 0.45f, "Bend", "%", 0.f, 100.f)->description =
			"How hard the hand bends the sheet. Bending tightens it and raises the pitch. Past about 60% is beyond what steel survives.";
		configParam(SHAPE_PARAM, 0.f, 1.f, 1.f, "Shape (arch to S-bend)", "%", 0.f, 100.f)->description =
			"An arch rings like a flexed sheet. An S-bend traps one note where it turns over: the musical saw.";
		configParam(SPOT_PARAM, -1.f, 1.f, 0.f, "Spot (where the S turns over)", "%", 0.f, 100.f)->description =
			"Moves the trapped note along the sheet: changes the note, and pans the sound.";
		configParam(HOLD_PARAM, 0.f, 1.f, 0.9f, "Hold (how firm the hand is)", "%", 0.f, 100.f)->description =
			"A firm hand follows BEND. A loose hand lets the sheet wobble on its own.";
		configParam(SNAP_PARAM, 0.f, 1.f, 0.f, "Snap", "%", 0.f, 100.f)->description =
			"The sheet prefers to sit bent one way or the other and snaps between them. Above about 60% it flips itself violently.";
		configParam(CRACK_PARAM, 0.f, 1.f, 0.35f, "Crack (click at each snap)", "%", 0.f, 100.f);
		configParam(BOW_PARAM, 0.f, 1.f, 0.27f, "Bow pressure", "%", 0.f, 100.f)->description =
			"About 27% is the pure saw voice, 50% chords, above 70% roar.";
		struct SpeedQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%.0f%%  (%.2f m/s)", getValue() * 100.0, bent::bowSpeed(getValue())); }
		};
		configParam<SpeedQ>(SPEED_PARAM, 0.f, 1.f, 0.44f, "Bow speed")->description = "Louder, and pushes the sheet harder.";
		configParam(BOWPOS_PARAM, -1.f, 1.f, 0.f, "Bow position (offset from the S-point)", "%", 0.f, 100.f)->description =
			"The bow rides the S-point. Away from it the bow picks other trapped notes.";
		configParam(HARD_PARAM, 0.f, 1.f, 0.5f, "Hard (beater)", "%", 0.f, 100.f)->description = "Soft mallet to hard beater; also how sharp the snap clicks are.";
		configParam(ROLL_PARAM, 0.f, 1.f, 0.f, "Roll (self-striking beaters)", "%", 0.f, 100.f)->description =
			"The two beaters of a flexatone, striking by themselves at 4 to 20 times a second. Off at zero.";
		configParam(INLEVEL_PARAM, 0.f, 1.f, 0.5f, "Input level", "%", 0.f, 100.f);

		configInput(VOCT_INPUT, "V/OCT (whole sheet)");
		configInput(BENDVOCT_INPUT, "BEND V/OCT (retunes by bending)");
		configInput(BEND_INPUT, "BEND CV (5 V = full swing; audio rate is fine)");
		configInput(SHAPE_INPUT, "SHAPE CV");
		configInput(SPOT_INPUT, "SPOT CV (5 V = full swing)");
		configInput(HOLD_INPUT, "HOLD CV");
		configInput(SNAP_INPUT, "SNAP CV");
		configInput(TAPER_INPUT, "TAPER CV");
		configInput(GRIP_INPUT, "GRIP CV");
		configInput(DIP_INPUT, "DIP CV");
		configInput(BOW_INPUT, "BOW CV");
		configInput(SPEED_INPUT, "SPEED CV");
		configInput(BOWPOS_INPUT, "BOW POS CV (5 V = full swing)");
		configInput(BOWGATE_INPUT, "BOW GATE (0 to 10 V scales the bow; on when unpatched)");
		configInput(STRIKE_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Velocity for STRIKE and ROLL (0 to 10 V; 7 V when unpatched)");
		configInput(ROLL_INPUT, "ROLL CV");
		configInput(STRETCH_INPUT, "STRETCH CV (0.3 per volt)");
		configInput(IN_INPUT, "Audio in (shakes the sheet)");
		configOutput(SNAP_OUTPUT, "Snap trigger (each time the sheet flips)");
		configOutput(BEND_OUTPUT, "Bend (the sheet's actual bend, bipolar)");
		configOutput(ENV_OUTPUT, "Energy envelope (0 to 10 V)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		v.seed = random::u64();
	}

	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }
	void newUnit() { v.seed = random::u64(); v.age.init(v.seed); }

	double cv(int id) { return inputs[id].getVoltage(); }

	void readKnobs() {
		using bent::clampd;
		auto p = [&](int id) { return (double)params[id].getValue(); };
		k.pitch = p(PITCH_PARAM) + cv(VOCT_INPUT);
		k.taper = clampd(p(TAPER_PARAM) + 0.1 * cv(TAPER_INPUT), 0, 1);
		k.grip = clampd(p(GRIP_PARAM) + 0.1 * cv(GRIP_INPUT), 0, 1);
		k.decay = p(DECAY_PARAM);
		k.bend = clampd(p(BEND_PARAM) + 0.2 * cv(BEND_INPUT), -1, 1);
		k.bendVoct = cv(BENDVOCT_INPUT);
		k.shape = clampd(p(SHAPE_PARAM) + 0.1 * cv(SHAPE_INPUT), 0, 1);
		k.spot = clampd(p(SPOT_PARAM) + 0.2 * cv(SPOT_INPUT), -1, 1);
		k.hold = clampd(p(HOLD_PARAM) + 0.1 * cv(HOLD_INPUT), 0, 1);
		k.snap = clampd(p(SNAP_PARAM) + 0.1 * cv(SNAP_INPUT), 0, 1);
		k.crack = p(CRACK_PARAM);
		k.dip = clampd(p(DIP_PARAM) + 0.1 * cv(DIP_INPUT), 0, 1);
		k.depth = p(DEPTH_PARAM);
		k.bow = clampd(p(BOW_PARAM) + 0.1 * cv(BOW_INPUT), 0, 1);
		k.speed = clampd(p(SPEED_PARAM) + 0.1 * cv(SPEED_INPUT), 0, 1);
		k.bowPos = clampd(p(BOWPOS_PARAM) + 0.2 * cv(BOWPOS_INPUT), -1, 1);
		k.bowGate = gateS;
		k.hard = p(HARD_PARAM);
		k.roll = clampd(p(ROLL_PARAM) + 0.1 * cv(ROLL_INPUT), 0, 1);
		k.inLevel = p(INLEVEL_PARAM);
		k.stretch = clampd(p(STRETCH_PARAM) + 0.3 * cv(STRETCH_INPUT), 0, 3);
		k.pick = p(PICKUP_PARAM);
		k.out = p(OUT_PARAM);
		k.age = p(AGE_PARAM);
	}

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) { rate = args.sampleRate; v.init(rate); needInit = false; }

		// bow gate: a level, 0 to 10 V, through a 50 ms slew; on when unpatched
		double gt = inputs[BOWGATE_INPUT].isConnected() ? bent::clampd(cv(BOWGATE_INPUT) / 10.0, 0, 1) : 1.0;
		gateS += (gt - gateS) * (1.0 - std::exp(-args.sampleTime / 0.05));

		// the engine reads the knobs once per control block; read them just before
		if (v.ctr == 0) readKnobs();

		double vel = inputs[VEL_INPUT].isConnected() ? bent::clampd(cv(VEL_INPUT) / 10.0, 0, 1) : 0.7;
		double strikeVel = -1;
		if (strikeTrig.process(inputs[STRIKE_INPUT].getVoltage(), 0.1f, 1.f) && vel > 0) strikeVel = vel;

		bool blockStart = v.ctr == 0;
		v.process(k, strikeVel, cv(IN_INPUT), params[LEVEL_PARAM].getValue(), vel);

		outputs[L_OUTPUT].setVoltage((float)v.outL);
		outputs[R_OUTPUT].setVoltage((float)v.outR);
		if (v.snapTrig) snapPulse.trigger(1e-3f);
		outputs[SNAP_OUTPUT].setVoltage(snapPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[BEND_OUTPUT].setVoltage((float)v.bendOut);
		outputs[ENV_OUTPUT].setVoltage((float)v.env);

		// display: about 60 updates a second is plenty
		if (blockStart && ++dispCtr >= 16) {
			dispCtr = 0;
			for (int g = 0; g < bent::G; g++) dispK[g] = (float)v.e->Kg[g];
			dispDip = (float)v.e->c.dip;
			dispBowX = (float)v.e->c.bowX;
			dispBow = (float)std::min(1.0, v.e->c.bowP / 0.6);
			dispEnv = (float)(v.env / 10.0);
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

// The sheet seen edge-on: its curvature integrated twice, water rising from the left, the bow as a mark.
struct SheetDisplay : TransparentWidget {
	BentSheetMetal* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		const int G = bent::G;
		float K[G];
		float dip = 0.f, bowX = 0.5f, bow = 0.3f, env = 0.3f;
		if (module) {
			for (int g = 0; g < G; g++) K[g] = module->dispK[g];
			dip = module->dispDip; bowX = module->dispBowX; bow = module->dispBow; env = module->dispEnv;
		} else {
			// browser: an S-bent blade
			for (int g = 0; g < G; g++) K[g] = 3.f * (2.f * (g + 0.5f) / G - 1.f);
		}
		// integrate twice, then take out the straight-line part so the ends sit level
		float y[G], s = 0.f, yy = 0.f, dx = 1.f / G;
		for (int g = 0; g < G; g++) { s += K[g] * dx; yy += s * dx; y[g] = yy; }
		float a = y[0], b = y[G - 1];
		float mx = 1e-4f;
		for (int g = 0; g < G; g++) { y[g] -= a + (b - a) * g / (G - 1); mx = std::max(mx, std::fabs(y[g])); }
		float w = box.size.x, h = box.size.y, pad = mm2px(2.5f);
		float amp = (h / 2 - pad) * std::tanh(mx * 12.f) / mx;
		float x0 = pad, x1 = w - pad;
		NVGcontext* vg = args.vg;
		nvgScissor(vg, 0, 0, w, h);
		// water
		if (dip > 0.005f) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, 0, x0 + (x1 - x0) * dip, h);
			nvgFillColor(vg, nvgRGBA(60, 130, 200, 70));
			nvgFill(vg);
		}
		// sheet
		nvgBeginPath(vg);
		for (int g = 0; g < G; g++) {
			float px = x0 + (x1 - x0) * (g + 0.5f) / G, py = h / 2 - amp * y[g];
			if (g == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
		}
		NVGcolor c = nvgLerpRGBA(nvgRGB(150, 160, 170), nvgRGB(240, 244, 248), std::min(1.f, env * 1.5f));
		nvgStrokeColor(vg, c);
		nvgStrokeWidth(vg, 1.6f);
		nvgStroke(vg);
		// bow
		if (bow > 0.f) {
			float bx = x0 + (x1 - x0) * bowX;
			nvgBeginPath(vg);
			nvgMoveTo(vg, bx, pad * 0.6f);
			nvgLineTo(vg, bx, h - pad * 0.6f);
			nvgStrokeColor(vg, nvgRGBA(217, 164, 65, (unsigned char)(80 + 175 * bow)));
			nvgStrokeWidth(vg, 1.4f);
			nvgStroke(vg);
		}
		nvgResetScissor(vg);
	}
};

struct BentSheetMetalWidget : ModuleWidget {
	BentSheetMetalWidget(BentSheetMetal* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BentSheetMetal.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef BentSheetMetal M;
		const int r1[11] = {M::PITCH_PARAM, M::TAPER_PARAM, M::GRIP_PARAM, M::DECAY_PARAM, M::DIP_PARAM, M::DEPTH_PARAM,
			M::STRETCH_PARAM, M::PICKUP_PARAM, M::OUT_PARAM, M::AGE_PARAM, M::LEVEL_PARAM};
		for (int i = 0; i < 11; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col11(i), Y1)), module, r1[i]));

		const int r2[9] = {M::BEND_PARAM, M::SHAPE_PARAM, M::SPOT_PARAM, M::HOLD_PARAM, M::SNAP_PARAM, M::CRACK_PARAM,
			M::BOW_PARAM, M::SPEED_PARAM, M::BOWPOS_PARAM};
		for (int i = 0; i < 9; i++) {
			if (i == 0 || i == 6) addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(ROW2_X[i], Y2)), module, r2[i]));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[i], Y2)), module, r2[i]));
		}

		SheetDisplay* d = new SheetDisplay;
		d->module = module;
		d->box.pos = mm2px(Vec(DISP_X, DISP_Y));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		addChild(d);

		const int r3[3] = {M::HARD_PARAM, M::ROLL_PARAM, M::INLEVEL_PARAM};
		for (int i = 0; i < 3; i++) addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(ROW3_KNOBS[i], Y3)), module, r3[i]));
		const int o3[3] = {M::SNAP_OUTPUT, M::BEND_OUTPUT, M::ENV_OUTPUT};
		for (int i = 0; i < 3; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(ROW3_OUTS[i], Y3)), module, o3[i]));

		const int r4[10] = {M::VOCT_INPUT, M::BENDVOCT_INPUT, M::BEND_INPUT, M::SHAPE_INPUT, M::SPOT_INPUT, M::HOLD_INPUT,
			M::SNAP_INPUT, M::TAPER_INPUT, M::GRIP_INPUT, M::DIP_INPUT};
		for (int i = 0; i < 10; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col10(i), Y4)), module, r4[i]));
		const int r5[9] = {M::BOW_INPUT, M::SPEED_INPUT, M::BOWPOS_INPUT, M::BOWGATE_INPUT, M::STRIKE_INPUT, M::VEL_INPUT,
			M::ROLL_INPUT, M::STRETCH_INPUT, M::IN_INPUT};
		for (int i = 0; i < 9; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col11(i), Y5)), module, r5[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col11(9), Y5)), module, M::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col11(10), Y5)), module, M::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		BentSheetMetal* m = dynamic_cast<BentSheetMetal*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("AGE unit %08llx", (unsigned long long)(m->v.seed >> 32))));
		menu->addChild(createMenuItem("New AGE unit (other drift of hand and rosin)", "", [=]() { m->newUnit(); }));
	}
};

Model* modelBentSheetMetal = createModel<BentSheetMetal, BentSheetMetalWidget>("BentSheetMetal");
