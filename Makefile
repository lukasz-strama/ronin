# Software Renderer Engine
# C23 + SDL2

CC      = gcc
CFLAGS  = -std=c23 -Wall -Wextra -pedantic -g -march=native -DUSE_SIMD
LDFLAGS = $(shell pkg-config --libs sdl2) -lm -lpthread
CFLAGS += $(shell pkg-config --cflags sdl2)
CFLAGS += -I src

TARGET  = engine

SRC     = src/core/main.c \
          src/core/log.c \
          src/core/camera.c \
          src/core/obj_loader.c \
          src/core/entity.c \
          src/core/console.c \
          src/core/level.c \
          src/core/collision_grid.c \
          src/core/chunk.c \
          src/core/threads.c \
          src/math/math.c \
          src/graphics/render.c \
          src/graphics/mesh.c \
          src/graphics/clip.c \
          src/graphics/texture.c \
          src/graphics/hud.c

OBJDIR  = build
OBJ     = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))

STB_IMAGE_URL = https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
STB_IMAGE_DEST = src/graphics/stb_image.h

all: $(STB_IMAGE_DEST) $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(STB_IMAGE_DEST):
	@echo "Downloading stb_image.h..."
	@mkdir -p $(dir $@)
	@if command -v curl >/dev/null 2>&1; then \
		curl -sL $(STB_IMAGE_URL) -o $@; \
	elif command -v wget >/dev/null 2>&1; then \
		wget -q $(STB_IMAGE_URL) -O $@; \
	else \
		echo "Error: curl or wget is required to download dependencies."; \
		exit 1; \
	fi

$(OBJDIR)/%.o: src/%.c $(STB_IMAGE_DEST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
