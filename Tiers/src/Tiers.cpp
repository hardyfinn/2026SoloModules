// Tiers: gate patterns built from how often each metric level plays.
#include "plugin.hpp"
#include "TiersEngine.hpp"

using namespace tiers;

// ---- LAYOUT (same numbers as res/make_panel.py)
static const float DISP_X = 6.0f, DISP_Y = 13.5f, DISP_W = 140.4f, DISP_H = 23.0f;
static const float COL0 = 18.0f, COLDX = 16.6f;
static const float KNOB_Y = 51.5f, CV_Y = 63.5f, ROW3_Y = 82.5f, IN_Y = 100.5f, OUT_Y = 117.5f;
static float colX(int i) { return COL0 + COLDX * i; }

static const char* LEVEL_NAMES[LEVELS] = {"1/2", "1/4", "1/8", "1/16", "1/32"};

struct Tiers : Module {
	enum ParamId {
		ENUMS(W_PARAM, LEVELS),
		HITS_PARAM, ACCENT_PARAM, ROTATE_PARAM,
		GEN_PARAM, MUTATE_PARAM, LOCK_PARAM, BACK_PARAM, PRESET_PARAM, LOAD_PARAM,
		MODE_PARAM, LENGTH_PARAM, SWING_PARAM, RES_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(W_INPUT, LEVELS),
		HITS_INPUT, ACCENT_INPUT, ROTATE_INPUT,
		CLOCK_INPUT, RESET_INPUT, GEN_INPUT, MUTATE_INPUT, LOCK_INPUT, BACK_INPUT, PRESET_INPUT, LOAD_INPUT,
		INPUTS_LEN
	};
	enum OutputId { GATE_OUTPUT, TRIG_OUTPUT, INV_OUTPUT, ACC_OUTPUT, LEVEL_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
	enum LightId { GEN_LIGHT, LOCK_LIGHT, BACK_LIGHT, LOAD_LIGHT, LIGHTS_LEN };

	tiers::Engine eng;
	tiers::Settings settings;
	tiers::Result res;         // pattern as it plays now (read by the screen)
	int numPresets = 0;
	const tiers::Preset* presetList = nullptr;
	int loaded = -1;    // preset last loaded (-1 = generated)
	bool edited = false, mutated = false;
	bool lockLatch = false;
	float gateWidth = 0.5f;  // fraction of a step

	dsp::SchmittTrigger clockTrig, resetTrig, genTrig, backTrig, loadTrig;
	dsp::BooleanTrigger genBtn, lockBtn, backBtn, loadBtn;
	dsp::PulseGenerator trigPulse, eocPulse, genFlash, backFlash, loadFlash;
	dsp::ClockDivider evalDiv;

	int step = -1;            // step now playing, -1 = before the first clock
	bool resetPending = false;
	double sinceClock = 0.0;  // seconds since the last clock edge
	double period = 0.125;    // measured clock period
	bool clocked = false;
	// what this step plays (times in seconds from the clock edge)
	bool stepHit = false, stepAcc = false, stepRat = false;
	double startA = 0.0, startB = -1.0, gateLen = 0.0, gateLenB = 0.0;
	bool firedA = false, firedB = false;
	float levelCv = 0.f;

	Tiers() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		presetList = presets(numPresets);
		const float defW[LEVELS] = {0.9f, 0.5f, 0.3f, 0.15f, 0.f};
		for (int l = 0; l < LEVELS; l++) {
			configParam(W_PARAM + l, 0.f, 1.f, defW[l], string::f("How often %s steps play", LEVEL_NAMES[l]), "%", 0.f, 100.f);
			configInput(W_INPUT + l, string::f("%s weight CV (+-5 V = +-50%%)", LEVEL_NAMES[l]));
		}
		getParamQuantity(W_PARAM + 4)->description = "On the 1/16 grid: chance that a hit doubles into two 32nd notes";
		configParam(HITS_PARAM, 0.f, 32.f, 6.f, "Hits (COUNT mode)")->snapEnabled = true;
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "Accents", "%", 0.f, 100.f);
		configParam(ROTATE_PARAM, 0.f, 31.f, 0.f, "Rotate", " steps")->snapEnabled = true;
		configButton(GEN_PARAM, "Generate a new pattern");
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (re-roll steps at each loop end)", "%", 0.f, 100.f);
		configButton(LOCK_PARAM, "Lock (stop mutation)");
		configButton(BACK_PARAM, "Back to the last generated or loaded pattern");
		configParam(PRESET_PARAM, 0.f, (float) (numPresets - 1), 0.f, "Preset")->snapEnabled = true;
		configButton(LOAD_PARAM, "Load preset");
		configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Mode", {"COUNT (exactly HITS steps)", "CHANCE (weights are probabilities)"});
		configSwitch(LENGTH_PARAM, 0.f, 1.f, 0.f, "Steps", {"16", "32"});
		configParam(SWING_PARAM, 0.5f, 0.75f, 0.5f, "Swing", "%", 0.f, 100.f);
		configSwitch(RES_PARAM, 0.f, 1.f, 0.f, "Step size", {"1/16 (1/32 = doubles)", "1/32"});
		configInput(HITS_INPUT, "Hits CV (+-5 V = +-16)");
		configInput(ACCENT_INPUT, "Accent CV (+-5 V = +-50%)");
		configInput(ROTATE_INPUT, "Rotate CV (0-10 V = whole length)");
		configInput(CLOCK_INPUT, "Clock (one pulse per step)");
		configInput(RESET_INPUT, "Reset");
		configInput(GEN_INPUT, "Generate trigger");
		configInput(MUTATE_INPUT, "Mutate CV (+-5 V = +-50%)");
		configInput(LOCK_INPUT, "Lock gate (high = locked)");
		configInput(BACK_INPUT, "Back trigger");
		configInput(PRESET_INPUT, "Preset CV (0-10 V = whole list)");
		configInput(LOAD_INPUT, "Load preset trigger");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(TRIG_OUTPUT, "Trigger");
		configOutput(INV_OUTPUT, "Inverted gate (the steps that rest)");
		configOutput(ACC_OUTPUT, "Accent gate");
		configOutput(LEVEL_OUTPUT, "Level of the last hit (1/2 = 10 V, 1/4 = 8, 1/8 = 6, 1/16 = 4, 1/32 = 2)");
		configOutput(EOC_OUTPUT, "End of cycle");
		evalDiv.setDivision(64);
		eng.rng.seed((uint64_t) random::u64());
		eng.generate();
		readSettings();
		eng.evaluate(settings, res);
	}

	static float clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

	int presetIndex() {
		float v = params[PRESET_PARAM].getValue() + inputs[PRESET_INPUT].getVoltage() / 10.f * numPresets;
		int i = (int) std::floor(v + 0.5f);
		return i < 0 ? 0 : i >= numPresets ? numPresets - 1 : i;
	}

	bool locked() { return lockLatch || inputs[LOCK_INPUT].getVoltage() >= 1.f; }

	void readSettings() {
		for (int l = 0; l < LEVELS; l++)
			settings.w[l] = clamp01(params[W_PARAM + l].getValue() + inputs[W_INPUT + l].getVoltage() * 0.1f);
		settings.count = params[MODE_PARAM].getValue() < 0.5f;
		settings.length = params[LENGTH_PARAM].getValue() > 0.5f ? 32 : 16;
		settings.res32 = params[RES_PARAM].getValue() > 0.5f;
		float h = params[HITS_PARAM].getValue() + inputs[HITS_INPUT].getVoltage() * 3.2f;
		settings.hits = (int) std::floor(h + 0.5f);
		settings.accent = clamp01(params[ACCENT_PARAM].getValue() + inputs[ACCENT_INPUT].getVoltage() * 0.1f);
		float rot = params[ROTATE_PARAM].getValue() + inputs[ROTATE_INPUT].getVoltage() / 10.f * settings.length;
		int r = (int) std::floor(rot + 0.5f);
		settings.rotate = ((r % settings.length) + settings.length) % settings.length;
	}

	void generate() {
		eng.generate(settings.res32);
		loaded = -1;
		edited = mutated = false;
		genFlash.trigger(0.15f);
	}

	void loadPreset(int i) {
		tiers::Settings s = settings;
		eng.loadPreset(presetList[i], s);
		for (int l = 0; l < LEVELS; l++) params[W_PARAM + l].setValue(s.w[l]);
		params[HITS_PARAM].setValue((float) s.hits);
		params[ACCENT_PARAM].setValue(s.accent);
		params[ROTATE_PARAM].setValue(0.f);
		params[MODE_PARAM].setValue(1.f);
		params[LENGTH_PARAM].setValue(s.length == 32 ? 1.f : 0.f);
		params[RES_PARAM].setValue(0.f);
		loaded = i;
		edited = mutated = false;
		loadFlash.trigger(0.15f);
	}

	void back() {
		eng.back();
		mutated = false;
		backFlash.trigger(0.15f);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		lockLatch = false;
		generate();
		step = -1;
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		generate();
	}

	void startStep(const ProcessArgs& args) {
		eng.evaluate(settings, res);
		int k = step;
		stepHit = res.hit[k];
		stepAcc = res.acc[k];
		stepRat = res.rat[k];
		// Swing pushes every second step later (straight at 50%, triplet feel near 67%).
		float swing = params[SWING_PARAM].getValue();
		startA = (k % 2 == 1) ? (swing - 0.5) * 2.0 * period : 0.0;
		double stepLeft = period - startA;
		if (stepRat) {
			startB = startA + stepLeft * 0.5;
			gateLen = gateLenB = std::max(0.001, stepLeft * 0.5 * std::min(gateWidth, 0.6f));
		}
		else {
			startB = -1.0;
			gateLen = std::max(0.001, stepLeft * gateWidth);
		}
		firedA = firedB = false;
		if (stepHit) levelCv = 10.f - 2.f * res.level[k];
	}

	void process(const ProcessArgs& args) override {
		readSettings();

		// buttons and trigger inputs
		bool gen = genBtn.process(params[GEN_PARAM].getValue() > 0.f) | genTrig.process(inputs[GEN_INPUT].getVoltage(), 0.1f, 1.f);
		if (gen) generate();
		if (lockBtn.process(params[LOCK_PARAM].getValue() > 0.f)) lockLatch = !lockLatch;
		if (backBtn.process(params[BACK_PARAM].getValue() > 0.f) | backTrig.process(inputs[BACK_INPUT].getVoltage(), 0.1f, 1.f)) back();
		if (loadBtn.process(params[LOAD_PARAM].getValue() > 0.f) | loadTrig.process(inputs[LOAD_INPUT].getVoltage(), 0.1f, 1.f)) {
			loadPreset(presetIndex());
			readSettings();
		}
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) resetPending = true;

		sinceClock += args.sampleTime;
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (clocked && sinceClock > 1e-3 && sinceClock < 4.0) period = sinceClock;
			clocked = true;
			sinceClock = 0.0;
			int len = settings.length;
			if (resetPending || step < 0) {
				step = 0;
				resetPending = false;
			}
			else {
				step = (step + 1) % len;
				if (step == 0) {
					float m = clamp01(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() * 0.1f);
					if (m > 0.f && !locked() && eng.mutate(m, settings) > 0) mutated = true;
				}
			}
			if (step >= len) step = 0;
			if (step == 0) eocPulse.trigger(1e-3f);
			startStep(args);
		}
		else if (evalDiv.process()) {
			eng.evaluate(settings, res);  // keep the screen live while knobs move
		}

		// gates for this step
		bool gate = false;
		if (step >= 0 && stepHit) {
			double t = sinceClock;
			if (t >= startA && t < startA + gateLen) gate = true;
			if (startB >= 0.0 && t >= startB && t < startB + gateLenB) gate = true;
			if (!firedA && t >= startA) { firedA = true; trigPulse.trigger(1e-3f); }
			if (startB >= 0.0 && !firedB && t >= startB) { firedB = true; trigPulse.trigger(1e-3f); }
		}
		bool inv = step >= 0 && !stepHit && sinceClock < period * gateWidth;
		outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f);
		outputs[TRIG_OUTPUT].setVoltage(trigPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[INV_OUTPUT].setVoltage(inv ? 10.f : 0.f);
		outputs[ACC_OUTPUT].setVoltage(gate && stepAcc ? 10.f : 0.f);
		outputs[LEVEL_OUTPUT].setVoltage(levelCv);
		outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

		lights[GEN_LIGHT].setBrightness(genFlash.process(args.sampleTime) ? 1.f : 0.f);
		lights[BACK_LIGHT].setBrightness(backFlash.process(args.sampleTime) ? 1.f : 0.f);
		lights[LOAD_LIGHT].setBrightness(loadFlash.process(args.sampleTime) ? 1.f : (presetIndex() != loaded ? 0.15f : 0.f));
		lights[LOCK_LIGHT].setBrightness(locked() ? 1.f : 0.f);
	}

	// ---- hand edits from the screen (UI thread; the audio thread only reads)
	void editStep(int k, int what) {
		if (k < 0 || k >= settings.length) return;
		if (what == 0) eng.setHit(k, settings, !res.hit[k]);
		else if (what == 1) {
			if (!res.hit[k]) eng.setHit(k, settings, true);
			eng.setAccent(k, settings, !res.acc[k]);
		}
		else {
			if (!res.hit[k]) eng.setHit(k, settings, true);
			eng.setRatchet(k, settings, !res.rat[k]);
		}
		edited = true;
		eng.evaluate(settings, res);
	}

	// ---- patch storage
	static json_t* arr(const float* v) {
		json_t* a = json_array();
		for (int i = 0; i < MAXS; i++) json_array_append_new(a, json_real(v[i]));
		return a;
	}
	static void unarr(json_t* a, float* v) {
		if (!a) return;
		for (int i = 0; i < MAXS && i < (int) json_array_size(a); i++) v[i] = (float) json_number_value(json_array_get(a, i));
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "r", arr(eng.cur.r));
		json_object_set_new(root, "q", arr(eng.cur.q));
		json_object_set_new(root, "a", arr(eng.cur.a));
		json_object_set_new(root, "seedR", arr(eng.seed.r));
		json_object_set_new(root, "seedQ", arr(eng.seed.q));
		json_object_set_new(root, "seedA", arr(eng.seed.a));
		json_object_set_new(root, "locked", json_boolean(lockLatch));
		json_object_set_new(root, "loaded", json_integer(loaded));
		json_object_set_new(root, "edited", json_boolean(edited));
		json_object_set_new(root, "mutated", json_boolean(mutated));
		json_object_set_new(root, "gateWidth", json_real(gateWidth));
		return root;
	}

	void dataFromJson(json_t* root) override {
		unarr(json_object_get(root, "r"), eng.cur.r);
		unarr(json_object_get(root, "q"), eng.cur.q);
		unarr(json_object_get(root, "a"), eng.cur.a);
		unarr(json_object_get(root, "seedR"), eng.seed.r);
		unarr(json_object_get(root, "seedQ"), eng.seed.q);
		unarr(json_object_get(root, "seedA"), eng.seed.a);
		json_t* j;
		if ((j = json_object_get(root, "locked"))) lockLatch = json_boolean_value(j);
		if ((j = json_object_get(root, "loaded"))) loaded = (int) json_integer_value(j);
		if ((j = json_object_get(root, "edited"))) edited = json_boolean_value(j);
		if ((j = json_object_get(root, "mutated"))) mutated = json_boolean_value(j);
		if ((j = json_object_get(root, "gateWidth"))) gateWidth = (float) json_number_value(j);
		if (loaded >= numPresets) loaded = -1;
		readSettings();
		eng.evaluate(settings, res);
	}
};

// ---- screen: status line and the step grid
struct TiersDisplay : OpaqueWidget {
	Tiers* module = nullptr;

	static const float GX0, GY0;

	void cellRect(int k, int len, float& x, float& y, float& w, float& h) {
		int rows = len > 16 ? 2 : 1;
		float gw = box.size.x - mm2px(3.2f), gh = box.size.y - mm2px(GY0 + 1.4f);
		float cw = gw / 16.f, rh = gh / rows;
		int r = k / 16, c = k % 16;
		x = mm2px(GX0) + c * cw + mm2px(0.3f);
		y = mm2px(GY0) + r * rh + mm2px(0.3f);
		w = cw - mm2px(0.6f);
		h = rh - mm2px(0.6f);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			OpaqueWidget::onButton(e);
			return;
		}
		int len = module->settings.length;
		for (int k = 0; k < len; k++) {
			float x, y, w, h;
			cellRect(k, len, x, y, w, h);
			if (e.pos.x >= x && e.pos.x < x + w && e.pos.y >= y && e.pos.y < y + h) {
				int what = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT ? 1 : (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL ? 2 : 0;
				module->editStep(k, what);
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
		const NVGcolor CORAL = nvgRGB(0xFF, 0x6B, 0x4A), CREAM = nvgRGB(0xF1, 0xE9, 0xD6);
		const NVGcolor SOFTC = nvgRGB(0xB8, 0xBC, 0xC8), CELL = nvgRGBA(0x2A, 0x32, 0x40, 140);
		const NVGcolor LEVEL_COL[LEVELS] = {nvgRGB(0xFF, 0x6B, 0x4A), nvgRGB(0xFF, 0xB3, 0x41), nvgRGB(0xE8, 0xDC, 0x5A),
		                                    nvgRGB(0x7D, 0xE2, 0xC3), nvgRGB(0x78, 0xA9, 0xFF)};
		nvgFontFaceId(vg, font->handle);

		// values (browser preview when there is no module)
		const char* demo = "X-xxX-xxX-x-X-xr";
		int len = 16, playing = 6, hits = 0;
		bool hit[MAXS] = {}, acc[MAXS] = {}, rat[MAXS] = {};
		bool res32 = false;
		std::string left, right;
		if (module) {
			const tiers::Settings& s = module->settings;
			len = s.length;
			res32 = s.res32;
			playing = module->step;
			for (int k = 0; k < len; k++) {
				hit[k] = module->res.hit[k];
				acc[k] = module->res.acc[k];
				rat[k] = module->res.rat[k];
			}
			hits = module->res.hits;
			left = string::f("%s  %d STEPS  %s  %d HITS%s", s.count ? "COUNT" : "CHANCE", len, res32 ? "1/32" : "1/16", hits,
			                 module->locked() ? "  LOCKED" : "");
			int pi = module->presetIndex();
			if (pi != module->loaded)
				right = string::f("LOAD > %s %s", module->presetList[pi].genre, module->presetList[pi].name);
			else {
				right = string::f("%s %s", module->presetList[pi].genre, module->presetList[pi].name);
				if (module->edited || module->mutated) right += module->mutated ? " (mutated)" : " (edited)";
			}
			if (module->loaded < 0 && pi == module->loaded) right = "GENERATED";
		}
		else {
			for (int k = 0; k < 16; k++) {
				char c = demo[k];
				hit[k] = c != '-';
				acc[k] = c == 'X' || c == 'R';
				rat[k] = c == 'r' || c == 'R';
				if (hit[k]) hits++;
			}
			left = string::f("CHANCE  16 STEPS  1/16  %d HITS", hits);
			right = "TECHNO Rumble kick";
		}

		nvgFontSize(vg, mm2px(2.5f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, SOFTC);
		nvgText(vg, mm2px(1.6f), mm2px(3.9f), left.c_str(), nullptr);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, CORAL);
		nvgText(vg, box.size.x - mm2px(1.6f), mm2px(3.9f), right.c_str(), nullptr);

		nvgFontSize(vg, mm2px(1.9f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		for (int k = 0; k < len; k++) {
			float x, y, w, h;
			cellRect(k, len, x, y, w, h);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x, y, w, h, mm2px(0.5f));
			nvgFillColor(vg, CELL);
			nvgFill(vg);
			// level stripe: which metric level this grid position belongs to
			nvgBeginPath(vg);
			nvgRect(vg, x, y + h - mm2px(0.9f), w, mm2px(0.9f));
			nvgFillColor(vg, nvgTransRGBA(LEVEL_COL[levelOf(k, res32)], 220));
			nvgFill(vg);
			if (hit[k]) {
				float full = h - mm2px(1.4f);
				float hh = acc[k] ? full : full * 0.62f;
				float yy = y + h - mm2px(1.2f) - hh;
				nvgFillColor(vg, acc[k] ? CORAL : CREAM);
				nvgBeginPath(vg);
				if (rat[k]) {
					nvgRoundedRect(vg, x + mm2px(0.5f), yy, w / 2 - mm2px(0.8f), hh, mm2px(0.3f));
					nvgRoundedRect(vg, x + w / 2 + mm2px(0.3f), yy, w / 2 - mm2px(0.8f), hh, mm2px(0.3f));
				}
				else nvgRoundedRect(vg, x + mm2px(0.5f), yy, w - mm2px(1.0f), hh, mm2px(0.3f));
				nvgFill(vg);
			}
			if (k == playing) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x - mm2px(0.15f), y - mm2px(0.15f), w + mm2px(0.3f), h + mm2px(0.3f), mm2px(0.6f));
				nvgStrokeColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
				nvgStrokeWidth(vg, mm2px(0.35f));
				nvgStroke(vg);
			}
			// beat numbers (every 4 steps)
			if (k % 4 == 0) {
				nvgFillColor(vg, nvgTransRGBA(SOFTC, 170));
				nvgText(vg, x + mm2px(0.6f), y + mm2px(2.1f), string::f("%d", k / 4 + 1).c_str(), nullptr);
			}
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};
const float TiersDisplay::GX0 = 1.6f;
const float TiersDisplay::GY0 = 5.6f;

struct PresetItem : MenuItem {
	Tiers* module;
	int index;
	void onAction(const ActionEvent& e) override {
		module->params[Tiers::PRESET_PARAM].setValue((float) index);
		module->loadPreset(index);
	}
};

struct TiersWidget : ModuleWidget {
	TiersWidget(Tiers* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Tiers.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		TiersDisplay* d = createWidget<TiersDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		// weights, HITS, ACCENT, ROTATE and their CV jacks
		for (int i = 0; i < 8; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(i), KNOB_Y)), module, Tiers::W_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), CV_Y)), module, Tiers::W_INPUT + i));
		}
		// row 3
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(colX(0), ROW3_Y)), module, Tiers::GEN_PARAM, Tiers::GEN_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(1), ROW3_Y)), module, Tiers::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(colX(2), ROW3_Y)), module, Tiers::LOCK_PARAM, Tiers::LOCK_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(colX(3), ROW3_Y)), module, Tiers::BACK_PARAM, Tiers::BACK_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(4), ROW3_Y)), module, Tiers::PRESET_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(colX(5), ROW3_Y)), module, Tiers::LOAD_PARAM, Tiers::LOAD_LIGHT));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colX(6) - 2.f, ROW3_Y)), module, Tiers::MODE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colX(7) - 2.f, ROW3_Y)), module, Tiers::LENGTH_PARAM));
		// inputs
		const int ins[8] = {Tiers::CLOCK_INPUT, Tiers::RESET_INPUT, Tiers::GEN_INPUT, Tiers::MUTATE_INPUT,
		                    Tiers::LOCK_INPUT, Tiers::BACK_INPUT, Tiers::PRESET_INPUT, Tiers::LOAD_INPUT};
		for (int i = 0; i < 8; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), IN_Y)), module, ins[i]));
		// row 5
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colX(0), OUT_Y)), module, Tiers::SWING_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colX(1) - 2.f, OUT_Y)), module, Tiers::RES_PARAM));
		for (int i = 0; i < 6; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(2 + i), OUT_Y)), module, Tiers::GATE_OUTPUT + i));
	}

	void appendContextMenu(Menu* menu) override {
		Tiers* m = getModule<Tiers>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Click a step: hit; Shift-click: accent; Ctrl-click: 32nd double"));
		static const char* GENRES[4] = {"HOUSE", "TECHNO", "FUNK", "DISCO"};
		for (int gi = 0; gi < 4; gi++) {
			const char* genre = GENRES[gi];
			menu->addChild(createSubmenuItem(string::f("Load preset: %s", genre), "", [=](Menu* sub) {
				for (int i = 0; i < m->numPresets; i++) {
					if (std::strcmp(m->presetList[i].genre, genre) != 0) continue;
					PresetItem* it = createMenuItem<PresetItem>(m->presetList[i].name, CHECKMARK(m->loaded == i));
					it->module = m;
					it->index = i;
					sub->addChild(it);
				}
			}));
		}
		menu->addChild(createSubmenuItem("Gate length", "", [=](Menu* sub) {
			const float v[4] = {0.1f, 0.25f, 0.5f, 0.9f};
			const char* n[4] = {"10% (short)", "25%", "50%", "90% (legato)"};
			for (int i = 0; i < 4; i++) {
				float val = v[i];
				sub->addChild(createCheckMenuItem(n[i], "", [=]() { return std::fabs(m->gateWidth - val) < 1e-3f; },
				                                  [=]() { m->gateWidth = val; }));
			}
		}));
		menu->addChild(createMenuItem("Clear hand edits", "", [=]() {
			m->eng.clearHandEdits(m->settings);
			m->edited = false;
		}));
	}
};

Model* modelTiers = createModel<Tiers, TiersWidget>("Tiers");
