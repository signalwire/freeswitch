#include <switch.h>
#include <test/switch_test.h>

// #define BENCHMARK 1

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_event)

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
  switch_event_t *event = NULL;
  switch_time_t start_ts, end_ts;
  int loops = 10, x = 0;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  char **index = NULL;
  uint64_t micro_total = 0;
  double micro_per = 0;
  double rate_per_sec = 0;

#ifdef BENCHMARK
  switch_time_t small_start_ts, small_end_ts;
#endif

  index = calloc(loops, sizeof(char *));
  for ( x = 0; x < loops; x++) {
    index[x] = switch_mprintf("%d", x);
  }

  /* START LOOPS */
  start_ts = switch_time_now();
  
  status = switch_event_create(&event, SWITCH_EVENT_MESSAGE);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "Failed to create event");

#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    status = switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, index[x], index[x]);
    fst_xcheck(status == SWITCH_STATUS_SUCCESS, "Failed to add header to event");
  }
#else 
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, index[x], index[x]) != SWITCH_STATUS_SUCCESS) {
      fst_fail("Failed to add header to event");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  printf("switch_event add_header: Total %" SWITCH_UINT64_T_FMT "us / %d loops, %.2f us per loop, %.0f loops per second\n",
       micro_total, loops, micro_per, rate_per_sec);
#endif


#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    fst_check_string_equals(switch_event_get_header(event, index[x]), index[x]);
  } 
#else 
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( !switch_event_get_header(event, index[x])) {
      fst_fail("Failed to lookup event header value");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  printf("switch_event get_header: Total %" SWITCH_UINT64_T_FMT "us / %d loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif

  switch_event_destroy(&event);
  /* END LOOPS */
  
  end_ts = switch_time_now();

  for ( x = 0; x < loops; x++) {
    free(index[x]);
  }
  free(index);

  micro_total = end_ts - start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  printf("switch_event Total %" SWITCH_UINT64_T_FMT "us / %d loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);

}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()



