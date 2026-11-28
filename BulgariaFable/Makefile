# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test demos test/test test/tool,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
ENGINE = src/dsp/BulgariaEngine.hpp
test/test: test/test.cpp $(ENGINE)
	$(CXX) -O2 -std=c++17 -Wall test/test.cpp -o test/test
test/tool: test/tool.cpp $(ENGINE)
	$(CXX) -O2 -std=c++17 -Wall test/tool.cpp -o test/tool
test/presets.txt: tools/make_presets.py
	python3 tools/make_presets.py
.PHONY: test demos
test: test/test
	./test/test
# Renders the demo MP3s (needs python3 + numpy + ffmpeg): pitch, gate and push from the C++ engine,
# played through the stand-in saw synth of tools/proto/bulgaria_proto.py.
demos: test/tool test/presets.txt
	mkdir -p test/demos
	python3 tools/make_demos.py test/demos
