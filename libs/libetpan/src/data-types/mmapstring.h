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
 * $Id: mmapstring.h,v 1.13 2006/03/22 08:10:47 hoa Exp $
 */

#ifndef __MMAP_STRING_H__

#define __MMAP_STRING_H__

#include <sys/types.h>

#ifndef LIBETPAN_CONFIG_H
#	include <libetpan/libetpan-config.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
#define TMPDIR "/tmp"
*/

typedef struct _MMAPString MMAPString;

struct _MMAPString
{
  char * str;
  size_t len;    
  size_t allocated_len;
  int fd;
  size_t mmapped_size;
  /*
  char * old_non_mmapped_str;
  */
};

/* configure location of mmaped files */

void mmap_string_set_tmpdir(char * directory);

/* Strings
 */
LIBETPAN_EXPORT
MMAPString * mmap_string_new (const char * init);

LIBETPAN_EXPORT
MMAPString * mmap_string_new_len (const char * init,
				  size_t len);   

LIBETPAN_EXPORT
MMAPString * mmap_string_sized_new (size_t dfl_size);

LIBETPAN_EXPORT
void mmap_string_free (MMAPString * string);

LIBETPAN_EXPORT
MMAPString * mmap_string_assign (MMAPString * string,
				 const char * rval);

LIBETPAN_EXPORT
MMAPString * mmap_string_truncate (MMAPString *string,
				   size_t len);    

LIBETPAN_EXPORT
MMAPString * mmap_string_set_size (MMAPString * string,
				   size_t len);

LIBETPAN_EXPORT
MMAPString * mmap_string_insert_len (MMAPString * string,
				     size_t pos,   
				     const char * val,
				     size_t len);  

LIBETPAN_EXPORT
MMAPString * mmap_string_append (MMAPString * string,
				 const char * val);

LIBETPAN_EXPORT
MMAPString * mmap_string_append_len (MMAPString * string,
				     const char * val,
				     size_t len);  

LIBETPAN_EXPORT
MMAPString * mmap_string_append_c (MMAPString * string,
				   char c);

LIBETPAN_EXPORT
MMAPString * mmap_string_prepend (MMAPString * string,
				  const char * val);

LIBETPAN_EXPORT
MMAPString * mmap_string_prepend_c (MMAPString * string,
				    char c);

LIBETPAN_EXPORT
MMAPString * mmap_string_prepend_len (MMAPString * string,
				      const char * val,
				      size_t len);  

LIBETPAN_EXPORT
MMAPString * mmap_string_insert (MMAPString * string,
				 size_t pos,
				 const char * val);

LIBETPAN_EXPORT
MMAPString * mmap_string_insert_c (MMAPString *string,
				   size_t pos,
				   char c);

LIBETPAN_EXPORT
MMAPString * mmap_string_erase(MMAPString * string,
			       size_t pos,    
			       size_t len);   

void mmap_string_set_ceil(size_t ceil);

int mmap_string_ref(MMAPString * string);
int mmap_string_unref(char * str);

#ifdef __cplusplus
}
#endif


#endif /* __MMAP_STRING_H__ */
