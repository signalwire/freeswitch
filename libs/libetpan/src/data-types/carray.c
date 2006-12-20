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
 * $Id: carray.c,v 1.10 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "carray.h"

#define MIN_ARRAY_SIZE 4

LIBETPAN_EXPORT
carray * carray_new(unsigned int initsize) {
  carray * array;

  array = (carray *) malloc(sizeof(carray));
  if (!array) return NULL;
  
  if (initsize < MIN_ARRAY_SIZE)
    initsize = MIN_ARRAY_SIZE;
  
  array->len = 0;
  array->max = initsize;
  array->array = (void **) malloc(sizeof(void *) * initsize);
  if (!array->array) {
    free(array);
    return NULL;
  }
  return array;
}

LIBETPAN_EXPORT
int carray_add(carray * array, void * data, unsigned int * index) {
  int r;
  
  r = carray_set_size(array, array->len + 1);
  if (r < 0)
    return r;

  array->array[array->len - 1] = data;
  if (index != NULL)
    * index = array->len - 1;

  return 0;
}

LIBETPAN_EXPORT
int carray_set_size(carray * array, unsigned int new_size)
{
  if (new_size > array->max) {
    unsigned int n = array->max * 2;
    void * new;

    while (n <= new_size)
      n *= 2;

    new = (void **) realloc(array->array, sizeof(void *) * n);
    if (!new)
      return -1;
    array->array = new;
    array->max = n;
  }
  array->len = new_size;

  return 0;
}

LIBETPAN_EXPORT
int carray_delete_fast(carray * array, unsigned int indx) {
  if (indx >= array->len)
    return -1;

  array->array[indx] = NULL;

  return 0;
}

LIBETPAN_EXPORT
int carray_delete(carray * array, unsigned int indx) {
  if (indx >= array->len)
    return -1;

  if (indx != --array->len)
    array->array[indx] = array->array[array->len];
  return 0;
}

LIBETPAN_EXPORT
int carray_delete_slow(carray * array, unsigned int indx) {
  if (indx >= array->len)
    return -1;

  if (indx != --array->len) 
    memmove(array->array + indx, array->array + indx + 1,
	    (array->len - indx) * sizeof(void *));
  return 0;
}

#ifdef NO_MACROS
LIBETPAN_EXPORT
void ** carray_data(carray * array) {
  return array->array;
}

LIBETPAN_EXPORT
unsigned int carray_count(carray * array) {
  return array->len;
}

LIBETPAN_EXPORT
void * carray_get(carray * array, unsigned int indx) {
  return array->array[indx];
}

LIBETPAN_EXPORT
void carray_set(carray * array, unsigned int indx, void * value) {
  array->array[indx] = value;
}
#endif

LIBETPAN_EXPORT
void carray_free(carray * array) {
  free(array->array);
  free(array);
}
