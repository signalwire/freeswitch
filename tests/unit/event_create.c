#include <stdio.h>
#include <switch.h>
#include <tap.h>

int main () {

  switch_event_t *event = NULL;
  switch_bool_t verbose = SWITCH_TRUE;
  const char *err = NULL;
  switch_time_t start_ts, end_ts;
  int rc = 0, loops = 1000;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  plan(1 + ( 3 * loops));

  
  status = switch_core_init(SCF_MINIMAL, verbose, &err);
  
  if ( !ok( status == SWITCH_STATUS_SUCCESS, "Initialize FreeSWITCH core\n")) {
    bail_out(0, "Bail due to failure to initialize FreeSWITCH[%s]", err);
  }

  /* START LOOPS */
  start_ts = switch_time_now();
  
  for ( int x = 0; x < loops; x++) {
    status = switch_event_create(&event, SWITCH_EVENT_MESSAGE);
    ok( status == SWITCH_STATUS_SUCCESS,"Create Event");
    
    status = switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "testing", "event_create");
    ok( status == SWITCH_STATUS_SUCCESS,"Add header to event");
    
    is(switch_event_get_header(event, "testing"), "event_create", "correct header value returned");
    
    switch_event_destroy(&event);
  } /* END LOOPS */
  
  end_ts = switch_time_now();

  note("Total %ldus, %ldus per loop, %ld loops per second\n", end_ts - start_ts,(end_ts - start_ts) / loops, 1000000/ ((end_ts - start_ts) / loops));

  switch_core_destroy();

  done_testing();
}
