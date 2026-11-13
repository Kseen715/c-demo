# Build and run on the board itself (Orange Pi 3B, RK3566): make run
# Rockchip's NPU runtime and its headers are downloaded into rknn/ on the
# first build, pinned to one release.
.PHONY: all clean distclean run

RKNN_TAG = v2.5.0
RKNN_URL = https://raw.githubusercontent.com/airockchip/rknn-toolkit2/$(RKNN_TAG)/rknpu2/runtime/Linux/librknn_api

DEPS_DIR = rknn
BUILD_DIR = build
TARGET = $(BUILD_DIR)/npu-matmul

LIB = $(DEPS_DIR)/librknnrt.so
HEADERS = $(DEPS_DIR)/rknn_api.h $(DEPS_DIR)/rknn_matmul_api.h

CC = gcc
# -isystem: no warnings from the downloaded headers. cortex-a55: the
# board's CPU, so the loops use its vector unit to the full.
CFLAGS = -Wall -Wextra -Werror -std=c99 -O3 -mcpu=cortex-a55 -fopenmp \
	-isystem $(DEPS_DIR)
# The runtime stays in rknn/, found next to the binary at run time.
LDFLAGS = -fopenmp -L$(DEPS_DIR) -Wl,-rpath,'$$ORIGIN/../$(DEPS_DIR)'
LDLIBS = -lrknnrt -lm

all: $(TARGET)

$(TARGET): main.c $(HEADERS) $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

$(BUILD_DIR) $(DEPS_DIR):
	mkdir -p $@

$(LIB): | $(DEPS_DIR)
	curl -fsSL -o $@ $(RKNN_URL)/aarch64/librknnrt.so
$(DEPS_DIR)/%.h: | $(DEPS_DIR)
	curl -fsSL -o $@ $(RKNN_URL)/include/$*.h

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(DEPS_DIR)
