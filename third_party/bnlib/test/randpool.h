/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#include "usuals.h"

/* Set this to whatever you need (must be > 512) */
#define RANDPOOLBITS 3072

void randPoolStir(void);
void randPoolAddBytes(byte const *buf, unsigned len);
void randPoolGetBytes(byte *buf, unsigned len);
byte randPoolGetByte(void);
