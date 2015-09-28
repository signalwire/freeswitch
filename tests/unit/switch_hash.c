#include <stdio.h>
#include <switch.h>
#include <tap.h>

// #define BENCHMARK 1

int main () {

  switch_event_t *event = NULL;
  switch_bool_t verbose = SWITCH_TRUE;
  const char *err = NULL;
  switch_time_t start_ts, end_ts;
  unsigned long long micro_total = 0;
  double micro_per = 0;
  double rate_per_sec = 0;
  int x = 0;

#ifdef BENCHMARK
  switch_time_t small_start_ts, small_end_ts;
#endif

  int rc = 0, loops = 10;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  char **index = NULL;
  switch_hash_t *hash = NULL;

#ifndef BENCHMARK
  plan(2 + ( 5 * loops));
#else
  plan(2);
#endif

  status = switch_core_init(SCF_MINIMAL, verbose, &err);
  
  if ( !ok( status == SWITCH_STATUS_SUCCESS, "Initialize FreeSWITCH core\n")) {
    bail_out(0, "Bail due to failure to initialize FreeSWITCH[%s]", err);
  }

  status = switch_core_hash_init(&hash);

  if ( !ok(status == SWITCH_STATUS_SUCCESS, "Create a new hash")) {
    bail_out(0, "Bail due to failure to create hash");
  }

  index = calloc(loops, sizeof(char *));
  for ( x = 0; x < loops; x++) {
    index[x] = switch_mprintf("%d", x);
  }

  /* START LOOPS */
  start_ts = switch_time_now();

  /* Insertion */
#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    status = switch_core_hash_insert(hash, index[x], (void *) index[x]);
    ok(status == SWITCH_STATUS_SUCCESS, "Insert into the hash");
  }
#else 
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    switch_core_hash_insert(hash, index[x], (void *) index[x]);
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  note("switch_hash insert: Total %ldus / %ld loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif


  /* Lookup */
#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    char *data = NULL;
    data = switch_core_hash_find(hash, index[x]);
    ok(data != NULL, "Successful lookup");
    is( index[x], data, "Returned correct data");
  }
#else
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( ! switch_core_hash_find(hash, index[x])) {
      fail("Failed to properly locate one of the values");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  note("switch_hash find: Total %ldus / %ld loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif


  /* Delete */
#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    char *data = NULL;
    data = switch_core_hash_delete(hash, index[x]);
    ok(data != NULL, "Create a new hash");
    is( index[x], data, "Returned correct data");
  }
#else
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( !switch_core_hash_delete(hash, index[x])) {
      fail("Failed to delete and return the value");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  note("switch_hash delete: Total %ldus / %d loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif


  end_ts = switch_time_now();
  /* END LOOPS */

  switch_core_hash_destroy(&hash);
  for ( x = 0; x < loops; x++) {
    free(index[x]);
  }
  free(index);

  micro_total = end_ts - start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  diag("switch_hash Total %ldus / %d loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);

  switch_core_destroy();

  done_testing();
}
