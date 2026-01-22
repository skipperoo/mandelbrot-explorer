# Compiler and Flags
CC = gcc
# -O3: Max optimization
# -pthread: Enable threading support
# -I src/include: Look for headers in the include folder
CFLAGS = -Wextra -O3 -ffast-math -march=native -pthread -I src/include
LDFLAGS = -lm -pthread -lGL -lEGL

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# --- Server Target Configuration ---
# Source files required for the main server
SERVER_SRCS = $(SRC_DIR)/server.c \
              $(SRC_DIR)/queue.c \
              $(SRC_DIR)/mandelbrot.c \
              $(SRC_DIR)/tpool.c \
              $(SRC_DIR)/websocket.c \
              $(SRC_DIR)/webserver.c \
              $(SRC_DIR)/websocket_utils.c

# Object files corresponding to server sources
SERVER_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SERVER_SRCS))

# Output Binary
SERVER_BIN = $(BIN_DIR)/server

# --- Benchmark Target Configuration (Optional) ---
# Assuming mandel_bench is a source file (src/mandel_bench.c)
BENCH_SRCS = $(SRC_DIR)/mandel_bench.c \
             $(SRC_DIR)/mandelbrot.c \
             $(SRC_DIR)/tpool.c

BENCH_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(BENCH_SRCS))
BENCH_BIN = $(BIN_DIR)/mandel_bench

# --- Rules ---

.PHONY: all clean dirs help

# Default target: build the server
all: dirs $(SERVER_BIN)

# Link the Server
$(SERVER_BIN): $(SERVER_OBJS)
	@echo "Linking Server..."
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Link the Benchmark (run 'make bench' to build this)
bench: dirs $(BENCH_BIN)

$(BENCH_BIN): $(BENCH_OBJS)
	@echo "Linking Benchmark..."
	$(CC) $(BENCH_OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Compile Source Files into Objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Create necessary directories
dirs:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Clean up build artifacts
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)

help:
	@echo "Available targets:"
	@echo "  make        - Build the main WebSocket server"
	@echo "  make bench  - Build the standalone benchmark tool"
	@echo "  make clean  - Remove all build files"


