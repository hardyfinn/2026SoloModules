# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
CXXFLAGS += -std=c++17
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline checks (no Rack needed): make test
test/render_trio: test/render_trio.cpp $(wildcard src/dsp/*.hpp)
	$(CXX) -O2 -std=c++17 -Wall test/render_trio.cpp -o test/render_trio
.PHONY: test
test: test/render_trio
	./test/render_trio
