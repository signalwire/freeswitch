#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

//#include "ks.h"
#include "../src/dht/ks_dht.h"

ks_dhtrt_routetable_t* rt;
ks_pool_t* pool;

static ks_thread_t *threads[10];


int doquery(ks_dhtrt_routetable_t* rt, uint8_t* id, enum ks_dht_nodetype_t type, enum ks_afflags_t family)
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
	printf("*** testbuckets - test01 start\n"); fflush(stdout);

   ks_dhtrt_routetable_t* rt;
   ks_dhtrt_initroute(&rt, pool);
   ks_dhtrt_deinitroute(&rt);
 
   ks_dhtrt_initroute(&rt, pool);
   ks_dht_nodeid_t nodeid, homeid;
   memset(homeid.id,  0xdd, KS_DHT_NODEID_SIZE);
   homeid.id[19] = 0;

   char ip[] = "192.168.100.100";
   unsigned short port = 7000;
   ks_dht_node_t* peer;
   ks_dht_node_t* peer1;
   
   ks_status_t status;  
   status = ks_dhtrt_create_node(rt, homeid, KS_DHT_LOCAL, ip, port, &peer);
   if (status == KS_STATUS_FAIL) {
       printf("*** ks_dhtrt_create_node test01 failed\n");
       exit(101);
   }
   
   peer = ks_dhtrt_find_node(rt, homeid);
   if (peer == 0)  {
      printf("*** ks_dhtrt_find_node test01  failed \n"); fflush(stdout);
      exit(102);
   }

   status = ks_dhtrt_create_node(rt, homeid, KS_DHT_LOCAL, ip, port, &peer1);
   if (status == KS_STATUS_FAIL) {
       printf("*** ks_dhtrt_create_node test01 did allow duplicate createnodes!!\n");
       exit(103);
   }
   if (peer != peer1) {
       printf("*** ks_dhtrt_create_node duplicate createnode did not return the same node!\n");
       exit(104);
   }
   
   status = ks_dhtrt_delete_node(rt, peer);
   if (status == KS_STATUS_FAIL) {
       printf("*** ks_dhtrt_delete_node test01 failed\n");
       exit(104);
   }

   printf("*** testbuckets - test01 complete\n\n\n"); fflush(stdout);
}

void test02()
{
	printf("*** testbuckets - test02 start\n"); fflush(stdout);

   ks_dht_node_t*  peer;
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
   printf("\n*** local query count expected 3, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, both);
   printf("\n*** remote query count expected 6, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, both);
   printf("\n*** both query count expected 9, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv4);
   printf("\n*** local AF_INET  query count expected 1, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv6);
   printf("\n*** local AF_INET6 query count expected 2, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv6);
   printf("\n*** AF_INET6 count expected 5, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv4);
   printf("\n*** remote AF_INET  query count expected 3, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv6);
   printf("\n*** remote AF_INET6 query count expected 3, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
   printf("\n*** AF_INET count expected 4, actual %d\n", qcount); fflush(stdout);

   nodeid.id[19] = 5;
   ks_dhtrt_touch_node(rt, nodeid);
   nodeid.id[19] = 6;
   ks_dhtrt_touch_node(rt, nodeid);

   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
   printf("\n*** AF_INET (after touch) count expected 6, actual %d\n", qcount); fflush(stdout);

   printf("*** testbuckets - test02 finished\n"); fflush(stdout);

   return;
}

/* this is similar to test2 but after mutiple table splits. */

void test03()
{
 printf("*** testbuckets - test03 start\n"); fflush(stdout);

   ks_dht_node_t*  peer;
   ks_dht_nodeid_t nodeid;
   memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

   char ipv6[] = "1234:1234:1234:1234";
   char ipv4[] = "123.123.123.123";
   unsigned short port = 7000;
   enum ks_afflags_t both = ifboth;

   ks_status_t status;

   for (int i=0; i<200; ++i) {
     if (i%10 == 0) {
           ++nodeid.id[0];
     }
     else {
           ++nodeid.id[1];
     } 
     ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv4, port, &peer);
     ks_dhtrt_touch_node(rt, nodeid);
   }

   for (int i=0; i<2; ++i) {
     if (i%10 == 0) {
           ++nodeid.id[0];
     }
     else {
           ++nodeid.id[1];
     }

     ks_dhtrt_create_node(rt, nodeid, KS_DHT_LOCAL, ipv4, port, &peer);
     ks_dhtrt_touch_node(rt, nodeid);
   }

   for (int i=0; i<201; ++i) {
     if (i%10 == 0) {
           ++nodeid.id[0];
     }
     else {
           ++nodeid.id[1];
     }
     ks_dhtrt_create_node(rt, nodeid, KS_DHT_REMOTE, ipv6, port, &peer);
     ks_dhtrt_touch_node(rt, nodeid);
   }


   int qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, both);
   printf("\n** local query count expected 2, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, both);
   printf("\n*** remote query count expected 20, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, both);
   printf("\n*** both query count expected 20, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv4);
   printf("\n*** local AF_INET  query count expected 2, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_LOCAL, ifv6);
   printf("\n*** local AF_INET6 query count expected 0, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv6);
   printf("\n*** AF_INET6 count expected 20, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv4);
   printf("\n*** remote AF_INET  query count expected 20, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, KS_DHT_REMOTE, ifv6);
   printf("\n*** remote AF_INET6 query count expected 20, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
   printf("\n*** AF_INET count expected 20, actual %d\n", qcount); fflush(stdout);

   printf("*** testbuckets - test03 finished\n\n\n"); fflush(stdout);
   return;
}

void test04()
{
   printf("*** testbuckets - test04 start\n"); fflush(stdout);

   ks_dht_node_t*  peer;
   ks_dht_nodeid_t nodeid;
   memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

   char ipv6[] = "1234:1234:1234:1234";
   char ipv4[] = "123.123.123.123";
   unsigned short port = 7000;
   enum ks_afflags_t both = ifboth;

   ks_status_t status;

   for (int i=0,i2=0; i<10000; ++i) {
     if (i%40 == 0) {
           ++nodeid.id[0];
           if(i2%40 == 0) {
               ++nodeid.id[1];
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

   
    memset(nodeid.id,  0x2f, KS_DHT_NODEID_SIZE);
    ks_time_t t0 = ks_time_now();
    int qcount = doquery(rt, nodeid.id, KS_DHT_BOTH, ifv4);
    ks_time_t t1 = ks_time_now();

     int tx = t1 - t0;
    t1 /= 1000;

    printf("*** query on 10k nodes in %d ms\n", tx);
 
    printf("*** testbuckets - test04 finished\n\n\n"); fflush(stdout);

   return;
}

/* test read/write node locking */
void test05()
{
 printf("*** testbuckets - test05 start\n"); fflush(stdout);

   ks_dht_node_t*  peer, *peer1, *peer2;
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

   printf("*** testbuckets - test05 finished\n\n\n"); fflush(stdout);

   return;
}

static int gindex = 1;
static ks_mutex_t *glock;
static int gstop = 0;

static int test06loops = 1000;
static int test06nodes = 200;  /* max at 255 */ 

static void *test06ex1(ks_thread_t *thread, void *data)
{
   while(!gstop) {
      ks_dhtrt_process_table(rt);
      ks_sleep(100);
   }
   return NULL;
}


static void *test06ex2(ks_thread_t *thread, void *data)
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
   
       for(int i=0; i<query.count; ++i) {
           ks_dhtrt_release_node(query.nodes[i]);
           ks_sleep(10000);
       } 
       ks_sleep(2000000);

   }
   return NULL;
}

static void *test06ex(ks_thread_t *thread, void *data)
{
   ks_dht_node_t*  peer;
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

   for(int loop=0; loop<test06loops; ++loop) {

	for (int i=0; i<test06nodes; ++i) {
		 ++nodeid.id[19];
		ks_dhtrt_create_node(rt, nodeid, KS_DHT_LOCAL, ipv4, port, &peer);
        ks_sleep(1000);
        ks_dhtrt_touch_node(rt, nodeid);
	}

	for (int i=0; i<test06nodes; ++i) {
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

void test06()
{
    int i;
    ks_mutex_create(&glock, KS_MUTEX_FLAG_DEFAULT, pool);

    ks_thread_t* t0;
    ks_thread_create(&t0, test06ex1, NULL, pool); 

    ks_thread_t* t1;
    ks_thread_create(&t1, test06ex2, NULL, pool); 

    for(i = 0; i < 10; i++) {
        ks_thread_create(&threads[i], test06ex, &i, pool);
    }

    printf("all threads started\n"); fflush(stdout);

    for(i = 0; i < 10; i++) {
        ks_thread_join(threads[i]);
    }
    gstop = 1;

    ks_thread_join(t1); 

    ks_thread_join(t0); 

    printf("all threads completed\n"); fflush(stdout);
   ks_dhtrt_dump(rt, 7);

    return;
}


int main(int argc, char* argv[]) {

   printf("testdhtbuckets - start\n");

   int tests[10];
  
	if (argc == 0) {
      tests[1] = 1;
      tests[2] = 1;
      tests[3] = 1;
      tests[4] = 1;
      tests[5] = 1;
      tests[6] = 0;
      tests[7] = 0;
      tests[8] = 0;
      tests[9] = 0;
	}
	else {
		for(int tix=1; tix<10 && tix<argc; ++tix) {
			long i = strtol(argv[tix], NULL, 0);
			tests[i] = 1;
		}
	}

   ks_init();
   ks_global_set_default_logger(7);


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

   ks_dhtrt_initroute(&rt, pool);
   ks_dhtrt_deinitroute(&rt);

   if (tests[1] == 1) {
	ks_dhtrt_initroute(&rt, pool);
	test01();
	ks_dhtrt_deinitroute(&rt);
   }

   if (tests[2] == 1) {
	ks_dhtrt_initroute(&rt, pool);
	test02();
	ks_dhtrt_deinitroute(&rt);
   }

   if (tests[3] == 1) {  
    ks_dhtrt_initroute(&rt, pool);
	test03();
	ks_dhtrt_deinitroute(&rt);
   }

   if (tests[4] == 1) {
	ks_dhtrt_initroute(&rt, pool);
	test04();
	ks_dhtrt_deinitroute(&rt);
   }

   if (tests[5] == 1) {
	ks_dhtrt_initroute(&rt, pool);
	test05();
	ks_dhtrt_deinitroute(&rt);
   }

   if (tests[6] == 1) {
	ks_dhtrt_initroute(&rt, pool);
    test06();
    ks_dhtrt_deinitroute(&rt);
   }
   return 0;

}
