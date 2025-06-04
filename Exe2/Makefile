CC = gcc
CFLAGS = -Wall -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

all: molecule_supplier molecule_requester atom_supplier

molecule_supplier: molecule_supplier.c
	$(CC) $(CFLAGS) -o molecule_supplier molecule_supplier.c $(LDFLAGS)

molecule_requester: molecule_requester.c
	$(CC) $(CFLAGS) -o molecule_requester molecule_requester.c $(LDFLAGS)

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) -o atom_supplier atom_supplier.c $(LDFLAGS)

coverage: all
	@echo "Running gcov on molecule_supplier.c..."
	gcov -b -c molecule_supplier.c
	@echo "Running gcov on molecule_requester.c..."
	gcov -b -c molecule_requester.c
	@echo "Running gcov on atom_supplier.c..."
	gcov -b -c atom_supplier.c

clean:
	rm -f molecule_supplier molecule_requester atom_supplier *.out *.gcda *.gcno *.gcov