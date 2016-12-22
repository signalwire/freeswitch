#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

//#include "ks.h"
#include "../src/dht/ks_dht.h"

ks_dht_t *dht;
ks_dhtrt_routetable_t *rt;
ks_pool_t *pool;
ks_thread_pool_t *tpool;


static ks_thread_t *threads[10];


int doquery(ks_dhtrt_routetable_t *rt, uint8_t *id, enum ks_dht_nodetype_t type, enum ks_afflags_t family)
{
	ks_dhtrt_querynodes_t query;
	memset(&query, 0, sizeof(query));
	query.max = 30;
	memcpy(&query.nodeid.id, id, KS_DHT_NODEID_SIZE);
	query.family =  family;
	query.type = type;

	return  ks_dhtrt_findclosest_nodes(rt, &query);
}

void test01()
{
	printf("**** testbuckets - test01 start\n"); fflush(stdout);

	ks_dhtrt_routetable_t *rt;
	ks_dhtrt_initroute(&rt, dht, pool, tpool);
	ks_dhtrt_deinitroute(&rt);
 
	ks_dhtrt_initroute(&rt, dht, pool, tpool);
	ks_dht_nodeid_t nodeid, homeid;
	memset(homeid.id,  0xdd, KS_DHT_NODEID_SIZE);
	homeid.id[19] = 0;

	char ip[] = "192.168.100.100";
	unsigned short port = 7000;
	ks_dht_node_t *peer;
	ks_dht_node_t *peer1;
	
	ks_status_t status;  
	status = ks_dhtrt_create_node(rt, homeid, KS_DHT_LOCAL, ip, port, &peer);
	if (status == KS_STATUS_FAIL) {
		printf("* **ks_dhtrt_create_node test01 failed\n");
		exit(101);
	}
	
	peer = ks_dhtrt_find_node(rt, homeid);
	if (peer == 0)  {
		printf("*** ks_dhtrt_find_node test01  failed \n"); fflush(stdout);
		exit(102);
	}

	status = ks_dhtrt_create_node(rt, homeid, KS_DHT_LOCAL, ip, port, &peer1);
	if (status == KS_STATUS_FAIL) {
		printf("**** ks_dhtrt_create_node test01 did allow duplicate createnodes!!\n");
		exit(103);
	}
	if (peer != peer1) {
		printf("**** ks_dhtrt_create_node duplicate createnode did not return the same node!\n");
		exit(104);
	}
	
	status = ks_dhtrt_delete_node(rt, peer);
	if (status == KS_STATUS_FAIL) {
		printf("****  ks_dhtrt_delete_node test01 failed\n");
		exit(104);
	}

	printf("**** testbuckets - test01 complete\n\n\n"); fflush(stdout);
}

void test02()
{
	printf("**** testbuckets - test02 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid;
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;
	enum ks_afflags_t both = ifboth;

	ks_status_t status;

	nodeid.id[0] = 1;
	status = ks_dhtrt_create_node(rt, nodeid, KS_DHT_LOCAL, ipv6, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[0] = 2;
	status = ks_dhtrt_create_node(rt,  nodeid,  KS_DHT_REMOTE, ipv6, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[0] = 3;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_REMOTE, ipv6, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[0] = 4;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_LOCAL, ipv6, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[1] = 1;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_REMOTE, ipv6, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);


	nodeid.id[19] = 1;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[19] = 2;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[19] = 3;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[19] = 4;
	status = ks_dhtrt_create_node(rt,  nodeid,  KS_DHT_LOCAL, ipv4, port, &peer);
	ks_dhtrt_touch_node(rt, nodeid);

	nodeid.id[19] = 5;
	status = ks_dhtrt_create_node(rt,  nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
	nodeid.id[19] = 6;
	status = ks_dhtrt_create_node(rt,  nodeid,  KS_DHT_LOCAL, ipv4, port, &peer);

	int qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, both);
	printf("\n* **local query count expected 3, actual %d\n", qcount); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, both);
	printf("\n* **remote query count expected 6, actual %d\n", qcount); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, both);
	printf("\n* **both query count expected 9, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv4);
	printf("\n* **local AF_INET  query count expected 1, actual %d\n", qcount); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv6);
	printf("\n* **local AF_INET6 query count expected 2, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv6);
	printf("\n* **AF_INET6 count expected 5, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv4);
	printf("\n* **remote AF_INET  query count expected 3, actual %d\n", qcount); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv6);
	printf("\n* **remote AF_INET6 query count expected 3, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
	printf("\n* **AF_INET count expected 4, actual %d\n", qcount); fflush(stdout);

	nodeid.id[19] = 5;
	ks_dhtrt_touch_node(rt, nodeid);
	nodeid.id[19] = 6;
	ks_dhtrt_touch_node(rt, nodeid);

	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
	printf("\n**** AF_INET (after touch) count expected 6, actual %d\n", qcount); fflush(stdout);

	printf("****  testbuckets - test02 finished\n"); fflush(stdout);

	return;
}

/* this is similar to test2 but after mutiple table splits. */

void test03()
{
	printf("**** testbuckets - test03 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid;
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;
	enum ks_afflags_t both = ifboth;

	ks_status_t status;
	int ipv4_remote = 0;
	int ipv4_local = 0;

	for (int i=0; i<200; ++i) {
		if (i%10 == 0) {
			++nodeid.id[0];
			nodeid.id[1] = 0;
		}
		else {
			 ++nodeid.id[1];
		} 
		ks_status_t s0 = ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
		 if (s0 == KS_STATUS_SUCCESS) {
			ks_dhtrt_touch_node(rt, nodeid);
			++ipv4_remote;
		}
	}

 
	for (int i=0; i<2; ++i) {
		 if (i%10 == 0) {
			++nodeid.id[0];
			nodeid.id[1] = 0;
		}
		else {
			++nodeid.id[1];
		}

		ks_status_t s0 = ks_dhtrt_create_node(rt, nodeid, KS_DHT_LOCAL, ipv4, port, &peer);
		if (s0 == KS_STATUS_SUCCESS) {
			ks_dhtrt_touch_node(rt, nodeid);
			++ipv4_local;
		}
	}

	for (int i=0; i<201; ++i) {
		if (i%10 == 0) {
			++nodeid.id[0];
			nodeid.id[1] = 0;
		}
		else {
			++nodeid.id[1];
		}
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv6, port, &peer);
		ks_dhtrt_touch_node(rt, nodeid);
	}


	ks_dhtrt_dump(rt, 7);


	int qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, both);
	printf("\n**** local query count expected 2, actual %d, max %d\n", qcount, ipv4_local); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, both);
	printf("\n**** remote query count expected 20, actual %d\n", qcount); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, both);
	printf("\n**** both query count expected 20, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv4);
	printf("\n**** local AF_INET  query count expected 2, actual %d\n", qcount); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv6);
	printf("\n**** local AF_INET6 query count expected 0, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv6);
	printf("\n**** AF_INET6 count expected 20, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv4);
	printf("\n**** remote AF_INET  query count expected 20, actual %d max %d\n", qcount, ipv4_remote); fflush(stdout);
	qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv6);
	printf("\n**** remote AF_INET6 query count expected 20, actual %d\n", qcount); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
	printf("\n**** AF_INET count expected 20, actual %d\n", qcount); fflush(stdout);

	printf("**** testbuckets - test03 finished\n\n\n"); fflush(stdout);
	return;
}

void test04()
{
	printf("**** testbuckets - test04 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid;
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;
	enum ks_afflags_t both = ifboth;

	ks_status_t status;

	for (int i=0,i2=0,i3=0; i<10000; ++i, ++i2,  ++i3) {
		if (i%20 == 0) {
			nodeid.id[0] =  nodeid.id[0] / 2;
			if (i2%20 == 0) {
				nodeid.id[1] = nodeid.id[1] / 2;
				i2 = 0;
				if (i3%20 == 0) {
					nodeid.id[2] = nodeid.id[2] / 2;
				}
			}
			else {
				++nodeid.id[3];
			}
		}
		else {
			++nodeid.id[1];
		}
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
		ks_dhtrt_touch_node(rt, nodeid);
	}

	
	memset(nodeid.id,  0x2f, KS_DHT_NODEID_SIZE);
	ks_time_t t0 = ks_time_now();
	int qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
	ks_time_t t1 = ks_time_now();

	int tx = t1 - t0;
	t1 /= 1000;

	printf("**** query on 10k nodes in %d ms\n", tx);
 
	printf("**** testbuckets - test04 finished\n\n\n"); fflush(stdout);

	return;
}

/* test read/write node locking */
void test05()
{
	printf("**** testbuckets - test05 start\n"); fflush(stdout);

	ks_dht_node_t  *peer, *peer1, *peer2;
	ks_dht_nodeid_t nodeid;
	ks_status_t s;

	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7001;

	ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
	
	peer1 = ks_dhtrt_find_node(rt, nodeid);
	printf("test05 - first find compelete\n"); fflush(stdout);

	peer2 =  ks_dhtrt_find_node(rt, nodeid);
	printf("test05 - second find compelete\n");   fflush(stdout);

	ks_dhtrt_delete_node(rt, peer);
	printf("test05 - delete compelete\n");   fflush(stdout);

	s = ks_dhtrt_release_node(peer1);
	if (s == KS_STATUS_FAIL) printf("release 1 failed\n"); fflush(stdout);

	s = ks_dhtrt_release_node(peer2);
	if (s == KS_STATUS_FAIL) printf("release 1 failed\n"); 

	printf("* **testbuckets - test05 finished\n\n\n"); fflush(stdout);

	return;
}


/* test06  */
/* ------  */
ks_dht_nodeid_t g_nodeid1;
ks_dht_nodeid_t g_nodeid2;
ks_dht_node_t  *g_peer;

static void *testnodelocking_ex1(ks_thread_t *thread, void *data)
{
	//lock=3 on entry
	ks_dhtrt_release_node(g_peer);                                //lock=2
	ks_dhtrt_release_node(g_peer);                                //lock=1
	ks_dhtrt_release_node(g_peer);                                //lock=0
	return NULL;
}

static void *testnodelocking_ex2(ks_thread_t *thread, void *data)
{
	// lock=4 on entry                                       
	ks_dht_node_t  *peer2 = ks_dhtrt_find_node(rt, g_nodeid1);   //lock=5
	ks_dhtrt_release_node(peer2);                                //lock=4
	ks_dhtrt_sharelock_node(peer2);                              //lock=5
	ks_dhtrt_release_node(peer2);                                //lock=4
	ks_dhtrt_sharelock_node(peer2);                              //lock=5
	ks_dhtrt_release_node(peer2);                                //lock=4                           
	ks_dhtrt_release_node(peer2);                                //lock=3
	ks_dhtrt_find_node(rt, g_nodeid1);                           //lock=4
	ks_dhtrt_release_node(peer2);                                //lock=3

	return NULL;
}


void test06()
{
	printf("**** testbuckets - test06 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	memset(g_nodeid1.id,  0xef, KS_DHT_NODEID_SIZE);
	memset(g_nodeid2.id,  0x1f, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;

	ks_dhtrt_create_node(rt, g_nodeid1, KS_DHT_REMOTE, ipv4, port, &peer);  // lock=1
	ks_dhtrt_touch_node(rt, g_nodeid1);

	ks_dht_node_t  *peer2 = ks_dhtrt_find_node(rt, g_nodeid1);   //lock=2
	peer2 = ks_dhtrt_find_node(rt, g_nodeid1);   //lock=3
	peer2 = ks_dhtrt_find_node(rt, g_nodeid1);   //lock=4

	ks_dhtrt_release_node(peer2);                                //lock=3
	ks_dhtrt_sharelock_node(peer2);                              //lock=4                            

	g_peer =  peer2;

	ks_thread_t *t0;
	ks_thread_create(&t0, testnodelocking_ex1, NULL, pool);

	ks_thread_t *t1;
	ks_thread_create(&t1, testnodelocking_ex2, NULL, pool);

	ks_thread_join(t1);
	ks_thread_join(t0);

	ks_dhtrt_delete_node(rt, peer2);

	printf("\n\n* **testbuckets - test06 -- check if the node gets deleted\n\n\n\n"); fflush(stdout);

	ks_dhtrt_process_table(rt);

	printf("**** testbuckets - test06 start\n"); fflush(stdout);

	return;
}



static int gindex = 1;
static ks_mutex_t *glock;
static int gstop = 0;

static int test60loops = 1000;
static int test60nodes = 200;  /* max at 255 */ 

static void *test60ex1(ks_thread_t *thread, void *data)
{
	while(!gstop) {
	ks_dhtrt_process_table(rt);
	ks_sleep(100);
	}
	return NULL;
}


static void *test60ex2(ks_thread_t *thread, void *data)
{
	ks_dht_nodeid_t nodeid;
	ks_dhtrt_querynodes_t query;


	while(!gstop) {

		memset(&query, 0, sizeof(query));
		memset(query.nodeid.id,  0xef, KS_DHT_NODEID_SIZE);
		query.max = 30;
		query.family = ifv4;
		query.type = KS_DHT_REMOTE;


		ks_dhtrt_findclosest_nodes(rt, &query);
		ks_sleep(10000);
	
		for (int i=0; i<query.count; ++i) {
			ks_dhtrt_release_node(query.nodes[i]);
			ks_sleep(10000);
		} 
		ks_sleep(2000000);

	}

	return NULL;
}

static void *test60ex(ks_thread_t *thread, void *data)
{
	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid;
	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;

	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	int *pi = data;
	int i = *pi;
	ks_mutex_lock(glock);
	nodeid.id[0] = ++gindex;
	ks_mutex_unlock(glock);

	printf("starting thread with i of %d\n", gindex); fflush(stdout);

	for (int loop=0; loop<test60loops; ++loop) {

		for (int i=0; i<test60nodes; ++i) {
			++nodeid.id[19];
			ks_dhtrt_create_node(rt, nodeid, KS_DHT_LOCAL, ipv4, port, &peer);
			ks_sleep(1000);
			ks_dhtrt_touch_node(rt, nodeid);
		}

		for (int i=0; i<test60nodes; ++i) {
			peer = ks_dhtrt_find_node(rt, nodeid);
			if (peer) {
				ks_dhtrt_delete_node(rt, peer);
				ks_sleep(400);
			}
			--nodeid.id[19];
		}

	}

	return 0;
	
}

void test60()
{
	printf("**** test60: starting\n"); fflush(stdout);
	int i;
	ks_mutex_create(&glock, KS_MUTEX_FLAG_DEFAULT, pool);

	ks_thread_t *t0;
	ks_thread_create(&t0, test60ex1, NULL, pool); 

	ks_thread_t *t1;
	ks_thread_create(&t1, test60ex2, NULL, pool); 

	for (i = 0; i < 10; i++) {
		ks_thread_create(&threads[i], test60ex, &i, pool);
	}

	printf("all threads started\n"); fflush(stdout);

	for (i = 0; i < 10; i++) {
		ks_thread_join(threads[i]);
	}
	gstop = 1;

	ks_thread_join(t1); 

	ks_thread_join(t0); 

	printf("all threads completed\n"); fflush(stdout);
	ks_dhtrt_dump(rt, 7);
	printf("**** test60: completed\n"); fflush(stdout);


	return;
}


void test30()
{
 printf("**** testbuckets - test03 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid;
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;
	enum ks_afflags_t both = ifboth;

	ks_status_t status;
	int ipv4_remote = 0;
	int ipv4_local = 0;

	for (int i=0; i<200; ++i) {
		if (i%10 == 0) {
			++nodeid.id[0];
			nodeid.id[1] = 0;
		}
		else {
			++nodeid.id[1];
		}
		ks_status_t s0 = ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
		if (s0 == KS_STATUS_SUCCESS) {
			ks_dhtrt_touch_node(rt, nodeid);
			++ipv4_remote;
		}
	}

	for (int i=0; i<2; ++i) {
		if (i%10 == 0) {
			++nodeid.id[0];
			nodeid.id[1] = 0;
		}
		else {
			++nodeid.id[1];
		}

		ks_status_t s0 = ks_dhtrt_create_node(rt, nodeid, KS_DHT_LOCAL, ipv4, port, &peer);
		if (s0 == KS_STATUS_SUCCESS) {
			ks_dhtrt_touch_node(rt, nodeid);
			++ipv4_local;
		}
	}

	for (int i=0; i<201; ++i) {
		if (i%10 == 0) {
			++nodeid.id[0];
			nodeid.id[1] = 0;
		}
		else {
			++nodeid.id[1];
		}
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv6, port, &peer);
		ks_dhtrt_touch_node(rt, nodeid);
	}


	ks_dhtrt_dump(rt, 7);


	int qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, both);
	printf("\n **** local query count expected 2, actual %d, max %d\n", qcount, ipv4_local); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, both);
	printf("\n **** local query count expected 2, actual %d, max %d\n", qcount, ipv4_local); fflush(stdout);

	qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, both);
	printf("\n **** local query count expected 20, actual %d, max %d\n", qcount, ipv4_local); fflush(stdout);

	return;
}







/* test resue of node memory */
void test50()
{
	printf("*** testbuckets - test50 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid, nodeid2;
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);
	memset(nodeid2.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;
	enum ks_afflags_t both = ifboth;

	ks_status_t status;

	for (int i=0,i2=0; i<200; ++i, ++i2) {
		if (i%20 == 0) {
			nodeid.id[0] =  nodeid.id[0] / 2;
			if (i2%20 == 0) {
				i2 = 0;
				nodeid.id[1] =  nodeid.id[1] / 2;
			}
			else {
				++nodeid.id[2];
			}
		}
		else {
			++nodeid.id[1];
		}
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
		ks_dhtrt_touch_node(rt, nodeid);
	}

	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);
	for (int i=0,i2=0; i<200; ++i, ++i2) {
		 if (i%20 == 0) { 
			nodeid.id[0] =  nodeid.id[0] / 2;
			if (i2%20 == 0) {
				i2 = 0; 
				nodeid.id[1] =  nodeid.id[1] / 2;
			}
			else {
				++nodeid.id[2];
			}
		}
		else {
			++nodeid.id[1];
		}
		ks_dht_node_t *n = ks_dhtrt_find_node(rt, nodeid);
		if (n != NULL) {
			ks_dhtrt_release_node(n);
			ks_dhtrt_delete_node(rt, n);
		}
	}

	ks_dhtrt_process_table(rt);

	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	for (int i=0,i2=0; i<200; ++i, ++i2) {
		if (i%20 == 0) {
			nodeid.id[0] =  nodeid.id[0] / 2;
			if (i2%20 == 0) {
				i2 = 0;
				nodeid.id[1] =  nodeid.id[1] / 2;
			}
			else {
				++nodeid.id[2];
			}
		}
		else {
			++nodeid.id[1];
		}
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
		ks_dhtrt_touch_node(rt, nodeid);
	}

	printf("**** testbuckets - test50 start\n"); fflush(stdout);
	return;
}


/* test process_table */
void test51()
{
	printf("**** testbuckets - test51 start\n"); fflush(stdout);

	ks_dht_node_t  *peer;
	ks_dht_nodeid_t nodeid, nodeid2;
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);
	memset(nodeid2.id,  0xef, KS_DHT_NODEID_SIZE);

	char ipv6[] = "1234:1234:1234:1234";
	char ipv4[] = "123.123.123.123";
	unsigned short port = 7000;
	enum ks_afflags_t both = ifboth;

	ks_status_t status;

	for (int i=0,i2=0; i<2; ++i, ++i2) {
		if (i%20 == 0) {
			nodeid.id[0] =  nodeid.id[0] / 2;
			if (i2%20 == 0) {
				i2 = 0;
				nodeid.id[1] =  nodeid.id[1] / 2;
			}
			else {
				++nodeid.id[2];
			}
		}
		else {
			++nodeid.id[1];
		}
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
		ks_dhtrt_touch_node(rt, nodeid);
	}

	for (int ix=0; ix<50; ++ix) {
		ks_dhtrt_process_table(rt);	
		ks_sleep(1000  *1000  *120);
		printf("* **pulse ks_dhtrt_process_table\n");
		if ( ix%2 == 0) ks_dhtrt_dump(rt, 7);
	}
 
	printf("**** testbuckets - test51 complete\n"); fflush(stdout);

	return;  
}



int main(int argc, char *argv[]) {

	printf("testdhtbuckets - start\n");

	int tests[100];
		if (argc == 0) {
			tests[0] = 1;
			tests[1] = 2;
			tests[2] = 3;
			tests[3] = 4;
			tests[4] = 5;
		}
	else {
		for(int tix=1; tix<100 && tix<argc; ++tix) {
			long i = strtol(argv[tix], NULL, 0);
			tests[tix] = i;
		}
	}

	ks_init();
	ks_global_set_default_logger(7);
	ks_dht_create(&dht, NULL, NULL);


  // ks_thread_pool_create(&tpool, 0, KS_DHT_TPOOL_MAX, KS_DHT_TPOOL_STACK, KS_PRI_NORMAL, KS_DHT_TPOOL_IDLE);
	
	tpool = 0;

	
	ks_status_t status;
	char *str = NULL;
	int bytes = 1024;
	ks_dht_nodeid_t homeid;
	ks_dht_nodeid_t nodeid, nodeid1, nodeid2;
	ks_dht_node_t *peer, *peer1, *peer2;

	memset(homeid.id,  0xde, KS_DHT_NODEID_SIZE);
	memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

	ks_init();
	status = ks_pool_open(&pool);

	printf("init/deinit routeable\n"); fflush(stdout);

	ks_dhtrt_initroute(&rt, dht, pool, tpool);
	ks_dhtrt_deinitroute(&rt);


	for (int tix=0; tix<argc; ++tix) {

		if (tests[tix] == 1) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test01();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 2) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test02();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 3) {  
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test03();
			ks_dhtrt_deinitroute(&rt);
			continue; 
		}

		if (tests[tix] == 4) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test04();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 5) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test05();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 6) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test06();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}


		if (tests[tix] == 30) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test30();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}


		if (tests[tix] == 50) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test50();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 51) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test51();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 60) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			test60();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}

		if (tests[tix] == 99) {
			ks_dhtrt_initroute(&rt, dht, pool, tpool);
			//testnodelocking();
			ks_dhtrt_deinitroute(&rt);
			continue;
		}


	}



	return 0;

}
