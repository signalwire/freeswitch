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

/**@CFILE sip_feature.c
 *
 * @brief Feature-related SIP header handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
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
#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_allow Allow Header
 *
 * The Allow header lists the set of methods supported by the user agent
 * generating the message.  Its syntax is defined in @RFC3261 as
 * follows:
 *
 * @code
 *    Allow  =  "Allow" HCOLON [Method *(COMMA Method)]
 * @endcode
 *
 * The parsed Allow header is stored in #sip_allow_t structure.
 *
 * Note that SIP methods are case-sensitive: "INVITE" method is different from
 * "Invite".
 *
 * @sa msg_header_find_item(), msg_header_replace_item(),
 * msg_header_remove_item()
 */

/**@ingroup sip_allow
 * @typedef struct msg_list_s sip_allow_t;
 *
 * The structure #sip_allow_t contains representation of an @Allow header.
 *
 * The #sip_allow_t is defined as follows:
 * @code
 * typedef struct msg_allow_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 *   uint32_t           k_bitmap;     // Bitmap of allowed methods
 * } sip_allow_t;
 * @endcode
 *
 * @note The field @a k_bitmap was added in @VERSION_1_12_5.
 */

#define sip_allow_dup_xtra msg_list_dup_xtra
#define sip_allow_dup_one  msg_list_dup_one
static msg_update_f sip_allow_update;

msg_hclass_t sip_allow_class[] =
SIP_HEADER_CLASS(allow, "Allow", "", k_items, list, allow);

issize_t sip_allow_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_allow_t *k = (sip_allow_t *)h;
  issize_t retval = msg_commalist_d(home, &s, &k->k_items, msg_token_scan);
  msg_header_update_params(k->k_common, 0);
  return retval;
}

issize_t sip_allow_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_allow(h));
  return msg_list_e(b, bsiz, h, f);
}

static int sip_allow_update(msg_common_t *h,
			  char const *name, isize_t namelen,
			  char const *value)
{
  sip_allow_t *k = (sip_allow_t *)h;

  if (name == NULL) {
    k->k_bitmap = 0;
  }
  else {
    sip_method_t method = sip_method_code(name);

    if (method >= 0 && method < 32)
      k->k_bitmap |= 1 << method;
  }

  return 0;
}

/** Return true if the method is listed in @Allow header. */
int sip_is_allowed(sip_allow_t const *allow,
		   sip_method_t method,
		   char const *name)
{
  if (method < sip_method_unknown || !allow)
    return 0;

  if (sip_method_unknown < method && method < 32)
    /* Well-known method */
    return (allow->k_bitmap & (1 << method)) != 0;

  if (method == sip_method_unknown &&
      (allow->k_bitmap & (1 << sip_method_unknown)) == 0)
    return 0;

  return msg_header_find_item(allow->k_common, name) != NULL;
}


/* ====================================================================== */

/**@SIP_HEADER sip_proxy_require Proxy-Require Header
 *
 * The Proxy-Require header is used to indicate proxy-sensitive features
 * that @b MUST be supported by the proxy.  Its syntax is defined in @RFC3261
 * as follows:
 *
 * @code
 *    Proxy-Require  =  "Proxy-Require" HCOLON option-tag *(COMMA option-tag)
 * @endcode
 *
 *
 * The parsed Proxy-Require header is stored in #sip_proxy_require_t structure.
 */

/**@ingroup sip_proxy_require
 * @typedef struct msg_list_s sip_proxy_require_t;
 *
 * The structure #sip_proxy_require_t contains representation of an
 * @ProxyRequire header.
 *
 * The #sip_proxy_require_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Dummy link
 *   msg_param_t       *k_items;      // List of items
 * } sip_proxy_require_t;
 * @endcode
 */

msg_hclass_t sip_proxy_require_class[] =
SIP_HEADER_CLASS_LIST(proxy_require, "Proxy-Require", "", list);

issize_t sip_proxy_require_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_proxy_require_t *k = (sip_proxy_require_t *)h;
  return msg_commalist_d(home, &s, &k->k_items, msg_token_scan);
}

issize_t sip_proxy_require_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_proxy_require(h));
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_require Require Header
 *
 * The Require header is used by clients to tell user agent servers about
 * options that the client expects the server to support in order to
 * properly process the request.  Its syntax is defined in @RFC3261
 * as follows:
 *
 * @code
 *    Require       =  "Require" HCOLON option-tag *(COMMA option-tag)
 * @endcode
 *
 * The parsed Require header is stored in #sip_require_t structure.
 */

/**@ingroup sip_require
 * @typedef struct msg_list_s sip_require_t;
 *
 * The structure #sip_require_t contains representation of an
 * @Require header.
 *
 * The #sip_require_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 * } sip_require_t;
 * @endcode
 */

msg_hclass_t sip_require_class[] =
SIP_HEADER_CLASS_LIST(require, "Require", "", list);

issize_t sip_require_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_require_t *k = (sip_require_t *)h;
  return msg_commalist_d(home, &s, &k->k_items, msg_token_scan);
}

issize_t sip_require_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_require(h));
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_supported Supported Header
 *
 * The Supported header enumerates all the capabilities of the client or
 * server.  Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Supported  =  ( "Supported" / "k" ) HCOLON
 *                  [option-tag *(COMMA option-tag)]
 * @endcode
 *
 * The parsed option-tags of Supported header
 * are stored in #sip_supported_t structure.
 */

/**@ingroup sip_supported
 * @typedef struct msg_list_s sip_supported_t;
 *
 * The structure #sip_supported_t contains representation of an
 * @Supported header.
 *
 * The #sip_supported_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 * } sip_supported_t;
 * @endcode
 */


msg_hclass_t sip_supported_class[] =
SIP_HEADER_CLASS_LIST(supported, "Supported", "k", list);

issize_t sip_supported_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_supported_t *k = (sip_supported_t *)h;
  return msg_commalist_d(home, &s, &k->k_items, msg_token_scan);
}

issize_t sip_supported_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_supported(h));
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_unsupported Unsupported Header
 *
 * The Unsupported header lists the features not supported by the server.
 * Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Unsupported  =  "Unsupported" HCOLON [option-tag *(COMMA option-tag)]
 * @endcode
 *
 *
 * The parsed Unsupported header is stored in #sip_unsupported_t structure.
 */

/**@ingroup sip_unsupported
 * @typedef struct msg_list_s sip_unsupported_t;
 *
 * The structure #sip_unsupported_t contains representation of an
 * @Unsupported header.
 *
 * The #sip_unsupported_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 * } sip_unsupported_t;
 * @endcode
 */

msg_hclass_t sip_unsupported_class[] =
SIP_HEADER_CLASS_LIST(unsupported, "Unsupported", "", list);

issize_t sip_unsupported_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_unsupported_t *k = (sip_unsupported_t *)h;
  return msg_commalist_d(home, &s, &k->k_items, msg_token_scan);
}

issize_t sip_unsupported_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_unsupported(h));
  return msg_list_e(b, bsiz, h, f);
}

/** Check if required feature is not supported.
 *
 * @retval NULL if all the required features are supported
 * @retval pointer to a @Unsupported header or
 *         #SIP_NONE if @a home is NULL
 */
sip_unsupported_t *sip_has_unsupported(su_home_t *home,
				       sip_supported_t const *support,
				       sip_require_t const *require)
{
  return sip_has_unsupported_any(home, support, NULL, NULL,
				 require, NULL, NULL);
}


/** Check if required feature is not supported.
 *
 * @retval NULL if all the required features are supported
 * @retval pointer to a @Unsupported header or
 *         #SIP_NONE if @a home is NULL
 */
sip_unsupported_t *
sip_has_unsupported2(su_home_t *home,
		     sip_supported_t const *support,
		     sip_require_t const *support_by_require,
		     sip_require_t const *require)
{
  return
    sip_has_unsupported_any(home,
			    support, support_by_require, NULL,
			    require, NULL, NULL);
}


/** Check if required features are not supported.
 *
 * The supported features can be listed in @Supported, @Require or
 * @ProxyRequire headers (in @a supported, @a by_require, or @a
 * by_proxy_require parameters, respectively)
 *
 * @param home (optional) home pointer for allocating @Unsupported header
 * @param supported @Supported features (may be NULL) [IN]
 * @param by_require  supported features listed by
 *                    @Require (may be NULL) [IN]
 * @param by_proxy_require supported features listed
 *                         by @ProxyRequire (may be NULL) [IN]
 *
 * @param require   list of required features (may be NULL) [IN]
 * @param require2  2nd list of required features (may be NULL) [IN]
 * @param require3  3rd list of required features (may be NULL) [IN]
 *
 * @retval NULL if all the required features are supported
 * @retval pointer to a @Unsupported header or
 *         #SIP_NONE if @a home is NULL
 */
sip_unsupported_t *
sip_has_unsupported_any(su_home_t *home,
			sip_supported_t const *supported,
			sip_require_t const *by_require,
			sip_proxy_require_t const *by_proxy_require,
			sip_require_t const *require,
			sip_require_t const *require2,
			sip_require_t const *require3)
{
  size_t i, j;
  sip_unsupported_t *unsupported = NULL;
  msg_param_t const empty[1] = { NULL };
  msg_param_t const *slist = empty;
  msg_param_t const *rlist = empty;
  msg_param_t const *prlist = empty;

  if (require2 == NULL)
    require2 = require3, require3 = NULL;
  if (require == NULL)
    require = require2, require2 = NULL;

  if (require && require->k_items) {
    if (supported && supported->k_items)
      slist = supported->k_items;
    if (by_require && by_require->k_items)
      rlist = by_require->k_items;
    if (by_proxy_require && by_proxy_require->k_items)
      prlist = by_proxy_require->k_items;

    for (i = 0; require->k_items && require->k_items[i];) {
      msg_param_t feature = require->k_items[i++];

      for (j = 0; slist[j]; j++)
	if (su_casematch(feature, slist[j])) {
	  feature = NULL;
	  break;
	}

      if (feature)
	for (j = 0; rlist[j]; j++)
	  if (su_casematch(feature, rlist[j])) {
	    feature = NULL;
	    break;
	  }

      if (feature)
	for (j = 0; prlist[j]; j++)
	  if (su_casematch(feature, prlist[j])) {
	    feature = NULL;
	    break;
	  }

      if (feature) {
	if (home) {
	  if (unsupported == NULL)
	    unsupported = sip_unsupported_make(home, feature);
	  else
	    msg_params_add(home,
			   (msg_param_t **)&unsupported->k_items,
			   feature);
	}
	else {
	  return (sip_unsupported_t *)SIP_NONE;
	}
      }

      if (require->k_items[i] == NULL && require2 && require2->k_items) {
	i = 0, require = require2, require2 = require3, require3 = NULL;
      }
    }
  }

  return unsupported;
}


int sip_has_feature(msg_list_t const *supported, char const *feature)
{
  size_t i;

  if (!feature || !feature[0])
    return 1;			/* Empty feature is always supported */

  for (; supported; supported = supported->k_next)
    if (supported->k_items)
      for (i = 0; supported->k_items[i]; i++)
	if (su_casematch(feature, supported->k_items[i]))
	  return 1;

  return 0;
}

/** Check that a feature is supported. */
int sip_has_supported(sip_supported_t const *supported, char const *feature)
{
  return sip_has_feature(supported, feature);
}

/* ====================================================================== */

/**@SIP_HEADER sip_path Path Header
 *
 * The Path header field is a SIP extension header field (@RFC3327) with
 * syntax very similar to the @RecordRoute header field. It is used in
 * conjunction with SIP REGISTER requests and with 200 class messages in
 * response to REGISTER (REGISTER responses).
 *
 * @code
 *    Path        =  "Path" HCOLON path-value *(COMMA path-value)
 *    path-value  =  name-addr *( SEMI rr-param )
 * @endcode
 *
 *
 * The parsed Path header is stored in #sip_path_t structure.
 */

/**@ingroup sip_path
 * @typedef typedef struct sip_route_s sip_path_t;
 *
 * The structure #sip_path_t contains representation of SIP @Path header.
 *
 * The #sip_path_t is defined as follows:
 * @code
 * typedef struct sip_route_s {
 *   sip_common_t        r_common[1];   // Common fragment info
 *   sip_path_t         *r_next;        // Link to next @Path
 *   char const         *r_display;     // Display name
 *   url_t               r_url[1];      // @Path URL
 *   msg_param_t const  *r_params;      // List of parameters
 * } sip_path_t;
 * @endcode
 */

msg_hclass_t sip_path_class[] =
SIP_HEADER_CLASS(path, "Path", "", r_params, prepend, any_route);

issize_t sip_path_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_any_route_d(home, h, s, slen);
}

issize_t sip_path_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_path(h));
  return sip_any_route_e(b, bsiz, h, flags);
}

/* ====================================================================== */

/**@SIP_HEADER sip_service_route Service-Route Header
 *
 * The "Service-Route" is a SIP extension header field (@RFC3608), which can
 * contain a route vector that will direct requests through a specific
 * sequence of proxies. A registrar may use a Service-Route header field to
 * inform a UA of a service route that, if used by the UA, will provide
 * services from a proxy or set of proxies associated with that registrar.
 * The Service-Route header field may be included by a registrar in the
 * response to a REGISTER request. The syntax for the Service-Route header
 * field is:
 *
 * @code
 *    Service-Route = "Service-Route" HCOLON sr-value *(COMMA sr-value)
 *    sr-value  =  name-addr *( SEMI rr-param )
 * @endcode
 *
 * The parsed Service-Route header is stored in #sip_service_route_t structure.
 *
 * @sa @RFC3608, @Path, @Route, @RecordRoute
 */

/**@ingroup sip_service_route
 * @typedef typedef struct sip_route_s sip_service_route_t;
 *
 * The structure #sip_service_route_t contains representation of SIP
 * @ServiceRoute header.
 *
 * The #sip_service_route_t is defined as follows:
 * @code
 * typedef struct sip_route_s {
 *   sip_common_t        r_common[1];   // Common fragment info
 *   sip_service_route_t*r_next;        // Link to next @ServiceRoute
 *   char const         *r_display;     // Display name
 *   url_t               r_url[1];      // Service-Route URL
 *   msg_param_t const  *r_params;      // List of parameters
 * } sip_service_route_t;
 * @endcode
 */

msg_hclass_t sip_service_route_class[] =
SIP_HEADER_CLASS(service_route, "Service-Route", "",
		 r_params, append, any_route);

issize_t sip_service_route_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_any_route_d(home, h, s, slen);
}

issize_t sip_service_route_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_service_route(h));
  return sip_any_route_e(b, bsiz, h, flags);
}
