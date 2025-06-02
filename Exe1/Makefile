CC = gcc
CFLAGS = -Wall

all: atom_warehouse atom_supplier

atom_warehouse: atom_warehouse.c
	$(CC) $(CFLAGS) -o atom_warehouse atom_warehouse.c

atom_supplier: atom_supplier.c
	$(CC) $(CFLAGS) -o atom_supplier atom_supplier.c

clean:
	rm -f atom_warehouse atom_supplier
