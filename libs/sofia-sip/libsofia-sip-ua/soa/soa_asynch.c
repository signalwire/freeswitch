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

/**@CFILE soa_asynch.c
 *
 * @brief Static implementation of Sofia SDP Offer/Answer Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 16 17:06:06 EEST 2005
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

struct soa_asynch_complete;

#define SU_MSG_ARG_T struct soa_asynch_completed

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_strlst.h>

#include "sofia-sip/soa.h"
#include <sofia-sip/sdp.h>
#include "sofia-sip/soa_session.h"

#define NONE ((void *)-1)
#define XXX assert(!"implemented")

typedef struct soa_asynch_session
{
  soa_session_t sss_session[1];
}
soa_asynch_session_t;

struct soa_asynch_completed
{
  soa_session_t *completed_session;
  unsigned       completed_terminated;
  int          (*completed_call)(soa_session_t *, soa_callback_f *);
};

static int soa_asynch_init(char const *, soa_session_t *, soa_session_t *);
static void soa_asynch_deinit(soa_session_t *);
static int soa_asynch_set_params(soa_session_t *ss, tagi_t const *tags);
static int soa_asynch_get_params(soa_session_t const *ss, tagi_t *tags);
static tagi_t *soa_asynch_get_paramlist(soa_session_t const *ss);
static int soa_asynch_generate_offer(soa_session_t *ss,
				    soa_callback_f *completed);
static int soa_asynch_generate_answer(soa_session_t *ss,
				     soa_callback_f *completed);
static int soa_asynch_process_answer(soa_session_t *ss,
					    soa_callback_f *completed);
static int soa_asynch_activate(soa_session_t *ss, char const *option);
static int soa_asynch_deactivate(soa_session_t *ss, char const *option);
static void soa_asynch_terminate(soa_session_t *ss, char const *option);

struct soa_session_actions const soa_asynch_actions =
  {
    (sizeof soa_asynch_actions),
    sizeof (struct soa_asynch_session),
    soa_asynch_init,
    soa_asynch_deinit,
    soa_asynch_set_params,
    soa_asynch_get_params,
    soa_asynch_get_paramlist,
    soa_base_media_features,
    soa_base_sip_require,
    soa_base_sip_supported,
    soa_base_remote_sip_features,
    soa_base_set_capability_sdp,
    soa_base_set_remote_sdp,
    soa_base_set_local_sdp,
    soa_asynch_generate_offer,
    soa_asynch_generate_answer,
    soa_asynch_process_answer,
    soa_asynch_activate,
    soa_asynch_deactivate,
    soa_asynch_terminate
  };

/* Initialize session */
static int soa_asynch_init(char const *name,
			   soa_session_t *ss,
			   soa_session_t *parent)
{
  return soa_base_init(name, ss, parent);
}

static void soa_asynch_deinit(soa_session_t *ss)
{
  soa_base_deinit(ss);
}

static int soa_asynch_set_params(soa_session_t *ss, tagi_t const *tags)
{
  return soa_base_set_params(ss, tags);
}

static int soa_asynch_get_params(soa_session_t const *ss, tagi_t *tags)
{
  return soa_base_get_params(ss, tags);
}

static tagi_t *soa_asynch_get_paramlist(soa_session_t const *ss)
{
  return soa_base_get_paramlist(ss);
}

static void soa_asynch_completed(su_root_magic_t *magic,
				 su_msg_r msg,
				 struct soa_asynch_completed *arg)
{
  soa_session_t *ss = arg->completed_session;

  if (arg->completed_terminated == ss->ss_terminated) {
    if (ss->ss_in_progress) {
      soa_callback_f *completed = ss->ss_in_progress;
      ss->ss_in_progress = NULL;

      /* Update local activity */
      if (arg->completed_call(ss, NULL) < 0)
	/* XXX - Process error */;
      
      completed(ss->ss_magic, ss);
    }
  }

  soa_session_unref(ss);
}

static int soa_asynch_generate_offer(soa_session_t *ss,
				     soa_callback_f *completed)
{
  sdp_session_t *sdp;
  sdp_media_t *m;
  uint16_t port = 5004;
  su_msg_r msg;

  if (ss->ss_user->ssd_sdp == NULL) {
    if (ss->ss_caps->ssd_unparsed == NULL)
      return soa_set_status(ss, 500, "No local session available");
  }

  if (ss->ss_user->ssd_sdp)
    return 0;			/* We are done */

  /* Generate a dummy SDP offer based on our capabilities */
  if (soa_set_local_sdp(ss, ss->ss_caps->ssd_unparsed, -1) < 0)
    return -1;
  sdp = ss->ss_user->ssd_sdp; assert(ss->ss_user->ssd_sdp);

  for (m = sdp->sdp_media; m; m = m->m_next)
    if (m->m_port == 0)
      m->m_port = port, port += 2;

  /* We pretend to be asynchronous */
  if (su_msg_create(msg,
		    su_root_task(ss->ss_root),
		    su_root_task(ss->ss_root),
		    soa_asynch_completed,
		    sizeof (struct soa_asynch_completed)) == -1)
    return soa_set_status(ss, 500, "Internal error");

  su_msg_data(msg)->completed_session = soa_session_ref(ss);
  su_msg_data(msg)->completed_terminated = ss->ss_terminated;
  su_msg_data(msg)->completed_call = soa_base_generate_offer;

  su_msg_send(msg);

  ss->ss_in_progress = completed;

  return 1;			/* Indicate caller of async operation */
}

static int soa_asynch_generate_answer(soa_session_t *ss,
				      soa_callback_f *completed)
{
  sdp_session_t *sdp;
  sdp_media_t *m;
  uint16_t port = 5004;
  su_msg_r msg;

  if (ss->ss_user->ssd_sdp == NULL) {
    if (ss->ss_caps->ssd_unparsed == NULL)
      return soa_set_status(ss, 500, "No local session available");
  }

  if (ss->ss_user->ssd_sdp)
    return 0;			/* We are done */

  /* Generate a dummy SDP offer based on our capabilities */
  if (soa_set_local_sdp(ss, ss->ss_caps->ssd_unparsed, -1) < 0)
    return -1;
  sdp = ss->ss_user->ssd_sdp; assert(ss->ss_user->ssd_sdp);

  for (m = sdp->sdp_media; m; m = m->m_next)
    if (m->m_port == 0)
      m->m_port = port, port += 2;

  /* We pretend to be asynchronous */
  if (su_msg_create(msg,
		    su_root_task(ss->ss_root),
		    su_root_task(ss->ss_root),
		    soa_asynch_completed,
		    sizeof (struct soa_asynch_completed)) == -1)
    return soa_set_status(ss, 500, "Internal error");

  su_msg_data(msg)->completed_session = soa_session_ref(ss);
  su_msg_data(msg)->completed_terminated = ss->ss_terminated;
  su_msg_data(msg)->completed_call = soa_base_generate_answer;

  su_msg_send(msg);

  ss->ss_in_progress = completed;

  return 1;			/* Indicate caller of async operation */
}

static int soa_asynch_process_answer(soa_session_t *ss,
					    soa_callback_f *completed)
{
  su_msg_r msg;

  /* We pretend to be asynchronous */
  if (su_msg_create(msg,
		    su_root_task(ss->ss_root),
		    su_root_task(ss->ss_root),
		    soa_asynch_completed,
		    sizeof (struct soa_asynch_completed)) == -1)
    return soa_set_status(ss, 500, "Internal error");

  su_msg_data(msg)->completed_session = soa_session_ref(ss);
  su_msg_data(msg)->completed_terminated = ss->ss_terminated;
  su_msg_data(msg)->completed_call = soa_base_process_answer;

  su_msg_send(msg);

  ss->ss_in_progress = completed;

  return 1;			/* Indicate caller of async operation */
}


static int soa_asynch_activate(soa_session_t *ss, char const *option)
{
  return soa_base_activate(ss, option);
}

static int soa_asynch_deactivate(soa_session_t *ss, char const *option)
{
  return soa_base_deactivate(ss, option);
}

static void soa_asynch_terminate(soa_session_t *ss, char const *option)
{
  ss->ss_in_progress = NULL;
  soa_description_free(ss, ss->ss_user);
  soa_base_terminate(ss, option);
}
