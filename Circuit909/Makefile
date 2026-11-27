# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard res/*.svg)
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAVs (no Rack needed): make test
test/voices: test/voices.cpp src/Engines909.hpp src/Common909.hpp src/VoiceSpecs.hpp
	$(CXX) -O2 -std=c++11 test/voices.cpp -o test/voices
.PHONY: test
test: test/voices
	mkdir -p test/out && ./test/voices test/out
