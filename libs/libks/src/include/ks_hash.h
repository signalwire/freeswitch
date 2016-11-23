/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * ks_hash.h -- Ks_Hash
 *
 */


/* ks_hash.h Copyright (C) 2002 Christopher Clark <firstname.lastname@cl.cam.ac.uk> */

#ifndef __KS_HASH_CWC22_H__
#define __KS_HASH_CWC22_H__

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#endif

#include "ks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ks_hash ks_hash_t;
typedef struct ks_hash_iterator ks_hash_iterator_t;

typedef enum {
	KS_UNLOCKED,
	KS_READLOCKED
} ks_locked_t;


/* Example of use:
 *
 *      ks_hash_t  *h;
 *      struct some_key   *k;
 *      struct some_value *v;
 *
 *      static unsigned int         hash_from_key_fn( void *k );
 *      static int                  keys_equal_fn ( void *key1, void *key2 );
 *
 *      h = ks_hash_create(16, hash_from_key_fn, keys_equal_fn);
 *      k = (struct some_key *)     malloc(sizeof(struct some_key));
 *      v = (struct some_value *)   malloc(sizeof(struct some_value));
 *
 *      (initialise k and v to suitable values)
 * 
 *      if (! ks_hash_insert(h,k,v) )
 *      {     exit(-1);               }
 *
 *      if (NULL == (found = ks_hash_search(h,k) ))
 *      {    printf("not found!");                  }
 *
 *      if (NULL == (found = ks_hash_remove(h,k) ))
 *      {    printf("Not found\n");                 }
 *
 */

/* Macros may be used to define type-safe(r) ks_hash access functions, with
 * methods specialized to take known key and value types as parameters.
 * 
 * Example:
 *
 * Insert this at the start of your file:
 *
 * DEFINE_KS_HASH_INSERT(insert_some, struct some_key, struct some_value);
 * DEFINE_KS_HASH_SEARCH(search_some, struct some_key, struct some_value);
 * DEFINE_KS_HASH_REMOVE(remove_some, struct some_key, struct some_value);
 *
 * This defines the functions 'insert_some', 'search_some' and 'remove_some'.
 * These operate just like ks_hash_insert etc., with the same parameters,
 * but their function signatures have 'struct some_key *' rather than
 * 'void *', and hence can generate compile time errors if your program is
 * supplying incorrect data as a key (and similarly for value).
 *
 * Note that the hash and key equality functions passed to ks_hash_create
 * still take 'void *' parameters instead of 'some key *'. This shouldn't be
 * a difficult issue as they're only defined and passed once, and the other
 * functions will ensure that only valid keys are supplied to them.
 *
 * The cost for this checking is increased code size and runtime overhead
 * - if performance is important, it may be worth switching back to the
 * unsafe methods once your program has been debugged with the safe methods.
 * This just requires switching to some simple alternative defines - eg:
 * #define insert_some ks_hash_insert
 *
 */


typedef enum {
	KS_HASH_FLAG_NONE = 0,
	KS_HASH_FLAG_DEFAULT = (1 << 0),
	KS_HASH_FLAG_FREE_KEY = (1 << 1),
	KS_HASH_FLAG_FREE_VALUE = (1 << 2),
	KS_HASH_FLAG_RWLOCK = (1 << 3),
	KS_HASH_FLAG_DUP_CHECK = (1 << 4)
} ks_hash_flag_t;

#define KS_HASH_FREE_BOTH KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE

typedef enum {
	KS_HASH_MODE_DEFAULT = 0,
	KS_HASH_MODE_CASE_SENSITIVE,
	KS_HASH_MODE_CASE_INSENSITIVE,
	KS_HASH_MODE_INT,
	KS_HASH_MODE_INT64,
	KS_HASH_MODE_PTR
} ks_hash_mode_t;



/*****************************************************************************
 * ks_hash_create
   
 * @name                    ks_hash_create
 * @param   minsize         minimum initial size of ks_hash
 * @param   hashfunction    function for hashing keys
 * @param   key_eq_fn       function for determining key equality
 * @return                  newly created ks_hash or NULL on failure
 */

KS_DECLARE(ks_status_t)
ks_hash_create_ex(ks_hash_t **hp, unsigned int minsize,
				  unsigned int (*hashfunction) (void*),
				  int (*key_eq_fn) (void*,void*), ks_hash_mode_t mode, ks_hash_flag_t flags, ks_hash_destructor_t destructor, ks_pool_t *pool);

/*****************************************************************************
 * ks_hash_insert
   
 * @name        ks_hash_insert
 * @param   h   the ks_hash to insert into
 * @param   k   the key - ks_hash claims ownership and will free on removal
 * @param   v   the value - does not claim ownership
 * @return      non-zero for successful insertion
 *
 * This function will cause the table to expand if the insertion would take
 * the ratio of entries to table size over the maximum load factor.
 *
 * This function does not check for repeated insertions with a duplicate key.
 * The value returned when using a duplicate key is undefined -- when
 * the ks_hash changes size, the order of retrieval of duplicate key
 * entries is reversed.
 * If in doubt, remove before insert.
 */


KS_DECLARE(int) ks_hash_insert_ex(ks_hash_t *h, void *k, void *v, ks_hash_flag_t flags, ks_hash_destructor_t destructor);
#define ks_hash_insert(_h, _k, _v) ks_hash_insert_ex(_h, _k, _v, 0, NULL)

#define DEFINE_KS_HASH_INSERT(fnname, keytype, valuetype)		\
	int fnname (ks_hash_t *h, keytype *k, valuetype *v)	\
	{															\
		return ks_hash_insert(h,k,v);							\
	}


KS_DECLARE(void) ks_hash_set_flags(ks_hash_t *h, ks_hash_flag_t flags);
KS_DECLARE(void) ks_hash_set_destructor(ks_hash_t *h, ks_hash_destructor_t destructor);

/*****************************************************************************
 * ks_hash_search
   
 * @name        ks_hash_search
 * @param   h   the ks_hash to search
 * @param   k   the key to search for  - does not claim ownership
 * @return      the value associated with the key, or NULL if none found
 */

KS_DECLARE(void *)
ks_hash_search(ks_hash_t *h, void *k, ks_locked_t locked);

#define DEFINE_KS_HASH_SEARCH(fnname, keytype, valuetype) \
	valuetype * fnname (ks_hash_t *h, keytype *k)	\
	{														\
		return (valuetype *) (ks_hash_search(h,k));		\
	}

/*****************************************************************************
 * ks_hash_remove
   
 * @name        ks_hash_remove
 * @param   h   the ks_hash to remove the item from
 * @param   k   the key to search for  - does not claim ownership
 * @return      the value associated with the key, or NULL if none found
 */

KS_DECLARE(void *) /* returns value */
ks_hash_remove(ks_hash_t *h, void *k);

#define DEFINE_KS_HASH_REMOVE(fnname, keytype, valuetype) \
	valuetype * fnname (ks_hash_t *h, keytype *k)	\
	{														\
		return (valuetype *) (ks_hash_remove(h,k));		\
	}


/*****************************************************************************
 * ks_hash_count
   
 * @name        ks_hash_count
 * @param   h   the ks_hash
 * @return      the number of items stored in the ks_hash
 */
KS_DECLARE(unsigned int)
ks_hash_count(ks_hash_t *h);

/*****************************************************************************
 * ks_hash_destroy
   
 * @name        ks_hash_destroy
 * @param   h   the ks_hash
 * @param       free_values     whether to call 'free' on the remaining values
 */

KS_DECLARE(void)
ks_hash_destroy(ks_hash_t **h);

KS_DECLARE(ks_hash_iterator_t*) ks_hash_first(ks_hash_t *h, ks_locked_t locked);
KS_DECLARE(void) ks_hash_last(ks_hash_iterator_t **iP);
KS_DECLARE(ks_hash_iterator_t*) ks_hash_next(ks_hash_iterator_t **iP);
KS_DECLARE(void) ks_hash_this(ks_hash_iterator_t *i, const void **key, ks_ssize_t *klen, void **val);
KS_DECLARE(void) ks_hash_this_val(ks_hash_iterator_t *i, void *val);
KS_DECLARE(ks_status_t) ks_hash_create(ks_hash_t **hp, ks_hash_mode_t mode, ks_hash_flag_t flags, ks_pool_t *pool);

KS_DECLARE(void) ks_hash_write_lock(ks_hash_t *h);
KS_DECLARE(void) ks_hash_write_unlock(ks_hash_t *h);
KS_DECLARE(ks_status_t) ks_hash_read_lock(ks_hash_t *h);
KS_DECLARE(ks_status_t) ks_hash_read_unlock(ks_hash_t *h);


static __inline uint32_t ks_hash_default_int64(void *ky)
{
	int64_t key = *((int64_t *)ky);
	key = (~key) + (key << 18);
	key = key ^ (key >> 31);
	key = key * 21;
	key = key ^ (key >> 11);
	key = key + (key << 6);
	key = key ^ (key >> 22);
	return (uint32_t) key;
}

static __inline int ks_hash_equalkeys_int64(void *k1, void *k2)
{
    return *(uint64_t *)k1 == *(uint64_t *)k2;
}

static __inline uint32_t ks_hash_default_int(void *ky) {
	uint32_t x = *((uint32_t *)ky);
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x);
	return x;
}

static __inline int ks_hash_equalkeys_int(void *k1, void *k2)
{
    return *(uint32_t *)k1 == *(uint32_t *)k2;
}
#if 0
}
#endif

static __inline uint32_t ks_hash_default_ptr(void *ky)
{
#ifdef KS_64BIT
	return ks_hash_default_int64(ky);
#endif
	return ks_hash_default_int(ky);
}

static __inline int ks_hash_equalkeys_ptr(void *k1, void *k2)
{
#ifdef KS_64BIT
	return ks_hash_equalkeys_int64(k1, k2);
#endif
	return ks_hash_equalkeys_int(k1, k2);
}


static __inline int ks_hash_equalkeys(void *k1, void *k2)
{
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

static __inline int ks_hash_equalkeys_ci(void *k1, void *k2)
{
    return strcasecmp((char *) k1, (char *) k2) ? 0 : 1;
}

static __inline uint32_t ks_hash_default(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	uint32_t hash = 0;
    int c;
	
	while ((c = *str)) {
		str++;
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}

static __inline uint32_t ks_hash_default_ci(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	uint32_t hash = 0;
    int c;
	
	while ((c = ks_tolower(*str))) {
		str++;
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}




#ifdef __cplusplus
} /* extern C */
#endif

#endif /* __KS_HASH_CWC22_H__ */

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
