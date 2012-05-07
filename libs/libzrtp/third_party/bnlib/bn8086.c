/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn8086.c - bnInit() for Intel x86 family in 16-bit mode.
 *
 * Written in 1995 by Colin Plumb.
 */

#include "lbn.h"
#include "bn16.h"
#include "bn32.h"

#ifndef BNINCLUDE
#error You must define BNINCLUDE to lbn8086.h to use assembly primitives.
#endif

void
bnInit(void)
{
	if (not386())
		bnInit_16();
	else
		bnInit_32();
}
