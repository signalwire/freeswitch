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

#ifndef NEA_TAG_H
/** Defined when <sofia-sip/nea_tag.h> has been included. */
#define NEA_TAG_H

/**@file sofia-sip/nea_tag.h
 * @brief Tags for Nokia User Agent Library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Mon Nov 28 18:54:26 EET 2005 mela
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef URL_TAG_H
#include <sofia-sip/url_tag.h>
#endif
#ifndef SIP_TAG_H
#include <sofia-sip/sip_tag.h>
#endif
#ifndef NTA_TAG_H
#include <sofia-sip/nta_tag.h>
#endif

SOFIA_BEGIN_DECLS

/** Event states */
typedef enum {
  nea_extended = -1,
  nea_embryonic = 0,		/** Before first notify */
  nea_pending,
  nea_active,
  nea_terminated
} nea_state_t;

/** Filter tag matching any nea tag. */
#define NEATAG_ANY()         neatag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t neatag_any;

/** Specify the minimum duration of a subscription (by default, 15 minutes) */
#define NEATAG_MIN_EXPIRES(x) neatag_min_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_min_expires;

#define NEATAG_MIN_EXPIRES_REF(x) neatag_min_expires_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_min_expires_ref;

#define NEATAG_MINSUB(x) neatag_min_expires, tag_uint_v((x))
#define NEATAG_MINSUB_REF(x) neatag_min_expires_ref, tag_uint_vr((&x))

/** Specify the default duration of a subscription (by default, 60 minutes) */
#define NEATAG_EXPIRES(x) neatag_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_expires;

#define NEATAG_EXPIRES_REF(x) neatag_expires_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_expires_ref;

/** Specify the maximum duration of a subscription (by default, 24 hours) */
#define NEATAG_MAX_EXPIRES(x) neatag_max_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_max_expires;

#define NEATAG_MAX_EXPIRES_REF(x) neatag_max_expires_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_max_expires_ref;

/** Indicate/require support for "eventlist" feature. */
#define NEATAG_EVENTLIST(x)  neatag_eventlist, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t neatag_eventlist;

#define NEATAG_EVENTLIST_REF(x) neatag_eventlist_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_eventlist_ref;

/** Specify the default throttle value for subscription. */
#define NEATAG_THROTTLE(x) neatag_throttle, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_throttle;

#define NEATAG_THROTTLE_REF(x) neatag_throttle_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_throttle_ref;

/** Specify the minimum throttle value for subscription. */
#define NEATAG_MINTHROTTLE(x) neatag_minthrottle, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_minthrottle;

#define NEATAG_MINTHROTTLE_REF(x) neatag_minthrottle_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_minthrottle_ref;

/** Specify dialog handle */
#define NEATAG_DIALOG(x)     neatag_dialog, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t neatag_dialog;

#define NEATAG_DIALOG_REF(x) neatag_dialog_ref, tag_ptr_vr((&x), (x))
SOFIAPUBVAR tag_typedef_t neatag_dialog_ref;

/* Server-specific tags */

/** Pass pointer to subscription */
#define NEATAG_SUB(x)        neatag_sub, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t neatag_sub;

#define NEATAG_SUB_REF(x)    neatag_sub_ref, tag_ptr_vr((&x), (x))
SOFIAPUBVAR tag_typedef_t neatag_sub_ref;

/** Use fake content. @sa nea_sub_auth() and nea_server_update(). */
#define NEATAG_FAKE(x)    neatag_fake, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t neatag_fake;

#define NEATAG_FAKE_REF(x) neatag_fake_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_fake_ref;

/** Specify reason for termination */
#define NEATAG_REASON(x)     neatag_reason, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t neatag_reason;

#define NEATAG_REASON_REF(x) neatag_reason_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_reason_ref;

/** Specify retry-after for termination */
#define NEATAG_RETRY_AFTER(x)    neatag_retry_after, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_retry_after;

#define NEATAG_RETRY_AFTER_REF(x) neatag_retry_after_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_retry_after_ref;

/** Specify extended state for subscription-state */
#define NEATAG_EXSTATE(x)    neatag_exstate, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t neatag_exstate;

#define NEATAG_EXSTATE_REF(x) neatag_exstate_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_exstate_ref;

/** Do not try to conform pre-3265 notifiers/watchers */
#define NEATAG_STRICT_3265(x)    neatag_strict_3265, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t neatag_strict_3265;

#define NEATAG_STRICT_3265_REF(x) neatag_strict_3265_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_strict_3265_ref;

/** Version number of content */
#define NEATAG_VERSION(x) neatag_version, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t neatag_version;

#define NEATAG_VERSION_REF(x) neatag_version_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_version_ref;

/** List of all NEA tags. */
/* extern tag_type_t nea_tag_list[]; */

SOFIA_END_DECLS

#endif
