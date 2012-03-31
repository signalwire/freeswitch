/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * rsaglue.h - RSA encryption and decryption
 */
#ifndef RSAGLUE_H
#define RSAGLUE_H

struct PubKey;
struct SecKey;
struct BigNum;
#include "usuals.h"

#define RSAGLUE_NOMEM	-1	/* Ran out of memory */
#define RSAGLUE_TOOBIG	-2	/* Key too big (currently impossible) */
#define RSAGLUE_TOOSMALL	-3	/* Key too small (encryption only) */
#define RSAGLUE_CORRUPT	-4	/* Decrypted data corrupt (decrypt only) */
#define RSAGLUE_UNRECOG	-5	/* Unrecognized data (decrypt only) */

/* Declarations */
int rsaKeyTooBig(struct PubKey const *pub, struct SecKey const *sec);

int
rsaPublicEncrypt(struct BigNum *bn, byte const *in, unsigned len,
	struct PubKey const *pub);
int
rsaPrivateEncrypt(struct BigNum *bn, byte const *in, unsigned len,
	struct PubKey const *pub, struct SecKey const *sec);
int
rsaPublicDecrypt(byte *buf, unsigned len, struct BigNum *bn,
	struct PubKey const *pub);
int
rsaPrivateDecrypt(byte *buf, unsigned len, struct BigNum *bn,
	struct PubKey const *pub, struct SecKey const *sec);

#endif /* !RSAGLUE_H */
