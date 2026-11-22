// Scanner: the sound of an MRI scanner from the inside.
//   SEQUENCE  ten real pulse sequences (prescan, localiser, T1, T2, FLAIR,
//             gradient echo, EPI, diffusion, angio, silent)
//   TR / SPACING / SLICES / RAMP  the sequence timing: how often it repeats,
//             the echo spacing (pitch; V/OCT tracks it), slices per TR, and
//             hard knocks vs. soft "silent" ramps
//   FIELD     1.5 T / 3 T / 7 T: louder, harder, brighter
//   HUMAN     heart-gated timing; DRIFT  phase encoding and diffusion
//             directions wander, so the pattern keeps changing
//   PLANT     the cold-head pump "chirp, thump", mains hum and air
//   SESSION   off, or a whole exam in order over 5-40 minutes
//   X Y Z     level of each coil; BORE / POSITION / EARS / BODY the tube you
//             lie in and what reaches you
//   GX GY GZ  the three gradient waveforms as CV (SLEW smooths them)
// The sound engine is in BoreDSP.hpp.
#include "plugin.hpp"
#include "BoreDSP.hpp"

using namespace bore;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 15.0f, DISP_W = 150.0f, DISP_H = 19.0f;
static const float COLX[5] = {19.0f, 49.0f, 79.0f, 109.0f, 139.0f};
static const float ROW1_Y = 49.0f;
static const float ROW2_Y = 70.0f;
static const float ROW3_Y = 90.0f;
static const float X3[8] = {12.0f, 30.5f, 49.0f, 67.5f, 86.0f, 104.5f, 125.0f, 145.0f};
static const float JX0 = 10.5f, JDX = 17.5f;
static const float IN_Y = 104.0f;
static const float OUT_Y = 118.0f;
static const float RX = 171.5f;
static const float RY[3] = {52.0f, 66.0f, 80.0f};

static const int SCOPE_N = 300;

struct Scanner : Module {
	enum ParamIds {
		SEQ_PARAM, TR_PARAM, SPACING_PARAM, SLICES_PARAM, RAMP_PARAM,
		FIELD_PARAM, HUMAN_PARAM, DRIFT_PARAM, PLANT_PARAM, SESSION_PARAM,
		X_PARAM, Y_PARAM, Z_PARAM, BORE_PARAM, POSITION_PARAM, BODY_PARAM, EARS_PARAM, SLEW_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT, RESET_INPUT, VOCT_INPUT, SEQ_INPUT, TR_INPUT, SPACING_INPUT, RAMP_INPUT, DRIFT_INPUT, AUDIO_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GX_OUTPUT, GY_OUTPUT, GZ_OUTPUT, TR_OUTPUT, SLICE_OUTPUT, PUMP_OUTPUT, SCAN_OUTPUT, EOS_OUTPUT,
		X_OUTPUT, Y_OUTPUT, Z_OUTPUT, L_OUTPUT, R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds { NUM_LIGHTS };

	ScannerEngine eng;
	ScanControls ctl;
	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator trPulse, slicePulse, pumpPulse, eosPulse;
	float slewed[3] = {0.f, 0.f, 0.f};
	int ctlDiv = 0;

	// display
	float scopeMin[3][SCOPE_N] = {}, scopeMax[3][SCOPE_N] = {};
	float binMin[3] = {0.f, 0.f, 0.f}, binMax[3] = {0.f, 0.f, 0.f};
	int scopeW = 0, binCount = 0;
	int dispMode = EPI, dispSlices = 8;
	float dispTr = 2.f, dispPitch = 523.f, dispField = 0.5f;
	bool dispSession = false;
	std::string dispStep;
	float dispStepT = 0.f, dispStepLen = 0.f;
	int dispStepIdx = 0;

	Scanner() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(SEQ_PARAM, 0.f, NUM_MODES - 1, EPI, "Sequence",
			{"Prescan / shim", "Localiser", "T1 spin echo", "T2 turbo spin echo", "FLAIR", "Gradient echo", "EPI (fMRI)", "Diffusion", "Angiography (TOF)", "Silent"});
		paramQuantities[SEQ_PARAM]->snapEnabled = true;
		configParam(TR_PARAM, 0.f, 1.f, 0.5f, "TR (repeat time)", "x", 16.f, 0.25f);
		configParam(SPACING_PARAM, 0.f, 1.f, 0.5f, "Echo spacing / pitch", " oct", 0.f, 4.f, -2.f);
		configParam(SLICES_PARAM, 1.f, 32.f, 10.f, "Slices per TR");
		paramQuantities[SLICES_PARAM]->snapEnabled = true;
		configParam(RAMP_PARAM, 0.f, 1.f, 0.f, "Ramp: hard knock to soft (silent)", "%", 0.f, 100.f);
		configParam(FIELD_PARAM, 0.f, 1.f, 0.5f, "Field strength (1.5 T, 3 T, 7 T)");
		configParam(HUMAN_PARAM, 0.f, 1.f, 0.f, "Human: heart-gated timing", "%", 0.f, 100.f);
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.15f, "Drift: pattern wanders", "%", 0.f, 100.f);
		configParam(PLANT_PARAM, 0.f, 1.f, 0.4f, "Plant: pump, hum, air", "%", 0.f, 100.f);
		configParam(SESSION_PARAM, 0.f, 1.f, 0.f, "Session (off, or whole exam length)");
		configParam(X_PARAM, 0.f, 1.f, 0.8f, "X coil level", "%", 0.f, 100.f);
		configParam(Y_PARAM, 0.f, 1.f, 0.8f, "Y coil level", "%", 0.f, 100.f);
		configParam(Z_PARAM, 0.f, 1.f, 0.8f, "Z coil level", "%", 0.f, 100.f);
		configParam(BORE_PARAM, 0.f, 1.f, 0.5f, "Bore: head coil, 60 cm, 70 cm wide");
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Position: head at the centre to near the opening", "%", 0.f, 100.f);
		configParam(BODY_PARAM, 0.f, 1.f, 0.3f, "Body: sound through the table and skull", "%", 0.f, 100.f);
		configSwitch(EARS_PARAM, 0.f, 2.f, 0.f, "Ears", {"Open", "Earplugs", "Headphones"});
		configParam(SLEW_PARAM, 0.f, 1.f, 0.f, "Gradient CV slew", "%", 0.f, 100.f);

		configInput(CLOCK_INPUT, "Clock (each pulse starts a TR)");
		configInput(RESET_INPUT, "Reset / start");
		configInput(VOCT_INPUT, "V/OCT (echo spacing pitch)");
		configInput(SEQ_INPUT, "Sequence CV (1 V per sequence)");
		configInput(TR_INPUT, "TR CV");
		configInput(SPACING_INPUT, "Spacing CV");
		configInput(RAMP_INPUT, "Ramp CV");
		configInput(DRIFT_INPUT, "Drift CV");
		configInput(AUDIO_INPUT, "Audio into the bore");

		configOutput(GX_OUTPUT, "X gradient CV");
		configOutput(GY_OUTPUT, "Y gradient CV");
		configOutput(GZ_OUTPUT, "Z gradient CV");
		configOutput(TR_OUTPUT, "TR trigger");
		configOutput(SLICE_OUTPUT, "Slice trigger");
		configOutput(PUMP_OUTPUT, "Pump trigger");
		configOutput(SCAN_OUTPUT, "Scanning gate");
		configOutput(EOS_OUTPUT, "End of series trigger");
		configOutput(X_OUTPUT, "X coil (dry)");
		configOutput(Y_OUTPUT, "Y coil (dry)");
		configOutput(Z_OUTPUT, "Z coil (dry)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		eng.init(48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		eng.init(e.sampleRate);
	}
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		eng.restart();
	}

	float cv(int in, float scale) { return inputs[in].isConnected() ? inputs[in].getVoltage() * scale : 0.f; }

	void readControls() {
		int mode = (int) std::round(params[SEQ_PARAM].getValue() + cv(SEQ_INPUT, 1.f));
		ctl.mode = clamp(mode, 0, NUM_MODES - 1);
		ctl.trMul = std::pow(2.f, (clamp(params[TR_PARAM].getValue() + cv(TR_INPUT, 0.1f), 0.f, 1.f) - 0.5f) * 4.f);
		float oct = (params[SPACING_PARAM].getValue() - 0.5f) * 4.f + cv(SPACING_INPUT, 0.4f) + cv(VOCT_INPUT, 1.f);
		ctl.timeMul = std::pow(2.f, -clamp(oct, -5.f, 5.f));
		ctl.slices = (int) params[SLICES_PARAM].getValue();
		ctl.ramp = clamp(params[RAMP_PARAM].getValue() + cv(RAMP_INPUT, 0.1f), 0.f, 1.f);
		ctl.field = params[FIELD_PARAM].getValue();
		ctl.human = params[HUMAN_PARAM].getValue();
		ctl.drift = clamp(params[DRIFT_PARAM].getValue() + cv(DRIFT_INPUT, 0.1f), 0.f, 1.f);
		ctl.plant = params[PLANT_PARAM].getValue();
		float s = params[SESSION_PARAM].getValue();
		ctl.session = s < 0.02f ? 0.f : 5.f + (s - 0.02f) / 0.98f * 35.f;
		ctl.lx = params[X_PARAM].getValue();
		ctl.ly = params[Y_PARAM].getValue();
		ctl.lz = params[Z_PARAM].getValue();
		ctl.clocked = inputs[CLOCK_INPUT].isConnected();
		ctl.bore.size = params[BORE_PARAM].getValue();
		ctl.bore.decay = 0.42f;
		ctl.bore.damp = 0.35f;
		ctl.bore.position = params[POSITION_PARAM].getValue();
		ctl.bore.body = params[BODY_PARAM].getValue();
		ctl.bore.ears = (int) std::round(params[EARS_PARAM].getValue());
	}

	void process(const ProcessArgs& args) override {
		if (ctlDiv <= 0) { readControls(); ctlDiv = 16; }
		ctlDiv--;
		bool clk = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		bool rst = resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		float audio = inputs[AUDIO_INPUT].getVoltage() * 0.2f;
		eng.process(ctl, audio, clk, rst);

		if (eng.trigTR) trPulse.trigger(0.005f);
		if (eng.trigSlice) slicePulse.trigger(0.002f);
		if (eng.trigPump) pumpPulse.trigger(0.005f);
		if (eng.trigEOS) eosPulse.trigger(0.01f);
		float dt = args.sampleTime;
		outputs[TR_OUTPUT].setVoltage(trPulse.process(dt) ? 10.f : 0.f);
		outputs[SLICE_OUTPUT].setVoltage(slicePulse.process(dt) ? 10.f : 0.f);
		outputs[PUMP_OUTPUT].setVoltage(pumpPulse.process(dt) ? 10.f : 0.f);
		outputs[EOS_OUTPUT].setVoltage(eosPulse.process(dt) ? 10.f : 0.f);
		outputs[SCAN_OUTPUT].setVoltage(eng.scanning ? 10.f : 0.f);

		float sl = params[SLEW_PARAM].getValue();
		float tau = 0.0002f + sl * sl * 0.3f;
		float a = 1.f - std::exp(-dt / tau);
		const int gOut[3] = {GX_OUTPUT, GY_OUTPUT, GZ_OUTPUT};
		const int cOut[3] = {X_OUTPUT, Y_OUTPUT, Z_OUTPUT};
		for (int i = 0; i < 3; i++) {
			slewed[i] += (eng.g[i] - slewed[i]) * a;
			outputs[gOut[i]].setVoltage(clamp(slewed[i] * 5.f, -6.f, 6.f));
			outputs[cOut[i]].setVoltage(eng.coilOut[i] * 5.f);
		}
		outputs[L_OUTPUT].setVoltage(eng.outL * 5.f);
		outputs[R_OUTPUT].setVoltage(eng.outR * 5.f);

		// scope: about a second across the screen
		int per = std::max(1, (int) (args.sampleRate / SCOPE_N));
		for (int i = 0; i < 3; i++) {
			if (binCount == 0) { binMin[i] = binMax[i] = eng.g[i]; }
			binMin[i] = std::min(binMin[i], eng.g[i]);
			binMax[i] = std::max(binMax[i], eng.g[i]);
		}
		if (++binCount >= per) {
			for (int i = 0; i < 3; i++) { scopeMin[i][scopeW] = binMin[i]; scopeMax[i][scopeW] = binMax[i]; }
			scopeW = (scopeW + 1) % SCOPE_N;
			binCount = 0;
		}
		dispMode = eng.curMode;
		dispSlices = eng.unitsThisTR;
		dispTr = eng.trLen;
		float base = eng.curMode == DIFFUSION ? 440.f : 523.25f;
		dispPitch = base / ctl.timeMul;
		dispField = ctl.field;
		dispSession = eng.sessionOn;
		dispStepT = (float) eng.stepT;
		dispStepLen = eng.stepLen;
		dispStepIdx = eng.sessStep;
	}
};

struct ScannerDisplay : Widget {
	Scanner* module = nullptr;

	static std::string mmss(float s) {
		int t = (int) s;
		return string::f("%d:%02d", t / 60, t % 60);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		float w = box.size.x, h = box.size.y;
		float pad = mm2px(1.4f);
		float top = mm2px(4.6f), bot = h - mm2px(4.4f);
		const NVGcolor cols[3] = {nvgRGB(0x6F, 0xB8, 0xFF), nvgRGB(0xFF, 0xB8, 0x4A), nvgRGB(0x7D, 0xE8, 0xA6)};
		// three lanes: X, Y, Z
		float lane = (bot - top) / 3.f;
		for (int a = 0; a < 3; a++) {
			float cy = top + lane * (a + 0.5f);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, pad, cy);
			nvgLineTo(args.vg, w - pad, cy);
			nvgStrokeColor(args.vg, nvgRGBA(0x30, 0x40, 0x58, 160));
			nvgStrokeWidth(args.vg, 0.6f);
			nvgStroke(args.vg);
			if (!module) continue;
			nvgBeginPath(args.vg);
			for (int i = 0; i < SCOPE_N; i++) {
				int k = (module->scopeW + i) % SCOPE_N;
				float x = pad + (w - 2 * pad) * i / (float) (SCOPE_N - 1);
				float y0 = cy - clamp(module->scopeMax[a][k], -1.f, 1.f) * lane * 0.45f;
				float y1 = cy - clamp(module->scopeMin[a][k], -1.f, 1.f) * lane * 0.45f;
				if (i == 0) nvgMoveTo(args.vg, x, y0);
				nvgLineTo(args.vg, x, y0);
				nvgLineTo(args.vg, x, std::max(y1, y0 + 0.5f));
			}
			nvgStrokeColor(args.vg, cols[a]);
			nvgStrokeWidth(args.vg, 1.0f);
			nvgStroke(args.vg);
		}
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.9f));
			int mode = module ? module->dispMode : EPI;
			std::string left = MODE_NAMES[ScannerEngine::clampi(mode)];
			float tr = module ? module->dispTr : 2.f;
			left += tr < 1.f ? string::f("  TR %d ms", (int) std::round(tr * 1000.f)) : string::f("  TR %.2f s", tr);
			left += string::f("  %d %s", module ? module->dispSlices : 10, (mode == GRE || mode == TOF || mode == SILENT || mode == LOCALISER) ? "BURSTS" : "SLICES");
			if (mode == EPI || mode == DIFFUSION) left += string::f("  %d Hz", (int) std::round(module ? module->dispPitch : 523.f));
			float f = module ? module->dispField : 0.5f;
			std::string right = f < 0.5f ? string::f("%.1f T", 1.5f + f * 3.f) : string::f("%.1f T", 3.f + (f - 0.5f) * 8.f);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, nvgRGB(0xE6, 0xEE, 0xF8));
			nvgText(args.vg, pad, top - mm2px(1.0f), left.c_str(), NULL);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, nvgRGB(0x6F, 0xB8, 0xFF));
			nvgText(args.vg, w - pad, top - mm2px(1.0f), right.c_str(), NULL);
			std::string low = "X  Y  Z gradients";
			if (module && module->dispSession) {
				low = string::f("SESSION %d/15  %s  %s / %s", module->dispStepIdx + 1, module->eng.stepName(),
					mmss(module->dispStepT).c_str(), mmss(module->dispStepLen).c_str());
			}
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(args.vg, nvgRGB(0x9A, 0xA8, 0xBA));
			nvgText(args.vg, pad, h - mm2px(1.1f), low.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct ScannerWidget : ModuleWidget {
	ScannerWidget(Scanner* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Scanner.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		ScannerDisplay* d = createWidget<ScannerDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		const int row1[5] = {Scanner::SEQ_PARAM, Scanner::TR_PARAM, Scanner::SPACING_PARAM, Scanner::SLICES_PARAM, Scanner::RAMP_PARAM};
		const int row2[5] = {Scanner::FIELD_PARAM, Scanner::HUMAN_PARAM, Scanner::DRIFT_PARAM, Scanner::PLANT_PARAM, Scanner::SESSION_PARAM};
		for (int i = 0; i < 5; i++) {
			addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COLX[i], ROW1_Y)), module, row1[i]));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[i], ROW2_Y)), module, row2[i]));
		}
		const int row3[6] = {Scanner::X_PARAM, Scanner::Y_PARAM, Scanner::Z_PARAM, Scanner::BORE_PARAM, Scanner::POSITION_PARAM, Scanner::BODY_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(X3[i], ROW3_Y)), module, row3[i]));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(X3[6], ROW3_Y - 1.0f)), module, Scanner::EARS_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(X3[7] + 2.0f, ROW3_Y)), module, Scanner::SLEW_PARAM));

		for (int i = 0; i < 9; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, IN_Y)), module, Scanner::CLOCK_INPUT + i));
		for (int i = 0; i < 8; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, OUT_Y)), module, Scanner::GX_OUTPUT + i));
		for (int i = 0; i < 3; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(RX, RY[i])), module, Scanner::X_OUTPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(RX, IN_Y)), module, Scanner::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(RX, OUT_Y)), module, Scanner::R_OUTPUT));
	}
};

Model* modelScanner = createModel<Scanner, ScannerWidget>("Scanner");
