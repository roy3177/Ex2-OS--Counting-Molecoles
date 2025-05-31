CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

SRCS = uds_server.cpp molecule_requester.cpp atom_supplier.cpp
BINS = uds_server molecule_requester atom_supplier

all: $(BINS)

uds_server: uds_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)


clean:
	rm -f $(BINS) *.gcno *.gcda *.gcov /tmp/my_*.sock /tmp/molecule_requester_*.sock
