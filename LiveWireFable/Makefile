# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test demos tune test/render,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
# Test 2 compares the C++ engine with the Fable reference (tools/proto/livewire_ref.c), rendered here once.
# Both are built at -O2 without fast-math so they can agree sample for sample.
ENGINE = src/dsp/LiveWireEngine.hpp src/dsp/Mapping.hpp src/dsp/Voice.hpp tools/proto/mapping.h tools/proto/scenes.h
test/lwref: tools/proto/livewire_ref.c tools/proto/mapping.h tools/proto/scenes.h
	$(CC) -O2 tools/proto/livewire_ref.c -o test/lwref -lm
test/ref/%.f32: test/lwref
	mkdir -p test/ref
	./test/lwref $* $@ 2> /dev/null
test/render: test/render.cpp $(ENGINE)
	$(CXX) -O2 -std=c++17 -pthread -Wno-class-memaccess test/render.cpp -o test/render
test/presets.txt: tools/make_presets.py
	python3 tools/make_presets.py
.PHONY: test demos tune
test: test/render test/presets.txt test/ref/arc.f32 test/ref/kit2.f32
	mkdir -p test/out
	./test/render test test/out test/ref
tune: test/render test/presets.txt
	./test/render tune
demos: test/render test/presets.txt
	mkdir -p test/demos
	./test/render demos test/demos
