# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Wextra -pedantic -g -pthread

# Directories
SRC_DIR = src
INC_DIR = include

# Source Files
DAEMON_SRC = $(SRC_DIR)/daemon.c $(SRC_DIR)/utils.c $(SRC_DIR)/dir_trie_api.c $(SRC_DIR)/parser.c
CLIENT_SRC = $(SRC_DIR)/client.c $(SRC_DIR)/utils.c $(SRC_DIR)/dir_trie_api.c $(SRC_DIR)/parser.c

# Header Files
INCLUDES = -I$(INC_DIR)

# Output Binaries
DAEMON_BIN = daemon
CLIENT_BIN = client

# Default Target
all: $(DAEMON_BIN) $(CLIENT_BIN) run-daemon

# Compile Daemon
$(DAEMON_BIN): $(DAEMON_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Compile Client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Run Daemon in Background with sudo
run-daemon: $(DAEMON_BIN)
	sudo ./$(DAEMON_BIN) &

# Clean Compiled Binaries
clean:
	rm -f $(DAEMON_BIN) $(CLIENT_BIN)
