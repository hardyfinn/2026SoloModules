# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res/Upright.svg res/samples
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAVs (no Rack needed): make test
TESTFLAGS = -O3 -funsafe-math-optimizations -std=c++17
ifeq ($(shell uname -m),x86_64)
TESTFLAGS += -march=nehalem
endif
test/render: test/render.cpp src/UprightEngine.hpp src/stb_vorbis_impl.cpp
	$(CXX) $(TESTFLAGS) test/render.cpp src/stb_vorbis_impl.cpp -o test/render
.PHONY: test
test: test/render
	mkdir -p test/out && ./test/render test/out
