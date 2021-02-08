#include <switch.h>
#include <test/switch_test.h>

// #define BENCHMARK 1

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_hash)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(benchmark)
{
  switch_time_t start_ts, end_ts;
  uint64_t micro_total = 0;
  double micro_per = 0;
  double rate_per_sec = 0;
  int x = 0;

#ifdef BENCHMARK
  switch_time_t small_start_ts, small_end_ts;
#endif

  int loops = 10;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  char **index = NULL;
  switch_hash_t *hash = NULL;

  fst_requires(switch_core_hash_init(&hash) == SWITCH_STATUS_SUCCESS);

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
    fst_xcheck(status == SWITCH_STATUS_SUCCESS, "Failed to insert into the hash");
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
  printf("switch_hash insert: Total %ldus / %ld loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif


  /* Lookup */
#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    char *data = NULL;
    data = switch_core_hash_find(hash, index[x]);
    fst_xcheck(data != NULL, "Lookup failed");
    fst_check_string_equals( index[x], data);
  }
#else
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( ! switch_core_hash_find(hash, index[x])) {
      fst_fail("Failed to properly locate one of the values");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  printf("switch_hash find: Total %ldus / %ld loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif


  /* Delete */
#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    char *data = NULL;
    data = switch_core_hash_delete(hash, index[x]);
    fst_xcheck(data != NULL, "Delete from the hash");
    fst_check_string_equals( index[x], data );
  }
#else
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( !switch_core_hash_delete(hash, index[x])) {
      fst_fail("Failed to delete and return the value");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  printf("switch_hash delete: Total %ldus / %d loops, %.2f us per loop, %.0f loops per second\n", 
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
  printf("switch_hash Total %" SWITCH_UINT64_T_FMT "us / %d loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()

