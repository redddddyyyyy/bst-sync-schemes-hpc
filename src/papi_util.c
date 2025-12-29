// papi_util.c
#include "papi_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#ifndef USE_PAPI
#define USE_PAPI 1
#endif

#if USE_PAPI
#include <papi.h>

static int g_papi_ok = 0;

static int g_num_events = 0;
static int g_codes[2] = { PAPI_NULL, PAPI_NULL };
static const char *g_names[2] = { NULL, NULL };

static pthread_mutex_t g_tot_mu = PTHREAD_MUTEX_INITIALIZER;
static long long g_totals[2] = {0, 0};

// Per-thread event set
static _Thread_local int t_event_set = PAPI_NULL;

static int papi_env_disabled(void) {
    const char *e = getenv("PAPI");
    return (e && e[0] == '0');
}

static unsigned long papi_tid(void) {
    return (unsigned long)(uintptr_t)pthread_self();
}

static int pick_event(const char **candidates, int *out_code, const char **out_name) {
    for (int i = 0; candidates[i]; i++) {
        int code = PAPI_NULL;
        int ret = PAPI_event_name_to_code((char*)candidates[i], &code);
        if (ret == PAPI_OK) {
            *out_code = code;
            *out_name = candidates[i];
            return 1;
        }
    }
    return 0;
}

void papi_reset_totals(void) {
    pthread_mutex_lock(&g_tot_mu);
    g_totals[0] = 0;
    g_totals[1] = 0;
    pthread_mutex_unlock(&g_tot_mu);
}

void papi_print_totals(void) {
    if (!g_papi_ok || papi_env_disabled()) return;

    pthread_mutex_lock(&g_tot_mu);
    long long a = g_totals[0];
    long long b = g_totals[1];
    pthread_mutex_unlock(&g_tot_mu);

    if (g_num_events >= 1 && g_names[0]) printf("%s (TOTAL): %lld\n", g_names[0], a);
    if (g_num_events >= 2 && g_names[1]) printf("%s (TOTAL): %lld\n", g_names[1], b);
}

void papi_init_or_die(void)
{
    if (papi_env_disabled()) { g_papi_ok = 0; return; }

    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        fprintf(stderr, "PAPI_library_init failed (ret=%d). Disabling PAPI.\n", ret);
        g_papi_ok = 0;
        return;
    }

    // Enable per-thread counting
    ret = PAPI_thread_init(papi_tid);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_thread_init failed: %s (continuing)\n", PAPI_strerror(ret));
        // Still allow single-thread counting; threads may not be supported in this setup
    }

    // Choose events once (portable-ish fallbacks)
    const char *ins_candidates[] = {
        "ix86arch::INSTRUCTION_RETIRED",
        "perf::PERF_COUNT_HW_INSTRUCTIONS",
        "perf::INSTRUCTIONS",
        NULL
    };
    const char *cyc_candidates[] = {
        "ix86arch::UNHALTED_CORE_CYCLES",
        "perf::PERF_COUNT_HW_CPU_CYCLES",
        "perf::CYCLES",
        "perf::CPU-CYCLES",
        NULL
    };

    g_num_events = 0;
    int code0 = PAPI_NULL, code1 = PAPI_NULL;
    const char *name0 = NULL, *name1 = NULL;

    int ok0 = pick_event(ins_candidates, &code0, &name0);
    int ok1 = pick_event(cyc_candidates, &code1, &name1);

    if (!ok0 && !ok1) {
        fprintf(stderr, "PAPI: no supported events on this host. Disabling.\n");
        g_papi_ok = 0;
        return;
    }

    if (ok0) { g_codes[g_num_events] = code0; g_names[g_num_events] = name0; g_num_events++; }
    if (ok1 && g_num_events < 2) { g_codes[g_num_events] = code1; g_names[g_num_events] = name1; g_num_events++; }

    g_papi_ok = 1;
}

void papi_start_counters(void)
{
    if (papi_env_disabled()) return;
    if (!g_papi_ok) return;

    // Already started for this thread
    if (t_event_set != PAPI_NULL) return;

    // Register this thread (safe even if it’s the main thread)
    int ret = PAPI_register_thread();
    if (ret != PAPI_OK) {
        // Don’t kill the program; just disable counters for this thread
        fprintf(stderr, "PAPI_register_thread failed: %s\n", PAPI_strerror(ret));
        return;
    }

    t_event_set = PAPI_NULL;
    ret = PAPI_create_eventset(&t_event_set);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_create_eventset failed: %s\n", PAPI_strerror(ret));
        t_event_set = PAPI_NULL;
        return;
    }

    for (int i = 0; i < g_num_events; i++) {
        ret = PAPI_add_event(t_event_set, g_codes[i]);
        if (ret != PAPI_OK) {
            fprintf(stderr, "PAPI_add_event failed (%s): %s\n",
                    g_names[i] ? g_names[i] : "event", PAPI_strerror(ret));
            PAPI_destroy_eventset(&t_event_set);
            t_event_set = PAPI_NULL;
            return;
        }
    }

    ret = PAPI_start(t_event_set);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_start failed: %s\n", PAPI_strerror(ret));
        PAPI_destroy_eventset(&t_event_set);
        t_event_set = PAPI_NULL;
        return;
    }
}

void papi_stop_and_accum(void)
{
    if (papi_env_disabled()) return;
    if (!g_papi_ok) return;
    if (t_event_set == PAPI_NULL) return;

    long long vals[2] = {0, 0};
    int ret = PAPI_stop(t_event_set, vals);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_stop failed: %s\n", PAPI_strerror(ret));
    } else {
        pthread_mutex_lock(&g_tot_mu);
        if (g_num_events >= 1) g_totals[0] += vals[0];
        if (g_num_events >= 2) g_totals[1] += vals[1];
        pthread_mutex_unlock(&g_tot_mu);
    }

    PAPI_cleanup_eventset(t_event_set);
    PAPI_destroy_eventset(&t_event_set);
    t_event_set = PAPI_NULL;

    // Unregister thread to be clean
    PAPI_unregister_thread();
}

#else

void papi_init_or_die(void) {}
void papi_start_counters(void) {}
void papi_stop_and_accum(void) {}
void papi_reset_totals(void) {}
void papi_print_totals(void) {}

#endif

