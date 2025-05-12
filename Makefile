CC=gcc
CFLAGS+= -Wall -Werror -std=gnu99 -g -pedantic -Iinclude
LDFLAGS=-pthread

CFLAGS+=$(shell sdl2-config --cflags)
LDFLAGS+=$(shell sdl2-config --libs) -lSDL2_image

SRC_DIR=src
INC_DIR=include
BUILD_DIR=build

SOURCES=$(wildcard $(SRC_DIR)/*.c)
OBJECTS=$(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

BINARIES=prgsem-main prgsem-module
BIN_TARGETS=$(addprefix $(BUILD_DIR)/,$(BINARIES))

OBJ_IO=$(BUILD_DIR)/prg_io_nonblock.o
OBJ_COMMON=$(BUILD_DIR)/event_queue.o $(BUILD_DIR)/messages.o $(BUILD_DIR)/window_thread.o $(BUILD_DIR)/computation.o $(BUILD_DIR)/common.o $(BUILD_DIR)/keyboard_thread.o $(BUILD_DIR)/pipe_thread.o

all: $(BUILD_DIR) $(BIN_TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Generic compilation rule
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Main binary
$(BUILD_DIR)/prgsem-main: $(BUILD_DIR)/prgsem-main.o $(OBJ_IO) $(OBJ_COMMON)
	$(CC) $^ $(LDFLAGS) -o $@

# Module binary
$(BUILD_DIR)/prgsem-module: $(BUILD_DIR)/prgsem-module.o $(OBJ_IO) $(OBJ_COMMON)
	$(CC) $^ $(LDFLAGS) -o $@

run-main: $(BUILD_DIR)/prgsem-main
	./$<

run-module: $(BUILD_DIR)/prgsem-module
	./$<

run-mkpipes:
	./scripts/create_pipes.sh && echo "Pipes successfully created!"

clean:
	rm -rf $(BUILD_DIR)

