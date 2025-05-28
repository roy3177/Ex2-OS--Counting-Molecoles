CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

all: atom_warehouse atom_supplier

atom_warehouse: atom_warehouse.cpp
	$(CXX) $(CXXFLAGS) -o atom_warehouse atom_warehouse.cpp $(LDFLAGS)

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o atom_supplier atom_supplier.cpp $(LDFLAGS)

clean:
	rm -f atom_warehouse atom_supplier *.gcno *.gcda *.gcov
