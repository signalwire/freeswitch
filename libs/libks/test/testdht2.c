#include <ks.h>
#include <../dht/ks_dht.h>
#include <../dht/ks_dht-int.h>
#include <tap.h>

#define TEST_DHT1_REGISTER_TYPE_BUFFER "d1:ad2:id20:12345678901234567890e1:q4:ping1:t2:421:y1:ze"
#define TEST_DHT1_PROCESS_QUERY_PING_BUFFER "d1:ad2:id20:12345678901234567890e1:q4:ping1:t2:421:y1:qe"

ks_status_t dht_z_callback(ks_dht2_t *dht, ks_sockaddr_t *raddr, ks_dht2_message_t *message)
{
	diag("dht_z_callback\n");
	ok(message->transactionid[0] == '4' && message->transactionid[1] == '2');
	ks_dht2_send_error(dht, raddr, message->transactionid, message->transactionid_length, 201, "Generic test error");
	return KS_STATUS_SUCCESS;
}

int main() {
  ks_size_t buflen;
  ks_status_t err;
  int mask = 0;
  ks_dht2_t *dht1 = NULL;
  ks_dht2_t dht2;
  ks_bool_t have_v4, have_v6;
  char v4[48] = {0}, v6[48] = {0};
  ks_sockaddr_t addr;
  ks_sockaddr_t raddr;
  
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

  err = ks_dht2_alloc(&dht1, NULL);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_dht2_init(dht1, NULL);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht2_prealloc(&dht2, dht1->pool);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_dht2_init(&dht2, NULL);
  ok(err == KS_STATUS_SUCCESS);

  ks_dht2_register_type(dht1, "z", dht_z_callback);
  
  if (have_v4) {
    err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
    err = ks_dht2_bind(dht1, &addr, NULL);
    ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT + 1, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
	err = ks_dht2_bind(&dht2, &addr, NULL);
	ok(err == KS_STATUS_SUCCESS);

	raddr = addr;
  }

  if (have_v6) {
	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);
	  
    err = ks_dht2_bind(dht1, &addr, NULL);
    ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT + 1, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht2_bind(&dht2, &addr, NULL);
	ok(err == KS_STATUS_SUCCESS);
  }

  buflen = strlen(TEST_DHT1_REGISTER_TYPE_BUFFER);
  memcpy(dht1->recv_buffer, TEST_DHT1_REGISTER_TYPE_BUFFER, buflen);
  dht1->recv_buffer_length = buflen;

  err = ks_dht2_process(dht1, &raddr);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht2_pulse(&dht2, 1000);
  ok(err == KS_STATUS_SUCCESS);

  //buflen = strlen(TEST_DHT1_PROCESS_QUERY_PING_BUFFER);
  //memcpy(dht1->recv_buffer, TEST_DHT1_PROCESS_QUERY_PING_BUFFER, buflen);
  //dht1->recv_buffer_length = buflen;

  //err = ks_dht2_process(dht1, &raddr);
  //ok(err == KS_STATUS_SUCCESS);
  
  err = ks_dht2_send_query_ping(dht1, &raddr);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht2_pulse(&dht2, 1000);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht2_pulse(dht1, 1000);
  ok(err == KS_STATUS_SUCCESS);

  diag("Cleanup\n");
  /* Cleanup and shutdown */

  err = ks_dht2_deinit(&dht2);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht2_deinit(dht1);
  ok(err == KS_STATUS_SUCCESS);

  err = ks_dht2_free(dht1);
  ok(err == KS_STATUS_SUCCESS);
  
  err = ks_shutdown();
  ok(err == KS_STATUS_SUCCESS);
  
  done_testing();
}
