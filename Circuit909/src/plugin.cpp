#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelKick909);
	p->addModel(modelSnare909);
	p->addModel(modelTom909);
	p->addModel(modelRim909);
	p->addModel(modelClap909);
	p->addModel(modelCymbals909);
}
