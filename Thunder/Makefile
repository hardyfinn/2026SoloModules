# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard res/*.svg)
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAV (no Rack needed): make test
test/render: test/render.cpp src/ThunderEngine.hpp
	$(CXX) -O2 -std=c++17 test/render.cpp -o test/render
test/lightning: test/lightning.cpp src/ThunderEngine.hpp src/LightningCore.hpp
	$(CXX) -O2 -std=c++17 test/lightning.cpp -o test/lightning
.PHONY: test
test: test/render test/lightning
	mkdir -p test/out && ./test/render test/out && ./test/lightning test/out
