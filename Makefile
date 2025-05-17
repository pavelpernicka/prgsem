CC = gcc
CFLAGS += -Wall -Werror -std=gnu99 -g -pedantic -Iinclude
LDFLAGS = -pthread

# SDL2
CFLAGS += $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build

VERSION_FILE := $(INC_DIR)/version.h
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

BINARIES := prgsem-main prgsem-module
BIN_TARGETS := $(addprefix $(BUILD_DIR)/,$(BINARIES))

# Version info from git
GIT_TAG := $(shell git describe --tags --always --dirty 2>/dev/null) # Check latest tag
GIT_VERSION := $(shell git describe --tags --abbrev=0 2>/dev/null || echo 1.0.0)
VERSION_MAJOR := $(shell echo $(GIT_VERSION) | cut -d. -f1)
VERSION_MINOR := $(shell echo $(GIT_VERSION) | cut -d. -f2)
VERSION_PATCH := $(shell echo $(GIT_VERSION) | cut -d. -f3)

all: version $(BUILD_DIR) $(BIN_TARGETS)

# Create build dir if needed
$(BUILD_DIR):
	mkdir -p $@

# Auto generate version.h
version: $(VERSION_FILE)

$(VERSION_FILE):
	@echo "Generating version header from Git info..."
	@echo "#ifndef VERSION_H" > $@
	@echo "#define VERSION_H" >> $@
	@echo "#define VERSION ((msg_version){ $(VERSION_MAJOR), $(VERSION_MINOR), $(VERSION_PATCH) })" >> $@
	@echo "#define APP_VERSION \"prgsem_main $(GIT_TAG)\"" >> $@
	@echo "#define MOD_VERSION \"prgsem_module $(GIT_TAG)\"" >> $@
	@echo "#endif // VERSION_H" >> $@

# Compilation rule
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(VERSION_FILE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link binaries
$(BUILD_DIR)/prgsem-main: \
	$(BUILD_DIR)/prgsem-main.o \
	$(BUILD_DIR)/event_queue.o \
	$(BUILD_DIR)/messages.o \
	$(BUILD_DIR)/window_thread.o \
	$(BUILD_DIR)/computation.o \
	$(BUILD_DIR)/common.o \
	$(BUILD_DIR)/keyboard_thread.o \
	$(BUILD_DIR)/pipe_thread.o \
	$(BUILD_DIR)/prg_io_nonblock.o
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/prgsem-module: \
	$(BUILD_DIR)/prgsem-module.o \
	$(BUILD_DIR)/event_queue.o \
	$(BUILD_DIR)/messages.o \
	$(BUILD_DIR)/common.o \
	$(BUILD_DIR)/keyboard_thread.o \
	$(BUILD_DIR)/pipe_thread.o \
	$(BUILD_DIR)/prg_io_nonblock.o
	$(CC) $^ $(LDFLAGS) -o $@

# Run helpers
run-main: $(BUILD_DIR)/prgsem-main
	./$<

run-module: $(BUILD_DIR)/prgsem-module
	./$<

run-mkpipes:
	./scripts/create_pipes.sh && echo "Pipes successfully created!"

clean:
	rm -rf $(BUILD_DIR) $(VERSION_FILE)

