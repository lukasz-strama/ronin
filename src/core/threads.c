#include "core/threads.h"
#include "core/log.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    pthread_t threads[MAX_WORKER_THREADS];
    int count;

    pthread_mutex_t mutex;
    pthread_cond_t start_cond;
    pthread_cond_t done_cond;

    atomic_int next_tile;
    atomic_int tiles_done;
    int total_tiles;

    int tiles_x;
    int tiles_y;
    int tile_size;
    int screen_w;
    int screen_h;

    TileFunc func;
    void *userdata;

    atomic_int frame_gen;
    bool shutdown;

    int tile_owners[1024];
} ThreadPool;

static ThreadPool g_pool = {0};

static __thread int t_worker_id = -1;

static void *worker_func(void *arg)
{
    t_worker_id = (int)(long)arg;
    int last_gen = 0;

    while (1)
    {
        pthread_mutex_lock(&g_pool.mutex);
        while (atomic_load(&g_pool.frame_gen) == last_gen && !g_pool.shutdown)
            pthread_cond_wait(&g_pool.start_cond, &g_pool.mutex);
        pthread_mutex_unlock(&g_pool.mutex);

        if (g_pool.shutdown)
            return NULL;

        last_gen = atomic_load(&g_pool.frame_gen);

        while (1)
        {
            int tile = atomic_fetch_add(&g_pool.next_tile, 1);
            if (tile >= g_pool.total_tiles)
                break;

            if (tile < 1024)
                g_pool.tile_owners[tile] = t_worker_id;

            int tx = tile % g_pool.tiles_x;
            int ty = tile / g_pool.tiles_x;
            int px = tx * g_pool.tile_size;
            int py = ty * g_pool.tile_size;
            int pw = g_pool.tile_size;
            int ph = g_pool.tile_size;

            if (px + pw > g_pool.screen_w)
                pw = g_pool.screen_w - px;
            if (py + ph > g_pool.screen_h)
                ph = g_pool.screen_h - py;

            g_pool.func(px, py, pw, ph, g_pool.userdata);

            int done = atomic_fetch_add(&g_pool.tiles_done, 1) + 1;
            if (done >= g_pool.total_tiles)
            {
                pthread_mutex_lock(&g_pool.mutex);
                pthread_cond_signal(&g_pool.done_cond);
                pthread_mutex_unlock(&g_pool.mutex);
            }
        }
    }

    return NULL;
}

void threadpool_init(int num_threads)
{
    if (num_threads < 1)
        num_threads = 1;
    if (num_threads > MAX_WORKER_THREADS)
        num_threads = MAX_WORKER_THREADS;

    memset(&g_pool, 0, sizeof(g_pool));
    pthread_mutex_init(&g_pool.mutex, NULL);
    pthread_cond_init(&g_pool.start_cond, NULL);
    pthread_cond_init(&g_pool.done_cond, NULL);

    atomic_store(&g_pool.frame_gen, 0);
    atomic_store(&g_pool.next_tile, 0);
    atomic_store(&g_pool.tiles_done, 0);
    g_pool.shutdown = false;
    g_pool.count = num_threads;

    for (int i = 0; i < num_threads; i++)
        pthread_create(&g_pool.threads[i], NULL, worker_func, (void *)(long)i);

    LOG_INFO("Thread pool initialized: %d workers", num_threads);
}

void threadpool_shutdown(void)
{
    if (g_pool.count == 0)
        return;

    pthread_mutex_lock(&g_pool.mutex);
    g_pool.shutdown = true;
    pthread_cond_broadcast(&g_pool.start_cond);
    pthread_mutex_unlock(&g_pool.mutex);

    for (int i = 0; i < g_pool.count; i++)
        pthread_join(g_pool.threads[i], NULL);

    pthread_mutex_destroy(&g_pool.mutex);
    pthread_cond_destroy(&g_pool.start_cond);
    pthread_cond_destroy(&g_pool.done_cond);

    int old_count = g_pool.count;
    memset(&g_pool, 0, sizeof(g_pool));
    LOG_INFO("Thread pool shut down (%d workers)", old_count);
}

void threadpool_dispatch(int tiles_x, int tiles_y, int tile_size,
                         int screen_w, int screen_h,
                         TileFunc func, void *userdata)
{
    g_pool.tiles_x = tiles_x;
    g_pool.tiles_y = tiles_y;
    g_pool.tile_size = tile_size;
    g_pool.screen_w = screen_w;
    g_pool.screen_h = screen_h;
    g_pool.func = func;
    g_pool.userdata = userdata;
    g_pool.total_tiles = tiles_x * tiles_y;

    atomic_store(&g_pool.next_tile, 0);
    atomic_store(&g_pool.tiles_done, 0);

    pthread_mutex_lock(&g_pool.mutex);
    atomic_fetch_add(&g_pool.frame_gen, 1);
    pthread_cond_broadcast(&g_pool.start_cond);
    pthread_mutex_unlock(&g_pool.mutex);

    pthread_mutex_lock(&g_pool.mutex);
    while (atomic_load(&g_pool.tiles_done) < g_pool.total_tiles)
        pthread_cond_wait(&g_pool.done_cond, &g_pool.mutex);
    pthread_mutex_unlock(&g_pool.mutex);
}

int threadpool_get_count(void)
{
    return g_pool.count;
}

bool threadpool_is_active(void)
{
    return g_pool.count > 0;
}

const int *threadpool_get_tile_owners(void)
{
    return g_pool.tile_owners;
}

int threadpool_get_tiles_x(void)
{
    return g_pool.tiles_x;
}

int threadpool_get_tiles_y(void)
{
    return g_pool.tiles_y;
}
