// Gatra: gamelan structure as a house / techno drum sequencer.
//   A straight 4/4 kick; the gong cycle (8-32 beats, polymeter against the bar)
//   drives drum roles; LAND moves the gong from the end of the cycle (gamelan)
//   through the "and" of 4 to the downbeat (techno); kotekan splits the hats
//   into two interlocking parts; swing; drop (angsel) and fill (kebyar).
// Engine: GatraCore in GamelanCore.hpp.
#include "plugin.hpp"
#include "GamelanCore.hpp"

using namespace irama;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float G_DISP_X = 6.0f, G_DISP_Y = 14.5f, G_DISP_W = 109.92f, G_DISP_H = 17.0f;
static const float G_COLX[4] = {16.f, 46.f, 76.f, 106.f};
static const float G_ROW1_Y = 46.0f, G_ROW2_Y = 65.0f, G_ROW3_Y = 79.5f;
static const float G_SWX[3] = {68.f, 87.f, 106.f};
static const float G_JX0 = 9.f, G_JDX = 14.85f, G_IN_Y = 93.0f;
static const float G_OX0 = 14.f, G_ODX = 23.48f, G_OA_Y = 106.0f, G_OB_Y = 119.5f;

struct CycleQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return string::f("%d beats", CYCLE_CHOICES[clamp((int)std::round(getValue()), 0, 6)]);
	}
};

struct Gatra : Module {
	enum ParamIds {
		CYCLE_PARAM, LAND_PARAM, DENSITY_PARAM, SWING_PARAM,
		MUTATE_PARAM, ANGSEL_PARAM, BEAT_PARAM, KOTEKAN_PARAM, KICK_PARAM,
		DROP_BUTTON, FILL_BUTTON,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT, RESET_INPUT, DROP_INPUT, FILL_INPUT, LAND_INPUT, DENSITY_INPUT, MUTATE_INPUT, CYCLE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		KICK_OUTPUT, GONG_OUTPUT, KENONG_OUTPUT, KEMPUL_OUTPUT, KETHUK_OUTPUT,
		KEMPYANG_OUTPUT, POLOS_OUTPUT, SANGSIH_OUTPUT, ACCENT_OUTPUT, ANGSEL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds { DROP_LIGHT, FILL_LIGHT, CLOCK_LIGHT, NUM_LIGHTS };

	GatraCore core;
	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator pulse[NUM_DRUM];
	GatraFrame pending;
	bool hasPending = false;
	double pendingLeft = 0.0;
	double sinceClock = 0.0, period = 0.0;
	bool angselGate = false;
	float clockLight = 0.f;

	Gatra() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<CycleQuantity>(CYCLE_PARAM, 0.f, 6.f, 2.f, "Gong cycle");
		getParamQuantity(CYCLE_PARAM)->snapEnabled = true;
		configParam(LAND_PARAM, 0.f, 1.f, 0.f, "Land (0 gong at the end, 0.5 on the and of 4, 1 on the downbeat)");
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.85f, "Kotekan density", "%", 0.f, 100.f);
		configParam(SWING_PARAM, 50.f, 75.f, 50.f, "Swing", "%");
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.2f, "Mutate (hat pattern per gong)", "%", 0.f, 100.f);
		configParam(ANGSEL_PARAM, 0.f, 1.f, 0.f, "Angsel chance (break before the gong)", "%", 0.f, 100.f);
		configSwitch(BEAT_PARAM, 0.f, 1.f, 0.f, "Gamelan beat", {"Quarter note", "Eighth note"});
		configSwitch(KOTEKAN_PARAM, 0.f, 2.f, 0.f, "Kotekan hats", {"Nyog cag (strict alternation)", "Telu (3 against 4)", "Empat (with shared hits)"});
		configSwitch(KICK_PARAM, 0.f, 1.f, 1.f, "Kick", {"Off", "Four on the floor"});
		configButton(DROP_BUTTON, "Drop (hold: break, release: gong on the next beat)");
		configButton(FILL_BUTTON, "Fill (hold: kebyar bursts)");

		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(DROP_INPUT, "Drop gate (e.g. Irama ANGSEL)");
		configInput(FILL_INPUT, "Fill gate");
		configInput(LAND_INPUT, "Land CV (0-10 V adds)");
		configInput(DENSITY_INPUT, "Density CV (±5 V)");
		configInput(MUTATE_INPUT, "Mutate CV (0-10 V adds)");
		configInput(CYCLE_INPUT, "Cycle CV (1 V per step)");

		configOutput(KICK_OUTPUT, "Kick (four on the floor)");
		configOutput(GONG_OUTPUT, "Gong (crash / big accent)");
		configOutput(KENONG_OUTPUT, "Kenong (clap)");
		configOutput(KEMPUL_OUTPUT, "Kempul (open hat / perc)");
		configOutput(KETHUK_OUTPUT, "Kethuk (off hat / perc)");
		configOutput(KEMPYANG_OUTPUT, "Kempyang (rim / shaker)");
		configOutput(POLOS_OUTPUT, "Polos (closed hat)");
		configOutput(SANGSIH_OUTPUT, "Sangsih (second hat / shaker)");
		configOutput(ACCENT_OUTPUT, "Accent (shared hits, gong)");
		configOutput(ANGSEL_OUTPUT, "Angsel gate (break in progress)");
		core.reseed(random::u32());
	}

	void onReset() override { core.reset(); }

	void fire(const GatraFrame& f) {
		for (int k = 0; k < NUM_DRUM; k++) if (f.trig[k]) pulse[k].trigger(k == D_GONG ? 0.02f : 0.005f);
	}

	void process(const ProcessArgs& args) override {
		int ci = (int)std::round(params[CYCLE_PARAM].getValue() + inputs[CYCLE_INPUT].getVoltage());
		core.cycle = CYCLE_CHOICES[clamp(ci, 0, 6)];
		core.land = clamp(params[LAND_PARAM].getValue() + inputs[LAND_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		core.density = clamp(params[DENSITY_PARAM].getValue() + inputs[DENSITY_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		core.mutate = clamp(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		core.angselChance = params[ANGSEL_PARAM].getValue();
		core.beatLen = params[BEAT_PARAM].getValue() > 0.5f ? 2 : 4;
		core.kotekan = clamp((int)std::round(params[KOTEKAN_PARAM].getValue()), 0, 2);
		core.kickOn = params[KICK_PARAM].getValue() > 0.5f;
		core.drop = params[DROP_BUTTON].getValue() > 0.5f || inputs[DROP_INPUT].getVoltage() >= 1.f;
		core.fill = params[FILL_BUTTON].getValue() > 0.5f || inputs[FILL_INPUT].getVoltage() >= 1.f;

		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) { core.reset(); hasPending = false; }

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (sinceClock > 0.0 && sinceClock < args.sampleRate * 4.0) period = sinceClock;
			sinceClock = 0.0;
			clockLight = 0.06f;
			if (hasPending) { fire(pending); hasPending = false; }
			GatraFrame f;
			core.clock(f);
			angselGate = f.angsel;
			float sw = params[SWING_PARAM].getValue() / 100.f;
			if (f.odd && sw > 0.505f && period > 0.0) {
				pending = f; hasPending = true;
				pendingLeft = (sw - 0.5f) * 2.0 * period;
			}
			else fire(f);
		}
		else sinceClock += 1.0;
		if (hasPending) {
			pendingLeft -= 1.0;
			if (pendingLeft <= 0.0) { fire(pending); hasPending = false; }
		}

		for (int k = 0; k < NUM_DRUM; k++) outputs[KICK_OUTPUT + k].setVoltage(pulse[k].process(args.sampleTime) ? 10.f : 0.f);
		outputs[ANGSEL_OUTPUT].setVoltage(angselGate ? 10.f : 0.f);

		clockLight = std::max(0.f, clockLight - args.sampleTime);
		lights[CLOCK_LIGHT].setBrightness(clockLight > 0.f ? 1.f : 0.f);
		lights[DROP_LIGHT].setBrightness(core.drop ? 1.f : (angselGate ? 0.4f : 0.f));
		lights[FILL_LIGHT].setBrightness(core.fill ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* d = json_array();
		for (int i = 0; i < 64; i++) json_array_append_new(d, json_real(core.dens[i]));
		json_object_set_new(root, "dens", d);
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* d = json_object_get(root, "dens");
		if (d) for (int i = 0; i < 64; i++) { json_t* v = json_array_get(d, i); if (v) core.dens[i] = (float)json_number_value(v); }
	}
};

// ---------------------------------------------------------------- display
struct GatraDisplay : TransparentWidget {
	Gatra* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		const float W = box.size.x, H = box.size.y;
		NVGcolor ink = nvgRGB(0xF3, 0xE6, 0xC8), gold = nvgRGB(0xE8, 0xB0, 0x40), dim = nvgRGBA(0xF3, 0xE6, 0xC8, 0x50), red = nvgRGB(0xE0, 0x5A, 0x3A);
		GatraCore demo;
		const GatraCore* c = module ? &module->core : &demo;

		static const char* KN[3] = {"NYOG CAG", "TELU", "EMPAT"};
		int sh = c->shift();
		std::string head = string::f("%d BEATS  %s", c->cycle, KN[c->kotekan]);
		nvgFontSize(args.vg, 10.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFillColor(args.vg, gold);
		nvgText(args.vg, 5, 3, head.c_str(), NULL);
		std::string lt = sh == 0 ? "GONG AT END" : (sh == c->beatLen ? "GONG ON THE ONE" : string::f("GONG +%d/16", sh));
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		nvgFillColor(args.vg, (module && module->angselGate) ? red : ink);
		nvgText(args.vg, W - 5, 3, (module && module->angselGate) ? "BREAK" : lt.c_str(), NULL);

		// the gong cycle as a strip of beats; bar lines where the kick's downbeats fall
		const int L = c->cycleLen();
		const int64_t p = c->pos < 0 ? 0 : c->pos;
		const int q = c->pos < 0 ? -1 : c->cyclePos(p);
		const int64_t start = p - (q < 0 ? 0 : q);
		const float x0 = 5, x1 = W - 5, y = H * 0.66f;
		const float cw = (x1 - x0) / L;
		for (int i = 0; i < L; i++) {
			float x = x0 + (i + 0.5f) * cw;
			if (((start + i) % 16 + 16) % 16 == 0) {
				nvgBeginPath(args.vg); nvgMoveTo(args.vg, x - cw * 0.5f, y - 6); nvgLineTo(args.vg, x - cw * 0.5f, y + 6);
				nvgStrokeColor(args.vg, dim); nvgStrokeWidth(args.vg, 1.f); nvgStroke(args.vg);
			}
			if (i % c->beatLen == 0) {
				int m = c->coloMask(i / c->beatLen + 1);
				float r = (m & C_GONG) ? 3.6f : (m & C_KENONG) ? 2.4f : (m & C_KEMPUL) ? 1.7f : 0.9f;
				nvgBeginPath(args.vg); nvgCircle(args.vg, x, y, r);
				nvgFillColor(args.vg, (m & C_GONG) ? gold : ink); nvgFill(args.vg);
			}
			if (i == q) {
				nvgBeginPath(args.vg); nvgRect(args.vg, x - cw * 0.5f, y + 7, std::max(cw, 1.5f), 2.f);
				nvgFillColor(args.vg, red); nvgFill(args.vg);
			}
		}
	}
};

struct GatraWidget : ModuleWidget {
	GatraWidget(Gatra* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gatra.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		GatraDisplay* d = createWidget<GatraDisplay>(mm2px(Vec(G_DISP_X, G_DISP_Y)));
		d->box.size = mm2px(Vec(G_DISP_W, G_DISP_H));
		d->module = module;
		addChild(d);

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(G_COLX[0], G_ROW1_Y)), module, Gatra::CYCLE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(G_COLX[1], G_ROW1_Y)), module, Gatra::LAND_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(G_COLX[2], G_ROW1_Y)), module, Gatra::DENSITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(G_COLX[3], G_ROW1_Y)), module, Gatra::SWING_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(G_COLX[0], G_ROW2_Y)), module, Gatra::MUTATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(G_COLX[1], G_ROW2_Y)), module, Gatra::ANGSEL_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(G_SWX[0], G_ROW2_Y)), module, Gatra::BEAT_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(G_SWX[1], G_ROW2_Y)), module, Gatra::KOTEKAN_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(G_SWX[2], G_ROW2_Y)), module, Gatra::KICK_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(G_COLX[0], G_ROW3_Y)), module, Gatra::DROP_BUTTON, Gatra::DROP_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(G_COLX[1], G_ROW3_Y)), module, Gatra::FILL_BUTTON, Gatra::FILL_LIGHT));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(G_COLX[3], G_ROW3_Y)), module, Gatra::CLOCK_LIGHT));

		for (int i = 0; i < 8; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G_JX0 + G_JDX * i, G_IN_Y)), module, Gatra::CLOCK_INPUT + i));
		for (int i = 0; i < 5; i++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(G_OX0 + G_ODX * i, G_OA_Y)), module, Gatra::KICK_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(G_OX0 + G_ODX * i, G_OB_Y)), module, Gatra::KEMPYANG_OUTPUT + i));
		}
	}
};

Model* modelGatra = createModel<Gatra, GatraWidget>("Gatra");
