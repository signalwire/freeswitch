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

/**@CFILE auth_plugin_delayed.c
 *
 * @brief Plugin for delayed authentication.
 *
 * This authentication plugin provides authentication operation that is
 * intentionally delayed. It serves as an example of server-side
 * authentication plugins.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Wed Apr 11 15:14:03 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define SU_MSG_ARG_T struct auth_splugin_t

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "auth_plugin_delayed";
#endif

#include <sofia-sip/su_debug.h>
#include <sofia-sip/su_wait.h>

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_tagarg.h>

#include "sofia-sip/auth_module.h"
#include "sofia-sip/auth_plugin.h"

struct auth_plugin_t
{
  su_root_t      *ap_root;
  auth_scheme_t  *ap_base;
  auth_splugin_t *ap_list;
  auth_splugin_t**ap_tail;
};

/* Digest (or Basic) with delay */

static int delayed_auth_init(auth_mod_t *am,
			     auth_scheme_t *base,
			     su_root_t *root,
			     tag_type_t tag, tag_value_t value, ...);

static void delayed_auth_method(auth_mod_t *am,
				auth_status_t *as,
				msg_auth_t *auth,
				auth_challenger_t const *ach);

static void delayed_auth_challenge(auth_mod_t *am,
				   auth_status_t *as,
				   auth_challenger_t const *ach);

static void delayed_auth_cancel(auth_mod_t *am, auth_status_t *as);

static void delayed_auth_destroy(auth_mod_t *am);

auth_scheme_t auth_scheme_delayed[1] =
  {{
      "Delayed",
      sizeof (struct { auth_mod_t mod[1]; auth_plugin_t plug[1]; }),
      delayed_auth_init,
      delayed_auth_method,
      delayed_auth_challenge,
      delayed_auth_cancel,
      delayed_auth_destroy
  }};

static int delayed_auth_init(auth_mod_t *am,
			     auth_scheme_t *base,
			     su_root_t *root,
			     tag_type_t tag, tag_value_t value, ...)
{
  auth_plugin_t *ap = AUTH_PLUGIN(am);
  int retval = -1;
  ta_list ta;

  ta_start(ta, tag, value);

  if (root && base && auth_init_default(am, base, root, ta_tags(ta)) != -1) {
    ap->ap_root = root;
    ap->ap_base = base;
    ap->ap_tail = &ap->ap_list;

    retval = 0;
  }

  ta_end(ta);

  return retval;
}

struct auth_splugin_t
{
  void const      *asp_cookie;
  auth_splugin_t  *asp_next;
  auth_splugin_t **asp_prev;
  auth_mod_t      *asp_am;
  auth_status_t   *asp_as;
  msg_auth_t      *asp_header;
  auth_challenger_t const *asp_ach;
  int              asp_canceled;
};

/* This is unique identifier */
#define delayed_asp_cookie ((void const *)(intptr_t)delayed_auth_cancel)

static void delayed_auth_method_recv(su_root_magic_t *rm,
				     su_msg_r msg,
				     auth_splugin_t *u);

static void delayed_auth_method(auth_mod_t *am,
				auth_status_t *as,
				msg_auth_t *auth,
				auth_challenger_t const *ach)
{
  auth_plugin_t *ap = AUTH_PLUGIN(am);
  su_msg_r mamc = SU_MSG_R_INIT;
  auth_splugin_t *asp;

  if (su_msg_create(mamc,
		    su_root_task(ap->ap_root),
		    su_root_task(ap->ap_root),
		    delayed_auth_method_recv,
		    sizeof *asp) == SU_FAILURE) {
    as->as_status = 500;
    as->as_phrase = "Asynchronous authentication failure";
    return;
  }

  asp = su_msg_data(mamc); assert(asp);

  asp->asp_cookie = delayed_asp_cookie;
  asp->asp_am = am;
  asp->asp_as = as;
  asp->asp_header = auth;
  asp->asp_ach = ach;
  asp->asp_canceled = 0;

  if (su_msg_send(mamc) == SU_FAILURE) {
    su_msg_destroy(mamc);
    as->as_status = 500;
    as->as_phrase = "Asynchronous authentication failure";
    return;
  }

  as->as_plugin = asp;

  as->as_status = 100;
  as->as_phrase = "Trying";

  return;
}

static void delayed_auth_method_recv(su_root_magic_t *rm,
				     su_msg_r msg,
				     auth_splugin_t *asp)
{
  auth_mod_t *am = asp->asp_am;
  auth_plugin_t *ap = AUTH_PLUGIN(am);

  if (asp->asp_canceled)
    return;

  ap->ap_base->asch_check(am, asp->asp_as, asp->asp_header, asp->asp_ach);

  if (asp->asp_as->as_callback)
    asp->asp_as->as_callback(asp->asp_as->as_magic, asp->asp_as);
}

static void delayed_auth_challenge(auth_mod_t *am,
				   auth_status_t *as,
				   auth_challenger_t const *ach)
{
  auth_plugin_t *ap = AUTH_PLUGIN(am);

  /* Invoke member function of base scheme */
  ap->ap_base->asch_challenge(am, as, ach);
}

static void delayed_auth_cancel(auth_mod_t *am, auth_status_t *as)
{
  auth_plugin_t *ap = AUTH_PLUGIN(am);

  (void)ap;			/* xyzzy */

  if (as->as_plugin && as->as_plugin->asp_cookie == delayed_asp_cookie)
    as->as_plugin->asp_canceled = 1;

  as->as_status = 500, as->as_phrase = "Authentication canceled";
}

static void delayed_auth_destroy(auth_mod_t *am)
{
  auth_plugin_t *ap = AUTH_PLUGIN(am);

  /* Invoke member function of base scheme */
  ap->ap_base->asch_destroy(am);
}
