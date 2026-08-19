CC ?= gcc
CPPFLAGS ?=
CPPFLAGS += -Iinclude -D_GNU_SOURCE -D_FORTIFY_SOURCE=2
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Werror -std=c11 -fstack-protector-strong -fPIE
LDFLAGS ?= -Wl,-z,relro,-z,now -pie

BIN := sentinel
COMPAT_BIN := ir-sentinel
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
TEST_BIN := tests/test_config

.PHONY: all clean run check

all: $(BIN) $(COMPAT_BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(COMPAT_BIN): $(BIN)
	ln -sfn $(BIN) $(COMPAT_BIN)

$(TEST_BIN): tests/test_config.c src/config.c src/log.c include/sentinel.h include/log.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_config.c src/config.c src/log.c -o $@ $(LDFLAGS)

check: all $(TEST_BIN)
	./$(TEST_BIN)
	./tests/test_cli.sh

clean:
	rm -f $(OBJ) $(BIN) $(COMPAT_BIN) $(TEST_BIN)

run: $(BIN)
	./$(BIN)
