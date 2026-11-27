// One module template for the 909 Circuits voices: params, jacks and panel positions
// all come from the voice's knob table (res/voices.py -> src/VoiceSpecs.hpp).
#pragma once
#include "plugin.hpp"
#include "Engines909.hpp"
#include "Wav909.hpp"
#include <osdialog.h>
#include <cstdio>
#include <atomic>
#include <mutex>

namespace c909 {

// ---- LAYOUT (same numbers in res/make_voice_panels.py) ----
static const float PW = 81.28f;   // 16 HP
static inline float bigX(int c) { return PW / 6.f + c * PW / 3.f; }       // rows 0-1
static inline float smallX(int c) { return PW / 8.f + c * PW / 4.f; }     // rows 2-4
static const float ROW_Y[5] = {27.f, 44.5f, 63.f, 76.5f, 90.f};
static const float JACK_Y = 112.f;
static inline float jackX(int i, int n) { return 7.5f + i * (PW - 15.f) / (n - 1); }

struct KnobQuantity : ParamQuantity {
	const KnobDef* def = nullptr;
	std::string getDisplayValueString() override {
		if (!def) return ParamQuantity::getDisplayValueString();
		float v = knobValue(*def, getValue());
		switch (def->disp) {
			case DISP_PCT: return string::f("%.0f %%", v * 100.f);
			case DISP_INT: return string::f("%.0f", v);
			default: return string::f("%.3g%s", v, def->unit);
		}
	}
};

static std::vector<std::string> splitNames(const char* s) {
	std::vector<std::string> out;
	std::string cur;
	for (const char* p = s; *p; p++) {
		if (*p == '|') { out.push_back(cur); cur.clear(); }
		else cur += *p;
	}
	out.push_back(cur);
	return out;
}

template <class E>
struct VoiceModule : Module {
	E engine;
	std::vector<float> k;
	dsp::SchmittTrigger trigIn, hitButton;
	dsp::ClockDivider lightDivider;
	uint32_t serial = 909;
	dsp::SchmittTrigger trig2In;
	int pendingWhich = 0;
	std::string samplePath;
	// a new sample is handed to the audio thread through here
	std::mutex sampleMutex;
	std::vector<float> newSample;   // already converted for the engine
	std::string sampleStatus;       // shown in the menu when a load failed
	std::atomic<bool> sampleReady{false};
	std::atomic<uint32_t> newSerial{0};
	int nK = 0, nCv = 0;
	// accent is read over 0.5 ms after a trigger (an accent through another module may
	// arrive a sample or two late); every voice waits the same, so the kit stays aligned
	int pending = -1;
	float pendingAcc = 0.f;

	int hitParam() const { return nK; }
	int randParam() const { return nK + 1; }
	int nearParam() const { return nK + 2; }
	dsp::BooleanTrigger randBtn, nearBtn;
	dsp::SchmittTrigger randIn, nearIn;
	std::atomic<int> keyRandom{0};   // 1 = R, 2 = N, set by the widget (hover + key)
	int randInput() const { return nCv + 2 + (E::EXTRA_TRIG ? 1 : 0); }
	int nearInput() const { return randInput() + 1; }
	int trigInput() const { return nCv; }
	int accentInput() const { return nCv + 1; }
	enum OutputIds { OUT_OUTPUT, NUM_OUTPUTS };

	VoiceModule() {
		const VoiceSpec& s = E::spec();
		nK = s.nKnobs; nCv = s.nCv;
		k.assign(nK, 0.f);
		config(nK + 3, nCv + 2 + (E::EXTRA_TRIG ? 1 : 0) + 2, NUM_OUTPUTS, 1);
		for (int i = 0; i < nK; i++) {
			const KnobDef& d = s.knobs[i];
			if (d.kind == 0) {
				KnobQuantity* q = configParam<KnobQuantity>(i, 0.f, 1.f, d.stock, d.tip);
				q->def = &d;
				if (d.norand) q->randomizeEnabled = false;
			}
			else {
				configSwitch(i, 0.f, d.kind == 1 ? 2.f : 1.f, d.stock, d.tip, splitNames(d.unit));
			}
		}
		configButton(nK, "Hit");
		configButton(nK + 1, "Random (every knob except LEVEL and AGE, anywhere on its range; switches stay; key R)");
		configButton(nK + 2, "Near stock (every knob except LEVEL and AGE; values near the stock 909 twice as likely as the ends; key N)");
		for (int i = 0; i < nCv; i++) {
			const CvDef& c = s.cv[i];
			if (c.knob < 0) configInput(i, "V/OCT");
			else configInput(i, std::string(s.knobs[c.knob].tip) + " CV (+-5 V = full range)");
		}
		configInput(nCv, "Trigger");
		configInput(nCv + 1, "Accent (gate = accented; 0-5 V = how much, read at each trigger)");
		if (E::EXTRA_TRIG) configInput(nCv + 2, "Open hat trigger (crash/ride: plays too)");
		configInput(randInput(), "Random trigger (as the ALL button)");
		configInput(nearInput(), "Near stock trigger (as the NEAR STOCK button)");
		configOutput(OUT_OUTPUT, s.name);
		lightDivider.setDivision(256);
		serial = random::u32();
		engine.age.setSerial(serial);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); engine.reset(); }

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "serial", json_integer(serial));
		if (E::USES_SAMPLE && !samplePath.empty()) json_object_set_new(root, "sample", json_string(samplePath.c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "serial");
		if (s) { serial = (uint32_t)json_integer_value(s); engine.age.setSerial(serial); }
		json_t* p = json_object_get(root, "sample");
		if (E::USES_SAMPLE && p && json_string_value(p)) {
			std::string path = json_string_value(p);
			if (!loadSample(path)) samplePath = path;   // keep the path, so a missing file is not forgotten
		}
	}

	// a WAV file (PCM 8/16/24/32-bit or 32/64-bit float; channels mixed), converted here,
	// off the audio thread, to the engine's clock and handed over by swapping
	bool loadSample(const std::string& path) {
		std::vector<float> data;
		float sr = 44100.f;
		if (!readWav(path, data, sr)) {
			sampleStatus = system::exists(path) ? "could not read " : "missing: ";
			sampleStatus += system::getFilename(path);
			return false;
		}
		if (data.size() > (size_t)(sr * 20.f)) data.resize((size_t)(sr * 20.f));
		std::vector<float> conv = E::prepareSample(data, sr);
		if (conv.empty()) { sampleStatus = "could not read " + system::getFilename(path); return false; }
		handOver(conv);
		samplePath = path;
		sampleStatus.clear();
		return true;
	}
	void clearSample() { std::vector<float> none; handOver(none); samplePath.clear(); sampleStatus.clear(); }
	void handOver(std::vector<float>& d) {
		std::lock_guard<std::mutex> lock(sampleMutex);
		newSample.swap(d);   // the old pending buffer (if any) is freed here, on this thread
		sampleReady = true;
	}

	static bool readWav(const std::string& path, std::vector<float>& out, float& sr) { return readWavFile(path, out, sr); }

	void process(const ProcessArgs& args) override {
		const VoiceSpec& s = E::spec();
		engine.setSampleRate(args.sampleRate);
		uint32_t ns = newSerial.exchange(0);
		if (ns) { serial = ns; engine.age.setSerial(ns); }
		if (E::USES_SAMPLE && sampleReady) {
			std::unique_lock<std::mutex> lock(sampleMutex, std::try_to_lock);
			if (lock.owns_lock()) { engine.swapSample(newSample); sampleReady = false; }   // swap, no copy
		}
		bool rnd = randBtn.process(params[randParam()].getValue() > 0.5f);
		bool near = nearBtn.process(params[nearParam()].getValue() > 0.5f);
		rnd |= randIn.process(inputs[randInput()].getVoltage(), 0.1f, 1.f);
		near |= nearIn.process(inputs[nearInput()].getVoltage(), 0.1f, 1.f);
		int key = keyRandom.exchange(0);
		rnd |= key == 1; near |= key == 2;
		if (rnd || near) randomise(near);
		for (int i = 0; i < nK; i++) k[i] = params[i].getValue();
		float voct = 0.f;
		for (int i = 0; i < nCv; i++) {
			if (!inputs[i].isConnected()) continue;
			int kn = s.cv[i].knob;
			if (kn < 0) { float v = inputs[i].getVoltage(); voct = std::isfinite(v) ? clamp(v, -10.f, 10.f) : 0.f; }
			else k[kn] = clamp(k[kn] + inputs[i].getVoltage() * 0.1f, 0.f, 1.f);
		}
		bool hit = trigIn.process(inputs[trigInput()].getVoltage(), 0.1f, 1.f);
		hit |= hitButton.process(params[hitParam()].getValue());
		bool hit2 = E::EXTRA_TRIG && trig2In.process(inputs[nCv + 2].getVoltage(), 0.1f, 1.f);
		if ((hit || hit2) && pending < 0) { pending = (int)(0.0005f * args.sampleRate); pendingAcc = 0.f; pendingWhich = 0; }
		// both triggers inside the window: the closed hat wins (it chokes the open one)
		if (pending >= 0) { if (hit) pendingWhich = 1; else if (hit2 && pendingWhich == 0) pendingWhich = 2; }
		if (pending >= 0) {
			float acc = inputs[accentInput()].isConnected() ? clamp(inputs[accentInput()].getVoltage() / 5.f, 0.f, 1.f) : 0.f;
			pendingAcc = std::max(pendingAcc, acc);
			if (pending-- == 0) {
				if (pendingWhich == 2) engine.hit2(pendingAcc, k.data());
				else engine.hit(pendingAcc, k.data());
			}
		}
		if (pending < 0 && engine.sleeping()) {
			engine.out = 0.f; engine.env = 0.f;
		}
		else engine.process(k.data(), voct);
		outputs[OUT_OUTPUT].setVoltage(engine.out);
		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[0].setBrightnessSmooth(engine.env * 0.1f, dt, 30.f);
		}
	}

	// the two random buttons: LEVEL, AGE (the knobs Rack's own randomise skips) and the
	// switches are left alone
	void randomise(bool nearStockOnly) {
		const VoiceSpec& s = E::spec();
		for (int i = 0; i < nK; i++) {
			const KnobDef& d = s.knobs[i];
			if (d.kind != 0 || d.norand) continue;
			float u1 = random::uniform(), u2 = random::uniform();
			params[i].setValue(nearStockOnly ? nearStock(d.stock, u1, u2) : u1);
		}
	}
	// every control back to the stock 909 (LEVEL and switches left alone)
	void resetToStock() {
		const VoiceSpec& s = E::spec();
		for (int i = 0; i < nK; i++) k[i] = params[i].getValue();
		for (int i = 0; i < nK; i++) {
			const KnobDef& d = s.knobs[i];
			if (d.kind != 0 || std::string(d.id) == "LEVEL") continue;
			params[i].setValue(E::stockFor(i, k.data()));
		}
	}
};

template <class E>
struct VoiceWidget : ModuleWidget {
	VoiceWidget(VoiceModule<E>* module) {
		setModule(module);
		const VoiceSpec& s = E::spec();
		setPanel(createPanel(asset::plugin(pluginInstance, std::string("res/") + s.slug + ".svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		for (int i = 0; i < s.nKnobs; i++) {
			const KnobDef& d = s.knobs[i];
			bool big = d.row < 2;
			Vec p = mm2px(Vec(big ? bigX(d.col) : smallX(d.col), ROW_Y[d.row]));
			if (d.kind == 1) addParam(createParamCentered<CKSSThree>(p, module, i));
			else if (d.kind == 2) addParam(createParamCentered<CKSS>(p, module, i));
			else if (big) addParam(createParamCentered<RoundBlackKnob>(p, module, i));
			else addParam(createParamCentered<RoundSmallBlackKnob>(p, module, i));
		}
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(bigX(2), ROW_Y[1])), module, s.nKnobs, 0));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(smallX(2), ROW_Y[s.randRow])), module, s.nKnobs + 1));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(smallX(3), ROW_Y[s.randRow])), module, s.nKnobs + 2));
		// one row of jacks: RAND, NEAR, TRIG, ACC, (OPEN), (V/OCT), OUT
		int ri = s.nCv + 2 + (E::EXTRA_TRIG ? 1 : 0);
		int n = 4 + (E::EXTRA_TRIG ? 1 : 0) + s.nCv + 1, j = 0;
		auto in = [&](int id) { addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX(j++, n), JACK_Y)), module, id)); };
		in(ri); in(ri + 1); in(s.nCv); in(s.nCv + 1);
		if (E::EXTRA_TRIG) in(s.nCv + 2);
		for (int i = 0; i < s.nCv; i++) in(i);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackX(j, n), JACK_Y)), module, VoiceModule<E>::OUT_OUTPUT));
	}

	// with the mouse over the module: R = random (ALL), N = near stock
	void onHoverKey(const HoverKeyEvent& e) override {
		VoiceModule<E>* m = dynamic_cast<VoiceModule<E>*>(this->module);
		if (m && e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == 0) {
			if (e.keyName == "r") { m->keyRandom = 1; e.consume(this); return; }
			if (e.keyName == "n") { m->keyRandom = 2; e.consume(this); return; }
		}
		ModuleWidget::onHoverKey(e);
	}
	void appendContextMenu(Menu* menu) override {
		VoiceModule<E>* m = dynamic_cast<VoiceModule<E>*>(this->module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Reset to stock 909", "", [=]() { m->resetToStock(); }));
		menu->addChild(createMenuItem("Random (every knob but LEVEL and AGE)", "R", [=]() { m->keyRandom = 1; }));
		menu->addChild(createMenuItem("Random near stock", "N", [=]() { m->keyRandom = 2; }));
		if (E::USES_SAMPLE) {
			menu->addChild(createMenuLabel(!m->sampleStatus.empty() ? "Source: built-in metal (" + m->sampleStatus + ")"
				: m->samplePath.empty() ? "Source: built-in metal" : "Source: " + system::getFilename(m->samplePath)));
			menu->addChild(createMenuItem("Load sample WAV (replaces the built-in source)", "", [=]() {
				char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, osdialog_filters_parse("WAV:wav,WAV"));
				if (path) { m->loadSample(path); std::free(path); }
			}));
			if (!m->samplePath.empty()) menu->addChild(createMenuItem("Back to the built-in source", "", [=]() { m->clearSample(); }));
		}
		menu->addChild(createMenuLabel(string::f("Serial number %u (sets this unit's AGE character)", (unsigned)m->serial)));
		menu->addChild(createMenuItem("New serial number (a different old unit)", "", [=]() {
			m->newSerial = random::u32();   // applied on the audio thread
		}));
	}
};

}  // namespace c909
