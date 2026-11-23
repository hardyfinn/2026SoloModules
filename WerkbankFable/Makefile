# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + reference-case renders (no Rack needed): make test
test/render: test/render.cpp test/ref_data.h test/parse_cases.h $(wildcard src/dsp/*.hpp)
	$(CXX) -O3 -std=c++17 test/render.cpp -o test/render
.PHONY: test
test: test/render
	mkdir -p test/out
	./test/render test/out
