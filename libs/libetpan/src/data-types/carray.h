/*
 * libEtPan! -- a mail stuff library
 *
 * carray - Implements simple dynamic pointer arrays
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
 * $Id: carray.h,v 1.16 2006/03/22 08:10:47 hoa Exp $
 */

#ifndef CARRAY_H
#define CARRAY_H

#ifndef LIBETPAN_CONFIG_H
#	include <libetpan/libetpan-config.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct carray_s {
  void ** array;
  unsigned int len;
  unsigned int max;
};

typedef struct carray_s carray;

/* Creates a new array of pointers, with initsize preallocated cells */
LIBETPAN_EXPORT
carray *   carray_new(unsigned int initsize);

/* Adds the pointer to data in the array.
   Returns the index of the pointer in the array or -1 on error */
LIBETPAN_EXPORT
int       carray_add(carray * array, void * data, unsigned int * index);

LIBETPAN_EXPORT
int carray_set_size(carray * array, unsigned int new_size);

/* Removes the cell at this index position. Returns TRUE on success.
   Order of elements in the array IS changed. */
LIBETPAN_EXPORT
int       carray_delete(carray * array, unsigned int indx);

/* Removes the cell at this index position. Returns TRUE on success.
   Order of elements in the array IS not changed. */
LIBETPAN_EXPORT
int       carray_delete_slow(carray * array, unsigned int indx);

/* remove without decreasing the size of the array */
LIBETPAN_EXPORT
int carray_delete_fast(carray * array, unsigned int indx);

/* Some of the following routines can be implemented as macros to
   be faster. If you don't want it, define NO_MACROS */
#ifdef NO_MACROS

/* Returns the array itself */
LIBETPAN_EXPORT
void **   carray_data(carray *);

/* Returns the number of elements in the array */
LIBETPAN_EXPORT
unsigned int carray_count(carray *);

/* Returns the contents of one cell */
LIBETPAN_EXPORT
void *    carray_get(carray * array, unsigned int indx);

/* Sets the contents of one cell */
LIBETPAN_EXPORT
void      carray_set(carray * array, unsigned int indx, void * value);

#else

#if 0
#define   carray_data(a)         (a->array)
#define   carray_count(a)        (a->len)
#define   carray_get(a, indx)    (a->array[indx])
#define   carray_set(a, indx, v) do { a->array[indx]=v; } while(0)
#endif

static inline void ** carray_data(carray * array) {
  return array->array;
}

static inline unsigned int carray_count(carray * array) {
  return array->len;
}

static inline void * carray_get(carray * array, unsigned int indx) {
  return array->array[indx];
}

static inline void carray_set(carray * array,
    unsigned int indx, void * value) {
  array->array[indx] = value;
}
#endif

LIBETPAN_EXPORT
void carray_free(carray * array);

#ifdef __cplusplus
}
#endif

#endif
