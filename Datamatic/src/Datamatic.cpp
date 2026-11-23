// Datamatic: numbers become rhythm (after Ryoji Ikeda). The engine is in
// DatamaticEngine.hpp; this file is knobs, jacks, the barcode screen and the
// right-click TEXT field.
#include "plugin.hpp"
#include "DatamaticEngine.hpp"
#include <atomic>
#include <mutex>

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float W = 101.6f;
static const float DISP_X = 6.0f, DISP_Y = 15.0f, DISP_W = 89.6f, DISP_H = 22.0f;
static const float KX[4] = {14.0f, 38.5f, 63.0f, 87.5f};
static const float ROW1_Y = 49.0f;
static const float ROW2_Y = 71.0f;
static const float JX0 = 9.3f, JDX = 16.6f;
static const float JA_Y = 88.0f, JB_Y = 103.0f, JC_Y = 118.0f;

struct Datamatic : Module {
	enum ParamIds {
		SOURCE_PARAM, WORD_PARAM, LENGTH_PARAM, RATE_PARAM,
		MUTATE_PARAM, ROLL_PARAM, CUT_PARAM, BPM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SOURCE_INPUT, WORD_INPUT, LENGTH_INPUT, RATE_INPUT, MUTATE_INPUT, ROLL_INPUT,
		CUT_INPUT, CLOCK_INPUT, RESET_INPUT, DATA_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SUB_OUTPUT, TONE_OUTPUT, CLICK_OUTPUT, NOISE_OUTPUT, DATA_OUTPUT, LINK_OUTPUT,
		CUT_OUTPUT, BIT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLOCK_LIGHT,
		NUM_LIGHTS
	};

	datamatic::Engine eng;
	datamatic::Settings st;
	dsp::SchmittTrigger clockTrig, resetTrig;
	float clockLight = 0.f;
	float laneFlash[4] = {0.f, 0.f, 0.f, 0.f};
	bool cutNow = false;

	// TEXT source: typed in the UI thread, picked up by the audio thread
	std::mutex textMutex;
	std::string pendingText = "datamatics";
	std::atomic<bool> textChanged{true};

	Datamatic() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(SOURCE_PARAM, 0.f, 7.f, 3.f, "Source",
		             {"Count", "Primes", "Fibonacci", "Pi", "e", "Text (right-click to type)", "Random (seed = WORD)", "Input (DATA jack)"});
		configParam(WORD_PARAM, 0.f, 255.f, 0.f, "Word (where in the data the loop starts)");
		getParamQuantity(WORD_PARAM)->snapEnabled = true;
		configParam(LENGTH_PARAM, 4.f, 64.f, 16.f, "Length", " bits");
		getParamQuantity(LENGTH_PARAM)->snapEnabled = true;
		configSwitch(RATE_PARAM, 0.f, 4.f, 1.f, "Rate (steps per clock)", {"1x", "2x", "4x", "8x", "16x"});
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate", "%", 0.f, 100.f);
		configParam(ROLL_PARAM, 0.f, 1.f, 0.f, "Roll", "%", 0.f, 100.f);
		configParam(CUT_PARAM, 0.f, 1.f, 0.f, "Cut", "%", 0.f, 100.f);
		configParam(BPM_PARAM, 40.f, 240.f, 130.f, "Internal tempo (when CLOCK is empty)", " BPM");
		configInput(SOURCE_INPUT, "Source CV (0-10 V across the 8 sources)");
		configInput(WORD_INPUT, "Word CV (10 V = +256)");
		configInput(LENGTH_INPUT, "Length CV (10 V = +60 bits)");
		configInput(RATE_INPUT, "Rate CV (2 V per step)");
		configInput(MUTATE_INPUT, "Mutate CV (10 V = full)");
		configInput(ROLL_INPUT, "Roll CV (10 V = full)");
		configInput(CUT_INPUT, "Cut CV (10 V = full)");
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(DATA_INPUT, "Data (0-10 V, read by the INPUT source)");
		configOutput(SUB_OUTPUT, "SUB gate");
		configOutput(TONE_OUTPUT, "TONE gate");
		configOutput(CLICK_OUTPUT, "CLICK trigger");
		configOutput(NOISE_OUTPUT, "NOISE trigger");
		configOutput(DATA_OUTPUT, "Data (byte under the playhead, 0-10 V)");
		configOutput(LINK_OUTPUT, "LINK to Test Pattern (6-channel poly)");
		configOutput(CUT_OUTPUT, "Cut gate (high during silence)");
		configOutput(BIT_OUTPUT, "Bit stream gate");
	}

	void onReset() override {
		std::lock_guard<std::mutex> lock(textMutex);
		pendingText = "datamatics";
		textChanged = true;
		eng.mask = 0;
	}

	void setText(const std::string& s) {
		std::lock_guard<std::mutex> lock(textMutex);
		pendingText = s;
		textChanged = true;
	}
	std::string getText() {
		std::lock_guard<std::mutex> lock(textMutex);
		return pendingText;
	}

	void process(const ProcessArgs& args) override {
		if (textChanged) {
			std::unique_lock<std::mutex> lock(textMutex, std::try_to_lock);
			if (lock.owns_lock()) {
				eng.text = pendingText;
				textChanged = false;
			}
		}
		auto cv = [&](int in) { return inputs[in].getVoltage(); };
		st.source = clamp((int) std::floor(params[SOURCE_PARAM].getValue() + cv(SOURCE_INPUT) * 0.8f + 0.5f), 0, 7);
		st.word = clamp((int) std::round(params[WORD_PARAM].getValue() + cv(WORD_INPUT) * 25.6f), 0, 255);
		st.length = clamp((int) std::round(params[LENGTH_PARAM].getValue() + cv(LENGTH_INPUT) * 6.f), 4, 64);
		st.rateIndex = clamp((int) std::round(params[RATE_PARAM].getValue() + cv(RATE_INPUT) * 0.5f), 0, 4);
		st.mutate = clamp(params[MUTATE_PARAM].getValue() + cv(MUTATE_INPUT) * 0.1f, 0.f, 1.f);
		st.roll = clamp(params[ROLL_PARAM].getValue() + cv(ROLL_INPUT) * 0.1f, 0.f, 1.f);
		st.cut = clamp(params[CUT_PARAM].getValue() + cv(CUT_INPUT) * 0.1f, 0.f, 1.f);
		st.bpm = params[BPM_PARAM].getValue();

		bool ext = inputs[CLOCK_INPUT].isConnected();
		bool clk = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		bool rst = resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		datamatic::Outputs o = eng.process(args.sampleTime, clk, rst, ext, cv(DATA_INPUT), st);

		const int lanes[4] = {SUB_OUTPUT, TONE_OUTPUT, CLICK_OUTPUT, NOISE_OUTPUT};
		for (int v = 0; v < 4; v++) {
			outputs[lanes[v]].setVoltage(o.gate[v] ? 10.f : 0.f);
			if (o.gate[v]) laneFlash[v] = 1.f;
			else laneFlash[v] = std::max(0.f, laneFlash[v] - args.sampleTime * 12.f);
		}
		outputs[DATA_OUTPUT].setVoltage(o.data);
		outputs[CUT_OUTPUT].setVoltage(o.cut ? 10.f : 0.f);
		outputs[BIT_OUTPUT].setVoltage(o.bit ? 10.f : 0.f);
		outputs[LINK_OUTPUT].setChannels(LINK_CHANNELS);
		for (int v = 0; v < 4; v++) outputs[LINK_OUTPUT].setVoltage(o.gate[v] ? 10.f : 0.f, v);
		outputs[LINK_OUTPUT].setVoltage(o.data, 4);
		outputs[LINK_OUTPUT].setVoltage(o.cut ? 10.f : 0.f, 5);

		if (o.clockTick) clockLight = 1.f;
		clockLight = std::max(0.f, clockLight - args.sampleTime * 10.f);
		lights[CLOCK_LIGHT].setBrightness(clockLight);
		cutNow = o.cut;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "text", json_string(getText().c_str()));
		json_object_set_new(root, "mask", json_string(std::to_string((unsigned long long) eng.mask).c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* t = json_object_get(root, "text");
		if (t) setText(json_string_value(t));
		json_t* m = json_object_get(root, "mask");
		if (m) {
			// keep the mutations: rebuild first so the next process() doesn't wipe them
			eng.text = getText();
			textChanged = false;
			int src = clamp((int) params[SOURCE_PARAM].getValue(), 0, 7);
			int word = clamp((int) params[WORD_PARAM].getValue(), 0, 255);
			eng.rebuild(src, word);
			eng.mask = std::strtoull(json_string_value(m), nullptr, 10);
		}
	}
};

// The screen: one row of bars per lane in the order that lane reads the loop,
// the playhead, and the source / word / length / rate line. Black and white.
struct DatamaticDisplay : Widget {
	Datamatic* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		float w = box.size.x, h = box.size.y;
		datamatic::Engine demo;
		const datamatic::Engine* e = &demo;
		int src = 3, word = 0, L = 16, rate = 2;
		bool cut = false;
		const float* flash = nullptr;
		if (module) {
			e = &module->eng;
			src = module->st.source; word = module->st.word; L = e->length; rate = e->rate;
			cut = module->cutNow;
			flash = module->laneFlash;
		}
		else {
			demo.rebuild(datamatic::SRC_PI, 0);
			demo.length = 16;
			demo.subPos = 5; demo.tonePos = 5; demo.fullPos = 11;
		}

		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		float pad = mm2px(1.2f);
		float textH = mm2px(4.2f);
		if (font) {
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, mm2px(3.1f));
			nvgFillColor(vg, nvgRGB(0xF2, 0xF2, 0xF2));
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			std::string s = string::f("%s  W%03d  L%02d  x%d", datamatic::SOURCE_NAMES[src], word, L, rate);
			nvgText(vg, pad, pad + textH * 0.45f, s.c_str(), nullptr);
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			if (cut) {
				nvgFillColor(vg, nvgRGB(0x0, 0x0, 0x0));
				nvgBeginPath(vg);
				nvgRect(vg, w - pad - mm2px(8.f), pad, mm2px(8.f), textH * 0.9f);
				nvgFillColor(vg, nvgRGB(0xF2, 0xF2, 0xF2));
				nvgFill(vg);
				nvgFillColor(vg, nvgRGB(0x0, 0x0, 0x0));
				nvgText(vg, w - pad - mm2px(1.f), pad + textH * 0.45f, "CUT", nullptr);
			}
			else {
				uint8_t b = (uint8_t) std::round(e->dataOut * 25.5f);
				std::string bits;
				for (int k = 7; k >= 0; k--) bits += ((b >> k) & 1) ? '1' : '0';
				nvgFillColor(vg, nvgRGB(0x9A, 0x9A, 0x9A));
				nvgText(vg, w - pad, pad + textH * 0.45f, bits.c_str(), nullptr);
			}
		}

		float top = pad + textH;
		float rowsH = h - top - pad;
		float rowH = rowsH / 4.f;
		float cellW = (w - 2 * pad) / L;
		int pos[4] = {e->subPos, e->tonePos, e->fullPos, e->fullPos};
		for (int v = 0; v < 4; v++) {
			float y = top + v * rowH;
			float fl = flash ? flash[v] : 0.f;
			for (int j = 0; j < L; j++) {
				bool b = false;
				switch (v) {
					case 0: b = e->subBit(j); break;
					case 1: b = e->toneBit(j); break;
					case 2: b = e->clickBit(j); break;
					default: b = e->noiseBit(j); break;
				}
				bool here = (j == pos[v]);
				if (!b && !here) continue;
				nvgBeginPath(vg);
				float x = pad + j * cellW;
				nvgRect(vg, x + (cellW > 4.f ? 0.5f : 0.f), y + 0.8f, std::max(1.f, cellW - (cellW > 4.f ? 1.f : 0.f)), rowH - 1.6f);
				int c = b ? (here ? 255 : (int) (150 + 60 * (1.f - fl))) : 70;
				if (cut) c = c / 3;
				nvgFillColor(vg, nvgRGB(c, c, c));
				nvgFill(vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct DataTextField : ui::TextField {
	Datamatic* module;
	void onChange(const ChangeEvent& e) override {
		module->setText(text);
	}
};

struct DatamaticWidget : ModuleWidget {
	DatamaticWidget(Datamatic* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Datamatic.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		DatamaticDisplay* d = createWidget<DatamaticDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		const int row1[4] = {Datamatic::SOURCE_PARAM, Datamatic::WORD_PARAM, Datamatic::LENGTH_PARAM, Datamatic::RATE_PARAM};
		const int row2[4] = {Datamatic::MUTATE_PARAM, Datamatic::ROLL_PARAM, Datamatic::CUT_PARAM, Datamatic::BPM_PARAM};
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(KX[i], ROW1_Y)), module, row1[i]));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(KX[i], ROW2_Y)), module, row2[i]));
		}
		addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(KX[3] + 7.5f, ROW2_Y - 8.6f)), module, Datamatic::CLOCK_LIGHT));

		const int ja[6] = {Datamatic::SOURCE_INPUT, Datamatic::WORD_INPUT, Datamatic::LENGTH_INPUT, Datamatic::RATE_INPUT, Datamatic::MUTATE_INPUT, Datamatic::ROLL_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JA_Y)), module, ja[i]));
		const int jb[4] = {Datamatic::CUT_INPUT, Datamatic::CLOCK_INPUT, Datamatic::RESET_INPUT, Datamatic::DATA_INPUT};
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JB_Y)), module, jb[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * 4, JB_Y)), module, Datamatic::CUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * 5, JB_Y)), module, Datamatic::BIT_OUTPUT));
		const int jc[6] = {Datamatic::SUB_OUTPUT, Datamatic::TONE_OUTPUT, Datamatic::CLICK_OUTPUT, Datamatic::NOISE_OUTPUT, Datamatic::DATA_OUTPUT, Datamatic::LINK_OUTPUT};
		for (int i = 0; i < 6; i++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JC_Y)), module, jc[i]));
	}

	void appendContextMenu(Menu* menu) override {
		Datamatic* module = dynamic_cast<Datamatic*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Text for the TEXT source (each letter = 8 bits)"));
		DataTextField* tf = new DataTextField;
		tf->module = module;
		tf->box.size.x = 220.f;
		tf->text = module->getText();
		menu->addChild(tf);
		menu->addChild(createMenuItem("Clear mutations", "", [=]() { module->eng.mask = 0; }));
	}
};

Model* modelDatamatic = createModel<Datamatic, DatamaticWidget>("Datamatic");
