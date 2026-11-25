// Long Metal: a very long line of metal heard from one point on it. Rails that sing before the train arrives,
// struck cables that zing and crack, and a line that amplifies itself and screams.
// Engine in src/dsp/LineEngine.hpp, knob mapping and the whole module chain in src/dsp/Mapping.hpp
// (shared with the offline tests). Spec: docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/LineEngine.hpp"
#include "dsp/Mapping.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static float col11(int i) { return 12.6f + 17.8f * i; }
static float col10(int i) { return 13.9f + 19.5f * i; }
static float col12(int i) { return 10.4f + 16.58f * i; }
static const float Y1 = 39.5f, Y2 = 62.0f, Y3 = 88.0f, Y4 = 104.5f, Y5 = 118.5f;
static const float DISP_X = 41.6f, DISP_Y = 13.0f, DISP_W = 156.6f, DISP_H = 11.0f;
static const float ROW2_X[9] = {17.0f, 43.0f, 63.0f, 80.0f, 95.0f, 117.0f, 134.0f, 151.0f, 168.0f};
static const float ROW3_X[9] = {12.6f, 30.4f, 48.2f, 66.0f, 83.8f, 101.6f, 133.0f, 154.0f, 175.0f};
static const int DISP_BINS = 80;

struct LongMetal : Module {
	// Param order is the preset order (tools/make_presets.py).
	enum ParamIds {
		PITCH_PARAM, LENGTH_PARAM, SPACING_PARAM, GAUGE_PARAM, STIFF_PARAM, SUPPORTS_PARAM, ENDA_PARAM, ENDB_PARAM,
		GROUND_PARAM, BRIGHT_PARAM, AGE_PARAM,
		LIVE_PARAM, DISTANCE_PARAM, LISTEN_PARAM, PASSTIME_PARAM, PASS_PARAM, WARP_PARAM, SWELL_PARAM, LOOSE_PARAM, GRIP_PARAM,
		TRAIN_PARAM, SPEED_PARAM, FLATS_PARAM, STRIKE_PARAM, HARD_PARAM, INLEVEL_PARAM, EVEN_PARAM, WIDTH_PARAM, LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT, STIFF_INPUT, SUPPORTS_INPUT, LIVE_INPUT, GROUND_INPUT, LISTEN_INPUT, DISTANCE_INPUT, PASS_INPUT, TRAIN_INPUT, SPEED_INPUT,
		STRIKE_INPUT, VEL_INPUT, WARP_INPUT, SWELL_INPUT, LOOSE_INPUT, GRIP_INPUT, IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { ENV_OUTPUT, CLACK_OUTPUT, PASS_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { PASS_LIGHT, STRIKE_LIGHT, NUM_LIGHTS };

	longmetal::Voice v;
	longmetal::Knobs k;
	double rate = 0;
	bool needInit = true;
	dsp::SchmittTrigger strikeTrig, passTrig, strikeBtn, passBtn;
	dsp::PulseGenerator clackPulse, strikeFlash;

	// read by the display (written about 60 times a second; a torn read only costs one odd frame)
	float dispE[DISP_BINS] = {};
	float dispAx[longmetal::NAX] = {};
	float dispPick = 0.12f, dispSrc = 0.82f, dispEndA = 0.3f, dispEndB = 0.3f, dispStuck = 0.f;
	int dispCtr = 0;

	LongMetal() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, -2.f, 1.f, 0.f, "Pitch (size of the whole structure)", " oct")->description =
			"Scales everything together: support tones, echo time and zing length. Adds to V/OCT.";
		configParam(LENGTH_PARAM, 16.f, 320.f, 320.f, "Length", " cells")->snapEnabled = true;
		getParamQuantity(LENGTH_PARAM)->description = "How much of the line is in play: a short pitched cable to a long line with seconds between echoes.";
		configParam(SPACING_PARAM, 1.f, 40.f, 9.f, "Spacing (between supports)")->snapEnabled = true;
		getParamQuantity(SPACING_PARAM)->description = "Coarse tuning of the support tones and of the echo time. Steps on purpose.";
		configSwitch(GAUGE_PARAM, 0.f, 1.f, 0.f, "Gauge", {"Light", "Heavy"});
		configParam(STIFF_PARAM, 0.f, 1.f, 0.51f, "Stiff (rope to rail)", "%", 0.f, 100.f)->description =
			"How much faster high frequencies travel than low ones. Stiff metal turns a far click into a falling zing.";
		configParam(SUPPORTS_PARAM, 0.f, 1.f, 0.71f, "Supports (sleepers, clamps)", "%", 0.f, 100.f)->description =
			"Below 30% the supports only notch the sound. Above 60% the line rings on glassy, inharmonic tones.";
		configParam(ENDA_PARAM, -1.f, 1.f, 0.31f, "End A (left end)", "%", 0.f, 100.f)->description = "0 absorbs, +-100% reflects; the sign flips the wave.";
		configParam(ENDB_PARAM, -1.f, 1.f, 0.31f, "End B (right end)", "%", 0.f, 100.f)->description = "0 absorbs, +-100% reflects; the sign flips the wave.";
		configParam(GROUND_PARAM, 0.f, 1.f, 0.5f, "Ground (how much low end the ballast soaks up)", "%", 0.f, 100.f);
		configParam(BRIGHT_PARAM, 0.f, 1.f, 0.42f, "Bright", "%", 0.f, 100.f)->description = "Less high-frequency loss along the line.";
		configParam(AGE_PARAM, 0.f, 1.f, 0.35f, "Age", "%", 0.f, 100.f)->description =
			"Irregular sleepers, uneven stiffness, dead and over-tight fastenings, patchy loss, worn wheels, thermal drift. At 0 the line is perfect.";

		struct LiveQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double g = longmetal::gainFromLive(getValue());
				if (g > 1.0) return string::f("%.0f%%  (sings by itself: +%.2f%% per cell)", getValue() * 100.0, (g - 1) * 100.0);
				return string::f("%.0f%%  (damped: -%.2f%% per cell)", getValue() * 100.0, (1 - g) * 100.0);
			}
		};
		configParam<LiveQ>(LIVE_PARAM, 0.f, 1.f, 0.225f, "Live")->description =
			"Loss along the line. Past the middle the line amplifies itself and sings with nothing playing it. Gate it with a square wave.";
		configParam(DISTANCE_PARAM, -1.f, 1.f, 0.7f, "Distance (where the train, hammer and input are)", "%", 0.f, 100.f)->description =
			"Measured from the listener; positive is to the right. Far is a thin glassy shimmer, near is the full impact.";
		configParam(LISTEN_PARAM, 0.f, 1.f, 0.12f, "Listen (where you stand on the line)", "%", 0.f, 100.f);
		configParam(PASSTIME_PARAM, 4.f, 120.f, 30.f, "Pass time", " s");
		configButton(PASS_PARAM, "Pass (the train comes through)");
		configParam(WARP_PARAM, 0.f, 1.f, 0.f, "Warp (loud waves travel differently)", "%", 0.f, 100.f)->description =
			"Stiffness follows the signal itself: a hard hit sprays into new frequencies.";
		configParam(SWELL_PARAM, 0.f, 1.f, 0.f, "Swell (energy bends the pitch)", "%", 0.f, 100.f);
		configParam(LOOSE_PARAM, 0.f, 1.f, 0.f, "Loose (rattling supports)", "%", 0.f, 100.f)->description = "The supports reflect more as the wave gets bigger.";
		configParam(GRIP_PARAM, 0.f, 1.f, 0.f, "Grip (break-loose threshold)", "%", 0.f, 100.f)->description =
			"Off at 0. The line is held by friction until a wave is strong enough to tear it loose, then it cracks and the release runs down the line. Soft hits do nothing.";

		configParam(TRAIN_PARAM, 0.f, 1.f, 0.5f, "Train", "%", 0.f, 100.f)->description = "Wheels rolling on the line. At rest the train rolls on the spot: a steady shimmer.";
		configParam(SPEED_PARAM, 0.f, 1.f, 0.49f, "Speed (wheel rate and grit)", "%", 0.f, 100.f);
		configParam(FLATS_PARAM, 0.f, 1.f, 0.5f, "Flats (wheel flats and joint clacks)", "%", 0.f, 100.f);
		configButton(STRIKE_PARAM, "Strike");
		configParam(HARD_PARAM, 0.f, 1.f, 0.8f, "Hard (hammer)", "%", 0.f, 100.f);
		configParam(INLEVEL_PARAM, 0.f, 1.f, 0.5f, "Input level", "%", 0.f, 100.f);
		configParam(EVEN_PARAM, 0.f, 1.f, 1.f, "Even (levelling)", "%", 0.f, 100.f)->description =
			"Near and far differ by 40 dB. EVEN brings them closer; at 0 you hear the real difference.";
		configParam(WIDTH_PARAM, 0.f, 1.f, 1.f, "Width", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.7f, "Level", "%", 0.f, 100.f);
		getParamQuantity(LEVEL_PARAM)->randomizeEnabled = false;
		getParamQuantity(LENGTH_PARAM)->randomizeEnabled = false;

		configInput(VOCT_INPUT, "V/OCT (whole structure)");
		configInput(STIFF_INPUT, "STIFF CV");
		configInput(SUPPORTS_INPUT, "SUPPORTS CV");
		configInput(LIVE_INPUT, "LIVE CV (audio rate is fine; a square gates the singing)");
		configInput(GROUND_INPUT, "GROUND CV");
		configInput(LISTEN_INPUT, "LISTEN CV");
		configInput(DISTANCE_INPUT, "DISTANCE CV (5 V = full swing; audio rate is fine)");
		configInput(PASS_INPUT, "PASS trigger");
		configInput(TRAIN_INPUT, "TRAIN CV");
		configInput(SPEED_INPUT, "SPEED CV");
		configInput(STRIKE_INPUT, "Strike trigger");
		configInput(VEL_INPUT, "Velocity for STRIKE (0 to 10 V; 6 V when unpatched)");
		configInput(WARP_INPUT, "WARP CV");
		configInput(SWELL_INPUT, "SWELL CV");
		configInput(LOOSE_INPUT, "LOOSE CV");
		configInput(GRIP_INPUT, "GRIP CV");
		configInput(IN_INPUT, "Audio in (enters the line at DISTANCE)");
		configOutput(ENV_OUTPUT, "Energy at the listener (0 to 10 V)");
		configOutput(CLACK_OUTPUT, "Clack trigger (rail joints, wheel flats, GRIP releases)");
		configOutput(PASS_OUTPUT, "Pass gate (high while the train comes through)");
		configOutput(L_OUTPUT, "Left (waves arriving from the left)");
		configOutput(R_OUTPUT, "Right (waves arriving from the right)");
	}

	void onReset() override { needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	double cv(int id) { return inputs[id].getVoltage(); }

	void readKnobs() {
		using longmetal::clampd;
		auto p = [&](int id) { return (double)params[id].getValue(); };
		k.pitch = p(PITCH_PARAM) + cv(VOCT_INPUT);
		k.length = p(LENGTH_PARAM);
		k.spacing = p(SPACING_PARAM);
		k.heavy = p(GAUGE_PARAM) > 0.5;
		k.stiff = clampd(p(STIFF_PARAM) + 0.1 * cv(STIFF_INPUT), 0, 1);
		k.supports = clampd(p(SUPPORTS_PARAM) + 0.1 * cv(SUPPORTS_INPUT), 0, 1);
		k.endA = p(ENDA_PARAM); k.endB = p(ENDB_PARAM);
		k.live = clampd(p(LIVE_PARAM) + 0.1 * cv(LIVE_INPUT), 0, 1);
		k.ground = clampd(p(GROUND_PARAM) + 0.1 * cv(GROUND_INPUT), 0, 1);
		k.bright = p(BRIGHT_PARAM);
		k.listen = clampd(p(LISTEN_PARAM) + 0.1 * cv(LISTEN_INPUT), 0, 1);
		k.distance = clampd(p(DISTANCE_PARAM) + 0.2 * cv(DISTANCE_INPUT), -1, 1);
		k.passTime = p(PASSTIME_PARAM);
		k.train = clampd(p(TRAIN_PARAM) + 0.1 * cv(TRAIN_INPUT), 0, 1);
		k.speed = clampd(p(SPEED_PARAM) + 0.1 * cv(SPEED_INPUT), 0, 1);
		k.flats = p(FLATS_PARAM);
		k.hard = p(HARD_PARAM);
		k.inLevel = p(INLEVEL_PARAM);
		k.warp = clampd(p(WARP_PARAM) + 0.1 * cv(WARP_INPUT), 0, 1);
		k.swell = clampd(p(SWELL_PARAM) + 0.1 * cv(SWELL_INPUT), 0, 1);
		k.loose = clampd(p(LOOSE_PARAM) + 0.1 * cv(LOOSE_INPUT), 0, 1);
		k.grip = clampd(p(GRIP_PARAM) + 0.1 * cv(GRIP_INPUT), 0, 1);
		k.age = p(AGE_PARAM);
		k.even = p(EVEN_PARAM); k.width = p(WIDTH_PARAM); k.level = p(LEVEL_PARAM);
	}

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) { rate = args.sampleRate; v.init(rate); needInit = false; }
		readKnobs();

		double vel = inputs[VEL_INPUT].isConnected() ? longmetal::clampd(cv(VEL_INPUT) / 10.0, 0, 1) : 0.6;
		double strikeVel = -1;
		bool st = strikeTrig.process(inputs[STRIKE_INPUT].getVoltage(), 0.1f, 1.f);
		bool sb = strikeBtn.process(params[STRIKE_PARAM].getValue());
		if ((st || sb) && vel > 0) { strikeVel = vel; strikeFlash.trigger(0.08f); }
		bool pt = passTrig.process(inputs[PASS_INPUT].getVoltage(), 0.1f, 1.f) | passBtn.process(params[PASS_PARAM].getValue());

		v.process(k, strikeVel, pt, cv(IN_INPUT));

		outputs[L_OUTPUT].setVoltage((float)v.outL);
		outputs[R_OUTPUT].setVoltage((float)v.outR);
		outputs[ENV_OUTPUT].setVoltage((float)v.env);
		if (v.clack) clackPulse.trigger(1e-3f);
		outputs[CLACK_OUTPUT].setVoltage(clackPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[PASS_OUTPUT].setVoltage(v.passGate ? 10.f : 0.f);
		lights[PASS_LIGHT].setBrightnessSmooth(v.passGate ? 1.f : 0.f, args.sampleTime);
		lights[STRIKE_LIGHT].setBrightness(strikeFlash.process(args.sampleTime) ? 1.f : 0.f);

		if (++dispCtr >= (int)(rate / 60.0)) {
			dispCtr = 0;
			const longmetal::Engine& e = *v.e;
			int N = e.p.N;
			for (int b = 0; b < DISP_BINS; b++) {
				int a = b * N / DISP_BINS, z = std::max(a + 1, (b + 1) * N / DISP_BINS);
				double s = 0; for (int i = a; i < z; i++) s += e.envc[i];
				dispE[b] = (float)(s / (z - a));
			}
			double span = std::max(1, N - 1);
			for (int a = 0; a < longmetal::NAX; a++) {
				double pos = e.trainPos + e.trainSide * e.ax[a].off;
				dispAx[a] = (k.train > 0 && v.passFade > 0) ? (float)(pos / span) : -1.f;
			}
			dispPick = (float)(e.p.pick / span);
			dispSrc = (float)(v.src / span);
			dispEndA = (float)e.p.rhoL; dispEndB = (float)e.p.rhoR;
			if (e.p.grip > 0) { int s = 0; for (int i = 0; i < N; i++) s += e.stuck[i]; dispStuck = (float)s / N; }
			else dispStuck = 0.f;
		}
	}
};

// The line seen from above: energy along it as brightness, the listener, the axles, the source point, the ends.
struct LineDisplay : TransparentWidget {
	LongMetal* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		float E[DISP_BINS], ax[longmetal::NAX];
		float pick = 0.12f, src = 0.82f, endA = 0.3f, endB = 0.3f, stuck = 0.f;
		if (module) {
			for (int b = 0; b < DISP_BINS; b++) E[b] = module->dispE[b];
			for (int a = 0; a < longmetal::NAX; a++) ax[a] = module->dispAx[a];
			pick = module->dispPick; src = module->dispSrc; endA = module->dispEndA; endB = module->dispEndB; stuck = module->dispStuck;
		} else {
			for (int b = 0; b < DISP_BINS; b++) { float x = (b + 0.5f) / DISP_BINS; E[b] = 1e-3f * std::exp(-std::fabs(x - 0.82f) * 9.f) + 2e-6f; }
			const float off[8] = {0, 3, 12, 15, 29, 32, 41, 44};
			for (int a = 0; a < 8; a++) ax[a] = 0.82f + off[a] / 319.f;
		}
		float w = box.size.x, h = box.size.y, pad = mm2px(2.0f);
		float x0 = pad, x1 = w - pad, yc = h * 0.5f;
		NVGcontext* vg = args.vg;
		nvgScissor(vg, 0, 0, w, h);
		// energy along the line (log scale, 70 dB)
		float bw = (x1 - x0) / DISP_BINS;
		for (int b = 0; b < DISP_BINS; b++) {
			float db = 10.f * std::log10(E[b] + 1e-12f);
			float t = std::min(1.f, std::max(0.f, (db + 70.f) / 70.f));
			if (t <= 0.f) continue;
			nvgBeginPath(vg);
			float hh = (h - 2 * pad) * (0.25f + 0.75f * t);
			nvgRect(vg, x0 + b * bw, yc - hh / 2, bw + 0.4f, hh);
			nvgFillColor(vg, nvgRGBA(201, 163, 107, (unsigned char)(30 + 200 * t * (1.f - 0.6f * stuck))));
			nvgFill(vg);
		}
		// the rail
		nvgBeginPath(vg); nvgMoveTo(vg, x0, yc); nvgLineTo(vg, x1, yc);
		nvgStrokeColor(vg, nvgRGBA(170, 176, 182, 140)); nvgStrokeWidth(vg, 0.8f); nvgStroke(vg);
		// ends: solid when they reflect, faint when they absorb
		for (int s = 0; s < 2; s++) {
			float r = std::fabs(s ? endB : endA), x = s ? x1 : x0;
			nvgBeginPath(vg); nvgMoveTo(vg, x, pad * 0.5f); nvgLineTo(vg, x, h - pad * 0.5f);
			nvgStrokeColor(vg, nvgRGBA(230, 232, 235, (unsigned char)(40 + 200 * r))); nvgStrokeWidth(vg, 1.6f); nvgStroke(vg);
		}
		// axles
		for (int a = 0; a < longmetal::NAX; a++) {
			if (ax[a] < 0.f || ax[a] > 1.f) continue;
			float x = x0 + (x1 - x0) * ax[a];
			nvgBeginPath(vg); nvgCircle(vg, x, yc, 1.4f);
			nvgFillColor(vg, nvgRGB(236, 232, 225)); nvgFill(vg);
		}
		// source point
		if (src >= 0.f && src <= 1.f) {
			float x = x0 + (x1 - x0) * src;
			nvgBeginPath(vg); nvgMoveTo(vg, x, h - pad * 0.4f); nvgLineTo(vg, x - 2.2f, h - 0.2f); nvgLineTo(vg, x + 2.2f, h - 0.2f); nvgClosePath(vg);
			nvgFillColor(vg, nvgRGB(201, 163, 107)); nvgFill(vg);
		}
		// listener
		{
			float x = x0 + (x1 - x0) * std::min(1.f, std::max(0.f, pick));
			nvgBeginPath(vg); nvgMoveTo(vg, x, pad * 0.3f); nvgLineTo(vg, x, h - pad * 0.3f);
			nvgStrokeColor(vg, nvgRGB(143, 200, 220)); nvgStrokeWidth(vg, 1.4f); nvgStroke(vg);
		}
		nvgResetScissor(vg);
	}
};

struct LongMetalWidget : ModuleWidget {
	LongMetalWidget(LongMetal* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LongMetal.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef LongMetal M;
		LineDisplay* d = new LineDisplay;
		d->module = module;
		d->box.pos = mm2px(Vec(DISP_X, DISP_Y));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		addChild(d);

		const int r1[11] = {M::PITCH_PARAM, M::LENGTH_PARAM, M::SPACING_PARAM, M::GAUGE_PARAM, M::STIFF_PARAM, M::SUPPORTS_PARAM,
			M::ENDA_PARAM, M::ENDB_PARAM, M::GROUND_PARAM, M::BRIGHT_PARAM, M::AGE_PARAM};
		for (int i = 0; i < 11; i++) {
			if (i == 3) addParam(createParamCentered<CKSS>(mm2px(Vec(col11(i), Y1)), module, r1[i]));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col11(i), Y1)), module, r1[i]));
		}

		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(ROW2_X[0], Y2)), module, M::LIVE_PARAM));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(ROW2_X[1], Y2)), module, M::DISTANCE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[2], Y2)), module, M::LISTEN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[3], Y2)), module, M::PASSTIME_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(ROW2_X[4], Y2)), module, M::PASS_PARAM, M::PASS_LIGHT));
		const int r2[4] = {M::WARP_PARAM, M::SWELL_PARAM, M::LOOSE_PARAM, M::GRIP_PARAM};
		for (int i = 0; i < 4; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[5 + i], Y2)), module, r2[i]));

		const int r3[9] = {M::TRAIN_PARAM, M::SPEED_PARAM, M::FLATS_PARAM, M::STRIKE_PARAM, M::HARD_PARAM, M::INLEVEL_PARAM,
			M::EVEN_PARAM, M::WIDTH_PARAM, M::LEVEL_PARAM};
		for (int i = 0; i < 9; i++) {
			if (i == 3) addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(ROW3_X[i], Y3)), module, r3[i], M::STRIKE_LIGHT));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW3_X[i], Y3)), module, r3[i]));
		}

		const int r4[10] = {M::VOCT_INPUT, M::STIFF_INPUT, M::SUPPORTS_INPUT, M::LIVE_INPUT, M::GROUND_INPUT, M::LISTEN_INPUT,
			M::DISTANCE_INPUT, M::PASS_INPUT, M::TRAIN_INPUT, M::SPEED_INPUT};
		for (int i = 0; i < 10; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col10(i), Y4)), module, r4[i]));
		const int r5[7] = {M::STRIKE_INPUT, M::VEL_INPUT, M::WARP_INPUT, M::SWELL_INPUT, M::LOOSE_INPUT, M::GRIP_INPUT, M::IN_INPUT};
		for (int i = 0; i < 7; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col12(i), Y5)), module, r5[i]));
		const int o5[5] = {M::ENV_OUTPUT, M::CLACK_OUTPUT, M::PASS_OUTPUT, M::L_OUTPUT, M::R_OUTPUT};
		for (int i = 0; i < 5; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col12(7 + i), Y5)), module, o5[i]));
	}
};

Model* modelLongMetal = createModel<LongMetal, LongMetalWidget>("LongMetal");
