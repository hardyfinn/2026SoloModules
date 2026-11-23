# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += grooves
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
test/engine_test: test/engine_test.cpp src/GrooverDSP.hpp
	$(CXX) -O2 -std=c++17 test/engine_test.cpp -o test/engine_test
.PHONY: test
test: test/engine_test
	./test/engine_test grooves/*.agr
