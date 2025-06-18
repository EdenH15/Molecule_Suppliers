#./drinks_bar_v6 -f atom.txt -T 1111 -U 2222
#./drinks_bar_v6 -f atom.txt -T 1234 -U 5678
#./atom_supplier -h 127.0.0.1 -p 1234
#./molecule_requester -h 127.0.0.1 -p

# Compiler and flags for coverage
CC = gcc
CFLAGS = -Wall -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

# Targets to build all executables
all: drinks_bar_v6 molecule_requester atom_supplier

# Compile each component with coverage flags
drinks_bar_v6: drinks_bar_v6.c drinks_bar_v6.h
	$(CC) $(CFLAGS) -o drinks_bar_v6 drinks_bar_v6.c $(LDFLAGS)

molecule_requester: molecule_requester.c
	$(CC) $(CFLAGS) -o molecule_requester molecule_requester.c $(LDFLAGS)

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) -o atom_supplier atom_supplier.c $(LDFLAGS)

# Run coverage analysis after execution
coverage: all
	@echo "Running gcov on drinks_bar_v6.c..."
	gcov -b -c drinks_bar_v6.c
	@echo "Running gcov on molecule_requester.c..."
	gcov -b -c molecule_requester.c
	@echo "Running gcov on atom_supplier.c..."
	gcov -b -c atom_supplier.c

# Clean all generated files
clean:
	rm -f drinks_bar_v6 molecule_requester atom_supplier *.out *.gcda *.gcno *.gcov inventory.txt