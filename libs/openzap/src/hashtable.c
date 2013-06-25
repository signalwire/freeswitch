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

#include "hashtable.h"
#include "hashtable_private.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/*
  Credit for primes table: Aaron Krowne
  http://br.endernet.org/~akrowne/
  http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
*/
static const unsigned int primes[] = {
	53, 97, 193, 389,
	769, 1543, 3079, 6151,
	12289, 24593, 49157, 98317,
	196613, 393241, 786433, 1572869,
	3145739, 6291469, 12582917, 25165843,
	50331653, 100663319, 201326611, 402653189,
	805306457, 1610612741
};
const unsigned int prime_table_length = sizeof(primes)/sizeof(primes[0]);
const float max_load_factor = 0.65f;

/*****************************************************************************/
OZ_DECLARE(struct hashtable *)
create_hashtable(unsigned int minsize,
                 unsigned int (*hashf) (void*),
                 int (*eqf) (void*,void*))
{
    struct hashtable *h;
    unsigned int pindex, size = primes[0];
    /* Check requested hashtable isn't too large */
    if (minsize > (1u << 30)) return NULL;
    /* Enforce size as prime */
    for (pindex=0; pindex < prime_table_length; pindex++) {
        if (primes[pindex] > minsize) { size = primes[pindex]; break; }
    }
    h = (struct hashtable *)malloc(sizeof(struct hashtable));
    if (NULL == h) return NULL; /*oom*/
    h->table = (struct entry **)malloc(sizeof(struct entry*) * size);
    if (NULL == h->table) { free(h); return NULL; } /*oom*/
    memset(h->table, 0, size * sizeof(struct entry *));
    h->tablelength  = size;
    h->primeindex   = pindex;
    h->entrycount   = 0;
    h->hashfn       = hashf;
    h->eqfn         = eqf;
    h->loadlimit    = (unsigned int) ceil(size * max_load_factor);
    return h;
}

/*****************************************************************************/
unsigned int
hash(struct hashtable *h, void *k)
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
static int
hashtable_expand(struct hashtable *h)
{
    /* Double the size of the table to accomodate more entries */
    struct entry **newtable;
    struct entry *e;
    struct entry **pE;
    unsigned int newsize, i, index;
    /* Check we're not hitting max capacity */
    if (h->primeindex == (prime_table_length - 1)) return 0;
    newsize = primes[++(h->primeindex)];

    newtable = (struct entry **)malloc(sizeof(struct entry*) * newsize);
    if (NULL != newtable)
		{
			memset(newtable, 0, newsize * sizeof(struct entry *));
			/* This algorithm is not 'stable'. ie. it reverses the list
			 * when it transfers entries between the tables */
			for (i = 0; i < h->tablelength; i++) {
				while (NULL != (e = h->table[i])) {
					h->table[i] = e->next;
					index = indexFor(newsize,e->h);
					e->next = newtable[index];
					newtable[index] = e;
				}
			}
			free(h->table);
			h->table = newtable;
		}
    /* Plan B: realloc instead */
    else 
		{
			newtable = (struct entry **)
				realloc(h->table, newsize * sizeof(struct entry *));
			if (NULL == newtable) { (h->primeindex)--; return 0; }
			h->table = newtable;
			memset(newtable[h->tablelength], 0, newsize - h->tablelength);
			for (i = 0; i < h->tablelength; i++) {
				for (pE = &(newtable[i]), e = *pE; e != NULL; e = *pE) {
					index = indexFor(newsize,e->h);
					if (index == i)
						{
							pE = &(e->next);
						}
					else
						{
							*pE = e->next;
							e->next = newtable[index];
							newtable[index] = e;
						}
				}
			}
		}
    h->tablelength = newsize;
    h->loadlimit   = (unsigned int) ceil(newsize * max_load_factor);
    return -1;
}

/*****************************************************************************/
OZ_DECLARE(unsigned int)
hashtable_count(struct hashtable *h)
{
    return h->entrycount;
}

/*****************************************************************************/
OZ_DECLARE(int)
hashtable_insert(struct hashtable *h, void *k, void *v, hashtable_flag_t flags)
{
    /* This method allows duplicate keys - but they shouldn't be used */
    unsigned int index;
    struct entry *e;
    if (++(h->entrycount) > h->loadlimit)
		{
			/* Ignore the return value. If expand fails, we should
			 * still try cramming just this value into the existing table
			 * -- we may not have memory for a larger table, but one more
			 * element may be ok. Next time we insert, we'll try expanding again.*/
			hashtable_expand(h);
		}
    e = (struct entry *)malloc(sizeof(struct entry));
    if (NULL == e) { --(h->entrycount); return 0; } /*oom*/
    e->h = hash(h,k);
    index = indexFor(h->tablelength,e->h);
    e->k = k;
    e->v = v;
	e->flags = flags;
    e->next = h->table[index];
    h->table[index] = e;
    return -1;
}

/*****************************************************************************/
OZ_DECLARE(void *) /* returns value associated with key */
hashtable_search(struct hashtable *h, void *k)
{
    struct entry *e;
    unsigned int hashvalue, index;
    hashvalue = hash(h,k);
    index = indexFor(h->tablelength,hashvalue);
    e = h->table[index];
    while (NULL != e)
		{
			/* Check hash value to short circuit heavier comparison */
			if ((hashvalue == e->h) && (h->eqfn(k, e->k))) return e->v;
			e = e->next;
		}
    return NULL;
}

/*****************************************************************************/
OZ_DECLARE(void *) /* returns value associated with key */
hashtable_remove(struct hashtable *h, void *k)
{
    /* TODO: consider compacting the table when the load factor drops enough,
     *       or provide a 'compact' method. */

    struct entry *e;
    struct entry **pE;
    void *v;
    unsigned int hashvalue, index;

    hashvalue = hash(h,k);
    index = indexFor(h->tablelength,hash(h,k));
    pE = &(h->table[index]);
    e = *pE;
    while (NULL != e)
		{
			/* Check hash value to short circuit heavier comparison */
			if ((hashvalue == e->h) && (h->eqfn(k, e->k)))
				{
					*pE = e->next;
					h->entrycount--;
					v = e->v;
					if (e->flags & HASHTABLE_FLAG_FREE_KEY) {
						freekey(e->k);
					}
					free(e);
					return v;
				}
			pE = &(e->next);
			e = e->next;
		}
    return NULL;
}

/*****************************************************************************/
/* destroy */
OZ_DECLARE(void)
hashtable_destroy(struct hashtable *h)
{
    unsigned int i;
    struct entry *e, *f;
    struct entry **table = h->table;

	for (i = 0; i < h->tablelength; i++)
        {
            e = table[i];
            while (NULL != e)
				{ f = e; e = e->next; if (f->flags & HASHTABLE_FLAG_FREE_KEY) freekey(f->k); if (f->flags & HASHTABLE_FLAG_FREE_VALUE) free(f->v); free(f); }
        }
    
    free(h->table);
    free(h);
}

OZ_DECLARE(struct hashtable_iterator *) hashtable_next(struct hashtable_iterator *i)
{

	if (i->e) {
		if ((i->e = i->e->next) != 0) { 
			return i;
		} else {
			i->pos++;
		}
	}
	
	while(i->pos < i->h->tablelength && !i->h->table[i->pos]) {
		i->pos++;
	}
	
	if (i->pos >= i->h->tablelength) {
		return NULL;
	}
	
	if ((i->e = i->h->table[i->pos]) != 0) { 
		return i;
	}

	return NULL;
}

OZ_DECLARE(struct hashtable_iterator *) hashtable_first(struct hashtable *h)
{
	h->iterator.pos = 0;
	h->iterator.e = NULL;
	h->iterator.h = h;
	return hashtable_next(&h->iterator);
}



OZ_DECLARE(void) hashtable_this(struct hashtable_iterator *i, const void **key, int *klen, void **val)
{
	if (i->e) {
		if (key) {
			*key = i->e->k;
		}
		if (klen) {
			*klen = (int)strlen(i->e->k);
		}
		if (val) {
			*val = i->e->v;
		}
	} else {
		if (key) {
			*key = NULL;
		}
		if (klen) {
			*klen = 0;
		}
		if (val) {
			*val = NULL;
		}
	}
}


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

