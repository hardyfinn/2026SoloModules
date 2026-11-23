#include "plugin.hpp"
#include "dsp/Sprecher.hpp"
#include <atomic>

using namespace wbui;
namespace L = layout::Sprecher;

static const std::vector<std::string> LANGS = {"English", "German", "French", "Spanish", "Italian", "Japanese", "Russian"};

// A parsed text: words with their frames at RATE 1. Built on the UI thread, read by the audio thread.
struct SpList {
	std::vector<std::string> labels, phs;
	std::vector<std::vector<wb::SpFrame>> frames;
	size_t size() const { return frames.size(); }
};

static SpList* buildList(const std::string& text, int lang) {
	SpList* L = new SpList;
	auto words = wb::SpText::get().parse(text, lang);
	for (auto& w : words) {
		auto f = wb::spWordTargets(w.ph, 1.0);
		if (f.empty()) continue;
		L->labels.push_back(w.label);
		L->phs.push_back(wb::SpText::phString(w.ph));
		L->frames.push_back(std::move(f));
	}
	return L;
}

struct Sprecher : Module {
	enum ParamIds { LANG_PARAM, ENGINE_PARAM, HOLD_PARAM, PITCH_PARAM, INFLECT_PARAM, RATE_PARAM, SIZE_PARAM, WHISPER_PARAM, AGE_PARAM, LEVEL_PARAM, SPEAK_PARAM, RESET_PARAM, NUM_PARAMS };
	enum InputIds { STEP_INPUT, RESET_INPUT, WORD_INPUT, VOCT_INPUT, RATE_INPUT, SIZE_INPUT, NUM_INPUTS };
	enum OutputIds { OUT_OUTPUT, F1_OUTPUT, BUSY_OUTPUT, END_OUTPUT, NUM_OUTPUTS };
	enum LightIds { BUSY_LIGHT, NUM_LIGHTS };

	// text: UI thread only (and JSON)
	std::string text = "1 2 3 4 5 6 7 8";
	int parsedLang = -1;
	bool textDirty = true;

	// list handover
	std::atomic<SpList*> incoming{nullptr};
	std::atomic<SpList*> current{nullptr};
	std::atomic<SpList*> retired{nullptr};
	std::atomic<int> speaking{-1};

	wb::SpScheduler sched;
	wb::SpFormant formant;
	wb::SpChip chip;
	wb::Schmitt stepTrig, resetTrig;
	dsp::BooleanTrigger speakBtn, resetBtn;
	dsp::PulseGenerator endPulse;
	Smooth sm[6];
	double semisSm = 0;
	int nextWord = 0;
	uint64_t seed = 1;
	bool needInit = true;
	double rate = 0;

	Sprecher() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(LANG_PARAM, 0.f, 6.f, 0.f, "Language", LANGS)->description =
			"Chooses the number words, the dictionary tried first and the letter-to-sound rules for unknown words. French and Russian use the English rules for unknown words.";
		configSwitch(ENGINE_PARAM, 0.f, 1.f, 1.f, "Engine", {"Chip (speech chip at 8 kHz)", "Formant (analog formant synthesiser)"});
		configSwitch(HOLD_PARAM, 0.f, 1.f, 1.f, "Hold", {"Off", "On: a held STEP gate sustains the stressed vowel"});
		configParam(PITCH_PARAM, 0.f, 1.f, 0.3333f, "Pitch", " Hz", 8.f, 55.f);
		configParam(INFLECT_PARAM, 0.f, 1.f, 0.3f, "Inflection", "%", 0.f, 100.f)->description = "Speech-like pitch movement when V/OCT is not patched.";
		configParam(RATE_PARAM, -1.f, 1.f, 0.f, "Rate", "x", 2.f);
		struct SizeQ : ParamQuantity {
			std::string getDisplayValueString() override {
				double v = getValue();
				const char* w = v < 0.8 ? "small" : v < 0.95 ? "smaller" : v <= 1.05 ? "adult" : v < 1.25 ? "large" : "giant";
				return string::f("%.2f (%s)", v, w);
			}
		};
		configParam<SizeQ>(SIZE_PARAM, 0.7f, 1.4f, 1.f, "Size (vocal tract length)");
		configParam(WHISPER_PARAM, 0.f, 1.f, 0.f, "Whisper", "%", 0.f, 100.f);
		configParam(AGE_PARAM, 0.f, 1.f, 0.3f, "Age", "%", 0.f, 100.f)->description =
			"From factory-fresh to tired: clock error and drift, formant offsets. Above 60% a little hiss.";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configButton(SPEAK_PARAM, "Speak (same as a STEP trigger; hold for HOLD)");
		configButton(RESET_PARAM, "Reset to the first word");
		configInput(STEP_INPUT, "Step: speak the next word (hold the gate to sustain with HOLD)");
		configInput(RESET_INPUT, "Reset: the next STEP speaks the first word");
		configInput(WORD_INPUT, "Word select (0 to 10 V across the list)");
		configInput(VOCT_INPUT, "V/OCT (sing: pitch follows from PITCH)");
		configInput(RATE_INPUT, "Rate CV (5 V = double)");
		configInput(SIZE_INPUT, "Size CV (0.07 per volt)");
		configOutput(OUT_OUTPUT, "Audio");
		configOutput(F1_OUTPUT, "First formant (0 V = 200 Hz, 80 Hz per volt)");
		configOutput(BUSY_OUTPUT, "Busy (10 V while a word sounds)");
		configOutput(END_OUTPUT, "End of word trigger");
		configLight(BUSY_LIGHT, "Busy");
		seed = random::u64();
		sched.reserve(4096);
		reparse();
	}
	~Sprecher() {
		delete incoming.exchange(nullptr);
		delete current.exchange(nullptr);
		delete retired.exchange(nullptr);
	}

	// UI thread: parse the text and hand it over
	void reparse() {
		parsedLang = (int)std::round(params[LANG_PARAM].getValue());
		SpList* nl = buildList(text, parsedLang);
		SpList* old = incoming.exchange(nl);
		delete old;    // never picked up
		textDirty = false;
	}
	void collect() { delete retired.exchange(nullptr); }

	void onReset() override { text = "1 2 3 4 5 6 7 8"; textDirty = true; needInit = true; }
	void onSampleRateChange(const SampleRateChangeEvent&) override { needInit = true; }

	void startWord(const SpList* Lst, int i) {
		if (!Lst || Lst->size() == 0) return;
		i = clamp(i, 0, (int)Lst->size() - 1);
		sched.startCopy(Lst->frames[i]);
		speaking.store(i);
	}

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) {
			rate = args.sampleRate;
			sched.init(rate);
			formant.init(rate, seed, (uint32_t)(seed >> 33) | 1u);
			chip.init(rate, seed, (uint32_t)(seed >> 33) | 1u);
			needInit = false;
		}
		// take a new word list when the previous one has been collected
		if (!retired.load()) {
			SpList* nl = incoming.exchange(nullptr);
			if (nl) {
				SpList* old = current.exchange(nl);
				retired.store(old);
				if (nextWord >= (int)nl->size()) nextWord = 0;
			}
		}
		const SpList* Lst = current.load();
		const int count = Lst ? (int)Lst->size() : 0;

		const double a = smoothCoef(args.sampleTime);
		const double pitchHz = 55.0 * std::pow(8.0, sm[0].process(params[PITCH_PARAM].getValue(), a));
		const double inflect = sm[1].process(params[INFLECT_PARAM].getValue(), a);
		const double rateK = sm[2].process(params[RATE_PARAM].getValue(), a);
		wb::SpVoiceParams vp;
		vp.size = wb::clampd(sm[3].process(params[SIZE_PARAM].getValue(), a) + 0.07 * inputs[SIZE_INPUT].getVoltage(), 0.6, 1.6);
		vp.whisper = sm[4].process(params[WHISPER_PARAM].getValue(), a);
		vp.age = params[AGE_PARAM].getValue();
		const double level = sm[5].process(params[LEVEL_PARAM].getValue(), a);
		const double speed = wb::clampd(std::exp2(rateK + inputs[RATE_INPUT].getVoltage() / 5.0), 0.25, 4.0);
		const bool hold = params[HOLD_PARAM].getValue() > 0.5f;
		const bool chipEngine = params[ENGINE_PARAM].getValue() < 0.5f;

		// RESET
		bool rs = resetTrig.process(inputs[RESET_INPUT].getVoltage());
		if (resetBtn.process(params[RESET_PARAM].getValue() > 0.f)) rs = true;
		if (rs) nextWord = 0;
		// STEP
		bool st = stepTrig.process(inputs[STEP_INPUT].getVoltage());
		if (speakBtn.process(params[SPEAK_PARAM].getValue() > 0.f)) st = true;
		const bool gateHigh = stepTrig.high || params[SPEAK_PARAM].getValue() > 0.f;
		if (st && count > 0) {
			if (inputs[WORD_INPUT].isConnected()) {
				int i = (int)std::floor(inputs[WORD_INPUT].getVoltage() / 10.f * count);
				startWord(Lst, clamp(i, 0, count - 1));
			} else {
				if (nextWord >= count) nextWord = 0;
				startWord(Lst, nextWord);
				nextWord = (nextWord + 1) % count;
			}
		}

		sched.tickRate(gateHigh, hold, speed);
		const wb::SpFrame& row = sched.row;

		double f0;
		if (inputs[VOCT_INPUT].isConnected()) f0 = pitchHz * std::exp2(wb::clampd(inputs[VOCT_INPUT].getVoltage(), -5.0, 5.0));
		else {
			double semis = inflect * (3.0 * row[11] - 4.0 * sched.progress() + 1.0);
			semisSm += (1.0 - std::exp(-args.sampleTime / 0.040)) * (semis - semisSm);
			f0 = pitchHz * std::exp2(semisSm / 12.0);
		}
		f0 = wb::clampd(f0, 20.0, 2000.0);
		double y, f1;
		if (chipEngine) { y = chip.process(row, f0, vp); f1 = chip.f1out; }
		else { y = formant.process(row, f0, vp); f1 = formant.f1out; }

		if (sched.finished) { endPulse.trigger(1e-3f); speaking.store(-1); }
		const bool busy = sched.busy();
		outputs[OUT_OUTPUT].setVoltage(outClamp(y * 5.0 * level));
		outputs[F1_OUTPUT].setVoltage((float)wb::clampd((f1 - 200.0) / 80.0, 0.0, 10.0));
		outputs[BUSY_OUTPUT].setVoltage(busy ? 10.f : 0.f);
		outputs[END_OUTPUT].setVoltage(endPulse.process(args.sampleTime) ? 10.f : 0.f);
		lights[BUSY_LIGHT].setBrightnessSmooth(busy ? 1.f : 0.f, args.sampleTime);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", seedToJson(seed));
		json_object_set_new(root, "text", json_string(text.c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		seedFromJson(root, seed);
		json_t* t = json_object_get(root, "text");
		if (t && json_is_string(t)) text = json_string_value(t);
		collect();
		reparse();
		needInit = true;
	}
};

struct SpTextField : LedDisplayTextField {
	Sprecher* module = nullptr;
	SpTextField() {
		multiline = true;
		color = nvgRGB(0xEF, 0xE9, 0xDD);
		bgColor = nvgRGBA(0, 0, 0, 0);
		textOffset = Vec(2, 2);
		placeholder = "Type words, numbers or [PH O N EH M Z]";
	}
	void onChange(const ChangeEvent& e) override {
		if (module) { module->text = getText(); module->textDirty = true; }
	}
};

struct WordDisplay : Widget {
	Sprecher* module = nullptr;
	std::string shown;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
			if (font && font->handle >= 0) {
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, 11.f);
				nvgFillColor(args.vg, nvgRGB(0xD6, 0x40, 0x2F));
				nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgText(args.vg, 4.f, box.size.y / 2.f, shown.c_str(), nullptr);
			}
		}
		Widget::drawLayer(args, layer);
	}
	void step() override {
		Widget::step();
		if (!module) { shown = "three  TH R 'IY"; return; }
		const SpList* Lst = module->current.load();
		int i = module->speaking.load();
		if (Lst && i >= 0 && i < (int)Lst->size()) shown = Lst->labels[i] + "   " + Lst->phs[i];
		else if (Lst) shown = string::f("%d words", (int)Lst->size());
		else shown = "";
	}
};

struct SprecherWidget : ModuleWidget {
	SpTextField* field = nullptr;
	SprecherWidget(Sprecher* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Sprecher.svg")));
		addScrews(this);
		field = createWidget<SpTextField>(mm(L::TEXT));
		field->box.size = mm2px(Vec(L::TEXT_SIZE.x, L::TEXT_SIZE.y));
		field->module = module;
		if (module) field->setText(module->text);
		else field->setText("we are the robots 1 2 3 4");
		addChild(field);
		WordDisplay* wd = createWidget<WordDisplay>(mm(L::WORD));
		wd->box.size = mm2px(Vec(L::WORD_SIZE.x, L::WORD_SIZE.y));
		wd->module = module;
		addChild(wd);

		auto* lang = createParamCentered<RoundBlackKnob>(mm(L::LANG), module, Sprecher::LANG_PARAM);
		lang->snap = true;
		addParam(lang);
		addParam(createParamCentered<CKSS>(mm(L::ENGINE), module, Sprecher::ENGINE_PARAM));
		addParam(createParamCentered<CKSS>(mm(L::HOLD), module, Sprecher::HOLD_PARAM));
		const layout::P kp[] = {L::PITCH, L::INFLECT, L::RATE, L::SIZE, L::WHISPER, L::AGE, L::LEVEL};
		for (int i = 0; i < 7; i++) addParam(createParamCentered<RoundBlackKnob>(mm(kp[i]), module, Sprecher::PITCH_PARAM + i));
		addParam(createParamCentered<VCVBezel>(mm(L::SPEAK), module, Sprecher::SPEAK_PARAM));
		addParam(createParamCentered<VCVBezel>(mm(L::RESET), module, Sprecher::RESET_PARAM));
		addChild(createLightCentered<MediumLight<OrangeLight>>(mm(L::BUSY_LIGHT), module, Sprecher::BUSY_LIGHT));
		const layout::P ip[] = {L::STEP_IN, L::RESET_IN, L::WORD_IN, L::VOCT_IN, L::RATE_IN, L::SIZE_IN};
		for (int i = 0; i < 6; i++) addInput(createInputCentered<PJ301MPort>(mm(ip[i]), module, Sprecher::STEP_INPUT + i));
		const layout::P op[] = {L::OUT, L::F1_OUT, L::BUSY_OUT, L::END_OUT};
		for (int i = 0; i < 4; i++) addOutput(createOutputCentered<PJ301MPort>(mm(op[i]), module, Sprecher::OUT_OUTPUT + i));
	}
	void step() override {
		ModuleWidget::step();
		Sprecher* m = dynamic_cast<Sprecher*>(module);
		if (!m) return;
		m->collect();
		// a preset or undo can change the text underneath the field
		if (field && APP->event->selectedWidget != field && field->getText() != m->text) field->setText(m->text);
		if (m->textDirty || (int)std::round(m->params[Sprecher::LANG_PARAM].getValue()) != m->parsedLang) m->reparse();
	}
	void appendContextMenu(Menu* menu) override {
		Sprecher* m = dynamic_cast<Sprecher*>(module);
		if (!m) return;
		appendUnitMenu(menu, m->seed, [=]() { m->seed = random::u64(); m->needInit = true; });
	}
};

Model* modelSprecher = createModel<Sprecher, SprecherWidget>("Sprecher");
