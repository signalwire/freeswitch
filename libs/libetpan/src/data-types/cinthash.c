/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
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
 * $Id: cinthash.c,v 1.9 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifndef LIBETPAN_CONFIG_H
#	include "libetpan-config.h"
#endif
#include <stdlib.h>
#include "cinthash.h"

struct cinthash_list {
  unsigned long hash;
  void * data;
  struct cinthash_list * next;
};

static struct cinthash_list HASH_LISTHEAD_NEW = { 0, NULL, NULL };

static inline int hash_list_add(cinthash_t * table,
                                unsigned long hash, void * data)
{
  struct cinthash_list * ht;
  int index;

  index = hash % table->hashtable_size;

  ht = malloc(sizeof(struct cinthash_list));
  if (ht == NULL)
    return -1;

  ht->hash = hash;
  ht->data = data;
  ht->next = table->table[index].next;

  table->table[index].next = ht;

  return 0;
}

static inline void hash_list_free(struct cinthash_list * list)
{
  struct cinthash_list * cur;
  struct cinthash_list * next;

  next = list;
  while (next != NULL) {
    cur = next;
    next = cur->next;
    free(cur);
  }
}

static inline int hash_list_remove(cinthash_t * table, unsigned long hash)
{
  struct cinthash_list * cur;
  int index;

  index = hash % table->hashtable_size;

  for(cur = &table->table[index] ; cur->next != NULL ; cur = cur->next) {
    if (cur->next->hash == hash) {
      struct cinthash_list * hash_data;

      hash_data = cur->next;
      cur->next = cur->next->next;

      free(hash_data);

      return 0;
    }
  }

  return -1;
}

static inline void * hash_list_find(cinthash_t * table, unsigned long hash)
{
  struct cinthash_list * cur;
  int index;

  index = hash % table->hashtable_size;

  for(cur = table->table[index].next ; cur != NULL ; cur = cur->next) {
    if (cur->hash == hash)
      return cur->data;
  }

  return NULL;
}

cinthash_t * cinthash_new(unsigned long hashtable_size)
{
  cinthash_t * ht;
  unsigned long i;

  ht = malloc(sizeof(cinthash_t));
  if (ht == NULL)
    return NULL;

  ht->table = malloc(sizeof(struct cinthash_list) * hashtable_size);
  if (ht->table == NULL)
    return NULL;

  ht->hashtable_size = hashtable_size;
  ht->count = 0;
  
  for(i = 0 ; i < hashtable_size ; i++)
    ht->table[i] = HASH_LISTHEAD_NEW;

  return ht;
}

void cinthash_free(cinthash_t * table)
{
  unsigned long i;

  for(i = 0 ; i < table->hashtable_size ; i++)
    hash_list_free(table->table[i].next);

  free(table->table);

  free(table);
}

int cinthash_add(cinthash_t * table, unsigned long hash, void * data)
{
  int index;

  index = hash % table->hashtable_size;

  if (table->table[index].data == NULL) {
    table->table[index].hash = hash;
    table->table[index].data = data;
    table->table[index].next = NULL;

    table->count ++;

    return 0;
  }
  else {
    int r;

    r = hash_list_add(table, hash, data);
    if (r == -1)
      return -1;

    table->count ++;

    return 0;
  }
}

int cinthash_remove(cinthash_t * table, unsigned long hash)
{
  int index;

  index = hash % table->hashtable_size;

  if (table->table[index].hash == hash) {
    table->table[index].hash = 0;
    table->table[index].data = NULL;

    table->count --;

    return 0;
  }
  else {
    int r;

    r = hash_list_remove(table, hash);

    table->count --;
    
    return 0;
  }
}

void * cinthash_find(cinthash_t * table, unsigned long hash)
{
  int index;

  index = hash % table->hashtable_size;

  if (table->table[index].hash == hash)
    return table->table[index].data;
 
  return hash_list_find(table, hash);
}

void cinthash_foreach_key(cinthash_t * table,
			  void (* func)(unsigned long, void *),
			  void * data)
{
  unsigned long index;
  struct cinthash_list * cur;

  for(index = 0 ; index < table->hashtable_size ; index ++) {
    if (table->table[index].data != NULL) {
      func(table->table[index].hash, data);
      for(cur = table->table[index].next ; cur != NULL ; cur = cur->next)
        func(cur->hash, data);
    }
  }
}

void cinthash_foreach_data(cinthash_t * table,
			   void (* func)(void *, void *),
			   void * data)
{
  unsigned long index;
  struct cinthash_list * cur;

  for(index = 0 ; index < table->hashtable_size ; index ++) {
    if (table->table[index].data != NULL) {
      func(table->table[index].data, data);
      for(cur = table->table[index].next ; cur != NULL ; cur = cur->next)
        func(cur->data, data);
    }
  }
}
