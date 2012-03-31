/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * keys.c - allocate and free PubKey and SecKey structures.
 */

#include "first.h"

#include "bn.h"

#include "keys.h"
#include "usuals.h"

void
pubKeyBegin(struct PubKey *pub)
{
	if (pub) {
		bnBegin(&pub->n);
		bnBegin(&pub->e);
	}
}

void
pubKeyEnd(struct PubKey *pub)
{
	if (pub) {
		bnEnd(&pub->n);
		bnEnd(&pub->e);
		wipe(pub);
	}
}

void
secKeyBegin(struct SecKey *sec)
{
	if (sec) {
		bnBegin(&sec->d);
		bnBegin(&sec->p);
		bnBegin(&sec->q);
		bnBegin(&sec->u);
	}
}

void
secKeyEnd(struct SecKey *sec)
{
	if (sec) {
		bnEnd(&sec->d);
		bnEnd(&sec->p);
		bnEnd(&sec->q);
		bnEnd(&sec->u);
		wipe(sec);
	}
}
