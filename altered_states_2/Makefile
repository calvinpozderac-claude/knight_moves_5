CXX      ?= g++
CXXFLAGS ?= -O3 -march=native -std=c++17 -pthread -Wall

build/as2: src/as2.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf build

.PHONY: clean
