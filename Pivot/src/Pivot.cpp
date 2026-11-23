// Pivot: metric modulation clock. Follows Live's clock, plays a chain of up to 8 tempo pivots
// ("old note = new note"), glides or snaps between them, and can land back on Live's bar lines.
#include "plugin.hpp"
#include "PivotEngine.hpp"

using namespace pivot;

// ---- LAYOUT (same numbers in res/make_panel.py)
static const float DISP_X = 6.0f, DISP_Y = 13.5f, DISP_W = 140.4f, DISP_H = 22.0f;
static const float GUT_X = 9.5f;
static const float COL0 = 24.0f, COLDX = 16.6f;
static const float BTN_Y = 45.5f, OLD_Y = 57.0f, NEW_Y = 70.0f;
static const float ROWA_Y = 85.5f, ROWB_Y = 104.0f, ROWC_Y = 118.5f;

static float colX(int i) { return COL0 + COLDX * i; }

struct Pivot : Module {
	enum ParamId {
		ENUMS(OLD_PARAM, NUM_STEPS),
		ENUMS(NEW_PARAM, NUM_STEPS),
		ENUMS(STEP_PARAM, NUM_STEPS),
		HOME_PARAM,
		NEXT_PARAM,
		BPM_PARAM,
		BAR_PARAM,
		INPPQN_PARAM,
		OUTPPQN_PARAM,
		LENGTH_PARAM,
		AUTO_PARAM,
		GLIDE_PARAM,
		QUANT_PARAM,
		END_PARAM,
		LAND_PARAM,
		PARAMS_LEN
	};
	enum InputId { CLOCK_INPUT, RESET_INPUT, NEXT_INPUT, HOME_INPUT, SELECT_INPUT, INPUTS_LEN };
	enum OutputId { CLOCK_OUTPUT, BEAT_OUTPUT, BAR_OUTPUT, RESET_OUTPUT, PIVOT_OUTPUT, GHOST_OUTPUT, RATIO_OUTPUT, AWAY_OUTPUT, OUTPUTS_LEN };
	enum LightId { HOME_LIGHT, ENUMS(STEP_LIGHT, NUM_STEPS), NEXT_LIGHT, LIGHTS_LEN };

	pivot::Engine eng;
	Settings settings;
	dsp::SchmittTrigger clockTrig, resetTrig, nextTrig, homeTrig;
	dsp::BooleanTrigger stepBtn[NUM_STEPS], homeBtn, nextBtn;
	dsp::PulseGenerator beatPulse, barPulse, resetPulse, pivotPulse;
	dsp::ClockDivider settingsDiv, lightDiv;
	int lastSelect = -1;
	float blink = 0.f;

	Pivot() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> notes;
		for (int i = 0; i < NUM_NOTES; i++)
			notes.push_back(string::f("%s (%s)", NOTE_NAMES[i], NOTE_LONG[i]));
		// default chain: x4/3, x3/4, x3/2, x2/3, x4/5, x5/4, x2/3, x3/2
		static const int defOld[NUM_STEPS] = {8, 9, 3, 9, 6, 5, 2, 7};
		static const int defNew[NUM_STEPS] = {9, 8, 6, 7, 5, 6, 0, 9};
		for (int i = 0; i < NUM_STEPS; i++) {
			configSwitch(OLD_PARAM + i, 0.f, NUM_NOTES - 1, defOld[i], string::f("Step %d OLD note (at the old tempo)", i + 1), notes);
			configSwitch(NEW_PARAM + i, 0.f, NUM_NOTES - 1, defNew[i], string::f("Step %d NEW note (becomes as long as OLD)", i + 1), notes);
			configButton(STEP_PARAM + i, string::f("Go to step %d", i + 1));
		}
		configButton(HOME_PARAM, "Home (back to Live's tempo)");
		configButton(NEXT_PARAM, "Next step");
		configParam(BPM_PARAM, 30.f, 300.f, 120.f, "Home tempo when CLOCK is unpatched", " BPM");
		getParamQuantity(BPM_PARAM)->snapEnabled = false;
		configParam(BAR_PARAM, 2.f, 12.f, 4.f, "Beats per bar");
		getParamQuantity(BAR_PARAM)->snapEnabled = true;
		std::vector<std::string> ppqn;
		for (int i = 0; i < NUM_PPQN; i++) ppqn.push_back(string::f("%d PPQN", PPQN_VALUES[i]));
		configSwitch(INPPQN_PARAM, 0.f, NUM_PPQN - 1, 5.f, "CLOCK in resolution", ppqn);
		configSwitch(OUTPPQN_PARAM, 0.f, NUM_PPQN - 1, 2.f, "CLOCK out resolution", ppqn);
		configParam(LENGTH_PARAM, 1.f, NUM_STEPS, 4.f, "Steps in the chain");
		getParamQuantity(LENGTH_PARAM)->snapEnabled = true;
		std::vector<std::string> autos = {"Off"};
		for (int i = 1; i < NUM_AUTO; i++) autos.push_back(string::f("Every %d bar%s", AUTO_BARS[i], AUTO_BARS[i] > 1 ? "s" : ""));
		configSwitch(AUTO_PARAM, 0.f, NUM_AUTO - 1, 0.f, "Auto advance", autos);
		std::vector<std::string> glides;
		for (int i = 0; i < NUM_GLIDES; i++) glides.push_back(GLIDE_NAMES[i]);
		configSwitch(GLIDE_PARAM, 0.f, NUM_GLIDES - 1, 0.f, "Glide into the new tempo", glides);
		configSwitch(QUANT_PARAM, 0.f, 2.f, 2.f, "Pivot on the next", {"Now", "Beat", "Bar"});
		configSwitch(END_PARAM, 0.f, 2.f, 0.f, "After the last step", {"Go home", "Loop to step 1", "Hold"});
		configSwitch(LAND_PARAM, 0.f, 1.f, 1.f, "Land home on Live's bar line", {"Off", "On"});

		configInput(CLOCK_INPUT, "Clock (from Live / CV Tools)");
		configInput(RESET_INPUT, "Reset");
		configInput(NEXT_INPUT, "Next step trigger");
		configInput(HOME_INPUT, "Home trigger");
		configInput(SELECT_INPUT, "Select step (1 V per step, 0 V = home)");
		configOutput(CLOCK_OUTPUT, "Pivoted clock");
		configOutput(BEAT_OUTPUT, "Beat");
		configOutput(BAR_OUTPUT, "Bar");
		configOutput(RESET_OUTPUT, "Reset");
		configOutput(PIVOT_OUTPUT, "Pivot (fires when the tempo changes)");
		configOutput(GHOST_OUTPUT, "Ghost clock (the tempo you just left)");
		configOutput(RATIO_OUTPUT, "Tempo ratio (1 V per doubling, 0 V = home)");
		configOutput(AWAY_OUTPUT, "Away (high while not home)");
		settingsDiv.setDivision(16);
		lightDiv.setDivision(256);
		readSettings();
	}

	void onReset() override {
		Events ev;
		ev.reset = true;
		eng.process(0.0, settings, ev);
	}

	void readSettings() {
		Settings& s = settings;
		s.cum[0] = 1.0;
		for (int i = 0; i < NUM_STEPS; i++) {
			int n, d;
			stepRatio((int) params[OLD_PARAM + i].getValue(), (int) params[NEW_PARAM + i].getValue(), n, d);
			double c = s.cum[i] * n / d;
			s.cum[i + 1] = c < 1.0 / 16 ? 1.0 / 16 : c > 16.0 ? 16.0 : c;
		}
		s.length = clamp((int) params[LENGTH_PARAM].getValue(), 1, NUM_STEPS);
		s.autoBars = AUTO_BARS[clamp((int) params[AUTO_PARAM].getValue(), 0, NUM_AUTO - 1)];
		s.quant = (int) params[QUANT_PARAM].getValue();
		s.atEnd = (int) params[END_PARAM].getValue();
		s.land = params[LAND_PARAM].getValue() > 0.5f;
		s.glideIdx = (int) params[GLIDE_PARAM].getValue();
		s.beatsPerBar = clamp((int) params[BAR_PARAM].getValue(), 2, 12);
		s.inPpqn = PPQN_VALUES[clamp((int) params[INPPQN_PARAM].getValue(), 0, NUM_PPQN - 1)];
		s.outPpqn = PPQN_VALUES[clamp((int) params[OUTPPQN_PARAM].getValue(), 0, NUM_PPQN - 1)];
		s.bpm = params[BPM_PARAM].getValue();
		s.clockConnected = inputs[CLOCK_INPUT].isConnected();
	}

	void process(const ProcessArgs& args) override {
		if (settingsDiv.process()) readSettings();

		Events ev;
		ev.clock = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		ev.reset = resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		ev.next = nextTrig.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 1.f);
		ev.next |= nextBtn.process(params[NEXT_PARAM].getValue() > 0.f);
		ev.home = homeTrig.process(inputs[HOME_INPUT].getVoltage(), 0.1f, 1.f);
		ev.home |= homeBtn.process(params[HOME_PARAM].getValue() > 0.f);
		for (int i = 0; i < NUM_STEPS; i++)
			if (stepBtn[i].process(params[STEP_PARAM + i].getValue() > 0.f)) ev.request = i + 1;
		if (inputs[SELECT_INPUT].isConnected()) {
			int k = clamp((int) std::round(inputs[SELECT_INPUT].getVoltage()), 0, NUM_STEPS);
			if (k != lastSelect) { lastSelect = k; ev.request = k; }
		}
		else lastSelect = -1;
		// a reset re-reads the clock state straight away
		if (ev.reset) settings.clockConnected = inputs[CLOCK_INPUT].isConnected();

		Out o = eng.process(args.sampleTime, settings, ev);

		if (o.beat) beatPulse.trigger(5e-3f);
		if (o.bar) barPulse.trigger(5e-3f);
		if (o.reset) resetPulse.trigger(5e-3f);
		if (o.pivot) pivotPulse.trigger(5e-3f);
		outputs[CLOCK_OUTPUT].setVoltage(o.clock ? 10.f : 0.f);
		outputs[BEAT_OUTPUT].setVoltage(beatPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[BAR_OUTPUT].setVoltage(barPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[RESET_OUTPUT].setVoltage(resetPulse.process(args.sampleTime) ? 10.f : 0.f);
		bool piv = pivotPulse.process(args.sampleTime);
		outputs[PIVOT_OUTPUT].setVoltage(piv ? 10.f : 0.f);
		outputs[GHOST_OUTPUT].setVoltage(o.ghost ? 10.f : 0.f);
		outputs[RATIO_OUTPUT].setVoltage((float) std::log2(o.ratio));
		outputs[AWAY_OUTPUT].setVoltage(o.away ? 10.f : 0.f);

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			blink += dt * 4.f;
			if (blink >= 1.f) blink -= 1.f;
			bool on = blink < 0.5f;
			int step = eng.step, pend = eng.pending;
			lights[HOME_LIGHT].setBrightness(step == 0 ? 1.f : (pend == 0 && on ? 0.6f : 0.f));
			for (int i = 0; i < NUM_STEPS; i++) {
				float b = 0.f;
				if (step == i + 1) b = 1.f;
				else if (pend == i + 1) b = on ? 0.6f : 0.f;
				else if (i < settings.length) b = 0.08f;
				lights[STEP_LIGHT + i].setBrightness(b);
			}
			lights[NEXT_LIGHT].setBrightnessSmooth(o.pivot || eng.mode != MODE_STEADY ? 1.f : 0.f, dt * 4.f);
		}
	}
};

// ---- screen: tempo, the two grids, and every step's OLD = NEW
struct PivotDisplay : Widget {
	Pivot* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		const NVGcolor AMBER = nvgRGB(0xFF, 0xB3, 0x41), MINT = nvgRGB(0x7D, 0xE2, 0xC3);
		const NVGcolor DIM = nvgRGB(0x5A, 0x60, 0x6E), SOFTC = nvgRGB(0xB8, 0xBC, 0xC8);
		nvgFontFaceId(vg, font->handle);

		// values (browser preview when there is no module)
		double home = 120.0, ratio = 4.0 / 3.0, H = 10.3;
		int step = 1, pend = -1, length = 4, mode = MODE_STEADY, B = 4;
		int oldI[NUM_STEPS] = {8, 9, 3, 9, 6, 5, 2, 7}, newI[NUM_STEPS] = {9, 8, 6, 7, 5, 6, 0, 9};
		double cum[NUM_STEPS + 1];
		const pivot::Engine* e = nullptr;
		if (module) {
			e = &module->eng;
			home = e->homeBpm; ratio = e->r; H = e->H; step = e->step; pend = e->pending; mode = e->mode;
			length = module->settings.length; B = module->settings.beatsPerBar;
			for (int i = 0; i < NUM_STEPS; i++) {
				oldI[i] = (int) module->params[Pivot::OLD_PARAM + i].getValue();
				newI[i] = (int) module->params[Pivot::NEW_PARAM + i].getValue();
			}
		}
		cum[0] = 1.0;
		for (int i = 0; i < NUM_STEPS; i++) {
			int n, d;
			stepRatio(oldI[i], newI[i], n, d);
			cum[i + 1] = cum[i] * n / d;
		}
		float w = box.size.x;

		// ---- line 1: tempo
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(6.2f));
		nvgFillColor(vg, AMBER);
		std::string bpm = string::f("%.1f", home * ratio);
		nvgText(vg, mm2px(1.6f), mm2px(6.6f), bpm.c_str(), nullptr);
		float bx = mm2px(1.6f) + mm2px(3.0f) * bpm.size() + mm2px(0.8f);
		nvgFontSize(vg, mm2px(2.6f));
		nvgFillColor(vg, SOFTC);
		nvgText(vg, bx, mm2px(6.6f), string::f("BPM  x%.3f of home %.1f", ratio, home).c_str(), nullptr);

		std::string state;
		if (mode == MODE_LAND) state = "LANDING";
		else if (mode == MODE_GLIDE) state = string::f("GLIDE TO %d", step);
		else if (step == 0) state = "HOME";
		else state = string::f("STEP %d OF %d", step, length);
		if (pend >= 0) state += pend == 0 ? "  > HOME" : string::f("  > %d", pend);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(3.0f));
		nvgFillColor(vg, step == 0 && mode == MODE_STEADY ? MINT : AMBER);
		nvgText(vg, w - mm2px(1.6f), mm2px(6.4f), state.c_str(), nullptr);

		// ---- lanes: Live's grid and Pivot's grid over the last two bars
		float lx0 = mm2px(12.f), lx1 = w - mm2px(1.6f);
		double span = 2.0 * B;
		auto xOf = [&](double h) { return lx0 + (float) ((h - (H - span)) / span) * (lx1 - lx0); };
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(2.2f));
		nvgFillColor(vg, MINT);
		nvgText(vg, mm2px(1.6f), mm2px(10.0f), "LIVE", nullptr);
		nvgFillColor(vg, AMBER);
		nvgText(vg, mm2px(1.6f), mm2px(13.4f), "PIVOT", nullptr);
		// Live: every beat at whole numbers of H
		for (long b = (long) std::ceil(H - span); b <= (long) std::floor(H); b++) {
			bool bar = (b % B) == 0;
			float x = xOf((double) b);
			nvgBeginPath(vg);
			nvgRect(vg, x - mm2px(0.25f), mm2px(bar ? 7.9f : 8.7f), mm2px(0.5f), mm2px(bar ? 2.4f : 1.6f));
			nvgFillColor(vg, bar ? MINT : nvgTransRGBA(MINT, 150));
			nvgFill(vg);
		}
		if (e) {
			for (int i = 0; i < pivot::Engine::HIST; i++) {
				double h = e->histH[i];
				int k = e->histKind[i];
				if (k == 0 || h < H - span || h > H) continue;
				float x = xOf(h);
				nvgBeginPath(vg);
				if (k == 3) {
					nvgRect(vg, x - mm2px(0.2f), mm2px(14.0f), mm2px(0.4f), mm2px(0.9f));
					nvgFillColor(vg, nvgTransRGBA(SOFTC, 120));
				}
				else {
					nvgRect(vg, x - mm2px(0.25f), mm2px(k == 2 ? 11.3f : 12.1f), mm2px(0.5f), mm2px(k == 2 ? 2.4f : 1.6f));
					nvgFillColor(vg, k == 2 ? AMBER : nvgTransRGBA(AMBER, 150));
				}
				nvgFill(vg);
			}
		}
		else {
			for (int b = 0; b < 11; b++) {
				float x = xOf(H - span + 0.3 + b * 0.75);
				nvgBeginPath(vg);
				nvgRect(vg, x - mm2px(0.25f), mm2px(b % 4 == 0 ? 11.3f : 12.1f), mm2px(0.5f), mm2px(b % 4 == 0 ? 2.4f : 1.6f));
				nvgFillColor(vg, b % 4 == 0 ? AMBER : nvgTransRGBA(AMBER, 150));
				nvgFill(vg);
			}
		}

		// ---- steps, each above its knobs
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
		for (int i = 0; i < NUM_STEPS; i++) {
			float x = mm2px(colX(i) - DISP_X);
			bool active = step == i + 1, used = i < length;
			int n, d;
			stepRatio(oldI[i], newI[i], n, d);
			nvgFontSize(vg, mm2px(2.2f));
			nvgFillColor(vg, active ? AMBER : used ? SOFTC : DIM);
			nvgText(vg, x, mm2px(17.6f), string::f("%s=%s", NOTE_NAMES[oldI[i]], NOTE_NAMES[newI[i]]).c_str(), nullptr);
			nvgFontSize(vg, mm2px(2.4f));
			nvgFillColor(vg, active ? AMBER : used ? MINT : DIM);
			std::string rs = n == d ? "x1" : string::f("x%d/%d", n, d);
			nvgText(vg, x, mm2px(20.7f), string::f("%s %.0f", rs.c_str(), home * cum[i + 1]).c_str(), nullptr);
		}
		nvgFontSize(vg, mm2px(2.2f));
		nvgFillColor(vg, step == 0 ? MINT : SOFTC);
		nvgText(vg, mm2px(GUT_X - DISP_X + 0.4f), mm2px(17.6f), "HOME", nullptr);
		nvgFontSize(vg, mm2px(2.4f));
		nvgText(vg, mm2px(GUT_X - DISP_X + 0.4f), mm2px(20.7f), string::f("%.0f", home).c_str(), nullptr);

		Widget::drawLayer(args, layer);
	}
};

struct PivotWidget : ModuleWidget {
	PivotWidget(Pivot* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pivot.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PivotDisplay* d = createWidget<PivotDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		// chain
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(GUT_X, BTN_Y)), module, Pivot::HOME_PARAM, Pivot::HOME_LIGHT));
		for (int i = 0; i < NUM_STEPS; i++) {
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(colX(i), BTN_Y)), module, Pivot::STEP_PARAM + i, Pivot::STEP_LIGHT + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(i), OLD_Y)), module, Pivot::OLD_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX(i), NEW_Y)), module, Pivot::NEW_PARAM + i));
		}

		// row A
		const int rowA[7] = {Pivot::BPM_PARAM, Pivot::BAR_PARAM, Pivot::INPPQN_PARAM, Pivot::OUTPPQN_PARAM,
		                     Pivot::LENGTH_PARAM, Pivot::AUTO_PARAM, Pivot::GLIDE_PARAM};
		for (int i = 0; i < 7; i++)
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colX(i), ROWA_Y)), module, rowA[i]));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(colX(7), ROWA_Y)), module, Pivot::NEXT_PARAM, Pivot::NEXT_LIGHT));

		// row B: switches + inputs
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(colX(0) - 3.f, ROWB_Y)), module, Pivot::QUANT_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(colX(1) - 3.f, ROWB_Y)), module, Pivot::END_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(colX(2) - 3.f, ROWB_Y)), module, Pivot::LAND_PARAM));
		const int ins[5] = {Pivot::CLOCK_INPUT, Pivot::RESET_INPUT, Pivot::NEXT_INPUT, Pivot::HOME_INPUT, Pivot::SELECT_INPUT};
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(3 + i), ROWB_Y)), module, ins[i]));

		// row C: outputs
		for (int i = 0; i < 8; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(i), ROWC_Y)), module, Pivot::CLOCK_OUTPUT + i));
	}
};

Model* modelPivot = createModel<Pivot, PivotWidget>("Pivot");
