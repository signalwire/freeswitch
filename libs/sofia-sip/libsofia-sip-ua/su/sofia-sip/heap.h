/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2007 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef SOFIA_SIP_HEAP_H
/** Defined when <sofia-sip/heap.h> has been included. */
#define SOFIA_SIP_HEAP_H

/**@file sofia-sip/heap.h
 *
 * Heap template implemented with dynamic array.
 *
 * This file contain template macros implementing @a heap in C. The @a heap
 * keeps its element in a known order and it can be used to implement, for
 * example, a prioritye queue or an ordered queue.
 *
 * The ordering within the heap is defined as follows:
 * - indexing starts from 1
 * - for each element with index @a [i] in the heap there are two descendant
 *   elements with indices @a [2*i] and @a [2*i+1],
 * - the heap guarantees that the descendant elements are never smaller than
 *   their parent element.
 * Therefore it follows that there is no element smaller than element at
 * index [1] in the rest of the heap.
 *
 * Adding and removing elements to the heap is an @a O(logN)
 * operation.
 *
 * The heap array is resizeable, and it usually contain pointers to the
 * actual entries. The template macros define two functions used to add and
 * remove entries to the heap. The @a add() function takes the element to be
 * added as its argument, the @a remove() function the index of the element
 * to be removed. The template defines also a predicate used to check if the
 * heap is full, and a function used to resize the heap.
 *
 * The heap user must define four primitives:
 * - less than comparison
 * - array setter
 * - heap array allocator
 * - empty element
 *
 * Please note that in order to remove an entry in the heap, the application
 * must know its index in the heap array.
 *
 * The heap struct is declared with macro HEAP_DECLARE(). The prototypes for
 * heap functions are instantiated with macro HEAP_PROTOS(). The
 * implementation is instantiated with macro HEAP_BODIES().
 *
 * Example code can be found from <su/torture_heap.c> and
 * <sresolv/sres_cache.c>.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 * @NEW_1_12_7.
 */

/** Minimum size of heap */
#define HEAP_MIN_SIZE 31

/** Declare heap structure type.
 *
 * The macro #HEAP_TYPE contains declaration of the heap structure.
 *
 * @showinitializer
 */
#define HEAP_TYPE struct { void *private; }

/** Prototypes for heap.
 *
 * The macro HEAP_PROTOS() expands to the prototypes of heap functions:
 * - prefix ## resize(argument, in_out_heap, size)
 * - prefix ## free(argument, in_heap)
 * - prefix ## is_full(heap)
 * - prefix ## size(heap)
 * - prefix ## used(heap)
 * - prefix ## add(heap, entry)
 * - prefix ## remove(heap, index)
 * - prefix ## get(heap, index)
 *
 * @param scope     scope of functions
 * @param heaptype  type of heap
 * @param prefix    function prefix
 * @param type      type of entries
 *
 * The declared functions will have scope @a scope (for example, @c static
 * or @c static inline). The declared function names will have prefix @a
 * prefix. The heap structure has type @a heaptype. The heap element type is
 * @a entrytype.
 *
 * @showinitializer
 */
#define HEAP_DECLARE(scope, heaptype, prefix, type) \
scope int prefix##resize(void *, heaptype *, size_t); \
scope int prefix##free(void *, heaptype *); \
scope int prefix##is_full(heaptype const); \
scope size_t prefix##size(heaptype const); \
scope size_t prefix##used(heaptype const); \
scope void prefix##sort(heaptype); \
scope int prefix##add(heaptype, type); \
scope type prefix##remove(heaptype, size_t); \
scope type prefix##get(heaptype, size_t)

/**Heap implementation.
 *
 * The macro HEAP_BODIES() expands to the bodies of heap functions:
 * - prefix ## resize(argument, heap, size)
 * - prefix ## free(argument, in_heap)
 * - prefix ## is_full(heap)
 * - prefix ## size(heap)
 * - prefix ## used(heap)
 * - prefix ## sort(heap)
 * - prefix ## add(heap, entry)
 * - prefix ## remove(heap, index)
 * - prefix ## get(heap, index)
 *
 * @param scope     scope of functions
 * @param prefix    function prefix for heap
 * @param heaptype  type of heap
 * @param type      type of heaped elements
 * @param less      function or macro comparing two entries
 * @param set       function or macro assigning entry to array
 * @param alloc     function allocating or freeing memory
 * @param null      empty element (returned when index is invalid)
 *
 * Functions have scope @a scope, e.g., @c static @c inline.
 * The heap structure has type @a type.
 * The function names start with @a prefix, the field names start
 * with @a pr. The entry type is @a entrytype.

 * The function (or macro) @a less compares two entries in heap. It gets two
 * arguments and it returns true if its left argument is less than its right
 * argument.

 * The function (or macro) @a set stores an entry in heap array. It gets
 * three arguments, first is heap array, second index to the array and third
 * the element to store at the given index.
 *
 * The function (or macro) @a halloc re-allocates the heap array. It
 * receives three arguments, first is the first @a argument given to @a
 * resize(), second the pointer to existing heap and third is the number of
 * bytes in the heap.
 */
#define HEAP_BODIES(scope, heaptype, prefix, type, less, set, alloc, null) \
scope int prefix##resize(void *realloc_arg, heaptype h[1], size_t new_size) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[2]; }; \
  struct prefix##priv *_priv; \
  size_t _offset = \
    (offsetof(struct prefix##priv, _heap[1]) - 1) / sizeof (type); \
  size_t _min_size = 32 - _offset; \
  size_t _bytes; \
  size_t _used = 0; \
 \
  _priv = *(void **)h; \
 \
  if (_priv) { \
    if (new_size == 0) \
      new_size = 2 * _priv->_size + _offset + 1; \
    _used = _priv->_used; \
    if (new_size < _used) \
      new_size = _used; \
  } \
 \
  if (new_size < _min_size) \
    new_size = _min_size; \
 \
  _bytes = (_offset + 1 + new_size) * sizeof (type); \
 \
  (void)realloc_arg; /* avoid warning */ \
  _priv = alloc(realloc_arg, *(struct prefix##priv **)h, _bytes); \
  if (!_priv) \
    return -1; \
 \
  *(struct prefix##priv **)h = _priv; \
  _priv->_size = new_size; \
  _priv->_used = _used; \
 \
  return 0; \
} \
 \
/** Free heap. */ \
scope int prefix##free(void *realloc_arg, heaptype h[1]) \
{ \
  (void)realloc_arg; \
  *(void **)h = alloc(realloc_arg, *(void **)h, 0); \
  return 0; \
} \
 \
/** Check if heap is full */ \
scope int prefix##is_full(heaptype h) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];}; \
  struct prefix##priv *_priv = *(void **)&h; \
 \
  return _priv == NULL || _priv->_used >= _priv->_size; \
} \
 \
/** Add an element to the heap */ \
scope int prefix##add(heaptype h, type e) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];};	\
  struct prefix##priv *_priv = *(void **)&h; \
  type *heap = _priv->_heap - 1; \
  size_t i, parent; \
 \
  if (_priv == NULL || _priv->_used >= _priv->_size) \
    return -1; \
 \
  for (i = ++_priv->_used; i > 1; i = parent) { \
    parent = i / 2; \
    if (!less(e, heap[parent])) \
      break; \
    set(heap, i, heap[parent]); \
  } \
 \
  set(heap, i, e); \
 \
  return 0; \
} \
 \
/** Remove element from heap */ \
scope type prefix##remove(heaptype h, size_t index) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];}; \
  struct prefix##priv *_priv = *(void **)&h; \
  type *heap = _priv->_heap - 1; \
  type retval[1]; \
  type e; \
 \
  size_t top, left, right, move; \
 \
  if (index - 1 >= _priv->_used) \
    return (null); \
 \
  move = _priv->_used--; \
  set(retval, 0, heap[index]); \
 \
  for (top = index;;index = top) { \
    left = 2 * top; \
    right = 2 * top + 1; \
 \
    if (left >= move) \
      break; \
    if (right < move && less(heap[right], heap[left])) \
      top = right; \
    else \
      top = left; \
    set(heap, index, heap[top]); \
  } \
 \
  if (index == move) \
    return *retval; \
 \
  e = heap[move]; \
  for (; index > 1; index = top) { \
    top = index / 2; \
    if (!less(e, heap[top])) \
      break; \
    set(heap, index, heap[top]); \
  } \
 \
  set(heap, index, e); \
 \
  return *retval; \
} \
 \
scope \
type prefix##get(heaptype h, size_t index) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];}; \
  struct prefix##priv *_priv = *(void **)&h; \
 \
  if (--index >= _priv->_used) \
    return (null); \
 \
  return _priv->_heap[index]; \
} \
 \
scope \
size_t prefix##size(heaptype const h) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];}; \
  struct prefix##priv *_priv = *(void **)&h; \
  return _priv ? _priv->_size : 0; \
} \
 \
scope \
size_t prefix##used(heaptype const h) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];}; \
  struct prefix##priv *_priv = *(void **)&h; \
  return _priv ? _priv->_used : 0; \
} \
static int prefix##_less(void *h, size_t a, size_t b) \
{ \
  type *_heap = h; return less(_heap[a], _heap[b]);	\
} \
static void prefix##_swap(void *h, size_t a, size_t b) \
{ \
  type *_heap = h; type _swap = _heap[a]; \
  set(_heap, a, _heap[b]); set(_heap, b, _swap); \
} \
scope void prefix##sort(heaptype h) \
{ \
  struct prefix##priv { size_t _size, _used; type _heap[1];}; \
  struct prefix##priv *_priv = *(void **)&h; \
  if (_priv) \
    su_smoothsort(_priv->_heap - 1, 1, _priv->_used, prefix##_less, prefix##_swap); \
} \
extern int const prefix##dummy_heap

#include <sofia-sip/su_types.h>

SOFIA_BEGIN_DECLS

SOFIAPUBFUN void su_smoothsort(void *base, size_t r0, size_t N,
			       int (*less)(void *base, size_t a, size_t b),
			       void (*swap)(void *base, size_t a, size_t b));

SOFIA_END_DECLS

#endif /** !defined(SOFIA_SIP_HEAP_H) */
