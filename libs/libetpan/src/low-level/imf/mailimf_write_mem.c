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
 * $Id: mailimf_write_mem.c,v 1.2 2004/11/21 21:53:37 hoa Exp $
 */

#include "mailimf_write_mem.h"
#include "mailimf_write_generic.h"

static int do_write(void * data, const char * str, size_t length)
{
  MMAPString * f;
  
  f = data;
  
  if (mmap_string_append_len(f, str, length) == NULL)
    return 0;
  else
    return length;
}

int mailimf_string_write_mem(MMAPString * f, int * col,
    const char * str, size_t length)
{
  return mailimf_string_write_driver(do_write, f, col, str, length);
}

int mailimf_fields_write_mem(MMAPString * f, int * col,
    struct mailimf_fields * fields)
{
  return mailimf_fields_write_driver(do_write, f, col, fields);
}

int mailimf_envelope_fields_write_mem(MMAPString * f, int * col,
    struct mailimf_fields * fields)
{
  return mailimf_envelope_fields_write_driver(do_write, f, col, fields);
}

int mailimf_field_write_mem(MMAPString * f, int * col,
    struct mailimf_field * field)
{
  return mailimf_field_write_driver(do_write, f, col, field);
}

int mailimf_quoted_string_write_mem(MMAPString * f, int * col,
    const char * string, size_t len)
{
  return mailimf_quoted_string_write_driver(do_write, f, col, string, len);
}

int mailimf_address_list_write_mem(MMAPString * f, int * col,
    struct mailimf_address_list * addr_list)
{
  return mailimf_address_list_write_driver(do_write, f, col, addr_list);
}

int mailimf_mailbox_list_write_mem(MMAPString * f, int * col,
    struct mailimf_mailbox_list * mb_list)
{
  return mailimf_mailbox_list_write_driver(do_write, f, col, mb_list);
}

int mailimf_header_string_write_mem(MMAPString * f, int * col,
    const char * str, size_t length)
{
  return mailimf_header_string_write_driver(do_write, f, col, str, length);
}

