// Keel: keeps a kick drum in the key of the track.
//   KEY / TUNING  -> V/OCT for any kick with a V/OCT input (and an absolute PITCH out)
//   PATTERN       -> intervals that step on each TRIG, MUTATE + LOCK
//   METER         -> hears the kick (AUDIO), shows its pitch, AUTO-corrects or LEARNs it
//   UNDERTONE     -> replaces the kick's low end with a sub tuned to the target
#include "plugin.hpp"
#include "KeelEngine.hpp"

using namespace keel;

// ---- LAYOUT (same numbers as res/make_panel.py)
static const float DISP_X = 6.0f, DISP_Y = 13.5f, DISP_W = 140.4f, DISP_H = 23.0f;
static const float COL0 = 18.0f, COLDX = 16.6f;
static const float ROW1_Y = 52.0f, ROW2_Y = 76.0f, IN_Y = 98.5f, OUT_Y = 116.5f;
static float colX(int i) { return COL0 + COLDX * i; }

static const char* NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct Keel : Module {
	enum ParamId {
		ROOT_PARAM, SCALE_PARAM, FOLLOW_PARAM, OCT_PARAM, FINE_PARAM, GLIDE_PARAM, KICK_PARAM, LEARN_PARAM,
		STEPS_PARAM, MUTATE_PARAM, SPREAD_PARAM, LOCK_PARAM,
		XOVER_PARAM, REPLACE_PARAM, DROP_PARAM, DRIVE_PARAM,
		AUTO_PARAM, SUBLVL_PARAM,
		PARAMS_LEN
	};
	enum InputId { KEY_INPUT, CHORD_INPUT, BAR_INPUT, TRIG_INPUT, AUDIO_INPUT, MUTATE_INPUT, LOCK_INPUT, LEARN_INPUT, INPUTS_LEN };
	enum OutputId { VOCT_OUTPUT, PITCH_OUTPUT, INTUNE_OUTPUT, SUB_OUTPUT, MIX_OUTPUT, OUTPUTS_LEN };
	enum LightId { LEARN_LIGHT, LOCK_LIGHT, LIGHTS_LEN };

	keel::Engine eng;
	keel::Settings live;     // as the knobs and jacks say now
	keel::Settings held;     // what the kick plays (key/chord changes land on BAR when patched)
	bool lockLatch = false;
	bool mutated = false;
	bool firstHit = true;
	bool inTune = false;
	float voctOut = 0.f, pitchOut = 0.f;

	dsp::SchmittTrigger trigIn, barIn, learnIn;
	dsp::BooleanTrigger learnBtn, lockBtn;
	dsp::PulseGenerator learnFlash;
	dsp::ClockDivider ctlDiv;

	Keel() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(ROOT_PARAM, 0.f, 11.f, 0.f, "Root (key of the track)",
		             {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
		std::vector<std::string> sc;
		for (int i = 0; i < NSCALES; i++) sc.push_back(scaleName(i));
		configSwitch(SCALE_PARAM, 0.f, (float) (NSCALES - 1), 0.f, "Scale (keeps the pattern's 3rds, 5ths and 7ths in key)", sc);
		configSwitch(FOLLOW_PARAM, 0.f, 1.f, 0.f, "Follow", {"KEY (the kick sits on the root)", "CHORD (the kick follows the chord's lowest note)"});
		configParam(OCT_PARAM, -2.f, 2.f, 0.f, "Octave (after Keel places the note nearest the kick's own pitch)", " oct")->snapEnabled = true;
		configParam(FINE_PARAM, -50.f, 50.f, 0.f, "Fine", " cents");
		configParam(GLIDE_PARAM, 0.f, 1.f, 0.f, "Glide (between notes)", " ms", 0.f, 500.f);
		configParam(KICK_PARAM, std::log2(20.f), std::log2(200.f), std::log2(49.5f),
		            "Kick's own pitch at 0 V (808 and 909 Circuits: 49.5 Hz; LEARN sets it)", " Hz", 2.f);
		configButton(LEARN_PARAM, "Learn the kick's own pitch from the last hit it heard");
		configParam(STEPS_PARAM, 1.f, 16.f, 1.f, "Steps in the pattern (1 = always the root)")->snapEnabled = true;
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (chance each step re-rolls at the end of a loop)", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.3f, "Spread (0 = roots and a few 5ths, 1 = 5th, 4th, octave, 3rd and 7th too)", "%", 0.f, 100.f);
		configButton(LOCK_PARAM, "Lock (freeze the pattern)");
		configParam(XOVER_PARAM, std::log2(60.f), std::log2(300.f), std::log2(120.f), "Crossover (below this the sub replaces the kick)", " Hz", 2.f);
		configParam(REPLACE_PARAM, 0.f, 1.f, 1.f, "Replace (0 = the kick's own low end, 1 = all tuned sub)", "%", 0.f, 100.f);
		configParam(DROP_PARAM, 0.f, 12.f, 3.f, "Drop (sub starts this far sharp and falls into the note, like an 808 sigh)", " semitones");
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive (sub harmonics)", "%", 0.f, 100.f);
		configSwitch(AUTO_PARAM, 0.f, 1.f, 0.f, "Auto", {"OFF", "ON (listens to AUDIO and corrects V/OCT every hit)"});
		configParam(SUBLVL_PARAM, 0.f, 2.f, 1.f, "Sub level", " dB", -10.f, 20.f)->randomizeEnabled = false;
		configInput(KEY_INPUT, "Key root (V/OCT, any octave; overrides ROOT)");
		configInput(CHORD_INPUT, "Chord (poly V/OCT; the lowest note is the chord root)");
		configInput(BAR_INPUT, "Bar (key and chord changes wait for this trigger)");
		configInput(TRIG_INPUT, "Kick trigger (patch the same trigger as the kick: steps the pattern)");
		configInput(AUDIO_INPUT, "Kick audio (meter, AUTO, LEARN and UNDERTONE)");
		configInput(MUTATE_INPUT, "Mutate CV (+-5 V = +-50%)");
		configInput(LOCK_INPUT, "Lock gate (high = locked)");
		configInput(LEARN_INPUT, "Learn trigger");
		configOutput(VOCT_OUTPUT, "V/OCT to the kick's V/OCT input");
		configOutput(PITCH_OUTPUT, "Target pitch (V/OCT, 0 V = C4) for a bass voice or a kick in tracking mode");
		configOutput(INTUNE_OUTPUT, "In tune (10 V while the last hit heard was within 15 cents)");
		configOutput(SUB_OUTPUT, "Sub tone alone");
		configOutput(MIX_OUTPUT, "Kick with its low end swapped for the tuned sub");
		configBypass(AUDIO_INPUT, MIX_OUTPUT);
		ctlDiv.setDivision(32);
		eng.rng.seed((uint64_t) random::u64());
		readSettings();
		held = live;
		eng.trigger(held, true);
		voctOut = eng.voctTarget(held);
		pitchOut = eng.pitchVolts();
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setSampleRate(e.sampleRate); }

	bool locked() { return lockLatch || inputs[LOCK_INPUT].getVoltage() >= 1.f; }

	void readSettings() {
		if (inputs[KEY_INPUT].isConnected()) live.root = pcOfVolts(inputs[KEY_INPUT].getVoltage());
		else live.root = (int) params[ROOT_PARAM].getValue();
		live.scale = (int) params[SCALE_PARAM].getValue();
		live.followChord = params[FOLLOW_PARAM].getValue() > 0.5f;
		live.chordPc = -1;
		int ch = inputs[CHORD_INPUT].getChannels();
		if (ch > 0) {
			float lo = 1e9f;
			for (int c = 0; c < ch; c++) lo = std::min(lo, inputs[CHORD_INPUT].getVoltage(c));
			live.chordPc = pcOfVolts(lo);
		}
		live.octave = (int) params[OCT_PARAM].getValue();
		live.fineCents = params[FINE_PARAM].getValue();
		live.kickHz = std::exp2(params[KICK_PARAM].getValue());
		live.steps = (int) params[STEPS_PARAM].getValue();
		live.autoTune = params[AUTO_PARAM].getValue() > 0.5f;
	}

	// what the kick plays takes the live settings, but key and chord wait for BAR
	void hold(bool bar) {
		int root = held.root, chord = held.chordPc;
		held = live;
		if (inputs[BAR_INPUT].isConnected() && !bar) {
			held.root = root;
			held.chordPc = chord;
		}
	}

	void onHit() {
		hold(false);
		bool wrapped = eng.trigger(held, firstHit);
		firstHit = false;
		if (wrapped && !locked()) {
			float amt = clamp(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
			if (eng.pat.mutate(eng.rng, amt, params[SPREAD_PARAM].getValue())) mutated = true;
			eng.retarget(held);
		}
		eng.voctHit = eng.voctTarget(held);
	}

	void learn() {
		if (!eng.meter.have) return;
		float hz = clamp(eng.learnedKickHz(), 20.f, 200.f);
		params[KICK_PARAM].setValue(std::log2(hz));
		eng.corr = 0.f;
		learnFlash.trigger(0.3f);
	}

	void process(const ProcessArgs& args) override {
		if (ctlDiv.process()) readSettings();

		if (lockBtn.process(params[LOCK_PARAM].getValue() > 0.f)) lockLatch = !lockLatch;
		if (barIn.process(inputs[BAR_INPUT].getVoltage(), 0.1f, 1.f)) {
			hold(true);
			// with no kick trigger patched, a bar change shows straight away
			if (!inputs[TRIG_INPUT].isConnected()) eng.retarget(held);
		}

		bool audio = inputs[AUDIO_INPUT].isConnected();
		float x = audio ? inputs[AUDIO_INPUT].getVoltage() : 0.f;
		if (!std::isfinite(x)) x = 0.f;

		bool hit = false;
		if (inputs[TRIG_INPUT].isConnected()) {
			if (trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f)) hit = true;
		}
		else if (audio && eng.onsetDet.process(x)) hit = true;

		if (hit) {
			onHit();
			if (audio) {
				eng.meter.onset();
				eng.under.onset();
			}
		}
		else if (!inputs[TRIG_INPUT].isConnected() && !audio && ctlDiv.getClock() == 0) {
			// nothing to step on: follow the knobs and jacks directly
			hold(false);
			eng.retarget(held);
			eng.voctHit = eng.voctTarget(held);
		}

		if (audio) {
			if (eng.meter.process(x)) {
				eng.measured(held);
				inTune = eng.lastValid && std::fabs(eng.lastErrCents) < 15.f;
			}
			eng.under.setXover(std::exp2(params[XOVER_PARAM].getValue()));
			eng.under.process(x, eng.targetHz, params[REPLACE_PARAM].getValue(), params[DROP_PARAM].getValue(),
			                  params[DRIVE_PARAM].getValue(), params[SUBLVL_PARAM].getValue());
		}

		if (learnBtn.process(params[LEARN_PARAM].getValue() > 0.f)) learn();
		if (learnIn.process(inputs[LEARN_INPUT].getVoltage(), 0.1f, 1.f)) learn();

		// glide both outputs towards the held note
		float vt = eng.voctTarget(held), pt = eng.pitchVolts();
		float g = params[GLIDE_PARAM].getValue() * 0.5f;
		if (g < 1e-3f) { voctOut = vt; pitchOut = pt; }
		else {
			float k = 1.f - std::exp(-args.sampleTime / (g / 3.f));
			voctOut += k * (vt - voctOut);
			pitchOut += k * (pt - pitchOut);
		}
		eng.voctNow = voctOut;
		outputs[VOCT_OUTPUT].setVoltage(voctOut);
		outputs[PITCH_OUTPUT].setVoltage(pitchOut);
		outputs[INTUNE_OUTPUT].setVoltage(inTune ? 10.f : 0.f);
		outputs[SUB_OUTPUT].setVoltage(audio ? eng.under.sub : 0.f);
		outputs[MIX_OUTPUT].setVoltage(audio ? eng.under.mix : 0.f);

		lights[LEARN_LIGHT].setBrightnessSmooth(learnFlash.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
		lights[LOCK_LIGHT].setBrightness(locked() ? 1.f : 0.f);
	}

	void onReset() override {
		eng.pat.clear();
		eng.corr = 0.f;
		lockLatch = mutated = false;
		firstHit = true;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_t* a = json_array();
		for (int i = 0; i < MAXSTEPS; i++) json_array_append_new(a, json_integer(eng.pat.iv[i]));
		json_object_set_new(r, "pattern", a);
		json_object_set_new(r, "locked", json_boolean(lockLatch));
		json_object_set_new(r, "mutated", json_boolean(mutated));
		json_object_set_new(r, "autoCorrection", json_real(eng.corr));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* a = json_object_get(r, "pattern");
		if (a && json_is_array(a))
			for (int i = 0; i < MAXSTEPS && i < (int) json_array_size(a); i++) {
				int v = (int) json_integer_value(json_array_get(a, i));
				eng.pat.iv[i] = v >= 0 && v < NIV ? v : IV_ROOT;
			}
		json_t* l = json_object_get(r, "locked");
		if (l) lockLatch = json_boolean_value(l);
		json_t* m = json_object_get(r, "mutated");
		if (m) mutated = json_boolean_value(m);
		json_t* c = json_object_get(r, "autoCorrection");
		if (c) eng.corr = clamp((float) json_number_value(c), -2.f, 2.f);
	}
};

// ---------------------------------------------------------------- screen
struct KeelDisplay : OpaqueWidget {
	Keel* module = nullptr;
	static const float GX0, GY0;

	void cellRect(int k, float& x, float& y, float& w, float& h) {
		float gw = box.size.x - 2.f * mm2px(GX0);
		w = gw / MAXSTEPS;
		x = mm2px(GX0) + k * w + mm2px(0.25f);
		w -= mm2px(0.5f);
		y = mm2px(GY0);
		h = box.size.y - y - mm2px(1.3f);
	}

	// click a step: next interval; right-click... is the module menu, so Shift-click goes back
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			OpaqueWidget::onButton(e);
			return;
		}
		for (int k = 0; k < MAXSTEPS; k++) {
			float x, y, w, h;
			cellRect(k, x, y, w, h);
			if (e.pos.x >= x && e.pos.x < x + w && e.pos.y >= y && e.pos.y < y + h) {
				bool back = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
				int& v = module->eng.pat.iv[k];
				v = (v + (back ? NIV - 1 : 1)) % NIV;
				if (k == module->eng.pat.pos) module->eng.retarget(module->held);
				e.consume(this);
				return;
			}
		}
		OpaqueWidget::onButton(e);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			OpaqueWidget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		const NVGcolor TEAL = nvgRGB(0x3C, 0xCF, 0xB4), CREAM = nvgRGB(0xEF, 0xE8, 0xD8), CORAL = nvgRGB(0xFF, 0x6B, 0x4A);
		const NVGcolor AMBER = nvgRGB(0xFF, 0xB3, 0x41), RED = nvgRGB(0xFF, 0x4D, 0x5E), GREEN = nvgRGB(0x7D, 0xE2, 0x7F);
		const NVGcolor SOFTC = nvgRGB(0xA3, 0xB0, 0xB6), CELL = nvgRGBA(0x26, 0x32, 0x38, 160);
		nvgFontFaceId(vg, font->handle);

		std::string l1, r1, l2, r2;
		NVGcolor c2 = SOFTC;
		int iv[MAXSTEPS] = {};
		int len = 4, pos = 1;
		bool locked = false;
		if (module) {
			const keel::Settings& s = module->held;
			const keel::Engine& e = module->eng;
			l1 = string::f("KEY %s %s", NOTE_NAMES[s.root], scaleName(s.scale));
			if (s.followChord) l1 += s.chordPc >= 0 ? string::f("  CHORD %s", NOTE_NAMES[s.chordPc]) : "  CHORD -";
			int m = e.targetMidi;
			r1 = string::f("KICK > %s%d  %.1f Hz  V/OCT %+.2f", NOTE_NAMES[((m % 12) + 12) % 12], m / 12 - 1, e.targetHz, module->voctOut);
			if (!module->inputs[Keel::AUDIO_INPUT].isConnected()) l2 = "PATCH THE KICK'S AUDIO TO HEAR IT";
			else if (!e.meter.have) l2 = "LISTENING...";
			else {
				int hm; float hc;
				noteOf(e.meter.hz, hm, hc);
				l2 = string::f("HEARD %.1f Hz  %s%d %+.0f c", e.meter.hz, NOTE_NAMES[((hm % 12) + 12) % 12], hm / 12 - 1, hc);
				if (e.lastValid) {
					float err = e.lastErrCents;
					r2 = std::fabs(err) < 15.f ? "IN TUNE" : string::f("%s %.0f c", err > 0 ? "SHARP" : "FLAT", std::fabs(err));
					c2 = std::fabs(err) < 15.f ? GREEN : std::fabs(err) < 50.f ? AMBER : RED;
				}
			}
			if (s.autoTune) r2 += string::f("%sAUTO %+.0f c", r2.empty() ? "" : "  ", 1200.f * e.corr);
			for (int k = 0; k < MAXSTEPS; k++) iv[k] = e.pat.iv[k];
			len = std::max(1, std::min(MAXSTEPS, s.steps));
			pos = e.pat.pos;
			locked = module->locked();
		}
		else {
			l1 = "KEY A MINOR  CHORD F";
			r1 = "KICK > F1  43.7 Hz  V/OCT -0.18";
			l2 = "HEARD 49.3 Hz  G1 +10 c";
			r2 = "SHARP 208 c";
			c2 = RED;
			const int demo[4] = {IV_ROOT, IV_FIFTH, IV_ROOT, IV_OCTAVE};
			for (int k = 0; k < 4; k++) iv[k] = demo[k];
		}

		nvgFontSize(vg, mm2px(2.5f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, TEAL);
		nvgText(vg, mm2px(1.6f), mm2px(3.9f), l1.c_str(), nullptr);
		nvgFillColor(vg, AMBER);
		nvgText(vg, mm2px(1.6f), mm2px(7.9f), l2.c_str(), nullptr);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, CREAM);
		nvgText(vg, box.size.x - mm2px(1.6f), mm2px(3.9f), r1.c_str(), nullptr);
		nvgFillColor(vg, c2);
		nvgText(vg, box.size.x - mm2px(1.6f), mm2px(7.9f), r2.c_str(), nullptr);

		nvgFontSize(vg, mm2px(3.4f));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		for (int k = 0; k < MAXSTEPS; k++) {
			float x, y, w, h;
			cellRect(k, x, y, w, h);
			bool on = k < len;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x, y, w, h, mm2px(0.5f));
			nvgFillColor(vg, on ? CELL : nvgRGBA(0x26, 0x32, 0x38, 50));
			nvgFill(vg);
			if (on && iv[k] != IV_ROOT) {
				// bar height = how far from the root the step goes
				static const float hgt[NIV] = {0.f, 0.7f, 0.55f, 1.f, 0.4f, 0.85f};
				float hh = (h - mm2px(1.2f)) * hgt[iv[k]];
				nvgBeginPath(vg);
				nvgRect(vg, x + mm2px(0.4f), y + h - mm2px(0.6f) - hh, w - mm2px(0.8f), hh);
				nvgFillColor(vg, nvgTransRGBA(CORAL, 70));
				nvgFill(vg);
			}
			nvgFillColor(vg, on ? (locked ? SOFTC : CREAM) : nvgTransRGBA(SOFTC, 60));
			nvgText(vg, x + w / 2, y + h / 2, ivLabel(iv[k]), nullptr);
			if (on && k == pos) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x - mm2px(0.15f), y - mm2px(0.15f), w + mm2px(0.3f), h + mm2px(0.3f), mm2px(0.6f));
				nvgStrokeColor(vg, locked ? CORAL : nvgRGB(0xFF, 0xFF, 0xFF));
				nvgStrokeWidth(vg, mm2px(0.35f));
				nvgStroke(vg);
			}
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};
const float KeelDisplay::GX0 = 1.6f;
const float KeelDisplay::GY0 = 10.2f;

struct KeelWidget : ModuleWidget {
	KeelWidget(Keel* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Keel.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		KeelDisplay* d = createWidget<KeelDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		// row 1: key + tuning
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(0), ROW1_Y)), module, Keel::ROOT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(1), ROW1_Y)), module, Keel::SCALE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colX(2) - 4.5f, ROW1_Y)), module, Keel::FOLLOW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(3), ROW1_Y)), module, Keel::OCT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(4), ROW1_Y)), module, Keel::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(5), ROW1_Y)), module, Keel::GLIDE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(6), ROW1_Y)), module, Keel::KICK_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(colX(7), ROW1_Y)), module, Keel::LEARN_PARAM, Keel::LEARN_LIGHT));
		// row 2: pattern + undertone
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(0), ROW2_Y)), module, Keel::STEPS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(1), ROW2_Y)), module, Keel::MUTATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(2), ROW2_Y)), module, Keel::SPREAD_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(colX(3), ROW2_Y)), module, Keel::LOCK_PARAM, Keel::LOCK_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(4), ROW2_Y)), module, Keel::XOVER_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(5), ROW2_Y)), module, Keel::REPLACE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(6), ROW2_Y)), module, Keel::DROP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(7), ROW2_Y)), module, Keel::DRIVE_PARAM));
		// inputs
		for (int i = 0; i < Keel::INPUTS_LEN; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), IN_Y)), module, i));
		// outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(0), OUT_Y)), module, Keel::VOCT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(1), OUT_Y)), module, Keel::PITCH_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(2), OUT_Y)), module, Keel::INTUNE_OUTPUT));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colX(3) - 2.f, OUT_Y)), module, Keel::AUTO_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colX(5), OUT_Y + 1.0f)), module, Keel::SUBLVL_PARAM));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(6), OUT_Y)), module, Keel::SUB_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(7), OUT_Y)), module, Keel::MIX_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Keel* m = getModule<Keel>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Click a step: next interval; Shift-click: previous"));
		menu->addChild(createSubmenuItem("Kick's own pitch", "", [=](Menu* sub) {
			struct K { const char* name; float hz; };
			static const K ks[] = {{"808 Circuits / 909 Circuits stock (49.5 Hz)", 49.5f},
			                       {"TR-606 style (about 60 Hz)", 60.f},
			                       {"TR-707 style (about 58.5 Hz)", 58.5f}};
			for (const K& k : ks) {
				float hz = k.hz;
				sub->addChild(createMenuItem(k.name, "", [=]() {
					m->params[Keel::KICK_PARAM].setValue(std::log2(hz));
					m->eng.corr = 0.f;
				}));
			}
			sub->addChild(createMenuLabel("Other kicks: patch AUDIO, play a hit, press LEARN"));
		}));
		menu->addChild(createMenuItem("Re-roll the pattern now", "", [=]() {
			m->eng.pat.mutate(m->eng.rng, 1.f, m->params[Keel::SPREAD_PARAM].getValue());
			m->eng.retarget(m->held);
			m->mutated = true;
		}));
		menu->addChild(createMenuItem("Clear the pattern (all roots)", "", [=]() {
			m->eng.pat.clear();
			m->eng.retarget(m->held);
			m->mutated = false;
		}));
		menu->addChild(createMenuItem("Clear the AUTO correction", "", [=]() { m->eng.corr = 0.f; }));
	}
};

Model* modelKeel = createModel<Keel, KeelWidget>("Keel");
