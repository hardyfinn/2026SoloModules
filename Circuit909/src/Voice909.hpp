// One module template for the 909 Circuits voices: params, jacks and panel positions
// all come from the voice's knob table (res/voices.py -> src/VoiceSpecs.hpp).
#pragma once
#include "plugin.hpp"
#include "Engines909.hpp"
#include <osdialog.h>
#include <cstdio>
#include <atomic>
#include <mutex>

namespace c909 {

// ---- LAYOUT (same numbers in res/make_voice_panels.py) ----
static const float VCOL[7] = {11.f, 31.04f, 51.08f, 71.12f, 91.16f, 111.2f, 131.24f};
static const float VROW[3] = {30.f, 52.f, 74.f};
static const float VROW_CV = 96.f, VROW_IO = 115.f;

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
	std::vector<float> newSample;
	float newRate = 44100.f;
	std::atomic<bool> sampleReady{false};
	int nK = 0, nCv = 0;
	// accent is read over 0.5 ms after a trigger (an accent through another module may
	// arrive a sample or two late); every voice waits the same, so the kit stays aligned
	int pending = -1;
	float pendingAcc = 0.f;

	int hitParam() const { return nK; }
	int trigInput() const { return nCv; }
	int accentInput() const { return nCv + 1; }
	enum OutputIds { ENV_OUTPUT, OUT_OUTPUT, NUM_OUTPUTS };

	VoiceModule() {
		const VoiceSpec& s = E::spec();
		nK = s.nKnobs; nCv = s.nCv;
		k.assign(nK, 0.f);
		config(nK + 1, nCv + 2 + (E::EXTRA_TRIG ? 1 : 0), NUM_OUTPUTS, 1);
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
		for (int i = 0; i < nCv; i++) {
			const CvDef& c = s.cv[i];
			if (c.knob < 0) configInput(i, "V/OCT");
			else configInput(i, std::string(s.knobs[c.knob].tip) + " CV (+-5 V = full range)");
		}
		configInput(nCv, "Trigger");
		configInput(nCv + 1, "Accent (gate = accented; 0-5 V = how much, read at each trigger)");
		if (E::EXTRA_TRIG) configInput(nCv + 2, "Open hat trigger (crash/ride: plays too)");
		configOutput(ENV_OUTPUT, "Envelope (0-10 V)");
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
		if (E::USES_SAMPLE && p && json_string_value(p)) loadSample(json_string_value(p));
	}

	// a mono float copy of a WAV file (PCM 8/16/24/32-bit or 32-bit float; channels mixed)
	bool loadSample(const std::string& path) {
		std::vector<float> data;
		float sr = 44100.f;
		if (!readWav(path, data, sr)) return false;
		if (data.size() > (size_t)(sr * 20.f)) data.resize((size_t)(sr * 20.f));
		handOver(data, sr);
		samplePath = path;
		return true;
	}
	void clearSample() { handOver(std::vector<float>(), 44100.f); samplePath.clear(); }
	void handOver(const std::vector<float>& d, float sr) {
		std::lock_guard<std::mutex> lock(sampleMutex);
		newSample = d; newRate = sr;
		sampleReady = true;
	}

	static bool readWav(const std::string& path, std::vector<float>& out, float& sr) {
		FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) return false;
		std::vector<uint8_t> b;
		uint8_t buf[65536];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && b.size() < (size_t)200e6) b.insert(b.end(), buf, buf + n);
		std::fclose(f);
		auto u16 = [&](size_t i) { return (uint32_t)b[i] | ((uint32_t)b[i + 1] << 8); };
		auto u32 = [&](size_t i) { return u16(i) | (u16(i + 2) << 16); };
		if (b.size() < 12 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) return false;
		int fmt = 0, ch = 0, bits = 0;
		size_t p = 12;
		while (p + 8 <= b.size()) {
			uint32_t len = u32(p + 4);
			size_t body = p + 8;
			if (!std::memcmp(&b[p], "fmt ", 4) && body + 16 <= b.size()) {
				fmt = (int)u16(body); ch = (int)u16(body + 2); sr = (float)u32(body + 4); bits = (int)u16(body + 14);
				if (fmt == 0xFFFE && body + 26 <= b.size()) fmt = (int)u16(body + 24);
			}
			else if (!std::memcmp(&b[p], "data", 4) && ch > 0 && bits > 0) {
				size_t bytes = std::min((size_t)len, b.size() - body);
				int bps = bits / 8;
				size_t frames = bytes / (bps * ch);
				out.assign(frames, 0.f);
				for (size_t i = 0; i < frames; i++) {
					float acc = 0.f;
					for (int c = 0; c < ch; c++) {
						size_t q = body + (i * ch + c) * bps;
						float v = 0.f;
						if (fmt == 3 && bits == 32) { float x; std::memcpy(&x, &b[q], 4); v = x; }
						else if (bits == 8) v = (b[q] - 128) / 128.f;
						else if (bits == 16) v = (int16_t)u16(q) / 32768.f;
						else if (bits == 24) v = (int32_t)((b[q] << 8) | (b[q + 1] << 16) | ((uint32_t)b[q + 2] << 24)) / 2147483648.f;
						else if (bits == 32) v = (int32_t)u32(q) / 2147483648.f;
						acc += std::isfinite(v) ? v : 0.f;
					}
					out[i] = acc / ch;
				}
				return frames > 0 && sr > 1000.f;
			}
			p = body + len + (len & 1);
		}
		return false;
	}

	void process(const ProcessArgs& args) override {
		const VoiceSpec& s = E::spec();
		engine.setSampleRate(args.sampleRate);
		if (E::USES_SAMPLE && sampleReady) {
			std::unique_lock<std::mutex> lock(sampleMutex, std::try_to_lock);
			if (lock.owns_lock()) { engine.setSample(newSample, newRate); sampleReady = false; }
		}
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
		if ((hit || hit2) && pending < 0) { pending = (int)(0.0005f * args.sampleRate); pendingAcc = 0.f; pendingWhich = hit2 && !hit ? 2 : 1; }
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
		outputs[ENV_OUTPUT].setVoltage(engine.env);
		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[0].setBrightnessSmooth(engine.env * 0.1f, dt, 30.f);
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
			Vec p = mm2px(Vec(VCOL[d.col], VROW[d.row]));
			if (d.kind == 1) addParam(createParamCentered<CKSSThree>(p, module, i));
			else if (d.kind == 2) addParam(createParamCentered<CKSS>(p, module, i));
			else addParam(createParamCentered<RoundBlackKnob>(p, module, i));
		}
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(VCOL[6], VROW[0])), module, s.nKnobs, 0));
		for (int i = 0; i < s.nCv; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(VCOL[i], VROW_CV)), module, i));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(VCOL[0], VROW_IO)), module, s.nCv));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(VCOL[1], VROW_IO)), module, s.nCv + 1));
		if (E::EXTRA_TRIG) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(VCOL[2], VROW_IO)), module, s.nCv + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(VCOL[5], VROW_IO)), module, VoiceModule<E>::ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(VCOL[6], VROW_IO)), module, VoiceModule<E>::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		VoiceModule<E>* m = dynamic_cast<VoiceModule<E>*>(this->module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Reset to stock 909", "", [=]() { m->resetToStock(); }));
		if (E::USES_SAMPLE) {
			menu->addChild(createMenuLabel(m->samplePath.empty() ? "Source: built-in metal" : "Source: " + system::getFilename(m->samplePath)));
			menu->addChild(createMenuItem("Load sample WAV (replaces the built-in source)", "", [=]() {
				char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, osdialog_filters_parse("WAV:wav,WAV"));
				if (path) { m->loadSample(path); std::free(path); }
			}));
			if (!m->samplePath.empty()) menu->addChild(createMenuItem("Back to the built-in source", "", [=]() { m->clearSample(); }));
		}
		menu->addChild(createMenuLabel(string::f("Serial number %u (sets this unit's AGE character)", (unsigned)m->serial)));
		menu->addChild(createMenuItem("New serial number (a different old unit)", "", [=]() {
			m->serial = random::u32();
			m->engine.age.setSerial(m->serial);
		}));
	}
};

}  // namespace c909
