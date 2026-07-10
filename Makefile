CC = gcc
CFLAGS = -Wall -I./include -pthread
LDFLAGS = -pthread

ifeq ($(OS),Windows_NT)
    CFLAGS += -IC:/msys64/ucrt64/include/ncursesw
    LDFLAGS += -lncursesw
else
    CFLAGS += $(shell pkg-config --cflags ncurses || pkg-config --cflags ncursesw)
    LDFLAGS += $(shell pkg-config --libs ncurses || pkg-config --libs ncursesw)
endif

SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = $(BIN_DIR)/simulador

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET) -v 16 -t 100

.PHONY: all clean run