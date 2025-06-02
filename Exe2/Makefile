all: molecule_supplier molecule_requester

molecule_supplier: molecule_supplier.c
	gcc -Wall -o molecule_supplier molecule_supplier.c

molecule_requester: molecule_requester.c
	gcc -Wall -o molecule_requester molecule_requester.c

clean:
	rm -f molecule_supplier molecule_requester
