// Lightning: a 4-slot hit catcher for Thunder. Sits to the right of Thunder (or takes any audio
// on IN), keeps whole hits with one button, already trimmed, and can send a kept sound's knob
// settings back to Thunder.
#include "plugin.hpp"
#include "LightningCore.hpp"
#include "Wav.hpp"
#include <osdialog.h>
#include <atomic>

// ---- LAYOUT (same numbers in res/make_lightning_panel.py) ---- 22 HP
static const float L_COL[4] = {13.f, 32.f, 51.f, 70.f};
static const float L_C5 = 94.5f;
static const float L_DISP_Y = 19.5f, L_DISP_W = 16.5f, L_DISP_H = 13.f;
static const float L_KEEP_DX = -4.8f, L_SEND_DX = 5.f, L_BTN_Y = 43.f;
static const float L_ROW[3] = {58.f, 73.f, 88.f};
static const float L_IO_TOP = 104.f, L_IO_BOT = 118.f;

using namespace lightning;

struct Lightning : Module {
	enum ParamIds {
		ENUMS(TUNE_PARAM, 4), ENUMS(DECAY_PARAM, 4), ENUMS(LEVEL_PARAM, 4),
		ENUMS(KEEP_PARAM, 4), ENUMS(SEND_PARAM, 4),
		SENS_PARAM, MIX_PARAM,
		NUM_PARAMS
	};
	enum InputIds { ENUMS(TRIG_INPUT, 4), IN_INPUT, NUM_INPUTS };
	enum OutputIds { ENUMS(OUT_OUTPUT, 4), MIX_OUTPUT, NUM_OUTPUTS };
	enum LightIds { ENUMS(KEEP_LIGHT, 4), ENUMS(SEND_LIGHT, 4), LINK_LIGHT, NUM_LIGHTS };

	Recorder rec;
	Clip clips[NUM_SLOTS][2]; // double buffer per slot: play one, fill the other
	std::atomic<int> active[NUM_SLOTS];
	std::atomic<int> version[NUM_SLOTS]; // goes up when a slot changes (the display redraws)
	SlotPlayer player[NUM_SLOTS];
	bool fromThunder[NUM_SLOTS];
	float vals[NUM_SLOTS][NUM_VALS];
	bool armed[NUM_SLOTS];
	uint32_t armedSerial[NUM_SLOTS];
	std::atomic<bool> audition[NUM_SLOTS];
	// sounds loaded on the UI thread, swapped in by process()
	Clip pending[NUM_SLOTS];
	std::atomic<int> pendingState[NUM_SLOTS]; // 0 empty, 1 writing, 2 ready
	dsp::SchmittTrigger trig[NUM_SLOTS], keepBtn[NUM_SLOTS], sendBtn[NUM_SLOTS];
	dsp::ClockDivider lightDivider;
	bool linked = false;
	uint32_t lastCount = 0;
	float blink = 0.f;
	float sendFlash[NUM_SLOTS];
	float keepFlash[NUM_SLOTS];
	bool outsFeedMix = false;
	bool semitones = false;
	float sr = 48000.f;

	Lightning() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int s = 0; s < NUM_SLOTS; s++) {
			configParam(TUNE_PARAM + s, -24.f, 24.f, 0.f, string::f("Slot %d tune", s + 1), " semitones");
			configParam(DECAY_PARAM + s, 0.f, 1.f, 1.f, string::f("Slot %d decay (fully up = the whole hit)", s + 1), "%", 0.f, 100.f);
			configParam(LEVEL_PARAM + s, 0.f, 1.f, 0.8f, string::f("Slot %d level", s + 1), " dB", -10.f, 40.f, 1.25f)->randomizeEnabled = false;
			configButton(KEEP_PARAM + s, string::f("Keep %d (the last whole hit goes into slot %d)", s + 1, s + 1));
			configButton(SEND_PARAM + s, string::f("Send %d (put slot %d's sound back on Thunder's knobs)", s + 1, s + 1));
			configInput(TRIG_INPUT + s, string::f("Trigger %d", s + 1));
			configOutput(OUT_OUTPUT + s, string::f("Slot %d (patched: leaves the mix)", s + 1));
			active[s] = 0;
			version[s] = 0;
			audition[s] = false;
			pendingState[s] = 0;
			fromThunder[s] = false;
			armed[s] = false;
			armedSerial[s] = 0;
			sendFlash[s] = keepFlash[s] = 0.f;
			std::fill(vals[s], vals[s] + NUM_VALS, 0.f);
		}
		configParam(SENS_PARAM, 0.f, 1.f, 0.5f, "IN sensitivity (how loud a hit must be to count)", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 0.8f, "Mix level", " dB", -10.f, 40.f, 1.25f)->randomizeEnabled = false;
		configInput(IN_INPUT, "Audio in (used instead of Thunder when patched)");
		configOutput(MIX_OUTPUT, "Mix");
		lightDivider.setDivision(64);
		setRate(48000.f);
	}

	// sizes the ring and every slot buffer (not on the audio thread except at a rate change)
	void setRate(float r) {
		sr = r;
		rec.setSampleRate(r);
		size_t cap = (size_t) (MAX_SEC * r) + 16;
		for (int s = 0; s < NUM_SLOTS; s++)
			for (int b = 0; b < 2; b++) {
				Clip& c = clips[s][b];
				if (c.data.size() < cap) c.data.resize(cap, 0.f);
			}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { setRate(e.sampleRate); }

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int s = 0; s < NUM_SLOTS; s++) clearSlot(s);
	}

	// UI thread: hand a sound to process()
	bool offer(int s, Clip& c) {
		int expected = 0;
		if (!pendingState[s].compare_exchange_strong(expected, 1)) return false;
		std::swap(pending[s], c);
		pendingState[s] = 2;
		return true;
	}

	void clearSlot(int s) {
		Clip c;
		c.data.assign((size_t) (MAX_SEC * sr) + 16, 0.f);
		c.len = 0;
		if (offer(s, c)) fromThunder[s] = false;
	}

	void loadWav(int s, const std::string& path) {
		thunder::Sample smp;
		std::string err;
		if (!readWav(path, smp, err)) {
			WARN("Lightning: %s: %s", path.c_str(), err.c_str());
			return;
		}
		Clip c;
		c.len = (int) std::min(smp.data.size(), (size_t) (MAX_SEC * smp.sr));
		c.data.assign(std::max((size_t) c.len, (size_t) (MAX_SEC * sr) + 16), 0.f);
		std::copy(smp.data.begin(), smp.data.begin() + c.len, c.data.begin());
		c.rate = smp.sr;
		if (offer(s, c)) fromThunder[s] = false;
	}

	bool saveWav(int s, const std::string& path) {
		const Clip& c = clips[s][active[s]];
		if (c.len <= 0) return false;
		return writeWav(path, c.data.data(), c.len, c.rate);
	}

	void capture(int s) {
		int target = 1 - active[s];
		player[s].stopBuffer(target);
		Clip& c = clips[s][target];
		int len = rec.copyDone(c.data);
		if (len <= 0) return;
		c.len = len;
		c.rate = sr;
		fromThunder[s] = rec.doneExact;
		std::copy(rec.doneVals, rec.doneVals + NUM_VALS, vals[s]);
		active[s] = target;
		version[s]++;
		keepFlash[s] = 1.f;
	}

	ThunderLink* thunder() {
		Module* m = leftExpander.module;
		if (!m || m->model != modelThunder) return nullptr;
		return dynamic_cast<ThunderLink*>(m);
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != sr) setRate(args.sampleRate);
		for (int s = 0; s < NUM_SLOTS; s++)
			if (pendingState[s] == 2) {
				int target = 1 - active[s];
				player[s].stopBuffer(target);
				std::swap(clips[s][target], pending[s]);
				active[s] = target;
				version[s]++;
				pendingState[s] = 0;
			}

		// ---- listen: audio IN if patched, else Thunder on the left
		ThunderLink* link = thunder();
		bool useIn = inputs[IN_INPUT].isConnected();
		float x = 0.f;
		if (link && !linked) lastCount = link->hitCount; // just placed: no fake hit
		linked = link != nullptr;
		if (useIn)
			x = inputs[IN_INPUT].getVoltage();
		else if (link) {
			if (link->hitCount != lastCount) {
				lastCount = link->hitCount;
				rec.startHit(std::max<int64_t>(0, rec.w - 1), true, link->hitVals, quietForThunder(link->hitVals));
			}
			x = link->linkOut;
		}
		float thresh = 3.f * std::pow(0.005f, params[SENS_PARAM].getValue());
		rec.push(x, useIn, thresh);

		// ---- KEEP: the last whole hit. If one is still sounding, wait for it to end.
		for (int s = 0; s < NUM_SLOTS; s++) {
			if (keepBtn[s].process(params[KEEP_PARAM + s].getValue())) {
				if (armed[s])
					armed[s] = false;
				else if (!rec.inHit && rec.haveDone)
					capture(s);
				else {
					armed[s] = true;
					armedSerial[s] = rec.doneSerial;
				}
			}
			if (armed[s] && rec.doneSerial != armedSerial[s]) {
				armed[s] = false;
				capture(s);
			}
			// SEND: back onto Thunder's knobs
			if (sendBtn[s].process(params[SEND_PARAM + s].getValue()) && link && fromThunder[s]) {
				int expected = 0;
				if (link->sendState.compare_exchange_strong(expected, 1)) {
					std::copy(vals[s], vals[s] + NUM_VALS, link->sendVals);
					link->sendState = 2;
					sendFlash[s] = 1.f;
				}
			}
		}

		// ---- play
		float mix = 0.f;
		for (int s = 0; s < NUM_SLOTS; s++) {
			bool t = trig[s].process(inputs[TRIG_INPUT + s].getVoltage(), 0.1f, 1.f);
			if (audition[s].exchange(false)) t = true;
			if (t) player[s].trigger(active[s]);
			float y = player[s].process(clips[s], sr, params[TUNE_PARAM + s].getValue(), params[DECAY_PARAM + s].getValue());
			y *= params[LEVEL_PARAM + s].getValue() * 1.25f;
			outputs[OUT_OUTPUT + s].setVoltage(y);
			if (outsFeedMix || !outputs[OUT_OUTPUT + s].isConnected()) mix += y;
		}
		outputs[MIX_OUTPUT].setVoltage(clamp(mix * params[MIX_PARAM].getValue() * 1.25f, -12.f, 12.f));

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			blink += dt * 4.f;
			if (blink >= 1.f) blink -= 1.f;
			for (int s = 0; s < NUM_SLOTS; s++) {
				keepFlash[s] = std::max(0.f, keepFlash[s] - dt * 3.f);
				sendFlash[s] = std::max(0.f, sendFlash[s] - dt * 3.f);
				float k = clips[s][active[s]].len > 0 ? 0.35f : 0.f;
				if (armed[s]) k = blink < 0.5f ? 1.f : 0.1f;
				lights[KEEP_LIGHT + s].setBrightness(std::max(k, keepFlash[s]));
				float sl = (link && fromThunder[s]) ? 0.3f : 0.f;
				lights[SEND_LIGHT + s].setBrightness(std::max(sl, sendFlash[s]));
			}
			lights[LINK_LIGHT].setBrightness(link && !useIn ? 1.f : 0.f);
		}
	}

	// ---- saving: the sounds live in the patch as WAVs, the rest in JSON
	void onSave(const SaveEvent& e) override {
		std::string dir = createPatchStorageDirectory();
		for (int s = 0; s < NUM_SLOTS; s++) {
			std::string path = system::join(dir, string::f("slot%d.wav", s + 1));
			if (clips[s][active[s]].len > 0) saveWav(s, path);
			else system::remove(path);
		}
	}

	void onAdd(const AddEvent& e) override {
		std::string dir = getPatchStorageDirectory();
		if (dir.empty()) return;
		for (int s = 0; s < NUM_SLOTS; s++) {
			std::string path = system::join(dir, string::f("slot%d.wav", s + 1));
			if (!system::isFile(path)) continue;
			bool keep = fromThunder[s];
			loadWav(s, path);
			fromThunder[s] = keep; // a sound kept from Thunder can still be sent back
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* slots = json_array();
		for (int s = 0; s < NUM_SLOTS; s++) {
			json_t* o = json_object();
			json_object_set_new(o, "fromThunder", json_boolean(fromThunder[s]));
			json_t* v = json_array();
			for (int i = 0; i < NUM_VALS; i++) json_array_append_new(v, json_real(vals[s][i]));
			json_object_set_new(o, "thunder", v);
			json_array_append_new(slots, o);
		}
		json_object_set_new(root, "slots", slots);
		json_object_set_new(root, "outsFeedMix", json_boolean(outsFeedMix));
		json_object_set_new(root, "semitones", json_boolean(semitones));
		return root;
	}

	void setSemitones(bool on) {
		semitones = on;
		for (int s = 0; s < NUM_SLOTS; s++) {
			paramQuantities[TUNE_PARAM + s]->snapEnabled = on;
			if (on) params[TUNE_PARAM + s].setValue(std::round(params[TUNE_PARAM + s].getValue()));
		}
	}

	void dataFromJson(json_t* root) override {
		json_t* slots = json_object_get(root, "slots");
		for (int s = 0; slots && s < NUM_SLOTS && s < (int) json_array_size(slots); s++) {
			json_t* o = json_array_get(slots, s);
			json_t* f = json_object_get(o, "fromThunder");
			fromThunder[s] = f && json_is_true(f);
			json_t* v = json_object_get(o, "thunder");
			for (int i = 0; v && i < NUM_VALS && i < (int) json_array_size(v); i++) vals[s][i] = json_number_value(json_array_get(v, i));
		}
		json_t* j;
		if ((j = json_object_get(root, "outsFeedMix"))) outsFeedMix = json_is_true(j);
		if ((j = json_object_get(root, "semitones"))) setSemitones(json_is_true(j));
	}
};

// ---- one slot's waveform: click to hear it ----
struct LightningDisplay : OpaqueWidget {
	Lightning* module = nullptr;
	int slot = 0;
	int seen = -1;
	float mn[64], mx[64];
	bool empty = true;

	void rebuild() {
		const Clip& c = module->clips[slot][module->active[slot]];
		empty = c.len <= 0;
		float peak = 1e-3f;
		for (int i = 0; i < 64; i++) {
			mn[i] = mx[i] = 0.f;
			if (empty) continue;
			int a = (int) ((int64_t) c.len * i / 64), b = (int) ((int64_t) c.len * (i + 1) / 64);
			for (int k = a; k < std::max(b, a + 1) && k < c.len; k++) {
				mn[i] = std::min(mn[i], c.data[k]);
				mx[i] = std::max(mx[i], c.data[k]);
			}
			peak = std::max(peak, std::max(mx[i], -mn[i]));
		}
		for (int i = 0; i < 64; i++) {
			mn[i] /= peak;
			mx[i] /= peak;
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->audition[slot] = true;
			e.consume(this);
		}
		OpaqueWidget::onButton(e);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return Widget::drawLayer(args, layer);
		NVGcontext* vg = args.vg;
		float w = box.size.x, h = box.size.y;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, w, h, 2.f);
		nvgFillColor(vg, nvgRGB(0x0c, 0x1a, 0x12));
		nvgFill(vg);
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		bool armed = false, thunderTag = false;
		float pos = 0.f;
		if (module) {
			int v = module->version[slot];
			if (v != seen) {
				seen = v;
				rebuild();
			}
			armed = module->armed[slot];
			thunderTag = module->fromThunder[slot];
			pos = module->player[slot].lastPos;
		}
		NVGcolor green = nvgRGB(0x66, 0xff, 0x77);
		if (!empty) {
			nvgBeginPath(vg);
			float mid = h * 0.55f, amp = h * 0.38f;
			for (int i = 0; i < 64; i++) {
				float x = 1.f + (w - 2.f) * (i + 0.5f) / 64.f;
				nvgMoveTo(vg, x, mid - mx[i] * amp);
				nvgLineTo(vg, x, mid - mn[i] * amp + 0.5f);
			}
			nvgStrokeColor(vg, green);
			nvgStrokeWidth(vg, 0.9f);
			nvgStroke(vg);
			if (pos > 0.f) {
				nvgBeginPath(vg);
				float x = 1.f + (w - 2.f) * pos;
				nvgMoveTo(vg, x, 2.f);
				nvgLineTo(vg, x, h - 2.f);
				nvgStrokeColor(vg, nvgRGB(0xff, 0xb2, 0x33));
				nvgStrokeWidth(vg, 1.f);
				nvgStroke(vg);
			}
		}
		if (font && font->handle >= 0) {
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, 9.f);
			nvgFillColor(vg, green);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(vg, 2.5f, 1.5f, string::f("%d", slot + 1).c_str(), NULL);
			if (thunderTag) {
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgText(vg, w - 2.5f, 1.5f, "T", NULL);
			}
			if (armed || empty) {
				nvgFontSize(vg, 7.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgText(vg, w / 2, h * 0.6f, armed ? "WAIT" : "EMPTY", NULL);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct LightningWidget : ModuleWidget {
	LightningWidget(Lightning* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lightning.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		typedef Lightning L;
		for (int s = 0; s < NUM_SLOTS; s++) {
			float x = L_COL[s];
			LightningDisplay* d = createWidget<LightningDisplay>(mm2px(Vec(x - L_DISP_W / 2, L_DISP_Y)));
			d->box.size = mm2px(Vec(L_DISP_W, L_DISP_H));
			d->module = module;
			d->slot = s;
			addChild(d);
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(x + L_KEEP_DX, L_BTN_Y)), module, L::KEEP_PARAM + s, L::KEEP_LIGHT + s));
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenLight>>>(mm2px(Vec(x + L_SEND_DX, L_BTN_Y)), module, L::SEND_PARAM + s, L::SEND_LIGHT + s));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, L_ROW[0])), module, L::TUNE_PARAM + s));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, L_ROW[1])), module, L::DECAY_PARAM + s));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, L_ROW[2])), module, L::LEVEL_PARAM + s));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, L_IO_TOP)), module, L::TRIG_INPUT + s));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, L_IO_BOT)), module, L::OUT_OUTPUT + s));
		}
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(L_C5, L_BTN_Y)), module, L::LINK_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(L_C5, L_ROW[0])), module, L::SENS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(L_C5, L_ROW[2])), module, L::MIX_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L_C5, L_IO_TOP)), module, L::IN_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L_C5, L_IO_BOT)), module, L::MIX_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Lightning* m = dynamic_cast<Lightning*>(this->module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Patched slot outs also feed MIX", "", &m->outsFeedMix));
		menu->addChild(createBoolMenuItem("TUNE in semitones", "", [=]() { return m->semitones; }, [=](bool on) { m->setSemitones(on); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Slots (click a waveform to hear it)"));
		for (int s = 0; s < NUM_SLOTS; s++) {
			menu->addChild(createSubmenuItem(string::f("Slot %d", s + 1), "", [=](Menu* sub) {
				sub->addChild(createMenuItem("Save as WAV...", "", [=]() {
					osdialog_filters* filters = osdialog_filters_parse("WAV:wav");
					char* path = osdialog_file(OSDIALOG_SAVE, nullptr, string::f("lightning-%d.wav", s + 1).c_str(), filters);
					osdialog_filters_free(filters);
					if (path) {
						std::string p = path;
						std::free(path);
						if (system::getExtension(p) != ".wav") p += ".wav";
						m->saveWav(s, p);
					}
				}));
				sub->addChild(createMenuItem("Load WAV...", "", [=]() {
					osdialog_filters* filters = osdialog_filters_parse("WAV:wav,WAV");
					char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
					osdialog_filters_free(filters);
					if (path) {
						m->loadWav(s, path);
						std::free(path);
					}
				}));
				sub->addChild(createMenuItem("Clear", "", [=]() { m->clearSlot(s); }));
			}));
		}
	}
};

Model* modelLightning = createModel<Lightning, LightningWidget>("Lightning");
