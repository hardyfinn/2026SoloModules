#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelDatamatic);
	p->addModel(modelTestPattern);
}
