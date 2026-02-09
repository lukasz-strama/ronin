#ifndef THREADS_H
#define THREADS_H

#include <stdbool.h>

#define MAX_WORKER_THREADS 16
#define TILE_SIZE 32

typedef void (*TileFunc)(int tile_x, int tile_y, int tile_w, int tile_h, void *userdata);

void threadpool_init(int num_threads);
void threadpool_shutdown(void);
void threadpool_dispatch(int tiles_x, int tiles_y, int tile_size,
                         int screen_w, int screen_h,
                         TileFunc func, void *userdata);
int threadpool_get_count(void);
bool threadpool_is_active(void);
const int *threadpool_get_tile_owners(void);
int threadpool_get_tiles_x(void);
int threadpool_get_tiles_y(void);

#endif
