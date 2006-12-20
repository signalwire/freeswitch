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
 * $Id: mailimap_sender.c,v 1.29 2006/10/20 00:13:30 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstream.h"
#include "mailimap_keywords.h"
#include "mailimap_sender.h"
#include "clist.h"
#include "mail.h"
#include <string.h>

#include <stdio.h>
#include <ctype.h>

/*
  TODO :
  implement progression for literal
*/

/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */




static int mailimap_atom_send(mailstream * fd, const char * atom);

static int mailimap_auth_type_send(mailstream * fd, const char * auth_type);

static int mailimap_base64_send(mailstream * fd, const char * base64);


static int mailimap_date_send(mailstream * fd,
			      struct mailimap_date * date);

static int mailimap_date_day_send(mailstream * fd, int day);

static int mailimap_date_month_send(mailstream * fd, int month);


/*
static gboolean mailimap_date_text_send(mailstream * fd,
					struct mailimap_date_text * date_text);
*/


static int mailimap_date_year_send(mailstream *fd, int year);

static int
mailimap_date_time_send(mailstream * fd,
			struct mailimap_date_time * date_time);

static int mailimap_digit_send(mailstream * fd, int digit);



static int
mailimap_fetch_type_send(mailstream * fd,
			 struct mailimap_fetch_type * fetch_type);


static int mailimap_fetch_att_send(mailstream * fd,
				   struct mailimap_fetch_att * fetch_att);


static int mailimap_flag_send(mailstream * fd,
			      struct mailimap_flag * flag);


static int mailimap_flag_extension_send(mailstream * fd,
					const char * flag_extension);


static int mailimap_flag_keyword_send(mailstream * fd,
				      const char * flag_keyword);


static int mailimap_flag_list_send(mailstream * fd,
				   struct mailimap_flag_list * flag_list);



static int mailimap_header_fld_name_send(mailstream * fd, const char * header);


static int
mailimap_header_list_send(mailstream * fd,
			  struct mailimap_header_list * header_list);

static int mailimap_number_send(mailstream * fd, uint32_t number);

static int mailimap_password_send(mailstream * fd, const char * pass);

static int mailimap_quoted_char_send(mailstream * fd, char ch);


static int mailimap_search_key_send(mailstream * fd,
				    struct mailimap_search_key * key);

static int
mailimap_section_send(mailstream * fd,
		      struct mailimap_section * section);

static int
mailimap_section_msgtext_send(mailstream * fd,
			      struct mailimap_section_msgtext *
			      section_msgtext);


static int
mailimap_section_part_send(mailstream * fd,
			   struct mailimap_section_part * section);


static int
mailimap_section_spec_send(mailstream * fd,
			   struct mailimap_section_spec * section_spec);


static int
mailimap_section_text_send(mailstream * fd,
			   struct mailimap_section_text * section_text);


static int
mailimap_sequence_num_send(mailstream * fd, uint32_t sequence_num);


static int mailimap_set_item_send(mailstream * fd,
				  struct mailimap_set_item * item);


static int mailimap_status_att_send(mailstream * fd, int * status_att);



static int
mailimap_store_att_flags_send(mailstream * fd,
			      struct mailimap_store_att_flags * store_flags);


static int mailimap_userid_send(mailstream * fd, const char * user);














/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */





static inline int mailimap_sized_token_send(mailstream * fd, const char * atom,
				     size_t len)
{
  if (mailstream_send_data_crlf(fd, atom, len, 0, NULL) == -1)
    return MAILIMAP_ERROR_STREAM;

  return MAILIMAP_NO_ERROR;
}

int mailimap_token_send(mailstream * fd, const char * atom)
{
  return mailimap_sized_token_send(fd, atom, strlen(atom));
}

int mailimap_char_send(mailstream * fd, char ch)
{
  if (mailstream_write(fd, &ch, 1) == -1)
    return MAILIMAP_ERROR_STREAM;

  return MAILIMAP_NO_ERROR;
}

static int
mailimap_struct_list_send(mailstream * fd, clist * list,
			  char symbol,
			  mailimap_struct_sender * sender)
{
  clistiter * cur;
  void * elt;
  int r;

  cur = clist_begin(list);

  if (cur == NULL)
    return MAILIMAP_NO_ERROR;

  elt = clist_content(cur);
  r = (* sender)(fd, elt);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  cur = clist_next(cur);

  while (cur != NULL) {
    r = mailimap_char_send(fd, symbol);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    elt = clist_content(cur);
    r = (* sender)(fd, elt);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    cur = clist_next(cur);
  }

  return MAILIMAP_NO_ERROR;
}
				   

int
mailimap_struct_spaced_list_send(mailstream * fd, clist * list,
				 mailimap_struct_sender * sender)
{
  return mailimap_struct_list_send(fd, list, ' ', sender);
}

int mailimap_space_send(mailstream * fd)
{
  return mailimap_char_send(fd, ' ');
}

int mailimap_crlf_send(mailstream * fd)
{
  int r;

  r = mailimap_char_send(fd, '\r');
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_char_send(fd, '\n');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

static int mailimap_oparenth_send(mailstream * fd)
{
  return mailimap_char_send(fd, '(');
}

static int mailimap_cparenth_send(mailstream * fd)
{
  return mailimap_char_send(fd, ')');
}

static int mailimap_dquote_send(mailstream * fd)
{
  return mailimap_char_send(fd, '"');
}

/*
   address         = "(" addr-name SP addr-adl SP addr-mailbox SP
                     addr-host ")"

   addr-adl        = nstring
                       ; Holds route from [RFC-822] route-addr if
                       ; non-NIL

   addr-host       = nstring
                       ; NIL indicates [RFC-822] group syntax.
                       ; Otherwise, holds [RFC-822] domain name

   addr-mailbox    = nstring
                       ; NIL indicates end of [RFC-822] group; if
                       ; non-NIL and addr-host is NIL, holds
                       ; [RFC-822] group name.
                       ; Otherwise, holds [RFC-822] local-part
                       ; after removing [RFC-822] quoting

   addr-name       = nstring
                       ; If non-NIL, holds phrase from [RFC-822]
                       ; mailbox after removing [RFC-822] quoting
*/

/*
=>   append          = "APPEND" SP mailbox [SP flag-list] [SP date-time] SP
                     literal
*/

int mailimap_append_send(mailstream * fd,
			 const char * mailbox,
			 struct mailimap_flag_list * flag_list,
			 struct mailimap_date_time * date_time,
			 size_t literal_size)
{
  int r;

  r = mailimap_token_send(fd, "APPEND");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_mailbox_send(fd, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  if (flag_list != NULL) {
    r = mailimap_space_send(fd);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_flag_list_send(fd, flag_list);
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }
  if (date_time != NULL) {
    r = mailimap_space_send(fd);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_date_time_send(fd, date_time);
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_literal_count_send(fd, literal_size);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   astring         = 1*ASTRING-CHAR / string

=>   ASTRING-CHAR   = ATOM-CHAR / resp-specials
*/

static int is_atom(const char * str)
{
  if (* str == '\0')
    return 0;
  
  while (* str != '\0') {
    unsigned char uch = (unsigned char) * str;
    
    if (uch != '-') {
      if (!isalnum(uch))
        return 0;
    }
    
    str ++;
  }
  
  return 1;
}

int mailimap_astring_send(mailstream * fd, const char * astring)
{
  /*
    workaround for buggy Courier-IMAP that does not accept 
    quoted-strings for fields name but prefer atoms.
  */
  if (is_atom(astring))
    return mailimap_atom_send(fd, astring);
  else
    return mailimap_quoted_send(fd, astring);
}

/*
=>   atom            = 1*ATOM-CHAR
*/

static int mailimap_atom_send(mailstream * fd, const char * atom)
{
  return mailimap_token_send(fd, atom);
}

/*
=>   ATOM-CHAR       = <any CHAR except atom-specials>
*/

/*
=>   atom-specials   = "(" / ")" / "{" / SP / CTL / list-wildcards /
                     quoted-specials / resp-specials
*/

/*
=>   authenticate    = "AUTHENTICATE" SP auth-type *(CRLF base64)
*/
  
int mailimap_authenticate_send(mailstream * fd,
			       const char * auth_type)
{
  int r;

  r = mailimap_token_send(fd, "AUTHENTICATE");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_auth_type_send(fd, auth_type);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_authenticate_resp_send(mailstream * fd,
				    const char * base64)
{
  int r;

  r = mailimap_base64_send(fd, base64);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

/*
=>   auth-type       = atom
                       ; Defined by [SASL]
*/

static int mailimap_auth_type_send(mailstream * fd, const char * auth_type)
{
  return mailimap_atom_send(fd, auth_type);
}


/*
=>   base64          = *(4base64-char) [base64-terminal]
*/

static int mailimap_base64_send(mailstream * fd, const char * base64)
{
  return mailimap_token_send(fd, base64);
}

/*
=>   base64-char     = ALPHA / DIGIT / "+" / "/"
                       ; Case-sensitive

   base64-terminal = (2base64-char "==") / (3base64-char "=")

   body            = "(" (body-type-1part / body-type-mpart) ")"

   body-extension  = nstring / number /
                      "(" body-extension *(SP body-extension) ")"
                       ; Future expansion.  Client implementations
                       ; MUST accept body-extension fields.  Server
                       ; implementations MUST NOT generate
                       ; body-extension fields except as defined by
                       ; future standard or standards-track
                       ; revisions of this specification.

   body-ext-1part  = body-fld-md5 [SP body-fld-dsp [SP body-fld-lang
                     *(SP body-extension)]]
                       ; MUST NOT be returned on non-extensible
                       ; "BODY" fetch

   body-ext-mpart  = body-fld-param [SP body-fld-dsp [SP body-fld-lang
                     *(SP body-extension)]]
                       ; MUST NOT be returned on non-extensible
                       ; "BODY" fetch

   body-fields     = body-fld-param SP body-fld-id SP body-fld-desc SP
                     body-fld-enc SP body-fld-octets

   body-fld-desc   = nstring

   body-fld-dsp    = "(" string SP body-fld-param ")" / nil

   body-fld-enc    = (DQUOTE ("7BIT" / "8BIT" / "BINARY" / "BASE64"/
                     "QUOTED-PRINTABLE") DQUOTE) / string

   body-fld-id     = nstring

   body-fld-lang   = nstring / "(" string *(SP string) ")"

   body-fld-lines  = number

   body-fld-md5    = nstring

   body-fld-octets = number

   body-fld-param  = "(" string SP string *(SP string SP string) ")" / nil

   body-type-1part = (body-type-basic / body-type-msg / body-type-text)
                     [SP body-ext-1part]

   body-type-basic = media-basic SP body-fields
                       ; MESSAGE subtype MUST NOT be "RFC822"

   body-type-mpart = 1*body SP media-subtype
                     [SP body-ext-mpart]

   body-type-msg   = media-message SP body-fields SP envelope
                     SP body SP body-fld-lines

   body-type-text  = media-text SP body-fields SP body-fld-lines

   capability      = ("AUTH=" auth-type) / atom
                       ; New capabilities MUST begin with "X" or be
                       ; registered with IANA as standard or
                       ; standards-track

   capability-data = "CAPABILITY" *(SP capability) SP "IMAP4rev1"
                     *(SP capability)
                       ; IMAP4rev1 servers which offer RFC 1730
                       ; compatibility MUST list "IMAP4" as the first
                       ; capability.

   CHAR8           = %x01-ff
                       ; any OCTET except NUL, %x00
*/

/*
=>   command         = tag SP (command-any / command-auth / command-nonauth /
                    command-select) CRLF
                       ; Modal based on state
*/

/*
=>   command-any     = "CAPABILITY" / "LOGOUT" / "NOOP" / x-command
                       ; Valid in all states
*/

int mailimap_capability_send(mailstream * fd)
{
  int r;

  r = mailimap_token_send(fd, "CAPABILITY");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

int mailimap_logout_send(mailstream * fd)
{
  int r;

  r = mailimap_token_send(fd, "LOGOUT");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

int mailimap_noop_send(mailstream * fd)
{
  int r;

  r = mailimap_token_send(fd, "NOOP");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

/*
=>   command-auth    = append / create / delete / examine / list / lsub /
                     rename / select / status / subscribe / unsubscribe
                       ; Valid only in Authenticated or Selected state
*/

/*
=>   command-nonauth = login / authenticate
                       ; Valid only when in Not Authenticated state
*/

/*
=>   command-select  = "CHECK" / "CLOSE" / "EXPUNGE" / copy / fetch / store /
                     uid / search
                       ; Valid only when in Selected state
*/

int mailimap_check_send(mailstream * fd)
{
  int r;

  r = mailimap_token_send(fd, "CHECK");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_close_send(mailstream * fd)
{
  int r;

  r = mailimap_token_send(fd, "CLOSE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_expunge_send(mailstream * fd)
{
  int r;

  r = mailimap_token_send(fd, "EXPUNGE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   continue-req    = "+" SP (resp-text / base64) CRLF
*/

/*
=>   copy            = "COPY" SP set SP mailbox
*/

int mailimap_copy_send(mailstream * fd,
		       struct mailimap_set * set,
		       const char * mb)
{
  int r;

  r = mailimap_token_send(fd, "COPY");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_set_send(fd, set);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_uid_copy_send(mailstream * fd,
			   struct mailimap_set * set,
			   const char * mb)
{
  int r;

  r = mailimap_token_send(fd, "UID");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return mailimap_copy_send(fd, set, mb);
}

/*
=>   create          = "CREATE" SP mailbox
                       ; Use of INBOX gives a NO error
*/

int mailimap_create_send(mailstream * fd,
			 const char * mb)
{
  int r;

  r = mailimap_token_send(fd, "CREATE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   date            = date-text / DQUOTE date-text DQUOTE
*/

static int mailimap_date_send(mailstream * fd,
			      struct mailimap_date * date)
{
  int r;

  r = mailimap_date_day_send(fd, date->dt_day);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '-');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_month_send(fd, date->dt_month);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '-');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_year_send(fd, date->dt_year);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   date-day        = 1*2DIGIT
                       ; Day of month
*/

static int mailimap_date_day_send(mailstream * fd, int day)
{
  return mailimap_number_send(fd, day);
}

/*
=>   date-day-fixed  = (SP DIGIT) / 2DIGIT
                       ; Fixed-format version of date-day
*/

static int mailimap_date_day_fixed_send(mailstream * fd, int day)
{
  int r;

  if (day < 10) {
    r = mailimap_space_send(fd);
    if (r != MAILIMAP_NO_ERROR)
      return r;

    r = mailimap_number_send(fd, day);
    if (r != MAILIMAP_NO_ERROR)
      return r;

    return MAILIMAP_NO_ERROR;
  }
  else
    return mailimap_number_send(fd, day);
}

/*
=>   date-month      = "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun" /
                     "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"
*/

static int mailimap_date_month_send(mailstream * fd, int month)
{
  const char * name;
  int r;

  name = mailimap_month_get_token_str(month);
  
  if (name == NULL)
    return MAILIMAP_ERROR_INVAL;
    
  r = mailimap_token_send(fd, name);
    if (r != MAILIMAP_NO_ERROR)
      return r;
  
  return MAILIMAP_NO_ERROR;
}

/*
=>   date-text       = date-day "-" date-month "-" date-year
*/

/*
static gboolean mailimap_date_text_send(mailstream * fd,
					struct mailimap_date_text * date_text)
{
  if (!mailimap_date_day_send(fd, date_text->day))
    return FALSE;
  if (!mailimap_char_send(fd, '-'))
    return FALSE;
  if (!mailimap_date_month_send(fd, date_text->month))
    return FALSE;
  if (!mailimap_char_send(fd, '-'))
    return FALSE;
  if (!mailimap_date_year_send(fd, date_text->year))
    return FALSE;

  return TRUE;
}
*/

/*
=>   date-year       = 4DIGIT
*/

static int mailimap_fixed_digit_send(mailstream * fd,
				     int num, int count)
{
  int r;
  
  if (count == 0)
    return MAILIMAP_NO_ERROR;
  
  r = mailimap_fixed_digit_send(fd, num / 10, count - 1);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_digit_send(fd, num % 10);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

static int mailimap_date_year_send(mailstream * fd, int year)
{
  int r;

  r = mailimap_fixed_digit_send(fd, year, 4);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   date-time       = DQUOTE date-day-fixed "-" date-month "-" date-year
                     SP time SP zone DQUOTE
*/

static int
mailimap_date_time_send(mailstream * fd,
			struct mailimap_date_time * date_time)
{
  int r;

  r = mailimap_date_day_fixed_send(fd, date_time->dt_day);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '-');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_month_send(fd, date_time->dt_month);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '-');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_year_send(fd, date_time->dt_month);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_fixed_digit_send(fd, date_time->dt_hour, 2);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, ':');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_fixed_digit_send(fd, date_time->dt_min, 2);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, ':');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_fixed_digit_send(fd, date_time->dt_sec, 2);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   delete          = "DELETE" SP mailbox
                       ; Use of INBOX gives a NO error
*/

int mailimap_delete_send(mailstream * fd, const char * mb)
{
  int r;

  r = mailimap_token_send(fd, "DELETE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*

digit

digit-nz        = %x31-39
                       ; 1-9
*/

static int mailimap_digit_send(mailstream * fd, int digit)
{
  return mailimap_char_send(fd, digit + '0');
}


/*
   envelope        = "(" env-date SP env-subject SP env-from SP env-sender SP
                     env-reply-to SP env-to SP env-cc SP env-bcc SP
                     env-in-reply-to SP env-message-id ")"

   env-bcc         = "(" 1*address ")" / nil

   env-cc          = "(" 1*address ")" / nil

   env-date        = nstring

   env-from        = "(" 1*address ")" / nil

   env-in-reply-to = nstring

   env-message-id  = nstring

   env-reply-to    = "(" 1*address ")" / nil

   env-sender      = "(" 1*address ")" / nil

   env-subject     = nstring

   env-to          = "(" 1*address ")" / nil
*/

/*
=>   examine         = "EXAMINE" SP mailbox
*/

int mailimap_examine_send(mailstream * fd, const char * mb)
{
  int r;
  
  r = mailimap_token_send(fd, "EXAMINE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   fetch           = "FETCH" SP set SP ("ALL" / "FULL" / "FAST" / fetch-att /
                     "(" fetch-att *(SP fetch-att) ")")
*/

static int
mailimap_fetch_att_list_send(mailstream * fd, clist * fetch_att_list);

static int
mailimap_fetch_type_send(mailstream * fd,
			 struct mailimap_fetch_type * fetch_type)
{
  switch (fetch_type->ft_type) {
  case MAILIMAP_FETCH_TYPE_ALL:
    return mailimap_token_send(fd, "ALL");
  case MAILIMAP_FETCH_TYPE_FULL:
    return mailimap_token_send(fd, "FULL");
  case MAILIMAP_FETCH_TYPE_FAST:
    return mailimap_token_send(fd, "FAST");
  case MAILIMAP_FETCH_TYPE_FETCH_ATT:
    return mailimap_fetch_att_send(fd, fetch_type->ft_data.ft_fetch_att);
  case MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST:
    return mailimap_fetch_att_list_send(fd,
        fetch_type->ft_data.ft_fetch_att_list);
  default:
    /* should not happen */
    return MAILIMAP_ERROR_INVAL;
  }
}

int mailimap_fetch_send(mailstream * fd,
			struct mailimap_set * set,
			struct mailimap_fetch_type * fetch_type)
{
  int r;

  r = mailimap_token_send(fd, "FETCH");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_set_send(fd, set);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_fetch_type_send(fd, fetch_type);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int
mailimap_uid_fetch_send(mailstream * fd,
			struct mailimap_set * set,
			struct mailimap_fetch_type * fetch_type)
{
  int r;
  
  r = mailimap_token_send(fd, "UID");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return mailimap_fetch_send(fd, set, fetch_type);
}

/* currently porting */

static int
mailimap_fetch_att_list_send(mailstream * fd, clist * fetch_att_list)
{
  int r;
  
  r = mailimap_oparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_struct_spaced_list_send(fd, fetch_att_list,
  				  (mailimap_struct_sender *)
				  mailimap_fetch_att_send);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_cparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   fetch-att       = "ENVELOPE" / "FLAGS" / "INTERNALDATE" /
                     "RFC822" [".HEADER" / ".SIZE" / ".TEXT"] /
                     "BODY" ["STRUCTURE"] / "UID" /
                     "BODY" [".PEEK"] section ["<" number "." nz-number ">"]
*/

static int mailimap_fetch_att_send(mailstream * fd,
				struct mailimap_fetch_att * fetch_att)
{
  int r;

  switch(fetch_att->att_type) {
  case MAILIMAP_FETCH_ATT_ENVELOPE:
    return mailimap_token_send(fd, "ENVELOPE");

  case MAILIMAP_FETCH_ATT_FLAGS:
    return mailimap_token_send(fd, "FLAGS");

  case MAILIMAP_FETCH_ATT_INTERNALDATE:
    return mailimap_token_send(fd, "INTERNALDATE");

  case MAILIMAP_FETCH_ATT_RFC822:
    return mailimap_token_send(fd, "RFC822");

  case MAILIMAP_FETCH_ATT_RFC822_HEADER:
    return mailimap_token_send(fd, "RFC822.HEADER");

  case MAILIMAP_FETCH_ATT_RFC822_SIZE:
    return mailimap_token_send(fd, "RFC822.SIZE");

  case MAILIMAP_FETCH_ATT_RFC822_TEXT:
    return mailimap_token_send(fd, "RFC822.TEXT");

  case MAILIMAP_FETCH_ATT_BODY:
    return mailimap_token_send(fd, "BODY");

  case MAILIMAP_FETCH_ATT_BODYSTRUCTURE:
    return mailimap_token_send(fd, "BODYSTRUCTURE");

  case MAILIMAP_FETCH_ATT_UID:
    return mailimap_token_send(fd, "UID");

  case MAILIMAP_FETCH_ATT_BODY_SECTION:

    r = mailimap_token_send(fd, "BODY");
    if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_section_send(fd, fetch_att->att_section);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    if (fetch_att->att_size != 0) {
      r = mailimap_char_send(fd, '<');
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_number_send(fd, fetch_att->att_offset);
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_char_send(fd, '.');
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_number_send(fd, fetch_att->att_size);
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_char_send(fd, '>');
      if (r != MAILIMAP_NO_ERROR)
	return r;
    }

    return MAILIMAP_NO_ERROR;

  case MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION:
    r = mailimap_token_send(fd, "BODY.PEEK");
    if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_section_send(fd, fetch_att->att_section);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    if (fetch_att->att_size != 0) {
      r = mailimap_char_send(fd, '<');
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_number_send(fd, fetch_att->att_offset);
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_char_send(fd, '.');
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_number_send(fd, fetch_att->att_size);
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_char_send(fd, '>');
      if (r != MAILIMAP_NO_ERROR)
	return r;
    }
    return MAILIMAP_NO_ERROR;

  default:
    /* should not happen */
    return MAILIMAP_ERROR_INVAL;
  }
}

/*
=>   flag            = "\Answered" / "\Flagged" / "\Deleted" /
                     "\Seen" / "\Draft" / flag-keyword / flag-extension
                       ; Does not include "\Recent"
*/

/*
enum {
  FLAG_ANSWERED,
  FLAG_FLAGGED,
  FLAG_DELETED,
  FLAG_SEEN,
  FLAG_DRAFT,
  FLAG_KEYWORD,
  FLAG_EXTENSION
};

struct mailimap_flag {
  gint type;
  gchar * flag_keyword;
  gchar * flag_extension;
};
*/

static int mailimap_flag_send(mailstream * fd,
	 			struct mailimap_flag * flag)
{
  switch(flag->fl_type) {
  case MAILIMAP_FLAG_ANSWERED:
    return mailimap_token_send(fd, "\\Answered");
  case MAILIMAP_FLAG_FLAGGED:
    return mailimap_token_send(fd, "\\Flagged");
  case MAILIMAP_FLAG_DELETED:
    return mailimap_token_send(fd, "\\Deleted");
  case MAILIMAP_FLAG_SEEN:
    return mailimap_token_send(fd, "\\Seen");
  case MAILIMAP_FLAG_DRAFT:
    return mailimap_token_send(fd, "\\Draft");
  case MAILIMAP_FLAG_KEYWORD:
    return mailimap_flag_keyword_send(fd, flag->fl_data.fl_keyword);
  case MAILIMAP_FLAG_EXTENSION:
    return mailimap_flag_extension_send(fd, flag->fl_data.fl_extension);
  default:
    /* should not happen */
    return MAILIMAP_ERROR_INVAL;
  }
}


/*
=>   flag-extension  = "\" atom
                       ; Future expansion.  Client implementations
                       ; MUST accept flag-extension flags.  Server
                       ; implementations MUST NOT generate
                       ; flag-extension flags except as defined by
                       ; future standard or standards-track
                       ; revisions of this specification.
*/

static int mailimap_flag_extension_send(mailstream * fd,
					     const char * flag_extension)
{
  int r;
  
  r = mailimap_char_send(fd, '\\');
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_atom_send(fd, flag_extension);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   flag-fetch      = flag / "\Recent"
*/

/*
=>   flag-keyword    = atom
*/

static int mailimap_flag_keyword_send(mailstream * fd,
 				const char * flag_keyword)
{
  return mailimap_token_send(fd, flag_keyword);
}

/*
=>   flag-list       = "(" [flag *(SP flag)] ")"
*/

static int mailimap_flag_list_send(mailstream * fd,
					struct mailimap_flag_list * flag_list)
{
  int r;
  
  r = mailimap_oparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (flag_list->fl_list != NULL) {
	r = mailimap_struct_spaced_list_send(fd, flag_list->fl_list,
            (mailimap_struct_sender *) mailimap_flag_send);
	if (r != MAILIMAP_NO_ERROR)
      return r;
  }

  r = mailimap_cparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   flag-perm       = flag / "\*"

   greeting        = "*" SP (resp-cond-auth / resp-cond-bye) CRLF
*/

/*
=>   header-fld-name = astring
*/

static int mailimap_header_fld_name_send(mailstream * fd, const char * header)
{
  return mailimap_astring_send(fd, header);
}

/*
=>   header-list     = "(" header-fld-name *(SP header-fld-name) ")"
*/

static int
mailimap_header_list_send(mailstream * fd,
    struct mailimap_header_list * header_list)
{
  int r;
  
  r = mailimap_oparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_struct_spaced_list_send(fd, header_list->hdr_list,
      (mailimap_struct_sender *) mailimap_header_fld_name_send);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_cparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   list            = "LIST" SP mailbox SP list-mailbox
*/

int mailimap_list_send(mailstream * fd,
				const char * mb,
				const char * list_mb)
{
  int r;
  
  r = mailimap_token_send(fd, "LIST");
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_list_mailbox_send(fd, list_mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   list-mailbox    = 1*list-char / string
*/

int
mailimap_list_mailbox_send(mailstream * fd, const char * pattern)
{
  return mailimap_quoted_send(fd, pattern);
}

/*
   list-char       = ATOM-CHAR / list-wildcards / resp-specials

   list-wildcards  = "%" / "*"
*/

/*
=>   literal         = "{" number "}" CRLF *CHAR8
                       ; Number represents the number of CHAR8s
*/

int
mailimap_literal_send(mailstream * fd, const char * literal,
		      size_t progr_rate,
		      progress_function * progr_fun)
{
  size_t len;
  uint32_t literal_len;
  int r;
  
  len = strlen(literal);
  literal_len = mailstream_get_data_crlf_size(literal, len);
  
  r = mailimap_literal_count_send(fd, literal_len);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_literal_data_send(fd, literal, len, progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
  "{" number "}" CRLF
*/

int
mailimap_literal_count_send(mailstream * fd, uint32_t count)
{
  int r;
  
  r = mailimap_char_send(fd, '{');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_number_send(fd, count);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '}');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
  *CHAR8
*/

int
mailimap_literal_data_send(mailstream * fd, const char * literal, uint32_t len,
			   size_t progr_rate,
			   progress_function * progr_fun)
{
  int r;
  
  r = mailimap_sized_token_send(fd, literal, len);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}


/*
=>   login           = "LOGIN" SP userid SP password
*/

int mailimap_login_send(mailstream * fd,
				const char * userid, const char * password)
{
  int r;
  
  r = mailimap_token_send(fd, "LOGIN");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_userid_send(fd, userid);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_password_send(fd, password);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   lsub            = "LSUB" SP mailbox SP list-mailbox
*/

int mailimap_lsub_send(mailstream * fd,
				const char * mb, const char * list_mb)
{
  int r;
  
  r = mailimap_token_send(fd, "LSUB");
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_list_mailbox_send(fd, list_mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   mailbox         = "INBOX" / astring
                       ; INBOX is case-insensitive.  All case variants of
                       ; INBOX (e.g. "iNbOx") MUST be interpreted as INBOX
                       ; not as an astring.  An astring which consists of
                       ; the case-insensitive sequence "I" "N" "B" "O" "X"
                       ; is considered to be INBOX and not an astring.
                       ;  Refer to section 5.1 for further
                       ; semantic details of mailbox names.
*/

int mailimap_mailbox_send(mailstream * fd, const char * mb)
{
  return mailimap_astring_send(fd, mb);
}

/*
   mailbox-data    =  "FLAGS" SP flag-list / "LIST" SP mailbox-list /
                      "LSUB" SP mailbox-list / "SEARCH" *(SP nz-number) /
                      "STATUS" SP mailbox SP "("
                      [status-att SP number *(SP status-att SP number)] ")" /
                      number SP "EXISTS" / number SP "RECENT"

   mailbox-list    = "(" [mbx-list-flags] ")" SP
                      (DQUOTE QUOTED-CHAR DQUOTE / nil) SP mailbox

   mbx-list-flags  = *(mbx-list-oflag SP) mbx-list-sflag
                     *(SP mbx-list-oflag) /
                     mbx-list-oflag *(SP mbx-list-oflag)

   mbx-list-oflag  = "\Noinferiors" / flag-extension
                       ; Other flags; multiple possible per LIST response

   mbx-list-sflag  = "\Noselect" / "\Marked" / "\Unmarked"
                       ; Selectability flags; only one per LIST response

   media-basic     = ((DQUOTE ("APPLICATION" / "AUDIO" / "IMAGE" / "MESSAGE" /
                     "VIDEO") DQUOTE) / string) SP media-subtype
                       ; Defined in [MIME-IMT]

   media-message   = DQUOTE "MESSAGE" DQUOTE SP DQUOTE "RFC822" DQUOTE
                       ; Defined in [MIME-IMT]

   media-subtype   = string
                       ; Defined in [MIME-IMT]

   media-text      = DQUOTE "TEXT" DQUOTE SP media-subtype
                       ; Defined in [MIME-IMT]

   message-data    = nz-number SP ("EXPUNGE" / ("FETCH" SP msg-att))

   msg-att         = "(" (msg-att-dynamic / msg-att-static)
                      *(SP (msg-att-dynamic / msg-att-static)) ")"

   msg-att-dynamic = "FLAGS" SP "(" [flag-fetch *(SP flag-fetch)] ")"
                       ; MAY change for a message

   msg-att-static  = "ENVELOPE" SP envelope / "INTERNALDATE" SP date-time /
                     "RFC822" [".HEADER" / ".TEXT"] SP nstring /
                     "RFC822.SIZE" SP number / "BODY" ["STRUCTURE"] SP body /
                     "BODY" section ["<" number ">"] SP nstring /
                     "UID" SP uniqueid
                       ; MUST NOT change for a message

   nil             = "NIL"

   nstring         = string / nil
*/

/*
=>   number          = 1*DIGIT
                       ; Unsigned 32-bit integer
                       ; (0 <= n < 4,294,967,296)
*/

/*
   nz-number       = digit-nz *DIGIT
                       ; Non-zero unsigned 32-bit integer
                       ; (0 < n < 4,294,967,296)
*/

static int mailimap_number_send(mailstream * fd, uint32_t number)
{
  int r;
  
  if (number / 10 != 0) {
    r = mailimap_number_send(fd, number / 10);
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }

  r = mailimap_digit_send(fd, number % 10);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   password        = astring
*/

static int mailimap_password_send(mailstream * fd, const char * pass)
{
  return mailimap_astring_send(fd, pass);
}

/*
=>   quoted          = DQUOTE *QUOTED-CHAR DQUOTE

=>   QUOTED-CHAR     = <any TEXT-CHAR except quoted-specials> /
                     "\" quoted-specials

=>   quoted-specials = DQUOTE / "\"
*/

static int is_quoted_specials(char ch)
{
  return (ch == '\"') || (ch == '\\');
}

static int mailimap_quoted_char_send(mailstream * fd, char ch)
{
  int r;
  
  if (is_quoted_specials(ch)) {
    r = mailimap_char_send(fd, '\\');
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_char_send(fd, ch);
	if (r != MAILIMAP_NO_ERROR)
      return r;
	
    return MAILIMAP_NO_ERROR;
  }
  else
    return mailimap_char_send(fd, ch);
}

int mailimap_quoted_send(mailstream * fd, const char * quoted)
{
  const char * pos;
  int r;
  
  pos = quoted;

  r = mailimap_dquote_send(fd);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  while (* pos != 0) {
    r = mailimap_quoted_char_send(fd, * pos);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    pos ++;
  }

  r = mailimap_dquote_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   rename          = "RENAME" SP mailbox SP mailbox
                       ; Use of INBOX as a destination gives a NO error
*/

int mailimap_rename_send(mailstream * fd, const char * mb,
				const char * new_name)
{
  int r;
  
  r = mailimap_token_send(fd, "RENAME");
  if (r != MAILIMAP_NO_ERROR)
	return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_mailbox_send(fd, new_name);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   response        = *(continue-req / response-data) response-done

   response-data   = "*" SP (resp-cond-state / resp-cond-bye /
                     mailbox-data / message-data / capability-data) CRLF

   response-done   = response-tagged / response-fatal

   response-fatal  = "*" SP resp-cond-bye CRLF
                       ; Server closes connection immediately

   response-tagged = tag SP resp-cond-state CRLF

   resp-cond-auth  = ("OK" / "PREAUTH") SP resp-text
                       ; Authentication condition

   resp-cond-bye   = "BYE" SP resp-text

   resp-cond-state = ("OK" / "NO" / "BAD") SP resp-text
                       ; Status condition

   resp-specials   = "]"

   resp-text       = ["[" resp-text-code "]" SP] text

   resp-text-code  = "ALERT" /
                     "BADCHARSET" [SP "(" astring *(SP astring) ")" ] /
                     capability-data / "PARSE" /
                     "PERMANENTFLAGS" SP "(" [flag-perm *(SP flag-perm)] ")" /
                     "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                     "UIDNEXT" SP nz-number / "UIDVALIDITY" SP nz-number /
                     "UNSEEN" SP nz-number /
                     atom [SP 1*<any TEXT-CHAR except "]">]
*/

/*
=>   search          = "SEARCH" [SP "CHARSET" SP astring] 1*(SP search-key)
                       ; CHARSET argument to MUST be registered with IANA
*/

int
mailimap_search_send(mailstream * fd, const char * charset,
		     struct mailimap_search_key * key)
{
  int r;
  
  r = mailimap_token_send(fd, "SEARCH");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (charset != NULL) {
    r = mailimap_space_send(fd);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    
    r = mailimap_token_send(fd, "CHARSET");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, charset);
	if (r != MAILIMAP_NO_ERROR)
      return r;
  }

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_search_key_send(fd, key);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int
mailimap_uid_search_send(mailstream * fd, const char * charset,
   				struct mailimap_search_key * key)
{
  int r;
  
  r = mailimap_token_send(fd, "UID");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return mailimap_search_send(fd, charset, key);
}


/*
=>   search-key      = "ALL" / "ANSWERED" / "BCC" SP astring /
                     "BEFORE" SP date / "BODY" SP astring /
                     "CC" SP astring / "DELETED" / "FLAGGED" /
                     "FROM" SP astring / "KEYWORD" SP flag-keyword / "NEW" /
                     "OLD" / "ON" SP date / "RECENT" / "SEEN" /
                     "SINCE" SP date / "SUBJECT" SP astring /
                     "TEXT" SP astring / "TO" SP astring /
                     "UNANSWERED" / "UNDELETED" / "UNFLAGGED" /
                     "UNKEYWORD" SP flag-keyword / "UNSEEN" /
                       ; Above this line were in [IMAP2]
                     "DRAFT" / "HEADER" SP header-fld-name SP astring /
                     "LARGER" SP number / "NOT" SP search-key /
                     "OR" SP search-key SP search-key /
                     "SENTBEFORE" SP date / "SENTON" SP date /
                     "SENTSINCE" SP date / "SMALLER" SP number /
                     "UID" SP set / "UNDRAFT" / set /
                     "(" search-key *(SP search-key) ")"
*/


static int mailimap_search_key_send(mailstream * fd,
   				struct mailimap_search_key * key)
{
  int r;
  
  switch (key->sk_type) {

  case MAILIMAP_SEARCH_KEY_ALL:
    return mailimap_token_send(fd, "ALL");

  case MAILIMAP_SEARCH_KEY_ANSWERED:
    return mailimap_token_send(fd, "ANSWERED");

  case MAILIMAP_SEARCH_KEY_BCC:
    r = mailimap_token_send(fd, "BCC");
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_bcc);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_BEFORE:
    r = mailimap_token_send(fd, "BEFORE");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_date_send(fd, key->sk_data.sk_before);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_BODY:
    r = mailimap_token_send(fd, "BODY");
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_body);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_CC:
    r = mailimap_token_send(fd, "CC");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_cc);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_DELETED:
    return mailimap_token_send(fd, "DELETED");

  case MAILIMAP_SEARCH_KEY_FLAGGED:
    return mailimap_token_send(fd, "FLAGGED");

  case MAILIMAP_SEARCH_KEY_FROM:
    r = mailimap_token_send(fd, "FROM");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_from);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_KEYWORD:
    r = mailimap_token_send(fd, "KEYWORD");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
	r = mailimap_flag_keyword_send(fd, key->sk_data.sk_keyword);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_NEW:
    return mailimap_token_send(fd, "NEW");

  case MAILIMAP_SEARCH_KEY_OLD:
    return mailimap_token_send(fd, "OLD");

  case MAILIMAP_SEARCH_KEY_ON:
    r = mailimap_token_send(fd, "ON");
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
	r = mailimap_date_send(fd, key->sk_data.sk_on);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_RECENT:
    return mailimap_token_send(fd, "RECENT");

  case MAILIMAP_SEARCH_KEY_SEEN:
    return mailimap_token_send(fd, "SEEN");

  case MAILIMAP_SEARCH_KEY_SINCE:
    r = mailimap_token_send(fd, "SINCE");
	if (r != MAILIMAP_NO_ERROR)
      return r;
	r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_date_send(fd, key->sk_data.sk_since);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_SUBJECT:
    r = mailimap_token_send(fd, "SUBJECT");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_subject);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_TEXT:
    r = mailimap_token_send(fd, "TEXT");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_text);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_TO:
    r = mailimap_token_send(fd, "TO");
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_astring_send(fd, key->sk_data.sk_text);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_UNANSWERED:
    return mailimap_token_send(fd, "UNANSWERED");

  case MAILIMAP_SEARCH_KEY_UNDELETED:
    return mailimap_token_send(fd, "UNDELETED");

  case MAILIMAP_SEARCH_KEY_UNFLAGGED:
    return mailimap_token_send(fd, "UNFLAGGED");

  case MAILIMAP_SEARCH_KEY_UNKEYWORD:
    r = mailimap_token_send(fd, "UNKEYWORD");
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_flag_keyword_send(fd, key->sk_data.sk_keyword);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_UNSEEN:
    return mailimap_token_send(fd, "UNSEEN");

  case MAILIMAP_SEARCH_KEY_DRAFT:
    return mailimap_token_send(fd, "DRAFT");

  case MAILIMAP_SEARCH_KEY_HEADER:
    r = mailimap_token_send(fd, "HEADER");
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_header_fld_name_send(fd,
        key->sk_data.sk_header.sk_header_name);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    r = mailimap_astring_send(fd,
        key->sk_data.sk_header.sk_header_value);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_LARGER:
    r = mailimap_token_send(fd, "LARGER");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_number_send(fd, key->sk_data.sk_larger);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_NOT:
    r = mailimap_token_send(fd, "NOT");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_search_key_send(fd, key->sk_data.sk_not);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_OR:
    r = mailimap_token_send(fd, "OR");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_search_key_send(fd, key->sk_data.sk_or.sk_or1);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_search_key_send(fd, key->sk_data.sk_or.sk_or2);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return TRUE;

  case MAILIMAP_SEARCH_KEY_SENTBEFORE:
    r = mailimap_token_send(fd, "SENTBEFORE");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_date_send(fd, key->sk_data.sk_sentbefore);
	if (r != MAILIMAP_NO_ERROR)
	  return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_SENTON:
    r = mailimap_token_send(fd, "SENTON");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_date_send(fd, key->sk_data.sk_senton);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_SENTSINCE:
    r = mailimap_token_send(fd, "SENTSINCE");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_date_send(fd, key->sk_data.sk_sentsince);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_SMALLER:
    r = mailimap_token_send(fd, "SMALLER");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_number_send(fd, key->sk_data.sk_smaller);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;
    
  case MAILIMAP_SEARCH_KEY_UID:
    r = mailimap_token_send(fd, "UID");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_set_send(fd, key->sk_data.sk_set);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SEARCH_KEY_UNDRAFT:
    return mailimap_token_send(fd, "UNDRAFT");

  case MAILIMAP_SEARCH_KEY_SET:
    return mailimap_set_send(fd, key->sk_data.sk_set);

  case MAILIMAP_SEARCH_KEY_MULTIPLE:
    r = mailimap_oparenth_send(fd);
	if (r != MAILIMAP_NO_ERROR)
	  return r;

    r = mailimap_struct_spaced_list_send(fd, key->sk_data.sk_multiple,
	  				(mailimap_struct_sender *)
					mailimap_search_key_send);
	if (r != MAILIMAP_NO_ERROR)
      return r;

    r = mailimap_cparenth_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;

    return MAILIMAP_NO_ERROR;
  default:
    /* should not happend */
    return MAILIMAP_ERROR_INVAL;
  }
}

/*
=>   section         = "[" [section-spec] "]"
*/

static int
mailimap_section_send(mailstream * fd,
    struct mailimap_section * section)
{
  int r;
  
  r = mailimap_char_send(fd, '[');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (section != NULL) {
    if (section->sec_spec != NULL) {
      r = mailimap_section_spec_send(fd, section->sec_spec);
      if (r != MAILIMAP_NO_ERROR)
	return r;
    }
  }

  r = mailimap_char_send(fd, ']');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   section-msgtext = "HEADER" / "HEADER.FIELDS" [".NOT"] SP header-list /
                     "TEXT"
                       ; top-level or MESSAGE/RFC822 part
*/

static int
mailimap_section_msgtext_send(mailstream * fd,
			      struct mailimap_section_msgtext *
			      section_msgtext)
{
  int r;
  
  switch (section_msgtext->sec_type) {
  case MAILIMAP_SECTION_MSGTEXT_HEADER:
    return mailimap_token_send(fd, "HEADER");

  case MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS:
    r = mailimap_token_send(fd, "HEADER.FIELDS");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_header_list_send(fd, section_msgtext->sec_header_list);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT:
    r = mailimap_token_send(fd, "HEADER.FIELDS.NOT");
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_space_send(fd);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_header_list_send(fd, section_msgtext->sec_header_list);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_SECTION_MSGTEXT_TEXT:
    return mailimap_token_send(fd, "TEXT");

  default:
    /* should not happend */
    return MAILIMAP_ERROR_INVAL;
  }
}

/*
=>   section-part    = nz-number *("." nz-number)
                       ; body part nesting
*/

static int
mailimap_pnumber_send(mailstream * fd, uint32_t * pnumber)
{
  return mailimap_number_send(fd, * pnumber);
}

static int
mailimap_section_part_send(mailstream * fd,
			   struct mailimap_section_part * section)
{
  int r;
  
  r = mailimap_struct_list_send(fd, section->sec_id, '.',
      (mailimap_struct_sender *) mailimap_pnumber_send);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   section-spec    = section-msgtext / (section-part ["." section-text])
*/

static int
mailimap_section_spec_send(mailstream * fd,
			   struct mailimap_section_spec * section_spec)
{
  int r;
  
  switch (section_spec->sec_type) {
  case MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT:
    return mailimap_section_msgtext_send(fd,
        section_spec->sec_data.sec_msgtext);

  case MAILIMAP_SECTION_SPEC_SECTION_PART:
    r = mailimap_section_part_send(fd, section_spec->sec_data.sec_part);
    if (r != MAILIMAP_NO_ERROR)
      return r;

    if (section_spec->sec_text != NULL) {
      r = mailimap_char_send(fd, '.');
      if (r != MAILIMAP_NO_ERROR)
	return r;
      r = mailimap_section_text_send(fd,
          section_spec->sec_text);
      if (r != MAILIMAP_NO_ERROR)
	return r;
    }

    return MAILIMAP_NO_ERROR;

  default:
    /* should not happen */
    return MAILIMAP_ERROR_INVAL;
  }
}

/*
=>   section-text    = section-msgtext / "MIME"
                       ; text other than actual body part (headers, etc.)
*/

static int
mailimap_section_text_send(mailstream * fd,
    struct mailimap_section_text * section_text)
{
  switch (section_text->sec_type) {
  case MAILIMAP_SECTION_TEXT_SECTION_MSGTEXT:
    return mailimap_section_msgtext_send(fd, section_text->sec_msgtext);

  case MAILIMAP_SECTION_TEXT_MIME:
    return mailimap_token_send(fd, "MIME");

  default:
    /* should not happen */
    return MAILIMAP_NO_ERROR;
  }
}

/*
=>   select          = "SELECT" SP mailbox
*/

int
mailimap_select_send(mailstream * fd, const char * mb)
{
  int r;
  
  r = mailimap_token_send(fd, "SELECT");
  if (r != MAILIMAP_NO_ERROR)
	return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   sequence-num    = nz-number / "*"
                       ; * is the largest number in use.  For message
                       ; sequence numbers, it is the number of messages
                       ; in the mailbox.  For unique identifiers, it is
                       ; the unique identifier of the last message in
                       ; the mailbox.
*/

/* if sequence_num == 0 then "*" */

static int
mailimap_sequence_num_send(mailstream * fd, uint32_t sequence_num)
{
  if (sequence_num == 0)
    return mailimap_char_send(fd, '*');
  else
    return mailimap_number_send(fd, sequence_num);
}

/*
=>   set             = sequence-num / (sequence-num ":" sequence-num) /
                     (set "," set)
                       ; Identifies a set of messages.  For message
                       ; sequence numbers, these are consecutive
                       ; numbers from 1 to the number of messages in
                       ; the mailbox
                       ; Comma delimits individual numbers, colon
                       ; delimits between two numbers inclusive.
                       ; Example: 2,4:7,9,12:* is 2,4,5,6,7,9,12,13,
                       ; 14,15 for a mailbox with 15 messages.
*/

static int mailimap_set_item_send(mailstream * fd,
 				struct mailimap_set_item * item)
{
  int r;
  
  if (item->set_first == item->set_last)
    return mailimap_sequence_num_send(fd, item->set_first);
  else {
    r = mailimap_sequence_num_send(fd, item->set_first);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_char_send(fd, ':');
	if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_sequence_num_send(fd, item->set_last);
	if (r != MAILIMAP_NO_ERROR)
      return r;
    return MAILIMAP_NO_ERROR;
  }
}

int mailimap_set_send(mailstream * fd,
    struct mailimap_set * set)
{
  return mailimap_struct_list_send(fd, set->set_list, ',',
      (mailimap_struct_sender *) mailimap_set_item_send);
}

/*
=>   status          = "STATUS" SP mailbox SP "(" status-att *(SP status-att) ")"
*/

static int
mailimap_status_att_list_send(mailstream * fd,
    struct mailimap_status_att_list * status_att_list)
{
  return mailimap_struct_spaced_list_send(fd, status_att_list->att_list,
      (mailimap_struct_sender *) mailimap_status_att_send);
}

int
mailimap_status_send(mailstream * fd, const char * mb,
    struct mailimap_status_att_list * status_att_list)
{
  int r;
  
  r = mailimap_token_send(fd, "STATUS");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '(');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_status_att_list_send(fd, status_att_list);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, ')');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   status-att      = "MESSAGES" / "RECENT" / "UIDNEXT" / "UIDVALIDITY" /
                     "UNSEEN"
*/


static int mailimap_status_att_send(mailstream * fd, int * status_att)
{
  const char * token;

  token = mailimap_status_att_get_token_str(* status_att);
  if (token == NULL) {
    /* should not happen */
    return MAILIMAP_ERROR_INVAL;
  }

  return mailimap_token_send(fd, token);
}

/*
=>   store           = "STORE" SP set SP store-att-flags
*/

int
mailimap_store_send(mailstream * fd,
		    struct mailimap_set * set,
		    struct mailimap_store_att_flags * store_att_flags)
{
  int r;
  
  r = mailimap_token_send(fd, "STORE");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_set_send(fd, set);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_store_att_flags_send(fd, store_att_flags);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int
mailimap_uid_store_send(mailstream * fd,
			struct mailimap_set * set,
			struct mailimap_store_att_flags * store_att_flags)
{
  int r;
  
  r = mailimap_token_send(fd, "UID");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return mailimap_store_send(fd, set, store_att_flags);
}

/*
=>   store-att-flags = (["+" / "-"] "FLAGS" [".SILENT"]) SP
                     (flag-list / (flag *(SP flag)))
*/

static int
mailimap_store_att_flags_send(mailstream * fd,
			      struct mailimap_store_att_flags * store_flags)
{
  int r;
  
  switch (store_flags->fl_sign) {
  case 1:
    r = mailimap_char_send(fd, '+');
    if (r != MAILIMAP_NO_ERROR)
      return r;
    break;
  case -1:
    r = mailimap_char_send(fd, '-');
    if (r != MAILIMAP_NO_ERROR)
      return r;
    break;
  }
  
  r = mailimap_token_send(fd, "FLAGS");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (store_flags->fl_silent) {
    r = mailimap_token_send(fd, ".SILENT");
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }
  
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_flag_list_send(fd, store_flags->fl_flag_list);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
   string          = quoted / literal
*/

/*
=>   subscribe       = "SUBSCRIBE" SP mailbox
*/

int mailimap_subscribe_send(mailstream * fd, const char * mb)
{
  int r;
  
  r = mailimap_token_send(fd, "SUBSCRIBE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

/*
=>   tag             = 1*<any ASTRING-CHAR except "+">
*/

int mailimap_tag_send(mailstream * fd, const char * tag)
{
  return mailimap_token_send(fd, tag);
}

/*
   text            = 1*TEXT-CHAR

   TEXT-CHAR       = <any CHAR except CR and LF>

   time            = 2DIGIT ":" 2DIGIT ":" 2DIGIT
                       ; Hours minutes seconds
*/

/*
=>   uid             = "UID" SP (copy / fetch / search / store)
                       ; Unique identifiers used instead of message
                       ; sequence numbers

functions uid_copy, uid_fetch ...
*/


/*
   uniqueid        = nz-number
                       ; Strictly ascending
*/

/*
=>   unsubscribe     = "UNSUBSCRIBE" SP mailbox
*/

int mailimap_unsubscribe_send(mailstream * fd,
				const char * mb)
{
  int r;
  
  r = mailimap_token_send(fd, "UNSUBSCRIBE");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_starttls_send(mailstream * fd)
{
  return mailimap_token_send(fd, "STARTTLS");
}

/*
=>   userid          = astring
*/

static int mailimap_userid_send(mailstream * fd, const char * user)
{
  return mailimap_astring_send(fd, user);
}

/*
   x-command       = "X" atom <experimental command arguments>

   zone            = ("+" / "-") 4DIGIT
                       ; Signed four-digit value of hhmm representing
                       ; hours and minutes east of Greenwich (that is,
                       ; the amount that the given time differs from
                       ; Universal Time).  Subtracting the timezone
                       ; from the given time will give the UT form.
                       ; The Universal Time zone is "+0000".
*/
