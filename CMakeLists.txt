cmake_minimum_required(VERSION 3.30)
project(ExeM2)

set(CMAKE_CXX_STANDARD 20)


add_executable(atom_warehouse step1_atom_warehouse/atom_warehouse.c)
add_executable(atom_supplier step1_atom_warehouse/atom_supplier.c
        Exe2/Makefile.c)

add_executable(molecule_supplier step2_molecule_supplier/molecule_supplier.c)
add_executable(molecule_requester step2_molecule_supplier/molecule_requester.c)