# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS += -Isrc/third_party/ymfm
SOURCES += $(wildcard src/*.cpp) $(wildcard src/dsp/*.cpp) $(wildcard src/third_party/ymfm/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

HOST_GOALS := test trims demos early hktest
ifeq ($(filter $(HOST_GOALS),$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine tests, loudness trims and demo WAVs (no Rack needed)
HOST_SRC := $(wildcard src/dsp/*.cpp) $(wildcard src/third_party/ymfm/*.cpp) test/hktest.cpp
HOST_OBJ := $(patsubst %.cpp,build-host/%.o,$(HOST_SRC))
build-host/%.o: %.cpp $(wildcard src/dsp/*.hpp) src/generated/Sounds.hpp src/generated/Trims.hpp
	@mkdir -p $(dir $@)
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 -Isrc/third_party/ymfm -c $< -o $@
test/hktest: $(HOST_OBJ)
	$(CXX) $(HOST_OBJ) -o $@ -lpthread
.PHONY: test trims demos early hktest
hktest: test/hktest
test: test/hktest
	./test/hktest test
trims: test/hktest
	./test/hktest trims src/generated/Trims.hpp
demos: test/hktest
	@mkdir -p demos
	./test/hktest demos demos
early: test/hktest
	@mkdir -p demos
	./test/hktest early demos
