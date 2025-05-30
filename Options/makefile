CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

SRCS = options_server.cpp molecule_requester.cpp atom_supplier.cpp
BINS = options_server molecule_requester atom_supplier

all: $(BINS)

options_server: options_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

run_server:
	./options_server -t 60 -T 5555 -U 7777 -o 12 -c 12 -h 30

run_supplier:
	./atom_supplier 127.0.0.1 5555

run_requester:
	./molecule_requester 127.0.0.1 7777

clean:
	rm -f $(BINS) *.gcno *.gcda *.gcov
