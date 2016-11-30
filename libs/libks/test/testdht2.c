#include <ks.h>
#include <../dht/ks_dht.h>
#include <tap.h>

int main() {
  ks_status_t err;
  int mask = 0;
  ks_dht2_t *dht1 = NULL;
  ks_dht2_t dht2;
  ks_bool_t have_v4, have_v6;
  char v4[48] = {0}, v6[48] = {0};
  ks_sockaddr_t addr;
  
  err = ks_init();
  ok(!err);

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
  
  if (have_v4) {
    err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
    err = ks_dht2_bind(dht1, &addr);
    ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT + 1, AF_INET);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht2_bind(&dht2, &addr);
	ok(err == KS_STATUS_SUCCESS);
  }

  if (have_v6) {
	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);
	  
    err = ks_dht2_bind(dht1, &addr);
    ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT + 1, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht2_bind(&dht2, &addr);
	ok(err == KS_STATUS_SUCCESS);
  }


  
  

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
