// Graft: a drum groove (kick, snare, hat, perc) rewritten by chance rules.
// Every EVERY bars each step of the pattern is rewritten once, like one
// generation of a stochastic L-system:
//   RATCHET  a hit splits in two at half length (x -> xx), up to four per step
//   DROP     a hit disappears (the kick on the beat is never dropped)
//   SPROUT   a rest grows a hit, sometimes on the off half of the step
//   HOME     the step goes back to the seed groove, so it stays related
// MUTATE does one generation now; SEED goes back to the groove as written.
#include "plugin.hpp"
#include "LSys.hpp"
#include "Layout.hpp"

using namespace lsys;
using namespace graft;

static const int EVERY_VALUES[5] = {0, 1, 2, 4, 8};

struct GrooveQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return GRAFT_GROOVE_NAMES[clamp((int) std::round(getValue()), 0, GRAFT_GROOVES - 1)];
	}
};
struct EveryQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int e = EVERY_VALUES[clamp((int) std::round(getValue()), 0, 4)];
		return e ? string::f("every %d bar%s", e, e > 1 ? "s" : "") : "frozen";
	}
};

struct GraftModule : Module {
	enum ParamIds { GROOVE_PARAM, EVERY_PARAM, RATCHET_PARAM, DROP_PARAM, SPROUT_PARAM, HOME_PARAM, MUTATE_PARAM, SEED_PARAM, NUM_PARAMS };
	enum InputIds { CLOCK_INPUT, RESET_INPUT, MUTATE_INPUT, SEED_INPUT, NUM_INPUTS };
	enum OutputIds { KICK_OUTPUT, SNARE_OUTPUT, HAT_OUTPUT, PERC_OUTPUT, NUM_OUTPUTS };
	enum LightIds { MUTATE_LIGHT, SEED_LIGHT, NUM_LIGHTS };

	Graft gr;
	ClockTracker clk;
	dsp::SchmittTrigger clockTrig, resetTrig, mutTrig, seedTrig, mutBtn, seedBtn;
	dsp::PulseGenerator pulse[GRAFT_LANES];
	float flash = 0.f;
	int lastGen = 0;

	GraftModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<GrooveQuantity>(GROOVE_PARAM, 0.f, GRAFT_GROOVES - 1, 0.f, "Seed groove");
		configParam<EveryQuantity>(EVERY_PARAM, 0.f, 4.f, 1.f, "New generation");
		paramQuantities[GROOVE_PARAM]->snapEnabled = true;
		paramQuantities[EVERY_PARAM]->snapEnabled = true;
		configParam(RATCHET_PARAM, 0.f, 1.f, 0.25f, "Ratchet chance", "%", 0.f, 100.f);
		configParam(DROP_PARAM, 0.f, 1.f, 0.15f, "Drop chance", "%", 0.f, 100.f);
		configParam(SPROUT_PARAM, 0.f, 1.f, 0.1f, "Sprout chance", "%", 0.f, 100.f);
		configParam(HOME_PARAM, 0.f, 1.f, 0.2f, "Home chance (back to the seed)", "%", 0.f, 100.f);
		configButton(MUTATE_PARAM, "Mutate now");
		configButton(SEED_PARAM, "Back to the seed groove");
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(MUTATE_INPUT, "Mutate trigger");
		configInput(SEED_INPUT, "Seed trigger");
		configOutput(KICK_OUTPUT, "Kick trigger");
		configOutput(SNARE_OUTPUT, "Snare trigger");
		configOutput(HAT_OUTPUT, "Hat trigger");
		configOutput(PERC_OUTPUT, "Perc trigger");
		gr.rng = Rng((uint32_t) random::u32());
	}

	void process(const ProcessArgs& args) override {
		gr.groove = clamp((int) std::round(params[GROOVE_PARAM].getValue()), 0, GRAFT_GROOVES - 1);
		gr.every = EVERY_VALUES[clamp((int) std::round(params[EVERY_PARAM].getValue()), 0, 4)];
		gr.ratchet = params[RATCHET_PARAM].getValue();
		gr.drop = params[DROP_PARAM].getValue();
		gr.sprout = params[SPROUT_PARAM].getValue();
		gr.home = params[HOME_PARAM].getValue();
		if (gr.loadedGroove != gr.groove) gr.loadSeed();
		clk.tick(args.sampleTime);
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) gr.reset();
		bool m1 = mutTrig.process(inputs[MUTATE_INPUT].getVoltage(), 0.1f, 1.f);
		bool m2 = mutBtn.process(params[MUTATE_PARAM].getValue());
		if (m1 || m2) gr.mutate();
		bool s1 = seedTrig.process(inputs[SEED_INPUT].getVoltage(), 0.1f, 1.f);
		bool s2 = seedBtn.process(params[SEED_PARAM].getValue());
		if (s1 || s2) gr.goHome();
		bool edge = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		if (edge) clk.edge();
		gr.process(edge, clk);
		if (gr.generation != lastGen) { flash = 1.f; lastGen = gr.generation; }
		flash = std::max(0.f, flash - args.sampleTime * 6.f);
		for (int l = 0; l < GRAFT_LANES; l++) {
			if (gr.fired[l]) pulse[l].trigger(1e-3f);
			outputs[KICK_OUTPUT + l].setVoltage(pulse[l].process(args.sampleTime) ? 10.f : 0.f);
		}
		lights[MUTATE_LIGHT].setBrightness(flash);
		lights[SEED_LIGHT].setBrightness(gr.generation == 0 ? 0.6f : 0.f);
	}

	// keep the grown pattern with the patch
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "groove", json_integer(gr.loadedGroove));
		json_object_set_new(root, "generation", json_integer(gr.generation));
		std::string cells;
		for (int l = 0; l < GRAFT_LANES; l++)
			for (int s = 0; s < GRAFT_STEPS; s++)
				cells += string::f("%d%x", gr.cell[l][s].div, gr.cell[l][s].mask);
		json_object_set_new(root, "cells", json_string(cells.c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* g = json_object_get(root, "groove");
		json_t* c = json_object_get(root, "cells");
		if (!g || !c) return;
		gr.groove = (int) json_integer_value(g);
		gr.loadSeed();
		std::string cells = json_string_value(c);
		if (cells.size() == (size_t) (GRAFT_LANES * GRAFT_STEPS * 2))
			for (int l = 0; l < GRAFT_LANES; l++)
				for (int s = 0; s < GRAFT_STEPS; s++) {
					int i = 2 * (l * GRAFT_STEPS + s);
					gr.cell[l][s].div = (uint8_t) (cells[i] - '0');
					gr.cell[l][s].mask = (uint8_t) std::stoi(cells.substr(i + 1, 1), nullptr, 16);
				}
		json_t* n = json_object_get(root, "generation");
		if (n) gr.generation = lastGen = (int) json_integer_value(n);
	}
};

// Screen: 4 lanes x 16 steps; a step split in 2 or 4 shows its ratchet;
// steps that differ from the seed are amber.
struct GraftDisplay : Widget {
	GraftModule* module = nullptr;
	Graft demo;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		if (!module && demo.loadedGroove < 0) {
			demo.loadSeed();
			demo.rng = Rng(3);
			demo.mutate(); demo.mutate();
		}
		const Graft& g = module ? module->gr : demo;
		auto font = screenFont();
		float w = box.size.x, h = box.size.y, pad = mm2px(1.2f);
		float lab = mm2px(3.4f);
		float x0 = pad + lab, x1 = w - pad, y0 = mm2px(4.6f), y1 = h - pad;
		screenText(args.vg, font, pad, mm2px(3.4f), 2.6f, LEAF, GRAFT_GROOVE_NAMES[g.loadedGroove < 0 ? 0 : g.loadedGroove]);
		screenText(args.vg, font, w - pad, mm2px(3.4f), 2.6f, CREAM, string::f("GEN %d", g.generation), NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		const char* names = "KSHP";
		float rh = (y1 - y0) / GRAFT_LANES, cw = (x1 - x0) / GRAFT_STEPS;
		if (g.step >= 0) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x0 + g.step * cw, y0, cw, y1 - y0);
			nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 28));
			nvgFill(args.vg);
		}
		for (int l = 0; l < GRAFT_LANES; l++) {
			float y = y0 + l * rh;
			screenText(args.vg, font, pad, y + rh * 0.75f, 2.4f, DIM, std::string(1, names[l]));
			for (int s = 0; s < GRAFT_STEPS; s++) {
				const Cell& c = g.cell[l][s];
				bool changed = !(c == g.seed[l][s]);
				float x = x0 + s * cw;
				if (!c.mask) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x + cw * 0.4f, y + rh * 0.42f, cw * 0.2f, rh * 0.16f);
					nvgFillColor(args.vg, nvgRGBA(242, 236, 217, s % 4 == 0 ? 60 : 25));
					nvgFill(args.vg);
					continue;
				}
				for (int b = 0; b < c.div; b++) {
					if (!(c.mask & (1 << b))) continue;
					float sw = cw / c.div;
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x + b * sw + 0.5f, y + 1.f, std::max(0.8f, sw - 1.f), rh - 2.f);
					nvgFillColor(args.vg, changed ? AMBER : LEAF);
					nvgFill(args.vg);
				}
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct GraftWidget : ModuleWidget {
	GraftWidget(GraftModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Graft.svg")));
		screws(this);
		GraftDisplay* d = createWidget<GraftDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_GROOVE_X, K_GROOVE_Y)), module, GraftModule::GROOVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_EVERY_X, K_EVERY_Y)), module, GraftModule::EVERY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_RATCHET_X, K_RATCHET_Y)), module, GraftModule::RATCHET_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_DROP_X, K_DROP_Y)), module, GraftModule::DROP_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_SPROUT_X, K_SPROUT_Y)), module, GraftModule::SPROUT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_HOME_X, K_HOME_Y)), module, GraftModule::HOME_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(B_MUTATE_X, B_MUTATE_Y)), module, GraftModule::MUTATE_PARAM, GraftModule::MUTATE_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(B_SEED_X, B_SEED_Y)), module, GraftModule::SEED_PARAM, GraftModule::SEED_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_CLOCK_X, I_CLOCK_Y)), module, GraftModule::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_RESET_X, I_RESET_Y)), module, GraftModule::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_MUTATE_X, I_MUTATE_Y)), module, GraftModule::MUTATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_SEED_X, I_SEED_Y)), module, GraftModule::SEED_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_KICK_X, O_KICK_Y)), module, GraftModule::KICK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_SNARE_X, O_SNARE_Y)), module, GraftModule::SNARE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_HAT_X, O_HAT_Y)), module, GraftModule::HAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_PERC_X, O_PERC_Y)), module, GraftModule::PERC_OUTPUT));
	}
};

Model* modelGraft = createModel<GraftModule, GraftWidget>("Graft");
