# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Offline engine checks (no Rack needed): make test
test/check: test/check.cpp src/DeidreEngine.hpp
	$(CXX) -O2 -std=c++11 test/check.cpp -o test/check
.PHONY: test
test: test/check
	./test/check
