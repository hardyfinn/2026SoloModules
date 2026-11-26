#include "plugin.hpp"
#include "dsp/Host.hpp"
#include "Progressions.hpp"
#include "LinkPort.hpp"
#include <set>

using namespace comper;

#include "ComperLayout.hpp"
using namespace comperlayout;

static const int BEATS_CHOICES[4] = {1, 2, 4, 8};
static const int PPB_CHOICES[4] = {1, 2, 4, 24};
static const std::vector<std::string> QUAL_LABELS = {"maj7", "6", "m7", "m6", "mMaj7", "7", "7alt", "m7b5", "dim7", "7sus", "triad", "minor triad"};

struct Comper : Module {
	enum ParamIds {
		STEPS_PARAM, BEATS_PARAM, KEY_PARAM,
		STYLE_PARAM, SPREAD_PARAM, REGISTER_PARAM, VOICES_PARAM, LEAD_PARAM, OUT_PARAM, ROOTS_PARAM,
		GROOVE_PARAM, DENSITY_PARAM, SWING_PARAM, REPEAT_PARAM, PUSH_PARAM, SEED_PARAM, NEW_PARAM,
		LOOSE_PARAM, TIMING_PARAM, ROLL_PARAM, WEIGHT_PARAM, SLOP_PARAM, PEDAL_PARAM,
		LISTEN_PARAM, TEMPO_PARAM, RUN_PARAM,
		ENUMS(ROOT_PARAM, 16), ENUMS(QUAL_PARAM, 16),
		// 2.3.0 (appended so old patches and presets keep their ids)
		OCTAVE_PARAM, LENGTH_PARAM, EDIT_ROOT_PARAM, EDIT_QUAL_PARAM,
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
	bool keepRootsWithWalker = false;
	LinkPort port;
	CompLink heard;
	dsp::BooleanTrigger newTrig;
	dsp::ClockDivider lightDiv;
	int pendingSeed = -1;
	// 2.3.0: the chart is edited on the screen (tap a step, then turn EDIT ROOT / QUAL)
	int editStep = 0;
	float lastEditRoot = -1.f, lastEditQual = -1.f;
	int lastEditStep = -1;
	// 2.3.0: preset browser on the screen
	std::string presetName;
	int presetIndex = -1;
	std::vector<float> presetSnap;
	bool snapPending = true;

	Comper() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		port.attach(this);
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
		configParam(OUT_PARAM, 0.f, 1.f, 0.f, "Autre (altered dominants, tritone substitutes, side-slips)", "%", 0.f, 100.f);
		configSwitch(ROOTS_PARAM, 0.f, 2.f, 0.f, "Roots in the left hand", {"Off (the bass has them)", "Some", "All (solo piano)"});
		configSwitch(GROOVE_PARAM, 0.f, 5.f, 0.f, "Groove", {"Swing", "Bossa", "Ballad", "Soul (sixteenths)", "House (sixteenths)", "Reggae (offbeat skank)"});
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density", "%", 0.f, 100.f);
		struct SwingQ : ParamQuantity {
			std::string getDisplayValueString() override {
				Comper* m = (Comper*)module;
				double bpm = m ? 60.0 / m->host.spb : 120.0;
				double v = getValue();
				int g = m ? (int)m->params[GROOVE_PARAM].getValue() : 0;
				if (g == 3 || g == 4) return string::f("%+.0f%% (sixteenth push %.2f)", v * 100.0, std::max(0.5, std::min(0.67, (g == 3 ? 0.54 : 0.5) + 0.12 * v)));
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
			configSwitch(QUAL_PARAM + i, 0.f, (float)(NUM_Q - 1), 0.f, string::f("Step %d quality", i + 1), QUAL_LABELS);
		}
		configParam(OCTAVE_PARAM, -3.f, 3.f, 0.f, "Octave (moves the chord outputs; the bass outputs stay)", " oct")->snapEnabled = true;
		configParam(LENGTH_PARAM, 0.f, 1.f, 0.6f, "Length (short to legato)", "%", 0.f, 100.f)->description =
			"How long the hands hold each chord. SLOP and LOOSE set how much the lengths vary: top notes ring longest, inner voices lift first, accents sit longer.";
		{
			struct EditRootQ : ParamQuantity {
				std::string getDisplayValueString() override { return NOTE_NAMES[pmod((int)std::round(getValue()), 12)]; }
			};
			configParam<EditRootQ>(EDIT_ROOT_PARAM, 0.f, 11.f, 2.f, "Root of the tapped step")->snapEnabled = true;
			configSwitch(EDIT_QUAL_PARAM, 0.f, (float)(NUM_Q - 1), (float)M7, "Quality of the tapped step", QUAL_LABELS);
		}
		for (int id : {(int)STEPS_PARAM, (int)BEATS_PARAM, (int)KEY_PARAM, (int)SEED_PARAM, (int)NEW_PARAM, (int)TEMPO_PARAM, (int)RUN_PARAM,
		               (int)OCTAVE_PARAM, (int)EDIT_ROOT_PARAM, (int)EDIT_QUAL_PARAM})
			getParamQuantity(id)->randomizeEnabled = false;
		for (int i = 0; i < 16; i++) { getParamQuantity(ROOT_PARAM + i)->randomizeEnabled = false; getParamQuantity(QUAL_PARAM + i)->randomizeEnabled = false; }

		configInput(CLOCK_INPUT, "Clock (pulses per beat in the right-click menu)");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run gate (high = run; overrides the button)");
		configInput(KEY_INPUT, "Key (V/oct, rounded to a semitone)");
		configInput(ROOT_INPUT, "Chord root (V/oct). Patching it switches from the chart to CV chords");
		configInput(QUAL_INPUT, "Chord quality (1 V per step: maj7 6 m7 m6 mMaj7 7 7alt m7b5 dim7 7sus triad minor-triad)");
		configInput(CHORD_VOCT_INPUT, "Chord notes (poly V/oct). Patching it switches to chords worked out from the notes");
		configInput(CHORD_GATE_INPUT, "Chord gates (poly; all notes count when unpatched)");
		configInput(SOLO_INPUT, "Soloist gate or audio");
		configInput(SOLO_VOCT_INPUT, "Soloist pitch (V/oct): the chords stay under it");
		configInput(DENSITY_INPUT, "DENSITY CV (0 to 10 V adds 0 to 100%)");
		configInput(OUT_INPUT, "AUTRE CV (0 to 10 V adds 0 to 100%)");
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
		ppbIndex = 0; channels = 12; bassEveryBar = false; resetReplays = true; keepRootsWithWalker = false;
		editStep = 0; lastEditStep = -1; presetName.clear(); presetIndex = -1; snapPending = true;
		loadProgression(0);
		host = Host();
	}

	static float cv01(Input& i, float scale) { return i.isConnected() ? i.getVoltage() * 0.1f * scale : 0.f; }

	void process(const ProcessArgs& args) override {
		if (newTrig.process(params[NEW_PARAM].getValue() > 0.f))
			params[SEED_PARAM].setValue((float)(random::u32() % 1000));

		syncEdit();
		// chart
		in.steps = clamp((int)params[STEPS_PARAM].getValue(), 1, 16);
		in.beatsPerStep = BEATS_CHOICES[clamp((int)params[BEATS_PARAM].getValue(), 0, 3)];
		in.key = (int)params[KEY_PARAM].getValue();
		in.keyConnected = inputs[KEY_INPUT].isConnected(); in.keyV = inputs[KEY_INPUT].getVoltage();
		for (int i = 0; i < 16; i++) {
			in.stepRoot[i] = clamp((int)params[ROOT_PARAM + i].getValue(), 0, 11);
			in.stepQual[i] = clamp((int)params[QUAL_PARAM + i].getValue(), 0, NUM_Q - 1);
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
		// the band: a pianist stops playing roots when a bassist arrives; a drummer's BAND moves the density
		port.read(this); heard = port.merged();
		if (heard.hasBass && !keepRootsWithWalker) p.roots = 0;
		float bandX = heard.hasDrum ? clamp(heard.bandX, -1.f, 1.f) : 0.f;
		p.groove = clamp((int)params[GROOVE_PARAM].getValue(), 0, NUM_GROOVES - 1);
		p.density = clamp(params[DENSITY_PARAM].getValue() + cv01(inputs[DENSITY_INPUT], 1.f) + 0.35f * bandX, 0.f, 1.f);
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
		p.human = true;
		p.length = clamp(params[LENGTH_PARAM].getValue(), 0.f, 1.f);
		in.octave = clamp((int)std::round(params[OCTAVE_PARAM].getValue()), -3, 3);
		in.seed = (int)params[SEED_PARAM].getValue();
		in.outChannels = channels; in.bassEveryBar = bassEveryBar; in.resetReplays = resetReplays;

		host.process(in, out, args.sampleTime);
		sendLink();

		int n = out.channels;
		outputs[VOCT_OUTPUT].setChannels(n); outputs[GATE_OUTPUT].setChannels(n); outputs[VEL_OUTPUT].setChannels(n);
		for (int c = 0; c < n; c++) {
			outputs[VOCT_OUTPUT].setVoltage(out.voct[c], c);
			outputs[GATE_OUTPUT].setVoltage(out.gate[c], c);
			outputs[VEL_OUTPUT].setVoltage(std::min(10.f, out.vel[c] * (1.f + 0.25f * bandX)), c);
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

	// EDIT ROOT / QUAL follow the tapped step; turning them writes into it
	void syncEdit() {
		int k = clamp(editStep, 0, 15);
		float er = params[EDIT_ROOT_PARAM].getValue(), eq = params[EDIT_QUAL_PARAM].getValue();
		float sr = params[ROOT_PARAM + k].getValue(), sq = params[QUAL_PARAM + k].getValue();
		if (k != lastEditStep) {
			params[EDIT_ROOT_PARAM].setValue(sr); params[EDIT_QUAL_PARAM].setValue(sq);
			lastEditRoot = sr; lastEditQual = sq; lastEditStep = k;
			return;
		}
		if (er != lastEditRoot) { params[ROOT_PARAM + k].setValue(er); lastEditRoot = er; }
		else if (sr != er) { params[EDIT_ROOT_PARAM].setValue(sr); lastEditRoot = sr; }
		if (eq != lastEditQual) { params[QUAL_PARAM + k].setValue(eq); lastEditQual = eq; }
		else if (sq != eq) { params[EDIT_QUAL_PARAM].setValue(sq); lastEditQual = sq; }
	}

	// ---- preset browser
	// params that count as "changed since the preset loaded" (not the transport or the edit knobs)
	bool countsForDirty(int id) const {
		return id != RUN_PARAM && id != NEW_PARAM && id != EDIT_ROOT_PARAM && id != EDIT_QUAL_PARAM;
	}
	void takeSnapshot() {
		presetSnap.resize(NUM_PARAMS);
		for (int i = 0; i < NUM_PARAMS; i++) presetSnap[i] = params[i].getValue();
		snapPending = false;
	}
	bool presetDirty() {
		if (snapPending) takeSnapshot();
		if ((int)presetSnap.size() != NUM_PARAMS) return false;
		for (int i = 0; i < NUM_PARAMS; i++)
			if (countsForDirty(i) && std::fabs(params[i].getValue() - presetSnap[i]) > 1e-4f) return true;
		return false;
	}

	// tell the band where we are and what we are about to play (Rack's expander messages, one sample a hop)
	void sendLink() {
		CompLink m;
		m.hasComp = true;
		m.resetCount = host.resetCount;
		m.running = host.running && !host.waitingFirstPulse;
		m.phi = host.phi; m.spb = host.spb;
		m.steps = in.steps; m.beatsPerStep = in.beatsPerStep;
		int key = host.keyOffset(in);
		for (int k = 0; k < 16; k++) { m.root[k] = pmod(in.stepRoot[k] + key, 12); m.qual[k] = in.stepQual[k]; }
		m.external = host.source != SRC_CHART;
		m.curRoot = host.srcChord.root; m.curQual = host.srcChord.q;
		m.groove = in.p.groove; m.swing = in.p.swing;
		m.planBar = host.planBar; m.planSlots = host.eng.slots;
		for (int s = 0; s < 16; s++) { m.planKind[s] = host.eng.cellmap[s]; m.planBeat[s] = host.eng.slotBeats[s]; m.planPush[s] = host.eng.pushOk[s]; }
		port.send(this, m);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppb", json_integer(ppbIndex));
		json_object_set_new(root, "channels", json_integer(channels));
		json_object_set_new(root, "bassEveryBar", json_boolean(bassEveryBar));
		json_object_set_new(root, "resetReplays", json_boolean(resetReplays));
		json_object_set_new(root, "keepRootsWithWalker", json_boolean(keepRootsWithWalker));
		json_object_set_new(root, "editStep", json_integer(editStep));
		if (!presetName.empty()) json_object_set_new(root, "presetName", json_string(presetName.c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppb")) ppbIndex = clamp((int)json_integer_value(j), 0, 3);
		if (json_t* j = json_object_get(root, "channels")) channels = clamp((int)json_integer_value(j), 1, 16);
		if (json_t* j = json_object_get(root, "bassEveryBar")) bassEveryBar = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "resetReplays")) resetReplays = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "keepRootsWithWalker")) keepRootsWithWalker = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "editStep")) { editStep = clamp((int)json_integer_value(j), 0, 15); lastEditStep = -1; }
		// a patch carries the preset name; a preset loaded from Rack's own menu doesn't, so the screen shows "--"
		if (json_t* j = json_object_get(root, "presetName")) presetName = json_string_value(j);
		else presetName.clear();
		snapPending = true;
	}
};

// ---------------------------------------------------------------- display
static NVGcolor AMBER() { return nvgRGB(0xF2, 0xC2, 0x5C); }
static NVGcolor DIMCOL() { return nvgRGB(0x8A, 0x7A, 0x55); }

// ---------------------------------------------------------------- the screen (2.3.0)
// Top: preset bar (tap the arrows to step, the name for a list). Middle: chord, voicing, source.
// Bottom: the 16-step chart; tap a step to edit it with EDIT ROOT and QUAL.
static std::vector<std::string> comperPresetFiles(Model* model) {
	std::vector<std::string> out;
	if (!model) return out;
	std::string dirs[2] = {model->getFactoryPresetDirectory(), model->getUserPresetDirectory()};
	for (const std::string& dir : dirs) {
		if (dir.empty() || !system::isDirectory(dir)) continue;
		std::vector<std::string> fs;
		for (const std::string& f : system::getEntries(dir)) {
			std::string ext = system::getExtension(f);
			if (ext == ".vcvm" || ext == "vcvm") fs.push_back(f);
		}
		std::sort(fs.begin(), fs.end());
		out.insert(out.end(), fs.begin(), fs.end());
	}
	return out;
}
static std::string comperPresetName(const std::string& path) {
	std::string n = system::getStem(path);
	size_t u = n.find('_');
	if (u != std::string::npos && u > 0 && u <= 3) {
		bool digits = true;
		for (size_t i = 0; i < u; i++) if (n[i] < '0' || n[i] > '9') digits = false;
		if (digits) n = n.substr(u + 1);
	}
	return n;
}
static int comperPresetIndex(Comper* m, const std::vector<std::string>& files) {
	if (m->presetIndex >= 0 && m->presetIndex < (int)files.size() && comperPresetName(files[m->presetIndex]) == m->presetName)
		return m->presetIndex;
	for (int i = 0; i < (int)files.size(); i++) if (comperPresetName(files[i]) == m->presetName) return i;
	return -1;
}
static void comperLoadPreset(ModuleWidget* mw, Comper* m, int idx) {
	std::vector<std::string> files = comperPresetFiles(m->model);
	if (files.empty() || !mw) return;
	idx = pmod(idx, (int)files.size());
	// presets made before 2.3.0 don't store these; start them from their defaults
	m->params[Comper::OCTAVE_PARAM].setValue(0.f);
	m->params[Comper::LENGTH_PARAM].setValue(0.6f);
	try {
		mw->loadAction(files[idx]);
	} catch (Exception& e) {
		WARN("Comper: could not load preset %s: %s", files[idx].c_str(), e.what());
		return;
	}
	m->presetIndex = idx;
	m->presetName = comperPresetName(files[idx]);
	m->lastEditStep = -1;
	m->takeSnapshot();
}

struct ComperScreen : OpaqueWidget {
	Comper* module = nullptr;
	static float K() { return RACK_GRID_WIDTH / 5.08f; }   // px per mm

	void onButton(const ButtonEvent& e) override {
		if (e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT || !module) { OpaqueWidget::onButton(e); return; }
		float x = e.pos.x / K(), y = e.pos.y / K(), w = box.size.x / K();
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		if (y < 7.4f) {
			std::vector<std::string> files = comperPresetFiles(module->model);
			int cur = comperPresetIndex(module, files);
			if (x < 9.f) comperLoadPreset(mw, module, cur < 0 ? (int)files.size() - 1 : cur - 1);
			else if (x > w - 9.f) comperLoadPreset(mw, module, cur < 0 ? 0 : cur + 1);
			else {
				Comper* m = module;
				Menu* menu = createMenu();
				menu->addChild(createMenuLabel("Presets"));
				for (int i = 0; i < (int)files.size(); i++)
					menu->addChild(createCheckMenuItem(string::f("%02d  %s", i + 1, comperPresetName(files[i]).c_str()), "",
						[=]() { return i == cur; }, [=]() { comperLoadPreset(mw, m, i); }));
			}
			e.consume(this);
			return;
		}
		if (y > 16.2f) {
			float cw = (w - 2.4f) / 16.f;
			int k = (int)std::floor((x - 1.2f) / cw);
			if (k >= 0 && k < 16) { module->editStep = k; }
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}

	void text(NVGcontext* vg, float x, float y, float sizeMm, NVGcolor c, int align, const std::string& s, float* bounds = nullptr) {
		nvgFontSize(vg, sizeMm * K()); nvgFillColor(vg, c);
		nvgTextAlign(vg, align | NVG_ALIGN_BASELINE);
		nvgText(vg, x * K(), y * K(), s.c_str(), NULL);
		if (bounds) nvgTextBounds(vg, x * K(), y * K(), s.c_str(), NULL, bounds);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		NVGcontext* vg = args.vg;
		nvgFontFaceId(vg, font->handle);
		const float k = K(), w = box.size.x / k;
		NVGcolor amber = AMBER(), dim = DIMCOL(), faint = nvgRGBA(0x8A, 0x7A, 0x55, 0x60);

		// ---- preset bar
		nvgBeginPath(vg); nvgRoundedRect(vg, 1.2f * k, 1.2f * k, (w - 2.4f) * k, 5.4f * k, 0.8f * k);
		nvgFillColor(vg, nvgRGB(0x1C, 0x1A, 0x14)); nvgFill(vg);
		nvgFillColor(vg, amber);
		nvgBeginPath(vg); nvgMoveTo(vg, 3.2f * k, 3.9f * k); nvgLineTo(vg, 5.8f * k, 2.0f * k); nvgLineTo(vg, 5.8f * k, 5.8f * k); nvgClosePath(vg); nvgFill(vg);
		nvgBeginPath(vg); nvgMoveTo(vg, (w - 3.2f) * k, 3.9f * k); nvgLineTo(vg, (w - 5.8f) * k, 2.0f * k); nvgLineTo(vg, (w - 5.8f) * k, 5.8f * k); nvgClosePath(vg); nvgFill(vg);
		std::string pname = "BOSSA", pright = "4 / 12 · TAP NAME FOR LIST";
		int pnum = 4; bool dirty = false;
		if (module) {
			std::vector<std::string> files = comperPresetFiles(module->model);
			int cur = comperPresetIndex(module, files);
			pnum = cur + 1;
			pname = module->presetName.empty() ? "" : string::uppercase(module->presetName);
			dirty = !module->presetName.empty() && module->presetDirty();
			pright = cur >= 0 ? string::f("%d / %d · TAP NAME FOR LIST", cur + 1, (int)files.size()) : string::f("%d PRESETS · TAP NAME FOR LIST", (int)files.size());
		}
		text(vg, 8.2f, 5.0f, 2.3f, dim, NVG_ALIGN_LEFT, "PRESET");
		std::string ps = pname.empty() ? "--" : (pnum > 0 ? string::f("%02d · ", pnum) : std::string()) + pname + (dirty ? " *" : "");
		text(vg, 21.0f, 5.15f, 3.4f, amber, NVG_ALIGN_LEFT, ps);
		text(vg, w - 8.0f, 5.0f, 2.3f, dim, NVG_ALIGN_RIGHT, pright);

		// ---- chord, voicing, source
		std::string cur = "Dm7", nxt = "G7", src = "CHART", voic = "F4 A4 C5 E5", seed = "1";
		int oct = 1, steps = 8, play = 1, sel = 5;
		Chord chart[16];
		static const int DR[8] = {2, 7, 0, 0, 2, 7, 0, 0}, DQ[8] = {M7, DOM, MAJ, MAJ, HDIM, ALT, M6, M6};
		for (int i = 0; i < 16; i++) chart[i] = Chord(DR[i % 8], DQ[i % 8]);
		if (module) {
			Host& h = module->host;
			src = SOURCE_NAMES[h.source];
			cur = h.haveSrc ? chordName(h.srcChord) : "--";
			nxt = "";
			if (h.source == SRC_CHART) {
				Chord c;
				if (h.chartNext(module->in, std::max(0.0, h.phi), c)) nxt = chordName(c);
			}
			oct = module->in.octave;
			voic = "";
			if (h.haveLast) {
				for (size_t i = 0; i < h.lastVoicing.size(); i++) { if (i) voic += " "; voic += noteName(h.lastVoicing[i] + 12 * oct); }
				if (h.lastChord != h.srcChord) voic += "  (" + chordName(h.lastChord) + ")";
			}
			seed = std::to_string(module->in.seed);
			if (!h.running) src += " STOPPED";
			steps = module->in.steps;
			play = h.source == SRC_CHART ? h.stepAt(module->in, std::max(0.0, h.phi)) : -1;
			sel = clamp(module->editStep, 0, 15);
			int key = h.keyOffset(module->in);
			for (int i = 0; i < 16; i++)
				chart[i] = Chord(pmod((int)module->params[Comper::ROOT_PARAM + i].getValue() + key, 12),
				                 clamp((int)module->params[Comper::QUAL_PARAM + i].getValue(), 0, NUM_Q - 1));
		}
		float b[4];
		text(vg, 2.5f, 13.6f, 6.6f, amber, NVG_ALIGN_LEFT, cur, b);
		if (!nxt.empty()) text(vg, b[2] / k + 3.0f, 13.6f, 3.8f, dim, NVG_ALIGN_LEFT, "> " + nxt);
		text(vg, w / 2 + 6.f, 13.4f, 3.2f, amber, NVG_ALIGN_CENTER, voic);
		std::string info = src + " · SEED " + seed + (oct ? string::f(" · OCT %+d", oct) : std::string());
		text(vg, w - 2.5f, 13.4f, 2.6f, dim, NVG_ALIGN_RIGHT, info);

		// ---- chart strip
		const float cy = 16.8f, ch = 7.8f, cw = (w - 2.4f) / 16.f;
		for (int i = 0; i < 16; i++) {
			float x = 1.2f + i * cw;
			bool used = i < steps, isSel = i == sel, isPlay = i == play;
			nvgBeginPath(vg); nvgRoundedRect(vg, (x + 0.25f) * k, cy * k, (cw - 0.5f) * k, ch * k, 0.6f * k);
			nvgFillColor(vg, isSel ? nvgRGB(0x3A, 0x2F, 0x14) : nvgRGB(0x17, 0x18, 0x1B)); nvgFill(vg);
			nvgStrokeWidth(vg, (isSel || isPlay ? 0.45f : 0.25f) * k);
			nvgStrokeColor(vg, isSel ? amber : isPlay ? nvgRGB(0x5A, 0x4B, 0x28) : nvgRGB(0x26, 0x28, 0x30)); nvgStroke(vg);
			text(vg, x + 0.9f, cy + 2.1f, 1.6f, dim, NVG_ALIGN_LEFT, std::to_string(i + 1));
			if (isPlay) { nvgBeginPath(vg); nvgCircle(vg, (x + cw - 1.4f) * k, (cy + 1.4f) * k, 0.6f * k); nvgFillColor(vg, amber); nvgFill(vg); }
			std::string n = chordName(chart[i]);
			text(vg, x + cw / 2, cy + 6.2f, n.size() < 6 ? 2.9f : 2.5f, used ? amber : faint, NVG_ALIGN_CENTER, n);
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

		ComperScreen* d = createWidget<ComperScreen>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H)); d->module = module; addChild(d);

		// chart block
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(STEPS_X, STEPS_Y)), module, Comper::STEPS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(BEATS_X, BEATS_Y)), module, Comper::BEATS_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(KEY_X, KEY_Y)), module, Comper::KEY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(EDIT_ROOT_X, EDIT_ROOT_Y)), module, Comper::EDIT_ROOT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(EDIT_QUAL_X, EDIT_QUAL_Y)), module, Comper::EDIT_QUAL_PARAM));
		// voicing
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(STYLE_X, STYLE_Y)), module, Comper::STYLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(SPREAD_X, SPREAD_Y)), module, Comper::SPREAD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(VOICES_X, VOICES_Y)), module, Comper::VOICES_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(LEAD_X, LEAD_Y)), module, Comper::LEAD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(REGISTER_X, REGISTER_Y)), module, Comper::REGISTER_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(OUT_X, OUT_Y)), module, Comper::OUT_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(ROOTS_X, ROOTS_Y)), module, Comper::ROOTS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(OCTAVE_X, OCTAVE_Y)), module, Comper::OCTAVE_PARAM));
		// rhythm
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(GROOVE_X, GROOVE_Y)), module, Comper::GROOVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(SWING_X, SWING_Y)), module, Comper::SWING_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(DENSITY_X, DENSITY_Y)), module, Comper::DENSITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(REPEAT_X, REPEAT_Y)), module, Comper::REPEAT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(PUSH_X, PUSH_Y)), module, Comper::PUSH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(LENGTH_X, LENGTH_Y)), module, Comper::LENGTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(SEED_X, SEED_Y)), module, Comper::SEED_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(NEW_X, NEW_Y)), module, Comper::NEW_PARAM));
		// hands
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(LOOSE_X, LOOSE_Y)), module, Comper::LOOSE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(TIMING_X, TIMING_Y)), module, Comper::TIMING_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(ROLL_X, ROLL_Y)), module, Comper::ROLL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(PEDAL_X, PEDAL_Y)), module, Comper::PEDAL_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(WEIGHT_X, WEIGHT_Y)), module, Comper::WEIGHT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(SLOP_X, SLOP_Y)), module, Comper::SLOP_PARAM));
		// band
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(LISTEN_X, LISTEN_Y)), module, Comper::LISTEN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(TEMPO_X, TEMPO_Y)), module, Comper::TEMPO_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(RUN_X, RUN_Y)), module, Comper::RUN_PARAM, Comper::RUN_LIGHT));
		addChild(createLightCentered<MediumLight<AmberLightBase>>(mm2px(Vec(HIT_X, HIT_Y)), module, Comper::HIT_LIGHT));
		addChild(createLightCentered<MediumLight<AmberLightBase>>(mm2px(Vec(SOLO_X, SOLO_Y)), module, Comper::SOLO_LIGHT));
		// jacks
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_CLOCK_X, IN_CLOCK_Y)), module, Comper::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_RESET_X, IN_RESET_Y)), module, Comper::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_RUN_X, IN_RUN_Y)), module, Comper::RUN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_KEY_X, IN_KEY_Y)), module, Comper::KEY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_ROOT_X, IN_ROOT_Y)), module, Comper::ROOT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_QUAL_X, IN_QUAL_Y)), module, Comper::QUAL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_CHORD_VOCT_X, IN_CHORD_VOCT_Y)), module, Comper::CHORD_VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_CHORD_GATE_X, IN_CHORD_GATE_Y)), module, Comper::CHORD_GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_SOLO_X, IN_SOLO_Y)), module, Comper::SOLO_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_SOLO_VOCT_X, IN_SOLO_VOCT_Y)), module, Comper::SOLO_VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_DENSITY_X, IN_DENSITY_Y)), module, Comper::DENSITY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_OUT_X, IN_OUT_Y)), module, Comper::OUT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_LOOSE_X, IN_LOOSE_Y)), module, Comper::LOOSE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_LISTEN_X, IN_LISTEN_Y)), module, Comper::LISTEN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_REGISTER_X, IN_REGISTER_Y)), module, Comper::REGISTER_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_VOCT_X, OUT_VOCT_Y)), module, Comper::VOCT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_GATE_X, OUT_GATE_Y)), module, Comper::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_VEL_X, OUT_VEL_Y)), module, Comper::VEL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_PEDAL_X, OUT_PEDAL_Y)), module, Comper::PEDAL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_SCALE_X, OUT_SCALE_Y)), module, Comper::SCALE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_BASS_VOCT_X, OUT_BASS_VOCT_Y)), module, Comper::BASS_VOCT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_BASS_GATE_X, OUT_BASS_GATE_Y)), module, Comper::BASS_GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_CHANGE_X, OUT_CHANGE_Y)), module, Comper::CHANGE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_EOC_X, OUT_EOC_Y)), module, Comper::EOC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Comper* m = dynamic_cast<Comper*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Load progression", "", [=](Menu* sub) {
			// grouped by the "Genre: " prefix; the first ones (no prefix) are the jazz standards
			std::vector<std::string> groups;
			for (int i = 0; i < NUM_PROGRESSIONS; i++) {
				std::string n = PROGRESSIONS[i].name; size_t c = n.find(": ");
				std::string g = c == std::string::npos ? "Jazz" : n.substr(0, c);
				if (std::find(groups.begin(), groups.end(), g) == groups.end()) groups.push_back(g);
			}
			for (const std::string& g : groups) {
				sub->addChild(createSubmenuItem(g, "", [=](Menu* gm) {
					for (int i = 0; i < NUM_PROGRESSIONS; i++) {
						std::string n = PROGRESSIONS[i].name; size_t c = n.find(": ");
						std::string gg = c == std::string::npos ? "Jazz" : n.substr(0, c);
						if (gg != g) continue;
						std::string label = c == std::string::npos ? n : n.substr(c + 2);
						gm->addChild(createMenuItem(label, PROGRESSIONS[i].text, [=]() { m->loadProgression(i); }));
					}
				}));
			}
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
		menu->addChild(createBoolPtrMenuItem("Keep roots with Walker in the row", "", &m->keepRootsWithWalker));
	}
};

Model* modelComper = createModel<Comper, ComperWidget>("Comper");
