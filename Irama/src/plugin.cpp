#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelIrama);
	p->addModel(modelGatra);
	p->addModel(modelAgeng);
	p->addModel(modelGangsa);
	p->addModel(modelReyong);
}
