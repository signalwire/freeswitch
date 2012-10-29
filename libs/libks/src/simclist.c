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
#define NDEBUG
#endif

#include <assert.h>


#include <sys/stat.h>       /* for open()'s access modes S_IRUSR etc */
#include <limits.h>

#if defined(_MSC_VER) || defined(__MINGW32__)
/* provide gettimeofday() missing in Windows */
int gettimeofday(struct timeval *tp, void *tzp) {
    DWORD t;

    /* XSI says: "If tzp is not a null pointer, the behavior is unspecified" */
    assert(tzp == NULL);

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

#define SIMCLIST_DUMPFORMAT_HEADERLEN   30  /* length of the header */

/* header for a list dump */
struct list_dump_header_s {
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
static int list_drop_elem(list_t *restrict l, struct list_entry_s *tmp, unsigned int pos);

/* set default values for initialized lists */
static int list_attributes_setdefaults(list_t *restrict l);

#ifndef NDEBUG
/* check whether the list internal REPresentation is valid -- Costs O(n) */
static int list_repOk(const list_t *restrict l);

/* check whether the list attribute set is valid -- Costs O(1) */
static int list_attrOk(const list_t *restrict l);
#endif

/* do not inline, this is recursive */
static void list_sort_quicksort(list_t *restrict l, int versus,
        unsigned int first, struct list_entry_s *fel,
        unsigned int last, struct list_entry_s *lel);

static inline void list_sort_selectionsort(list_t *restrict l, int versus,
        unsigned int first, struct list_entry_s *fel,
        unsigned int last, struct list_entry_s *lel);

static void *list_get_minmax(const list_t *restrict l, int versus);

static inline struct list_entry_s *list_findpos(const list_t *restrict l, int posstart);

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


/* list initialization */
int list_init(list_t *restrict l) {
    if (l == NULL) return -1;

    seed_random();

    l->numels = 0;

    /* head/tail sentinels and mid pointer */
    l->head_sentinel = (struct list_entry_s *)malloc(sizeof(struct list_entry_s));
    l->tail_sentinel = (struct list_entry_s *)malloc(sizeof(struct list_entry_s));
    l->head_sentinel->next = l->tail_sentinel;
    l->tail_sentinel->prev = l->head_sentinel;
    l->head_sentinel->prev = l->tail_sentinel->next = l->mid = NULL;
    l->head_sentinel->data = l->tail_sentinel->data = NULL;

    /* iteration attributes */
    l->iter_active = 0;
    l->iter_pos = 0;
    l->iter_curentry = NULL;

    /* free-list attributes */
    l->spareels = (struct list_entry_s **)malloc(SIMCLIST_MAX_SPARE_ELEMS * sizeof(struct list_entry_s *));
    l->spareelsnum = 0;

#ifdef SIMCLIST_WITH_THREADS
    l->threadcount = 0;
#endif

    list_attributes_setdefaults(l);

    assert(list_repOk(l));
    assert(list_attrOk(l));

    return 0;
}

void list_destroy(list_t *restrict l) {
    unsigned int i;

    list_clear(l);
    for (i = 0; i < l->spareelsnum; i++) {
        free(l->spareels[i]);
    }
    free(l->spareels);
    free(l->head_sentinel);
    free(l->tail_sentinel);
}

int list_attributes_setdefaults(list_t *restrict l) {
    l->attrs.comparator = NULL;
    l->attrs.seeker = NULL;

    /* also free() element data when removing and element from the list */
    l->attrs.meter = NULL;
    l->attrs.copy_data = 0;

    l->attrs.hasher = NULL;

    /* serializer/unserializer */
    l->attrs.serializer = NULL;
    l->attrs.unserializer = NULL;

    assert(list_attrOk(l));

    return 0;
}

/* setting list properties */
int list_attributes_comparator(list_t *restrict l, element_comparator comparator_fun) {
    if (l == NULL) return -1;

    l->attrs.comparator = comparator_fun;

    assert(list_attrOk(l));

    return 0;
}

int list_attributes_seeker(list_t *restrict l, element_seeker seeker_fun) {
    if (l == NULL) return -1;

    l->attrs.seeker = seeker_fun;
    assert(list_attrOk(l));

    return 0;
}

int list_attributes_copy(list_t *restrict l, element_meter metric_fun, int copy_data) {
    if (l == NULL || (metric_fun == NULL && copy_data != 0)) return -1;

    l->attrs.meter = metric_fun;
    l->attrs.copy_data = copy_data;

    assert(list_attrOk(l));

    return 0;
}

int list_attributes_hash_computer(list_t *restrict l, element_hash_computer hash_computer_fun) {
    if (l == NULL) return -1;

    l->attrs.hasher = hash_computer_fun;
    assert(list_attrOk(l));
    return 0;
}

int list_attributes_serializer(list_t *restrict l, element_serializer serializer_fun) {
    if (l == NULL) return -1;

    l->attrs.serializer = serializer_fun;
    assert(list_attrOk(l));
    return 0;
}

int list_attributes_unserializer(list_t *restrict l, element_unserializer unserializer_fun) {
    if (l == NULL) return -1;

    l->attrs.unserializer = unserializer_fun;
    assert(list_attrOk(l));
    return 0;
}

int list_append(list_t *restrict l, const void *data) {
    return list_insert_at(l, data, l->numels);
}

int list_prepend(list_t *restrict l, const void *data) {
    return list_insert_at(l, data, 0);
}

void *list_fetch(list_t *restrict l) {
    return list_extract_at(l, 0);
}

void *list_get_at(const list_t *restrict l, unsigned int pos) {
    struct list_entry_s *tmp;

    tmp = list_findpos(l, pos);

    return (tmp != NULL ? tmp->data : NULL);
}

void *list_get_max(const list_t *restrict l) {
    return list_get_minmax(l, +1);
}

void *list_get_min(const list_t *restrict l) {
    return list_get_minmax(l, -1);
}

/* REQUIRES {list->numels >= 1}
 * return the min (versus < 0) or max value (v > 0) in l */
static void *list_get_minmax(const list_t *restrict l, int versus) {
    void *curminmax;
    struct list_entry_s *s;

    if (l->attrs.comparator == NULL || l->numels == 0)
        return NULL;

    curminmax = l->head_sentinel->next->data;
    for (s = l->head_sentinel->next->next; s != l->tail_sentinel; s = s->next) {
        if (l->attrs.comparator(curminmax, s->data) * versus > 0)
            curminmax = s->data;
    }

    return curminmax;
}

/* set tmp to point to element at index posstart in l */
static inline struct list_entry_s *list_findpos(const list_t *restrict l, int posstart) {
    struct list_entry_s *ptr;
    float x;
    int i;

    /* accept 1 slot overflow for fetching head and tail sentinels */
    if (posstart < -1 || posstart > (int)l->numels) return NULL;

    x = (float)(posstart+1) / l->numels;
    if (x <= 0.25) {
        /* first quarter: get to posstart from head */
        for (i = -1, ptr = l->head_sentinel; i < posstart; ptr = ptr->next, i++);
    } else if (x < 0.5) {
        /* second quarter: get to posstart from mid */
        for (i = (l->numels-1)/2, ptr = l->mid; i > posstart; ptr = ptr->prev, i--);
    } else if (x <= 0.75) {
        /* third quarter: get to posstart from mid */
        for (i = (l->numels-1)/2, ptr = l->mid; i < posstart; ptr = ptr->next, i++);
    } else {
        /* fourth quarter: get to posstart from tail */
        for (i = l->numels, ptr = l->tail_sentinel; i > posstart; ptr = ptr->prev, i--);
    }

    return ptr;
}

void *list_extract_at(list_t *restrict l, unsigned int pos) {
    struct list_entry_s *tmp;
    void *data;

    if (l->iter_active || pos >= l->numels) return NULL;

    tmp = list_findpos(l, pos);
    data = tmp->data;

    tmp->data = NULL;   /* save data from list_drop_elem() free() */
    list_drop_elem(l, tmp, pos);
    l->numels--;

    assert(list_repOk(l));

    return data;
}

int list_insert_at(list_t *restrict l, const void *data, unsigned int pos) {
    struct list_entry_s *lent, *succ, *prec;

    if (l->iter_active || pos > l->numels) return -1;

    /* this code optimizes malloc() with a free-list */
    if (l->spareelsnum > 0) {
        lent = l->spareels[l->spareelsnum-1];
        l->spareelsnum--;
    } else {
        lent = (struct list_entry_s *)malloc(sizeof(struct list_entry_s));
        if (lent == NULL)
            return -1;
    }

    if (l->attrs.copy_data) {
        /* make room for user' data (has to be copied) */
        size_t datalen = l->attrs.meter(data);
        lent->data = (struct list_entry_s *)malloc(datalen);
        memcpy(lent->data, data, datalen);
    } else {
        lent->data = (void*)data;
    }

    /* actually append element */
    prec = list_findpos(l, pos-1);
    succ = prec->next;

    prec->next = lent;
    lent->prev = prec;
    lent->next = succ;
    succ->prev = lent;

    l->numels++;

    /* fix mid pointer */
    if (l->numels == 1) { /* first element, set pointer */
        l->mid = lent;
    } else if (l->numels % 2) {    /* now odd */
        if (pos >= (l->numels-1)/2) l->mid = l->mid->next;
    } else {                /* now even */
        if (pos <= (l->numels-1)/2) l->mid = l->mid->prev;
    }

    assert(list_repOk(l));

    return 1;
}

int list_delete(list_t *restrict l, const void *data) {
	int pos, r;

	pos = list_locate(l, data);
	if (pos < 0)
		return -1;

	r = list_delete_at(l, pos);
	if (r < 0)
		return -1;

    assert(list_repOk(l));

	return 0;
}

int list_delete_at(list_t *restrict l, unsigned int pos) {
    struct list_entry_s *delendo;


    if (l->iter_active || pos >= l->numels) return -1;

    delendo = list_findpos(l, pos);

    list_drop_elem(l, delendo, pos);

    l->numels--;


    assert(list_repOk(l));

    return  0;
}

int list_delete_range(list_t *restrict l, unsigned int posstart, unsigned int posend) {
    struct list_entry_s *lastvalid, *tmp, *tmp2;
    unsigned int numdel, midposafter, i;
    int movedx;

    if (l->iter_active || posend < posstart || posend >= l->numels) return -1;

    numdel = posend - posstart + 1;
    if (numdel == l->numels) return list_clear(l);

    tmp = list_findpos(l, posstart);    /* first el to be deleted */
    lastvalid = tmp->prev;              /* last valid element */

    midposafter = (l->numels-1-numdel)/2;

    midposafter = midposafter < posstart ? midposafter : midposafter+numdel;
    movedx = midposafter - (l->numels-1)/2;

    if (movedx > 0) { /* move right */
        for (i = 0; i < (unsigned int)movedx; l->mid = l->mid->next, i++);
    } else {    /* move left */
        movedx = -movedx;
        for (i = 0; i < (unsigned int)movedx; l->mid = l->mid->prev, i++);
    }

    assert(posstart == 0 || lastvalid != l->head_sentinel);
    i = posstart;
    if (l->attrs.copy_data) {
        /* also free element data */
        for (; i <= posend; i++) {
            tmp2 = tmp;
            tmp = tmp->next;
            if (tmp2->data != NULL) free(tmp2->data);
            if (l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS) {
                l->spareels[l->spareelsnum++] = tmp2;
            } else {
                free(tmp2);
            }
        }
    } else {
        /* only free containers */
        for (; i <= posend; i++) {
            tmp2 = tmp;
            tmp = tmp->next;
            if (l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS) {
                l->spareels[l->spareelsnum++] = tmp2;
            } else {
                free(tmp2);
            }
        }
    }
    assert(i == posend+1 && (posend != l->numels || tmp == l->tail_sentinel));

    lastvalid->next = tmp;
    tmp->prev = lastvalid;

    l->numels -= posend - posstart + 1;

    assert(list_repOk(l));

    return numdel;
}

int list_clear(list_t *restrict l) {
    struct list_entry_s *s;
    unsigned int numels;

    /* will be returned */
    numels = l->numels;

    if (l->iter_active) return -1;

    if (l->attrs.copy_data) {        /* also free user data */
        /* spare a loop conditional with two loops: spareing elems and freeing elems */
        for (s = l->head_sentinel->next; l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS && s != l->tail_sentinel; s = s->next) {
            /* move elements as spares as long as there is room */
            if (s->data != NULL) free(s->data);
            l->spareels[l->spareelsnum++] = s;
        }
        while (s != l->tail_sentinel) {
            /* free the remaining elems */
            if (s->data != NULL) free(s->data);
            s = s->next;
            free(s->prev);
        }
        l->head_sentinel->next = l->tail_sentinel;
        l->tail_sentinel->prev = l->head_sentinel;
    } else { /* only free element containers */
        /* spare a loop conditional with two loops: spareing elems and freeing elems */
        for (s = l->head_sentinel->next; l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS && s != l->tail_sentinel; s = s->next) {
            /* move elements as spares as long as there is room */
            l->spareels[l->spareelsnum++] = s;
        }
        while (s != l->tail_sentinel) {
            /* free the remaining elems */
            s = s->next;
            free(s->prev);
        }
        l->head_sentinel->next = l->tail_sentinel;
        l->tail_sentinel->prev = l->head_sentinel;
    }
    l->numels = 0;
    l->mid = NULL;

    assert(list_repOk(l));

    return numels;
}

unsigned int list_size(const list_t *restrict l) {
    return l->numels;
}

int list_empty(const list_t *restrict l) {
    return (l->numels == 0);
}

int list_locate(const list_t *restrict l, const void *data) {
    struct list_entry_s *el;
    int pos = 0;

    if (l->attrs.comparator != NULL) {
        /* use comparator */
        for (el = l->head_sentinel->next; el != l->tail_sentinel; el = el->next, pos++) {
            if (l->attrs.comparator(data, el->data) == 0) break;
        }
    } else {
        /* compare references */
        for (el = l->head_sentinel->next; el != l->tail_sentinel; el = el->next, pos++) {
            if (el->data == data) break;
        }
    }
    if (el == l->tail_sentinel) return -1;

    return pos;
}

void *list_seek(list_t *restrict l, const void *indicator) {
    const struct list_entry_s *iter;

    if (l->attrs.seeker == NULL) return NULL;

    for (iter = l->head_sentinel->next; iter != l->tail_sentinel; iter = iter->next) {
        if (l->attrs.seeker(iter->data, indicator) != 0) return iter->data;
    }

    return NULL;
}

int list_contains(const list_t *restrict l, const void *data) {
    return (list_locate(l, data) >= 0);
}

int list_concat(const list_t *l1, const list_t *l2, list_t *restrict dest) {
    struct list_entry_s *el, *srcel;
    unsigned int cnt;
    int err;


    if (l1 == NULL || l2 == NULL || dest == NULL || l1 == dest || l2 == dest)
        return -1;

    list_init(dest);

    dest->numels = l1->numels + l2->numels;
    if (dest->numels == 0)
        return 0;

    /* copy list1 */
    srcel = l1->head_sentinel->next;
    el = dest->head_sentinel;
    while (srcel != l1->tail_sentinel) {
        el->next = (struct list_entry_s *)malloc(sizeof(struct list_entry_s));
        el->next->prev = el;
        el = el->next;
        el->data = srcel->data;
        srcel = srcel->next;
    }
    dest->mid = el;     /* approximate position (adjust later) */
    /* copy list 2 */
    srcel = l2->head_sentinel->next;
    while (srcel != l2->tail_sentinel) {
        el->next = (struct list_entry_s *)malloc(sizeof(struct list_entry_s));
        el->next->prev = el;
        el = el->next;
        el->data = srcel->data;
        srcel = srcel->next;
    }
    el->next = dest->tail_sentinel;
    dest->tail_sentinel->prev = el;

    /* fix mid pointer */
    err = l2->numels - l1->numels;
    if ((err+1)/2 > 0) {        /* correct pos RIGHT (err-1)/2 moves */
        err = (err+1)/2;
        for (cnt = 0; cnt < (unsigned int)err; cnt++) dest->mid = dest->mid->next;
    } else if (err/2 < 0) { /* correct pos LEFT (err/2)-1 moves */
        err = -err/2;
        for (cnt = 0; cnt < (unsigned int)err; cnt++) dest->mid = dest->mid->prev;
    }

    assert(!(list_repOk(l1) && list_repOk(l2)) || list_repOk(dest));

    return 0;
}

int list_sort(list_t *restrict l, int versus) {
    if (l->iter_active || l->attrs.comparator == NULL) /* cannot modify list in the middle of an iteration */
        return -1;

    if (l->numels <= 1)
        return 0;
    list_sort_quicksort(l, versus, 0, l->head_sentinel->next, l->numels-1, l->tail_sentinel->prev);
    assert(list_repOk(l));
    return 0;
}

#ifdef SIMCLIST_WITH_THREADS
struct list_sort_wrappedparams {
    list_t *restrict l;
    int versus;
    unsigned int first, last;
    struct list_entry_s *fel, *lel;
};

static void *list_sort_quicksort_threadwrapper(void *wrapped_params) {
    struct list_sort_wrappedparams *wp = (struct list_sort_wrappedparams *)wrapped_params;
    list_sort_quicksort(wp->l, wp->versus, wp->first, wp->fel, wp->last, wp->lel);
    free(wp);
    pthread_exit(NULL);
    return NULL;
}
#endif

static inline void list_sort_selectionsort(list_t *restrict l, int versus,
        unsigned int first, struct list_entry_s *fel,
        unsigned int last, struct list_entry_s *lel) {
    struct list_entry_s *cursor, *toswap, *firstunsorted;
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

static void list_sort_quicksort(list_t *restrict l, int versus,
        unsigned int first, struct list_entry_s *fel,
        unsigned int last, struct list_entry_s *lel) {
    unsigned int pivotid;
    unsigned int i;
    register struct list_entry_s *pivot;
    struct list_entry_s *left, *right;
    void *tmpdata;
#ifdef SIMCLIST_WITH_THREADS
    pthread_t tid;
    int traised;
#endif


    if (last <= first)      /* <= 1-element lists are always sorted */
        return;

    if (last - first+1 <= SIMCLIST_MINQUICKSORTELS) {
        list_sort_selectionsort(l, versus, first, fel, last, lel);
        return;
    }

    /* base of iteration: one element list */
    if (! (last > first)) return;

    pivotid = (get_random() % (last - first + 1));
    /* pivotid = (last - first + 1) / 2; */

    /* find pivot */
    if (pivotid < (last - first + 1)/2) {
        for (i = 0, pivot = fel; i < pivotid; pivot = pivot->next, i++);
    } else {
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
            } else {
                left = left->next;
            }
        }
    } else {                /* right part longer */
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
            } else {
                right = right->prev;
            }
        }
    }

    /* sort sublists A and B :       |---A---| pivot |---B---| */

#ifdef SIMCLIST_WITH_THREADS
    traised = 0;
    if (pivotid > 0) {
        /* prepare wrapped args, then start thread */
        if (l->threadcount < SIMCLIST_MAXTHREADS-1) {
            struct list_sort_wrappedparams *wp = (struct list_sort_wrappedparams *)malloc(sizeof(struct list_sort_wrappedparams));
            l->threadcount++;
            traised = 1;
            wp->l = l;
            wp->versus = versus;
            wp->first = first;
            wp->fel = fel;
            wp->last = first+pivotid-1;
            wp->lel = pivot->prev;
            if (pthread_create(&tid, NULL, list_sort_quicksort_threadwrapper, wp) != 0) {
                free(wp);
                traised = 0;
                list_sort_quicksort(l, versus, first, fel, first+pivotid-1, pivot->prev);
            }
        } else {
            list_sort_quicksort(l, versus, first, fel, first+pivotid-1, pivot->prev);
        }
    }
    if (first + pivotid < last) list_sort_quicksort(l, versus, first+pivotid+1, pivot->next, last, lel);
    if (traised) {
        pthread_join(tid, (void **)NULL);
        l->threadcount--;
    }
#else
    if (pivotid > 0) list_sort_quicksort(l, versus, first, fel, first+pivotid-1, pivot->prev);
    if (first + pivotid < last) list_sort_quicksort(l, versus, first+pivotid+1, pivot->next, last, lel);
#endif
}

int list_iterator_start(list_t *restrict l) {
    if (l->iter_active) return 0;
    l->iter_pos = 0;
    l->iter_active = 1;
    l->iter_curentry = l->head_sentinel->next;
    return 1;
}

void *list_iterator_next(list_t *restrict l) {
    void *toret;

    if (! l->iter_active) return NULL;

    toret = l->iter_curentry->data;
    l->iter_curentry = l->iter_curentry->next;
    l->iter_pos++;

    return toret;
}

int list_iterator_hasnext(const list_t *restrict l) {
    if (! l->iter_active) return 0;
    return (l->iter_pos < l->numels);
}

int list_iterator_stop(list_t *restrict l) {
    if (! l->iter_active) return 0;
    l->iter_pos = 0;
    l->iter_active = 0;
    return 1;
}

int list_hash(const list_t *restrict l, list_hash_t *restrict hash) {
    struct list_entry_s *x;
    list_hash_t tmphash;

    assert(hash != NULL);

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
        return -1;
#endif
    } else {
        /* hash each element with the user-given function */
        for (x = l->head_sentinel->next; x != l->tail_sentinel; x = x->next) {
            tmphash += tmphash ^ l->attrs.hasher(x->data);
            tmphash += tmphash % l->numels;
        }
    }

    *hash = tmphash;

    return 0;
}

#ifndef SIMCLIST_NO_DUMPRESTORE
int list_dump_getinfo_filedescriptor(int fd, list_dump_info_t *restrict info) {
    int32_t terminator_head, terminator_tail;
    uint32_t elemlen;
    off_t hop;


    /* version */
    READ_ERRCHECK(fd, & info->version, sizeof(info->version));
    info->version = ntohs(info->version);
    if (info->version > SIMCLIST_DUMPFORMAT_VERSION) {
        errno = EILSEQ;
        return -1;
    }

    /* timestamp.tv_sec and timestamp.tv_usec */
    READ_ERRCHECK(fd, & info->timestamp.tv_sec, sizeof(info->timestamp.tv_sec));
    info->timestamp.tv_sec = ntohl(info->timestamp.tv_sec);
    READ_ERRCHECK(fd, & info->timestamp.tv_usec, sizeof(info->timestamp.tv_usec));
    info->timestamp.tv_usec = ntohl(info->timestamp.tv_usec);

    /* list terminator (to check thereafter) */
    READ_ERRCHECK(fd, & terminator_head, sizeof(terminator_head));
    terminator_head = ntohl(terminator_head);

    /* list size */
    READ_ERRCHECK(fd, & info->list_size, sizeof(info->list_size));
    info->list_size = ntohl(info->list_size);

    /* number of elements */
    READ_ERRCHECK(fd, & info->list_numels, sizeof(info->list_numels));
    info->list_numels = ntohl(info->list_numels);

    /* length of each element (for checking for consistency) */
    READ_ERRCHECK(fd, & elemlen, sizeof(elemlen));
    elemlen = ntohl(elemlen);

    /* list hash */
    READ_ERRCHECK(fd, & info->list_hash, sizeof(info->list_hash));
    info->list_hash = ntohl(info->list_hash);

    /* check consistency */
    if (elemlen > 0) {
        /* constant length, hop by size only */
        hop = info->list_size;
    } else {
        /* non-constant length, hop by size + all element length blocks */
        hop = info->list_size + elemlen*info->list_numels;
    }
    if (lseek(fd, hop, SEEK_CUR) == -1) {
        return -1;
    }

    /* read the trailing value and compare with terminator_head */
    READ_ERRCHECK(fd, & terminator_tail, sizeof(terminator_tail));
    terminator_tail = ntohl(terminator_tail);

    if (terminator_head == terminator_tail)
        info->consistent = 1;
    else
        info->consistent = 0;

    return 0;
}

int list_dump_getinfo_file(const char *restrict filename, list_dump_info_t *restrict info) {
    int fd, ret;

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) return -1;

    ret = list_dump_getinfo_filedescriptor(fd, info);
    close(fd);

    return ret;
}

int list_dump_filedescriptor(const list_t *restrict l, int fd, size_t *restrict len) {
    struct list_entry_s *x;
    void *ser_buf;
    uint32_t bufsize;
    struct timeval timeofday;
    struct list_dump_header_s header;

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
    header.ver = htons( SIMCLIST_DUMPFORMAT_VERSION );

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
        if (htonl(list_hash(l, & header.listhash)) != 0) {
            /* could not compute list hash! */
            return -1;
        }
    } else {
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
            ser_buf = l->attrs.serializer(l->head_sentinel->next->data, & header.elemlen);
            free(ser_buf);
            /* request custom serialization of each element */
            for (x = l->head_sentinel->next; x != l->tail_sentinel; x = x->next) {
                ser_buf = l->attrs.serializer(x->data, &bufsize);
                header.totlistlen += bufsize;
                if (header.elemlen != 0) {      /* continue on speculation */
                    if (header.elemlen != bufsize) {
                        free(ser_buf);
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
                } else {                        /* speculation found broken */
                    WRITE_ERRCHECK(fd, & bufsize, sizeof(size_t));
                    WRITE_ERRCHECK(fd, ser_buf, bufsize);
                }
                free(ser_buf);
            }
        } else if (l->attrs.meter != NULL) {
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
                } else {
                    WRITE_ERRCHECK(fd, &bufsize, sizeof(size_t));
                    WRITE_ERRCHECK(fd, x->data, bufsize);
                }
            }
        }
        /* adjust endianness */
        header.elemlen = htonl(header.elemlen);
        header.totlistlen = htonl(header.totlistlen);
    }

    /* write random terminator */
    WRITE_ERRCHECK(fd, & header.rndterm, sizeof(header.rndterm));        /* list terminator */


    /* write header */
    lseek(fd, 0, SEEK_SET);

    WRITE_ERRCHECK(fd, & header.ver, sizeof(header.ver));                        /* version */
    WRITE_ERRCHECK(fd, & header.timestamp_sec, sizeof(header.timestamp_sec));    /* timestamp seconds */
    WRITE_ERRCHECK(fd, & header.timestamp_usec, sizeof(header.timestamp_usec));  /* timestamp microseconds */
    WRITE_ERRCHECK(fd, & header.rndterm, sizeof(header.rndterm));                /* random terminator */

    WRITE_ERRCHECK(fd, & header.totlistlen, sizeof(header.totlistlen));          /* total length of elements */
    WRITE_ERRCHECK(fd, & header.numels, sizeof(header.numels));                  /* number of elements */
    WRITE_ERRCHECK(fd, & header.elemlen, sizeof(header.elemlen));                /* size of each element, or 0 for independent */
    WRITE_ERRCHECK(fd, & header.listhash, sizeof(header.listhash));              /* list hash, or 0 for "ignore" */


    /* possibly store total written length in "len" */
    if (len != NULL) {
        *len = sizeof(header) + ntohl(header.totlistlen);
    }

    return 0;
}

int list_restore_filedescriptor(list_t *restrict l, int fd, size_t *restrict len) {
    struct list_dump_header_s header;
    unsigned long cnt;
    void *buf;
    uint32_t elsize, totreadlen, totmemorylen;

    memset(& header, 0, sizeof(header));

    /* read header */

    /* version */
    READ_ERRCHECK(fd, &header.ver, sizeof(header.ver));
    header.ver = ntohs(header.ver);
    if (header.ver != SIMCLIST_DUMPFORMAT_VERSION) {
        errno = EILSEQ;
        return -1;
    }

    /* timestamp */
    READ_ERRCHECK(fd, & header.timestamp_sec, sizeof(header.timestamp_sec));
    header.timestamp_sec = ntohl(header.timestamp_sec);
    READ_ERRCHECK(fd, & header.timestamp_usec, sizeof(header.timestamp_usec));
    header.timestamp_usec = ntohl(header.timestamp_usec);

    /* list terminator */
    READ_ERRCHECK(fd, & header.rndterm, sizeof(header.rndterm));

    header.rndterm = ntohl(header.rndterm);

    /* total list size */
    READ_ERRCHECK(fd, & header.totlistlen, sizeof(header.totlistlen));
    header.totlistlen = ntohl(header.totlistlen);

    /* number of elements */
    READ_ERRCHECK(fd, & header.numels, sizeof(header.numels));
    header.numels = ntohl(header.numels);

    /* length of every element, or '0' = variable */
    READ_ERRCHECK(fd, & header.elemlen, sizeof(header.elemlen));
    header.elemlen = ntohl(header.elemlen);

    /* list hash, or 0 = 'ignore' */
    READ_ERRCHECK(fd, & header.listhash, sizeof(header.listhash));
    header.listhash = ntohl(header.listhash);


    /* read content */
    totreadlen = totmemorylen = 0;
    if (header.elemlen > 0) {
        /* elements have constant size = header.elemlen */
        if (l->attrs.unserializer != NULL) {
            /* use unserializer */
            buf = malloc(header.elemlen);
            for (cnt = 0; cnt < header.numels; cnt++) {
                READ_ERRCHECK(fd, buf, header.elemlen);
                list_append(l, l->attrs.unserializer(buf, & elsize));
                totmemorylen += elsize;
            }
        } else {
            /* copy verbatim into memory */
            for (cnt = 0; cnt < header.numels; cnt++) {
                buf = malloc(header.elemlen);
                READ_ERRCHECK(fd, buf, header.elemlen);
                list_append(l, buf);
            }
            totmemorylen = header.numels * header.elemlen;
        }
        totreadlen = header.numels * header.elemlen;
    } else {
        /* elements have variable size. Each element is preceded by its size */
        if (l->attrs.unserializer != NULL) {
            /* use unserializer */
            for (cnt = 0; cnt < header.numels; cnt++) {
                READ_ERRCHECK(fd, & elsize, sizeof(elsize));
                buf = malloc((size_t)elsize);
                READ_ERRCHECK(fd, buf, elsize);
                totreadlen += elsize;
                list_append(l, l->attrs.unserializer(buf, & elsize));
                totmemorylen += elsize;
            }
        } else {
            /* copy verbatim into memory */
            for (cnt = 0; cnt < header.numels; cnt++) {
                READ_ERRCHECK(fd, & elsize, sizeof(elsize));
                buf = malloc(elsize);
                READ_ERRCHECK(fd, buf, elsize);
                totreadlen += elsize;
                list_append(l, buf);
            }
            totmemorylen = totreadlen;
        }
    }

    READ_ERRCHECK(fd, &elsize, sizeof(elsize));  /* read list terminator */
    elsize = ntohl(elsize);

    /* possibly verify the list consistency */
    /* wrt hash */
    /* don't do that
    if (header.listhash != 0 && header.listhash != list_hash(l)) {
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

int list_dump_file(const list_t *restrict l, const char *restrict filename, size_t *restrict len) {
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

    list_dump_filedescriptor(l, fd, len);
    close(fd);

    return 0;
}

int list_restore_file(list_t *restrict l, const char *restrict filename, size_t *restrict len) {
    int fd;

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) return -1;

    list_restore_filedescriptor(l, fd, len);
    close(fd);

    return 0;
}
#endif /* ifndef SIMCLIST_NO_DUMPRESTORE */


static int list_drop_elem(list_t *restrict l, struct list_entry_s *tmp, unsigned int pos) {
    if (tmp == NULL) return -1;

    /* fix mid pointer. This is wrt the PRE situation */
    if (l->numels % 2) {    /* now odd */
        /* sort out the base case by hand */
        if (l->numels == 1) l->mid = NULL;
        else if (pos >= l->numels/2) l->mid = l->mid->prev;
    } else {                /* now even */
        if (pos < l->numels/2) l->mid = l->mid->next;
    }

    tmp->prev->next = tmp->next;
    tmp->next->prev = tmp->prev;

    /* free what's to be freed */
    if (l->attrs.copy_data && tmp->data != NULL)
        free(tmp->data);

    if (l->spareelsnum < SIMCLIST_MAX_SPARE_ELEMS) {
        l->spareels[l->spareelsnum++] = tmp;
    } else {
        free(tmp);
    }

    return 0;
}

/* ready-made comparators and meters */
#define SIMCLIST_NUMBER_COMPARATOR(type)     int list_comparator_##type(const void *a, const void *b) { return( *(type *)a < *(type *)b) - (*(type *)a > *(type *)b); }

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

int list_comparator_string(const void *a, const void *b) { return strcmp((const char *)b, (const char *)a); }

/* ready-made metric functions */
#define SIMCLIST_METER(type)        size_t list_meter_##type(const void *el) { if (el) { /* kill compiler whinge */ } return sizeof(type); }

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

size_t list_meter_string(const void *el) { return strlen((const char *)el) + 1; }

/* ready-made hashing functions */
#define SIMCLIST_HASHCOMPUTER(type)    list_hash_t list_hashcomputer_##type(const void *el) { return (list_hash_t)(*(type *)el); }

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

list_hash_t list_hashcomputer_string(const void *el) {
    size_t l;
    list_hash_t hash = 123;
    const char *str = (const char *)el;
    char plus;

    for (l = 0; str[l] != '\0'; l++) {
        if (l) plus = hash ^ str[l];
        else plus = hash ^ (str[l] - str[0]);
        hash += (plus << (CHAR_BIT * (l % sizeof(list_hash_t))));
    }

    return hash;
}


#ifndef NDEBUG
static int list_repOk(const list_t *restrict l) {
    int ok, i;
    struct list_entry_s *s;

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
        for (i = -1, s = l->head_sentinel; i < (int)(l->numels-1)/2 && s->next != NULL; i++, s = s->next) {
            if (s->next->prev != s) break;
        }
        ok = (i == (int)(l->numels-1)/2 && l->mid == s);
        if (!ok) return 0;
        for (; s->next != NULL; i++, s = s->next) {
            if (s->next->prev != s) break;
        }
        ok = (i == (int)l->numels && s == l->tail_sentinel);
    }

    return ok;
}

static int list_attrOk(const list_t *restrict l) {
    int ok;

    ok = (l->attrs.copy_data == 0 || l->attrs.meter != NULL);
    return ok;
}

#endif

