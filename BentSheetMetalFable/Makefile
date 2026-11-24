# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test demos test/render,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
ENGINE = src/dsp/SheetEngine.hpp src/dsp/Mapping.hpp test/bridge.h tools/proto/scenes.h
# Test 1 compares against the Fable reference engine, rendered here once (a few minutes, in parallel).
SCENES = saw flex dip break alive
REFS = $(foreach s,$(SCENES),test/ref/raw-$(s).f32) $(foreach s,$(SCENES),test/ref2/raw-$(s).f32)
test/bsref: tools/proto/bent_ref.c tools/proto/scenes.h
	$(CC) -O3 -ffast-math tools/proto/bent_ref.c -o test/bsref -lm
test/bsref2: tools/proto/bent_ref.c tools/proto/scenes.h
	$(CC) -O2 tools/proto/bent_ref.c -o test/bsref2 -lm
test/ref2/raw-%.f32: test/bsref2
	mkdir -p test/ref2
	cd test/ref2 && ../bsref2 $* 2> /dev/null
test/ref/raw-%.f32: test/bsref
	mkdir -p test/ref
	cd test/ref && ../bsref $* 2> /dev/null
test/render: test/render.cpp $(ENGINE)
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 -pthread test/render.cpp -o test/render
.PHONY: test demos
test: test/render $(REFS) test/presets.txt
	mkdir -p test/out
	./test/render test/out test/ref
test/presets.txt: tools/make_presets.py
	python3 tools/make_presets.py
demos: test/render test/presets.txt
	mkdir -p test/demos
	./test/render test/demos demos
