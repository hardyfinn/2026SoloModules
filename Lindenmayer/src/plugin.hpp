#pragma once
#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelSprout;
extern Model* modelGraft;
extern Model* modelBloom;

// Shared look for the three screens.
static const NVGcolor LEAF = nvgRGB(155, 215, 112);
static const NVGcolor CREAM = nvgRGB(242, 236, 217);
static const NVGcolor AMBER = nvgRGB(255, 170, 80);
static const NVGcolor DIM = nvgRGBA(242, 236, 217, 70);

inline std::shared_ptr<window::Font> screenFont() {
	return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
}
inline void screenText(NVGcontext* vg, std::shared_ptr<window::Font> f, float x, float y, float sizeMm, NVGcolor c,
                       const std::string& s, int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE) {
	if (!f) return;
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextAlign(vg, align);
	nvgFillColor(vg, c);
	nvgText(vg, x, y, s.c_str(), NULL);
}
inline void screws(ModuleWidget* w) {
	w->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewSilver>(Vec(w->box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	w->addChild(createWidget<ScrewSilver>(Vec(w->box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
