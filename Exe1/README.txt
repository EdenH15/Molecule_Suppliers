Q1 - Operating Systems EXE 2 - Q: 1
======================================

This directory contains the implementation of Step 1 of the "Warehouse Molecules" assignment.
In this step, we implemented a TCP server and a TCP client that communicate using a custom text-based protocol
for adding atoms to a central warehouse.

--------------------------------------------------------------------------------
1. Files
--------------------------------------------------------------------------------

- atom_warehouse.c  → TCP server that manages the atom warehouse and handles multiple clients using select().
- atom_supplier.c   → TCP client that connects to the warehouse server and sends commands interactively.
- Makefile          → For compiling both server and client.
- README.txt        → This file.

--------------------------------------------------------------------------------
2. Compilation
--------------------------------------------------------------------------------

To compile both the server and the client, run:

    make

To clean the directory (remove executables), run:

    make clean

--------------------------------------------------------------------------------
3. Running the Server
--------------------------------------------------------------------------------

To start the warehouse server, run:

    ./atom_warehouse <tcp-port>

For example:

    ./atom_warehouse 5555

The server will listen on the specified port and accept incoming TCP connections.
It supports multiple concurrent clients using select().

--------------------------------------------------------------------------------
4. Running the Client
--------------------------------------------------------------------------------

To start the supplier client, run:

    ./atom_supplier <hostname/IP> <tcp-port>

For example:

    ./atom_supplier 127.0.0.1 5555

The client connects to the warehouse and allows the user to type commands like:

    ADD OXYGEN 3
    ADD CARBON 1
    ADD HYDROGEN 10

Each command must end with a newline.
To exit the client, press Ctrl+D (EOF).

--------------------------------------------------------------------------------
5. Notes
--------------------------------------------------------------------------------

- Invalid commands will be rejected with an error message.
- The server prints the updated warehouse inventory after every valid command.
- The server prints an error message for unknown atom types or malformed commands.
- The client uses getaddrinfo() to support both IP and DNS hostnames.
- The protocol is plain text over TCP, one line per command.

--------------------------------------------------------------------------------
6. Authors
--------------------------------------------------------------------------------

- Maayan Turgeman
- Eden Hassin