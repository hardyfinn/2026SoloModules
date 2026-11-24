#pragma once
#include <rack.hpp>
#include <functional>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelEssphor;
extern Model* modelEssphorRes;
extern Model* modelEssphorMod;

// ---------------------------------------------------------------- shared widgets

// A small button that steps a switch parameter through its values (right-click lists them).
struct CycleButton : SvgSwitch {
	CycleButton() {
		momentary = false;
		addFrame(Svg::load(asset::system("res/ComponentLibrary/TL1105_0.svg")));
	}
};

struct AmberLightBase : GrayModuleLightWidget {
	AmberLightBase() { addBaseColor(nvgRGB(224, 176, 74)); }
};
struct LatchButton : VCVLightBezel<AmberLightBase> {
	LatchButton() { momentary = false; }
};
struct HoldButton : VCVLightBezel<AmberLightBase> {
	HoldButton() { momentary = true; }
};

// Text in a dark window, drawn on the light layer so it stays readable with the room lights down.
struct TextDisplay : Widget {
	std::function<std::string()> text;
	float fontSize = 10.f;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || !text) return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		std::string s = text();
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextLetterSpacing(args.vg, 0.3f);
		nvgFillColor(args.vg, nvgRGB(242, 192, 96));
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, box.size.x / 2, box.size.y / 2 + 0.5f, s.c_str(), NULL);
	}
};

inline TextDisplay* makeDisplay(float cx, float cy, float w, float h, std::function<std::string()> f, float size = 10.f) {
	TextDisplay* d = new TextDisplay;
	d->box.pos = mm2px(Vec(cx - w / 2, cy - h / 2));
	d->box.size = mm2px(Vec(w, h));
	d->text = f;
	d->fontSize = size;
	return d;
}

// A parameter quantity whose display text comes from a function (mode-dependent knobs).
struct FnQuantity : ParamQuantity {
	std::function<std::string(float)> fn;
	std::string getDisplayValueString() override { return fn ? fn(getValue()) : ParamQuantity::getDisplayValueString(); }
	std::string getUnit() override { return fn ? "" : ParamQuantity::getUnit(); }
};

inline std::string fmtSeconds(double s) {
	if (s < 1.0) return string::f("%.1f ms", s * 1000.0);
	return string::f("%.2f s", s);
}
inline std::string fmtHz(double hz) {
	if (hz < 1.0) return string::f("%.3f Hz", hz);
	if (hz < 100.0) return string::f("%.2f Hz", hz);
	return string::f("%.1f Hz", hz);
}

// Clock pulses-per-beat submenu, shared by Essphor and Essphor Mod.
inline MenuItem* ppqnMenu(int* ppqn) {
	static const int V[4] = {1, 2, 4, 24};
	return createIndexSubmenuItem("Clock input", {"1 pulse per beat", "2 pulses per beat", "4 pulses per beat", "24 pulses per beat"},
		[=]() { for (int i = 0; i < 4; i++) if (*ppqn == V[i]) return (size_t)i; return (size_t)0; },
		[=](size_t i) { *ppqn = V[std::min<size_t>(i, 3)]; });
}
