#include <stdio.h>

#define userPrintf printf
#define userPuts(s) fputs(s, stdout)
#define userFlush() fflush(stdout)
#define userPutc putchar
