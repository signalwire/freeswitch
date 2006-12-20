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

#ifndef ANNOTATEMORE_TYPES_H

#define ANNOTATEMORE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailstream.h>
#include <libetpan/clist.h>

/*
   ANNOTATEMORE grammar
   see [draft-daboo-imap-annotatemore-07] for further information

   annotate-data     = "ANNOTATION" SP mailbox SP entry-list
                       ; empty string for mailbox implies
                       ; server annotation.

   att-value         = attrib SP value

   attrib            = string
                       ; dot-separated attribute name
                       ; MUST NOT contain "*" or "%"
   attrib-match      = string
                       ; dot-separated attribute name
                       ; MAY contain "*" or "%" for use as wildcards

   attribs           = attrib-match / "(" attrib-match *(SP attrib-match) ")"
                       ; attribute specifiers that can include wildcards

   command-auth      /= setannotation / getannotation
                       ; adds to original IMAP command

   entries           = entry-match / "(" entry-match *(SP entry-match) ")"
                       ; entry specifiers that can include wildcards

   entry             = string
                       ; slash-separated path to entry
                       ; MUST NOT contain "*" or "%"

   entry-att         = entry SP "(" att-value *(SP att-value) ")"

   entry-list        = entry-att *(SP entry-att) /
                       "(" entry *(SP entry) ")"
                       ; entry attribute-value pairs list for
                       ; GETANNOTATION response, or
                       ; parenthesised entry list for unsolicited
                       ; notification of annotation changes

   entry-match       = string
                       ; slash-separated path to entry
                       ; MAY contain "*" or "%" for use as wildcards

   getannotation     = "GETANNOTATION" SP list-mailbox SP entries SP attribs
                       ; empty string for list-mailbox implies
                       ; server annotation.

   response-data     /= "*" SP annotate-data CRLF
                       ; adds to original IMAP data responses

   resp-text-code    =/ "ANNOTATEMORE" SP "TOOBIG" /
                        "ANNOTATEMORE" SP "TOOMANY"
                       ; new response codes for SETANNOTATION failures

   setannotation     = "SETANNOTATION" SP list-mailbox SP setentryatt
                       ; empty string for list-mailbox implies
                       ; server annotation.

   setentryatt       = entry-att / "(" entry-att *(SP entry-att) ")"

   value             = nstring
*/

/*
  only need to recognize types that can be "embedded" into main
  IMAPrev1 types.
*/
enum {
  MAILIMAP_ANNOTATEMORE_TYPE_ANNOTATE_DATA,          /* child of response-data   */
  MAILIMAP_ANNOTATEMORE_TYPE_RESP_TEXT_CODE,         /* child of resp-text-code  */
};

/*
  error codes for annotatemore.
*/
enum {
  MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_UNSPECIFIED, /* unspecified response   */
  MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_TOOBIG,      /* annotation too big     */
  MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_TOOMANY,     /* too many annotations   */
};

void mailimap_annotatemore_attrib_free(char * attrib);

void mailimap_annotatemore_value_free(char * value);

void mailimap_annotatemore_entry_free(char * entry);

struct mailimap_annotatemore_att_value  {
  char * attrib;
  char * value;
};

LIBETPAN_EXPORT
struct mailimap_annotatemore_att_value *
mailimap_annotatemore_att_value_new(char * attrib, char * value);

void mailimap_annotatemore_att_value_free(struct
        mailimap_annotatemore_att_value * att_value);

struct mailimap_annotatemore_entry_att {
  char * entry;
  clist * att_value_list;
  /* list of (struct mailimap_annotatemore_att_value *) */
};

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att *
mailimap_annotatemore_entry_att_new(char * entry, clist * list);

LIBETPAN_EXPORT
void mailimap_annotatemore_entry_att_free(struct
        mailimap_annotatemore_entry_att * en_att);

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att *
mailimap_annotatemore_entry_att_new_empty(char * entry);

LIBETPAN_EXPORT
int mailimap_annotatemore_entry_att_add(struct
        mailimap_annotatemore_entry_att * en_att,
        struct mailimap_annotatemore_att_value * at_value);

enum {
  MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ERROR,          /* error condition */
  MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_ATT_LIST, /* entry-att-list */
  MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_LIST,     /* entry-list */
};

struct mailimap_annotatemore_entry_list {
  int en_list_type;
  clist * en_list_data;
  /* either a list of (struct annotatemore_entry_att *)
     or a list of (char *) */
};

struct mailimap_annotatemore_entry_list *
mailimap_annotatemore_entry_list_new(int type, clist * en_att_list, clist * en_list);

void mailimap_annotatemore_entry_list_free(struct
        mailimap_annotatemore_entry_list * en_list);

struct mailimap_annotatemore_annotate_data {
  char * mailbox;
  struct mailimap_annotatemore_entry_list * entry_list;
};

struct mailimap_annotatemore_annotate_data *
mailimap_annotatemore_annotate_data_new(char * mb, struct
        mailimap_annotatemore_entry_list * en_list);

LIBETPAN_EXPORT
void mailimap_annotatemore_annotate_data_free(struct
        mailimap_annotatemore_annotate_data * an_data);

struct mailimap_annotatemore_entry_match_list {
  clist * entry_match_list; /* list of (char *) */
};

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_match_list *
mailimap_annotatemore_entry_match_list_new(clist * en_list);

LIBETPAN_EXPORT
void mailimap_annotatemore_entry_match_list_free(
        struct mailimap_annotatemore_entry_match_list * en_list);

struct mailimap_annotatemore_attrib_match_list {
  clist * attrib_match_list; /* list of (char *) */
};

LIBETPAN_EXPORT
struct mailimap_annotatemore_attrib_match_list *
mailimap_annotatemore_attrib_match_list_new(clist * at_list);

LIBETPAN_EXPORT
void mailimap_annotatemore_attrib_match_list_free(
        struct mailimap_annotatemore_attrib_match_list * at_list);

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_match_list *
mailimap_annotatemore_entry_match_list_new_empty();

LIBETPAN_EXPORT
int mailimap_annotatemore_entry_match_list_add(
      struct mailimap_annotatemore_entry_match_list * en_list,
      char * entry);

LIBETPAN_EXPORT
struct mailimap_annotatemore_attrib_match_list *
mailimap_annotatemore_attrib_match_list_new_empty();

LIBETPAN_EXPORT
int mailimap_annotatemore_attrib_match_list_add(
      struct mailimap_annotatemore_attrib_match_list * at_list,
      char * attrib);

struct mailimap_annotatemore_entry_att_list {
  clist * entry_att_list; /* list of (mailimap_annotatemore_entry_att *) */
};

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att_list *
mailimap_annotatemore_entry_att_list_new(clist * en_list);

LIBETPAN_EXPORT
void mailimap_annotatemore_entry_att_list_free(
      struct mailimap_annotatemore_entry_att_list * en_list);

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att_list *
mailimap_annotatemore_entry_att_list_new_empty();

LIBETPAN_EXPORT
int mailimap_annotatemore_entry_att_list_add(
      struct mailimap_annotatemore_entry_att_list * en_list,
      struct mailimap_annotatemore_entry_att * en_att);

void
mailimap_annotatemore_free(struct mailimap_extension_data * ext_data);

#ifdef __cplusplus
}
#endif

#endif
