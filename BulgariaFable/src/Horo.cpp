// Horo: clock and accent sequencer for Bulgarian uneven dance metres, companion to Bulgaria.
// Engine in src/dsp/HoroEngine.hpp (shared with the offline tests). Spec: docs/HORO-BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/HoroEngine.hpp"
#include <atomic>

// ---- LAYOUT (same numbers in res/make_horo_panel.py) ----
static const float DISP_X = 3.0f, DISP_Y = 14.0f, DISP_W = 44.8f, DISP_H = 24.0f;
static const float METRE_X = 16.0f, METRE_Y = 53.0f;
static const float RBTN_X = 39.0f, RBTN_Y = 53.0f;
static const float KROW_Y = 72.0f;
static const float KROW_X[3] = {9.5f, 25.4f, 41.3f};
static const float IN_Y = 90.0f;
static const float IN_X[4] = {7.4f, 19.4f, 31.4f, 43.4f};
static const float OUT_Y[2] = {104.5f, 117.0f};
static const float OUT_X[3] = {9.0f, 25.4f, 41.8f};

struct HoroPctQ : ParamQuantity {
	std::string getDisplayValueString() override { return string::f("%.0f%%", 100.0 * getValue()); }
};

struct Horo;

struct MetreQ : ParamQuantity {
	std::string getDisplayValueString() override;
};

struct LiltQ : ParamQuantity {
	std::string getDisplayValueString() override {
		float v = getValue();
		if (std::fabs(v) < 0.005f) return "0 (straight)";
		return string::f("%+.0f%% (%s)", 100.0 * v, v > 0 ? "long beats stretched" : "toward even");
	}
};

struct Horo : Module {
	enum ParamIds { METRE_P, BARS_P, LILT_P, ACCENT_P, RESET_P, NUM_PARAMS };
	enum InputIds { CLOCK_INPUT, RESET_INPUT, METRE_INPUT, LILT_INPUT, NUM_INPUTS };
	enum OutputIds { PULSE_OUTPUT, BEAT_OUTPUT, LONG_OUTPUT, BAR_OUTPUT, PHRASE_OUTPUT, ACCENT_OUTPUT, NUM_OUTPUTS };
	enum LightIds { BEAT_LIGHT, BAR_LIGHT, PHRASE_LIGHT, NUM_LIGHTS };

	horo::Engine e;
	horo::Params p;
	double rate = 0;
	bool needInit = true;
	dsp::PulseGenerator beatL, barL, phraseL;
	uint32_t lastBeat = 0, lastBar = 0, lastPhrase = 0;

	// user groupings: edited on the UI thread, copied into p on the audio thread
	horo::Metre userUI[horo::NUM_USER];
	std::atomic<int> userSeq{0};
	int userSeen = 0;

	// display snapshot
	int dispSlot = 2, dispReq = 2, dispLastK = -1, dispBar = 0, dispBars = 4;
	horo::Metre dispMetre, dispReqMetre;
	bool dispRunning = false;

	Horo() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<MetreQ>(METRE_P, 0.f, (float)(horo::NUM_SLOTS - 1), 2.f, "Metre")->snapEnabled = true;
		std::vector<std::string> bl;
		for (int i = 0; i < horo::NUM_BARS; i++) bl.push_back(string::f("%d", horo::BARS_VALUES[i]));
		configSwitch(BARS_P, 0.f, (float)(horo::NUM_BARS - 1), 3.f, "Bars per phrase", bl);
		configParam<LiltQ>(LILT_P, -1.f, 1.f, 0.f, "Lilt")->description =
			"Stretches the long beats (right) or squeezes them toward an even bar (left). The bar keeps its length.";
		configParam<HoroPctQ>(ACCENT_P, 0.f, 1.f, 0.8f, "Accent depth");
		configButton(RESET_P, "Reset (next clock is the first pulse of the phrase)");
		configInput(CLOCK_INPUT, "Clock (one edge per pulse, e.g. sixteenths)");
		configInput(RESET_INPUT, "Reset");
		configInput(METRE_INPUT, "Metre CV (0.5 V per slot, applied at the bar line)");
		configInput(LILT_INPUT, "Lilt CV (+0.2 per volt)");
		configOutput(PULSE_OUTPUT, "Pulse clock (with lilt)");
		configOutput(BEAT_OUTPUT, "Beat gate (start of every group; long beats give longer gates)");
		configOutput(LONG_OUTPUT, "Long beat gate");
		configOutput(BAR_OUTPUT, "Bar trigger");
		configOutput(PHRASE_OUTPUT, "Phrase trigger (patch to Bulgaria's PHRASE)");
		configOutput(ACCENT_OUTPUT, "Accent CV (0 to 10 V, strongest on the downbeat)");
		configLight(BEAT_LIGHT, "Beat");
		configLight(BAR_LIGHT, "Bar");
		configLight(PHRASE_LIGHT, "Phrase");
		for (int i = 0; i < horo::NUM_USER; i++) userUI[i] = horo::factoryMetre(horo::NUM_FACTORY + i);
		dispMetre = dispReqMetre = horo::factoryMetre(2);
	}

	void onReset() override {
		for (int i = 0; i < horo::NUM_USER; i++) userUI[i] = horo::factoryMetre(horo::NUM_FACTORY + i);
		p.phraseMode = horo::LAST_BEAT; p.changeAtPhrase = false;
		userSeq++;
		needInit = true;
	}

	void setUser(int i, const std::string& s) {
		horo::Metre m;
		if (horo::parseGrouping(s, m)) { userUI[i] = m; userSeq++; }
	}

	void process(const ProcessArgs& args) override {
		if (needInit || args.sampleRate != rate) { rate = args.sampleRate; e.init(rate); needInit = false; }
		int seq = userSeq.load();
		if (seq != userSeen) { for (int i = 0; i < horo::NUM_USER; i++) p.user[i] = userUI[i]; userSeen = seq; }

		float mcv = inputs[METRE_INPUT].isConnected() ? inputs[METRE_INPUT].getVoltage() : 0.f;
		p.slot = clamp((int)std::lround(params[METRE_P].getValue()) + (int)std::lround(2.f * mcv), 0, horo::NUM_SLOTS - 1);
		float lcv = inputs[LILT_INPUT].isConnected() ? 0.2f * inputs[LILT_INPUT].getVoltage() : 0.f;
		p.lilt = clamp(params[LILT_P].getValue() + lcv, -1.f, 1.f);
		p.accent = params[ACCENT_P].getValue();
		p.bars = horo::BARS_VALUES[clamp((int)std::lround(params[BARS_P].getValue()), 0, horo::NUM_BARS - 1)];

		horo::Inputs in;
		in.clock = inputs[CLOCK_INPUT].getVoltage();
		in.reset = std::max(inputs[RESET_INPUT].getVoltage(), params[RESET_P].getValue() > 0.5f ? 10.f : 0.f);
		e.process(p, in);

		outputs[PULSE_OUTPUT].setVoltage((float)e.gateV(e.pulse));
		outputs[BEAT_OUTPUT].setVoltage((float)e.gateV(e.beat));
		outputs[LONG_OUTPUT].setVoltage((float)e.gateV(e.lng));
		outputs[BAR_OUTPUT].setVoltage((float)e.gateV(e.barOut()));
		outputs[PHRASE_OUTPUT].setVoltage((float)e.gateV(e.phraseOut()));
		outputs[ACCENT_OUTPUT].setVoltage((float)e.accentV);

		if (e.beatCount != lastBeat) { lastBeat = e.beatCount; beatL.trigger(0.08f); }
		if (e.barCount != lastBar) { lastBar = e.barCount; barL.trigger(0.08f); }
		if (e.phraseCount != lastPhrase) { lastPhrase = e.phraseCount; phraseL.trigger(0.08f); }
		lights[BEAT_LIGHT].setBrightness(beatL.process(args.sampleTime) ? 1.f : 0.f);
		lights[BAR_LIGHT].setBrightness(barL.process(args.sampleTime) ? 1.f : 0.f);
		lights[PHRASE_LIGHT].setBrightness(phraseL.process(args.sampleTime) ? 1.f : 0.f);

		dispSlot = e.slot; dispReq = p.slot; dispMetre = e.m; dispReqMetre = p.metreFor(p.slot);
		dispLastK = e.lastK; dispBar = e.bar; dispBars = p.bars; dispRunning = e.started;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		for (int i = 0; i < horo::NUM_USER; i++)
			json_object_set_new(root, string::f("user%d", i + 1).c_str(), json_string(userUI[i].str("+").c_str()));
		json_object_set_new(root, "phraseMode", json_integer(p.phraseMode));
		json_object_set_new(root, "changeAtPhrase", json_boolean(p.changeAtPhrase));
		return root;
	}
	void dataFromJson(json_t* root) override {
		for (int i = 0; i < horo::NUM_USER; i++) {
			json_t* s = json_object_get(root, string::f("user%d", i + 1).c_str());
			if (s && json_is_string(s)) setUser(i, json_string_value(s));
		}
		json_t* pm = json_object_get(root, "phraseMode");
		if (pm) p.phraseMode = clamp((int)json_integer_value(pm), 0, 2);
		json_t* cp = json_object_get(root, "changeAtPhrase");
		if (cp) p.changeAtPhrase = json_is_true(cp);
		userSeq++;
	}
};

std::string MetreQ::getDisplayValueString() {
	int s = clamp((int)std::lround(getValue()), 0, horo::NUM_SLOTS - 1);
	Horo* m = dynamic_cast<Horo*>(module);
	horo::Metre mt = (m && s >= horo::NUM_FACTORY) ? m->userUI[s - horo::NUM_FACTORY] : horo::factoryMetre(s);
	return string::f("%s (%d: %s)", horo::SLOT_NAMES[s], mt.count(), mt.str("+").c_str());
}

struct HoroDisplay : Widget {
	Horo* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, mm2px(1.2f));
		nvgFillColor(args.vg, nvgRGB(18, 14, 13));
		nvgFill(args.vg);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		horo::Metre m = module ? module->dispMetre : horo::factoryMetre(2);
		int slot = module ? module->dispSlot : 2, req = module ? module->dispReq : 2;
		horo::Metre rm = module ? module->dispReqMetre : m;
		int lastK = module ? module->dispLastK : -1;
		int bar = module ? module->dispBar : 0, bars = module ? module->dispBars : 4;
		bool running = module && module->dispRunning;
		bool pending = (req != slot) || !(rm == m);

		NVGcolor gold = nvgRGB(232, 176, 74), ink = nvgRGB(241, 234, 223), dim = nvgRGB(120, 104, 92), red = nvgRGB(217, 83, 74);
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		float W = box.size.x;
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			// line 1: name and count (the coming metre, dimmed, while a change waits for the bar line)
			std::string l1 = pending ? string::f("%s %d", horo::SLOT_NAMES[req], rm.count()) : string::f("%s %d", horo::SLOT_NAMES[slot], m.count());
			nvgFontSize(args.vg, mm2px(4.0f));
			nvgFillColor(args.vg, pending ? dim : gold);
			nvgText(args.vg, mm2px(1.6f), mm2px(5.2f), l1.c_str(), NULL);
			// line 2: grouping
			std::string l2 = m.str(" ");
			float fs = m.ng > 8 ? 3.4f * 8.f / m.ng + 0.6f : 3.4f;
			nvgFontSize(args.vg, mm2px(fs));
			nvgFillColor(args.vg, ink);
			nvgText(args.vg, mm2px(1.6f), mm2px(9.6f), l2.c_str(), NULL);
			// line 4: bar
			nvgFontSize(args.vg, mm2px(2.6f));
			nvgFillColor(args.vg, dim);
			std::string l4 = string::f("BAR %d/%d", (running ? bar : 0) + 1, bars);
			nvgText(args.vg, mm2px(1.6f), box.size.y - mm2px(1.4f), l4.c_str(), NULL);
		}

		// line 3: one dot per pulse, gap between groups, two rows above 16 pulses
		int N = m.count();
		int split = m.ng;   // groups in row 1
		if (N > 16) {
			int best = 1, bestD = 99, acc = 0;
			for (int i = 0; i < m.ng - 1; i++) { acc += m.g[i]; int d = std::abs(2 * acc - N); if (d < bestD) { bestD = d; best = i + 1; } }
			split = best;
		}
		int shortest = 99;
		for (int i = 0; i < m.ng; i++) shortest = std::min(shortest, m.g[i]);
		int curG = -1;
		if (running && lastK >= 0 && lastK < N) { int a = 0; for (int i = 0; i < m.ng; i++) { if (lastK < a + m.g[i]) { curG = i; break; } a += m.g[i]; } }
		float x0 = mm2px(1.8f), avail = W - 2 * x0;
		float rowY[2] = {mm2px(N > 16 ? 13.2f : 15.0f), mm2px(17.4f)};
		int k = 0;
		for (int row = 0; row < (N > 16 ? 2 : 1); row++) {
			int g0 = row == 0 ? 0 : split, g1 = row == 0 ? split : m.ng;
			int n = 0;
			for (int i = g0; i < g1; i++) n += m.g[i];
			int gaps = g1 - g0 - 1;
			float unit = avail / (n + 0.9f * gaps);
			float r = std::min(mm2px(0.85f), unit * 0.32f);
			float x = x0 + unit * 0.5f;
			for (int i = g0; i < g1; i++) {
				bool lg = m.g[i] > shortest;
				for (int j = 0; j < m.g[i]; j++) {
					NVGcolor c = (k == lastK && running) ? (j == 0 ? red : gold) : (i == curG ? nvgRGB(130, 98, 50) : nvgRGB(70, 60, 54));
					nvgBeginPath(args.vg);
					nvgCircle(args.vg, x, rowY[row], (lg ? 1.25f : 1.0f) * r);
					nvgFillColor(args.vg, c);
					nvgFill(args.vg);
					x += unit; k++;
				}
				x += 0.9f * unit;
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct HoroWidget : ModuleWidget {
	HoroWidget(Horo* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Horo.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		typedef Horo M;

		HoroDisplay* d = createWidget<HoroDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(METRE_X, METRE_Y)), module, M::METRE_P));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(RBTN_X, RBTN_Y)), module, M::RESET_P));
		const int kr[3] = {M::BARS_P, M::LILT_P, M::ACCENT_P};
		for (int i = 0; i < 3; i++) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(KROW_X[i], KROW_Y)), module, kr[i]));
		const int in[4] = {M::CLOCK_INPUT, M::RESET_INPUT, M::METRE_INPUT, M::LILT_INPUT};
		for (int i = 0; i < 4; i++) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_X[i], IN_Y)), module, in[i]));
		const int out[6] = {M::PULSE_OUTPUT, M::BEAT_OUTPUT, M::LONG_OUTPUT, M::BAR_OUTPUT, M::PHRASE_OUTPUT, M::ACCENT_OUTPUT};
		for (int i = 0; i < 6; i++) addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_X[i % 3], OUT_Y[i / 3])), module, out[i]));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(OUT_X[1] + 5.4f, OUT_Y[0] - 4.6f)), module, M::BEAT_LIGHT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(OUT_X[0] + 5.4f, OUT_Y[1] - 4.6f)), module, M::BAR_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(OUT_X[1] + 6.4f, OUT_Y[1] - 4.6f)), module, M::PHRASE_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Horo* m = dynamic_cast<Horo*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Phrase trigger on", {"Last beat", "Last long beat", "Last pulse"}, &m->p.phraseMode));
		menu->addChild(createIndexSubmenuItem("Change metre at", {"Next bar", "Next phrase"},
			[=]() { return m->p.changeAtPhrase ? 1 : 0; }, [=](int i) { m->p.changeAtPhrase = (i == 1); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("User metres: groups of 1 to 9, e.g. 2+2+3 (32 pulses max)"));
		for (int i = 0; i < horo::NUM_USER; i++) {
			struct UserField : ui::TextField {
				Horo* m; int i;
				void onChange(const ChangeEvent& e) override { m->setUser(i, text); ui::TextField::onChange(e); }
				void onSelectKey(const SelectKeyEvent& e) override {
					if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
						m->setUser(i, text);
						ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
						if (overlay) overlay->requestDelete();
						e.consume(this);
					}
					if (!e.getTarget()) ui::TextField::onSelectKey(e);
				}
			};
			menu->addChild(createSubmenuItem(string::f("User %d", i + 1), m->userUI[i].str("+"), [=](Menu* sub) {
				UserField* f = new UserField;
				f->m = m; f->i = i;
				f->box.size.x = 140;
				f->text = m->userUI[i].str("+");
				sub->addChild(createMenuLabel("Type a grouping (applies at the next bar line)"));
				sub->addChild(f);
			}));
		}
	}
};

Model* modelHoro = createModel<Horo, HoroWidget>("Horo");
