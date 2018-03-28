BUILD_DIR ?= ./build
SRC_DIR = ./src
INC_DIR = $(SRC_DIR)

CC=h5cc
CFLAGS=-Wall -g -O2 -fpic -I$(INC_DIR)

.PHONY: all
all: plugin example

.PHONY: plugin
plugin: $(BUILD_DIR)/plugin.so

.PHONY: example
example: $(BUILD_DIR)/example

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/plugin.so: $(BUILD_DIR)/plugin.o $(BUILD_DIR)/file.o $(BUILD_DIR)/err.o
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $^ -o $(BUILD_DIR)/plugin.so

$(BUILD_DIR)/example: $(BUILD_DIR)/test.o $(BUILD_DIR)/file.o $(BUILD_DIR)/err.o
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $(BUILD_DIR)/example

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
