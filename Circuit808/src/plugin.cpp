#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelKick808);
	p->addModel(modelSnare808);
	p->addModel(modelClap808);
	p->addModel(modelTomConga808);
	p->addModel(modelRim808);
	p->addModel(modelClave808);
	p->addModel(modelMaracas808);
	p->addModel(modelCowbell808);
	p->addModel(modelBench808);
}
