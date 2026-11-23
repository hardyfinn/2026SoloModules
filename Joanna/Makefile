# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res/Joanna.svg
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAVs (no Rack needed): make test
# Built with Rack's own speed flags so the CPU check means something.
TESTFLAGS = -O3 -funsafe-math-optimizations -std=c++17
ifeq ($(shell uname -m),x86_64)
TESTFLAGS += -march=nehalem
endif
test/render: test/render.cpp src/JoannaEngine.hpp src/JoannaPresets.hpp
	$(CXX) $(TESTFLAGS) test/render.cpp -o test/render
.PHONY: test
test: test/render
	mkdir -p test/out && ./test/render test/out
