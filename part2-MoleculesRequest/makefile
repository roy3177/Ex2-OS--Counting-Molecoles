CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

SRCS = molecule_server.cpp molecule_requester.cpp atom_supplier2.cpp
BINS = molecule_server molecule_requester atom_supplier

all: $(BINS)

molecule_server: molecule_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

atom_supplier: atom_supplier2.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.gcno *.gcda *.gcov
