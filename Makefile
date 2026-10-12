CXX ?= c++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -Wpedantic -O2 -pthread -Iinclude
HEADERS = $(wildcard include/*.hpp include/*.h)
all: build/bridge
build:
	mkdir -p build
build/bridge: src/main.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@
build/test: tests/test.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) tests/test.cpp -o $@
test: build/test
	./build/test
clean:
	rm -rf build
.PHONY: all test clean

FQBN ?= esp32:esp32:esp32
firmware:
	arduino-cli compile --fqbn $(FQBN) firmware/inventory_node
.PHONY: firmware

build/integration: tests/integration.cpp include/wire.h | build
	$(CXX) $(CXXFLAGS) tests/integration.cpp -o $@
integration: all build/integration
	./build/integration

.PHONY: integration
