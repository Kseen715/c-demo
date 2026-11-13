# Build and run on the board itself (Orange Pi 3B, RK3566): make run
# Rockchip's NPU runtime, its header, the model and the test files are
# downloaded into rknn/ on the first build, pinned to one release.
.PHONY: all clean distclean run

RKNN_TAG = v2.5.0
RKNN_URL = https://raw.githubusercontent.com/airockchip/rknn-toolkit2/$(RKNN_TAG)
STB_COMMIT = 9600522de53973336fa033a289858a4354cbb04c
STB_URL = https://raw.githubusercontent.com/nothings/stb/$(STB_COMMIT)

DEPS_DIR = rknn
BUILD_DIR = build
TARGET = $(BUILD_DIR)/npu-classify

LIB = $(DEPS_DIR)/librknnrt.so
HEADERS = $(DEPS_DIR)/rknn_api.h $(DEPS_DIR)/stb_image.h
MODEL = $(DEPS_DIR)/mobilenet_v1.rknn
IMAGE = $(DEPS_DIR)/dog_224x224.jpg
LABELS = $(DEPS_DIR)/labels.txt

CC = gcc
# -isystem: no warnings from the downloaded headers.
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -isystem $(DEPS_DIR)
# The runtime stays in rknn/, found next to the binary at run time.
LDFLAGS = -L$(DEPS_DIR) -Wl,-rpath,'$$ORIGIN/../$(DEPS_DIR)'
LDLIBS = -lrknnrt -lm

all: $(TARGET)

$(TARGET): main.c $(HEADERS) $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS) $(LDLIBS)

run: $(TARGET) $(MODEL) $(IMAGE) $(LABELS)
	./$(TARGET) $(MODEL) $(IMAGE) $(LABELS)

$(BUILD_DIR) $(DEPS_DIR):
	mkdir -p $@

FETCH = curl -fsSL -o $@

$(LIB): | $(DEPS_DIR)
	$(FETCH) $(RKNN_URL)/rknpu2/runtime/Linux/librknn_api/aarch64/librknnrt.so
$(DEPS_DIR)/rknn_api.h: | $(DEPS_DIR)
	$(FETCH) $(RKNN_URL)/rknpu2/runtime/Linux/librknn_api/include/rknn_api.h
$(DEPS_DIR)/stb_image.h: | $(DEPS_DIR)
	$(FETCH) $(STB_URL)/stb_image.h
$(MODEL): | $(DEPS_DIR)
	$(FETCH) $(RKNN_URL)/rknpu2/examples/rknn_api_demo/model/RK3566_RK3568/mobilenet_v1.rknn
$(IMAGE): | $(DEPS_DIR)
	$(FETCH) $(RKNN_URL)/rknn-toolkit2/examples/tflite/mobilenet_v1/dog_224x224.jpg
$(LABELS): | $(DEPS_DIR)
	$(FETCH) $(RKNN_URL)/rknn-toolkit2/examples/tflite/mobilenet_v1/labels.txt

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(DEPS_DIR)
