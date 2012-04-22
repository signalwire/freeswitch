/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#include <stdio.h>

#define userPrintf printf
#define userPuts(s) fputs(s, stdout)
#define userFlush() fflush(stdout)
#define userPutc putchar
