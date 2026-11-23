#include "plugin.hpp"
#include "dsp/Voice.hpp"

using namespace cicada;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 14.0f, DISP_W = 52.0f, DISP_H = 9.0f;
static const float PLIGHT_X = 61.5f, PLIGHT_Y = 18.5f;
static const float R1_Y = 40.0f, MECH_X = 11.0f;
static const float R1_X[3] = {36.0f, 62.0f, 88.0f};
static const float R2_Y = 62.0f;
static const float R2_X[5] = {12.0f, 31.4f, 50.8f, 70.2f, 89.6f};
static const float R3_Y = 84.0f;
static const float R3_X[5] = {12.0f, 30.0f, 46.0f, 62.0f, 78.0f};
static const float VSW_X = 94.0f;
static const float JX0 = 8.6f, JDX = 12.06f;
static const float JA_Y = 101.5f, JB_Y = 115.0f;

static const int MAX_VOICES = 16;

struct Cicada : Module {
	enum ParamIds {
		MECH_PARAM, PITCH_PARAM, PURE_PARAM, STROKE_PARAM, BODY_PARAM, OPEN_PARAM, TEMP_PARAM, LOOSE_PARAM,
		DISTANCE_PARAM, RATE_PARAM, CHIRP_PARAM, GAP_PARAM, SONG_PARAM, LENGTH_PARAM, VMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT, PURE_INPUT, STROKE_INPUT, OPEN_INPUT, RATE_INPUT, CHIRP_INPUT, TEMP_INPUT, VOCT_INPUT,
		GATE_INPUT, TRIG_INPUT, CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { PULSE_OUTPUT, CHIRP_OUTPUT, ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { PULSE_LIGHT, NUM_LIGHTS };

	Voice voices[MAX_VOICES];
	Distance distance;
	dsp::PulseGenerator pulseGen[MAX_VOICES];
	double env[MAX_VOICES] = {};
	int channels = 0;
	int individuals = 1;               // used when no poly cable sets the count
	uint32_t seed = 1;
	bool prevGateP = false, prevTrigP = false;
	dsp::ClockDivider lightDiv;
	float pulseLight = 0.f;
	double envAtt = 0.0, envRel = 0.0;

	// for the display
	int dispSong = 0, dispMech = FILE_MECH;
	float dispTemp = 22.5f;
	bool dispResting = false, dispHold = false, dispTriggered = false;

	Cicada() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configSwitch(MECH_PARAM, 0.f, 2.f, 1.f, "Mechanism",
			{"WING: one impulse per wingbeat (flies, mosquitoes, bees)", "FILE: a scraper crossing a file of teeth (crickets, katydids, grasshoppers)", "TYMBAL: ribs buckling in a row, two alternating sides (cicadas)"});
		configParam(PITCH_PARAM, 0.f, 1.f, 0.752f, "Pitch (carrier)", " Hz", 160.f, 100.f)->description =
			"Frequency of the ringing body part: the tymbal plate or the wing. In WING it is the body's resonance.";

		struct PureQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = getValue();
				return string::f("Q %.1f, teeth %d%% locked", mapQ(k), (int)std::round(mapLock(k) * 100));
			}
		};
		configParam<PureQuantity>(PURE_PARAM, 0.f, 1.f, 0.885f, "Pure")->description =
			"From rasp to whistle. Raises the resonance and locks the teeth to it: a cricket at the top, a katydid low down.";

		struct StrokeQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = getValue();
				int mech = module ? 2 - (int)std::round(module->params[MECH_PARAM].getValue()) : FILE_MECH;
				if (mech == TYMBAL) return string::f("%d ribs", 1 + (int)(k * 11 + 0.5));
				if (mech == FILE_MECH) return string::f("pulse %d%% of the cycle", (int)std::round((0.05 + 0.9 * k) * 100));
				return string::f("upstroke %d%%", (int)std::round(k * 100));
			}
		};
		configParam<StrokeQuantity>(STROKE_PARAM, 0.f, 1.f, 0.589f, "Stroke")->description =
			"What one muscle twitch does. TYMBAL: how many ribs buckle. FILE: how long the wing scrapes. WING: how strong the upstroke is.";
		configParam(BODY_PARAM, 0.f, 1.f, 0.f, "Body", "%", 0.f, 100.f)->description =
			"How much the abdomen resonator colours the sound. Turn up for the cicada 'wah' that OPEN plays.";
		configParam(OPEN_PARAM, 0.f, 1.f, 0.5f, "Open (body tuning)", " x carrier", 4.f, 0.5f)->description =
			"Where the abdomen is tuned, half to double the carrier. The centre is tuned to the carrier and loudest. Several songs move it by themselves.";

		struct TempQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = getValue();
				return string::f("%.1f °C (speed x%.2f)", mapTempC(k), mapTemp(k));
			}
		};
		configParam<TempQuantity>(TEMP_PARAM, 0.f, 1.f, 0.5f, "Temperature")->description =
			"Night temperature. Warmer insects sing faster and a little higher, all rates together.";
		configParam(LOOSE_PARAM, 0.f, 1.f, 0.4f, "Loose", "%", 0.f, 100.f)->description =
			"From machine-exact to a live animal: timing and loudness wobble, dropped pulses, the odd pause.";
		configParam(DISTANCE_PARAM, 0.f, 1.f, 0.15f, "Distance", "%", 0.f, 100.f)->description =
			"Air and leaves between you and the insect: treble loss and a scatter of short reflections.";

		struct RateQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double k = getValue();
				bool track = module && module->params[VMODE_PARAM].getValue() < 0.5f;
				if (track) return string::f("%+.1f semitones", (k - 0.5) * 96.0);
				return string::f("%.1f pulses/s", mapRate(k));
			}
		};
		configParam<RateQuantity>(RATE_PARAM, 0.f, 1.f, 0.468f, "Rate")->description =
			"Muscle rate: pulses per second. With the V/OCT switch at RATE it transposes the played pitch by up to four octaves.";

		struct ChirpQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				int n = mapChirp(getValue());
				return n == 0 ? "Trill (continuous)" : string::f("%d pulse%s per chirp", n, n == 1 ? "" : "s");
			}
		};
		configParam<ChirpQuantity>(CHIRP_PARAM, 0.f, 1.f, 0.441f, "Chirp")->description =
			"Pulses per chirp, 1 to 32. Fully up the insect trills without stopping.";
		configParam(GAP_PARAM, 0.f, 1.f, 0.409f, "Gap", " s", 200.f, 0.02f)->description =
			"Silence between chirps (at 22.5 °C; warmer is shorter).";
		configSwitch(SONG_PARAM, 0.f, (float)(NUM_SONGS - 1), 0.f, "Song",
			{"STEADY", "SWELL: rises, then dies off lower and slower", "PULSATE: swell with a slow throb", "RISE: phrases that grow",
			 "SLUR: long tone that falls at the end", "TICK-BUZZ: ticks, then a buzz", "SINK: chirps speed up, sink and fade", "TALK: the abdomen makes syllables"});

		struct LengthQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				double s = mapLength(getValue());
				return s < 0 ? "Hold (never ends)" : string::f("%.2f s", s);
			}
		};
		configParam<LengthQuantity>(LENGTH_PARAM, 0.f, 1.f, 1.f, "Length")->description =
			"How long one song phrase lasts before it releases and rests. Fully up it holds forever. A patched GATE overrides it.";
		configSwitch(VMODE_PARAM, 0.f, 1.f, 1.f, "V/OCT goes to",
			{"RATE: V/OCT plays the pulse rate (formant voice, 0 V = C4)", "PITCH: V/OCT transposes the carrier"});

		configInput(PITCH_INPUT, "Pitch CV (5 V = full turn)");
		configInput(PURE_INPUT, "Pure CV");
		configInput(STROKE_INPUT, "Stroke CV");
		configInput(OPEN_INPUT, "Open CV (the 'wah')");
		configInput(RATE_INPUT, "Rate CV");
		configInput(CHIRP_INPUT, "Chirp CV");
		configInput(TEMP_INPUT, "Temperature CV");
		configInput(VOCT_INPUT, "V/OCT (polyphonic: one insect per channel)");
		configInput(GATE_INPUT, "Gate: sings while high, releases when low");
		configInput(TRIG_INPUT, "Trigger: one chirp per trigger");
		configInput(CLOCK_INPUT, "Clock: each edge starts a chirp");
		configOutput(PULSE_OUTPUT, "Pulse trigger (one per muscle twitch)");
		configOutput(CHIRP_OUTPUT, "Chirp gate");
		configOutput(ENV_OUTPUT, "Envelope (loudness of the song)");
		configOutput(L_OUTPUT, "Left (mono sum when R is not patched)");
		configOutput(R_OUTPUT, "Right");

		lightDiv.setDivision(64);
		seed = random::u32();
		setSampleRate(48000.f);
	}

	void setSampleRate(float sr) {
		for (int v = 0; v < MAX_VOICES; v++) voices[v].setSampleRate(sr);
		distance.setSampleRate(sr);
		envAtt = 1.0 - std::exp(-1.0 / (0.002 * sr));
		envRel = 1.0 - std::exp(-1.0 / (0.060 * sr));
		channels = 0;                      // re-initialise the voices on the next sample
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		individuals = 1;
		distance.reset();
		channels = 0;
	}
	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		channels = 0;
	}

	float knob(int p, int in, int c) {
		float v = params[p].getValue();
		if (in >= 0 && inputs[in].isConnected()) v += inputs[in].getPolyVoltage(c) * 0.2f;
		return clamp(v, 0.f, 1.f);
	}

	void process(const ProcessArgs& args) override {
		bool gateP = inputs[GATE_INPUT].isConnected(), trigP = inputs[TRIG_INPUT].isConnected(), clockP = inputs[CLOCK_INPUT].isConnected();
		int n = std::max({inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels(),
		                  inputs[TRIG_INPUT].getChannels(), inputs[CLOCK_INPUT].getChannels()});
		if (n <= 1) n = individuals;
		n = clamp(n, 1, MAX_VOICES);
		bool freeRun = !gateP && !trigP;
		if (n != channels) {
			for (int v = 0; v < n; v++) {
				Individual ind = individual(v, n);
				if (!freeRun) ind.delay = 0.0;
				if (v >= channels) voices[v].init(seed + 17u * v, ind, gateP);
				else { ind.delay = 0.0; voices[v].ind = ind; }
			}
			channels = n;
		}

		Params base;
		base.mech = 2 - (int)std::round(params[MECH_PARAM].getValue());
		base.body = params[BODY_PARAM].getValue();
		base.loose = params[LOOSE_PARAM].getValue();
		base.gap = params[GAP_PARAM].getValue();
		base.song = (int)std::round(params[SONG_PARAM].getValue());
		base.length = params[LENGTH_PARAM].getValue();
		base.track = params[VMODE_PARAM].getValue() < 0.5f;
		distance.setDistance(params[DISTANCE_PARAM].getValue());

		double L = 0.0, R = 0.0;
		float maxPulse = 0.f;
		for (int c = 0; c < n; c++) {
			Params p = base;
			p.pitch = knob(PITCH_PARAM, PITCH_INPUT, c);
			p.pure = knob(PURE_PARAM, PURE_INPUT, c);
			p.stroke = knob(STROKE_PARAM, STROKE_INPUT, c);
			p.open = knob(OPEN_PARAM, OPEN_INPUT, c);
			p.rate = knob(RATE_PARAM, RATE_INPUT, c);
			p.chirp = knob(CHIRP_PARAM, CHIRP_INPUT, c);
			p.temp = knob(TEMP_PARAM, TEMP_INPUT, c);
			p.voct = inputs[VOCT_INPUT].getPolyVoltage(c);
			double l, r;
			voices[c].process(p, inputs[GATE_INPUT].getPolyVoltage(c), inputs[TRIG_INPUT].getPolyVoltage(c),
			                  inputs[CLOCK_INPUT].getPolyVoltage(c), gateP, trigP, clockP, l, r);
			L += l; R += r;
			if (voices[c].pulseOut) {
				pulseGen[c].trigger(1e-3f);
				maxPulse = std::max(maxPulse, (float)std::min(1.0, voices[c].pulseLevel));
			}
			double m = std::fabs((l + r) * 0.70710678 * OUT_GAIN);
			env[c] += (m - env[c]) * (m > env[c] ? envAtt : envRel);
			outputs[PULSE_OUTPUT].setVoltage(pulseGen[c].process(args.sampleTime) ? 10.f : 0.f, c);
			outputs[CHIRP_OUTPUT].setVoltage(voices[c].chirpOut ? 10.f : 0.f, c);
			outputs[ENV_OUTPUT].setVoltage(clamp((float)(env[c] * 1.6), 0.f, 10.f), c);
		}
		outputs[PULSE_OUTPUT].setChannels(n);
		outputs[CHIRP_OUTPUT].setChannels(n);
		outputs[ENV_OUTPUT].setChannels(n);

		double s = 1.0 / std::sqrt((double)n);
		L *= s; R *= s;
		distance.process(L, R);
		float outL = (float)softClip(L * OUT_GAIN), outR = (float)softClip(R * OUT_GAIN);
		if (outputs[R_OUTPUT].isConnected()) {
			outputs[L_OUTPUT].setVoltage(outL);
			outputs[R_OUTPUT].setVoltage(outR);
		}
		else {
			outputs[L_OUTPUT].setVoltage((outL + outR) * 0.70710678f);
			outputs[R_OUTPUT].setVoltage(0.f);
		}

		pulseLight = std::max(pulseLight, maxPulse);
		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[PULSE_LIGHT].setBrightness(pulseLight);
			pulseLight = std::max(0.f, pulseLight - dt / 0.02f);
			dispSong = base.song; dispMech = base.mech;
			dispTemp = (float)mapTempC(knob(TEMP_PARAM, TEMP_INPUT, 0));
			dispResting = voices[0].sstate == Voice::REST && !trigP;
			dispHold = mapLength(base.length) < 0 && !gateP;
			dispTriggered = trigP;
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "individuals", json_integer(individuals));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* i = json_object_get(root, "individuals");
		if (i) individuals = clamp((int)json_integer_value(i), 1, 8);
		channels = 0;
	}
};

struct CicadaDisplay : Widget {
	Cicada* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		int song = module ? module->dispSong : STEADY;
		int mech = module ? module->dispMech : FILE_MECH;
		float temp = module ? module->dispTemp : 22.5f;
		bool resting = module ? module->dispResting : false;
		std::string name = (module && module->dispTriggered) ? "TRIGGERED" : SONG_NAMES[clamp(song, 0, NUM_SONGS - 1)];
		std::string sub = string::f("%s %.1fC", MECH_NAMES[clamp(mech, 0, 2)], temp);
		NVGcolor amber = nvgRGB(242, 179, 61), ink = nvgRGB(239, 233, 210);
		if (resting) amber = nvgRGBA(242, 179, 61, 110);
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(5.6f));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, amber);
			nvgText(args.vg, mm2px(1.8f), box.size.y / 2, name.c_str(), NULL);
			nvgFontSize(args.vg, mm2px(2.6f));
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, ink);
			nvgText(args.vg, box.size.x - mm2px(1.6f), box.size.y / 2, sub.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct CicadaWidget : ModuleWidget {
	CicadaWidget(Cicada* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Cicada.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		CicadaDisplay* d = createWidget<CicadaDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(PLIGHT_X, PLIGHT_Y)), module, Cicada::PULSE_LIGHT));

		addParam(createParamCentered<CKSSThree>(mm2px(Vec(MECH_X, R1_Y)), module, Cicada::MECH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(R1_X[0], R1_Y)), module, Cicada::PITCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R1_X[1], R1_Y)), module, Cicada::PURE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R1_X[2], R1_Y)), module, Cicada::STROKE_PARAM));

		const int r2[5] = {Cicada::BODY_PARAM, Cicada::OPEN_PARAM, Cicada::TEMP_PARAM, Cicada::LOOSE_PARAM, Cicada::DISTANCE_PARAM};
		for (int i = 0; i < 5; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R2_X[i], R2_Y)), module, r2[i]));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(R3_X[0], R3_Y)), module, Cicada::RATE_PARAM));
		const int r3[4] = {Cicada::CHIRP_PARAM, Cicada::GAP_PARAM, Cicada::SONG_PARAM, Cicada::LENGTH_PARAM};
		for (int i = 0; i < 4; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(R3_X[i + 1], R3_Y)), module, r3[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(VSW_X, R3_Y)), module, Cicada::VMODE_PARAM));

		const int ja[8] = {Cicada::PITCH_INPUT, Cicada::PURE_INPUT, Cicada::STROKE_INPUT, Cicada::OPEN_INPUT,
		                   Cicada::RATE_INPUT, Cicada::CHIRP_INPUT, Cicada::TEMP_INPUT, Cicada::VOCT_INPUT};
		for (int i = 0; i < 8; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JA_Y)), module, ja[i]));
		const int jbIn[3] = {Cicada::GATE_INPUT, Cicada::TRIG_INPUT, Cicada::CLOCK_INPUT};
		for (int i = 0; i < 3; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JB_Y)), module, jbIn[i]));
		const int jbOut[5] = {Cicada::PULSE_OUTPUT, Cicada::CHIRP_OUTPUT, Cicada::ENV_OUTPUT, Cicada::L_OUTPUT, Cicada::R_OUTPUT};
		for (int i = 0; i < 5; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * (3 + i), JB_Y)), module, jbOut[i]));
	}

	void appendContextMenu(Menu* menu) override {
		Cicada* m = dynamic_cast<Cicada*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		static const int counts[6] = {1, 2, 3, 4, 6, 8};
		menu->addChild(createSubmenuItem("Individuals (without a poly cable)", string::f("%d", m->individuals), [=](Menu* sub) {
			for (int c : counts) {
				sub->addChild(createCheckMenuItem(string::f("%d", c), "",
					[=]() { return m->individuals == c; },
					[=]() { m->individuals = c; m->channels = 0; }));
			}
		}));
	}
};

Model* modelCicada = createModel<Cicada, CicadaWidget>("Cicada");
