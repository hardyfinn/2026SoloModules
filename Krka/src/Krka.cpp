// Krka: four function generators with a library of 260 shapes, chains,
// assignable CV and 18 presets. Every parameter has its own knob, so there
// are no menus. Functionally modelled on a quad complex envelope generator
// (XAOC Zadar); the shapes, code and panel are our own.
//
// Per channel (rows A-D): TRIG in + FIRE button, BANK (A-Z) and SHAPE (0-9)
// pick one of 260 shapes, REV plays it backwards, TIME 0.85 ms - 30 min (or
// note values with CLOCK patched), RESP bends the level axis, WARP bends the
// time axis, LEVEL, GATE (hold at the shape's peak while the gate is high),
// ANALOG retrigger (restart from where it is instead of jumping), PHASE (in
// a chain, start this far through the channel before) and REPEAT, DEST + DEPTH route the CV input to one of eight
// parameters, OUT 0-10 V and END trigger.
// Global: ALGO picks one of 16 chains, CLOCK, RESET, PRESET + LOAD / SAVE.
#include "plugin.hpp"
#include "KrkaDSP.hpp"
#include "KrkaLayout.hpp"

using namespace krka;

enum Dest { D_SHAPE, D_REV, D_TIME, D_LEVEL, D_RESP, D_WARP, D_PHASE, D_REPEAT, NUM_DESTS };
static const char* const DEST_NAMES[NUM_DESTS] = {"SHAPE", "REV", "TIME", "LEVEL", "RESP", "WARP", "PHASE", "REPEAT"};
static const int NUM_PRESETS = 18;
static const int REPEAT_FOREVER = 101;

// per-channel parameter offsets
enum ChParam { P_BANK, P_NUM, P_REV, P_TIME, P_RESP, P_WARP, P_LEVEL, P_GATE, P_ANALOG, P_PHASE, P_REPEAT, P_DEST, P_DEPTH, NPC };

struct BankQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int b = clamp((int) std::round(getValue()), 0, NUM_BANKS - 1);
		return string::f("%c  %s", 'A' + b, BANK_NAMES[b]);
	}
};
struct DestQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return DEST_NAMES[clamp((int) std::round(getValue()), 0, NUM_DESTS - 1)];
	}
};
struct RepeatQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int r = (int) std::round(getValue());
		if (r >= REPEAT_FOREVER) return "forever";
		if (r == 0) return "play once";
		return string::f("%d more", r);
	}
};
struct TimeQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float s = knobSeconds(getValue());
		if (s < 1.f) return string::f("%.3g ms", s * 1000.f);
		if (s < 60.f) return string::f("%.3g s", s);
		return string::f("%.3g min", s / 60.f);
	}
};
struct AlgoQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int a = clamp((int) std::round(getValue()), 0, NUM_ALGOS - 1);
		return string::f("%d  %s", a + 1, ALGOS[a].name);
	}
};

static std::string timeText(float sec) {
	if (sec < 0.01f) return string::f("%.2f ms", sec * 1000.f);
	if (sec < 1.f) return string::f("%d ms", (int) std::round(sec * 1000.f));
	if (sec < 10.f) return string::f("%.2f s", sec);
	if (sec < 60.f) return string::f("%.1f s", sec);
	return string::f("%.1f min", sec / 60.f);
}

struct Krka : Module {
	enum ParamIds {
		CH_PARAMS = 0,
		ALGO_PARAM = CH_PARAMS + NUM_CH * NPC,
		NUM_STORED,                         // everything above goes in presets
		PRESET_PARAM = NUM_STORED,
		LOAD_PARAM, SAVE_PARAM,
		FIRE_PARAM,
		NUM_PARAMS = FIRE_PARAM + NUM_CH
	};
	enum InputIds {
		TRIG_INPUT,
		CV_INPUT = TRIG_INPUT + NUM_CH,
		ALGO_INPUT = CV_INPUT + NUM_CH,
		CLOCK_INPUT, RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		END_OUTPUT = OUT_OUTPUT + NUM_CH,
		NUM_OUTPUTS = END_OUTPUT + NUM_CH
	};
	enum LightIds {
		REV_LIGHT,
		GATE_LIGHT = REV_LIGHT + NUM_CH,
		ANALOG_LIGHT = GATE_LIGHT + NUM_CH,
		FIRE_LIGHT = ANALOG_LIGHT + NUM_CH,
		LOAD_LIGHT = FIRE_LIGHT + NUM_CH,
		SAVE_LIGHT,
		CLOCK_LIGHT,
		NUM_LIGHTS
	};

	Channel ch[NUM_CH];
	ChSettings set[NUM_CH];
	dsp::SchmittTrigger trig[NUM_CH], clockTrig, resetTrig, loadTrig, saveTrig;
	dsp::PulseGenerator endPulse[NUM_CH];
	unsigned startedMask = 0;
	int algo = 0;

	// clock
	int ppqn = 1;
	int64_t sampleCount = 0, lastClock = -1;
	float beatSec = 0.f;
	float clockLight = 0.f;
	bool ignoreClock[NUM_CH] = {false, false, false, false};

	// presets
	float presetVal[NUM_PRESETS][NUM_STORED];
	bool presetUsed[NUM_PRESETS] = {};
	int lastPreset = -1;
	float saveFlash = 0.f;

	// for the screen
	int dispDest[NUM_CH] = {};
	bool dispSynced[NUM_CH] = {};
	int dispDiv[NUM_CH] = {};

	int pid(int c, int p) const { return CH_PARAMS + c * NPC + p; }

	Krka() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const char* defaults[NUM_CH] = {"A", "B", "C", "D"};
		for (int c = 0; c < NUM_CH; c++) {
			std::string n = defaults[c];
			configParam<BankQuantity>(pid(c, P_BANK), 0.f, NUM_BANKS - 1, 0.f, n + " bank");
			configParam(pid(c, P_NUM), 0.f, SHAPES_PER_BANK - 1, (float) (c * 2), n + " shape");
			paramQuantities[pid(c, P_BANK)]->snapEnabled = true;
			paramQuantities[pid(c, P_NUM)]->snapEnabled = true;
			configSwitch(pid(c, P_REV), 0.f, 1.f, 0.f, n + " reverse", {"Forwards", "Backwards"});
			configParam<TimeQuantity>(pid(c, P_TIME), 0.f, 1.f, 0.55f, n + " time");
			configParam(pid(c, P_RESP), -1.f, 1.f, 0.f, n + " response (bends the level axis)", "%", 0.f, 100.f);
			configParam(pid(c, P_WARP), -1.f, 1.f, 0.f, n + " warp (bends the time axis)", "%", 0.f, 100.f);
			configParam(pid(c, P_LEVEL), 0.f, 1.f, 1.f, n + " level", "%", 0.f, 100.f);
			configSwitch(pid(c, P_GATE), 0.f, 1.f, 0.f, n + " gate mode", {"One-shot", "Gate: hold at the peak while TRIG is high"});
			configSwitch(pid(c, P_ANALOG), 0.f, 1.f, 0.f, n + " retrigger", {"Digital: restart from the beginning", "Analog: restart from where it is"});
			configParam(pid(c, P_PHASE), 0.f, 1.f, 1.f, n + " phase (starts this far through the previous channel in a chain)", "%", 0.f, 100.f);
			configParam<RepeatQuantity>(pid(c, P_REPEAT), 0.f, (float) REPEAT_FOREVER, 0.f, n + " repeat");
			paramQuantities[pid(c, P_REPEAT)]->snapEnabled = true;
			configParam<DestQuantity>(pid(c, P_DEST), 0.f, NUM_DESTS - 1, (float) D_TIME, n + " CV destination");
			paramQuantities[pid(c, P_DEST)]->snapEnabled = true;
			configParam(pid(c, P_DEPTH), -1.f, 1.f, 0.f, n + " CV depth", "%", 0.f, 100.f);
			configButton(FIRE_PARAM + c, n + " fire (manual trigger / gate)");
			configInput(TRIG_INPUT + c, n + " trigger / gate");
			configInput(CV_INPUT + c, n + " CV (to DEST)");
			configOutput(OUT_OUTPUT + c, n + " envelope (0-10 V)");
			configOutput(END_OUTPUT + c, n + " end trigger");
		}
		// a useful starting spread: A short perc, B a pluck, C a swell, D an LFO wave
		params[pid(0, P_BANK)].setValue(1);
		params[pid(1, P_BANK)].setValue(0);
		params[pid(2, P_BANK)].setValue(12);
		params[pid(3, P_BANK)].setValue(5);
		params[pid(3, P_NUM)].setValue(0);
		params[pid(3, P_REPEAT)].setValue(REPEAT_FOREVER);
		configParam<AlgoQuantity>(ALGO_PARAM, 0.f, NUM_ALGOS - 1, 0.f, "Chain algorithm");
		paramQuantities[ALGO_PARAM]->snapEnabled = true;
		configParam(PRESET_PARAM, 1.f, NUM_PRESETS, 1.f, "Preset");
		paramQuantities[PRESET_PARAM]->snapEnabled = true;
		configButton(LOAD_PARAM, "Load preset");
		configButton(SAVE_PARAM, "Save preset");
		configInput(ALGO_INPUT, "Algorithm CV (1 V = 1.6 algorithms)");
		configInput(CLOCK_INPUT, "Clock (TIME becomes note values)");
		configInput(RESET_INPUT, "Reset (stops every channel)");
		for (int i = 0; i < NUM_PRESETS; i++)
			for (int p = 0; p < NUM_STORED; p++)
				presetVal[i][p] = 0.f;
	}

	void onReset() override {
		for (int i = 0; i < NUM_PRESETS; i++) presetUsed[i] = false;
		for (int c = 0; c < NUM_CH; c++) { ch[c] = Channel(); ignoreClock[c] = false; }
		lastPreset = -1;
		ppqn = 1;
	}

	// Start channel c; followers whose PHASE is 0 start with it. Each channel
	// starts at most once per sample, so loops can't run away.
	void startChannel(int c) {
		if (startedMask & (1u << c)) return;
		startedMask |= 1u << c;
		if (!ch[c].trigger(set[c])) return;
		unsigned m = ALGOS[algo].next[c];
		for (int f = 0; f < NUM_CH; f++)
			if ((m & (1u << f)) && set[f].phase <= 0.f) startChannel(f);
	}

	void process(const ProcessArgs& args) override {
		float sr = args.sampleRate;
		sampleCount++;

		// ---- clock
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (lastClock >= 0) {
				float p = (sampleCount - lastClock) / sr;
				if (p > 0.001f && p < 20.f) beatSec = p * ppqn;
			}
			lastClock = sampleCount;
			clockLight = 1.f;
		}
		clockLight = std::max(0.f, clockLight - args.sampleTime * 10.f);
		bool clocked = inputs[CLOCK_INPUT].isConnected() && beatSec > 0.f;
		if (!inputs[CLOCK_INPUT].isConnected()) { lastClock = -1; beatSec = 0.f; }

		// ---- presets
		int slot = clamp((int) std::round(params[PRESET_PARAM].getValue()) - 1, 0, NUM_PRESETS - 1);
		if (saveTrig.process(params[SAVE_PARAM].getValue())) {
			for (int p = 0; p < NUM_STORED; p++) presetVal[slot][p] = params[p].getValue();
			presetUsed[slot] = true;
			lastPreset = slot;
			saveFlash = 1.f;
		}
		if (loadTrig.process(params[LOAD_PARAM].getValue()) && presetUsed[slot]) {
			for (int p = 0; p < NUM_STORED; p++) params[p].setValue(presetVal[slot][p]);
			lastPreset = slot;
		}
		saveFlash = std::max(0.f, saveFlash - args.sampleTime * 3.f);
		lights[SAVE_LIGHT].setBrightness(saveFlash);
		lights[LOAD_LIGHT].setBrightness(presetUsed[slot] ? (slot == lastPreset ? 1.f : 0.25f) : 0.f);

		// ---- algorithm
		algo = clamp((int) std::round(params[ALGO_PARAM].getValue() + inputs[ALGO_INPUT].getVoltage() * 1.6f), 0, NUM_ALGOS - 1);

		// ---- settings (knob + CV)
		for (int c = 0; c < NUM_CH; c++) {
			ChSettings& s = set[c];
			int dest = clamp((int) std::round(params[pid(c, P_DEST)].getValue()), 0, NUM_DESTS - 1);
			float cv = inputs[CV_INPUT + c].isConnected() ? inputs[CV_INPUT + c].getVoltage() * params[pid(c, P_DEPTH)].getValue() : 0.f;
			int shape = (int) std::round(params[pid(c, P_BANK)].getValue()) * SHAPES_PER_BANK + (int) std::round(params[pid(c, P_NUM)].getValue());
			if (dest == D_SHAPE) shape += (int) std::round(cv * SHAPES_PER_BANK);  // 1 V = one bank
			s.shape = clamp(shape, 0, NUM_SHAPES - 1);
			bool rev = params[pid(c, P_REV)].getValue() > 0.5f;
			if (dest == D_REV && cv > 0.5f) rev = !rev;  // comparator; DEPTH sets how easily it flips
			s.reverse = rev;
			float tk = params[pid(c, P_TIME)].getValue();
			bool sync = clocked && !ignoreClock[c];
			if (sync) {
				int idx = (int) std::round(tk * (NUM_DIVS - 1));
				if (dest == D_TIME) idx += (int) std::round(cv * 2.f);  // about an octave per volt
				idx = clamp(idx, 0, NUM_DIVS - 1);
				s.time = DIV_BEATS[idx] * beatSec;
				dispDiv[c] = idx;
			}
			else {
				s.time = knobSeconds(tk) * (dest == D_TIME ? std::exp2(cv) : 1.f);  // 1 V/oct, + = longer
				s.time = clampf(s.time, 0.0002f, 7200.f);
			}
			dispSynced[c] = sync;
			s.resp = params[pid(c, P_RESP)].getValue() + (dest == D_RESP ? cv * 0.2f : 0.f);
			s.warp = params[pid(c, P_WARP)].getValue() + (dest == D_WARP ? cv * 0.2f : 0.f);
			s.level = clampf(params[pid(c, P_LEVEL)].getValue() + (dest == D_LEVEL ? cv * 0.1f : 0.f), 0.f, 1.f);
			s.gateMode = params[pid(c, P_GATE)].getValue() > 0.5f;
			s.analog = params[pid(c, P_ANALOG)].getValue() > 0.5f;
			s.phase = clampf(params[pid(c, P_PHASE)].getValue() + (dest == D_PHASE ? cv * 0.1f : 0.f), 0.f, 1.f);
			int rep = (int) std::round(params[pid(c, P_REPEAT)].getValue());
			if (rep >= REPEAT_FOREVER) s.repeats = -1;
			else s.repeats = clamp(rep + (dest == D_REPEAT ? (int) std::round(cv * 10.f) : 0), 0, REPEAT_FOREVER - 1);
			dispDest[c] = dest;
		}

		// ---- reset
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			for (int c = 0; c < NUM_CH; c++) ch[c].stop();

		// ---- triggers
		startedMask = 0;
		bool gate[NUM_CH];
		for (int c = 0; c < NUM_CH; c++) {
			float v = std::max(inputs[TRIG_INPUT + c].getVoltage(), params[FIRE_PARAM + c].getValue() * 10.f);
			bool edge = trig[c].process(v, 0.1f, 1.f);
			gate[c] = trig[c].isHigh();
			if (edge && takesOwnTrigger(algo, c)) startChannel(c);
		}

		// ---- run
		for (int c = 0; c < NUM_CH; c++) {
			Step st = ch[c].process(set[c], args.sampleTime, gate[c]);
			unsigned m = ALGOS[algo].next[c];
			for (int f = 0; f < NUM_CH; f++)
				if ((m & (1u << f)) && st.passed(set[f].phase)) startChannel(f);
			if (st.end) endPulse[c].trigger(1e-3f);
		}
		for (int c = 0; c < NUM_CH; c++) {
			outputs[OUT_OUTPUT + c].setVoltage(10.f * set[c].level * ch[c].value);
			outputs[END_OUTPUT + c].setVoltage(endPulse[c].process(args.sampleTime) ? 10.f : 0.f);
			lights[REV_LIGHT + c].setBrightness(set[c].reverse ? 1.f : 0.f);
			lights[GATE_LIGHT + c].setBrightness(set[c].gateMode ? 1.f : 0.f);
			lights[ANALOG_LIGHT + c].setBrightness(set[c].analog ? 1.f : 0.f);
			lights[FIRE_LIGHT + c].setSmoothBrightness(ch[c].running ? 0.3f + 0.7f * ch[c].value : 0.f, args.sampleTime);
		}
		lights[CLOCK_LIGHT].setBrightness(clockLight);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* ps = json_array();
		for (int i = 0; i < NUM_PRESETS; i++) {
			if (!presetUsed[i]) { json_array_append_new(ps, json_null()); continue; }
			json_t* o = json_object();
			for (int p = 0; p < NUM_STORED; p++)
				json_object_set_new(o, string::f("p%d", p).c_str(), json_real(presetVal[i][p]));
			json_array_append_new(ps, o);
		}
		json_object_set_new(root, "presets", ps);
		json_object_set_new(root, "lastPreset", json_integer(lastPreset));
		json_object_set_new(root, "ppqn", json_integer(ppqn));
		json_t* ig = json_array();
		for (int c = 0; c < NUM_CH; c++) json_array_append_new(ig, json_boolean(ignoreClock[c]));
		json_object_set_new(root, "ignoreClock", ig);
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* ps = json_object_get(root, "presets");
		if (ps) {
			for (int i = 0; i < NUM_PRESETS && i < (int) json_array_size(ps); i++) {
				json_t* o = json_array_get(ps, i);
				presetUsed[i] = json_is_object(o);
				if (!presetUsed[i]) continue;
				for (int p = 0; p < NUM_STORED; p++) {
					json_t* v = json_object_get(o, string::f("p%d", p).c_str());
					presetVal[i][p] = v ? (float) json_number_value(v) : params[p].getValue();
				}
			}
		}
		json_t* lp = json_object_get(root, "lastPreset");
		if (lp) lastPreset = (int) json_integer_value(lp);
		json_t* pq = json_object_get(root, "ppqn");
		if (pq) ppqn = std::max(1, (int) json_integer_value(pq));
		json_t* ig = json_object_get(root, "ignoreClock");
		if (ig)
			for (int c = 0; c < NUM_CH && c < (int) json_array_size(ig); c++)
				ignoreClock[c] = json_is_true(json_array_get(ig, c));
	}
};

// The screen: one tile per channel with its shape (as bent by REV, WARP and
// RESP), a playhead, and the time / repeat / mode; a status line underneath.
struct KrkaDisplay : Widget {
	Krka* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		const NVGcolor coral = nvgRGB(255, 138, 91), cream = nvgRGB(243, 235, 221), teal = nvgRGB(120, 205, 200);
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		float w = box.size.x, h = box.size.y;
		float pad = mm2px(1.4f);
		float statusH = mm2px(4.2f);
		float tw = (w - pad) / NUM_CH;
		for (int c = 0; c < NUM_CH; c++) {
			ChSettings s;
			float ph = 0.f;
			bool running = false, synced = false;
			int dest = D_TIME, div = 0;
			if (module) {
				s = module->set[c];
				ph = module->ch[c].ph;
				running = module->ch[c].running;
				synced = module->dispSynced[c];
				dest = module->dispDest[c];
				div = module->dispDiv[c];
			}
			else {
				const int demo[NUM_CH] = {13, 10, 120, 50};
				s.shape = demo[c];
				s.repeats = c == 3 ? -1 : 0;
				ph = 0.3f + 0.15f * c;
				running = true;
			}
			float x0 = pad + c * tw, x1 = x0 + tw - pad;
			float ty = mm2px(3.4f);
			float cy0 = mm2px(4.6f), cy1 = h - statusH - mm2px(4.4f);
			// frame
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, x0, pad * 0.5f, x1 - x0, h - statusH - pad * 0.5f, 3.f);
			nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 8));
			nvgFill(args.vg);
			// curve
			const int N = 96;
			nvgBeginPath(args.vg);
			for (int i = 0; i <= N; i++) {
				float x = (float) i / N;
				float v = shapeOut(s, x);
				float px = x0 + mm2px(1.f) + (x1 - x0 - mm2px(2.f)) * x;
				float py = cy1 - (cy1 - cy0) * v * (0.25f + 0.75f * s.level);
				if (i == 0) nvgMoveTo(args.vg, px, py);
				else nvgLineTo(args.vg, px, py);
			}
			nvgStrokeColor(args.vg, coral);
			nvgStrokeWidth(args.vg, 1.3f);
			nvgLineJoin(args.vg, NVG_ROUND);
			nvgStroke(args.vg);
			if (running) {
				float v = shapeOut(s, ph);
				float px = x0 + mm2px(1.f) + (x1 - x0 - mm2px(2.f)) * ph;
				float py = cy1 - (cy1 - cy0) * v * (0.25f + 0.75f * s.level);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, px, cy0);
				nvgLineTo(args.vg, px, cy1);
				nvgStrokeColor(args.vg, nvgRGBA(243, 235, 221, 60));
				nvgStrokeWidth(args.vg, 1.f);
				nvgStroke(args.vg);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, px, py, 2.2f);
				nvgFillColor(args.vg, cream);
				nvgFill(args.vg);
			}
			if (font) {
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, mm2px(2.9f));
				nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
				nvgFillColor(args.vg, coral);
				std::string head = string::f("%c ", 'A' + c);
				nvgText(args.vg, x0 + mm2px(1.f), ty, head.c_str(), NULL);
				nvgFillColor(args.vg, cream);
				std::string nm = string::f("  %c%d %s", 'A' + s.shape / SHAPES_PER_BANK, s.shape % SHAPES_PER_BANK, SHAPES[s.shape].name);
				nvgText(args.vg, x0 + mm2px(1.f), ty, nm.c_str(), NULL);
				std::string t = synced ? DIV_NAMES[div] : timeText(s.time);
				if (s.repeats < 0) t += "  LOOP";
				else if (s.repeats > 0) t += string::f("  x%d", s.repeats + 1);
				if (s.reverse) t += "  REV";
				if (s.gateMode) t += "  GATE";
				if (s.analog) t += "  AN";
				nvgFillColor(args.vg, teal);
				nvgFontSize(args.vg, mm2px(2.6f));
				nvgText(args.vg, x0 + mm2px(1.f), h - statusH - mm2px(1.2f), t.c_str(), NULL);
				nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
				nvgFillColor(args.vg, nvgRGBA(120, 205, 200, 150));
				std::string d = std::string("CV>") + DEST_NAMES[dest];
				nvgText(args.vg, x1 - mm2px(1.f), ty, d.c_str(), NULL);
			}
		}
		if (font) {
			int algo = module ? module->algo : 4;
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.9f));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, coral);
			std::string a = string::f("CHAIN %d  %s", algo + 1, ALGOS[algo].name);
			nvgText(args.vg, pad + mm2px(1.f), h - mm2px(1.0f), a.c_str(), NULL);
			std::string r;
			if (module) {
				int slot = clamp((int) std::round(module->params[Krka::PRESET_PARAM].getValue()) - 1, 0, NUM_PRESETS - 1);
				r = string::f("PRESET %d%s", slot + 1, module->presetUsed[slot] ? (slot == module->lastPreset ? " (loaded)" : " (saved)") : " (empty)");
				if (module->beatSec > 0.f && module->inputs[Krka::CLOCK_INPUT].isConnected())
					r = string::f("CLOCK %.1f BPM   ", 60.f / module->beatSec) + r;
			}
			else r = "PRESET 1 (empty)";
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, cream);
			nvgText(args.vg, w - pad - mm2px(1.f), h - mm2px(1.0f), r.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct KrkaWidget : ModuleWidget {
	KrkaWidget(Krka* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Krka.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		KrkaDisplay* d = createWidget<KrkaDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		for (int c = 0; c < NUM_CH; c++) {
			float y = ROW_Y[c];
			auto P = [&](int p) { return Krka::CH_PARAMS + c * NPC + p; };
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLX[0], y)), module, Krka::TRIG_INPUT + c));
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(COLX[1], y)), module, Krka::FIRE_PARAM + c, Krka::FIRE_LIGHT + c));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[2], y)), module, P(P_BANK)));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[3], y)), module, P(P_NUM)));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COLX[4], y)), module, P(P_REV), Krka::REV_LIGHT + c));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[5], y)), module, P(P_TIME)));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[6], y)), module, P(P_RESP)));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[7], y)), module, P(P_WARP)));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[8], y)), module, P(P_LEVEL)));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(COLX[9], y)), module, P(P_GATE), Krka::GATE_LIGHT + c));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(COLX[10], y)), module, P(P_ANALOG), Krka::ANALOG_LIGHT + c));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[11], y)), module, P(P_PHASE)));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[12], y)), module, P(P_REPEAT)));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COLX[13], y)), module, P(P_DEST)));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(COLX[14], y)), module, P(P_DEPTH)));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLX[15], y)), module, Krka::CV_INPUT + c));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLX[16], y)), module, Krka::OUT_OUTPUT + c));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLX[17], y)), module, Krka::END_OUTPUT + c));
		}
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(GCX, G_ALGO_Y)), module, Krka::ALGO_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GCX, G_ALGOCV_Y)), module, Krka::ALGO_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GX[0], G_CLOCK_Y)), module, Krka::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(GX[1], G_CLOCK_Y)), module, Krka::RESET_INPUT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(GX[0] + 5.6f, G_CLOCK_Y - 4.6f)), module, Krka::CLOCK_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(GCX, G_PRESET_Y)), module, Krka::PRESET_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(GX[0], G_BTN_Y)), module, Krka::LOAD_PARAM, Krka::LOAD_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(GX[1], G_BTN_Y)), module, Krka::SAVE_PARAM, Krka::SAVE_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Krka* module = dynamic_cast<Krka*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Clock"));
		const int ppqns[4] = {1, 2, 4, 24};
		for (int i = 0; i < 4; i++) {
			int q = ppqns[i];
			menu->addChild(createCheckMenuItem(string::f("%d pulse%s per beat", q, q == 1 ? "" : "s"), "",
				[=]() { return module->ppqn == q; }, [=]() { module->ppqn = q; module->beatSec = 0.f; module->lastClock = -1; }));
		}
		for (int c = 0; c < NUM_CH; c++)
			menu->addChild(createBoolPtrMenuItem(string::f("Channel %c ignores the clock (free time)", 'A' + c), "", &module->ignoreClock[c]));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Presets"));
		for (int i = 0; i < NUM_PRESETS; i++) {
			if (!module->presetUsed[i]) continue;
			menu->addChild(createMenuItem(string::f("Clear preset %d", i + 1), "", [=]() {
				module->presetUsed[i] = false;
				if (module->lastPreset == i) module->lastPreset = -1;
			}));
		}
		menu->addChild(createMenuItem("Clear all presets", "", [=]() {
			for (int i = 0; i < NUM_PRESETS; i++) module->presetUsed[i] = false;
			module->lastPreset = -1;
		}));
	}
};

Model* modelKrka = createModel<Krka, KrkaWidget>("Krka");
