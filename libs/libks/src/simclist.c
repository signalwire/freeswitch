/*
* Copyright (c) 2007,2008,2009,2010,2011 Mij <mij@bitchx.it>
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


/*
* SimCList library. See http://mij.oltrelinux.com/devel/simclist
*/

/* SimCList implementation, version 1.6 */
#include <ks.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>          /* for setting errno */
#include <sys/types.h>
#ifndef _WIN32
/* not in Windows! */
#   include <unistd.h>
#   include <stdint.h>
#endif
#ifndef SIMCLIST_NO_DUMPRESTORE
/* includes for dump/restore */
#   include <time.h>
#   include <sys/uio.h>     /* for READ_ERRCHECK() and write() */
#   include <fcntl.h>       /* for open() etc */
#   ifndef _WIN32
#       include <arpa/inet.h>  /* for htons() on UNIX */
#   else
#       include <winsock2.h>  /* for htons() on Windows */
#   endif
#endif

/* disable asserts */
#ifndef SIMCLIST_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <assert.h>


#include <sys/stat.h>       /* for open()'s access modes S_IRUSR etc */
#include <limits.h>

#if defined(_MSC_VER) || defined(__MINGW32__)
/* provide gettimeofday() missing in Windows */
#ifdef _MSC_VER
#pragma comment(lib, "Winmm.lib")
#endif
int gettimeofday(struct timeval *tp, void *tzp) {
	DWORD t;

	/* XSI says: "If tzp is not a null pointer, the behavior is unspecified" */
	ks_assert(tzp == NULL);

	t = timeGetTime();
	tp->tv_sec = t / 1000;
	tp->tv_usec = t % 1000;
	return 0;
}
#endif


/* work around lack of inttypes.h support in broken Microsoft Visual Studio compilers */
#if !defined(_WIN32) || !defined(_MSC_VER)
#   include <inttypes.h>   /* (u)int*_t */
#else
#   include <basetsd.h>
typedef UINT8   uint8_t;
typedef UINT16  uint16_t;
typedef ULONG32 uint32_t;
typedef UINT64  uint64_t;
typedef INT8    int8_t;
typedef INT16   int16_t;
typedef LONG32  int32_t;
typedef INT64   int64_t;
#endif


/* define some commodity macros for Dump/Restore functionality */
#ifndef SIMCLIST_NO_DUMPRESTORE
/* write() decorated with error checking logic */
#define WRITE_ERRCHECK(fd, msgbuf, msglen)      do {                                                    \
                                                    if (write(fd, msgbuf, msglen) < 0) return -1;       \
                                                } while (0);
/* READ_ERRCHECK() decorated with error checking logic */
#define READ_ERRCHECK(fd, msgbuf, msglen)      do {                                                     \
                                                    if (read(fd, msgbuf, msglen) != msglen) {           \
                                                        /*errno = EPROTO;*/                             \
                                                        return -1;                                      \
                                                    }                                                   \
                                                } while (0);

/* convert 64bit integers from host to network format */
#define hton64(x)       (\
        htons(1) == 1 ?                                         \
            (uint64_t)x      /* big endian */                   \
        :       /* little endian */                             \
        ((uint64_t)((((uint64_t)(x) & 0xff00000000000000ULL) >> 56) | \
            (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) | \
            (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) | \
            (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) | \
            (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) | \
            (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) | \
            (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) | \
            (((uint64_t)(x) & 0x00000000000000ffULL) << 56)))   \
        )

/* convert 64bit integers from network to host format */
#define ntoh64(x)       (hton64(x))
#endif

/* some OSes don't have EPROTO (eg OpenBSD) */
#ifndef EPROTO
#define EPROTO  EIO
#endif

#ifdef SIMCLIST_WITH_THREADS
/* limit (approx) to the number of threads running
* for threaded operations. Only meant when
* SIMCLIST_WITH_THREADS is defined */
#define SIMCLIST_MAXTHREADS   2
#endif

/*
* how many elems to keep as spare. During a deletion, an element
* can be saved in a "free-list", not free()d immediately. When
* latter insertions are performed, spare elems can be used instead
* of malloc()ing new elems.
*
* about this param, some values for appending
* 10 million elems into an empty list:
* (#, time[sec], gain[%], gain/no[%])
* 0    2,164   0,00    0,00    <-- feature disabled
* 1    1,815   34,9    34,9
* 2    1,446   71,8    35,9    <-- MAX gain/no
* 3    1,347   81,7    27,23
* 5    1,213   95,1    19,02
* 8    1,064   110,0   13,75
* 10   1,015   114,9   11,49   <-- MAX gain w/ likely sol
* 15   1,019   114,5   7,63
* 25   0,985   117,9   4,72
* 50   1,088   107,6   2,15
* 75   1,016   114,8   1,53
* 100  0,988   117,6   1,18
* 150  1,022   114,2   0,76
* 200  0,939   122,5   0,61    <-- MIN time
*/
#ifndef SIMCLIST_MAX_SPARE_ELEMS
#define SIMCLIST_MAX_SPARE_ELEMS        5
#endif


#ifdef SIMCLIST_WITH_THREADS
#include <pthread.h>
#endif

#include "simclist.h"


/* minumum number of elements for sorting with quicksort instead of insertion */
#define SIMCLIST_MINQUICKSORTELS        24


/* list dump declarations */
#define SIMCLIST_DUMPFORMAT_VERSION     1   /* (short integer) version of fileformat managed by _dump* and _restore* functions */

// @todo this is not correct, the header would be padded by default on version for 2 more bytes, and treating the structure as 30 bytes would cut the last 2 bytes off the listhash at the end
#define SIMCLIST_DUMPFORMAT_HEADERLEN   30  /* length of the header */

/* header for a list dump */
struct ks_list_dump_header_s {
	uint16_t ver;               /* version */
	int32_t timestamp_sec;      /* dump timestamp, seconds since UNIX Epoch */
	int32_t timestamp_usec;     /* dump timestamp, microseconds since timestamp_sec */
	int32_t rndterm;            /* random value terminator -- terminates the data sequence */

	uint32_t totlistlen;        /* sum of every element' size, bytes */
	uint32_t numels;            /* number of elements */
	uint32_t elemlen;           /* bytes length of an element, for constant-size lists, <= 0 otherwise */
	int32_t listhash;           /* hash of the list at the time of dumping, or 0 if to be ignored */
};



/* deletes tmp from list, with care wrt its position (head, tail, middle) */
static int ks_list_drop_elem(ks_list_t *restrict l, struct ks_list_entry_s *tmp, unsigned int pos);

/* set default values for initialized lists */
static int ks_list_attributes_setdefaults(ks_list_t *restrict l);

/* check whether the list internal REPresentation is valid -- Costs O(n) */
static int ks_list_repOk(const ks_list_t *restrict l);

/* check whether the list attribute set is valid -- Costs O(1) */
static int ks_list_attrOk(const ks_list_t *restrict l);

/* do not inline, this is recursive */
static void ks_list_sort_quicksort(ks_list_t *restrict l, int versus,
	unsigned int first, struct ks_list_entry_s *fel,
	unsigned int last, struct ks_list_entry_s *lel);

static inline void ks_list_sort_selectionsort(ks_list_t *restrict l, int versus,
	unsigned int first, struct ks_list_entry_s *fel,
	unsigned int last, struct ks_list_entry_s *lel);

static void *ks_list_get_minmax(const ks_list_t *restrict l, int versus);

static inline struct ks_list_entry_s *ks_list_findpos(const ks_list_t *restrict l, int posstart);

/*
* Random Number Generator
*
* The user is expected to seed the RNG (ie call srand()) if
* SIMCLIST_SYSTEM_RNG is defined.
*
* Otherwise, a self-contained RNG based on LCG is used; see
* http://en.wikipedia.org/wiki/Linear_congruential_generator .
*
* Facts pro local RNG:
* 1. no need for the user to call srand() on his own
* 2. very fast, possibly faster than OS
* 3. avoid interference with user's RNG
*
* Facts pro system RNG:
* 1. may be more accurate (irrelevant for SimCList randno purposes)
* 2. why reinvent the wheel
*
* Default to local RNG for user's ease of use.
*/

#ifdef SIMCLIST_SYSTEM_RNG
/* keep track whether we initialized already (non-0) or not (0) */
static unsigned random_seed = 0;

/* use local RNG */
static inline void seed_random(void) {
	if (random_seed == 0)
		random_seed = (unsigned)getpid() ^ (unsigned)time(NULL);
}

static inline long get_random(void) {
	random_seed = (1664525 * random_seed + 1013904223);
	return random_seed;
}

#else
/* use OS's random generator */
#   define  seed_random()
#   define  get_random()        (rand())
#endif


static void ks_list_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	ks_list_t *l = (ks_list_t *)ptr;

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_list_clear(l);
		break;
	case KS_MPCL_DESTROY:
		ks_rwl_write_lock(l->lock);
		for (unsigned int i = 0; i < l->spareelsnum; i++) ks_pool_free(l->pool, &l->spareels[i]);
		l->spareelsnum = 0;
		ks_pool_free(l->pool, &l->spareels);
		ks_pool_free(l->pool, &l->head_sentinel);
		ks_pool_free(l->pool, &l->tail_sentinel);
		ks_rwl_write_unlock(l->lock);
		ks_rwl_destroy(&l->lock);
		break;
	}
}

/* list initialization */
KS_DECLARE(ks_status_t) ks_list_create(ks_list_t **list, ks_pool_t *pool) {
	ks_list_t *l = NULL;

	ks_assert(list);
	ks_assert(pool);

	seed_random();

	l = ks_pool_alloc(pool, sizeof(ks_list_t));
	ks_assert(l);

	l->pool = pool;
	l->numels = 0;

	ks_rwl_create(&l->lock, pool);
	ks_assert(l->lock);

	/* head/tail sentinels and mid pointer */
	l->head_sentinel = (struct ks_list_entry_s *)ks_pool_alloc(pool, sizeof(struct ks_list_entry_s));
	l->tail_sentinel = (struct ks_list_entry_s *)ks_pool_alloc(pool, sizeof(struct ks_list_entry_s));
	l->head_sentinel->next = l->tail_sentinel;
	l->tail_sentinel->prev = l->head_sentinel;
	l->head_sentinel->prev = l->tail_sentinel->next = l->mid = NULL;
	l->head_sentinel->data = l->tail_sentinel->data = NULL;

	/* iteration attributes */
	l->iter_active = 0;
	l->iter_pos = 0;
	l->iter_curentry = NULL;

	/* free-list attributes */
	l->spareels = (struct ks_list_entry_s **)ks_pool_alloc(pool, SIMCLIST_MAX_SPARE_ELEMS * sizeof(struct ks_list_entry_s *));
	l->spareelsnum = 0;

#ifdef SIMCLIST_WITH_THREADS
	l->threadcount = 0;
#endif

	ks_list_attributes_setdefaults(l);

	ks_assert(ks_list_repOk(l));
	ks_assert(ks_list_attrOk(l));

	ks_assert(ks_pool_set_cleanup(pool, l, NULL, ks_list_cleanup) == KS_STATUS_SUCCESS);

	*list = l;
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_list_destroy(ks_list_t **list) {
	ks_list_t *l = NULL;

	ks_assert(list);
	
	l = *list;
	*list = NULL;
	if (!l) return KS_STATUS_FAIL;

	ks_pool_free(l->pool, &l);

	return KS_STATUS_SUCCESS;
}

int ks_list_attributes_setdefaults(ks_list_t *restrict l) {
	l->attrs.comparator = NULL;
	l->attrs.seeker = NULL;

	/* also free() element data when removing and element from the list */
	l->attrs.meter = NULL;
	l->attrs.copy_data = 0;

	l->attrs.hasher = NULL;

	/* serializer/unserializer */
	l->attrs.serializer = NULL;
	l->attrs.unserializer = NULL;

	ks_assert(ks_list_attrOk(l));

	return 0;
}

/* setting list properties */
int ks_list_attributes_comparator(ks_list_t *restrict l, element_comparator comparator_fun) {
	if (l == NULL) return -1;

	ks_rwl_write_lock(l->lock);
	l->attrs.comparator = comparator_fun;

	ks_assert(ks_list_attrOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

int ks_list_attributes_seeker(ks_list_t *restrict l, element_seeker seeker_fun) {
	if (l == NULL) return -1;

	ks_rwl_write_lock(l->lock);
	l->attrs.seeker = seeker_fun;

	ks_assert(ks_list_attrOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

int ks_list_attributes_copy(ks_list_t *restrict l, element_meter metric_fun, int copy_data) {
	if (l == NULL || (metric_fun == NULL && copy_data != 0)) return -1;

	ks_rwl_write_lock(l->lock);
	l->attrs.meter = metric_fun;
	l->attrs.copy_data = copy_data;

	ks_assert(ks_list_attrOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

int ks_list_attributes_hash_computer(ks_list_t *restrict l, element_hash_computer hash_computer_fun) {
	if (l == NULL) return -1;

	ks_rwl_write_lock(l->lock);
	l->attrs.hasher = hash_computer_fun;

	ks_assert(ks_list_attrOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

int ks_list_attributes_serializer(ks_list_t *restrict l, element_serializer serializer_fun) {
	if (l == NULL) return -1;

	ks_rwl_write_lock(l->lock);
	l->attrs.serializer = serializer_fun;

	ks_assert(ks_list_attrOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

int ks_list_attributes_unserializer(ks_list_t *restrict l, element_unserializer unserializer_fun) {
	if (l == NULL) return -1;

	ks_rwl_write_lock(l->lock);
	l->attrs.unserializer = unserializer_fun;

	ks_assert(ks_list_attrOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

KS_DECLARE(int) ks_list_append(ks_list_t *restrict l, const void *data) {
	return ks_list_insert_at(l, data, l->numels);
}

KS_DECLARE(int) ks_list_prepend(ks_list_t *restrict l, const void *data) {
	return ks_list_insert_at(l, data, 0);
}

KS_DECLARE(void *) ks_list_fetch(ks_list_t *restrict l) {
	return ks_list_extract_at(l, 0);
}

KS_DECLARE(void *) ks_list_get_at(const ks_list_t *restrict l, unsigned int pos) {
	struct ks_list_entry_s *tmp;
	void *data = NULL;

	ks_rwl_read_lock(l->lock);
	tmp = ks_list_findpos(l, pos);
	data = tmp != NULL ? tmp->data : NULL;
	ks_rwl_read_unlock(l->lock);

	return data;
}

KS_DECLARE(void *) ks_list_get_max(const ks_list_t *restrict l) {
	return ks_list_get_minmax(l, +1);
}

KS_DECLARE(void *) ks_list_get_min(const ks_list_t *restrict l) {
	return ks_list_get_minmax(l, -1);
}

/* REQUIRES {list->numels >= 1}
* return the min (versus < 0) or max value (v > 0) in l */
static void *ks_list_get_minmax(const ks_list_t *restrict l, int versus) {
	void *curminmax;
	struct ks_list_entry_s *s;

	if (l->attrs.comparator == NULL || l->numels == 0)
		return NULL;

	ks_rwl_read_lock(l->lock);
	curminmax = l->head_sentinel->next->data;
	for (s = l->head_sentinel->next->next; s != l->tail_sentinel; s = s->next) {
		if (l->attrs.comparator(curminmax, s->data) * versus > 0)
			curminmax = s->data;
	}
	ks_rwl_read_unlock(l->lock);

	return curminmax;
}

/* set tmp to point to element at index posstart in l */
static inline struct ks_list_entry_s *ks_list_findpos(const ks_list_t *restrict l, int posstart) {
	struct ks_list_entry_s *ptr;
	float x;
	int i;

	/* accept 1 slot overflow for fetching head and tail sentinels */
	if (posstart < -1 || posstart >(int)l->numels) return NULL;

	x = (float)(posstart + 1) / l->numels;
	if (x <= 0.25) {
		/* first quarter: get to posstart from head */
		for (i = -1, ptr = l->head_sentinel; i < posstart; ptr = ptr->next, i++);
	}
	else if (x < 0.5) {
		/* second quarter: get to posstart from mid */
		for (i = (l->numels - 1) / 2, ptr = l->mid; i > posstart; ptr = ptr->prev, i--);
	}
	else if (x <= 0.75) {
		/* third quarter: get to posstart from mid */
		for (i = (l->numels - 1) / 2, ptr = l->mid; i < posstart; ptr = ptr->next, i++);
	}
	else {
		/* fourth quarter: get to posstart from tail */
		for (i = l->numels, ptr = l->tail_sentinel; i > posstart; ptr = ptr->prev, i--);
	}

	return ptr;
}

KS_DECLARE(void *) ks_list_extract_at(ks_list_t *restrict l, unsigned int pos) {
	struct ks_list_entry_s *tmp;
	void *data;

	if (l->iter_active || pos >= l->numels) return NULL;

	ks_rwl_write_lock(l->lock);
	tmp = ks_list_findpos(l, pos);
	data = tmp->data;

	tmp->data = NULL;   /* save data from ks_list_drop_elem() free() */
	ks_list_drop_elem(l, tmp, pos);
	l->numels--;

	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return data;
}

KS_DECLARE(int) ks_list_insert_at(ks_list_t *restrict l, const void *data, unsigned int pos) {
	struct ks_list_entry_s *lent, *succ, *prec;

	if (l->iter_active || pos > l->numels) return -1;

	ks_rwl_write_lock(l->lock);
	/* this code optimizes malloc() with a free-list */
	if (l->spareelsnum > 0) {
		lent = l->spareels[l->spareelsnum - 1];
		l->spareelsnum--;
	}
	else {
		lent = (struct ks_list_entry_s *)ks_pool_alloc(l->pool, sizeof(struct ks_list_entry_s));
		ks_assert(lent);
	}

	if (l->attrs.copy_data) {
		/* make room for user' data (has to be copied) */
		ks_size_t datalen = l->attrs.meter(data);
		lent->data = (struct ks_list_entry_s *)ks_pool_alloc(l->pool, datalen);
		memcpy(lent->data, data, datalen);
	}
	else {
		lent->data = (void*)data;
	}

	/* actually append element */
	prec = ks_list_findpos(l, pos - 1);
	succ = prec->next;

	prec->next = lent;
	lent->prev = prec;
	lent->next = succ;
	succ->prev = lent;

	l->numels++;

	/* fix mid pointer */
	if (l->numels == 1) { /* first element, set pointer */
		l->mid = lent;
	}
	else if (l->numels % 2) {    /* now odd */
		if (pos >= (l->numels - 1) / 2) l->mid = l->mid->next;
	}
	else {                /* now even */
		if (pos <= (l->numels - 1) / 2) l->mid = l->mid->prev;
	}

	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return 1;
}

KS_DECLARE(int) ks_list_delete(ks_list_t *restrict l, const void *data) {
	int pos, r;
	int ret = 0;

	ks_rwl_write_lock(l->lock);

	pos = ks_list_locate(l, data, KS_TRUE);
	if (pos < 0) {
		ret = -1;
		goto done;
	}

	r = ks_list_delete_at(l, pos);
	if (r < 0) ret = -1;

done:
	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return ret;
}

KS_DECLARE(int) ks_list_delete_at(ks_list_t *restrict l, unsigned int pos) {
	struct ks_list_entry_s *delendo;

	if (l->iter_active || pos >= l->numels) return -1;

	ks_rwl_write_lock(l->lock);

	delendo = ks_list_findpos(l, pos);

	ks_list_drop_elem(l, delendo, pos);

	l->numels--;

	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return  0;
}

KS_DECLARE(int) ks_list_delete_range(ks_list_t *restrict l, unsigned int posstart, unsigned int posend) {
	struct ks_list_entry_s *lastvalid, *tmp, *tmp2;
	unsigned int numdel, midposafter, i;
	int movedx;

	if (l->iter_active || posend < posstart || posend >= l->numels) return -1;

	numdel = posend - posstart + 1;
	if (numdel == l->numels) return ks_list_clear(l);

	ks_rwl_write_lock(l->lock);

	tmp = ks_list_findpos(l, posstart);    /* first el to be deleted */
	lastvalid = tmp->prev;              /* last valid element */

	midposafter = (l->numels - 1 - numdel) / 2;

	midposafter = midposafter < posstart ? midposafter : midposafter + numdel;
	movedx = midposafter - (l->numels - 1) / 2;

	if (movedx > 0) { /* move right */
		for (i = 0; i < (unsigned int)movedx; l->mid = l->mid->next, i++);
	}
	else {    /* move left */
		movedx = -movedx;
		for (i = 0; i < (unsigned int)movedx; l->mid = l->mid->prev, i++);
	}

	ks_assert(posstart == 0 || lastvalid != l->head_sentinel);
	i = posstart;
	if (l->attrs.copy_data) {
		/* also free element data */
		for (; i <= posend; i++) {
			tmp2 = tmp;
			tmp = tmp->next;
			if (tmp2->data != NULL) ks_pool_free(l->pool, &tmp2->data);
			if (l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS) {
				l->spareels[l->spareelsnum++] = tmp2;
			}
			else {
				ks_pool_free(l->pool, &tmp2);
			}
		}
	}
	else {
		/* only free containers */
		for (; i <= posend; i++) {
			tmp2 = tmp;
			tmp = tmp->next;
			if (l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS) {
				l->spareels[l->spareelsnum++] = tmp2;
			}
			else {
				ks_pool_free(l->pool, &tmp2);
			}
		}
	}
	ks_assert(i == posend + 1 && (posend != l->numels || tmp == l->tail_sentinel));

	lastvalid->next = tmp;
	tmp->prev = lastvalid;

	l->numels -= posend - posstart + 1;

	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return numdel;
}

KS_DECLARE(int) ks_list_clear(ks_list_t *restrict l) {
	struct ks_list_entry_s *s;
	unsigned int numels;
	int ret = -1;

	ks_rwl_write_lock(l->lock);

	/* will be returned */
	numels = l->numels;

	if (l->iter_active) {
		ret = -1;
		goto done;
	}

	if (l->attrs.copy_data) {        /* also free user data */
									 /* spare a loop conditional with two loops: spareing elems and freeing elems */
		for (s = l->head_sentinel->next; l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS && s != l->tail_sentinel; s = s->next) {
			/* move elements as spares as long as there is room */
			if (s->data != NULL) ks_pool_free(l->pool, &s->data);
			l->spareels[l->spareelsnum++] = s;
		}
		while (s != l->tail_sentinel) {
			/* free the remaining elems */
			if (s->data != NULL) ks_pool_free(l->pool, &s->data);
			s = s->next;
			ks_pool_free(l->pool, &s->prev);
		}
		l->head_sentinel->next = l->tail_sentinel;
		l->tail_sentinel->prev = l->head_sentinel;
	}
	else { /* only free element containers */
		   /* spare a loop conditional with two loops: spareing elems and freeing elems */
		for (s = l->head_sentinel->next; l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS && s != l->tail_sentinel; s = s->next) {
			/* move elements as spares as long as there is room */
			l->spareels[l->spareelsnum++] = s;
		}
		while (s != l->tail_sentinel) {
			/* free the remaining elems */
			s = s->next;
			ks_pool_free(l->pool, &s->prev);
		}
		l->head_sentinel->next = l->tail_sentinel;
		l->tail_sentinel->prev = l->head_sentinel;
	}
	l->numels = 0;
	l->mid = NULL;

done:
	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return numels;
}

KS_DECLARE(unsigned int) ks_list_size(const ks_list_t *restrict l) {
	return l->numels;
}

KS_DECLARE(int) ks_list_empty(const ks_list_t *restrict l) {
	return (l->numels == 0);
}

KS_DECLARE(int) ks_list_locate(const ks_list_t *restrict l, const void *data, ks_bool_t prelocked) {
	struct ks_list_entry_s *el;
	int pos = 0;

	if (!prelocked)	ks_rwl_read_lock(l->lock);

	if (l->attrs.comparator != NULL) {
		/* use comparator */
		for (el = l->head_sentinel->next; el != l->tail_sentinel; el = el->next, pos++) {
			if (l->attrs.comparator(data, el->data) == 0) break;
		}
	}
	else {
		/* compare references */
		for (el = l->head_sentinel->next; el != l->tail_sentinel; el = el->next, pos++) {
			if (el->data == data) break;
		}
	}
	if (!prelocked) ks_rwl_read_unlock(l->lock);
	if (el == l->tail_sentinel) return -1;

	return pos;
}

KS_DECLARE(void *) ks_list_seek(ks_list_t *restrict l, const void *indicator) {
	const struct ks_list_entry_s *iter;
	void *ret = NULL;

	if (l->attrs.seeker == NULL) return NULL;

	ks_rwl_read_lock(l->lock);

	for (iter = l->head_sentinel->next; iter != l->tail_sentinel; iter = iter->next) {
		if (l->attrs.seeker(iter->data, indicator) != 0) {
			ret = iter->data;
			break;
		}
	}

	ks_rwl_read_unlock(l->lock);

	return ret;
}

KS_DECLARE(int) ks_list_contains(const ks_list_t *restrict l, const void *data) {
	return (ks_list_locate(l, data, KS_FALSE) >= 0);
}

KS_DECLARE(int) ks_list_concat(const ks_list_t *l1, const ks_list_t *l2, ks_list_t *restrict dest) {
	struct ks_list_entry_s *el, *srcel;
	unsigned int cnt;
	int err;


	if (l1 == NULL || l2 == NULL || dest == NULL || l1 == dest || l2 == dest)
		return -1;

	//ks_list_init(dest);
	ks_rwl_read_lock(l1->lock);
	ks_rwl_read_lock(l2->lock);
	ks_rwl_write_lock(dest->lock);

	dest->numels = l1->numels + l2->numels;
	if (dest->numels == 0) goto done;

	/* copy list1 */
	srcel = l1->head_sentinel->next;
	el = dest->head_sentinel;
	while (srcel != l1->tail_sentinel) {
		el->next = (struct ks_list_entry_s *)ks_pool_alloc(dest->pool, sizeof(struct ks_list_entry_s));
		el->next->prev = el;
		el = el->next;
		el->data = srcel->data;
		srcel = srcel->next;
	}
	dest->mid = el;     /* approximate position (adjust later) */
						/* copy list 2 */
	srcel = l2->head_sentinel->next;
	while (srcel != l2->tail_sentinel) {
		el->next = (struct ks_list_entry_s *)ks_pool_alloc(dest->pool, sizeof(struct ks_list_entry_s));
		el->next->prev = el;
		el = el->next;
		el->data = srcel->data;
		srcel = srcel->next;
	}
	el->next = dest->tail_sentinel;
	dest->tail_sentinel->prev = el;

	/* fix mid pointer */
	err = l2->numels - l1->numels;
	if ((err + 1) / 2 > 0) {        /* correct pos RIGHT (err-1)/2 moves */
		err = (err + 1) / 2;
		for (cnt = 0; cnt < (unsigned int)err; cnt++) dest->mid = dest->mid->next;
	}
	else if (err / 2 < 0) { /* correct pos LEFT (err/2)-1 moves */
		err = -err / 2;
		for (cnt = 0; cnt < (unsigned int)err; cnt++) dest->mid = dest->mid->prev;
	}

done:

	ks_assert(!(ks_list_repOk(l1) && ks_list_repOk(l2)) || ks_list_repOk(dest));

	ks_rwl_write_unlock(dest->lock);
	ks_rwl_read_unlock(l2->lock);
	ks_rwl_read_unlock(l1->lock);

	return 0;
}

KS_DECLARE(int) ks_list_sort(ks_list_t *restrict l, int versus) {
	if (l->iter_active || l->attrs.comparator == NULL) /* cannot modify list in the middle of an iteration */
		return -1;

	if (l->numels <= 1)
		return 0;

	ks_rwl_write_lock(l->lock);
	ks_list_sort_quicksort(l, versus, 0, l->head_sentinel->next, l->numels - 1, l->tail_sentinel->prev);

	ks_assert(ks_list_repOk(l));

	ks_rwl_write_unlock(l->lock);

	return 0;
}

#ifdef SIMCLIST_WITH_THREADS
struct ks_list_sort_wrappedparams {
	ks_list_t *restrict l;
	int versus;
	unsigned int first, last;
	struct ks_list_entry_s *fel, *lel;
};

static void *ks_list_sort_quicksort_threadwrapper(void *wrapped_params) {
	struct ks_list_sort_wrappedparams *wp = (struct ks_list_sort_wrappedparams *)wrapped_params;
	ks_list_sort_quicksort(wp->l, wp->versus, wp->first, wp->fel, wp->last, wp->lel);
	ks_pool_free(wp->l->pool, &wp);
	pthread_exit(NULL);
	return NULL;
}
#endif

static inline void ks_list_sort_selectionsort(ks_list_t *restrict l, int versus,
	unsigned int first, struct ks_list_entry_s *fel,
	unsigned int last, struct ks_list_entry_s *lel) {
	struct ks_list_entry_s *cursor, *toswap, *firstunsorted;
	void *tmpdata;

	if (last <= first) /* <= 1-element lists are always sorted */
		return;

	for (firstunsorted = fel; firstunsorted != lel; firstunsorted = firstunsorted->next) {
		/* find min or max in the remainder of the list */
		for (toswap = firstunsorted, cursor = firstunsorted->next; cursor != lel->next; cursor = cursor->next)
			if (l->attrs.comparator(toswap->data, cursor->data) * -versus > 0) toswap = cursor;
		if (toswap != firstunsorted) { /* swap firstunsorted with toswap */
			tmpdata = firstunsorted->data;
			firstunsorted->data = toswap->data;
			toswap->data = tmpdata;
		}
	}
}

static void ks_list_sort_quicksort(ks_list_t *restrict l, int versus,
	unsigned int first, struct ks_list_entry_s *fel,
	unsigned int last, struct ks_list_entry_s *lel) {
	unsigned int pivotid;
	unsigned int i;
	register struct ks_list_entry_s *pivot;
	struct ks_list_entry_s *left, *right;
	void *tmpdata;
#ifdef SIMCLIST_WITH_THREADS
	pthread_t tid;
	int traised;
#endif


	if (last <= first)      /* <= 1-element lists are always sorted */
		return;

	if (last - first + 1 <= SIMCLIST_MINQUICKSORTELS) {
		ks_list_sort_selectionsort(l, versus, first, fel, last, lel);
		return;
	}

	/* base of iteration: one element list */
	if (!(last > first)) return;

	pivotid = (get_random() % (last - first + 1));
	/* pivotid = (last - first + 1) / 2; */

	/* find pivot */
	if (pivotid < (last - first + 1) / 2) {
		for (i = 0, pivot = fel; i < pivotid; pivot = pivot->next, i++);
	}
	else {
		for (i = last - first, pivot = lel; i > pivotid; pivot = pivot->prev, i--);
	}

	/* smaller PIVOT bigger */
	left = fel;
	right = lel;
	/* iterate     --- left ---> PIV <--- right --- */
	while (left != pivot && right != pivot) {
		for (; left != pivot && (l->attrs.comparator(left->data, pivot->data) * -versus <= 0); left = left->next);
		/* left points to a smaller element, or to pivot */
		for (; right != pivot && (l->attrs.comparator(right->data, pivot->data) * -versus >= 0); right = right->prev);
		/* right points to a bigger element, or to pivot */
		if (left != pivot && right != pivot) {
			/* swap, then move iterators */
			tmpdata = left->data;
			left->data = right->data;
			right->data = tmpdata;

			left = left->next;
			right = right->prev;
		}
	}

	/* now either left points to pivot (end run), or right */
	if (right == pivot) {    /* left part longer */
		while (left != pivot) {
			if (l->attrs.comparator(left->data, pivot->data) * -versus > 0) {
				tmpdata = left->data;
				left->data = pivot->prev->data;
				pivot->prev->data = pivot->data;
				pivot->data = tmpdata;
				pivot = pivot->prev;
				pivotid--;
				if (pivot == left) break;
			}
			else {
				left = left->next;
			}
		}
	}
	else {                /* right part longer */
		while (right != pivot) {
			if (l->attrs.comparator(right->data, pivot->data) * -versus < 0) {
				/* move current right before pivot */
				tmpdata = right->data;
				right->data = pivot->next->data;
				pivot->next->data = pivot->data;
				pivot->data = tmpdata;
				pivot = pivot->next;
				pivotid++;
				if (pivot == right) break;
			}
			else {
				right = right->prev;
			}
		}
	}

	/* sort sublists A and B :       |---A---| pivot |---B---| */

#ifdef SIMCLIST_WITH_THREADS
	traised = 0;
	if (pivotid > 0) {
		/* prepare wrapped args, then start thread */
		if (l->threadcount < SIMCLIST_MAXTHREADS - 1) {
			struct ks_list_sort_wrappedparams *wp = (struct ks_list_sort_wrappedparams *)ks_pool_alloc(l->pool, sizeof(struct ks_list_sort_wrappedparams));
			l->threadcount++;
			traised = 1;
			wp->l = l;
			wp->versus = versus;
			wp->first = first;
			wp->fel = fel;
			wp->last = first + pivotid - 1;
			wp->lel = pivot->prev;
			if (pthread_create(&tid, NULL, ks_list_sort_quicksort_threadwrapper, wp) != 0) {
				ks_pool_free(l->pool, &wp);
				traised = 0;
				ks_list_sort_quicksort(l, versus, first, fel, first + pivotid - 1, pivot->prev);
			}
		}
		else {
			ks_list_sort_quicksort(l, versus, first, fel, first + pivotid - 1, pivot->prev);
		}
	}
	if (first + pivotid < last) ks_list_sort_quicksort(l, versus, first + pivotid + 1, pivot->next, last, lel);
	if (traised) {
		pthread_join(tid, (void **)NULL);
		l->threadcount--;
	}
#else
	if (pivotid > 0) ks_list_sort_quicksort(l, versus, first, fel, first + pivotid - 1, pivot->prev);
	if (first + pivotid < last) ks_list_sort_quicksort(l, versus, first + pivotid + 1, pivot->next, last, lel);
#endif
}

KS_DECLARE(int) ks_list_iterator_start(ks_list_t *restrict l) {
	if (l->iter_active) return 0;
	ks_rwl_write_lock(l->lock);
	l->iter_pos = 0;
	l->iter_active = 1;
	l->iter_curentry = l->head_sentinel->next;
	ks_rwl_write_unlock(l->lock);
	return 1;
}

KS_DECLARE(void *) ks_list_iterator_next(ks_list_t *restrict l) {
	void *toret;

	if (!l->iter_active) return NULL;

	ks_rwl_write_lock(l->lock);
	toret = l->iter_curentry->data;
	l->iter_curentry = l->iter_curentry->next;
	l->iter_pos++;
	ks_rwl_write_unlock(l->lock);

	return toret;
}

KS_DECLARE(int) ks_list_iterator_hasnext(const ks_list_t *restrict l) {
	int ret = 0;
	if (!l->iter_active) return 0;
	ks_rwl_read_lock(l->lock);
	ret = (l->iter_pos < l->numels);
	ks_rwl_read_unlock(l->lock);
	return ret;
}

KS_DECLARE(int) ks_list_iterator_stop(ks_list_t *restrict l) {
	if (!l->iter_active) return 0;
	ks_rwl_write_lock(l->lock);
	l->iter_pos = 0;
	l->iter_active = 0;
	ks_rwl_write_unlock(l->lock);
	return 1;
}

KS_DECLARE(int) ks_list_hash(const ks_list_t *restrict l, ks_list_hash_t *restrict hash) {
	struct ks_list_entry_s *x;
	ks_list_hash_t tmphash;
	int ret = 0;

	ks_assert(hash != NULL);

	ks_rwl_read_lock(l->lock);

	tmphash = l->numels * 2 + 100;
	if (l->attrs.hasher == NULL) {
#ifdef SIMCLIST_ALLOW_LOCATIONBASED_HASHES
		/* ENABLE WITH CARE !! */
		#warning "Memlocation-based hash is consistent only for testing modification in the same program run."
			int i;

		/* only use element references */
		for (x = l->head_sentinel->next; x != l->tail_sentinel; x = x->next) {
			for (i = 0; i < sizeof(x->data); i++) {
				tmphash += (tmphash ^ (uintptr_t)x->data);
			}
			tmphash += tmphash % l->numels;
		}
#else
		ret = -1;
#endif
	}
	else {
		/* hash each element with the user-given function */
		for (x = l->head_sentinel->next; x != l->tail_sentinel; x = x->next) {
			tmphash += tmphash ^ l->attrs.hasher(x->data);
			tmphash += tmphash % l->numels;
		}
	}

	ks_rwl_read_unlock(l->lock);

	*hash = tmphash;

	return ret;
}

#ifndef SIMCLIST_NO_DUMPRESTORE
int ks_list_dump_getinfo_filedescriptor(int fd, ks_list_dump_info_t *restrict info) {
	int32_t terminator_head, terminator_tail;
	uint32_t elemlen;
	off_t hop;


	/* version */
	READ_ERRCHECK(fd, &info->version, sizeof(info->version));
	info->version = ntohs(info->version);
	if (info->version > SIMCLIST_DUMPFORMAT_VERSION) {
		errno = EILSEQ;
		return -1;
	}

	/* timestamp.tv_sec and timestamp.tv_usec */
	READ_ERRCHECK(fd, &info->timestamp.tv_sec, sizeof(info->timestamp.tv_sec));
	info->timestamp.tv_sec = ntohl(info->timestamp.tv_sec);
	READ_ERRCHECK(fd, &info->timestamp.tv_usec, sizeof(info->timestamp.tv_usec));
	info->timestamp.tv_usec = ntohl(info->timestamp.tv_usec);

	/* list terminator (to check thereafter) */
	READ_ERRCHECK(fd, &terminator_head, sizeof(terminator_head));
	terminator_head = ntohl(terminator_head);

	/* list size */
	READ_ERRCHECK(fd, &info->list_size, sizeof(info->list_size));
	info->list_size = ntohl(info->list_size);

	/* number of elements */
	READ_ERRCHECK(fd, &info->list_numels, sizeof(info->list_numels));
	info->list_numels = ntohl(info->list_numels);

	/* length of each element (for checking for consistency) */
	READ_ERRCHECK(fd, &elemlen, sizeof(elemlen));
	elemlen = ntohl(elemlen);

	/* list hash */
	READ_ERRCHECK(fd, &info->list_hash, sizeof(info->list_hash));
	info->list_hash = ntohl(info->list_hash);

	/* check consistency */
	if (elemlen > 0) {
		/* constant length, hop by size only */
		hop = info->list_size;
	}
	else {
		/* non-constant length, hop by size + all element length blocks */
		hop = info->list_size + elemlen*info->list_numels;
	}
	if (lseek(fd, hop, SEEK_CUR) == -1) {
		return -1;
	}

	/* read the trailing value and compare with terminator_head */
	READ_ERRCHECK(fd, &terminator_tail, sizeof(terminator_tail));
	terminator_tail = ntohl(terminator_tail);

	if (terminator_head == terminator_tail)
		info->consistent = 1;
	else
		info->consistent = 0;

	return 0;
}

int ks_list_dump_getinfo_file(const char *restrict filename, ks_list_dump_info_t *restrict info) {
	int fd, ret;

	fd = open(filename, O_RDONLY, 0);
	if (fd < 0) return -1;

	ret = ks_list_dump_getinfo_filedescriptor(fd, info);
	close(fd);

	return ret;
}

int ks_list_dump_filedescriptor(const ks_list_t *restrict l, int fd, ks_size_t *restrict len) {
	struct ks_list_entry_s *x;
	void *ser_buf;
	uint32_t bufsize;
	struct timeval timeofday;
	struct ks_list_dump_header_s header;

	if (l->attrs.meter == NULL && l->attrs.serializer == NULL) {
		errno = ENOTTY;
		return -1;
	}

	/****       DUMP FORMAT      ****

	[ ver   timestamp   |  totlen   numels  elemlen     hash    |   DATA ]

	where DATA can be:
	@ for constant-size list (element size is constant; elemlen > 0)
	[ elem    elem    ...    elem ]
	@ for other lists (element size dictated by element_meter each time; elemlen <= 0)
	[ size elem     size elem       ...     size elem ]

	all integers are encoded in NETWORK BYTE FORMAT
	*****/


	/* prepare HEADER */
	/* version */
	header.ver = htons(SIMCLIST_DUMPFORMAT_VERSION);

	/* timestamp */
	gettimeofday(&timeofday, NULL);
	header.timestamp_sec = htonl(timeofday.tv_sec);
	header.timestamp_usec = htonl(timeofday.tv_usec);

	header.rndterm = htonl((int32_t)get_random());

	/* total list size is postprocessed afterwards */

	/* number of elements */
	header.numels = htonl(l->numels);

	/* include an hash, if possible */
	if (l->attrs.hasher != NULL) {
		if (htonl(ks_list_hash(l, &header.listhash)) != 0) {
			/* could not compute list hash! */
			return -1;
		}
	}
	else {
		header.listhash = htonl(0);
	}

	header.totlistlen = header.elemlen = 0;

	/* leave room for the header at the beginning of the file */
	if (lseek(fd, SIMCLIST_DUMPFORMAT_HEADERLEN, SEEK_SET) < 0) {
		/* errno set by lseek() */
		return -1;
	}

	/* write CONTENT */
	if (l->numels > 0) {
		/* SPECULATE that the list has constant element size */

		if (l->attrs.serializer != NULL) {  /* user user-specified serializer */
											/* get preliminary length of serialized element in header.elemlen */
			ser_buf = l->attrs.serializer(l->head_sentinel->next->data, &header.elemlen);
			ks_pool_free(l->pool, &ser_buf);
			/* request custom serialization of each element */
			for (x = l->head_sentinel->next; x != l->tail_sentinel; x = x->next) {
				ser_buf = l->attrs.serializer(x->data, &bufsize);
				header.totlistlen += bufsize;
				if (header.elemlen != 0) {      /* continue on speculation */
					if (header.elemlen != bufsize) {
						ks_pool_free(l->pool, &ser_buf);
						/* constant element length speculation broken! */
						header.elemlen = 0;
						header.totlistlen = 0;
						x = l->head_sentinel;
						if (lseek(fd, SIMCLIST_DUMPFORMAT_HEADERLEN, SEEK_SET) < 0) {
							/* errno set by lseek() */
							return -1;
						}
						/* restart from the beginning */
						continue;
					}
					/* speculation confirmed */
					WRITE_ERRCHECK(fd, ser_buf, bufsize);
				}
				else {                        /* speculation found broken */
					WRITE_ERRCHECK(fd, &bufsize, sizeof(ks_size_t));
					WRITE_ERRCHECK(fd, ser_buf, bufsize);
				}
				ks_pool_free(l->pool, &ser_buf);
			}
		}
		else if (l->attrs.meter != NULL) {
			header.elemlen = (uint32_t)l->attrs.meter(l->head_sentinel->next->data);

			/* serialize the element straight from its data */
			for (x = l->head_sentinel->next; x != l->tail_sentinel; x = x->next) {
				bufsize = l->attrs.meter(x->data);
				header.totlistlen += bufsize;
				if (header.elemlen != 0) {
					if (header.elemlen != bufsize) {
						/* constant element length speculation broken! */
						header.elemlen = 0;
						header.totlistlen = 0;
						x = l->head_sentinel;
						/* restart from the beginning */
						continue;
					}
					WRITE_ERRCHECK(fd, x->data, bufsize);
				}
				else {
					WRITE_ERRCHECK(fd, &bufsize, sizeof(ks_size_t));
					WRITE_ERRCHECK(fd, x->data, bufsize);
				}
			}
		}
		/* adjust endianness */
		header.elemlen = htonl(header.elemlen);
		header.totlistlen = htonl(header.totlistlen);
	}

	/* write random terminator */
	WRITE_ERRCHECK(fd, &header.rndterm, sizeof(header.rndterm));        /* list terminator */


																		/* write header */
	lseek(fd, 0, SEEK_SET);

	WRITE_ERRCHECK(fd, &header.ver, sizeof(header.ver));                        /* version */
	WRITE_ERRCHECK(fd, &header.timestamp_sec, sizeof(header.timestamp_sec));    /* timestamp seconds */
	WRITE_ERRCHECK(fd, &header.timestamp_usec, sizeof(header.timestamp_usec));  /* timestamp microseconds */
	WRITE_ERRCHECK(fd, &header.rndterm, sizeof(header.rndterm));                /* random terminator */

	WRITE_ERRCHECK(fd, &header.totlistlen, sizeof(header.totlistlen));          /* total length of elements */
	WRITE_ERRCHECK(fd, &header.numels, sizeof(header.numels));                  /* number of elements */
	WRITE_ERRCHECK(fd, &header.elemlen, sizeof(header.elemlen));                /* size of each element, or 0 for independent */
	WRITE_ERRCHECK(fd, &header.listhash, sizeof(header.listhash));              /* list hash, or 0 for "ignore" */


																				/* possibly store total written length in "len" */
	if (len != NULL) {
		*len = sizeof(header) + ntohl(header.totlistlen);
	}

	return 0;
}

int ks_list_restore_filedescriptor(ks_list_t *restrict l, int fd, ks_size_t *restrict len) {
	struct ks_list_dump_header_s header;
	unsigned long cnt;
	void *buf;
	uint32_t elsize, totreadlen, totmemorylen;

	memset(&header, 0, sizeof(header));

	/* read header */

	/* version */
	READ_ERRCHECK(fd, &header.ver, sizeof(header.ver));
	header.ver = ntohs(header.ver);
	if (header.ver != SIMCLIST_DUMPFORMAT_VERSION) {
		errno = EILSEQ;
		return -1;
	}

	/* timestamp */
	READ_ERRCHECK(fd, &header.timestamp_sec, sizeof(header.timestamp_sec));
	header.timestamp_sec = ntohl(header.timestamp_sec);
	READ_ERRCHECK(fd, &header.timestamp_usec, sizeof(header.timestamp_usec));
	header.timestamp_usec = ntohl(header.timestamp_usec);

	/* list terminator */
	READ_ERRCHECK(fd, &header.rndterm, sizeof(header.rndterm));

	header.rndterm = ntohl(header.rndterm);

	/* total list size */
	READ_ERRCHECK(fd, &header.totlistlen, sizeof(header.totlistlen));
	header.totlistlen = ntohl(header.totlistlen);

	/* number of elements */
	READ_ERRCHECK(fd, &header.numels, sizeof(header.numels));
	header.numels = ntohl(header.numels);

	/* length of every element, or '0' = variable */
	READ_ERRCHECK(fd, &header.elemlen, sizeof(header.elemlen));
	header.elemlen = ntohl(header.elemlen);

	/* list hash, or 0 = 'ignore' */
	READ_ERRCHECK(fd, &header.listhash, sizeof(header.listhash));
	header.listhash = ntohl(header.listhash);


	/* read content */
	totreadlen = totmemorylen = 0;
	if (header.elemlen > 0) {
		/* elements have constant size = header.elemlen */
		if (l->attrs.unserializer != NULL) {
			/* use unserializer */
			buf = ks_pool_alloc(l->pool, header.elemlen);
			for (cnt = 0; cnt < header.numels; cnt++) {
				READ_ERRCHECK(fd, buf, header.elemlen);
				ks_list_append(l, l->attrs.unserializer(buf, &elsize));
				totmemorylen += elsize;
			}
		}
		else {
			/* copy verbatim into memory */
			for (cnt = 0; cnt < header.numels; cnt++) {
				buf = ks_pool_alloc(l->pool, header.elemlen);
				READ_ERRCHECK(fd, buf, header.elemlen);
				ks_list_append(l, buf);
			}
			totmemorylen = header.numels * header.elemlen;
		}
		totreadlen = header.numels * header.elemlen;
	}
	else {
		/* elements have variable size. Each element is preceded by its size */
		if (l->attrs.unserializer != NULL) {
			/* use unserializer */
			for (cnt = 0; cnt < header.numels; cnt++) {
				READ_ERRCHECK(fd, &elsize, sizeof(elsize));
				buf = ks_pool_alloc(l->pool, (ks_size_t)elsize);
				READ_ERRCHECK(fd, buf, elsize);
				totreadlen += elsize;
				ks_list_append(l, l->attrs.unserializer(buf, &elsize));
				totmemorylen += elsize;
			}
		}
		else {
			/* copy verbatim into memory */
			for (cnt = 0; cnt < header.numels; cnt++) {
				READ_ERRCHECK(fd, &elsize, sizeof(elsize));
				buf = ks_pool_alloc(l->pool, elsize);
				READ_ERRCHECK(fd, buf, elsize);
				totreadlen += elsize;
				ks_list_append(l, buf);
			}
			totmemorylen = totreadlen;
		}
	}

	READ_ERRCHECK(fd, &elsize, sizeof(elsize));  /* read list terminator */
	elsize = ntohl(elsize);

	/* possibly verify the list consistency */
	/* wrt hash */
	/* don't do that
	if (header.listhash != 0 && header.listhash != ks_list_hash(l)) {
	errno = ECANCELED;
	return -1;
	}
	*/

	/* wrt header */
	if (totreadlen != header.totlistlen && (int32_t)elsize == header.rndterm) {
		errno = EPROTO;
		return -1;
	}

	/* wrt file */
	if (lseek(fd, 0, SEEK_CUR) != lseek(fd, 0, SEEK_END)) {
		errno = EPROTO;
		return -1;
	}

	if (len != NULL) {
		*len = totmemorylen;
	}

	return 0;
}

int ks_list_dump_file(const ks_list_t *restrict l, const char *restrict filename, ks_size_t *restrict len) {
	int fd, oflag, mode;

#ifndef _WIN32
	oflag = O_RDWR | O_CREAT | O_TRUNC;
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
	oflag = _O_RDWR | _O_CREAT | _O_TRUNC;
	mode = _S_IRUSR | _S_IWUSR | _S_IRGRP | _S_IROTH;
#endif
	fd = open(filename, oflag, mode);
	if (fd < 0) return -1;

	ks_rwl_write_lock(l->lock);
	ks_list_dump_filedescriptor(l, fd, len);
	ks_rwl_write_unlock(l->lock);
	close(fd);

	return 0;
}

int ks_list_restore_file(ks_list_t *restrict l, const char *restrict filename, ks_size_t *restrict len) {
	int fd;

	fd = open(filename, O_RDONLY, 0);
	if (fd < 0) return -1;

	ks_rwl_write_lock(l->lock);
	ks_list_restore_filedescriptor(l, fd, len);
	ks_rwl_write_unlock(l->lock);
	close(fd);

	return 0;
}
#endif /* ifndef SIMCLIST_NO_DUMPRESTORE */


static int ks_list_drop_elem(ks_list_t *restrict l, struct ks_list_entry_s *tmp, unsigned int pos) {
	if (tmp == NULL) return -1;

	/* fix mid pointer. This is wrt the PRE situation */
	if (l->numels % 2) {    /* now odd */
							/* sort out the base case by hand */
		if (l->numels == 1) l->mid = NULL;
		else if (pos >= l->numels / 2) l->mid = l->mid->prev;
	}
	else {                /* now even */
		if (pos < l->numels / 2) l->mid = l->mid->next;
	}

	tmp->prev->next = tmp->next;
	tmp->next->prev = tmp->prev;

	/* free what's to be freed */
	if (l->attrs.copy_data && tmp->data != NULL)
		ks_pool_free(l->pool, &tmp->data);

	if (l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS) {
		l->spareels[l->spareelsnum++] = tmp;
	}
	else {
		ks_pool_free(l->pool, &tmp);
	}

	return 0;
}

/* ready-made comparators and meters */
#define SIMCLIST_NUMBER_COMPARATOR(type)     int ks_list_comparator_##type(const void *a, const void *b) { return( *(type *)a < *(type *)b) - (*(type *)a > *(type *)b); }

SIMCLIST_NUMBER_COMPARATOR(int8_t)
SIMCLIST_NUMBER_COMPARATOR(int16_t)
SIMCLIST_NUMBER_COMPARATOR(int32_t)
SIMCLIST_NUMBER_COMPARATOR(int64_t)

SIMCLIST_NUMBER_COMPARATOR(uint8_t)
SIMCLIST_NUMBER_COMPARATOR(uint16_t)
SIMCLIST_NUMBER_COMPARATOR(uint32_t)
SIMCLIST_NUMBER_COMPARATOR(uint64_t)

SIMCLIST_NUMBER_COMPARATOR(float)
SIMCLIST_NUMBER_COMPARATOR(double)

int ks_list_comparator_string(const void *a, const void *b) { return strcmp((const char *)b, (const char *)a); }

/* ready-made metric functions */
#define SIMCLIST_METER(type)        ks_size_t ks_list_meter_##type(const void *el) { if (el) { /* kill compiler whinge */ } return sizeof(type); }

SIMCLIST_METER(int8_t)
SIMCLIST_METER(int16_t)
SIMCLIST_METER(int32_t)
SIMCLIST_METER(int64_t)

SIMCLIST_METER(uint8_t)
SIMCLIST_METER(uint16_t)
SIMCLIST_METER(uint32_t)
SIMCLIST_METER(uint64_t)

SIMCLIST_METER(float)
SIMCLIST_METER(double)

ks_size_t ks_list_meter_string(const void *el) { return strlen((const char *)el) + 1; }

/* ready-made hashing functions */
#define SIMCLIST_HASHCOMPUTER(type)    ks_list_hash_t ks_list_hashcomputer_##type(const void *el) { return (ks_list_hash_t)(*(type *)el); }

SIMCLIST_HASHCOMPUTER(int8_t)
SIMCLIST_HASHCOMPUTER(int16_t)
SIMCLIST_HASHCOMPUTER(int32_t)
SIMCLIST_HASHCOMPUTER(int64_t)

SIMCLIST_HASHCOMPUTER(uint8_t)
SIMCLIST_HASHCOMPUTER(uint16_t)
SIMCLIST_HASHCOMPUTER(uint32_t)
SIMCLIST_HASHCOMPUTER(uint64_t)

SIMCLIST_HASHCOMPUTER(float)
SIMCLIST_HASHCOMPUTER(double)

ks_list_hash_t ks_list_hashcomputer_string(const void *el) {
	ks_size_t l;
	ks_list_hash_t hash = 123;
	const char *str = (const char *)el;
	char plus;

	for (l = 0; str[l] != '\0'; l++) {
		if (l) plus = (char)(hash ^ str[l]);
		else plus = (char)(hash ^ (str[l] - str[0]));
		hash += (plus << (CHAR_BIT * (l % sizeof(ks_list_hash_t))));
	}

	return hash;
}


static int ks_list_repOk(const ks_list_t *restrict l) {
	int ok, i;
	struct ks_list_entry_s *s;

	ok = (l != NULL) && (
		/* head/tail checks */
		(l->head_sentinel != NULL && l->tail_sentinel != NULL) &&
		(l->head_sentinel != l->tail_sentinel) && (l->head_sentinel->prev == NULL && l->tail_sentinel->next == NULL) &&
		/* empty list */
		(l->numels > 0 || (l->mid == NULL && l->head_sentinel->next == l->tail_sentinel && l->tail_sentinel->prev == l->head_sentinel)) &&
		/* spare elements checks */
		l->spareelsnum <= SIMCLIST_MAX_SPARE_ELEMS
		);

	if (!ok) return 0;

	if (l->numels >= 1) {
		/* correct referencing */
		for (i = -1, s = l->head_sentinel; i < (int)(l->numels - 1) / 2 && s->next != NULL; i++, s = s->next) {
			if (s->next->prev != s) break;
		}
		ok = (i == (int)(l->numels - 1) / 2 && l->mid == s);
		if (!ok) return 0;
		for (; s->next != NULL; i++, s = s->next) {
			if (s->next->prev != s) break;
		}
		ok = (i == (int)l->numels && s == l->tail_sentinel);
	}

	return ok;
}

static int ks_list_attrOk(const ks_list_t *restrict l) {
	int ok;

	ok = (l->attrs.copy_data == 0 || l->attrs.meter != NULL);
	return ok;
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
