#pragma once
#include <rack.hpp>
#include "layout.hpp"

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelTakt;
extern Model* modelBlech;
extern Model* modelZap;
extern Model* modelVokoder;
extern Model* modelSprecher;
extern Model* modelOrchester;
extern Model* modelDynamo;

namespace wbui {

inline Vec mm(const layout::P& p) { return mm2px(Vec(p.x, p.y)); }

// the panel accent colour
struct OrangeLight : GrayModuleLightWidget {
	OrangeLight() { addBaseColor(nvgRGB(232, 115, 58)); }
};

// BLECH pad: a momentary metal plate
struct Pad : app::SvgSwitch {
	Pad() {
		momentary = true;
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/Pad_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/Pad_1.svg")));
	}
};

// 5 ms one-pole knob smoothing; the first call jumps to the value
struct Smooth {
	double v = 0;
	bool init = false;
	double process(double x, double a) {
		if (!init) { v = x; init = true; }
		else v += a * (x - v);
		return v;
	}
};
inline double smoothCoef(float sampleTime) { return 1.0 - std::exp(-(double)sampleTime / 0.005); }

// the unit seed, saved in the patch as hex
inline json_t* seedToJson(uint64_t seed) { return json_string(string::f("%016llx", (unsigned long long)seed).c_str()); }
inline bool seedFromJson(json_t* root, uint64_t& seed) {
	json_t* s = json_object_get(root, "seed");
	if (s && json_is_string(s)) { seed = std::strtoull(json_string_value(s), nullptr, 16); return true; }
	return false;
}
inline void appendUnitMenu(Menu* menu, uint64_t seed, std::function<void()> renew) {
	menu->addChild(new MenuSeparator);
	menu->addChild(createMenuLabel(string::f("Unit %08llx", (unsigned long long)(seed >> 32))));
	menu->addChild(createMenuItem("New unit", "", renew));
}

inline void addScrews(ModuleWidget* w) {
	w->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewSilver>(Vec(w->box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	w->addChild(createWidget<ScrewSilver>(Vec(w->box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

// exponential knob 0..1 -> lo..hi
inline double expMap(double k, double lo, double hi) { return lo * std::pow(hi / lo, k); }

inline std::string timeString(double t) {
	if (t < 1.0) return string::f("%.1f ms", t * 1000.0);
	return string::f("%.2f s", t);
}

// a 0..1 knob that displays an exponential time
template <int LO_US, int HI_US>
struct ExpTimeQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return timeString(expMap(clamp(getValue(), 0.f, 1.f), LO_US * 1e-6, HI_US * 1e-6));
	}
	void setDisplayValueString(std::string s) override {
		double t = std::atof(s.c_str());
		if (s.find("ms") != std::string::npos) t *= 1e-3;
		if (t <= 0) return;
		double k = std::log(t / (LO_US * 1e-6)) / std::log((double)HI_US / LO_US);
		setValue(clamp((float)k, 0.f, 1.f));
	}
};

inline float outClamp(double v) { return (float)std::min(std::max(v, -11.0), 11.0); }

}  // namespace wbui
