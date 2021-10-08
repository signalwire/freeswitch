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

/**@CFILE sresolv.c
 * @brief Sofia DNS Resolver interface using su_root_t.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Teemu Jalava <Teemu.Jalava@nokia.com>
 * @author Mikko Haataja
 *
 * @todo The resolver should allow handling arbitrary records, too.
 */

#include "config.h"

#define SU_TIMER_ARG_T  struct sres_sofia_s
#define SU_WAKEUP_ARG_T struct sres_sofia_register_s
#define SRES_ASYNC_T    struct sres_sofia_s

#include <sofia-sip/sresolv.h>

#define SU_LOG sresolv_log
#include <sofia-sip/su_debug.h>

#include <string.h>
#include <assert.h>

/* ====================================================================== */
/* Glue functions for Sofia root (reactor) */

#define TAG_NAMESPACE "sres"

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>

tag_typedef_t srestag_any = NSTAG_TYPEDEF(*);
tag_typedef_t srestag_resolv_conf = STRTAG_TYPEDEF(resolv_conf);
tag_typedef_t srestag_resolv_conf_ref = REFTAG_TYPEDEF(srestag_resolv_conf);

tag_typedef_t srestag_cache = PTRTAG_TYPEDEF(cache);
tag_typedef_t srestag_cache_ref = REFTAG_TYPEDEF(srestag_cache);

typedef struct sres_sofia_s sres_sofia_t;
typedef struct sres_sofia_register_s sres_sofia_register_t;

struct sres_sofia_register_s {
  sres_sofia_t *reg_ptr;
  su_socket_t reg_socket;
  int reg_index;		/**< Registration index */
};

struct sres_sofia_s {
  sres_resolver_t *srs_resolver;
  su_root_t  	  *srs_root;
  su_timer_t 	  *srs_timer;
  su_socket_t      srs_socket;
  sres_sofia_register_t srs_reg[SRES_MAX_NAMESERVERS];
};

static int sres_sofia_update(sres_sofia_t *,
			     su_socket_t new_socket,
			     su_socket_t old_socket);

static void sres_sofia_timer(su_root_magic_t *magic,
			     su_timer_t *t,
			     sres_sofia_t *arg);

static int sres_sofia_set_timer(sres_sofia_t *srs, unsigned long interval);

static int sres_sofia_poll(su_root_magic_t *, su_wait_t *,
			   sres_sofia_register_t *);

/**Create a resolver.
 *
 * The function sres_resolver_create() is used to allocate and initialize
 * the resolver object using the Sofia asynchronous reactor #su_root_t.
 */
sres_resolver_t *
sres_resolver_create(su_root_t *root,
		     char const *conf_file_path,
		     tag_type_t tag, tag_value_t value, ...)
{
  sres_resolver_t *res;
  sres_sofia_t *srs;
  sres_cache_t *cache = NULL;
  ta_list ta;

  if (root == NULL)
    return su_seterrno(EFAULT), (void *)NULL;

  ta_start(ta, tag, value);
  tl_gets(ta_args(ta),
	  SRESTAG_RESOLV_CONF_REF(conf_file_path),
	  SRESTAG_CACHE_REF(cache),
	  TAG_END());
  ta_end(ta);

  res = sres_resolver_new_with_cache(conf_file_path, cache, NULL);
  srs = res ? su_zalloc(0, sizeof *srs) : NULL;

  if (res && srs) {
    su_timer_t *t;

    srs->srs_resolver = res;
    srs->srs_root = root;
    srs->srs_socket = INVALID_SOCKET;

    sres_resolver_set_async(res, sres_sofia_update, srs, 0);

    t = su_timer_create(su_root_task(root), SRES_RETRANSMIT_INTERVAL);
    srs->srs_timer = t;

    if (!srs->srs_timer)
      SU_DEBUG_3(("sres: cannot create timer\n" VA_NONE));
#if nomore
    else if (su_timer_set_for_ever(t, sres_sofia_timer, srs) < 0)
      SU_DEBUG_3(("sres: cannot set timer\n" VA_NONE));
#else
    else if (sres_resolver_set_timer_cb(res, sres_sofia_set_timer, srs) < 0)
      SU_DEBUG_3(("sres: cannot set timer cb\n" VA_NONE));
#endif
    else
      return res;		/* Success! */

    sres_resolver_destroy(res), res = NULL;
  }

  return res;
}

/** Destroy a resolver object. */
int
sres_resolver_destroy(sres_resolver_t *res)
{
  sres_sofia_t *srs;

  if (res == NULL)
    return su_seterrno(EFAULT);

  srs = sres_resolver_get_async(res, sres_sofia_update);
  if (srs == NULL)
    return su_seterrno(EINVAL);

  /* Remove sockets from too, zap timers. */
  sres_sofia_update(srs, INVALID_SOCKET, INVALID_SOCKET);

  sres_resolver_unref(res);

  return 0;
}

/**Update registered socket.
 *
 * @retval 0 if success
 * @retval -1 upon failure
 */
static int sres_sofia_update(sres_sofia_t *srs,
			     su_socket_t new_socket,
			     su_socket_t old_socket)
{
  char const *what = NULL;
  su_wait_t wait[1];
  sres_sofia_register_t *reg = NULL;
  sres_sofia_register_t *old_reg = NULL;
  int i, index = -1, error = 0;
  int N = SRES_MAX_NAMESERVERS;

  SU_DEBUG_9(("sres_sofia_update(%p, %d, %d)\n",
	      (void *)srs, (int)new_socket, (int)old_socket));

  if (srs == NULL)
    return 0;

  if (srs->srs_root == NULL)
    return -1;

  if (old_socket == new_socket) {
    if (old_socket == INVALID_SOCKET) {
      sres_resolver_set_async(srs->srs_resolver, sres_sofia_update, NULL, 0);
      /* Destroy srs */
      for (i = 0; i < N; i++) {
	if (!srs->srs_reg[i].reg_index)
	  continue;
	su_root_deregister(srs->srs_root, srs->srs_reg[i].reg_index);
	memset(&srs->srs_reg[i], 0, sizeof(srs->srs_reg[i]));
      }
      su_timer_destroy(srs->srs_timer), srs->srs_timer = NULL;
      su_free(NULL, srs);
    }
    return 0;
  }

  if (old_socket != INVALID_SOCKET)
    for (i = 0; i < N; i++)
      if ((srs->srs_reg + i)->reg_socket == old_socket) {
	old_reg = srs->srs_reg + i;
	break;
      }

  if (new_socket != INVALID_SOCKET) {
    if (old_reg == NULL) {
      for (i = 0; i < N; i++) {
	if (!(srs->srs_reg + i)->reg_ptr)
	  break;
      }
      if (i > N)
	return su_seterrno(ENOMEM);

      reg = srs->srs_reg + i;
    }
    else
      reg = old_reg;
  }

  if (reg) {
    if (su_wait_create(wait, new_socket, SU_WAIT_IN | SU_WAIT_ERR) == -1) {
      reg = NULL;
      what = "su_wait_create";
      error = su_errno();
    }

    if (reg)
      index = su_root_register(srs->srs_root, wait, sres_sofia_poll, reg, 0);

    if (index < 0) {
      reg = NULL;
      what = "su_root_register";
      error = su_errno();
      su_wait_destroy(wait);
    }
  }

  if (old_reg) {
    if (old_socket == srs->srs_socket)
      srs->srs_socket = INVALID_SOCKET;
    su_root_deregister(srs->srs_root, old_reg->reg_index);
    memset(old_reg, 0, sizeof *old_reg);
  }

  if (reg) {
    srs->srs_socket = new_socket;

    reg->reg_ptr = srs;
    reg->reg_socket = new_socket;
    reg->reg_index = index;
  }

  if (!what)
    return 0;		/* success */

  SU_DEBUG_3(("sres: %s: %s\n", what, su_strerror(error)));

  return su_seterrno(error);
}


/** Return a socket registered to su_root_t object.
 *
 * @retval sockfd if succesful
 * @retval INVALID_SOCKET (-1) upon an error
 *
 * @ERRORS
 * @ERROR EFAULT Invalid argument passed.
 * @ERROR EINVAL Resolver is not using su_root_t.
 */
su_socket_t sres_resolver_root_socket(sres_resolver_t *res)
{
  sres_sofia_t *srs;
  int i, N = SRES_MAX_NAMESERVERS;

  if (res == NULL)
    return (void)su_seterrno(EFAULT), INVALID_SOCKET;

  srs = sres_resolver_get_async(res, sres_sofia_update);

  if (!srs)
    return su_seterrno(EINVAL);

  if (sres_resolver_set_async(res, sres_sofia_update, srs, 1) == NULL)
    return INVALID_SOCKET;

  if (srs->srs_socket != INVALID_SOCKET)
    return srs->srs_socket;

  for (i = 0; i < N; i++) {
    if (!srs->srs_reg[i].reg_ptr)
      break;
  }

  if (i < N) {
    srs->srs_socket = srs->srs_reg[i].reg_socket;
  }
  else {
    su_socket_t socket;
    if (sres_resolver_sockets(res, &socket, 1) < 0)
      return INVALID_SOCKET;
  }

  return srs->srs_socket;
}


/** Sofia timer wrapper. */
static
void
sres_sofia_timer(su_root_magic_t *magic, su_timer_t *t, sres_sofia_t *srs)
{
  sres_resolver_timer(srs->srs_resolver, -1);
}

/** Sofia timer set wrapper. */
static
int
sres_sofia_set_timer(sres_sofia_t *srs, unsigned long interval)
{
  if (interval > SU_DURATION_MAX)
    interval = SU_DURATION_MAX;
  return su_timer_set_interval(srs->srs_timer, sres_sofia_timer, srs,
			       (su_duration_t)interval);
}


/** Sofia poll/select wrapper, called by su_root_t object */
static
int
sres_sofia_poll(su_root_magic_t *magic,
		su_wait_t *w,
		sres_sofia_register_t *reg)
{
  sres_sofia_t *srs = reg->reg_ptr;
  int retval = 0;
  su_socket_t socket = reg->reg_socket;
  int events = su_wait_events(w, socket);

  if (events & SU_WAIT_ERR)
    retval = sres_resolver_error(srs->srs_resolver, socket);
  if (events & SU_WAIT_IN)
    retval = sres_resolver_receive(srs->srs_resolver, socket);

  return retval;
}
