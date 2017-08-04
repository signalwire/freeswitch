#include "ks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

int main(int argc, char **argv)
{
	ks_pool_t *pool;
	uint32_t *buf = NULL;
	intptr_t ptr = 0;

	ks_init();

	plan(4);

	ks_pool_open(&pool);

	buf = (uint32_t *)ks_pool_alloc(pool, sizeof(uint32_t) * 1);
	ok(buf != NULL);

	ptr = (intptr_t)buf;

	buf = (uint32_t *)ks_pool_resize(buf, sizeof(uint32_t) * 1);
	ok(buf != NULL);

	ok((intptr_t)buf == ptr);

	buf = (uint32_t *)ks_pool_resize(buf, sizeof(uint32_t) * 2);
	ok(buf != NULL);

	ks_pool_free(&buf);

	ks_pool_close(&pool);

	ks_shutdown();

	done_testing();
}
