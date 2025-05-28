CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

all: molecule_server molecule_requester

molecule_server: molecule_server.cpp
	$(CXX) $(CXXFLAGS) -o molecule_server molecule_server.cpp $(LDFLAGS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o molecule_requester molecule_requester.cpp $(LDFLAGS)

clean:
	rm -f molecule_server molecule_requester  *.gcno *.gcda *.gcov
