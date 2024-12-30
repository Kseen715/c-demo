.PHONY: main clean run
ifneq ($V,1)
Q ?= @
endif

OUTPUT = macros

DEBUG = -g -O0
# DEBUG = -O3
CC = gcc
INCLUDE = -I. -I/usr/local/include
CFLAGS = -Wall -Wextra -Werror -Winline -pipe -std=c99 $(DEBUG) $(INCLUDE)

LDFLAGS = -L/usr/local/lib
LDLIBS = -lwiringPi

BUILD_DIR = build
SRC = $(wildcard *.c)
OBJ = $(patsubst %.c,%.o,$(SRC))
OBJ := $(addprefix $(BUILD_DIR)/,$(OBJ))


OUTPUT := $(BUILD_DIR)/$(OUTPUT)

main: $(OUTPUT)

# Link object files into executable
$(OUTPUT): $(OBJ)
	$Q echo "[linking $@]"
	$Q $(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# Compile source files into object files
$(BUILD_DIR)/%.o: $(SRC) build_dir
	$Q echo "[compiling $<]"
	$Q $(CC) -c -o $@ $< $(CFLAGS)

build_dir:
	$Q @if [ ! -d $(BUILD_DIR) ]; then mkdir -p $(BUILD_DIR); fi

run: $(OUTPUT)
	$Q echo "[running $(OUTPUT)]"
	$Q ./$(OUTPUT)

clean:
	$Q rm -rf $(BUILD_DIR)
