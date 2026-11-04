# Linux / WSL2 / macOS build.  `make` -> build/az
CXX      ?= g++
CXXFLAGS ?= -O3 -march=native -ffast-math -std=c++17

all: build/az build/opening_search build/nrpa build/libnnastar.so

build/az: src/az.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ src/az.cpp

clean:
	rm -rf build

.PHONY: all clean

build/opening_search: src/opening_search.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ src/opening_search.cpp

build/nrpa: src/nrpa.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread -o $@ src/nrpa.cpp

build/libnnastar.so: src/nnastar.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -fPIC -shared -pthread -o $@ src/nnastar.cpp

build/heur_search: src/heur_search.cpp src/eng.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread -o $@ src/heur_search.cpp

build/reach_lab: src/reach_lab.cpp src/heur_search.cpp src/eng.h
	g++ -O3 -march=native -std=c++17 -pthread -o $@ src/reach_lab.cpp
