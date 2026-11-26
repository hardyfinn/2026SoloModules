// Molten: Plate Driver's singing sheet with a flame under it. Heat loosens it into chords and roar, sinks its
// partials, melts it into a dense seething liquid, and every quench casts a new tuning.
// Engine in src/dsp/MoltenEngine.hpp, knob mapping in src/dsp/Mapping.hpp (shared with the offline tests).
// Spec: docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/MoltenEngine.hpp"
#include "dsp/Mapping.hpp"

using namespace molten;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float W_MM = 223.52f;
static float colA(int i) { return 45.0f + 21.2f * i; }     // rows 1 and 2 (9 columns right of the big knobs)
static float colS(int i) { return 9.0f + 14.68f * i; }     // row 3 (15 small knobs)
static float colJ(int i) { return 9.0f + 18.68f * i; }     // jack rows (12 columns)
static const float BIG_X = 22.0f;
static const float Y1 = 35.0f, Y2 = 63.0f, Y3 = 87.0f, Y4 = 101.0f, Y5 = 115.0f;
static const float RING_R = 11.0f;
static const float DISP_X0 = 5.0f, DISP_Y = 12.8f, DISP_H = 5.0f;

struct Molten : Module {
	enum ParamIds {
		HEAT_PARAM, QUENCH_PARAM, WATER_PARAM, MASS_PARAM, FLAME_PARAM, WIDTH_PARAM, LATENT_PARAM, SUPERCOOL_PARAM, SELF_PARAM, PINGS_PARAM,
		DRIVE_PARAM, GAIN_PARAM, FOCUS_PARAM, PITCH_PARAM, TRACK_PARAM, POS_PARAM, GRAB_PARAM, RELEASE_PARAM, TENSION_PARAM, LOOSE_PARAM,
		SAG_PARAM, CAST_PARAM, BODY_PARAM, SIZE_PARAM, THICK_PARAM, CURVE_PARAM, DECAY_PARAM,
		PICKUP_PARAM, AMP_PARAM, OUT_PARAM,
		MALLET_PARAM, INLEVEL_PARAM, BRIGHT_PARAM, AGE_PARAM, LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, GATE_INPUT, STRIKE_INPUT, VEL_INPUT, QUENCH_INPUT, IN_INPUT, HEAT_INPUT, FLAME_INPUT, WIDTH_INPUT, TENSION_INPUT, LOOSE_INPUT, DRIVE_INPUT,
		FOCUS_INPUT, SIZE_INPUT, THICK_INPUT, POS_INPUT, GRAB_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { TEMP_OUTPUT, LIQUID_OUTPUT, ROAR_OUTPUT, ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { QUENCH_LIGHT, NUM_LIGHTS };

	molten::Engine* e = nullptr;
	molten::Age age;
	molten::Decimator dec;
	molten::Leveller lev;
	uint64_t seed = 1;
	double rate = 0;
	bool os = true;
	bool needInit = true;
	int ctr = 0;
	double gateS = 1.0;
	double roarS = 0, envS = 0;
	// read by the display widgets (UI thread)
	float ringRoar = 0.f, ringEnv = 0.f, ringTemp = 0.f;
	float zoneT[K] = {}, zonePhi[K] = {};
	float flameX = 0.3f, flameW = 2.f, flameHeat = 0.f;
	bool quenchShown = false;
	dsp::SchmittTrigger strikeTrig, gateTrig, quenchTrig;
	molten::Knobs k;
	double castSaved[N];
	bool castLoaded = false;

	Molten() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		struct HeatQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double v = 2.0 * getValue();
				const char* w = v < 0.15 ? "cold" : v < 0.6 ? "warm" : v < 0.95 ? "red hot" : v < 1.05 ? "melting point" : v < 1.5 ? "molten" : "white hot";
				return string::f("%.2f (%s)", v, w);
			}
		};
		configParam<HeatQ>(HEAT_PARAM, 0.f, 1.f, 0.f, "Heat (flame temperature; noon is the melting point)")->description =
			"The body follows the flame with a lag set by MASS. Warm metal splits into chords at a drive where cold metal sings one tone; red-hot metal sinks; past noon it melts.";
		configButton(QUENCH_PARAM, "Quench (hold: flame off, body in water)");
		configParam(WATER_PARAM, 0.f, 1.f, 0.4f, "Quench strength", "%", 0.f, 100.f)->description = "How violently the water cools the body while QUENCH is held.";
		struct MassQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%.2f s", 1.0 / (12.0 * std::pow(0.0125, (double)getValue()))); }
		};
		configParam<MassQ>(MASS_PARAM, 0.f, 1.f, 0.3f, "Mass (how slowly the body follows the flame)")->description = "Time constant of the heating, from a flash to about a minute.";
		configParam(FLAME_PARAM, 0.f, 1.f, 0.3f, "Flame position", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, 1.f, "Flame width", "%", 0.f, 100.f)->description =
			"Wide heats the whole body evenly. Narrow heats one spot: only some partials sink and only part of the body melts.";
		configParam(LATENT_PARAM, 0.f, 1.f, 0.45f, "Latent heat (how long it lingers half molten)", "%", 0.f, 100.f);
		configParam(SUPERCOOL_PARAM, 0.f, 1.f, 0.f, "Supercool", "%", 0.f, 100.f)->description =
			"How far below the melting point the liquid can be cooled before it freezes by itself. Turned up, it waits for a hard strike and then flashes solid.";
		configParam(SELF_PARAM, 0.f, 1.f, 0.f, "Self-heating (loud playing heats the body)", "%", 0.f, 100.f);
		configParam(PINGS_PARAM, 0.f, 1.f, 0.3f, "Stress pings (on heating and freezing)", "%", 0.f, 100.f);

		struct DriveQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double v = getValue();
				return v <= 0 ? "off" : string::f("%.0f%%  (force %.0f at C4)", v * 100.0, driveForce(v));
			}
		};
		configParam<DriveQ>(DRIVE_PARAM, 0.f, 1.f, 0.35f, "Drive")->description =
			"How hard the driver pushes the sheet. Low: one pure tone. Middle: chords. High: roar. Heat lowers all three thresholds.";
		configParam(GAIN_PARAM, 0.f, 1.f, std::log(3.f) / std::log(30.f), "Loop gain", "", 30.f, 20.f)->description =
			"Low gain starts slowly and organically; high gain starts at once and hits hard.";
		configParam(FOCUS_PARAM, 0.f, 1.f, 0.5f, "Focus", "%", 0.f, 100.f)->description =
			"A band-pass in the loop at the played pitch. High focus keeps singing near the pitch while the body sags past it; low focus lets the song sink with the body.";
		configParam(PITCH_PARAM, -3.f, 3.f, 0.f, "Pitch (of the cold, uncast body)", " Hz", 2.f, (float)C4);
		configSwitch(TRACK_PARAM, 0.f, 1.f, 1.f, "Track", {"Focus (only the loop filter follows pitch)", "Plate (the whole body follows pitch)"});
		configParam(POS_PARAM, 0.05f, 0.95f, 0.31f, "Driver and pickup position", "%", 0.f, 100.f);
		configParam(GRAB_PARAM, 0.f, 1.f, 0.f, "Grab (hand on the sheet)", "%", 0.f, 100.f);
		configParam(RELEASE_PARAM, 0.f, 1.f, 0.f, "Release damping (while the gate is low)", "%", 0.f, 100.f);
		configParam(TENSION_PARAM, -2.f, 1.f, 0.f, "Tension (liquid pitch against the cold body)", " oct")->description =
			"Surface tension sets the pitch of the molten body: from two octaves under to an octave over the cold sheet.";
		configParam(LOOSE_PARAM, 0.f, 1.f, 0.625f, "Looseness (how readily hot metal and liquid split and roar)", "%", 0.f, 100.f);

		configParam(SAG_PARAM, 0.f, 1.f, 0.67f, "Sag (how far heat sinks the solid's pitch)", "%", 0.f, 100.f);
		configParam(CAST_PARAM, 0.f, 1.f, 0.5f, "Cast (how far each freeze retunes the body)", "%", 0.f, 100.f);
		configParam(BODY_PARAM, 0.f, 15.f, 0.f, "Body", "")->snapEnabled = true;
		getParamQuantity(BODY_PARAM)->description = "Which sheet out of sixteen: the fine detail of its modes and couplings.";
		configParam(SIZE_PARAM, 0.f, 1.f, std::log10(65.4f / 20.f), "Size (lowest mode at C4)", " Hz", 10.f, 20.f);
		configParam(THICK_PARAM, 0.f, 1.f, 0.5f, "Thickness", "%", 0.f, 100.f)->description = "Thin sheets stretch and bloom readily; thick ones stay pure longer.";
		configParam(CURVE_PARAM, 0.f, 1.f, 0.6f, "Curvature", "%", 0.f, 100.f);
		struct DecayQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%.2f s", 9.0 * getValue()); }
		};
		configParam<DecayQ>(DECAY_PARAM, 0.15f, 4.f, 1.f, "Decay (lowest mode cold, T60)");
		configParam(PICKUP_PARAM, 0.f, 1.f, 0.2f, "Pickup saturation", "%", 0.f, 100.f);
		configParam(AMP_PARAM, 0.f, 1.f, 0.2f, "Amplifier saturation", "%", 0.f, 100.f);
		configParam(OUT_PARAM, 0.f, 1.f, 0.f, "Output saturation", "%", 0.f, 100.f);
		configParam(MALLET_PARAM, 0.f, 1.f, 0.5f, "Mallet (strike brightness)", " Hz", 10.f, 300.f);
		configParam(INLEVEL_PARAM, 0.f, 1.f, 0.5f, "Input level", "%", 0.f, 100.f);
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.5f, "Bright", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"Impurity and analog looseness: a flickering flame, uneven zones, slag, dented modes, loop drift, hiss, hum and warm-up.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.7f, "Level", "%", 0.f, 100.f)->description = "At 70% a cold pure tone is about 5 V peak.";
		getParamQuantity(LEVEL_PARAM)->randomizeEnabled = false;
		getParamQuantity(QUENCH_PARAM)->randomizeEnabled = false;

		configInput(VOCT_INPUT, "V/OCT");
		configInput(GATE_INPUT, "Gate (drive on while high; on when unpatched)");
		configInput(STRIKE_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Strike velocity (0 to 10 V; 8 V when unpatched). Above 6 V a strike freezes supercooled liquid");
		configInput(QUENCH_INPUT, "Quench gate");
		configInput(IN_INPUT, "Audio into the loop");
		configInput(HEAT_INPUT, "HEAT CV");
		configInput(FLAME_INPUT, "FLAME CV");
		configInput(WIDTH_INPUT, "WIDTH CV");
		configInput(TENSION_INPUT, "TENSION CV (0.3 octave per volt)");
		configInput(LOOSE_INPUT, "LOOSE CV");
		configInput(DRIVE_INPUT, "DRIVE CV");
		configInput(FOCUS_INPUT, "FOCUS CV");
		configInput(SIZE_INPUT, "SIZE CV (0.1 decade per volt)");
		configInput(THICK_INPUT, "THICK CV");
		configInput(POS_INPUT, "POS CV");
		configInput(GRAB_INPUT, "GRAB CV (10 V = full)");
		configOutput(TEMP_OUTPUT, "Temperature (0 V cold, 5 V melting point, 10 V white hot)");
		configOutput(LIQUID_OUTPUT, "Liquid (0 V solid, 10 V all molten)");
		configOutput(ROAR_OUTPUT, "Roar (0 V one tone, 10 V full roar)");
		configOutput(ENV_OUTPUT, "Energy envelope (0 to 10 V)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		e = new molten::Engine();
		seed = random::u64();
		for (int j = 0; j < N; j++) castSaved[j] = 1.0;
	}
	~Molten() { delete e; }

	void init(double sr) {
		rate = sr;
		os = sr < 88200.0;
		e->setRate(os ? 2 * sr : sr);
		if (e->body != 0 || !castLoaded) e->build(0);
		e->reset();
		age.init(seed);
		readKnobs();
		mapKnobs(k, *e, &age);
		e->setThermal(0.0);
		if (castLoaded) for (int j = 0; j < N; j++) e->cast[j] = e->castT[j] = castSaved[j];
		castLoaded = false;
		lev.setRate(sr); lev.reset(); dec.reset();
		ctr = 0;
		needInit = false;
	}
	void newUnit() { seed = random::u64(); age.init(seed); }

	void onReset() override { needInit = true; for (int j = 0; j < N; j++) e->castT[j] = 1.0; }
	void onSampleRateChange(const SampleRateChangeEvent&) override {
		// keep the casting through a rate change
		for (int j = 0; j < N; j++) castSaved[j] = e->castT[j];
		castLoaded = true; needInit = true;
	}

	double cv(int id) { return inputs[id].getVoltage(); }

	void readKnobs() {
		auto p = [&](int id) { return (double)params[id].getValue(); };
		k.heat = clampd(p(HEAT_PARAM) + 0.1 * cv(HEAT_INPUT), 0, 1);
		k.quenchAmt = p(WATER_PARAM);
		bool qGate = inputs[QUENCH_INPUT].isConnected() && quenchTrig.isHigh();
		k.quenching = p(QUENCH_PARAM) > 0.5 || qGate;
		k.mass = p(MASS_PARAM);
		k.flame = clampd(p(FLAME_PARAM) + 0.1 * cv(FLAME_INPUT), 0, 1);
		k.width = clampd(p(WIDTH_PARAM) + 0.1 * cv(WIDTH_INPUT), 0, 1);
		k.latent = p(LATENT_PARAM);
		k.supercool = p(SUPERCOOL_PARAM);
		k.self = p(SELF_PARAM);
		k.pings = p(PINGS_PARAM);
		k.drive = clampd(p(DRIVE_PARAM) + 0.1 * cv(DRIVE_INPUT), 0, 1);
		k.gain = 20.0 * std::pow(30.0, clampd(p(GAIN_PARAM), 0, 1));
		k.focus = clampd(p(FOCUS_PARAM) + 0.1 * cv(FOCUS_INPUT), 0, 1);
		k.pitch = p(PITCH_PARAM) + inputs[VOCT_INPUT].getVoltage();
		k.trackPlate = p(TRACK_PARAM) > 0.5;
		k.pos = clampd(p(POS_PARAM) + 0.1 * cv(POS_INPUT), 0.05, 0.95);
		k.grab = clampd(p(GRAB_PARAM) + 0.1 * cv(GRAB_INPUT), 0, 1);
		k.release = p(RELEASE_PARAM);
		k.tension = clampd(p(TENSION_PARAM) + 0.3 * cv(TENSION_INPUT), -2, 1);
		k.loose = clampd(p(LOOSE_PARAM) + 0.1 * cv(LOOSE_INPUT), 0, 1);
		k.sag = p(SAG_PARAM);
		k.cast = p(CAST_PARAM);
		k.body = clamp((int)std::round(p(BODY_PARAM)), 0, 15);
		k.size = 20.0 * std::pow(10.0, clampd(p(SIZE_PARAM) + 0.1 * cv(SIZE_INPUT), 0, 1));
		k.thick = clampd(p(THICK_PARAM) + 0.1 * cv(THICK_INPUT), 0, 1);
		k.curve = p(CURVE_PARAM);
		k.decay = p(DECAY_PARAM);
		k.pick = p(PICKUP_PARAM); k.amp = p(AMP_PARAM); k.out = p(OUT_PARAM);
		k.mallet = p(MALLET_PARAM);
		k.inLevel = p(INLEVEL_PARAM);
		k.bright = p(BRIGHT_PARAM);
		k.age = p(AGE_PARAM);
		k.gate = gateS;
	}

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) init(args.sampleRate);

		bool gateHigh = true;
		if (inputs[GATE_INPUT].isConnected()) { gateTrig.process(inputs[GATE_INPUT].getVoltage(), 0.1f, 1.f); gateHigh = gateTrig.isHigh(); }
		gateS += ((gateHigh ? 1.0 : 0.0) - gateS) * (1.0 - std::exp(-args.sampleTime / 0.004));
		if (inputs[QUENCH_INPUT].isConnected()) quenchTrig.process(inputs[QUENCH_INPUT].getVoltage(), 0.1f, 1.f);

		const int ctrlLen = molten::BLK / (os ? 2 : 1);
		if (ctr == 0) {
			int oldBody = e->body;
			readKnobs();
			age.update(molten::BLK / e->fe, e->emean);
			mapKnobs(k, *e, &age);
			if (e->body != oldBody) e->reset();
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
		outputs[TEMP_OUTPUT].setVoltage((float)(10.0 * clampd(e->Tmean / 2.0, 0, 1)));
		outputs[LIQUID_OUTPUT].setVoltage((float)(10.0 * clampd(e->M, 0, 1)));
		lights[QUENCH_LIGHT].setBrightness(k.quenching ? 1.f : 0.f);

		if (ctr == 0) {
			ringRoar = (float)roarS; ringEnv = (float)envS; ringTemp = (float)e->Tmean;
			for (int z = 0; z < K; z++) { zoneT[z] = (float)e->T[z]; zonePhi[z] = (float)e->phi[z]; }
			flameX = (float)e->c.xf; flameW = (float)e->c.wf; flameHeat = (float)e->c.heat;
			quenchShown = k.quenching;
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(string::f("%016llx", (unsigned long long)seed).c_str()));
		json_t* arr = json_array();
		bool any = false;
		for (int j = 0; j < N; j++) { if (e->castT[j] != 1.0) any = true; json_array_append_new(arr, json_real(e->castT[j])); }
		if (any) json_object_set_new(root, "casting", arr); else json_decref(arr);
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) seed = std::strtoull(json_string_value(s), nullptr, 16);
		for (int j = 0; j < N; j++) castSaved[j] = 1.0;
		json_t* arr = json_object_get(root, "casting");
		if (arr && json_is_array(arr) && (int)json_array_size(arr) == N)
			for (int j = 0; j < N; j++) castSaved[j] = clampd(json_number_value(json_array_get(arr, j)), 0.5, 1.5);
		castLoaded = true;
		needInit = true;
	}
};

// Colour of metal at temperature t (0 cold, 1 melting point, 2 white hot).
static NVGcolor glow(float t) {
	t = std::max(0.f, std::min(2.f, t));
	struct S { float t; int r, g, b; };
	static const S s[] = {{0.0f, 28, 30, 33}, {0.3f, 60, 22, 18}, {0.6f, 150, 30, 15}, {0.9f, 220, 80, 20}, {1.2f, 250, 150, 40}, {1.6f, 255, 220, 120}, {2.0f, 255, 250, 235}};
	for (int i = 1; i < 7; i++)
		if (t <= s[i].t) {
			float x = (t - s[i - 1].t) / (s[i].t - s[i - 1].t);
			return nvgRGB((int)(s[i - 1].r + x * (s[i].r - s[i - 1].r)), (int)(s[i - 1].g + x * (s[i].g - s[i - 1].g)), (int)(s[i - 1].b + x * (s[i].b - s[i - 1].b)));
		}
	return nvgRGB(255, 250, 235);
}

// The strip of 16 zones: glowing by temperature, a blue-white edge on molten zones, a flame marker.
struct ZoneDisplay : TransparentWidget {
	Molten* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		float w = box.size.x, hgt = box.size.y, cw = w / K;
		for (int z = 0; z < K; z++) {
			float t = module ? module->zoneT[z] : 0.f, ph = module ? module->zonePhi[z] : 0.f;
			if (!module) t = 0.15f + 1.7f * std::exp(-std::pow((z - 5.f) / 3.f, 2.f)), ph = z >= 4 && z <= 6 ? 1.f : 0.f;   // browser preview
			nvgBeginPath(args.vg);
			nvgRect(args.vg, z * cw + 0.6f, 0.6f, cw - 1.2f, hgt - 1.2f);
			nvgFillColor(args.vg, glow(t));
			nvgFill(args.vg);
			if (ph > 0.02f) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, z * cw + 0.6f, 0.6f, cw - 1.2f, hgt - 1.2f);
				NVGcolor c = nvgRGB(170, 210, 255); c.a = 0.25f + 0.75f * ph;
				nvgStrokeColor(args.vg, c);
				nvgStrokeWidth(args.vg, 1.4f);
				nvgStroke(args.vg);
			}
		}
		if (module) {
			// flame marker under the strip: width shows how wide the flame is, brightness its heat
			float fx = module->flameX * w, fw = std::min(w, module->flameW * w * 1.2f);
			NVGcolor c = module->quenchShown ? nvgRGB(120, 190, 255) : nvgRGB(255, 170, 60);
			c.a = module->quenchShown ? 0.9f : 0.25f + 0.75f * std::min(1.f, module->flameHeat / 2.f);
			nvgBeginPath(args.vg);
			if (module->quenchShown) nvgRect(args.vg, 0, hgt - 1.2f, w, 1.2f);
			else nvgRect(args.vg, std::max(0.f, fx - fw / 2), hgt - 1.2f, std::min(w, fx + fw / 2) - std::max(0.f, fx - fw / 2), 1.2f);
			nvgFillColor(args.vg, c);
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, fx - 2.5f, hgt + 2.5f); nvgLineTo(args.vg, fx + 2.5f, hgt + 2.5f); nvgLineTo(args.vg, fx, hgt - 0.5f); nvgClosePath(args.vg);
			NVGcolor m = nvgRGB(255, 170, 60); m.a = 0.9f;
			nvgFillColor(args.vg, m);
			nvgFill(args.vg);
		}
	}
};

// Ring around DRIVE (dim blue one tone, white chords, red roar) and around HEAT (the body's mean temperature).
struct Ring : TransparentWidget {
	Molten* module = nullptr;
	float radius = 0;
	bool heatRing = false;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || !module) return;
		NVGcolor c; float a;
		if (heatRing) {
			c = glow(module->ringTemp);
			a = std::min(1.f, 0.1f + module->ringTemp);
		} else {
			float roar = module->ringRoar, en = module->ringEnv;
			if (roar < 0.5f) c = nvgLerpRGBA(nvgRGB(60, 110, 255), nvgRGB(240, 240, 240), roar * 2.f);
			else c = nvgLerpRGBA(nvgRGB(240, 240, 240), nvgRGB(255, 40, 30), (roar - 0.5f) * 2.f);
			a = 0.15f + 0.85f * std::min(1.f, en * 1.5f);
		}
		if (a <= 0.02f) return;
		c.a = a;
		Vec ctr = box.size.div(2);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, ctr.x, ctr.y, radius);
		nvgStrokeColor(args.vg, c);
		nvgStrokeWidth(args.vg, 2.2f);
		nvgStroke(args.vg);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, ctr.x, ctr.y, radius);
		NVGcolor hc = c; hc.a = a * 0.25f;
		nvgStrokeColor(args.vg, hc);
		nvgStrokeWidth(args.vg, 6.f);
		nvgStroke(args.vg);
	}
};

struct MoltenWidget : ModuleWidget {
	MoltenWidget(Molten* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Molten.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Molten M;
		{
			ZoneDisplay* d = new ZoneDisplay;
			d->module = module;
			d->box.pos = mm2px(Vec(DISP_X0, DISP_Y));
			d->box.size = mm2px(Vec(W_MM - 2 * DISP_X0, DISP_H));
			addChild(d);
		}
		auto ring = [&](float y, bool heat) {
			Ring* r = new Ring;
			r->module = module; r->heatRing = heat;
			float rpx = mm2px(RING_R);
			r->radius = rpx;
			r->box.size = Vec(2 * rpx + 8, 2 * rpx + 8);
			r->box.pos = mm2px(Vec(BIG_X, y)).minus(r->box.size.div(2));
			addChild(r);
		};
		ring(Y1, true);
		ring(Y2, false);
		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(BIG_X, Y1)), module, M::HEAT_PARAM));
		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(BIG_X, Y2)), module, M::DRIVE_PARAM));
		// row 1
		addParam(createLightParamCentered<VCVLightBezel<BlueLight>>(mm2px(Vec(colA(0), Y1)), module, M::QUENCH_PARAM, M::QUENCH_LIGHT));
		const int r1[8] = {M::WATER_PARAM, M::MASS_PARAM, M::FLAME_PARAM, M::WIDTH_PARAM, M::LATENT_PARAM, M::SUPERCOOL_PARAM, M::SELF_PARAM, M::PINGS_PARAM};
		for (int i = 0; i < 8; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colA(i + 1), Y1)), module, r1[i]));
		// row 2
		const int r2[3] = {M::GAIN_PARAM, M::FOCUS_PARAM, M::PITCH_PARAM};
		for (int i = 0; i < 3; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colA(i), Y2)), module, r2[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colA(3), Y2)), module, M::TRACK_PARAM));
		const int r2b[5] = {M::POS_PARAM, M::GRAB_PARAM, M::RELEASE_PARAM, M::TENSION_PARAM, M::LOOSE_PARAM};
		for (int i = 0; i < 5; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colA(i + 4), Y2)), module, r2b[i]));
		// row 3
		const int r3[15] = {M::SAG_PARAM, M::CAST_PARAM, M::BODY_PARAM, M::SIZE_PARAM, M::THICK_PARAM, M::CURVE_PARAM, M::DECAY_PARAM,
			M::PICKUP_PARAM, M::AMP_PARAM, M::OUT_PARAM, M::MALLET_PARAM, M::INLEVEL_PARAM, M::BRIGHT_PARAM, M::AGE_PARAM, M::LEVEL_PARAM};
		for (int i = 0; i < 15; i++) addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colS(i), Y3)), module, r3[i]));   // BODY snaps (snapEnabled)
		// jacks
		const int j4[12] = {M::VOCT_INPUT, M::GATE_INPUT, M::STRIKE_INPUT, M::VEL_INPUT, M::QUENCH_INPUT, M::IN_INPUT, M::HEAT_INPUT, M::FLAME_INPUT, M::WIDTH_INPUT, M::TENSION_INPUT, M::LOOSE_INPUT, M::DRIVE_INPUT};
		for (int i = 0; i < 12; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colJ(i), Y4)), module, j4[i]));
		const int j5[5] = {M::FOCUS_INPUT, M::SIZE_INPUT, M::THICK_INPUT, M::POS_INPUT, M::GRAB_INPUT};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colJ(i), Y5)), module, j5[i]));
		const int o5[6] = {M::TEMP_OUTPUT, M::LIQUID_OUTPUT, M::ROAR_OUTPUT, M::ENV_OUTPUT, M::L_OUTPUT, M::R_OUTPUT};
		for (int i = 0; i < 6; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colJ(i + 6), Y5)), module, o5[i]));
	}

	void appendContextMenu(Menu* menu) override {
		Molten* m = dynamic_cast<Molten*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		int changed = 0; double dev = 0;
		for (int j = 0; j < N; j++) { double d = std::fabs(m->e->castT[j] - 1.0); if (d > 1e-6) changed++; dev = std::max(dev, d); }
		menu->addChild(createMenuLabel(changed ? string::f("Casting: %d modes retuned, up to %.1f%%", changed, 100.0 * dev) : std::string("Casting: original tuning")));
		menu->addChild(createMenuLabel(string::f("Body %d, castings this session %ld", m->e->body, m->e->casts)));
		menu->addChild(createMenuItem("Forget casting (back to the original tuning)", "", [=]() { m->e->forgetCasting(); }));
		menu->addChild(createMenuLabel(string::f("AGE unit %08llx", (unsigned long long)(m->seed >> 32))));
		menu->addChild(createMenuItem("New AGE unit (other dents, other uneven zones)", "", [=]() { m->newUnit(); }));
	}
};

Model* modelMolten = createModel<Molten, MoltenWidget>("Molten");
