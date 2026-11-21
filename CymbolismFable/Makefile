# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS += -O3
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res/Cymbolism.svg
DISTRIBUTABLES += $(wildcard LICENSE*)

ifeq ($(filter test test/%,$(MAKECMDGOALS)),)
include $(RACK_DIR)/plugin.mk
# the engine is C++17 (Rack's default is C++11)
CXXFLAGS := $(filter-out -std=c++11,$(CXXFLAGS)) -std=c++17
endif

# Offline checks and demo WAVs (no Rack needed): make test
TESTSRC = src/CyModes.cpp
test/render: test/render.cpp src/CyEngine.hpp src/CyModes.hpp src/CyModes.cpp src/CyGeometry.hpp
	$(CXX) -O2 -std=c++17 -pthread test/render.cpp $(TESTSRC) -o test/render
test/modes_check: test/modes_check.cpp src/CyModes.hpp src/CyModes.cpp src/CyGeometry.hpp
	$(CXX) -O2 -std=c++17 test/modes_check.cpp $(TESTSRC) -o test/modes_check
test/runner_check: test/runner_check.cpp src/CyRunner.hpp src/CyEngine.hpp src/CyModes.cpp
	$(CXX) -O2 -std=c++17 -pthread test/runner_check.cpp $(TESTSRC) -o test/runner_check
.PHONY: test
test: test/modes_check test/render test/runner_check
	./test/modes_check
	./test/runner_check
	./test/render .
