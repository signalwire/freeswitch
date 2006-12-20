/*
 * libEtPan! -- a mail stuff library
 *
 * clist - Implements simple generic double-linked pointer lists
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
 * $Id: clist.c,v 1.10 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <stdlib.h>
#ifndef LIBETPAN_CONFIG_H
#	include "libetpan-config.h"
#endif

#include "clist.h"

clist * clist_new(void) {
  clist * lst;
  
  lst = (clist *) malloc(sizeof(clist));
  if (!lst) return NULL;
  
  lst->first = lst->last = NULL;
  lst->count = 0;
  
  return lst;
}

void clist_free(clist * lst) {
  clistcell * l1, * l2;

  l1 = lst->first;
  while (l1) {
    l2 = l1->next;
    free(l1);
    l1 = l2;
  }

  free(lst);
}

#ifdef NO_MACROS
int clist_isempty(clist * lst) {
  return ((lst->first==lst->last) && (lst->last==NULL));
}

clistiter * clist_begin(clist * lst) {
  return lst->first;
}

clistiter * clist_end(clist * lst) {
  return lst->last;
}

clistiter * clist_next(clistiter * iter) {
  if (iter)
    return iter->next;
  else
    return NULL;
}

clistiter * clist_previous(clistiter * iter) {
  if (iter)
    return iter->previous;
  else
    return NULL;
}

void * clist_content(clistiter * iter) {
  if (iter)
    return iter->data;
  else
    return NULL;
}

int clist_count(clist * lst) {
  return lst->count;
}

int clist_prepend(clist * lst, void * data) {
  return clist_insert_before(lst, lst->first, data);
}

int clist_append(clist * lst, void * data) {
  return clist_insert_after(lst, lst->last, data);
}
#endif

int clist_insert_before(clist * lst, clistiter * iter, void * data) {
  clistcell * c;

  c = (clistcell *) malloc(sizeof(clistcell));
  if (!c) return -1;

  c->data = data;
  lst->count++;
  
  if (clist_isempty(lst)) {
    c->previous = c->next = NULL;
    lst->first = lst->last = c;
    return 0;
  }
  
  if (!iter) {
    c->previous = lst->last;
    c->previous->next = c;
    c->next = NULL;
    lst->last = c;
    return 0;
  }

  c->previous = iter->previous;
  c->next = iter;
  c->next->previous = c;
  if (c->previous)
    c->previous->next = c;
  else
    lst->first = c;

  return 0;
}

int clist_insert_after(clist * lst, clistiter * iter, void * data) {
  clistcell * c;

  c = (clistcell *) malloc(sizeof(clistcell));
  if (!c) return -1;

  c->data = data;
  lst->count++;
  
  if (clist_isempty(lst)) {
    c->previous = c->next = NULL;
    lst->first = lst->last = c;
    return 0;
  }
  
  if (!iter) {
    c->previous = lst->last;
    c->previous->next = c;
    c->next = NULL;
    lst->last = c;
    return 0;
  }

  c->previous = iter;
  c->next = iter->next;
  if (c->next)
    c->next->previous = c;
  else
    lst->last = c;
  c->previous->next = c;

  return 0;
}

clistiter * clist_delete(clist * lst, clistiter * iter) {
  clistiter * ret;
  
  if (!iter) return NULL;

  if (iter->previous) 
    iter->previous->next = iter->next;
  else
    lst->first = iter->next;

  if (iter->next) {
    iter->next->previous = iter->previous;
    ret = iter->next;
  }  else {
    lst->last = iter->previous;
    ret = NULL;
  }

  free(iter);
  lst->count--;
  
  return ret;
}



void clist_foreach(clist * lst, clist_func func, void * data)
{
  clistiter * cur;

  for(cur = clist_begin(lst) ; cur != NULL ; cur = cur->next)
    func(cur->data, data);
}

void clist_concat(clist * dest, clist * src)
{
  if (src->first == NULL) {
    /* do nothing */
  }
  else if (dest->last == NULL) {
    dest->first = src->first;
    dest->last = src->last;
  }
  else {
    dest->last->next = src->first;
    src->first->previous = dest->last;
    dest->last = src->last;
  }
  
  dest->count += src->count;
  src->last = src->first = NULL;
}

static inline clistiter * internal_clist_nth(clist * lst, int index)
{
  clistiter * cur;

  cur = clist_begin(lst);
  while ((index > 0) && (cur != NULL)) {
    cur = cur->next;
    index --;
  }

  if (cur == NULL)
    return NULL;

  return cur;
}

void * clist_nth_data(clist * lst, int index)
{
  clistiter * cur;

  cur = internal_clist_nth(lst, index);
  if (cur == NULL)
    return NULL;
  
  return cur->data;
}

clistiter * clist_nth(clist * lst, int index)
{
  return internal_clist_nth(lst, index);
}
