# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test demos test/%,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
ENGINE = src/dsp/LineEngine.hpp src/dsp/Mapping.hpp test/bridge.h tools/proto/scenes.h
# Test 1 compares against the Fable reference engine, rendered here once.
SCENES = rail zaps hard live live2
REFS = $(foreach s,$(SCENES),test/ref/$(s).f32)
test/lmref: tools/proto/longmetal_ref.c tools/proto/scenes.h
	$(CC) -O2 tools/proto/longmetal_ref.c -o test/lmref -lm
test/ref/%.f32: test/lmref
	mkdir -p test/ref
	./test/lmref $* $@ 2> /dev/null
test/render: test/render.cpp $(ENGINE)
	$(CXX) -O3 -ffast-math -std=c++17 -pthread test/render.cpp -o test/render
.PHONY: test demos
test: test/render $(REFS) test/presets.txt
	mkdir -p test/out
	./test/render test/out test/ref
test/presets.txt: tools/make_presets.py
	python3 tools/make_presets.py
demos: test/render test/presets.txt
	mkdir -p test/demos
	./test/render test/demos demos
