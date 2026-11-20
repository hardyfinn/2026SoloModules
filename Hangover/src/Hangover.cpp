// Hangover: a handpan (hang drum) for VCV Rack, with the real instrument's dirt left in.
// The sound engine is in HangoverDSP.hpp; this file is the panel, the jacks and the screen.
#include "plugin.hpp"
#include "HangoverDSP.hpp"

using namespace hangover;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 6.0f, DISP_Y = 14.5f, DISP_W = 104.0f, DISP_H = 50.0f;
static const float MX[3] = {130.0f, 160.0f, 190.0f};
static const float MY[3] = {26.0f, 43.0f, 60.0f};
static const float DIRT_Y = 86.0f, DIRT_MX = 19.0f, DX0 = 40.0f, DDX = 19.5f, NEW_X = 199.0f;
static const float ROWA_Y = 106.0f, ROWB_Y = 119.0f;
static const float TRIG_X0 = 10.0f, TRIG_DX = 12.2f;
static const float POLY_X0 = 140.0f, POLY_DX = 15.5f;
static const float CV_X0 = 10.0f, CV_DX = 16.0f;
static const float OUT_X[2] = {186.5f, 202.0f};

static const int NUM_DIRT = 8;
static const char* DIRT_NAMES[NUM_DIRT] = {"Tuning", "Beat", "Rattle", "Bleed", "Air", "Skin", "Slop", "Room"};

struct Hangover : Module {
	enum ParamId {
		SCALE_PARAM, KEY_PARAM, DECAY_PARAM,
		FORCE_PARAM, TOUCH_PARAM, STRIKE_PARAM,
		MUTE_PARAM, GU_PARAM, ROLL_PARAM,
		DIRT_PARAM,
		ENUMS(DIRTK_PARAM, NUM_DIRT),
		NEW_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(TRIG_INPUT, 9),
		GU_INPUT,
		GATE_INPUT, VOCT_INPUT, VEL_INPUT, MUTE_INPUT, ROLL_INPUT,
		DECAY_CV_INPUT, TOUCH_CV_INPUT, STRIKE_CV_INPUT, MUTE_CV_INPUT, GU_CV_INPUT, DIRT_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId { L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	HangoverEngine eng;
	dsp::SchmittTrigger trig[9], guTrig, gateTrig[16], muteTrig, newTrig;
	dsp::ClockDivider paramDiv;
	float lastSr = 0.f;
	// for the screen
	float dispDirt[NUM_DIRT] = {};
	float dispMaster = 0.5f;
	bool dispMute = false, dispRoll = false;
	float dispRattle = 0.f;
	int lastRattleHits = 0;

	Hangover() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> scaleNames;
		for (int i = 0; i < NUM_SCALES; i++) scaleNames.push_back(HG_SCALES[i].name);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES - 1, 0.f, "Scale", scaleNames);
		std::vector<std::string> keyNames;
		for (int i = 0; i < 12; i++) keyNames.push_back(HG_NOTE_NAMES[i]);
		configSwitch(KEY_PARAM, 0.f, 11.f, 2.f, "Key (the ding's note)", keyNames);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.7f, "Decay (how long the ding rings)", " s", 30.f, 0.4f);
		configParam(FORCE_PARAM, 0.f, 1.f, 0.7f, "Force (when VEL is not patched)", "%", 0.f, 100.f);
		configParam(TOUCH_PARAM, 0.f, 1.f, 0.3f, "Touch: soft finger pad to hard slap", "%", 0.f, 100.f);
		configParam(STRIKE_PARAM, 0.f, 1.f, 0.3f, "Strike spot: centre of the field (fundamental) to edge (octave + fifth)", "%", 0.f, 100.f);
		configParam(MUTE_PARAM, 0.f, 1.f, 0.f, "Mute: palm resting on the shell", "%", 0.f, 100.f);
		configParam(GU_PARAM, 0.f, 1.f, 0.5f, "Gu: Helmholtz bass from the port underneath", "%", 0.f, 100.f);
		configParam(ROLL_PARAM, 0.f, 1.f, 0.5f, "Roll speed (while ROLL is high)", " hits/s", 4.f, 6.f);
		configParam(DIRT_PARAM, 0.f, 1.f, 0.5f, "Dirt: scales all eight dirt knobs (middle = as set)", "%", 0.f, 200.f);
		const float defs[NUM_DIRT] = {0.3f, 0.3f, 0.2f, 0.3f, 0.3f, 0.3f, 0.2f, 0.25f};
		const char* desc[NUM_DIRT] = {
			"Tuning: fields, octaves and fifths drift off true; hard hits start a little sharp",
			"Beat: every partial is a pair of close modes that beat slowly",
			"Rattle: a loose part and one loose field buzz on hard hits",
			"Bleed: the shell knocks the neighbours and they ring in sympathy",
			"Air: breath through the gu, turbulence and room tone",
			"Skin: finger and palm noise, the odd fingernail",
			"Slop: late, uneven, bounced and grazed strikes",
			"Room: a small wooden room around the instrument"};
		for (int i = 0; i < NUM_DIRT; i++) configParam(DIRTK_PARAM + i, 0.f, 1.f, defs[i], desc[i], "%", 0.f, 100.f);
		configButton(NEW_PARAM, "New hang (re-roll this instrument's imperfections)");

		configInput(TRIG_INPUT + 0, "Ding (centre note) trigger");
		for (int i = 1; i < 9; i++) configInput(TRIG_INPUT + i, string::f("Tone field %d trigger (ascending)", i));
		configInput(GU_INPUT, "Gu (bass slap beside the port) trigger");
		configInput(GATE_INPUT, "Gate (poly): each rising edge strikes the field nearest V/OCT");
		configInput(VOCT_INPUT, "V/OCT (poly): picks the nearest field (unpatched: the ding)");
		configInput(VEL_INPUT, "Velocity 0-10 V (poly, also used by the trigger inputs)");
		configInput(MUTE_INPUT, "Mute gate: palm down on the shell");
		configInput(ROLL_INPUT, "Roll gate: thumb roll on the last field played");
		configInput(DECAY_CV_INPUT, "Decay CV");
		configInput(TOUCH_CV_INPUT, "Touch CV");
		configInput(STRIKE_CV_INPUT, "Strike spot CV");
		configInput(MUTE_CV_INPUT, "Mute CV");
		configInput(GU_CV_INPUT, "Gu CV");
		configInput(DIRT_CV_INPUT, "Dirt CV");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		paramDiv.setDivision(16);
		eng.reseed(random::u32());
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		eng.silence();
	}

	float cv(int id) { return inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f : 0.f; }

	void updateParams() {
		HgParams& p = eng.p;
		p.scale = (int) std::round(params[SCALE_PARAM].getValue());
		p.root = (int) std::round(params[KEY_PARAM].getValue());
		float d = clamp(params[DECAY_PARAM].getValue() + cv(DECAY_CV_INPUT), 0.f, 1.f);
		p.decay = 0.4f * std::pow(30.f, d);
		p.force = params[FORCE_PARAM].getValue();
		p.touch = clamp(params[TOUCH_PARAM].getValue() + cv(TOUCH_CV_INPUT), 0.f, 1.f);
		p.strike = clamp(params[STRIKE_PARAM].getValue() + cv(STRIKE_CV_INPUT), 0.f, 1.f);
		p.mute = clamp(params[MUTE_PARAM].getValue() + cv(MUTE_CV_INPUT), 0.f, 1.f);
		p.gu = clamp(params[GU_PARAM].getValue() + cv(GU_CV_INPUT), 0.f, 1.f);
		p.roll = params[ROLL_PARAM].getValue();
		float master = clamp(params[DIRT_PARAM].getValue() + cv(DIRT_CV_INPUT), 0.f, 1.f);
		float m = 2.f * master;
		float a[NUM_DIRT];
		for (int i = 0; i < NUM_DIRT; i++) a[i] = clamp(params[DIRTK_PARAM + i].getValue() * m, 0.f, 1.f);
		p.tuning = a[0]; p.beat = a[1]; p.rattle = a[2]; p.bleed = a[3];
		p.air = a[4]; p.skin = a[5]; p.slop = a[6]; p.room = a[7];
		for (int i = 0; i < NUM_DIRT; i++) dispDirt[i] = a[i];
		dispMaster = master;
	}

	float velocity(int c) {
		if (inputs[VEL_INPUT].isConnected()) {
			int ch = std::min(c, inputs[VEL_INPUT].getChannels() - 1);
			return clamp(inputs[VEL_INPUT].getVoltage(ch) * 0.1f, 0.f, 1.f);
		}
		return eng.p.force;
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != lastSr) {
			lastSr = args.sampleRate;
			eng.setSampleRate(args.sampleRate);
		}
		if (paramDiv.process()) updateParams();
		if (newTrig.process(params[NEW_PARAM].getValue())) eng.reseed(random::u32());

		// single-field triggers
		for (int i = 0; i < 9; i++) {
			if (trig[i].process(inputs[TRIG_INPUT + i].getVoltage(), 0.1f, 1.f) && i < eng.nNotes)
				eng.strike(i, velocity(0));
		}
		if (guTrig.process(inputs[GU_INPUT].getVoltage(), 0.1f, 1.f)) eng.strikeGu(velocity(0));

		// poly gate + pitch (e.g. Peal)
		int nch = inputs[GATE_INPUT].getChannels();
		bool pitch = inputs[VOCT_INPUT].isConnected();
		int pch = inputs[VOCT_INPUT].getChannels();
		for (int c = 0; c < nch; c++) {
			if (gateTrig[c].process(inputs[GATE_INPUT].getVoltage(c), 0.1f, 1.f)) {
				int n = pitch ? eng.nearest(inputs[VOCT_INPUT].getVoltage(std::min(c, pch - 1))) : 0;
				eng.strike(n, velocity(c));
			}
		}

		// palm and roll
		bool mute = inputs[MUTE_INPUT].getVoltage() >= 1.f;
		if (muteTrig.process(inputs[MUTE_INPUT].getVoltage(), 0.1f, 1.f)) eng.palm();
		float target = mute ? 1.f : 0.f;
		eng.muteGate += (target - eng.muteGate) * (mute ? 0.01f : 0.002f);
		bool roll = inputs[ROLL_INPUT].getVoltage() >= 1.f;
		eng.setRoll(roll);
		dispMute = mute || eng.p.mute > 0.5f;
		dispRoll = roll;

		float l, r;
		eng.process(l, r);
		if (eng.rattleHits != lastRattleHits) { dispRattle = 1.f; lastRattleHits = eng.rattleHits; }
		dispRattle *= 0.9997f;
		if (outputs[R_OUTPUT].isConnected()) {
			outputs[L_OUTPUT].setVoltage(l);
			outputs[R_OUTPUT].setVoltage(r);
		}
		else outputs[L_OUTPUT].setVoltage(0.5f * (l + r));
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer((json_int_t) eng.seed));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s) eng.reseed((uint32_t) json_integer_value(s));
	}
};

// ---- the screen: the hang seen from above, fields glowing as they ring

struct HangoverDisplay : Widget {
	Hangover* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		const NVGcolor cream = nvgRGB(243, 230, 207), soft = nvgRGB(201, 180, 154);
		const NVGcolor copper = nvgRGB(227, 154, 85), rust = nvgRGB(200, 101, 58);
		float w = box.size.x, h = box.size.y;

		// demo data for the module browser
		static HangoverEngine demo;
		static bool demoInit = false;
		if (!demoInit) { demo.layout(); demoInit = true; }
		const HangoverEngine& e = module ? module->eng : demo;
		float env[MAXN], flash[MAXN];
		for (int i = 0; i < MAXN; i++) {
			env[i] = module ? e.note[i].env : (i == 0 ? 0.6f : (i == 4 ? 0.35f : 0.f));
			flash[i] = module ? e.note[i].flash : (i == 0 ? 0.7f : 0.f);
		}

		// the shell
		float cx = mm2px(28.f), cy = h / 2.f, R = mm2px(22.5f);
		nvgBeginPath(vg);
		nvgCircle(vg, cx, cy, R);
		NVGpaint shellPaint = nvgRadialGradient(vg, cx - R * 0.3f, cy - R * 0.35f, R * 0.1f, R * 1.1f,
		                                        nvgRGB(92, 70, 52), nvgRGB(38, 27, 19));
		nvgFillPaint(vg, shellPaint);
		nvgFill(vg);
		nvgStrokeColor(vg, nvgTransRGBA(copper, 110));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		for (int i = 0; i < e.nNotes; i++) {
			const HgNote& n = e.note[i];
			float x = cx + n.px * R, y = cy + n.py * R;
			float fr = (i == 0 ? 6.4f : 4.6f) * (R / mm2px(22.5f));
			float rr = mm2px(fr) * (i == 0 ? 1.f : 1.f - 0.12f * (i - 1) / 8.f);
			float glow = clamp(env[i] * 0.9f + flash[i] * 0.6f, 0.f, 1.f);
			// dimple
			nvgBeginPath(vg);
			if (i == 0) nvgCircle(vg, x, y, rr);
			else nvgEllipse(vg, x, y, rr, rr * 0.82f);
			nvgFillColor(vg, nvgRGBA(20, 13, 9, 200));
			nvgFill(vg);
			if (glow > 0.01f) {
				NVGpaint gp = nvgRadialGradient(vg, x, y, rr * 0.1f, rr * 1.25f,
				                                nvgRGBAf(1.f, 0.66f, 0.33f, 0.95f * glow), nvgRGBAf(1.f, 0.5f, 0.2f, 0.f));
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, rr * 1.3f);
				nvgFillPaint(vg, gp);
				nvgFill(vg);
			}
			nvgBeginPath(vg);
			if (i == 0) nvgCircle(vg, x, y, rr);
			else nvgEllipse(vg, x, y, rr, rr * 0.82f);
			nvgStrokeColor(vg, nvgTransRGBA(soft, 90));
			nvgStrokeWidth(vg, 0.6f);
			nvgStroke(vg);
			if (font) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, mm2px(i == 0 ? 2.9f : 2.5f));
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, glow > 0.5f ? nvgRGB(30, 18, 10) : cream);
				std::string nm = string::f("%s%d", HG_NOTE_NAMES[((n.midi % 12) + 12) % 12], n.midi / 12 - 1);
				nvgText(vg, x, y, nm.c_str(), NULL);
			}
		}
		// the gu, marked under the shell
		if (font) {
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, mm2px(2.3f));
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(vg, nvgTransRGBA(copper, 200));
			nvgText(vg, mm2px(1.4f), h - mm2px(1.4f), string::f("gu %.0f Hz", e.helmFreq).c_str(), NULL);
		}

		// right side: scale, notes, dirt bars
		if (font) {
			float x0 = mm2px(55.f), right = w - mm2px(2.f);
			nvgFontFaceId(vg, font->handle);
			int sc = clamp(e.p.scale, 0, NUM_SCALES - 1);
			nvgFontSize(vg, mm2px(4.0f));
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(vg, cream);
			std::string title = string::f("%s %s", HG_NOTE_NAMES[clamp(e.p.root, 0, 11)], HG_SCALES[sc].name);
			nvgText(vg, x0, mm2px(5.6f), title.c_str(), NULL);
			nvgFontSize(vg, mm2px(2.4f));
			nvgFillColor(vg, soft);
			std::string notes;
			for (int i = 0; i < e.nNotes; i++) {
				const HgNote& n = e.note[i];
				notes += string::f("%s%d", HG_NOTE_NAMES[((n.midi % 12) + 12) % 12], n.midi / 12 - 1);
				notes += i == 0 ? " | " : " ";
			}
			nvgText(vg, x0, mm2px(9.6f), notes.c_str(), NULL);

			// dirt bars
			float master = module ? module->dispMaster : 0.5f;
			nvgFontSize(vg, mm2px(2.5f));
			nvgFillColor(vg, rust);
			nvgText(vg, x0, mm2px(15.4f), string::f("DIRT %d%%", (int) std::round(master * 200.f)).c_str(), NULL);
			const float defs[NUM_DIRT] = {0.3f, 0.3f, 0.2f, 0.3f, 0.3f, 0.3f, 0.2f, 0.25f};
			for (int i = 0; i < NUM_DIRT; i++) {
				float a = module ? module->dispDirt[i] : defs[i];
				float y = mm2px(19.6f + 3.55f * i);
				nvgFillColor(vg, soft);
				nvgFontSize(vg, mm2px(2.3f));
				nvgText(vg, x0, y, DIRT_NAMES[i], NULL);
				float bx = x0 + mm2px(13.f), bw = right - bx, bh = mm2px(1.3f);
				nvgBeginPath(vg);
				nvgRect(vg, bx, y - bh, bw, bh);
				nvgFillColor(vg, nvgRGBA(255, 255, 255, 18));
				nvgFill(vg);
				bool hot = (i == 2 && module && module->dispRattle > 0.05f);
				nvgBeginPath(vg);
				nvgRect(vg, bx, y - bh, bw * a, bh);
				nvgFillColor(vg, hot ? nvgRGB(255, 210, 120) : rust);
				nvgFill(vg);
			}
			// status
			std::string st;
			if (module && module->dispRoll) st += "ROLL  ";
			if (module && module->dispMute) st += "MUTED  ";
			nvgFontSize(vg, mm2px(2.5f));
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			nvgFillColor(vg, copper);
			nvgText(vg, right, h - mm2px(1.4f), st.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

struct HangoverWidget : ModuleWidget {
	HangoverWidget(Hangover* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Hangover.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		HangoverDisplay* d = createWidget<HangoverDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);

		const int ids[3][3] = {
			{Hangover::SCALE_PARAM, Hangover::KEY_PARAM, Hangover::DECAY_PARAM},
			{Hangover::FORCE_PARAM, Hangover::TOUCH_PARAM, Hangover::STRIKE_PARAM},
			{Hangover::MUTE_PARAM, Hangover::GU_PARAM, Hangover::ROLL_PARAM}};
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++) {
				if (r == 0 && c < 2) addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(MX[c], MY[r])), module, ids[r][c]));
				else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(MX[c], MY[r])), module, ids[r][c]));
			}

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(DIRT_MX, DIRT_Y)), module, Hangover::DIRT_PARAM));
		for (int i = 0; i < NUM_DIRT; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(DX0 + DDX * i, DIRT_Y)), module, Hangover::DIRTK_PARAM + i));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(NEW_X, DIRT_Y)), module, Hangover::NEW_PARAM));

		for (int i = 0; i < 9; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TRIG_X0 + TRIG_DX * i, ROWA_Y)), module, Hangover::TRIG_INPUT + i));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TRIG_X0 + TRIG_DX * 9, ROWA_Y)), module, Hangover::GU_INPUT));
		const int poly[5] = {Hangover::GATE_INPUT, Hangover::VOCT_INPUT, Hangover::VEL_INPUT, Hangover::MUTE_INPUT, Hangover::ROLL_INPUT};
		for (int i = 0; i < 5; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(POLY_X0 + POLY_DX * i, ROWA_Y)), module, poly[i]));
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CV_X0 + CV_DX * i, ROWB_Y)), module, Hangover::DECAY_CV_INPUT + i));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_X[0], ROWB_Y)), module, Hangover::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_X[1], ROWB_Y)), module, Hangover::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Hangover* m = getModule<Hangover>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("This hang: #%u", (unsigned) m->eng.seed)));
		menu->addChild(createMenuItem("New hang (re-roll its imperfections)", "", [=]() { m->eng.reseed(random::u32()); }));
		menu->addChild(createMenuItem("Set all dirt knobs to zero (a clean model)", "", [=]() {
			for (int i = 0; i < NUM_DIRT; i++) m->params[Hangover::DIRTK_PARAM + i].setValue(0.f);
		}));
		menu->addChild(createMenuItem("Stop everything ringing", "", [=]() { m->eng.silence(); }));
	}
};

Model* modelHangover = createModel<Hangover, HangoverWidget>("Hangover");
