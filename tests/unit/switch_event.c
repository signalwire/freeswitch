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

#ifdef BENCHMARK
FST_TEST_BEGIN(dup_uniq_bench)
{
  switch_event_t *src = NULL, *dup = NULL;
  switch_status_t status;
  const int loops = 4000;
  const int Ns[4] = {50, 100, 191, 400};
  int i, j, count, k, S, Nv;
  char name[32], val[64];
  switch_event_header_t *hp;
  switch_time_t start_ts, end_ts;
  double per_us;

  /* A channel's variables are a SWITCH_EVENT_CHANNEL_DATA event => EF_UNIQ_HEADERS.
   * switch_event_create() also adds the standard event headers, so the source
   * count is (191 added + standard); assert relative to the actual source count. */
  status = switch_event_create(&src, SWITCH_EVENT_CHANNEL_DATA);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "create CHANNEL_DATA event");
  fst_xcheck(switch_test_flag(src, EF_UNIQ_HEADERS) != 0, "CHANNEL_DATA event is EF_UNIQ_HEADERS");

  for (i = 0; i < 191; i++) {
    switch_snprintf(name, sizeof(name), "var_%d", i);
    switch_snprintf(val, sizeof(val), "value_%d_some_padding_payload", i);
    switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, name, val);
  }
  S = 0;
  for (hp = src->headers; hp; hp = hp->next) {
    S++;
  }
  fst_xcheck(S >= 191, "source has at least the 191 added headers");

  /* correctness: dup preserves header count, the uniq flag, and values */
  status = switch_event_dup(&dup, src);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "dup ok");
  count = 0;
  for (hp = dup->headers; hp; hp = hp->next) {
    count++;
  }
  fst_xcheck(count == S, "dup header count equals source");
  fst_xcheck(switch_test_flag(dup, EF_UNIQ_HEADERS) != 0, "dup keeps EF_UNIQ_HEADERS");
  fst_check_string_equals(switch_event_get_header(dup, "var_0"), "value_0_some_padding_payload");
  fst_check_string_equals(switch_event_get_header(dup, "var_190"), "value_190_some_padding_payload");
  switch_event_destroy(&dup);

  /* correctness: uniqueness preserved (re-setting a key must not duplicate it in the dup) */
  switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, "var_0", "REPLACED");
  status = switch_event_dup(&dup, src);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "dup ok (2)");
  k = 0;
  for (hp = dup->headers; hp; hp = hp->next) {
    if (!strcmp(hp->name, "var_0")) {
      k++;
    }
  }
  fst_xcheck(k == 1, "var_0 present exactly once after re-set + dup");
  fst_check_string_equals(switch_event_get_header(dup, "var_0"), "REPLACED");
  switch_event_destroy(&dup);
  switch_event_destroy(&src);

  /* timing sweep: dup cost vs header count (baseline ~O(n^2), patched ~O(n)) */
  for (j = 0; j < 4; j++) {
    Nv = Ns[j];
    switch_event_create(&src, SWITCH_EVENT_CHANNEL_DATA);
    for (i = 0; i < Nv; i++) {
      switch_snprintf(name, sizeof(name), "var_%d", i);
      switch_snprintf(val, sizeof(val), "value_%d_some_padding_payload", i);
      switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, name, val);
    }
    start_ts = switch_time_now();
    for (i = 0; i < loops; i++) {
      switch_event_dup(&dup, src);
      switch_event_destroy(&dup);
    }
    end_ts = switch_time_now();
    per_us = (double)(end_ts - start_ts) / (double)loops;
    printf("DUP_BENCH N=%d loops=%d total=%" SWITCH_UINT64_T_FMT "us per_dup=%.3f us\n",
           Nv, loops, (uint64_t)(end_ts - start_ts), per_us);
    switch_event_destroy(&src);
  }
}
FST_TEST_END()
#endif /* BENCHMARK */

FST_TEST_BEGIN(dup_faithful_copy)
{
  /* Regression for the switch_event_dup O(n^2)->O(n) change (suppressing the
   * per-add EF_UNIQ_HEADERS dedup scan while cloning). dup must reproduce the
   * source faithfully:
   *   (1) a well-formed EF_UNIQ source stays unique through dup (the real case);
   *   (2) a malformed source that already holds duplicate names (only possible
   *       if headers were added before EF_UNIQ_HEADERS was set) is copied
   *       faithfully -- dup PRESERVES the duplicates rather than silently
   *       collapsing to the last value. A dup-bearing EF_UNIQ event is already
   *       a bug upstream of here; a copy must not silently drop data. */
  switch_event_t *src = NULL, *dup = NULL;
  switch_event_header_t *hp;
  switch_status_t status;
  int n;

  /* (1) well-formed EF_UNIQ source -> unique dup */
  status = switch_event_create(&src, SWITCH_EVENT_CHANNEL_DATA);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "create CHANNEL_DATA");
  fst_xcheck(switch_test_flag(src, EF_UNIQ_HEADERS) != 0, "CHANNEL_DATA is EF_UNIQ");
  switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, "k", "v1");
  switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, "k", "v2");
  n = 0;
  for (hp = src->headers; hp; hp = hp->next) {
    if (!strcmp(hp->name, "k")) {
      n++;
    }
  }
  fst_xcheck(n == 1, "EF_UNIQ source keeps 'k' unique (collapsed to last)");
  status = switch_event_dup(&dup, src);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "dup ok (unique)");
  n = 0;
  for (hp = dup->headers; hp; hp = hp->next) {
    if (!strcmp(hp->name, "k")) {
      n++;
    }
  }
  fst_xcheck(n == 1, "dup of unique source stays unique");
  fst_check_string_equals(switch_event_get_header(dup, "k"), "v2");
  switch_event_destroy(&dup);
  switch_event_destroy(&src);

  /* (2) malformed source (duplicate names under EF_UNIQ) -> faithful copy */
  status = switch_event_create(&src, SWITCH_EVENT_GENERAL);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "create GENERAL");
  fst_xcheck(switch_test_flag(src, EF_UNIQ_HEADERS) == 0, "GENERAL is not EF_UNIQ");
  switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, "dupkey", "first");
  switch_event_add_header_string(src, SWITCH_STACK_BOTTOM, "dupkey", "second");
  switch_set_flag(src, EF_UNIQ_HEADERS);
  n = 0;
  for (hp = src->headers; hp; hp = hp->next) {
    if (!strcmp(hp->name, "dupkey")) {
      n++;
    }
  }
  fst_xcheck(n == 2, "malformed source holds 2 'dupkey' headers");
  status = switch_event_dup(&dup, src);
  fst_xcheck(status == SWITCH_STATUS_SUCCESS, "dup ok (malformed)");
  fst_xcheck(switch_test_flag(dup, EF_UNIQ_HEADERS) != 0, "dup keeps EF_UNIQ flag");
  n = 0;
  for (hp = dup->headers; hp; hp = hp->next) {
    if (!strcmp(hp->name, "dupkey")) {
      n++;
    }
  }
  fst_xcheck(n == 2, "dup faithfully preserves both 'dupkey' headers (no silent collapse)");
  switch_event_destroy(&dup);
  switch_event_destroy(&src);
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()



