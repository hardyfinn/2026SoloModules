# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp) $(wildcard src/dsp/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(filter test presets,$(MAKECMDGOALS)),)
else
include $(RACK_DIR)/plugin.mk
endif

# Offline checks + demo WAVs (no Rack needed): make test
# The engine in src/dsp must be the reference in tools/proto, unchanged.
ENGINE_DEPS = src/dsp/birdie_ref.h src/dsp/birdie_ref.cpp src/dsp/BirdieCore.hpp src/dsp/presets.h
test/ref.o: tools/proto/birdie_ref.c tools/proto/birdie_ref.h
	$(CC) -O2 -std=c99 -c tools/proto/birdie_ref.c -o test/ref.o \
		-Dbd_init=ref_bd_init -Dbd_note=ref_bd_note -Dbd_phrase=ref_bd_phrase -Dbd_process=ref_bd_process
test/render: test/render.cpp test/ref.o $(ENGINE_DEPS)
	$(CXX) -O2 -std=c++17 test/render.cpp src/dsp/birdie_ref.cpp test/ref.o -o test/render
.PHONY: test presets
test: test/render
	cmp tools/proto/birdie_ref.c src/dsp/birdie_ref.cpp
	cmp tools/proto/birdie_ref.h src/dsp/birdie_ref.h
	cmp tools/proto/presets.h src/dsp/presets.h
	mkdir -p test/out
	./test/render test/out

# Writes presets/Birdie/*.vcvm from tools/proto/presets.h
presets: tools/export_presets.cpp $(ENGINE_DEPS)
	$(CXX) -O2 -std=c++17 tools/export_presets.cpp src/dsp/birdie_ref.cpp -o tools/export_presets
	./tools/export_presets presets/Birdie
