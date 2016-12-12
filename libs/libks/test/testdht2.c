#include <ks.h>
#include <../dht/ks_dht.h>
#include <../dht/ks_dht-int.h>
#include <tap.h>

#define TEST_DHT1_REGISTER_TYPE_BUFFER "d1:ad2:id20:12345678901234567890e1:q4:ping1:t2:421:y1:ze"
#define TEST_DHT1_PROCESS_QUERY_PING_BUFFER "d1:ad2:id20:12345678901234567890e1:q4:ping1:t2:421:y1:qe"

ks_status_t dht_z_callback(ks_dht_t *dht, ks_dht_message_t *message)
{
	diag("dht_z_callback\n");
	ok(message->transactionid[0] == '4' && message->transactionid[1] == '2');
	ks_dht_send_error(dht, message->endpoint, &message->raddr, message->transactionid, message->transactionid_length, 201, "Generic test error");
	return KS_STATUS_SUCCESS;
}

int main() {
	//ks_size_t buflen;
  ks_status_t err;
  int mask = 0;
  ks_dht_t *dht1 = NULL;
  ks_dht_t dht2;
  ks_dht_t *dht3 = NULL;
  ks_dht_endpoint_t *ep1;
  ks_dht_endpoint_t *ep2;
  ks_dht_endpoint_t *ep3;
  ks_bool_t have_v4, have_v6;
  char v4[48] = {0}, v6[48] = {0};
  ks_sockaddr_t addr;
  ks_sockaddr_t raddr1;
  //ks_sockaddr_t raddr2;
  //ks_sockaddr_t raddr3;

  err = ks_init();
  ok(!err);

  ks_global_set_default_logger(7);

  err = ks_find_local_ip(v4, sizeof(v4), &mask, AF_INET, NULL);
  ok(err == KS_STATUS_SUCCESS);
  have_v4 = !zstr_buf(v4);
  
  //err = ks_find_local_ip(v6, sizeof(v6), NULL, AF_INET6, NULL);
  //ok(err == KS_STATUS_SUCCESS);
  have_v6 = KS_FALSE;//!zstr_buf(v6);

  ok(have_v4 || have_v6);

  if (have_v4) {
	  diag("Binding to %s on ipv4\n", v4);
  }
  if (have_v6) {
	  diag("Binding to %s on ipv6\n", v6);
  }

  err = ks_dht_alloc(&dht1, NULL);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_dht_init(dht1);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht_prealloc(&dht2, dht1->pool);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_dht_init(&dht2);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht_alloc(&dht3, NULL);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_dht_init(dht3);
  ok(err == KS_STATUS_SUCCESS);

  
  ks_dht_register_type(dht1, "z", dht_z_callback);
  
  if (have_v4) {
    err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
    err = ks_dht_bind(dht1, NULL, &addr, &ep1);
    ok(err == KS_STATUS_SUCCESS);

	raddr1 = addr;
	
	err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT + 1, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
	err = ks_dht_bind(&dht2, NULL, &addr, &ep2);
	ok(err == KS_STATUS_SUCCESS);

	//raddr2 = addr;

	err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT + 2, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
	err = ks_dht_bind(dht3, NULL, &addr, &ep3);
	ok(err == KS_STATUS_SUCCESS);

	//raddr3 = addr;
  }

  if (have_v6) {
	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);
	  
    err = ks_dht_bind(dht1, NULL, &addr, NULL);
    ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT + 1, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht_bind(&dht2, NULL, &addr, NULL);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT + 2, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht_bind(dht3, NULL, &addr, NULL);
	ok(err == KS_STATUS_SUCCESS);
  }

  //diag("Custom type tests\n");
  
  //buflen = strlen(TEST_DHT1_REGISTER_TYPE_BUFFER);
  //memcpy(dht1->recv_buffer, TEST_DHT1_REGISTER_TYPE_BUFFER, buflen);
  //dht1->recv_buffer_length = buflen;

  //err = ks_dht_process(dht1, ep1, &raddr);
  //ok(err == KS_STATUS_SUCCESS);

  //ks_dht_pulse(dht1, 100);

  //ks_dht_pulse(&dht2, 100);

  
  //buflen = strlen(TEST_DHT1_PROCESS_QUERY_PING_BUFFER);
  //memcpy(dht1->recv_buffer, TEST_DHT1_PROCESS_QUERY_PING_BUFFER, buflen);
  //dht1->recv_buffer_length = buflen;

  //err = ks_dht_process(dht1, &raddr);
  //ok(err == KS_STATUS_SUCCESS);

  
  diag("Ping test\n");
  
  ks_dht_send_ping(&dht2, ep2, &raddr1); // Queue ping from dht2 to dht1

  ks_dht_pulse(&dht2, 100); // Send queued ping from dht2 to dht1
  
  ks_dht_pulse(dht1, 100); // Receive and process ping query from dht2, queue and send ping response

  ks_dht_pulse(&dht2, 100); // Receive and process ping response from dht1

  // Test blind find_node from dht3 to dht1 to find dht2 nodeid

  diag("Find_Node test\n");

  ks_dht_send_findnode(dht3, ep3, &raddr1, &ep2->nodeid); // Queue findnode from dht3 to dht1

  ks_dht_pulse(dht3, 100); // Send queued findnode from dht3 to dht1

  ks_dht_pulse(dht1, 100); // Receive and process findnode query from dht3, queue and send findnode response

  ks_dht_pulse(dht3, 100); // Receive and process findnode response from dht1

  ok(ks_dhtrt_find_node(dht3->rt_ipv4, ep2->nodeid) != NULL);
  
  diag("Cleanup\n");
  /* Cleanup and shutdown */

  err = ks_dht_deinit(dht3);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht_free(&dht3);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht_deinit(&dht2);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht_deinit(dht1);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht_free(&dht1);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_shutdown();
  ok(err == KS_STATUS_SUCCESS);
  
  done_testing();
}
