CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Werror -std=c11 -D_GNU_SOURCE
CPPFLAGS += -Iinclude
LDFLAGS ?=

BIN := ir-sentinel
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)

run: $(BIN)
	./$(BIN)
