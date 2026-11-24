# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
CXXFLAGS += -std=c++17
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res presets
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline checks + MIDI renders (no Rack needed): make test
test/render: test/render.cpp test/simulate.hpp test/golden.h src/dsp/Engine.hpp src/dsp/Host.hpp
	$(CXX) -O2 -std=c++17 test/render.cpp -o test/render
WALKER_DSP = src/dsp/WalkerEngine.hpp src/dsp/WalkerHost.hpp src/dsp/Upright.hpp src/dsp/Link.hpp src/dsp/Engine.hpp
test/render_walker: test/render_walker.cpp test/walker_sim.hpp test/walker_golden.h test/walker_presets.h $(WALKER_DSP)
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 test/render_walker.cpp -o test/render_walker
test/render_upright: test/render_upright.cpp test/upright_cases.h src/dsp/Upright.hpp src/dsp/Engine.hpp
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 test/render_upright.cpp -o test/render_upright
RIDER_DSP = src/dsp/RiderEngine.hpp src/dsp/RiderKit.hpp src/dsp/RiderKitData.hpp src/dsp/RiderHost.hpp src/dsp/Link.hpp src/dsp/Engine.hpp src/dsp/Host.hpp
test/render_rider: test/render_rider.cpp test/rider_golden.h test/rider_presets.h test/golden.h $(RIDER_DSP)
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 test/render_rider.cpp -o test/render_rider
test/render_rider_kit: test/render_rider_kit.cpp test/rider_kit_cases.h $(RIDER_DSP)
	$(CXX) -O3 -funsafe-math-optimizations -std=c++17 test/render_rider_kit.cpp -o test/render_rider_kit
.PHONY: test
test: test/render test/render_walker test/render_upright test/render_rider test/render_rider_kit
	mkdir -p test/out test/out/upright
	./test/render test/out
	./test/render_walker test/out
	./test/render_upright test/out/upright
	python3 tools/check_upright.py test/out/upright
	mkdir -p test/out/rider
	./test/render_rider test/out
	./test/render_rider_kit test/out/rider
