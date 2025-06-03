all: drinks_bar molecule_requester atom_supplier

drinks_bar: drinks_bar_v4.c drinks_bar_v4.h
	gcc -Wall -o drinks_bar drinks_bar_v4.c

molecule_requester: molecule_requester.c
	gcc -Wall -o molecule_requester molecule_requester.c

atom_supplier: atom_supplier.c
	gcc -Wall -o atom_supplier atom_supplier.c

clean:
	rm -f drinks_bar molecule_requester atom_supplier
