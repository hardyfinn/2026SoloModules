# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
ifneq ($(MAKECMDGOALS),demos)
include $(RACK_DIR)/plugin.mk
endif
endif

# Offline engine checks (no Rack needed): make test
# Demo renders from the C++ engines into test/out: make demos
ENGINE := src/dsp/Common.hpp src/dsp/Grains.hpp src/dsp/Res.hpp src/dsp/Mod.hpp src/dsp/Wav.hpp
test/render: test/render.cpp test/noage.cpp test/noage.hpp test/demos.hpp test/presets.h $(ENGINE)
	$(CXX) -O2 -std=c++17 -Wall test/render.cpp test/noage.cpp -o test/render
.PHONY: test demos
test: test/render
	mkdir -p test/out
	./test/render test/out
demos: test/render
	mkdir -p test/out
	./test/render test/out --demos
