CC = gcc
CFLAGS = -Wextra -O3 -ffast-math -march=native -pthread -I src/include
LDFLAGS = -lm -pthread -lGL -lEGL

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SERVER_SRCS = $(SRC_DIR)/server.c \
              $(SRC_DIR)/queue.c \
              $(SRC_DIR)/mandelbrot.c \
              $(SRC_DIR)/tpool.c \
              $(SRC_DIR)/websocket.c \
              $(SRC_DIR)/webserver.c \
              $(SRC_DIR)/websocket_utils.c

SERVER_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SERVER_SRCS))
SERVER_BIN = $(BIN_DIR)/server

.PHONY: all clean dirs bench debug help

all: dirs $(SERVER_BIN)

debug: CFLAGS += -DDEBUG=1
debug: all

valgrind: CFLAGS = -Wextra -Og -g -ffast-math -march=native -pthread -I src/include
valgrind: clean all
	valgrind --leak-check=full  --show-leak-kinds=all --track-origins=yes ./$(SERVER_BIN) -b

$(SERVER_BIN): $(SERVER_OBJS)
	@echo "Linking Server..."
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

bench: all
	./$(SERVER_BIN) -b 2> /dev/null

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

dirs:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)

help:
	@echo "Available targets:"
	@echo "  make        - Build server"
	@echo "  make debug  - Build with DEBUG=1"
	@echo "  make bench  - Build and run benchmark"
	@echo "  make clean  - Remove build files"
