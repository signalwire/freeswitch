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
 * $Id: mailimf_write_file.h,v 1.3 2005/06/01 12:22:18 smarinier Exp $
 */

#ifndef MAILIMF_WRITE_H

#define MAILIMF_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <libetpan/mailimf_types.h>

#define MAILIMF_WRITE_COMPATIBILITY

/*
  mailimf_string_write_file writes a string to a given stream
  
  @param f is the stream
  @param col (* col) is the column number where we will start to
    write the text, the ending column will be stored in (* col)
  @param str is the string to write
*/
LIBETPAN_EXPORT
int mailimf_string_write_file(FILE * f, int * col,
    const char * str, size_t length);


/*
  mailimf_fields_write_file writes the fields to a given stream
  
  @param f is the stream
  @param col (* col) is the column number where we will start to
    write the text, the ending column will be stored in (* col)
  @param fields is the fields to write
*/
LIBETPAN_EXPORT
int mailimf_fields_write_file(FILE * f, int * col,
    struct mailimf_fields * fields);


/*
  mailimf_envelope_fields_write_file writes only some fields to a given stream
  
  @param f is the stream
  @param col (* col) is the column number where we will start to
    write the text, the ending column will be stored in (* col)
  @param fields is the fields to write
*/
LIBETPAN_EXPORT
int mailimf_envelope_fields_write_file(FILE * f, int * col,
    struct mailimf_fields * fields);


/*
  mailimf_field_write_file writes a field to a given stream
  
  @param f is the stream
  @param col (* col) is the column number where we will start to
    write the text, the ending column will be stored in (* col)
  @param field is the field to write
*/
LIBETPAN_EXPORT
int mailimf_field_write_file(FILE * f, int * col,
    struct mailimf_field * field);

/*
  mailimf_quoted_string_write_file writes a string that is quoted
  to a given stream
  
  @param f is the stream
  @param col (* col) is the column number where we will start to
    write the text, the ending column will be stored in (* col)
  @param string is the string to quote and write
*/
LIBETPAN_EXPORT
int mailimf_quoted_string_write_file(FILE * f, int * col,
    const char * string, size_t len);

LIBETPAN_EXPORT
int mailimf_address_list_write_file(FILE * f, int * col,
    struct mailimf_address_list * addr_list);

LIBETPAN_EXPORT
int mailimf_mailbox_list_write_file(FILE * f, int * col,
    struct mailimf_mailbox_list * mb_list);

/*
  mailimf_header_string_write_file writes a header value and fold the header
    if needed.
  
  @param f is the stream
  @param col (* col) is the column number where we will start to
    write the text, the ending column will be stored in (* col)
  @param str is the string to write
*/

LIBETPAN_EXPORT
int mailimf_header_string_write_file(FILE * f, int * col,
    const char * str, size_t length);



/* binary compatibility with 0.34 - begin */

#ifdef MAILIMF_WRITE_COMPATIBILITY
LIBETPAN_EXPORT
int mailimf_string_write(FILE * f, int * col,
    const char * str, size_t length);

LIBETPAN_EXPORT
int mailimf_fields_write(FILE * f, int * col,
    struct mailimf_fields * fields);

LIBETPAN_EXPORT
int mailimf_envelope_fields_write(FILE * f, int * col,
    struct mailimf_fields * fields);

LIBETPAN_EXPORT
int mailimf_field_write(FILE * f, int * col,
    struct mailimf_field * field);

LIBETPAN_EXPORT
int mailimf_quoted_string_write(FILE * f, int * col,
    const char * string, size_t len);

LIBETPAN_EXPORT
int mailimf_address_list_write(FILE * f, int * col,
    struct mailimf_address_list * addr_list);

LIBETPAN_EXPORT
int mailimf_mailbox_list_write(FILE * f, int * col,
    struct mailimf_mailbox_list * mb_list);

LIBETPAN_EXPORT
int mailimf_header_string_write(FILE * f, int * col,
    const char * str, size_t length);
#endif

/* binary compatibility with 0.34 - end */


#ifdef __cplusplus
}
#endif

#endif
