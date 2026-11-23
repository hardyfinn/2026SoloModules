// Ectoplasm: a Haer edition of the Ectocore for VCV Rack.
// SPDX-License-Identifier: GPL-3.0-only
//
// Based on rack/src/Ectocore.cpp from github.com/schollz/_core
// (Copyright 2026 Zack Scholl, GPL-3.0-only). Modified by Haer, 2026-11-20:
// new 24 HP panel; every shifted (hold TAP / hold MODE) function on its own
// knob; dedicated BANK, TEMPO, PLAY and RESET; CV inputs for most of them; a
// text display; plain WAV folders. The sound engine is the unchanged Ectocore
// firmware compiled for the desktop (see engine/ and README.md).
#include "plugin.hpp"
#include <osdialog.h>
#include "Storage.hpp"
#include "State.hpp"
#include <atomic>
#include <cstring>

// ---- LAYOUT (mm, shared with res/make_panel.py) ----
static const float DISP_X = 7.f, DISP_Y = 13.5f, DISP_W = 107.92f, DISP_H = 10.f;
static const float BREAK_X = 60.96f, BREAK_Y = 40.f, RING_R = 12.8f;
static const float GRIM_X = 17.f, AMEN_X = 104.92f, TOP_Y = 35.f;
static const float BTN_Y = 49.5f;
static const float BTN_X[4] = {11.f, 24.f, 97.92f, 110.92f}; // MODE MULT TAP PLAY
static const float ROW2_Y = 64.f;
static const float ROW2_X[4] = {17.f, 41.f, 80.92f, 104.92f}; // SAMPLE BANK JUMP TEMPO
static const float LAYER_X[5] = {15.f, 38.f, 60.96f, 83.92f, 106.92f};
static const float TAP_Y = 79.6f, MODE_Y = 94.1f;
static const float JACK_X0 = 8.f, JACK_DX = 13.24f, JACK_Y1 = 107.5f, JACK_Y2 = 119.f;

struct Ectoplasm : engine::Module {
	enum Params {
		// engine knobs (firmware order), then engine buttons (firmware order)
		BREAK, GRIMOIRE, AMEN, JUMP, SAMPLE,
		MODE, MULT, BANK_BTN, TAP,
		// Ectoplasm's own
		BANK, TEMPO,
		DRIVE, FILTER, GATE, RETRIG, PITCH,
		AMIGA, SATURATE, SMEAR, JITTER, CHAOS,
		PLAY,
		NUM_PARAMS
	};
	enum Inputs {
		AMEN_CV, BREAK_CV, SAMPLE_CV, CLOCK_IN, // engine order
		RESET_IN, PLAY_IN, BANK_CV, GRIM_CV, TEMPO_CV,
		DRIVE_CV, FILTER_CV, GATE_CV, PITCH_CV, RETRIG_CV,
		NUM_INPUTS
	};
	enum Outputs {LEFT, RIGHT, TRIGGER, CLOCK_OUT, NUM_OUTPUTS};
	enum Lights {RING, MODES = RING + 18 * 3, TAP_LIGHT = MODES + 4, PLAY_LIGHT, TRIGGER_LIGHT, CLOCK_LIGHT, NUM_LIGHTS};
	enum Layer {L_DRIVE, L_FILTER, L_GATE, L_RETRIG, L_PITCH, L_AMIGA, L_SATURATE, L_SMEAR, L_JITTER, L_CHAOS, NUM_LAYER};

	struct Update {CoreState state;};
	CoreEngine *engine = nullptr;
	ecto::Storage storage;
	ecto::Bank *active = nullptr;
	ecto::Queue<Update, 16> edits;
	ecto::Queue<CoreState, 8> snapshots;
	CoreState initial{}, uiState{}, restoreState{};
	bool restorePending = false;
	std::atomic<bool> resetRequested{false};
	std::atomic<unsigned> selectedBank{0}, selectedSlot{0}, slice{0}, tempo{0}, rune{0}, trigMode{0};
	std::atomic<bool> running{false}, haveSamples{false};
	std::mutex pathMutex;
	std::string folder;
	dsp::SchmittTrigger clockTrigger, resetTrigger, playTrigger, playButton;
	dsp::SampleRateConverter<2> resampler;
	dsp::Frame<2> resampleInput{}, resampleOutput[64]{}, outputQueue[256]{};
	unsigned outCount = 0, outRead = 0, outWrite = 0;
	double corePhase = 0;
	unsigned ticks = 0;
	int requestedBank = -1;
	// Dedicated controls act like the hardware's shifted knobs: only when they
	// move. The first reading after load is remembered without applying it, so
	// a patch's saved engine state is kept.
	int lastLayer[NUM_LAYER];
	int lastBank = -1, lastTempo = -1;
	bool primed = false;
	std::atomic<bool> maintenanceStop{false};
	std::thread maintenance;
	std::atomic<CoreEngine*> preparedEngine{nullptr}, retiredEngine{nullptr};

	static CoreEngine *newEngine() {std::random_device random; return core_engine_create((uint64_t(random()) << 32) | random());}

	Ectoplasm() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(BREAK, 0, 1, 0, "Break: effect probability", "%", 0, 100);
		configParam(GRIMOIRE, 0, 1, 0, "Grimoire: effect bank (rune 1-7)");
		configParam(AMEN, 0, 1, 0, "Amen: straight, 1-32 step loops, or random (fully right)");
		configParam(JUMP, 0, 1, .5f, "Jump: left = wander off, right = jump and return");
		configParam(SAMPLE, 0, 1, 0, "Sample");
		configButton(MODE, "Mode: trigger output Kick / Snare / Transient / Random (hold for the Mode layer)");
		configButton(MULT, "Mult: tap to multiply the clock output, hold to divide");
		configButton(BANK_BTN, "Bank");
		configButton(TAP, "Tap tempo (hold for the Tap layer)");
		for (int p = MODE; p <= TAP; ++p) getParamQuantity(p)->randomizeEnabled = false;
		configParam(BANK, 0, 15, 0, "Bank (of the banks that hold samples)", "", 0, 1, 1);
		getParamQuantity(BANK)->snapEnabled = true;
		configParam(TEMPO, 30, 300, 120, "Tempo (when no clock is patched)", " BPM");
		getParamQuantity(TEMPO)->snapEnabled = true;
		configParam(DRIVE, 0, 1, .5f, "Drive: volume to the middle, then saturation, then fuzz");
		configParam(FILTER, 0, 1, .5f, "DJ filter: low-pass left, high-pass right, off in the middle");
		configParam(GATE, 0, 1, 0, "Gate: slice gating (off at minimum)");
		configParam(RETRIG, 0, 1, 0, "Retrig: probability of stutters", "%", 0, 100);
		configParam(PITCH, 0, 1, .5f, "Pitch: down left, up right");
		configParam(AMIGA, 0, 1, 0, "Amiga: lo-fi resampling amount");
		configParam(SATURATE, 0, 1, 0, "Saturate: digital saturation", "%", 0, 100);
		configParam(SMEAR, 0, 1, 0, "Smear: digital smear", "%", 0, 100);
		configParam(JITTER, 0, 1, 0, "Jitter: digital jitter", "%", 0, 100);
		configParam(CHAOS, 0, 1, 0, "Chaos: trembler", "%", 0, 100);
		configButton(PLAY, "Play / stop");
		getParamQuantity(PLAY)->randomizeEnabled = false;
		configInput(AMEN_CV, "Amen CV (slice select, see Device settings)");
		configInput(BREAK_CV, "Break CV (effect probability)");
		configInput(SAMPLE_CV, "Sample CV");
		configInput(CLOCK_IN, "Clock (2 PPQN)");
		configInput(RESET_IN, "Reset (next clock is beat 1)");
		configInput(PLAY_IN, "Play / stop trigger");
		configInput(BANK_CV, "Bank CV (0-10 V across the banks)");
		configInput(GRIM_CV, "Grimoire CV (0-10 V across the runes)");
		configInput(TEMPO_CV, "Tempo CV (10 BPM per volt)");
		configInput(DRIVE_CV, "Drive CV (0-10 V)");
		configInput(FILTER_CV, "Filter CV (+-5 V)");
		configInput(GATE_CV, "Gate CV (0-10 V)");
		configInput(PITCH_CV, "Pitch CV (+-5 V)");
		configInput(RETRIG_CV, "Retrig CV (0-10 V)");
		configOutput(LEFT, "Left audio");
		configOutput(RIGHT, "Right audio");
		configOutput(TRIGGER, "Trigger (mode set by MODE)");
		configOutput(CLOCK_OUT, "Clock");
		configBypass(LEFT, LEFT);
		engine = newEngine();
		if (!engine) throw Exception("Cannot allocate the Ectoplasm engine");
		core_engine_get_state(engine, &initial);
		uiState = initial;
		resampler.setQuality(8);
		resampler.setRates(CORE_RATE, CORE_RATE);
		for (int &v : lastLayer) v = -1;
		maintenance = std::thread([this] {
			while (!maintenanceStop) {
				if (auto *old = retiredEngine.exchange(nullptr)) core_engine_destroy(old);
				if (!preparedEngine.load() && resetRequested.exchange(false)) {
					auto *next = newEngine();
					if (next) preparedEngine.store(next);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		});
	}
	~Ectoplasm() {
		maintenanceStop = true;
		maintenance.join();
		core_engine_destroy(engine);
		core_engine_destroy(preparedEngine.exchange(nullptr));
		core_engine_destroy(retiredEngine.exchange(nullptr));
		delete active;
	}
	std::string getFolder() {std::lock_guard<std::mutex> lock(pathMutex); return folder;}
	void choose(const std::string &path) {
		{std::lock_guard<std::mutex> lock(pathMutex); folder = path;}
		storage.load(path, selectedBank.load(), true);
	}
	void collectUi() {CoreState state; while (snapshots.pop(state)) uiState = state;}
	static bool read(void *user, unsigned b, unsigned s, unsigned v, uint64_t pos, void *out, size_t n) {
		auto *m = static_cast<Ectoplasm*>(user);
		return m->storage.read(m->active, b, s, v, pos, out, n);
	}
	void onSampleRateChange(const SampleRateChangeEvent &e) override {
		Module::onSampleRateChange(e);
		resampler.setRates(CORE_RATE, int(e.sampleRate));
		outCount = outRead = outWrite = 0;
		corePhase = 0;
	}
	void onReset(const ResetEvent &e) override {Module::onReset(e); resetRequested = true; primed = false;}
	void applyStartupPlayback() {
		core_engine_apply_startup_playback(engine);
		CoreState state;
		core_engine_get_state(engine, &state);
		if (state.startup_wait) {
			outCount = outRead = outWrite = 0;
			corePhase = 0;
			if (resampler.st) {
				speex_resampler_reset_mem(resampler.st);
				resampleInput = {};
				const int latency = speex_resampler_get_input_latency(resampler.st);
				for (int i = 0; i <= 2 * latency; ++i) {
					int inFrames = 1, outFrames = 64;
					resampler.process(&resampleInput, &inFrames, resampleOutput, &outFrames);
				}
				speex_resampler_reset_mem(resampler.st);
			}
		}
	}
	float unit(int param, int input, bool bipolar) {
		float v = params[param].getValue();
		if (inputs[input].isConnected()) v += inputs[input].getVoltage() / 10.f;
		(void)bipolar;
		return clamp(v, 0.f, 1.f);
	}
	static int knob1024(float v) {return clamp(int(std::lround(v * 1023.f)), 0, 1023);}
	void dedicatedControls() {
		// Hidden layers: knob (+ CV) on the firmware's 0..1023 scale.
		int now[NUM_LAYER];
		now[L_DRIVE] = knob1024(unit(DRIVE, DRIVE_CV, false));
		now[L_FILTER] = knob1024(unit(FILTER, FILTER_CV, true));
		now[L_GATE] = knob1024(unit(GATE, GATE_CV, false));
		now[L_RETRIG] = knob1024(unit(RETRIG, RETRIG_CV, false));
		now[L_PITCH] = knob1024(unit(PITCH, PITCH_CV, true));
		now[L_AMIGA] = knob1024(params[AMIGA].getValue());
		now[L_SATURATE] = knob1024(params[SATURATE].getValue());
		now[L_SMEAR] = knob1024(params[SMEAR].getValue());
		now[L_JITTER] = knob1024(params[JITTER].getValue());
		now[L_CHAOS] = knob1024(params[CHAOS].getValue());
		int bankCount = core_haer_bank_count(engine);
		float bankV = params[BANK].getValue() / 15.f;
		if (inputs[BANK_CV].isConnected()) bankV += inputs[BANK_CV].getVoltage() / 10.f;
		bankV = clamp(bankV, 0.f, 1.f);
		int bank = bankCount > 1 ? int(std::lround(bankV * (bankCount - 1))) : 0;
		float bpm = params[TEMPO].getValue();
		if (inputs[TEMPO_CV].isConnected()) bpm += inputs[TEMPO_CV].getVoltage() * 10.f;
		int bpmI = clamp(int(std::lround(bpm)), 30, 300);
		if (!primed) {
			for (int i = 0; i < NUM_LAYER; ++i) lastLayer[i] = now[i];
			lastBank = bank;
			lastTempo = bpmI;
			primed = true;
			return;
		}
		for (int i = 0; i < NUM_LAYER; ++i) {
			if (std::abs(now[i] - lastLayer[i]) < 2) continue;
			lastLayer[i] = now[i];
			switch (i) {
				case L_DRIVE: core_haer_drive(engine, now[i]); break;
				case L_FILTER: core_haer_filter(engine, now[i]); break;
				case L_GATE: core_haer_gate(engine, now[i]); break;
				case L_RETRIG: core_haer_retrig(engine, now[i]); break;
				case L_PITCH: core_haer_pitch(engine, now[i]); break;
				default: core_haer_mode_layer(engine, i - L_AMIGA, now[i]); break;
			}
		}
		if (bank != lastBank) {lastBank = bank; core_haer_bank(engine, bank);}
		if (bpmI != lastTempo) {lastTempo = bpmI; core_haer_tempo(engine, bpmI);}
	}
	void process(const ProcessArgs &args) override {
		if (auto *fresh = preparedEngine.exchange(nullptr)) {
			CoreState previous;
			core_engine_get_state(engine, &previous);
			auto *old = engine;
			engine = fresh;
			retiredEngine.store(old);
			resetRequested = false;
			primed = false;
			if (active) {
				core_engine_set_catalogue(engine, active->library->banks.data(), read, this);
				core_engine_set_resident_bank(engine, active->index);
				CoreState s;
				core_engine_get_state(engine, &s);
				ecto::applySettings(*active->library, s);
				core_engine_update_settings(engine, &s);
			}
			CoreState reboot;
			core_engine_get_state(engine, &reboot);
			reboot.start_tempo = previous.start_tempo;
			reboot.startup_wait = previous.startup_wait;
			core_engine_update_settings(engine, &reboot);
			core_engine_apply_start_tempo(engine);
			applyStartupPlayback();
			outCount = outRead = outWrite = 0;
		}
		ecto::Bank *bank;
		if (storage.retired.canPush() && storage.ready.pop(bank)) {
			if (active && bank->library->generation < active->library->generation) storage.retired.push(bank);
			else {
				bool same = active && active->library->generation == bank->library->generation;
				bool playbackStartup = !active || restorePending;
				bool startup = !active || active->library->root != bank->library->root || restorePending;
				if (active) storage.retired.push(active);
				active = bank;
				storage.activeGeneration = bank->library->generation;
				core_engine_set_catalogue(engine, bank->library->banks.data(), read, this);
				if (restorePending) {core_engine_set_state(engine, &restoreState); restorePending = false;}
				core_engine_set_resident_bank(engine, bank->index);
				if (!same && (!hasPatchSettings || bank->library->importSettings)) {
					auto s = initial;
					ecto::applySettings(*bank->library, s);
					core_engine_update_settings(engine, &s);
					hasPatchSettings = true;
				}
				if (startup) core_engine_apply_start_tempo(engine);
				if (playbackStartup) applyStartupPlayback();
				requestedBank = -1;
				haveSamples = true;
			}
		}
		Update update;
		while (edits.pop(update)) {core_engine_update_settings(engine, &update.state); hasPatchSettings = true;}

		CoreControls controls{};
		for (unsigned i = 0; i < 5; ++i) controls.knobs[i] = params[i].getValue();
		if (inputs[GRIM_CV].isConnected()) controls.knobs[GRIMOIRE] = clamp(controls.knobs[GRIMOIRE] + inputs[GRIM_CV].getVoltage() / 10.f, 0.f, 1.f);
		for (unsigned i = 0; i < 4; ++i) controls.buttons[i] = params[MODE + i].getValue() > .5f;
		for (unsigned i = 0; i < 3; ++i) {controls.cv[i] = inputs[i].getVoltage(); controls.connected[i] = inputs[i].isConnected();}
		core_engine_controls(engine, &controls);
		if ((ticks % 32) == 0) dedicatedControls();
		bool play = playButton.process(params[PLAY].getValue());
		if (playTrigger.process(inputs[PLAY_IN].getVoltage(), .1f, 1.f)) play = true;
		if (play) core_haer_play_toggle(engine);
		if (resetTrigger.process(inputs[RESET_IN].getVoltage(), .1f, 1.f)) core_haer_reset(engine);
		bool edge = clockTrigger.process(inputs[CLOCK_IN].getVoltage(), .1f, 1.f);
		if (edge) core_engine_clock(engine, true);
		else if (!clockTrigger.isHigh()) core_engine_clock(engine, false);

		corePhase += CORE_RATE / double(args.sampleRate);
		while (corePhase >= 1) {
			corePhase -= 1;
			int16_t pcm[2];
			core_engine_process(engine, pcm);
			for (unsigned c = 0; c < 2; ++c) resampleInput.samples[c] = float(pcm[c]) / 32768.f;
			int inFrames = 1, outFrames = 64;
			resampler.process(&resampleInput, &inFrames, resampleOutput, &outFrames);
			for (int i = 0; i < outFrames && outCount < 256; ++i) {
				outputQueue[outWrite] = resampleOutput[i];
				outWrite = (outWrite + 1) % 256;
				++outCount;
			}
		}
		if (outCount) {
			outputs[LEFT].setVoltage(outputQueue[outRead].samples[0] * 5);
			outputs[RIGHT].setVoltage(outputQueue[outRead].samples[1] * 5);
			outRead = (outRead + 1) % 256;
			--outCount;
		} else {
			outputs[LEFT].setVoltage(0);
			outputs[RIGHT].setVoltage(0);
		}
		CoreDisplay display{};
		core_engine_display(engine, &display);
		outputs[TRIGGER].setVoltage(display.trigger ? 10 : 0);
		outputs[CLOCK_OUT].setVoltage(display.clock ? 10 : 0);
		if ((++ticks % 44) == 0) {
			for (unsigned i = 0; i < 18; ++i)
				for (unsigned c = 0; c < 3; ++c) lights[RING + 3 * i + c].setBrightness(display.rgb[i][c] / 117.f);
			unsigned mode = 0;
			for (unsigned i = 0; i < 4; ++i) {lights[MODES + i].setBrightness(display.mode[i]); if (display.mode[i]) mode = i;}
			lights[TAP_LIGHT].setBrightness(display.tap);
			lights[PLAY_LIGHT].setBrightness(!display.stopped && haveSamples);
			lights[TRIGGER_LIGHT].setBrightness(display.trigger);
			lights[CLOCK_LIGHT].setBrightness(display.clock);
			selectedBank = display.bank;
			selectedSlot = display.slot;
			slice = display.slice;
			tempo = display.tempo;
			rune = display.rune;
			trigMode = mode;
			running = !display.stopped;
			if (display.requested_bank < 16 && requestedBank != int(display.requested_bank)) {
				requestedBank = display.requested_bank;
				storage.requestedBank = requestedBank;
			}
			if (display.reboot_requested) resetRequested = true;
		}
		if (ticks % 512 == 0) {CoreState s; core_engine_get_state(engine, &s); snapshots.push(s);}
	}
	bool hasPatchSettings = false;
	json_t *dataToJson() override {
		auto *j = json_object();
		json_object_set_new(j, "schema", json_integer(1));
		auto path = getFolder();
		json_object_set_new(j, "folder", json_string(path.c_str()));
		CoreState s;
		core_engine_get_state(engine, &s);
		json_object_set_new(j, "state", ecto::stateJson(s));
		return j;
	}
	void dataFromJson(json_t *j) override {
		for (int p = MODE; p <= TAP; ++p) params[p].setValue(0);
		primed = false;
		restoreState = initial;
		restorePending = ecto::stateFromJson(json_object_get(j, "state"), restoreState);
		hasPatchSettings = restorePending;
		if (restorePending) restoreState.tempo = start_tempo_resolve(restoreState.start_tempo, restoreState.tempo);
		if (restorePending && !core_engine_set_state(engine, &restoreState)) restorePending = hasPatchSettings = false;
		if (restorePending) applyStartupPlayback();
		outCount = outRead = outWrite = 0;
		auto *path = json_string_value(json_object_get(j, "folder"));
		if (path && *path) {
			{std::lock_guard<std::mutex> lock(pathMutex); folder = path;}
			storage.load(path, restorePending ? restoreState.bank : 0);
		}
	}
};

struct StartTempoField : ui::TextField {
	Ectoplasm *module = nullptr;
	void onAction(const ActionEvent &e) override {
		uint16_t bpm = 0;
		if (start_tempo_parse(text.data(), text.size(), &bpm) && bpm) {
			auto state = module->uiState;
			state.start_tempo = bpm;
			if (module->edits.push({state})) module->uiState = state;
		}
		setText(module->uiState.start_tempo ? std::to_string(module->uiState.start_tempo) : "130");
		e.consume(this);
	}
};

// Two-line text display: position and tempo above, sample name / status below.
struct EctoDisplay : widget::Widget {
	Ectoplasm *module = nullptr;
	std::string name, status;
	unsigned frame = 0;
	void step() override {
		Widget::step();
		if (!module || (frame++ % 15)) return;
		name = module->storage.sampleName(module->selectedBank, module->selectedSlot);
		status = module->storage.status();
	}
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		NVGcontext *vg = args.vg;
		nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		NVGcolor cyan = nvgRGB(0x7f, 0xe0, 0xc4), dim = nvgRGB(0x6f, 0x9e, 0x92);
		char line[96];
		if (!module) {
			nvgFontSize(vg, 13.f); nvgFillColor(vg, cyan);
			nvgText(vg, 6, 15, "B01 S01  SL 01  172 BPM  RUNE 1", nullptr);
			nvgFontSize(vg, 10.f); nvgFillColor(vg, dim);
			nvgText(vg, 6, 27, "amen break 172bpm", nullptr);
			return;
		}
		static const char *modes[] = {"KICK", "SNARE", "TRANS", "RAND"};
		bool ok = module->haveSamples;
		if (ok) snprintf(line, sizeof line, "B%02u S%02u  SL %02u  %3u BPM  RUNE %u  %s%s", module->selectedBank.load() + 1, module->selectedSlot.load() + 1,
			module->slice.load() + 1, module->tempo.load(), module->rune.load() + 1, modes[module->trigMode.load() & 3], module->running ? "" : "  STOP");
		else snprintf(line, sizeof line, "NO SAMPLES  %3u BPM  RUNE %u", module->tempo.load(), module->rune.load() + 1);
		nvgFontSize(vg, 13.f); nvgFillColor(vg, cyan);
		nvgText(vg, 6, 15, line, nullptr);
		std::string second = ok ? (name.empty() ? status : name) : (status == "Choose a sample folder" ? "Right-click > Choose sample folder" : status);
		if (second.size() > 54) second = second.substr(0, 53) + "~";
		nvgFontSize(vg, 10.f); nvgFillColor(vg, dim);
		nvgText(vg, 6, 27, second.c_str(), nullptr);
	}
};

struct EctoplasmWidget : app::ModuleWidget {
	EctoplasmWidget(Ectoplasm *m) {
		setModule(m);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ectoplasm.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto *disp = createWidget<EctoDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		disp->box.size = mm2px(Vec(DISP_W, DISP_H));
		disp->module = m;
		addChild(disp);

		// BREAK and its sixteen-light ring (the firmware's own LED wheel)
		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(BREAK_X, BREAK_Y)), m, Ectoplasm::BREAK));
		for (unsigned i = 0; i < 16; ++i) {
			float a = -2.356f + i * 4.712f / 15.f;
			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(BREAK_X + std::sin(a) * RING_R, BREAK_Y - std::cos(a) * RING_R)), m, Ectoplasm::RING + 3 * i));
		}
		// the hardware's two status lights, at the ends of the display
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(DISP_X - 2.6f, DISP_Y + DISP_H / 2)), m, Ectoplasm::RING + 3 * 16));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(DISP_X + DISP_W + 2.6f, DISP_Y + DISP_H / 2)), m, Ectoplasm::RING + 3 * 17));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(GRIM_X, TOP_Y)), m, Ectoplasm::GRIMOIRE));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(AMEN_X, TOP_Y)), m, Ectoplasm::AMEN));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(BTN_X[0], BTN_Y)), m, Ectoplasm::MODE));
		for (unsigned i = 0; i < 4; ++i)
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(4.6f, BTN_Y - 3.f + i * 2.f)), m, Ectoplasm::MODES + i));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(BTN_X[1], BTN_Y)), m, Ectoplasm::MULT));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(BTN_X[2], BTN_Y)), m, Ectoplasm::TAP, Ectoplasm::TAP_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(BTN_X[3], BTN_Y)), m, Ectoplasm::PLAY, Ectoplasm::PLAY_LIGHT));

		const int row2[4] = {Ectoplasm::SAMPLE, Ectoplasm::BANK, Ectoplasm::JUMP, Ectoplasm::TEMPO};
		for (int i = 0; i < 4; ++i) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ROW2_X[i], ROW2_Y)), m, row2[i]));
		const int tapLayer[5] = {Ectoplasm::DRIVE, Ectoplasm::FILTER, Ectoplasm::GATE, Ectoplasm::RETRIG, Ectoplasm::PITCH};
		const int modeLayer[5] = {Ectoplasm::AMIGA, Ectoplasm::SATURATE, Ectoplasm::SMEAR, Ectoplasm::JITTER, Ectoplasm::CHAOS};
		for (int i = 0; i < 5; ++i) {
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(LAYER_X[i], TAP_Y)), m, tapLayer[i]));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(LAYER_X[i], MODE_Y)), m, modeLayer[i]));
		}

		const int row1[9] = {Ectoplasm::CLOCK_IN, Ectoplasm::RESET_IN, Ectoplasm::PLAY_IN, Ectoplasm::AMEN_CV, Ectoplasm::BREAK_CV,
			Ectoplasm::SAMPLE_CV, Ectoplasm::BANK_CV, Ectoplasm::GRIM_CV, Ectoplasm::TEMPO_CV};
		for (int i = 0; i < 9; ++i) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + i * JACK_DX, JACK_Y1)), m, row1[i]));
		const int row2in[5] = {Ectoplasm::DRIVE_CV, Ectoplasm::FILTER_CV, Ectoplasm::GATE_CV, Ectoplasm::RETRIG_CV, Ectoplasm::PITCH_CV};
		for (int i = 0; i < 5; ++i) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + i * JACK_DX, JACK_Y2)), m, row2in[i]));
		const int outs[4] = {Ectoplasm::TRIGGER, Ectoplasm::CLOCK_OUT, Ectoplasm::LEFT, Ectoplasm::RIGHT};
		for (int i = 0; i < 4; ++i) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JACK_X0 + (i + 5) * JACK_DX, JACK_Y2)), m, outs[i]));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(JACK_X0 + 5 * JACK_DX + 4.2f, JACK_Y2 - 3.6f)), m, Ectoplasm::TRIGGER_LIGHT));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(JACK_X0 + 6 * JACK_DX + 4.2f, JACK_Y2 - 3.6f)), m, Ectoplasm::CLOCK_LIGHT));
	}

	void appendContextMenu(ui::Menu *menu) override {
		auto *m = getModule<Ectoplasm>();
		if (!m) return;
		m->collectUi();
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel(m->storage.status()));
		menu->addChild(createMenuItem("Choose sample folder…", "", [m] {
			char *path = osdialog_file(OSDIALOG_OPEN_DIR, nullptr, nullptr, nullptr);
			if (path) {m->choose(path); std::free(path);}
		}));
		menu->addChild(createMenuItem("Reload sample folder", "", [m] {auto path = m->getFolder(); if (!path.empty()) m->storage.load(path, m->selectedBank.load());}));
		menu->addChild(createMenuItem("Import settings from sample folder", "", [m] {auto path = m->getFolder(); if (!path.empty()) m->storage.load(path, m->selectedBank.load(), true);}));
		menu->addChild(createMenuItem("Reveal sample folder", "", [m] {auto path = m->getFolder(); if (!path.empty()) system::openDirectory(path);}));
		menu->addChild(createMenuItem("Reboot engine", "", [m] {m->resetRequested = true;}));
		menu->addChild(createSubmenuItem("Device settings", "", [m](ui::Menu *sub) {
			sub->addChild(createSubmenuItem("Startup playback", m->uiState.startup_wait ? "Wait" : "Play immediately", [m](ui::Menu *mnu) {
				for (bool wait : {false, true})
					mnu->addChild(createCheckMenuItem(wait ? "Wait for clock or PLAY" : "Play immediately (default)", "",
						[m, wait] {return m->uiState.startup_wait == wait;},
						[m, wait] {auto s = m->uiState; s.startup_wait = wait; if (m->edits.push({s})) m->uiState = s;}));
				mnu->addChild(createMenuLabel("Applies on patch open or reboot."));
			}));
			sub->addChild(createSubmenuItem("Start tempo", m->uiState.start_tempo ? std::to_string(m->uiState.start_tempo) + " BPM" : "Default", [m](ui::Menu *mnu) {
				mnu->addChild(createCheckMenuItem("Default (no override)", "", [m] {return m->uiState.start_tempo == 0;},
					[m] {auto s = m->uiState; s.start_tempo = 0; if (m->edits.push({s})) m->uiState = s;}));
				mnu->addChild(createMenuLabel("Fixed BPM (30–300): enter to save"));
				auto *field = new StartTempoField;
				field->module = m;
				field->box.size = Vec(240, 28);
				field->setText(m->uiState.start_tempo ? std::to_string(m->uiState.start_tempo) : "130");
				mnu->addChild(field);
				mnu->addChild(createMenuLabel("Applies on patch open or reboot."));
			}));
			auto toggle = [&](const char *label, bool CoreState::*field) {
				sub->addChild(createCheckMenuItem(label, "", [m, field] {return m->uiState.*field;},
					[m, field] {auto s = m->uiState; s.*field = !(s.*field); m->edits.push({s});}));
			};
			toggle("Stop when clock stops", &CoreState::clock_stop);
			toggle("Clock output is a trigger", &CoreState::clock_trigger);
			toggle("Clock follows slices", &CoreState::clock_slice);
			const char *names[] = {"Amen CV bipolar", "Break CV bipolar", "Sample CV bipolar"};
			for (unsigned i = 0; i < 3; ++i)
				sub->addChild(createCheckMenuItem(names[i], "", [m, i] {return m->uiState.bipolar[i];},
					[m, i] {auto s = m->uiState; s.bipolar[i] = !s.bipolar[i]; m->edits.push({s});}));
			sub->addChild(createSubmenuItem("Amen CV behavior", "", [m](ui::Menu *mnu) {
				const char *names[] = {"Jump", "Repeat", "Split"};
				for (unsigned i = 0; i < 3; ++i)
					mnu->addChild(createCheckMenuItem(names[i], "", [m, i] {return m->uiState.amen_behavior == i;},
						[m, i] {auto s = m->uiState; s.amen_behavior = i; m->edits.push({s});}));
			}));
			sub->addChild(createCheckMenuItem("Sample CV: 1 V/oct", "", [m] {return m->uiState.sample_mapping == 1;},
				[m] {auto s = m->uiState; s.sample_mapping = 1 - s.sample_mapping; m->edits.push({s});}));
			sub->addChild(createSubmenuItem("Reset from a CV jack", "", [m](ui::Menu *mnu) {
				const char *names[] = {"None (use RESET)", "Amen", "Break", "Sample", "Clock"};
				for (int i = -1; i < 4; ++i)
					mnu->addChild(createCheckMenuItem(names[i + 1], "", [m, i] {return m->uiState.reset_input == i;},
						[m, i] {auto s = m->uiState; s.reset_input = i; m->edits.push({s});}));
			}));
			sub->addChild(createSubmenuItem("Ring brightness", "", [m](ui::Menu *mnu) {
				for (unsigned v : {0u, 25u, 50u, 75u, 100u})
					mnu->addChild(createCheckMenuItem(std::to_string(v) + "%", "", [m, v] {return m->uiState.brightness == v;},
						[m, v] {auto s = m->uiState; s.brightness = v; m->edits.push({s});}));
			}));
		}));
		menu->addChild(createSubmenuItem("Grimoire (effects per rune)", "", [m](ui::Menu *sub) {
			static const char *effects[] = {"Saturate", "Shaper", "Fuzz", "Bitcrush", "Time stretch", "Delay", "Comb", "Beat repeat",
				"Tighten", "Expand / reverb", "Pan", "Scratch", "Filter", "Repitch", "Reverse", "Tape stop"};
			for (unsigned r = 0; r < 7; ++r)
				sub->addChild(createSubmenuItem("Rune " + std::to_string(r + 1), "", [m, r](ui::Menu *mnu) {
					for (unsigned f = 0; f < 16; ++f)
						mnu->addChild(createCheckMenuItem(effects[f], "", [m, r, f] {return m->uiState.runes[r][f];},
							[m, r, f] {auto s = m->uiState; s.runes[r][f] = !s.runes[r][f]; m->edits.push({s});}));
				}));
		}));
	}
};

Model *modelEctoplasm = createModel<Ectoplasm, EctoplasmWidget>("Ectoplasm");
