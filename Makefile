# Compiler
CC = gcc

# Source files
SRC = src/main.c src/parser.c src/dir_trie_api.c

# Output binary
OUT = main

# Compilation flags (optional, add -Wall -Wextra -g for debugging)
CFLAGS = -Wall -Wextra -g -pthread

# Default target
all: $(OUT) setcap

# Build target
$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

# Set capabilities on the binary
setcap:
	sudo setcap 'cap_sys_nice=eip' $(OUT)

# Run the program
run: $(OUT)
	./$(OUT)

# Clean compiled files
clean:
	rm -f $(OUT)
