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
 * $Id: mail_cache_db.c,v 1.20 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mail_cache_db.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "libetpan-config.h"

#include "maillock.h"

#if DBVERS >= 1
#include <db.h>
#endif

#if DBVERS >= 1
static struct mail_cache_db * mail_cache_db_new(DB * db)
{
  struct mail_cache_db * cache_db;
  
  cache_db = malloc(sizeof(* cache_db));
  if (cache_db == NULL)
    return NULL;
  cache_db->internal_database = db;
  
  return cache_db;
}

static void mail_cache_db_free(struct mail_cache_db * cache_db)
{
  free(cache_db);
}
#endif

int mail_cache_db_open(const char * filename,
    struct mail_cache_db ** pcache_db)
{
#if DBVERS >= 1
  DB * dbp;
#if DBVERS > 1
  int r;
#endif
  struct mail_cache_db * cache_db;

#if DB_VERSION_MAJOR >= 3
  r = db_create(&dbp, NULL, 0);
  if (r != 0)
    goto err;

#if (DB_VERSION_MAJOR >= 4) && ((DB_VERSION_MAJOR > 4) || (DB_VERSION_MINOR >= 1))
  r = dbp->open(dbp, NULL, filename, NULL, DB_BTREE, DB_CREATE,
      S_IRUSR | S_IWUSR);
#else
  r = dbp->open(dbp, filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR);
#endif
  if (r != 0)
    goto close_db;
#else
#if DBVERS > 1  
  r = db_open(filename, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR,
      NULL, NULL, &dbp);
  if (r != 0)
    goto err;
#elif DBVERS == 1
  dbp = dbopen(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, DB_BTREE, NULL);
  if (dbp == NULL)
    goto err;
#else
  goto err;
#endif  
#endif

  cache_db = mail_cache_db_new(dbp);
  if (cache_db == NULL)
    goto close_db;
  
  * pcache_db = cache_db;

  return 0;

 close_db:
#if DBVERS > 1  
  dbp->close(dbp, 0);
#elif DBVERS == 1
  dbp->close(dbp);
#endif
 err:
  return -1;
#else
  return -1;
#endif
}

void mail_cache_db_close(struct mail_cache_db * cache_db)
{
#if DBVERS >= 1
  DB * dbp;

  dbp = cache_db->internal_database;

#if DBVERS > 1  
  dbp->close(cache_db->internal_database, 0);
#elif DBVERS == 1
  dbp->close(cache_db->internal_database);
#endif
  
  mail_cache_db_free(cache_db);
#endif
}

int mail_cache_db_open_lock(const char * filename,
    struct mail_cache_db ** pcache_db)
{
  int r;
  struct mail_cache_db * cache_db;
  
  r = maillock_write_lock(filename, -1);
  if (r < 0)
    goto err;

  r = mail_cache_db_open(filename, &cache_db);
  if (r < 0)
    goto unlock;
  
  * pcache_db = cache_db;

  return 0;

 unlock:
  maillock_write_unlock(filename, -1);
 err:
  return -1;
}

void mail_cache_db_close_unlock(const char * filename,
    struct mail_cache_db * cache_db)
{
  mail_cache_db_close(cache_db);
  maillock_write_unlock(filename, -1);
}


int mail_cache_db_put(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, const void * value, size_t value_len)
{
#if DBVERS >= 1
  int r;
  DBT db_key;
  DBT db_data;
  DB * dbp;
  
  dbp = cache_db->internal_database;

  memset(&db_key, 0, sizeof(db_key));
  memset(&db_data, 0, sizeof(db_data));
  db_key.data = (void *) key;
  db_key.size = key_len;
  db_data.data = (void *) value;
  db_data.size = value_len;
  
#if DBVERS > 1  
  r = dbp->put(dbp, NULL, &db_key, &db_data, 0);
#elif DBVERS == 1
  r = dbp->put(dbp, &db_key, &db_data, 0);
#else
  r = -1;
#endif
  if (r != 0)
    return -1;
  
  return 0;
#else
  return -1;
#endif
}

int mail_cache_db_get(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, void ** pvalue, size_t * pvalue_len)
{
#if DBVERS >= 1
  int r;
  DBT db_key;
  DBT db_data;
  DB * dbp;
  
  dbp = cache_db->internal_database;
  
  memset(&db_key, 0, sizeof(db_key));
  memset(&db_data, 0, sizeof(db_data));
  db_key.data = (void *) key;
  db_key.size = key_len;
  
#if DBVERS > 1  
  r = dbp->get(dbp, NULL, &db_key, &db_data, 0);
#elif DBVERS == 1
  r = dbp->get(dbp, &db_key, &db_data, 0);
#else
  r = -1;
#endif
  
  if (r != 0)
    return -1;
  
  * pvalue = db_data.data;
  * pvalue_len = db_data.size;
  
  return 0;
#else
  return -1;
#endif
}

int mail_cache_db_del(struct mail_cache_db * cache_db,
    const void * key, size_t key_len)
{
#if DBVERS >= 1
  int r;
  DBT db_key;
  DB * dbp;
  
  dbp = cache_db->internal_database;
  
  memset(&db_key, 0, sizeof(db_key));
  db_key.data = (void *) key;
  db_key.size = key_len;
  
#if DBVERS > 1  
  r = dbp->del(dbp, NULL, &db_key, 0);
#elif DBVERS == 1
  r = dbp->del(dbp, &db_key, 0);
#else
  r = -1;
#endif
  if (r != 0)
    return -1;
  
  return 0;
#else
  return -1;
#endif
}

#if DBVERS > 1  
int mail_cache_db_clean_up(struct mail_cache_db * cache_db,
    chash * exist)
{
  DB * dbp;
  int r;
  DBC * dbcp;
  DBT db_key;
  DBT db_data;
  
  dbp = cache_db->internal_database;
 
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
  r = dbp->cursor(dbp, NULL, &dbcp);
#else
  r = dbp->cursor(dbp, NULL, &dbcp, 0);
#endif  
  if (r != 0)
    return -1;
  
  memset(&db_key, 0, sizeof(db_key));
  memset(&db_data, 0, sizeof(db_data));
  
  while (1) {
    chashdatum hash_key;
    chashdatum hash_data;
    
    r = dbcp->c_get(dbcp, &db_key, &db_data, DB_NEXT);
    if (r != 0)
      break;
    
    hash_key.data = db_key.data;
    hash_key.len = db_key.size;

    r = chash_get(exist, &hash_key, &hash_data);
    if (r < 0) {
      r = dbcp->c_del(dbcp, 0);
      if (r != 0)
        return -1;
    }
  }
  
  r = dbcp->c_close(dbcp);
  if (r != 0)
    return -1;
  
  return 0;
}
#elif DBVERS == 1
int mail_cache_db_clean_up(struct mail_cache_db * cache_db,
    chash * exist)
{
  DB * dbp;
  int r;
  DBT db_key;
  DBT db_data;
  
  dbp = cache_db->internal_database;
  
  r = dbp->seq(dbp, &db_key, &db_data, R_FIRST);
  if (r == -1)
    return -1;
  
  while (r == 0) {
    chashdatum hash_key;
    chashdatum hash_data;
    
    hash_key.data = db_key.data;
    hash_key.len = db_key.size;

    r = chash_get(exist, &hash_key, &hash_data);
    if (r < 0) {
      r = dbp->del(dbp, &db_key, 0);
      if (r != 0)
        return -1;
    }
    
    r = dbp->seq(dbp, &db_key, &db_data, R_NEXT);
    if (r < 0)
      return -1;
  }
  
  return 0;
}
#else
int mail_cache_db_clean_up(struct mail_cache_db * cache_db,
    chash * exist)
{
  return -1;
}
#endif

int mail_cache_db_get_size(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, size_t * pvalue_len)
{
#if DBVERS >= 1
  int r;
  DBT db_key;
  DBT db_data;
  DB * dbp;
  
  dbp = cache_db->internal_database;
  
  memset(&db_key, 0, sizeof(db_key));
  memset(&db_data, 0, sizeof(db_data));
  db_key.data = (void *) key;
  db_key.size = key_len;
#if DBVERS > 1  
  db_data.flags = DB_DBT_USERMEM;
  db_data.ulen = 0;
#endif
  
#if DBVERS > 1  
  r = dbp->get(dbp, NULL, &db_key, &db_data, 0);
#elif DBVERS == 1
  r = dbp->get(dbp, &db_key, &db_data, 0);
#else
  r = -1;
#endif
  
  if (r != 0)
    return -1;
  
  * pvalue_len = db_data.size;
  
  return 0;
#else
  return -1;
#endif
}

#if DBVERS > 1  
int mail_cache_db_get_keys(struct mail_cache_db * cache_db,
    chash * keys)
{
  DB * dbp;
  int r;
  DBC * dbcp;
  DBT db_key;
  DBT db_data;
  
  dbp = cache_db->internal_database;
  
  r = dbp->cursor(dbp, NULL, &dbcp, 0);
  if (r != 0)
    return -1;
  
  memset(&db_key, 0, sizeof(db_key));
  memset(&db_data, 0, sizeof(db_data));
  
  while (1) {
    chashdatum hash_key;
    chashdatum hash_data;
    
    r = dbcp->c_get(dbcp, &db_key, &db_data, DB_NEXT);
    if (r != 0)
      break;
    
    hash_key.data = db_key.data;
    hash_key.len = db_key.size;
    hash_data.data = NULL;
    hash_data.len = 0;
    
    r = chash_set(keys, &hash_key, &hash_data, NULL);
    if (r < 0) {
      return -1;
    }
  }
  
  r = dbcp->c_close(dbcp);
  if (r != 0)
    return -1;
  
  return 0;
}
#elif DBVERS == 1
int mail_cache_db_get_keys(struct mail_cache_db * cache_db,
    chash * keys)
{
  DB * dbp;
  int r;
  DBT db_key;
  DBT db_data;
  
  dbp = cache_db->internal_database;
  
  r = dbp->seq(dbp, &db_key, &db_data, R_FIRST);
  if (r == -1)
    return -1;
  
  while (r == 0) {
    chashdatum hash_key;
    chashdatum hash_data;
    
    hash_key.data = db_key.data;
    hash_key.len = db_key.size;
    hash_data.data = NULL;
    hash_data.len = 0;
    
    r = chash_set(keys, &hash_key, &hash_data, NULL);
    if (r < 0) {
      return -1;
    }
    
    r = dbp->seq(dbp, &db_key, &db_data, R_NEXT);
    if (r < 0)
      return -1;
  }
  
  return 0;
}
#else
int mail_cache_db_get_keys(struct mail_cache_db * cache_db,
    chash * keys)
{
  return -1;
}
#endif
