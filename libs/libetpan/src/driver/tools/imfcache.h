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
 * $Id: imfcache.h,v 1.9 2004/11/21 21:53:35 hoa Exp $
 */

#ifndef IMFCACHE_H

#define IMFCACHE_H

#include <stdio.h>
#include "mailimf.h"
#include "maildriver_types.h"
#include "mmapstring.h"

#ifdef __cplusplus
extern "C" {
#endif

int mail_serialize_clear(MMAPString * mmapstr, size_t * index);

int mail_serialize_write(MMAPString * mmapstr, size_t * index,
			 char * buf, size_t size);

int mail_serialize_read(MMAPString * mmapstr, size_t * index,
			char * buf, size_t size);

int mailimf_cache_int_write(MMAPString * mmapstr, size_t * index,
			    uint32_t value);
int mailimf_cache_string_write(MMAPString * mmapstr, size_t * index,
			       char * str, size_t length);
int mailimf_cache_int_read(MMAPString * mmapstr, size_t * index,
			   uint32_t * result);
int mailimf_cache_string_read(MMAPString * mmapstr, size_t * index,
			      char ** result);

int mailimf_cache_fields_write(MMAPString * mmapstr, size_t * index,
			       struct mailimf_fields * fields);
int mailimf_cache_fields_read(MMAPString * mmapstr, size_t * index,
			      struct mailimf_fields ** result);

#ifdef __cplusplus
}
#endif

#endif
