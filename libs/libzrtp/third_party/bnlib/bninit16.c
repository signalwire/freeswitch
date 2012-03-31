/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bninit16.c - Provide an init function that sets things up for 16-bit
 * operation.  This is a seaparate tiny file so you can compile two bn
 * packages into the library and write a custom init routine.
 *
 * Written in 1995 by Colin Plumb.
 */

#include "bn.h"
#include "bn16.h"

void
bnInit(void)
{
	bnInit_16();
}
