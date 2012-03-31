/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * kb.h - interface for keyboard I/O
 */

/* The implementation is in kbunix.c, kbmsdos.c, kbvms.c, etc. */

void kbCbreak(void), kbNorm(void);
int kbGet(void);
void kbFlush(int thorough);
