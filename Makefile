# Software Renderer Engine
# C23 + SDL2

CC      = gcc
CFLAGS  = -std=c23 -Wall -Wextra -pedantic -g
LDFLAGS = $(shell pkg-config --libs sdl2) -lm
CFLAGS += $(shell pkg-config --cflags sdl2)

TARGET  = engine
SRC     = main.c math.c render.c mesh.c
OBJ     = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
