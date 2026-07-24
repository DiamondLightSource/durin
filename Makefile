BUILD_DIR ?= ./build
SRC_DIR = ./src
TEST_DIR = ./test
INC_DIR = $(SRC_DIR)

BSLZ4_SRC_DIR = ./bslz4/src
BSLZ4_BUILD_DIR = ./bslz4/build
BSLZ4_INC_DIR = $(BSLZ4_SRC_DIR)

CC=h5cc
# -std=gnu99 provides for strtok_r
CFLAGS=-DH5_USE_110_API -Wall -g -O2 -fpic -I$(INC_DIR) -I$(BSLZ4_INC_DIR) -std=gnu99 -shlib

# include https://github.com/kiyo-masui/bitshuffle (to handle
# e.g. bitshuffle compressed pixel_masks)
use_BITSHUFFLE =
ifeq ($(use_BITSHUFFLE),)
else
BITSHUFFLE_SRC_DIR = ../bitshuffle-master/src/
BITSHUFFLE_INC_DIR = ../bitshuffle-master/src/
BITSHUFFLE_OBJS    = $(BUILD_DIR)/bshuf_h5filter.o
CFLAGS += -DUSE_BITSHUFFLE -I$(BITSHUFFLE_INC_DIR)
endif

.PHONY: plugin
plugin: $(BUILD_DIR)/durin-plugin.so

.PHONY: all
all: plugin example test_plugin

.PHONY: example
example: $(BUILD_DIR)/example

.PHONY: test_plugin
test_plugin: $(BUILD_DIR)/test_plugin

$(BUILD_DIR)/test_plugin: $(TEST_DIR)/generic_data_plugin.f90 $(TEST_DIR)/test_generic_host.f90
	mkdir -p $(BUILD_DIR)
	gfortran -O -g -fopenmp -ldl $(TEST_DIR)/generic_data_plugin.f90 $(TEST_DIR)/test_generic_host.f90 -o $@ -J$(BUILD_DIR)

$(BUILD_DIR)/bshuf_h5filter.o: $(BITSHUFFLE_SRC_DIR)/bshuf_h5filter.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BSLZ4_BUILD_DIR)/%.o: $(BSLZ4_SRC_DIR)/%.c
	mkdir -p $(BSLZ4_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/bslz4.a: $(BSLZ4_BUILD_DIR)/lz4.o $(BSLZ4_BUILD_DIR)/bitshuffle.o \
$(BSLZ4_BUILD_DIR)/bitshuffle_core.o $(BSLZ4_BUILD_DIR)/iochain.o
	mkdir -p $(BUILD_DIR)
	ar rcs $@ $^

$(BUILD_DIR)/durin-plugin.so: $(BUILD_DIR)/plugin.o $(BUILD_DIR)/file.o $(BUILD_DIR)/err.o $(BUILD_DIR)/filters.o $(BITSHUFFLE_OBJS) \
$(BUILD_DIR)/bslz4.a
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared -noshlib $^ -o $(BUILD_DIR)/durin-plugin.so

$(BUILD_DIR)/example: $(BUILD_DIR)/test.o $(BUILD_DIR)/file.o $(BUILD_DIR)/err.o $(BUILD_DIR)/filters.o \
$(BUILD_DIR)/bslz4.a
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $(BUILD_DIR)/example

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
	rm -r $(BSLZ4_BUILD_DIR)
