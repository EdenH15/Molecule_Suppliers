##./atom_warehouse -p 1234
##./atom_supplier 127.0.0.1 1234

CC = gcc
CFLAGS = -Wall -fprofile-arcs -ftest-coverage
LDFLAGS = -lgcov

SRC = atom_warehouse.c atom_supplier.c
BIN = atom_warehouse atom_supplier

all: $(BIN)

atom_warehouse: atom_warehouse.c atom_warehouse.h
	$(CC) $(CFLAGS) atom_warehouse.c -o atom_warehouse $(LDFLAGS)

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) atom_supplier.c -o atom_supplier $(LDFLAGS)

coverage: $(BIN)
	@for file in $(SRC); do \
		echo "Running gcov on $$file..."; \
		gcov $$file; \
	done

clean:
	rm -f $(BIN) *.gcda *.gcno *.gcov *.out
