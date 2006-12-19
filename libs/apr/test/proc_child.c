#include "apr.h"
#include <stdio.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_IO_H
#include <io.h>
#endif
#include <stdlib.h>

int main(void)
{
    char buf[256];
    apr_ssize_t bytes;
    
    bytes = read(STDIN_FILENO, buf, 256);
    if (bytes > 0)
        write(STDOUT_FILENO, buf, bytes);

    return 0; /* just to keep the compiler happy */
}
