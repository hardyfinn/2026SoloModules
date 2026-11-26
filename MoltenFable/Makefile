# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test demos,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
ENGINE = src/dsp/MoltenEngine.hpp src/dsp/Mapping.hpp
# Test 1 compares against the Fable reference engine, rendered here once.
REFS = test/ref/raw-heat.f32 test/ref/raw-cast.f32 test/ref/raw-flame.f32 test/ref/raw-pushed.f32
test/mref: tools/proto/molten_ref.c tools/proto/scenes.h
	$(CC) -O3 -ffast-math tools/proto/molten_ref.c -o test/mref -lm
test/ref/raw-%.f32: test/mref
	mkdir -p test/ref
	cd test/ref && ../mref $* > /dev/null 2>&1
# The same reference without -ffast-math: how far the reference differs from itself once its chaos parts
# (test 1 holds the port to 1.5 times that).
REFS2 = test/ref2/raw-heat.f32 test/ref2/raw-cast.f32 test/ref2/raw-flame.f32 test/ref2/raw-pushed.f32
test/mref2: tools/proto/molten_ref.c tools/proto/scenes.h
	$(CC) -O3 tools/proto/molten_ref.c -o test/mref2 -lm
test/ref2/raw-%.f32: test/mref2
	mkdir -p test/ref2
	cd test/ref2 && ../mref2 $* > /dev/null 2>&1
test/render: test/render.cpp test/scenes.h $(ENGINE)
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 -pthread test/render.cpp -o test/render
.PHONY: test demos
test: test/render $(REFS) $(REFS2)
	mkdir -p test/out
	./test/render test/out test/ref
demos: test/render
	mkdir -p test/demos
	./test/render test/demos demos
