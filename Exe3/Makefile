CC = gcc
CFLAGS = -Wall -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

all: drinks_bar molecule_requester atom_supplier

drinks_bar: drinks_bar.c drinks_bar.h
	$(CC) $(CFLAGS) -o drinks_bar drinks_bar.c $(LDFLAGS)

molecule_requester: molecule_requester.c
	$(CC) $(CFLAGS) -o molecule_requester molecule_requester.c $(LDFLAGS)

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) -o atom_supplier atom_supplier.c $(LDFLAGS)


coverage: drinks_bar molecule_requester atom_supplier
	@echo "Running gcov on drinks_bar.c..."
	gcov -b -c drinks_bar.c
	@echo "Running gcov on molecule_requester.c..."
	gcov -b -c molecule_requester.c
	@echo "Running gcov on atom_supplier.c..."
	gcov -b -c atom_supplier.c

clean:
	rm -f drinks_bar molecule_requester atom_supplier *.out *.gcda *.gcno *.gcov