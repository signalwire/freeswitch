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
 * $Id: generic_cache.h,v 1.17 2004/11/21 21:53:35 hoa Exp $
 */

#ifndef GENERIC_CACHE_H

#define GENERIC_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generic_cache_types.h"
#include "mailmessage_types.h"
#include "chash.h"
#include "carray.h"
#include "mail_cache_db_types.h"

int generic_cache_create_dir(char * dirname);

int generic_cache_store(char * filename, char * content, size_t length);
int generic_cache_read(char * filename, char ** result, size_t * result_len);

int generic_cache_fields_read(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mailimf_fields ** result);
  
int generic_cache_fields_write(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mailimf_fields * fields);
  
int generic_cache_flags_read(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mail_flags ** result);
  
int generic_cache_flags_write(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mail_flags * flags);
  
int generic_cache_delete(struct mail_cache_db * cache_db, char * keyname);

#if 0
int generic_cache_fields_read(DB * dbp, MMAPString * mmapstr,
			      char * keyname, struct mailimf_fields ** result);

int generic_cache_fields_write(DB * dbp, MMAPString * mmapstr,
			       char * keyname, struct mailimf_fields * fields);

int generic_cache_flags_read(DB * dbp, MMAPString * mmapstr,
			     char * keyname, struct mail_flags ** result);

int generic_cache_flags_write(DB * dbp, MMAPString * mmapstr,
			      char * keyname, struct mail_flags * flags);

int generic_cache_delete(DB * dbp, char * keyname);
#endif

struct mail_flags_store * mail_flags_store_new(void);

void mail_flags_store_clear(struct mail_flags_store * flags_store);

void mail_flags_store_free(struct mail_flags_store * flags_store);

int mail_flags_store_set(struct mail_flags_store * flags_store,
    mailmessage * msg);

void mail_flags_store_sort(struct mail_flags_store * flags_store);

struct mail_flags *
mail_flags_store_get(struct mail_flags_store * flags_store, uint32_t index);

int mail_flags_compare(struct mail_flags * flags1, struct mail_flags * flags2);

#ifdef __cplusplus
}
#endif

#endif
