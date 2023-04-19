#include "fspr.h"
#include "fspr_file_io.h"
#include "fspr.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif

int main(void)
{
    char buf[256];
    fspr_file_t *err;
    fspr_pool_t *p;

    fspr_initialize();
    atexit(fspr_terminate);

    fspr_pool_create(&p, NULL);
    fspr_file_open_stdin(&err, p);

    while (1) {
        fspr_size_t length = 256;
        fspr_file_read(err, buf, &length);
    }
    exit(0); /* just to keep the compiler happy */
}
