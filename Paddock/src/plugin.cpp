#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelSwerve);
	p->addModel(modelGallop);
	p->addModel(modelTumble);
}
