// Essphor: live rhythmic grains. Part A of docs/BUILD-INSTRUCTIONS.md. The sound is in src/dsp/Grains.hpp.
#include "plugin.hpp"
#include "dsp/Grains.hpp"
#include "dsp/Wav.hpp"
#include "layout.hpp"
#include <osdialog.h>
#include <mutex>
#include <atomic>

using namespace essphor;

static const float VOLT = 0.2f;   // internal 1.0 = 5 V
static const int OVERVIEW = 160;

struct Essphor : Module {
	enum ParamIds {
		PITCH_PARAM, RATE_PARAM, SIZE_PARAM, CONTOUR_PARAM, SCAN_PARAM, SPRAY_PARAM, PATTERN_PARAM, WET_PARAM,
		FEEDBACK_PARAM, RAND_PITCH_PARAM, RAND_RATE_PARAM, RAND_SIZE_PARAM, RAND_AMP_PARAM, RAND_REV_PARAM, AGE_PARAM,
		SCALE_PARAM, TEMPO_PARAM, LOCK_PARAM, RATE_MODE_PARAM, SCAN_MODE_PARAM, SPRAY_MODE_PARAM, WET_MODE_PARAM, FB_MODE_PARAM, RP_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_CV_INPUT, RATE_CV_INPUT, SIZE_CV_INPUT, CONTOUR_CV_INPUT, SCAN_CV_INPUT, SPRAY_CV_INPUT, PATTERN_CV_INPUT, WET_CV_INPUT,
		FEEDBACK_CV_INPUT, RAND_PITCH_CV_INPUT, RAND_RATE_CV_INPUT, RAND_SIZE_CV_INPUT, RAND_AMP_CV_INPUT, RAND_REV_CV_INPUT, AGE_CV_INPUT,
		VOCT_INPUT, IN_L_INPUT, IN_R_INPUT, CLOCK_INPUT, RESET_INPUT, LOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { GRAIN_OUTPUT, OUT_L_OUTPUT, OUT_R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { LOCK_LIGHT, CLOCK_LIGHT, GRAIN_LIGHT, NUM_LIGHTS };

	GrainEngine eng;
	Clock clk;
	dsp::SchmittTrigger clockTrig, resetTrig, lockTrig;
	dsp::PulseGenerator grainPulse;
	dsp::ClockDivider lightDiv;
	float clockLight = 0.f, grainLight = 0.f;

	// settings saved in the patch
	double bufSec = 4.0;
	int ppqn = 1;
	bool keepRunning = true;
	bool saveBuffer = false;
	uint64_t patternSeed = 1, rngSeed = 1;

	// requests from the UI thread, applied in process()
	std::atomic<bool> reqNewPattern{false};
	std::atomic<double> reqBufSec{0.0};
	std::mutex loadMutex;
	bool loadPending = false;
	std::vector<float> loadL, loadR;
	std::string loadedName;

	// buffer overview for the display
	float overview[OVERVIEW] = {};
	int ovBin = -1;
	bool ovRebuild = false;

	Essphor() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, -36.f, 36.f, 0.f, "Pitch", " semitones")->description = "Pitch shift of each grain. The sum with CV and V/OCT is snapped to SCALE.";
		auto* rate = configParam<FnQuantity>(RATE_PARAM, 0.f, 1.f, 0.75f, "Grain rate");
		rate->fn = [this](float v) {
			if (params[RATE_MODE_PARAM].getValue() < 0.5f) return std::string(RATE_DIV_NAMES[rateDivIndex(v)]);
			return fmtHz(rateFreeHz(v));
		};
		auto* size = configParam<FnQuantity>(SIZE_PARAM, 0.f, 1.f, 4.f / 9.f, "Grain size");
		size->fn = [this](float v) {
			double b = sizeBeats(v), bps = std::max(1e-3, clk.bps);
			return string::f("%.3g beats (%s)", b, fmtSeconds(b / bps).c_str());
		};
		configParam(CONTOUR_PARAM, -1.f, 1.f, 0.f, "Contour")->description = "Grain window: -1 sharp attack and long tail, 0 smooth, +1 slow swell and sharp end.";
		auto* scan = configParam<FnQuantity>(SCAN_PARAM, 0.f, 1.f, 0.625f, "Scan");
		scan->fn = [this](float v) {
			switch ((int)params[SCAN_MODE_PARAM].getValue()) {
				case SCAN_POSITION: return string::f("%.1f%% of the buffer", v * 100.f);
				case SCAN_DELAY_SYNC: return string::f("%s behind", DELAY_DIV_NAMES[delayDivIndex(v)]);
				case SCAN_DELAY_FREE: return fmtSeconds(delayFreeSec(v, bufSec)) + " behind";
				default: return string::f("%.2fx speed", stretchSpeed(v));
			}
		};
		configParam(SPRAY_PARAM, 0.f, 1.f, 0.f, "Spray", "%", 0.f, 100.f)->description = "Random start offset and stereo spread of each grain.";
		configParam(PATTERN_PARAM, 0.f, 1.f, 0.f, "Pattern", "%", 0.f, 100.f)->description = "Depth of a fixed 16-step pitch pattern (up to two octaves). New pattern in the menu.";
		configParam(WET_PARAM, 0.f, 1.f, 0.5f, "Wet", "%", 0.f, 100.f);
		configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Feedback", "%", 0.f, 100.f);
		configParam(RAND_PITCH_PARAM, 0.f, 1.f, 0.f, "Random pitch", "%", 0.f, 100.f);
		configParam(RAND_RATE_PARAM, 0.f, 1.f, 0.f, "Random timing", "%", 0.f, 100.f);
		configParam(RAND_SIZE_PARAM, 0.f, 1.f, 0.f, "Random size", "%", 0.f, 100.f);
		configParam(RAND_AMP_PARAM, 0.f, 1.f, 0.f, "Random level", "%", 0.f, 100.f);
		configParam(RAND_REV_PARAM, 0.f, 1.f, 0.f, "Reverse probability", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.f, "Age", "%", 0.f, 100.f)->description =
			"An older, cheaper sampler: duller and grittier recording, hiss, fewer bits, drifting pitch and loose timing.";
		std::vector<std::string> scales(SCALE_NAMES, SCALE_NAMES + NUM_SCALES);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES - 1, 0.f, "Scale", scales);
		configParam(TEMPO_PARAM, 30.f, 300.f, 120.f, "Tempo (when CLOCK is unpatched)", " BPM")->snapEnabled = false;
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock", {"Off (recording)", "On (buffer frozen)"});
		configSwitch(RATE_MODE_PARAM, 0.f, 1.f, 0.f, "Rate mode", {"SYNC", "FREE"});
		configSwitch(SCAN_MODE_PARAM, 0.f, 3.f, 0.f, "Scan mode", {"STRETCH", "POSITION", "DELAY SYNC", "DELAY FREE"});
		configSwitch(SPRAY_MODE_PARAM, 0.f, 3.f, 0.f, "Spray mode", {"RANDOM", "RANDOM MONO", "WARP", "WARP MONO"});
		configSwitch(WET_MODE_PARAM, 0.f, 1.f, 0.f, "Wet mode", {"MIX", "SEND"});
		configSwitch(FB_MODE_PARAM, 0.f, 1.f, 0.f, "Feedback mode", {"GRAIN", "SOS"});
		configSwitch(RP_MODE_PARAM, 0.f, 2.f, 0.f, "Random pitch mode", {"DETUNE", "RANDOM", "RANDOM MONO"});
		for (int m = RATE_MODE_PARAM; m <= RP_MODE_PARAM; m++) getParamQuantity(m)->randomizeEnabled = false;
		getParamQuantity(LOCK_PARAM)->randomizeEnabled = false;
		getParamQuantity(TEMPO_PARAM)->randomizeEnabled = false;

		const char* cvNames[15] = {"Pitch", "Rate", "Size", "Contour", "Scan", "Spray", "Pattern", "Wet", "Feedback",
			"Random pitch", "Random timing", "Random size", "Random level", "Reverse", "Age"};
		for (int i = 0; i < 15; i++) configInput(PITCH_CV_INPUT + i, string::f("%s CV (±5 V sweeps the knob)", cvNames[i]));
		configInput(VOCT_INPUT, "V/OCT (added to PITCH before the scale)");
		configInput(IN_L_INPUT, "Left");
		configInput(IN_R_INPUT, "Right (follows left when unpatched)");
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(LOCK_INPUT, "Lock gate (high = locked, OR-ed with the button)");
		configOutput(GRAIN_OUTPUT, "Grain trigger (1 ms, 10 V at each grain)");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
		configLight(LOCK_LIGHT, "Locked");
		configLight(CLOCK_LIGHT, "Beat");
		configLight(GRAIN_LIGHT, "Grain");

		lightDiv.setDivision(32);
		patternSeed = random::u64() | 1;
		rngSeed = random::u64() | 1;
		eng.setPatternSeed(patternSeed);
		eng.seed(rngSeed);
		clk.reset();
	}

	void initEngine(double fs) {
		eng.init(fs, bufSec);
		std::fill(overview, overview + OVERVIEW, 0.f);
		ovBin = -1;
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { initEngine(e.sampleRate); }

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		if (eng.fs > 0) eng.clearBuffer();
		std::fill(overview, overview + OVERVIEW, 0.f);
	}

	double knobCv(int param, int input, double lo, double hi) {
		double v = params[param].getValue();
		if (inputs[input].isConnected()) v += inputs[input].getVoltage() * 0.1 * (hi - lo);
		return std::max(lo, std::min(hi, v));
	}

	// UI thread: read a file, convert it to the engine rate, hand it over.
	bool requestLoad(const std::string& path) {
		WavData wd;
		if (!readWav(path, wd)) return false;
		double fs = APP->engine->getSampleRate();
		std::vector<float> l = resampleLinear(wd.l, wd.sampleRate, fs);
		std::vector<float> r = wd.r.empty() ? l : resampleLinear(wd.r, wd.sampleRate, fs);
		std::lock_guard<std::mutex> lk(loadMutex);
		loadL.swap(l); loadR.swap(r); loadPending = true;
		loadedName = system::getFilename(path);
		return true;
	}

	void applyPendingLoad() {
		std::unique_lock<std::mutex> lk(loadMutex, std::try_to_lock);
		if (!lk.owns_lock() || !loadPending) return;
		eng.loadBuffer(loadL, loadR);
		eng.p.lock = true;
		params[LOCK_PARAM].setValue(1.f);
		loadPending = false;
		loadL.clear(); loadR.clear();
		ovRebuild = true;
	}

	void rebuildOverview() {
		std::fill(overview, overview + OVERVIEW, 0.f);
		for (int i = 0; i < eng.Bn; i++) {
			int b = (int)((int64_t)i * OVERVIEW / eng.Bn);
			overview[b] = std::max(overview[b], std::max(std::fabs(eng.bufL[i]), std::fabs(eng.bufR[i])));
		}
		ovRebuild = false;
	}

	void process(const ProcessArgs& args) override {
		double rb = reqBufSec.exchange(0.0);
		if (rb > 0.0) { bufSec = rb; initEngine(args.sampleRate); }
		if (eng.fs != args.sampleRate) initEngine(args.sampleRate);
		if (reqNewPattern.exchange(false)) eng.setPatternSeed(patternSeed);
		applyPendingLoad();
		if (ovRebuild) rebuildOverview();

		// clock
		clk.ppqn = ppqn;
		const bool clockOn = inputs[CLOCK_INPUT].isConnected();
		const bool edge = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		const bool rst = resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		clk.tick(args.sampleTime, clockOn, edge, rst, params[TEMPO_PARAM].getValue());

		// parameters
		GrainParams& p = eng.p;
		p.pitch = knobCv(PITCH_PARAM, PITCH_CV_INPUT, -36, 36) + inputs[VOCT_INPUT].getVoltage() * 12.0;
		p.scale = (int)params[SCALE_PARAM].getValue();
		p.sync = params[RATE_MODE_PARAM].getValue() < 0.5f;
		p.rateKnob = knobCv(RATE_PARAM, RATE_CV_INPUT, 0, 1);
		p.sizeKnob = knobCv(SIZE_PARAM, SIZE_CV_INPUT, 0, 1);
		p.contour = knobCv(CONTOUR_PARAM, CONTOUR_CV_INPUT, -1, 1);
		p.scanMode = (int)params[SCAN_MODE_PARAM].getValue();
		p.scan = knobCv(SCAN_PARAM, SCAN_CV_INPUT, 0, 1);
		p.sprayMode = (int)params[SPRAY_MODE_PARAM].getValue();
		p.spray = knobCv(SPRAY_PARAM, SPRAY_CV_INPUT, 0, 1);
		p.pattern = knobCv(PATTERN_PARAM, PATTERN_CV_INPUT, 0, 1);
		p.wet = knobCv(WET_PARAM, WET_CV_INPUT, 0, 1);
		p.send = params[WET_MODE_PARAM].getValue() > 0.5f;
		p.fbMode = (int)params[FB_MODE_PARAM].getValue();
		p.feedback = knobCv(FEEDBACK_PARAM, FEEDBACK_CV_INPUT, 0, 1);
		p.rpMode = (int)params[RP_MODE_PARAM].getValue();
		p.randPitch = knobCv(RAND_PITCH_PARAM, RAND_PITCH_CV_INPUT, 0, 1);
		p.randRate = knobCv(RAND_RATE_PARAM, RAND_RATE_CV_INPUT, 0, 1);
		p.randSize = knobCv(RAND_SIZE_PARAM, RAND_SIZE_CV_INPUT, 0, 1);
		p.randAmp = knobCv(RAND_AMP_PARAM, RAND_AMP_CV_INPUT, 0, 1);
		p.randRev = knobCv(RAND_REV_PARAM, RAND_REV_CV_INPUT, 0, 1);
		p.age = knobCv(AGE_PARAM, AGE_CV_INPUT, 0, 1);
		lockTrig.process(inputs[LOCK_INPUT].getVoltage(), 0.1f, 1.f);
		p.lock = params[LOCK_PARAM].getValue() > 0.5f || (inputs[LOCK_INPUT].isConnected() && lockTrig.isHigh());
		p.run = keepRunning || !clk.stopped;

		// audio
		const bool lOn = inputs[IN_L_INPUT].isConnected(), rOn = inputs[IN_R_INPUT].isConnected();
		const float l = lOn ? inputs[IN_L_INPUT].getVoltage() : (rOn ? inputs[IN_R_INPUT].getVoltage() : 0.f);
		const float r = rOn ? inputs[IN_R_INPUT].getVoltage() : l;
		double oL, oR;
		const int w0 = eng.w;
		eng.process(l * VOLT, r * VOLT, clk, oL, oR);
		outputs[OUT_L_OUTPUT].setVoltage((float)oL / VOLT);
		outputs[OUT_R_OUTPUT].setVoltage((float)oR / VOLT);

		// buffer overview: track the bin under the write head
		if (eng.w != w0 && eng.Bn > 0) {
			int b = (int)((int64_t)w0 * OVERVIEW / eng.Bn);
			if (b != ovBin) { overview[b] = 0.f; ovBin = b; }
			overview[b] = std::max(overview[b], std::max(std::fabs(eng.bufL[w0]), std::fabs(eng.bufR[w0])));
		}

		if (eng.fired) {
			grainPulse.trigger(1e-3f);
			grainLight = std::max(grainLight, (float)std::min(1.0, 0.3 + eng.firedLevel));
		}
		if (eng.beat) clockLight = 1.f;
		outputs[GRAIN_OUTPUT].setVoltage(grainPulse.process(args.sampleTime) ? 10.f : 0.f);

		if (lightDiv.process()) {
			const float dt = args.sampleTime * lightDiv.getDivision();
			lights[LOCK_LIGHT].setBrightness((float)eng.lockA);
			lights[CLOCK_LIGHT].setBrightness(clockLight);
			lights[GRAIN_LIGHT].setBrightness(grainLight);
			clockLight = std::max(0.f, clockLight - dt * 8.f);
			grainLight = std::max(0.f, grainLight - dt * 12.f);
		}
	}

	// ---------------------------------------------------------------- patch storage
	std::string bufferPath() { return system::join(getPatchStorageDirectory(), "buffer.wav"); }

	void onSave(const SaveEvent& e) override {
		Module::onSave(e);
		const bool want = saveBuffer && params[LOCK_PARAM].getValue() > 0.5f && eng.Bn > 0;
		if (want) {
			std::string dir = createPatchStorageDirectory();
			std::vector<float> l(eng.bufL.begin(), eng.bufL.end()), r(eng.bufR.begin(), eng.bufR.end());
			// write from the write head on, so the file starts where the loop starts
			std::rotate(l.begin(), l.begin() + eng.w % eng.Bn, l.end());
			std::rotate(r.begin(), r.begin() + eng.w % eng.Bn, r.end());
			writeWav(system::join(dir, "buffer.wav"), l, &r, eng.fs, true);
		} else {
			std::string path = bufferPath();
			if (system::isFile(path)) system::remove(path);
		}
	}

	void onAdd(const AddEvent& e) override {
		Module::onAdd(e);
		std::string path = bufferPath();
		if (saveBuffer && system::isFile(path)) requestLoad(path);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "bufferSeconds", json_real(bufSec));
		json_object_set_new(root, "ppqn", json_integer(ppqn));
		json_object_set_new(root, "keepRunning", json_boolean(keepRunning));
		json_object_set_new(root, "saveBuffer", json_boolean(saveBuffer));
		json_object_set_new(root, "patternSeed", json_string(string::f("%016llx", (unsigned long long)patternSeed).c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "bufferSeconds")) {
			double s = json_number_value(j);
			bufSec = s >= 12 ? 16.0 : s >= 6 ? 8.0 : 4.0;
			reqBufSec = bufSec;
		}
		if (json_t* j = json_object_get(root, "ppqn")) { int v = (int)json_integer_value(j); ppqn = (v == 2 || v == 4 || v == 24) ? v : 1; }
		if (json_t* j = json_object_get(root, "keepRunning")) keepRunning = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "saveBuffer")) saveBuffer = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "patternSeed")) {
			if (json_is_string(j)) { patternSeed = std::strtoull(json_string_value(j), nullptr, 16) | 1; reqNewPattern = true; }
		}
	}
};

// ---------------------------------------------------------------- buffer display
struct BufferDisplay : Widget {
	Essphor* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		const float W = box.size.x, H = box.size.y, mid = H / 2, pad = 2.f;
		NVGcolor amber = nvgRGB(242, 192, 96);
		// waveform overview
		nvgBeginPath(args.vg);
		for (int i = 0; i < OVERVIEW; i++) {
			float v = module ? std::min(1.f, module->overview[i]) : 0.25f * (0.5f + 0.5f * std::sin(i * 0.37f)) * (0.5f + 0.5f * std::sin(i * 0.05f));
			float x = pad + (W - 2 * pad) * (i + 0.5f) / OVERVIEW;
			float h = std::max(0.4f, v * (mid - pad));
			nvgRect(args.vg, x - 0.4f, mid - h, 0.8f, 2 * h);
		}
		nvgFillColor(args.vg, nvgTransRGBA(amber, 140));
		nvgFill(args.vg);
		if (!module || module->eng.Bn <= 0) return;
		const GrainEngine& e = module->eng;
		auto xAt = [&](double pos) { return pad + (W - 2 * pad) * (float)(pos / e.Bn); };
		// grains
		for (int i = 0; i < MAX_GRAINS; i++) {
			const Grain& g = e.g[i];
			if (!g.on) continue;
			double pos = std::fmod(g.pos + g.inc * g.age, (double)e.Bn);
			if (pos < 0) pos += e.Bn;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, xAt(pos), mid, 1.1f);
			nvgFillColor(args.vg, nvgRGBA(95, 179, 161, 220));
			nvgFill(args.vg);
		}
		// write head
		nvgBeginPath(args.vg);
		float wx = xAt(e.w);
		nvgMoveTo(args.vg, wx, 1.f); nvgLineTo(args.vg, wx, H - 1.f);
		nvgStrokeColor(args.vg, e.lockA >= 1.0 ? nvgRGB(120, 120, 120) : nvgRGB(236, 90, 70));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);
		// caption
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 8.f);
			nvgFillColor(args.vg, amber);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
			std::string s = string::f("%.0f s%s", module->bufSec, e.lockA >= 1.0 ? "  LOCKED" : "");
			nvgText(args.vg, W - 3.f, 1.5f, s.c_str(), NULL);
		}
	}
};

// ---------------------------------------------------------------- widget
struct EssphorWidget : ModuleWidget {
	EssphorWidget(Essphor* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Essphor.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		namespace L = layout::Essphor;
		using E = Essphor;
		auto big = [&](float x, float y, int id) { addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(x, y)), module, id)); };
		auto knob = [&](float x, float y, int id) { addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, y)), module, id)); };
		auto cyc = [&](float x, float y, int id) { addParam(createParamCentered<CycleButton>(mm2px(Vec(x, y)), module, id)); };
		auto in = [&](float x, float y, int id) { addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
		auto out = [&](float x, float y, int id) { addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
		auto modeText = [module](int id, std::vector<std::string> names, std::string dflt) {
			return [=]() {
				if (!module) return dflt;
				int i = (int)module->params[id].getValue();
				return names[clamp(i, 0, (int)names.size() - 1)];
			};
		};

		big(L::PITCH_X, L::PITCH_Y, E::PITCH_PARAM); big(L::RATE_X, L::RATE_Y, E::RATE_PARAM);
		big(L::SIZE_X, L::SIZE_Y, E::SIZE_PARAM); big(L::CONTOUR_X, L::CONTOUR_Y, E::CONTOUR_PARAM);
		big(L::SCAN_X, L::SCAN_Y, E::SCAN_PARAM); big(L::SPRAY_X, L::SPRAY_Y, E::SPRAY_PARAM);
		big(L::PATTERN_X, L::PATTERN_Y, E::PATTERN_PARAM); big(L::WET_X, L::WET_Y, E::WET_PARAM);
		knob(L::FEEDBACK_X, L::FEEDBACK_Y, E::FEEDBACK_PARAM); knob(L::RAND_PITCH_X, L::RAND_PITCH_Y, E::RAND_PITCH_PARAM);
		knob(L::RAND_RATE_X, L::RAND_RATE_Y, E::RAND_RATE_PARAM); knob(L::RAND_SIZE_X, L::RAND_SIZE_Y, E::RAND_SIZE_PARAM);
		knob(L::RAND_AMP_X, L::RAND_AMP_Y, E::RAND_AMP_PARAM); knob(L::RAND_REV_X, L::RAND_REV_Y, E::RAND_REV_PARAM);
		knob(L::AGE_X, L::AGE_Y, E::AGE_PARAM);
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(L::SCALE_X, L::SCALE_Y)), module, E::SCALE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(L::TEMPO_X, L::TEMPO_Y)), module, E::TEMPO_PARAM));
		addParam(createLightParamCentered<LatchButton>(mm2px(Vec(L::LOCK_X, L::LOCK_Y)), module, E::LOCK_PARAM, E::LOCK_LIGHT));

		cyc(L::RATE_MODE_X, L::RATE_MODE_Y, E::RATE_MODE_PARAM);
		cyc(L::SCAN_MODE_X, L::SCAN_MODE_Y, E::SCAN_MODE_PARAM);
		cyc(L::SPRAY_MODE_X, L::SPRAY_MODE_Y, E::SPRAY_MODE_PARAM);
		cyc(L::WET_MODE_X, L::WET_MODE_Y, E::WET_MODE_PARAM);
		cyc(L::FB_MODE_X, L::FB_MODE_Y, E::FB_MODE_PARAM);
		cyc(L::RP_MODE_X, L::RP_MODE_Y, E::RP_MODE_PARAM);
		addChild(makeDisplay(L::RATE_MODE_DISP_X, L::RATE_MODE_DISP_Y, L::RATE_MODE_DISP_W, L::RATE_MODE_DISP_H,
			[module]() {
				if (!module) return std::string("1/16");
				float v = module->params[E::RATE_PARAM].getValue();
				if (module->params[E::RATE_MODE_PARAM].getValue() < 0.5f) return std::string(RATE_DIV_NAMES[rateDivIndex(v)]);
				double hz = rateFreeHz(v);
				return hz < 10 ? string::f("%.2fHz", hz) : string::f("%.0fHz", hz);
			}));
		addChild(makeDisplay(L::SCAN_MODE_DISP_X, L::SCAN_MODE_DISP_Y, L::SCAN_MODE_DISP_W, L::SCAN_MODE_DISP_H,
			modeText(E::SCAN_MODE_PARAM, {"STRETCH", "POSITION", "DLY SYNC", "DLY FREE"}, "STRETCH")));
		addChild(makeDisplay(L::SPRAY_MODE_DISP_X, L::SPRAY_MODE_DISP_Y, L::SPRAY_MODE_DISP_W, L::SPRAY_MODE_DISP_H,
			modeText(E::SPRAY_MODE_PARAM, {"RANDOM", "RND MONO", "WARP", "WARP MONO"}, "RANDOM")));
		addChild(makeDisplay(L::WET_MODE_DISP_X, L::WET_MODE_DISP_Y, L::WET_MODE_DISP_W, L::WET_MODE_DISP_H,
			modeText(E::WET_MODE_PARAM, {"MIX", "SEND"}, "MIX")));
		addChild(makeDisplay(L::FB_MODE_DISP_X, L::FB_MODE_DISP_Y, L::FB_MODE_DISP_W, L::FB_MODE_DISP_H,
			modeText(E::FB_MODE_PARAM, {"GRAIN", "SOS"}, "GRAIN")));
		addChild(makeDisplay(L::RP_MODE_DISP_X, L::RP_MODE_DISP_Y, L::RP_MODE_DISP_W, L::RP_MODE_DISP_H,
			modeText(E::RP_MODE_PARAM, {"DETUNE", "RANDOM", "RND MONO"}, "DETUNE")));
		addChild(makeDisplay(L::SCALE_DISP_X, L::SCALE_DISP_Y, L::SCALE_DISP_W, L::SCALE_DISP_H,
			[module]() { return std::string(SCALE_NAMES[module ? clamp((int)module->params[E::SCALE_PARAM].getValue(), 0, NUM_SCALES - 1) : 0]); }));
		addChild(makeDisplay(L::PATTERN_DISP_X, L::PATTERN_DISP_Y, L::PATTERN_DISP_W, L::PATTERN_DISP_H,
			[module]() {
				if (!module) return std::string("PAT 0000");
				return string::f("PAT %04X", (unsigned)((module->patternSeed >> 48) & 0xFFFF));
			}));
		BufferDisplay* bd = new BufferDisplay;
		bd->module = module;
		bd->box.pos = mm2px(Vec(L::BUF_DISP_X - L::BUF_DISP_W / 2, L::BUF_DISP_Y - L::BUF_DISP_H / 2));
		bd->box.size = mm2px(Vec(L::BUF_DISP_W, L::BUF_DISP_H));
		addChild(bd);
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(L::CLOCK_LIGHT_X, L::CLOCK_LIGHT_Y)), module, E::CLOCK_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(L::GRAIN_LIGHT_X, L::GRAIN_LIGHT_Y)), module, E::GRAIN_LIGHT));

		const float cvx[15] = {L::PITCH_CV_X, L::RATE_CV_X, L::SIZE_CV_X, L::CONTOUR_CV_X, L::SCAN_CV_X, L::SPRAY_CV_X, L::PATTERN_CV_X, L::WET_CV_X,
			L::FEEDBACK_CV_X, L::RAND_PITCH_CV_X, L::RAND_RATE_CV_X, L::RAND_SIZE_CV_X, L::RAND_AMP_CV_X, L::RAND_REV_CV_X, L::AGE_CV_X};
		const float cvy[15] = {L::PITCH_CV_Y, L::RATE_CV_Y, L::SIZE_CV_Y, L::CONTOUR_CV_Y, L::SCAN_CV_Y, L::SPRAY_CV_Y, L::PATTERN_CV_Y, L::WET_CV_Y,
			L::FEEDBACK_CV_Y, L::RAND_PITCH_CV_Y, L::RAND_RATE_CV_Y, L::RAND_SIZE_CV_Y, L::RAND_AMP_CV_Y, L::RAND_REV_CV_Y, L::AGE_CV_Y};
		for (int i = 0; i < 15; i++) in(cvx[i], cvy[i], E::PITCH_CV_INPUT + i);
		in(L::VOCT_X, L::VOCT_Y, E::VOCT_INPUT);
		in(L::IN_L_X, L::IN_L_Y, E::IN_L_INPUT); in(L::IN_R_X, L::IN_R_Y, E::IN_R_INPUT);
		in(L::CLOCK_X, L::CLOCK_Y, E::CLOCK_INPUT); in(L::RESET_X, L::RESET_Y, E::RESET_INPUT);
		in(L::LOCK_IN_X, L::LOCK_IN_Y, E::LOCK_INPUT);
		out(L::GRAIN_OUT_X, L::GRAIN_OUT_Y, E::GRAIN_OUTPUT);
		out(L::OUT_L_X, L::OUT_L_Y, E::OUT_L_OUTPUT); out(L::OUT_R_X, L::OUT_R_Y, E::OUT_R_OUTPUT);
	}

	void appendContextMenu(Menu* menu) override {
		Essphor* m = dynamic_cast<Essphor*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Buffer length", {"4 seconds", "8 seconds", "16 seconds"},
			[=]() { return m->bufSec >= 12 ? (size_t)2 : m->bufSec >= 6 ? (size_t)1 : (size_t)0; },
			[=](size_t i) { m->reqBufSec = i == 2 ? 16.0 : i == 1 ? 8.0 : 4.0; m->bufSec = i == 2 ? 16.0 : i == 1 ? 8.0 : 4.0; }));
		menu->addChild(ppqnMenu(&m->ppqn));
		menu->addChild(createBoolPtrMenuItem("Grains keep running without a clock", "", &m->keepRunning));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("New pattern", "", [=]() { m->patternSeed = random::u64() | 1; m->reqNewPattern = true; }));
		menu->addChild(createMenuItem("Load WAV into buffer…", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("WAV:wav,WAV");
			char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
			osdialog_filters_free(filters);
			if (!path) return;
			std::string p(path);
			std::free(path);
			if (!m->requestLoad(p)) osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, "Essphor could not read that WAV file.");
		}));
		menu->addChild(createBoolPtrMenuItem("Save buffer with patch (while locked)", "", &m->saveBuffer));
	}
};

Model* modelEssphor = createModel<Essphor, EssphorWidget>("Essphor");
