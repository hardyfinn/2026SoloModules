#include "plugin.hpp"
#include "dsp/Host.hpp"
#include "Progressions.hpp"

using namespace comper;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 4.0f, DISP_Y = 13.0f, DISP_W = 118.0f, DISP_H = 16.0f;
static const float SMALLK_X[3] = {134.0f, 153.0f, 172.0f};
static const float SMALLK_Y = 23.0f;
static const float STEP_X0 = 17.5f, STEP_DX = 10.25f;
static const float Y_STEPLIGHT = 34.8f, Y_ROOT = 40.6f, Y_QUAL = 48.4f, Y_STEPNAME = 54.6f;
static const float COL_X0 = 9.0f, COL_DX = 11.06f;
static const float Y_ROWA = 73.0f, Y_ROWB = 89.0f, Y_IN1 = 104.6f, Y_IN2 = 117.6f;
static const int OUT_COL0 = 11;
static float colX(int i) { return COL_X0 + i * COL_DX; }
static float stepX(int i) { return STEP_X0 + i * STEP_DX; }

static const int BEATS_CHOICES[4] = {1, 2, 4, 8};
static const int PPB_CHOICES[4] = {1, 2, 4, 24};

struct Comper : Module {
	enum ParamIds {
		STEPS_PARAM, BEATS_PARAM, KEY_PARAM,
		STYLE_PARAM, SPREAD_PARAM, REGISTER_PARAM, VOICES_PARAM, LEAD_PARAM, OUT_PARAM, ROOTS_PARAM,
		GROOVE_PARAM, DENSITY_PARAM, SWING_PARAM, REPEAT_PARAM, PUSH_PARAM, SEED_PARAM, NEW_PARAM,
		LOOSE_PARAM, TIMING_PARAM, ROLL_PARAM, WEIGHT_PARAM, SLOP_PARAM, PEDAL_PARAM,
		LISTEN_PARAM, TEMPO_PARAM, RUN_PARAM,
		ENUMS(ROOT_PARAM, 16), ENUMS(QUAL_PARAM, 16),
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT, RESET_INPUT, RUN_INPUT, KEY_INPUT, ROOT_INPUT, QUAL_INPUT, CHORD_VOCT_INPUT, CHORD_GATE_INPUT,
		SOLO_INPUT, SOLO_VOCT_INPUT, DENSITY_INPUT, OUT_INPUT, LOOSE_INPUT, LISTEN_INPUT, REGISTER_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { VOCT_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, PEDAL_OUTPUT, SCALE_OUTPUT, BASS_VOCT_OUTPUT, BASS_GATE_OUTPUT, CHANGE_OUTPUT, EOC_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(STEP_LIGHT, 16), HIT_LIGHT, SOLO_LIGHT, RUN_LIGHT, NUM_LIGHTS };

	Host host;
	HostIn in;
	HostOut out;
	int ppbIndex = 0;
	int channels = 12;
	bool bassEveryBar = false;
	bool resetReplays = true;
	dsp::BooleanTrigger newTrig;
	dsp::ClockDivider lightDiv;
	int pendingSeed = -1;

	Comper() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STEPS_PARAM, 1.f, 16.f, 4.f, "Chart steps")->snapEnabled = true;
		configSwitch(BEATS_PARAM, 0.f, 3.f, 2.f, "Beats per step", {"1", "2", "4", "8"});
		configParam(KEY_PARAM, -6.f, 5.f, 0.f, "Key (transposes the chart and CV chords)", " semitones")->snapEnabled = true;
		configSwitch(STYLE_PARAM, 0.f, 4.f, 1.f, "Voicing style", {"Shell (3rd and 7th, plus one)", "Rootless (four notes, no root)", "Quartal (stacked fourths)", "Spread (wide, five notes)", "Mixed (changes every two bars)"});
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.25f, "Spread (close to open, drop-2)", "%", 0.f, 100.f);
		struct RegQ : ParamQuantity {
			std::string getDisplayValueString() override { return noteName((int)std::round(getValue())); }
		};
		configParam<RegQ>(REGISTER_PARAM, 60.f, 79.f, 69.f, "Register (where the top note sits)")->description = "REGISTER CV: 1 V per octave around the knob.";
		configParam(VOICES_PARAM, 2.f, 5.f, 5.f, "Most notes in a chord")->snapEnabled = true;
		configParam(LEAD_PARAM, 0.f, 1.f, 1.f, "Voice leading", "%", 0.f, 100.f)->description = "How hard each voice tries to move as little as possible to the next chord.";
		configParam(OUT_PARAM, 0.f, 1.f, 0.f, "Out (altered dominants, tritone substitutes, side-slips)", "%", 0.f, 100.f);
		configSwitch(ROOTS_PARAM, 0.f, 2.f, 0.f, "Roots in the left hand", {"Off (the bass has them)", "Some", "All (solo piano)"});
		configSwitch(GROOVE_PARAM, 0.f, 4.f, 0.f, "Groove", {"Swing", "Bossa", "Ballad", "Soul (sixteenths)", "House (sixteenths)"});
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density", "%", 0.f, 100.f);
		struct SwingQ : ParamQuantity {
			std::string getDisplayValueString() override {
				Comper* m = (Comper*)module;
				double bpm = m ? 60.0 / m->host.spb : 120.0;
				double v = getValue();
				int g = m ? (int)m->params[GROOVE_PARAM].getValue() : 0;
				if (g >= 3) return string::f("%+.0f%% (sixteenth push %.2f)", v * 100.0, std::max(0.5, std::min(0.67, (g == 3 ? 0.54 : 0.5) + 0.12 * v)));
				double base = (g == 0 || g == 2) ? autoSwing(bpm) : 1.0;
				double r = std::max(1.0, std::min(3.0, base * std::pow(2.0, v * 0.6)));
				return string::f("%s%+.0f%% (long:short %.2f)", std::fabs(v) < 0.01 ? "auto " : "", v * 100.0, r);
			}
		};
		configParam<SwingQ>(SWING_PARAM, -1.f, 1.f, 0.f, "Swing (0 = follows tempo)");
		configParam(REPEAT_PARAM, 0.f, 1.f, 0.6f, "Repeat (keep the two-bar figure)", "%", 0.f, 100.f);
		configParam(PUSH_PARAM, 0.f, 1.f, 0.85f, "Push (upbeat hits take the coming chord)", "%", 0.f, 100.f);
		configParam(SEED_PARAM, 0.f, 999.f, 1.f, "Seed (a new take at the next bar line)")->snapEnabled = true;
		configButton(NEW_PARAM, "New seed");
		configParam(LOOSE_PARAM, 0.f, 2.5f, 1.f, "Loose (0 machine, 1 a good player, 2.5 drunk)", "%", 0.f, 100.f)->description =
			"Scales everything human: playing behind the beat, rolled chords, louder top notes, loudness drift, staggered releases, dropped and ghosted notes.";
		configParam(TIMING_PARAM, 0.f, 2.f, 1.f, "Timing trim (x LOOSE)", "%", 0.f, 100.f);
		configParam(ROLL_PARAM, 0.f, 2.f, 1.f, "Roll trim (x LOOSE)", "%", 0.f, 100.f);
		configParam(WEIGHT_PARAM, 0.f, 2.f, 1.f, "Weight trim (x LOOSE): accents and top-note voicing", "%", 0.f, 100.f);
		configParam(SLOP_PARAM, 0.f, 2.f, 1.f, "Slop trim (x LOOSE): lengths, dropped and ghosted notes", "%", 0.f, 100.f);
		configParam(PEDAL_PARAM, 0.f, 1.f, 0.f, "Pedal depth (0 never pedals)", "%", 0.f, 100.f);
		configParam(LISTEN_PARAM, 0.f, 1.f, 0.f, "Listen to the soloist", "%", 0.f, 100.f)->description = "Needs the SOLO input. Leaves room while the soloist plays and answers in the gaps.";
		configParam(TEMPO_PARAM, 40.f, 260.f, 120.f, "Tempo (when CLOCK is unpatched)", " bpm");
		configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
		for (int i = 0; i < 16; i++) {
			struct RootQ : ParamQuantity {
				std::string getDisplayValueString() override { return NOTE_NAMES[pmod((int)std::round(getValue()), 12)]; }
			};
			configParam<RootQ>(ROOT_PARAM + i, 0.f, 11.f, 0.f, string::f("Step %d root", i + 1))->snapEnabled = true;
			configSwitch(QUAL_PARAM + i, 0.f, 9.f, 0.f, string::f("Step %d quality", i + 1), {"maj7", "6", "m7", "m6", "mMaj7", "7", "7alt", "m7b5", "dim7", "7sus"});
		}
		for (int id : {(int)STEPS_PARAM, (int)BEATS_PARAM, (int)KEY_PARAM, (int)SEED_PARAM, (int)NEW_PARAM, (int)TEMPO_PARAM, (int)RUN_PARAM})
			getParamQuantity(id)->randomizeEnabled = false;
		for (int i = 0; i < 16; i++) { getParamQuantity(ROOT_PARAM + i)->randomizeEnabled = false; getParamQuantity(QUAL_PARAM + i)->randomizeEnabled = false; }

		configInput(CLOCK_INPUT, "Clock (pulses per beat in the right-click menu)");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run gate (high = run; overrides the button)");
		configInput(KEY_INPUT, "Key (V/oct, rounded to a semitone)");
		configInput(ROOT_INPUT, "Chord root (V/oct). Patching it switches from the chart to CV chords");
		configInput(QUAL_INPUT, "Chord quality (1 V per step: maj7 6 m7 m6 mMaj7 7 7alt m7b5 dim7 7sus)");
		configInput(CHORD_VOCT_INPUT, "Chord notes (poly V/oct). Patching it switches to chords worked out from the notes");
		configInput(CHORD_GATE_INPUT, "Chord gates (poly; all notes count when unpatched)");
		configInput(SOLO_INPUT, "Soloist gate or audio");
		configInput(SOLO_VOCT_INPUT, "Soloist pitch (V/oct): the chords stay under it");
		configInput(DENSITY_INPUT, "DENSITY CV (0 to 10 V adds 0 to 100%)");
		configInput(OUT_INPUT, "OUT CV (0 to 10 V adds 0 to 100%)");
		configInput(LOOSE_INPUT, "LOOSE CV (0 to 10 V adds 0 to 250%)");
		configInput(LISTEN_INPUT, "LISTEN CV (0 to 10 V adds 0 to 100%)");
		configInput(REGISTER_INPUT, "REGISTER CV (1 V per octave)");
		configOutput(VOCT_OUTPUT, "Notes (poly V/oct, 0 V = middle C)");
		configOutput(GATE_OUTPUT, "Gates (poly)");
		configOutput(VEL_OUTPUT, "Velocity (poly, 0 to 10 V)");
		configOutput(PEDAL_OUTPUT, "Sustain pedal (0 to 10 V)");
		configOutput(SCALE_OUTPUT, "Chord scale (poly V/oct, for quantisers)");
		configOutput(BASS_VOCT_OUTPUT, "Bass root (V/oct, E1 to Eb2)");
		configOutput(BASS_GATE_OUTPUT, "Bass gate (high while running, drops for 5 ms at each chord change)");
		configOutput(CHANGE_OUTPUT, "Chord change trigger");
		configOutput(EOC_OUTPUT, "End of chart trigger");
		configLight(HIT_LIGHT, "Chord played");
		configLight(SOLO_LIGHT, "Soloist heard");
		lightDiv.setDivision(64);
		loadProgression(0);
	}

	void loadProgression(int i) {
		const Progression& p = PROGRESSIONS[i];
		std::vector<Chord> ch; std::string bad;
		if (!parseChart(p.text, ch, bad)) return;
		setChart(ch);
		int b = 2;
		for (int k = 0; k < 4; k++) if (BEATS_CHOICES[k] == p.beats) b = k;
		params[BEATS_PARAM].setValue((float)b);
	}
	void setChart(const std::vector<Chord>& ch) {
		params[STEPS_PARAM].setValue((float)ch.size());
		for (size_t k = 0; k < ch.size() && k < 16; k++) {
			params[ROOT_PARAM + k].setValue((float)ch[k].root);
			params[QUAL_PARAM + k].setValue((float)ch[k].q);
		}
	}
	std::string chartText() {
		int steps = (int)params[STEPS_PARAM].getValue();
		std::string s;
		for (int k = 0; k < steps; k++) {
			if (k) s += " ";
			s += chordName(Chord((int)params[ROOT_PARAM + k].getValue(), (int)params[QUAL_PARAM + k].getValue()));
		}
		return s;
	}

	void onReset() override {
		ppbIndex = 0; channels = 12; bassEveryBar = false; resetReplays = true;
		loadProgression(0);
		host = Host();
	}

	static float cv01(Input& i, float scale) { return i.isConnected() ? i.getVoltage() * 0.1f * scale : 0.f; }

	void process(const ProcessArgs& args) override {
		if (newTrig.process(params[NEW_PARAM].getValue() > 0.f))
			params[SEED_PARAM].setValue((float)(random::u32() % 1000));

		// chart
		in.steps = clamp((int)params[STEPS_PARAM].getValue(), 1, 16);
		in.beatsPerStep = BEATS_CHOICES[clamp((int)params[BEATS_PARAM].getValue(), 0, 3)];
		in.key = (int)params[KEY_PARAM].getValue();
		in.keyConnected = inputs[KEY_INPUT].isConnected(); in.keyV = inputs[KEY_INPUT].getVoltage();
		for (int i = 0; i < 16; i++) {
			in.stepRoot[i] = clamp((int)params[ROOT_PARAM + i].getValue(), 0, 11);
			in.stepQual[i] = clamp((int)params[QUAL_PARAM + i].getValue(), 0, 9);
		}
		in.rootConnected = inputs[ROOT_INPUT].isConnected(); in.rootV = inputs[ROOT_INPUT].getVoltage();
		in.qualConnected = inputs[QUAL_INPUT].isConnected(); in.qualV = inputs[QUAL_INPUT].getVoltage();
		in.chordConnected = inputs[CHORD_VOCT_INPUT].isConnected();
		in.chordChannels = inputs[CHORD_VOCT_INPUT].getChannels();
		for (int c = 0; c < in.chordChannels; c++) in.chordV[c] = inputs[CHORD_VOCT_INPUT].getVoltage(c);
		in.chordGateConnected = inputs[CHORD_GATE_INPUT].isConnected();
		in.chordGateChannels = inputs[CHORD_GATE_INPUT].getChannels();
		for (int c = 0; c < in.chordGateChannels; c++) in.chordGate[c] = inputs[CHORD_GATE_INPUT].getVoltage(c);
		// transport
		in.tempo = params[TEMPO_PARAM].getValue();
		in.clockConnected = inputs[CLOCK_INPUT].isConnected(); in.clock = inputs[CLOCK_INPUT].getVoltage();
		in.ppb = PPB_CHOICES[clamp(ppbIndex, 0, 3)];
		in.reset = inputs[RESET_INPUT].getVoltage();
		in.runConnected = inputs[RUN_INPUT].isConnected(); in.runGate = inputs[RUN_INPUT].getVoltage();
		in.runButton = params[RUN_PARAM].getValue() > 0.5f;
		in.soloConnected = inputs[SOLO_INPUT].isConnected(); in.solo = inputs[SOLO_INPUT].getVoltage();
		in.soloPitchConnected = inputs[SOLO_VOCT_INPUT].isConnected(); in.soloPitch = inputs[SOLO_VOCT_INPUT].getVoltage();
		// player
		Params& p = in.p;
		p.style = clamp((int)params[STYLE_PARAM].getValue(), 0, 4);
		p.spread = params[SPREAD_PARAM].getValue();
		p.reg = clamp(params[REGISTER_PARAM].getValue() + (inputs[REGISTER_INPUT].isConnected() ? inputs[REGISTER_INPUT].getVoltage() * 12.f : 0.f), 60.f, 79.f);
		p.voices = clamp((int)params[VOICES_PARAM].getValue(), 2, 5);
		p.lead = params[LEAD_PARAM].getValue();
		p.out = clamp(params[OUT_PARAM].getValue() + cv01(inputs[OUT_INPUT], 1.f), 0.f, 1.f);
		p.roots = clamp((int)params[ROOTS_PARAM].getValue(), 0, 2);
		p.groove = clamp((int)params[GROOVE_PARAM].getValue(), 0, 4);
		p.density = clamp(params[DENSITY_PARAM].getValue() + cv01(inputs[DENSITY_INPUT], 1.f), 0.f, 1.f);
		p.swing = params[SWING_PARAM].getValue();
		p.repeat = params[REPEAT_PARAM].getValue();
		p.push = params[PUSH_PARAM].getValue();
		p.loose = clamp(params[LOOSE_PARAM].getValue() + cv01(inputs[LOOSE_INPUT], 2.5f), 0.f, 2.5f);
		p.timing = params[TIMING_PARAM].getValue();
		p.roll = params[ROLL_PARAM].getValue();
		p.weight = params[WEIGHT_PARAM].getValue();
		p.slop = params[SLOP_PARAM].getValue();
		p.pedal = params[PEDAL_PARAM].getValue();
		p.listen = clamp(params[LISTEN_PARAM].getValue() + cv01(inputs[LISTEN_INPUT], 1.f), 0.f, 1.f);
		in.seed = (int)params[SEED_PARAM].getValue();
		in.outChannels = channels; in.bassEveryBar = bassEveryBar; in.resetReplays = resetReplays;

		host.process(in, out, args.sampleTime);

		int n = out.channels;
		outputs[VOCT_OUTPUT].setChannels(n); outputs[GATE_OUTPUT].setChannels(n); outputs[VEL_OUTPUT].setChannels(n);
		for (int c = 0; c < n; c++) {
			outputs[VOCT_OUTPUT].setVoltage(out.voct[c], c);
			outputs[GATE_OUTPUT].setVoltage(out.gate[c], c);
			outputs[VEL_OUTPUT].setVoltage(out.vel[c], c);
		}
		outputs[PEDAL_OUTPUT].setVoltage(out.pedal);
		outputs[SCALE_OUTPUT].setChannels(std::max(1, out.scaleN));
		for (int c = 0; c < out.scaleN; c++) outputs[SCALE_OUTPUT].setVoltage(out.scale[c], c);
		outputs[BASS_VOCT_OUTPUT].setVoltage(out.bassV);
		outputs[BASS_GATE_OUTPUT].setVoltage(out.bassGate);
		outputs[CHANGE_OUTPUT].setVoltage(out.change);
		outputs[EOC_OUTPUT].setVoltage(out.eoc);

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			bool chart = host.source == SRC_CHART;
			int st = host.stepAt(in, std::max(0.0, host.phi));
			for (int i = 0; i < 16; i++)
				lights[STEP_LIGHT + i].setBrightnessSmooth(chart && i == st ? 1.f : (i < in.steps ? 0.08f : 0.f), dt);
			lights[HIT_LIGHT].setBrightnessSmooth(host.t - host.hitAt < 0.06 ? 1.f : 0.f, dt);
			lights[SOLO_LIGHT].setBrightness(host.soloSnd ? 1.f : 0.f);
			lights[RUN_LIGHT].setBrightness(host.running ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppb", json_integer(ppbIndex));
		json_object_set_new(root, "channels", json_integer(channels));
		json_object_set_new(root, "bassEveryBar", json_boolean(bassEveryBar));
		json_object_set_new(root, "resetReplays", json_boolean(resetReplays));
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppb")) ppbIndex = clamp((int)json_integer_value(j), 0, 3);
		if (json_t* j = json_object_get(root, "channels")) channels = clamp((int)json_integer_value(j), 1, 16);
		if (json_t* j = json_object_get(root, "bassEveryBar")) bassEveryBar = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "resetReplays")) resetReplays = json_boolean_value(j);
	}
};

// ---------------------------------------------------------------- display
static NVGcolor AMBER() { return nvgRGB(0xF2, 0xC2, 0x5C); }
static NVGcolor DIMCOL() { return nvgRGB(0x8A, 0x7A, 0x55); }

struct ChordDisplay : TransparentWidget {
	Comper* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		std::string cur = "Dm7", nxt = "G7", src = "CHART", voic = "F3 A3 C4 E4", seed = "1";
		bool demo = !module;
		if (module) {
			Host& h = module->host;
			src = SOURCE_NAMES[h.source];
			cur = h.haveSrc ? chordName(h.srcChord) : "--";
			nxt = "";
			if (h.source == SRC_CHART) {
				Chord c;
				if (h.chartNext(module->in, std::max(0.0, h.phi), c)) nxt = chordName(c);
			}
			voic = "";
			if (h.haveLast) {
				for (size_t i = 0; i < h.lastVoicing.size(); i++) { if (i) voic += " "; voic += noteName(h.lastVoicing[i]); }
				if (h.lastChord != h.srcChord) voic += "  (" + chordName(h.lastChord) + ")";
			}
			seed = std::to_string(module->in.seed);
			if (!h.running) src += "  STOPPED";
		}
		(void)demo;
		float w = box.size.x, hgt = box.size.y;
		nvgFillColor(args.vg, AMBER());
		nvgFontSize(args.vg, 25.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, 6.f, hgt * 0.58f, cur.c_str(), NULL);
		float b[4]; nvgTextBounds(args.vg, 6.f, hgt * 0.58f, cur.c_str(), NULL, b);
		if (!nxt.empty()) {
			nvgFontSize(args.vg, 14.f); nvgFillColor(args.vg, DIMCOL());
			std::string s = "> " + nxt;
			nvgText(args.vg, b[2] + 8.f, hgt * 0.58f, s.c_str(), NULL);
		}
		nvgFontSize(args.vg, 11.f); nvgFillColor(args.vg, DIMCOL());
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(args.vg, w - 6.f, 14.f, src.c_str(), NULL);
		std::string sd = "SEED " + seed;
		nvgText(args.vg, w - 6.f, 28.f, sd.c_str(), NULL);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(args.vg, AMBER()); nvgFontSize(args.vg, 12.f);
		nvgText(args.vg, 6.f, hgt - 6.f, voic.c_str(), NULL);
	}
};

struct StepNames : TransparentWidget {
	Comper* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 9.5f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
		int steps = module ? clamp((int)module->params[Comper::STEPS_PARAM].getValue(), 1, 16) : 4;
		int key = module ? module->host.keyOffset(module->in) : 0;
		for (int i = 0; i < 16; i++) {
			Chord c;
			if (module) c = Chord(pmod((int)module->params[Comper::ROOT_PARAM + i].getValue() + key, 12), (int)module->params[Comper::QUAL_PARAM + i].getValue());
			else { static const int R[4] = {2, 7, 0, 0}, Q[4] = {M7, DOM, MAJ, MAJ}; c = Chord(R[i % 4], Q[i % 4]); }
			std::string s = chordName(c);
			nvgFillColor(args.vg, i < steps ? AMBER() : nvgRGBA(0x8A, 0x7A, 0x55, 0x50));
			float x = mm2px(stepX(i)) - box.pos.x;
			nvgText(args.vg, x, box.size.y - 3.f, s.c_str(), NULL);
		}
	}
};

struct AmberLightBase : GrayModuleLightWidget { AmberLightBase() { addBaseColor(nvgRGB(0xF2, 0xC2, 0x5C)); } };

struct ChartField : ui::TextField {
	Comper* module = nullptr;
	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			std::vector<Chord> ch; std::string bad;
			if (parseChart(text, ch, bad)) {
				module->setChart(ch);
				ui::MenuOverlay* ov = getAncestorOfType<ui::MenuOverlay>();
				if (ov) ov->requestDelete();
			} else {
				text = "?? " + bad + " ?? " + text;
				selectAll();
			}
			e.consume(this);
		}
		if (!e.getTarget()) TextField::onSelectKey(e);
	}
};

struct ComperWidget : ModuleWidget {
	ComperWidget(Comper* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Comper.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		ChordDisplay* d = createWidget<ChordDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H)); d->module = module; addChild(d);
		StepNames* sn = createWidget<StepNames>(mm2px(Vec(0.f, Y_STEPNAME - 4.f)));
		sn->box.size = mm2px(Vec(182.88f, 5.f)); sn->module = module; addChild(sn);

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(SMALLK_X[0], SMALLK_Y)), module, Comper::STEPS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(SMALLK_X[1], SMALLK_Y)), module, Comper::BEATS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(SMALLK_X[2], SMALLK_Y)), module, Comper::KEY_PARAM));
		for (int i = 0; i < 16; i++) {
			addChild(createLightCentered<SmallLight<AmberLightBase>>(mm2px(Vec(stepX(i), Y_STEPLIGHT)), module, Comper::STEP_LIGHT + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(stepX(i), Y_ROOT)), module, Comper::ROOT_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(stepX(i), Y_QUAL)), module, Comper::QUAL_PARAM + i));
		}
		// row A
		const int rowA[12] = {Comper::STYLE_PARAM, Comper::SPREAD_PARAM, Comper::REGISTER_PARAM, Comper::VOICES_PARAM,
			Comper::GROOVE_PARAM, Comper::DENSITY_PARAM, Comper::SWING_PARAM, Comper::REPEAT_PARAM,
			Comper::LOOSE_PARAM, Comper::TIMING_PARAM, Comper::ROLL_PARAM, Comper::WEIGHT_PARAM};
		for (int i = 0; i < 12; i++) {
			Vec pos = mm2px(Vec(colX(i), Y_ROWA));
			if (i == 8) addParam(createParamCentered<RoundLargeBlackKnob>(pos, module, rowA[i]));
			else if (i >= 9) addParam(createParamCentered<Trimpot>(pos, module, rowA[i]));
			else addParam(createParamCentered<RoundBlackKnob>(pos, module, rowA[i]));
		}
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(12), Y_ROWA)), module, Comper::LISTEN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(13), Y_ROWA)), module, Comper::TEMPO_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(colX(14), Y_ROWA)), module, Comper::RUN_PARAM, Comper::RUN_LIGHT));
		// row B
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(0), Y_ROWB)), module, Comper::LEAD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(1), Y_ROWB)), module, Comper::OUT_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(colX(2), Y_ROWB)), module, Comper::ROOTS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(4), Y_ROWB)), module, Comper::PUSH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(5), Y_ROWB)), module, Comper::SEED_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(colX(6), Y_ROWB)), module, Comper::NEW_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colX(8), Y_ROWB)), module, Comper::SLOP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(9), Y_ROWB)), module, Comper::PEDAL_PARAM));
		addChild(createLightCentered<MediumLight<AmberLightBase>>(mm2px(Vec(colX(12), Y_ROWB)), module, Comper::HIT_LIGHT));
		addChild(createLightCentered<MediumLight<AmberLightBase>>(mm2px(Vec(colX(13), Y_ROWB)), module, Comper::SOLO_LIGHT));
		// jacks
		const int in1[10] = {Comper::CLOCK_INPUT, Comper::RESET_INPUT, Comper::RUN_INPUT, Comper::KEY_INPUT, Comper::ROOT_INPUT,
			Comper::QUAL_INPUT, Comper::CHORD_VOCT_INPUT, Comper::CHORD_GATE_INPUT, Comper::SOLO_INPUT, Comper::SOLO_VOCT_INPUT};
		for (int i = 0; i < 10; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), Y_IN1)), module, in1[i]));
		const int in2[5] = {Comper::DENSITY_INPUT, Comper::OUT_INPUT, Comper::LOOSE_INPUT, Comper::LISTEN_INPUT, Comper::REGISTER_INPUT};
		for (int i = 0; i < 5; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), Y_IN2)), module, in2[i]));
		const int out1[5] = {Comper::VOCT_OUTPUT, Comper::GATE_OUTPUT, Comper::VEL_OUTPUT, Comper::PEDAL_OUTPUT, Comper::SCALE_OUTPUT};
		for (int i = 0; i < 5; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(OUT_COL0 + i), Y_IN1)), module, out1[i]));
		const int out2[4] = {Comper::BASS_VOCT_OUTPUT, Comper::BASS_GATE_OUTPUT, Comper::CHANGE_OUTPUT, Comper::EOC_OUTPUT};
		for (int i = 0; i < 4; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(OUT_COL0 + i), Y_IN2)), module, out2[i]));
	}

	void appendContextMenu(Menu* menu) override {
		Comper* m = dynamic_cast<Comper*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Load progression", "", [=](Menu* sub) {
			for (int i = 0; i < NUM_PROGRESSIONS; i++)
				sub->addChild(createMenuItem(PROGRESSIONS[i].name, PROGRESSIONS[i].text, [=]() { m->loadProgression(i); }));
		}));
		menu->addChild(createMenuLabel("Type a chart (Enter to set; % repeats, up to 16 steps):"));
		ChartField* f = new ChartField;
		f->module = m; f->box.size.x = 360.f; f->text = m->chartText();
		f->placeholder = "Dm7 G7 Cmaj7 %";
		menu->addChild(f);
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock pulses per beat", {"1", "2", "4", "24 (MIDI clock)"},
			[=]() { return m->ppbIndex; }, [=](size_t i) { m->ppbIndex = (int)i; }));
		menu->addChild(createIndexSubmenuItem("Output channels", {"8", "12", "16"},
			[=]() { return m->channels == 8 ? 0 : m->channels == 16 ? 2 : 1; },
			[=](size_t i) { m->channels = i == 0 ? 8 : i == 2 ? 16 : 12; }));
		menu->addChild(createBoolPtrMenuItem("Bass retriggers every bar", "", &m->bassEveryBar));
		menu->addChild(createBoolPtrMenuItem("Reset replays the same take", "", &m->resetReplays));
	}
};

Model* modelComper = createModel<Comper, ComperWidget>("Comper");
