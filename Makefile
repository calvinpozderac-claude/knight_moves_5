# Linux / WSL2 / macOS build.  `make` -> build/az
CXX      ?= g++
CXXFLAGS ?= -O3 -march=native -ffast-math -std=c++17

all: build/az build/opening_search

build/az: src/az.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ src/az.cpp

clean:
	rm -rf build

.PHONY: all clean

build/opening_search: src/opening_search.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ src/opening_search.cpp
