# Software Renderer Engine
# C23 + SDL2

CC      = gcc
CFLAGS  = -std=c23 -Wall -Wextra -pedantic -g
LDFLAGS = $(shell pkg-config --libs sdl2) -lm
CFLAGS += $(shell pkg-config --cflags sdl2)
CFLAGS += -I src

TARGET  = engine

SRC     = src/core/main.c \
          src/core/log.c \
          src/core/camera.c \
          src/math/math.c \
          src/graphics/render.c \
          src/graphics/mesh.c \
          src/graphics/clip.c

OBJDIR  = build
OBJ     = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
