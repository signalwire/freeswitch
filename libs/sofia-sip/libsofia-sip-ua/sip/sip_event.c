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

/**@CFILE sip_event.c
 * @brief Event SIP headers.
 *
 * Implementation of header classes for event-related SIP headers @Event,
 * @AllowEvents, and @SubscriptionState.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Sep 13 21:24:15 EEST 2001 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_event Event Header
 *
 * The Event header is used to indicate the which event or class of events
 * the message contains or subscribes. Its syntax is defined in @RFC3265 as
 * follows:
 *
 * @code
 *   Event             =  ( "Event" / "o" ) HCOLON event-type
 *                         *( SEMI event-param )
 *   event-type        =  event-package *( "." event-template )
 *   event-package     =  token-nodot
 *   event-template    =  token-nodot
 *   token-nodot       =  1*( alphanum / "-"  / "!" / "%" / "*"
 *                             / "_" / "+" / "`" / "'" / "~" )
 *   event-param      =  generic-param / ( "id" EQUAL token )
 * @endcode
 *
 * The parsed Event header is stored in #sip_event_t structure.
 */

/**@ingroup sip_event
 * @typedef struct sip_event_s sip_event_t;
 *
 * The structure #sip_event_t contains representation of an @Event header.
 *
 * The #sip_event_t is defined as follows:
 * @code
 * typedef struct sip_event_s
 * {
 *   sip_common_t        o_common;	    // Common fragment info
 *   sip_error_t        *o_next;	    // Link to next (dummy)
 *   char const *        o_type;	    // Event type
 *   msg_param_t const  *o_params;	    // List of parameters
 *   char const         *o_id;	    	    // Event ID
 * } sip_event_t;
 * @endcode
 */

static msg_xtra_f sip_event_dup_xtra;
static msg_dup_f sip_event_dup_one;
static msg_update_f sip_event_update;

msg_hclass_t sip_event_class[] =
SIP_HEADER_CLASS(event, "Event", "o", o_params, single, event);

issize_t sip_event_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_event_t *o = h->sh_event;
  size_t n;

  n = span_token(s); if (n == 0) return -1;
  o->o_type = s; s += n;
  while (IS_LWS(*s)) { *s++ = '\0'; }
  if (*s == ';') {
    if (msg_params_d(home, &s, &o->o_params) < 0 || *s)
      return -1;
    msg_header_update_params(o->o_common, 0);
  }
  return 0;
}

issize_t sip_event_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  char *end = b + bsiz, *b0 = b;
  sip_event_t const *o = h->sh_event;

  assert(sip_is_event(h));
  MSG_STRING_E(b, end, o->o_type);
  MSG_PARAMS_E(b, end, o->o_params, flags);

  return b - b0;
}

isize_t sip_event_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_event_t const *o = h->sh_event;

  MSG_PARAMS_SIZE(offset, o->o_params);
  offset += MSG_STRING_SIZE(o->o_type);

  return offset;
}

/** Duplicate one #sip_event_t object */
char *sip_event_dup_one(sip_header_t *dst, sip_header_t const *src,
			char *b, isize_t xtra)
{
  sip_event_t *o_dst = dst->sh_event;
  sip_event_t const *o_src = src->sh_event;

  char *end = b + xtra;
  b = msg_params_dup(&o_dst->o_params, o_src->o_params, b, xtra);
  MSG_STRING_DUP(b, o_dst->o_type, o_src->o_type);
  assert(b <= end); (void)end;

  return b;
}

/** Update parameters in @Event header. */
static int sip_event_update(msg_common_t *h,
			   char const *name, isize_t namelen,
			   char const *value)
{
  sip_event_t *o = (sip_event_t *)h;

  if (name == NULL) {
    o->o_id = NULL;
  }
  else if (namelen == strlen("id") && su_casenmatch(name, "id", namelen)) {
    o->o_id = value;
  }

  return 0;
}

/* ====================================================================== */

/**@SIP_HEADER sip_allow_events Allow-Events Header
 *
 * The Allow-Events header is used to indicate which events or classes of
 * events the notifier supports. Its syntax is defined in @RFC3265 as
 * follows:
 *
 * @code
 *    Allow-Events = ( "Allow-Events" / "u" ) HCOLON event-type
 *                                           *(COMMA event-type)
 * @endcode
 *
 * The parsed Allow-Events header is stored in #sip_allow_events_t structure.
 *
 * Note that the event name is case-sensitive. The event "Presence" is
 * different from "presence". However, it is very unwise to use such event
 * names.
 *
 * @sa @Event, @RFC3265, msg_header_find_item(), msg_header_replace_item(),
 * msg_header_remove_item()
 */

/**@ingroup sip_allow_events
 * @typedef struct msg_list_s sip_allow_events_t;
 *
 * The structure #sip_allow_events_t contains representation of an
 * @AllowEvents header.
 *
 * The #sip_allow_events_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 * } sip_allow_events_t;
 * @endcode
 */

msg_hclass_t sip_allow_events_class[] =
SIP_HEADER_CLASS_LIST(allow_events, "Allow-Events", "u", list);

issize_t sip_allow_events_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_list_d(home, h, s, slen);
}

issize_t sip_allow_events_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_allow_events(h));
  return msg_list_e(b, bsiz, h, f);
}

/** Append an event to a @AllowEvents header.
 *
 * @note This function @b does @b duplicate @p event.
 *
 * @deprecated Use msg_header_replace_item() directly.
 */
int sip_allow_events_add(su_home_t *home,
			 sip_allow_events_t *ae,
			 char const *event)
{
  event = su_strdup(home, event);
  if (!event)
    return -1;
  return msg_header_replace_item(home, ae->k_common, event);
}

/* ====================================================================== */

/**@SIP_HEADER sip_subscription_state Subscription-State Header
 *
 * The Subscription-State header is used to indicate in which state a
 * subscription is. Its syntax is defined in @RFC3265 section 4.2.4 as
 * follows:
 *
 * @code
 *    Subscription-State   = "Subscription-State" HCOLON substate-value
 *                           *( SEMI subexp-params )
 *    substate-value       = "active" / "pending" / "terminated"
 *                           / extension-substate
 *    extension-substate   = token
 *    subexp-params        =   ("reason" EQUAL event-reason-value)
 *                           / ("expires" EQUAL delta-seconds)
 *                           / ("retry-after" EQUAL delta-seconds)
 *                           / generic-param
 *    event-reason-value   =   "deactivated"
 *                           / "probation"
 *                           / "rejected"
 *                           / "timeout"
 *                           / "giveup"
 *                           / "noresource"
 *                           / event-reason-extension
 *    event-reason-extension = token
 * @endcode
 *
 * The parsed Subscription-State header
 * is stored in #sip_subscription_state_t structure.
 */

/**@ingroup sip_subscription_state
 * @typedef struct sip_subscription_state_s sip_subscription_state_t;
 *
 * The structure #sip_subscription_state_t contains representation of an
 * @SubscriptionState header.
 *
 * The #sip_subscription_state_t is defined as follows:
 * @code
 * typedef struct sip_subscription_state_s
 * {
 *   sip_common_t       ss_common[1];
 *   sip_unknown_t     *ss_next;
 *   // Subscription state: "pending", "active" or "terminated"
 *   char const        *ss_substate;
 *   msg_param_t const *ss_params;      // List of parameters
 *   char const        *ss_reason;      // Reason of terminating
 *   char const        *ss_expires;     // Subscription lifetime in seconds
 *   char const        *ss_retry_after; // Value of retry-after parameter
 * } sip_subscription_state_t;
 * @endcode
 */

static msg_xtra_f sip_subscription_state_dup_xtra;
static msg_dup_f sip_subscription_state_dup_one;
static msg_update_f sip_subscription_state_update;

msg_hclass_t sip_subscription_state_class[] =
SIP_HEADER_CLASS(subscription_state, "Subscription-State", "",
		 ss_params, single,
		 subscription_state);

issize_t sip_subscription_state_d(su_home_t *home, sip_header_t *h,
				  char *s, isize_t slen)
{
   sip_subscription_state_t *ss = h->sh_subscription_state;
   ss->ss_substate = s;

   s += span_token(s); /* forwards the pointer to the end of substate-value */
   if (s == ss->ss_substate)
     return -1;
   if (IS_LWS(*s)) {
     *s = '\0'; s += span_lws(s + 1) + 1;
   }

   /* check if parameters are present and if so parse them */
   if (*s  == ';') {
     if ( msg_params_d(home, &s, &ss->ss_params) < 0)
       return -1;
     if (msg_header_update_params(ss->ss_common, 0) < 0)
       return -1;
   }

   return 0;
}

issize_t sip_subscription_state_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *end = b + bsiz, *b0 = b;
  sip_subscription_state_t const *ss = h->sh_subscription_state;

  assert(sip_is_subscription_state(h));

  MSG_STRING_E(b, end, ss->ss_substate);
  MSG_PARAMS_E(b, end, ss->ss_params, flags);

  return b - b0;
}

isize_t sip_subscription_state_dup_xtra(sip_header_t const *h, isize_t offset)
{
   sip_subscription_state_t const *ss = h->sh_subscription_state;

   /* Calculates memory size occupied */
   MSG_PARAMS_SIZE(offset, ss->ss_params);
   offset += MSG_STRING_SIZE(ss->ss_substate);

   return offset;
}

/** Duplicate one #sip_subscription_state_t object */
char *sip_subscription_state_dup_one(sip_header_t *dst, sip_header_t const *src,
				     char *b, isize_t xtra)
{
  sip_subscription_state_t *ss_dst = dst->sh_subscription_state;
  sip_subscription_state_t const *ss_src = src->sh_subscription_state;
  char *end = b + xtra;

  b = msg_params_dup(&ss_dst->ss_params, ss_src->ss_params, b, xtra);
  MSG_STRING_DUP(b, ss_dst->ss_substate, ss_src->ss_substate);
  assert(b <= end); (void)end;

  return b;
}

static int sip_subscription_state_update(msg_common_t *h,
					 char const *name, isize_t namelen,
					 char const *value)
{
  sip_subscription_state_t *ss = (sip_subscription_state_t *)h;

  if (name == NULL) {
    ss->ss_reason = NULL;
    ss->ss_retry_after = NULL;
    ss->ss_expires = NULL;
  }
#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

  else if (MATCH(reason)) {
    ss->ss_reason = value;
  }
  else if (MATCH(retry-after)) {
    ss->ss_retry_after = value;
  }
  else if (MATCH(expires)) {
    ss->ss_expires = value;
  }

#undef MATCH

  return 0;
}

#if 0				/* More dead headers */

/* ====================================================================== */

/**@SIP_HEADER sip_publication Publication Header
 *
 * The Publication header is used to indicate the which publication or class
 * of publications the message contains. Its syntax is defined
 * in (draft-niemi-simple-publish-00.txt) as follows:
 *
 * @code
 *   Publication          =  ( "Publication") HCOLON publish-package
 *                         *( SEMI publish-param )
 *   publish-package      =  token-nodot
 *   token-nodot          =  1*( alphanum / "-"  / "!" / "%" / "*"
 *                               / "_" / "+" / "`" / "'" / "~" )
 *   publish-param        = generic-param / pstream / ptype
 *   pstream              = "stream" EQUAL token
 *   ptype                = "type" EQUAL token
 * @endcode
 *
 *
 * The parsed Publication header is stored in #sip_publication_t structure.
 */

/**@ingroup sip_publication
 * @brief Structure for Publication header.
 */
struct sip_publication_s
{
  sip_common_t        pub_common;	    /**< Common fragment info */
  sip_error_t        *pub_next;	            /**< Link to next (dummy) */
  char const *        pub_package;          /**< Publication packaage */
  msg_param_t const  *pub_params;	    /**< List of parameters */
  msg_param_t         pub_type; 	    /**< Publication type */
  msg_param_t         pub_stream;	    /**< Publication stream */
};

static msg_xtra_f sip_publication_dup_xtra;
static msg_dup_f sip_publication_dup_one;

msg_hclass_t sip_publication_class[] =
SIP_HEADER_CLASS(publication, "Publication", "", pub_params, single,
		 publication);

su_inline void sip_publication_update(sip_publication_t *pub);

issize_t sip_publication_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_publication_t *pub = h->sh_publication;
  size_t n;

  n = span_token(s); if (n == 0) return -1;
  pub->pub_package = s; s += n;
  while (IS_LWS(*s)) { *s++ = '\0'; }
  if (*s == ';') {
    if (msg_params_d(home, &s, &pub->pub_params) < 0 || *s)
      return -1;
    sip_publication_update(pub);
  }
  return 0;
}

issize_t sip_publication_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  char *end = b + bsiz, *b0 = b;
  sip_publication_t const *pub = h->sh_publication;

  assert(sip_is_publication(h));
  MSG_STRING_E(b, end, pub->pub_package);
  MSG_PARAMS_E(b, end, pub->pub_params, flags);

  return b - b0;
}

isize_t sip_publication_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_publication_t const *pub = h->sh_publication;

  MSG_PARAMS_SIZE(offset, pub->pub_params);
  offset += MSG_STRING_SIZE(pub->pub_package);

  return offset;
}

/** Duplicate one #sip_publication_t object */
char *sip_publication_dup_one(sip_header_t *dst, sip_header_t const *src,
			char *b, isize_t xtra)
{
  sip_publication_t *pub_dst = dst->sh_publication;
  sip_publication_t const *pub_src = src->sh_publication;

  char *end = b + xtra;
  b = msg_params_dup(&pub_dst->pub_params, pub_src->pub_params, b, xtra);
  MSG_STRING_DUP(b, pub_dst->pub_package, pub_src->pub_package);
  if (pub_dst->pub_params)
    sip_publication_update(pub_dst);
  assert(b <= end);

  return b;
}

su_inline void sip_publication_update(sip_publication_t *pub)
{
  size_t i;

  if (pub->pub_params)
    for (i = 0; pub->pub_params[i]; i++) {
      if (su_casenmatch(pub->pub_params[i], "stream=", strlen("stream=")))
	pub->pub_stream = pub->pub_params[i] + strlen("stream=");
      else if (su_casenmatch(pub->pub_params[i], "type=", strlen("type=")))
	pub->pub_type = pub->pub_params[i] + strlen("type=");
    }
}

/* ====================================================================== */

/**@SIP_HEADER sip_allow_publications Allow-Publication Header
 *
 * The Allow-Publication header is used to indicate which publications or classes of
 * publications the server supports.  Its syntax is defined in [niemi]
 * (draft-niemi-simple-publish-00.txt) as follows:
 *
 * @code
 *   Allow-Publications   = "Allow-Publications" HCOLON publish-type
 *                          * ( COMMA publish-type )
 * @endcode
 *
 *
 * The parsed Allow-Publication Header
 * is stored in #sip_allow_publications_t structure.
 */

msg_hclass_t sip_allow_publications_class[] =
SIP_HEADER_CLASS_LIST(allow_publications, "Allow-Publications", "", list);

issize_t sip_allow_publications_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_list_d(home, h, s, slen);
}

issize_t sip_allow_publications_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_allow_publications(h));
  return msg_list_e(b, bsiz, h, f);
}

/** Append an publication to a Allow-Publications header. */
int sip_allow_publications_add(su_home_t *home,
			       sip_allow_publications_t *ae,
			       char const *e)
{
  e = su_strdup(home, e);
  if (!e)
    return -1;
  return msg_params_replace(home, (msg_param_t **)&ae->k_items, e);
}

#endif
