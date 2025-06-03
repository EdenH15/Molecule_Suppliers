#ifndef MOLECULE_SUPPLIER_H
#define MOLECULE_SUPPLIER_H

#include <stdint.h>

// Global atom counts
extern uint64_t hydrogen;
extern uint64_t oxygen;
extern uint64_t carbon;

void print_inventory(void);
void handle_add_command(const char *cmd);
int deliver_molecules(const char *cmd);

#endif // MOLECULE_SUPPLIER_H
