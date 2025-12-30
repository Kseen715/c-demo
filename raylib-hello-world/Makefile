SOURCE_FILES = main.c
BUILD_DIR = build
TARGET = $(BUILD_DIR)/main
CC = gcc

all: ${TARGET}

${TARGET}: ${SOURCE_FILES} ${BUILD_DIR}
	$(CC) $(SOURCE_FILES) -o $(TARGET) -Wl,-Bstatic -lraylib -Wl,-Bdynamic -lm

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: ${TARGET}
	./${TARGET}

.PHONY: all clean run