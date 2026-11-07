CXXFLAGS ?= -O3 -march=native -std=c++17

all: search check exhaust

search: search.cpp eldrow.hpp
	$(CXX) $(CXXFLAGS) -o $@ search.cpp

check: check.cpp eldrow.hpp
	$(CXX) $(CXXFLAGS) -o $@ check.cpp

# Independent re-check: no shared code, reads the feedback matrix written by fbmatrix.py.
exhaust: exhaust.cpp
	$(CXX) $(CXXFLAGS) -pthread -o $@ exhaust.cpp

fb.bin: words.txt fbmatrix.py verify.py
	python3 -I fbmatrix.py words.txt fb.bin

clean:
	rm -f search check exhaust fb.bin

.PHONY: all clean
