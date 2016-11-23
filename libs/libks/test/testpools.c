#include "ks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

static void fill(char *str, int bytes, char c)
{
	memset(str, c, bytes -1);
	*(str+(bytes-1)) = '\0';
}

struct foo {
	int x;
	char *str;
};


void cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
	struct foo *foo = (struct foo *) ptr;

	printf("Cleanup %p action: %d\n", ptr, action);

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		printf("DESTROY STR [%s]\n", foo->str);
		free(foo->str);
		foo->str = NULL;
	}
}



int main(int argc, char **argv)
{
	ks_pool_t *pool;
	int err = 0;
	char *str = NULL;
	int bytes = 1024;
	ks_status_t status;
	struct foo *foo;

	ks_init();

	plan(11);

	if (argc > 1) {
		int tmp = atoi(argv[1]);

		if (tmp > 0) {
			bytes = tmp;
		} else {
			fprintf(stderr, "INVALID\n");
			exit(255);
		}
	}

	status = ks_pool_open(&pool);

	printf("OPEN:\n");
	ok(status == KS_STATUS_SUCCESS);
	if (status != KS_STATUS_SUCCESS) {
		fprintf(stderr, "OPEN ERR: %d [%s]\n", err, ks_pool_strerror(status));
		exit(255);
	}

	printf("ALLOC:\n");
	str = ks_pool_alloc(pool, bytes);

	ok(str != NULL);
	if (!str) {
		fprintf(stderr, "ALLOC ERR\n");
		exit(255);
	}

	fill(str, bytes, '.');
	printf("%s\n", str);

	printf("FREE:\n");

	status = ks_pool_safe_free(pool, str);
	if (status != KS_STATUS_SUCCESS) {
		fprintf(stderr, "FREE ERR: [%s]\n", ks_pool_strerror(err));
		exit(255);
	}

	printf("ALLOC2:\n");

	str = ks_pool_alloc(pool, bytes);

	ok(str != NULL);
	if (!str) {
		fprintf(stderr, "ALLOC2 ERR: [FAILED]\n");
		exit(255);
	}

	fill(str, bytes, '-');
	printf("%s\n", str);


	printf("ALLOC OBJ:\n");

	foo = ks_pool_alloc(pool, sizeof(struct foo));

	ok(foo != NULL);
	if (!foo) {
		fprintf(stderr, "ALLOC OBJ: [FAILED]\n");
		exit(255);
	} else {
		printf("ALLOC OBJ [%p]:\n", (void *) foo);
	}

	foo->x = 12;
	foo->str = strdup("This is a test 1234 abcd; This will be called on explicit free\n");
	ks_pool_set_cleanup(pool, foo, NULL, 0, cleanup);

	printf("FREE OBJ:\n");

	status = ks_pool_safe_free(pool, foo);
	ok(status == KS_STATUS_SUCCESS);
	if (status != KS_STATUS_SUCCESS) {
		fprintf(stderr, "FREE OBJ ERR: [%s]\n", ks_pool_strerror(status));
		exit(255);
	}


	printf("ALLOC OBJ2:\n");

	foo = ks_pool_alloc(pool, sizeof(struct foo));

	ok(foo != NULL);
	if (!foo) {
		fprintf(stderr, "ALLOC OBJ2: [FAILED]\n");
		exit(255);
	} else {
		printf("ALLOC OBJ2 [%p]:\n", (void *) foo);
	}

	foo->x = 12;
	foo->str = strdup("This is a second test 1234 abcd; This will be called on pool clear/destroy\n");
	ks_pool_set_cleanup(pool, foo, NULL, 0, cleanup);


	printf("ALLOC OBJ3:\n");

	foo = ks_pool_alloc(pool, sizeof(struct foo));

	ok(foo != NULL);
	if (!foo) {
		fprintf(stderr, "ALLOC OBJ3: [FAILED]\n");
		exit(255);
	} else {
		printf("ALLOC OBJ3 [%p]:\n", (void *) foo);
	}

	foo->x = 12;
	foo->str = strdup("This is a third test 1234 abcd; This will be called on pool clear/destroy\n");
	ks_pool_set_cleanup(pool, foo, NULL, 0, cleanup);



	printf("RESIZE:\n");
	bytes *= 2;
	str = ks_pool_resize(pool, str, bytes);

	ok(str != NULL);
	if (!str) {
		fprintf(stderr, "RESIZE ERR: [FAILED]\n");
		exit(255);
	}

	fill(str, bytes, '*');
	printf("%s\n", str);


	printf("FREE 2:\n");

	status = ks_pool_free(pool, str);
	ok(status == KS_STATUS_SUCCESS);
	if (status != KS_STATUS_SUCCESS) {
		fprintf(stderr, "FREE2 ERR: [%s]\n", ks_pool_strerror(status));
		exit(255);
	}


	printf("CLEAR:\n");
	status = ks_pool_clear(pool);

	ok(status == KS_STATUS_SUCCESS);
	if (status != KS_STATUS_SUCCESS) {
		fprintf(stderr, "CLEAR ERR: [%s]\n", ks_pool_strerror(status));
		exit(255);
	}

	printf("CLOSE:\n");
	status = ks_pool_close(&pool);
	
	ok(status == KS_STATUS_SUCCESS);
	if (status != KS_STATUS_SUCCESS) {
		fprintf(stderr, "CLOSE ERR: [%s]\n", ks_pool_strerror(err));
		exit(255);
	}

	ks_shutdown();

	done_testing();
}
