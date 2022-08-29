# Install
BIN = buildup-editor

# Flags
CFLAGS += -std=c18 -Wall -Wextra -pedantic -Wno-unused-function -O2

SRC = main.c external/md4c.c external/md4c-html.c external/entity.c
OBJ = $(SRC:.c=.o)

$(BIN): main.c
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -o bin/$(BIN) -I./external -I./lib -lX11 -lm
