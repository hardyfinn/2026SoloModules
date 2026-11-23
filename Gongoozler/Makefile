# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test demo,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAV (no Rack needed): make test
test/render: test/render.cpp src/GongEngine.hpp
	$(CXX) -O2 -std=c++17 test/render.cpp -o test/render
.PHONY: test
test: test/render
	./test/render .

# Demo WAVs (one per starting point + test/demo.cpp's short piece): make demo
test/demo: test/demo.cpp src/GongEngine.hpp
	$(CXX) -O2 -std=c++17 test/demo.cpp -o test/demo
.PHONY: demo
demo: test/render test/demo
	./test/render . wav
	./test/demo gongoozler-demo.wav
