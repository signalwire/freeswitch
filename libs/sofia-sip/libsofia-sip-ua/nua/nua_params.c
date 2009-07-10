/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

/**@CFILE nua_register.c
 * @brief REGISTER and registrations
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 11:48:49 EET 2006 ppessi
 */

#include "config.h"

#include <sofia-sip/su_string.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/token64.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_tag_inline.h>

#include <sofia-sip/bnf.h>

#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/msg_parser.h>

#include "nua_stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

/* ====================================================================== */
/* Helper macros and functions for handling #nua_handle_preferences_t. */

#define NHP_IS_ANY_SET(nhp) nhp_is_any_set((nhp))

/** Check if any preference is set in @a nhp. */
su_inline int nhp_is_any_set(nua_handle_preferences_t const *nhp)
{
  char nhp_zero[sizeof nhp->nhp_set] = { 0 };
  return memcmp(&nhp->nhp_set, nhp_zero, sizeof nhp->nhp_set) != 0;
}

/** Copy set parameters from @a b to @a a.
 *
 * If preference is set in @a b, mark it set also in @a a.
 */
su_inline void nhp_or_set(nua_handle_preferences_t *a,
			  nua_handle_preferences_t const *b)
{
  memcpy(a, b, offsetof(nua_handle_preferences_t, nhp_set));

  /* Bitwise or of bitfields, casted to unsigned */
  a->nhp_set_.set_unsigned[0] |= b->nhp_set_.set_unsigned[0];
  a->nhp_set_.set_unsigned[1] |= b->nhp_set_.set_unsigned[1];
}

static int nhp_set_tags(su_home_t *home,
			nua_handle_preferences_t *nhp,
			nua_global_preferences_t *ngp,
			tagi_t const *tags);

static int nhp_merge_lists(su_home_t *home,
			   msg_hclass_t *hc,
			   msg_list_t **return_new_list,
			   msg_list_t const *old_list,
			   int already_set,
			   int already_parsed,
			   int always_merge,
			   tag_value_t value);

static int nhp_save_params(nua_handle_t *nh,
			   su_home_t *tmphome,
			   nua_global_preferences_t *gsrc,
			   nua_handle_preferences_t *src);

/* ====================================================================== */
/* Magical NUTAG_USER_AGENT() - add NHP_USER_AGENT there if it is not there */

#define NHP_USER_AGENT PACKAGE_NAME "/" PACKAGE_VERSION

static int already_contains_package_name(char const *s)
{
  char const pn[] = " " PACKAGE_NAME "/";
  size_t pnlen = strlen(pn + 1);

  return su_casenmatch(s, pn + 1, pnlen) || su_strcasestr(s, pn);
}

/* ====================================================================== */
/* Stack and handle parameters */

static int nua_stack_set_smime_params(nua_t *nua, tagi_t const *tags);

/** @internal Methods allowed by default. */
static char const nua_allow_str[] =
"INVITE, ACK, BYE, CANCEL, OPTIONS, PRACK, "
"MESSAGE, SUBSCRIBE, NOTIFY, REFER, UPDATE";

/** @internal Set default parameters */
int nua_stack_set_defaults(nua_handle_t *nh,
			   nua_handle_preferences_t *nhp)
{
  su_home_t *home = (su_home_t *)nh;

  /* Set some defaults */
  NHP_SET(nhp, retry_count, 3);
  NHP_SET(nhp, max_subscriptions, 20);

  NHP_SET(nhp, media_enable, 1);
  NHP_SET(nhp, invite_enable, 1);
  NHP_SET(nhp, auto_alert, 0);
  NHP_SET(nhp, early_media, 0);
  NHP_SET(nhp, only183_100rel, 0);
  NHP_SET(nhp, auto_answer, 0);
  NHP_SET(nhp, auto_ack, 1);
  NHP_SET(nhp, invite_timeout, 120);

  nhp->nhp_session_timer = 1800;
  nhp->nhp_refresher = nua_no_refresher;

  NHP_SET(nhp, min_se, 120);
  NHP_SET(nhp, update_refresh, 0);

  NHP_SET(nhp, message_enable, 1);
  NHP_SET(nhp, win_messenger_enable, 0);
  if (getenv("PIMIW_HACK") != 0)
    NHP_SET(nhp, message_auto_respond, 1);

  NHP_SET(nhp, media_features,  0);
  NHP_SET(nhp, callee_caps, 0);
  NHP_SET(nhp, service_route_enable, 1);
  NHP_SET(nhp, path_enable, 1);

  NHP_SET(nhp, refer_expires, 300);
  NHP_SET(nhp, refer_with_id, 1);

  NHP_SET(nhp, substate, nua_substate_active);
  NHP_SET(nhp, sub_expires, 3600);

  NHP_SET(nhp, allow, sip_allow_make(home, nua_allow_str));
  NHP_SET(nhp, supported, sip_supported_make(home, "timer, 100rel"));
  NHP_SET(nhp, user_agent, su_strdup(home, NHP_USER_AGENT));

  NHP_SET(nhp, outbound, su_strdup(home, "natify"));

  NHP_SET(nhp, keepalive, 120000);

  NHP_SET(nhp, appl_method,
	  sip_allow_make(home, "INVITE, REGISTER, PUBLISH, SUBSCRIBE"));

  if (!nhp->nhp_allow ||
      !nhp->nhp_supported ||
      !nhp->nhp_user_agent ||
      !nhp->nhp_outbound)
    return -1;

  return 0;
}

/** @internal Set the default from field */
int nua_stack_set_from(nua_t *nua, int initial, tagi_t const *tags)
{
  sip_from_t const *from = NONE;
  char const *str = NONE;
  sip_from_t *f = NULL,  f0[1];
  int set;

  tl_gets(tags,
	  /* By nua_stack_set_from() */
	  SIPTAG_FROM_REF(from),
	  SIPTAG_FROM_STR_REF(str),
	  TAG_END());

  if (!initial && from == NONE && str == NONE)
    return 0;

  sip_from_init(f0);

  if (from && from != NONE) {
    f0->a_display = from->a_display;
    *f0->a_url = *from->a_url;
    f = sip_from_dup(nua->nua_home, f0);
    set = 1;
  }
  else if (str && str != NONE) {
    f = sip_from_make(nua->nua_home, str);
    if (f)
      *f0 = *f, f = f0, f->a_params = NULL;
    set = 1;
  }
  else {
    sip_contact_t const *m;

    m = nua_stack_get_contact(nua->nua_registrations);

    if (m) {
      f0->a_display = m->m_display;
      *f0->a_url = *m->m_url;
      f = sip_from_dup(nua->nua_home, f0);
    }
    set = 0;
  }

  if (!f)
    return -1;

  nua->nua_from_is_set = set;
  *nua->nua_from = *f;
  return 0;
}

/** @internal Initialize instance ID. */
int nua_stack_init_instance(nua_handle_t *nh, tagi_t const *tags)
{
  nua_handle_preferences_t *nhp = nh->nh_prefs;

  char const *instance = NONE;

  tl_gets(tags, NUTAG_INSTANCE_REF(instance), TAG_END());

  if (instance != NONE) {
    NHP_SET(nhp, instance, su_strdup(nh->nh_home, instance));
    if (instance && !nhp->nhp_instance)
      return -1;
  }

  return 0;
}

/**@fn void nua_set_params(nua_t *nua, tag_type_t tag, tag_value_t value, ...)
 *
 * Set @nua parameters, shared by all handles.
 *
 * @param nua             Pointer to NUA stack object
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *   NUTAG_ALLOW(), SIPTAG_ALLOW(), and SIPTAG_ALLOW_STR() \n
 *   NUTAG_ALLOW_EVENTS(), SIPTAG_ALLOW_EVENTS(), and
 *                         SIPTAG_ALLOW_EVENTS_STR() \n
 *   NUTAG_AUTOACK() \n
 *   NUTAG_AUTOALERT() \n
 *   NUTAG_AUTOANSWER() \n
 *   NUTAG_CALLEE_CAPS() \n
 *   NUTAG_DETECT_NETWORK_UPDATES() \n
 *   NUTAG_EARLY_ANSWER() \n
 *   NUTAG_EARLY_MEDIA() \n
 *   NUTAG_ENABLEINVITE() \n
 *   NUTAG_ENABLEMESSAGE() \n
 *   NUTAG_ENABLEMESSENGER() \n
 *   NUTAG_INITIAL_ROUTE() \n
 *   NUTAG_INITIAL_ROUTE_STR() \n
 *   NUTAG_INSTANCE() \n
 *   NUTAG_INVITE_TIMER() \n
 *   NUTAG_KEEPALIVE() \n
 *   NUTAG_KEEPALIVE_STREAM() \n
 *   NUTAG_MAX_SUBSCRIPTIONS() \n
 *   NUTAG_MEDIA_ENABLE() \n
 *   NUTAG_MEDIA_FEATURES() \n
 *   NUTAG_MIN_SE() \n
 *   NUTAG_M_DISPLAY() \n
 *   NUTAG_M_FEATURES() \n
 *   NUTAG_M_PARAMS() \n
 *   NUTAG_M_USERNAME() \n
 *   NUTAG_ONLY183_100REL() \n
 *   NUTAG_OUTBOUND() \n
 *   NUTAG_PATH_ENABLE() \n
 *   NUTAG_PROXY() (aka NTATAG_DEFAULT_PROXY()) \n
 *   NUTAG_REFER_EXPIRES() \n
 *   NUTAG_REFER_WITH_ID() \n
 *   NUTAG_REFRESH_WITHOUT_SDP() \n
 *   NUTAG_REGISTRAR() \n
 *   NUTAG_RETRY_COUNT() \n
 *   NUTAG_SERVICE_ROUTE_ENABLE() \n
 *   NUTAG_SESSION_REFRESHER() \n
 *   NUTAG_SESSION_TIMER() \n
 *   NUTAG_SMIME_ENABLE() \n
 *   NUTAG_SMIME_KEY_ENCRYPTION() \n
 *   NUTAG_SMIME_MESSAGE_DIGEST() \n
 *   NUTAG_SMIME_MESSAGE_ENCRYPTION() \n
 *   NUTAG_SMIME_OPT() \n
 *   NUTAG_SMIME_PROTECTION_MODE() \n
 *   NUTAG_SMIME_SIGNATURE() \n
 *   NUTAG_SOA_NAME() \n
 *   NUTAG_SUBSTATE() \n
 *   NUTAG_SUB_EXPIRES() \n
 *   NUTAG_SUPPORTED(), SIPTAG_SUPPORTED(), and SIPTAG_SUPPORTED_STR() \n
 *   NUTAG_UPDATE_REFRESH() \n
 *   NUTAG_USER_AGENT(), SIPTAG_USER_AGENT() and SIPTAG_USER_AGENT_STR() \n
 *   SIPTAG_ORGANIZATION() and SIPTAG_ORGANIZATION_STR() \n
 *
 * nua_set_params() also accepts any soa tags, defined in
 * <sofia-sip/soa_tag.h>, and nta tags, defined in <sofia-sip/nta_tag.h>.
 *
 * @par Events:
 *     #nua_r_set_params
 *
 * @par SIP Header as NUA Parameters
 * The @nua parameters include SIP headers @Allow, @Supported, @Organization,
 * @UserAgent and @From. They are included in most of the SIP messages sent
 * by @nua. They are set in the same way as the tagged arguments are
 * used to populate a SIP message.
 * @par
 * When multiple tags for the same header are specified, the behaviour
 * depends on the header type. If only a single header field can be included
 * in a SIP message, the latest non-NULL value is used, e.g., @Organization.
 * However, if the SIP header can consist of multiple lines or header fields
 * separated by comma, in this case, @Allow and @Supported, all the tagged
 * values are concatenated.
 * @par
 * However, if the tag value is #SIP_NONE (-1 casted as a void pointer), the
 * values from previous tags are ignored.
 *
 * For example, the nua_set_params() call like this:
 * @code
 * nua_set_params(nua,
 *                SIPTAG_USER_AGENT_STR("tester/1.0"),
 *                SIPTAG_ALLOW_STR("INVITE,CANCEL,BYE,ACK"),
 *                SIPTAG_ORGANIZATION(NULL),
 *                SIPTAG_USER_AGENT(NULL),
 *                SIPTAG_ALLOW(SIP_NONE),
 *                TAG_END());
 * @endcode
 * will leave @Allow and @Organization headers empty. The @UserAgent header
 * will contain value "tester/1.0".
 * @code
 * nua_set_params(nua,
 *                SIPTAG_ORGANIZATION_STR("Malevolent Microwavers"),
 *                SIPTAG_ALLOW_STR("OPTIONS"),
 *                SIPTAG_ALLOW(SIP_NONE),
 *                SIPTAG_ORGANIZATION_STR("The Phone Company"),
 *                SIPTAG_ALLOW_STR("SUBSCRIBE"),
 *                SIPTAG_ALLOW(NULL),
 *                SIPTAG_ORGANIZATION_STR(NULL),
 *                TAG_END());
 * @endcode
 * sets the header @Allow with value <code>SUBSCRIBE</code> and the
 * header @Organization will have value <code>The Phone Company</code>.
 *
 */

/**@fn void nua_set_hparams(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Set the handle-specific parameters.
 *
 * The handle-specific parameters override default or global parameters set
 * by nua_set_params(). The handle-specific parameters are set by several
 * other operations: nua_invite(), nua_respond(), nua_ack(),
 * nua_prack(), nua_update(), nua_info(), nua_bye(), nua_options(),
 * nua_message(), nua_register(), nua_publish(), nua_refer(),
 * nua_subscribe(), nua_notify(), nua_refer(), and nua_notifier().
 *
 * @param nh              Pointer to a NUA handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *     nothing
 *
 * @par Tags Used to Set Handle-Specific Parameters:
 *   NUTAG_ALLOW(), SIPTAG_ALLOW(), and SIPTAG_ALLOW_STR() \n
 *   NUTAG_ALLOW_EVENTS(), SIPTAG_ALLOW_EVENTS(), and
 *                         SIPTAG_ALLOW_EVENTS_STR() \n
 *   NUTAG_AUTH_CACHE() \n
 *   NUTAG_AUTOACK() \n
 *   NUTAG_AUTOALERT() \n
 *   NUTAG_AUTOANSWER() \n
 *   NUTAG_CALLEE_CAPS() \n
 *   NUTAG_EARLY_ANSWER() \n
 *   NUTAG_EARLY_MEDIA() \n
 *   NUTAG_ENABLEINVITE() \n
 *   NUTAG_ENABLEMESSAGE() \n
 *   NUTAG_ENABLEMESSENGER() \n
 *   NUTAG_INITIAL_ROUTE() \n
 *   NUTAG_INITIAL_ROUTE_STR() \n
 *   NUTAG_INSTANCE() \n
 *   NUTAG_INVITE_TIMER() \n
 *   NUTAG_KEEPALIVE() \n
 *   NUTAG_KEEPALIVE_STREAM() \n
 *   NUTAG_MAX_SUBSCRIPTIONS() \n
 *   NUTAG_MEDIA_ENABLE() \n
 *   NUTAG_MEDIA_FEATURES() \n
 *   NUTAG_MIN_SE() \n
 *   NUTAG_M_DISPLAY() \n
 *   NUTAG_M_FEATURES() \n
 *   NUTAG_M_PARAMS() \n
 *   NUTAG_M_USERNAME() \n
 *   NUTAG_ONLY183_100REL() \n
 *   NUTAG_OUTBOUND() \n
 *   NUTAG_PATH_ENABLE() \n
 *   NUTAG_PROXY() (aka NTATAG_DEFAULT_PROXY()) \n
 *   NUTAG_REFER_EXPIRES() \n
 *   NUTAG_REFER_WITH_ID() \n
 *   NUTAG_REFRESH_WITHOUT_SDP() \n
 *   NUTAG_REGISTRAR() \n
 *   NUTAG_RETRY_COUNT() \n
 *   NUTAG_SERVICE_ROUTE_ENABLE() \n
 *   NUTAG_SESSION_REFRESHER() \n
 *   NUTAG_SESSION_TIMER() \n
 *   NUTAG_SOA_NAME() \n
 *   NUTAG_SUBSTATE() \n
 *   NUTAG_SUB_EXPIRES() \n
 *   NUTAG_SUPPORTED(), SIPTAG_SUPPORTED(), and SIPTAG_SUPPORTED_STR() \n
 *   NUTAG_UPDATE_REFRESH() \n
 *   NUTAG_USER_AGENT(), SIPTAG_USER_AGENT() and SIPTAG_USER_AGENT_STR() \n
 *   SIPTAG_ORGANIZATION() and SIPTAG_ORGANIZATION_STR() \n
 * Any soa tags are also considered as handle-specific parameters. They are
 * defined in <sofia-sip/soa_tag.h>.
 *
 * The global parameters that can not be set by nua_set_hparams() include
 * NUTAG_DETECT_NETWORK_UPDATES(), NUTAG_SMIME_* tags, and all NTA tags.
 *
 * @par Events:
 *     #nua_r_set_params
 */

/** @NUA_EVENT nua_r_set_params
 *
 * Response to nua_set_params() or nua_set_hparams().
 *
 * @param status 200 when successful, error code otherwise
 * @param phrase a short textual description of @a status code
 * @param nh     NULL when responding to nua_set_params(),
 *               operation handle when responding to nua_set_hparams()
 * @param hmagic NULL when responding to nua_set_params(),
 *               application contact associated with the operation handle
 *               when responding to nua_set_hparams()
 * @param sip    NULL
 * @param tags   None
 *
 * @sa nua_set_params(), nua_set_hparams(),
 * #nua_r_get_params, nua_get_params(), nua_get_hparams()
 *
 * @END_NUA_EVENT
 */

int nua_stack_set_params(nua_t *nua, nua_handle_t *nh, nua_event_t e,
			 tagi_t const *tags)
{
  nua_handle_t *dnh = nua->nua_dhandle;

  int status;
  char const *phrase;

  nua_handle_preferences_t tmp[1];
  int any_changes = 0;

  enter;

  {
    su_home_t tmphome[1] = { SU_HOME_INIT(tmphome) };
    nua_handle_preferences_t *nhp = nh->nh_prefs;
    nua_handle_preferences_t const *dnhp = dnh->nh_prefs;
    nua_global_preferences_t gtmp[1], *ngp = NULL;

    *tmp = *nhp; NHP_UNSET_ALL(tmp);

    /*
     * Supported features, allowed methods and events are merged
     * with previous or default settings.
     *
     * Here we just copy pointers from default settings. However when saving
     * settings we have to be extra careful so that we
     * 1) zero the pointers if the setting has not been modified
     * 2) do not free pointer if the setting has been modified
     * See NHP_ZAP_OVERRIDEN() below for gorier details.
     */
    if (!NHP_ISSET(nhp, supported))
      tmp->nhp_supported = dnhp->nhp_supported;
    if (!NHP_ISSET(nhp, allow))
      tmp->nhp_allow = dnhp->nhp_allow;
    if (!NHP_ISSET(nhp, allow_events))
      tmp->nhp_allow_events = dnhp->nhp_allow_events;
    if (!NHP_ISSET(nhp, appl_method))
      tmp->nhp_appl_method = dnhp->nhp_appl_method;

    if (nh == dnh) /* nua_set_params() call, save stack-wide params, too */
      ngp = gtmp, *gtmp = *nua->nua_prefs;

    /* Set and save parameters to tmp */
    if (!nh->nh_used_ptags &&
	nhp_set_tags(tmphome, tmp, NULL, nh->nh_ptags) < 0)
      status = 900, phrase = "Error storing default handle parameters";
    else if (nhp_set_tags(tmphome, tmp, ngp, tags) < 0)
      status = 900, phrase = "Error storing parameters";
    else if ((any_changes = nhp_save_params(nh, tmphome, ngp, tmp)) < 0)
      status = 900, phrase = su_strerror(ENOMEM);
    else
      status = 200, phrase = "OK", nh->nh_used_ptags = 1;

    su_home_deinit(tmphome);
  }

  if (status == 200) {
    nua_handle_preferences_t const *nhp = nh->nh_prefs;
    nua_handle_preferences_t const *dnhp = dnh->nh_prefs;

    if (!nh->nh_soa && NHP_GET(nhp, dnhp, media_enable)) {
      /* Create soa when needed */
      char const *soa_name = NHP_GET(nhp, dnhp, soa_name);

      if (dnh->nh_soa)
	nh->nh_soa = soa_clone(dnh->nh_soa, nua->nua_root, nh);
      else
	nh->nh_soa = soa_create(soa_name, nua->nua_root, nh);

      if (!nh->nh_soa)
	status = 900, phrase = "Error Creating SOA Object";
      else if (soa_set_params(nh->nh_soa, TAG_NEXT(nh->nh_ptags)) < 0)
	status = 900, phrase = "Error Setting SOA Parameters";
    }
    else if (nh->nh_soa && !NHP_GET(nhp, dnhp, media_enable)) {
      /* ... destroy soa when not needed */
      soa_destroy(nh->nh_soa), nh->nh_soa = NULL;
    }

    if (status == 200 && tags && nh->nh_soa &&
	soa_set_params(nh->nh_soa, TAG_NEXT(tags)) < 0)
      status = 900, phrase = "Error Setting SOA Parameters";
  }

  if (status == 200 && nh == dnh) {
    /* Set stack-specific things below */
    if (nua_stack_set_smime_params(nua, tags) < 0) {
      status = 900, phrase = "Error setting S/MIME parameters";
    }
    else if (nua->nua_nta &&
	     nta_agent_set_params(nua->nua_nta, TAG_NEXT(tags)) < 0) {
      status = 900, phrase = "Error setting NTA parameters";
    }
    else {
      nua_stack_set_from(nua, 0, tags);

      if (nua->nua_prefs->ngp_detect_network_updates)
	nua_stack_launch_network_change_detector(nua);
    }
  }

  if (status != 200) {
    if (e == nua_i_none)
      SU_DEBUG_1(("nua_set_params(): failed: %s\n", phrase));
    return UA_EVENT2(e, status, phrase), -1;
  }
  else {
    if (e == nua_r_set_params)
      UA_EVENT2(e, status, phrase);

    if (any_changes) {
      nua_handle_preferences_t changed[1];

      *changed = *nh->nh_prefs;
      memcpy(&changed->nhp_set_, &tmp->nhp_set_, sizeof changed->nhp_set_);

      nua_dialog_update_params(nh->nh_ds,
			       changed,
			       nh->nh_prefs,
			       dnh->nh_prefs);
    }
    return 0;
  }
}


/** Parse parameters from tags to @a nhp or @a ngp.
 *
 * @param home allocate new values from @a home
 * @param nhp  structure to store handle preferences
 * @param ngp  structure to store global preferences
 * @param tags list of tags to parse
 */
static int nhp_set_tags(su_home_t *home,
			nua_handle_preferences_t *nhp,
			nua_global_preferences_t *ngp,
			tagi_t const *tags)
{

/* Set copy of string to handle pref structure */
#define NHP_SET_STR(nhp, name, v)				 \
  if ((v) != (tag_value_t)0) {					 \
    char const *_value = (char const *)v;			 \
    char *_new = _value ? su_strdup(home, _value) : NULL;	 \
    if (NHP_ISSET(nhp, name))					 \
      su_free(home, (void *)nhp->nhp_##name);			 \
    NHP_SET(nhp, name, _new);					 \
    if (_new == NULL && _value != NULL)				 \
      return -1;						 \
  }

/* Set copy of string from url to handle pref structure */
#define NHP_SET_STR_BY_URL(nhp, type, name, v)			 \
  if ((v) != (tag_value_t)-1) {					 \
    url_t const *_value = (url_t const *)(v);			 \
    type *_new = (type *)url_as_string(home, (void *)_value);	 \
    if (NHP_ISSET(nhp, name))					 \
      su_free(home, (void *)nhp->nhp_##name);			 \
    NHP_SET(nhp, name, _new);					 \
    if (_new == NULL && _value != NULL)				 \
      return -1;						 \
  }

/* Set copy of header to handle pref structure */
#define NHP_SET_HEADER(nhp, name, hdr, v)			 \
  if ((v) != 0) {						 \
    sip_##hdr##_t const *_value = (sip_##hdr##_t const *)(v);	 \
    sip_##hdr##_t *_new = NULL;				 \
    if (_value != SIP_NONE)					 \
      _new = sip_##name##_dup(home, _value);			 \
    if (NHP_ISSET(nhp, name))					 \
      msg_header_free_all(home, (void *)nhp->nhp_##name);	 \
    NHP_SET(nhp, name, _new);					 \
    if (_new == NULL && _value != SIP_NONE)			 \
      return -1;						 \
  }

/* Set header made of string to handle pref structure */
#define NHP_SET_HEADER_STR(nhp, name, hdr, v)			 \
  if ((v) != 0) {						 \
    char const *_value = (char const *)(v);			 \
    sip_##hdr##_t *_new = NULL;				 \
    if (_value != SIP_NONE)					 \
      _new = sip_##name##_make(home, _value);			 \
    if (NHP_ISSET(nhp, name))					 \
      msg_header_free_all(home, (void *)nhp->nhp_##name);	 \
    NHP_SET(nhp, name, _new);					 \
    if (_new == NULL && _value != SIP_NONE)			 \
      return -1;						 \
  }

/* Append copy of header to handle pref structure */
#define NHP_APPEND_HEADER(nhp, name, hdr, is_str, next, v)	 \
  {								 \
    sip_##hdr##_t const *_value = (sip_##hdr##_t const *)(v);	 \
    char const *_str = (char const *)(v);			 \
    sip_##hdr##_t *_new = NULL;					 \
    sip_##hdr##_t **_end = &nhp->nhp_##name;			 \
    if (_value != SIP_NONE && _value != NULL) {			 \
      _new = (is_str)						 \
	? sip_##hdr##_make(home, _str)				 \
	: sip_##hdr##_dup(home, _value);			 \
      if (_new == NULL) return -1;				 \
    }								 \
    if (NHP_ISSET(nhp, name))					 \
      while(*_end)						 \
	_end = next(*_end);					 \
    nhp->nhp_set.nhb_##name = 1;				 \
    *_end = _new;						 \
  }

/* Set copy of string from header to handle pref structure */
#define NHP_SET_STR_BY_HEADER(nhp, name, v)			 \
  if ((v) != 0) {					 \
    sip_##name##_t const *_value = (sip_##name##_t const *)(v);	 \
    char *_new = NULL;						 \
    if (_value != SIP_NONE)					 \
      _new = sip_header_as_string(home, (void *)_value);	 \
    if (NHP_ISSET(nhp, name))					 \
      su_free(home, (void *)nhp->nhp_##name);			 \
    NHP_SET(nhp, name, _new);					 \
    if (_new == NULL && _value != SIP_NONE)			 \
      return -1;						 \
  }


  tagi_t const *t;

  for (t = tags; t; t = tl_next(t)) {
    tag_type_t tag = t->t_tag;
    tag_value_t value = t->t_value;

    if (tag == NULL)
      break;
    /* NUTAG_RETRY_COUNT(retry_count) */
    else if (tag == nutag_retry_count) {
      NHP_SET(nhp, retry_count, (unsigned)value);
    }
    /* NUTAG_MAX_SUBSCRIPTIONS(max_subscriptions) */
    else if (tag == nutag_max_subscriptions) {
      NHP_SET(nhp, max_subscriptions, (unsigned)value);
    }
    /* NUTAG_SOA_NAME(soa_name) */
    else if (tag == nutag_soa_name) {
      NHP_SET_STR(nhp, soa_name, value);
    }
    /* NUTAG_MEDIA_ENABLE(media_enable) */
    else if (tag == nutag_media_enable) {
      NHP_SET(nhp, media_enable, value != 0);
    }
    /* NUTAG_ENABLEINVITE(invite_enable) */
    else if (tag == nutag_enableinvite) {
      NHP_SET(nhp, invite_enable, value != 0);
    }
    /* NUTAG_AUTOALERT(auto_alert) */
    else if (tag == nutag_autoalert) {
      NHP_SET(nhp, auto_alert, value != 0);
    }
    /* NUTAG_EARLY_ANSWER(early_answer) */
    else if (tag == nutag_early_answer) {
      NHP_SET(nhp, early_answer, value != 0);
    }
    /* NUTAG_EARLY_MEDIA(early_media) */
    else if (tag == nutag_early_media) {
      NHP_SET(nhp, early_media, value != 0);
    }
    /* NUTAG_ONLY183_100REL(only183_100rel) */
    else if (tag == nutag_only183_100rel) {
      NHP_SET(nhp, only183_100rel, value != 0);
    }
    /* NUTAG_AUTOANSWER(auto_answer) */
    else if (tag == nutag_autoanswer) {
      NHP_SET(nhp, auto_answer, value != 0);
    }
    /* NUTAG_AUTOACK(auto_ack) */
    else if (tag == nutag_autoack) {
      NHP_SET(nhp, auto_ack, value != 0);
    }
    /* NUTAG_INVITE_TIMER(invite_timeout) */
    else if (tag == nutag_invite_timer) {
      NHP_SET(nhp, invite_timeout, (unsigned)value);
    }
    /* NUTAG_SESSION_TIMER(session_timer) */
    else if (tag == nutag_session_timer) {
      NHP_SET(nhp, session_timer, (unsigned)value);
    }
    /* NUTAG_MIN_SE(min_se) */
    else if (tag == nutag_min_se) {
      NHP_SET(nhp, min_se, (unsigned)value);
    }
    /* NUTAG_SESSION_REFRESHER(refresher) */
    else if (tag == nutag_session_refresher) {
      int refresher = value;

      if (refresher >= nua_remote_refresher)
	refresher = nua_remote_refresher;
      else if (refresher <= nua_no_refresher)
	refresher = nua_no_refresher;

      NHP_SET(nhp, refresher, (enum nua_session_refresher)refresher);
    }
    /* NUTAG_UPDATE_REFRESH(update_refresh) */
    else if (tag == nutag_update_refresh) {
      NHP_SET(nhp, update_refresh, value != 0);
    }
    /* NUTAG_REFRESH_WITHOUT_SDP(refresh_without_sdp) */
    else if (tag == nutag_refresh_without_sdp) {
      NHP_SET(nhp, refresh_without_sdp, value != 0);
    }
    /* NUTAG_ENABLEMESSAGE(message_enable) */
    else if (tag == nutag_enablemessage) {
      NHP_SET(nhp, message_enable, value != 0);
    }
    /* NUTAG_ENABLEMESSENGER(win_messenger_enable) */
    else if (tag == nutag_enablemessenger) {
      NHP_SET(nhp, win_messenger_enable, value != 0);
    }
    /* NUTAG_CALLEE_CAPS(callee_caps) */
    else if (tag == nutag_callee_caps) {
      NHP_SET(nhp, callee_caps, value != 0);
    }
    /* NUTAG_MEDIA_FEATURES(media_features) */
    else if (tag == nutag_media_features) {
      NHP_SET(nhp, media_features, value != 0);
    }
    /* NUTAG_SERVICE_ROUTE_ENABLE(service_route_enable) */
    else if (tag == nutag_service_route_enable) {
      NHP_SET(nhp, service_route_enable, value != 0);
    }
    /* NUTAG_PATH_ENABLE(path_enable) */
    else if (tag == nutag_path_enable) {
      NHP_SET(nhp, path_enable, value != 0);
    }
    /* NUTAG_AUTH_CACHE(auth_cache) */
    else if (tag == nutag_auth_cache) {
      if (value >= 0 && value < (tag_value_t)_nua_auth_cache_invalid)
	NHP_SET(nhp, auth_cache, (int)value);
    }
    /* NUTAG_REFER_EXPIRES(refer_expires) */
    else if (tag == nutag_refer_expires) {
      NHP_SET(nhp, refer_expires, value);
    }
    /* NUTAG_REFER_WITH_ID(refer_with_id) */
    else if (tag == nutag_refer_with_id) {
      NHP_SET(nhp, refer_with_id, value != 0);
    }
    /* NUTAG_SUBSTATE(substate) */
    else if (tag == nutag_substate) {
      NHP_SET(nhp, substate, (int)value);
    }
    /* NUTAG_SUB_EXPIRES(sub_expires) */
    else if (tag == nutag_sub_expires) {
      NHP_SET(nhp, sub_expires, value);
    }
    /* NUTAG_KEEPALIVE(keepalive) */
    else if (tag == nutag_keepalive) {
      NHP_SET(nhp, keepalive, (unsigned)value);
    }
    /* NUTAG_KEEPALIVE_STREAM(keepalive_stream) */
    else if (tag == nutag_keepalive_stream) {
      NHP_SET(nhp, keepalive_stream, (unsigned)value);
    }

    /* NUTAG_SUPPORTED(feature) */
    /* SIPTAG_SUPPORTED_STR(supported_str) */
    /* SIPTAG_SUPPORTED(supported) */
    else if (tag == nutag_supported ||
	     tag == siptag_supported ||
	     tag == siptag_supported_str) {
      int ok;
      sip_supported_t *supported = NULL;

      ok = nhp_merge_lists(home,
			   sip_supported_class, &supported, nhp->nhp_supported,
			   NHP_ISSET(nhp, supported), /* already set by tags */
			   tag == siptag_supported, /* dup it, don't make */
			   tag == nutag_supported, /* merge with old value */
			   t->t_value);
      if (ok < 0)
	return -1;
      else if (ok)
	NHP_SET(nhp, supported, supported);
    }
    /* NUTAG_ALLOW(allowing) */
    /* SIPTAG_ALLOW_STR(allow_str) */
    /* SIPTAG_ALLOW(allow) */
    else if (tag == nutag_allow ||
	     tag == siptag_allow_str ||
	     tag == siptag_allow) {
      int ok;
      msg_list_t *allow = NULL;

      ok = nhp_merge_lists(home,
			   sip_allow_class,
			   &allow,
			   (msg_list_t const *)nhp->nhp_allow,
			   NHP_ISSET(nhp, allow), /* already set by tags */
			   tag == siptag_allow, /* dup it, don't make */
			   tag == nutag_allow, /* merge with old value */
			   t->t_value);
      if (ok < 0)
	return -1;
      else if (ok)
	NHP_SET(nhp, allow, (sip_allow_t *)allow);
    }
    /* NUTAG_ALLOW_EVENTS(allow_events) */
    /* SIPTAG_ALLOW_EVENTS_STR(allow_events) */
    /* SIPTAG_ALLOW_EVENTS(allow_events) */
    else if (tag == nutag_allow_events ||
	     tag == siptag_allow_events_str ||
	     tag == siptag_allow_events) {
      int ok;
      sip_allow_events_t *allow_events = NULL;

      ok = nhp_merge_lists(home,
			   sip_allow_events_class,
			   &allow_events,
			   nhp->nhp_allow_events,
			   NHP_ISSET(nhp, allow_events), /* already set */
			   tag == siptag_allow_events, /* dup it, don't make */
			   tag == nutag_allow_events, /* merge with old value */
			   t->t_value);
      if (ok < 0)
	return -1;
      else if (ok)
	NHP_SET(nhp, allow_events, allow_events);
    }
    /* NUTAG_APPL_METHOD(appl_method) */
    else if (tag == nutag_appl_method) {
      if (t->t_value == 0) {
	NHP_SET(nhp, appl_method, NULL);
      }
      else {
	int ok;
	msg_list_t *appl_method = NULL;

	ok = nhp_merge_lists(home,
			     sip_allow_class,
			     &appl_method,
			     (msg_list_t const *)nhp->nhp_appl_method,
			     /* already set by tags? */
			     NHP_ISSET(nhp, appl_method),
			     0, /* dup it, don't make */
			     1, /* merge with old value */
			     t->t_value);
	if (ok < 0)
	  return -1;
	else if (ok)
	  NHP_SET(nhp, appl_method, (sip_allow_t *)appl_method);
      }
    }
    else if (tag == nutag_initial_route ||
	     tag == nutag_initial_route_str) {
#define next_route(r) (&(r)->r_next)
      NHP_APPEND_HEADER(nhp, initial_route, route,
			(tag == nutag_initial_route_str),
			next_route,
			t->t_value);
      sip_route_fix(nhp->nhp_initial_route);
    }
    /* SIPTAG_USER_AGENT(user_agent) */
    else if (tag == siptag_user_agent) {
      NHP_SET_STR_BY_HEADER(nhp, user_agent, value);
    }
    /* SIPTAG_USER_AGENT_STR(user_agent_str) */
    else if (tag == siptag_user_agent_str && value != 0) {
      if (value == -1)
	value = 0;
      NHP_SET_STR(nhp, user_agent, value);
    }
    /* NUTAG_USER_AGENT(ua_name) */
    else if (tag == nutag_user_agent) {
      /* Add contents of NUTAG_USER_AGENT() to our distribution name */
      char const *str = (void *)value, *ua;

      if (str && !already_contains_package_name(str))
	ua = su_sprintf(home, "%s %s", str, NHP_USER_AGENT);
      else if (str)
	ua = su_strdup(home, str);
      else
	ua = su_strdup(home, NHP_USER_AGENT);

      NHP_SET(nhp, user_agent, ua);
    }
    /* SIPTAG_ORGANIZATION(organization) */
    else if (tag == siptag_organization) {
      NHP_SET_STR_BY_HEADER(nhp, organization, value);
    }
    /* SIPTAG_ORGANIZATION_STR(organization_str) */
    else if (tag == siptag_organization_str) {
      if (value == -1)
	value = 0;
      NHP_SET_STR(nhp, organization, value);
    }
    /* NUTAG_REGISTRAR(registrar) */
    else if (tag == nutag_registrar) {
      NHP_SET_STR_BY_URL(nhp, char, registrar, value);
      if (NHP_ISSET(nhp, registrar) && su_strmatch(nhp->nhp_registrar, "*"))
	NHP_SET_STR(nhp, registrar, 0);
    }
    /* NUTAG_INSTANCE(instance) */
    else if (tag == nutag_instance) {
      NHP_SET_STR(nhp, instance, value);
    }
    /* NUTAG_M_DISPLAY(m_display) */
    else if (tag == nutag_m_display) {
      NHP_SET_STR(nhp, m_display, value);
    }
    /* NUTAG_M_USERNAME(m_username) */
    else if (tag == nutag_m_username) {
      NHP_SET_STR(nhp, m_username, value);
    }
    /* NUTAG_M_PARAMS(m_params) */
    else if (tag == nutag_m_params) {
      NHP_SET_STR(nhp, m_params, value);
    }
    /* NUTAG_M_FEATURES(m_features) */
    else if (tag == nutag_m_features) {
      NHP_SET_STR(nhp, m_features, value);
    }
    /* NUTAG_OUTBOUND(outbound) */
    else if (tag == nutag_outbound) {
      NHP_SET_STR(nhp, outbound, value);
    }
    /* NUTAG_PROXY() (aka NTATAG_DEFAULT_PROXY()) */
    else if (tag == ntatag_default_proxy) {
      NHP_SET_STR_BY_URL(nhp, url_string_t, proxy, value);
    }
    /* NUTAG_DETECT_NETWORK_UPDATES(detect_network_updates) */
    else if (ngp && tag == nutag_detect_network_updates) {
      int detector = (int)value;

      if (detector < NUA_NW_DETECT_NOTHING)
	detector = NUA_NW_DETECT_NOTHING;
      else if (detector > NUA_NW_DETECT_TRY_FULL)
	detector = NUA_NW_DETECT_TRY_FULL;

      ngp->ngp_detect_network_updates = detector;
      ngp->ngp_set.ngp_detect_network_updates = 1;
    }
    /* NUTAG_SHUTDOWN_EVENTS() */
    else if (ngp && tag == nutag_shutdown_events) {
      ngp->ngp_shutdown_events = value != 0;
      ngp->ngp_set.ngp_shutdown_events = 1;
    }
  }

  return 0;
}

/** Merge (when needed) new values with old values. */
static int nhp_merge_lists(su_home_t *home,
			   msg_hclass_t *hc,
			   msg_list_t **return_new_list,
			   msg_list_t const *old_list,
			   int already_set,
			   int already_parsed,
			   int always_merge,
			   tag_value_t value)
{
  msg_list_t *list, *elems;

  if (value == -1) {
    *return_new_list = NULL;
    return 1;
  }

  if (value == 0) {
    if (!already_set && !always_merge) {
      *return_new_list = NULL;
      return 1;
    }
    return 0;
  }

  if (already_parsed)
    elems = (void *)msg_header_dup_as(home, hc, (msg_header_t *)value);
  else
    elems = (void *)msg_header_make(home, hc, (char const *)value);

  if (!elems)
    return -1;

  list = (msg_list_t *)old_list;

  if (!already_set) {
    if (always_merge && list) {
      list = (void *)msg_header_dup_as(home, hc, (void *)old_list);
      if (!list)
	return -1;
    }
    else
      list = NULL;
  }

  if (!list) {
    *return_new_list = elems;
    return 1;
  }

  /* Add contents to the new list to the old list */
  if (msg_params_join(home, (msg_param_t **)&list->k_items, elems->k_items,
		      2 /* prune */, 0 /* don't dup */) < 0)
    return -1;

  *return_new_list =
    (msg_list_t *)msg_header_dup_as(home, hc, (msg_header_t *)list);
  if (!*return_new_list)
    return -1;

  msg_header_free(home, (msg_header_t *)list);
  msg_header_free(home, (msg_header_t *)elems);

  return 1;
}

/** Save parameters in @a gtmp and @a tmp.
 *
 * @retval 1 - parameters were changed
 * @retval 0 - no changes in parameters
 * @retval -1 - an error
 */
static
int nhp_save_params(nua_handle_t *nh,
		    su_home_t *tmphome,
		    nua_global_preferences_t *gsrc,
		    nua_handle_preferences_t *src)
{
  su_home_t *home = nh->nh_home;
  nua_t *nua = nh->nh_nua;
  nua_handle_t *dnh = nua->nua_dhandle;
  nua_handle_preferences_t *dst = nh->nh_prefs, old[1];

  if (gsrc) {
    *nua->nua_prefs = *gsrc;	/* No pointers this far */
  }

  if (!NHP_IS_ANY_SET(src))
    return 0;

  if (nh == dnh || nh->nh_prefs != dnh->nh_prefs) {
    dst = nh->nh_prefs, *old = *dst;
  }
  else {
    dst = su_zalloc(home, sizeof *dst), memset(old, 0, sizeof *old);
    if (!dst)
      return -1;
  }

  /* Move allocations from tmphome to home */
  su_home_move(nh->nh_home, tmphome);

  /* Copy parameters that are set from src to dst */
  nhp_or_set(dst, src);

  /* Handle pointer items. Free changed ones and zap unset ones. */
  /* Note that pointer items where !NHP_ISSET(old, pref) are not freed
     (because they were just on loan from the default preference set) */
#define NHP_ZAP_OVERRIDEN(old, dst, free, pref)				\
  (((NHP_ISSET(old, pref) &&						\
     (old)->nhp_##pref && (old)->nhp_##pref != (dst)->nhp_##pref)	\
    ? (free)(home, (void *)(old)->nhp_##pref) : (void)0),		\
   (void)(!(dst)->nhp_set.nhb_##pref ? (dst)->nhp_##pref = NULL : NULL))

  NHP_ZAP_OVERRIDEN(old, dst, su_free, soa_name);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, registrar);
  NHP_ZAP_OVERRIDEN(old, dst, msg_header_free, allow);
  NHP_ZAP_OVERRIDEN(old, dst, msg_header_free, supported);
  NHP_ZAP_OVERRIDEN(old, dst, msg_header_free, allow_events);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, user_agent);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, organization);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, m_display);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, m_username);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, m_params);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, m_features);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, instance);
  NHP_ZAP_OVERRIDEN(old, dst, su_free, outbound);
  NHP_ZAP_OVERRIDEN(old, dst, msg_header_free, appl_method);
  NHP_ZAP_OVERRIDEN(old, dst, msg_header_free, initial_route);

  nh->nh_prefs = dst;

  return memcmp(dst, old, sizeof *dst) != 0;
}

static int nua_handle_tags_filter(tagi_t const *f, tagi_t const *t);
static int nua_handle_param_filter(tagi_t const *f, tagi_t const *t);

/** Save taglist to a handle */
int nua_handle_save_tags(nua_handle_t *nh, tagi_t *tags)
{
  /* Initialization parameters */
  url_string_t const *url = NULL;
  sip_to_t const *p_to = NULL;
  char const *to_str = NULL;
  sip_from_t const *p_from = NULL;
  char const *from_str = NULL;
  nua_handle_t *identity = NULL;

  tagi_t const *t;

  su_home_t tmphome[SU_HOME_AUTO_SIZE(1024)];

  int error;

#if HAVE_OPEN_C
  /* Nice. An old symbian compiler */
  tagi_t tagfilter[2];
  tagi_t paramfilter[2];

  tagfilter[0].t_tag = tag_filter;
  tagfilter[0].t_value = tag_filter_v(nua_handle_tags_filter);
  tagfilter[1].t_tag = (tag_type_t)0;
  tagfilter[1].t_value = (tag_value_t)0;

  paramfilter[0].t_tag = tag_filter;
  paramfilter[0].t_value = tag_filter_v(nua_handle_param_filter);
  paramfilter[1].t_tag = (tag_type_t)0;
  paramfilter[1].t_value = (tag_value_t)0;

#else
  tagi_t const tagfilter[] = {
    { TAG_FILTER(nua_handle_tags_filter) },
    { TAG_NULL() }
  };
  tagi_t const paramfilter[] = {
    { TAG_FILTER(nua_handle_param_filter) },
    { TAG_NULL() }
  };
#endif

  for (t = tags; t; t = tl_next(t)) {
    if (t->t_tag == NULL)
      break;
    /* SIPTAG_FROM_REF(p_from) */
    else if (t->t_tag == siptag_from) {
      p_from = (sip_from_t *)t->t_value, from_str = NULL;
    }
    /* SIPTAG_FROM_STR_REF(from_str) */
    else if (t->t_tag == siptag_from_str) {
      from_str = (char const *)t->t_value, p_from = NULL;
    }
    /* SIPTAG_TO_REF(p_to) */
    else if (t->t_tag == siptag_to) {
      p_to = (sip_to_t *)t->t_value, to_str = NULL;
    }
    /* SIPTAG_TO_STR_REF(to_str) */
    else if (t->t_tag == siptag_to_str) {
      to_str = (char const *)t->t_value, p_to = NULL;
    }
    /* NUTAG_IDENTITY_REF(identity) */
    else if (t->t_tag == nutag_identity) {
      identity = (nua_handle_t *)t->t_value;
    }
    /* NUTAG_URL_REF(url) */
    else if (t->t_tag == nutag_url) {
      url = (url_string_t *)t->t_value;
    }
    /* NUTAG_SIPS_URL_REF(url) */
    else if (t->t_tag == nutag_sips_url) {
      url = (url_string_t *)t->t_value;
    }
  }

  su_home_auto(tmphome, sizeof tmphome);

  if (p_from)
    ;
  else if (from_str)
    p_from = sip_from_make(tmphome, from_str);
  else
    p_from = SIP_NONE;

  if (p_to)
    ;
  else if (to_str)
    p_to = sip_to_make(tmphome, to_str);
  else if (url)
    p_to = sip_to_create(tmphome, url),
      p_to ? sip_aor_strip((url_t*)p_to->a_url) : 0;
  else
    p_to = SIP_NONE;

  if (p_to == NULL || p_from == NULL) {
    su_home_deinit(tmphome);
    return -1;
  }

  nh->nh_tags =
    tl_filtered_tlist(nh->nh_home, tagfilter,
		      TAG_IF(p_from != SIP_NONE, SIPTAG_FROM(p_from)),
		      TAG_IF(p_from != SIP_NONE, TAG_FILTER(nua_handle_tags_filter)),
		      TAG_IF(p_to != SIP_NONE, SIPTAG_TO(p_to)),
		      TAG_IF(p_to != SIP_NONE, TAG_FILTER(nua_handle_tags_filter)),
		      TAG_NEXT(tags));

  nh->nh_ptags =
    tl_filtered_tlist(nh->nh_home, paramfilter, TAG_NEXT(tags));

  error = nh->nh_tags == NULL || nh->nh_ptags == NULL;

  if (!error)
    tl_gets(nh->nh_tags,	/* These does not change while nh lives */
	    SIPTAG_FROM_REF(nh->nh_ds->ds_local),
	    SIPTAG_TO_REF(nh->nh_ds->ds_remote),
	    TAG_END());

  if (nh->nh_ptags && nh->nh_ptags->t_tag == NULL)
    su_free(nh->nh_home, nh->nh_ptags), nh->nh_ptags = NULL;

  if (identity)
    nh->nh_identity = nua_handle_ref(identity);

  su_home_deinit(tmphome);

  return -error;
}

/** Filter tags used for settings. */
static int nua_handle_param_filter(tagi_t const *f, tagi_t const *t)
{
  char const *ns;

  if (!t || !t->t_tag)
    return 0;

  if (t->t_tag == nutag_url ||
      t->t_tag == nutag_sips_url ||
      t->t_tag == nutag_identity)
    return 0;

  ns = t->t_tag->tt_ns;
  if (!ns)
    return 0;

  return strcmp(ns, "nua") == 0 || strcmp(ns, "soa") == 0;
}

/** Filter tags stored permanently as taglist. */
static int nua_handle_tags_filter(tagi_t const *f, tagi_t const *t)
{
  tag_type_t tag;

  if (!t || !t->t_tag)
    return 0;

  tag = t->t_tag;

  if (tag == tag_filter)
    return 0;

  /* Accept @From or @To only when they are followed by
     TAG_FILTER(nua_handle_tags_filter) */
  if (tag == siptag_from || tag == siptag_to) {
    t = tl_next(t);
    return t && t->t_tag == tag_filter &&
      t->t_value == (tag_value_t)nua_handle_tags_filter;
  }

  if (tag == nutag_identity)
    return 0;
  if (tag == siptag_from_str)
    return 0;
  if (tag == siptag_to_str)
    return 0;

  /** Ignore @CSeq, @RSeq, @RAck, @Timestamp, and @ContentLength */
  if (tag == siptag_cseq || tag == siptag_cseq_str)
    return 0;
  if (tag == siptag_rseq || tag == siptag_rseq_str)
    return 0;
  if (tag == siptag_rack || tag == siptag_rack_str)
    return 0;
  if (tag == siptag_timestamp || tag == siptag_timestamp_str)
    return 0;
  if (tag == siptag_content_length || tag == siptag_content_length_str)
    return 0;

  return ! nua_handle_param_filter(f, t);
}

static
int nua_stack_set_smime_params(nua_t *nua, tagi_t const *tags)
{
#if HAVE_SOFIA_SMIME
  int           smime_enable = nua->sm->sm_enable;
  int           smime_opt = nua->sm->sm_opt;
  int           smime_protection_mode = nua->sm->sm_protection_mode;
  char const   *smime_message_digest = NONE;
  char const   *smime_signature = NONE;
  char const   *smime_key_encryption = NONE;
  char const   *smime_message_encryption = NONE;
  char const   *smime_path = NONE;

  int n;

  n = tl_gets(tags,
	      NUTAG_SMIME_ENABLE_REF(smime_enable),
	      NUTAG_SMIME_OPT_REF(smime_opt),
	      NUTAG_SMIME_PROTECTION_MODE_REF(smime_protection_mode),
	      NUTAG_SMIME_MESSAGE_DIGEST_REF(smime_message_digest),
	      NUTAG_SMIME_SIGNATURE_REF(smime_signature),
	      NUTAG_SMIME_KEY_ENCRYPTION_REF(smime_key_encryption),
	      NUTAG_SMIME_MESSAGE_ENCRYPTION_REF(smime_message_encryption),
	      NUTAG_CERTIFICATE_DIR_REF(smime_path),
	      TAG_NULL());
  if (n <= 0)
    return n;

  /* XXX - all other S/MIME parameters? */
  return sm_set_params(nua->sm, smime_enable, smime_opt,
		       smime_protection_mode, smime_path);
#endif

  return 0;
}

/**@fn void nua_get_params(nua_t *nua, tag_type_t tag, tag_value_t value, ...)
 *
 * Get NUA parameters matching with the given filter.
 * The values of NUA parameters is returned in #nua_r_get_params event.
 *
 * @param nua             Pointer to NUA stack object
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     TAG_ANY() \n
 *     otherwise same tags as nua_set_params()
 *
 * @par Events:
 *     #nua_r_get_params
 *
 * @par Examples
 * Find out default values of all parameters:
 * @code
 *    nua_get_params(nua, TAG_ANY(), TAG_END());
 * @endcode
 */

/**@fn void nua_get_hparams(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
 *
 * Get values of handle-specific parameters in #nua_r_get_params event.
 *
 * Application will specify either expilicit list of tags it is interested
 * in, or a filter (at the moment, TAG_ANY()). The values are returned as a
 * list of tags in the #nua_r_get_params event.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * The handle-specific parameters will contain only the parameters actually
 * modified by application, either by nua_set_hparams() or some other
 * handle-specific call. Currently, no NTA parameters are returned. They are
 * returned only when application asks for user-agent-level parameters using
 * either nua_get_params() or using default handle, eg.
 * @code
 * nua_get_hparams(nua_default(nua), TAG_ANY())
 * @endcode
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     #TAG_ANY \n
 *     othervise same tags as nua_set_hparams()
 *
 * @par Events:
 *     #nua_r_get_params
 */

/** @NUA_EVENT nua_r_get_params
 *
 * Answer to nua_get_params() or nua_get_hparams().
 *
 * @param status 200 when succesful, error code otherwise
 * @param phrase a short textual description of @a status code
 * @param nh     NULL when responding to nua_get_params(),
 *               operation handle when responding to nua_get_hparams()
 * @param hmagic NULL when responding to nua_get_params(),
 *               application contact associated with the operation handle
 *               when responding to nua_get_hparams()
 * @param sip    NULL
 * @param tags
 *   NUTAG_APPL_METHOD() \n
 *   NUTAG_AUTH_CACHE() \n
 *   NUTAG_AUTOACK() \n
 *   NUTAG_AUTOALERT() \n
 *   NUTAG_AUTOANSWER() \n
 *   NUTAG_CALLEE_CAPS() \n
 *   NUTAG_DETECT_NETWORK_UPDATES() \n
 *   NUTAG_EARLY_ANSWER() \n
 *   NUTAG_EARLY_MEDIA() \n
 *   NUTAG_ENABLEINVITE() \n
 *   NUTAG_ENABLEMESSAGE() \n
 *   NUTAG_ENABLEMESSENGER() \n
 *   NUTAG_INITIAL_ROUTE() \n
 *   NUTAG_INITIAL_ROUTE_STR() \n
 *   NUTAG_INSTANCE() \n
 *   NUTAG_INVITE_TIMER() \n
 *   NUTAG_KEEPALIVE() \n
 *   NUTAG_KEEPALIVE_STREAM() \n
 *   NUTAG_MAX_SUBSCRIPTIONS() \n
 *   NUTAG_MEDIA_ENABLE() \n
 *   NUTAG_MEDIA_FEATURES() \n
 *   NUTAG_MIN_SE() \n
 *   NUTAG_M_DISPLAY() \n
 *   NUTAG_M_FEATURES() \n
 *   NUTAG_M_PARAMS() \n
 *   NUTAG_M_USERNAME() \n
 *   NUTAG_ONLY183_100REL() \n
 *   NUTAG_OUTBOUND() \n
 *   NUTAG_PATH_ENABLE() \n
 *   NUTAG_REFER_EXPIRES() \n
 *   NUTAG_REFER_WITH_ID() \n
 *   NUTAG_REFRESH_WITHOUT_SDP() \n
 *   NUTAG_REGISTRAR() \n
 *   NUTAG_RETRY_COUNT() \n
 *   NUTAG_SERVICE_ROUTE_ENABLE() \n
 *   NUTAG_SESSION_REFRESHER() \n
 *   NUTAG_SESSION_TIMER() \n
 *   NUTAG_SMIME_ENABLE() \n
 *   NUTAG_SMIME_KEY_ENCRYPTION() \n
 *   NUTAG_SMIME_MESSAGE_DIGEST() \n
 *   NUTAG_SMIME_MESSAGE_ENCRYPTION() \n
 *   NUTAG_SMIME_OPT() \n
 *   NUTAG_SMIME_PROTECTION_MODE() \n
 *   NUTAG_SMIME_SIGNATURE() \n
 *   NUTAG_SOA_NAME() \n
 *   NUTAG_SUBSTATE() \n
 *   NUTAG_SUB_EXPIRES() \n
 *   NUTAG_UPDATE_REFRESH() \n
 *   NUTAG_USER_AGENT() \n
 *   SIPTAG_ALLOW() \n
 *   SIPTAG_ALLOW_STR() \n
 *   SIPTAG_ALLOW_EVENTS() \n
 *   SIPTAG_ALLOW_EVENTS_STR() \n
 *   SIPTAG_FROM() \n
 *   SIPTAG_FROM_STR() \n
 *   SIPTAG_ORGANIZATION() \n
 *   SIPTAG_ORGANIZATION_STR() \n
 *   SIPTAG_SUPPORTED() \n
 *   SIPTAG_SUPPORTED_STR() \n
 *   SIPTAG_USER_AGENT() \n
 *   SIPTAG_USER_AGENT_STR() \n
 *
 * @sa nua_get_params(), nua_get_hparams(),
 * nua_set_params(), nua_set_hparams(), #nua_r_set_params
 *
 * @END_NUA_EVENT
 */

/**@internal
 * Send a list of NUA parameters to the application.
 *
 * This function gets invoked when application calls either nua_get_params()
 * or nua_get_hparams().
 *
 * The parameter tag list will initially contain all the relevant parameter
 * tags, and it will be filtered down to parameters asked by application.
 *
 * The handle-specific parameters will contain only the parameters actually
 * modified by application, either by nua_set_hparams() or some other
 * handle-specific call. NTA parameters are returned only when application
 * asks for user-agent-level parameters using nua_get_params().
 *
 */
int nua_stack_get_params(nua_t *nua, nua_handle_t *nh, nua_event_t e,
			 tagi_t const *tags)
{
  nua_handle_t *dnh = nua->nua_dhandle;
  nua_global_preferences_t const *ngp = nua->nua_prefs;
  nua_handle_preferences_t const *nhp = nh->nh_prefs;
  nua_handle_preferences_t const nhp_zero[1] = {{ 0 }};
  tagi_t *lst;

  int has_from;
  sip_from_t from[1];

  sip_contact_t const *m;

  /* nta */
  unsigned udp_mtu = 0;
  usize_t max_proceeding = 0;
  unsigned sip_t1 = 0, sip_t2 = 0, sip_t4 = 0, sip_t1x64 = 0;
  unsigned debug_drop_prob = 0;
  url_string_t const *proxy = NULL;
  sip_contact_t const *aliases = NULL;
  unsigned flags = 0;

  /* soa */
  tagi_t *media_params = NULL;

  su_home_t tmphome[SU_HOME_AUTO_SIZE(16536)];

  enter;

  if (nh == dnh)
    nta_agent_get_params(nua->nua_nta,
			 NTATAG_UDP_MTU_REF(udp_mtu),
			 NTATAG_MAX_PROCEEDING_REF(max_proceeding),
			 NTATAG_SIP_T1_REF(sip_t1),
			 NTATAG_SIP_T2_REF(sip_t2),
			 NTATAG_SIP_T4_REF(sip_t4),
			 NTATAG_SIP_T1X64_REF(sip_t1x64),
			 NTATAG_DEBUG_DROP_PROB_REF(debug_drop_prob),
			 NTATAG_DEFAULT_PROXY_REF(proxy),
			 NTATAG_ALIASES_REF(aliases),
			 NTATAG_SIPFLAGS_REF(flags),
			 TAG_END());

  if (nh->nh_ds->ds_local)
    has_from = 1, *from = *nh->nh_ds->ds_local, from->a_params = NULL;
  else /* if (nua->nua_from_is_set) */
    has_from = 1, *from = *nua->nua_from;

  media_params = soa_get_paramlist(nh->nh_soa, TAG_END());

  m = nua_stack_get_contact(nua->nua_registrations);

  /* Include tag in the list returned to user
   * if it has been earlier set (by user) */
#define TIF(TAG, pref) \
  TAG_IF(nhp->nhp_set.nhb_##pref, TAG(nhp->nhp_##pref))

  /* Include tag in the list returned to user
   * if it has been earlier set (by user)
   * but always include in the default parameters */
#define TIFD(TAG, pref) \
  TAG_IF(nh == dnh || nhp->nhp_set.nhb_##pref, TAG(nhp->nhp_##pref))

  /* Include string tag made out of SIP header
   * if it has been earlier set (by user) */
#define TIF_STR(TAG, pref)						\
  TAG_IF(nhp->nhp_set.nhb_##pref,					\
	 TAG(nhp->nhp_set.nhb_##pref && nhp->nhp_##pref			\
	     ? sip_header_as_string(tmphome, (void *)nhp->nhp_##pref) : NULL))

  /* Include header tag made out of string
   * if it has been earlier set (by user) */
#define TIF_SIP(TAG, pref)						\
  TAG_IF(nhp->nhp_set.nhb_##pref,					\
	 TAG(nhp->nhp_set.nhb_##pref && nhp->nhp_##pref			\
	     ? sip_##pref##_make(tmphome, (char *)nhp->nhp_##pref)	\
	     : NULL))

  if (nh != dnh && nhp == dnh->nh_prefs)
    nhp = nhp_zero;

  su_home_auto(tmphome, sizeof(tmphome));

  lst = tl_filtered_tlist
    (tmphome, tags,
     TAG_IF(has_from, SIPTAG_FROM(from)),
     TAG_IF(has_from,
	    SIPTAG_FROM_STR(has_from
			    ? sip_header_as_string(tmphome, (void *)from)
			    : NULL)),

     TIF(NUTAG_RETRY_COUNT, retry_count),
     TIF(NUTAG_MAX_SUBSCRIPTIONS, max_subscriptions),

     TIF(NUTAG_SOA_NAME, soa_name),
     TIF(NUTAG_MEDIA_ENABLE, media_enable),
     TIF(NUTAG_ENABLEINVITE, invite_enable),
     TIF(NUTAG_AUTOALERT, auto_alert),
     TIF(NUTAG_EARLY_ANSWER, early_answer),
     TIF(NUTAG_EARLY_MEDIA, early_media),
     TIF(NUTAG_ONLY183_100REL, only183_100rel),
     TIF(NUTAG_AUTOANSWER, auto_answer),
     TIF(NUTAG_AUTOACK, auto_ack),
     TIF(NUTAG_INVITE_TIMER, invite_timeout),

     TIFD(NUTAG_SESSION_TIMER, session_timer),
     TIF(NUTAG_MIN_SE, min_se),
     TIFD(NUTAG_SESSION_REFRESHER, refresher),
     TIF(NUTAG_UPDATE_REFRESH, update_refresh),
     TIF(NUTAG_REFRESH_WITHOUT_SDP, refresh_without_sdp),

     TIF(NUTAG_ENABLEMESSAGE, message_enable),
     TIF(NUTAG_ENABLEMESSENGER, win_messenger_enable),
     /* TIF(NUTAG_AUTORESPOND, autorespond), */

     TIF(NUTAG_CALLEE_CAPS, callee_caps),
     TIF(NUTAG_MEDIA_FEATURES, media_features),
     TIF(NUTAG_SERVICE_ROUTE_ENABLE, service_route_enable),
     TIF(NUTAG_PATH_ENABLE, path_enable),
     TIF(NUTAG_AUTH_CACHE, auth_cache),
     TIF(NUTAG_REFER_EXPIRES, refer_expires),
     TIF(NUTAG_REFER_WITH_ID, refer_with_id),

     TIF(NUTAG_SUBSTATE, substate),
     TIF(NUTAG_SUB_EXPIRES, sub_expires),

     TIF(SIPTAG_SUPPORTED, supported),
     TIF_STR(SIPTAG_SUPPORTED_STR, supported),
     TIF(SIPTAG_ALLOW, allow),
     TIF_STR(SIPTAG_ALLOW_STR, allow),
     TIF_STR(NUTAG_APPL_METHOD, appl_method),
     TIF(SIPTAG_ALLOW_EVENTS, allow_events),
     TIF_STR(SIPTAG_ALLOW_EVENTS_STR, allow_events),
     TIF_SIP(SIPTAG_USER_AGENT, user_agent),
     TIF(SIPTAG_USER_AGENT_STR, user_agent),
     TIF(NUTAG_USER_AGENT, user_agent),

     TIF_SIP(SIPTAG_ORGANIZATION, organization),
     TIF(SIPTAG_ORGANIZATION_STR, organization),

     TIF(NUTAG_INITIAL_ROUTE, initial_route),
     TIF_STR(NUTAG_INITIAL_ROUTE_STR, initial_route),

     TIF(NUTAG_REGISTRAR, registrar),
     TIF(NUTAG_KEEPALIVE, keepalive),
     TIF(NUTAG_KEEPALIVE_STREAM, keepalive_stream),

     TIF(NUTAG_INSTANCE, instance),
     TIF(NUTAG_M_DISPLAY, m_display),
     TIF(NUTAG_M_USERNAME, m_username),
     TIF(NUTAG_M_PARAMS, m_params),
     TIF(NUTAG_M_FEATURES, m_features),
     TIF(NUTAG_OUTBOUND, outbound),

     /* Handle-specific proxy */
     TAG_IF(nh != dnh && nhp->nhp_set.nhb_proxy,
	    NUTAG_PROXY(nhp->nhp_proxy)),

     /* Skip user-agent-level parameters if parameters are for handle only */
     TAG_IF(nh != dnh, TAG_NEXT(media_params)),

  /* Include tag in the list returned to user
   * if it has been earlier set (by user) */
#define GIF(TAG, pref) \
     TAG_IF(ngp->ngp_set.ngp_##pref, TAG(ngp->ngp_##pref))

     GIF(NUTAG_DETECT_NETWORK_UPDATES, detect_network_updates),
     GIF(NUTAG_SHUTDOWN_EVENTS, shutdown_events),

     NTATAG_CONTACT(m),

#if HAVE_SOFIA_SMIME
     NUTAG_SMIME_ENABLE(nua->sm->sm_enable),
     NUTAG_SMIME_OPT(nua->sm->sm_opt),
     NUTAG_SMIME_PROTECTION_MODE(nua->sm->sm_protection_mode),
     NUTAG_SMIME_MESSAGE_DIGEST(nua->sm->sm_message_digest),
     NUTAG_SMIME_SIGNATURE(nua->sm->sm_signature),
     NUTAG_SMIME_KEY_ENCRYPTION(nua->sm->sm_key_encryption),
     NUTAG_SMIME_MESSAGE_ENCRYPTION(nua->sm->sm_message_encryption),
#endif

     NTATAG_UDP_MTU(udp_mtu),
     NTATAG_MAX_PROCEEDING(max_proceeding),
     NTATAG_SIP_T1(sip_t1),
     NTATAG_SIP_T2(sip_t2),
     NTATAG_SIP_T4(sip_t4),
     NTATAG_SIP_T1X64(sip_t1x64),
     NTATAG_DEBUG_DROP_PROB(debug_drop_prob),
     /* Stack-wide proxy */
     NTATAG_DEFAULT_PROXY(proxy),
     NTATAG_ALIASES(aliases),
     NTATAG_SIPFLAGS(flags),

     TAG_NEXT(media_params));

  nua_stack_event(nua, nh, NULL, nua_r_get_params, SIP_200_OK, lst);

  su_home_deinit(tmphome);

  tl_vfree(media_params);

  return 0;
}
