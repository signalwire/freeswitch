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
 * $Id: mmapstring.c,v 1.22 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mmapstring.h"

#include "chash.h"

#include <stdlib.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#else
#	include <unistd.h>
#	include <sys/mman.h>
#endif
#include <string.h>
#ifdef LIBETPAN_REENTRANT
#	ifndef WIN32
#		include <pthread.h>
#	endif
#endif

#include "libetpan-config.h"

#define MMAPSTRING_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MMAPSTRING_MIN(a, b) ((a) < (b) ? (a) : (b))

#define MMAP_STRING_DEFAULT_CEIL (8 * 1024 * 1024)

#define DEFAULT_TMP_PATH "/tmp"

static char tmpdir[PATH_MAX] = DEFAULT_TMP_PATH;

static size_t mmap_string_ceil = MMAP_STRING_DEFAULT_CEIL;

/* MMAPString references */

#ifdef LIBETPAN_REENTRANT
#	ifdef WIN32

		static	CRITICAL_SECTION mmapstring_lock ;
		static  CRITICAL_SECTION* _get_critical() {
					InitializeCriticalSection(&mmapstring_lock);
					return &mmapstring_lock;
				}
		static	int	_lock_initialized = 0;
		static void __mutex_lock(CRITICAL_SECTION* section) {
			if (!_lock_initialized)
				InitializeCriticalSection(section);
		    EnterCriticalSection( section);
		}
		static void __mutex_unlock(CRITICAL_SECTION* section) {
			if (!_lock_initialized)
				InitializeCriticalSection(section);
			LeaveCriticalSection( section);
		}
#		define MUTEX_LOCK(x) __mutex_lock(x)
#		define MUTEX_UNLOCK(x) __mutex_unlock(x)
#	else
		static pthread_mutex_t mmapstring_lock = PTHREAD_MUTEX_INITIALIZER;
#		define MUTEX_LOCK(x) pthread_mutex_lock(x)
#		define MUTEX_UNLOCK(x) pthread_mutex_unlock(x)
#	endif
#else
#	define MUTEX_LOCK(x) 
#	define MUTEX_UNLOCK(x)
#endif
static chash * mmapstring_hashtable = NULL;

static void mmapstring_hashtable_init(void)
{
  mmapstring_hashtable = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
}

void mmap_string_set_tmpdir(char * directory)
{
  strncpy(tmpdir, directory, PATH_MAX);
  tmpdir[PATH_MAX - 1] = 0;
}


int mmap_string_ref(MMAPString * string)
{
  chash * ht;
  int r;
  chashdatum key;
  chashdatum data;
  
  MUTEX_LOCK(&mmapstring_lock);

  if (mmapstring_hashtable == NULL) {
    mmapstring_hashtable_init();
  }
  ht = mmapstring_hashtable;
  
  if (ht == NULL) {

	MUTEX_UNLOCK(&mmapstring_lock);
    return -1;
  }
  
  key.data = &string->str;
  key.len = sizeof(string->str);
  data.data = string;
  data.len = 0;
  
  r = chash_set(mmapstring_hashtable, &key, &data, NULL);
 
  MUTEX_UNLOCK(&mmapstring_lock);
  
  if (r < 0)
    return r;

  return 0;
}

int mmap_string_unref(char * str)
{
  MMAPString * string;
  chash * ht;
  chashdatum key;
  chashdatum data;
  int r;

  MUTEX_LOCK(&mmapstring_lock);

  ht = mmapstring_hashtable;
  
  if (ht == NULL) {

	MUTEX_UNLOCK(&mmapstring_lock);
    return -1;
  }
  
  key.data = &str;
  key.len = sizeof(str);

  r = chash_get(ht, &key, &data);
  if (r < 0)
    string = NULL;
  else
    string = data.data;
  
  if (string != NULL) {
    chash_delete(ht, &key, NULL);
    if (chash_count(ht) == 0) {
      chash_free(ht);
      mmapstring_hashtable = NULL;
    }
  }
  
  MUTEX_UNLOCK(&mmapstring_lock);

  if (string != NULL) {
    mmap_string_free(string);
    return 0;
  }
  else
    return -1;
}



/* MMAPString */

#define MY_MAXSIZE ((size_t) -1)

static inline size_t
nearest_power (size_t base, size_t num)    
{
  if (num > MY_MAXSIZE / 2) {
    return MY_MAXSIZE;
  }
  else {
    size_t n = base;
    
    while (n < num)
      n <<= 1;
    
    return n;
  }
}

void mmap_string_set_ceil(size_t ceil)
{
  mmap_string_ceil = ceil;
}

/* Strings.
 */

/* SEB */
#ifndef MMAP_UNAVAILABLE
static MMAPString * mmap_string_realloc_file(MMAPString * string)
{
  char * data;

  if (string->fd == -1) {
    char tmpfilename[PATH_MAX];
    int fd;

    * tmpfilename = 0;
    strcat(tmpfilename, tmpdir);
    strcat(tmpfilename, "/libetpan-mmapstring-XXXXXX");

    fd = mkstemp(tmpfilename);
    if (fd == -1)
      return NULL;

    if (unlink(tmpfilename) == -1) {
      close(fd);
      return NULL;
    }
    
    if (ftruncate(fd, string->allocated_len) == -1) {
      close(fd);
      return NULL;
    }

    data = mmap(NULL, string->allocated_len, PROT_WRITE | PROT_READ,
		MAP_SHARED, fd, 0);

    if (data == (char *)MAP_FAILED) {
      close(fd);
      return NULL;
    }

    if (string->str != NULL)
      memcpy(data, string->str, string->len + 1);

    string->fd = fd;
    string->mmapped_size = string->allocated_len;
    free(string->str);
    string->str = data;
  }
  else {
    if (munmap(string->str, string->mmapped_size) == -1)
      return NULL;

    if (ftruncate(string->fd, string->allocated_len) == -1)
      return NULL;
    
    data = mmap(NULL, string->allocated_len, PROT_WRITE | PROT_READ,
		MAP_SHARED, string->fd, 0);

    if (data == (char *)MAP_FAILED)
      return NULL;

    string->mmapped_size = string->allocated_len;
    string->str = data;
  }
  
  return string;
}
#endif

static MMAPString * mmap_string_realloc_memory(MMAPString * string)
{
  char * tmp;

  tmp =  realloc (string->str, string->allocated_len);
  
  if (tmp == NULL)
    string = NULL;
  else
    string->str = tmp;

  return string;
}

static MMAPString *
mmap_string_maybe_expand (MMAPString* string,
			  size_t len) 
{
  if (string->len + len >= string->allocated_len)
    {
      size_t old_size;
      MMAPString * newstring;

      old_size = string->allocated_len;

      string->allocated_len = nearest_power (1, string->len + len + 1);
      
#ifndef MMAP_UNAVAILABLE
      if (string->allocated_len > mmap_string_ceil)
	newstring = mmap_string_realloc_file(string);
      else {
#endif
	newstring = mmap_string_realloc_memory(string);
#ifndef MMAP_UNAVAILABLE
	if (newstring == NULL)
	  newstring = mmap_string_realloc_file(string);
      }
#endif

      if (newstring == NULL)
	string->allocated_len = old_size;
      
      return newstring;
    }

  return string;
}

MMAPString*
mmap_string_sized_new (size_t dfl_size)
{
  MMAPString *string;
 
  string = malloc(sizeof(* string));
  if (string == NULL)
    return NULL;

  string->allocated_len = 0;
  string->len   = 0;
  string->str   = NULL;
  string->fd    = -1;
  string->mmapped_size = 0;

  if (mmap_string_maybe_expand (string, MMAPSTRING_MAX (dfl_size, 2)) == NULL)
    return NULL;

  string->str[0] = 0;

  return string;
}

MMAPString*
mmap_string_new (const char *init)
{
  MMAPString *string;

  string = mmap_string_sized_new (init ? strlen (init) + 2 : 2);
  if (string == NULL)
    return NULL;

  if (init)
    mmap_string_append (string, init);

  return string;
}

MMAPString*
mmap_string_new_len (const char *init,
		     size_t len)    
{
  MMAPString *string;

  if (len <= 0)
    return mmap_string_new ("");
  else
    {
      string = mmap_string_sized_new (len);
      if (string == NULL)
        return string;

      if (init)
        mmap_string_append_len (string, init, len);
      
      return string;
    }
}

void
mmap_string_free (MMAPString *string)
{
  if (string == NULL)
    return;

/* SEB */
#ifndef MMAP_UNAVAILABLE
  if (string->fd != -1) {
    munmap(string->str, string->mmapped_size);
    close(string->fd);
  }
  else
#endif
  {
    free (string->str);
  }
  free(string);
}

MMAPString*
mmap_string_assign (MMAPString     *string,
		    const char *rval)
{
  mmap_string_truncate (string, 0);
  if (mmap_string_append (string, rval) == NULL)
    return NULL;

  return string;
}

MMAPString*
mmap_string_truncate (MMAPString *string,
		      size_t    len)    
{
  string->len = MMAPSTRING_MIN (len, string->len);
  string->str[string->len] = 0;

  return string;
}

/**
 * mmap_string_set_size:
 * @string: a #MMAPString
 * @len: the new length
 * 
 * Sets the length of a #MMAPString. If the length is less than
 * the current length, the string will be truncated. If the
 * length is greater than the current length, the contents
 * of the newly added area are undefined. (However, as
 * always, string->str[string->len] will be a nul byte.) 
 * 
 * Return value: @string
 **/
MMAPString*
mmap_string_set_size (MMAPString *string,
		      size_t    len)    
{
  if (len >= string->allocated_len)
    if (mmap_string_maybe_expand (string, len - string->len) == NULL)
      return NULL;
  
  string->len = len;
  string->str[len] = 0;

  return string;
}

/*
static int in_mapped_zone(MMAPString * string, char * val)
{
  return (val >= string->str) && (val < string->str + string->mmapped_size);
}
*/

MMAPString*
mmap_string_insert_len (MMAPString     *string,
			size_t       pos,    
			const char *val,
			size_t       len)    
{
  if (mmap_string_maybe_expand (string, len) == NULL)
    return NULL;
    
  if (pos < string->len)
    memmove (string->str + pos + len, string->str + pos, string->len - pos);

  /* insert the new string */
  memmove (string->str + pos, val, len);

  string->len += len;

  string->str[string->len] = 0;

  return string;
}

MMAPString*
mmap_string_append (MMAPString     *string,
		    const char *val)
{  
  return mmap_string_insert_len (string, string->len, val, strlen(val));
}

MMAPString*
mmap_string_append_len (MMAPString	 *string,
			const char *val,
			size_t       len)    
{
  return mmap_string_insert_len (string, string->len, val, len);
}

MMAPString*
mmap_string_append_c (MMAPString *string,
		      char    c)
{
  return mmap_string_insert_c (string, string->len, c);
}

MMAPString*
mmap_string_prepend (MMAPString     *string,
		     const char *val)
{
  return mmap_string_insert_len (string, 0, val, strlen(val));
}

MMAPString*
mmap_string_prepend_len (MMAPString	  *string,
			 const char *val,
			 size_t       len)    
{
  return mmap_string_insert_len (string, 0, val, len);
}

MMAPString*
mmap_string_prepend_c (MMAPString *string,
		       char    c)
{  
  return mmap_string_insert_c (string, 0, c);
}

MMAPString*
mmap_string_insert (MMAPString     *string,
		    size_t       pos,    
		    const char *val)
{
  return mmap_string_insert_len (string, pos, val, strlen(val));
}

MMAPString*
mmap_string_insert_c (MMAPString *string,
		      size_t   pos,    
		      char    c)
{
  if (mmap_string_maybe_expand (string, 1) == NULL)
    return NULL;
  
  /* If not just an append, move the old stuff */
  if (pos < string->len)
    memmove (string->str + pos + 1, string->str + pos, string->len - pos);

  string->str[pos] = c;

  string->len += 1;

  string->str[string->len] = 0;

  return string;
}

MMAPString*
mmap_string_erase (MMAPString *string,
		   size_t    pos,    
		   size_t    len)    
{
  if ((pos + len) < string->len)
    memmove (string->str + pos, string->str + pos + len,
	     string->len - (pos + len));
  
  string->len -= len;
  
  string->str[string->len] = 0;

  return string;
}
