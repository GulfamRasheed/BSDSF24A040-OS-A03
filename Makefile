CC = gcc
CFLAGS = -Wall -Werror -std=c99
LDFLAGS = -lreadline

TARGET = bin/myshell
SRC_DIR = src
INCLUDE_DIR = include

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/shell.c $(SRC_DIR)/execute.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/shell.h
	$(CC) $(CFLAGS) -c -I$(INCLUDE_DIR) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean