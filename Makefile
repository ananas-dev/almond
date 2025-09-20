CC = clang
CFLAGS =  -I./vendor -Wall -Wextra -std=c11 -g
TARGET = game
SRCDIR = src
SRC = $(SRCDIR)/main.c $(SRCDIR)/map.c $(SRCDIR)/geometry.c $(SRCDIR)/shader.c $(SRCDIR)/arena_linux.c
LIBS = -lSDL3 -lSDL_ttf -lm -lGL -lGLEW

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean
	

