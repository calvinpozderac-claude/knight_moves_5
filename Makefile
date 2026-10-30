# Linux / WSL2 / macOS build.  `make` -> build/az
CXX      ?= g++
CXXFLAGS ?= -O3 -march=native -ffast-math -std=c++17

all: build/az

build/az: src/az.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ src/az.cpp

clean:
	rm -rf build

.PHONY: all clean
