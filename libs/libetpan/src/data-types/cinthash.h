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
 * $Id: cinthash.h,v 1.6 2004/11/21 21:53:31 hoa Exp $
 */

#ifndef CINTHASH_H

#define CINTHASH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cinthash_t {
  struct cinthash_list * table;
  unsigned long hashtable_size ;
  unsigned long count;
} cinthash_t;

cinthash_t * cinthash_new(unsigned long hashtable_size);
void cinthash_free(cinthash_t * table);

int cinthash_add(cinthash_t * table, unsigned long hash, void * data);
int cinthash_remove(cinthash_t * table, unsigned long hash);
void * cinthash_find(cinthash_t * table, unsigned long hash);

void cinthash_foreach_key(cinthash_t * table,
			  void (* func)(unsigned long, void *),
			  void * data);

void cinthash_foreach_data(cinthash_t * table,
			   void (* fun)(void *, void *),
			   void * data);

#ifdef __cplusplus
}
#endif

#endif
