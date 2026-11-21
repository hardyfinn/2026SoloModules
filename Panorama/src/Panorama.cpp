// Panorama: steel pan voice.
//   Every note area of a tenor, double second or triple cello pan is a set of
//   tuned modes that ring into their neighbours. GATE + V/OCT (polyphonic,
//   e.g. from Peal) strike the nearest note area; held gates can roll.
//   Tuning is deliberately imperfect: DETUNE and PARTIALS scale a fixed random
//   error per note (RE-ROLL for a different pan), and the FUND / OCT / 5TH
//   knobs hand-tune whichever note is selected on the screen.
// The sound engine is in PanoramaDSP.hpp.
#include "plugin.hpp"
#include "PanoramaDSP.hpp"
#include <atomic>

using namespace panorama;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 15.0f, DISP_W = 100.0f, DISP_H = 66.0f;
static const float KX[4] = {120.0f, 143.0f, 166.0f, 189.0f};
static const float ROW_Y[3] = {27.0f, 49.0f, 71.0f};
static const float ED_Y = 94.0f;
static const float ED_X[3] = {14.0f, 31.0f, 48.0f};
static const float ROLL_BTN_X = 64.0f, TYPE_X = 79.0f, TUNE_X = 98.0f;
static const float IN_Y = 116.5f;
static const float IN_X[5] = {12.0f, 30.5f, 49.0f, 67.5f, 86.0f};
static const float CV_Y[3] = {91.0f, 104.5f, 118.0f};

struct Panorama : Module {
	enum ParamId {
		STICK_PARAM, POSITION_PARAM, FORCE_PARAM, DECAY_PARAM,
		NEIGHBOURS_PARAM, BLOOM_PARAM, GLIDE_PARAM, DAMP_PARAM,
		DETUNE_PARAM, PARTIALS_PARAM, ROLL_PARAM, WIDTH_PARAM,
		FUND_PARAM, OCT_PARAM, FIFTH_PARAM,
		REROLL_PARAM, TYPE_PARAM, TUNE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, MUTE_INPUT, ROLL_INPUT,
		STICK_INPUT, POSITION_INPUT, NEIGHBOURS_INPUT, BLOOM_INPUT,
		DETUNE_INPUT, PARTIALS_INPUT, DECAY_INPUT, GLIDE_INPUT, DAMP_INPUT, ROLLRATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId { LEFT_OUTPUT, RIGHT_OUTPUT, OUTPUTS_LEN };
	enum LightId { REROLL_LIGHT, LIGHTS_LEN };

	PanEngine eng;
	Controls ctl;
	dsp::SchmittTrigger gateTrig[16], rerollTrig;
	// rolls: one per input channel
	bool held[16] = {};
	int heldNote[16];
	float heldForce[16];
	float rollT[16];
	int rollAlt[16] = {};
	float rerollFlash = 0.f;
	uint32_t rng = 0x2468ACE1u;

	// options
	bool bendMode = false;   // bend the area to the exact input pitch
	bool foldMode = true;    // fold notes outside the range by octaves (else ignore)

	// screen selection + hand tuning knobs
	int sel = 6;
	int selType = -1;
	float lastEdit[3] = {NAN, NAN, NAN};
	std::atomic<int> clickNote{-1};
	std::atomic<int> menuAction{0};  // 1 clear selected, 2 clear all, 3 re-roll, 4 perfect

	// screen copies
	float dispEnergy[MAX_NOTES] = {};
	float dispFund[MAX_NOTES] = {};
	float dispSel[3] = {};
	int dispType = 0, dispSel_ = 6;
	uint32_t dispSeed = 0;
	int dispCount = 0;
	float dispBendCents[MAX_NOTES] = {};

	Panorama() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(STICK_PARAM, 0.f, 1.f, 0.5f, "Stick", "%", 0.f, 100.f)->description = "Soft rubber (dark) to hard (bright, more click)";
		configParam(POSITION_PARAM, 0.f, 1.f, 0.08f, "Strike position", "%", 0.f, 100.f)->description = "Sweet spot in the middle of the note (round, the way a pan is normally played) to its edge (thinner and brighter, knocks the neighbours)";
		configParam(FORCE_PARAM, 0.f, 1.f, 0.7f, "Force", "%", 0.f, 100.f)->description = "How hard each strike is. With VEL patched, 7 V plays at this level";
		configParam(DECAY_PARAM, 0.f, 1.f, 0.45f, "Decay", "x", 0.f, 1.f)->description = "Ring time of every note";
		configParam(NEIGHBOURS_PARAM, 0.f, 1.f, 0.35f, "Neighbours", "%", 0.f, 100.f)->description = "How much the note areas around the struck one sing along";
		configParam(BLOOM_PARAM, 0.f, 1.f, 0.5f, "Bloom", "%", 0.f, 100.f)->description = "How strongly a hard hit drives the upper harmonics through the steel: the brightness that swells in just after the stick lands. Soft notes stay pure either way";
		configParam(GLIDE_PARAM, -30.f, 30.f, 15.f, "Onset glide", " cents")->description = "Hard hits start this far off pitch and settle. Real pans start sharp (positive)";
		configParam(DAMP_PARAM, 0.f, 1.f, 0.f, "Damp", "%", 0.f, 100.f)->description = "Hand resting on the pan: shorter, duller notes";
		configParam(DETUNE_PARAM, 0.f, 30.f, 4.f, "Detune (fundamentals)", " cents")->description = "Scale of each note's own random tuning error. 0 = mathematically exact";
		configParam(PARTIALS_PARAM, 0.f, 50.f, 8.f, "Partials (octave + fifth)", " cents")->description = "Scale of each note's random octave and fifth error. 0 = exactly harmonic";
		configParam(ROLL_PARAM, 0.f, 1.f, 0.f, "Roll", "")->description = "Off, or the speed held gates roll at (6 to 28 strikes a second)";
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.6f, "Stereo width", "%", 0.f, 100.f);
		configParam(FUND_PARAM, -50.f, 50.f, 0.f, "Selected note: fundamental", " cents")->description = "Hand tuning of the note picked on the screen (added to the random error)";
		configParam(OCT_PARAM, -50.f, 50.f, 0.f, "Selected note: octave", " cents");
		configParam(FIFTH_PARAM, -50.f, 50.f, 0.f, "Selected note: fifth (12th)", " cents");
		configButton(REROLL_PARAM, "Re-roll the random tuning (a different pan)");
		configSwitch(TYPE_PARAM, 0.f, 2.f, 2.f, "Pan", {"Triple cello", "Double second", "Tenor"});
		configParam(TUNE_PARAM, -100.f, 100.f, 0.f, "Tune (whole pan)", " cents");
		getParamQuantity(TYPE_PARAM)->snapEnabled = true;

		configInput(VOCT_INPUT, "Pitch (V/OCT, polyphonic; 0 V = C4)");
		configInput(GATE_INPUT, "Gate / trigger (polyphonic)");
		configInput(VEL_INPUT, "Velocity (0-10 V, polyphonic)");
		configInput(MUTE_INPUT, "Palm mute (gate)");
		configInput(ROLL_INPUT, "Roll (gate: held notes roll while high)");
		configInput(STICK_INPUT, "Stick CV");
		configInput(POSITION_INPUT, "Position CV");
		configInput(NEIGHBOURS_INPUT, "Neighbours CV");
		configInput(BLOOM_INPUT, "Bloom CV");
		configInput(DETUNE_INPUT, "Detune CV");
		configInput(PARTIALS_INPUT, "Partials CV");
		configInput(DECAY_INPUT, "Decay CV");
		configInput(GLIDE_INPUT, "Glide CV");
		configInput(DAMP_INPUT, "Damp CV");
		configInput(ROLLRATE_INPUT, "Roll speed CV");
		configOutput(LEFT_OUTPUT, "Left (mono when R is unpatched)");
		configOutput(RIGHT_OUTPUT, "Right");
		for (int c = 0; c < 16; c++) { heldNote[c] = 0; heldForce[c] = 0.f; rollT[c] = 0.f; }
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.setSampleRate(e.sampleRate); }
	void onReset() override {
		eng.tun.clearHand();
		eng.tun.roll(20261120u);
		bendMode = false;
		foldMode = true;
		syncKnobs();
	}

	float rnd() {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
		return (rng >> 8) * (1.f / 16777216.f);
	}

	int typeFromSwitch() { return 2 - (int) std::round(params[TYPE_PARAM].getValue()); }

	// put the selected note's hand tuning on the three knobs
	void syncKnobs() {
		int t = eng.lay.type;
		sel = clamp(sel, 0, eng.lay.count - 1);
		for (int k = 0; k < 3; k++) {
			float v = eng.tun.hand[t][sel][k];
			params[FUND_PARAM + k].setValue(v);
			lastEdit[k] = v;
		}
	}

	float cv(int in, float scale) { return inputs[in].isConnected() ? inputs[in].getVoltage() * scale : 0.f; }

	// note area for a pitch (in MIDI); -1 if ignored
	int mapPitch(float midi, float& bendCents) {
		const TypeSpec& T = TYPES[eng.lay.type];
		bendCents = 0.f;
		if (midi < T.lowMidi - 0.5f || midi > T.highMidi + 0.5f) {
			if (!foldMode) return -1;
			while (midi < T.lowMidi - 0.5f) midi += 12.f;
			while (midi > T.highMidi + 0.5f) midi -= 12.f;
		}
		int i = eng.lay.nearest(midi);
		bendCents = (midi - eng.lay.n[i].midi) * 100.f;
		return i;
	}

	void hit(int i, float force, float bendCents) {
		if (i < 0) return;
		float b = bendMode ? std::pow(2.f, bendCents / 1200.f) : 1.f;
		if (eng.bend[i] != b) {
			eng.bend[i] = b;
			eng.updateNote(i);
		}
		eng.strike(i, force);
	}

	void process(const ProcessArgs& args) override {
		// ---- controls
		ctl.type = typeFromSwitch();
		ctl.stick = clamp(params[STICK_PARAM].getValue() + cv(STICK_INPUT, 0.1f), 0.f, 1.f);
		ctl.position = clamp(params[POSITION_PARAM].getValue() + cv(POSITION_INPUT, 0.1f), 0.f, 1.f);
		float dk = clamp(params[DECAY_PARAM].getValue() + cv(DECAY_INPUT, 0.1f), 0.f, 1.f);
		ctl.decay = 0.25f * std::pow(10.f, dk);  // 0.25x .. 2.5x (0.45 = about 0.7x)
		ctl.decay = std::min(ctl.decay, 2.5f);
		ctl.neighbours = clamp(params[NEIGHBOURS_PARAM].getValue() + cv(NEIGHBOURS_INPUT, 0.1f), 0.f, 1.f);
		ctl.bloom = clamp(params[BLOOM_PARAM].getValue() + cv(BLOOM_INPUT, 0.1f), 0.f, 1.f);
		ctl.glide = clamp(params[GLIDE_PARAM].getValue() + cv(GLIDE_INPUT, 6.f), -30.f, 30.f);
		ctl.damp = clamp(params[DAMP_PARAM].getValue() + cv(DAMP_INPUT, 0.1f), 0.f, 1.f);
		ctl.mute = inputs[MUTE_INPUT].getVoltage() > 1.f ? 1.f : 0.f;
		ctl.detune = clamp(params[DETUNE_PARAM].getValue() + cv(DETUNE_INPUT, 3.f), 0.f, 30.f);
		ctl.partials = clamp(params[PARTIALS_PARAM].getValue() + cv(PARTIALS_INPUT, 5.f), 0.f, 50.f);
		ctl.tune = params[TUNE_PARAM].getValue();
		ctl.width = params[WIDTH_PARAM].getValue();

		// ---- pan type change: new layout, keep selection in range
		if (ctl.type != eng.lay.type) {
			eng.update(ctl);
			sel = clamp(sel, 0, eng.lay.count - 1);
		}
		if (selType != eng.lay.type || std::isnan(lastEdit[0])) {
			selType = eng.lay.type;
			syncKnobs();
		}

		// ---- screen clicks + menu actions (from the UI thread)
		int ck = clickNote.exchange(-1);
		if (ck >= 0 && ck < eng.lay.count) {
			sel = ck;
			syncKnobs();
			eng.update(ctl);
			hit(ck, params[FORCE_PARAM].getValue(), 0.f);
		}
		int act = menuAction.exchange(0);
		if (act == 1) { for (int k = 0; k < 3; k++) eng.tun.hand[eng.lay.type][sel][k] = 0.f; syncKnobs(); eng.fullDirty = true; }
		if (act == 2) { eng.tun.clearHand(); syncKnobs(); eng.fullDirty = true; }
		if (act == 3 || rerollTrig.process(params[REROLL_PARAM].getValue())) {
			eng.tun.roll((uint32_t) (rnd() * 4.0e9f) + 1u);
			eng.fullDirty = true;
			rerollFlash = 0.25f;
		}
		if (act == 4) {
			params[DETUNE_PARAM].setValue(0.f);
			params[PARTIALS_PARAM].setValue(0.f);
			eng.tun.clearHand();
			syncKnobs();
			eng.fullDirty = true;
		}

		// ---- hand tuning knobs edit the selected note
		for (int k = 0; k < 3; k++) {
			float v = params[FUND_PARAM + k].getValue();
			if (v != lastEdit[k]) {
				eng.tun.hand[eng.lay.type][sel][k] = v;
				lastEdit[k] = v;
				eng.fullDirty = true;
			}
		}

		// controls reach the engine on its next 16-sample block
		eng.target = ctl;

		// ---- strikes
		int ch = std::max(inputs[GATE_INPUT].getChannels(), inputs[VOCT_INPUT].getChannels());
		if (!inputs[GATE_INPUT].isConnected()) ch = 0;
		float rollK = params[ROLL_PARAM].getValue() + cv(ROLLRATE_INPUT, 0.1f);
		bool rollGate = inputs[ROLL_INPUT].getVoltage() > 1.f;
		bool rolling = rollK > 0.01f || rollGate;
		float rollHz = rollK > 0.01f ? 6.f + 22.f * clamp(rollK, 0.f, 1.f) : 14.f;
		if (inputs[ROLL_INPUT].isConnected() && !rollGate) rolling = false;
		bool anyStrike = false;
		for (int c = 0; c < ch; c++) {
			float g = inputs[GATE_INPUT].getPolyVoltage(c);
			bool rise = gateTrig[c].process(g, 0.1f, 1.f);
			if (rise) {
				if (!anyStrike) { eng.update(ctl); anyStrike = true; }  // strike with the current knobs
				float bc = 0.f;
				int i;
				if (inputs[VOCT_INPUT].isConnected()) i = mapPitch(60.f + 12.f * inputs[VOCT_INPUT].getPolyVoltage(c), bc);
				else i = sel;
				float f = params[FORCE_PARAM].getValue();
				if (inputs[VEL_INPUT].isConnected()) f *= clamp(inputs[VEL_INPUT].getPolyVoltage(c), 0.f, 10.f) / 7.f;
				f = clamp(f, 0.f, 1.25f);
				hit(i, f, bc);
				held[c] = i >= 0;
				heldNote[c] = i;
				heldForce[c] = f;
				rollT[c] = 1.f / rollHz;
				rollAlt[c] = 0;
			}
			if (!gateTrig[c].isHigh()) held[c] = false;
			if (held[c] && rolling) {
				rollT[c] -= args.sampleTime;
				if (rollT[c] <= 0.f) {
					rollAlt[c] ^= 1;
					// two sticks: alternate hands a little uneven, rolls sit under the first hit
					float f = heldForce[c] * 0.72f * (rollAlt[c] ? 0.86f : 1.f) * (0.94f + 0.12f * rnd());
					eng.strike(heldNote[c], f, 0.5f);
					rollT[c] += (1.f / rollHz) * (0.9f + 0.2f * rnd());
				}
			}
		}
		for (int c = ch; c < 16; c++) held[c] = false;

		// ---- audio
		float L, R;
		eng.process(L, R);
		if (outputs[RIGHT_OUTPUT].isConnected()) {
			outputs[LEFT_OUTPUT].setVoltage(L);
			outputs[RIGHT_OUTPUT].setVoltage(R);
		}
		else
			outputs[LEFT_OUTPUT].setVoltage(0.5f * (L + R) * 1.2f);

		rerollFlash = std::max(0.f, rerollFlash - args.sampleTime);
		lights[REROLL_LIGHT].setBrightnessSmooth(rerollFlash > 0.f ? 1.f : 0.f, args.sampleTime);

		// ---- screen copies (cheap, every 256 samples)
		if (++dispCounter >= 256) {
			dispCounter = 0;
			int t = eng.lay.type;
			dispType = t;
			dispCount = eng.lay.count;
			dispSel_ = sel;
			dispSeed = eng.tun.seed;
			for (int i = 0; i < eng.lay.count; i++) {
				dispEnergy[i] = eng.energy[i] * (eng.active[i] ? 1.f : 0.f);
				dispFund[i] = eng.errCents(t, i, 0, eng.cur);
				dispBendCents[i] = 1200.f * std::log2(eng.bend[i]);
			}
			for (int k = 0; k < 3; k++) dispSel[k] = eng.errCents(t, sel, k, eng.cur);
		}
	}

	int dispCounter = 0;

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer(eng.tun.seed));
		json_object_set_new(root, "bend", json_boolean(bendMode));
		json_object_set_new(root, "fold", json_boolean(foldMode));
		json_object_set_new(root, "selected", json_integer(sel));
		json_t* h = json_array();
		for (int t = 0; t < NUM_TYPES; t++)
			for (int i = 0; i < MAX_NOTES; i++)
				for (int k = 0; k < 3; k++) json_array_append_new(h, json_real(eng.tun.hand[t][i][k]));
		json_object_set_new(root, "hand", h);
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "seed"))) eng.tun.roll((uint32_t) json_integer_value(j));
		if ((j = json_object_get(root, "bend"))) bendMode = json_boolean_value(j);
		if ((j = json_object_get(root, "fold"))) foldMode = json_boolean_value(j);
		if ((j = json_object_get(root, "selected"))) sel = (int) json_integer_value(j);
		json_t* h = json_object_get(root, "hand");
		if (h && json_is_array(h) && json_array_size(h) == (size_t) (NUM_TYPES * MAX_NOTES * 3)) {
			size_t q = 0;
			for (int t = 0; t < NUM_TYPES; t++)
				for (int i = 0; i < MAX_NOTES; i++)
					for (int k = 0; k < 3; k++) eng.tun.hand[t][i][k] = (float) json_number_value(json_array_get(h, q++));
		}
		eng.fullDirty = true;
		lastEdit[0] = NAN;  // re-sync knobs on the next sample
	}
};

// ---- screen: the drum face(s), note areas light up as they ring ----
struct PanDisplay : Widget {
	Panorama* module = nullptr;
	Layout demo;
	PanDisplay() { demo.build(TENOR); }

	// geometry of pan p (of n) inside the screen
	void panGeom(int nPans, int p, float& cx, float& cy, float& R) {
		float w = box.size.x, h = box.size.y - mm2px(6.f);
		if (nPans == 1) { R = std::min(w, h) * 0.47f; cx = w * 0.5f; cy = h * 0.5f + mm2px(0.6f); }
		else if (nPans == 2) { R = std::min(w / 4.f, h / 2.f) * 0.94f; cx = w * (p == 0 ? 0.26f : 0.74f); cy = h * 0.5f + mm2px(0.6f); }
		else { R = std::min(w / 6.f, h / 2.f) * 0.95f; cx = w * (0.17f + 0.33f * p); cy = h * 0.5f + mm2px(0.6f) + (p == 1 ? -mm2px(2.f) : mm2px(2.f)); }
	}

	const Layout& layout() {
		if (module && module->dispType != demo.type) demo.build(module->dispType);
		return demo;
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			const Layout& L = layout();
			int nP = TYPES[L.type].numPans;
			int best = -1;
			float bd = 1e9f;
			for (int i = 0; i < L.count; i++) {
				float cx, cy, R;
				panGeom(nP, L.n[i].pan, cx, cy, R);
				float x = cx + L.n[i].px * R, y = cy + L.n[i].py * R;
				float d = std::hypot(e.pos.x - x, e.pos.y - y) / (L.n[i].size * R);
				if (d < bd) { bd = d; best = i; }
			}
			if (best >= 0 && bd < 1.4f) module->clickNote.store(best);
			e.consume(this);
		}
		Widget::onButton(e);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		const Layout& L = layout();
		const NVGcolor steel = nvgRGB(168, 176, 184), ink = nvgRGB(241, 233, 214), gold = nvgRGB(255, 196, 64);
		const NVGcolor sharp = nvgRGB(255, 120, 90), flat = nvgRGB(110, 180, 255);
		int nP = TYPES[L.type].numPans;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		// pan rims
		for (int p = 0; p < nP; p++) {
			float cx, cy, R;
			panGeom(nP, p, cx, cy, R);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, R);
			NVGpaint pt = nvgRadialGradient(args.vg, cx - R * 0.3f, cy - R * 0.35f, R * 0.1f, R * 1.1f, nvgRGBA(70, 76, 84, 255), nvgRGBA(26, 28, 32, 255));
			nvgFillPaint(args.vg, pt);
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, nvgTransRGBA(steel, 150));
			nvgStrokeWidth(args.vg, 1.2f);
			nvgStroke(args.vg);
		}
		int sel = module ? module->dispSel_ : 7;
		for (int i = 0; i < L.count; i++) {
			const NoteArea& a = L.n[i];
			float cx, cy, R;
			panGeom(nP, a.pan, cx, cy, R);
			float x = cx + a.px * R, y = cy + a.py * R;
			float r = a.size * R;
			float en = module ? module->dispEnergy[i] : (i == 7 ? 0.3f : 0.f);
			float glow = clamp(std::sqrt(en) * 1.6f, 0.f, 1.f);
			// note areas are slightly oval, long axis pointing at the centre
			float ang = std::atan2(a.py, a.px);
			nvgSave(args.vg);
			nvgTranslate(args.vg, x, y);
			nvgRotate(args.vg, ang);
			nvgBeginPath(args.vg);
			nvgEllipse(args.vg, 0, 0, r * 1.05f, r * 0.82f);
			nvgFillColor(args.vg, nvgLerpRGBA(nvgRGBA(54, 58, 64, 255), gold, glow));
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, i == sel ? gold : nvgTransRGBA(steel, 120));
			nvgStrokeWidth(args.vg, i == sel ? 1.6f : 0.7f);
			nvgStroke(args.vg);
			nvgRestore(args.vg);
			if (font) {
				nvgFontFaceId(args.vg, font->handle);
				float fsz = clamp(r * 0.7f, mm2px(1.7f), mm2px(2.8f));
				nvgFontSize(args.vg, fsz);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFillColor(args.vg, glow > 0.55f ? nvgRGB(30, 24, 10) : ink);
				std::string name = pcName(a.midi);
				nvgText(args.vg, x, y - fsz * 0.12f, name.c_str(), NULL);
				// tuning mark: a short bar, right = sharp, left = flat
				float dev = module ? module->dispFund[i] + module->dispBendCents[i] : 0.f;
				if (std::fabs(dev) > 0.5f) {
					float bw = clamp(dev / 30.f, -1.f, 1.f) * r * 0.75f;
					nvgBeginPath(args.vg);
					nvgRect(args.vg, std::min(x, x + bw), y + fsz * 0.48f, std::fabs(bw), std::max(1.f, fsz * 0.14f));
					nvgFillColor(args.vg, dev > 0 ? sharp : flat);
					nvgFill(args.vg);
				}
			}
		}
		// status line
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.9f));
			float by = box.size.y - mm2px(1.2f);
			int t = module ? module->dispType : 0;
			int s = clamp(sel, 0, L.count - 1);
			int midi = L.n[s].midi;
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, gold);
			std::string left = string::f("%s  %s%d", TYPE_NAMES[t], pcName(midi), midi / 12 - 1);
			nvgText(args.vg, mm2px(1.4f), by, left.c_str(), NULL);
			float d0 = module ? module->dispSel[0] : 2.1f, d1 = module ? module->dispSel[1] : -3.4f, d2 = module ? module->dispSel[2] : 6.0f;
			std::string right = string::f("FUND %+.1f  OCT %+.1f  5TH %+.1f c", d0, d1, d2);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, ink);
			nvgText(args.vg, box.size.x - mm2px(1.4f), by, right.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct PanoramaWidget : ModuleWidget {
	PanoramaWidget(Panorama* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Panorama.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PanDisplay* d = createWidget<PanDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		const int grid[3][4] = {
			{Panorama::STICK_PARAM, Panorama::POSITION_PARAM, Panorama::FORCE_PARAM, Panorama::DECAY_PARAM},
			{Panorama::NEIGHBOURS_PARAM, Panorama::BLOOM_PARAM, Panorama::GLIDE_PARAM, Panorama::DAMP_PARAM},
			{Panorama::DETUNE_PARAM, Panorama::PARTIALS_PARAM, Panorama::ROLL_PARAM, Panorama::WIDTH_PARAM},
		};
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 4; c++)
				addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(KX[c], ROW_Y[r])), module, grid[r][c]));

		for (int k = 0; k < 3; k++)
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(ED_X[k], ED_Y)), module, Panorama::FUND_PARAM + k));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(ROLL_BTN_X, ED_Y)), module, Panorama::REROLL_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(ROLL_BTN_X + 4.6f, ED_Y - 4.2f)), module, Panorama::REROLL_LIGHT));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(TYPE_X, ED_Y)), module, Panorama::TYPE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(TUNE_X, ED_Y)), module, Panorama::TUNE_PARAM));

		const int ins[5] = {Panorama::VOCT_INPUT, Panorama::GATE_INPUT, Panorama::VEL_INPUT, Panorama::MUTE_INPUT, Panorama::ROLL_INPUT};
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_X[i], IN_Y)), module, ins[i]));

		const int cvs[3][4] = {
			{Panorama::STICK_INPUT, Panorama::POSITION_INPUT, Panorama::DECAY_INPUT, Panorama::GLIDE_INPUT},
			{Panorama::NEIGHBOURS_INPUT, Panorama::BLOOM_INPUT, Panorama::DAMP_INPUT, Panorama::ROLLRATE_INPUT},
			{Panorama::DETUNE_INPUT, Panorama::PARTIALS_INPUT, -1, -1},
		};
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 4; c++)
				if (cvs[r][c] >= 0) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(KX[c], CV_Y[r])), module, cvs[r][c]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(KX[2], CV_Y[2])), module, Panorama::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(KX[3], CV_Y[2])), module, Panorama::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Panorama* module = dynamic_cast<Panorama*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Pitch", {"Snap to the nearest note area", "Bend the note area to the exact pitch"},
			[=]() { return module->bendMode ? 1 : 0; },
			[=](int i) { module->bendMode = i == 1; }));
		menu->addChild(createIndexSubmenuItem("Notes outside the pan's range", {"Fold in by octaves", "Ignore"},
			[=]() { return module->foldMode ? 0 : 1; },
			[=](int i) { module->foldMode = i == 0; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Tuning"));
		menu->addChild(createMenuItem("Re-roll the random tuning (a different pan)", "", [=]() { module->menuAction.store(3); }));
		menu->addChild(createMenuItem("Clear hand tuning of the selected note", "", [=]() { module->menuAction.store(1); }));
		menu->addChild(createMenuItem("Clear hand tuning of all notes", "", [=]() { module->menuAction.store(2); }));
		menu->addChild(createMenuItem("Perfect tuning (DETUNE + PARTIALS to 0, clear hand tuning)", "", [=]() { module->menuAction.store(4); }));
	}
};

Model* modelPanorama = createModel<Panorama, PanoramaWidget>("Panorama");
