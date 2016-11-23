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

#include "ks.h"
#include "ks_hash.h"

struct entry
{
    void *k, *v;
    unsigned int h;
	ks_hash_flag_t flags;
	ks_hash_destructor_t destructor;
    struct entry *next;
};

struct ks_hash_iterator {
	unsigned int pos;
	ks_locked_t locked;
	struct entry *e;
	struct ks_hash *h;
};

struct ks_hash {
	ks_pool_t *pool;
    unsigned int tablelength;
    struct entry **table;
    unsigned int entrycount;
    unsigned int loadlimit;
    unsigned int primeindex;
    unsigned int (*hashfn) (void *k);
    int (*eqfn) (void *k1, void *k2);
	ks_hash_flag_t flags;
	ks_hash_destructor_t destructor;
	ks_rwl_t *rwl;
	ks_mutex_t *mutex;
	uint32_t readers;
};

/*****************************************************************************/

/*****************************************************************************/
static inline unsigned int
hash(ks_hash_t *h, void *k)
{
    /* Aim to protect against poor hash functions by adding logic here
     * - logic taken from java 1.4 ks_hash source */
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
//#define freekey(X) free(X)

/*
  Credit for primes table: Aaron Krowne
  http://br.endernet.org/~akrowne/
  http://planetmath.org/encyclopedia/GoodKs_HashPrimes.html
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

static void ks_hash_cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
	//ks_hash_t *hash = (ks_hash_t *) ptr;

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		//ks_hash_destroy(&hash);
		break;
	}

}

KS_DECLARE(ks_status_t) ks_hash_create(ks_hash_t **hp, ks_hash_mode_t mode, ks_hash_flag_t flags, ks_pool_t *pool)
{
	return ks_hash_create_ex(hp, 16, NULL, NULL, mode, flags, NULL, pool);
}

KS_DECLARE(void) ks_hash_set_flags(ks_hash_t *h, ks_hash_flag_t flags)
{
	h->flags = flags;
}

KS_DECLARE(void) ks_hash_set_destructor(ks_hash_t *h, ks_hash_destructor_t destructor)
{
	h->destructor = destructor;
}


KS_DECLARE(ks_status_t)
ks_hash_create_ex(ks_hash_t **hp, unsigned int minsize,
				  unsigned int (*hashf) (void*),
				  int (*eqf) (void*,void*), ks_hash_mode_t mode, ks_hash_flag_t flags, ks_hash_destructor_t destructor, ks_pool_t *pool)
{
    ks_hash_t *h;
    unsigned int pindex, size = primes[0];

	switch(mode) {
	case KS_HASH_MODE_CASE_INSENSITIVE:
		ks_assert(hashf == NULL);
		hashf = ks_hash_default_ci;
		break;
	case KS_HASH_MODE_INT:
		ks_assert(hashf == NULL);
		ks_assert(eqf == NULL);
		hashf = ks_hash_default_int;
		eqf = ks_hash_equalkeys_int;
		break;
	case KS_HASH_MODE_INT64:
		ks_assert(hashf == NULL);
		ks_assert(eqf == NULL);
		hashf = ks_hash_default_int64;
		eqf = ks_hash_equalkeys_int64;
		break;
	case KS_HASH_MODE_PTR:
		ks_assert(hashf == NULL);
		ks_assert(eqf == NULL);
		hashf = ks_hash_default_ptr;
		eqf = ks_hash_equalkeys_ptr;
		break;
	default:
		break;
	}

	if (flags == KS_HASH_FLAG_DEFAULT) {
		flags = KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_RWLOCK;
	}

	ks_assert(pool);
	if (!hashf) hashf = ks_hash_default;
	if (!eqf) eqf = ks_hash_equalkeys;
	if (!minsize) minsize = 16;

    /* Check requested ks_hash isn't too large */
    if (minsize > (1u << 30)) {*hp = NULL; return KS_STATUS_FAIL;}
    /* Enforce size as prime */
    for (pindex=0; pindex < prime_table_length; pindex++) {
        if (primes[pindex] > minsize) { 
			size = primes[pindex]; 
			break; 
		}
    }

    h = (ks_hash_t *) ks_pool_alloc(pool, sizeof(ks_hash_t));
	h->pool = pool;
	h->flags = flags;
	h->destructor = destructor;

	if ((flags & KS_HASH_FLAG_RWLOCK)) {
		ks_rwl_create(&h->rwl, h->pool);
	}

	ks_mutex_create(&h->mutex, KS_MUTEX_FLAG_DEFAULT, h->pool);
	

    if (NULL == h) abort(); /*oom*/

    h->table = (struct entry **)ks_pool_alloc(h->pool, sizeof(struct entry*) * size);

    if (NULL == h->table) abort(); /*oom*/

    //memset(h->table, 0, size * sizeof(struct entry *));
    h->tablelength  = size;
    h->primeindex   = pindex;
    h->entrycount   = 0;
    h->hashfn       = hashf;
    h->eqfn         = eqf;
    h->loadlimit    = (unsigned int) ceil(size * max_load_factor);

	*hp = h;

	ks_pool_set_cleanup(pool, h, NULL, 0, ks_hash_cleanup);

	return KS_STATUS_SUCCESS;
}

/*****************************************************************************/
static int
ks_hash_expand(ks_hash_t *h)
{
    /* Double the size of the table to accomodate more entries */
    struct entry **newtable;
    struct entry *e;
    struct entry **pE;
    unsigned int newsize, i, index;
    /* Check we're not hitting max capacity */
    if (h->primeindex == (prime_table_length - 1)) return 0;
    newsize = primes[++(h->primeindex)];

    newtable = (struct entry **)ks_pool_alloc(h->pool, sizeof(struct entry*) * newsize);
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
			ks_pool_safe_free(h->pool, h->table);
			h->table = newtable;
		}
    /* Plan B: realloc instead */
    else 
		{
			newtable = (struct entry **)
				ks_pool_resize(h->pool, h->table, newsize * sizeof(struct entry *));
			if (NULL == newtable) { (h->primeindex)--; return 0; }
			h->table = newtable;
			memset(newtable[h->tablelength], 0, newsize - h->tablelength);
			for (i = 0; i < h->tablelength; i++) {
				for (pE = &(newtable[i]), e = *pE; e != NULL; e = *pE) {
					index = indexFor(newsize,e->h);

					if (index == i) {
						pE = &(e->next);
					} else {
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
KS_DECLARE(unsigned int)
ks_hash_count(ks_hash_t *h)
{
    return h->entrycount;
}

static void * _ks_hash_remove(ks_hash_t *h, void *k, unsigned int hashvalue, unsigned int index) {
    /* TODO: consider compacting the table when the load factor drops enough,
     *       or provide a 'compact' method. */

    struct entry *e;
    struct entry **pE;
    void *v;


    pE = &(h->table[index]);
    e = *pE;
    while (NULL != e) {
		/* Check hash value to short circuit heavier comparison */
		if ((hashvalue == e->h) && (h->eqfn(k, e->k))) {
			*pE = e->next;
			h->entrycount--;
			v = e->v;
			if (e->flags & KS_HASH_FLAG_FREE_KEY) {
				ks_pool_free(h->pool, e->k);
			}
			if (e->flags & KS_HASH_FLAG_FREE_VALUE) {
				ks_pool_safe_free(h->pool, e->v); 
				v = NULL;
			} else if (e->destructor) {
				e->destructor(e->v);
				v = e->v = NULL;
			} else if (h->destructor) {
				h->destructor(e->v);
				v = e->v = NULL;
			}
			ks_pool_safe_free(h->pool, e);
			return v;
		}
		pE = &(e->next);
		e = e->next;
	}
    return NULL;
}

/*****************************************************************************/
KS_DECLARE(int)
ks_hash_insert_ex(ks_hash_t *h, void *k, void *v, ks_hash_flag_t flags, ks_hash_destructor_t destructor)
{
    struct entry *e;
	unsigned int hashvalue = hash(h, k);
    unsigned index = indexFor(h->tablelength, hashvalue);

	ks_hash_write_lock(h);

	if (!flags) {
		flags = h->flags;
	}

	if (flags & KS_HASH_FLAG_DUP_CHECK) {
		_ks_hash_remove(h, k, hashvalue, index);
	}

    if (++(h->entrycount) > h->loadlimit)
		{
			/* Ignore the return value. If expand fails, we should
			 * still try cramming just this value into the existing table
			 * -- we may not have memory for a larger table, but one more
			 * element may be ok. Next time we insert, we'll try expanding again.*/
			ks_hash_expand(h);
			index = indexFor(h->tablelength, hashvalue);
		}
    e = (struct entry *)ks_pool_alloc(h->pool, sizeof(struct entry));
    if (NULL == e) { --(h->entrycount); return 0; } /*oom*/
    e->h = hashvalue;
    e->k = k;
    e->v = v;
	e->flags = flags;
	e->destructor = destructor;
    e->next = h->table[index];
    h->table[index] = e;

	ks_hash_write_unlock(h);

    return -1;
}


KS_DECLARE(void) ks_hash_write_lock(ks_hash_t *h)
{
	if ((h->flags & KS_HASH_FLAG_RWLOCK)) {
		ks_rwl_write_lock(h->rwl);
	} else {
		ks_mutex_lock(h->mutex);
	}
}

KS_DECLARE(void) ks_hash_write_unlock(ks_hash_t *h)
{
	if ((h->flags & KS_HASH_FLAG_RWLOCK)) {
		ks_rwl_write_unlock(h->rwl);
	} else {
		ks_mutex_unlock(h->mutex);
	}
}

KS_DECLARE(ks_status_t) ks_hash_read_lock(ks_hash_t *h)
{
	if (!(h->flags & KS_HASH_FLAG_RWLOCK)) {
		return KS_STATUS_INACTIVE;
	}

	ks_rwl_read_lock(h->rwl);

	ks_mutex_lock(h->mutex);
	h->readers++;
	ks_mutex_unlock(h->mutex);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_hash_read_unlock(ks_hash_t *h)
{
	if (!(h->flags & KS_HASH_FLAG_RWLOCK)) {
		return KS_STATUS_INACTIVE;
	}

	ks_mutex_lock(h->mutex);
	h->readers--;
	ks_mutex_unlock(h->mutex);

	ks_rwl_read_unlock(h->rwl);

	return KS_STATUS_SUCCESS;
}

/*****************************************************************************/
KS_DECLARE(void *) /* returns value associated with key */
ks_hash_search(ks_hash_t *h, void *k, ks_locked_t locked)
{
    struct entry *e;
    unsigned int hashvalue, index;
	void *v = NULL;

	ks_assert(locked != KS_READLOCKED || (h->flags & KS_HASH_FLAG_RWLOCK));

    hashvalue = hash(h,k);
    index = indexFor(h->tablelength,hashvalue);

	if (locked == KS_READLOCKED) {
		ks_rwl_read_lock(h->rwl);

		ks_mutex_lock(h->mutex);
		h->readers++;
		ks_mutex_unlock(h->mutex);
	}

    e = h->table[index];
    while (NULL != e) {
		/* Check hash value to short circuit heavier comparison */
		if ((hashvalue == e->h) && (h->eqfn(k, e->k))) {
			v = e->v;
			break;
		}
		e = e->next;
	}

    return v;
}

/*****************************************************************************/
KS_DECLARE(void *) /* returns value associated with key */
ks_hash_remove(ks_hash_t *h, void *k)
{
	void *v;
	unsigned int hashvalue = hash(h,k);

	ks_hash_write_lock(h);
	v = _ks_hash_remove(h, k, hashvalue, indexFor(h->tablelength,hashvalue));
	ks_hash_write_unlock(h);

	return v;
}

/*****************************************************************************/
/* destroy */
KS_DECLARE(void)
ks_hash_destroy(ks_hash_t **h)
{
    unsigned int i;
    struct entry *e, *f;
    struct entry **table = (*h)->table;
	ks_pool_t *pool;

	ks_hash_write_lock(*h);

	for (i = 0; i < (*h)->tablelength; i++) {
		e = table[i];
		while (NULL != e) {
			f = e; e = e->next; 

			if (f->flags & KS_HASH_FLAG_FREE_KEY) {
				ks_pool_free((*h)->pool, f->k); 
			}
			
			if (f->flags & KS_HASH_FLAG_FREE_VALUE) {
				ks_pool_safe_free((*h)->pool, f->v); 
			} else if (f->destructor) {
				f->destructor(f->v);
				f->v = NULL;
			} else if ((*h)->destructor) {
				(*h)->destructor(f->v);
				f->v = NULL;
			}
			ks_pool_safe_free((*h)->pool, f); 
		}
	}

	pool = (*h)->pool;
    ks_pool_safe_free(pool, (*h)->table);
	ks_hash_write_unlock(*h);
	if ((*h)->rwl) ks_pool_free(pool, (*h)->rwl);
	ks_pool_free(pool, (*h)->mutex);
	ks_pool_free(pool, *h);
	pool = NULL;
	*h = NULL;


}

KS_DECLARE(void) ks_hash_last(ks_hash_iterator_t **iP)
{
	ks_hash_iterator_t *i = *iP;

	//ks_assert(i->locked != KS_READLOCKED || (i->h->flags & KS_HASH_FLAG_RWLOCK));

	if (i->locked == KS_READLOCKED) {
		ks_mutex_lock(i->h->mutex);
		i->h->readers--;
		ks_mutex_unlock(i->h->mutex);
		
		ks_rwl_read_unlock(i->h->rwl);
	}

	ks_pool_free(i->h->pool, i);
	
	*iP = NULL;
}

KS_DECLARE(ks_hash_iterator_t *) ks_hash_next(ks_hash_iterator_t **iP)
{

	ks_hash_iterator_t *i = *iP;

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
		goto end;
	}
	
	if ((i->e = i->h->table[i->pos]) != 0) { 
		return i;
	}

 end:

	ks_hash_last(iP);

	return NULL;
}

KS_DECLARE(ks_hash_iterator_t *) ks_hash_first(ks_hash_t *h, ks_locked_t locked)
{
	ks_hash_iterator_t *iterator;

	ks_assert(locked != KS_READLOCKED || (h->flags & KS_HASH_FLAG_RWLOCK));

	iterator = ks_pool_alloc(h->pool, sizeof(*iterator));
	ks_assert(iterator);

	iterator->pos = 0;
	iterator->e = NULL;
	iterator->h = h;

	if (locked == KS_READLOCKED) {
		ks_rwl_read_lock(h->rwl);
		iterator->locked = locked;
		ks_mutex_lock(h->mutex);
		h->readers++;
		ks_mutex_unlock(h->mutex);
	}

	return ks_hash_next(&iterator);
}

KS_DECLARE(void) ks_hash_this_val(ks_hash_iterator_t *i, void *val)
{
	if (i->e) {
		i->e->v = val;
	}
}

KS_DECLARE(void) ks_hash_this(ks_hash_iterator_t *i, const void **key, ks_ssize_t *klen, void **val)
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

