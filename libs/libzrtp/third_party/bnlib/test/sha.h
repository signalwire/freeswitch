/* --------------------------------- SHA.H ------------------------------- */

/*
 * NIST Secure Hash Algorithm.
 *
 * Written 2 September 1992, Peter C. Gutmann.
 * This implementation placed in the public domain.
 *
 * Modified 1 June 1993, Colin Plumb.
 * Renamed to SHA and comments updated a bit 1 November 1995, Colin Plumb.
 * These modifications placed in the public domain.
 *
 * Comments to pgut1@cs.aukuni.ac.nz
 */

/* Typedefs for various word sizes */
#include "types.h"

/*
 * Since 64-bit machines are the wave of the future, we may as well
 * support them directly.
 */

/* The SHA block size and message digest sizes, in bytes */

#define SHA_BLOCKSIZE	64
#define SHA_DIGESTSIZE	20

/*
 * The structure for storing SHA info.
 * data[] is placed first in case offsets of 0 are faster
 * for some reason; it's the most often accessed field.
 */

struct SHAContext {
	word32 data[ 16 ];		/* SHA data buffer */
	word32 digest[ 5 ];		/* Message digest */
#ifdef HAVE64
	word64 count;
#else
	word32 countHi, countLo;	/* 64-bit byte count */
#endif
};

/* Which standard?  FIPS 180 or FIPS 180.1? */

#define SHA_VERSION 1

/* Whether the machine is little-endian or not */

#if !defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
#define BIG_ENDIAN 1
#endif

void shaInit(struct SHAContext *sha);
void shaTransform(struct SHAContext *sha);
void shaUpdate(struct SHAContext *sha, word8 const *buffer, unsigned count);
void shaFinal(struct SHAContext *shaInfo, word8 *hash);
