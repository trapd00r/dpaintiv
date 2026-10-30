CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2) -lm $(shell pkg-config --libs x11)
SRC = src/main.c src/ui.c src/draw.c src/iff.c src/font.c src/assets.c src/builtin.c
CORE = src/ui.c src/draw.c src/iff.c src/font.c src/assets.c src/builtin.c
BIN = dpaint

.PHONY: all clean run smoke

all: $(BIN)

$(BIN): $(SRC) src/dpaint.h
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN) smoke

run: $(BIN)
	./$(BIN)

smoke: tools/smoke.c $(CORE) src/dpaint.h
	$(CC) -std=c11 -Wall -Wextra -O2 -Isrc -o smoke tools/smoke.c $(CORE) -lm
	./smoke
