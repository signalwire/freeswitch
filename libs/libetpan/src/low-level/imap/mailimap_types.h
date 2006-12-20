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
 * $Id: mailimap_types.h,v 1.29 2006/10/20 00:13:30 hoa Exp $
 */

/*
  IMAP4rev1 grammar

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

   append          = "APPEND" SP mailbox [SP flag-list] [SP date-time] SP
                     literal

   astring         = 1*ASTRING-CHAR / string

   ASTRING-CHAR   = ATOM-CHAR / resp-specials

   atom            = 1*ATOM-CHAR

   ATOM-CHAR       = <any CHAR except atom-specials>

   atom-specials   = "(" / ")" / "{" / SP / CTL / list-wildcards /
                     quoted-specials / resp-specials

   authenticate    = "AUTHENTICATE" SP auth-type *(CRLF base64)

   auth-type       = atom
                       ; Defined by [SASL]

   base64          = *(4base64-char) [base64-terminal]

   base64-char     = ALPHA / DIGIT / "+" / "/"
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

   command         = tag SP (command-any / command-auth / command-nonauth /
                     command-select) CRLF
                       ; Modal based on state

   command-any     = "CAPABILITY" / "LOGOUT" / "NOOP" / x-command
                       ; Valid in all states

   command-auth    = append / create / delete / examine / list / lsub /
                     rename / select / status / subscribe / unsubscribe
                       ; Valid only in Authenticated or Selected state

   command-nonauth = login / authenticate
                       ; Valid only when in Not Authenticated state

   command-select  = "CHECK" / "CLOSE" / "EXPUNGE" / copy / fetch / store /
                     uid / search
                       ; Valid only when in Selected state

   continue-req    = "+" SP (resp-text / base64) CRLF

   copy            = "COPY" SP set SP mailbox

   create          = "CREATE" SP mailbox
                       ; Use of INBOX gives a NO error

   date            = date-text / DQUOTE date-text DQUOTE

   date-day        = 1*2DIGIT
                       ; Day of month

   date-day-fixed  = (SP DIGIT) / 2DIGIT
                       ; Fixed-format version of date-day

   date-month      = "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun" /
                     "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"

   date-text       = date-day "-" date-month "-" date-year

   date-year       = 4DIGIT

   date-time       = DQUOTE date-day-fixed "-" date-month "-" date-year
                     SP time SP zone DQUOTE

   delete          = "DELETE" SP mailbox
                       ; Use of INBOX gives a NO error

   digit-nz        = %x31-39
                       ; 1-9

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

   examine         = "EXAMINE" SP mailbox

   fetch           = "FETCH" SP set SP ("ALL" / "FULL" / "FAST" / fetch-att /
                     "(" fetch-att *(SP fetch-att) ")")

   fetch-att       = "ENVELOPE" / "FLAGS" / "INTERNALDATE" /
                     "RFC822" [".HEADER" / ".SIZE" / ".TEXT"] /
                     "BODY" ["STRUCTURE"] / "UID" /
                     "BODY" [".PEEK"] section ["<" number "." nz-number ">"]

   flag            = "\Answered" / "\Flagged" / "\Deleted" /
                     "\Seen" / "\Draft" / flag-keyword / flag-extension
                       ; Does not include "\Recent"

   flag-extension  = "\" atom
                       ; Future expansion.  Client implementations
                       ; MUST accept flag-extension flags.  Server
                       ; implementations MUST NOT generate
                       ; flag-extension flags except as defined by
                       ; future standard or standards-track
                       ; revisions of this specification.

   flag-fetch      = flag / "\Recent"

   flag-keyword    = atom

   flag-list       = "(" [flag *(SP flag)] ")"

   flag-perm       = flag / "\*"

   greeting        = "*" SP (resp-cond-auth / resp-cond-bye) CRLF

   header-fld-name = astring

   header-list     = "(" header-fld-name *(SP header-fld-name) ")"

   list            = "LIST" SP mailbox SP list-mailbox

   list-mailbox    = 1*list-char / string

   list-char       = ATOM-CHAR / list-wildcards / resp-specials

   list-wildcards  = "%" / "*"

   literal         = "{" number "}" CRLF *CHAR8
                       ; Number represents the number of CHAR8s

   login           = "LOGIN" SP userid SP password

   lsub            = "LSUB" SP mailbox SP list-mailbox

   mailbox         = "INBOX" / astring
                       ; INBOX is case-insensitive.  All case variants of
                       ; INBOX (e.g. "iNbOx") MUST be interpreted as INBOX
                       ; not as an astring.  An astring which consists of
                       ; the case-insensitive sequence "I" "N" "B" "O" "X"
                       ; is considered to be INBOX and not an astring.
                       ;  Refer to section 5.1 for further
                       ; semantic details of mailbox names.

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

   number          = 1*DIGIT
                       ; Unsigned 32-bit integer
                       ; (0 <= n < 4,294,967,296)

   nz-number       = digit-nz *DIGIT
                       ; Non-zero unsigned 32-bit integer
                       ; (0 < n < 4,294,967,296)

   password        = astring

   quoted          = DQUOTE *QUOTED-CHAR DQUOTE

   QUOTED-CHAR     = <any TEXT-CHAR except quoted-specials> /
                     "\" quoted-specials

   quoted-specials = DQUOTE / "\"

   rename          = "RENAME" SP mailbox SP mailbox
                       ; Use of INBOX as a destination gives a NO error

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

   search          = "SEARCH" [SP "CHARSET" SP astring] 1*(SP search-key)
                       ; CHARSET argument to MUST be registered with IANA

   search-key      = "ALL" / "ANSWERED" / "BCC" SP astring /
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

   section         = "[" [section-spec] "]"

   section-msgtext = "HEADER" / "HEADER.FIELDS" [".NOT"] SP header-list /
                     "TEXT"
                       ; top-level or MESSAGE/RFC822 part

   section-part    = nz-number *("." nz-number)
                       ; body part nesting

   section-spec    = section-msgtext / (section-part ["." section-text])

   section-text    = section-msgtext / "MIME"
                       ; text other than actual body part (headers, etc.)

   select          = "SELECT" SP mailbox

   sequence-num    = nz-number / "*"
                       ; * is the largest number in use.  For message
                       ; sequence numbers, it is the number of messages
                       ; in the mailbox.  For unique identifiers, it is
                       ; the unique identifier of the last message in
                       ; the mailbox.

   set             = sequence-num / (sequence-num ":" sequence-num) /
                     (set "," set)
                       ; Identifies a set of messages.  For message
                       ; sequence numbers, these are consecutive
                       ; numbers from 1 to the number of messages in
                       ; the mailbox
                       ; Comma delimits individual numbers, colon
                       ; delimits between two numbers inclusive.
                       ; Example: 2,4:7,9,12:* is 2,4,5,6,7,9,12,13,
                       ; 14,15 for a mailbox with 15 messages.


   status          = "STATUS" SP mailbox SP "(" status-att *(SP status-att) ")"

   status-att      = "MESSAGES" / "RECENT" / "UIDNEXT" / "UIDVALIDITY" /
                     "UNSEEN"

   store           = "STORE" SP set SP store-att-flags

   store-att-flags = (["+" / "-"] "FLAGS" [".SILENT"]) SP
                     (flag-list / (flag *(SP flag)))

   string          = quoted / literal

   subscribe       = "SUBSCRIBE" SP mailbox

   tag             = 1*<any ASTRING-CHAR except "+">

   text            = 1*TEXT-CHAR

   TEXT-CHAR       = <any CHAR except CR and LF>

   time            = 2DIGIT ":" 2DIGIT ":" 2DIGIT
                       ; Hours minutes seconds

   uid             = "UID" SP (copy / fetch / search / store)
                       ; Unique identifiers used instead of message
                       ; sequence numbers

   uniqueid        = nz-number
                       ; Strictly ascending

   unsubscribe     = "UNSUBSCRIBE" SP mailbox

   userid          = astring

   x-command       = "X" atom <experimental command arguments>

   zone            = ("+" / "-") 4DIGIT
                       ; Signed four-digit value of hhmm representing
                       ; hours and minutes east of Greenwich (that is,
                       ; the amount that the given time differs from
                       ; Universal Time).  Subtracting the timezone
                       ; from the given time will give the UT form.
                       ; The Universal Time zone is "+0000".
*/


#ifndef MAILIMAP_TYPES_H

#define MAILIMAP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailstream.h>
#include <libetpan/clist.h>


/*
  IMPORTANT NOTE:
  
  All allocation functions will take as argument allocated data
  and will store these data in the structure they will allocate.
  Data should be persistant during all the use of the structure
  and will be freed by the free function of the structure

  allocation functions will return NULL on failure
*/


/*
  mailimap_address represents a mail address

  - personal_name is the name to display in an address
    '"name"' in '"name" <address@domain>', should be allocated
    with a malloc()
  
  - source_route is the source-route information in the
    mail address (RFC 822), should be allocated with a malloc()

  - mailbox_name is the name of the mailbox 'address' in
    '"name" <address@domain>', should be allocated with a malloc()

  - host_name is the name of the host 'domain' in
    '"name" <address@domain>', should be allocated with a malloc()

  if mailbox_name is not NULL and host_name is NULL, this is the name
  of a group, the next addresses in the list are elements of the group
  until we reach an address with a NULL mailbox_name.
*/

struct mailimap_address {
  char * ad_personal_name; /* can be NULL */
  char * ad_source_route;  /* can be NULL */
  char * ad_mailbox_name;  /* can be NULL */
  char * ad_host_name;     /* can be NULL */
};


struct mailimap_address *
mailimap_address_new(char * ad_personal_name, char * ad_source_route,
		     char * ad_mailbox_name, char * ad_host_name);

void mailimap_address_free(struct mailimap_address * addr);


/* this is the type of MIME body parsed by IMAP server */

enum {
  MAILIMAP_BODY_ERROR,
  MAILIMAP_BODY_1PART, /* single part */
  MAILIMAP_BODY_MPART  /* multi-part */
};

/*
  mailimap_body represent a MIME body parsed by IMAP server

  - type is the type of the MIME part (single part or multipart)

  - body_1part is defined if this is a single part

  - body_mpart is defined if this is a multipart
*/

struct mailimap_body {
  int bd_type;
  /* can be MAILIMAP_BODY_1PART or MAILIMAP_BODY_MPART */
  union {
    struct mailimap_body_type_1part * bd_body_1part; /* can be NULL */
    struct mailimap_body_type_mpart * bd_body_mpart; /* can be NULL */
  } bd_data;
};


struct mailimap_body *
mailimap_body_new(int bd_type,
		  struct mailimap_body_type_1part * bd_body_1part,
		  struct mailimap_body_type_mpart * bd_body_mpart);

void mailimap_body_free(struct mailimap_body * body);



/*
  this is the type of MIME body extension
*/

enum {
  MAILIMAP_BODY_EXTENSION_ERROR,
  MAILIMAP_BODY_EXTENSION_NSTRING, /* string */
  MAILIMAP_BODY_EXTENSION_NUMBER,  /* number */
  MAILIMAP_BODY_EXTENSION_LIST     /* list of
                                      (struct mailimap_body_extension *) */
};

/*
  mailimap_body_extension is a future extension header field value

  - type is the type of the body extension (string, number or
    list of extension)

  - nstring is a string value if the type is string

  - number is a integer value if the type is number

  - list is a list of body extension if the type is a list
*/

struct mailimap_body_extension {
  int ext_type;
  /*
    can be MAILIMAP_BODY_EXTENSION_NSTRING, MAILIMAP_BODY_EXTENSION_NUMBER
    or MAILIMAP_BODY_EXTENSION_LIST
  */
  union {
    char * ext_nstring;    /* can be NULL */
    uint32_t ext_number;
    clist * ext_body_extension_list;
    /* list of (struct mailimap_body_extension *) */
    /* can be NULL */
  } ext_data;
};

struct mailimap_body_extension *
mailimap_body_extension_new(int ext_type, char * ext_nstring,
    uint32_t ext_number,
    clist * ext_body_extension_list);

void mailimap_body_extension_free(struct mailimap_body_extension * be);


/*
  mailimap_body_ext_1part is the extended result part of a single part
  bodystructure.
  
  - body_md5 is the value of the Content-MD5 header field, should be 
    allocated with malloc()

  - body_disposition is the value of the Content-Disposition header field

  - body_language is the value of the Content-Language header field
  
  - body_extension_list is the list of extension fields value.
*/

struct mailimap_body_ext_1part {
  char * bd_md5;   /* != NULL */
  struct mailimap_body_fld_dsp * bd_disposition; /* can be NULL */
  struct mailimap_body_fld_lang * bd_language;   /* can be NULL */
  
  clist * bd_extension_list; /* list of (struct mailimap_body_extension *) */
                               /* can be NULL */
};

struct mailimap_body_ext_1part *
mailimap_body_ext_1part_new(char * bd_md5,
			    struct mailimap_body_fld_dsp * bd_disposition,
			    struct mailimap_body_fld_lang * bd_language,
			    clist * bd_extension_list);


void
mailimap_body_ext_1part_free(struct mailimap_body_ext_1part * body_ext_1part);


/*
  mailimap_body_ext_mpart is the extended result part of a multipart
  bodystructure.

  - body_parameter is the list of parameters of Content-Type header field
  
  - body_disposition is the value of Content-Disposition header field

  - body_language is the value of Content-Language header field

  - body_extension_list is the list of extension fields value.
*/

struct mailimap_body_ext_mpart {
  struct mailimap_body_fld_param * bd_parameter; /* != NULL */
  struct mailimap_body_fld_dsp * bd_disposition; /* can be NULL */
  struct mailimap_body_fld_lang * bd_language;   /* can be NULL */
  clist * bd_extension_list; /* list of (struct mailimap_body_extension *) */
                               /* can be NULL */
};

struct mailimap_body_ext_mpart *
mailimap_body_ext_mpart_new(struct mailimap_body_fld_param * bd_parameter,
			    struct mailimap_body_fld_dsp * bd_disposition,
			    struct mailimap_body_fld_lang * bd_language,
			    clist * bd_extension_list);

void
mailimap_body_ext_mpart_free(struct mailimap_body_ext_mpart * body_ext_mpart);


/*
  mailimap_body_fields is the MIME fields of a MIME part.
  
  - body_parameter is the list of parameters of Content-Type header field

  - body_id is the value of Content-ID header field, should be allocated
    with malloc()

  - body_description is the value of Content-Description header field,
    should be allocated with malloc()

  - body_encoding is the value of Content-Transfer-Encoding header field
  
  - body_disposition is the value of Content-Disposition header field

  - body_size is the size of the MIME part
*/

struct mailimap_body_fields {
  struct mailimap_body_fld_param * bd_parameter; /* != NULL */
  char * bd_id;                                  /* can be NULL */
  char * bd_description;                         /* can be NULL */
  struct mailimap_body_fld_enc * bd_encoding;    /* != NULL */
  uint32_t bd_size;
};

struct mailimap_body_fields *
mailimap_body_fields_new(struct mailimap_body_fld_param * bd_parameter,
			 char * bd_id,
			 char * bd_description,
			 struct mailimap_body_fld_enc * bd_encoding,
			 uint32_t bd_size);

void
mailimap_body_fields_free(struct mailimap_body_fields * body_fields);



/*
  mailimap_body_fld_dsp is the parsed value of the Content-Disposition field

  - disposition_type is the type of Content-Disposition
    (usually attachment or inline), should be allocated with malloc()

  - attributes is the list of Content-Disposition attributes
*/

struct mailimap_body_fld_dsp {
  char * dsp_type;                     /* != NULL */
  struct mailimap_body_fld_param * dsp_attributes; /* != NULL */
};

struct mailimap_body_fld_dsp *
mailimap_body_fld_dsp_new(char * dsp_type,
    struct mailimap_body_fld_param * dsp_attributes);

void mailimap_body_fld_dsp_free(struct mailimap_body_fld_dsp * bfd);



/* these are the different parsed values for Content-Transfer-Encoding */

enum {
  MAILIMAP_BODY_FLD_ENC_7BIT,             /* 7bit */
  MAILIMAP_BODY_FLD_ENC_8BIT,             /* 8bit */
  MAILIMAP_BODY_FLD_ENC_BINARY,           /* binary */
  MAILIMAP_BODY_FLD_ENC_BASE64,           /* base64 */
  MAILIMAP_BODY_FLD_ENC_QUOTED_PRINTABLE, /* quoted-printable */
  MAILIMAP_BODY_FLD_ENC_OTHER             /* other */
};

/*
  mailimap_body_fld_enc is a parsed value for Content-Transfer-Encoding

  - type is the kind of Content-Transfer-Encoding, this can be
    MAILIMAP_BODY_FLD_ENC_7BIT, MAILIMAP_BODY_FLD_ENC_8BIT,
    MAILIMAP_BODY_FLD_ENC_BINARY, MAILIMAP_BODY_FLD_ENC_BASE64,
    MAILIMAP_BODY_FLD_ENC_QUOTED_PRINTABLE or MAILIMAP_BODY_FLD_ENC_OTHER

  - in case of MAILIMAP_BODY_FLD_ENC_OTHER, this value is defined,
    should be allocated with malloc()
*/

struct mailimap_body_fld_enc {
  int enc_type;
  char * enc_value; /* can be NULL */
};

struct mailimap_body_fld_enc *
mailimap_body_fld_enc_new(int enc_type, char * enc_value);

void mailimap_body_fld_enc_free(struct mailimap_body_fld_enc * bfe);


/* this is the type of Content-Language header field value */

enum {
  MAILIMAP_BODY_FLD_LANG_ERROR,  /* error parse */
  MAILIMAP_BODY_FLD_LANG_SINGLE, /* single value */
  MAILIMAP_BODY_FLD_LANG_LIST    /* list of values */
};

/*
  mailimap_body_fld_lang is the parsed value of the Content-Language field

  - type is the type of content, this can be MAILIMAP_BODY_FLD_LANG_SINGLE
    if this is a single value or MAILIMAP_BODY_FLD_LANG_LIST if there are
    several values

  - single is the single value if the type is MAILIMAP_BODY_FLD_LANG_SINGLE,
    should be allocated with malloc()

  - list is the list of value if the type is MAILIMAP_BODY_FLD_LANG_LIST,
    all elements of the list should be allocated with malloc()
*/

struct mailimap_body_fld_lang {
  int lg_type;
  union {
    char * lg_single; /* can be NULL */
    clist * lg_list; /* list of string (char *), can be NULL */
  } lg_data;
};

struct mailimap_body_fld_lang *
mailimap_body_fld_lang_new(int lg_type, char * lg_single, clist * lg_list);

void
mailimap_body_fld_lang_free(struct mailimap_body_fld_lang * fld_lang);



/*
  mailimap_single_body_fld_param is a body field parameter
  
  - name is the name of the parameter, should be allocated with malloc()
  
  - value is the value of the parameter, should be allocated with malloc()
*/

struct mailimap_single_body_fld_param {
  char * pa_name;  /* != NULL */
  char * pa_value; /* != NULL */
};

struct mailimap_single_body_fld_param *
mailimap_single_body_fld_param_new(char * pa_name, char * pa_value);

void
mailimap_single_body_fld_param_free(struct mailimap_single_body_fld_param * p);


/*
  mailmap_body_fld_param is a list of parameters
  
  - list is the list of parameters.
*/

struct mailimap_body_fld_param {
  clist * pa_list; /* list of (struct mailimap_single_body_fld_param *) */
                /* != NULL */
};

struct mailimap_body_fld_param *
mailimap_body_fld_param_new(clist * pa_list);

void
mailimap_body_fld_param_free(struct mailimap_body_fld_param * fld_param);


/*
  this is the kind of single part: a text part
  (when Content-Type is text/xxx), a message part (when Content-Type is
  message/rfc2822) or a basic part (others than multpart/xxx)
*/

enum {
  MAILIMAP_BODY_TYPE_1PART_ERROR, /* parse error */
  MAILIMAP_BODY_TYPE_1PART_BASIC, /* others then multipart/xxx */
  MAILIMAP_BODY_TYPE_1PART_MSG,   /* message/rfc2822 */
  MAILIMAP_BODY_TYPE_1PART_TEXT   /* text/xxx */
};


/*
  mailimap_body_type_1part is 

  - type is the kind of single part, this can be
  MAILIMAP_BODY_TYPE_1PART_BASIC, MAILIMAP_BODY_TYPE_1PART_MSG or
  MAILIMAP_BODY_TYPE_1PART_TEXT.

  - body_type_basic is the basic part when type is
    MAILIMAP_BODY_TYPE_1PART_BASIC

  - body_type_msg is the message part when type is
    MAILIMAP_BODY_TYPE_1PART_MSG
    
  - body_type_text is the text part when type is
    MAILIMAP_BODY_TYPE_1PART_TEXT
*/

struct mailimap_body_type_1part {
  int bd_type;
  union {
    struct mailimap_body_type_basic * bd_type_basic; /* can be NULL */
    struct mailimap_body_type_msg * bd_type_msg;     /* can be NULL */
    struct mailimap_body_type_text * bd_type_text;   /* can be NULL */
  } bd_data;
  struct mailimap_body_ext_1part * bd_ext_1part;   /* can be NULL */
};

struct mailimap_body_type_1part *
mailimap_body_type_1part_new(int bd_type,
    struct mailimap_body_type_basic * bd_type_basic,
    struct mailimap_body_type_msg * bd_type_msg,
    struct mailimap_body_type_text * bd_type_text,
    struct mailimap_body_ext_1part * bd_ext_1part);

void
mailimap_body_type_1part_free(struct mailimap_body_type_1part * bt1p);



/*
  mailimap_body_type_basic is a basic field (with Content-Type other
  than multipart/xxx, message/rfc2822 and text/xxx

  - media_basic will be the MIME type of the part
  
  - body_fields will be the parsed fields of the MIME part
*/

struct mailimap_body_type_basic {
  struct mailimap_media_basic * bd_media_basic; /* != NULL */
  struct mailimap_body_fields * bd_fields; /* != NULL */
};

struct mailimap_body_type_basic *
mailimap_body_type_basic_new(struct mailimap_media_basic * bd_media_basic,
			     struct mailimap_body_fields * bd_fields);

void mailimap_body_type_basic_free(struct mailimap_body_type_basic *
				   body_type_basic);

/*
  mailimap_body_type_mpart is a MIME multipart.

  - body_list is the list of sub-parts.

  - media_subtype is the subtype of the multipart (for example
    in multipart/alternative, this is "alternative")
    
  - body_ext_mpart is the extended fields of the MIME multipart
*/

struct mailimap_body_type_mpart {
  clist * bd_list; /* list of (struct mailimap_body *) */
                     /* != NULL */
  char * bd_media_subtype; /* != NULL */
  struct mailimap_body_ext_mpart * bd_ext_mpart; /* can be NULL */
};

struct mailimap_body_type_mpart *
mailimap_body_type_mpart_new(clist * bd_list, char * bd_media_subtype,
    struct mailimap_body_ext_mpart * bd_ext_mpart);

void mailimap_body_type_mpart_free(struct mailimap_body_type_mpart *
    body_type_mpart);

/*
  mailimap_body_type_msg is a MIME message part

  - body_fields is the MIME fields of the MIME message part

  - envelope is the list of parsed RFC 822 fields of the MIME message

  - body is the sub-part of the message

  - body_lines is the number of lines of the message part
*/

struct mailimap_body_type_msg {
  struct mailimap_body_fields * bd_fields; /* != NULL */
  struct mailimap_envelope * bd_envelope;       /* != NULL */
  struct mailimap_body * bd_body;               /* != NULL */
  uint32_t bd_lines;
};

struct mailimap_body_type_msg *
mailimap_body_type_msg_new(struct mailimap_body_fields * bd_fields,
			   struct mailimap_envelope * bd_envelope,
			   struct mailimap_body * bd_body,
			   uint32_t bd_lines);

void
mailimap_body_type_msg_free(struct mailimap_body_type_msg * body_type_msg);



/*
  mailimap_body_type_text is a single MIME part where Content-Type is text/xxx

  - media-text is the subtype of the text part (for example, in "text/plain",
    this is "plain", should be allocated with malloc()

  - body_fields is the MIME fields of the MIME message part

  - body_lines is the number of lines of the message part
*/

struct mailimap_body_type_text {
  char * bd_media_text;                         /* != NULL */
  struct mailimap_body_fields * bd_fields; /* != NULL */
  uint32_t bd_lines;
};

struct mailimap_body_type_text *
mailimap_body_type_text_new(char * bd_media_text,
    struct mailimap_body_fields * bd_fields,
    uint32_t bd_lines);

void
mailimap_body_type_text_free(struct mailimap_body_type_text * body_type_text);



/* this is the type of capability field */

enum {
  MAILIMAP_CAPABILITY_AUTH_TYPE, /* when the capability is an
                                      authentication type */
  MAILIMAP_CAPABILITY_NAME       /* other type of capability */
};

/*
  mailimap_capability is a capability of the IMAP server

  - type is the type of capability, this is either a authentication type
    (MAILIMAP_CAPABILITY_AUTH_TYPE) or an other type of capability
    (MAILIMAP_CAPABILITY_NAME)

  - auth_type is a type of authentication "name" in "AUTH=name",
    auth_type can be for example "PLAIN", when this is an authentication type,
    should be allocated with malloc()

  - name is a type of capability when this is not an authentication type,
    should be allocated with malloc()
*/

struct mailimap_capability {
  int cap_type;
  union {
    char * cap_auth_type; /* can be NULL */
    char * cap_name;      /* can be NULL */
  } cap_data;
};

struct mailimap_capability *
mailimap_capability_new(int cap_type, char * cap_auth_type, char * cap_name);

void mailimap_capability_free(struct mailimap_capability * c);




/*
  mailimap_capability_data is a list of capability

  - list is the list of capability
*/

struct mailimap_capability_data {
  clist * cap_list; /* list of (struct mailimap_capability *), != NULL */
};

struct mailimap_capability_data *
mailimap_capability_data_new(clist * cap_list);

void
mailimap_capability_data_free(struct mailimap_capability_data * cap_data);



/* this is the type of continue request data */

enum {
  MAILIMAP_CONTINUE_REQ_ERROR,  /* on parse error */ 
  MAILIMAP_CONTINUE_REQ_TEXT,   /* when data is a text response */
  MAILIMAP_CONTINUE_REQ_BASE64  /* when data is a base64 response */
};

/*
  mailimap_continue_req is a continue request (a response prefixed by "+")

  - type is the type of continue request response
    MAILIMAP_CONTINUE_REQ_TEXT (when information data is text),
    MAILIMAP_CONTINUE_REQ_BASE64 (when information data is base64)
  
  - text is the information of type text in case of text data

  - base64 is base64 encoded data in the other case, should be allocated
    with malloc()
*/

struct mailimap_continue_req {
  int cr_type;
  union {
    struct mailimap_resp_text * cr_text; /* can be NULL */
    char * cr_base64;                    /* can be NULL */
  } cr_data;
};

struct mailimap_continue_req *
mailimap_continue_req_new(int cr_type, struct mailimap_resp_text * cr_text,
			  char * cr_base64);

void mailimap_continue_req_free(struct mailimap_continue_req * cont_req);


/*
  mailimap_date_time is a date
  
  - day is the day of month (1 to 31)

  - month (1 to 12)

  - year (4 digits)

  - hour (0 to 23)
  
  - min (0 to 59)

  - sec (0 to 59)

  - zone (this is the decimal value that we can read, for example:
    for "-0200", the value is -200)
*/

struct mailimap_date_time {
  int dt_day;
  int dt_month;
  int dt_year;
  int dt_hour;
  int dt_min;
  int dt_sec;
  int dt_zone;
};

struct mailimap_date_time *
mailimap_date_time_new(int dt_day, int dt_month, int dt_year, int dt_hour,
		       int dt_min, int dt_sec, int dt_zone);

void mailimap_date_time_free(struct mailimap_date_time * date_time);



/*
  mailimap_envelope is the list of fields that can be parsed by
  the IMAP server.

  - date is the (non-parsed) content of the "Date" header field,
    should be allocated with malloc()
  
  - subject is the subject of the message, should be allocated with
    malloc()
  
  - sender is the the parsed content of the "Sender" field

  - reply-to is the parsed content of the "Reply-To" field

  - to is the parsed content of the "To" field

  - cc is the parsed content of the "Cc" field
  
  - bcc is the parsed content of the "Bcc" field
  
  - in_reply_to is the content of the "In-Reply-To" field,
    should be allocated with malloc()

  - message_id is the content of the "Message-ID" field,
    should be allocated with malloc()
*/

struct mailimap_envelope {
  char * env_date;                             /* can be NULL */
  char * env_subject;                          /* can be NULL */
  struct mailimap_env_from * env_from;         /* can be NULL */
  struct mailimap_env_sender * env_sender;     /* can be NULL */
  struct mailimap_env_reply_to * env_reply_to; /* can be NULL */
  struct mailimap_env_to * env_to;             /* can be NULL */
  struct mailimap_env_cc * env_cc;             /* can be NULL */
  struct mailimap_env_bcc * env_bcc;           /* can be NULL */
  char * env_in_reply_to;                      /* can be NULL */
  char * env_message_id;                       /* can be NULL */
};

struct mailimap_envelope *
mailimap_envelope_new(char * env_date, char * env_subject,
		      struct mailimap_env_from * env_from,
		      struct mailimap_env_sender * env_sender,
		      struct mailimap_env_reply_to * env_reply_to,
		      struct mailimap_env_to * env_to,
		      struct mailimap_env_cc* env_cc,
		      struct mailimap_env_bcc * env_bcc,
		      char * env_in_reply_to, char * env_message_id);

void mailimap_envelope_free(struct mailimap_envelope * env);



/*
  mailimap_env_bcc is the parsed "Bcc" field
  
  - list is the list of addresses
*/

struct mailimap_env_bcc {
  clist * bcc_list; /* list of (struct mailimap_address *), != NULL */
};

struct mailimap_env_bcc * mailimap_env_bcc_new(clist * bcc_list);

void mailimap_env_bcc_free(struct mailimap_env_bcc * env_bcc);


/*
  mailimap_env_cc is the parsed "Cc" field
  
  - list is the list of addresses
*/

struct mailimap_env_cc {
  clist * cc_list; /* list of (struct mailimap_address *), != NULL */
};

struct mailimap_env_cc * mailimap_env_cc_new(clist * cc_list);

void mailimap_env_cc_free(struct mailimap_env_cc * env_cc);



/*
  mailimap_env_from is the parsed "From" field
  
  - list is the list of addresses
*/

struct mailimap_env_from {
  clist * frm_list; /* list of (struct mailimap_address *) */
                /* != NULL */
};

struct mailimap_env_from * mailimap_env_from_new(clist * frm_list);

void mailimap_env_from_free(struct mailimap_env_from * env_from);



/*
  mailimap_env_reply_to is the parsed "Reply-To" field
  
  - list is the list of addresses
*/

struct mailimap_env_reply_to {
  clist * rt_list; /* list of (struct mailimap_address *), != NULL */
};

struct mailimap_env_reply_to * mailimap_env_reply_to_new(clist * rt_list);

void
mailimap_env_reply_to_free(struct mailimap_env_reply_to * env_reply_to);



/*
  mailimap_env_sender is the parsed "Sender" field
  
  - list is the list of addresses
*/

struct mailimap_env_sender {
  clist * snd_list; /* list of (struct mailimap_address *), != NULL */
};

struct mailimap_env_sender * mailimap_env_sender_new(clist * snd_list);

void mailimap_env_sender_free(struct mailimap_env_sender * env_sender);



/*
  mailimap_env_to is the parsed "To" field
  
  - list is the list of addresses
*/

struct mailimap_env_to {
  clist * to_list; /* list of (struct mailimap_address *), != NULL */
};

struct mailimap_env_to * mailimap_env_to_new(clist * to_list);

void mailimap_env_to_free(struct mailimap_env_to * env_to);


/* this is the type of flag */

enum {
  MAILIMAP_FLAG_ANSWERED,  /* \Answered flag */
  MAILIMAP_FLAG_FLAGGED,   /* \Flagged flag */
  MAILIMAP_FLAG_DELETED,   /* \Deleted flag */
  MAILIMAP_FLAG_SEEN,      /* \Seen flag */
  MAILIMAP_FLAG_DRAFT,     /* \Draft flag */
  MAILIMAP_FLAG_KEYWORD,   /* keyword flag */
  MAILIMAP_FLAG_EXTENSION  /* \extension flag */
};


/*
  mailimap_flag is a message flag (that we can associate with a message)
  
  - type is the type of the flag, MAILIMAP_FLAG_XXX

  - keyword is the flag when the flag is of keyword type,
    should be allocated with malloc()
  
  - extension is the flag when the flag is of extension type, should be
    allocated with malloc()
*/

struct mailimap_flag {
  int fl_type;
  union {
    char * fl_keyword;   /* can be NULL */
    char * fl_extension; /* can be NULL */
  } fl_data;
};

struct mailimap_flag * mailimap_flag_new(int fl_type,
    char * fl_keyword, char * fl_extension);

void mailimap_flag_free(struct mailimap_flag * f);




/* this is the type of flag */

enum {
  MAILIMAP_FLAG_FETCH_ERROR,  /* on parse error */
  MAILIMAP_FLAG_FETCH_RECENT, /* \Recent flag */
  MAILIMAP_FLAG_FETCH_OTHER   /* other type of flag */
};

/*
  mailimap_flag_fetch is a message flag (when we fetch it)

  - type is the type of flag fetch
  
  - flag is the flag when this is not a \Recent flag
*/

struct mailimap_flag_fetch {
  int fl_type;
  struct mailimap_flag * fl_flag; /* can be NULL */
};

struct mailimap_flag_fetch *
mailimap_flag_fetch_new(int fl_type, struct mailimap_flag * fl_flag);

void mailimap_flag_fetch_free(struct mailimap_flag_fetch * flag_fetch);




/* this is the type of flag */

enum {
  MAILIMAP_FLAG_PERM_ERROR, /* on parse error */
  MAILIMAP_FLAG_PERM_FLAG,  /* to specify that usual flags can be changed */
  MAILIMAP_FLAG_PERM_ALL    /* to specify that new flags can be created */
};


/*
  mailimap_flag_perm is a flag returned in case of PERMANENTFLAGS response
  
  - type is the type of returned PERMANENTFLAGS, it can be
    MAILIMAP_FLAG_PERM_FLAG (the given flag can be changed permanently) or
    MAILIMAP_FLAG_PERM_ALL (new flags can be created)
  
  - flag is the given flag when type is MAILIMAP_FLAG_PERM_FLAG
*/

struct mailimap_flag_perm {
  int fl_type;
  struct mailimap_flag * fl_flag; /* can be NULL */
};

struct mailimap_flag_perm *
mailimap_flag_perm_new(int fl_type, struct mailimap_flag * fl_flag);

void mailimap_flag_perm_free(struct mailimap_flag_perm * flag_perm);


/*
  mailimap_flag_list is a list of flags
  
  - list is a list of flags
*/

struct mailimap_flag_list {
  clist * fl_list; /* list of (struct mailimap_flag *), != NULL */
};

struct mailimap_flag_list *
mailimap_flag_list_new(clist * fl_list);

void mailimap_flag_list_free(struct mailimap_flag_list * flag_list);




/* this is the type of greeting response */

enum {
  MAILIMAP_GREETING_RESP_COND_ERROR, /* on parse error */
  MAILIMAP_GREETING_RESP_COND_AUTH,  /* when connection is accepted */
  MAILIMAP_GREETING_RESP_COND_BYE    /* when connection is refused */
};

/*
  mailimap_greeting is the response returned on connection

  - type is the type of response on connection, either
  MAILIMAP_GREETING_RESP_COND_AUTH if connection is accepted or
  MAIMIMAP_GREETING_RESP_COND_BYE if connection is refused
*/

struct mailimap_greeting {
  int gr_type;
  union {
    struct mailimap_resp_cond_auth * gr_auth; /* can be NULL */
    struct mailimap_resp_cond_bye * gr_bye;   /* can be NULL */
  } gr_data;
};

struct mailimap_greeting *
mailimap_greeting_new(int gr_type,
    struct mailimap_resp_cond_auth * gr_auth,
    struct mailimap_resp_cond_bye * gr_bye);

void mailimap_greeting_free(struct mailimap_greeting * greeting);


/*
  mailimap_header_list is a list of headers that can be specified when
  we want to fetch fields

  - list is a list of header names, each header name should be allocated
    with malloc()
*/

struct mailimap_header_list {
  clist * hdr_list; /* list of astring (char *), != NULL */
};

struct mailimap_header_list *
mailimap_header_list_new(clist * hdr_list);

void
mailimap_header_list_free(struct mailimap_header_list * header_list);



/* this is the type of mailbox STATUS that can be returned */

enum {
  MAILIMAP_STATUS_ATT_MESSAGES,    /* when requesting the number of
                                      messages */
  MAILIMAP_STATUS_ATT_RECENT,      /* when requesting the number of
                                      recent messages */
  MAILIMAP_STATUS_ATT_UIDNEXT,     /* when requesting the next unique
                                      identifier */
  MAILIMAP_STATUS_ATT_UIDVALIDITY, /* when requesting the validity of
                                      message unique identifiers*/
  MAILIMAP_STATUS_ATT_UNSEEN       /* when requesting the number of
                                      unseen messages */
};

/*
  mailimap_status_info is a returned information when a STATUS of 
  a mailbox is requested

  - att is the type of mailbox STATUS, the value can be 
    MAILIMAP_STATUS_ATT_MESSAGES, MAILIMAP_STATUS_ATT_RECENT,
    MAILIMAP_STATUS_ATT_UIDNEXT, MAILIMAP_STATUS_ATT_UIDVALIDITY or
    MAILIMAP_STATUS_ATT_UNSEEN

  - value is the value of the given information
*/

struct mailimap_status_info {
  int st_att;
  uint32_t st_value;
};

struct mailimap_status_info *
mailimap_status_info_new(int st_att, uint32_t st_value);

void mailimap_status_info_free(struct mailimap_status_info * info);



/*
  mailimap_mailbox_data_status is the list of information returned
  when a STATUS of a mailbox is requested

  - mailbox is the name of the mailbox, should be allocated with malloc()
  
  - status_info_list is the list of information returned
*/

struct mailimap_mailbox_data_status {
  char * st_mailbox;
  clist * st_info_list; /* list of (struct mailimap_status_info *) */
                            /* can be NULL */
};

struct mailimap_mailbox_data_status *
mailimap_mailbox_data_status_new(char * st_mailbox,
    clist * st_info_list);

void
mailimap_mailbox_data_status_free(struct mailimap_mailbox_data_status * info);



/* this is the type of mailbox information that is returned */

enum {
  MAILIMAP_MAILBOX_DATA_ERROR,  /* on parse error */
  MAILIMAP_MAILBOX_DATA_FLAGS,  /* flag that are applicable to the mailbox */
  MAILIMAP_MAILBOX_DATA_LIST,   /* this is a mailbox in the list of mailboxes
                                   returned on LIST command*/
  MAILIMAP_MAILBOX_DATA_LSUB,   /* this is a mailbox in the list of
                                   subscribed mailboxes returned on LSUB
                                   command */
  MAILIMAP_MAILBOX_DATA_SEARCH, /* this is a list of messages numbers or
                                   unique identifiers returned
                                   on a SEARCH command*/
  MAILIMAP_MAILBOX_DATA_STATUS, /* this is the list of information returned
                                   on a STATUS command */
  MAILIMAP_MAILBOX_DATA_EXISTS, /* this is the number of messages in the
                                   mailbox */
  MAILIMAP_MAILBOX_DATA_RECENT, /* this is the number of recent messages
                                   in the mailbox */
  MAILIMAP_MAILBOX_DATA_EXTENSION_DATA  /* this mailbox-data stores data
                                           returned by an extension */
};

/*
  mailimap_mailbox_data is an information related to a mailbox
  
  - type is the type of mailbox_data that is filled, the value of this field
    can be MAILIMAP_MAILBOX_DATA_FLAGS, MAILIMAP_MAILBOX_DATA_LIST,
    MAILIMAP_MAILBOX_DATA_LSUB, MAILIMAP_MAILBOX_DATA_SEARCH,
    MAILIMAP_MAILBOX_DATA_STATUS, MAILIMAP_MAILBOX_DATA_EXISTS
    or MAILIMAP_MAILBOX_DATA_RECENT.

  - flags is the flags that are applicable to the mailbox when
    type is MAILIMAP_MAILBOX_DATA_FLAGS

  - list is a mailbox in the list of mailboxes returned on LIST command
    when type is MAILIMAP_MAILBOX_DATA_LIST

  - lsub is a mailbox in the list of subscribed mailboxes returned on
    LSUB command when type is MAILIMAP_MAILBOX_DATA_LSUB

  - search is a list of messages numbers or unique identifiers returned
    on SEARCH command when type MAILIMAP_MAILBOX_DATA_SEARCH, each element
    should be allocated with malloc()

  - status is a list of information returned on STATUS command when
    type is MAILIMAP_MAILBOX_DATA_STATUS

  - exists is the number of messages in the mailbox when type
    is MAILIMAP_MAILBOX_DATA_EXISTS

  - recent is the number of recent messages in the mailbox when type
    is MAILIMAP_MAILBOX_DATA_RECENT
*/

struct mailimap_mailbox_data {
  int mbd_type;
  union {
    struct mailimap_flag_list * mbd_flags;   /* can be NULL */
    struct mailimap_mailbox_list * mbd_list; /* can be NULL */
    struct mailimap_mailbox_list * mbd_lsub; /* can be NULL */
    clist * mbd_search;  /* list of nz-number (uint32_t *), can be NULL */
    struct mailimap_mailbox_data_status *  mbd_status; /* can be NULL */
    uint32_t mbd_exists;
    uint32_t mbd_recent;
    struct mailimap_extension_data * mbd_extension; /* can be NULL */
  } mbd_data;
};

struct mailimap_mailbox_data *
mailimap_mailbox_data_new(int mbd_type, struct mailimap_flag_list * mbd_flags,
    struct mailimap_mailbox_list * mbd_list,
    struct mailimap_mailbox_list * mbd_lsub,
    clist * mbd_search,
    struct mailimap_mailbox_data_status * mbd_status,
    uint32_t mbd_exists,
    uint32_t mbd_recent,
    struct mailimap_extension_data * mbd_extension);

void
mailimap_mailbox_data_free(struct mailimap_mailbox_data * mb_data);



/* this is the type of mailbox flags */

enum {
  MAILIMAP_MBX_LIST_FLAGS_SFLAG,    /* mailbox single flag - a flag in
                                       {\NoSelect, \Marked, \Unmarked} */
  MAILIMAP_MBX_LIST_FLAGS_NO_SFLAG  /* mailbox other flag -  mailbox flag
                                       other than \NoSelect \Marked and
                                       \Unmarked) */
};

/* this is a single flag type */

enum {
  MAILIMAP_MBX_LIST_SFLAG_ERROR,
  MAILIMAP_MBX_LIST_SFLAG_MARKED,
  MAILIMAP_MBX_LIST_SFLAG_NOSELECT,
  MAILIMAP_MBX_LIST_SFLAG_UNMARKED
};

/*
  mailimap_mbx_list_flags is a mailbox flag

  - type is the type of mailbox flag, it can be MAILIMAP_MBX_LIST_FLAGS_SFLAG,
    or MAILIMAP_MBX_LIST_FLAGS_NO_SFLAG.

  - oflags is a list of "mailbox other flag"
  
  - sflag is a mailbox single flag
*/

struct mailimap_mbx_list_flags {
  int mbf_type;
  clist * mbf_oflags; /* list of
                         (struct mailimap_mbx_list_oflag *), != NULL */
  int mbf_sflag;
};

struct mailimap_mbx_list_flags *
mailimap_mbx_list_flags_new(int mbf_type,
    clist * mbf_oflags, int mbf_sflag);

void
mailimap_mbx_list_flags_free(struct mailimap_mbx_list_flags * mbx_list_flags);



/* this is the type of the mailbox other flag */

enum {
  MAILIMAP_MBX_LIST_OFLAG_ERROR,       /* on parse error */
  MAILIMAP_MBX_LIST_OFLAG_NOINFERIORS, /* \NoInferior flag */
  MAILIMAP_MBX_LIST_OFLAG_FLAG_EXT     /* other flag */
};

/*
  mailimap_mbx_list_oflag is a mailbox other flag

  - type can be MAILIMAP_MBX_LIST_OFLAG_NOINFERIORS when this is 
    a \NoInferior flag or MAILIMAP_MBX_LIST_OFLAG_FLAG_EXT

  - flag_ext is set when MAILIMAP_MBX_LIST_OFLAG_FLAG_EXT and is
    an extension flag, should be allocated with malloc()
*/

struct mailimap_mbx_list_oflag {
  int of_type;
  char * of_flag_ext; /* can be NULL */
};

struct mailimap_mbx_list_oflag *
mailimap_mbx_list_oflag_new(int of_type, char * of_flag_ext);

void
mailimap_mbx_list_oflag_free(struct mailimap_mbx_list_oflag * oflag);



/*
  mailimap_mailbox_list is a list of mailbox flags

  - mb_flag is a list of mailbox flags

  - delimiter is the delimiter of the mailbox path

  - mb is the name of the mailbox, should be allocated with malloc()
*/

struct mailimap_mailbox_list {
  struct mailimap_mbx_list_flags * mb_flag; /* can be NULL */
  char mb_delimiter;
  char * mb_name; /* != NULL */
};

struct mailimap_mailbox_list *
mailimap_mailbox_list_new(struct mailimap_mbx_list_flags * mbx_flags,
    char mb_delimiter, char * mb_name);

void
mailimap_mailbox_list_free(struct mailimap_mailbox_list * mb_list);



/* this is the MIME type */

enum {
  MAILIMAP_MEDIA_BASIC_APPLICATION, /* application/xxx */
  MAILIMAP_MEDIA_BASIC_AUDIO,       /* audio/xxx */
  MAILIMAP_MEDIA_BASIC_IMAGE,       /* image/xxx */
  MAILIMAP_MEDIA_BASIC_MESSAGE,     /* message/xxx */
  MAILIMAP_MEDIA_BASIC_VIDEO,       /* video/xxx */
  MAILIMAP_MEDIA_BASIC_OTHER        /* for all other cases */
};


/*
  mailimap_media_basic is the MIME type

  - type can be MAILIMAP_MEDIA_BASIC_APPLICATION, MAILIMAP_MEDIA_BASIC_AUDIO,
    MAILIMAP_MEDIA_BASIC_IMAGE, MAILIMAP_MEDIA_BASIC_MESSAGE,
    MAILIMAP_MEDIA_BASIC_VIDEO or MAILIMAP_MEDIA_BASIC_OTHER

  - basic_type is defined when type is MAILIMAP_MEDIA_BASIC_OTHER, should
    be allocated with malloc()

  - subtype is the subtype of the MIME type, for example, this is
    "data" in "application/data", should be allocated with malloc()
*/

struct mailimap_media_basic {
  int med_type;
  char * med_basic_type; /* can be NULL */
  char * med_subtype;    /* != NULL */
};

struct mailimap_media_basic *
mailimap_media_basic_new(int med_type,
    char * med_basic_type, char * med_subtype);

void
mailimap_media_basic_free(struct mailimap_media_basic * media_basic);



/* this is the type of message data */

enum {
  MAILIMAP_MESSAGE_DATA_ERROR,
  MAILIMAP_MESSAGE_DATA_EXPUNGE,
  MAILIMAP_MESSAGE_DATA_FETCH
};

/*
  mailimap_message_data is an information related to a message

  - number is the number or the unique identifier of the message
  
  - type is the type of information, this value can be
    MAILIMAP_MESSAGE_DATA_EXPUNGE or MAILIMAP_MESSAGE_DATA_FETCH
    
  - msg_att is the message data
*/

struct mailimap_message_data {
  uint32_t mdt_number;
  int mdt_type;
  struct mailimap_msg_att * mdt_msg_att; /* can be NULL */
                                     /* if type = EXPUNGE, can be NULL */
};

struct mailimap_message_data *
mailimap_message_data_new(uint32_t mdt_number, int mdt_type,
    struct mailimap_msg_att * mdt_msg_att);

void
mailimap_message_data_free(struct mailimap_message_data * msg_data);



/* this the type of the message attributes */

enum {
  MAILIMAP_MSG_ATT_ITEM_ERROR,   /* on parse error */
  MAILIMAP_MSG_ATT_ITEM_DYNAMIC, /* dynamic message attributes (flags) */
  MAILIMAP_MSG_ATT_ITEM_STATIC   /* static messages attributes
                                    (message content) */
};

/*
  mailimap_msg_att_item is a message attribute

  - type is the type of message attribute, the value can be
    MAILIMAP_MSG_ATT_ITEM_DYNAMIC or MAILIMAP_MSG_ATT_ITEM_STATIC
  
  - msg_att_dyn is a dynamic message attribute when type is
    MAILIMAP_MSG_ATT_ITEM_DYNAMIC

  - msg_att_static is a static message attribute when type is
    MAILIMAP_MSG_ATT_ITEM_STATIC
*/

struct mailimap_msg_att_item {
  int att_type;
  union {
    struct mailimap_msg_att_dynamic * att_dyn;   /* can be NULL */
    struct mailimap_msg_att_static * att_static; /* can be NULL */
  } att_data;
};

struct mailimap_msg_att_item *
mailimap_msg_att_item_new(int att_type,
    struct mailimap_msg_att_dynamic * att_dyn,
    struct mailimap_msg_att_static * att_static);

void
mailimap_msg_att_item_free(struct mailimap_msg_att_item * item);


/*
  mailimap_msg_att is a list of attributes
  
  - list is a list of message attributes

  - number is the message number or unique identifier, this field
    has been added for implementation purpose
*/

struct mailimap_msg_att {
  clist * att_list; /* list of (struct mailimap_msg_att_item *) */
                /* != NULL */
  uint32_t att_number; /* extra field to store the message number,
		     used for mailimap */
};

struct mailimap_msg_att * mailimap_msg_att_new(clist * att_list);

void mailimap_msg_att_free(struct mailimap_msg_att * msg_att);


/*
  mailimap_msg_att_dynamic is a dynamic message attribute
  
  - list is a list of flags (that have been fetched)
*/

struct mailimap_msg_att_dynamic {
  clist * att_list; /* list of (struct mailimap_flag_fetch *) */
  /* can be NULL */
};

struct mailimap_msg_att_dynamic *
mailimap_msg_att_dynamic_new(clist * att_list);

void
mailimap_msg_att_dynamic_free(struct mailimap_msg_att_dynamic * msg_att_dyn);



/*
  mailimap_msg_att_body_section is a MIME part content
  
  - section is the location of the MIME part in the message
  
  - origin_octet is the offset of the requested part of the MIME part
  
  - body_part is the content or partial content of the MIME part,
    should be allocated through a MMAPString

  - length is the size of the content
*/

struct mailimap_msg_att_body_section {
  struct mailimap_section * sec_section; /* != NULL */
  uint32_t sec_origin_octet;
  char * sec_body_part; /* can be NULL */
  size_t sec_length;
};

struct mailimap_msg_att_body_section *
mailimap_msg_att_body_section_new(struct mailimap_section * section,
    uint32_t sec_origin_octet,
    char * sec_body_part,
    size_t sec_length);

void
mailimap_msg_att_body_section_free(struct mailimap_msg_att_body_section * 
    msg_att_body_section);



/*
  this is the type of static message attribute
*/

enum {
  MAILIMAP_MSG_ATT_ERROR,         /* on parse error */
  MAILIMAP_MSG_ATT_ENVELOPE,      /* this is the fields that can be
                                    parsed by the server */
  MAILIMAP_MSG_ATT_INTERNALDATE,  /* this is the message date kept
                                     by the server */
  MAILIMAP_MSG_ATT_RFC822,        /* this is the message content
                                     (header and body) */
  MAILIMAP_MSG_ATT_RFC822_HEADER, /* this is the message header */
  MAILIMAP_MSG_ATT_RFC822_TEXT,   /* this is the message text part */
  MAILIMAP_MSG_ATT_RFC822_SIZE,   /* this is the size of the message content */
  MAILIMAP_MSG_ATT_BODY,          /* this is the MIME description of
                                     the message */
  MAILIMAP_MSG_ATT_BODYSTRUCTURE, /* this is the MIME description of the
                                     message with additional information */
  MAILIMAP_MSG_ATT_BODY_SECTION,  /* this is a MIME part content */
  MAILIMAP_MSG_ATT_UID            /* this is the message unique identifier */
};

/*
  mailimap_msg_att_static is a given part of the message
  
  - type is the type of the static message attribute, the value can be 
    MAILIMAP_MSG_ATT_ENVELOPE, MAILIMAP_MSG_ATT_INTERNALDATE,
    MAILIMAP_MSG_ATT_RFC822, MAILIMAP_MSG_ATT_RFC822_HEADER,
    MAILIMAP_MSG_ATT_RFC822_TEXT, MAILIMAP_MSG_ATT_RFC822_SIZE,
    MAILIMAP_MSG_ATT_BODY, MAILIMAP_MSG_ATT_BODYSTRUCTURE,
    MAILIMAP_MSG_ATT_BODY_SECTION, MAILIMAP_MSG_ATT_UID

  - env is the headers parsed by the server if type is
    MAILIMAP_MSG_ATT_ENVELOPE

  - internal_date is the date of message kept by the server if type is
    MAILIMAP_MSG_ATT_INTERNALDATE

  - rfc822 is the message content if type is MAILIMAP_MSG_ATT_RFC822,
    should be allocated through a MMAPString

  - rfc822_header is the message header if type is
    MAILIMAP_MSG_ATT_RFC822_HEADER, should be allocated through a MMAPString

  - rfc822_text is the message text part if type is
    MAILIMAP_MSG_ATT_RFC822_TEXT, should be allocated through a MMAPString

  - rfc822_size is the message size if type is MAILIMAP_MSG_ATT_SIZE

  - body is the MIME description of the message

  - bodystructure is the MIME description of the message with additional
    information

  - body_section is a MIME part content

  - uid is a unique message identifier
*/

struct mailimap_msg_att_static {
  int att_type;
  union {
    struct mailimap_envelope * att_env;            /* can be NULL */
    struct mailimap_date_time * att_internal_date; /* can be NULL */
    struct {
      char * att_content; /* can be NULL */
      size_t att_length;
    } att_rfc822;        
    struct {
      char * att_content; /* can be NULL */
      size_t att_length;
    } att_rfc822_header;
    struct {
      char * att_content; /* can be NULL */
      size_t att_length;
    } att_rfc822_text;
    uint32_t att_rfc822_size;
    struct mailimap_body * att_bodystructure; /* can be NULL */
    struct mailimap_body * att_body;          /* can be NULL */
    struct mailimap_msg_att_body_section * att_body_section; /* can be NULL */
    uint32_t att_uid;
  } att_data;
};

struct mailimap_msg_att_static *
mailimap_msg_att_static_new(int att_type, struct mailimap_envelope * att_env,
    struct mailimap_date_time * att_internal_date,
    char * att_rfc822,
    char * att_rfc822_header,
    char * att_rfc822_text,
    size_t att_length,
    uint32_t att_rfc822_size,
    struct mailimap_body * att_bodystructure,
    struct mailimap_body * att_body,
    struct mailimap_msg_att_body_section * att_body_section,
    uint32_t att_uid);

void
mailimap_msg_att_static_free(struct mailimap_msg_att_static * item);



/* this is the type of a response element */

enum {
  MAILIMAP_RESP_ERROR,     /* on parse error */
  MAILIMAP_RESP_CONT_REQ,  /* continuation request */
  MAILIMAP_RESP_RESP_DATA  /* response data */
};

/*
  mailimap_cont_req_or_resp_data is a response element
  
  - type is the type of response, the value can be MAILIMAP_RESP_CONT_REQ
    or MAILIMAP_RESP_RESP_DATA

  - cont_req is a continuation request

  - resp_data is a reponse data
*/

struct mailimap_cont_req_or_resp_data {
  int rsp_type;
  union {
    struct mailimap_continue_req * rsp_cont_req;   /* can be NULL */
    struct mailimap_response_data * rsp_resp_data; /* can be NULL */
  } rsp_data;
};

struct mailimap_cont_req_or_resp_data *
mailimap_cont_req_or_resp_data_new(int rsp_type,
    struct mailimap_continue_req * rsp_cont_req,
    struct mailimap_response_data * rsp_resp_data);

void
mailimap_cont_req_or_resp_data_free(struct mailimap_cont_req_or_resp_data *
				    cont_req_or_resp_data);


/*
  mailimap_response is a list of response elements

  - cont_req_or_resp_data_list is a list of response elements

  - resp_done is an ending response element
*/

struct mailimap_response {
  clist * rsp_cont_req_or_resp_data_list;
  /* list of (struct mailiap_cont_req_or_resp_data *) */
                                   /* can be NULL */
  struct mailimap_response_done * rsp_resp_done; /* != NULL */
};

struct mailimap_response *
mailimap_response_new(clist * rsp_cont_req_or_resp_data_list,
    struct mailimap_response_done * rsp_resp_done);

void
mailimap_response_free(struct mailimap_response * resp);



/* this is the type of an untagged response */

enum {
  MAILIMAP_RESP_DATA_TYPE_ERROR,           /* on parse error */
  MAILIMAP_RESP_DATA_TYPE_COND_STATE,      /* condition state response */
  MAILIMAP_RESP_DATA_TYPE_COND_BYE,        /* BYE response (server is about
                                              to close the connection) */
  MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA,    /* response related to a mailbox */
  MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA,    /* response related to a message */
  MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA, /* capability information */
  MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA   /* data parsed by extension */
};

/*
  mailimap_reponse_data is an untagged response

  - type is the type of the untagged response, it can be
    MAILIMAP_RESP_DATA_COND_STATE, MAILIMAP_RESP_DATA_COND_BYE,
    MAILIMAP_RESP_DATA_MAILBOX_DATA, MAILIMAP_RESP_DATA_MESSAGE_DATA
    or MAILIMAP_RESP_DATA_CAPABILITY_DATA

  - cond_state is a condition state response

  - bye is a BYE response (server is about to close the connection)
  
  - mailbox_data is a response related to a mailbox

  - message_data is a response related to a message

  - capability is information about capabilities
*/

struct mailimap_response_data {
  int rsp_type;
  union {
    struct mailimap_resp_cond_state * rsp_cond_state;      /* can be NULL */
    struct mailimap_resp_cond_bye * rsp_bye;               /* can be NULL */
    struct mailimap_mailbox_data * rsp_mailbox_data;       /* can be NULL */
    struct mailimap_message_data * rsp_message_data;       /* can be NULL */
    struct mailimap_capability_data * rsp_capability_data; /* can be NULL */
    struct mailimap_extension_data * rsp_extension_data;   /* can be NULL */
  } rsp_data;
};

struct mailimap_response_data *
mailimap_response_data_new(int rsp_type,
    struct mailimap_resp_cond_state * rsp_cond_state,
    struct mailimap_resp_cond_bye * rsp_bye,
    struct mailimap_mailbox_data * rsp_mailbox_data,
    struct mailimap_message_data * rsp_message_data,
    struct mailimap_capability_data * rsp_capability_data,
    struct mailimap_extension_data * rsp_extension_data);

void
mailimap_response_data_free(struct mailimap_response_data * resp_data);



/* this is the type of an ending response */

enum {
  MAILIMAP_RESP_DONE_TYPE_ERROR,  /* on parse error */
  MAILIMAP_RESP_DONE_TYPE_TAGGED, /* tagged response */
  MAILIMAP_RESP_DONE_TYPE_FATAL   /* fatal error response */
};

/*
  mailimap_response_done is an ending response

  - type is the type of the ending response

  - tagged is a tagged response

  - fatal is a fatal error response
*/

struct mailimap_response_done {
  int rsp_type;
  union {
    struct mailimap_response_tagged * rsp_tagged; /* can be NULL */
    struct mailimap_response_fatal * rsp_fatal;   /* can be NULL */
  } rsp_data;
};

struct mailimap_response_done *
mailimap_response_done_new(int rsp_type,
    struct mailimap_response_tagged * rsp_tagged,
    struct mailimap_response_fatal * rsp_fatal);

void mailimap_response_done_free(struct mailimap_response_done *
				 resp_done);


/*
  mailimap_response_fatal is a fatal error response

  - bye is a BYE response text
*/

struct mailimap_response_fatal {
  struct mailimap_resp_cond_bye * rsp_bye; /* != NULL */
};

struct mailimap_response_fatal *
mailimap_response_fatal_new(struct mailimap_resp_cond_bye * rsp_bye);

void mailimap_response_fatal_free(struct mailimap_response_fatal * resp_fatal);



/*
  mailimap_response_tagged is a tagged response

  - tag is the sent tag, should be allocated with malloc()

  - cond_state is a condition state response
*/

struct mailimap_response_tagged {
  char * rsp_tag; /* != NULL */
  struct mailimap_resp_cond_state * rsp_cond_state; /* != NULL */
};

struct mailimap_response_tagged *
mailimap_response_tagged_new(char * rsp_tag,
    struct mailimap_resp_cond_state * rsp_cond_state);

void
mailimap_response_tagged_free(struct mailimap_response_tagged * tagged);


/* this is the type of an authentication condition response */

enum {
  MAILIMAP_RESP_COND_AUTH_ERROR,   /* on parse error */
  MAILIMAP_RESP_COND_AUTH_OK,      /* authentication is needed */
  MAILIMAP_RESP_COND_AUTH_PREAUTH  /* authentication is not needed */
};

/*
  mailimap_resp_cond_auth is an authentication condition response

  - type is the type of the authentication condition response,
    the value can be MAILIMAP_RESP_COND_AUTH_OK or
    MAILIMAP_RESP_COND_AUTH_PREAUTH

  - text is a text response
*/

struct mailimap_resp_cond_auth {
  int rsp_type;
  struct mailimap_resp_text * rsp_text; /* != NULL */
};

struct mailimap_resp_cond_auth *
mailimap_resp_cond_auth_new(int rsp_type,
    struct mailimap_resp_text * rsp_text);

void
mailimap_resp_cond_auth_free(struct mailimap_resp_cond_auth * cond_auth);



/*
  mailimap_resp_cond_bye is a BYE response

  - text is a text response
*/

struct mailimap_resp_cond_bye {
  struct mailimap_resp_text * rsp_text; /* != NULL */
};

struct mailimap_resp_cond_bye *
mailimap_resp_cond_bye_new(struct mailimap_resp_text * rsp_text);

void
mailimap_resp_cond_bye_free(struct mailimap_resp_cond_bye * cond_bye);



/* this is the type of a condition state response */

enum {
  MAILIMAP_RESP_COND_STATE_OK,
  MAILIMAP_RESP_COND_STATE_NO,
  MAILIMAP_RESP_COND_STATE_BAD
};

/*
  mailimap_resp_cond_state is a condition state reponse
  
  - type is the type of the condition state response

  - text is a text response
*/

struct mailimap_resp_cond_state {
  int rsp_type;
  struct mailimap_resp_text * rsp_text; /* can be NULL */
};

struct mailimap_resp_cond_state *
mailimap_resp_cond_state_new(int rsp_type,
    struct mailimap_resp_text * rsp_text);

void
mailimap_resp_cond_state_free(struct mailimap_resp_cond_state * cond_state);



/*
  mailimap_resp_text is a text response

  - resp_code is a response code
  
  - text is a human readable text, should be allocated with malloc()
*/

struct mailimap_resp_text {
  struct mailimap_resp_text_code * rsp_code; /* can be NULL */
  char * rsp_text; /* can be NULL */
};

struct mailimap_resp_text *
mailimap_resp_text_new(struct mailimap_resp_text_code * resp_code,
		       char * rsp_text);

void mailimap_resp_text_free(struct mailimap_resp_text * resp_text);



/* this is the type of the response code */

enum {
  MAILIMAP_RESP_TEXT_CODE_ALERT,           /* ALERT response */
  MAILIMAP_RESP_TEXT_CODE_BADCHARSET,      /* BADCHARSET response */
  MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA, /* CAPABILITY response */
  MAILIMAP_RESP_TEXT_CODE_PARSE,           /* PARSE response */
  MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS,  /* PERMANENTFLAGS response */
  MAILIMAP_RESP_TEXT_CODE_READ_ONLY,       /* READONLY response */
  MAILIMAP_RESP_TEXT_CODE_READ_WRITE,      /* READWRITE response */
  MAILIMAP_RESP_TEXT_CODE_TRY_CREATE,      /* TRYCREATE response */
  MAILIMAP_RESP_TEXT_CODE_UIDNEXT,         /* UIDNEXT response */
  MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY,     /* UIDVALIDITY response */
  MAILIMAP_RESP_TEXT_CODE_UNSEEN,          /* UNSEEN response */
  MAILIMAP_RESP_TEXT_CODE_OTHER,           /* other type of response */
  MAILIMAP_RESP_TEXT_CODE_EXTENSION        /* extension response */
};

/*
  mailimap_resp_text_code is a response code
  
  - type is the type of the response code, the value can be
    MAILIMAP_RESP_TEXT_CODE_ALERT, MAILIMAP_RESP_TEXT_CODE_BADCHARSET,
    MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA, MAILIMAP_RESP_TEXT_CODE_PARSE,
    MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS, MAILIMAP_RESP_TEXT_CODE_READ_ONLY,
    MAILIMAP_RESP_TEXT_CODE_READ_WRITE, MAILIMAP_RESP_TEXT_CODE_TRY_CREATE,
    MAILIMAP_RESP_TEXT_CODE_UIDNEXT, MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY,
    MAILIMAP_RESP_TEXT_CODE_UNSEEN or MAILIMAP_RESP_TEXT_CODE_OTHER
    
  - badcharset is a list of charsets if type
    is MAILIMAP_RESP_TEXT_CODE_BADCHARSET, each element should be
    allocated with malloc()

  - cap_data is a list of capabilities

  - perm_flags is a list of flags, this is the flags that can be changed
    permanently on the messages of the mailbox.

  - uidnext is the next unique identifier of a message
  
  - uidvalidity is the unique identifier validity value

  - first_unseen is the number of the first message without the \Seen flag
  
  - atom is a keyword for an extension response code, should be allocated
    with malloc()

  - atom_value is the data related with the extension response code,
    should be allocated with malloc()
*/

struct mailimap_resp_text_code {
  int rc_type;
  union {
    clist * rc_badcharset; /* list of astring (char *) */
    /* can be NULL */
    struct mailimap_capability_data * rc_cap_data; /* != NULL */
    clist * rc_perm_flags; /* list of (struct mailimap_flag_perm *) */
    /* can be NULL */
    uint32_t rc_uidnext;
    uint32_t rc_uidvalidity;
    uint32_t rc_first_unseen;
    struct {
      char * atom_name;  /* can be NULL */
      char * atom_value; /* can be NULL */
    } rc_atom;
    struct mailimap_extension_data * rc_ext_data; /* can be NULL */
  } rc_data;
};

struct mailimap_resp_text_code *
mailimap_resp_text_code_new(int rc_type, clist * rc_badcharset,
    struct mailimap_capability_data * rc_cap_data,
    clist * rc_perm_flags,
    uint32_t rc_uidnext, uint32_t rc_uidvalidity,
    uint32_t rc_first_unseen, char * rc_atom, char * rc_atom_value,
    struct mailimap_extension_data * rc_ext_data);

void
mailimap_resp_text_code_free(struct mailimap_resp_text_code * resp_text_code);


/*
  mailimap_section is a MIME part section identifier

  section_spec is the MIME section identifier
*/

struct mailimap_section {
  struct mailimap_section_spec * sec_spec; /* can be NULL */
};

struct mailimap_section *
mailimap_section_new(struct mailimap_section_spec * sec_spec);

void mailimap_section_free(struct mailimap_section * section);


/* this is the type of the message/rfc822 part description */

enum {
  MAILIMAP_SECTION_MSGTEXT_HEADER,            /* header fields part of the
                                                 message */
  MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS,     /* given header fields of the
                                                 message */
  MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT, /* header fields of the
                                                 message except the given */
  MAILIMAP_SECTION_MSGTEXT_TEXT               /* text part  */
};

/*
  mailimap_section_msgtext is a message/rfc822 part description
  
  - type is the type of the content part and the value can be
    MAILIMAP_SECTION_MSGTEXT_HEADER, MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS,
    MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT
    or MAILIMAP_SECTION_MSGTEXT_TEXT

  - header_list is the list of headers when type is
    MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS or
    MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT
*/

struct mailimap_section_msgtext {
  int sec_type;
  struct mailimap_header_list * sec_header_list; /* can be NULL */
};

struct mailimap_section_msgtext *
mailimap_section_msgtext_new(int sec_type,
    struct mailimap_header_list * sec_header_list);

void
mailimap_section_msgtext_free(struct mailimap_section_msgtext * msgtext);



/*
  mailimap_section_part is the MIME part location in a message
  
  - section_id is a list of number index of the sub-part in the mail structure,
    each element should be allocated with malloc()

*/

struct mailimap_section_part {
  clist * sec_id; /* list of nz-number (uint32_t *) */
                      /* != NULL */
};

struct mailimap_section_part *
mailimap_section_part_new(clist * sec_id);

void
mailimap_section_part_free(struct mailimap_section_part * section_part);



/* this is the type of section specification */

enum {
  MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT, /* if requesting data of the root
                                            MIME message/rfc822 part */
  MAILIMAP_SECTION_SPEC_SECTION_PART     /* location of the MIME part
                                            in the message */
};

/*
  mailimap_section_spec is a section specification

  - type is the type of the section specification, the value can be
    MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT or
    MAILIMAP_SECTION_SPEC_SECTION_PART

  - section_msgtext is a message/rfc822 part description if type is
    MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT

  - section_part is a body part location in the message if type is
    MAILIMAP_SECTION_SPEC_SECTION_PART
  
  - section_text is a body part location for a given MIME part,
    this can be NULL if the body of the part is requested (and not
    the MIME header).
*/

struct mailimap_section_spec {
  int sec_type;
  union {
    struct mailimap_section_msgtext * sec_msgtext; /* can be NULL */
    struct mailimap_section_part * sec_part;       /* can be NULL */
  } sec_data;
  struct mailimap_section_text * sec_text;       /* can be NULL */
};

struct mailimap_section_spec *
mailimap_section_spec_new(int sec_type,
    struct mailimap_section_msgtext * sec_msgtext,
    struct mailimap_section_part * sec_part,
    struct mailimap_section_text * sec_text);

void
mailimap_section_spec_free(struct mailimap_section_spec * section_spec);



/* this is the type of body part location for a given MIME part */

enum {
  MAILIMAP_SECTION_TEXT_ERROR,           /* on parse error **/
  MAILIMAP_SECTION_TEXT_SECTION_MSGTEXT, /* if the MIME type is
                                            message/rfc822, headers or text
                                            can be requested */
  MAILIMAP_SECTION_TEXT_MIME             /* for all MIME types,
                                            MIME headers can be requested */
};

/*
  mailimap_section_text is the body part location for a given MIME part

  - type can be MAILIMAP_SECTION_TEXT_SECTION_MSGTEXT or
    MAILIMAP_SECTION_TEXT_MIME

  - section_msgtext is the part of the MIME part when MIME type is
    message/rfc822 than can be requested, when type is
    MAILIMAP_TEXT_SECTION_MSGTEXT
*/

struct mailimap_section_text {
  int sec_type;
  struct mailimap_section_msgtext * sec_msgtext; /* can be NULL */
};

struct mailimap_section_text *
mailimap_section_text_new(int sec_type,
    struct mailimap_section_msgtext * sec_msgtext);

void
mailimap_section_text_free(struct mailimap_section_text * section_text);










/* ************************************************************************* */
/* the following part concerns only the IMAP command that are sent */


/*
  mailimap_set_item is a message set

  - first is the first message of the set
  - last is the last message of the set

  this can be message numbers of message unique identifiers
*/

struct mailimap_set_item {
  uint32_t set_first;
  uint32_t set_last;
};

struct mailimap_set_item *
mailimap_set_item_new(uint32_t set_first, uint32_t set_last);

void mailimap_set_item_free(struct mailimap_set_item * set_item);



/*
  set is a list of message sets

  - list is a list of message sets
*/

struct mailimap_set {
  clist * set_list; /* list of (struct mailimap_set_item *) */
};

struct mailimap_set * mailimap_set_new(clist * list);

void mailimap_set_free(struct mailimap_set * set);


/*
  mailimap_date is a date

  - day is the day in the month (1 to 31)

  - month (1 to 12)

  - year (4 digits)
*/

struct mailimap_date {
  int dt_day;
  int dt_month;
  int dt_year;
};

struct mailimap_date *
mailimap_date_new(int dt_day, int dt_month, int dt_year);

void mailimap_date_free(struct mailimap_date * date);




/* this is the type of fetch attribute for a given message */

enum {
  MAILIMAP_FETCH_ATT_ENVELOPE,          /* to fetch the headers parsed by
                                           the IMAP server */
  MAILIMAP_FETCH_ATT_FLAGS,             /* to fetch the flags */
  MAILIMAP_FETCH_ATT_INTERNALDATE,      /* to fetch the date of the message
                                           kept by the server */
  MAILIMAP_FETCH_ATT_RFC822,            /* to fetch the entire message */
  MAILIMAP_FETCH_ATT_RFC822_HEADER,     /* to fetch the headers */
  MAILIMAP_FETCH_ATT_RFC822_SIZE,       /* to fetch the size */
  MAILIMAP_FETCH_ATT_RFC822_TEXT,       /* to fetch the text part */
  MAILIMAP_FETCH_ATT_BODY,              /* to fetch the MIME structure */
  MAILIMAP_FETCH_ATT_BODYSTRUCTURE,     /* to fetch the MIME structure with
                                           additional information */
  MAILIMAP_FETCH_ATT_UID,               /* to fetch the unique identifier */
  MAILIMAP_FETCH_ATT_BODY_SECTION,      /* to fetch a given part */
  MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION  /* to fetch a given part without
                                           marking the message as read */
};


/*
  mailimap_fetch_att is the description of the fetch attribute

  - type is the type of fetch attribute, the value can be
    MAILIMAP_FETCH_ATT_ENVELOPE, MAILIMAP_FETCH_ATT_FLAGS,
    MAILIMAP_FETCH_ATT_INTERNALDATE, MAILIMAP_FETCH_ATT_RFC822,
    MAILIMAP_FETCH_ATT_RFC822_HEADER, MAILIMAP_FETCH_ATT_RFC822_SIZE,
    MAILIMAP_FETCH_ATT_RFC822_TEXT, MAILIMAP_FETCH_ATT_BODY,
    MAILIMAP_FETCH_ATT_BODYSTRUCTURE, MAILIMAP_FETCH_ATT_UID,
    MAILIMAP_FETCH_ATT_BODY_SECTION or MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION

  - section is the location of the part to fetch if type is
    MAILIMAP_FETCH_ATT_BODY_SECTION or MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION

  - offset is the first byte to fetch in the given part

  - size is the maximum size of the part to fetch
*/

struct mailimap_fetch_att {
  int att_type;
  struct mailimap_section * att_section;
  uint32_t att_offset;
  uint32_t att_size;
};

struct mailimap_fetch_att *
mailimap_fetch_att_new(int att_type, struct mailimap_section * att_section,
		       uint32_t att_offset, uint32_t att_size);


void mailimap_fetch_att_free(struct mailimap_fetch_att * fetch_att);


/* this is the type of a FETCH operation */

enum {
  MAILIMAP_FETCH_TYPE_ALL,            /* equivalent to (FLAGS INTERNALDATE
                                         RFC822.SIZE ENVELOPE) */
  MAILIMAP_FETCH_TYPE_FULL,           /* equivalent to (FLAGS INTERNALDATE
                                         RFC822.SIZE ENVELOPE BODY) */
  MAILIMAP_FETCH_TYPE_FAST,           /* equivalent to (FLAGS INTERNALDATE
                                         RFC822.SIZE) */
  MAILIMAP_FETCH_TYPE_FETCH_ATT,      /* when there is only of fetch
                                         attribute */
  MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST  /* when there is a list of fetch
                                         attributes */
};

/*
  mailimap_fetch_type is the description of the FETCH operation

  - type can be MAILIMAP_FETCH_TYPE_ALL, MAILIMAP_FETCH_TYPE_FULL,
    MAILIMAP_FETCH_TYPE_FAST, MAILIMAP_FETCH_TYPE_FETCH_ATT or
    MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST

  - fetch_att is a fetch attribute if type is MAILIMAP_FETCH_TYPE_FETCH_ATT

  - fetch_att_list is a list of fetch attributes if type is
    MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST
*/

struct mailimap_fetch_type {
  int ft_type;
  union {
    struct mailimap_fetch_att * ft_fetch_att;
    clist * ft_fetch_att_list; /* list of (struct mailimap_fetch_att *) */
  } ft_data;
};

struct mailimap_fetch_type *
mailimap_fetch_type_new(int ft_type,
    struct mailimap_fetch_att * ft_fetch_att,
    clist * ft_fetch_att_list);


void mailimap_fetch_type_free(struct mailimap_fetch_type * fetch_type);



/*
  mailimap_store_att_flags is the description of the STORE operation
  (change flags of a message)

  - sign can be 0 (set flag), +1 (add flag) or -1 (remove flag)

  - silent has a value of 1 if the flags are changed with no server
    response

  - flag_list is the list of flags to change
*/

struct mailimap_store_att_flags {
  int fl_sign;
  int fl_silent;
  struct mailimap_flag_list * fl_flag_list;
};

struct mailimap_store_att_flags *
mailimap_store_att_flags_new(int fl_sign, int fl_silent,
			     struct mailimap_flag_list * fl_flag_list);

void mailimap_store_att_flags_free(struct mailimap_store_att_flags *
    store_att_flags);



/* this is the condition of the SEARCH operation */

enum {
  MAILIMAP_SEARCH_KEY_ALL,        /* all messages */
  MAILIMAP_SEARCH_KEY_ANSWERED,   /* messages with the flag \Answered */
  MAILIMAP_SEARCH_KEY_BCC,        /* messages whose Bcc field contains the
                                     given string */
  MAILIMAP_SEARCH_KEY_BEFORE,     /* messages whose internal date is earlier
                                     than the specified date */
  MAILIMAP_SEARCH_KEY_BODY,       /* message that contains the given string
                                     (in header and text parts) */
  MAILIMAP_SEARCH_KEY_CC,         /* messages whose Cc field contains the
                                     given string */
  MAILIMAP_SEARCH_KEY_DELETED,    /* messages with the flag \Deleted */
  MAILIMAP_SEARCH_KEY_FLAGGED,    /* messages with the flag \Flagged */ 
  MAILIMAP_SEARCH_KEY_FROM,       /* messages whose From field contains the
                                     given string */
  MAILIMAP_SEARCH_KEY_KEYWORD,    /* messages with the flag keyword set */
  MAILIMAP_SEARCH_KEY_NEW,        /* messages with the flag \Recent and not
                                     the \Seen flag */
  MAILIMAP_SEARCH_KEY_OLD,        /* messages that do not have the
                                     \Recent flag set */
  MAILIMAP_SEARCH_KEY_ON,         /* messages whose internal date is the
                                     specified date */
  MAILIMAP_SEARCH_KEY_RECENT,     /* messages with the flag \Recent */
  MAILIMAP_SEARCH_KEY_SEEN,       /* messages with the flag \Seen */
  MAILIMAP_SEARCH_KEY_SINCE,      /* messages whose internal date is later
                                     than specified date */
  MAILIMAP_SEARCH_KEY_SUBJECT,    /* messages whose Subject field contains the
                                     given string */
  MAILIMAP_SEARCH_KEY_TEXT,       /* messages whose text part contains the
                                     given string */
  MAILIMAP_SEARCH_KEY_TO,         /* messages whose To field contains the
                                     given string */
  MAILIMAP_SEARCH_KEY_UNANSWERED, /* messages with no flag \Answered */
  MAILIMAP_SEARCH_KEY_UNDELETED,  /* messages with no flag \Deleted */
  MAILIMAP_SEARCH_KEY_UNFLAGGED,  /* messages with no flag \Flagged */
  MAILIMAP_SEARCH_KEY_UNKEYWORD,  /* messages with no flag keyword */ 
  MAILIMAP_SEARCH_KEY_UNSEEN,     /* messages with no flag \Seen */
  MAILIMAP_SEARCH_KEY_DRAFT,      /* messages with no flag \Draft */
  MAILIMAP_SEARCH_KEY_HEADER,     /* messages whose given field 
                                     contains the given string */
  MAILIMAP_SEARCH_KEY_LARGER,     /* messages whose size is larger then
                                     the given size */
  MAILIMAP_SEARCH_KEY_NOT,        /* not operation of the condition */
  MAILIMAP_SEARCH_KEY_OR,         /* or operation between two conditions */
  MAILIMAP_SEARCH_KEY_SENTBEFORE, /* messages whose date given in Date header
                                     is earlier than the specified date */
  MAILIMAP_SEARCH_KEY_SENTON,     /* messages whose date given in Date header
                                     is the specified date */
  MAILIMAP_SEARCH_KEY_SENTSINCE,  /* messages whose date given in Date header
                                     is later than specified date */
  MAILIMAP_SEARCH_KEY_SMALLER,    /* messages whose size is smaller than
                                     the given size */
  MAILIMAP_SEARCH_KEY_UID,        /* messages whose unique identifiers are
                                     in the given range */
  MAILIMAP_SEARCH_KEY_UNDRAFT,    /* messages with no flag \Draft */
  MAILIMAP_SEARCH_KEY_SET,        /* messages whose number (or unique
                                     identifiers in case of UID SEARCH) are
                                     in the given range */
  MAILIMAP_SEARCH_KEY_MULTIPLE    /* the boolean operator between the
                                     conditions is AND */
};

/*
  mailimap_search_key is the condition on the messages to return
  
  - type is the type of the condition

  - bcc is the text to search in the Bcc field when type is
    MAILIMAP_SEARCH_KEY_BCC, should be allocated with malloc()

  - before is a date when type is MAILIMAP_SEARCH_KEY_BEFORE

  - body is the text to search in the message when type is
    MAILIMAP_SEARCH_KEY_BODY, should be allocated with malloc()

  - cc is the text to search in the Cc field when type is
    MAILIMAP_SEARCH_KEY_CC, should be allocated with malloc()
  
  - from is the text to search in the From field when type is
    MAILIMAP_SEARCH_KEY_FROM, should be allocated with malloc()

  - keyword is the keyword flag name when type is MAILIMAP_SEARCH_KEY_KEYWORD,
    should be allocated with malloc()

  - on is a date when type is MAILIMAP_SEARCH_KEY_ON

  - since is a date when type is MAILIMAP_SEARCH_KEY_SINCE
  
  - subject is the text to search in the Subject field when type is
    MAILIMAP_SEARCH_KEY_SUBJECT, should be allocated with malloc()

  - text is the text to search in the text part of the message when
    type is MAILIMAP_SEARCH_KEY_TEXT, should be allocated with malloc()

  - to is the text to search in the To field when type is
    MAILIMAP_SEARCH_KEY_TO, should be allocated with malloc()

  - unkeyword is the keyword flag name when type is
    MAILIMAP_SEARCH_KEY_UNKEYWORD, should be allocated with malloc()

  - header_name is the header name when type is MAILIMAP_SEARCH_KEY_HEADER,
    should be allocated with malloc()

  - header_value is the text to search in the given header when type is
    MAILIMAP_SEARCH_KEY_HEADER, should be allocated with malloc()

  - larger is a size when type is MAILIMAP_SEARCH_KEY_LARGER

  - not is a condition when type is MAILIMAP_SEARCH_KEY_NOT

  - or1 is a condition when type is MAILIMAP_SEARCH_KEY_OR

  - or2 is a condition when type is MAILIMAP_SEARCH_KEY_OR
  
  - sentbefore is a date when type is MAILIMAP_SEARCH_KEY_SENTBEFORE

  - senton is a date when type is MAILIMAP_SEARCH_KEY_SENTON

  - sentsince is a date when type is MAILIMAP_SEARCH_KEY_SENTSINCE

  - smaller is a size when type is MAILIMAP_SEARCH_KEY_SMALLER

  - uid is a set of messages when type is MAILIMAP_SEARCH_KEY_UID

  - set is a set of messages when type is MAILIMAP_SEARCH_KEY_SET

  - multiple is a set of message when type is MAILIMAP_SEARCH_KEY_MULTIPLE
*/

struct mailimap_search_key {
  int sk_type;
  union {
    char * sk_bcc;
    struct mailimap_date * sk_before;
    char * sk_body;
    char * sk_cc;
    char * sk_from;
    char * sk_keyword;
    struct mailimap_date * sk_on;
    struct mailimap_date * sk_since;
    char * sk_subject;
    char * sk_text;
    char * sk_to;
    char * sk_unkeyword;
    struct {
      char * sk_header_name;
      char * sk_header_value;
    } sk_header;
    uint32_t sk_larger;
    struct mailimap_search_key * sk_not;
    struct {
      struct mailimap_search_key * sk_or1;
      struct mailimap_search_key * sk_or2;
    } sk_or;
    struct mailimap_date * sk_sentbefore;
    struct mailimap_date * sk_senton;
    struct mailimap_date * sk_sentsince;
    uint32_t sk_smaller;
    struct mailimap_set * sk_uid;
    struct mailimap_set * sk_set;
    clist * sk_multiple; /* list of (struct mailimap_search_key *) */
  } sk_data;
};

struct mailimap_search_key *
mailimap_search_key_new(int sk_type,
    char * sk_bcc, struct mailimap_date * sk_before, char * sk_body,
    char * sk_cc, char * sk_from, char * sk_keyword,
    struct mailimap_date * sk_on, struct mailimap_date * sk_since,
    char * sk_subject, char * sk_text, char * sk_to,
    char * sk_unkeyword, char * sk_header_name,
    char * sk_header_value, uint32_t sk_larger,
    struct mailimap_search_key * sk_not,
    struct mailimap_search_key * sk_or1,
    struct mailimap_search_key * sk_or2,
    struct mailimap_date * sk_sentbefore,
    struct mailimap_date * sk_senton,
    struct mailimap_date * sk_sentsince,
    uint32_t sk_smaller, struct mailimap_set * sk_uid,
    struct mailimap_set * sk_set, clist * sk_multiple);


void mailimap_search_key_free(struct mailimap_search_key * key);


/*
  mailimap_status_att_list is a list of mailbox STATUS request type

  - list is a list of mailbox STATUS request type
    (value of elements in the list can be MAILIMAP_STATUS_ATT_MESSAGES,
    MAILIMAP_STATUS_ATT_RECENT, MAILIMAP_STATUS_ATT_UIDNEXT,
    MAILIMAP_STATUS_ATT_UIDVALIDITY or MAILIMAP_STATUS_ATT_UNSEEN),
    each element should be allocated with malloc()
*/

struct mailimap_status_att_list {
  clist * att_list; /* list of (uint32_t *) */
};

struct mailimap_status_att_list *
mailimap_status_att_list_new(clist * att_list);

void mailimap_status_att_list_free(struct mailimap_status_att_list *
    status_att_list);




/* internal use functions */


uint32_t * mailimap_number_alloc_new(uint32_t number);

void mailimap_number_alloc_free(uint32_t * pnumber);


void mailimap_addr_host_free(char * addr_host);

void mailimap_addr_mailbox_free(char * addr_mailbox);

void mailimap_addr_adl_free(char * addr_adl);

void mailimap_addr_name_free(char * addr_name);

void mailimap_astring_free(char * astring);

void mailimap_atom_free(char * atom);

void mailimap_auth_type_free(char * auth_type);

void mailimap_base64_free(char * base64);

void mailimap_body_fld_desc_free(char * body_fld_desc);

void mailimap_body_fld_id_free(char * body_fld_id);

void mailimap_body_fld_md5_free(char * body_fld_md5);

void mailimap_env_date_free(char * date);

void mailimap_env_in_reply_to_free(char * in_reply_to);

void mailimap_env_message_id_free(char * message_id);

void mailimap_env_subject_free(char * subject);

void mailimap_flag_extension_free(char * flag_extension);

void mailimap_flag_keyword_free(char * flag_keyword);

void
mailimap_header_fld_name_free(char * header_fld_name);

void mailimap_literal_free(char * literal);

void mailimap_mailbox_free(char * mailbox);

void
mailimap_mailbox_data_search_free(clist * data_search);

void mailimap_media_subtype_free(char * media_subtype);

void mailimap_media_text_free(char * media_text);

void mailimap_msg_att_envelope_free(struct mailimap_envelope * env);

void
mailimap_msg_att_internaldate_free(struct mailimap_date_time * date_time);

void
mailimap_msg_att_rfc822_free(char * str);

void
mailimap_msg_att_rfc822_header_free(char * str);

void
mailimap_msg_att_rfc822_text_free(char * str);

void
mailimap_msg_att_body_free(struct mailimap_body * body);

void
mailimap_msg_att_bodystructure_free(struct mailimap_body * body);

void mailimap_nstring_free(char * str);

void
mailimap_string_free(char * str);

void mailimap_tag_free(char * tag);

void mailimap_text_free(char * text);





/* IMAP connection */

/* this is the state of the IMAP connection */

enum {
  MAILIMAP_STATE_DISCONNECTED,
  MAILIMAP_STATE_NON_AUTHENTICATED,
  MAILIMAP_STATE_AUTHENTICATED,
  MAILIMAP_STATE_SELECTED,
  MAILIMAP_STATE_LOGOUT
};

/*
  mailimap is an IMAP connection

  - response is a human readable message returned with a reponse,
    must be accessed read-only

  - stream is the connection with the IMAP server

  - stream_buffer is the buffer where the data to parse are stored

  - state is the state of IMAP connection

  - tag is the current tag being used in IMAP connection

  - response_buffer is the buffer for response messages

  - connection_info is the information returned in response
    for the last command about the connection

  - selection_info is the information returned in response
    for the last command about the current selected mailbox

  - response_info is the other information returned in response
    for the last command
*/

struct mailimap {
  char * imap_response;
  
  /* internals */
  mailstream * imap_stream;

  size_t imap_progr_rate;
  progress_function * imap_progr_fun;

  MMAPString * imap_stream_buffer;
  MMAPString * imap_response_buffer;

  int imap_state;
  int imap_tag;

  struct mailimap_connection_info * imap_connection_info;
  struct mailimap_selection_info * imap_selection_info;
  struct mailimap_response_info * imap_response_info;
  
  struct {
    void * sasl_conn;
    const char * sasl_server_fqdn;
    const char * sasl_login;
    const char * sasl_auth_name;
    const char * sasl_password;
    const char * sasl_realm;
    void * sasl_secret;
  } imap_sasl;
};

typedef struct mailimap mailimap;


/*
  mailimap_connection_info is the information about the connection
  
  - capability is the list of capability of the IMAP server
*/

struct mailimap_connection_info {
  struct mailimap_capability_data * imap_capability;
};

struct mailimap_connection_info *
mailimap_connection_info_new(void);

void
mailimap_connection_info_free(struct mailimap_connection_info * conn_info);


/* this is the type of mailbox access */

enum {
  MAILIMAP_MAILBOX_READONLY,
  MAILIMAP_MAILBOX_READWRITE
};

/*
  mailimap_selection_info is information about the current selected mailbox

  - perm_flags is a list of flags that can be changed permanently on the
    messages of the mailbox

  - perm is the access on the mailbox, value can be
    MAILIMAP_MAILBOX_READONLY or MAILIMAP_MAILBOX_READWRITE

  - uidnext is the next unique identifier

  - uidvalidity is the unique identifiers validity

  - first_unseen is the number of the first unseen message

  - flags is a list of flags that can be used on the messages of
    the mailbox

  - exists is the number of messages in the mailbox
  
  - recent is the number of recent messages in the mailbox

  - unseen is the number of unseen messages in the mailbox
*/

struct mailimap_selection_info {
  clist * sel_perm_flags; /* list of (struct flag_perm *) */
  int sel_perm;
  uint32_t sel_uidnext;
  uint32_t sel_uidvalidity;
  uint32_t sel_first_unseen;
  struct mailimap_flag_list * sel_flags;
  uint32_t sel_exists;
  uint32_t sel_recent;
  uint32_t sel_unseen;
};

struct mailimap_selection_info *
mailimap_selection_info_new(void);

void
mailimap_selection_info_free(struct mailimap_selection_info * sel_info);


/*
  mailimap_response_info is the other information returned in the 
  response for a command

  - alert is the human readable text returned with ALERT response

  - parse is the human readable text returned with PARSE response

  - badcharset is a list of charset returned with a BADCHARSET response

  - trycreate is set to 1 if a trycreate response was returned
  
  - mailbox_list is a list of mailboxes
  
  - mailbox_lsub is a list of subscribed mailboxes

  - search_result is a list of message numbers or unique identifiers

  - status is a STATUS response

  - expunged is a list of message numbers

  - fetch_list is a list of fetch response
*/

struct mailimap_response_info {
  char * rsp_alert;
  char * rsp_parse;
  clist * rsp_badcharset; /* list of (char *) */
  int rsp_trycreate;
  clist * rsp_mailbox_list; /* list of (struct mailimap_mailbox_list *) */
  clist * rsp_mailbox_lsub; /* list of (struct mailimap_mailbox_list *) */
  clist * rsp_search_result; /* list of (uint32_t *) */
  struct mailimap_mailbox_data_status * rsp_status;
  clist * rsp_expunged; /* list of (uint32_t 32 *) */
  clist * rsp_fetch_list; /* list of (struct mailimap_msg_att *) */
  clist * rsp_extension_list; /* list of (struct mailimap_extension_data *) */
  char * rsp_atom;
  char * rsp_value;
};

struct mailimap_response_info *
mailimap_response_info_new(void);

void
mailimap_response_info_free(struct mailimap_response_info * resp_info);


/* these are the possible returned error codes */

enum {
  MAILIMAP_NO_ERROR = 0,
  MAILIMAP_NO_ERROR_AUTHENTICATED = 1,
  MAILIMAP_NO_ERROR_NON_AUTHENTICATED = 2,
  MAILIMAP_ERROR_BAD_STATE,
  MAILIMAP_ERROR_STREAM,
  MAILIMAP_ERROR_PARSE,
  MAILIMAP_ERROR_CONNECTION_REFUSED,
  MAILIMAP_ERROR_MEMORY,
  MAILIMAP_ERROR_FATAL,
  MAILIMAP_ERROR_PROTOCOL,
  MAILIMAP_ERROR_DONT_ACCEPT_CONNECTION,
  MAILIMAP_ERROR_APPEND,
  MAILIMAP_ERROR_NOOP,
  MAILIMAP_ERROR_LOGOUT,
  MAILIMAP_ERROR_CAPABILITY,
  MAILIMAP_ERROR_CHECK,
  MAILIMAP_ERROR_CLOSE,
  MAILIMAP_ERROR_EXPUNGE,
  MAILIMAP_ERROR_COPY,
  MAILIMAP_ERROR_UID_COPY,
  MAILIMAP_ERROR_CREATE,
  MAILIMAP_ERROR_DELETE,
  MAILIMAP_ERROR_EXAMINE,
  MAILIMAP_ERROR_FETCH,
  MAILIMAP_ERROR_UID_FETCH,
  MAILIMAP_ERROR_LIST,
  MAILIMAP_ERROR_LOGIN,
  MAILIMAP_ERROR_LSUB,
  MAILIMAP_ERROR_RENAME,
  MAILIMAP_ERROR_SEARCH,
  MAILIMAP_ERROR_UID_SEARCH,
  MAILIMAP_ERROR_SELECT,
  MAILIMAP_ERROR_STATUS,
  MAILIMAP_ERROR_STORE,
  MAILIMAP_ERROR_UID_STORE,
  MAILIMAP_ERROR_SUBSCRIBE,
  MAILIMAP_ERROR_UNSUBSCRIBE,
  MAILIMAP_ERROR_STARTTLS,
  MAILIMAP_ERROR_INVAL,
  MAILIMAP_ERROR_EXTENSION
};


#ifdef __cplusplus
}
#endif

#endif

