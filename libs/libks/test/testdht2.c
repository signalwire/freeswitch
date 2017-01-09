#include <ks.h>
#include <ks_dht.h>
#include <ks_dht-int.h>
#include <tap.h>

ks_dht_storageitem_skey_t sk;
ks_dht_storageitem_pkey_t pk;

ks_status_t dht2_updated_callback(ks_dht_t *dht, ks_dht_storageitem_t *item)
{
	diag("dht2_updated_callback\n");
	return KS_STATUS_SUCCESS;
}

ks_status_t dht2_distribute_callback(ks_dht_t *dht, ks_dht_storageitem_t *item)
{
	diag("dht2_distribute_callback\n");
	return KS_STATUS_SUCCESS;
}

ks_status_t dht2_put_callback(ks_dht_t *dht, ks_dht_job_t *job)
{
	diag("dht2_put_callback\n");
	return KS_STATUS_SUCCESS;
}

ks_status_t dht2_get_token_callback(ks_dht_t *dht, ks_dht_job_t *job)
{
	char buf[KS_DHT_TOKEN_SIZE * 2 + 1];
	const char *v = "Hello World!";
	size_t v_len = strlen(v);
	ks_dht_storageitem_signature_t sig;
	ks_dht_storageitem_t *mutable = NULL;
	
	diag("dht2_get_token_callback %s\n", ks_dht_hex(job->response_token.token, buf, KS_DHT_TOKEN_SIZE));

	ks_dht_storageitem_signature_generate(&sig, &sk, NULL, 0, 1, (uint8_t *)v, v_len);
	// @todo check if exists
	ks_dht_storageitem_create_mutable(&mutable, dht->pool, &job->query_target, (uint8_t *)v, v_len, &pk, NULL, 0, 1, &sig);
	mutable->sk = sk;
	ks_dht_storageitems_insert(dht, mutable);
	
	ks_dht_put(dht, &job->raddr, dht2_put_callback, NULL, &job->response_token, 0, mutable);
	return KS_STATUS_SUCCESS;
}

ks_status_t dht2_search_callback(ks_dht_t *dht, ks_dht_job_t *job)
{
	ks_dht_search_t *search = (ks_dht_search_t *)job->data;
	diag("dht2_search_callback %d\n", search->results_length);
	return KS_STATUS_SUCCESS;
}

int main() {
	//ks_size_t buflen;
  ks_status_t err;
  int mask = 0;
  ks_dht_nodeid_t nodeid;
  ks_dht_t *dht1 = NULL;
  ks_dht_t *dht2 = NULL;
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
  ks_dht_nodeid_t target;
  //ks_dht_storageitem_t *immutable = NULL;
  ks_dht_storageitem_t *mutable1 = NULL;
  ks_dht_storageitem_t *mutable2 = NULL;
  const char *v = "Hello World!";
  size_t v_len = strlen(v);
  //ks_dht_storageitem_skey_t sk; //= { { 0xe0, 0x6d, 0x31, 0x83, 0xd1, 0x41, 0x59, 0x22, 0x84, 0x33, 0xed, 0x59, 0x92, 0x21, 0xb8, 0x0b,
  //0xd0, 0xa5, 0xce, 0x83, 0x52, 0xe4, 0xbd, 0xf0, 0x26, 0x2f, 0x76, 0x78, 0x6e, 0xf1, 0xc7, 0x4d,
  //0xb7, 0xe7, 0xa9, 0xfe, 0xa2, 0xc0, 0xeb, 0x26, 0x9d, 0x61, 0xe3, 0xb3, 0x8e, 0x45, 0x0a, 0x22,
  //0xe7, 0x54, 0x94, 0x1a, 0xc7, 0x84, 0x79, 0xd6, 0xc5, 0x4e, 0x1f, 0xaf, 0x60, 0x37, 0x88, 0x1d } };
  //ks_dht_storageitem_pkey_t pk; //= { { 0x77, 0xff, 0x84, 0x90, 0x5a, 0x91, 0x93, 0x63, 0x67, 0xc0, 0x13, 0x60, 0x80, 0x31, 0x04, 0xf9,
  //0x24, 0x32, 0xfc, 0xd9, 0x04, 0xa4, 0x35, 0x11, 0x87, 0x6d, 0xf5, 0xcd, 0xf3, 0xe7, 0xe5, 0x48 } };
  //uint8_t sk1[KS_DHT_STORAGEITEM_SKEY_SIZE];
  //uint8_t pk1[KS_DHT_STORAGEITEM_PKEY_SIZE];
  ks_dht_storageitem_signature_t sig;
  //char sk_buf[KS_DHT_STORAGEITEM_SKEY_SIZE * 2 + 1];
  //char pk_buf[KS_DHT_STORAGEITEM_PKEY_SIZE * 2 + 1];
  //const char *test1vector = "3:seqi1e1:v12:Hello World!";
  //const char *test1vector = "4:salt6:foobar3:seqi1e1:v12:Hello World!";
  //size_t test1vector_len = strlen(test1vector);
  //uint8_t test1vector_sig[KS_DHT_STORAGEITEM_SIGNATURE_SIZE];
  //char test1vector_buf[KS_DHT_STORAGEITEM_SIGNATURE_SIZE * 2 + 1];

  err = ks_init();
  ok(!err);

  ks_global_set_default_logger(KS_LOG_LEVEL_INFO);

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

  ks_dht_dehex(nodeid.id, "0000000000000000000000000000000000000001", KS_DHT_NODEID_SIZE);
  err = ks_dht_create(&dht1, NULL, NULL, &nodeid);
  ok(err == KS_STATUS_SUCCESS);
  
  ks_dht_dehex(nodeid.id, "0000000000000000000000000000000000000002", KS_DHT_NODEID_SIZE);
  err = ks_dht_create(&dht2, NULL, NULL, &nodeid);
  ok(err == KS_STATUS_SUCCESS);

  ks_dht_dehex(nodeid.id, "0000000000000000000000000000000000000003", KS_DHT_NODEID_SIZE);
  err = ks_dht_create(&dht3, NULL, NULL, &nodeid);
  ok(err == KS_STATUS_SUCCESS);
  
  if (have_v4) {
    err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
    err = ks_dht_bind(dht1, &addr, &ep1);
    ok(err == KS_STATUS_SUCCESS);

	raddr1 = addr;
	
	err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT + 1, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
	err = ks_dht_bind(dht2, &addr, &ep2);
	ok(err == KS_STATUS_SUCCESS);

	//raddr2 = addr;

	err = ks_addr_set(&addr, v4, KS_DHT_DEFAULT_PORT + 2, AF_INET);
	ok(err == KS_STATUS_SUCCESS);
	
	err = ks_dht_bind(dht3, &addr, &ep3);
	ok(err == KS_STATUS_SUCCESS);

	//raddr3 = addr;
  }

  if (have_v6) {
	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);
	  
    err = ks_dht_bind(dht1, &addr, NULL);
    ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT + 1, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht_bind(dht2, &addr, NULL);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_addr_set(&addr, v6, KS_DHT_DEFAULT_PORT + 2, AF_INET6);
	ok(err == KS_STATUS_SUCCESS);

	err = ks_dht_bind(dht3, &addr, NULL);
	ok(err == KS_STATUS_SUCCESS);
  }

  diag("Ping test\n");
  
  ks_dht_ping(dht2, &raddr1, NULL, NULL); // (QUERYING)

  ks_dht_pulse(dht2, 100); // Send queued ping from dht2 to dht1 (RESPONDING)

  ks_dht_pulse(dht1, 100); // Receive and process ping query from dht2, queue and send ping response

  ok(ks_dhtrt_find_node(dht1->rt_ipv4, dht2->nodeid) == NULL); // The node should be dubious, and thus not be returned as good yet

  ks_dht_pulse(dht2, 100); // Receive and process ping response from dht1 (PROCESSING then COMPLETING)

  ok(ks_dhtrt_find_node(dht2->rt_ipv4, dht1->nodeid) != NULL); // The node should be good, and thus be returned as good

  ks_dht_pulse(dht2, 100); // Call finish callback and purge the job (COMPLETING)

  diag("Pulsing for route table pings\n"); // Wait for route table pinging to catch up
  for (int i = 0; i < 10; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  ok(ks_dhtrt_find_node(dht1->rt_ipv4, dht2->nodeid) != NULL); // The node should be good by now, and thus be returned as good

  
  ks_dht_ping(dht3, &raddr1, NULL, NULL); // (QUERYING)

  ks_dht_pulse(dht3, 100); // Send queued ping from dht3 to dht1 (RESPONDING)
  
  ks_dht_pulse(dht1, 100); // Receive and process ping query from dht3, queue and send ping response

  ok(ks_dhtrt_find_node(dht1->rt_ipv4, dht3->nodeid) == NULL); // The node should be dubious, and thus not be returned as good yet

  ks_dht_pulse(dht3, 100); // Receive and process ping response from dht1 (PROCESSING then COMPLETING)

  ok(ks_dhtrt_find_node(dht3->rt_ipv4, dht1->nodeid) != NULL); // The node should be good, and thus be returned as good

  ks_dht_pulse(dht3, 100); // Call finish callback and purge the job (COMPLETING)

  diag("Pulsing for route table pings\n"); // Wait for route table pinging to catch up
  for (int i = 0; i < 10; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  ok(ks_dhtrt_find_node(dht1->rt_ipv4, dht2->nodeid) != NULL); // The node should be good by now, and thus be returned as good

  // Test bootstrap find_node from dht3 to dht1 to find dht2 nodeid

  /*
  diag("Find_Node test\n");

  ks_dht_findnode(dht3, NULL, &raddr1, NULL, NULL, &dht2->nodeid);

  ks_dht_pulse(dht3, 100); // Send queued findnode from dht3 to dht1

  ks_dht_pulse(dht1, 100); // Receive and process findnode query from dht3, queue and send findnode response

  ok(ks_dhtrt_find_node(dht1->rt_ipv4, dht3->nodeid) == NULL); // The node should be dubious, and thus not be returned as good yet

  ks_dht_pulse(dht3, 100); // Receive and process findnode response from dht1
  
  ks_dht_pulse(dht3, 100); // Call finish callback and purge the job (COMPLETING)

  ok(ks_dhtrt_find_node(dht3->rt_ipv4, dht2->nodeid) == NULL); // The node should be dubious, and thus not be returned as good yet
  
  diag("Pulsing for route table pings\n"); // Wait for route table pinging to catch up
  for (int i = 0; i < 10; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  ok(ks_dhtrt_find_node(dht3->rt_ipv4, dht2->nodeid) != NULL); // The node should be good by now, and thus be returned as good
  */

  diag("Search test\n");
  
  ks_dht_search(dht3, dht2_search_callback, NULL, dht3->rt_ipv4, &dht2->nodeid);
  diag("Pulsing for route table pings\n"); // Wait for route table pinging to catch up
  for (int i = 0; i < 20; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  
  //diag("Get test\n");
  

  /*
  ks_dht_storageitem_target_immutable((uint8_t *)v, v_len, &target);
  ks_dht_storageitem_create_immutable(&immutable, dht1->pool, &target, (uint8_t *)v, v_len);
  ks_dht_storageitems_insert(dht1, immutable);
  */
  
  /*
  crypto_sign_keypair(pk.key, sk.key);

  ks_dht_storageitem_signature_generate(&sig, &sk, NULL, 0, 1, (uint8_t *)v, v_len);
  ks_dht_storageitem_target_mutable(&pk, NULL, 0, &target);
  ks_dht_storageitem_create_mutable(&mutable, dht1->pool, &target, (uint8_t *)v, v_len, &pk, NULL, 0, 1, &sig);
  mutable->sk = sk;
  ks_dht_storageitems_insert(dht1, mutable);

  ks_dht_get(dht2, &raddr1, dht2_get_callback, NULL, &target, NULL, 0);
 
  ks_dht_pulse(dht2, 100); // send get query

  ks_dht_pulse(dht1, 100); // receive get query and send get response

  ks_dht_pulse(dht2, 100); // receive get response

  ok(ks_dht_storageitems_find(dht2, &target) != NULL); // item should be verified and stored

  ks_dht_pulse(dht2, 100); // Call finish callback and purge the job (COMPLETING)
  */

  /*
  diag("Put test\n");

  crypto_sign_keypair(pk.key, sk.key);

  ks_dht_storageitem_target_mutable(&pk, NULL, 0, &target);

  ks_dht_get(dht2, &raddr1, dht2_get_token_callback, NULL, &target, NULL, 0); // create job
  
  for (int i = 0; i < 20; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  */

  /*
  diag("Publish test\n");
  
  crypto_sign_keypair(pk.key, sk.key);

  ks_dht_storageitem_target_mutable(&pk, NULL, 0, &target);
  
  ks_dht_storageitem_signature_generate(&sig, &sk, NULL, 0, 1, (uint8_t *)v, v_len);
  
  ks_dht_storageitem_create_mutable(&mutable, dht2->pool, &target, (uint8_t *)v, v_len, &pk, NULL, 0, 1, &sig);
  mutable->sk = sk;
  ks_dht_storageitems_insert(dht2, mutable);
  
  ks_dht_publish(dht2, &raddr1, dht2_put_callback, NULL, 0, mutable); // create job
  
  for (int i = 0; i < 20; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  */

  
  diag("Distribute test\n");
  
  crypto_sign_keypair(pk.key, sk.key);

  ks_dht_storageitem_target_mutable(&pk, NULL, 0, &target);
  
  ks_dht_storageitem_signature_generate(&sig, &sk, NULL, 0, 1, (uint8_t *)v, v_len);
  
  ks_dht_storageitem_create_mutable(&mutable2, dht2->pool, &target, (uint8_t *)v, v_len, &pk, NULL, 0, 1, &sig);
  mutable2->sk = sk;
  ks_dht_storageitems_insert(dht2, mutable2);
  
  ks_dht_distribute(dht2, dht2_distribute_callback, NULL, dht2->rt_ipv4, 0, mutable2); // create job
  
  for (int i = 0; i < 30; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  ks_dht_storageitem_dereference(mutable2);
  ok(mutable2->refc == 0);

  mutable1 = ks_dht_storageitems_find(dht1, &target);
  ok(mutable1 != NULL);
  
  ks_dht_storageitem_callback(mutable1, dht2_updated_callback);
  ks_dht_storageitem_callback(mutable2, dht2_updated_callback);

  ks_dht_storageitem_signature_generate(&sig, &sk, NULL, 0, 2, (uint8_t *)v, v_len);
  mutable1->seq = 2;
  mutable1->sig = sig;
  
  //ks_dht_storageitem_signature_generate(&sig, &sk, NULL, 0, 2, (uint8_t *)v, v_len);
  //mutable2->seq = 2;
  //mutable2->sig = sig;

  ks_dht_distribute(dht2, dht2_distribute_callback, NULL, dht2->rt_ipv4, 0, mutable2);
  for (int i = 0; i < 30; ++i) {
	  ks_dht_pulse(dht1, 100);
	  ks_dht_pulse(dht2, 100);
	  ks_dht_pulse(dht3, 100);
  }
  ks_dht_storageitem_dereference(mutable1);
  
  /* Cleanup and shutdown */
  diag("Cleanup\n");

  ks_dht_destroy(&dht3);

  ks_dht_destroy(&dht2);

  ks_dht_destroy(&dht1);

  ks_shutdown();
  
  done_testing();
}
