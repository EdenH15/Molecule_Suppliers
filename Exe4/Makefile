#./drinks_bar -T 1234 -U 5678 -o 2 -c 3 -h 4 -t 30
#./molecule_requester -h 127.0.0.1 -p 5678
#./atom_supplier -h 127.0.0.1 -p 5678

CC = gcc
CFLAGS = -Wall -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

all: drinks_bar molecule_requester atom_supplier

drinks_bar: drinks_bar_v4.c drinks_bar_v4.h
	$(CC) $(CFLAGS) -o drinks_bar drinks_bar_v4.c $(LDFLAGS)

molecule_requester: molecule_requester.c
	$(CC) $(CFLAGS) -o molecule_requester molecule_requester.c $(LDFLAGS)

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) -o atom_supplier atom_supplier.c $(LDFLAGS)

coverage: drinks_bar molecule_requester atom_supplier
	@echo "Running gcov on drinks_bar_v4.c..."
	gcov -b -c drinks_bar_v4.c
	@echo "Running gcov on molecule_requester.c..."
	gcov -b -c molecule_requester.c
	@echo "Running gcov on atom_supplier.c..."
	gcov -b -c atom_supplier.c

clean:
	rm -f drinks_bar molecule_requester atom_supplier *.out *.gcda *.gcno *.gcov
