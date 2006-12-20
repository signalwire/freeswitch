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
 * $Id: mail_cache_db.h,v 1.6 2005/04/07 00:05:25 hoa Exp $
 */

#ifndef MAIL_CACHE_DB_H

#define MAIL_CACHE_DB_H

#include <sys/types.h>
#include "mail_cache_db_types.h"
#include "chash.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
  this module will handle a database "f(key) -> value" in a file
  
  berkeley DB or other can be used for implementation of low-level file.
*/

/*
  mail_cache_db_open()
  
  This function opens the file "filename".
  The pointer return in pcache_db should be used for further references
  to the database.
*/

int mail_cache_db_open(const char * filename,
    struct mail_cache_db ** pcache_db);

/*
  mail_cache_db_close()
  
  This function closes the opened database.
  The pointer cannot be used later.
*/

void mail_cache_db_close(struct mail_cache_db * cache_db);

/*
  mail_cache_db_open_lock()
  
  This function opens and locks the file "filename".
  The pointer return in pcache_db should be used for further references
  to the database.
*/

int mail_cache_db_open_lock(const char * filename,
    struct mail_cache_db ** pcache_db);

/*
  mail_cache_db_open_unlock()
  
  This function closes and unlocks the opened database.
  The pointer cannot be used later.
*/

void mail_cache_db_close_unlock(const char * filename,
    struct mail_cache_db * cache_db);

/*
  mail_cache_db_put()
  
  This function will store a given key and value in the database.
*/

int mail_cache_db_put(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, const void * value, size_t value_len);

/*
  mail_cache_db_get()
  
  This function will retrieve the value corresponding to a given key
  from the database.
*/

int mail_cache_db_get(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, void ** pvalue, size_t * pvalue_len);

/*
  mail_cache_db_get_size()
  
  This function will retrieve the size of the value corresponding
  to a given key from the database.
*/

int mail_cache_db_get_size(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, size_t * pvalue_len);

/*
  mail_cache_db_del()

  This function will delete the given key and the corresponding value
  from the database.
*/

int mail_cache_db_del(struct mail_cache_db * cache_db,
    const void * key, size_t key_len);

/*
  mail_cache_clean_up()

  This function will delete the key all the key/value pairs of the
  database file which key does not exist in the given hash.
*/

int mail_cache_db_clean_up(struct mail_cache_db * cache_db,
    chash * exist);

/*
  mail_cache_db_get_keys()

  This function will get all keys of the database and will
  store them to the given chash.
*/

int mail_cache_db_get_keys(struct mail_cache_db * cache_db,
    chash * keys);

#ifdef __cplusplus
}
#endif

#endif
