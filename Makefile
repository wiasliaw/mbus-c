# Entry file
PROD_ENTRY := main

# Directory definitions
INC_DIR := include
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin

# Compilation related flags and parameters
CC := gcc
CFLAGS := -Wall -I$(INC_DIR)

INCS_FILES = $(wildcard $(INC_DIR)/*.h)
SRCS_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJS_FILES = $(SRCS_FILES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET_FILES := $(BIN_DIR)/$(PROD_ENTRY)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

main:	$(OBJS_FILES)
	@mkdir -p $(BIN_DIR)
	gcc -o $(TARGET_FILES) $(OBJS_FILES)

clean:
	-rm $(OBJ_DIR)/*.o $(TARGET_FILES)

valgrind: main
	@valgrind --leak-check=full $(TARGET_FILES)

cppcheck:
	cppcheck -i $(INCS_FILES) $(SRCS_FILES)
