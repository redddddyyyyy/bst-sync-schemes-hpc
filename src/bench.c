#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#include "bst.h"
#include "papi_util.h"
#include "workload.h"

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
        "         rwlock (global RW-lock, readers concurrent)\n"
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

/* Worker for RW-lock multithreaded search */
static void *worker_rwlock(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    long local_found = 0;

    papi_start_counters();

    for (long i = 0; i < a->count; ++i) {
        int key = a->keys[a->start + i];
        if (bst_search_rwlock(a->tree, key)) {
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

/* Parse "R/I/D" into three ints. Returns 0 on success, nonzero on parse error. */
static int parse_mix(const char *s, int *r, int *i, int *d) {
    if (!s) return 1;
    return (sscanf(s, "%d/%d/%d", r, i, d) == 3) ? 0 : 2;
}

/* Dispatch a single mixed op against the chosen mode. */
static int run_op(const char *mode, bst_t *tree, const wl_op_t *op) {
    /* For Day 1 we only have search implementations for all modes;
       insert/delete fall back to bst_*_seq variants where present,
       guarded by the mode's lock for cg/rwlock. Day 2+ will replace
       these with mode-specific insert/delete. */
    if (op->kind == WL_OP_SEARCH) {
        if (strcmp(mode, "cg") == 0)     return bst_search_cg(tree, op->key);
        if (strcmp(mode, "rwlock") == 0) return bst_search_rwlock(tree, op->key);
        return bst_search_seq(tree, op->key);
    }
    if (op->kind == WL_OP_INSERT) {
        if (strcmp(mode, "cg") == 0)     { bst_insert_cg(tree, op->key); return 0; }
        if (strcmp(mode, "rwlock") == 0) { bst_insert_rwlock(tree, op->key); return 0; }
        bst_insert_seq(tree, op->key);   return 0;
    }
    /* WL_OP_DELETE: not implemented yet (Day 2). For now: treat as search to keep
       the op-count books correct without lying about the mode's behavior. */
    if (strcmp(mode, "cg") == 0)     return bst_search_cg(tree, op->key);
    if (strcmp(mode, "rwlock") == 0) return bst_search_rwlock(tree, op->key);
    return bst_search_seq(tree, op->key);
}

int main(int argc, char **argv)
{
    /* New-style CLI: starts with one or more `--flag value` pairs,
       followed by the legacy positionals (N_INIT N_THREADS MODE [TREE]).
       Note N_SEARCH is replaced by --ops in this path. */
    int   wl_pct_s = -1, wl_pct_i = -1, wl_pct_d = -1;
    long  wl_ops   = -1;
    uint64_t wl_seed = 0;
    int   arg_i = 1;

    while (arg_i < argc && strncmp(argv[arg_i], "--", 2) == 0) {
        const char *flag = argv[arg_i];
        const char *val  = (arg_i + 1 < argc) ? argv[arg_i + 1] : NULL;
        if (!val) { fprintf(stderr, "Flag %s needs a value\n", flag); return EXIT_FAILURE; }

        if (strcmp(flag, "--mix") == 0) {
            if (parse_mix(val, &wl_pct_s, &wl_pct_i, &wl_pct_d) != 0) {
                fprintf(stderr, "--mix expects R/I/D, got '%s'\n", val);
                return EXIT_FAILURE;
            }
        } else if (strcmp(flag, "--ops") == 0) {
            wl_ops = strtol(val, NULL, 10);
        } else if (strcmp(flag, "--seed") == 0) {
            wl_seed = (uint64_t)strtoull(val, NULL, 10);
        } else {
            fprintf(stderr, "Unknown flag '%s'\n", flag);
            return EXIT_FAILURE;
        }
        arg_i += 2;
    }

    int use_workload = (wl_pct_s >= 0 && wl_ops > 0);

    /* After the flag block, what follows depends on the path. */
    if (use_workload) {
        /* New CLI: N_INIT N_THREADS MODE [TREE] */
        int remaining = argc - arg_i;
        if (remaining < 3 || remaining > 4) {
            fprintf(stderr,
                "Usage: %s [--mix R/I/D --ops N --seed S] N_INIT N_THREADS MODE [TREE]\n",
                argv[0]);
            return EXIT_FAILURE;
        }
        long N_init    = strtol(argv[arg_i++], NULL, 10);
        int  nthreads  = atoi(argv[arg_i++]);
        const char *mode      = argv[arg_i++];
        const char *tree_mode = (arg_i < argc) ? argv[arg_i] : "bal";

        if (N_init <= 0 || nthreads <= 0) {
            fprintf(stderr, "N_INIT and N_THREADS must be positive.\n");
            return EXIT_FAILURE;
        }

        bst_t *tree = bst_create();
        if (!tree) { fprintf(stderr, "bst_create failed\n"); return EXIT_FAILURE; }
        if      (strcmp(tree_mode, "bal")  == 0) bst_build_balanced(tree, (int)N_init);
        else if (strcmp(tree_mode, "seq")  == 0) bst_build_sequential(tree, (int)N_init);
        else if (strcmp(tree_mode, "rand") == 0) bst_build_random(tree, (int)N_init);
        else { fprintf(stderr, "Unknown TREE '%s'\n", tree_mode); bst_destroy(tree); return EXIT_FAILURE; }

        wl_op_t *ops = (wl_op_t *)malloc(sizeof(wl_op_t) * (size_t)wl_ops);
        if (!ops) { fprintf(stderr, "malloc failed\n"); bst_destroy(tree); return EXIT_FAILURE; }

        int rc = wl_generate(ops, (size_t)wl_ops, wl_pct_s, wl_pct_i, wl_pct_d,
                             (int)N_init, wl_seed);
        if (rc != 0) {
            fprintf(stderr, "wl_generate rc=%d\n", rc);
            free(ops); bst_destroy(tree); return EXIT_FAILURE;
        }

        /* Single-threaded execution for Day 1; multithreaded mixed runner lands Day 3. */
        long c_s = 0, c_i = 0, c_d = 0;
        long found = 0;
        struct timespec ts0, ts1;
        clock_gettime(CLOCK_MONOTONIC, &ts0);
        for (long k = 0; k < wl_ops; ++k) {
            if (ops[k].kind == WL_OP_SEARCH) { found += run_op(mode, tree, &ops[k]); c_s++; }
            else if (ops[k].kind == WL_OP_INSERT) { run_op(mode, tree, &ops[k]); c_i++; }
            else { run_op(mode, tree, &ops[k]); c_d++; }
        }
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        double elapsed = (ts1.tv_sec - ts0.tv_sec) + (ts1.tv_nsec - ts0.tv_nsec)*1e-9;

        printf("Mode: %s (mixed)\n", mode);
        printf("ops_search=%ld ops_insert=%ld ops_delete=%ld\n", c_s, c_i, c_d);
        printf("found=%ld elapsed=%.6f throughput=%.2f Mops/s\n",
               found, elapsed, (elapsed > 0.0) ? (double)wl_ops / elapsed / 1e6 : 0.0);
        printf("Total found: %ld\n", found);
        printf("Elapsed time: %.6f seconds\n", elapsed);

        free(ops);
        bst_destroy(tree);
        (void)nthreads; /* Day 1 single-thread; flag retained for CLI shape */
        return EXIT_SUCCESS;
    }

    /* ===== Legacy positional CLI below (unchanged from pre-rename behavior, with fg->rwlock) ===== */

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
    /* --- Multithreaded modes: cg, rwlock, ideal --- */
    else if (strcmp(mode, "cg") == 0 ||
             strcmp(mode, "rwlock") == 0 ||
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
            } else if (strcmp(mode, "rwlock") == 0) {
                fn = worker_rwlock;
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

