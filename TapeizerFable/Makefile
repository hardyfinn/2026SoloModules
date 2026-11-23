# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
# Demo renders: ./test/render test/out STEMDIR DEMODIR  (stems from tools/proto/export_stems.py)
test/render: test/render.cpp test/ref_data.h src/dsp/Tape.hpp src/dsp/Engine.hpp
	$(CXX) -O3 -std=c++17 test/render.cpp -o test/render
.PHONY: test
test: test/render
	mkdir -p test/out
	./test/render test/out
