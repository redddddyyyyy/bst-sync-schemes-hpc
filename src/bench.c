#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#include "bst.h"
#include "papi_util.h"

/* Simple wall-clock helper using CLOCK_MONOTONIC */
static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

typedef struct {
    bst_t *tree;
    int   *keys;
    long   start;
    long   count;
    long   found;
} thread_arg_t;

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s N_INIT N_SEARCH N_THREADS MODE [TREE]\n"
        "  MODE = seq    (sequential search, no locks)\n"
        "         cg     (coarse-grained global mutex)\n"
        "         fg     (global RW-lock, readers concurrent)\n"
        "         ideal  (parallel search, read-only, no locks)\n"
        "  TREE = bal    (default, perfectly balanced)\n"
        "         seq    (sequential inserts 0..N_INIT-1, unbalanced)\n"
        "         rand   (random insert order)\n",
        prog);
}

/* Worker for CG-lock multithreaded search */
static void *worker_cg(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    long local_found = 0;

    // Per-thread counters start here
    papi_start_counters();

    for (long i = 0; i < a->count; ++i) {
        int key = a->keys[a->start + i];
        if (bst_search_cg(a->tree, key)) {
            local_found++;
        }
    }

    // Per-thread counters stop + add into global totals
    papi_stop_and_accum();

    a->found = local_found;
    return NULL;
}

/* Worker for FG RW-lock multithreaded search */
static void *worker_fg(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    long local_found = 0;

    papi_start_counters();

    for (long i = 0; i < a->count; ++i) {
        int key = a->keys[a->start + i];
        if (bst_search_fg(a->tree, key)) {
            local_found++;
        }
    }

    papi_stop_and_accum();

    a->found = local_found;
    return NULL;
}

/* Worker for ideal read-only parallel search (no locks) */
static void *worker_ideal(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    long local_found = 0;

    papi_start_counters();

    for (long i = 0; i < a->count; ++i) {
        int key = a->keys[a->start + i];
        /* Read-only search on an immutable tree: safe without locks */
        if (bst_search_seq(a->tree, key)) {
            local_found++;
        }
    }

    papi_stop_and_accum();

    a->found = local_found;
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 5 && argc != 6) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    long N_init   = strtol(argv[1], NULL, 10);
    long N_search = strtol(argv[2], NULL, 10);
    int  nthreads = atoi(argv[3]);
    const char *mode      = argv[4];
    const char *tree_mode = (argc == 6) ? argv[5] : "bal";

    if (N_init <= 0 || N_search <= 0 || nthreads <= 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("BST benchmark: N_INIT=%ld, N_SEARCH=%ld, N_THREADS=%d, MODE=%s, TREE=%s\n",
           N_init, N_search, nthreads, mode, tree_mode);

    /* Build tree according to tree_mode */
    bst_t *tree = bst_create();
    if (!tree) {
        fprintf(stderr, "bst_create failed\n");
        return EXIT_FAILURE;
    }

    if (strcmp(tree_mode, "bal") == 0) {
        bst_build_balanced(tree, (int)N_init);
    } else if (strcmp(tree_mode, "seq") == 0) {
        bst_build_sequential(tree, (int)N_init);
    } else if (strcmp(tree_mode, "rand") == 0) {
        bst_build_random(tree, (int)N_init);
    } else {
        fprintf(stderr, "Unknown TREE mode '%s'\n", tree_mode);
        bst_destroy(tree);
        return EXIT_FAILURE;
    }

    /* Allocate and fill search keys */
    int *search_keys = (int *)malloc(sizeof(int) * (size_t)N_search);
    if (!search_keys) {
        fprintf(stderr, "malloc failed for search_keys\n");
        bst_destroy(tree);
        return EXIT_FAILURE;
    }

    for (long i = 0; i < N_search; ++i) {
        search_keys[i] = (int)(i % (2 * N_init));
    }

    /* Init PAPI once */
    papi_init_or_die();

    /* --- Sequential baseline: single-threaded search --- */
    if (strcmp(mode, "seq") == 0) {
        long found = 0;

        papi_reset_totals();
        double t0 = now_sec();

        papi_start_counters();
        for (long i = 0; i < N_search; ++i) {
            if (bst_search_seq(tree, search_keys[i])) {
                found++;
            }
        }
        papi_stop_and_accum();

        double t1 = now_sec();

        papi_print_totals();

        printf("Mode: seq (single-thread)\n");
        printf("Total found: %ld\n", found);
        printf("Elapsed time: %.6f seconds\n", t1 - t0);
    }
    /* --- Multithreaded modes: cg, fg, ideal --- */
    else if (strcmp(mode, "cg") == 0 ||
             strcmp(mode, "fg") == 0 ||
             strcmp(mode, "ideal") == 0)
    {
        pthread_t    *threads = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)nthreads);
        thread_arg_t *args    = (thread_arg_t *)malloc(sizeof(thread_arg_t) * (size_t)nthreads);
        if (!threads || !args) {
            fprintf(stderr, "malloc failed for threads/args\n");
            free(threads);
            free(args);
            free(search_keys);
            bst_destroy(tree);
            return EXIT_FAILURE;
        }

        long base   = N_search / nthreads;
        long rem    = N_search % nthreads;
        long offset = 0;

        for (int t = 0; t < nthreads; ++t) {
            long cnt      = base + (t < rem ? 1 : 0);
            args[t].tree  = tree;
            args[t].keys  = search_keys;
            args[t].start = offset;
            args[t].count = cnt;
            args[t].found = 0;
            offset       += cnt;
        }

        papi_reset_totals();
        double t0 = now_sec();

        for (int t = 0; t < nthreads; ++t) {
            void *(*fn)(void *);
            if (strcmp(mode, "cg") == 0) {
                fn = worker_cg;
            } else if (strcmp(mode, "fg") == 0) {
                fn = worker_fg;
            } else { /* ideal */
                fn = worker_ideal;
            }

            if (pthread_create(&threads[t], NULL, fn, &args[t]) != 0) {
                fprintf(stderr, "pthread_create failed for thread %d\n", t);
                // join what we started (best effort)
                for (int j = 0; j < t; ++j) pthread_join(threads[j], NULL);
                free(threads);
                free(args);
                free(search_keys);
                bst_destroy(tree);
                return EXIT_FAILURE;
            }
        }

        for (int t = 0; t < nthreads; ++t) {
            pthread_join(threads[t], NULL);
        }

        double t1 = now_sec();

        long total_found = 0;
        for (int t = 0; t < nthreads; ++t) {
            total_found += args[t].found;
        }

        papi_print_totals();

        printf("Mode: %s (multithreaded)\n", mode);
        printf("Total found: %ld\n", total_found);
        printf("Elapsed time: %.6f seconds\n", t1 - t0);

        free(threads);
        free(args);
    }
    else {
        fprintf(stderr, "Unknown MODE '%s'\n", mode);
        free(search_keys);
        bst_destroy(tree);
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    free(search_keys);
    bst_destroy(tree);
    return EXIT_SUCCESS;
}

