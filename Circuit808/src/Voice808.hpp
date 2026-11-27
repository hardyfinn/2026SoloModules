// One module template for the 808 Circuits voices: params, jacks and panel positions
// all come from the voice's knob table (res/voices.py -> src/VoiceSpecs.hpp).
#pragma once
#include "plugin.hpp"
#include "VoiceEngines.hpp"

namespace c808 {

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
	uint32_t serial = 808;
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
		config(nK + 1, nCv + 2, NUM_OUTPUTS, 1);
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
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "serial");
		if (s) { serial = (uint32_t)json_integer_value(s); engine.age.setSerial(serial); }
	}

	void process(const ProcessArgs& args) override {
		const VoiceSpec& s = E::spec();
		engine.setSampleRate(args.sampleRate);
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
		if (hit && pending < 0) { pending = (int)(0.0005f * args.sampleRate); pendingAcc = 0.f; }
		if (pending >= 0) {
			float acc = inputs[accentInput()].isConnected() ? clamp(inputs[accentInput()].getVoltage() / 5.f, 0.f, 1.f) : 0.f;
			pendingAcc = std::max(pendingAcc, acc);
			if (pending-- == 0) engine.hit(pendingAcc, k.data());
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

	// every control back to the stock 808 (LEVEL and switches left alone)
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
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(VCOL[5], VROW_IO)), module, VoiceModule<E>::ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(VCOL[6], VROW_IO)), module, VoiceModule<E>::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		VoiceModule<E>* m = dynamic_cast<VoiceModule<E>*>(this->module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Reset to stock 808", "", [=]() { m->resetToStock(); }));
		menu->addChild(createMenuLabel(string::f("Serial number %u (sets this unit's AGE character)", (unsigned)m->serial)));
		menu->addChild(createMenuItem("New serial number (a different old unit)", "", [=]() {
			m->serial = random::u32();
			m->engine.age.setSerial(m->serial);
		}));
	}
};

}  // namespace c808
