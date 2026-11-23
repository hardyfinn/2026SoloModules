// Chirp: a pitch-swept voice. One trigger plays a train of chirps; each chirp
// glides from PITCH + SWEEP octaves to PITCH and fades. Kicks, toms, zaps,
// bats, frogs, crickets, birds, and (with LOOP) cicadas and slow modulation.
// Polyphonic: one voice per TRIG / V/OCT channel, up to 16.
#include "plugin.hpp"
#include "ChirpEngine.hpp"

using namespace chirp;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COLS[4] = {11.0f, 30.8f, 50.5f, 70.3f};
static const float ROW_Y[2] = {30.0f, 51.0f};
static const float TRIG_Y = 67.5f;
static const float LIGHT_X = 17.6f, LIGHT_Y = 63.0f;
static const float SCOPE_X = 22.0f, SCOPE_Y = 60.6f, SCOPE_W = 37.0f, SCOPE_H = 12.4f;
static const float JY[3] = {85.0f, 99.0f, 113.0f};

struct ChirpModule : Module {
	enum ParamId { PITCH_P, SWEEP_P, TIME_P, CURVE_P, DECAY_P, TONE_P, REPEAT_P, RATE_P, TRIG_P, LOOP_P, NUM_PARAMS };
	enum InputId { VOCT_I, SWEEP_I, TIME_I, CURVE_I, DECAY_I, TONE_I, REPEAT_I, RATE_I, TRIG_I, NUM_INPUTS };
	enum OutputId { ENV_O, PITCH_O, OUT_O, NUM_OUTPUTS };
	enum LightId { TRIG_L, NUM_LIGHTS };

	Voice voices[16];
	dsp::SchmittTrigger trig[16];
	dsp::SchmittTrigger button;
	dsp::ClockDivider lightDiv;

	// for the screen (written on the audio thread, read on the UI thread)
	Settings shown;
	float shownT = 1e9f;
	bool shownActive = false;

	ChirpModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		struct PitchQ : ParamQuantity {
			std::string getDisplayValueString() override {
				float v = mapPitch(getValue());
				return string::f("%.1f Hz (%+.2f V)", 261.6256f * std::pow(2.f, v), v);
			}
		};
		configParam<PitchQ>(PITCH_P, 0.f, 1.f, knobPitch(-2.4f), "Pitch")->description =
			"Where the chirp lands. Adds to V/OCT. A kick lands low; a bird lands high.";
		struct SweepQ : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%+.2f octaves", mapSweep(getValue())); }
		};
		configParam<SweepQ>(SWEEP_P, 0.f, 1.f, knobSweep(3.2f), "Sweep")->description =
			"Where the chirp starts, in octaves from PITCH. Right of centre starts high and falls (kicks, zaps, bats). "
			"Left of centre starts low and rises (frogs, rising bird calls). Centre: no sweep, a plain blip.";
		struct TimeQ : ParamQuantity {
			std::string getDisplayValueString() override {
				float s = mapTime(getValue());
				return s < 1.f ? string::f("%.1f ms", s * 1000.f) : string::f("%.2f s", s);
			}
		};
		configParam<TimeQ>(TIME_P, 0.f, 1.f, knobTime(0.06f), "Sweep time")->description =
			"How long the glide takes, 0.5 ms to 2 s.";
		configParam(CURVE_P, 0.f, 1.f, knobCurve(-0.7f), "Curve", "%", 0.f, 200.f, -100.f)->description =
			"Shape of the glide. Left: moves fast at first then settles (the classic kick drop). "
			"Centre: even in octaves. Right: holds, then dives at the end.";
		struct DecayQ : ParamQuantity {
			std::string getDisplayValueString() override {
				float s = mapDecay(getValue());
				return s < 1.f ? string::f("%.0f ms", s * 1000.f) : string::f("%.2f s", s);
			}
		};
		configParam<DecayQ>(DECAY_P, 0.f, 1.f, knobDecay(0.45f), "Decay")->description =
			"How long each chirp takes to fade away, 2 ms to 4 s.";
		configParam(TONE_P, 0.f, 1.f, 0.05f, "Tone", "%", 0.f, 100.f)->description =
			"0 is a pure sine. Up to halfway it grows bright like a saw. Past halfway it is driven into a buzzy square: frogs, insects, rims.";
		struct RepeatQ : ParamQuantity {
			std::string getDisplayValueString() override {
				int n = mapRepeat(getValue());
				return n == 1 ? "1 chirp" : string::f("%d chirps", n);
			}
		};
		configParam<RepeatQ>(REPEAT_P, 0.f, 1.f, 0.f, "Repeat")->description =
			"Chirps per trigger, 1 to 16. Crickets, bats and frogs call in short trains.";
		struct RateQ : ParamQuantity {
			std::string getDisplayValueString() override {
				float hz = mapRate(getValue());
				return string::f("%.2f per second", hz);
			}
		};
		configParam<RateQ>(RATE_P, 0.f, 1.f, knobRate(10.f), "Rate")->description =
			"Chirps per second inside a train (and in LOOP), 0.5 to 200.";
		configButton(TRIG_P, "Trigger");
		configSwitch(LOOP_P, 0.f, 1.f, 0.f, "Loop", {"Off: chirp on triggers", "On: keep chirping at RATE"});

		configInput(VOCT_I, "V/OCT (adds to PITCH)");
		configInput(SWEEP_I, "Sweep CV (1 V = 1 octave)");
		configInput(TIME_I, "Sweep time CV (+/-5 V = full knob)");
		configInput(CURVE_I, "Curve CV (+/-5 V = full knob)");
		configInput(DECAY_I, "Decay CV (+/-5 V = full knob)");
		configInput(TONE_I, "Tone CV (+/-5 V = full knob)");
		configInput(REPEAT_I, "Repeat CV (+/-5 V = full knob)");
		configInput(RATE_I, "Rate CV (+/-5 V = full knob)");
		configInput(TRIG_I, "Trigger");
		configOutput(ENV_O, "Envelope (0-10 V)");
		configOutput(PITCH_O, "Pitch contour (V/OCT, follows the sweep: patch to any oscillator)");
		configOutput(OUT_O, "Audio");
		lightDiv.setDivision(64);
	}

	void onReset() override {
		for (int c = 0; c < 16; c++) voices[c] = Voice();
	}

	static float k(float knob, float cv) { return clamp(knob + cv * 0.1f, 0.f, 1.f); }

	void process(const ProcessArgs& args) override {
		int chans = std::max(1, std::max(inputs[TRIG_I].getChannels(), inputs[VOCT_I].getChannels()));
		bool btn = button.process(params[TRIG_P].getValue());
		bool loop = params[LOOP_P].getValue() > 0.5f;
		float kn[8];
		for (int i = 0; i < 8; i++) kn[i] = params[i].getValue();

		for (int c = 0; c < chans; c++) {
			Settings s;
			s.pitch = mapPitch(kn[PITCH_P]) + inputs[VOCT_I].getPolyVoltage(c);
			s.sweep = clamp(mapSweep(kn[SWEEP_P]) + inputs[SWEEP_I].getPolyVoltage(c), -10.f, 10.f);
			s.time = mapTime(k(kn[TIME_P], inputs[TIME_I].getPolyVoltage(c)));
			s.curve = mapCurve(k(kn[CURVE_P], inputs[CURVE_I].getPolyVoltage(c)));
			s.decay = mapDecay(k(kn[DECAY_P], inputs[DECAY_I].getPolyVoltage(c)));
			s.tone = k(kn[TONE_P], inputs[TONE_I].getPolyVoltage(c));
			s.repeat = mapRepeat(k(kn[REPEAT_P], inputs[REPEAT_I].getPolyVoltage(c)));
			s.rate = mapRate(k(kn[RATE_P], inputs[RATE_I].getPolyVoltage(c)));
			s.loop = loop;

			bool t = trig[c].process(inputs[TRIG_I].getPolyVoltage(c), 0.1f, 1.f);
			if (t || btn) voices[c].trigger(s);
			float o, e, p;
			voices[c].process(s, args.sampleRate, o, e, p);
			outputs[OUT_O].setVoltage(5.f * o, c);
			outputs[ENV_O].setVoltage(10.f * e, c);
			outputs[PITCH_O].setVoltage(p, c);
			if (c == 0) { shown = s; shownT = voices[0].t; shownActive = voices[0].active || e > 0.001f; }
		}
		outputs[OUT_O].setChannels(chans);
		outputs[ENV_O].setChannels(chans);
		outputs[PITCH_O].setChannels(chans);

		if (lightDiv.process()) {
			float e = 0.f;
			for (int c = 0; c < chans; c++) e = std::max(e, voices[c].env);
			lights[TRIG_L].setBrightness(e);
		}
	}
};

// Screen: the shape of one chirp. Bright line = pitch glide, dim fill = loudness,
// dot = where channel 1 is now.
struct ChirpScope : Widget {
	ChirpModule* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		Settings s;
		float tNow = 1e9f;
		bool act = false;
		if (module) { s = module->shown; tNow = module->shownT; act = module->shownActive; }
		else { s.sweep = 3.2f; s.time = 0.06f; s.curve = -0.7f; s.decay = 0.45f; }
		const float w = box.size.x, h = box.size.y, pad = 2.f;
		float win = std::max(s.time * 1.25f, std::min(s.decay * 0.7f, 4.f));
		if (s.repeat > 1 || s.loop) win = std::max(win, std::min(4.f, 1.f / s.rate * 1.02f));
		win = std::max(win, 0.002f);
		const int N = 80;
		float kk = std::pow(2.f, s.curve * 3.f);
		float lo = std::min(0.f, s.sweep), hi = std::max(0.f, s.sweep);
		if (hi - lo < 0.5f) { float m = 0.5f * (hi + lo); lo = m - 0.25f; hi = m + 0.25f; }

		// loudness
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, pad, h - pad);
		for (int i = 0; i <= N; i++) {
			float t = win * i / N;
			float e = std::exp(-6.9f * t / s.decay);
			nvgLineTo(args.vg, pad + (w - 2 * pad) * i / N, h - pad - (h - 2 * pad) * e);
		}
		nvgLineTo(args.vg, w - pad, h - pad);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGBA(120, 200, 170, 50));
		nvgFill(args.vg);

		// pitch glide
		nvgBeginPath(args.vg);
		for (int i = 0; i <= N; i++) {
			float t = win * i / N;
			float p = std::min(1.f, t / s.time);
			float v = s.sweep * (1.f - std::pow(p, kk));
			float x = pad + (w - 2 * pad) * i / N;
			float y = h - pad - (h - 2 * pad) * (v - lo) / (hi - lo);
			if (i == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);
		}
		nvgStrokeColor(args.vg, nvgRGB(140, 235, 195));
		nvgStrokeWidth(args.vg, 1.3f);
		nvgStroke(args.vg);

		if (act && tNow < win) {
			float p = std::min(1.f, tNow / s.time);
			float v = s.sweep * (1.f - std::pow(p, kk));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, pad + (w - 2 * pad) * tNow / win, h - pad - (h - 2 * pad) * (v - lo) / (hi - lo), 2.f);
			nvgFillColor(args.vg, nvgRGB(255, 240, 200));
			nvgFill(args.vg);
		}
		Widget::drawLayer(args, layer);
	}
};

struct ChirpWidget : ModuleWidget {
	ChirpWidget(ChirpModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Chirp.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int r = 0; r < 2; r++)
			for (int i = 0; i < 4; i++)
				addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLS[i], ROW_Y[r])), module, r * 4 + i));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(COLS[0], TRIG_Y)), module, ChirpModule::TRIG_P));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(LIGHT_X, LIGHT_Y)), module, ChirpModule::TRIG_L));
		addParam(createParamCentered<CKSS>(mm2px(Vec(COLS[3], TRIG_Y)), module, ChirpModule::LOOP_P));

		ChirpScope* sc = createWidget<ChirpScope>(mm2px(Vec(SCOPE_X, SCOPE_Y)));
		sc->box.size = mm2px(Vec(SCOPE_W, SCOPE_H));
		sc->module = module;
		addChild(sc);

		for (int r = 0; r < 2; r++)
			for (int i = 0; i < 4; i++)
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[i], JY[r])), module, r * 4 + i));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[0], JY[2])), module, ChirpModule::TRIG_I));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[1], JY[2])), module, ChirpModule::ENV_O));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[2], JY[2])), module, ChirpModule::PITCH_O));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[3], JY[2])), module, ChirpModule::OUT_O));
	}
};

Model* modelChirp = createModel<ChirpModule, ChirpWidget>("Chirp");
