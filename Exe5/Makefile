CC = gcc
CFLAGS = -Wall -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

all: drinks_bar molecule_requester atom_supplier

drinks_bar: drinks_bar_v5.c drinks_bar_v5.h
	$(CC) $(CFLAGS) -o drinks_bar drinks_bar_v5.c $(LDFLAGS)

molecule_requester: molecule_requester.c
	$(CC) $(CFLAGS) -o molecule_requester molecule_requester.c $(LDFLAGS)

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) -o atom_supplier atom_supplier.c $(LDFLAGS)

coverage: drinks_bar molecule_requester atom_supplier
	@echo "Running gcov on drinks_bar..."
	gcov -b -c drinks_bar
	@echo "Running gcov on molecule_requester..."
	gcov -b -c molecule_requester
	@echo "Running gcov on atom_supplier..."
	gcov -b -c atom_supplier


clean:
	rm -f drinks_bar molecule_requester atom_supplier *.out *.gcda *.gcno *.gcov
