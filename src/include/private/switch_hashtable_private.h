/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * switch_hashtable_private.h -- Hashtable
 *
 */

/* hashtable_private.h -- Copyright (C) 2002, 2004 Christopher Clark <firstname.lastname@cl.cam.ac.uk> */

#ifndef __HASHTABLE_PRIVATE_CWC22_H__
#define __HASHTABLE_PRIVATE_CWC22_H__

#include "switch_hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif
/*****************************************************************************/

struct entry
{
    void *k, *v;
    unsigned int h;
	hashtable_flag_t flags;
	hashtable_destructor_t destructor;
    struct entry *next;
};

struct switch_hashtable_iterator {
	unsigned int pos;
	struct entry *e;
	struct switch_hashtable *h;
};

struct switch_hashtable {
    unsigned int tablelength;
    struct entry **table;
    unsigned int entrycount;
    unsigned int loadlimit;
    unsigned int primeindex;
    unsigned int (*hashfn) (void *k);
    int (*eqfn) (void *k1, void *k2);
};

/*****************************************************************************/

/*****************************************************************************/
static inline unsigned int
hash(switch_hashtable_t *h, void *k)
{
    /* Aim to protect against poor hash functions by adding logic here
     * - logic taken from java 1.4 hashtable source */
    unsigned int i = h->hashfn(k);
    i += ~(i << 9);
    i ^=  ((i >> 14) | (i << 18)); /* >>> */
    i +=  (i << 4);
    i ^=  ((i >> 10) | (i << 22)); /* >>> */
    return i;
}


/*****************************************************************************/
/* indexFor */
static __inline__ unsigned int
indexFor(unsigned int tablelength, unsigned int hashvalue) {
    return (hashvalue % tablelength);
}

/* Only works if tablelength == 2^N */
/*static inline unsigned int
  indexFor(unsigned int tablelength, unsigned int hashvalue)
  {
  return (hashvalue & (tablelength - 1u));
  }
*/

/*****************************************************************************/
#define freekey(X) free(X)
/*define freekey(X) ; */

#ifdef __cplusplus
}
#endif

/*****************************************************************************/

#endif /* __HASHTABLE_PRIVATE_CWC22_H__*/

/*
 * Copyright (c) 2002, Christopher Clark
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
