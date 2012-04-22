/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef MD5_H
#define MD5_H

#include <stddef.h>
#include "usuals.h"

struct MD5Context {
	word32 buf[4];
	word32 bytes[2];
	word32 in[16];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, byte const *buf, size_t len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(word32 buf[4], word32 const in[16]);

void byteSwap(word32 *buf, unsigned words);

#endif /* !MD5_H */
