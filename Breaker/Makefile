# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test check,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Demo WAV + check from the real engine (no Rack needed): make test
test/render: test/render.cpp test/synth_break.hpp src/Engine.hpp src/Analysis.hpp
	$(CXX) -O2 -std=c++11 test/render.cpp -o test/render
.PHONY: test
test: test/render
	mkdir -p test/out && ./test/render

# Analyser check on made-up breaks: make check
test/analyse_check: test/analyse_check.cpp test/synth_break.hpp src/Analysis.hpp
	$(CXX) -O2 -std=c++11 test/analyse_check.cpp -o test/analyse_check
.PHONY: check
check: test/analyse_check
	./test/analyse_check
