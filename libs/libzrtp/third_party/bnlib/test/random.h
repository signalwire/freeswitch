/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#include "usuals.h"

int randSourceSet(char const *string, unsigned len, int pri);

void randBytes(byte *dest, unsigned len);
unsigned randRange(unsigned range);

unsigned randEvent(int event);
void randFlush(void);

void randAccum(unsigned count);
