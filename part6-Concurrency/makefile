CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

SRCS = file_server.cpp molecule_requester_6.cpp atom_supplier_6.cpp
BINS = file_server molecule_requester atom_supplier

all: $(BINS)

file_server: file_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

molecule_requester: molecule_requester_6.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

atom_supplier: atom_supplier_6.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)


clean:
	rm -f $(BINS) *.gcno *.gcda *.gcov /tmp/my_*.sock /tmp/molecule_requester_*.sock
