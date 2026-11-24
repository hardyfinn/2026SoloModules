#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelEssphor);
	p->addModel(modelEssphorRes);
	p->addModel(modelEssphorMod);
}
