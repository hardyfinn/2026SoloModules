// Greybox: one-pad 12-bit sampler voice in the manner of the late-80s rack samplers
// (900 / 950: pitch is the speed of the DAC clock, 36 dB tracking filter, cyclic
// time-stretch) and the drum machine-sampler (60: companded 12-bit words, fixed clock).
// Spec: vcv-modules/greybox.md (+ crate.md). All sound is in GreyboxDSP.hpp; this file
// is Rack glue, threading and persistence (structure shared with Crate).
#include "plugin.hpp"
#include "GreyboxDSP.hpp"
#include <osdialog.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// ---- LAYOUT (mm, same numbers in res/make_panel.py) ----
static const float COL[6] = {10.0f, 28.35f, 46.7f, 65.06f, 83.41f, 101.76f};
static const float ROW1_Y = 31.0f;
static const float ROW2_Y = 54.0f;
static const float ROW3_Y = 76.0f;
static const float ROW3_X[8] = {8.0f, 19.5f, 31.0f, 45.0f, 60.0f, 75.0f, 89.0f, 102.0f};
static const float JACK_A_Y = 97.0f;
static const float JACK_B_Y = 114.0f;

static const size_t MEM_WORDS[2] = {480000, 1440000};   // 750 KB, 2.25 MB
static const double MAX_SOURCE_SECS = 64.0;

struct Memory {
	std::vector<int16_t> codes;
	double fsRec = 48000;
	bool companded = false;
};

struct Greybox;

// RATE tooltip in Hz with the audio bandwidth, and typing a rate in Hz.
struct RateQuantity : ParamQuantity {
	int model() const;
	std::string getDisplayValueString() override {
		double fs = greybox::recRate(model(), getValue());
		return string::f("%.0f Hz (bandwidth %.1f kHz)", fs, fs / 2500.0);
	}
	void setDisplayValueString(std::string s) override {
		double hz = std::atof(s.c_str());
		if (hz < 100) hz *= 1000.0;                       // "32" means 32 kHz
		int m = model();
		double v = std::log(hz / greybox::FS_MIN) / std::log(greybox::FS_MAX[m] / greybox::FS_MIN);
		setValue((float)std::min(std::max(v, 0.0), 1.0));
	}
};

struct Greybox : Module {
	enum ParamId {
		MODEL_PARAM, GAIN_PARAM, RATE_PARAM, TUNE_PARAM, FILTER_PARAM, ENV_PARAM, DECAY_PARAM,
		START_PARAM, END_PARAM, LOOP_PARAM, STRETCH_PARAM, DTIME_PARAM, LEVEL_PARAM, AGE_PARAM,
		MUTATE_PARAM, MUT_BTN_PARAM, LOCK_PARAM, REC_BTN_PARAM, THRU_PARAM, PLAY_BTN_PARAM, PARAMS_LEN
	};
	enum InputId { TRIG_INPUT, VOCT_INPUT, VEL_INPUT, FILTER_INPUT, DECAY_INPUT, START_INPUT, IN_INPUT, REC_INPUT, MUT_INPUT, INPUTS_LEN };
	enum OutputId { OUT_OUTPUT, END_OUTPUT, OUTPUTS_LEN };
	enum LightId { REC_LIGHT, CLIP_LIGHT, PLAY_LIGHT, ENUMS(MEM_LIGHT, 4), LIGHTS_LEN };

	greybox::Voice voice;

	// memory hand-off between the worker and the audio thread
	Memory* active = nullptr;
	std::atomic<Memory*> pending{nullptr};
	std::atomic<Memory*> trash{nullptr};

	// source audio (worker and UI threads only)
	std::mutex sourceMutex;
	std::vector<float> source;
	double sourceRate = 48000;
	std::string sourcePath;
	bool sourceIsRecording = false;
	std::string missingName;

	// recording (audio thread writes, worker commits)
	std::vector<float> staging;
	double stagingRate = 48000;
	size_t recLen = 0;
	bool recording = false;
	bool recPrev = false;
	std::atomic<size_t> commitLen{0};

	// worker
	enum Job { JOB_CAPTURE = 1, JOB_COMMIT = 2 };
	std::atomic<int> jobs{0};
	std::atomic<bool> busy{false};
	std::atomic<bool> lastCaptureClipped{false};
	std::atomic<bool> running{true};
	std::mutex jobMutex;
	std::condition_variable jobCv;
	std::thread worker;

	// settings
	bool snapTune = true;
	bool follow = true;
	std::atomic<int> memSetting{0};
	uint32_t ageSeed = 1;

	// capture-parameter debounce
	float lastGain = -1.f, lastRate = -1.f, lastStretch = -99.f, lastDtime = -1.f;
	int lastModel = -1, lastMem = -1;
	int debounce = -1;

	// triggers
	dsp::SchmittTrigger trigIn, mutIn, playBtn;
	dsp::PulseGenerator endPulse;
	float clipHold = 0.f, captureFlash = 0.f;
	bool mutBtnPrev = false;
	float mutHeld = 0.f;
	bool mutCleared = false;
	float blinkPhase = 0.f;
	int lightDiv = 0;

	// mutation offsets (spec 9)
	int tuneMut = 0;
	float decayMut = 0.f, startMut = 0.f, filtMut = 0.f;
	bool revMut = false;
	int tuneMutActive = 0;
	float decayMutActive = 0.f, startMutActive = 0.f;
	bool revActive = false;

	Greybox() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(MODEL_PARAM, 0.f, 2.f, 1.f, "Model", {"900", "950", "60"});
		configParam(GAIN_PARAM, 0.f, 24.f, 0.f, "Input gain (drive into the capture clipper)", " dB");
		configParam<RateQuantity>(RATE_PARAM, 0.f, 1.f, 1.f, "Sampling rate");
		configParam(TUNE_PARAM, -12.f, 12.f, 0.f, "Tune", " semitones");
		configParam(FILTER_PARAM, 0.f, 99.f, 99.f, "Filter");
		configParam(ENV_PARAM, 0.f, 1.f, 0.f, "Filter envelope (closes as the sound decays)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 1.f, "Decay (fully right = off)", "%", 0.f, 100.f);
		configParam(START_PARAM, 0.f, 1.f, 0.f, "Start", "%", 0.f, 100.f);
		configParam(END_PARAM, 0.f, 1.f, 1.f, "End", "%", 0.f, 100.f);
		configSwitch(LOOP_PARAM, 0.f, 2.f, 0.f, "Loop", {"Off", "Forward", "Alternating"});
		configParam(STRETCH_PARAM, -2.f, 2.f, 0.f, "Time-stretch (centre = off)", "%", 2.f, 100.f);
		configParam(DTIME_PARAM, 0.f, 1.f, (float)(std::log(10.0) / std::log(30.0)), "Stretch cycle (D-time)", " samples", 30.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.f, "Age (worn DAC, clock and filter drift, hiss, droop, memory rot)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.3f, "Mutate depth", "%", 0.f, 100.f);
		configButton(MUT_BTN_PARAM, "Mutate (hold 1 s to clear)");
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock mutation", {"Off", "On"});
		configSwitch(REC_BTN_PARAM, 0.f, 1.f, 0.f, "Record", {"Off", "Recording"});
		configSwitch(THRU_PARAM, 0.f, 1.f, 0.f, "Thru (live input through the converters)", {"Off", "On"});
		configButton(PLAY_BTN_PARAM, "Play");
		paramQuantities[TUNE_PARAM]->snapEnabled = snapTune;

		configInput(TRIG_INPUT, "Trigger");
		configInput(VOCT_INPUT, "V/Oct pitch");
		configInput(VEL_INPUT, "Velocity (0-10 V)");
		configInput(FILTER_INPUT, "Filter CV (±5 V = ±50)");
		configInput(DECAY_INPUT, "Decay CV");
		configInput(START_INPUT, "Start CV");
		configInput(IN_INPUT, "Audio (record / thru)");
		configInput(REC_INPUT, "Record gate");
		configInput(MUT_INPUT, "Mutate trigger");
		configOutput(OUT_OUTPUT, "Audio");
		configOutput(END_OUTPUT, "End of sample trigger");
		configLight(CLIP_LIGHT, "Input clip");
		configLight(MEM_LIGHT, "Memory used");

		ageSeed = random::u32();
		voice.setSeed(ageSeed);
		staging.resize((size_t)(MAX_SOURCE_SECS * 48000));
		worker = std::thread([this] { workerLoop(); });
	}

	~Greybox() {
		running = false;
		jobCv.notify_all();
		if (worker.joinable()) worker.join();
		delete active;
		delete pending.exchange(nullptr);
		delete trash.exchange(nullptr);
	}

	int model() { return clamp((int)std::round(params[MODEL_PARAM].getValue()), 0, 2); }

	double stretchFactor() {
		double s = std::pow(2.0, (double)params[STRETCH_PARAM].getValue());
		return std::fabs(s - 1.0) <= 0.02 ? 1.0 : s;         // detent: exactly off
	}

	int dtimeSamples() {
		return (int)std::round(100.0 * std::pow(30.0, (double)params[DTIME_PARAM].getValue()));
	}

	// ---- worker thread ----------------------------------------------------

	void requestJob(int j) {
		jobs |= j;
		jobCv.notify_one();
	}

	void workerLoop() {
		while (running) {
			{
				std::unique_lock<std::mutex> lk(jobMutex);
				jobCv.wait_for(lk, std::chrono::milliseconds(30));
			}
			int j = jobs.exchange(0);
			delete trash.exchange(nullptr);
			if (!j) continue;
			busy = true;
			if (j & JOB_COMMIT) {
				size_t n = commitLen.load();
				std::lock_guard<std::mutex> lk(sourceMutex);
				source.assign(staging.begin(), staging.begin() + std::min(n, staging.size()));
				sourceRate = stagingRate;
				sourcePath.clear();
				sourceIsRecording = true;
				missingName.clear();
				commitLen = 0;
				j |= JOB_CAPTURE;
			}
			if (j & JOB_CAPTURE) runCapture();
			busy = false;
		}
	}

	void runCapture() {
		greybox::CaptureSettings cs;
		cs.model = model();
		cs.fsRec = greybox::recRate(cs.model, params[RATE_PARAM].getValue());
		cs.gain = std::pow(10.f, params[GAIN_PARAM].getValue() / 20.f);
		cs.stretch = stretchFactor();
		cs.dtime = dtimeSamples();
		cs.nMax = MEM_WORDS[clamp(memSetting.load(), 0, 1)];
		greybox::CaptureResult r;
		{
			std::lock_guard<std::mutex> lk(sourceMutex);
			r = greybox::capture(source, sourceRate, cs);
		}
		Memory* m = new Memory;
		m->codes = std::move(r.codes);
		m->fsRec = r.fsRec;
		m->companded = r.companded;
		lastCaptureClipped = r.clipped;
		delete pending.exchange(m);
	}

	// ---- UI-thread actions -------------------------------------------------

	bool loadFile(const std::string& path) {
		std::vector<float> buf;
		double fs = 0;
		if (!greybox::readWav(path.c_str(), buf, fs, MAX_SOURCE_SECS)) return false;
		{
			std::lock_guard<std::mutex> lk(sourceMutex);
			source = std::move(buf);
			sourceRate = fs;
			sourcePath = path;
			sourceIsRecording = false;
			missingName.clear();
		}
		requestJob(JOB_CAPTURE);
		return true;
	}

	void clearSample() {
		{
			std::lock_guard<std::mutex> lk(sourceMutex);
			source.clear();
			sourcePath.clear();
			sourceIsRecording = false;
			missingName.clear();
		}
		requestJob(JOB_CAPTURE);
	}

	std::string sampleName() {
		std::lock_guard<std::mutex> lk(sourceMutex);
		if (!missingName.empty()) return "Missing: " + missingName;
		if (sourceIsRecording) return "Recording";
		if (!sourcePath.empty()) return system::getFilename(sourcePath);
		return "";
	}

	// ---- mutation (spec 9) ------------------------------------------------------

	void mutate() {
		if (params[LOCK_PARAM].getValue() > 0.5f) return;
		float m = params[MUTATE_PARAM].getValue();
		int range = (int)std::round(7.f * m);
		tuneMut = range > 0 ? (int)(random::u32() % (2 * range + 1)) - range : 0;
		decayMut = (random::uniform() * 2.f - 1.f) * 0.3f * m;
		startMut = random::uniform() < 0.5f ? random::uniform() * 0.1f * m : 0.f;
		filtMut = -45.f * m + random::uniform() * 55.f * m;    // -45m .. +10m, mostly closes
		if (random::uniform() < 0.15f * m) revMut = !revMut;
	}

	void clearMutation() {
		tuneMut = 0;
		decayMut = startMut = filtMut = 0.f;
		revMut = false;
	}

	// ---- audio -----------------------------------------------------------------

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		voice.setSampleRate(e.sampleRate);
		if (!recording && commitLen.load() == 0) {
			staging.assign((size_t)(MAX_SOURCE_SECS * e.sampleRate), 0.f);
			stagingRate = e.sampleRate;
		}
	}

	void process(const ProcessArgs& args) override {
		// swap in fresh memory once the voice is quiet
		if (pending.load()) {
			if (voice.active()) voice.fadeOut();
			else {
				Memory* m = pending.exchange(nullptr);
				Memory* old = active;
				active = m;
				voice.setMemory(active->codes.data(), active->codes.size(), active->fsRec, active->companded);
				delete trash.exchange(old);
				if (lastCaptureClipped) captureFlash = 0.3f;
			}
		}

		// capture parameters changed -> re-run capture after 150 ms
		int md = model();
		float g = params[GAIN_PARAM].getValue();
		float rt = params[RATE_PARAM].getValue();
		float st = (float)stretchFactor();
		float dt = st != 1.f ? params[DTIME_PARAM].getValue() : lastDtime;
		int ms = memSetting.load();
		if (md != lastModel || g != lastGain || rt != lastRate || st != lastStretch || dt != lastDtime || ms != lastMem) {
			if (lastModel >= 0) debounce = (int)(0.15f * args.sampleRate);
			lastModel = md; lastGain = g; lastRate = rt; lastStretch = st; lastDtime = dt; lastMem = ms;
		}
		if (debounce >= 0 && --debounce < 0) requestJob(JOB_CAPTURE);

		// recording
		bool recNow = params[REC_BTN_PARAM].getValue() > 0.5f || inputs[REC_INPUT].getVoltage() >= 1.f;
		if (recNow && !recPrev && !recording && commitLen.load() == 0 && !busy) {
			recording = true;
			recLen = 0;
			stagingRate = args.sampleRate;
		}
		if (recording) {
			float x = inputs[IN_INPUT].getVoltage() / 5.f;
			if (recLen < staging.size()) staging[recLen++] = std::isfinite(x) ? x : 0.f;
			if (!recNow || recLen >= staging.size()) {
				recording = false;
				params[REC_BTN_PARAM].setValue(0.f);
				if (recLen >= (size_t)(0.005 * args.sampleRate)) {
					commitLen = recLen;
					requestJob(JOB_COMMIT);
				}
			}
		}
		recPrev = recNow;

		// mutation
		if (mutIn.process(inputs[MUT_INPUT].getVoltage(), 0.1f, 1.f)) mutate();
		bool mb = params[MUT_BTN_PARAM].getValue() > 0.5f;
		if (mb) {
			mutHeld += args.sampleTime;
			if (mutHeld >= 1.f && !mutCleared) { clearMutation(); mutCleared = true; }
		}
		else if (mutBtnPrev) {
			if (!mutCleared) mutate();
			mutHeld = 0.f;
			mutCleared = false;
		}
		mutBtnPrev = mb;

		// trigger
		bool trig = trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		if (playBtn.process(params[PLAY_BTN_PARAM].getValue())) trig = true;
		if (trig) {
			tuneMutActive = tuneMut;
			decayMutActive = decayMut;
			startMutActive = startMut;
			revActive = revMut;
		}

		greybox::Frame f;
		f.trig = trig;
		f.model = md;
		float tune = params[TUNE_PARAM].getValue();
		float tuneTotal = clamp(tune + (float)tuneMutActive, -12.f, 12.f);
		f.semis = tuneTotal + 12.f * inputs[VOCT_INPUT].getVoltage();
		f.fsRecLive = greybox::recRate(md, rt);
		f.vel = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 1.f;
		f.decay = clamp(params[DECAY_PARAM].getValue() + decayMutActive + inputs[DECAY_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		f.start = clamp(params[START_PARAM].getValue() + startMutActive + inputs[START_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		f.end = params[END_PARAM].getValue();
		f.loop = clamp((int)std::round(params[LOOP_PARAM].getValue()), 0, 2);
		f.reverse = revActive;
		f.filter = params[FILTER_PARAM].getValue() + filtMut + 10.f * inputs[FILTER_INPUT].getVoltage();
		f.envAmt = params[ENV_PARAM].getValue();
		f.follow = follow;
		f.level = params[LEVEL_PARAM].getValue();
		f.age = params[AGE_PARAM].getValue();
		f.thru = params[THRU_PARAM].getValue() > 0.5f || recording;
		f.trigConnected = inputs[TRIG_INPUT].isConnected();
		f.in = inputs[IN_INPUT].getVoltage();
		f.gain = std::pow(10.f, g / 20.f);

		greybox::Result r = voice.process(f);
		if (r.end) endPulse.trigger(1e-3f);
		if (r.inClip && (f.thru || recording)) clipHold = 0.05f;
		outputs[OUT_OUTPUT].setVoltage(r.out);
		outputs[END_OUTPUT].setVoltage(endPulse.process(args.sampleTime) ? 10.f : 0.f);

		// lights (every 32 samples)
		if (++lightDiv >= 32) {
			float ldt = args.sampleTime * lightDiv;
			lightDiv = 0;
			clipHold = std::max(0.f, clipHold - ldt);
			captureFlash = std::max(0.f, captureFlash - ldt);
			blinkPhase += ldt * 4.f;
			if (blinkPhase >= 1.f) blinkPhase -= 1.f;
			bool processing = busy || jobs.load() != 0 || pending.load() != nullptr;
			float recL = recording ? 1.f : (processing ? (blinkPhase < 0.5f ? 0.8f : 0.f) : 0.f);
			lights[REC_LIGHT].setBrightness(recL);
			lights[CLIP_LIGHT].setBrightness(clipHold > 0.f || captureFlash > 0.f ? 1.f : 0.f);
			lights[PLAY_LIGHT].setBrightnessSmooth(voice.playing ? 1.f : 0.f, ldt);
			double fill = active ? (double)active->codes.size() / MEM_WORDS[clamp(ms, 0, 1)] : 0.0;
			if (recording) fill = (double)recLen / staging.size();
			for (int i = 0; i < 4; i++) lights[MEM_LIGHT + i].setBrightness(clamp((float)(fill * 4.0 - i), 0.f, 1.f));
		}
	}

	// ---- persistence -------------------------------------------------------

	void onSave(const SaveEvent& e) override {
		std::lock_guard<std::mutex> lk(sourceMutex);
		if (sourceIsRecording && !source.empty()) {
			std::string dir = createPatchStorageDirectory();
			greybox::writeWavFloat(system::join(dir, "recording.wav").c_str(), source, sourceRate);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		{
			std::lock_guard<std::mutex> lk(sourceMutex);
			json_object_set_new(root, "path", json_string(sourcePath.c_str()));
			json_object_set_new(root, "recorded", json_boolean(sourceIsRecording));
		}
		json_object_set_new(root, "seed", json_integer(ageSeed));
		json_object_set_new(root, "snapTune", json_boolean(snapTune));
		json_object_set_new(root, "follow", json_boolean(follow));
		json_object_set_new(root, "memory", json_integer(memSetting.load()));
		json_object_set_new(root, "tuneMut", json_integer(tuneMut));
		json_object_set_new(root, "decayMut", json_real(decayMut));
		json_object_set_new(root, "startMut", json_real(startMut));
		json_object_set_new(root, "filtMut", json_real(filtMut));
		json_object_set_new(root, "revMut", json_boolean(revMut));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "seed")) { ageSeed = (uint32_t)json_integer_value(j); voice.setSeed(ageSeed); }
		if (json_t* j = json_object_get(root, "snapTune")) snapTune = json_boolean_value(j);
		paramQuantities[TUNE_PARAM]->snapEnabled = snapTune;
		if (json_t* j = json_object_get(root, "follow")) follow = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "memory")) memSetting = clamp((int)json_integer_value(j), 0, 1);
		if (json_t* j = json_object_get(root, "tuneMut")) tuneMut = (int)json_integer_value(j);
		if (json_t* j = json_object_get(root, "decayMut")) decayMut = (float)json_number_value(j);
		if (json_t* j = json_object_get(root, "startMut")) startMut = (float)json_number_value(j);
		if (json_t* j = json_object_get(root, "filtMut")) filtMut = (float)json_number_value(j);
		if (json_t* j = json_object_get(root, "revMut")) revMut = json_boolean_value(j);
		tuneMutActive = tuneMut; decayMutActive = decayMut; startMutActive = startMut; revActive = revMut;

		bool recorded = false;
		if (json_t* j = json_object_get(root, "recorded")) recorded = json_boolean_value(j);
		std::string path;
		if (json_t* j = json_object_get(root, "path")) path = json_string_value(j);
		if (recorded) {
			std::string p = system::join(getPatchStorageDirectory(), "recording.wav");
			if (loadFile(p)) {
				std::lock_guard<std::mutex> lk(sourceMutex);
				sourcePath.clear();
				sourceIsRecording = true;
			}
			else { std::lock_guard<std::mutex> lk(sourceMutex); missingName = "recording"; }
		}
		else if (!path.empty()) {
			if (!loadFile(path)) { std::lock_guard<std::mutex> lk(sourceMutex); missingName = system::getFilename(path); }
		}
	}
};

int RateQuantity::model() const {
	Greybox* m = dynamic_cast<Greybox*>(module);
	return m ? m->model() : greybox::M950;
}

// ---- widget ------------------------------------------------------------------

struct GreyboxWidget : ModuleWidget {
	GreyboxWidget(Greybox* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Greybox.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// row 1: voice
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COL[0], ROW1_Y)), module, Greybox::TUNE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COL[1], ROW1_Y)), module, Greybox::DECAY_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COL[2], ROW1_Y)), module, Greybox::FILTER_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[3], ROW1_Y)), module, Greybox::ENV_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[4], ROW1_Y)), module, Greybox::START_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[5], ROW1_Y)), module, Greybox::END_PARAM));

		// row 2: capture
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(COL[0] - 2.6f, ROW2_Y)), module, Greybox::MODEL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[1], ROW2_Y)), module, Greybox::GAIN_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(COL[1] + 7.0f, ROW2_Y + 5.6f)), module, Greybox::CLIP_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[2], ROW2_Y)), module, Greybox::RATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[3], ROW2_Y)), module, Greybox::STRETCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[4], ROW2_Y)), module, Greybox::DTIME_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(COL[5] + 2.6f, ROW2_Y)), module, Greybox::LOOP_PARAM));

		// row 3: record / play / output / wear / mutation
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(ROW3_X[0], ROW3_Y)), module, Greybox::REC_BTN_PARAM, Greybox::REC_LIGHT));
		for (int i = 0; i < 4; i++)
			addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(ROW3_X[0] - 3.9f + 2.6f * i, ROW3_Y + 6.0f)), module, Greybox::MEM_LIGHT + i));
		addParam(createParamCentered<CKSS>(mm2px(Vec(ROW3_X[1], ROW3_Y)), module, Greybox::THRU_PARAM));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenLight>>>(mm2px(Vec(ROW3_X[2], ROW3_Y)), module, Greybox::PLAY_BTN_PARAM, Greybox::PLAY_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW3_X[3], ROW3_Y)), module, Greybox::LEVEL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW3_X[4], ROW3_Y)), module, Greybox::AGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW3_X[5], ROW3_Y)), module, Greybox::MUTATE_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(ROW3_X[6], ROW3_Y)), module, Greybox::MUT_BTN_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(ROW3_X[7], ROW3_Y)), module, Greybox::LOCK_PARAM));

		// jacks
		const int rowA[6] = {Greybox::TRIG_INPUT, Greybox::VOCT_INPUT, Greybox::VEL_INPUT, Greybox::FILTER_INPUT, Greybox::DECAY_INPUT, Greybox::START_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], JACK_A_Y)), module, rowA[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[0], JACK_B_Y)), module, Greybox::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[1], JACK_B_Y)), module, Greybox::REC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[2], JACK_B_Y)), module, Greybox::MUT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[4], JACK_B_Y)), module, Greybox::END_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], JACK_B_Y)), module, Greybox::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Greybox* m = getModule<Greybox>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		std::string name = m->sampleName();
		menu->addChild(createMenuLabel(name.empty() ? "No sample" : "Sample: " + name));
		menu->addChild(createMenuItem("Load sample…", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("WAV:wav,WAV");
			char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
			osdialog_filters_free(filters);
			if (!path) return;
			std::string p = path;
			std::free(path);
			if (!m->loadFile(p))
				osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, "Could not read that file. Greybox loads PCM or float WAV files.");
		}));
		menu->addChild(createMenuItem("Clear sample", "", [=]() { m->clearSample(); }));
		menu->addChild(createBoolMenuItem("Snap TUNE to semitones", "",
			[=]() { return m->snapTune; },
			[=](bool v) { m->snapTune = v; m->paramQuantities[Greybox::TUNE_PARAM]->snapEnabled = v; }));
		menu->addChild(createBoolPtrMenuItem("Filter follows pitch", "", &m->follow));
		menu->addChild(createIndexSubmenuItem("Memory", {"750 KB (12 s at 40 kHz)", "2.25 MB (36 s at 40 kHz)"},
			[=]() { return (size_t)m->memSetting.load(); },
			[=](size_t i) { m->memSetting = (int)i; }));
	}
};

Model* modelGreybox = createModel<Greybox, GreyboxWidget>("Greybox");
