BUILD_DIR ?= ./build
SRC_DIR = ./src
TEST_DIR = ./test
INC_DIR = $(SRC_DIR)

CC=h5cc
CFLAGS=-Wall -g -O2 -fpic -I$(INC_DIR)

.PHONY: all
all: plugin example test_plugin

.PHONY: plugin
plugin: $(BUILD_DIR)/durin-plugin.so

.PHONY: example
example: $(BUILD_DIR)/example

.PHONY: test_plugin
test_plugin: $(BUILD_DIR)/test_plugin

$(BUILD_DIR)/test_plugin: $(TEST_DIR)/generic_data_plugin.f90 $(TEST_DIR)/test_generic_host.f90
	mkdir -p $(BUILD_DIR)
	gfortran -O -g -fopenmp -ldl $(TEST_DIR)/generic_data_plugin.f90 $(TEST_DIR)/test_generic_host.f90 -o $@ -J$(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/durin-plugin.so: $(BUILD_DIR)/plugin.o $(BUILD_DIR)/file.o $(BUILD_DIR)/err.o
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $^ -o $(BUILD_DIR)/durin-plugin.so

$(BUILD_DIR)/example: $(BUILD_DIR)/test.o $(BUILD_DIR)/file.o $(BUILD_DIR)/err.o
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $(BUILD_DIR)/example

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
