// Breaker: load a break, Breaker finds and sorts its hits (kick, snare, hat, other)
// and plays new patterns made from them. The analysis lives in Analysis.hpp, the
// pattern generator and voices in Engine.hpp; this file is the Rack module and panel.
#include "plugin.hpp"
#include "Engine.hpp"
#include "Wav.hpp"
#include "Layout.hpp"
#include <osdialog.h>

using namespace breaker;
using namespace layout;

static const NVGcolor LANE_COL[NUM_LANES] = {nvgRGB(0xFF, 0x4D, 0x4D), nvgRGB(0xFF, 0xD2, 0x3F), nvgRGB(0x46, 0xD3, 0xFF), nvgRGB(0xB9, 0x8C, 0xFF)};
static const char LANE_LETTER[NUM_LANES] = {'K', 'S', 'H', 'O'};
static const int BARS_VALUES[3] = {1, 2, 4};

struct BreakerModule : Module {
	enum ParamIds {
		STYLE_PARAM, DENSITY_PARAM, MUTATE_PARAM, GHOSTS_PARAM, FILLS_PARAM, SWING_PARAM,
		SENS_PARAM, BARS_PARAM, PITCH_PARAM, DECAY_PARAM, VARIETY_PARAM, LEVEL_PARAM,
		NEW_PARAM, LOCK_PARAM, LOAD_PARAM, RUN_PARAM, NUM_PARAMS
	};
	enum InputIds { CLOCK_INPUT, RESET_INPUT, NEW_INPUT, LOCK_INPUT, MUTATE_INPUT, STYLE_INPUT, DENS_INPUT, VOCT_INPUT, NUM_INPUTS };
	enum OutputIds {
		KICK_OUTPUT, SNARE_OUTPUT, HAT_OUTPUT, OTHER_OUTPUT,
		KGATE_OUTPUT, SGATE_OUTPUT, HGATE_OUTPUT, OGATE_OUTPUT,
		L_OUTPUT, R_OUTPUT, ACCENT_OUTPUT, CLK_OUTPUT, NUM_OUTPUTS
	};
	enum LightIds { NEW_LIGHT, LOCK_LIGHT, LOAD_LIGHT, RUN_LIGHT, NUM_LIGHTS };

	BreakEngine engine;
	Knobs knobs;
	BreakData* brk = nullptr;                    // owned by the audio thread
	std::atomic<BreakData*> pending{nullptr};    // GUI -> audio
	std::atomic<BreakData*> trash{nullptr};      // audio -> GUI (deleted there)
	// GUI-thread state
	std::shared_ptr<const Audio> audio;
	std::string path;
	std::vector<Override> overrides;
	int barsOverride = 0;                        // 0 = guess
	bool bySound = false;                        // sort hits by sound (not a drum break)
	float analysedSens = -1.f;
	bool newHeld = false;
	float newFlash = 0.f;
	int lightDiv = 0;

	BreakerModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(STYLE_PARAM, 0.f, NUM_STYLES - 1, 0.f, "Style",
			{"Break (its own rhythm)", "Breaks (funk breakbeat)", "Jungle / drum & bass", "Hip hop (boom bap)", "Broken techno"});
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density (noon = as the style writes it)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (how much changes at each bar line)", "%", 0.f, 100.f);
		configParam(GHOSTS_PARAM, 0.f, 1.f, 0.5f, "Ghost notes", "%", 0.f, 200.f);
		configParam(FILLS_PARAM, 0.f, 1.f, 0.25f, "Fills (chance of a fill at the end of each 4-bar phrase)", "%", 0.f, 100.f);
		configParam(SWING_PARAM, 0.5f, 0.75f, 0.5f, "Swing", "%", 0.f, 100.f);
		configParam(SENS_PARAM, 0.f, 1.f, 0.5f, "Sensitivity (find fewer / more hits)", "%", 0.f, 100.f);
		configSwitch(BARS_PARAM, 0.f, 2.f, 1.f, "Pattern length", {"1 bar", "2 bars", "4 bars"});
		configParam(PITCH_PARAM, -12.f, 12.f, 0.f, "Pitch (speed and pitch together, like a sampler)", " semitones");
		configParam(DECAY_PARAM, 0.f, 1.f, 1.f, "Decay (fully right: each hit plays to the next one)", "%", 0.f, 100.f);
		configParam(VARIETY_PARAM, 0.f, 1.f, 0.f, "Variety (0: one hit per drum; up: use all of the break's hits of that kind)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configButton(NEW_PARAM, "New pattern");
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock (stop mutation)", {"Off", "Locked"});
		configButton(LOAD_PARAM, "Load a break (WAV)");
		configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run (when CLOCK is not patched)", {"Stopped", "Running"});
		getParamQuantity(LOCK_PARAM)->randomizeEnabled = false;
		getParamQuantity(RUN_PARAM)->randomizeEnabled = false;
		getParamQuantity(SENS_PARAM)->randomizeEnabled = false;
		configInput(CLOCK_INPUT, "Clock (16th notes; unpatched = plays at the break's own tempo)");
		configInput(RESET_INPUT, "Reset");
		configInput(NEW_INPUT, "New pattern trigger");
		configInput(LOCK_INPUT, "Lock gate");
		configInput(MUTATE_INPUT, "Mutate CV (0-10 V adds)");
		configInput(STYLE_INPUT, "Style CV (2 V per style)");
		configInput(DENS_INPUT, "Density CV (+-5 V)");
		configInput(VOCT_INPUT, "Pitch (1 V/oct)");
		const char* ln[NUM_LANES] = {"Kick", "Snare", "Hat", "Other"};
		for (int l = 0; l < NUM_LANES; l++) {
			configOutput(KICK_OUTPUT + l, std::string(ln[l]) + " audio (patching it takes it out of the mix)");
			configOutput(KGATE_OUTPUT + l, std::string(ln[l]) + " gate");
		}
		configOutput(L_OUTPUT, "Mix left");
		configOutput(R_OUTPUT, "Mix right");
		configOutput(ACCENT_OUTPUT, "Accent gate (strong beats)");
		configOutput(CLK_OUTPUT, "Clock out (16ths as played, with swing)");
	}
	~BreakerModule() {
		delete brk;
		delete pending.exchange(nullptr);
		delete trash.exchange(nullptr);
	}

	// ---- GUI thread
	void reanalyse() {
		if (!audio) return;
		float sens = params[SENS_PARAM].getValue();
		analysedSens = sens;
		BreakData* d = analyse(audio, system::getFilename(path), sens, barsOverride, overrides, bySound);
		delete pending.exchange(d);
	}
	bool loadFile(const std::string& p) {
		std::shared_ptr<Audio> a = loadWav(p);
		if (!a) return false;
		audio = a;
		path = p;
		overrides.clear();
		reanalyse();
		return true;
	}
	void reclassify(const Hit& h, int type) {
		for (auto it = overrides.begin(); it != overrides.end();)
			it = std::llabs(it->frame - h.start) < 64 ? overrides.erase(it) : it + 1;
		if (type != h.autoType) overrides.push_back({h.start, type});
		reanalyse();
	}
	void guiStep() {
		delete trash.exchange(nullptr);
		if (audio && std::fabs(params[SENS_PARAM].getValue() - analysedSens) > 0.004f) reanalyse();
	}

	// ---- audio thread
	void process(const ProcessArgs& args) override {
		if (BreakData* n = pending.exchange(nullptr)) {
			BreakData* old = brk;
			brk = n;
			engine.setBreak(brk);
			delete trash.exchange(old);   // rare: only if the GUI hasn't collected the last one
		}
		Knobs& k = knobs;
		k.style = clamp((int) std::round(params[STYLE_PARAM].getValue() + inputs[STYLE_INPUT].getVoltage() / 2.f), 0, NUM_STYLES - 1);
		k.density = clamp(params[DENSITY_PARAM].getValue() + inputs[DENS_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		k.mutate = clamp(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		k.ghosts = params[GHOSTS_PARAM].getValue();
		k.fills = params[FILLS_PARAM].getValue();
		k.swing = params[SWING_PARAM].getValue();
		k.variety = params[VARIETY_PARAM].getValue();
		k.decay = params[DECAY_PARAM].getValue();
		k.level = params[LEVEL_PARAM].getValue();
		k.pitch = params[PITCH_PARAM].getValue() / 12.f + inputs[VOCT_INPUT].getVoltage();
		k.pitch = clamp(k.pitch, -3.f, 3.f);
		k.bars = BARS_VALUES[clamp((int) params[BARS_PARAM].getValue(), 0, 2)];
		k.lock = params[LOCK_PARAM].getValue() > 0.5f || inputs[LOCK_INPUT].getVoltage() >= 1.f;
		k.run = params[RUN_PARAM].getValue() > 0.5f;
		k.clockConnected = inputs[CLOCK_INPUT].isConnected();
		k.clock = inputs[CLOCK_INPUT].getVoltage();
		k.reset = inputs[RESET_INPUT].getVoltage();
		bool newBtn = params[NEW_PARAM].getValue() > 0.5f;
		if (newBtn && !newHeld) { engine.edits.push(4 << 16); newFlash = 0.15f; }
		newHeld = newBtn;
		k.newTrig = inputs[NEW_INPUT].getVoltage();
		for (int l = 0; l < NUM_LANES; l++) k.laneOutConnected[l] = outputs[KICK_OUTPUT + l].isConnected();

		Outs o;
		engine.process(k, args.sampleTime, o);
		for (int l = 0; l < NUM_LANES; l++) {
			outputs[KICK_OUTPUT + l].setVoltage(o.lane[l]);
			outputs[KGATE_OUTPUT + l].setVoltage(o.gate[l]);
		}
		outputs[L_OUTPUT].setVoltage(o.L);
		outputs[R_OUTPUT].setVoltage(o.R);
		outputs[ACCENT_OUTPUT].setVoltage(o.accent);
		outputs[CLK_OUTPUT].setVoltage(o.clk);

		newFlash -= args.sampleTime;
		if (++lightDiv >= 64) {
			lightDiv = 0;
			lights[NEW_LIGHT].setBrightness(newFlash > 0 || engine.newT.high ? 1.f : 0.f);
			lights[LOCK_LIGHT].setBrightness(k.lock ? 1.f : 0.f);
			lights[LOAD_LIGHT].setBrightness(brk ? 0.f : 0.5f);
			lights[RUN_LIGHT].setBrightness(k.clockConnected ? 0.2f : k.run ? 1.f : 0.f);
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine.edits.push(6 << 16);
	}
	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		engine.edits.push(4 << 16);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "path", json_string(path.c_str()));
		json_object_set_new(root, "barsOverride", json_integer(barsOverride));
		json_object_set_new(root, "bySound", json_boolean(bySound));
		json_t* ov = json_array();
		for (auto& o : overrides) {
			json_t* e = json_array();
			json_array_append_new(e, json_integer(o.frame));
			json_array_append_new(e, json_integer(o.type));
			json_array_append_new(ov, e);
		}
		json_object_set_new(root, "overrides", ov);
		json_t* pat = json_array();
		const Pattern& P = engine.pat;
		for (int l = 0; l < NUM_LANES; l++)
			for (int c = 0; c < CELLS; c++) {
				json_t* e = json_array();
				json_array_append_new(e, json_real(P.rMain[l][c]));
				json_array_append_new(e, json_real(P.rGhost[l][c]));
				json_array_append_new(e, json_real(P.rHit[l][c]));
				json_array_append_new(e, json_integer(P.force[l][c]));
				json_array_append_new(pat, e);
			}
		json_object_set_new(root, "pattern", pat);
		json_t* locks = json_array();
		for (int l = 0; l < NUM_LANES; l++) json_array_append_new(locks, json_boolean(engine.laneLock[l]));
		json_object_set_new(root, "laneLocks", locks);
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* b = json_object_get(root, "barsOverride")) barsOverride = (int) json_integer_value(b);
		bySound = json_is_true(json_object_get(root, "bySound"));
		if (json_t* pat = json_object_get(root, "pattern")) {
			Pattern P = engine.pat;
			for (int i = 0; i < NUM_LANES * CELLS && i < (int) json_array_size(pat); i++) {
				json_t* e = json_array_get(pat, i);
				int l = i / CELLS, c = i % CELLS;
				P.rMain[l][c] = (float) json_number_value(json_array_get(e, 0));
				P.rGhost[l][c] = (float) json_number_value(json_array_get(e, 1));
				P.rHit[l][c] = (float) json_number_value(json_array_get(e, 2));
				P.force[l][c] = (int8_t) json_integer_value(json_array_get(e, 3));
			}
			engine.pat = P;
			engine.base = P;
		}
		if (json_t* locks = json_object_get(root, "laneLocks"))
			for (int l = 0; l < NUM_LANES; l++) engine.laneLock[l] = json_is_true(json_array_get(locks, l));
		std::string p;
		if (json_t* j = json_object_get(root, "path")) p = json_string_value(j) ? json_string_value(j) : "";
		if (!p.empty()) {
			std::vector<Override> ov;
			if (json_t* a = json_object_get(root, "overrides"))
				for (size_t i = 0; i < json_array_size(a); i++) {
					json_t* e = json_array_get(a, i);
					ov.push_back({(int64_t) json_integer_value(json_array_get(e, 0)), (int) json_integer_value(json_array_get(e, 1))});
				}
			if (loadFile(p)) { overrides = ov; reanalyse(); }
		}
	}
};

template <typename TBase = GrayModuleLightWidget>
struct TOrangeLight : TBase {
	TOrangeLight() { this->addBaseColor(nvgRGB(0xFF, 0x7A, 0x1A)); }
};
using OrangeLight = TOrangeLight<>;

static void chooseBreak(BreakerModule* m) {
	std::string dir = m->path.empty() ? "" : system::getDirectory(m->path);
	osdialog_filters* filters = osdialog_filters_parse("WAV:wav,WAV");
	char* p = osdialog_file(OSDIALOG_OPEN, dir.empty() ? NULL : dir.c_str(), NULL, filters);
	osdialog_filters_free(filters);
	if (p) {
		m->loadFile(p);
		std::free(p);
	}
}

struct LoadButton : VCVLightBezel<OrangeLight> {
	void onDragEnd(const DragEndEvent& e) override {
		VCVLightBezel<OrangeLight>::onDragEnd(e);
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		if (BreakerModule* m = module ? dynamic_cast<BreakerModule*>(module) : nullptr) chooseBreak(m);
	}
};

static std::shared_ptr<window::Font> monoFont() {
	return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
}

static const float TAG_Y = 5.4f, TAG_H = 3.3f, TAG_W = 4.0f;
static const float LANE_Y = 6.2f, LANE_STEP = 4.5f, CELL_H = 3.6f, CELLS_X = 11.f;

// ---- top screen: waveform + a tag on every hit (click a tag to change its type)
struct WaveDisplay : Widget {
	BreakerModule* module = nullptr;
	float hx(const BreakData* b, const Hit& h) const {
		float x0 = mm2px(2.5f), x1 = box.size.x - mm2px(2.5f);
		return x0 + (x1 - x0) * (float) h.start / (float) std::max<size_t>(1, b->audio->size());
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || !module->brk || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) { Widget::onButton(e); return; }
		const BreakData* b = module->brk;
		int best = -1;
		float bd = mm2px(2.4f);
		for (size_t i = 0; i < b->hits.size(); i++) {
			float d = std::fabs(hx(b, b->hits[i]) - e.pos.x);
			if (d < bd) { bd = d; best = (int) i; }
		}
		if (best >= 0) {
			Hit h = b->hits[best];
			module->reclassify(h, (h.type + 1) % NUM_LANES);
			e.consume(this);
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		std::shared_ptr<window::Font> font = monoFont();
		if (!font) return;
		NVGcontext* vg = args.vg;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, mm2px(2.9f));
		const BreakData* b = module ? module->brk : nullptr;
		float x0 = mm2px(2.5f), x1 = box.size.x - mm2px(2.5f);
		float y0 = mm2px(TAG_Y + TAG_H + 0.4f), y1 = box.size.y - mm2px(1.2f), mid = (y0 + y1) / 2, amp = (y1 - y0) / 2;
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		if (!b) {
			nvgFillColor(vg, nvgRGB(0xF2, 0xEF, 0xE6));
			nvgText(vg, x0, mm2px(4.4f), "NO BREAK LOADED", NULL);
			nvgFillColor(vg, nvgRGB(0x9A, 0x9A, 0x9A));
			nvgText(vg, x0, mid, "Press LOAD or drop a WAV on the module.", NULL);
			nvgText(vg, x0, mid + mm2px(4.f), "Patterns still play from the gate outs.", NULL);
			return;
		}
		char buf[160];
		std::string nm = b->name.size() > 34 ? b->name.substr(0, 33) + "~" : b->name;
		snprintf(buf, sizeof(buf), "%s   %d BAR%s   %.1f BPM%s%s", nm.c_str(), b->loopBars, b->loopBars > 1 ? "S" : "", b->bpm, b->barsGuessed ? "" : " (set)", module->bySound ? "   BY SOUND" : "");
		nvgFillColor(vg, nvgRGB(0xF2, 0xEF, 0xE6));
		nvgText(vg, x0, mm2px(4.2f), buf, NULL);
		// counts, right aligned
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		float rx = x1;
		for (int t = NUM_LANES - 1; t >= 0; t--) {
			snprintf(buf, sizeof(buf), "%c %d", LANE_LETTER[t], (int) b->byType[t].size());
			nvgFillColor(vg, LANE_COL[t]);
			nvgText(vg, rx, mm2px(4.2f), buf, NULL);
			float bounds[4];
			nvgTextBounds(vg, 0, 0, buf, NULL, bounds);
			rx -= (bounds[2] - bounds[0]) + mm2px(2.2f);
		}
		// waveform
		nvgBeginPath(vg);
		int cols = std::max(1, (int) (x1 - x0));
		for (int i = 0; i < cols; i++) {
			int p = i * BreakData::PEAKS / cols;
			float x = x0 + i + 0.5f;
			nvgMoveTo(vg, x, mid - b->peakMax[p] * amp);
			nvgLineTo(vg, x, mid - std::min(b->peakMin[p], -0.002f) * amp);
		}
		nvgStrokeColor(vg, nvgRGB(0x6F, 0x7A, 0x72));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
		// hit tags
		int playing[NUM_LANES];
		for (int l = 0; l < NUM_LANES; l++) playing[l] = module->engine.lastHit[l];
		nvgFontSize(vg, mm2px(2.5f));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		for (size_t i = 0; i < b->hits.size(); i++) {
			const Hit& h = b->hits[i];
			float x = hx(b, h);
			NVGcolor c = LANE_COL[h.type];
			bool on = false;
			for (int l = 0; l < NUM_LANES; l++) if (playing[l] == (int) i) on = true;
			nvgBeginPath(vg);
			nvgMoveTo(vg, x, mm2px(TAG_Y + TAG_H));
			nvgLineTo(vg, x, y1);
			nvgStrokeColor(vg, nvgTransRGBA(c, on ? 255 : (h.ghost ? 110 : 190)));
			nvgStrokeWidth(vg, on ? 2.f : 1.2f);
			nvgStroke(vg);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x - mm2px(TAG_W / 2), mm2px(TAG_Y), mm2px(TAG_W), mm2px(TAG_H), 2.f);
			nvgFillColor(vg, on ? nvgRGB(0xFF, 0xFF, 0xFF) : c);
			nvgFill(vg);
			if (h.type != h.autoType) {   // your correction: white ring
				nvgStrokeColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
				nvgStrokeWidth(vg, 1.f);
				nvgStroke(vg);
			}
			char s[2] = {LANE_LETTER[h.type], 0};
			nvgFillColor(vg, nvgRGB(0x11, 0x11, 0x11));
			nvgText(vg, x, mm2px(TAG_Y + TAG_H / 2 + 0.1f), s, NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

// ---- lower screen: the pattern, one bar at a time
struct GridDisplay : Widget {
	BreakerModule* module = nullptr;
	float cw() const { return (box.size.x - mm2px(CELLS_X + 3.f)) / STEPS; }
	int shownBar() const {
		int s = module ? module->engine.dispStep.load() : -1;
		return s < 0 ? 0 : s / STEPS;
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) { Widget::onButton(e); return; }
		float y = e.pos.y / mm2px(1.f);
		int l = (int) std::floor((y - LANE_Y + 0.45f) / LANE_STEP);
		if (l < 0 || l >= NUM_LANES) return;
		if (e.pos.x < mm2px(CELLS_X)) {
			module->engine.edits.push((3 << 16) | l);
		}
		else {
			int s = (int) ((e.pos.x - mm2px(CELLS_X)) / cw());
			if (s < 0 || s >= STEPS) return;
			int c = shownBar() * STEPS + s;
			bool ghost = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			module->engine.edits.push(((ghost ? 2 : 1) << 16) | (l * CELLS + c));
		}
		e.consume(this);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		std::shared_ptr<window::Font> font = monoFont();
		if (!font) return;
		NVGcontext* vg = args.vg;
		nvgFontFaceId(vg, font->handle);
		Knobs k;
		BreakEngine* e = module ? &module->engine : nullptr;
		if (module) k = module->knobs;
		int bar = shownBar();
		int s = e ? e->dispStep.load() : -1;
		char buf[120];
		nvgFontSize(vg, mm2px(2.8f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, nvgRGB(0xF2, 0xEF, 0xE6));
		snprintf(buf, sizeof(buf), "%s   BAR %d/%d   MUTATE %d%%", STYLE_NAMES[k.style], bar + 1, k.bars, (int) std::round(k.mutate * 100));
		nvgText(vg, mm2px(2.5f), mm2px(4.2f), buf, NULL);
		std::string lk;
		if (k.lock) lk = "LOCKED";
		else if (e) {
			for (int l = 0; l < NUM_LANES; l++) if (e->laneLock[l]) { lk += lk.empty() ? "LOCKED:" : ""; lk += " "; lk += LANE_LETTER[l]; }
		}
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, nvgRGB(0xFF, 0x7A, 0x1A));
		nvgText(vg, box.size.x - mm2px(2.5f), mm2px(4.2f), lk.c_str(), NULL);
		float w = cw();
		for (int l = 0; l < NUM_LANES; l++) {
			float y = mm2px(LANE_Y + l * LANE_STEP);
			nvgFontSize(vg, mm2px(3.f));
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(vg, LANE_COL[l]);
			char let[2] = {LANE_LETTER[l], 0};
			nvgText(vg, mm2px(2.6f), y + mm2px(3.f), let, NULL);
			if (e && (e->laneLock[l] || k.lock)) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, mm2px(6.6f), y + mm2px(0.8f), mm2px(2.f), mm2px(2.f), 1.f);
				nvgFillColor(vg, nvgRGB(0xFF, 0x7A, 0x1A));
				nvgFill(vg);
			}
			for (int st = 0; st < STEPS; st++) {
				int c = bar * STEPS + st;
				int state = e ? e->cellState(k, l, c) : ((st % 4 == 0 && l == 2) ? C_HIT : C_OFF);
				float x = mm2px(CELLS_X) + st * w;
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x + 1.5f, y, w - 3.f, mm2px(CELL_H), 2.f);
				if (state == C_OFF) nvgFillColor(vg, st % 4 ? nvgRGB(0x22, 0x25, 0x2B) : nvgRGB(0x30, 0x34, 0x3D));
				else nvgFillColor(vg, state == C_GHOST ? nvgTransRGBA(LANE_COL[l], 110) : LANE_COL[l]);
				nvgFill(vg);
				if (state == C_ACCENT) {
					nvgBeginPath(vg);
					nvgRect(vg, x + 3.f, y + 1.5f, w - 6.f, 1.5f);
					nvgFillColor(vg, nvgRGBA(255, 255, 255, 200));
					nvgFill(vg);
				}
				if (e && e->mutatedBar[l][c] && (long) e->mutatedBar[l][c] >= e->barCount) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, x + 1.5f, y, w - 3.f, mm2px(CELL_H), 2.f);
					nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 170));
					nvgStrokeWidth(vg, 1.f);
					nvgStroke(vg);
				}
				if (e && e->pat.force[l][c] != F_NONE) {
					nvgBeginPath(vg);
					nvgCircle(vg, x + w - 4.f, y + 3.f, 1.3f);
					nvgFillColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
					nvgFill(vg);
				}
			}
		}
		if (s >= 0) {
			float x = mm2px(CELLS_X) + (s % STEPS) * w;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + 0.5f, mm2px(LANE_Y - 0.6f), w - 1.f, mm2px(LANE_STEP * 3 + CELL_H + 1.2f), 2.5f);
			nvgStrokeColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
			nvgStrokeWidth(vg, 1.4f);
			nvgStroke(vg);
		}
		Widget::drawLayer(args, layer);
	}
};

struct BreakerWidget : ModuleWidget {
	BreakerWidget(BreakerModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Breaker.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		WaveDisplay* wd = createWidget<WaveDisplay>(mm2px(Vec(WAVE_X, WAVE_Y)));
		wd->box.size = mm2px(Vec(WAVE_W, WAVE_H));
		wd->module = module;
		addChild(wd);
		GridDisplay* gd = createWidget<GridDisplay>(mm2px(Vec(GRID_X, GRID_Y)));
		gd->box.size = mm2px(Vec(GRID_W, GRID_H));
		gd->module = module;
		addChild(gd);

		typedef BreakerModule M;
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(K_STYLE_X, K_STYLE_Y)), module, M::STYLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_DENSITY_X, K_DENSITY_Y)), module, M::DENSITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_MUTATE_X, K_MUTATE_Y)), module, M::MUTATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_GHOSTS_X, K_GHOSTS_Y)), module, M::GHOSTS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_FILLS_X, K_FILLS_Y)), module, M::FILLS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_SWING_X, K_SWING_Y)), module, M::SWING_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_SENS_X, K_SENS_Y)), module, M::SENS_PARAM));
		RoundSmallBlackKnob* bars = createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_BARS_X, K_BARS_Y)), module, M::BARS_PARAM);
		bars->snap = true;
		addParam(bars);
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_PITCH_X, K_PITCH_Y)), module, M::PITCH_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_DECAY_X, K_DECAY_Y)), module, M::DECAY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_VARIETY_X, K_VARIETY_Y)), module, M::VARIETY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_LEVEL_X, K_LEVEL_Y)), module, M::LEVEL_PARAM));

		addParam(createLightParamCentered<VCVLightBezel<OrangeLight>>(mm2px(Vec(B_NEW_X, B_NEW_Y)), module, M::NEW_PARAM, M::NEW_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(B_LOCK_X, B_LOCK_Y)), module, M::LOCK_PARAM, M::LOCK_LIGHT));
		addParam(createLightParamCentered<LoadButton>(mm2px(Vec(B_LOAD_X, B_LOAD_Y)), module, M::LOAD_PARAM, M::LOAD_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(B_RUN_X, B_RUN_Y)), module, M::RUN_PARAM, M::RUN_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_CLOCK_X, I_CLOCK_Y)), module, M::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_RESET_X, I_RESET_Y)), module, M::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_NEW_X, I_NEW_Y)), module, M::NEW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_LOCK_X, I_LOCK_Y)), module, M::LOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_MUTATE_X, I_MUTATE_Y)), module, M::MUTATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_STYLE_X, I_STYLE_Y)), module, M::STYLE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_DENS_X, I_DENS_Y)), module, M::DENS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_VOCT_X, I_VOCT_Y)), module, M::VOCT_INPUT));

		const float ox[NUM_LANES] = {O_KICK_X, O_SNARE_X, O_HAT_X, O_OTHER_X};
		for (int l = 0; l < NUM_LANES; l++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(ox[l], O_KICK_Y)), module, M::KICK_OUTPUT + l));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(ox[l], O_KGATE_Y)), module, M::KGATE_OUTPUT + l));
		}
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_L_X, O_L_Y)), module, M::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_R_X, O_R_Y)), module, M::R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_ACCENT_X, O_ACCENT_Y)), module, M::ACCENT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_CLK_X, O_CLK_Y)), module, M::CLK_OUTPUT));
	}

	void step() override {
		if (BreakerModule* m = dynamic_cast<BreakerModule*>(module)) m->guiStep();
		ModuleWidget::step();
	}

	void onPathDrop(const PathDropEvent& e) override {
		BreakerModule* m = dynamic_cast<BreakerModule*>(module);
		if (m && !e.paths.empty()) { m->loadFile(e.paths[0]); e.consume(this); }
	}

	void appendContextMenu(Menu* menu) override {
		BreakerModule* m = dynamic_cast<BreakerModule*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Load break (WAV)...", m->path.empty() ? "" : system::getFilename(m->path), [=]() { chooseBreak(m); }));
		menu->addChild(createSubmenuItem("Break length", "", [=](Menu* sub) {
			const char* names[5] = {"Guess", "1 bar", "2 bars", "4 bars", "8 bars"};
			const int vals[5] = {0, 1, 2, 4, 8};
			for (int i = 0; i < 5; i++)
				sub->addChild(createCheckMenuItem(names[i], "", [=]() { return m->barsOverride == vals[i]; },
					[=]() { m->barsOverride = vals[i]; m->reanalyse(); }));
		}));
		menu->addChild(createBoolMenuItem("Sort hits by sound (for samples that aren't drums)", "",
			[=]() { return m->bySound; }, [=](bool v) { m->bySound = v; m->reanalyse(); }));
		menu->addChild(createMenuItem("Undo my hit corrections", "", [=]() { m->overrides.clear(); m->reanalyse(); }, m->overrides.empty()));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Lock a lane (or click its letter)"));
		for (int l = 0; l < NUM_LANES; l++)
			menu->addChild(createCheckMenuItem(LANE_NAMES[l], "", [=]() { return m->engine.laneLock[l]; },
				[=]() { m->engine.edits.push((3 << 16) | l); }));
		menu->addChild(createMenuItem("Clear mutation (back to the last NEW)", "", [=]() { m->engine.edits.push(5 << 16); }));
		menu->addChild(createMenuItem("Clear my step edits", "", [=]() { m->engine.edits.push(7 << 16); }));
	}
};

Model* modelBreaker = createModel<BreakerModule, BreakerWidget>("Breaker");
