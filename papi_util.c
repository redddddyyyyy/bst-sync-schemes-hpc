#include <stdio.h>
#include <papi.h>

/*
 * Simple PAPI wrapper that NEVER aborts the program.
 * If PAPI doesn't work on this machine, we just print a warning
 * and let the benchmark continue normally.
 */

static int g_event_set = PAPI_NULL;
static int g_papi_ok   = 0;   // 0 = PAPI disabled, 1 = usable

void papi_init_or_die(void)
{
    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        fprintf(stderr,
                "PAPI_library_init failed (ret=%d). Disabling PAPI counters.\n",
                ret);
        g_papi_ok = 0;
        return;
    }

    g_papi_ok = 1;
}

void papi_start_counters(void)
{
    if (!g_papi_ok) {
        // PAPI not available; nothing to do.
        return;
    }

    int ret;
    int event_code;

    g_event_set = PAPI_NULL;

    ret = PAPI_event_name_to_code("PAPI_TOT_INS", &event_code);
    if (ret != PAPI_OK) {
        fprintf(stderr,
                "PAPI_event_name_to_code failed for 'ix86arch::UNHALTED_CORE_CYCLES': %s\n",
                PAPI_strerror(ret));
        g_papi_ok = 0;   // disable further PAPI attempts
        return;
    }

    ret = PAPI_create_eventset(&g_event_set);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_create_eventset failed: %s\n", PAPI_strerror(ret));
        g_event_set = PAPI_NULL;
        g_papi_ok = 0;
        return;
    }

    ret = PAPI_add_event(g_event_set, event_code);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_add_event failed: %s\n", PAPI_strerror(ret));
        PAPI_destroy_eventset(&g_event_set);
        g_event_set = PAPI_NULL;
        g_papi_ok = 0;
        return;
    }

    ret = PAPI_start(g_event_set);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_start failed: %s\n", PAPI_strerror(ret));
        PAPI_destroy_eventset(&g_event_set);
        g_event_set = PAPI_NULL;
        g_papi_ok = 0;
        return;
    }
}

void papi_stop_and_print(void)
{
    if (!g_papi_ok || g_event_set == PAPI_NULL) {
        // Counters were never started or PAPI disabled
        fprintf(stderr,
                "PAPI_stop: no active event set; skipping hardware counter report.\n");
        return;
    }

    long long counts = 0;
    int ret = PAPI_stop(g_event_set, &counts);
    if (ret != PAPI_OK) {
        fprintf(stderr, "PAPI_stop failed: %s\n", PAPI_strerror(ret));
    } else {
        printf("ix86arch::UNHALTED_CORE_CYCLES: %lld\n", counts);
    }

    PAPI_cleanup_eventset(&g_event_set);
    PAPI_destroy_eventset(&g_event_set);
    g_event_set = PAPI_NULL;
}

