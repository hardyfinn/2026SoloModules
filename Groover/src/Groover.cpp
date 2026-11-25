// Groover: puts Ableton grooves on VCV drums.
//   Load Live groove files (.agr) or MIDI files (.mid), or use the built-ins.
//   CLOCK in -> GROOVED CLOCK out: each grid step pushed or pulled to the
//   groove (early hits are possible because the clock's tempo is predicted).
//   8 trigger lanes: triggers from a sequencer on the straight clock are
//   delayed to land on the groove. ACCENT out: the groove's velocity per step.
// The engine is in GrooverDSP.hpp.
#include "plugin.hpp"
#include "GrooverDSP.hpp"
#include <osdialog.h>
#include <atomic>

using namespace groover;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 14.5f, DISP_W = 91.6f, DISP_H = 23.0f;
static const float KX[3] = {13.0f, 47.0f, 81.0f};
static const float CVX[3] = {25.0f, 59.0f, 93.0f};
static const float ROW1_Y = 51.0f, ROW2_Y = 72.0f;
static const float IO_Y = 91.0f;
static const float CLK_X = 10.0f, RST_X = 24.0f, LOAD_X = 41.0f;
static const float OCLK_X = 64.5f, OACC_X = 78.5f, OLOOP_X = 92.0f;
static const float LIN_Y = 104.5f, LOUT_Y = 118.0f;
static const float LANE_X0 = 9.0f, LANE_DX = 11.94f;

static const int NUM_LANES = 8;
static const int PPQN_OPTS[6] = {1, 2, 4, 8, 24, 48};
static const float LOOKAHEAD_MS[5] = {0.f, 5.f, 10.f, 20.f, 40.f};

struct GrooveBank {
	std::vector<Groove> list;  // built-ins first, then loaded files
	int builtins = 0;
	std::vector<std::string> paths;  // for the loaded ones (may be empty strings)
};

struct Groover : Module {
	enum ParamId {
		GROOVE_PARAM, AMOUNT_PARAM, TIMING_PARAM,
		VELOCITY_PARAM, RANDOM_PARAM, QUANTIZE_PARAM,
		LOAD_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		GROOVE_INPUT, AMOUNT_INPUT, TIMING_INPUT,
		VELOCITY_INPUT, RANDOM_INPUT, QUANTIZE_INPUT,
		CLOCK_INPUT, RESET_INPUT,
		ENUMS(LANE_INPUT, NUM_LANES),
		INPUTS_LEN
	};
	enum OutputId {
		CLOCK_OUTPUT, ACCENT_OUTPUT, LOOP_OUTPUT,
		ENUMS(LANE_OUTPUT, NUM_LANES),
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	GrooveBank* bank = nullptr;
	std::atomic<GrooveBank*> pending{nullptr};
	std::atomic<GrooveBank*> trash{nullptr};

	Clock clk;
	GroovedClock gclk;
	Lane lanes[NUM_LANES];
	Rng rng;
	dsp::SchmittTrigger clockTrig, resetTrig, laneTrig[NUM_LANES];
	dsp::PulseGenerator clockPulse, loopPulse;
	double now = 0.0;
	float accent = 10.f;

	// options
	int ppqnIndex = 2;    // 4 PPQN (16ths)
	int baseOverride = -1;  // -1 = the groove's own base
	int lookIndex = 0;

	// screen (written by the audio thread)
	std::atomic<int> dispGroove{0};
	std::atomic<long> dispStep{-1};
	std::atomic<float> dispBpm{120.f};
	std::atomic<bool> dispRunning{false};
	Settings dispSt;

	struct GrooveQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Groover* m = dynamic_cast<Groover*>(module);
			if (!m || !m->bank) return ParamQuantity::getDisplayValueString();
			int i = m->indexFor(getValue());
			return string::f("%d: %s", i + 1, m->bank->list[i].name.c_str());
		}
		std::string getUnit() override { return ""; }
	};

	Groover() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<GrooveQuantity>(GROOVE_PARAM, 0.f, 1.f, 0.5f / 16.f, "Groove");
		configParam(AMOUNT_PARAM, 0.f, 1.3f, 1.f, "Amount (Live's Global Amount: scales the timing)", "%", 0.f, 100.f);
		configParam(TIMING_PARAM, 0.f, 1.f, 1.f, "Timing (how strongly hits move to the groove)", "%", 0.f, 100.f);
		configParam(VELOCITY_PARAM, -1.f, 1.f, 1.f, "Velocity (how much the groove's loudness reaches ACCENT; negative flips it)", "%", 0.f, 100.f);
		configParam(RANDOM_PARAM, 0.f, 1.f, 0.f, "Random (per-hit timing wobble, up to a quarter step)", "%", 0.f, 100.f);
		configParam(QUANTIZE_PARAM, 0.f, 1.f, 0.f, "Quantize (lanes: pulls loose input hits to the grid before grooving)", "%", 0.f, 100.f);
		configButton(LOAD_PARAM, "Load groove file (.agr or .mid)");
		const char* cvn[6] = {"Groove select", "Amount", "Timing", "Velocity", "Random", "Quantize"};
		for (int i = 0; i < 6; i++) configInput(GROOVE_INPUT + i, string::f("%s CV (±5 V covers the whole range)", cvn[i]));
		configInput(CLOCK_INPUT, "Clock (straight; 16ths by default, PPQN in the right-click menu)");
		configInput(RESET_INPUT, "Reset (back to the start of the groove)");
		configOutput(CLOCK_OUTPUT, "Grooved clock (one pulse per grid step, moved to the groove)");
		configOutput(ACCENT_OUTPUT, "Accent (0-10 V: the groove's velocity for the current step)");
		configOutput(LOOP_OUTPUT, "Loop (trigger at the start of each pass of the groove)");
		for (int i = 0; i < NUM_LANES; i++) {
			configInput(LANE_INPUT + i, string::f("Lane %d (triggers or gates on the straight grid)", i + 1));
			configOutput(LANE_OUTPUT + i, string::f("Lane %d grooved", i + 1));
		}
		bank = new GrooveBank;
		bank->list = builtinGrooves();
		bank->builtins = (int) bank->list.size();
		bank->paths.clear();
		clk.reset();
		gclk.reset();
	}

	~Groover() {
		delete bank;
		delete pending.exchange(nullptr);
		delete trash.exchange(nullptr);
	}

	int indexFor(float v) const {
		int n = bank ? (int) bank->list.size() : 1;
		int i = (int) std::floor(clamp(v, 0.f, 1.f) * n);
		return clamp(i, 0, n - 1);
	}

	// ---- loading (UI thread). Builds a new bank and hands it to the audio thread.
	void installBank(GrooveBank* nb) {
		delete trash.exchange(nullptr);
		GrooveBank* old = pending.exchange(nb);
		delete old;
	}

	GrooveBank* copyBank() {
		GrooveBank* cur = pending.load();
		GrooveBank* src = cur ? cur : bank;
		GrooveBank* nb = new GrooveBank(*src);
		return nb;
	}

	// Adds files; returns a message for anything that failed. Selects the first new one.
	std::string addFiles(const std::vector<std::string>& paths, bool replaceLoaded) {
		GrooveBank* nb = copyBank();
		if (replaceLoaded) {
			nb->list.resize(nb->builtins);
			nb->paths.clear();
		}
		std::string errs;
		int firstNew = -1;
		for (const std::string& p : paths) {
			Groove g; std::string why;
			if (loadGrooveFile(p, g, why)) {
				if (firstNew < 0) firstNew = (int) nb->list.size();
				nb->list.push_back(g);
				nb->paths.push_back(p);
			} else {
				errs += system::getFilename(p) + ": " + why + "\n";
			}
			if (nb->list.size() >= 256) break;
		}
		int n = (int) nb->list.size();
		installBank(nb);
		if (firstNew >= 0) params[GROOVE_PARAM].setValue((firstNew + 0.5f) / n);
		return errs;
	}

	void loadFileDialog() {
		osdialog_filters* f = osdialog_filters_parse("Grooves (.agr .mid):agr,mid,midi");
		char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, f);
		osdialog_filters_free(f);
		if (!path) return;
		std::string p = path;
		std::free(path);
		std::string errs = addFiles({p}, false);
		if (!errs.empty()) osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Groover couldn't load:\n" + errs).c_str());
	}

	void loadFolderDialog() {
		char* path = osdialog_file(OSDIALOG_OPEN_DIR, NULL, NULL, NULL);
		if (!path) return;
		std::string dir = path;
		std::free(path);
		std::vector<std::string> files;
		for (const std::string& e : system::getEntries(dir)) {
			std::string ext = string::lowercase(system::getExtension(e));
			if (ext == ".agr" || ext == ".mid" || ext == ".midi") files.push_back(e);
		}
		std::sort(files.begin(), files.end());
		if (files.empty()) {
			osdialog_message(OSDIALOG_INFO, OSDIALOG_OK, "No .agr or .mid files in that folder.");
			return;
		}
		std::string errs = addFiles(files, true);
		if (!errs.empty()) osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Groover couldn't load:\n" + errs).c_str());
	}

	// Save the selected groove as an .agr, with the knobs' settings, for Live's Groove Pool.
	void saveAgrDialog() {
		Groove g = bank->list[indexFor(params[GROOVE_PARAM].getValue())];
		g.timing = params[TIMING_PARAM].getValue() * 100.f;
		g.random = params[RANDOM_PARAM].getValue() * 100.f;
		g.velocity = params[VELOCITY_PARAM].getValue() * 100.f;
		g.quantize = params[QUANTIZE_PARAM].getValue() * 100.f;
		if (baseOverride >= 0) g.base = baseOverride;
		osdialog_filters* f = osdialog_filters_parse("Ableton groove (.agr):agr");
		char* path = osdialog_file(OSDIALOG_SAVE, NULL, (g.name + ".agr").c_str(), f);
		osdialog_filters_free(f);
		if (!path) return;
		std::string p = path;
		std::free(path);
		if (string::lowercase(system::getExtension(p)) != ".agr") p += ".agr";
		std::ofstream of(p);
		of << writeAgr(g);
	}

	void clearLoaded() {
		GrooveBank* nb = copyBank();
		nb->list.resize(nb->builtins);
		nb->paths.clear();
		installBank(nb);
	}

	// Copy the selected groove's own Groove Pool settings onto the knobs.
	void useGrooveSettings() {
		const Groove& g = bank->list[indexFor(params[GROOVE_PARAM].getValue())];
		params[TIMING_PARAM].setValue(clamp(g.timing / 100.f, 0.f, 1.f));
		params[RANDOM_PARAM].setValue(clamp(g.random / 100.f, 0.f, 1.f));
		params[VELOCITY_PARAM].setValue(clamp(g.velocity / 100.f, -1.f, 1.f));
		params[QUANTIZE_PARAM].setValue(clamp(g.quantize / 100.f, 0.f, 1.f));
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		ppqnIndex = 2;
		baseOverride = -1;
		lookIndex = 0;
		clk.reset();
		gclk.reset();
		for (Lane& l : lanes) l.clear();
	}

	float knob(int p, int in, float lo, float hi) {
		float v = params[p].getValue();
		if (inputs[in].isConnected()) v += inputs[in].getVoltage() * (hi - lo) / 10.f;
		return clamp(v, lo, hi);
	}

	void process(const ProcessArgs& args) override {
		// pick up a new bank from the UI thread
		if (GrooveBank* nb = pending.exchange(nullptr)) {
			GrooveBank* old = bank;
			bank = nb;
			delete trash.exchange(old);
		}
		const double dt = args.sampleTime;
		now += dt;

		int gi = indexFor(knob(GROOVE_PARAM, GROOVE_INPUT, 0.f, 1.f));
		const Groove& g = bank->list[gi];
		Settings st;
		st.base = baseOverride >= 0 ? baseOverride : g.base;
		st.amount = knob(AMOUNT_PARAM, AMOUNT_INPUT, 0.f, 1.3f);
		st.timing = knob(TIMING_PARAM, TIMING_INPUT, 0.f, 1.f);
		st.velocity = knob(VELOCITY_PARAM, VELOCITY_INPUT, -1.f, 1.f);
		st.random = knob(RANDOM_PARAM, RANDOM_INPUT, 0.f, 1.f);
		st.quantize = knob(QUANTIZE_PARAM, QUANTIZE_INPUT, 0.f, 1.f);

		// ---- clock
		clk.ppqn = PPQN_OPTS[ppqnIndex];
		clk.tick(dt);
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			clk.reset();
			gclk.reset();
		}
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) clk.pulse();
		double pos = clk.pos();
		double beatSec = clk.beatSeconds();

		// ---- grooved clock
		long fired = gclk.process(g, st, pos);
		if (fired >= 0) {
			float stepSec = (float) (BASE_BEATS[st.base] * beatSec);
			clockPulse.trigger(clamp(stepSec * 0.5f, 1e-3f, 0.01f));
			accent = 10.f * stepAccent(g, st, fired);
			int n = g.steps(st.base);
			if (n > 0 && fired % n == 0) loopPulse.trigger(1e-3f);
			dispStep = fired;
		}

		// ---- lanes
		double look = LOOKAHEAD_MS[lookIndex] * 1e-3;
		for (int i = 0; i < NUM_LANES; i++) {
			Lane& ln = lanes[i];
			if (inputs[LANE_INPUT + i].isConnected()) {
				float v = inputs[LANE_INPUT + i].getVoltage();
				bool wasHigh = laneTrig[i].isHigh();
				if (laneTrig[i].process(v, 0.1f, 1.f)) {
					long k;
					float r = rng.uni() * 2.f - 1.f;
					double d = laneDelay(g, st, pos, beatSec, r, look, k);
					ln.lastDelay = d;
					ln.lastOn = now + d;
					ln.push(now + d, true);
					if (!outputs[CLOCK_OUTPUT].isConnected()) {
						accent = 10.f * stepAccent(g, st, k);
						if (!clk.running) dispStep = k;
					}
				} else if (wasHigh && !laneTrig[i].isHigh()) {
					// keep the gate length, but at least 1 ms
					ln.push(std::max(now + ln.lastDelay, ln.lastOn + 1e-3), false);
				}
			} else if (ln.count) {
				ln.clear();
			}
			ln.process(now);
			outputs[LANE_OUTPUT + i].setVoltage(ln.out ? 10.f : 0.f);
		}

		outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(dt) ? 10.f : 0.f);
		outputs[LOOP_OUTPUT].setVoltage(loopPulse.process(dt) ? 10.f : 0.f);
		outputs[ACCENT_OUTPUT].setVoltage(accent);

		// screen
		dispGroove = gi;
		dispBpm = (float) clk.bpm();
		dispRunning = clk.running;
		dispSt = st;
	}

	// ---- patch storage: loaded grooves are saved inside the patch, so a patch
	// still works on a computer that doesn't have the files.
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppqn", json_integer(ppqnIndex));
		json_object_set_new(root, "base", json_integer(baseOverride));
		json_object_set_new(root, "lookahead", json_integer(lookIndex));
		GrooveBank* b = pending.load();
		if (!b) b = bank;
		json_t* arr = json_array();
		for (size_t i = b->builtins; i < b->list.size(); i++) {
			const Groove& g = b->list[i];
			json_t* o = json_object();
			json_object_set_new(o, "name", json_string(g.name.c_str()));
			size_t pi = i - b->builtins;
			json_object_set_new(o, "path", json_string(pi < b->paths.size() ? b->paths[pi].c_str() : ""));
			json_object_set_new(o, "length", json_real(g.length));
			json_object_set_new(o, "base", json_integer(g.base));
			json_object_set_new(o, "quantize", json_real(g.quantize));
			json_object_set_new(o, "timing", json_real(g.timing));
			json_object_set_new(o, "random", json_real(g.random));
			json_object_set_new(o, "velocity", json_real(g.velocity));
			json_t* notes = json_array();
			for (const Note& n : g.notes) {
				json_array_append_new(notes, json_real(n.t));
				json_array_append_new(notes, json_real(n.vel));
			}
			json_object_set_new(o, "notes", notes);
			json_array_append_new(arr, o);
		}
		json_object_set_new(root, "grooves", arr);
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppqn")) ppqnIndex = clamp((int) json_integer_value(j), 0, 5);
		if (json_t* j = json_object_get(root, "base")) baseOverride = clamp((int) json_integer_value(j), -1, NUM_BASES - 1);
		if (json_t* j = json_object_get(root, "lookahead")) lookIndex = clamp((int) json_integer_value(j), 0, 4);
		json_t* arr = json_object_get(root, "grooves");
		GrooveBank* nb = new GrooveBank;
		nb->list = builtinGrooves();
		nb->builtins = (int) nb->list.size();
		size_t i;
		json_t* o;
		json_array_foreach(arr, i, o) {
			Groove g;
			if (json_t* j = json_object_get(o, "name")) g.name = json_string_value(j);
			if (json_t* j = json_object_get(o, "length")) g.length = json_number_value(j);
			if (json_t* j = json_object_get(o, "base")) g.base = clamp((int) json_integer_value(j), 0, NUM_BASES - 1);
			if (json_t* j = json_object_get(o, "quantize")) g.quantize = json_number_value(j);
			if (json_t* j = json_object_get(o, "timing")) g.timing = json_number_value(j);
			if (json_t* j = json_object_get(o, "random")) g.random = json_number_value(j);
			if (json_t* j = json_object_get(o, "velocity")) g.velocity = json_number_value(j);
			json_t* notes = json_object_get(o, "notes");
			size_t n = json_array_size(notes);
			for (size_t k = 0; k + 1 < n; k += 2)
				g.notes.push_back({json_number_value(json_array_get(notes, k)), (float) json_number_value(json_array_get(notes, k + 1))});
			if (g.notes.empty()) continue;
			g.build();
			nb->list.push_back(g);
			json_t* p = json_object_get(o, "path");
			nb->paths.push_back(p ? json_string_value(p) : "");
		}
		installBank(nb);
	}
};

// ---------------------------------------------------------------- screen
struct GrooverDisplay : Widget {
	Groover* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		const float w = box.size.x, h = box.size.y;
		const NVGcolor orange = nvgRGB(255, 159, 28), cream = nvgRGB(239, 232, 216), soft = nvgRGB(159, 179, 181);

		// sample content for the module browser
		static std::vector<Groove> preview = builtinGrooves();
		const Groove* g = &preview[11];
		Settings st; st.base = g->base; st.velocity = 1.f;
		long step = -1;
		float bpm = 124.f;
		bool running = false;
		int gi = 11, total = (int) preview.size();
		bool user = false;
		if (module && module->bank) {
			GrooveBank* b = module->bank;
			gi = clamp((int) module->dispGroove, 0, (int) b->list.size() - 1);
			g = &b->list[gi];
			st = module->dispSt;
			step = module->dispStep;
			bpm = module->dispBpm;
			running = module->dispRunning;
			total = (int) b->list.size();
			user = gi >= b->builtins;
		}
		int n = g->steps(st.base);
		if (n <= 0) return;

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		const float pad = 4.f;
		const float top = mm2px(6.2f), bot = h - mm2px(2.2f);
		const float midY = top + (bot - top) * 0.42f;

		// how many steps to show: one pass of the groove, at most 32 at a time
		int show = std::min(n, 32);
		long page = 0;
		if (step >= 0) page = ((step % n) / show);
		int first = (int) (page * show);
		const float cw = (w - 2 * pad) / show;
		auto xOf = [&](float s) { return pad + cw * (s + 0.5f); };

		// beat lines
		double gb = BASE_BEATS[st.base];
		for (int s = 0; s < show; s++) {
			double beat = (first + s) * gb;
			bool onBeat = std::fabs(beat - std::round(beat)) < 1e-6;
			bool onBar = onBeat && ((long) std::lround(beat)) % 4 == 0;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, xOf(s), top);
			nvgLineTo(args.vg, xOf(s), bot);
			nvgStrokeColor(args.vg, nvgTransRGBA(soft, onBar ? 110 : (onBeat ? 60 : 22)));
			nvgStrokeWidth(args.vg, onBar ? 0.9f : 0.6f);
			nvgStroke(args.vg);
		}
		// steps: velocity bar under, dot moved by the groove
		long cur = step >= 0 ? step % n : -1;
		for (int s = 0; s < show; s++) {
			int k = first + s;
			if (k >= n) break;
			float off = g->off[st.base][k] * st.timing * st.amount;
			off = clamp(off, -0.5f, 0.75f);
			float a = stepAccent(*g, st, k);
			bool now = (k == cur);
			NVGcolor c = now ? orange : nvgTransRGBA(cream, 200);
			// velocity bar
			float bh = (bot - midY - mm2px(1.4f)) * a;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, xOf(s) - cw * 0.28f, bot - bh, cw * 0.56f, bh);
			nvgFillColor(args.vg, now ? nvgTransRGBA(orange, 200) : nvgTransRGBA(soft, 90));
			nvgFill(args.vg);
			// hollow grid position + dot at the grooved position
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, xOf(s), midY, std::min(cw * 0.3f, 3.2f));
			nvgStrokeColor(args.vg, nvgTransRGBA(soft, 90));
			nvgStrokeWidth(args.vg, 0.6f);
			nvgStroke(args.vg);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, xOf(s + off), midY, std::min(cw * 0.3f, 3.2f));
			nvgFillColor(args.vg, std::fabs(off) > 0.01f ? (now ? orange : nvgRGB(255, 190, 110)) : c);
			nvgFill(args.vg);
		}

		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(3.0f));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, cream);
			std::string name = string::f("%d/%d %s%s", gi + 1, total, user ? "* " : "", g->name.c_str());
			if (name.size() > 30) name = name.substr(0, 29) + "…";
			nvgText(args.vg, pad, mm2px(4.4f), name.c_str(), NULL);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, soft);
			std::string info = string::f("%s %gb %s", BASE_NAMES[st.base], g->length,
			                             running ? string::f("%.0f BPM", bpm).c_str() : "no clock");
			if (n > show) info = string::f("p%ld/%d ", page + 1, (n + show - 1) / show) + info;
			nvgText(args.vg, w - pad, mm2px(4.4f), info.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct LoadButton : VCVButton {
	void onDragEnd(const DragEndEvent& e) override {
		VCVButton::onDragEnd(e);
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		Groover* m = dynamic_cast<Groover*>(module);
		if (m) m->loadFileDialog();
	}
};

struct GrooverWidget : ModuleWidget {
	GrooverWidget(Groover* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Groover.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		GrooverDisplay* d = createWidget<GrooverDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		const int row1[3] = {Groover::GROOVE_PARAM, Groover::AMOUNT_PARAM, Groover::TIMING_PARAM};
		const int row2[3] = {Groover::VELOCITY_PARAM, Groover::RANDOM_PARAM, Groover::QUANTIZE_PARAM};
		for (int i = 0; i < 3; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(KX[i], ROW1_Y)), module, row1[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CVX[i], ROW1_Y)), module, Groover::GROOVE_INPUT + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(KX[i], ROW2_Y)), module, row2[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CVX[i], ROW2_Y)), module, Groover::VELOCITY_INPUT + i));
		}
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CLK_X, IO_Y)), module, Groover::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RST_X, IO_Y)), module, Groover::RESET_INPUT));
		addParam(createParamCentered<LoadButton>(mm2px(Vec(LOAD_X, IO_Y)), module, Groover::LOAD_PARAM));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OCLK_X, IO_Y)), module, Groover::CLOCK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OACC_X, IO_Y)), module, Groover::ACCENT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OLOOP_X, IO_Y)), module, Groover::LOOP_OUTPUT));
		for (int i = 0; i < NUM_LANES; i++) {
			float x = LANE_X0 + LANE_DX * i;
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, LIN_Y)), module, Groover::LANE_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, LOUT_Y)), module, Groover::LANE_OUTPUT + i));
		}
	}

	// drop .agr / .mid files (or a folder's worth) straight onto the module
	void onPathDrop(const PathDropEvent& e) override {
		Groover* m = dynamic_cast<Groover*>(module);
		if (!m) return;
		std::vector<std::string> files;
		for (const std::string& p : e.paths) {
			if (system::isDirectory(p)) {
				for (const std::string& f : system::getEntries(p)) {
					std::string ext = string::lowercase(system::getExtension(f));
					if (ext == ".agr" || ext == ".mid" || ext == ".midi") files.push_back(f);
				}
			} else files.push_back(p);
		}
		std::sort(files.begin(), files.end());
		if (files.empty()) return;
		std::string errs = m->addFiles(files, false);
		if (!errs.empty()) osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Groover couldn't load:\n" + errs).c_str());
		e.consume(this);
	}

	void appendContextMenu(Menu* menu) override {
		Groover* m = dynamic_cast<Groover*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Grooves (you can also drop files on the panel)"));
		menu->addChild(createMenuItem("Load groove file (.agr / .mid)…", "", [=]() { m->loadFileDialog(); }));
		menu->addChild(createMenuItem("Load a folder of grooves…", "replaces loaded ones", [=]() { m->loadFolderDialog(); }));
		menu->addChild(createMenuItem("Remove loaded grooves", "", [=]() { m->clearLoaded(); }, m->bank->list.size() <= (size_t) m->bank->builtins));
		menu->addChild(createMenuItem("Save this groove as .agr for Live…", "with the knob settings", [=]() { m->saveAgrDialog(); }));
		menu->addChild(createMenuItem("Show the built-in grooves as .agr files", "drag them into Live", [=]() { system::openDirectory(asset::plugin(pluginInstance, "grooves")); }));
		menu->addChild(createMenuItem("Copy this groove's own settings to the knobs", "Timing / Random / Velocity / Quantize", [=]() { m->useGrooveSettings(); }));
		menu->addChild(createSubmenuItem("Choose groove", "", [=](Menu* sub) {
			GrooveBank* b = m->bank;
			int n = (int) b->list.size();
			int cur = m->indexFor(m->params[Groover::GROOVE_PARAM].getValue());
			for (int i = 0; i < n; i++) {
				std::string label = string::f("%d. %s%s", i + 1, i >= b->builtins ? "* " : "", b->list[i].name.c_str());
				sub->addChild(createCheckMenuItem(label, "", [=]() { return cur == i; },
				                                  [=]() { m->params[Groover::GROOVE_PARAM].setValue((i + 0.5f) / n); }));
			}
		}));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock input", {"1 PPQN (quarters)", "2 PPQN (8ths)", "4 PPQN (16ths)", "8 PPQN (32nds)", "24 PPQN (MIDI clock)", "48 PPQN"},
		                                      [=]() { return m->ppqnIndex; }, [=](size_t i) { m->ppqnIndex = (int) i; }));
		menu->addChild(createIndexSubmenuItem("Base (grid the groove works on)", {"From the groove", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"},
		                                      [=]() { return (size_t) (m->baseOverride + 1); }, [=](size_t i) { m->baseOverride = (int) i - 1; }));
		menu->addChild(createIndexSubmenuItem("Lane look-ahead (lets lane hits land early)", {"Off (early hits land on the grid)", "5 ms", "10 ms", "20 ms", "40 ms"},
		                                      [=]() { return (size_t) m->lookIndex; }, [=](size_t i) { m->lookIndex = (int) i; }));
	}
};

Model* modelGroover = createModel<Groover, GrooverWidget>("Groover");
