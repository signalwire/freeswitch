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

/**@internal
 * @file auth_plugin.c
 * @brief Plugin interface for authentication verification modules.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr 27 15:23:31 2004 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <sofia-sip/auth_digest.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "auth_plugin";
#endif

#include <sofia-sip/su_debug.h>

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/su_tagarg.h>

#include "sofia-sip/auth_module.h"
#include "sofia-sip/auth_plugin.h"

extern auth_scheme_t auth_scheme_basic[];
extern auth_scheme_t auth_scheme_digest[];
extern auth_scheme_t auth_scheme_delayed[];

enum { N = 32 };

static auth_scheme_t *schemes[N] = {
  auth_scheme_basic,
  auth_scheme_digest,
  auth_scheme_delayed
};

/** Register an authentication plugin.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int auth_mod_register_plugin(auth_scheme_t *asch)
{
  int i;

  for (i = 0; schemes[i]; i++) {
    if (i == N)
      return -1;
  }

  schemes[i] = asch;

  return 0;
}

/**Create an authentication plugin module.
 *
 * The function auth_mod_create() creates a module used to authenticate the
 * requests.
 *
 * @param root pointer to a su_root_t object
 * @param tag,value,... tagged argument list
 *
 * @TAGS
 * AUTHTAG_METHOD(), AUTHTAG_REALM(), AUTHTAG_DB(), AUTHTAG_ALLOW(),
 * AUTHTAG_QOP(), AUTHTAG_ALGORITHM(), AUTHTAG_EXPIRES(),
 * AUTHTAG_BLACKLIST(), AUTHTAG_FORBIDDEN(), AUTHTAG_ANONYMOUS(),
 * AUTHTAG_REMOTE().
 */
auth_mod_t *auth_mod_create(su_root_t *root,
			    tag_type_t tag, tag_value_t value, ...)
{
  auth_mod_t *am = NULL;

  ta_list ta;

  char const *method = NULL;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  AUTHTAG_METHOD_REF(method),
	  TAG_NULL());

  if (method) {
    auth_scheme_t *bscheme = NULL;
    char const *base;
    size_t len;

    base = strrchr(method, '+');
    if (base)
      len = base++ - method;
    else
      len = strlen(method);

    if (base == NULL)
      ;
    else if (su_casematch(base, "Basic"))
      bscheme = auth_scheme_basic;
    else if (su_casematch(base, "Digest"))
      bscheme = auth_scheme_digest;

    if (base == NULL || bscheme) {
      int i;

      for (i = 0; schemes[i] && i < N; i++) {
	if (su_casenmatch(schemes[i]->asch_method, method, len) &&
	    schemes[i]->asch_method[len] == 0) {
	  am = auth_mod_alloc(schemes[i], ta_tags(ta));
	  if (schemes[i]->asch_init(am, bscheme, root, ta_tags(ta)) == -1) {
	    auth_mod_destroy(am), am = NULL;
	  }
	  break;
	}
      }
    }
  }

  ta_end(ta);

  return am;
}
