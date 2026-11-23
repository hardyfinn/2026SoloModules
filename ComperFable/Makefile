# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
CXXFLAGS += -std=c++17
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline checks + MIDI renders (no Rack needed): make test
test/render: test/render.cpp test/simulate.hpp test/golden.h src/dsp/Engine.hpp src/dsp/Host.hpp
	$(CXX) -O2 -std=c++17 test/render.cpp -o test/render
.PHONY: test
test: test/render
	mkdir -p test/out
	./test/render test/out
