#include <stdlib.h>
#include <stdio.h>
#include "mpool.h"
#include <string.h>

int main(int argc, char **argv)
{
	mpool_t *pool;
	int err = 0;
	char *str = NULL;
	int x = 0;
	int bytes = 1024;

	if (argc > 1) {
		int tmp = atoi(argv[1]);

		if (tmp > 0) {
			bytes = tmp;
		} else {
			fprintf(stderr, "INVALID\n");
			exit(255);
		}
	}

	pool = mpool_open(MPOOL_FLAG_ANONYMOUS, 0, NULL, &err);

	if (!pool || err != MPOOL_ERROR_NONE) {
		fprintf(stderr, "ERR: %d [%s]\n", err, mpool_strerror(err));
		exit(255);
	}

	str = mpool_alloc(pool, bytes, &err);
	memset(str+x, '.', bytes -1);
	*(str+(bytes-1)) = '\0';

	printf("%s\n", str);

	//mpool_clear(pool);
	err = mpool_close(pool);
	
	if (err != MPOOL_ERROR_NONE) {
		fprintf(stderr, "ERR: [%s]\n", mpool_strerror(err));
		exit(255);
	}
	
	exit(0);
}
