# Install
BIN = buildup-editor

# Flags
CFLAGS += -std=c17 -Wall -Wextra -pedantic -Wno-unused-function -O0
# Optimization flag was -O2 but was changed for debugging

SRC = main.c external/md4c.c external/md4c-html.c external/entity.c
OBJ = $(SRC:.c=.o)

$(BIN): main.c
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) -g $(SRC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -o bin/$(BIN) -I./external -I./lib -lX11 -lm
