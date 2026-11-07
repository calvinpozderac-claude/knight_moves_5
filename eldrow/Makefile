CXXFLAGS ?= -O3 -march=native -std=c++17

all: search check

search: search.cpp eldrow.hpp
	$(CXX) $(CXXFLAGS) -o $@ search.cpp

check: check.cpp eldrow.hpp
	$(CXX) $(CXXFLAGS) -o $@ check.cpp

clean:
	rm -f search check

.PHONY: all clean
