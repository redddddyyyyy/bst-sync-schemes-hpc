#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void papi_init_or_die(void);
void papi_start_counters(void);

void papi_stop_and_accum(void);
void papi_reset_totals(void);
void papi_print_totals(void);

#ifdef __cplusplus
}
#endif

