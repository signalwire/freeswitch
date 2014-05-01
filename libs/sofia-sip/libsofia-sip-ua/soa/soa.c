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

/**@CFILE soa.c
 * @brief Sofia SDP Offer/Answer Engine interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Aug  3 20:27:15 EEST 2005
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_wait.h>

#include "sofia-sip/soa.h"
#include "sofia-sip/sdp.h"
#include "sofia-sip/soa_session.h"
#include "sofia-sip/soa_add.h"

#include <sofia-sip/hostdomain.h>
#include <sofia-sip/bnf.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_localinfo.h>
#include <sofia-sip/su_uniqueid.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/su_errno.h>

#ifndef _MSC_VER
#define NONE ((void *)-1)
#else
#define NONE ((void *)(INT_PTR)-1)
#endif
#define XXX assert(!"implemented")

typedef unsigned longlong ull;

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "soa";
#endif

/* ======================================================================== */

/* Internal prototypes */
enum soa_sdp_kind {
  soa_capability_sdp_kind,
  soa_user_sdp_kind,
  soa_remote_sdp_kind
};

static int soa_set_sdp(soa_session_t *ss,
		       enum soa_sdp_kind what,
		       sdp_session_t const *sdp0,
		       char const *sdp_str, issize_t str_len);

/* ======================================================================== */

#define SOA_VALID_ACTIONS(a)					\
  ((a)->sizeof_soa_session_actions >= (int)sizeof (*actions) && \
   (a)->sizeof_soa_session >= (int)sizeof(soa_session_t) &&     \
   (a)->soa_name != NULL &&					\
   (a)->soa_init != NULL &&					\
   (a)->soa_deinit != NULL &&					\
   (a)->soa_set_params != NULL &&				\
   (a)->soa_get_params != NULL &&				\
   (a)->soa_get_paramlist != NULL &&				\
   (a)->soa_media_features != NULL &&				\
   (a)->soa_sip_require != NULL &&				\
   (a)->soa_sip_supported != NULL &&				\
   (a)->soa_remote_sip_features != NULL &&			\
   (a)->soa_set_capability_sdp != NULL &&			\
   (a)->soa_set_remote_sdp != NULL &&				\
   (a)->soa_set_user_sdp != NULL &&				\
   (a)->soa_generate_offer != NULL &&				\
   (a)->soa_generate_answer != NULL &&				\
   (a)->soa_process_answer != NULL &&				\
   (a)->soa_process_reject != NULL &&				\
   (a)->soa_activate_session != NULL &&				\
   (a)->soa_deactivate_session != NULL &&			\
   (a)->soa_terminate_session != NULL)

/* ======================================================================== */

/**@var SOA_DEBUG
 *
 * Environment variable determining the default debug log level.
 *
 * The SOA_DEBUG environment variable is used to determine the default
 * debug logging level. The normal level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, su_log_global, SOFIA_DEBUG
 */
extern char const SOA_DEBUG[];

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/**Debug log for @b soa module.
 *
 * The soa_log is the log object used by @b soa module. The level of
 * #soa_log is set using #SOA_DEBUG environment variable.
 */
su_log_t soa_log[] = { SU_LOG_INIT("soa", "SOA_DEBUG", SU_DEBUG) };

/* Add " around string */
#define NICE(s) s ? "\"" : "", s ? s : "(nil)", s ? "\"" : ""

/* ======================================================================== */

/* API Functions */

struct soa_namenode
{
  struct soa_namenode const *next;
  char const *basename;
  struct soa_session_actions const *actions;
};

#define SOA_NAMELISTLEN (16)

static struct soa_namenode const soa_default_node =
  {
    NULL, "default", &soa_default_actions
  };

static struct soa_namenode const *soa_namelist = &soa_default_node;

/** Add a named soa backend */
int soa_add(char const *name,
	    struct soa_session_actions const *actions)
{
  struct soa_namenode const *n;
  struct soa_namenode *e;

  SU_DEBUG_9(("soa_add(%s%s%s, %p) called\n", NICE(name), (void *)actions));

  if (name == NULL || actions == NULL)
    return su_seterrno(EFAULT);

  if (!SOA_VALID_ACTIONS(actions))
    return su_seterrno(EINVAL);

  for (n = soa_namelist; n; n = n->next) {
    if (su_casematch(name, n->basename))
      return 0;
  }

  e = malloc(sizeof *e); if (!e) return -1;

  e->next = soa_namelist;
  e->basename = name;
  e->actions = actions;

  soa_namelist = e;

  return 0;
}

/** Search for a named backend */
struct soa_session_actions const *soa_find(char const *name)
{
  SU_DEBUG_9(("soa_find(%s%s%s) called\n", NICE(name)));

  if (name) {
    struct soa_namenode const *n;
    size_t baselen = strcspn(name, ":/");

    for (n = soa_namelist; n; n = n->next) {
      if (su_casenmatch(name, n->basename, baselen))
	break;
    }

    if (n == NULL)
      return (void)su_seterrno(ENOENT), NULL;

    return n->actions;
  }

  return NULL;
}

/* ======================================================================== */

/** Create a soa session. */
soa_session_t *soa_create(char const *name,
			  su_root_t *root,
			  soa_magic_t *magic)
{
  struct soa_session_actions const *actions = &soa_default_actions;

  soa_session_t *ss;
  size_t namelen;

  SU_DEBUG_9(("soa_create(\"%s\", %p, %p) called\n",
	      name ? name : "default", (void *)root, (void *)magic));

  if (name && name[0]) {
    struct soa_namenode const *n;
    size_t baselen = strcspn(name, ":/");

    for (n = soa_namelist; n; n = n->next) {
      if (su_casenmatch(name, n->basename, baselen))
	break;
    }
    if (n == NULL)
      return (void)su_seterrno(ENOENT), NULL;

    actions = n->actions; assert(actions);
  }
  else
    name = "default";

  assert(SOA_VALID_ACTIONS(actions));

  if (root == NULL)
    return (void)su_seterrno(EFAULT), NULL;

  namelen = strlen(name) + 1;

  ss = su_home_new(actions->sizeof_soa_session + namelen);
  if (ss) {
    ss->ss_root = root;
    ss->ss_magic = magic;
    ss->ss_actions = actions;
    ss->ss_name = strcpy((char *)ss + actions->sizeof_soa_session, name);

    /* Calls soa_static_init by default */
    if (ss->ss_actions->soa_init(name, ss, NULL) < 0)
      /* Calls soa_static_deinit by default */
      ss->ss_actions->soa_deinit(ss), ss = NULL;
  }

  return ss;
}

/** Create a copy of a @soa session object. */
soa_session_t *soa_clone(soa_session_t *parent_ss,
			 su_root_t *root,
			 soa_magic_t *magic)
{
  soa_session_t *ss;
  size_t namelen;

  SU_DEBUG_9(("soa_clone(%s::%p, %p, %p) called\n",
	      parent_ss ? parent_ss->ss_actions->soa_name : "",
	      (void *)parent_ss, (void *)root, (void *)magic));

  if (parent_ss == NULL || root == NULL)
    return (void)su_seterrno(EFAULT), NULL;

  namelen = strlen(parent_ss->ss_name) + 1;

  ss = su_home_new(parent_ss->ss_actions->sizeof_soa_session + namelen);
  if (ss) {
    ss->ss_root = root;
    ss->ss_magic = magic;
    ss->ss_actions = parent_ss->ss_actions;
    ss->ss_name = strcpy((char *)ss + ss->ss_actions->sizeof_soa_session,
			 parent_ss->ss_name);

    /* Calls soa_static_init by default */
    if (ss->ss_actions->soa_init(NULL, ss, parent_ss) < 0)
      /* Calls soa_static_deinit by default */
      ss->ss_actions->soa_deinit(ss), ss = NULL;
  }

  return ss;
}

/** Increase reference count */
soa_session_t *soa_session_ref(soa_session_t *ss)
{
  SU_DEBUG_9(("soa_session_ref(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));
  return su_home_ref(ss->ss_home);
}

/** Decrease reference count */
void soa_session_unref(soa_session_t *ss)
{
  SU_DEBUG_9(("soa_session_unref(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));
  su_home_unref(ss->ss_home);
}

/* Initialize session */
int soa_base_init(char const *name,
		     soa_session_t *ss,
		     soa_session_t *parent)
{
  if (parent) {
#define DUP(d, dup, s) if ((s) && !((d) = dup(ss->ss_home, (s)))) return -1
    su_home_t *home = ss->ss_home;

    if (soa_description_dup(home, ss->ss_caps, parent->ss_caps) < 0)
      return -1;
    if (soa_description_dup(home, ss->ss_user, parent->ss_user) < 0)
      return -1;
    if (soa_description_dup(home, ss->ss_local, parent->ss_local) < 0)
      return -1;
    if (soa_description_dup(home, ss->ss_remote, parent->ss_remote) < 0)
      return -1;

    DUP(ss->ss_address, su_strdup, parent->ss_address);
    ss->ss_af = parent->ss_af;
    DUP(ss->ss_hold, su_strdup, parent->ss_hold);

    DUP(ss->ss_cname, su_strdup, parent->ss_cname);

    ss->ss_srtp_enable = parent->ss_srtp_enable;
    ss->ss_srtp_confidentiality = parent->ss_srtp_confidentiality;
    ss->ss_srtp_integrity = parent->ss_srtp_integrity;
  }

  return 0;
}

/** Destroy a session. */
void soa_destroy(soa_session_t *ss)
{
  SU_DEBUG_9(("soa_destroy(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss) {
    ss->ss_active = 0;
    ss->ss_terminated++;
    /* Calls soa_static_deinit() by default. */
    ss->ss_actions->soa_deinit(ss);
    su_home_unref(ss->ss_home);
  }
}

void soa_base_deinit(soa_session_t *ss)
{
  (void)ss;
}

/** Set parameters.
 *
 * @param ss soa session object
 * @param tag, value, ... tagged parameter list
 *
 * @return Number of parameters set, or -1 upon an error.
 *
 * @TAGS
 * SOATAG_CAPS_SDP(),
 * SOATAG_CAPS_SDP_STR(),
 * SOATAG_USER_SDP(),
 * SOATAG_USER_SDP_STR(),
 * SOATAG_REMOTE_SDP(),
 * SOATAG_REMOTE_SDP_STR(),
 * SOATAG_AF(),
 * SOATAG_ADDRESS(),
 * SOATAG_AUDIO_AUX() (currently for "default" only),
 * SOATAG_HOLD(),
 * SOATAG_RTP_SELECT(),
 * SOATAG_RTP_SORT(),
 * SOATAG_RTP_MISMATCH(),
 * SOATAG_SRTP_ENABLE(),
 * SOATAG_SRTP_CONFIDENTIALITY(), and
 * SOATAG_SRTP_INTEGRITY().
 */
int soa_set_params(soa_session_t *ss, tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  int n;

  SU_DEBUG_9(("soa_set_params(%s::%p, ...) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss == NULL)
    return su_seterrno(EFAULT), -1;

  ta_start(ta, tag, value);

  /* Calls soa_static_set_params() by default. */
  n = ss->ss_actions->soa_set_params(ss, ta_args(ta));

  ta_end(ta);

  return n;
}

/**Base method for setting parameters.
 *
 * @param ss   soa session object
 * @param tags  tag item list
 *
 * @return Number of parameters set, or -1 upon an error.
 *
 * @TAGS
 * SOATAG_CAPS_SDP(),
 * SOATAG_CAPS_SDP_STR(),
 * SOATAG_USER_SDP(),
 * SOATAG_USER_SDP_STR(),
 * SOATAG_REMOTE_SDP(),
 * SOATAG_REMOTE_SDP_STR(),
 * SOATAG_AF(),
 * SOATAG_ADDRESS(),
 * SOATAG_HOLD(),
 * SOATAG_RTP_SELECT(),
 * SOATAG_RTP_SORT(),
 * SOATAG_RTP_MISMATCH(),
 * SOATAG_SRTP_ENABLE(),
 * SOATAG_SRTP_CONFIDENTIALITY(), and
 * SOATAG_SRTP_INTEGRITY().
 */
int soa_base_set_params(soa_session_t *ss, tagi_t const *tags)
{
  int n, change_session = 0;

  sdp_session_t const *caps_sdp, *user_sdp;
  char const *caps_sdp_str, *user_sdp_str;

  int af;
  char const *media_address, *hold;
  int rtp_select, rtp_sort;
  int rtp_mismatch;
  int srtp_enable, srtp_confidentiality, srtp_integrity;

  af = ss->ss_af;

  hold = ss->ss_hold;
  media_address = ss->ss_address;

  rtp_select = (int)ss->ss_rtp_select;
  rtp_sort = (int)ss->ss_rtp_sort;
  rtp_mismatch = ss->ss_rtp_mismatch;

  srtp_enable = ss->ss_srtp_enable;
  srtp_confidentiality = ss->ss_srtp_confidentiality;
  srtp_integrity = ss->ss_srtp_integrity;

  caps_sdp = user_sdp = NONE;
  caps_sdp_str = user_sdp_str = NONE;

  n = tl_gets(tags,

	      SOATAG_CAPS_SDP_REF(caps_sdp),
	      SOATAG_CAPS_SDP_STR_REF(caps_sdp_str),

	      SOATAG_USER_SDP_REF(user_sdp),
	      SOATAG_USER_SDP_STR_REF(user_sdp_str),

	      SOATAG_AF_REF(af),
	      SOATAG_ADDRESS_REF(media_address),
	      SOATAG_HOLD_REF(hold),

	      SOATAG_RTP_SELECT_REF(rtp_select),
	      SOATAG_RTP_SORT_REF(rtp_sort),
	      SOATAG_RTP_MISMATCH_REF(rtp_mismatch),

	      SOATAG_SRTP_ENABLE_REF(srtp_enable),
	      SOATAG_SRTP_CONFIDENTIALITY_REF(srtp_confidentiality),
	      SOATAG_SRTP_INTEGRITY_REF(srtp_integrity),

	      TAG_END());

  if (n <= 0)
    return n;

  if (caps_sdp != NONE || caps_sdp_str != NONE) {
    if (caps_sdp == NONE) caps_sdp = NULL;
    if (caps_sdp_str == NONE) caps_sdp_str = NULL;

    if (caps_sdp || caps_sdp_str) {
      if (soa_set_capability_sdp(ss, caps_sdp, caps_sdp_str, -1) < 0) {
	return -1;
      }
    }
    else {
      soa_description_free(ss, ss->ss_caps);
    }
  }

  if (user_sdp != NONE || user_sdp_str != NONE) {
    if (user_sdp == NONE) user_sdp = NULL;
    if (user_sdp_str == NONE) user_sdp_str = NULL;

    if (user_sdp || user_sdp_str) {
      if (soa_set_user_sdp(ss, user_sdp, user_sdp_str, -1) < 0) {
	return -1;
      }
      if (ss->ss_caps->ssd_str == NULL)
	soa_set_capability_sdp(ss, user_sdp, user_sdp_str, -1);
    }
    else {
      soa_description_free(ss, ss->ss_user);
    }
  }

  if (af < SOA_AF_ANY || af > SOA_AF_IP6_IP4)
    af = ss->ss_af;

  if (rtp_select < SOA_RTP_SELECT_SINGLE || rtp_select > SOA_RTP_SELECT_ALL)
    rtp_select = (int)ss->ss_rtp_select;
  if (rtp_sort < SOA_RTP_SORT_DEFAULT || rtp_sort > SOA_RTP_SORT_REMOTE)
    rtp_sort = (int)ss->ss_rtp_sort;
  rtp_mismatch = rtp_mismatch != 0;

  srtp_enable = srtp_enable != 0;
  srtp_confidentiality = srtp_confidentiality != 0;
  srtp_integrity = srtp_integrity != 0;

  change_session
    =  af != (int)ss->ss_af
    || rtp_select != (int)ss->ss_rtp_select
    || rtp_sort != (int)ss->ss_rtp_sort
    || rtp_mismatch != (int)ss->ss_rtp_mismatch
    || srtp_enable != (int)ss->ss_srtp_enable
    || srtp_confidentiality != (int)ss->ss_srtp_confidentiality
    || srtp_integrity != (int)ss->ss_srtp_integrity
    ;

  ss->ss_af = (enum soa_af)af;

  ss->ss_rtp_select = rtp_select;
  ss->ss_rtp_sort = rtp_sort;
  ss->ss_rtp_mismatch = rtp_mismatch;

  ss->ss_srtp_enable = srtp_enable;
  ss->ss_srtp_confidentiality = srtp_confidentiality;
  ss->ss_srtp_integrity = srtp_integrity;

  if (!su_casematch(media_address, ss->ss_address)) {
    char const *addr = ss->ss_address;
    ss->ss_address = su_strdup(ss->ss_home, media_address);
    su_free(ss->ss_home, (void *)addr);
    change_session = 1;
  }

  if (hold == (char const *)1)
    hold = "*";

  if (!su_casematch(hold, ss->ss_hold)) {
    char const *h = ss->ss_hold;
    ss->ss_hold = su_strdup(ss->ss_home, hold);
    su_free(ss->ss_home, (void *)h);
    change_session = 1;
  }

  if (change_session)
    ss->ss_user_version++;

  return n;
}

/** Get tagged parameters.
 *
 * @param ss soa session object
 * @param tag, value, ... tagged parameter list
 *
 * @return Number of parameters get, or -1 upon an error.
 *
 * @TAGS
 * SOATAG_CAPS_SDP(),
 * SOATAG_CAPS_SDP_STR(),
 * SOATAG_USER_SDP(),
 * SOATAG_USER_SDP_STR(),
 * SOATAG_LOCAL_SDP(),
 * SOATAG_LOCAL_SDP_STR(),
 * SOATAG_REMOTE_SDP(),
 * SOATAG_REMOTE_SDP_STR(),
 * SOATAG_AF(),
 * SOATAG_ADDRESS(),
 * SOATAG_AUDIO_AUX() (currently for "default" only),
 * SOATAG_HOLD(),
 * SOATAG_RTP_SELECT(),
 * SOATAG_RTP_SORT(),
 * SOATAG_RTP_MISMATCH(),
 * SOATAG_SRTP_ENABLE(),
 * SOATAG_SRTP_CONFIDENTIALITY(), and
 * SOATAG_SRTP_INTEGRITY().
 */
int soa_get_params(soa_session_t const *ss,
		   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  int n;

  SU_DEBUG_9(("soa_get_params(%s::%p, ...) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss == NULL)
    return su_seterrno(EFAULT), -1;

  ta_start(ta, tag, value);

  /* Calls soa_static_get_params() by default. */
  n = ss->ss_actions->soa_get_params(ss, ta_args(ta));

  ta_end(ta);

  return n;
}

/**Base method for getting tagged parameters.
 *
 * @param ss soa session object
 * @param tags   tag item list
 *
 * @return Number of parameters get, or -1 upon an error.
 *
 * @TAGS
 * SOATAG_CAPS_SDP(),
 * SOATAG_CAPS_SDP_STR(),
 * SOATAG_USER_SDP(),
 * SOATAG_USER_SDP_STR(),
 * SOATAG_LOCAL_SDP(),
 * SOATAG_LOCAL_SDP_STR(),
 * SOATAG_REMOTE_SDP(),
 * SOATAG_REMOTE_SDP_STR(),
 * SOATAG_AF(),
 * SOATAG_ADDRESS(),
 * SOATAG_HOLD(),
 * SOATAG_RTP_SELECT(),
 * SOATAG_RTP_SORT(),
 * SOATAG_RTP_MISMATCH(),
 * SOATAG_SRTP_ENABLE(),
 * SOATAG_SRTP_CONFIDENTIALITY(), and
 * SOATAG_SRTP_INTEGRITY().
 */
int soa_base_get_params(soa_session_t const *ss, tagi_t *tags)
{
  int n;

  n = tl_tgets(tags,
	       SOATAG_CAPS_SDP(ss->ss_caps->ssd_sdp),
	       SOATAG_CAPS_SDP_STR(ss->ss_caps->ssd_str),

	       SOATAG_USER_SDP(ss->ss_user->ssd_sdp),
	       SOATAG_USER_SDP_STR(ss->ss_user->ssd_str),

	       SOATAG_LOCAL_SDP(ss->ss_local->ssd_sdp),
	       SOATAG_LOCAL_SDP_STR(ss->ss_local->ssd_str),

	       SOATAG_REMOTE_SDP(ss->ss_remote->ssd_sdp),
	       SOATAG_REMOTE_SDP_STR(ss->ss_remote->ssd_unparsed),

	       SOATAG_AF(ss->ss_af),
	       SOATAG_ADDRESS(ss->ss_address),
	       SOATAG_HOLD(ss->ss_hold),

	       SOATAG_RTP_SELECT((int)ss->ss_rtp_select),
	       SOATAG_RTP_SORT((int)ss->ss_rtp_sort),
	       SOATAG_RTP_MISMATCH(ss->ss_rtp_mismatch),

	       SOATAG_SRTP_ENABLE(ss->ss_srtp_enable),
	       SOATAG_SRTP_CONFIDENTIALITY(ss->ss_srtp_confidentiality),
	       SOATAG_SRTP_INTEGRITY(ss->ss_srtp_integrity),

	       TAG_END());

  return n;
}

/** Return a list of parameters. */
tagi_t *soa_get_paramlist(soa_session_t const *ss,
			  tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  tagi_t *params = NULL;

  SU_DEBUG_9(("soa_get_paramlist(%s::%p, ...) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss) {
    ta_start(ta, tag, value);
    /* Calls soa_static_get_paramlist() by default. */
    params = ss->ss_actions->soa_get_paramlist(ss, ta_tags(ta));
    ta_end(ta);
  }

  return params;
}


/** Base bethod for getting list of parameters. */
tagi_t *soa_base_get_paramlist(soa_session_t const *ss,
			       tag_type_t tag, tag_value_t value,
			       ...)
{
  ta_list ta;
  tagi_t *params;

  if (ss == NULL)
    return NULL;

  ta_start(ta, tag, value);

  params = tl_llist(
		   TAG_IF(ss->ss_caps->ssd_sdp,
			  SOATAG_CAPS_SDP(ss->ss_caps->ssd_sdp)),
		   TAG_IF(ss->ss_caps->ssd_str,
			  SOATAG_CAPS_SDP_STR(ss->ss_caps->ssd_str)),

		   TAG_IF(ss->ss_user->ssd_sdp,
			  SOATAG_USER_SDP(ss->ss_user->ssd_sdp)),
		   TAG_IF(ss->ss_user->ssd_str,
			  SOATAG_USER_SDP_STR(ss->ss_user->ssd_str)),

		   TAG_IF(ss->ss_local->ssd_sdp,
			  SOATAG_LOCAL_SDP(ss->ss_local->ssd_sdp)),
		   TAG_IF(ss->ss_user->ssd_str,
			  SOATAG_LOCAL_SDP_STR(ss->ss_local->ssd_str)),

		   TAG_IF(ss->ss_remote->ssd_sdp,
			  SOATAG_REMOTE_SDP(ss->ss_remote->ssd_sdp)),
		   TAG_IF(ss->ss_remote->ssd_str,
			  SOATAG_REMOTE_SDP_STR(ss->ss_remote->ssd_unparsed)),

		   SOATAG_AF(ss->ss_af),
		   TAG_IF(ss->ss_address,
			  SOATAG_ADDRESS(ss->ss_address)),

		   SOATAG_SRTP_ENABLE(ss->ss_srtp_enable),
		   SOATAG_SRTP_CONFIDENTIALITY(ss->ss_srtp_confidentiality),
		   SOATAG_SRTP_INTEGRITY(ss->ss_srtp_integrity),

		   ta_tags(ta));

  ta_end(ta);

  return params;
}

#include <sofia-sip/sip_status.h>

/** Convert @soa error to a SIP response code and phrase. */
int soa_error_as_sip_response(soa_session_t *ss,
			      char const **return_phrase)
{
  SU_DEBUG_9(("soa_error_as_sip_response(%s::%p, ...) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss == NULL || ss->ss_status < 400 || ss->ss_status >= 700) {
    if (return_phrase)
      *return_phrase = sip_500_Internal_server_error;
    return 500;
  }

  if (return_phrase)
    *return_phrase = ss->ss_phrase;
  return ss->ss_status;
}

/** Convert @soa error to a SIP @Reason header. */
char const *soa_error_as_sip_reason(soa_session_t *ss)
{
  char const *phrase;
  int status;
  char *reason;

  SU_DEBUG_9(("soa_error_as_sip_reason(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss == NULL)
    return "SIP;cause=500;text=\"Internal Server Error\"";

  status = soa_error_as_sip_response(ss, &phrase);

  reason = su_sprintf(ss->ss_home, "SIP;cause=%u;text=\"%s\"", status, phrase);

  if (ss->ss_reason)
    su_free(ss->ss_home, reason);

  return ss->ss_reason = reason;
}


/** Return SIP @Warning code and text. */
int soa_get_warning(soa_session_t *ss, char const **return_text)
{
  if (!ss)
    return 0;

  if (!ss->ss_wcode)
    return 0;

  if (return_text)
    *return_text = ss->ss_warning;

  return ss->ss_wcode;
}

/** Return SDP description of capabilities.
 *
 * @param ss  pointer to @soa session
 * @param return_sdp      return value for capability SDP structure
 * @param return_sdp_str  return value for capability SDP string
 * @param return_len  return value for length of capability SDP string
 *
 * @retval 0 if there is no description to return
 * @retval 1 if description is returned
 * @retval -1 upon an error
 *
 * @sa @RFC3261 section 11, soa_set_capability_sdp(),
 * SOATAG_CAPS_SDP(), SOATAG_CAPS_SDP_STR(),
 * nua_options(), #nua_i_options
 */
int soa_get_capability_sdp(soa_session_t const *ss,
			   struct sdp_session_s const **return_sdp,
			   char const **return_sdp_str,
			   isize_t *return_len)
{
  sdp_session_t const *sdp;
  char const *sdp_str;

  SU_DEBUG_9(("soa_get_capability_sdp(%s::%p, [%p], [%p], [%p]) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss,
	      (void *)return_sdp, (void *)return_sdp_str, (void *)return_len));

  if (ss == NULL)
    return (void)su_seterrno(EFAULT), -1;

  sdp = ss->ss_caps->ssd_sdp;
  sdp_str = ss->ss_caps->ssd_str;

  if (sdp == NULL)
    return 0;
  if (return_sdp)
    *return_sdp = sdp;
  if (return_sdp_str)
    *return_sdp_str = sdp_str;
  if (return_len)
    *return_len = strlen(sdp_str);

  return 1;
}


/** Set capability SDP.
 *
 * Capability SDP is used instead of user SDP when generating OPTIONS
 * responses describing media capabilities.
 *
 * @param ss  pointer to @soa session
 * @param sdp pointer to SDP session structure
 * @param str pointer to string containing SDP session description
 * @param len lenght of string @a str
 *
 * @retval 1 when SDP is stored and it differs from previously stored
 * @retval 0 when SDP is identical to previously stored one (and user version
 *           returned by soa_get_user_version() is not incremented)
 * @retval -1 upon an error
 *
 * @sa @RFC3261 section 11, soa_get_capability_sdp(),
 * SOATAG_CAPS_SDP(), SOATAG_CAPS_SDP_STR(),
 * nua_options(), #nua_i_options
 */
int soa_set_capability_sdp(soa_session_t *ss,
			   struct sdp_session_s const *sdp,
			   char const *str, issize_t len)
{
  SU_DEBUG_9(("soa_set_capability_sdp(%s::%p, %p, %p, "MOD_ZD") called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, (void *)sdp, (void *)str, (ssize_t)len));

  return soa_set_sdp(ss, soa_capability_sdp_kind, sdp, str, len);
}

/** Set capabilities */
int
soa_base_set_capability_sdp(soa_session_t *ss,
			    sdp_session_t *_sdp,
			    char const *str0, isize_t len0)
{
  sdp_session_t sdp[1];
  sdp_origin_t o[1] = {{ sizeof(o) }};
  sdp_connection_t *c, c0[1] = {{ sizeof(c0) }};
  char c_address[64];
  sdp_time_t t[1] = {{ sizeof(t) }};
  sdp_media_t *m;

  *sdp = *_sdp;

  if (sdp->sdp_origin)
    *o = *sdp->sdp_origin;
  else
    o->o_address = c0;

  if (soa_init_sdp_origin(ss, o, c_address) < 0)
    return -1;

  sdp->sdp_origin = o;

  if (!sdp->sdp_subject)
    sdp->sdp_subject = "-";	/* s=- */

  sdp->sdp_time = t;		/* t=0 0 */

  /* Set port to zero - or should we check that port is already zero? */
  for (m = sdp->sdp_media; m; m = m->m_next)
    m->m_port = 0;

  if (sdp->sdp_connection == NULL) {
    c = sdp->sdp_origin->o_address;

    for (m = sdp->sdp_media; m; m = m->m_next)
      if (m->m_connections == NULL)
	break;
    if (m)
      sdp->sdp_connection = c;
  }

  return soa_description_set(ss, ss->ss_caps, sdp, str0, len0);
}

/**Return user SDP description.
 *
 * <i>User SDP</i> is used as basis for SDP Offer/Answer negotiation. It can
 * be very minimal template, consisting just m= line containing media name,
 * transport protocol, port number and list of supported codecs.
 *
 * The SDP used as an offer or answer (generated by soa_generate_answer() or
 * soa_generate_offer()) is known as <i>local SDP</i> and it is available
 * with soa_get_local_sdp() or SOATAG_LOCAL_SDP()/SOATAG_LOCAL_SDP_STR()
 * tags.
 *
 * @param ss  pointer to @soa session
 * @param return_sdp SDP  session structure return value
 * @param return_sdp_str  return value for pointer to string
 *                        containing the user SDP session description
 * @param return_len  return value for user SDP session description string
 *                    length
 *
 * Any of the parameters @a return_sdp, @a return_sdp_str, or @a return_len
 * may be NULL.
 *
 * @retval 0 if there is no description to return
 * @retval 1 if description is returned
 * @retval -1 upon an error
 *
 * @sa soa_get_local_sdp(), soa_set_user_sdp(), soa_get_user_version(),
 * SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(), soa_get_remote_sdp(),
 * soa_get_capability_sdp()
 */
int soa_get_user_sdp(soa_session_t const *ss,
		     struct sdp_session_s const **return_sdp,
		     char const **return_sdp_str,
		     isize_t *return_len)
{
  sdp_session_t const *sdp;
  char const *sdp_str;

  SU_DEBUG_9(("soa_get_user_sdp(%s::%p, [%p], [%p], [%p]) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss,
			  (void *)return_sdp, (void *)return_sdp_str, (void *)return_len));

  if (ss == NULL)
    return (void)su_seterrno(EFAULT), -1;

  sdp = ss->ss_user->ssd_sdp;
  sdp_str = ss->ss_user->ssd_str;

  if (sdp == NULL)
    return 0;
  if (return_sdp)
    *return_sdp = sdp;
  if (return_sdp_str)
    *return_sdp_str = sdp_str;
  if (return_len)
    *return_len = strlen(sdp_str);

  return 1;
}

/**Returns the version number of user session description. The version
 * numbering starts from zero and it is incremented each time
 * soa_set_user_sdp() or soa_set_params() modifies user SDP.
 *
 * @param ss  pointer to @soa session

 * @return Sequence number of user SDP.
 *
 * @sa soa_set_user_sdp(), soa_get_user_sdp(), soa_set_params(),
 * SOATAG_USER_SDP(), SOATAG_USER_SDP_STR()
 */
int soa_get_user_version(soa_session_t const *ss)
{
  assert(ss != NULL);
  return ss ? (int)ss->ss_user_version : -1;
}

/**Store user SDP to soa session.
 *
 * User SDP is used as basis for SDP Offer/Answer negotiation. It can be
 * very minimal, consisting just m= line containing media name, transport
 * protocol port number and list of supported codecs.
 *
 * The SDP used as an offer or answer (generated by soa_generate_answer() or
 * soa_generate_offer()) is known as <i>local SDP</i> and it is available
 * with soa_get_local_sdp() or SOATAG_LOCAL_SDP()/SOATAG_LOCAL_SDP_STR()
 * tags.
 *
 * @param ss  pointer to @soa session
 * @param sdp pointer to SDP session structure
 * @param str pointer to string containing SDP session description
 * @param len lenght of string @a str
 *
 * Either @a sdp or @a str must be non-NULL. If @a len is -1, length of
 * string @a str is calculated using strlen().
 *
 * @retval 1 when SDP is stored and it differs from previously stored
 * @retval 0 when SDP is identical to previously stored one (and user version
 *           returned by soa_get_user_version() is not incremented)
 * @retval -1 upon an error
 *
 * @sa soa_get_user_sdp(), soa_get_user_version(), soa_set_params(),
 * SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(), soa_generate_offer(),
 * soa_generate_answer(), soa_get_local_sdp(), soa_set_capability_sdp(),
 * soa_set_remote_sdp()
 */
int soa_set_user_sdp(soa_session_t *ss,
		     struct sdp_session_s const *sdp,
		     char const *str, issize_t len)
{
  SU_DEBUG_9(("soa_set_user_sdp(%s::%p, %p, %p, "MOD_ZD") called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, (void *)sdp, (void *)str, (ssize_t)len));

  return soa_set_sdp(ss, soa_user_sdp_kind, sdp, str, len);
}

/** Set user SDP (base version). */
int soa_base_set_user_sdp(soa_session_t *ss,
			  sdp_session_t *sdp, char const *str0, isize_t len0)
{
  ++ss->ss_user_version;
  return soa_description_set(ss, ss->ss_user, sdp, str0, len0);
}

/**Return remote SDP description of the session.
 *
 * <i>Remote SDP</i> is used, together with <i>User SDP</i> as basis for SDP
 * Offer/Answer negotiation.
 *
 * @param ss  pointer to @soa session
 * @param return_sdp SDP  session structure return value
 * @param return_sdp_str  return value for pointer to string
 *                        containing the user SDP session description
 * @param return_len  return value for user SDP session descrioption string
 *                    length
 *
 * Any of the parameters @a return_sdp, @a return_sdp_str, or @a return_len
 * may be NULL.
 *
 * @retval 0 if there is no description to return
 * @retval 1 if description is returned
 * @retval -1 upon an error
 *
 * @sa soa_set_remote_sdp(), soa_get_remote_version(), soa_get_params(),
 * soa_get_paramlist(), SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR(),
 * soa_get_local_sdp(), soa_get_user_sdp(), soa_get_capability_sdp().
 */
int soa_get_remote_sdp(soa_session_t const *ss,
		       struct sdp_session_s const **return_sdp,
		       char const **return_sdp_str,
		       isize_t *return_len)
{
  sdp_session_t const *sdp;
  char const *sdp_str;

  SU_DEBUG_9(("soa_get_remote_sdp(%s::%p, [%p], [%p], [%p]) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss,
			  (void *)return_sdp, (void *)return_sdp_str, (void *)return_len));

  if (ss == NULL)
    return (void)su_seterrno(EFAULT), -1;

  sdp = ss->ss_remote->ssd_sdp;
  sdp_str = ss->ss_remote->ssd_str;

  if (sdp == NULL)
    return 0;
  if (return_sdp)
    *return_sdp = sdp;
  if (return_sdp_str)
    *return_sdp_str = sdp_str;
  if (return_len)
    *return_len = strlen(sdp_str);

  return 1;
}

/**Returns the version number of remote session description. The version
 * numbering starts from zero and it is incremented each time
 * soa_set_remote_sdp() or soa_set_params() modifies remote SDP.
 *
 * @param ss  pointer to @soa session

 * @return Sequence number of remote SDP.
 *
 * @sa soa_set_remote_sdp(), soa_get_remote_sdp(), soa_set_params(),
 * SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR()
 */
int soa_get_remote_version(soa_session_t const *ss)
{
  assert(ss != NULL);
  return ss->ss_remote_version;
}

/** Set remote SDP (offer or answer).
 *
 * <i>Remote SDP</i> is an SDP offer or answer received from the remote end.
 * It is used together with <i>User SDP</i> as basis for SDP Offer/Answer
 * negotiation in soa_generate_answer() or soa_process_answer(). Remote SDP
 * can be set using soa_set_params() and SOATAG_REMOTE_SDP() or
 * SOATAG_REMOTE_SDP_STR() tags, too.
 *
 * If the SDP Offer/Answer negotiation step cannot be completed and the
 * received remote offer or answer should be ignored, call
 * soa_clear_remote_sdp().
 *
 * @param ss  pointer to @soa session
 * @param sdp pointer to SDP session structure
 * @param str pointer to string containing SDP session description
 * @param len lenght of string @a str
 *
 * Either @a sdp or @a str must be non-NULL. If @a len is -1, length of
 * string @a str is calculated using strlen().
 *
 * @retval 1 when SDP is stored and it differs from previously stored
 * @retval 0 when SDP is identical to previously stored one (and remote version
 *           returned by soa_get_remote_version() is not incremented)
 * @retval -1 upon an error
 *
 * @sa soa_has_received_sdp(), soa_get_remote_sdp(),
 * soa_get_remote_version(), soa_set_params(), SOATAG_REMOTE_SDP(),
 * SOATAG_REMOTE_SDP_STR(), soa_generate_answer(), soa_process_answer(),
 * soa_clear_remote_sdp(), soa_init_offer_answer(), soa_get_local_sdp(),
 * soa_set_user_sdp(), soa_set_capability_sdp().
 */
int soa_set_remote_sdp(soa_session_t *ss,
		       struct sdp_session_s const *sdp,
		       char const *str, issize_t len)
{
  SU_DEBUG_9(("soa_set_remote_sdp(%s::%p, %p, %p, "MOD_ZD") called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, (void *)sdp, (void *)str, (ssize_t)len));

  return soa_set_sdp(ss, soa_remote_sdp_kind, sdp, str, len);
}

/** Base method for setting the remote SDP (offer or answer). */
int soa_base_set_remote_sdp(soa_session_t *ss,
			    int new_version,
			    sdp_session_t *sdp, char const *str0, isize_t len0)
{
  /* This is cleared in soa_generate_answer() or soa_process_answer(). */
  ss->ss_unprocessed_remote = 1;

  if (!new_version)
    return 0;

  soa_set_activity(ss, sdp->sdp_media, soa_activity_remote);

  ss->ss_remote_version++;

  return soa_description_set(ss, ss->ss_remote, sdp, str0, len0);
}

/** Clear remote SDP.
 *
 * Remote SDP (offer or answer) should be cleared after a it has been stored
 * in the SDP session object using soa_set_remote_sdp() or soa_set_params(),
 * but the SDP Offer/Answer negotiation step (soa_generate_answer() or
 * soa_process_answer()) cannot be completed.
 *
 * @param ss  pointer to @soa session
 *
 * @retval 0  when successful
 * @retval -1 upon an error
 *
 * @sa soa_init_offer_answer(), soa_set_remote_sdp(),
 * soa_has_received_sdp(), soa_set_params(), SOATAG_REMOTE_SDP(),
 * SOATAG_REMOTE_SDP_STR(), soa_generate_answer(), soa_process_answer(),
 * soa_process_reject().
 */
int soa_clear_remote_sdp(soa_session_t *ss)
{
  SU_DEBUG_9(("soa_clear_remote_sdp(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (!ss)
    return (void)su_seterrno(EFAULT), -1;

  ss->ss_unprocessed_remote = 0;

  return 0;
}

/** Check if remote SDP has been saved but it has not been processed.
 *
 * @sa soa_init_offer_answer(), soa_set_remote_sdp(), soa_generate_answer(),
 * soa_process_answer(), soa_clear_remote_sdp().
 */
int soa_has_received_sdp(soa_session_t const *ss)
{
  return ss && ss->ss_unprocessed_remote;
}


/**Get local SDP.
 *
 * The <i>local SDP</i> is used as an offer or answer and it is generated by
 * soa_generate_offer() or soa_generate_answer(), respectively. It can be
 * retrieved using soa_get_params() or soa_get_paramlist() with
 * SOATAG_LOCAL_SDP() or SOATAG_LOCAL_SDP_STR() tags.
 *
 * @param ss  pointer to @soa session
 * @param return_sdp SDP  session structure return value
 * @param return_sdp_str  return value for pointer to string
 *                        containing the user SDP session description
 * @param return_len  return value for user SDP session descrioption string
 *                    length
 *
 * Any of the parameters @a return_sdp, @a return_sdp_str, or @a return_len
 * may be NULL.
 *
 * @retval 0 if there is no description to return
 * @retval 1 if description is returned
 * @retval -1 upon an error
 *
 * @sa soa_generate_offer(), soa_generate_answer(), soa_get_params(),
 * soa_get_paramlist(), SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR(),
 * soa_get_user_sdp(), soa_get_remote_sdp(), soa_get_capability_sdp().
 */
int soa_get_local_sdp(soa_session_t const *ss,
		      struct sdp_session_s const **return_sdp,
		      char const **return_sdp_str,
		      isize_t *return_len)
{
  sdp_session_t const *sdp;
  char const *sdp_str;

  SU_DEBUG_9(("soa_get_local_sdp(%s::%p, [%p], [%p], [%p]) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss,
			  (void *)return_sdp, (void *)return_sdp_str, (void *)return_len));

  if (ss == NULL)
    return (void)su_seterrno(EFAULT), -1;

  sdp = ss->ss_local->ssd_sdp;
  sdp_str = ss->ss_local->ssd_str;

  if (sdp == NULL)
    return 0;
  if (return_sdp)
    *return_sdp = sdp;
  if (return_sdp_str)
    *return_sdp_str = sdp_str;
  if (return_len)
    *return_len = strlen(sdp_str);

  return 1;
}

/**Initialize the offer/answer state machine.
 *
 * @param ss  pointer to @soa session
 *
 * @retval 0  when successful
 * @retval -1 upon an error
 */
int soa_init_offer_answer(soa_session_t *ss)
{
  int complete;

  SU_DEBUG_9(("soa_init_offer_answer(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (!ss)
    return 0;

  complete = ss->ss_complete;

  ss->ss_complete = 0;
  ss->ss_offer_sent = 0;
  ss->ss_offer_recv = 0;
  ss->ss_answer_sent = 0;
  ss->ss_answer_recv = 0;

  ss->ss_unprocessed_remote = 0;

  return complete;
}

/** Return list of media fetures. */
char **soa_media_features(soa_session_t *ss, int live, su_home_t *home)
{
  SU_DEBUG_9(("soa_media_features(%s::%p, %u, %p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, live, (void *)home));

  if (ss)
    /* Calls soa_base_media_features() by default. */
    return ss->ss_actions->soa_media_features(ss, live, home);
  else
    return (void)su_seterrno(EFAULT), NULL;
}

char **soa_base_media_features(soa_session_t *ss, int live, su_home_t *home)
{
  return su_zalloc(home, 8 * sizeof (char **));
}

char const * const * soa_sip_require(soa_session_t const *ss)
{
  SU_DEBUG_9(("soa_sip_require(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss)
    /* Calls soa_base_sip_require() by default */
    return ss->ss_actions->soa_sip_require(ss);
  else
    return (void)su_seterrno(EFAULT), NULL;
}

char const * const * soa_base_sip_require(soa_session_t const *ss)
{
  static char const *null = NULL;
  return &null;
}

char const * const * soa_sip_supported(soa_session_t const *ss)
{
  SU_DEBUG_9(("soa_sip_supported(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  if (ss)
    /* Calls soa_base_sip_supported() by default */
    return ss->ss_actions->soa_sip_supported(ss);
  else
    return (void)su_seterrno(EFAULT), NULL;
}

char const * const * soa_base_sip_supported(soa_session_t const *ss)
{
  static char const *null = NULL;
  return &null;
}

int soa_remote_sip_features(soa_session_t *ss,
			    char const * const * supported,
			    char const * const * require)
{
  SU_DEBUG_9(("soa_remote_sip_features(%s::%p, %p, %p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, (void *)supported, (void *)require));

  if (ss)
    /* Calls soa_base_remote_sip_features() by default */
    return ss->ss_actions->soa_remote_sip_features(ss, supported, require);
  else
    return (void)su_seterrno(EFAULT), -1;
}

int soa_base_remote_sip_features(soa_session_t *ss,
				    char const * const * supported,
				    char const * const * require)
{
  return 0;
}


/**Generate offer.
 *
 * When generating the offer the user SDP is augmented with the required SDP
 * lines (v=, o=, t=, c=, a=rtpmap, etc.).
 *
 * The resulting SDP is also known as <i>local SDP</i>. It is available with
 * soa_get_local_sdp() or with soa_get_params() and soa_get_paramlist() tags
 * SOATAG_LOCAL_SDP() or SOATAG_LOCAL_SDP_STR().
 *
 * The user SDP has been stored to the soa session with soa_set_user_sdp()
 * or with soa_set_params() tags SOATAG_USER_SDP() or SOATAG_USER_SDP_STR().
 * There are various other parameters directing the generation of offer, set
 * by soa_set_params().
 *
 * @param ss pointer to session object
 * @param always always send offer (even if offer/answer has been completed)
 * @param completed pointer to callback function which is invoked when
 *                  operation is completed (currently not in use)
 *
 * @retval 1 when operation is successful
 * @retval 0 when operation was not needed
 * @retval -1 upon an error
 *
 * @ERRORS
 */
int soa_generate_offer(soa_session_t *ss,
		       int always,
		       soa_callback_f *completed)
{
  SU_DEBUG_9(("soa_generate_offer(%s::%p, %u) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, always));

  /** @ERROR EFAULT Bad address. */
  if (ss == NULL)
    return su_seterrno(EFAULT), -1;

  /** @ERROR EALREADY An operation is already in progress */
  if (ss->ss_in_progress)
    return su_seterrno(EALREADY), -1;

  /** @ERROR EPROTO We have received offer, now we should send answer */
  if (ss->ss_offer_recv && !ss->ss_answer_sent)
    return su_seterrno(EPROTO), -1;

  /** @ERROR EPROTO We have received SDP, but it has not been processed */
  if (soa_has_received_sdp(ss))
    return su_seterrno(EPROTO), -1;

  /** @ERROR EPROTO We have sent an offer, but have received no answer */
  if (ss->ss_offer_sent && !ss->ss_answer_recv)
    return su_seterrno(EPROTO), -1;

  /** @ERROR EPROTO We have received offer. */
  if (ss->ss_unprocessed_remote)
    return su_seterrno(EPROTO), -1;

  /* We should avoid actual operation unless always is true */
  (void)always;  /* We always regenerate offer */

  /* Calls soa_static_generate_offer() by default. */
  return ss->ss_actions->soa_generate_offer(ss, completed);

  /** @sa soa_init_offer_answer(), soa_set_user_sdp(), soa_get_local_sdp(),
   * soa_set_remote_sdp(), soa_process_answer(), soa_process_reject(),
   * soa_generate_answer(), soa_set_params(), soa_get_params(),
   * soa_get_paramlist(), SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(),
   * SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR().
   */
}

int soa_base_generate_offer(soa_session_t *ss,
			    soa_callback_f *completed)
{
  sdp_session_t const *sdp = ss->ss_local->ssd_sdp;

  (void)completed;

  if (!sdp)
    return -1;

  soa_set_activity(ss, sdp->sdp_media, soa_activity_local); /* Wanted activity */

  ss->ss_offer_sent = 1;
  ss->ss_answer_recv = 0;

  return 0;
}

/**Process offer, generate answer.
 *
 * When generating the offer the soa session combines remote offer with
 * <i>user SDP</i>. There are various other parameters directing the
 * generation of answer, set by soa_set_params().
 *
 * Before calling soa_generate_answer() the remote SDP offer should have
 * been stored into the soa session @a ss with soa_set_remote_sdp() or with
 * the soa_set_params() tags SOATAG_REMOTE_SDP() or SOATAG_REMOTE_SDP_STR().
 *
 * Also, the <i>user SDP</i> should have been stored into @a ss with
 * soa_set_user_sdp() or with the soa_set_params() tags SOATAG_USER_SDP() or
 * SOATAG_USER_SDP_STR().
 *
 * The resulting SDP is also known as <i>local SDP</i>. It is available with
 * soa_get_local_sdp() or with the soa_get_params() or soa_get_paramlist()
 * tags SOATAG_LOCAL_SDP() and SOATAG_LOCAL_SDP_STR().
 *
 * @param ss  pointer to session object
 * @param completed  pointer to callback function which is invoked when
 *                   operation is completed (currently not in use)
 *
 * @retval 0 when operation is successful
 * @retval -1 upon an error
 *
 * @ERRORS
 */
int soa_generate_answer(soa_session_t *ss,
			soa_callback_f *completed)
{
  SU_DEBUG_9(("soa_generate_answer(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  /** @ERROR EFAULT Bad address as @a ss. */
  if (ss == NULL)
    return su_seterrno(EFAULT), -1;

  /** @ERROR EALREADY An operation is already in progress. */
  if (ss->ss_in_progress)
    return su_seterrno(EALREADY), -1;

  /** @ERROR EPROTO We have sent an offer, but have received no answer. */
  if (ss->ss_offer_sent && !ss->ss_answer_recv)
    return su_seterrno(EPROTO), -1;

  /** @ERROR EPROTO We have not received offer. */
  if (!ss->ss_unprocessed_remote)
    return su_seterrno(EPROTO), -1;

  /* Calls soa_static_generate_answer() by default. */
  return ss->ss_actions->soa_generate_answer(ss, completed);

  /**@sa soa_init_offer_answer(), soa_set_user_sdp(), soa_set_remote_sdp(),
   * soa_get_local_sdp(), soa_process_reject(), soa_generate_offer(),
   * soa_set_params(), soa_get_params(), soa_get_paramlist(),
   * SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(), SOATAG_REMOTE_SDP(),
   * SOATAG_REMOTE_SDP_STR().
   */
}

/** Base method for processing offer, generating answer. */
int soa_base_generate_answer(soa_session_t *ss,
			     soa_callback_f *completed)
{
  sdp_session_t const *l_sdp = ss->ss_local->ssd_sdp;
  sdp_session_t const *r_sdp = ss->ss_remote->ssd_sdp;
  sdp_session_t *rsession;

  (void)completed;

  if (!l_sdp || !r_sdp)
    return -1;
  rsession = sdp_session_dup(ss->ss_home, r_sdp);
  if (!rsession)
    return -1;

  if (ss->ss_rsession)
    su_free(ss->ss_home, ss->ss_rsession);
  ss->ss_rsession = rsession;

  soa_set_activity(ss, l_sdp->sdp_media, soa_activity_session);

  ss->ss_offer_recv = 1;
  ss->ss_answer_sent = 1;
  ss->ss_complete = 1;
  ss->ss_unprocessed_remote = 0;

  return 0;
}

/** Complete offer-answer after receiving answer.
 *
 * The SDP offer/answer negotiation is completed after receiving answer from
 * remote end. The answer is combined with the offer, and the application
 * should activate the media and codecs according to the negotiation result,
 * available as <i>local SDP</i>.
 *
 * @param ss  pointer to session object
 * @param completed  pointer to callback function which is invoked when
 *                   operation is completed (currently not in use)
 *
 * @retval 1 when operation is successful
 * @retval 0 when operation was not needed
 * @retval -1 upon an error
 *
 * @ERRORS
 */
int soa_process_answer(soa_session_t *ss,
		       soa_callback_f *completed)
{
  SU_DEBUG_9(("soa_process_answer(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  /** @ERROR EFAULT Bad address as @a ss. */
  if (ss == NULL)
    return su_seterrno(EFAULT), -1;

  /** @ERROR EALREADY An operation is already in progress. */
  if (ss->ss_in_progress)
    return su_seterrno(EALREADY), -1;

  /** @ERROR EPROTO We have not sent an offer
      or already have received answer. */
  if (!ss->ss_offer_sent || ss->ss_answer_recv)
    return su_seterrno(EPROTO), -1;

  /** @ERROR EPROTO We have not received answer. */
  if (!ss->ss_unprocessed_remote)
    return su_seterrno(EPROTO), -1;

  /**@sa soa_init_offer_answer(), soa_set_user_sdp(), soa_set_remote_sdp(),
   * soa_get_local_sdp(), soa_generate_offer(), soa_generate_answer(),
   * soa_process_reject(), soa_set_params(), soa_get_params(),
   * soa_get_paramlist(), SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(),
   * SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR().
   */

  /* Calls soa_static_process_answer() by default. */
  return ss->ss_actions->soa_process_answer(ss, completed);
}

/** Base method for completing offer-answer after receiving answer.
 */
int soa_base_process_answer(soa_session_t *ss,
			    soa_callback_f *completed)
{
  sdp_session_t const *l_sdp = ss->ss_local->ssd_sdp;
  sdp_session_t const *r_sdp = ss->ss_remote->ssd_sdp;
  sdp_session_t *rsession;

  (void)completed;

  if (!l_sdp || !r_sdp)
    return -1;
  rsession = sdp_session_dup(ss->ss_home, r_sdp);
  if (!rsession)
    return -1;

  if (ss->ss_rsession)
    su_free(ss->ss_home, ss->ss_rsession);
  ss->ss_rsession = rsession;

  soa_set_activity(ss, l_sdp->sdp_media, soa_activity_session);

  ss->ss_answer_recv = 1;
  ss->ss_complete = 1;
  ss->ss_unprocessed_remote = 0;

  return 0;
}

/** Process rejection of offer.
 *
 * If the SDP offer was rejected (e.g., an offer in re-INVITE asked remote
 * end to add video to the session but the request was rejected), the
 * session should be restored to the state it was before last offer-answer
 * negotation round with soa_process_reject().
 *
 * @param ss  pointer to session object
 * @param completed  pointer to callback function which is invoked when
 *                   operation is completed (currently not in use)
 *
 * @retval 1 when operation is successful
 * @retval 0 when operation was not needed
 * @retval -1 upon an error
 *
 * @ERRORS
 */
int soa_process_reject(soa_session_t *ss,
		       soa_callback_f *completed)
{
  SU_DEBUG_9(("soa_process_reject(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  /** @ERROR EFAULT Bad address as @a ss. */
  if (ss == NULL)
    return su_seterrno(EFAULT), -1;

  /** @ERROR EALREADY An operation is already in progress. */
  if (ss->ss_in_progress)
    return su_seterrno(EALREADY), -1;

  /** @ERROR EPROTO We have not sent an offer
      or already have received answer. */
  if (!ss->ss_offer_sent || ss->ss_answer_recv)
    return su_seterrno(EPROTO), -1;

  /**@sa soa_init_offer_answer(), soa_set_user_sdp(), soa_set_remote_sdp(),
   * soa_get_local_sdp(), soa_generate_offer(), soa_generate_answer(),
   * soa_process_answer(), soa_set_params(), soa_get_params(),
   * soa_get_paramlist(), SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(),
   * SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR().
   */

  /* Calls soa_static_process_reject() by default. */
  return ss->ss_actions->soa_process_reject(ss, completed);
}

/** Base method for processing rejection of offer.
 */
int soa_base_process_reject(soa_session_t *ss,
			    soa_callback_f *completed)
{
  sdp_session_t const *l_sdp = ss->ss_local->ssd_sdp;

  (void)completed;

  ss->ss_offer_sent = 0;

  soa_set_activity(ss, l_sdp ? l_sdp->sdp_media : NULL, soa_activity_session);

  return 0;
}

/** Activate session.
 *
 * Mark soa session as active.
 *
 * @retval 0 when operation was successful
 * @retval -1 upon an error
 *
 * @ERRORS
 */
int soa_activate(soa_session_t *ss, char const *option)
{
  SU_DEBUG_9(("soa_activate(%s::%p, %s%s%s) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, NICE(option)));

  /** @ERROR EFAULT Bad address as @a ss. */
  if (ss == NULL)
    return -1;

  ss->ss_active = 1;

  /* Calls soa_static_activate() by default. */
  return ss->ss_actions->soa_activate_session(ss, option);
}

int soa_base_activate(soa_session_t *ss, char const *option)
{
  (void)ss;
  (void)option;
  return 0;
}

/** Deactivate session.
 *
 * Mark soa session as inactive.
 *
 * @retval 0 when operation was successful
 * @retval -1 upon an error
 *
 * @ERRORS
 */
int soa_deactivate(soa_session_t *ss, char const *option)
{
  SU_DEBUG_9(("soa_deactivate(%s::%p, %s%s%s) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss, NICE(option)));

  /** @ERROR EFAULT Bad address as @a ss. */
  if (ss == NULL)
    return -1;

  ss->ss_active = 0;

  /* Calls soa_static_deactivate() by default. */
  return ss->ss_actions->soa_deactivate_session(ss, option);
}

int soa_base_deactivate(soa_session_t *ss, char const *option)
{
  (void)ss;
  (void)option;
  return 0;
}

/** Terminate session. */
void soa_terminate(soa_session_t *ss, char const *option)
{
  SU_DEBUG_9(("soa_terminate(%s::%p) called\n",
	      ss ? ss->ss_actions->soa_name : "", (void *)ss));

  /** @ERROR EFAULT Bad address as @a ss. */
  if (ss == NULL)
    return;

  ss->ss_active = 0;
  ss->ss_terminated++;

  /* Calls soa_static_terminate() by default. */
  ss->ss_actions->soa_terminate_session(ss, option);
}

void soa_base_terminate(soa_session_t *ss, char const *option)
{
  (void)option;

  soa_init_offer_answer(ss);
  ss->ss_oa_rounds = 0;

  soa_description_free(ss, ss->ss_remote);
  soa_set_activity(ss, NULL, soa_activity_session);
}

/** Return true if the SDP Offer/Answer negotation is complete.
 *
 * The soa_init_offer_answer() clears the completion flag.
 */
int soa_is_complete(soa_session_t const *ss)
{
  return ss && ss->ss_complete;
}

/** Return true if audio has been activated. */
int soa_is_audio_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_local_activity->ma_audio : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if video has been activated. */
int soa_is_video_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_local_activity->ma_video : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if image sharing has been activated. */
int soa_is_image_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_local_activity->ma_image : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if messaging session has been activated. */
int soa_is_chat_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_local_activity->ma_chat : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if remote audio is active (not on hold). */
int soa_is_remote_audio_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_remote_activity->ma_audio : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if remote video is active (not on hold). */
int soa_is_remote_video_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_remote_activity->ma_video : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if image sharing is active (not on hold). */
int soa_is_remote_image_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_remote_activity->ma_image : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/** Return true if chat session is active (not on hold). */
int soa_is_remote_chat_active(soa_session_t const *ss)
{
  int ma = ss ? ss->ss_remote_activity->ma_chat : SOA_ACTIVE_DISABLED;
  if (ma >= 4) ma |= -8;
  return ma;
}

/* ======================================================================== */
/* Methods used by soa instances */

int soa_set_status(soa_session_t *ss, int status, char const *phrase)
{
  if (ss) {
    ss->ss_status = status, ss->ss_phrase = phrase;
    ss->ss_wcode = 0, ss->ss_warning = NULL;
  }
  return -1;
}

int soa_set_warning(soa_session_t *ss, int code, char const *text)
{
  if (ss)
    ss->ss_wcode = code, ss->ss_warning = text;
  return -1;
}

void soa_set_activity(soa_session_t *ss,
		      sdp_media_t const *m,
		      enum soa_activity activity)
{
  struct soa_media_activity *ma;
  sdp_connection_t const *c;
  int mode, swap;
  int l_audio = SOA_ACTIVE_DISABLED, r_audio = SOA_ACTIVE_DISABLED;
  int l_video = SOA_ACTIVE_DISABLED, r_video = SOA_ACTIVE_DISABLED;
  int l_chat = SOA_ACTIVE_DISABLED,  r_chat = SOA_ACTIVE_DISABLED;
  int l_image = SOA_ACTIVE_DISABLED, r_image = SOA_ACTIVE_DISABLED;

  int *l, *r;

  for (; m; m = m->m_next) {
    if (m->m_type == sdp_media_audio)
      l = &l_audio, r = &r_audio;
    else if (m->m_type == sdp_media_video)
      l = &l_video, r = &r_video;
    else if (m->m_type == sdp_media_image)
      l = &l_image, r = &r_image;
    else if (su_casematch(m->m_type_name, "message"))
      l = &l_chat, r = &r_chat;
    else
      continue;

    if (m->m_rejected) {
      if (*l < 0) *l = SOA_ACTIVE_REJECTED;
      if (*r < 0) *r = SOA_ACTIVE_REJECTED;
      continue;
    }

    mode = m->m_mode, swap = ((mode << 1) & 2) | ((mode >> 1) & 1);

    c = sdp_media_connections((sdp_media_t *)m);

    switch (activity) {
    case soa_activity_local:
      *l &= SOA_ACTIVE_SENDRECV;
      *l |= c && c->c_mcast ? swap : mode;
      break;
    case soa_activity_remote:
      *r &= SOA_ACTIVE_SENDRECV;
      *r = c && c->c_mcast ? mode : swap;
      break;
    case soa_activity_session:
      *l &= SOA_ACTIVE_SENDRECV;
      *l |= c && c->c_mcast ? swap : mode;
      *r &= SOA_ACTIVE_SENDRECV;
      *r = c && c->c_mcast ? swap : mode;
      break;
    }
  }

  if (activity == soa_activity_local ||
      activity == soa_activity_session) {
    ma = ss->ss_local_activity;
    ma->ma_audio = l_audio;
    ma->ma_video = l_video;
    ma->ma_image = l_image;
    ma->ma_chat = l_chat;
  }

  if (activity == soa_activity_remote ||
      activity == soa_activity_session) {
    ma = ss->ss_remote_activity;
    ma->ma_audio = r_audio;
    ma->ma_video = r_video;
    ma->ma_image = r_image;
    ma->ma_chat = r_chat;
  }
}

/* ----------------------------------------------------------------------*/
/* Handle SDP */


/**
 * Parses and stores session description
 *
 * @param ss instance pointer
 * @param what caps, local or remote
 * @param sdp0 new sdp (parsed)
 * @param sdp_str new sdp (unparsed)
 * @param str_len length on unparsed data
 **/
static
int soa_set_sdp(soa_session_t *ss,
		enum soa_sdp_kind what,
		sdp_session_t const *sdp0,
		char const *sdp_str, issize_t str_len)
{
  struct soa_description *ssd;
  int flags, new_version, retval;
  sdp_parser_t *parser = NULL;
  sdp_session_t sdp[1];

  if (ss == NULL)
    return -1;

  switch (what) {
  case soa_capability_sdp_kind:
    ssd = ss->ss_caps;
    flags = sdp_f_config;
    break;
  case soa_user_sdp_kind:
    ssd = ss->ss_user;
    flags = sdp_f_config;
    break;
  case soa_remote_sdp_kind:
    ssd = ss->ss_remote;
    flags = sdp_f_mode_0000;
    break;
  default:
    return -1;
  }

  if (sdp0) {
    new_version = sdp_session_cmp(sdp0, ssd->ssd_sdp) != 0;
    sdp_str = NULL, str_len = -1;
  }
  else if (sdp_str) {
    if (str_len == -1)
      str_len = strlen(sdp_str);
    new_version = !su_strnmatch(sdp_str, ssd->ssd_unparsed, str_len + 1);
  }
  else
    return (void)su_seterrno(EINVAL), -1;

  if (!new_version) {
    if (what == soa_remote_sdp_kind) {
      *sdp = *ssd->ssd_sdp;
      /* Calls soa_static_set_remote_sdp() by default */
      return ss->ss_actions->soa_set_remote_sdp(ss, new_version,
						sdp, sdp_str, str_len);
      /* XXX - should check changes by soa_set_remote_sdp */
    }
    return 0;
  }

  if (sdp0) {
    /* note: case 1 - src in parsed form */
    *sdp = *sdp0;
  }
  else /* if (sdp_str) */ {
    /* note: case 2 - src in unparsed form */
    parser = sdp_parse(ss->ss_home, sdp_str, str_len, flags | sdp_f_anynet);

    if (sdp_parsing_error(parser)) {
      sdp_parser_free(parser);
      return soa_set_status(ss, 400, "Bad Session Description");
    }

    *sdp = *sdp_session(parser);
  }

  switch (what) {
  case soa_capability_sdp_kind:
    /* Calls soa_static_set_capability_sdp() by default */
    retval = ss->ss_actions->soa_set_capability_sdp(ss, sdp, sdp_str, str_len);
    break;
  case soa_user_sdp_kind:
    /* Calls soa_static_set_user_sdp() by default */
    retval =  ss->ss_actions->soa_set_user_sdp(ss, sdp, sdp_str, str_len);
    break;
  case soa_remote_sdp_kind:
    /* Calls soa_static_set_remote_sdp() by default */
    retval = ss->ss_actions->soa_set_remote_sdp(ss, 1, sdp, sdp_str, str_len);
    break;
  default:
    retval = soa_set_status(ss, 500, "Internal Error");
    break;
  }

  if (parser)
    sdp_parser_free(parser);

  return retval;
}


/** Set session descriptions. */
int soa_description_set(soa_session_t *ss,
			struct soa_description *ssd,
			sdp_session_t *sdp,
			char const *sdp_str,
			isize_t str_len)
{
  int retval = -1;

  sdp_printer_t *printer = NULL;
  sdp_session_t *sdp_new;
  char *sdp_str_new;
  char *sdp_str0_new;

  void *tbf1, *tbf2, *tbf3, *tbf4;

  /* Store description in three forms: unparsed, parsed and reprinted */

  sdp_new = sdp_session_dup(ss->ss_home, sdp);
  printer = sdp_print(ss->ss_home, sdp, NULL, 0, 0);
  sdp_str_new = (char *)sdp_message(printer);
  if (sdp_str)
    sdp_str0_new = su_strndup(ss->ss_home, sdp_str, str_len);
  else
    sdp_str0_new = sdp_str_new;

  if (sdp_new && printer && sdp_str_new && sdp_str0_new) {
    tbf1 = ssd->ssd_sdp, tbf2 = ssd->ssd_printer;
    tbf3 = (void *)ssd->ssd_str, tbf4 = (void *)ssd->ssd_unparsed;

    ssd->ssd_sdp = sdp_new;
    ssd->ssd_printer = printer;
    ssd->ssd_str = sdp_str_new;
    ssd->ssd_unparsed = sdp_str0_new;

    retval = 1;
  }
  else {
    tbf1 = sdp_new, tbf2 = printer, tbf3 = sdp_str_new, tbf4 = sdp_str0_new;
  }

  su_free(ss->ss_home, tbf1);
  sdp_printer_free(tbf2);
  if (tbf3 != tbf4)
    su_free(ss->ss_home, tbf4);

  return retval;
}


/** Duplicate a session descriptions. */
int soa_description_dup(su_home_t *home,
			struct soa_description *ssd,
			struct soa_description const *ssd0)
{
  if (ssd0->ssd_sdp) {
    ssd->ssd_sdp = sdp_session_dup(home, ssd0->ssd_sdp);
    ssd->ssd_printer = sdp_print(home, ssd->ssd_sdp, NULL, 0, 0);
    ssd->ssd_str = (char *)sdp_message(ssd->ssd_printer);
    if (ssd0->ssd_str != ssd0->ssd_unparsed)
      ssd->ssd_unparsed = su_strdup(home, ssd0->ssd_unparsed);
    else
      ssd->ssd_unparsed = ssd->ssd_str;
  }

  return 0;
}

/** Free session descriptions. */
void soa_description_free(soa_session_t *ss,
			  struct soa_description *ssd)
{
  void *tbf1, *tbf2, *tbf3, *tbf4;

  tbf1 = ssd->ssd_sdp, tbf2 = ssd->ssd_printer;
  tbf3 = (void *)ssd->ssd_str, tbf4 = (void *)ssd->ssd_unparsed;

  memset(ssd, 0, sizeof *ssd);

  su_free(ss->ss_home, tbf1);
  sdp_printer_free(tbf2);
  if (tbf3 != tbf4)
    su_free(ss->ss_home, tbf4);
}

/** Initialize SDP o= line */
int
soa_init_sdp_origin(soa_session_t *ss, sdp_origin_t *o, char buffer[64])
{
  return soa_init_sdp_origin_with_session(ss, o, buffer, NULL);
}


/** Check if c= has valid address
 */
int
soa_check_sdp_connection(sdp_connection_t const *c)
{
  return c != NULL &&
    c->c_nettype &&c->c_address &&
    strcmp(c->c_address, "") &&
    strcmp(c->c_address, "0.0.0.0") &&
    strcmp(c->c_address, "::");
}


/** Initialize SDP o= line with values from @a sdp session. */
int
soa_init_sdp_origin_with_session(soa_session_t *ss,
				 sdp_origin_t *o,
				 char buffer[64],
				 sdp_session_t const *sdp)
{
  if (ss == NULL || o == NULL || buffer == NULL)
    return su_seterrno(EFAULT);

  assert(o->o_address);

  if (!o->o_username)
    o->o_username = "-";

  if (o->o_id == 0)
    su_randmem(&o->o_id, sizeof o->o_id);
  o->o_id &= ((unsigned longlong)1 << 63) - 1;

  if (o->o_version == 0)
    su_randmem(&o->o_version, sizeof o->o_version);
  o->o_version &= ((unsigned longlong)1 << 63) - 1;

  if (!soa_check_sdp_connection(o->o_address) ||
      host_is_local(o->o_address->c_address))
    return soa_init_sdp_connection_with_session(ss, o->o_address, buffer, sdp);

  return 0;
}


/** Obtain a local address for SDP connection structure */
int
soa_init_sdp_connection(soa_session_t *ss,
			sdp_connection_t *c,
			char buffer[64])
{
  return soa_init_sdp_connection_with_session(ss, c, buffer, NULL);
}

static su_localinfo_t const *best_listed_address_in_localinfo(
  su_localinfo_t const *res, char const *address, int ip4, int ip6);
static sdp_connection_t const *best_listed_address_in_session(
  sdp_session_t const *sdp, char const *address0, int ip4, int ip6);
static su_localinfo_t const *best_listed_address(
  su_localinfo_t *li0, char const *address, int ip4, int ip6);


/** Obtain a local address for SDP connection structure.
 *
 * Prefer an address already found in @a sdp.
 */
int
soa_init_sdp_connection_with_session(soa_session_t *ss,
				     sdp_connection_t *c,
				     char buffer[64],
				     sdp_session_t const *sdp)
{
  su_localinfo_t *res, hints[1] = {{ LI_CANONNAME | LI_NUMERIC }}, li0[1];
  su_localinfo_t const *li, *li4, *li6;
  char const *address;
  char const *source = NULL;
  int ip4, ip6, error;
  char abuffer[64];		/* getting value from ss_address */

  if (ss == NULL || c == NULL || buffer == NULL)
    return su_seterrno(EFAULT), -1;

  address = ss->ss_address;

  if (host_is_ip_address(address)) {
    /* Use the application-specified address -
     * do not check that it is found from the local address list */
    c->c_nettype = sdp_net_in;

    if (host_is_ip4_address(address))
      c->c_addrtype = sdp_addr_ip4;
    else
      c->c_addrtype = sdp_addr_ip6;

    if (!host_is_ip6_reference(address)) {
      c->c_address = strcpy(buffer, address);
    }
    else {
      /* Remove brackets [] around the reference */
      size_t len = strlen(address + 1);
      c->c_address = memcpy(buffer, address + 1, len - 1);
      buffer[len - 1] = '\0';
    }

    SU_DEBUG_5(("%s: using SOATAG_ADDRESS(\"%s\")\n", __func__, c->c_address));

    return 0;
  }

  /* XXX - using LI_SCOPE_LINK requires some tweaking */
  hints->li_scope = LI_SCOPE_GLOBAL | LI_SCOPE_SITE /* | LI_SCOPE_LINK */;

  for (res = NULL; res == NULL;) {
    if ((error = su_getlocalinfo(hints, &res)) < 0
	&& error != ELI_NOADDRESS) {
      SU_DEBUG_1(("%s: su_localinfo: %s\n", __func__,
		  su_gli_strerror(error)));
      return -1;
    }
    if (hints->li_scope & LI_SCOPE_HOST)
      break;
    hints->li_scope |= LI_SCOPE_HOST;
  }

  if (c->c_nettype != sdp_net_in ||
      (c->c_addrtype != sdp_addr_ip4 && c->c_addrtype != sdp_addr_ip6)) {
    c->c_nettype = sdp_net_in, c->c_addrtype = (sdp_addrtype_e)0;
    c->c_address = strcpy(buffer, "");
  }

  switch (ss->ss_af) {
  case SOA_AF_IP4_ONLY:
    ip4 = 1, ip6 = 0;
    break;
  case SOA_AF_IP6_ONLY:
    ip6 = 1, ip4 = 0;
    break;
  case SOA_AF_IP4_IP6:
    ip4 = 2, ip6 = 1;
    break;
  case SOA_AF_IP6_IP4:
    ip4 = 1, ip6 = 2;
    break;
  default:
    ip4 = ip6 = 1;
  }

  if (ip4 && ip6) {
    /* Prefer address family already used in session, if any */
    sdp_addrtype_e addrtype = (sdp_addrtype_e)0;
    char const *because = "error";

    if (sdp && sdp->sdp_connection &&
	sdp->sdp_connection->c_nettype == sdp_net_in) {
      addrtype = sdp->sdp_connection->c_addrtype;
      because = "an existing c= line";
    }
    else if (sdp) {
      int mip4 = 0, mip6 = 0;
      sdp_media_t const *m;

      for (m = sdp->sdp_media; m; m = m->m_next) {
	sdp_connection_t const *mc;

	if (m->m_rejected)
	  continue;

	for (mc = m->m_connections; mc; mc = mc->c_next) {
	  if (mc->c_nettype == sdp_net_in) {
	    if (mc->c_addrtype == sdp_addr_ip4)
	      mip4++;
	    else if (mc->c_addrtype == sdp_addr_ip6)
	      mip6++;
	  }
	}
      }

      if (mip4 && mip6)
	/* Mixed. */;
      else if (mip4)
	addrtype = sdp_addr_ip4, because = "an existing c= line under m=";
      else if (mip6)
	addrtype = sdp_addr_ip6, because = "an existing c= line under m=";
    }

    if (addrtype == 0)
      addrtype = c->c_addrtype, because = "the user sdp";

    if (addrtype == sdp_addr_ip4) {
      if (ip6 >= ip4)
	SU_DEBUG_5(("%s: prefer %s because of %s\n", __func__, "IP4", because));
      ip4 = 2, ip6 = 1;
    }
    else if (addrtype == sdp_addr_ip6) {
      if (ip4 >= ip6)
	SU_DEBUG_5(("%s: prefer %s because of %s\n", __func__, "IP4", because));
      ip6 = 2, ip4 = 1;
    }
  }

  li = NULL, li4 = NULL, li6 = NULL;

  if (li == NULL && ss->ss_address) {
    li = best_listed_address_in_localinfo(res, ss->ss_address, ip4, ip6);
    if (li)
      source = "local address from SOATAG_ADDRESS() list";
  }

  if (li == NULL && ss->ss_address && sdp) {
    sdp_connection_t const *c;
    c = best_listed_address_in_session(sdp, ss->ss_address, ip4, ip6);
    if (c) {
      li = memset(li0, 0, sizeof li0);
      if (c->c_addrtype == sdp_addr_ip4)
	li0->li_family = AF_INET;
#if SU_HAVE_IN6
      else
	li0->li_family = AF_INET6;
#endif
      li0->li_canonname = (char *)c->c_address;
      source = "address from SOATAG_ADDRESS() list already in session";
    }
  }

  if (li == NULL && ss->ss_address) {
    memset(li0, 0, sizeof li0);
    li0->li_canonname = abuffer;
    li = best_listed_address(li0, ss->ss_address, ip4, ip6);
    if (li)
      source = "address from SOATAG_ADDRESS() list";
  }

  if (li == NULL) {
    for (li = res; li; li = li->li_next) {
      if (su_casematch(li->li_canonname, c->c_address))
	break;
    }
    if (li)
      source = "the proposed local address";
  }

  /* Check if session-level c= address is local */
  if (li == NULL && sdp && sdp->sdp_connection) {
    for (li = res; li; li = li->li_next) {
      if (!su_casematch(li->li_canonname, sdp->sdp_connection->c_address))
	continue;
#if HAVE_SIN6
      if (li->li_family == AF_INET6) {
	if (ip6 >= ip4)
	  break;
	else if (!li6)
	  li6 = li;		/* Best IP6 address */
      }
#endif
      else if (li->li_family == AF_INET) {
	if (ip4 >= ip6)
	  break;
	else if (!li4)
	  li4 = li;		/* Best IP4 address */
      }
    }

    if (li == NULL && ip4)
      li = li4;
    if (li == NULL && ip6)
      li = li6;
    if (li)
      source = "an existing session-level c= line";
  }

  /* Check for best local media-level c= address */
  if (li == NULL && sdp) {
    sdp_media_t const *m;

    for (m = sdp->sdp_media; m; m = m->m_next) {
      sdp_connection_t const *mc;

      if (m->m_rejected)
	continue;

      for (mc = m->m_connections; mc; mc = mc->c_next) {
	for (li = res; li; li = li->li_next) {
	  if (!su_casematch(li->li_canonname, mc->c_address))
	    continue;
#if HAVE_SIN6
	  if (li->li_family == AF_INET6) {
	    if (ip6 > ip4)
	      break;
	    else if (!li6)
	      li6 = li;		/* Best IP6 address */
	  }
#endif
	  else if (li->li_family == AF_INET) {
	    if (ip4 > ip6)
	      break;
	    else if (!li4)
	      li4 = li;		/* Best IP4 address */
	  }
	}
      }

      if (li)
	break;
    }

    if (li == NULL && ip4)
      li = li4;
    if (li == NULL && ip6)
      li = li6;
    if (li)
      source = "an existing c= address from media descriptions";
  }

  /* Check if o= address is local */
  if (li == NULL && sdp && sdp->sdp_origin) {
    char const *address = sdp->sdp_origin->o_address->c_address;

    for (li = res; li; li = li->li_next) {
      if (!su_casematch(li->li_canonname, address))
	continue;
#if HAVE_SIN6
      if (li->li_family == AF_INET6) {
	if (ip6 >= ip4)
	  break;
	else if (!li6)
	  li6 = li;		/* Best IP6 address */
      }
#endif
      else if (li->li_family == AF_INET) {
	if (ip4 >= ip6)
	  break;
	else if (!li4)
	  li4 = li;		/* Best IP4 address */
      }
    }

    if (li == NULL && ip4)
      li = li4;
    if (li == NULL && ip6)
      li = li6;
    if (li)
      source = "an existing address from o= line";
  }

  if (li == NULL) {
    for (li = res; li; li = li->li_next) {
      if (li->li_family == AF_INET) {
	if (ip4 >= ip6)
	  break;
	else if (!li4)
	  li4 = li;		/* Best IP4 address */
      }
#if HAVE_SIN6
      else if (li->li_family == AF_INET6) {
	if (ip6 >= ip4)
	  break;
	else if (!li6)
	  li6 = li;		/* Best IP6 address */
      }
#endif
    }

    if (li == NULL && ip4)
      li = li4;
    if (li == NULL && ip6)
      li = li6;

    if (li)
      source = "a local address";
  }

  if (li) {
    char const *typename;

    if (li->li_family == AF_INET)
      c->c_nettype = sdp_net_in,  c->c_addrtype = sdp_addr_ip4, typename = "IP4";
#if HAVE_SIN6
    else if (li->li_family == AF_INET6)
      c->c_nettype = sdp_net_in,  c->c_addrtype = sdp_addr_ip6, typename = "IP6";
#endif
    else
      typename = "???";

    assert(strlen(li->li_canonname) < 64);
    c->c_address = strcpy(buffer, li->li_canonname);

    SU_DEBUG_5(("%s: selected IN %s %s (%s)\n", __func__,
		typename, li->li_canonname, source));
  }

  su_freelocalinfo(res);

  if (!li)
    return -1;
  else
    return 0;
}

/* Search for first entry from SOATAG_ADDRESS() list on localinfo list */
static su_localinfo_t const *
best_listed_address_in_localinfo(su_localinfo_t const *res,
				 char const *address,
				 int ip4,
				 int ip6)
{
  su_localinfo_t const  *li = NULL, *best = NULL;
  size_t n;

  SU_DEBUG_3(("%s: searching for %s from list \"%s\"\n",
	      __func__, ip6 && !ip4 ? "IP6 " : !ip6 && ip4 ? "IP4 " : "",
	      address));

  for (; address[0]; address += n + strspn(address, ", ")) {
    n = strcspn(address, ", ");
    if (n == 0)
      continue;

    for (li = res; li; li = li->li_next) {
      if (su_casenmatch(li->li_canonname, address, n) &&
	  li->li_canonname[n] == '\0')
	break;
    }

    if (li == NULL)
      continue;
#if HAVE_SIN6
    else if (li->li_family == AF_INET6) {
      if (ip6 >= ip4)
	return li;
      else if (ip6 && !best)
	best = li;		/* Best IP6 address */
    }
#endif
    else if (li->li_family == AF_INET) {
      if (ip4 >= ip6)
	return li;
      else if (ip4 && !best)
	best = li;		/* Best IP4 address */
    }
  }

  return best;
}

/* Search for first entry from SOATAG_ADDRESS() list in session */
static sdp_connection_t const *
best_listed_address_in_session(sdp_session_t const *sdp,
			       char const *address0,
			       int ip4,
			       int ip6)
{
  sdp_connection_t *c = NULL, *best = NULL;
  sdp_media_t *m;
  char const *address;
  size_t n;

  for (address = address0; address[0]; address += n + strspn(address, ", ")) {
    n = strcspn(address, ", ");
    if (n == 0)
      continue;

    c = sdp->sdp_connection;

    if (c && su_casenmatch(c->c_address, address, n) && c->c_address[n] == 0)
      ;
    else
      for (m = sdp->sdp_media; m; m = m->m_next) {
	if (m->m_connections && m->m_connections != sdp->sdp_connection) {
	  c = sdp->sdp_connection;
	  if (su_casenmatch(c->c_address, address, n) && c->c_address[n] == 0)
	    break;
	  c = NULL;
	}
      }

    if (c == NULL || c->c_nettype != sdp_net_in)
      continue;
#if HAVE_SIN6
    else if (c->c_addrtype == sdp_addr_ip6) {
      if (ip6 >= ip4)
	return c;
      else if (ip6 && !best)
	best = c;		/* Best IP6 address */
    }
#endif
    else if (c->c_addrtype == sdp_addr_ip4) {
      if (ip4 >= ip6)
	return c;
      else if (ip4 && !best)
	best = c;		/* Best IP4 address */
    }
  }

  if (best || sdp->sdp_origin == NULL)
    return best;

  /* Check if address on list is already been used on o= line */
  for (address = address0; address[0]; address += n + strspn(address, ", ")) {
    n = strcspn(address, ", ");
    if (n == 0)
      continue;
    c = sdp->sdp_origin->o_address;

    if (su_casenmatch(c->c_address, address, n) && c->c_address[n] != 0)
      continue;
#if HAVE_SIN6
    else if (c->c_addrtype == sdp_addr_ip6) {
      if (ip6 >= ip4)
	return c;
      else if (ip6 && !best)
	best = c;		/* Best IP6 address */
    }
#endif
    else if (c->c_addrtype == sdp_addr_ip4) {
      if (ip4 >= ip6)
	return c;
      else if (ip4 && !best)
	best = c;		/* Best IP4 address */
    }
  }

  return best;
}

static su_localinfo_t const *
best_listed_address(su_localinfo_t *li0,
		    char const *address,
		    int ip4,
		    int ip6)
{
  size_t n, best = 0;
  char *buffer = (char *)li0->li_canonname;

  for (; address[0]; address += n + strspn(address + n, " ,")) {
    if ((n = span_ip6_address(address))) {
#if SU_HAVE_IN6
      if (ip6 > ip4) {
	li0->li_family = AF_INET6;
	strncpy(buffer, address, n)[n] = '\0';
	return li0;
      }
      else if (ip6 && !best) {
	li0->li_family = AF_INET6;
	strncpy(buffer, address, best = n)[n] = '\0';
      }
#endif
    }
    else if ((n = span_ip4_address(address))) {
      if (ip4 > ip6) {
	li0->li_family = AF_INET;
	strncpy(buffer, address, n)[n] = '\0';
	return li0;
      }
      else if (ip4 && !best) {
	li0->li_family = AF_INET;
	strncpy(buffer, address, best = n)[n] = '\0';
      }
    }
    else {
      n = strcspn(address, " ,");
    }
  }

  if (best)
    return li0;
  else
    return NULL;
}
