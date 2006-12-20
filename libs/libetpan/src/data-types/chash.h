/*
 * libEtPan! -- a mail stuff library
 *
 * chash - Implements generic hash tables.
 *
 * Copyright (c) 1999-2005, Gaël Roualland <gael.roualland@iname.com>
 * interface changes - 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: chash.h,v 1.15 2006/03/22 08:10:47 hoa Exp $
 */

#ifndef CHASH_H
#define CHASH_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBETPAN_CONFIG_H
#	include <libetpan/libetpan-config.h>
#endif

typedef struct {
  void * data;
  unsigned int len;
} chashdatum;

struct chash {
  unsigned int size;
  unsigned int count;
  int copyvalue;
  int copykey;
  struct chashcell ** cells; 
};

typedef struct chash chash;

struct chashcell {
  unsigned int func;
  chashdatum key;
  chashdatum value;
  struct chashcell * next;
};

typedef struct chashcell chashiter;

#define CHASH_COPYNONE    0
#define CHASH_COPYKEY     1
#define CHASH_COPYVALUE   2
#define CHASH_COPYALL     (CHASH_COPYKEY | CHASH_COPYVALUE)

#define CHASH_DEFAULTSIZE 13
  
/* Allocates a new (empty) hash using this initial size and the given flags,
   specifying which data should be copied in the hash.
    CHASH_COPYNONE  : Keys/Values are not copied.
    CHASH_COPYKEY   : Keys are dupped and freed as needed in the hash.
    CHASH_COPYVALUE : Values are dupped and freed as needed in the hash.
    CHASH_COPYALL   : Both keys and values are dupped in the hash.
 */
LIBETPAN_EXPORT
chash * chash_new(unsigned int size, int flags);

/* Frees a hash */
LIBETPAN_EXPORT
void chash_free(chash * hash);

/* Removes all elements from a hash */
LIBETPAN_EXPORT
void chash_clear(chash * hash);

/* Adds an entry in the hash table.
   Length can be 0 if key/value are strings.
   If an entry already exists for this key, it is replaced, and its value
   is returned. Otherwise, the data pointer will be NULL and the length
   field be set to TRUE or FALSe to indicate success or failure. */
LIBETPAN_EXPORT
int chash_set(chash * hash,
	      chashdatum * key,
	      chashdatum * value,
	      chashdatum * oldvalue);

/* Retrieves the data associated to the key if it is found in the hash table.
   The data pointer and the length will be NULL if not found*/
LIBETPAN_EXPORT
int chash_get(chash * hash,
	      chashdatum * key, chashdatum * result);

/* Removes the entry associated to this key if it is found in the hash table,
   and returns its contents if not dupped (otherwise, pointer will be NULL
   and len TRUE). If entry is not found both pointer and len will be NULL. */
LIBETPAN_EXPORT
int chash_delete(chash * hash,
		 chashdatum * key,
		 chashdatum * oldvalue);

/* Resizes the hash table to the passed size. */
LIBETPAN_EXPORT
int chash_resize(chash * hash, unsigned int size);

/* Returns an iterator to the first non-empty entry of the hash table */
LIBETPAN_EXPORT
chashiter * chash_begin(chash * hash);

/* Returns the next non-empty entry of the hash table */
LIBETPAN_EXPORT
chashiter * chash_next(chash * hash, chashiter * iter);

/* Some of the following routines can be implemented as macros to
   be faster. If you don't want it, define NO_MACROS */
#ifdef NO_MACROS
/* Returns the size of the hash table */
LIBETPAN_EXPORT
unsigned int          chash_size(chash * hash);

/* Returns the number of entries in the hash table */
LIBETPAN_EXPORT
unsigned int          chash_count(chash * hash);

/* Returns the key part of the entry pointed by the iterator */
LIBETPAN_EXPORT
void chash_key(chashiter * iter, chashdatum * result);

/* Returns the value part of the entry pointed by the iterator */
LIBETPAN_EXPORT
void chash_value(chashiter * iter, chashdatum * result);

#else
static inline unsigned int chash_size(chash * hash)
{
  return hash->size;
}

static inline unsigned int chash_count(chash * hash)
{
  return hash->count;
}

static inline void chash_key(chashiter * iter, chashdatum * result)
{
  * result = iter->key;
}

static inline void chash_value(chashiter * iter, chashdatum * result)
{
  * result = iter->value;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
