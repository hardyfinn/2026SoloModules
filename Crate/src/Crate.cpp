// Crate: one-pad 12-bit sampler voice in the manner of a late-80s hip-hop drum sampler.
// 26.04 kHz / 12-bit memory, drop-sample pitch with no interpolation, channel filters,
// live sampling with the 33 -> 45 trick, AGE, MUTATE + LOCK. Spec: vcv-modules/crate.md.
// All sound is in CrateDSP.hpp; this file is Rack glue, threading and persistence.
#include "plugin.hpp"
#include "CrateDSP.hpp"
#include <osdialog.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// ---- LAYOUT (mm, same numbers in res/make_panel.py) ----
static const float COL[5] = {10.0f, 25.3f, 40.64f, 56.0f, 71.3f};
static const float ROW1_Y = 28.0f;
static const float ROW2_Y = 50.0f;
static const float ROW3_Y = 72.0f;
static const float ROW3_X[6] = {8.5f, 21.4f, 34.3f, 47.2f, 60.1f, 73.0f};
static const float JACK_A_Y = 96.0f;
static const float JACK_B_Y = 113.0f;

static const double SPEED_K[3] = {1.0, 1.35, 2.34};
static const double MEM_SECS[3] = {2.5, 5.0, 10.0};
static const double MAX_SOURCE_SECS = 24.0;

struct Memory {
	std::vector<int16_t> codes;
};

struct Crate : Module {
	enum ParamId {
		GAIN_PARAM, SPEED_PARAM, TUNE_PARAM, DECAY_PARAM, START_PARAM, END_PARAM,
		LOOP_PARAM, CHANNEL_PARAM, LEVEL_PARAM, AGE_PARAM, MUTATE_PARAM, MUT_BTN_PARAM,
		LOCK_PARAM, REC_BTN_PARAM, THRU_PARAM, PLAY_BTN_PARAM, PARAMS_LEN
	};
	enum InputId { TRIG_INPUT, VOCT_INPUT, VEL_INPUT, DECAY_INPUT, START_INPUT, IN_INPUT, REC_INPUT, MUT_INPUT, INPUTS_LEN };
	enum OutputId { OUT_OUTPUT, END_OUTPUT, OUTPUTS_LEN };
	enum LightId { REC_LIGHT, CLIP_LIGHT, PLAY_LIGHT, ENUMS(MEM_LIGHT, 4), LIGHTS_LEN };

	crate::Voice voice;

	// memory hand-off between the worker and the audio thread
	Memory* active = nullptr;
	std::atomic<Memory*> pending{nullptr};
	std::atomic<Memory*> trash{nullptr};

	// source audio (worker and UI threads only)
	std::mutex sourceMutex;
	std::vector<float> source;
	double sourceRate = 48000;
	std::string sourcePath;        // empty for a recording
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
	std::atomic<int> memSetting{2};
	uint32_t ageSeed = 1;

	// capture-parameter debounce
	float lastGain = -1.f;
	int lastSpeed = -1, lastMem = -1;
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
	int tuneMut = 0, chanMut = 0;
	float decayMut = 0.f, startMut = 0.f;
	int tuneMutActive = 0;
	float decayMutActive = 0.f, startMutActive = 0.f;

	Crate() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(GAIN_PARAM, 0.f, 24.f, 0.f, "Input gain (drive into the capture clipper)", " dB");
		configSwitch(SPEED_PARAM, 0.f, 2.f, 0.f, "Capture speed (33 = as is, 45 / 78 = sampled fast, played back down)", {"33", "45", "78"});
		configParam(TUNE_PARAM, -12.f, 12.f, 0.f, "Tune", " semitones");
		configParam(DECAY_PARAM, 0.f, 1.f, 1.f, "Decay (fully right = off)", "%", 0.f, 100.f);
		configParam(START_PARAM, 0.f, 1.f, 0.f, "Start", "%", 0.f, 100.f);
		configParam(END_PARAM, 0.f, 1.f, 1.f, "End", "%", 0.f, 100.f);
		configSwitch(LOOP_PARAM, 0.f, 1.f, 0.f, "Loop", {"Off", "On"});
		configSwitch(CHANNEL_PARAM, 0.f, 3.f, 0.f, "Output channel",
			{"1·2 dynamic lowpass", "3·4 lowpass 5 kHz", "5·6 lowpass 10 kHz", "7·8 unfiltered"});
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.f, "Age (worn DAC, drift, hiss, memory rot)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.3f, "Mutate depth", "%", 0.f, 100.f);
		configButton(MUT_BTN_PARAM, "Mutate (hold 1 s to clear)");
		configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock mutation", {"Off", "On"});
		configSwitch(REC_BTN_PARAM, 0.f, 1.f, 0.f, "Record", {"Off", "Recording"});
		configSwitch(THRU_PARAM, 0.f, 1.f, 0.f, "Thru (live input through the converters)", {"Off", "On"});
		configButton(PLAY_BTN_PARAM, "Play");
		paramQuantities[TUNE_PARAM]->snapEnabled = snapTune;
		paramQuantities[CHANNEL_PARAM]->snapEnabled = true;

		configInput(TRIG_INPUT, "Trigger");
		configInput(VOCT_INPUT, "V/Oct pitch");
		configInput(VEL_INPUT, "Velocity (0-10 V)");
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

	~Crate() {
		running = false;
		jobCv.notify_all();
		if (worker.joinable()) worker.join();
		delete active;
		delete pending.exchange(nullptr);
		delete trash.exchange(nullptr);
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
		float gainDb = params[GAIN_PARAM].getValue();
		int sp = clamp((int)std::round(params[SPEED_PARAM].getValue()), 0, 2);
		int ms = clamp(memSetting.load(), 0, 2);
		crate::CaptureResult r;
		{
			std::lock_guard<std::mutex> lk(sourceMutex);
			r = crate::capture(source, sourceRate, std::pow(10.f, gainDb / 20.f), SPEED_K[sp], MEM_SECS[ms]);
		}
		Memory* m = new Memory;
		m->codes = std::move(r.codes);
		lastCaptureClipped = r.clipped;
		delete pending.exchange(m);
	}

	// ---- UI-thread actions -------------------------------------------------

	bool loadFile(const std::string& path) {
		std::vector<float> buf;
		double fs = 0;
		if (!crate::readWav(path.c_str(), buf, fs, MAX_SOURCE_SECS)) return false;
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
		requestJob(JOB_CAPTURE);   // empty source -> empty memory
	}

	std::string sampleName() {
		std::lock_guard<std::mutex> lk(sourceMutex);
		if (!missingName.empty()) return "Missing: " + missingName;
		if (sourceIsRecording) return "Recording";
		if (!sourcePath.empty()) return system::getFilename(sourcePath);
		return "";
	}

	// ---- mutation ------------------------------------------------------------

	void mutate() {
		if (params[LOCK_PARAM].getValue() > 0.5f) return;
		float m = params[MUTATE_PARAM].getValue();
		int range = (int)std::round(7.f * m);
		tuneMut = range > 0 ? (int)(random::u32() % (2 * range + 1)) - range : 0;
		decayMut = (random::uniform() * 2.f - 1.f) * 0.3f * m;
		startMut = random::uniform() < 0.5f ? random::uniform() * 0.1f * m : 0.f;
		if (random::uniform() < 0.25f * m) chanMut = (int)(random::u32() % 4);
	}

	void clearMutation() {
		tuneMut = chanMut = 0;
		decayMut = startMut = 0.f;
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
				voice.setMemory(active->codes.data(), active->codes.size());
				delete trash.exchange(old);
				if (lastCaptureClipped) captureFlash = 0.3f;
			}
		}

		// capture parameters changed -> re-run capture after 150 ms
		float g = params[GAIN_PARAM].getValue();
		int sp = (int)std::round(params[SPEED_PARAM].getValue());
		int ms = memSetting.load();
		if (g != lastGain || sp != lastSpeed || ms != lastMem) {
			if (lastSpeed >= 0) debounce = (int)(0.15f * args.sampleRate);
			lastGain = g; lastSpeed = sp; lastMem = ms;
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
		}

		crate::Frame f;
		f.trig = trig;
		float tune = params[TUNE_PARAM].getValue();
		float tuneTotal = clamp(tune + (float)tuneMutActive, -12.f, 12.f);
		f.ratioSemis = tuneTotal + 12.f * inputs[VOCT_INPUT].getVoltage();
		f.k = SPEED_K[clamp(sp, 0, 2)];
		f.vel = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 1.f;
		f.decay = clamp(params[DECAY_PARAM].getValue() + decayMutActive + inputs[DECAY_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		f.start = clamp(params[START_PARAM].getValue() + startMutActive + inputs[START_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		f.end = params[END_PARAM].getValue();
		f.loop = params[LOOP_PARAM].getValue() > 0.5f;
		f.channel = ((int)std::round(params[CHANNEL_PARAM].getValue()) + chanMut) & 3;
		f.level = params[LEVEL_PARAM].getValue();
		f.age = params[AGE_PARAM].getValue();
		f.thru = params[THRU_PARAM].getValue() > 0.5f || recording;
		f.trigConnected = inputs[TRIG_INPUT].isConnected();
		f.in = inputs[IN_INPUT].getVoltage();
		f.gain = std::pow(10.f, g / 20.f);

		crate::Result r = voice.process(f);
		if (r.end) endPulse.trigger(1e-3f);
		if (r.inClip && (f.thru || recording)) clipHold = 0.05f;
		outputs[OUT_OUTPUT].setVoltage(r.out);
		outputs[END_OUTPUT].setVoltage(endPulse.process(args.sampleTime) ? 10.f : 0.f);

		// lights (every 32 samples)
		if (++lightDiv >= 32) {
			float dt = args.sampleTime * lightDiv;
			lightDiv = 0;
			clipHold = std::max(0.f, clipHold - dt);
			captureFlash = std::max(0.f, captureFlash - dt);
			blinkPhase += dt * 4.f;
			if (blinkPhase >= 1.f) blinkPhase -= 1.f;
			bool processing = busy || jobs.load() != 0 || pending.load() != nullptr;
			float recL = recording ? 1.f : (processing ? (blinkPhase < 0.5f ? 0.8f : 0.f) : 0.f);
			lights[REC_LIGHT].setBrightness(recL);
			lights[CLIP_LIGHT].setBrightness(clipHold > 0.f || captureFlash > 0.f ? 1.f : 0.f);
			lights[PLAY_LIGHT].setBrightnessSmooth(voice.playing ? 1.f : 0.f, dt);
			double fill = active ? active->codes.size() / (MEM_SECS[clamp(ms, 0, 2)] * crate::SP_RATE) : 0.0;
			if (recording) fill = (double)recLen / staging.size();
			for (int i = 0; i < 4; i++) lights[MEM_LIGHT + i].setBrightness(clamp((float)(fill * 4.0 - i), 0.f, 1.f));
		}
	}

	// ---- persistence -------------------------------------------------------

	void onSave(const SaveEvent& e) override {
		std::lock_guard<std::mutex> lk(sourceMutex);
		if (sourceIsRecording && !source.empty()) {
			std::string dir = createPatchStorageDirectory();
			crate::writeWavFloat(system::join(dir, "recording.wav").c_str(), source, sourceRate);
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
		json_object_set_new(root, "memLength", json_integer(memSetting.load()));
		json_object_set_new(root, "tuneMut", json_integer(tuneMut));
		json_object_set_new(root, "decayMut", json_real(decayMut));
		json_object_set_new(root, "startMut", json_real(startMut));
		json_object_set_new(root, "chanMut", json_integer(chanMut));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "seed")) { ageSeed = (uint32_t)json_integer_value(j); voice.setSeed(ageSeed); }
		if (json_t* j = json_object_get(root, "snapTune")) snapTune = json_boolean_value(j);
		paramQuantities[TUNE_PARAM]->snapEnabled = snapTune;
		if (json_t* j = json_object_get(root, "memLength")) memSetting = clamp((int)json_integer_value(j), 0, 2);
		if (json_t* j = json_object_get(root, "tuneMut")) tuneMut = (int)json_integer_value(j);
		if (json_t* j = json_object_get(root, "decayMut")) decayMut = (float)json_number_value(j);
		if (json_t* j = json_object_get(root, "startMut")) startMut = (float)json_number_value(j);
		if (json_t* j = json_object_get(root, "chanMut")) chanMut = (int)json_integer_value(j) & 3;
		tuneMutActive = tuneMut; decayMutActive = decayMut; startMutActive = startMut;

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

// ---- widget ------------------------------------------------------------------

struct CrateWidget : ModuleWidget {
	CrateWidget(Crate* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Crate.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// row 1
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COL[0], ROW1_Y)), module, Crate::TUNE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COL[1], ROW1_Y)), module, Crate::DECAY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[2], ROW1_Y)), module, Crate::START_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[3], ROW1_Y)), module, Crate::END_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[4], ROW1_Y)), module, Crate::LEVEL_PARAM));

		// row 2
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[0], ROW2_Y)), module, Crate::GAIN_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(COL[0] + 6.6f, ROW2_Y + 5.4f)), module, Crate::CLIP_LIGHT));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(COL[1] - 2.0f, ROW2_Y)), module, Crate::SPEED_PARAM));
		RoundBlackSnapKnob* ch = createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(COL[2], ROW2_Y)), module, Crate::CHANNEL_PARAM);
		ch->minAngle = -0.75f * M_PI;
		ch->maxAngle = 0.75f * M_PI;
		addParam(ch);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[3], ROW2_Y)), module, Crate::AGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[4], ROW2_Y)), module, Crate::MUTATE_PARAM));

		// row 3
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(ROW3_X[0], ROW3_Y)), module, Crate::REC_BTN_PARAM, Crate::REC_LIGHT));
		for (int i = 0; i < 4; i++)
			addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(ROW3_X[0] - 3.9f + 2.6f * i, ROW3_Y + 6.0f)), module, Crate::MEM_LIGHT + i));
		addParam(createParamCentered<CKSS>(mm2px(Vec(ROW3_X[1], ROW3_Y)), module, Crate::THRU_PARAM));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenLight>>>(mm2px(Vec(ROW3_X[2], ROW3_Y)), module, Crate::PLAY_BTN_PARAM, Crate::PLAY_LIGHT));
		addParam(createParamCentered<CKSS>(mm2px(Vec(ROW3_X[3], ROW3_Y)), module, Crate::LOOP_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(ROW3_X[4], ROW3_Y)), module, Crate::MUT_BTN_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(ROW3_X[5], ROW3_Y)), module, Crate::LOCK_PARAM));

		// jacks
		const int rowA[5] = {Crate::TRIG_INPUT, Crate::VOCT_INPUT, Crate::VEL_INPUT, Crate::DECAY_INPUT, Crate::START_INPUT};
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], JACK_A_Y)), module, rowA[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[0], JACK_B_Y)), module, Crate::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[1], JACK_B_Y)), module, Crate::REC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[2], JACK_B_Y)), module, Crate::MUT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[3], JACK_B_Y)), module, Crate::END_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[4], JACK_B_Y)), module, Crate::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Crate* m = getModule<Crate>();
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
				osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, "Could not read that file. Crate loads PCM or float WAV files.");
		}));
		menu->addChild(createMenuItem("Clear sample", "", [=]() { m->clearSample(); }));
		menu->addChild(createBoolMenuItem("Snap TUNE to semitones", "",
			[=]() { return m->snapTune; },
			[=](bool v) { m->snapTune = v; m->paramQuantities[Crate::TUNE_PARAM]->snapEnabled = v; }));
		menu->addChild(createIndexSubmenuItem("Memory length", {"2.5 s", "5 s", "10 s"},
			[=]() { return (size_t)m->memSetting.load(); },
			[=](size_t i) { m->memSetting = (int)i; }));
	}
};

Model* modelCrate = createModel<Crate, CrateWidget>("Crate");
