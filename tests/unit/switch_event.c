#include <stdio.h>
#include <switch.h>
#include <tap.h>

// #define BENCHMARK 1

int main () {
  switch_event_t *event = NULL;
  switch_bool_t verbose = SWITCH_TRUE;
  const char *err = NULL;
  switch_time_t start_ts, end_ts;
  int rc = 0, loops = 10, x = 0;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  char **index = NULL;
  unsigned long long micro_total = 0;
  double micro_per = 0;
  double rate_per_sec = 0;

#ifdef BENCHMARK
  switch_time_t small_start_ts, small_end_ts;

  plan(2);
#else
  plan(2 + ( 2 * loops));
#endif

  status = switch_core_init(SCF_MINIMAL, verbose, &err);
  
  if ( !ok( status == SWITCH_STATUS_SUCCESS, "Initialize FreeSWITCH core\n")) {
    bail_out(0, "Bail due to failure to initialize FreeSWITCH[%s]", err);
  }

  index = calloc(loops, sizeof(char *));
  for ( x = 0; x < loops; x++) {
    index[x] = switch_mprintf("%d", x);
  }

  /* START LOOPS */
  start_ts = switch_time_now();
  
  status = switch_event_create(&event, SWITCH_EVENT_MESSAGE);
  ok( status == SWITCH_STATUS_SUCCESS,"Create Event");

#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    status = switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, index[x], index[x]);
    ok( status == SWITCH_STATUS_SUCCESS,"Add header to event");
  }
#else 
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, index[x], index[x]) != SWITCH_STATUS_SUCCESS) {
      fail("Failed to add header to event");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  note("switch_event add_header: Total %ldus / %ld loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);
#endif


#ifndef BENCHMARK
  for ( x = 0; x < loops; x++) {
    is(switch_event_get_header(event, index[x]), index[x], "correct header value returned");
  } 
#else 
  small_start_ts = switch_time_now();
  for ( x = 0; x < loops; x++) {
    if ( !switch_event_get_header(event, index[x])) {
      fail("Failed to lookup event header value");
    }
  }
  small_end_ts = switch_time_now();

  micro_total = small_end_ts - small_start_ts;
  micro_per = micro_total / (double) loops;
  rate_per_sec = 1000000 / micro_per;
  note("switch_event get_header: Total %ldus / %ld loops, %.2f us per loop, %.0f loops per second\n", 
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
  diag("switch_event Total %ldus / %d loops, %.2f us per loop, %.0f loops per second\n", 
       micro_total, loops, micro_per, rate_per_sec);

  switch_core_destroy();

  done_testing();
}
