/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@ingroup sdp_parser
 * @CFILE sdp_parse.c
 * @brief Simple SDP parser interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date  Created: Fri Feb 18 10:25:08 2000 ppessi
 *
 * @sa @RFC4566, @RFC2327.
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include "sofia-sip/sdp.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

/** @typedef struct sdp_parser_s sdp_parser_t;
 *
 * SDP parser handle.
 *
 * The SDP parser handle returned by sdp_parse() contains either
 * a successfully parsed SDP session #sdp_session_t or an error message.
 * If sdp_session() returns non-NULL, parsing was successful.
 *
 * @sa #sdp_session_t, sdp_parse(), sdp_session(), sdp_parsing_error(),
 * sdp_sanity_check(), sdp_parser_home(), sdp_parser_free(), @RFC4566,
 * @RFC2327.
 */

struct sdp_parser_s {
  su_home_t       pr_home[1];
  union {
    char          pru_error[128];
    sdp_session_t pru_session[1];
  } pr_output;
  char      *pr_message;

  sdp_mode_t pr_session_mode;

  unsigned   pr_ok : 1;

  unsigned   pr_strict : 1;
  unsigned   pr_anynet : 1;
  unsigned   pr_mode_0000 : 1;
  unsigned   pr_mode_manual : 1;
  unsigned   pr_insane : 1;
  unsigned   pr_c_missing : 1;
  unsigned   pr_config : 1;
};

#define is_posdigit(c) ((c) >= '1' && (c) <= '9')
#define is_digit(c) ((c) >= '0' && (c) <= '9')
#define is_space(c) ((c) == ' ')
#define is_tab(c) ((c) == '\t')

#define pr_error   pr_output.pru_error
#define pr_session pr_output.pru_session

#ifdef _MSC_VER
#undef STRICT
#endif
#define STRICT(pr) (pr->pr_strict)

/* Static parser object used when running out of memory */
static const struct sdp_parser_s no_mem_error =
{
  { SU_HOME_INIT(no_mem_error) },
  { "sdp: not enough memory" }
};

/* Internal prototypes */
static void parse_message(sdp_parser_t *p);
static int parsing_error(sdp_parser_t *p, char const *fmt, ...);

/** Parse an SDP message.
 *
 * The function sdp_parse() parses an SDP message @a msg of size @a
 * msgsize. Parsing is done according to the given @a flags. The SDP message
 * may not contain a NUL.
 *
 * The parsing result is stored to an #sdp_session_t structure.
 *
 * @param home    memory home
 * @param msg     pointer to message
 * @param msgsize size of the message (excluding final NUL, if any)
 * @param flags   flags affecting the parsing.
 *
 * The following flags are used by parser:
 *
 * @li #sdp_f_strict Parser should accept only messages conforming strictly
 *                   to the specification.
 * @li #sdp_f_anynet Parser accepts unknown network or address types.
 * @li #sdp_f_insane Do not run sanity check.
 * @li #sdp_f_c_missing  Sanity check does not require c= for each m= line
 * @li #sdp_f_mode_0000 Parser regards "c=IN IP4 0.0.0.0" as "a=inactive"
 *                      (likewise with c=IN IP6 ::)
 * @li #sdp_f_mode_manual Do not generate or parse SDP mode
 * @li #sdp_f_config   Parse config files (any line can be missing)
 *
 * @return
 * Always a valid parser handle.
 *
 * @todo Parser accepts some non-conforming SDP even with #sdp_f_strict.
 *
 * @sa sdp_session(), sdp_parsing_error(), sdp_sanity_check(),
 * sdp_parser_home(), sdp_parser_free(), @RFC4566, @RFC2327.
 */
sdp_parser_t *
sdp_parse(su_home_t *home, char const msg[], issize_t msgsize, int flags)
{
  sdp_parser_t *p;
  char *b;
  size_t len;

  if (msgsize == -1 || msg == NULL) {
    p = su_home_clone(home, sizeof(*p));
    if (p)
      parsing_error(p, "invalid input message");
    else
      p = (sdp_parser_t*)&no_mem_error;
    return p;
  }

  if (msgsize == -1 && msg)
    len = strlen(msg);
  else
    len = msgsize;

  if (len > ISSIZE_MAX)
    len = ISSIZE_MAX;

  p = su_home_clone(home, sizeof(*p) + len + 1);

  if (p) {
    b = strncpy((void *)(p + 1), msg, len);
    b[len] = 0;

    p->pr_message = b;
    p->pr_strict = (flags & sdp_f_strict) != 0;
    p->pr_anynet = (flags & sdp_f_anynet) != 0;
    p->pr_mode_0000 = (flags & sdp_f_mode_0000) != 0;
    p->pr_insane = (flags & sdp_f_insane) != 0;
    p->pr_c_missing = (flags & sdp_f_c_missing) != 0;
    if (flags & sdp_f_config)
      p->pr_c_missing = 1, p->pr_config = 1;
    p->pr_mode_manual = (flags & sdp_f_mode_manual) != 0;
    p->pr_session_mode = sdp_sendrecv;

    parse_message(p);

    return p;
  }

  if (p)
    sdp_parser_free(p);

  return (sdp_parser_t*)&no_mem_error;
}


/** Obtain memory home used by parser */
su_home_t *sdp_parser_home(sdp_parser_t *parser)
{
  if (parser != &no_mem_error)
    return parser->pr_home;
  else
    return NULL;
}

/** Retrieve an SDP session structure.
 *
 * The function sdp_session() returns a pointer to the SDP session
 * structure associated with the SDP parser @a p. The pointer and all the
 * data in the structure are valid until sdp_parser_free() is called.
 *
 * @param p SDP parser
 *
 * @return
 *   The function sdp_session() returns a pointer to an parsed SDP message
 *   or NULL, if an error has occurred.  */
sdp_session_t *
sdp_session(sdp_parser_t *p)
{
  return p && p->pr_ok ? p->pr_session : NULL;
}

/** Get a parsing error message.
 *
 * The function sdp_parsing_error() returns the error message associated
 * with an SDP parser @a p.
 *
 * @param p SDP parser
 *
 * @return
 * The function sdp_parsing_error() returns a C string describing parsing
 * error, or NULL if no error occurred.
 */
char const *sdp_parsing_error(sdp_parser_t *p)
{
  return !p->pr_ok ? p->pr_error : NULL;
}

/** Free an SDP parser.
 *
 * The function sdp_parser_free() frees an SDP parser object along with
 * the memory blocks associated with it.
 *
 * @param p pointer to the SDP parser to be freed
 */
void sdp_parser_free(sdp_parser_t *p)
{
  if (p && p != &no_mem_error)
    su_home_unref(p->pr_home);
}

/* ========================================================================= */

/* =========================================================================
 * Private part
 */

/* Parsing tokens */
#define SPACE " "
#define TAB   "\011"
#define CRLF  "\015\012"
#define ALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define DIGIT "0123456789"
#define TOKEN ALPHA DIGIT "-!#$%&'*+.^_`{|}~"

/* ========================================================================= */
/* Parsing functions */

static void post_session(sdp_parser_t *p, sdp_session_t *sdp);
static void parse_origin(sdp_parser_t *p, char *r, sdp_origin_t **result);
static void parse_subject(sdp_parser_t *p, char *r, sdp_text_t **result);
static void parse_information(sdp_parser_t *p, char *r, sdp_text_t **result);
static void parse_uri(sdp_parser_t *p, char *r, sdp_text_t **result);
static void parse_email(sdp_parser_t *p, char *r, sdp_list_t **result);
static void parse_phone(sdp_parser_t *p, char *r, sdp_list_t **result);
static void parse_connection(sdp_parser_t *p, char *r, sdp_connection_t **result);
static void parse_bandwidth(sdp_parser_t *p, char *r, sdp_bandwidth_t **result);
static void parse_time(sdp_parser_t *p, char *r, sdp_time_t **result);
static void parse_repeat(sdp_parser_t *p, char *r, sdp_repeat_t **result);
static void parse_zone(sdp_parser_t *p, char *r, sdp_zone_t **result);
static void parse_key(sdp_parser_t *p, char *r, sdp_key_t **result);
static void parse_session_attr(sdp_parser_t *p, char *r, sdp_attribute_t **result);
static void parse_media(sdp_parser_t *p, char *r, sdp_media_t **result);
static void parse_payload(sdp_parser_t *p, char *r, sdp_rtpmap_t **result);
static void parse_media_attr(sdp_parser_t *p, char *r, sdp_media_t *m,
			     sdp_attribute_t **result);
static int parse_rtpmap(sdp_parser_t *p, char *r, sdp_media_t *m);
static int parse_fmtp(sdp_parser_t *p, char *r, sdp_media_t *m);
static void parse_text_list(sdp_parser_t *p, char *r, sdp_list_t **result);

static void parse_descs(sdp_parser_t *p, char *r, char *m, sdp_media_t **result);

static int parse_ul(sdp_parser_t *p, char **r, unsigned long *result,
		    unsigned long max_value);
static int parse_ull(sdp_parser_t *p, char **r, uint64_t *result,
		     uint64_t max_value);
static void parse_alloc_error(sdp_parser_t *p, const char *typename);
static char *next(char **message, const char *sep, const char *strip);
static char *token(char **message, const char *sep, const char *legal,
		   const char *strip);
#if 0
static void check_mandatory(sdp_parser_t *p, sdp_session_t *sdp);
#endif

/* -------------------------------------------------------------------------
 * Macro PARSE_ALLOC
 *
 * Description:
 *   This macro declares a pointer (v) of given type (t). It then allocates
 *   an structure of given type (t). If allocation was succesful, it assigns
 *   the XX_size member with appropriate value.
 */
#define PARSE_ALLOC(p, t, v) \
 t *v = su_salloc(p->pr_home, sizeof(*v)); \
 if (!v && (parse_alloc_error(p, #t), 1)) return;

/* -------------------------------------------------------------------------
 * Macro PARSE_CHECK_REST
 *
 * Description:
 *   This macro check if there is extra data at the end of field.
 */
#define PARSE_CHECK_REST(p, s, n)\
 if (*s && (parsing_error(p, "extra data after %s (\"%.04s\")", n, s), 1)) \
    return

/* -------------------------------------------------------------------------
 * Function parse_message() - parse an SDP message
 *
 * Description:
 *   This function parses an SDP message, which is copied into the
 *   p->pr_message. The p->pr_message is modified during the parsing,
 *   and parts of it are returned in p->pr_session.
 *
 * Parameters:
 *   p - pointer to SDP parser object
 */
static void parse_message(sdp_parser_t *p)
{
/*
   announcement =        proto-version
                         origin-field
                         session-name-field
                         information-field
                         uri-field
                         email-fields
                         phone-fields
                         connection-field
                         bandwidth-fields
                         time-fields
                         key-field
                         attribute-fields
                         media-descriptions
*/

  sdp_session_t *sdp = p->pr_session;
  char *record, *rest;
  char const *strip;
  char *message = p->pr_message;
  char field = '\0';
  sdp_list_t **emails = &sdp->sdp_emails;
  sdp_list_t **phones = &sdp->sdp_phones;
  sdp_bandwidth_t **bandwidths = &sdp->sdp_bandwidths;
  sdp_time_t **times = &sdp->sdp_time;
  sdp_repeat_t **repeats = NULL;
  sdp_zone_t **zones = NULL;
  sdp_attribute_t **attributes = &sdp->sdp_attributes;

  if (!STRICT(p))
    strip = SPACE TAB;		/* skip initial whitespace */
  else
    strip = "";

  p->pr_ok = 1;
  p->pr_session->sdp_size = sizeof(p->pr_session);

  /* Require that version comes first */
  record = next(&message, CRLF, strip);

  if (!su_strmatch(record, "v=0")) {
    if (!p->pr_config || !record || record[1] != '=') {
      parsing_error(p, "bad SDP message");
      return;
    }
  }
  else {
    record = next(&message, CRLF, strip);
  }

  /*
    XXX - the lines in SDP are in certain order, which we don't check here.
     For stricter parsing we might want to parse o= and s= next.
  */

  for (;
       record && p->pr_ok;
       record = next(&message, CRLF, strip)) {
    field = record[0];

    rest = record + 2; rest += strspn(rest, strip);

    if (record[1] != '=') {
      parsing_error(p, "bad line \"%s\"", record);
      return;
    }

    switch (field) {
    case 'o':
      parse_origin(p, rest, &sdp->sdp_origin);
      break;

    case 's':
      parse_subject(p, rest, &sdp->sdp_subject);
      break;

    case 'i':
      parse_information(p, rest, &sdp->sdp_information);
      break;

    case 'u':
      parse_uri(p, rest, &sdp->sdp_uri);
      break;

    case 'e':
      parse_email(p, rest, emails);
      emails = &(*emails)->l_next;
      break;

    case 'p':
      parse_phone(p, rest, phones);
      phones = &(*phones)->l_next;
      break;

    case 'c':
      parse_connection(p, rest, &sdp->sdp_connection);
      break;

    case 'b':
      parse_bandwidth(p, rest, bandwidths);
      bandwidths = &(*bandwidths)->b_next;
      break;

    case 't':
      parse_time(p, rest, times);
      repeats = &(*times)->t_repeat;
      zones = &(*times)->t_zone;
      times = &(*times)->t_next;
      break;

    case 'r':
      if (repeats)
	parse_repeat(p, rest, repeats);
      else
	parsing_error(p, "repeat field without time field");
      break;

    case 'z':
      if (zones)
	parse_zone(p, rest, zones), zones = NULL;
      else
	parsing_error(p, "zone field without time field");
      break;

    case 'k':
      parse_key(p, rest, &sdp->sdp_key);
      break;

    case 'a':
      parse_session_attr(p, rest, attributes);
      if (*attributes)
	attributes = &(*attributes)->a_next;
      break;

    case 'm':
      parse_descs(p, record, message, &sdp->sdp_media);
      post_session(p, sdp);
      return;

    default:
      parsing_error(p, "unknown field \"%s\"", record);
      return;
    }
  }

  post_session(p, sdp);
}
#ifdef SOFIA_AUTO_CORRECT_INADDR_ANY
int sdp_connection_is_inaddr_any(sdp_connection_t const *c)
{
  return
    c &&
    c->c_nettype == sdp_net_in &&
    ((c->c_addrtype == sdp_addr_ip4 && su_strmatch(c->c_address, "0.0.0.0")) ||
     (c->c_addrtype == sdp_addr_ip6 && su_strmatch(c->c_address, "::")));
}
#endif

/**Postprocess session description.
 *
 * Postprocessing includes setting the session backpointer for each media,
 * doing sanity checks and setting rejected and mode flags.
 */
static void post_session(sdp_parser_t *p, sdp_session_t *sdp)
{
  sdp_media_t *m;
#ifdef SOFIA_AUTO_CORRECT_INADDR_ANY
  sdp_connection_t const *c;
#endif

  if (!p->pr_ok)
    return;

  /* Set session back-pointer */
  for (m = sdp->sdp_media; m; m = m->m_next) {
    m->m_session = sdp;
  }

  if (p->pr_config) {
    if (sdp->sdp_version[0] != 0)
      parsing_error(p, "Incorrect version");
    return;
  }

  /* Go through all media and set mode */
  for (m = sdp->sdp_media; m; m = m->m_next) {
    if (m->m_port == 0) {
      m->m_mode = sdp_inactive;
      m->m_rejected = 1;
      continue;
    }

#ifdef SOFIA_AUTO_CORRECT_INADDR_ANY
    c = sdp_media_connections(m);


    if (p->pr_mode_0000 && sdp_connection_is_inaddr_any(c)) {
      /* Reset recvonly flag */
      m->m_mode &= ~sdp_recvonly;
    }
#endif
  }

  if (p->pr_insane)
    return;

  /* Verify that all mandatory fields are present */
  if (sdp_sanity_check(p) < 0)
    return;
}

/** Validates that all mandatory fields exist
 *
 * Checks that all necessary fields (v=, o=) exists in the parsed sdp. If
 * strict, check that all mandatory fields (c=, o=, s=, t=) are present.
 * This function also goes through all media, marks rejected media as such,
 * and updates the mode accordingly.
 *
 * @retval 0 if parsed SDP description is valid
 * @retval -1 if some SDP line is missing
 * @retval -2 if c= line is missing
 */
int sdp_sanity_check(sdp_parser_t *p)
{
  sdp_session_t *sdp = p->pr_session;
  sdp_media_t *m;

  if (!p || !p->pr_ok)
    return -1;
  else if (sdp->sdp_version[0] != 0)
    return parsing_error(p, "Incorrect version");
  else if (!sdp->sdp_origin)
    return parsing_error(p, "No o= present");
  else if (p->pr_strict && !sdp->sdp_subject)
    return parsing_error(p, "No s= present");
  else if (p->pr_strict && !sdp->sdp_time)
    return parsing_error(p, "No t= present");

  /* If there is no session level c= check that one exists for all media */
  /* c= line may be missing if this is a RTSP description */
  if (!p->pr_c_missing && !sdp->sdp_connection) {
    for (m = sdp->sdp_media ; m ; m = m->m_next) {
      if (!m->m_connections && !m->m_rejected) {
	parsing_error(p, "No c= on either session level or all mediums");
	return -2;
      }
    }
  }

  return 0;
}

#if 0
/**
 * Parse a "v=" field
 *
 * The function parser_version() parses the SDP version field.
 *
 * @param p      pointer to SDP parser object
 * @param r      pointer to record data
 * @param result pointer to which parsed record is assigned
 */
static void parse_version(sdp_parser_t *p, char *r, sdp_version_t *result)
{
  /*
   proto-version =       "v=" 1*DIGIT CRLF
                         ;[RFC2327] describes version 0
   */
  if (parse_ul(p, &r, result, 0))
    parsing_error(p, "version \"%s\" is invalid", r);
  else if (*result > 0)
    parsing_error(p, "unknown version v=%s", r);
}
#endif

/* -------------------------------------------------------------------------
 * Function parse_origin() - parse an "o=" field
 *
 * Description:
 *   This function parses an SDP origin field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_origin(sdp_parser_t *p, char *r, sdp_origin_t **result)
{
  /*
   origin-field =        "o=" username space
                         sess-id space sess-version space
                         nettype space addrtype space
                         addr CRLF

   username =            safe
                         ;pretty wide definition, but doesn't include space

   sess-id =             1*(DIGIT)
                         ;should be unique for this originating username/host

   sess-version =        1*(DIGIT)
                         ;0 is a new session


   */
  PARSE_ALLOC(p, sdp_origin_t, o);

  *result = o;

  o->o_username = token(&r, SPACE TAB, NULL, SPACE TAB);
  if (!o->o_username) {
    parsing_error(p, "invalid username");
    return;
  }
  if (parse_ull(p, &r, &o->o_id, 0)) {
    parsing_error(p, "invalid session id");
    return;
  }

  if (parse_ull(p, &r, &o->o_version, 0)) {
    parsing_error(p, "invalid session version");
    return;
  }

  parse_connection(p, r, &o->o_address);
}

/* -------------------------------------------------------------------------
 * Function parse_subject() - parse an "s=" field
 *
 * Description:
 *   This function parses an SDP subject field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_subject(sdp_parser_t *p, char *r, sdp_text_t **result)
{
  /*
   session-name-field =  "s=" text CRLF
   text =                byte-string
   */
  *result = r;
}

/* -------------------------------------------------------------------------
 * Function parse_information() - parse an "i=" field
 *
 * Description:
 *   This function parses an SDP information field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_information(sdp_parser_t *p, char *r, sdp_text_t **result)
{
  /*
   information-field =   ["i=" text CRLF]
   */
  *result = r;
}

/* -------------------------------------------------------------------------
 * Function parse_uri() - parse an "u=" field
 *
 * Description:
 *   This function parses an SDP URI field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_uri(sdp_parser_t *p, char *r, sdp_text_t **result)
{
  /*
    uri-field =           ["u=" uri CRLF]

    uri=                  ;defined in RFC1630
  */
  /* XXX - no syntax checking here */
  *result = r;
}

/* -------------------------------------------------------------------------
 * Function parse_email() - parse an "e=" field
 *
 * Description:
 *   This function parses an SDP email field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_email(sdp_parser_t *p, char *r, sdp_list_t **result)
{
  /*
   email-fields =        *("e=" email-address CRLF)

   email-address =       email | email "(" email-safe ")" |
                         email-safe "<" email ">"

   email =               ;defined in RFC822  */
  parse_text_list(p, r, result);
}

/* -------------------------------------------------------------------------
 * Function parse_phone() - parse an "p=" field
 *
 * Description:
 *   This function parses an SDP phone field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_phone(sdp_parser_t *p, char *r, sdp_list_t **result)
{
  /*
   phone-fields =        *("p=" phone-number CRLF)

   phone-number =        phone | phone "(" email-safe ")" |
                         email-safe "<" phone ">"

   phone =               "+" POS-DIGIT 1*(space | "-" | DIGIT)
                         ;there must be a space or hyphen between the
                         ;international code and the rest of the number.
  */
  parse_text_list(p, r, result);
}

/* -------------------------------------------------------------------------
 * Function parse_connection() - parse an "c=" field
 *
 * Description:
 *   This function parses an SDP connection field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_connection(sdp_parser_t *p, char *r, sdp_connection_t **result)
{
  /*
   connection-field =    ["c=" nettype space addrtype space
                         connection-address CRLF]
                         ;a connection field must be present
                         ;in every media description or at the
                         ;session-level

   nettype =             "IN"
                         ;list to be extended

   addrtype =            "IP4" | "IP6"
                         ;list to be extended

   connection-address =  multicast-address
                         | addr

   multicast-address =   3*(decimal-uchar ".") decimal-uchar "/" ttl
                         [ "/" integer ]
                         ;multicast addresses may be in the range
                         ;224.0.0.0 to 239.255.255.255

   ttl =                 decimal-uchar

   addr =                FQDN | unicast-address

   FQDN =                4*(alpha-numeric|"-"|".")
                         ;fully qualified domain name as specified in RFC1035

   unicast-address =     IP4-address | IP6-address

   IP4-address =         b1 "." decimal-uchar "." decimal-uchar "." b4
   b1 =                  decimal-uchar
                         ;less than "224"; not "0" or "127"
   b4 =                  decimal-uchar
                         ;not "0"

   IP6-address =         ;to be defined
   */
  PARSE_ALLOC(p, sdp_connection_t, c);

  *result = c;

  if (su_casenmatch(r, "IN", 2)) {
    char *s;

    /* nettype is internet */
    c->c_nettype = sdp_net_in;
    s = token(&r, SPACE TAB, NULL, NULL);

    /* addrtype */
    s = token(&r, SPACE TAB, NULL, NULL);
    if (su_casematch(s, "IP4"))
      c->c_addrtype = sdp_addr_ip4;
    else if (su_casematch(s, "IP6"))
      c->c_addrtype = sdp_addr_ip6;
    else {
      parsing_error(p, "unknown IN address type: %s", s);
      return;
    }

    /* address */
    s = next(&r, SPACE TAB, SPACE TAB);
    c->c_address = s;
    if (!s || !*s) {
      parsing_error(p, "invalid address");
      return;
    }

    /* ttl */
    s = strchr(s, '/');
    if (s) {
      unsigned long value;
      *s++ = 0;
      if (parse_ul(p, &s, &value, 256) ||
	  (*s && *s != '/')) {
	parsing_error(p, "invalid ttl");
	return;
      }
      c->c_ttl = value;
      c->c_mcast = 1;

      /* multiple groups */
      value = 1;
      if (*s++ == '/')
	if (parse_ul(p, &s, &value, 0) || *s) {
	  parsing_error(p, "invalid number of multicast groups");
	  return;
	}
      c->c_groups = value;
    }
    else
      c->c_groups = 1;
  }
  else if (p->pr_anynet) {
    c->c_nettype = sdp_net_x;
    c->c_addrtype = sdp_addr_x;
    c->c_address = r;
    c->c_ttl = 0;
    c->c_groups = 1;
  }
  else
    parsing_error(p, "invalid address");
}

/* -------------------------------------------------------------------------
 * Function parse_bandwidth() - parse an "b=" field
 *
 * Description:
 *   This function parses an SDP bandwidth field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_bandwidth(sdp_parser_t *p, char *r, sdp_bandwidth_t **result)
{
  /*
   bandwidth-fields =    *("b=" bwtype ":" bandwidth CRLF)
   bwtype =              token
   bandwidth =           1*(DIGIT)
   */
  /* NOTE: bwtype can also be like X-barf */
  sdp_bandwidth_e modifier;
  char *name;
  unsigned long value;

  name = token(&r, ":", TOKEN, SPACE TAB);

  if (name == NULL || parse_ul(p, &r, &value, 0)) {
    parsing_error(p, "invalid bandwidth");
    return;
  }

  if (su_casematch(name, "CT"))
    modifier = sdp_bw_ct, name = "CT";
  else if (su_casematch(name, "TIAS") == 1)
    modifier = sdp_bw_tias, name = "TIAS";
  else if (su_casematch(name, "AS") == 1)
    modifier = sdp_bw_as, name = "AS";
  else
	modifier = sdp_bw_x, name = "BW-X";

  if (STRICT(p))
    PARSE_CHECK_REST(p, r, "b");

  {
    PARSE_ALLOC(p, sdp_bandwidth_t, b);
    *result = b;
    b->b_modifier = modifier;
    b->b_modifier_name = name;
    b->b_value = value;
  }
}

/* -------------------------------------------------------------------------
 * Function parse_time() - parse an "t=" field
 *
 * Description:
 *   This function parses an SDP time field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_time(sdp_parser_t *p, char *r, sdp_time_t **result)
{
  /*
   time-fields =         1*( "t=" start-time SP stop-time
                         *(CRLF repeat-fields) CRLF)
                         [zone-adjustments CRLF]

   start-time =          time / "0"

   stop-time =           time / "0"

   time =                POS-DIGIT 9*DIGIT
                         ; Decimal representation of NTP time in
                         ; seconds since 1900.  The representation
                         ; of NTP time is an unbounded length field
                         ; containing at least 10 digits.  Unlike the
                         ; 64-bit representation used elsewhere, time
                         ; in SDP does not wrap in the year 2036.
   */
  PARSE_ALLOC(p, sdp_time_t, t);
  *result = t;
  if (parse_ul(p, &r, &t->t_start, 0) ||
      parse_ul(p, &r, &t->t_stop, 0))
    parsing_error(p, "invalid time");
  else if (STRICT(p)) {
    PARSE_CHECK_REST(p, r, "t");
  }
}

/**
 * Parse an "r=" field
 *
 * The function parse_repeat() parses an SDP repeat field.
 *
 * @param p      pointer to SDP parser object
 * @param r      pointer to record data
 * @param result pointer to which parsed record is assigned
 *
 */
static void parse_repeat(sdp_parser_t *p, char *d, sdp_repeat_t **result)
{
  /*
   repeat-fields =       %x72 "=" repeat-interval 2*(SP typed-time)

   repeat-interval =     POS-DIGIT *DIGIT [fixed-len-time-unit]

   typed-time =          1*DIGIT [fixed-len-time-unit]

   fixed-len-time-unit = %x64 / %x68 / %x6d / %x73 ; "d" | "h" | "m" | "s"
   */

  unsigned long tt, *interval;
  size_t i;
  int n, N;
  char *s;
  sdp_repeat_t *r;
  int strict = STRICT(p);

  /** Count number of intervals */
  for (N = 0, s = d; *s; ) {
    if (!(is_posdigit(*s) || (!strict && (*s) == '0')))
      break;
    do { s++; } while (is_digit(*s));
    if (*s && strchr(strict ? "dhms" : "dhmsDHMS", *s))
      s++;
    N++;
    if (!(i = strict ? is_space(*s) : strspn(s, SPACE TAB)))
      break;
    s += i;
  }

  PARSE_CHECK_REST(p, s, "r");
  if (N < 2) {
    parsing_error(p, "invalid repeat");
    return;
  }
  if (!(r = su_salloc(p->pr_home, offsetof(sdp_repeat_t, r_offsets[N - 1])))) {
    parse_alloc_error(p, "sdp_repeat_t");
    return;
  }

  r->r_number_of_offsets = N - 2;
  r->r_offsets[N - 2] = 0;

  for (n = 0, interval = &r->r_interval; n < N; n++) {
    tt = strtoul(d, &d, 10);

    switch (*d) {
    case 'd': case 'D': tt *= 24;
    case 'h': case 'H': tt *= 60;
    case 'm': case 'M': tt *= 60;
    case 's': case 'S': d++;
      break;
    }

    interval[n] = tt;

    while (is_space(*d))
      d++;
  }

  *result = r;
}

/* -------------------------------------------------------------------------
 * Function parse_zone() - parse an "z=" field
 *
 * Description:
 *   This function parses an SDP time zone field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 *
 */
static void parse_zone(sdp_parser_t *p, char *r, sdp_zone_t **result)
{
  char *s;
  size_t i;
  int n, N;
  sdp_zone_t *z;

  /*
   zone-adjustments =    time space ["-"] typed-time
                         *(space time space ["-"] typed-time)
   */

  /** Count number of timezones, check syntax */
  for (N = 0, s = r; *s;) {
    if (!(is_posdigit(*s) || (!STRICT(p) && (*s) == '0')))
      break;
    do { s++; } while (is_digit(*s));
    if (!(i = STRICT(p) ? is_space(*s) : strspn(s, SPACE TAB)))
      break;
    s += i;
    if (!(*s == '-' || is_posdigit(*s) || (!STRICT(p) && (*s) == '0')))
      break;
    do { s++; } while (is_digit(*s));
    if (*s && strchr("dhms", *s))
      s++;
    N++;
    if (!(i = STRICT(p) ? is_space(*s) : strspn(s, SPACE TAB)))
      break;
    s += i;
  }

  PARSE_CHECK_REST(p, s, "z");

  if (N < 1) {
    parsing_error(p, "invalid timezone");
    return;
  }
  if (!(z = su_salloc(p->pr_home, offsetof(sdp_zone_t, z_adjustments[N])))) {
    parse_alloc_error(p, "sdp_zone_t");
    return;
  }

  z->z_number_of_adjustments = N;

  for (n = 0; n < N; n++) {
    unsigned long at = strtoul(r, &r, 10);
    long offset = strtol(r, &r, 10);
    switch (*r) {
    case 'd': offset *= 24;
    case 'h': offset *= 60;
    case 'm': offset *= 60;
    case 's': r++;
      break;
    }

    z->z_adjustments[n].z_at = at;
    z->z_adjustments[n].z_offset = offset;
  }

  *result = z;
}

/* -------------------------------------------------------------------------
 * Function parse_key() - parse an "k=" field
 *
 * Description:
 *   This function parses an SDP key field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 *
 */
static void parse_key(sdp_parser_t *p, char *r, sdp_key_t **result)
{
  char *s;
  /*
   key-field =           ["k=" key-type CRLF]

   key-type =            "prompt" |
                         "clear:" key-data |
                         "base64:" key-data |
                         "uri:" uri

   key-data =            email-safe | "~" | "
   */

  s = token(&r, ":", TOKEN, SPACE TAB);
  if (!s) {
    parsing_error(p, "invalid key method");
    return;
  }

  {
    PARSE_ALLOC(p, sdp_key_t, k);
    *result = k;

    /* These are defined as key-sensitive in RFC 4566 */
#define MATCH(s, tok) \
    (STRICT(p) ? su_strmatch((s), (tok)) : su_casematch((s), (tok)))

    if (MATCH(s, "clear"))
      k->k_method = sdp_key_clear, k->k_method_name = "clear";
    else if (MATCH(s, "base64"))
      k->k_method = sdp_key_base64, k->k_method_name = "base64";
    else if (MATCH(s, "uri"))
      k->k_method = sdp_key_uri, k->k_method_name = "uri";
    else if (MATCH(s, "prompt"))
      k->k_method = sdp_key_prompt, k->k_method_name = "prompt";
    else if (!STRICT(p))
      k->k_method = sdp_key_x, k->k_method_name = s;
    else {
      parsing_error(p, "invalid key method");
      return;
    }

    k->k_material = r;
  }
}

/* -------------------------------------------------------------------------
 * Function parse_session_attr() - parse a session "a=" field
 *
 * Description:
 *   This function parses an SDP attribute field regarding whole session.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_session_attr(sdp_parser_t *p, char *r, sdp_attribute_t **result)
{
  /*
   attribute-fields =    *("a=" attribute CRLF)

   attribute =           (att-field ":" att-value) / att-field

   att-field =           token

   att-value =           byte-string
   */

  char *name = NULL, *value = NULL;

  if (!(name = token(&r, ":", TOKEN, SPACE TAB))) {
    parsing_error(p,"invalid attribute name");
    return;
  }

  if (*r)
    value = r;
  else
    PARSE_CHECK_REST(p, r, "a");

  if (su_casematch(name, "charset")) {
    p->pr_session->sdp_charset = value;
    return;
  }

  if (p->pr_mode_manual)
    ;
  else if (su_casematch(name, "inactive"))
    p->pr_session_mode = sdp_inactive;
  else if (su_casematch(name, "sendonly"))
    p->pr_session_mode = sdp_sendonly;
  else if (su_casematch(name, "recvonly"))
    p->pr_session_mode = sdp_recvonly;
  else if (su_casematch(name, "sendrecv"))
    p->pr_session_mode = sdp_sendrecv;

  {
    PARSE_ALLOC(p, sdp_attribute_t, a);
    *result = a;

    a->a_name  = name;
    a->a_value = value;
  }
}

/* -------------------------------------------------------------------------
 * Function parse_media() - parse an "m=" field
 *
 * Description:
 *   This function parses an SDP media field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_media(sdp_parser_t *p, char *r, sdp_media_t **result)
{
  /*
   media-descriptions =  *( media-field
                         information-field
                         *(connection-field)
                         bandwidth-fields
                         key-field
                         attribute-fields )

   media-field =         "m=" media space port ["/" integer]
                         space proto 1*(space fmt) CRLF

   media =               token
                         ;typically "audio", "video", "application"
                         ;or "data" or "text"

   fmt =                 token
                         ;typically an RTP payload type for audio
                         ;and video media

   proto =               token *("/" token)
                         ;typically "RTP/AVP" or "udp" for IP4

   port =                1*(DIGIT)
                         ;should in the range "1024" to "65535" inclusive
   */
  char *s;
  unsigned long value;
  PARSE_ALLOC(p, sdp_media_t, m);

  *result = m;

  m->m_mode = sdp_sendrecv;

  s = token(&r, SPACE, TOKEN, NULL);
  if (!s) {
    parsing_error(p, "m= invalid media field");
    return;
  }

  sdp_media_type(m, s);

  /* Accept m=* in configuration file */
  if (p->pr_config && m->m_type == sdp_media_any) {
    r += strspn(r, SPACE TAB);
    if (r[0] == '\0') {
      m->m_proto = sdp_proto_any, m->m_proto_name = "*";
      return;
    }
  }

  if (parse_ul(p, &r, &value, 0)) {
    parsing_error(p, "m= invalid port number");
    return;
  }
  m->m_port = value;

  if (*r == '/') {
    r++;
    if (parse_ul(p, &r, &value, 0)) {
      parsing_error(p, "m= invalid port specification");
      return;
    }
    m->m_number_of_ports = value;
  }

  s = token(&r, SPACE, "/" TOKEN, SPACE);
  if (s == NULL) {
    parsing_error(p, "m= missing protocol");
    return;
  }

  if (!STRICT(p) && su_casematch(s, "RTP"))
    m->m_proto = sdp_proto_rtp, m->m_proto_name = "RTP/AVP";
  else
    sdp_media_transport(m, s);

  /* RTP format list */
  if (*r && sdp_media_has_rtp(m)) {
	  parse_payload(p, r, &m->m_rtpmaps);
	  return;
  }

  /* "normal" format list */
  if (*r) {
    sdp_list_t **fmt = &m->m_format;

    while (r && *r) {
      PARSE_ALLOC(p, sdp_list_t, l);
      *fmt = l;
      l->l_text = token(&r, SPACE TAB, TOKEN, SPACE TAB);
      fmt = &l->l_next;
    }
  }
}

/** Set media type */
void sdp_media_type(sdp_media_t *m, char const *s)
{
  if (su_strmatch(s, "*"))
    m->m_type = sdp_media_any, m->m_type_name = "*";
  else if (su_casematch(s, "audio"))
    m->m_type = sdp_media_audio, m->m_type_name = "audio";
  else if (su_casematch(s, "video"))
    m->m_type = sdp_media_video, m->m_type_name = "video";
  else if (su_casematch(s, "application"))
    m->m_type = sdp_media_application, m->m_type_name = "application";
  else if (su_casematch(s, "data"))
    m->m_type = sdp_media_data, m->m_type_name = "data";
  else if (su_casematch(s, "control"))
    m->m_type = sdp_media_control, m->m_type_name = "control";
  else if (su_casematch(s, "message"))
    m->m_type = sdp_media_message, m->m_type_name = "message";
  else if (su_casematch(s, "image"))
    m->m_type = sdp_media_image, m->m_type_name = "image";
  else if (su_casematch(s, "red"))
    m->m_type = sdp_media_red, m->m_type_name = "red";
  else if (su_casematch(s, "text"))
    m->m_type = sdp_media_text, m->m_type_name = "text";
  else
    m->m_type = sdp_media_x, m->m_type_name = s;
}

/** Set transport protocol.
 *
 * Set the @m->m_proto to a well-known protocol type as
 * well as canonize case of @a m_proto_name.
 */
void sdp_media_transport(sdp_media_t *m, char const *s)
{
  if (m == NULL || s == NULL)
    ;
  else if (su_strmatch(s, "*"))
    m->m_proto = sdp_proto_any, m->m_proto_name = "*";
  else if (su_casematch(s, "RTP/AVP"))
    m->m_proto = sdp_proto_rtp, m->m_proto_name = "RTP/AVP";
  else if (su_casematch(s, "RTP/SAVP"))
    m->m_proto = sdp_proto_srtp, m->m_proto_name = "RTP/SAVP";
  else if (su_casematch(s, "UDP/TLS/RTP/SAVP"))
    m->m_proto = sdp_proto_srtp, m->m_proto_name = "RTP/SAVP";
  else if (su_casematch(s, "RTP/SAVPF"))
	  m->m_proto = sdp_proto_extended_srtp, m->m_proto_name = "RTP/SAVPF";
  else if (su_casematch(s, "UDP/TLS/RTP/SAVPF"))
    m->m_proto = sdp_proto_extended_srtp, m->m_proto_name = "UDP/TLS/RTP/SAVPF";
  else if (su_casematch(s, "RTP/AVPF"))
	  m->m_proto = sdp_proto_extended_rtp, m->m_proto_name = "RTP/AVPF";
  else if (su_casematch(s, "UDP/RTP/AVPF"))
    m->m_proto = sdp_proto_extended_rtp, m->m_proto_name = "UDP/RTP/AVPF";
  else if (su_casematch(s, "udptl"))
    /* Lower case - be compatible with people living by T.38 examples */
    m->m_proto = sdp_proto_udptl, m->m_proto_name = "udptl";
  else if (su_casematch(s, "TCP/MSRP"))
    m->m_proto = sdp_proto_msrp, m->m_proto_name = "TCP/MSRP";
  else if (su_casematch(s, "TCP/TLS/MSRP"))
    m->m_proto = sdp_proto_msrps, m->m_proto_name = "TCP/TLS/MSRP";
  else if (su_casematch(s, "UDP"))
    m->m_proto = sdp_proto_udp, m->m_proto_name = "UDP";
  else if (su_casematch(s, "TCP"))
    m->m_proto = sdp_proto_tcp, m->m_proto_name = "TCP";
  else if (su_casematch(s, "TLS"))
    m->m_proto = sdp_proto_tls, m->m_proto_name = "TLS";
  else
    m->m_proto = sdp_proto_x, m->m_proto_name = s;
}

/** Check if media uses RTP as its transport protocol.  */
int sdp_media_has_rtp(sdp_media_t const *m)
{
	return m && (m->m_proto == sdp_proto_rtp || m->m_proto == sdp_proto_srtp || m->m_proto == sdp_proto_extended_srtp || m->m_proto == sdp_proto_extended_rtp);
}

#define RTPMAP(pt, encoding, rate, params) \
  { sizeof(sdp_rtpmap_t), NULL, encoding, rate, (char *)params, NULL, 1, pt, 0 }

/* rtpmaps for well-known codecs */
static sdp_rtpmap_t const
  sdp_rtpmap_pcmu = RTPMAP(0, "PCMU", 8000, 0),
  sdp_rtpmap_1016 = RTPMAP(1, "1016", 8000, 0),
  sdp_rtpmap_g721 = RTPMAP(2, "G721", 8000, 0),
  sdp_rtpmap_gsm =  RTPMAP(3, "GSM",  8000, 0),
  sdp_rtpmap_g723 = RTPMAP(4, "G723", 8000, 0),
  sdp_rtpmap_dvi4_8000 = RTPMAP(5, "DVI4", 8000, 0),
  sdp_rtpmap_dvi4_16000 = RTPMAP(6, "DVI4", 16000, 0),
  sdp_rtpmap_lpc = RTPMAP(7, "LPC",  8000, 0),
  sdp_rtpmap_pcma = RTPMAP(8, "PCMA", 8000, 0),
  sdp_rtpmap_g722 = RTPMAP(9, "G722", 8000, 0),
  sdp_rtpmap_l16_2 = RTPMAP(10, "L16", 44100, "2"),
  sdp_rtpmap_l16 = RTPMAP(11, "L16", 44100, 0),
  sdp_rtpmap_qcelp = RTPMAP(12, "QCELP", 8000, 0),
  sdp_rtpmap_cn = RTPMAP(13, "CN", 8000, 0),
  sdp_rtpmap_mpa = RTPMAP(14, "MPA", 90000, 0),
  sdp_rtpmap_g728 = RTPMAP(15, "G728", 8000, 0),
  sdp_rtpmap_dvi4_11025 = RTPMAP(16, "DVI4", 11025, 0),
  sdp_rtpmap_dvi4_22050 = RTPMAP(17, "DVI4", 22050, 0),
  sdp_rtpmap_g729 = RTPMAP(18, "G729", 8000, 0),
  sdp_rtpmap_reserved_cn = RTPMAP(19, "CN", 8000, 0),
  /* video codecs */
  sdp_rtpmap_celb = RTPMAP(25, "CelB", 90000, 0),
  sdp_rtpmap_jpeg = RTPMAP(26, "JPEG", 90000, 0),
  sdp_rtpmap_nv = RTPMAP(28, "nv",   90000, 0),
  sdp_rtpmap_h261 = RTPMAP(31, "H261", 90000, 0),
  sdp_rtpmap_mpv = RTPMAP(32, "MPV",  90000, 0),
  sdp_rtpmap_mp2t = RTPMAP(33, "MP2T", 90000, 0),
  sdp_rtpmap_h263 = RTPMAP(34, "H263", 90000, 0);

/** Table of rtpmap structures by payload type numbers.
 *
 * The table of reserved payload numbers is constructed from @RFC3551
 * and @RFC1890. Note the clock rate of G722.
 *
 * Use sdp_rtpmap_dup() to copy these structures.
 */
sdp_rtpmap_t const * const sdp_rtpmap_well_known[128] =
{
  &sdp_rtpmap_pcmu,		/* 0 */
  &sdp_rtpmap_1016,		/* 1 */
  &sdp_rtpmap_g721,		/* 2 */
  &sdp_rtpmap_gsm,		/* 3 */
  &sdp_rtpmap_g723,		/* 4 */
  &sdp_rtpmap_dvi4_8000,	/* 5 */
  &sdp_rtpmap_dvi4_16000,	/* 6 */
  &sdp_rtpmap_lpc,		/* 7 */
  &sdp_rtpmap_pcma,		/* 8 */
  &sdp_rtpmap_g722,		/* 9 */
  &sdp_rtpmap_l16_2,		/* 10 */
  &sdp_rtpmap_l16,		/* 11 */
  &sdp_rtpmap_qcelp,		/* 12 */
  &sdp_rtpmap_cn,		/* 13 */
  &sdp_rtpmap_mpa,		/* 14 */
  &sdp_rtpmap_g728,		/* 15 */
  &sdp_rtpmap_dvi4_11025,	/* 16 */
  &sdp_rtpmap_dvi4_22050,	/* 17 */
  &sdp_rtpmap_g729,		/* 18 */
  &sdp_rtpmap_reserved_cn,	/* 19 */
  NULL,				/* 20 */
  NULL,				/* 21 */
  NULL,				/* 22 */
  NULL,				/* 23 */
  NULL,				/* 24 */
  &sdp_rtpmap_celb,		/* 25 */
  &sdp_rtpmap_jpeg,		/* 26 */
  NULL,				/* 27 */
  &sdp_rtpmap_nv,		/* 28 */
  NULL,				/* 29 */
  NULL,				/* 30 */
  &sdp_rtpmap_h261,		/* 31 */
  &sdp_rtpmap_mpv,		/* 32 */
  &sdp_rtpmap_mp2t,		/* 33 */
  &sdp_rtpmap_h263,		/* 34 */
  NULL,
};

/**
 * The function parse_payload() parses an RTP payload type list, and
 * creates an rtpmap structure for each payload type.
 *
 * @param p       pointer to SDP parser object
 * @param r       pointer to record data
 * @param result  pointer to which parsed record is assigned
 */
static void parse_payload(sdp_parser_t *p, char *r, sdp_rtpmap_t **result)
{
  while (*r) {
    unsigned long value;

    if (parse_ul(p, &r, &value, 128) == 0) {
      PARSE_ALLOC(p, sdp_rtpmap_t, rm);

      assert(0 <= value && value < 128);

      *result = rm; result = &rm->rm_next;

      if (sdp_rtpmap_well_known[value]) {
	*rm = *sdp_rtpmap_well_known[value];
      }
      else {
	rm->rm_predef = 1;
	rm->rm_pt = value;
	rm->rm_encoding = "";
	rm->rm_rate = 0;
      }
    }
    else if (p->pr_config && r[0] == '*' && (r[1] == ' ' || r[1] == '\0')) {
      PARSE_ALLOC(p, sdp_rtpmap_t, rm);

      *result = rm; result = &rm->rm_next;

      rm->rm_predef = 1;
      rm->rm_any = 1;
      rm->rm_encoding = "*";
      rm->rm_rate = 0;

      return;
    }
    else {
      parsing_error(p, "m= invalid format for RTP/AVT");

      return;
    }
  }
}

/* -------------------------------------------------------------------------
 * Function parse_media_attr() - parse a media-specific "a=" field
 *
 * Description:
 *   This function parses a media-specific attribute field.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   r      - pointer to record data
 *   result - pointer to which parsed record is assigned
 */
static void parse_media_attr(sdp_parser_t *p, char *r, sdp_media_t *m,
			     sdp_attribute_t **result)
{
  /*
   attribute-fields =    *("a=" attribute CRLF)

   attribute =           (att-field ":" att-value) / att-field

   att-field =           token

   att-value =           byte-string

   a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters>]
   a=fmtp:<payload type> <parameters>
   */
  int rtp = sdp_media_has_rtp(m);
  char *name = NULL, *value = NULL;
  int n;

  if (!(name = token(&r, ":", TOKEN, SPACE TAB))) {
    parsing_error(p,"invalid attribute name");
    return;
  }

  if (*r)
    value = r;
  else
    PARSE_CHECK_REST(p, r, "a");

  if (p->pr_mode_manual)
    ;
  else if (m->m_port == 0 || su_casematch(name, "inactive")) {
    m->m_mode = sdp_inactive;
    return;
  }
  else if (su_casematch(name, "sendonly")) {
    m->m_mode = sdp_sendonly;
    return;
  }
  else if (su_casematch(name, "recvonly")) {
    m->m_mode = sdp_recvonly;
    return;
  }
  else if (su_casematch(name, "sendrecv")) {
    m->m_mode = sdp_sendrecv;
    return;
  }

  if (rtp && su_casematch(name, "rtpmap")) {
	  if ((n = parse_rtpmap(p, r, m)) == 0 || n < -1)
		  return;
  }
  else if (rtp && su_casematch(name, "fmtp")) {
    if ((n = parse_fmtp(p, r, m)) == 0 || n < -1)
      return;
  }
  else {
    PARSE_ALLOC(p, sdp_attribute_t, a);
    *result = a;

    a->a_name  = name;
    a->a_value = value;
  }
}

/** Parse rtpmap attribute.
 *
 * a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters>]
 */
static int parse_rtpmap(sdp_parser_t *p, char *r, sdp_media_t *m)
{
  unsigned long pt, rate;
  char *encoding, *params;
  sdp_rtpmap_t *rm;

  int strict = STRICT(p);

  if (parse_ul(p, &r, &pt, 128)) {
    if (strict)
      parsing_error(p, "a=rtpmap: invalid payload type");
    return -1;
  }

  for (rm = m->m_rtpmaps; rm; rm = rm->rm_next)
    if (rm->rm_pt == pt)
      break;

  if (!rm) {
    if (strict)
      parsing_error(p, "a=rtpmap:%lu: unknown payload type", pt);
    return -1;
  }

  encoding = token(&r, "/", TOKEN, NULL);
  if (!r) {
    parsing_error(p, "a=rtpmap:%lu: missing <clock rate>", pt);
    return -2;
  }
  if (parse_ul(p, &r, &rate, 0)) {
    parsing_error(p, "a=rtpmap:%lu %s: invalid <clock rate>", pt, encoding);
    return -2;
  }

  if (*r == '/')
    params = ++r;
  else
    params = 0;

  rm->rm_predef = 0;
  rm->rm_encoding = encoding;
  rm->rm_rate = rate;
  rm->rm_params = params;

  return 0;
}

/** Parse fmtp attribute.
 *
 * a=fmtp:<payload type> <parameters>
 */
static int parse_fmtp(sdp_parser_t *p, char *r, sdp_media_t *m)
{
  unsigned long pt;
  sdp_rtpmap_t *rm;

  int strict = STRICT(p);

  if (parse_ul(p, &r, &pt, 128)) {
    if (strict)
      parsing_error(p, "a=rtpmap: invalid payload type");
    return -1;
  }

  for (rm = m->m_rtpmaps; rm; rm = rm->rm_next)
    if (rm->rm_pt == pt)
      break;

  if (!rm) {
    if (strict)
      parsing_error(p, "a=fmtp:%lu: unknown payload type", pt);
    return -1;
  }

  rm->rm_fmtp = r;
  return 0;
}

/* -------------------------------------------------------------------------
 * Function parse_descs() - parse media descriptors
 *
 * Description:
 *   This function parses media descriptors at the end of SDP message.
 *
 * Parameters:
 *   p      - pointer to SDP parser object
 *   record - pointer to first media field
 *   message - pointer to rest
 *   medias - pointer to which parsed media structures are assigned
 */
static void parse_descs(sdp_parser_t *p,
			char *record,
			char *message,
			sdp_media_t **medias)
{
  char *rest;
  const char *strip;
  sdp_media_t *m = NULL;
  sdp_connection_t **connections = NULL;
  sdp_bandwidth_t **bandwidths = NULL;
  sdp_attribute_t **attributes = NULL;

  if (!STRICT(p))
    strip = SPACE TAB;		/* skip initial whitespace */
  else
    strip = "";

  for (;
       record && p->pr_ok;
       record = next(&message, CRLF, strip)) {
    char field = record[0];

    rest = record + 2; rest += strspn(rest, strip);

    if (record[1] == '=') switch (field) {
    case 'c':
      assert(connections);
      parse_connection(p, rest, connections);
      connections = &(*connections)->c_next;
      break;

    case 'b':
      assert(bandwidths);
      parse_bandwidth(p, rest, bandwidths);
      bandwidths = &(*bandwidths)->b_next;
      break;

    case 'k':
      parse_key(p, rest, &m->m_key);
      break;

    case 'a':
      assert(attributes);
      parse_media_attr(p, rest, m, attributes);
      if (*attributes)
	attributes = &(*attributes)->a_next;
      break;

    case 'm':
      parse_media(p, rest, medias);
      m = *medias;
      if (m) {
	m->m_mode = p->pr_session_mode;
	medias = &m->m_next;
	connections = &m->m_connections;
	bandwidths = &m->m_bandwidths;
	attributes = &m->m_attributes;
      }
    }
  }
}

static void parse_text_list(sdp_parser_t *p, char *r, sdp_list_t **result)
{
  PARSE_ALLOC(p, sdp_list_t, l);

  *result = l;

  l->l_text = r;
}

/*
 * parse_ul: parse an unsigned long
 */
static int parse_ul(sdp_parser_t *p, char **r,
		    unsigned long *result, unsigned long max)
{
  char *ul = *r;

  ul += strspn(ul, SPACE TAB);

  *result = strtoul(ul, r, 10);
  if (ul != *r && !(max && max <= *result)) {
    *r += strspn(*r, SPACE TAB);
    return 0;
  }

  return -1;
}

#if !HAVE_STRTOULL
#if !((defined(WIN32) || defined(_WIN32)) && (_MSC_VER >= 1800))
unsigned long long strtoull(char const *string, char **return_end, int base);
#endif
#endif

/*
 * parse_ull: parse an unsigned long long
 */
static int parse_ull(sdp_parser_t *p, char **r,
		     uint64_t *result, uint64_t max)
{
  unsigned long long ull;

  char *s = *r;

  s += strspn(s, SPACE TAB);

  ull = strtoull(s, r, 10);

  if (s != *r && !(max && max <= ull)) {
    *result = (uint64_t)ull;
    *r += strspn(*r, SPACE TAB);
    return 0;
  }

  return -1;
}

static char *token(char **message,
		   const char *sep,
		   const char *legal,
		   const char *strip)
{
  size_t n;
  char *retval = *message;

  if (strip)
    retval += strspn(retval, strip);

  if (legal)
    n = strspn(retval, legal);
  else
    n = strcspn(retval, sep);

  if (n == 0)
    return NULL;

  if (retval[n]) {
    retval[n++] = '\0';
    n += strspn(retval + n, sep);
  }

  *message = retval + n;

  if (*retval == '\0')
    return NULL;

  return retval;
}

static char *next(char **message, const char *sep, const char *strip)
{
  size_t n;
  char *retval = *message;

  if (strip[0])
    retval += strspn(retval, strip);

  n = strcspn(retval, sep);

  if (n == 0)
    return NULL;

  if (retval[n]) {
    retval[n++] = '\0';
    n += strspn(retval + n, sep);
  }

  *message = retval + n;

  if (*retval == '\0')
    return NULL;

  return retval;
}

static int parsing_error(sdp_parser_t *p, char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  memset(p->pr_error, 0, sizeof(p->pr_error));
  vsnprintf(p->pr_error, sizeof(p->pr_error), fmt, ap);
  va_end(ap);

  p->pr_ok = 0;

  return -1;
}

static void parse_alloc_error(sdp_parser_t *p, const char *typename)
{
  parsing_error(p, "memory exhausted (while allocating memory for %s)",
		typename);
}
