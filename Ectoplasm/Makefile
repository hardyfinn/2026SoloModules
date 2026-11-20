# Ectoplasm. Based on rack/Makefile from github.com/schollz/_core (GPL-3.0-only).
# The engine is the Ectocore firmware itself: engine/lib/core_engine/prepare.py
# turns its C headers into one desktop translation unit, using Clang's AST.
# Needs: python3 (no extra packages) and clang (Xcode's on macOS).
RACK_DIR ?= ../Rack-SDK
PYTHON ?= python3

SOURCES += $(wildcard src/*.cpp)
OBJECTS += build/core-engine.o build/card-format.o build/pcg.o
DISTRIBUTABLES += res README.md LICENSE.txt licenses
EXTRA_CXXFLAGS += -std=c++17
EXTRA_FLAGS += -fwrapv -ffp-contract=off -fno-unsafe-math-optimizations

include $(RACK_DIR)/plugin.mk

CLANG ?= $(if $(ARCH_MAC),$(CC),clang)

# std::filesystem needs macOS 10.15 (Intel) / 11.0 (Apple Silicon).
ifdef ARCH_MAC
ifdef ARCH_ARM64
EXTRA_FLAGS += -mmacosx-version-min=11.0
else
EXTRA_FLAGS += -mmacosx-version-min=10.15
endif
endif

ENGINE_SOURCES := $(wildcard engine/lib/*.h) engine/lib/fuzz.py engine/lib/resonantfilter.py engine/lib/crossfade4_441.pregenerated.h $(wildcard engine/lib/core_engine/*) engine/main.c

build/generated/engine.c: $(ENGINE_SOURCES)
	$(PYTHON) engine/lib/core_engine/prepare.py --out build/generated --clang $(CLANG)

build/core-engine.o: build/generated/engine.c
	$(CLANG) $(filter-out -fno-gnu-unique,$(CFLAGS)) -std=c11 -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-format -c $< -o $@

build/card-format.o: engine/lib/core_engine/card_format.c engine/lib/core_engine/card_format.h
	mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@

build/pcg.o: engine/lib/pcg_basic.c
	mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@
