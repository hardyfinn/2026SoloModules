// Peal: a change ringing sequencer. Each clock pulse rings one bell; the order
// of the bells (the row) changes every row by the rules of a ringing method.
// The ringing itself is in PealEngine.hpp.
#include "plugin.hpp"
#include "PealEngine.hpp"

using namespace peal;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 14.5f, DISP_W = 116.0f, DISP_H = 28.5f;
static const float BTN_X[2] = {137.0f, 165.0f};
static const float BTN_Y[2] = {23.0f, 38.5f};
static const float KNOB_X[6] = {16.0f, 46.0f, 76.0f, 106.0f, 136.0f, 166.0f};
static const float KNOB_Y = 58.0f;
static const float J_X0 = 10.0f, J_DX = 14.808f;
static const float ROWA_Y = 84.0f, ROWB_Y = 100.0f, ROWC_Y = 116.0f;

static const int NUM_SCALES = 8;
static const char* SCALE_NAMES[NUM_SCALES] = {"MAJOR", "MINOR", "DORIAN", "MIXOLYDIAN", "PENT MAJ", "PENT MIN", "WHOLE TONE", "CHROMATIC"};
static const int SCALE_LEN[NUM_SCALES] = {7, 7, 7, 7, 5, 5, 6, 12};
static const int SCALES[NUM_SCALES][12] = {
	{0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10},
	{0, 2, 4, 7, 9}, {0, 3, 5, 7, 10}, {0, 2, 4, 6, 8, 10}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};

struct Peal : Module {
	enum ParamIds {
		BELLS_PARAM, METHOD_PARAM, CHANCE_PARAM, GAP_PARAM, GATE_PARAM, SCALE_PARAM,
		GO_PARAM, RESET_PARAM, BOB_PARAM, SINGLE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT, RESET_INPUT, GO_INPUT, BOB_INPUT, SINGLE_INPUT, TUNE_INPUT, VOCT_INPUT,
		BELLS_INPUT, METHOD_INPUT, CHANCE_INPUT, GAP_INPUT, GATE_INPUT, SCALE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PITCH_OUTPUT, PLACE_OUTPUT, ROW_OUTPUT, GATES_OUTPUT, TUNES_OUTPUT,
		STRIKE_OUTPUT, HAND_OUTPUT, CHANGE_OUTPUT, LEAD_OUTPUT, ROUNDS_OUTPUT, CALL_OUTPUT,
		ENUMS(BELL_OUTPUT, MAXB),
		NUM_OUTPUTS
	};
	enum LightIds {
		GO_LIGHT, RESET_LIGHT, BOB_LIGHT, SINGLE_LIGHT,
		NUM_LIGHTS
	};

	Ringer eng;
	bool going = true;
	dsp::SchmittTrigger clockTrig, resetTrig, goTrig, bobTrig, singleTrig;
	dsp::BooleanTrigger goBtn, resetBtn, bobBtn, singleBtn;
	dsp::PulseGenerator strikePulse, changePulse, leadPulse, roundsPulse, callPulse, resetFlash;
	float gateLeft[MAXB] = {};
	float clockPeriod = 0.25f, sinceClock = 0.f;
	float pitchOut = 0.f, placeOut = 0.f;
	bool handOut = true;
	// for the screen
	int dispScale = 0, dispGap = 1;
	float eventAge = 99.f;
	int lastEvent = 0;  // 1 bob 2 single 3 go 4 that's all 5 rounds

	Peal() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(BELLS_PARAM, 4.f, 12.f, 8.f, "Bells (stage)");
		paramQuantities[BELLS_PARAM]->snapEnabled = true;
		configSwitch(METHOD_PARAM, 0.f, NUM_METHODS - 1, 1.f, "Method",
		             {"Plain Hunt", "Plain Bob", "Little Bob", "Grandsire", "Stedman", "Cambridge Surprise", "Wander (self-mutating)"});
		configParam(CHANCE_PARAM, 0.f, 1.f, 0.f, "Call chance (Wander: mutation)", "%", 0.f, 100.f);
		configParam(GAP_PARAM, 0.f, 4.f, 1.f, "Open handstroke gap", " pulses");
		paramQuantities[GAP_PARAM]->snapEnabled = true;
		configParam(GATE_PARAM, 0.02f, 1.f, 0.5f, "Gate length", "% of a pulse", 0.f, 100.f);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES - 1, 0.f, "Scale",
		             {"Major", "Minor", "Dorian", "Mixolydian", "Pentatonic major", "Pentatonic minor", "Whole tone", "Chromatic"});
		configButton(GO_PARAM, "Go / That's all");
		configButton(RESET_PARAM, "Reset to rounds");
		configButton(BOB_PARAM, "Bob");
		configButton(SINGLE_PARAM, "Single");
		configInput(CLOCK_INPUT, "Clock (one bell per pulse)");
		configInput(RESET_INPUT, "Reset");
		configInput(GO_INPUT, "Go / That's all (trigger flips)");
		configInput(BOB_INPUT, "Bob");
		configInput(SINGLE_INPUT, "Single");
		configInput(TUNE_INPUT, "Tune (poly V/OCT chord, tenor first)");
		configInput(VOCT_INPUT, "Transpose V/OCT");
		configInput(BELLS_INPUT, "Bells CV (1 V per bell)");
		configInput(METHOD_INPUT, "Method CV (1 V per method)");
		configInput(CHANCE_INPUT, "Call chance CV (10 V = 100 %)");
		configInput(GAP_INPUT, "Gap CV (1 V per pulse)");
		configInput(GATE_INPUT, "Gate length CV (10 V = full)");
		configInput(SCALE_INPUT, "Scale CV (1 V per scale)");
		configOutput(PITCH_OUTPUT, "Pitch of the bell just struck (V/OCT)");
		configOutput(PLACE_OUTPUT, "Place in the row (0 V lead, 10 V last)");
		configOutput(ROW_OUTPUT, "Row: pitches in ringing order (poly)");
		configOutput(GATES_OUTPUT, "Bell gates (poly, channel 1 = treble)");
		configOutput(TUNES_OUTPUT, "Bell pitches (poly, channel 1 = treble)");
		configOutput(STRIKE_OUTPUT, "Strike (any bell)");
		configOutput(HAND_OUTPUT, "Handstroke (high) / backstroke (low)");
		configOutput(CHANGE_OUTPUT, "Change (start of each row)");
		configOutput(LEAD_OUTPUT, "Lead end");
		configOutput(ROUNDS_OUTPUT, "Come round (back to rounds)");
		configOutput(CALL_OUTPUT, "Call (bob or single taken)");
		for (int i = 0; i < MAXB; i++)
			configOutput(BELL_OUTPUT + i, string::f("Bell %d%s", i + 1, i == 0 ? " (treble)" : ""));
	}

	int stepped(int param, int input, float perVolt, int lo, int hi) {
		float v = params[param].getValue();
		if (inputs[input].isConnected()) v += inputs[input].getVoltage() * perVolt;
		return clamp((int) std::round(v), lo, hi);
	}

	float bellPitch(int bell, int stage, int scale) {
		int d = stage - 1 - bell;  // degrees up from the tenor
		float v = 0.f;
		int ch = inputs[TUNE_INPUT].getChannels();
		if (ch > 0) v = inputs[TUNE_INPUT].getVoltage(d % ch) + (float) (d / ch);
		else {
			int n = SCALE_LEN[scale];
			v = (SCALES[scale][d % n] + 12 * (d / n)) / 12.f;
		}
		return v + inputs[VOCT_INPUT].getVoltage();
	}

	void onReset() override {
		eng.reset();
		going = true;
	}

	void process(const ProcessArgs& args) override {
		const float dt = args.sampleTime;
		RingParams p;
		p.stage = stepped(BELLS_PARAM, BELLS_INPUT, 1.f, 4, MAXB);
		p.method = stepped(METHOD_PARAM, METHOD_INPUT, 1.f, 0, NUM_METHODS - 1);
		p.gap = stepped(GAP_PARAM, GAP_INPUT, 1.f, 0, 8);
		float chance = params[CHANCE_PARAM].getValue();
		if (inputs[CHANCE_INPUT].isConnected()) chance += inputs[CHANCE_INPUT].getVoltage() * 0.1f;
		p.chance = clamp(chance, 0.f, 1.f);
		int scale = stepped(SCALE_PARAM, SCALE_INPUT, 1.f, 0, NUM_SCALES - 1);
		float gateFrac = params[GATE_PARAM].getValue();
		if (inputs[GATE_INPUT].isConnected()) gateFrac += inputs[GATE_INPUT].getVoltage() * 0.1f;
		gateFrac = clamp(gateFrac, 0.02f, 1.f);

		if (goBtn.process(params[GO_PARAM].getValue() > 0.f) | goTrig.process(inputs[GO_INPUT].getVoltage(), 0.1f, 1.f))
			going = !going;
		p.going = going;
		if (bobBtn.process(params[BOB_PARAM].getValue() > 0.f) | bobTrig.process(inputs[BOB_INPUT].getVoltage(), 0.1f, 1.f))
			eng.call(1);
		if (singleBtn.process(params[SINGLE_PARAM].getValue() > 0.f) | singleTrig.process(inputs[SINGLE_INPUT].getVoltage(), 0.1f, 1.f))
			eng.call(2);
		if (resetBtn.process(params[RESET_PARAM].getValue() > 0.f) | resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			eng.reset();
			resetFlash.trigger(0.15f);
			for (int i = 0; i < MAXB; i++) gateLeft[i] = 0.f;
		}

		sinceClock += dt;
		eventAge += dt;
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (sinceClock < 4.f) clockPeriod = sinceClock;
			sinceClock = 0.f;
			Strike s = eng.tick(p);
			if (s.gap) handOut = true;
			if (s.struck) {
				handOut = s.hand;
				gateLeft[s.bell] = std::max(0.001f, clockPeriod * gateFrac);
				strikePulse.trigger(1e-3f);
				pitchOut = bellPitch(s.bell, eng.stage, scale);
				placeOut = eng.stage > 1 ? 10.f * s.place / (eng.stage - 1) : 0.f;
			}
			if (s.rowStart) changePulse.trigger(1e-3f);
			if (s.leadEnd) leadPulse.trigger(1e-3f);
			if (s.rounds) { roundsPulse.trigger(1e-3f); lastEvent = 5; eventAge = 0.f; }
			if (s.call) { callPulse.trigger(1e-3f); lastEvent = s.call; eventAge = 0.f; }
			if (s.go) { lastEvent = 3; eventAge = 0.f; }
			if (s.thatsAll) { lastEvent = 4; eventAge = 0.f; }
		}

		// bell gates
		int n = eng.stage;
		for (int i = 0; i < MAXB; i++) {
			bool on = false;
			if (gateLeft[i] > 0.f) { gateLeft[i] -= dt; on = true; }
			float v = (on && i < n) ? 10.f : 0.f;
			outputs[BELL_OUTPUT + i].setVoltage(v);
			if (i < n) outputs[GATES_OUTPUT].setVoltage(v, i);
		}
		outputs[GATES_OUTPUT].setChannels(n);
		outputs[TUNES_OUTPUT].setChannels(n);
		outputs[ROW_OUTPUT].setChannels(n);
		for (int i = 0; i < n; i++) {
			outputs[TUNES_OUTPUT].setVoltage(bellPitch(i, n, scale), i);
			outputs[ROW_OUTPUT].setVoltage(bellPitch(eng.row[i] < n ? eng.row[i] : 0, n, scale), i);
		}
		outputs[PITCH_OUTPUT].setVoltage(pitchOut);
		outputs[PLACE_OUTPUT].setVoltage(placeOut);
		outputs[STRIKE_OUTPUT].setVoltage(strikePulse.process(dt) ? 10.f : 0.f);
		outputs[HAND_OUTPUT].setVoltage(handOut ? 10.f : 0.f);
		outputs[CHANGE_OUTPUT].setVoltage(changePulse.process(dt) ? 10.f : 0.f);
		outputs[LEAD_OUTPUT].setVoltage(leadPulse.process(dt) ? 10.f : 0.f);
		outputs[ROUNDS_OUTPUT].setVoltage(roundsPulse.process(dt) ? 10.f : 0.f);
		outputs[CALL_OUTPUT].setVoltage(callPulse.process(dt) ? 10.f : 0.f);

		lights[GO_LIGHT].setBrightness(going ? 1.f : 0.f);
		lights[RESET_LIGHT].setBrightnessSmooth(resetFlash.process(dt) ? 1.f : 0.f, dt);
		bool bobOn = eng.pendingCall == 1 || (eng.activeCall == 1);
		bool sglOn = eng.pendingCall == 2 || (eng.activeCall == 2);
		lights[BOB_LIGHT].setBrightness(bobOn ? 1.f : 0.f);
		lights[SINGLE_LIGHT].setBrightness(sglOn ? 1.f : 0.f);
		dispScale = scale;
		dispGap = p.gap;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "going", json_boolean(going));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* g = json_object_get(root, "going");
		if (g) going = json_boolean_value(g);
	}
};

// The screen: the last rows written out like a ringer's diagram, the treble's
// path in red and the 2's in blue, the bell being struck lit up, and status.
struct PealDisplay : Widget {
	Peal* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		NVGcontext* vg = args.vg;
		const float h = box.size.y;
		const NVGcolor bronze = nvgRGB(232, 178, 98), dim = nvgRGBA(232, 178, 98, 90), cream = nvgRGB(240, 230, 210);
		const NVGcolor red = nvgRGB(255, 90, 80), blue = nvgRGB(110, 170, 255);

		// rows to draw (oldest first)
		const int SHOW = 8;
		HistRow rows[SHOW];
		int count = 0, curPos = -1, stage = 8;
		std::string method = "PLAIN BOB", stageName = "MAJOR", status = "", info = "";
		bool cover = false;
		if (module) {
			Ringer& e = module->eng;
			stage = e.stage;
			int nrows = std::min(e.histCount, SHOW);
			for (int i = 0; i < nrows; i++) {
				int idx = (e.histHead - nrows + i + HIST * 2) % HIST;
				rows[i] = e.hist[idx];
			}
			count = nrows;
			curPos = e.pos == 0 ? e.stage : e.pos;  // blows rung in the newest row
			method = e.inMethod ? METHOD_NAMES[e.method] : (module->going ? "STAND BY" : "ROUNDS");
			if (e.inMethod && e.method != e.methodSel) method += " *";
			stageName = STAGE_NAMES[e.stage];
			cover = e.inMethod && e.cover;
			if (e.inMethod) info = string::f("LEAD %d  %d/%d", e.leads + 1, e.changeIdx + 1, (int) e.lead.ch.size());
			else info = module->going ? "GO AT HANDSTROKE" : "PRESS GO";
			if (module->eventAge < 1.2f) {
				const char* ev[] = {"", "BOB!", "SINGLE!", "GO!", "THAT'S ALL", "ROUNDS"};
				status = ev[module->lastEvent];
			}
			else if (e.pendingCall) status = e.pendingCall == 1 ? "BOB..." : "SINGLE...";
			info += string::f("  GAP %d  %s", module->dispGap, SCALE_NAMES[module->dispScale]);
		}
		else {
			const char* demo[SHOW] = {"12345678", "21436587", "24163857", "42618375", "46281735", "64827153", "68472513", "86745231"};
			for (int i = 0; i < SHOW; i++) {
				rows[i].n = 8;
				for (int k = 0; k < 8; k++) rows[i].b[k] = (uint8_t) (demo[i][k] - '1');
			}
			count = SHOW; curPos = 5; info = "LEAD 1  8/16  GAP 1  MAJOR";
		}

		nvgFontFaceId(vg, font->handle);
		// grid
		const float gx = mm2px(2.5f), gy = mm2px(3.0f);
		const float cw = mm2px(4.4f), rh = (h - gy - mm2px(1.4f)) / SHOW;
		auto cx = [&](int place) { return gx + cw * (place + 0.5f); };
		auto cy = [&](int r) { return gy + rh * (r + 0.5f); };
		// lines for treble and the 2
		for (int bell : {1, 0}) {
			nvgBeginPath(vg);
			bool started = false;
			for (int r = 0; r < count; r++) {
				int pl = -1;
				for (int k = 0; k < rows[r].n; k++) if (rows[r].b[k] == bell) pl = k;
				if (pl < 0) continue;
				if (!started) { nvgMoveTo(vg, cx(pl), cy(r)); started = true; }
				else nvgLineTo(vg, cx(pl), cy(r));
			}
			nvgStrokeColor(vg, bell == 0 ? nvgTransRGBA(red, 170) : nvgTransRGBA(blue, 150));
			nvgStrokeWidth(vg, 1.6f);
			nvgLineJoin(vg, NVG_ROUND);
			nvgStroke(vg);
		}
		nvgFontSize(vg, mm2px(3.4f));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		for (int r = 0; r < count; r++) {
			bool newest = (r == count - 1);
			if (rows[r].call) {
				nvgFillColor(vg, nvgTransRGBA(bronze, 200));
				nvgFontSize(vg, mm2px(2.4f));
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgText(vg, cx(rows[r].n) - cw * 0.2f, cy(r), rows[r].call == 1 ? "-" : "s", NULL);
				nvgFontSize(vg, mm2px(3.4f));
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			}
			for (int k = 0; k < rows[r].n; k++) {
				char c[2] = {BELL_CHARS[rows[r].b[k]], 0};
				NVGcolor col = newest ? (k < curPos ? cream : dim) : dim;
				if (newest && k == curPos - 1) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, cx(k) - cw * 0.45f, cy(r) - rh * 0.5f, cw * 0.9f, rh, 2.f);
					nvgFillColor(vg, nvgTransRGBA(bronze, 120));
					nvgFill(vg);
					col = cream;
				}
				if (rows[r].b[k] == 0) col = newest ? red : nvgTransRGBA(red, 200);
				nvgFillColor(vg, col);
				nvgText(vg, cx(k), cy(r), c, NULL);
			}
			if (rows[r].leadHead) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, gx, cy(r) - rh * 0.5f);
				nvgLineTo(vg, gx + cw * stage, cy(r) - rh * 0.5f);
				nvgStrokeColor(vg, nvgTransRGBA(bronze, 120));
				nvgStrokeWidth(vg, 0.7f);
				nvgStroke(vg);
			}
		}
		// text column
		const float tx = gx + cw * 12 + mm2px(5.5f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(4.0f));
		nvgFillColor(vg, bronze);
		nvgText(vg, tx, gy + mm2px(3.4f), method.c_str(), NULL);
		nvgFontSize(vg, mm2px(3.2f));
		nvgFillColor(vg, cream);
		std::string st = stageName + (cover ? " + COVER" : "");
		nvgText(vg, tx, gy + mm2px(8.4f), st.c_str(), NULL);
		nvgFontSize(vg, mm2px(2.6f));
		nvgFillColor(vg, dim);
		// info wraps onto two lines
		size_t cut = info.find("  GAP");
		nvgText(vg, tx, gy + mm2px(13.6f), info.substr(0, cut).c_str(), NULL);
		if (cut != std::string::npos) nvgText(vg, tx, gy + mm2px(17.4f), info.substr(cut + 2).c_str(), NULL);
		if (!status.empty()) {
			nvgFontSize(vg, mm2px(4.6f));
			nvgFillColor(vg, status[0] == 'B' || status[0] == 'S' ? red : bronze);
			nvgText(vg, tx, h - mm2px(2.0f), status.c_str(), NULL);
		}
	}
};

struct PealWidget : ModuleWidget {
	PealWidget(Peal* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Peal.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PealDisplay* d = createWidget<PealDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(BTN_X[0], BTN_Y[0])), module, Peal::GO_PARAM, Peal::GO_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(BTN_X[1], BTN_Y[0])), module, Peal::RESET_PARAM, Peal::RESET_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(BTN_X[0], BTN_Y[1])), module, Peal::BOB_PARAM, Peal::BOB_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(BTN_X[1], BTN_Y[1])), module, Peal::SINGLE_PARAM, Peal::SINGLE_LIGHT));

		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(KNOB_X[i], KNOB_Y)), module, Peal::BELLS_PARAM + i));

		auto jx = [](int slot) { return J_X0 + J_DX * slot; };
		for (int i = 0; i < 7; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jx(i), ROWA_Y)), module, Peal::CLOCK_INPUT + i));
		for (int i = 0; i < 5; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jx(7 + i), ROWA_Y)), module, Peal::PITCH_OUTPUT + i));
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jx(i), ROWB_Y)), module, Peal::BELLS_INPUT + i));
		for (int i = 0; i < 6; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jx(6 + i), ROWB_Y)), module, Peal::STRIKE_OUTPUT + i));
		for (int i = 0; i < MAXB; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jx(i), ROWC_Y)), module, Peal::BELL_OUTPUT + i));
	}
};

Model* modelPeal = createModel<Peal, PealWidget>("Peal");
