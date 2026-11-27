# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard res/*.svg)
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += res/fonts

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAVs (no Rack needed): make test
test/voices: test/voices.cpp src/Engines909.hpp src/Common909.hpp src/VoiceSpecs.hpp
	$(CXX) -O2 -std=c++14 test/voices.cpp -o test/voices
test/bench: test/bench.cpp src/BenchVoices909.hpp src/Engines909.hpp src/Common909.hpp src/VoiceSpecs.hpp
	$(CXX) -O2 -std=c++14 test/bench.cpp -o test/bench
.PHONY: test
test: test/voices test/bench
	mkdir -p test/out && ./test/voices test/out && ./test/bench
