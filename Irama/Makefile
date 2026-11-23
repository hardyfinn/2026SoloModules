# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test test-instruments,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks + demo WAV (no Rack needed): make test
test/render: test/render.cpp src/GamelanCore.hpp
	$(CXX) -O2 -std=c++11 test/render.cpp -o test/render
.PHONY: test
test: test/render
	./test/render .

# Instrument engine checks + demo (Ageng, Gangsa, Reyong): make test-instruments
test/instruments: test/instruments.cpp src/Instruments.hpp src/BronzeDSP.hpp src/GamelanCore.hpp
	$(CXX) -O2 -std=c++11 test/instruments.cpp -o test/instruments
.PHONY: test-instruments
test-instruments: test/instruments
	./test/instruments .
