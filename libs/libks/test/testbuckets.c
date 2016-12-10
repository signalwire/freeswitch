#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"

//#include "ks.h"
#include "../src/dht/ks_dht.h"

ks_dhtrt_routetable_t* rt;
ks_pool_t* pool;


int doquery(ks_dhtrt_routetable_t* rt, uint8_t* id, enum ks_dht_nodetype_t type, enum ipfamily family)
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
 printf("testbuckets - test01 start\n"); fflush(stdout);
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
   
   ks_status_t status;  
   status = ks_dhtrt_create_node(rt, homeid, ks_dht_local_t, ip, port, &peer);
   if (status == KS_STATUS_FAIL) {
       printf("*** ks_dhtrt_create_node test01 failed\n");
       exit(101);
   }
   
   peer = ks_dhtrt_find_node(rt, homeid);
   if (peer == 0)  {
      printf("*** ks_dhtrt_find_node test01  failed \n"); fflush(stdout);
      exit(102);
   }

   status = ks_dhtrt_create_node(rt, homeid, ks_dht_local_t, ip, port, &peer);
   if (status != KS_STATUS_FAIL) {
       printf("*** ks_dhtrt_create_node test01 allowed duplicate!!\n");
       exit(103);
   }
   
   status = ks_dhtrt_delete_node(rt, peer);
   if (status == KS_STATUS_FAIL) {
       printf("*** ks_dhtrt_delete_node test01 failed\n");
       exit(104);
   }

   printf("*** testbuckets - test01 complete\n"); fflush(stdout);
}

void test02()
{
   ks_dht_node_t*  peer;
   ks_dht_nodeid_t nodeid;
   memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

   char ipv6[] = "1234:1234:1234:1234";
   char ipv4[] = "123.123.123.123";
   unsigned short port = 7000;
   enum ipfamily both = ifboth;

   ks_status_t status;

   nodeid.id[0] = 1;
   status = ks_dhtrt_create_node(rt, nodeid, ks_dht_local_t, ipv6, port, &peer);
   nodeid.id[0] = 2;
   status = ks_dhtrt_create_node(rt,  nodeid,  ks_dht_remote_t, ipv6, port, &peer);
   nodeid.id[0] = 3;
   status = ks_dhtrt_create_node(rt,  nodeid, ks_dht_remote_t, ipv6, port, &peer);
   nodeid.id[0] = 4;
   status = ks_dhtrt_create_node(rt,  nodeid, ks_dht_local_t, ipv6, port, &peer);
   nodeid.id[1] = 1;
   status = ks_dhtrt_create_node(rt,  nodeid, ks_dht_remote_t, ipv6, port, &peer);


   nodeid.id[19] = 1;
   status = ks_dhtrt_create_node(rt,  nodeid, ks_dht_remote_t, ipv4, port, &peer);
   nodeid.id[19] = 2;
   status = ks_dhtrt_create_node(rt,  nodeid, ks_dht_remote_t, ipv4, port, &peer);
   nodeid.id[19] = 3;
   status = ks_dhtrt_create_node(rt,  nodeid, ks_dht_remote_t, ipv4, port, &peer);
   nodeid.id[19] = 4;
   status = ks_dhtrt_create_node(rt,  nodeid,  ks_dht_local_t, ipv4, port, &peer);

   int qcount = doquery(rt, nodeid.id, ks_dht_local_t, both);
   printf("\n*** local query count expected 3, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_remote_t, both);
   printf("\n*** remote query count expected 6, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_both_t, both);
   printf("\n*** both query count expected 9, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_local_t, ifv4);
   printf("\n*** local AF_INET  query count expected 1, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_local_t, ifv6);
   printf("\n*** local AF_INET6 query count expected 2, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_both_t, ifv6);
   printf("\n*** AF_INET6 count expected 5, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_remote_t, ifv4);
   printf("\n*** remote AF_INET  query count expected 3, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_remote_t, ifv6);
   printf("\n*** remote AF_INET6 query count expected 3, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_both_t, ifv4);
   printf("\n*** AF_INET count expected 4, actual %d\n", qcount); fflush(stdout);

   return;
}

/* this is similar to test2 but after mutiple table splits. */

void test03()
{
   ks_dht_node_t*  peer;
   ks_dht_nodeid_t nodeid;
   memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

   char ipv6[] = "1234:1234:1234:1234";
   char ipv4[] = "123.123.123.123";
   unsigned short port = 7000;
   enum ipfamily both = ifboth;

   ks_status_t status;

   for (int i=0; i<200; ++i) {
     if (i%10 == 0) {
           ++nodeid.id[0];
     }
     else {
           ++nodeid.id[1];
     } 
     ks_dhtrt_create_node(rt, nodeid, ks_dht_remote_t, ipv4, port, &peer);
   }

   for (int i=0; i<2; ++i) {
     if (i%10 == 0) {
           ++nodeid.id[0];
     }
     else {
           ++nodeid.id[1];
     }

     ks_dhtrt_create_node(rt, nodeid, ks_dht_local_t, ipv4, port, &peer);
   }

   for (int i=0; i<201; ++i) {
     if (i%10 == 0) {
           ++nodeid.id[0];
     }
     else {
           ++nodeid.id[1];
     }
     ks_dhtrt_create_node(rt, nodeid, ks_dht_remote_t, ipv6, port, &peer);
   }


   int qcount = doquery(rt, nodeid.id, ks_dht_local_t, both);
   printf("\n** local query count expected 3, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_remote_t, both);
   printf("\n*** remote query count expected 6, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_both_t, both);
   printf("\n*** both query count expected 9, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_local_t, ifv4);
   printf("\n*** local AF_INET  query count expected 1, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_local_t, ifv6);
   printf("\n*** local AF_INET6 query count expected 2, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_both_t, ifv6);
   printf("\n*** AF_INET6 count expected 5, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_remote_t, ifv4);
   printf("\n** remote AF_INET  query count expected 3, actual %d\n", qcount); fflush(stdout);
   qcount = doquery(rt, nodeid.id, ks_dht_remote_t, ifv6);
   printf("\n*** remote AF_INET6 query count expected 3, actual %d\n", qcount); fflush(stdout);

   qcount = doquery(rt, nodeid.id, ks_dht_both_t, ifv4);
   printf("\n*** AF_INET count expected 4, actual %d\n", qcount); fflush(stdout);

   return;
}

void test04()
{
   ks_dht_node_t*  peer;
   ks_dht_nodeid_t nodeid;
   memset(nodeid.id,  0xef, KS_DHT_NODEID_SIZE);

   char ipv6[] = "1234:1234:1234:1234";
   char ipv4[] = "123.123.123.123";
   unsigned short port = 7000;
   enum ipfamily both = ifboth;

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
     ks_dhtrt_create_node(rt, nodeid, ks_dht_remote_t, ipv4, port, &peer);
   }

   
    memset(nodeid.id,  0x2f, KS_DHT_NODEID_SIZE);
    ks_time_t t0 = ks_time_now();
    int qcount = doquery(rt, nodeid.id, ks_dht_both_t, ifv4);
    ks_time_t t1 = ks_time_now();

     int tx = t1 - t0;
    t1 /= 1000;

    printf("*** query on 10k nodes in %d ms\n", tx);
 

   return;
}



int main(int argx, char* argv[]) {

   printf("testdhtbuckets - start\n");

   ks_init();

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

   ks_dhtrt_initroute(&rt, pool);
   test01();
   test02();
   test03();
   test04();

   return 0;

}
