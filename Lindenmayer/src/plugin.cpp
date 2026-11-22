#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelSprout);
	p->addModel(modelGraft);
	p->addModel(modelBloom);
}
