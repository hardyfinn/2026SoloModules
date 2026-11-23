// Lilt: relabi generator.
//   Four LFOs at ratios of a felt PULSE, summed; four thresholds on the sum
//   give TRIG and GATE outputs that lean towards the beat and away again.
//   DRIFT detunes the LFOs off their ratios, MUTATE keeps changing that,
//   GRID pulls triggers back onto 1/4-pulse ticks. CLOCK locks the pulse.
// The engine is in LiltDSP.hpp.
#include "plugin.hpp"
#include "LiltDSP.hpp"

using namespace lilt;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 14.5f, DISP_W = 170.88f, DISP_H = 19.0f;
static const float GX[4] = {14.0f, 38.0f, 62.0f, 86.0f};
static const float ROW1_Y = 50.0f;
static const float ROW2_Y = 73.0f;
static const float SNAP_X = 79.0f, DIR_X = 94.0f;
static const float CV_X0 = 9.0f, CV_DX = 17.4f;
static const float TRIM_Y = 92.5f;
static const float CVJ_Y = 101.0f;
static const float IO_Y = 117.0f;
static const float LX[4] = {115.5f, 134.0f, 152.5f, 171.0f};
static const float RATIO_Y = 48.5f;
static const float LEVEL_Y = 67.0f;
static const float LFOJ_Y = 79.6f;

static const int HIST = 360;  // screen points (8 pulses)
static const int PPQN_OPTS[4] = {1, 2, 4, 24};

struct Lilt : Module {
	enum ParamId {
		PULSE_PARAM, DRIFT_PARAM, MUTATE_PARAM, GRID_PARAM,
		SHAPE_PARAM, THRESH_PARAM, SPREAD_PARAM, SNAP_PARAM, DIR_PARAM,
		ENUMS(RATIO_PARAM, NUM_LFO),
		ENUMS(LEVEL_PARAM, NUM_LFO),
		ENUMS(ATT_PARAM, 6),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(CV_INPUT, 6),  // PULSE DRIFT MUTATE THRESH SPREAD GRID
		CLOCK_INPUT, RESET_INPUT, FREEZE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		BEAT_OUTPUT, SUM_OUTPUT, ACC_OUTPUT,
		ENUMS(LFO_OUTPUT, NUM_LFO),
		ENUMS(TRIG_OUTPUT, NUM_CH),
		ENUMS(GATE_OUTPUT, NUM_CH),
		OUTPUTS_LEN
	};
	enum LightId {
		BEAT_LIGHT, FREEZE_LIGHT,
		ENUMS(TRIG_LIGHT, NUM_CH),
		LIGHTS_LEN
	};

	RelabiEngine eng;
	RelabiControls ctl;
	RelabiFrame fr;
	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator trigPulse[NUM_CH], beatPulse;
	float trigFlash[NUM_CH] = {};
	float beatFlash = 0.f;

	// clock
	int ppqnIndex = 2;  // 4 PPQN
	float clockPeriod = 0.f, sinceClock = 0.f;
	int clockCount = 0;
	bool clockLive = false;
	uint32_t seedValue = 1;

	// screen history (written by the audio thread, read by the screen)
	float hSum[HIST] = {}, hLfo[NUM_LFO][HIST] = {};
	uint8_t hTrig[HIST] = {}, hBeat[HIST] = {};
	int hPos = 0;
	float hAcc = 0.f;
	uint8_t pendTrig = 0, pendBeat = 0;
	// screen readouts
	float dispBpm = 120.f, dispRatio[NUM_LFO] = {1.f, 0.75f, 1.5f, 1.f / 3.f};
	int dispSnap[NUM_LFO] = {8, 7, 11, 3};
	bool dispSnapOn = true, dispClock = false, dispFreeze = false;
	float dispThr[NUM_CH] = {-0.3f, -0.1f, 0.1f, 0.3f};
	float dispDrift = 0.2f, dispMutate = 0.f, dispGrid = 0.f;

	Lilt() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PULSE_PARAM, 20.f, 300.f, 120.f, "Pulse (the beat the LFOs lean on; ignored when CLOCK is patched)", " BPM");
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.2f, "Drift (pulls each LFO off its ratio, up to 15 %)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (keeps changing the drift, phases and levels)", "%", 0.f, 100.f);
		configParam(GRID_PARAM, 0.f, 1.f, 0.f, "Grid (pulls triggers onto 1/4-pulse ticks)", "%", 0.f, 100.f);
		configParam(SHAPE_PARAM, 0.f, 1.f, 0.f, "LFO shape (sine, triangle, soft square)", "%", 0.f, 100.f);
		configParam(THRESH_PARAM, -1.f, 1.f, 0.f, "Threshold (where the four trigger lines sit)", " V", 0.f, 5.f);
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.5f, "Spread (how far apart the four lines are)", "%", 0.f, 100.f);
		configSwitch(SNAP_PARAM, 0.f, 1.f, 1.f, "Snap ratios to musical fractions", {"Off", "On"});
		configSwitch(DIR_PARAM, 0.f, 2.f, 2.f, "Edge", {"Falling (sum drops through a line)", "Both (gates toggle)", "Rising (sum climbs through a line)"});
		const float defRatio[NUM_LFO] = {0.f, -0.415f, 0.585f, -1.585f};
		const float defLevel[NUM_LFO] = {1.f, 0.8f, 0.6f, 0.5f};
		for (int i = 0; i < NUM_LFO; i++) {
			configParam(RATIO_PARAM + i, -3.f, 3.f, defRatio[i], string::f("LFO %d ratio (cycles per pulse)", i + 1), "x", 2.f);
			configParam(LEVEL_PARAM + i, -1.f, 1.f, defLevel[i], string::f("LFO %d level in the sum (negative flips it)", i + 1), "%", 0.f, 100.f);
			configOutput(LFO_OUTPUT + i, string::f("LFO %d (±5 V)", i + 1));
		}
		const char* cvNames[6] = {"Pulse (1 V/oct)", "Drift", "Mutate", "Threshold", "Spread", "Grid"};
		for (int i = 0; i < 6; i++) {
			configParam(ATT_PARAM + i, -1.f, 1.f, 1.f, string::f("%s CV amount", cvNames[i]), "%", 0.f, 100.f);
			configInput(CV_INPUT + i, string::f("%s CV", cvNames[i]));
		}
		configInput(CLOCK_INPUT, "Clock (locks the pulse; PPQN in the right-click menu)");
		configInput(RESET_INPUT, "Reset (all LFOs back to the one)");
		configInput(FREEZE_INPUT, "Freeze (gate high holds MUTATE)");
		configOutput(BEAT_OUTPUT, "Beat (a trigger on every pulse)");
		configOutput(SUM_OUTPUT, "Sum of the LFOs (±5 V)");
		configOutput(ACC_OUTPUT, "Accent (0-10 V: how steeply the sum crossed at the last trigger)");
		for (int k = 0; k < NUM_CH; k++) {
			configOutput(TRIG_OUTPUT + k, string::f("Trigger %d", k + 1));
			configOutput(GATE_OUTPUT + k, string::f("Gate %d", k + 1));
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		ppqnIndex = 2;
		RelabiEngine fresh;
		eng = fresh;
	}

	void reroll() {
		seedValue = random::u32() | 1u;
		eng.seed(seedValue);
	}

	float cv(int i) {
		return inputs[CV_INPUT + i].getVoltage() * params[ATT_PARAM + i].getValue();
	}

	void process(const ProcessArgs& args) override {
		const float dt = args.sampleTime;

		// ---- clock
		clockLive = inputs[CLOCK_INPUT].isConnected();
		bool beatFromClock = false;
		bool clockBeat = false;
		sinceClock += dt;
		if (clockLive) {
			if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
				if (sinceClock < 4.f) clockPeriod = sinceClock;
				sinceClock = 0.f;
				int ppqn = PPQN_OPTS[ppqnIndex];
				clockCount = (clockCount + 1) % ppqn;
				if (eng.syncBeat((float) clockCount / ppqn) || clockCount == 0) clockBeat = true;
			}
			if (sinceClock > 4.f) clockPeriod = 0.f;
			beatFromClock = true;
		}

		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			eng.reset();
			clockCount = 0;
		}

		// ---- controls
		float pulseCv = cv(0);
		if (clockLive && clockPeriod > 0.f) {
			ctl.bpm = 60.f / (clockPeriod * PPQN_OPTS[ppqnIndex]);
			ctl.speed = std::pow(2.f, clamp(pulseCv, -4.f, 4.f));
		}
		else {
			ctl.bpm = clamp(params[PULSE_PARAM].getValue() * std::pow(2.f, clamp(pulseCv, -4.f, 4.f)), 5.f, 1200.f);
			ctl.speed = 1.f;
		}
		ctl.drift = clamp(params[DRIFT_PARAM].getValue() + cv(1) * 0.1f, 0.f, 1.f);
		ctl.mutate = clamp(params[MUTATE_PARAM].getValue() + cv(2) * 0.1f, 0.f, 1.f);
		ctl.thresh = clamp(params[THRESH_PARAM].getValue() + cv(3) * 0.2f, -1.2f, 1.2f);
		ctl.spread = clamp(params[SPREAD_PARAM].getValue() + cv(4) * 0.1f, 0.f, 1.f);
		ctl.grid = clamp(params[GRID_PARAM].getValue() + cv(5) * 0.1f, 0.f, 1.f);
		ctl.shape = params[SHAPE_PARAM].getValue();
		ctl.snap = params[SNAP_PARAM].getValue() > 0.5f;
		ctl.dir = 2 - (int) std::round(params[DIR_PARAM].getValue());
		ctl.freeze = inputs[FREEZE_INPUT].getVoltage() > 1.f;
		for (int i = 0; i < NUM_LFO; i++) {
			ctl.ratioLog2[i] = params[RATIO_PARAM + i].getValue();
			ctl.level[i] = params[LEVEL_PARAM + i].getValue();
		}

		eng.process(dt, ctl, beatFromClock, fr);
		bool beat = fr.pulse || clockBeat;

		// ---- outputs
		if (beat) { beatPulse.trigger(1e-3f); beatFlash = 0.1f; }
		outputs[BEAT_OUTPUT].setVoltage(beatPulse.process(dt) ? 10.f : 0.f);
		outputs[SUM_OUTPUT].setVoltage(fr.sum * 5.f);
		outputs[ACC_OUTPUT].setVoltage(fr.acc * 10.f);
		uint8_t tb = 0;
		for (int i = 0; i < NUM_LFO; i++) outputs[LFO_OUTPUT + i].setVoltage(fr.lfo[i] * 5.f);
		for (int k = 0; k < NUM_CH; k++) {
			if (fr.trig[k]) { trigPulse[k].trigger(1e-3f); trigFlash[k] = 0.08f; tb |= 1 << k; }
			outputs[TRIG_OUTPUT + k].setVoltage(trigPulse[k].process(dt) ? 10.f : 0.f);
			outputs[GATE_OUTPUT + k].setVoltage(fr.gate[k] ? 10.f : 0.f);
			trigFlash[k] = std::max(0.f, trigFlash[k] - dt);
			lights[TRIG_LIGHT + k].setBrightnessSmooth(trigFlash[k] > 0.f || fr.gate[k] ? (trigFlash[k] > 0.f ? 1.f : 0.25f) : 0.f, dt);
		}
		beatFlash = std::max(0.f, beatFlash - dt);
		lights[BEAT_LIGHT].setBrightnessSmooth(beatFlash > 0.f ? 1.f : 0.f, dt);
		lights[FREEZE_LIGHT].setBrightness(ctl.freeze ? 1.f : 0.f);

		// ---- screen history: 8 pulses across
		pendTrig |= tb;
		if (beat) pendBeat = 1;
		hAcc += dt * ctl.bpm / 60.f * (HIST / 8.f);
		if (hAcc >= 1.f) {
			hAcc -= std::floor(hAcc);
			hSum[hPos] = fr.sum;
			for (int i = 0; i < NUM_LFO; i++) hLfo[i][hPos] = fr.lfo[i];
			hTrig[hPos] = pendTrig;
			hBeat[hPos] = pendBeat;
			pendTrig = 0;
			pendBeat = 0;
			hPos = (hPos + 1) % HIST;
		}
		dispBpm = ctl.bpm;
		dispSnapOn = ctl.snap;
		dispClock = clockLive;
		dispFreeze = ctl.freeze;
		dispDrift = ctl.drift;
		dispMutate = ctl.mutate;
		dispGrid = ctl.grid;
		for (int i = 0; i < NUM_LFO; i++) {
			dispSnap[i] = snapIndex(ctl.ratioLog2[i]);
			dispRatio[i] = eng.ratioOf(ctl, i) * ctl.speed;
		}
		for (int k = 0; k < NUM_CH; k++) dispThr[k] = fr.thr[k];
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppqn", json_integer(ppqnIndex));
		json_t* d = json_array();
		for (int i = 0; i < NUM_LFO; i++) json_array_append_new(d, json_real(eng.detune[i]));
		json_object_set_new(root, "detune", d);
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* p = json_object_get(root, "ppqn");
		if (p) ppqnIndex = clamp((int) json_integer_value(p), 0, 3);
		json_t* d = json_object_get(root, "detune");
		if (d && json_is_array(d))
			for (int i = 0; i < NUM_LFO && i < (int) json_array_size(d); i++)
				eng.detune[i] = (float) json_number_value(json_array_get(d, i));
	}
};

struct LiltDisplay : Widget {
	Lilt* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		const NVGcolor chCol[NUM_CH] = {nvgRGB(255, 111, 89), nvgRGB(255, 193, 69), nvgRGB(91, 224, 179), nvgRGB(122, 184, 255)};
		const NVGcolor cream = nvgRGB(241, 233, 214), soft = nvgRGB(191, 176, 198);
		float w = box.size.x, h = box.size.y;
		float pad = mm2px(1.4f);
		float top = mm2px(1.4f), bot = h - mm2px(4.6f);
		auto yOf = [&](float v) { return top + (bot - top) * (1.f - (clamp(v, -1.25f, 1.25f) + 1.25f) / 2.5f); };
		auto xOf = [&](int i) { return pad + (w - 2 * pad) * i / (float) (HIST - 1); };

		// demo data for the module browser
		static float dSum[HIST], dLfo[NUM_LFO][HIST];
		static uint8_t dTrig[HIST], dBeat[HIST];
		const float* sum = dSum;
		const uint8_t* trig = dTrig;
		const uint8_t* beat = dBeat;
		int start = 0;
		float thr[NUM_CH] = {-0.3f, -0.1f, 0.1f, 0.3f};
		if (module) {
			sum = module->hSum; trig = module->hTrig; beat = module->hBeat;
			start = module->hPos;
			for (int k = 0; k < NUM_CH; k++) thr[k] = module->dispThr[k];
		}
		else {
			const float r[NUM_LFO] = {1.f, 0.75f, 1.5f, 1.f / 3.f}, l[NUM_LFO] = {1.f, 0.8f, 0.6f, 0.5f};
			bool ab[NUM_CH] = {};
			for (int i = 0; i < HIST; i++) {
				float t = i * 8.f / HIST, s = 0.f;
				for (int j = 0; j < NUM_LFO; j++) { dLfo[j][i] = std::sin(TWO_PI * r[j] * 1.04f * t); s += l[j] * dLfo[j][i]; }
				dSum[i] = s / 2.9f;
				dTrig[i] = 0;
				for (int k = 0; k < NUM_CH; k++) {
					bool a = dSum[i] > thr[k];
					if (a && !ab[k] && i > 0) dTrig[i] |= 1 << k;
					ab[k] = a;
				}
				dBeat[i] = (i % (HIST / 8)) == 0;
			}
		}

		// beat lines
		nvgStrokeWidth(args.vg, 0.6f);
		for (int i = 0; i < HIST; i++) {
			if (!beat[(start + i) % HIST]) continue;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, xOf(i), top);
			nvgLineTo(args.vg, xOf(i), bot);
			nvgStrokeColor(args.vg, nvgRGBA(255, 193, 69, 45));
			nvgStroke(args.vg);
		}
		// threshold lines
		for (int k = 0; k < NUM_CH; k++) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, pad, yOf(thr[k]));
			nvgLineTo(args.vg, w - pad, yOf(thr[k]));
			nvgStrokeColor(args.vg, nvgTransRGBA(chCol[k], 90));
			nvgStrokeWidth(args.vg, 0.7f);
			nvgStroke(args.vg);
		}
		// the four LFOs, faint
		for (int j = 0; j < NUM_LFO; j++) {
			const float* L = module ? module->hLfo[j] : dLfo[j];
			nvgBeginPath(args.vg);
			for (int i = 0; i < HIST; i++) {
				float y = top + (bot - top) * (0.5f - 0.5f * L[(start + i) % HIST] * 0.9f);
				if (i == 0) nvgMoveTo(args.vg, xOf(i), y);
				else nvgLineTo(args.vg, xOf(i), y);
			}
			nvgStrokeColor(args.vg, nvgTransRGBA(soft, 38));
			nvgStrokeWidth(args.vg, 0.6f);
			nvgStroke(args.vg);
		}
		// the sum
		nvgBeginPath(args.vg);
		for (int i = 0; i < HIST; i++) {
			float y = yOf(sum[(start + i) % HIST]);
			if (i == 0) nvgMoveTo(args.vg, xOf(i), y);
			else nvgLineTo(args.vg, xOf(i), y);
		}
		nvgStrokeColor(args.vg, cream);
		nvgStrokeWidth(args.vg, 1.3f);
		nvgLineJoin(args.vg, NVG_ROUND);
		nvgStroke(args.vg);
		// trigger dots where they fired
		for (int i = 0; i < HIST; i++) {
			uint8_t b = trig[(start + i) % HIST];
			if (!b) continue;
			for (int k = 0; k < NUM_CH; k++) {
				if (!(b & (1 << k))) continue;
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, xOf(i), yOf(thr[k]), 1.6f);
				nvgFillColor(args.vg, chCol[k]);
				nvgFill(args.vg);
			}
		}

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.9f));
			float bpm = 120.f, drift = 0.2f, mut = 0.f, grid = 0.f;
			bool clk = false, frz = false, snapOn = true;
			float ratio[NUM_LFO] = {1.f, 0.75f, 1.5f, 1.f / 3.f};
			int snap[NUM_LFO] = {8, 7, 11, 3};
			if (module) {
				bpm = module->dispBpm; drift = module->dispDrift; mut = module->dispMutate; grid = module->dispGrid;
				clk = module->dispClock; frz = module->dispFreeze; snapOn = module->dispSnapOn;
				for (int i = 0; i < NUM_LFO; i++) { ratio[i] = module->dispRatio[i]; snap[i] = module->dispSnap[i]; }
			}
			std::string left = string::f("%s%d BPM  DRIFT %d%%  MUT %d%%%s  GRID %d%%", clk ? "CLK " : "",
			                             (int) std::round(bpm), (int) std::round(drift * 100.f), (int) std::round(mut * 100.f),
			                             frz ? " FROZEN" : "", (int) std::round(grid * 100.f));
			float by = h - mm2px(1.3f);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, cream);
			nvgText(args.vg, pad, by, left.c_str(), NULL);
			std::string right;
			for (int i = 0; i < NUM_LFO; i++) {
				if (i) right += "  ";
				if (snapOn && drift < 0.005f && mut < 0.005f) right += SNAP_NAMES[snap[i]];
				else right += string::f("%.3g", ratio[i]);
			}
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, nvgRGB(255, 193, 69));
			nvgText(args.vg, w - pad, by, right.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct LiltWidget : ModuleWidget {
	LiltWidget(Lilt* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lilt.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		LiltDisplay* d = createWidget<LiltDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		const int row1[4] = {Lilt::PULSE_PARAM, Lilt::DRIFT_PARAM, Lilt::MUTATE_PARAM, Lilt::GRID_PARAM};
		for (int i = 0; i < 4; i++)
			addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(GX[i], ROW1_Y)), module, row1[i]));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(GX[0] + 7.5f, ROW1_Y - 7.0f)), module, Lilt::BEAT_LIGHT));
		const int row2[3] = {Lilt::SHAPE_PARAM, Lilt::THRESH_PARAM, Lilt::SPREAD_PARAM};
		for (int i = 0; i < 3; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(GX[i], ROW2_Y)), module, row2[i]));
		addParam(createParamCentered<CKSS>(mm2px(Vec(SNAP_X, ROW2_Y)), module, Lilt::SNAP_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(DIR_X, ROW2_Y)), module, Lilt::DIR_PARAM));

		for (int i = 0; i < 6; i++) {
			float x = CV_X0 + CV_DX * i;
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, TRIM_Y)), module, Lilt::ATT_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, CVJ_Y)), module, Lilt::CV_INPUT + i));
		}
		const int ins[3] = {Lilt::CLOCK_INPUT, Lilt::RESET_INPUT, Lilt::FREEZE_INPUT};
		for (int i = 0; i < 3; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CV_X0 + CV_DX * i, IO_Y)), module, ins[i]));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(CV_X0 + CV_DX * 2 + 5.6f, IO_Y - 6.0f)), module, Lilt::FREEZE_LIGHT));
		const int outs[3] = {Lilt::BEAT_OUTPUT, Lilt::SUM_OUTPUT, Lilt::ACC_OUTPUT};
		for (int i = 0; i < 3; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(CV_X0 + CV_DX * (3 + i), IO_Y)), module, outs[i]));

		for (int i = 0; i < NUM_LFO; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(LX[i], RATIO_Y)), module, Lilt::RATIO_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(LX[i], LEVEL_Y)), module, Lilt::LEVEL_PARAM + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(LX[i], LFOJ_Y)), module, Lilt::LFO_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(LX[i], CVJ_Y)), module, Lilt::TRIG_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(LX[i], IO_Y)), module, Lilt::GATE_OUTPUT + i));
			addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(LX[i] + 6.4f, CVJ_Y - 3.4f)), module, Lilt::TRIG_LIGHT + i));
		}
	}

	void appendContextMenu(Menu* menu) override {
		Lilt* module = dynamic_cast<Lilt*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock input", {"1 per beat", "2 per beat", "4 per beat (16ths)", "24 per beat (MIDI)"},
			[=]() { return module->ppqnIndex; },
			[=](int i) { module->ppqnIndex = i; }));
		menu->addChild(createMenuItem("Re-roll drift directions", "", [=]() { module->reroll(); }));
	}
};

Model* modelLilt = createModel<Lilt, LiltWidget>("Lilt");
