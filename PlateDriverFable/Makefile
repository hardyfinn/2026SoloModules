# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
ENGINE = src/dsp/PlateEngine.hpp src/dsp/Mapping.hpp
# Test 1 compares against the Fable reference engine, rendered here once (about 1 minute).
REFS = test/ref/raw-swell.f32 test/ref/raw-h-stages.f32 test/ref/raw-h-pump.f32 test/ref/raw-h-pulse.f32
test/pdref: tools/proto/plate_driver_ref.c
	$(CC) -O2 -ffast-math tools/proto/plate_driver_ref.c -o test/pdref -lm
test/ref/raw-%.f32: test/pdref
	mkdir -p test/ref
	cd test/ref && ../pdref $* > /dev/null
test/render: test/render.cpp test/scenes.h $(ENGINE)
	$(CXX) -O3 -std=c++17 -pthread test/render.cpp -o test/render
.PHONY: test
test: test/render $(REFS)
	mkdir -p test/out
	./test/render test/out test/ref
