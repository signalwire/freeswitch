#include "ks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

typedef struct ks_dht_nodeid_s { uint8_t id[20]; } ks_dht_nodeid_t;

struct x {
	int i;
	ks_pool_t *pool;
};

static void *test1_thread(ks_thread_t *thread, void *data)
{
	struct x *mydata = (struct x *) data;

	//ks_log(KS_LOG_DEBUG, "Thread %d\n", mydata->i);
	ks_sleep(100000);
	ks_pool_free(mydata->pool, &mydata);
	return NULL;
}

static int test1() 
{
	ks_pool_t *pool;
	ks_thread_pool_t *tp = NULL;
	int i = 0;

	ks_pool_open(&pool);

	ks_thread_pool_create(&tp, 2, 10, KS_THREAD_DEFAULT_STACK, KS_PRI_NORMAL, 5);
	

	for (i = 0; i < 500; i++) {
		struct x *data = ks_pool_alloc(pool, sizeof(*data));
		data->i = i;
		data->pool = pool;
		ks_sleep(1);
		ks_thread_pool_add_job(tp, test1_thread, data);
	}


	while(ks_thread_pool_backlog(tp)) {
		ks_sleep(100000);
	}

	ks_sleep(10000000);

	ks_thread_pool_destroy(&tp);
	ks_pool_close(&pool);

	return 1;
}

int main(int argc, char **argv)
{


	ks_init();
	ks_global_set_default_logger(7);

	plan(1);

	ok(test1());

	ks_shutdown();

	done_testing();
}
