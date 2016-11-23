#include <ks.h>
#include <tap.h>

/* 
   Test should cover end to end DHT functionality that isn't covered by a more specific test file
   * Find ip
   * init 2 or more clients(with dedicated ports)
   * add ip to clients
   * exchange peers between clients
   * shutdown clients
   * cleanup ks
 */

int main() {
  int err = 0;
  char v4[48] = {0}, v6[48] = {0};
  int mask = 0, have_v4 = 0, have_v6 = 0;
  int A_port = 5998, B_port = 5999;
  dht_handle_t *A_h = NULL, *B_h = NULL;
  ks_dht_af_flag_t af_flags = 0;
  static ks_sockaddr_t bootstrap[1];
  
  err = ks_init();
  ok(!err);

  err = ks_find_local_ip(v4, sizeof(v4), &mask, AF_INET, NULL);
  ok(err == KS_STATUS_SUCCESS);
  have_v4 = !zstr_buf(v4);
  
  err = ks_find_local_ip(v6, sizeof(v6), NULL, AF_INET6, NULL);
  ok(err == KS_STATUS_SUCCESS);

  have_v6 = !zstr_buf(v6);

  ok(have_v4 || have_v6);
  if (have_v4) {
    af_flags |= KS_DHT_AF_INET4;
  }
  diag("Adding local bind ipv4 of (%s) %d\n", v4, have_v4);

  if (have_v6) {
    af_flags |= KS_DHT_AF_INET6;
  }
  diag("Adding local bind ipv6 of (%s) %d\n", v6, have_v6);

  err = ks_dht_init(&A_h, af_flags, NULL, A_port);
  ok(err == KS_STATUS_SUCCESS);

  if (have_v4) {
    err = ks_dht_add_ip(A_h, v4, A_port);
    ok(err == KS_STATUS_SUCCESS);
  }

  if (have_v6) {
    err = ks_dht_add_ip(A_h, v6, A_port);
    ok(err == KS_STATUS_SUCCESS);
  }

  err = ks_dht_init(&B_h, af_flags, NULL, B_port);
  ok(err == KS_STATUS_SUCCESS);

  if (have_v4) {
    err = ks_dht_add_ip(B_h, v4, B_port);
    ok(err == KS_STATUS_SUCCESS);
  }

  if (have_v6) {
    err = ks_dht_add_ip(B_h, v6, B_port);
    ok(err == KS_STATUS_SUCCESS);
  }

  ks_dht_start(A_h);
  ks_dht_start(B_h);

  ks_addr_set(&bootstrap[0], v4, B_port, 0);

  /* Have A ping B */
  dht_ping_node(A_h, &bootstrap[0]);

  /* Start test series */

  /* Absent in Test and Example App */
  /*
     This function is called from the test app, with the intent of processing and handling network packets(buf, buflen, from).
     Tests for this function should include successful processing of new inbound messages, as well as validation of bad inbound messages.
     KS_DECLARE(int) dht_periodic(dht_handle_t *h, const void *buf, size_t buflen, ks_sockaddr_t *from); */
  /*
     This function is like the dht_ping_node, except it only adds the node, and waits for dht_periodic to decide when to ping the node.
     Doing a node ping first confirms that we have working networking to the new remote node.
     KS_DECLARE(int) dht_insert_node(dht_handle_t *h, const unsigned char *id, ks_sockaddr_t *sa); */
  /*
     Queries for node stats. Will be used for validating that a node was successfully added. Call before the ping, ping, call after, and compare.
     KS_DECLARE(int) dht_nodes(dht_handle_t *h, int af, int *good_return, int *dubious_return, int *cached_return, int *incoming_return); */
  /*
     Sets(or changes?) the local DHT listening port. Would be very interesting to see what happens if this is called after nodes are pinged.
     KS_DECLARE(void) ks_dht_set_port(dht_handle_t *h, unsigned int port); */


  /* Present in Example App but Absent in Test */
  /*
     ks_dht_send_message_mutable_cjson(h, alice_secretkey, alice_publickey, NULL, message_id, 1, output, 600); */
  /*
     ks_separate_string(cmd_dup, " ", argv, (sizeof(argv) / sizeof(argv[0]))); */
  /*
     ks_dht_api_find_node(h, argv[2], argv[3], ipv6); */
  /*
     ks_global_set_default_logger(atoi(line + 9)); */
  /*
     ks_dht_set_param(h, DHT_PARAM_AUTOROUTE, KS_TRUE); */
  /*
     Like dht_periodic, except executes only one loop of work.
     ks_dht_one_loop(h, 0); */
  /*
     Returns a list of local bindings. Most useful after the DHT_PARAM_AUTOROUTE to return which routes it bound to.
     ks_dht_get_bind_addrs(h, &bindings, &len); */
  /*
     Callback for different message type actions. Called from the dht_periodic functions.
     ks_dht_set_callback(h, callback, NULL); */
  /*
     Executes a search for a particular SHA hash. Pings known nodes to see if they have the hash. callback is called with results.
     dht_search(h, hash, globals.port, AF_INET, callback, NULL); */
  /*
     Print the contents of the 'dht tables' to stdout. Need a version that gets info in a testable format.
     dht_dump_tables(h, stdout); */
  /* dht_get_nodes(h, sin, &num, sin6, &num6); */
  /*
     Shuts down the DHT handle, and should properly clean up.
     dht_uninit(&h); */

  /* Cleanup and shutdown */

  todo("ks_dht_stop()");
  todo("ks_dht_destroy()");
  
  err = ks_shutdown();
  ok(!err);
  
  done_testing();
}
