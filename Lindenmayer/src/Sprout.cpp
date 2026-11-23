// Sprout: a rhythm that grows by L-system rules.
// BAR mode: the current generation's string is spread evenly over one cycle
// (LENGTH clock pulses), so each generation packs more, finer hits into the
// same bar. GROW steps up one generation every N cycles and starts again from
// GEN when the string gets too dense. STREAM mode reads one symbol per clock
// from the endless word instead: it never loops but always sounds related.
// Each symbol (A, B, C) has its own trigger output.
#include "plugin.hpp"
#include "LSys.hpp"
#include "Layout.hpp"

using namespace lsys;
using namespace sprout;

static const int GROW_VALUES[5] = {0, 1, 2, 4, 8};

struct RuleQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int r = clamp((int) std::round(getValue()), 0, SPROUT_RULES - 1);
		return std::string(SPROUT_RULESETS[r].name) + "  " + SPROUT_RULESETS[r].text;
	}
};
struct GrowQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int g = GROW_VALUES[clamp((int) std::round(getValue()), 0, 4)];
		return g ? string::f("every %d cycle%s", g, g > 1 ? "s" : "") : "hold";
	}
};

struct SproutModule : Module {
	enum ParamIds { RULE_PARAM, GEN_PARAM, GROW_PARAM, LENGTH_PARAM, MODE_PARAM, QUANT_PARAM, NUM_PARAMS };
	enum InputIds { CLOCK_INPUT, RESET_INPUT, GEN_INPUT, NUM_INPUTS };
	enum OutputIds { A_OUTPUT, B_OUTPUT, C_OUTPUT, CYCLE_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	Sprout sp;
	ClockTracker clk;
	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator pulse[4];
	int lastGenKnob = -1;

	SproutModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<RuleQuantity>(RULE_PARAM, 0.f, SPROUT_RULES - 1, 0.f, "Rule");
		configParam(GEN_PARAM, 0.f, 12.f, 2.f, "Start generation");
		configParam<GrowQuantity>(GROW_PARAM, 0.f, 4.f, 2.f, "Grow");
		configParam(LENGTH_PARAM, 4.f, 32.f, 16.f, "Cycle length", " clocks");
		for (int p : {RULE_PARAM, GEN_PARAM, GROW_PARAM, LENGTH_PARAM}) paramQuantities[p]->snapEnabled = true;
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Mode", {"Bar: the generation fills one cycle", "Stream: one symbol per clock, endless"});
		configSwitch(QUANT_PARAM, 0.f, 1.f, 0.f, "Quantise", {"Off: hits land between clocks", "On: hits snap to the clock"});
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(GEN_INPUT, "Generation CV (1 V per generation, overrides GROW)");
		configOutput(A_OUTPUT, "A trigger");
		configOutput(B_OUTPUT, "B trigger");
		configOutput(C_OUTPUT, "C trigger");
		configOutput(CYCLE_OUTPUT, "Start of cycle");
	}

	void process(const ProcessArgs& args) override {
		sp.rule = clamp((int) std::round(params[RULE_PARAM].getValue()), 0, SPROUT_RULES - 1);
		sp.grow = GROW_VALUES[clamp((int) std::round(params[GROW_PARAM].getValue()), 0, 4)];
		sp.length = clamp((int) std::round(params[LENGTH_PARAM].getValue()), 4, 32);
		sp.stream = params[MODE_PARAM].getValue() > 0.5f;
		sp.quant = params[QUANT_PARAM].getValue() > 0.5f;
		int knob = (int) std::round(params[GEN_PARAM].getValue());
		sp.startGen = std::min(knob, sp.maxGen());
		if (knob != lastGenKnob) { sp.gen = sp.startGen; lastGenKnob = knob; }
		if (inputs[GEN_INPUT].isConnected()) {
			sp.gen = clamp(knob + (int) std::round(inputs[GEN_INPUT].getVoltage()), 0, sp.maxGen());
			sp.grow = 0;
		}
		clk.tick(args.sampleTime);
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) sp.reset();
		bool edge = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		if (edge) clk.edge();
		sp.process(edge, clk);
		for (int i = 0; i < 3; i++) if (sp.fired[i]) pulse[i].trigger(1e-3f);
		if (sp.cycleStart) pulse[3].trigger(1e-3f);
		for (int i = 0; i < 4; i++) outputs[A_OUTPUT + i].setVoltage(pulse[i].process(args.sampleTime) ? 10.f : 0.f);
	}
};

// Screen: the rule, then the generation as a strip of cells across one cycle.
struct SproutDisplay : Widget {
	SproutModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		Sprout demo;
		const Sprout& s = module ? module->sp : demo;
		auto font = screenFont();
		float w = box.size.x, h = box.size.y, pad = mm2px(1.4f);
		screenText(args.vg, font, pad, mm2px(3.6f), 2.8f, LEAF, SPROUT_RULESETS[s.rule].name);
		screenText(args.vg, font, w - pad, mm2px(3.6f), 2.5f, DIM, SPROUT_RULESETS[s.rule].text, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		const std::vector<uint8_t>& word = s.current();
		int L = (int) word.size(), first = 0, n = L;
		if (s.stream) { n = 32; first = std::max(0, s.lastIndex - 8); }
		float x0 = pad, x1 = w - pad, y0 = mm2px(6.0f), y1 = h - mm2px(5.4f);
		const NVGcolor cols[3] = {LEAF, CREAM, AMBER};
		for (int i = 0; i < n; i++) {
			int k = (first + i) % std::max(L, 1);
			float a = x0 + (x1 - x0) * i / n, b = x0 + (x1 - x0) * (i + 1) / n;
			int sym = word[k];
			float top = y0 + (y1 - y0) * (sym == 0 ? 0.f : sym == 1 ? 0.45f : 0.25f);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, a + 0.4f, top, std::max(0.8f, b - a - 0.8f), y1 - top);
			bool now = module && k == s.lastIndex;
			nvgFillColor(args.vg, now ? cols[sym] : nvgTransRGBA(cols[sym], 90));
			nvgFill(args.vg);
		}
		std::string info = s.stream ? string::f("STREAM  symbol %d", s.lastIndex + 1)
		                            : string::f("GEN %d  %d per cycle", s.gen, L);
		if (!s.stream && s.grow) info += string::f("  grows /%d", s.grow);
		screenText(args.vg, font, pad, h - mm2px(1.2f), 2.6f, CREAM, info);
		Widget::drawLayer(args, layer);
	}
};

struct SproutWidget : ModuleWidget {
	SproutWidget(SproutModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Sprout.svg")));
		screws(this);
		SproutDisplay* d = createWidget<SproutDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_RULE_X, K_RULE_Y)), module, SproutModule::RULE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_GEN_X, K_GEN_Y)), module, SproutModule::GEN_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_GROW_X, K_GROW_Y)), module, SproutModule::GROW_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_LENGTH_X, K_LENGTH_Y)), module, SproutModule::LENGTH_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_MODE_X, S_MODE_Y)), module, SproutModule::MODE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_QUANT_X, S_QUANT_Y)), module, SproutModule::QUANT_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_CLOCK_X, I_CLOCK_Y)), module, SproutModule::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_RESET_X, I_RESET_Y)), module, SproutModule::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_GEN_CV_X, I_GEN_CV_Y)), module, SproutModule::GEN_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_A_X, O_A_Y)), module, SproutModule::A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_B_X, O_B_Y)), module, SproutModule::B_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_C_X, O_C_Y)), module, SproutModule::C_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_CYCLE_X, O_CYCLE_Y)), module, SproutModule::CYCLE_OUTPUT));
	}
};

Model* modelSprout = createModel<SproutModule, SproutWidget>("Sprout");
