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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_extension.h"

#include <stdlib.h>
#include <stdio.h>

#include "clist.h"
#include "annotatemore.h"
#include "acl.h"
#include "uidplus.h"

/*
  the list of registered extensions (struct mailimap_extension_api *)

  the list of extension is kept as a simple clist.
*/

static clist * mailimap_extension_list = NULL;

static struct mailimap_extension_api * internal_extension_list[] = {
  &mailimap_extension_annotatemore,
  &mailimap_extension_acl,
  &mailimap_extension_uidplus,
};

LIBETPAN_EXPORT
int
mailimap_extension_register(struct mailimap_extension_api * extension)
{
  if (mailimap_extension_list == NULL) {
    mailimap_extension_list = clist_new();
    if (mailimap_extension_list == NULL)
      return MAILIMAP_ERROR_MEMORY;
  }

  return clist_append(mailimap_extension_list, extension);
}

LIBETPAN_EXPORT
void
mailimap_extension_unregister_all(void)
{
  clist_free(mailimap_extension_list);
  mailimap_extension_list = NULL;
}

LIBETPAN_EXPORT
int
mailimap_extension_data_parse(int calling_parser,
        mailstream * fd, MMAPString * buffer,
        size_t * index, struct mailimap_extension_data ** result,
        size_t progr_rate,
        progress_function * progr_fun)
{
  clistiter * cur;
  int r;
  unsigned int i;
  
  for(i = 0 ; i < sizeof(internal_extension_list) / sizeof(* internal_extension_list) ; i ++) {
    struct mailimap_extension_api * ext;
    
    ext = internal_extension_list[i];
    r = ext->ext_parser(calling_parser, fd, buffer, index, result,
        progr_rate, progr_fun);
    if (r != MAILIMAP_ERROR_PARSE)
      return r;
  }
  
  if (mailimap_extension_list == NULL)
    return MAILIMAP_ERROR_PARSE;

  for (cur = clist_begin(mailimap_extension_list);
        cur != NULL; cur = clist_next(cur)) {
    struct mailimap_extension_api * ext;
    
    ext = clist_content(cur);
    r = ext->ext_parser(calling_parser, fd, buffer, index, result,
        progr_rate, progr_fun);
    if (r != MAILIMAP_ERROR_PARSE)
      return r;
  }

  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
struct mailimap_extension_data *
mailimap_extension_data_new(struct mailimap_extension_api * extension,
        int type, void * data)
{
  struct mailimap_extension_data * ext_data;

  ext_data = malloc(sizeof(* ext_data));
  if (ext_data == NULL)
    return NULL;

  ext_data->ext_extension = extension;
  ext_data->ext_type = type;
  ext_data->ext_data = data;

  return ext_data;
}

LIBETPAN_EXPORT
void
mailimap_extension_data_free(struct
        mailimap_extension_data * data)
{
  if (data == NULL)
    return;

  if (data->ext_extension != NULL)
    data->ext_extension->ext_free(data);
  else
    free(data);
}

void mailimap_extension_data_store(mailimap * session,
    struct mailimap_extension_data ** ext_data)
{
  int r;

  if (session->imap_response_info) {
    r = clist_append(session->imap_response_info->rsp_extension_list,
        * ext_data);
    if (r == 0)
	    * ext_data = NULL;
    else {
	/* TODO must handle error case */	
    }
  }
}

LIBETPAN_EXPORT
int mailimap_has_extension(mailimap * session, char * extension_name)
{
  if (session->imap_connection_info != NULL) {
    if (session->imap_connection_info->imap_capability != NULL) {
      clist * list;
      clistiter * cur;
      
      list = session->imap_connection_info->imap_capability->cap_list;
      for(cur = clist_begin(list) ; cur != NULL ; cur = clist_next(cur)) {
        struct mailimap_capability * cap;
        
        cap = clist_content(cur);
        if (cap->cap_type != MAILIMAP_CAPABILITY_NAME)
          continue;
        
        if (strcasecmp(cap->cap_data.cap_name, extension_name) == 0)
          return 1;
      }
    }
  }
  
  return 0;
}
