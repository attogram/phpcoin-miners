# Compiler and Flags
CC = gcc
# Added -march=native and -funroll-loops for aggressive optimization
CFLAGS = -Wall -O3 -march=native -funroll-loops -pthread
LDFLAGS = -static
LIBS = -lgmp -lcurl -largon2 -lcrypto -lpthread

# Source and Object Files
SRCS_MINER = src/miner_core.c src/c_miner.c
OBJS_MINER = $(SRCS_MINER:.c=.o)

# Executables
TARGET_MINER = c_miner

.PHONY: all clean

all: $(TARGET_MINER)

# --- Build Rules ---

$(TARGET_MINER): $(OBJS_MINER)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# --- Housekeeping ---

clean:
	rm -f src/*.o $(TARGET_MINER)

# --- PHONY targets for convenience ---
run-miner: all
	@echo "Usage: ./c_miner -n <node> -a <address> -t <threads>"

.DEFAULT_GOAL := all
