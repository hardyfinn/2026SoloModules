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
test/render: test/render.cpp src/KickEngine.hpp
	$(CXX) -O2 -std=c++11 test/render.cpp -o test/render
test/voices: test/voices.cpp src/VoiceEngines.hpp src/Common808.hpp src/VoiceSpecs.hpp
	$(CXX) -O2 -std=c++11 test/voices.cpp -o test/voices
test/bench: test/bench.cpp src/BenchVoices.hpp src/VoiceEngines.hpp src/KickEngine.hpp src/metal/MetalEngine.hpp
	$(CXX) -O2 -std=c++11 test/bench.cpp -o test/bench
.PHONY: test
test: test/render test/voices test/bench
	mkdir -p test/out && ./test/render test/out && ./test/voices test/out && ./test/bench
