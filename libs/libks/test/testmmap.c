#include "ks.h"
#include "tap.h"

static ks_pool_t *pool = NULL;

#define LOOP_COUNT 1000000

ks_status_t test1()
{
	int i;
	void *mem, *last_mem = NULL;

	for (i = 0; i < LOOP_COUNT; i++) {
		if (last_mem) {
			ks_pool_free(pool, &last_mem);
		}
		mem = ks_pool_alloc(pool, 1024);
		last_mem = mem;
	}

	return KS_STATUS_SUCCESS;
}

int main(int argc, char **argv)
{
	ks_init();

	ok(ks_pool_open(&pool) == KS_STATUS_SUCCESS);

	ok(test1() == KS_STATUS_SUCCESS);

	ok(ks_pool_close(&pool) == KS_STATUS_SUCCESS);

	ks_shutdown();

	done_testing();

	return 0;
}
