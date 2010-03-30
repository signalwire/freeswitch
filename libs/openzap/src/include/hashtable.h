/* Copyright (C) 2002 Christopher Clark <firstname.lastname@cl.cam.ac.uk> */

#ifndef __HASHTABLE_CWC22_H__
#define __HASHTABLE_CWC22_H__
#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#endif
#include "openzap.h"

struct hashtable;
struct hashtable_iterator;

/* Example of use:
 *
 *      struct hashtable  *h;
 *      struct some_key   *k;
 *      struct some_value *v;
 *
 *      static unsigned int         hash_from_key_fn( void *k );
 *      static int                  keys_equal_fn ( void *key1, void *key2 );
 *
 *      h = create_hashtable(16, hash_from_key_fn, keys_equal_fn);
 *      k = (struct some_key *)     malloc(sizeof(struct some_key));
 *      v = (struct some_value *)   malloc(sizeof(struct some_value));
 *
 *      (initialise k and v to suitable values)
 * 
 *      if (! hashtable_insert(h,k,v) )
 *      {     exit(-1);               }
 *
 *      if (NULL == (found = hashtable_search(h,k) ))
 *      {    printf("not found!");                  }
 *
 *      if (NULL == (found = hashtable_remove(h,k) ))
 *      {    printf("Not found\n");                 }
 *
 */

/* Macros may be used to define type-safe(r) hashtable access functions, with
 * methods specialized to take known key and value types as parameters.
 * 
 * Example:
 *
 * Insert this at the start of your file:
 *
 * DEFINE_HASHTABLE_INSERT(insert_some, struct some_key, struct some_value);
 * DEFINE_HASHTABLE_SEARCH(search_some, struct some_key, struct some_value);
 * DEFINE_HASHTABLE_REMOVE(remove_some, struct some_key, struct some_value);
 *
 * This defines the functions 'insert_some', 'search_some' and 'remove_some'.
 * These operate just like hashtable_insert etc., with the same parameters,
 * but their function signatures have 'struct some_key *' rather than
 * 'void *', and hence can generate compile time errors if your program is
 * supplying incorrect data as a key (and similarly for value).
 *
 * Note that the hash and key equality functions passed to create_hashtable
 * still take 'void *' parameters instead of 'some key *'. This shouldn't be
 * a difficult issue as they're only defined and passed once, and the other
 * functions will ensure that only valid keys are supplied to them.
 *
 * The cost for this checking is increased code size and runtime overhead
 * - if performance is important, it may be worth switching back to the
 * unsafe methods once your program has been debugged with the safe methods.
 * This just requires switching to some simple alternative defines - eg:
 * #define insert_some hashtable_insert
 *
 */

/*****************************************************************************
 * create_hashtable
   
 * @name                    create_hashtable
 * @param   minsize         minimum initial size of hashtable
 * @param   hashfunction    function for hashing keys
 * @param   key_eq_fn       function for determining key equality
 * @return                  newly created hashtable or NULL on failure
 */

OZ_DECLARE(struct hashtable *)
create_hashtable(unsigned int minsize,
                 unsigned int (*hashfunction) (void*),
                 int (*key_eq_fn) (void*,void*));

/*****************************************************************************
 * hashtable_insert
   
 * @name        hashtable_insert
 * @param   h   the hashtable to insert into
 * @param   k   the key - hashtable claims ownership and will free on removal
 * @param   v   the value - does not claim ownership
 * @return      non-zero for successful insertion
 *
 * This function will cause the table to expand if the insertion would take
 * the ratio of entries to table size over the maximum load factor.
 *
 * This function does not check for repeated insertions with a duplicate key.
 * The value returned when using a duplicate key is undefined -- when
 * the hashtable changes size, the order of retrieval of duplicate key
 * entries is reversed.
 * If in doubt, remove before insert.
 */


typedef enum {
	HASHTABLE_FLAG_NONE = 0,
	HASHTABLE_FLAG_FREE_KEY = (1 << 0),
	HASHTABLE_FLAG_FREE_VALUE = (1 << 1)
} hashtable_flag_t;

OZ_DECLARE(int)
hashtable_insert(struct hashtable *h, void *k, void *v, hashtable_flag_t flags);

#define DEFINE_HASHTABLE_INSERT(fnname, keytype, valuetype)		\
	int fnname (struct hashtable *h, keytype *k, valuetype *v)	\
	{															\
		return hashtable_insert(h,k,v);							\
	}

/*****************************************************************************
 * hashtable_search
   
 * @name        hashtable_search
 * @param   h   the hashtable to search
 * @param   k   the key to search for  - does not claim ownership
 * @return      the value associated with the key, or NULL if none found
 */

OZ_DECLARE(void *)
hashtable_search(struct hashtable *h, void *k);

#define DEFINE_HASHTABLE_SEARCH(fnname, keytype, valuetype) \
	valuetype * fnname (struct hashtable *h, keytype *k)	\
	{														\
		return (valuetype *) (hashtable_search(h,k));		\
	}

/*****************************************************************************
 * hashtable_remove
   
 * @name        hashtable_remove
 * @param   h   the hashtable to remove the item from
 * @param   k   the key to search for  - does not claim ownership
 * @return      the value associated with the key, or NULL if none found
 */

OZ_DECLARE(void *) /* returns value */
hashtable_remove(struct hashtable *h, void *k);

#define DEFINE_HASHTABLE_REMOVE(fnname, keytype, valuetype) \
	valuetype * fnname (struct hashtable *h, keytype *k)	\
	{														\
		return (valuetype *) (hashtable_remove(h,k));		\
	}


/*****************************************************************************
 * hashtable_count
   
 * @name        hashtable_count
 * @param   h   the hashtable
 * @return      the number of items stored in the hashtable
 */
OZ_DECLARE(unsigned int)
hashtable_count(struct hashtable *h);


/*****************************************************************************
 * hashtable_destroy
   
 * @name        hashtable_destroy
 * @param   h   the hashtable
 * @param       free_values     whether to call 'free' on the remaining values
 */

OZ_DECLARE(void)
hashtable_destroy(struct hashtable *h);

OZ_DECLARE(struct hashtable_iterator*) hashtable_first(struct hashtable *h);
OZ_DECLARE(struct hashtable_iterator*) hashtable_next(struct hashtable_iterator *i);
OZ_DECLARE(void) hashtable_this(struct hashtable_iterator *i, const void **key, int *klen, void **val);

#endif /* __HASHTABLE_CWC22_H__ */

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
