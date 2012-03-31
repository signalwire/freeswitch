/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn68000.c - bnInit() for Motorola 680x0 family, 16 or 32-bit.
 *
 * Written in 1995 by Colin Plumb.
 */

#include "lbn.h"
#include "bn16.h"
#include "bn32.h"

#ifndef BNINCLUDE
#error You must define BNINCLUDE to lbn68000.h to use assembly primitives.
#endif

void
bnInit(void)
{
	if (is68020())
		bnInit_32();
	else
		bnInit_16();
}
