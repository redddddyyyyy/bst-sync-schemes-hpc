#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#include "bst.h"
#include "papi_util.h"

typedef struct {
    bst_t *tree;
    int   *keys;
    long   start;
    long   count;
    long   found;
} thread_arg_t;

static pthread_mutex_t g_tree_mutex = PTHREAD_MUTEX_INITIALIZER;

static double now_sec(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void *worker_cg(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    long local_found = 0;

    for (long i = 0; i < a->count; ++i) {
        int key = a->keys[a->start + i];

        pthread_mutex_lock(&g_tree_mutex);
        if (bst_search_seq(a->tree, key)) {
            local_found++;
        }
        pthread_mutex_unlock(&g_tree_mutex);
    }

    a->found = local_found;
    return NULL;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s N_INIT N_SEARCH N_THREADS [TREE]\\n"
        "  TREE = bal (default, perfectly balanced)\\n"
        "         seq (sequential inserts 0..N_INIT-1)\\n"
        "         rand (random inserts)\\n",
        prog);
}

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 5) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    long N_init   = strtol(argv[1], NULL, 10);
    long N_search = strtol(argv[2], NULL, 10);
    long N_threads = strtol(argv[3], NULL, 10);
    const char *tree_mode = (argc == 5) ? argv[4] : "bal";

    if (N_init <= 0 || N_search <= 0 || N_threads <= 0) {
        fprintf(stderr, "All numeric arguments must be positive.\\n");
        return EXIT_FAILURE;
    }

    bst_t *tree = bst_create();
    if (!tree) {
        fprintf(stderr, "bst_create failed\\n");
        return EXIT_FAILURE;
    }

    if (strcmp(tree_mode, "bal") == 0) {
        bst_build_balanced(tree, (int)N_init);
    } else if (strcmp(tree_mode, "seq") == 0) {
        bst_build_sequential(tree, (int)N_init);
    } else if (strcmp(tree_mode, "rand") == 0) {
        bst_build_random(tree, (int)N_init);
    } else {
        fprintf(stderr, "Unknown TREE mode '%s'\\n", tree_mode);
        bst_destroy(tree);
        return EXIT_FAILURE;
    }

    int *search_keys = malloc(sizeof(int) * (size_t)N_search);
    if (!search_keys) {
        fprintf(stderr, "malloc failed for search_keys\\n");
        bst_destroy(tree);
        return EXIT_FAILURE;
    }
    for (long i = 0; i < N_search; ++i) {
        search_keys[i] = (int)(i % (2 * N_init));
    }

    papi_init_or_die();

    pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)N_threads);
    thread_arg_t *args = malloc(sizeof(thread_arg_t) * (size_t)N_threads);
    if (!threads || !args) {
        fprintf(stderr, "malloc failed for thread structures\\n");
        free(search_keys);
        bst_destroy(tree);
        return EXIT_FAILURE;
    }

    long base = N_search / N_threads;
    long rem  = N_search % N_threads;

    long offset = 0;
    for (long t = 0; t < N_threads; ++t) {
        long chunk = base + (t < rem ? 1 : 0);
        args[t].tree  = tree;
        args[t].keys  = search_keys;
        args[t].start = offset;
        args[t].count = chunk;
        args[t].found = 0;
        offset += chunk;
    }

    double t0 = now_sec();
    papi_start_counters();

    for (long t = 0; t < N_threads; ++t) {
        if (pthread_create(&threads[t], NULL, worker_cg, &args[t]) != 0) {
            perror("pthread_create");
            N_threads = t;
            break;
        }
    }

    long total_found = 0;
    for (long t = 0; t < N_threads; ++t) {
        if (pthread_join(threads[t], NULL) != 0) {
            perror("pthread_join");
        } else {
            total_found += args[t].found;
        }
    }

    papi_stop_and_print();
    double t1 = now_sec();

    double elapsed = t1 - t0;
    double throughput = (elapsed > 0.0)
        ? (double)N_search / elapsed / 1e6
        : 0.0;

    printf("Mode: cg (coarse‑grain mutex)\\n");
    printf("Total found: %ld\\n", total_found);
    printf("Elapsed time: %.6f seconds\\n", elapsed);
    printf("Throughput: %.2f M ops/s\\n", throughput);

    free(threads);
    free(args);
    free(search_keys);
    bst_destroy(tree);

    return EXIT_SUCCESS;
}
