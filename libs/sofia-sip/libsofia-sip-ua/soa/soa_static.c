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

/**@CFILE soa_static.c
 *
 * @brief Static implementation of Sofia SDP Offer/Answer Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 16 17:06:06 EEST 2005
 *
 * @par Use-cases
 *  1. no existing session
 *    a) generating offer (upgrade with user-SDP)
 *    b) generating answer (upgrade with remote-SDP, rejects with user-SDP)
 *  2. session exists
 *    a) generating offer: 
 *       upgrades with user-SDP
 *    b) generating answer: 
 *       upgrades with remote-SDP, rejects with user-SDP
 *    c) processing answer: 
 *       rejects with user-SDP, no upgrades
 *
 * Upgrading session with user SDP:
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

struct soa_static_complete;

#define SU_MSG_ARG_T struct soa_static_completed

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/string0.h>
#include <sofia-sip/bnf.h>

#include "sofia-sip/soa.h"
#include <sofia-sip/sdp.h>
#include "sofia-sip/soa_session.h"

#define NONE ((void *)-1)
#define XXX assert(!"implemented")

#if !HAVE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#endif

typedef struct soa_static_session
{
  soa_session_t sss_session[1];
  char *sss_audio_aux;
}
soa_static_session_t;

static int soa_static_init(char const *, soa_session_t *, soa_session_t *);
static void soa_static_deinit(soa_session_t *);
static int soa_static_set_params(soa_session_t *ss, tagi_t const *tags);
static int soa_static_get_params(soa_session_t const *ss, tagi_t *tags);
static tagi_t *soa_static_get_paramlist(soa_session_t const *ss,
					tag_type_t tag, tag_value_t value, 
					...);
static int soa_static_set_capability_sdp(soa_session_t *ss, 
				       sdp_session_t *sdp,
				       char const *, isize_t);
static int soa_static_set_remote_sdp(soa_session_t *ss, 
				   int new_version,
				   sdp_session_t *sdp,
				   char const *, isize_t);
static int soa_static_set_user_sdp(soa_session_t *ss, 
				   sdp_session_t *sdp,
				   char const *, isize_t);
static int soa_static_generate_offer(soa_session_t *ss, soa_callback_f *);
static int soa_static_generate_answer(soa_session_t *ss, soa_callback_f *);
static int soa_static_process_answer(soa_session_t *ss, soa_callback_f *);
static int soa_static_process_reject(soa_session_t *ss, soa_callback_f *);

static int soa_static_activate(soa_session_t *ss, char const *option);
static int soa_static_deactivate(soa_session_t *ss, char const *option);
static void soa_static_terminate(soa_session_t *ss, char const *option);

struct soa_session_actions const soa_default_actions = 
  {
    (sizeof soa_default_actions),
    sizeof (struct soa_static_session),
    "static",
    soa_static_init,
    soa_static_deinit,
    soa_static_set_params,
    soa_static_get_params,
    soa_static_get_paramlist,
    soa_base_media_features,
    soa_base_sip_require,
    soa_base_sip_supported,
    soa_base_remote_sip_features,
    soa_static_set_capability_sdp,
    soa_static_set_remote_sdp,
    soa_static_set_user_sdp,
    soa_static_generate_offer,
    soa_static_generate_answer,
    soa_static_process_answer,
    soa_static_process_reject,
    soa_static_activate,
    soa_static_deactivate,
    soa_static_terminate
  };

/* Initialize session */
static int soa_static_init(char const *name,
			   soa_session_t *ss,
			   soa_session_t *parent)
{
  return soa_base_init(name, ss, parent);
}

static void soa_static_deinit(soa_session_t *ss)
{
  soa_base_deinit(ss);
}

static int soa_static_set_params(soa_session_t *ss, tagi_t const *tags)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;
  char const *audio_aux = sss->sss_audio_aux;
  int n, m;

  n = tl_gets(tags,
	      SOATAG_AUDIO_AUX_REF(audio_aux),
	      TAG_END());

  if (n > 0 && str0casecmp(audio_aux, sss->sss_audio_aux)) {
    char *s = su_strdup(ss->ss_home, audio_aux), *tbf = sss->sss_audio_aux;
    if (s == NULL && audio_aux != NULL)
      return -1;
    sss->sss_audio_aux = s;
    if (tbf)
      su_free(ss->ss_home, tbf);
  }

  m = soa_base_set_params(ss, tags);
  if (m < 0)
    return m;

  return n + m;
}

static int soa_static_get_params(soa_session_t const *ss, tagi_t *tags)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;

  int n, m;

  n = tl_tgets(tags,
	       SOATAG_AUDIO_AUX(sss->sss_audio_aux),
	       TAG_END());
  m = soa_base_get_params(ss, tags);
  if (m < 0)
    return m;

  return n + m;
}

static tagi_t *soa_static_get_paramlist(soa_session_t const *ss,
					tag_type_t tag, tag_value_t value, 
					...)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;

  ta_list ta;
  tagi_t *tl;

  ta_start(ta, tag, value);

  tl = soa_base_get_paramlist(ss,
			      TAG_IF(sss->sss_audio_aux,
				     SOATAG_AUDIO_AUX(sss->sss_audio_aux)),
			      TAG_NEXT(ta_args(ta)));

  ta_end(ta);

  return tl;
}

static int soa_static_set_capability_sdp(soa_session_t *ss, 
					 sdp_session_t *sdp,
					 char const *sdp_str, 
					 isize_t sdp_len)
{
  return soa_base_set_capability_sdp(ss, sdp, sdp_str, sdp_len);
}


static int soa_static_set_remote_sdp(soa_session_t *ss, 
				     int new_version,
				     sdp_session_t *sdp,
				     char const *sdp_str, 
				     isize_t sdp_len)
{
  return soa_base_set_remote_sdp(ss, new_version, sdp, sdp_str, sdp_len);
}


static int soa_static_set_user_sdp(soa_session_t *ss, 
				   sdp_session_t *sdp,
				   char const *sdp_str, 
				   isize_t sdp_len)
{
  return soa_base_set_user_sdp(ss, sdp, sdp_str, sdp_len);
}

/** Generate a rejected m= line */
sdp_media_t *soa_sdp_make_rejected_media(su_home_t *home, 
					 sdp_media_t const *m,
					 sdp_session_t *sdp,
					 int include_all_codecs)
{
  sdp_media_t rejected[1] = {{ sizeof (rejected) }};

  rejected->m_type = m->m_type;
  rejected->m_type_name = m->m_type_name;
  rejected->m_port = 0;
  rejected->m_proto = m->m_proto;
  rejected->m_proto_name = m->m_proto_name;

  if (include_all_codecs) {
    rejected->m_rtpmaps = m->m_rtpmaps;
  }

  rejected->m_rejected = 1;

  return sdp_media_dup(home, rejected, sdp);
}

/** Expand a @a truncated SDP.
 */
sdp_session_t *soa_sdp_expand_media(su_home_t *home,
				    sdp_session_t const *truncated,
				    sdp_session_t const *complete)
{
  sdp_session_t *expanded;
  sdp_media_t **m0;
  sdp_media_t * const *m1;

  expanded = sdp_session_dup(home, truncated);

  if (expanded) {
    for (m0 = &expanded->sdp_media, m1 = &complete->sdp_media;
	 *m1;
	 m1 = &(*m1)->m_next) {
      if (!*m0) {
	*m0 = soa_sdp_make_rejected_media(home, *m1, expanded, 0);
	if (!*m0)
	  return NULL;
      }
      m0 = &(*m0)->m_next;
    }
  }

  return expanded;
}

/** Check if @a session should be upgraded with @a remote */ 
int soa_sdp_upgrade_is_needed(sdp_session_t const *session,
			      sdp_session_t const *remote)
{
  sdp_media_t const *rm, *lm;

  if (!remote)
    return 0;
  if (!session)
    return 1;

  for (rm = remote->sdp_media, lm = session->sdp_media; 
       rm && lm ; rm = rm->m_next, lm = lm->m_next) {
    if (rm->m_rejected)
      continue;
    if (lm->m_rejected)
      break;
  }

  return rm != NULL;
}

/** Check if codec is in auxiliary list */
int soa_sdp_is_auxiliary_codec(sdp_rtpmap_t const *rm, char const *auxiliary)
{
  char const *codec;
  size_t clen, alen;
  char const *match;

  if (!rm || !rm->rm_encoding || !auxiliary)
    return 0;

  codec = rm->rm_encoding;

  clen = strlen(codec), alen = strlen(auxiliary);

  if (clen > alen)
    return 0;

  for (match = auxiliary;
       (match = strcasestr(match, codec));
       match = match + 1) {
    if (IS_ALPHANUM(match[clen]) || match[clen] == '-')
      continue;
    if (match != auxiliary &&
	(IS_ALPHANUM(match[-1]) || match[-1] == '-'))
      continue;
    return 1;
  }

  return 0;
}


/** Find first matching media in table. */
sdp_media_t *soa_sdp_matching(soa_session_t *ss, 
			      sdp_media_t *mm[],
			      sdp_media_t const *with,
			      int *return_common_codecs)
{
  int i, j = -1;
  sdp_media_t *m;
  sdp_rtpmap_t const *rm;
  soa_static_session_t *sss = (soa_static_session_t *)ss;
  char const *auxiliary;

  auxiliary = with->m_type == sdp_media_audio ? sss->sss_audio_aux : NULL;

  /* Looking for a single codec */
  if (with->m_rtpmaps && with->m_rtpmaps->rm_next == NULL)
    auxiliary = NULL;

  for (i = 0; mm[i]; i++) {
    if (!sdp_media_match_with(mm[i], with))
      continue;
    
    if (!sdp_media_uses_rtp(with))
      break;

    if (!return_common_codecs)
      break;

    /* Check also rtpmaps  */
    for (rm = mm[i]->m_rtpmaps; rm; rm = rm->rm_next) {
      /* Ignore auxiliary codecs */
      if (auxiliary && soa_sdp_is_auxiliary_codec(rm, auxiliary))
	continue;

      if (sdp_rtpmap_find_matching(with->m_rtpmaps, rm))
	break;
    }
    if (rm)
      break;
    if (j == -1)
      j = i;
  }

  if (return_common_codecs)
    *return_common_codecs = mm[i] != NULL;

  if (mm[i] == NULL && j != -1)
    i = j;			/* return m= line without common codecs */

  m = mm[i];

  for (; mm[i]; i++)
    mm[i] = mm[i + 1];

  return m;
}

/** Set payload types in @a l_m according to the values in @a r_m.
 * 
 * @retval number of common codecs
 */
int soa_sdp_set_rtpmap_pt(sdp_media_t *l_m, 
			  sdp_media_t const *r_m)
{
  sdp_rtpmap_t *lrm, **next_lrm;
  sdp_rtpmap_t const *rrm;

  int local_codecs = 0, common_codecs = 0;

  unsigned char dynamic_pt[128];
  unsigned pt;

  for (next_lrm = &l_m->m_rtpmaps; (lrm = *next_lrm); ) {
    if (lrm->rm_any) {
      /* Remove codecs known only by pt number */
      *next_lrm = lrm->rm_next;
      continue;
    }
    else {
      next_lrm = &lrm->rm_next;
    }

    local_codecs++;

    rrm = sdp_rtpmap_find_matching(r_m->m_rtpmaps, lrm);

    /* XXX - do fmtp comparison */

    if (rrm) {
      /* Use same payload type as remote */
      if (lrm->rm_pt != rrm->rm_pt) {
	lrm->rm_predef = 0;
	lrm->rm_pt = rrm->rm_pt;
      }
      common_codecs++;
    }
    else {
      /* Determine payload type later */
      lrm->rm_any = 1;
    }
  }
  
  if (local_codecs == common_codecs)
    return common_codecs;

  /* Select unique dynamic payload type for each payload */

  memset(dynamic_pt, 0, sizeof dynamic_pt);

  for (lrm = l_m->m_rtpmaps; lrm; lrm = lrm->rm_next) {
    if (!lrm->rm_any)
      dynamic_pt[lrm->rm_pt] = 1;
  }

  for (rrm = r_m->m_rtpmaps; rrm; rrm = rrm->rm_next) {
    dynamic_pt[rrm->rm_pt] = 1;
  }

  for (next_lrm = &l_m->m_rtpmaps; (lrm = *next_lrm); ) {
    if (!lrm->rm_any) {
      next_lrm = &lrm->rm_next;
      continue;
    }
    
    lrm->rm_any = 0;

    pt = lrm->rm_pt;

    if (dynamic_pt[pt]) {
      for (pt = 96; pt < 128; pt++)
        if (!dynamic_pt[pt])
          break;
      
      if (pt == 128) {
        for (pt = 0; pt < 128; pt++)
          if (!sdp_rtpmap_well_known[pt] && !dynamic_pt[pt])
            break;
      }

      if (pt == 128)  {
        for (pt = 0; pt < 128; pt++)
          if (!dynamic_pt[pt])
            break;
      }

      if (pt == 128) {
        /* Too many payload types */
        *next_lrm = lrm->rm_next;
        continue;
      }

      lrm->rm_pt = pt;
      lrm->rm_predef = 0;
    }

    dynamic_pt[pt] = 1;
  
    next_lrm = &lrm->rm_next;
  }

  return common_codecs;
}


/** Sort rtpmaps in @a inout_list according to the values in @a rrm.
 *
 * @return Number of common codecs
 */
int soa_sdp_sort_rtpmap(sdp_rtpmap_t **inout_list, 
			sdp_rtpmap_t const *rrm,
			char const *auxiliary)
{
  sdp_rtpmap_t *sorted = NULL, **next = &sorted, **left;
  sdp_rtpmap_t *aux = NULL, **next_aux = &aux;

  int common_codecs = 0;

  assert(inout_list);
  if (!inout_list)
    return 0;

  /* If remote has only single codec, ignore list of auxiliary codecs */
  if (rrm && !rrm->rm_next)
    auxiliary = NULL;

  /* Insertion sort from *inout_list to sorted */
  for (; rrm && *inout_list; rrm = rrm->rm_next) {
    for (left = inout_list; *left; left = &(*left)->rm_next) {
      if (sdp_rtpmap_match(rrm, (*left)))
	break;
    }
    if (!*left)
      continue;

    if (auxiliary && soa_sdp_is_auxiliary_codec(rrm, auxiliary)) {
      *next_aux = *left, next_aux = &(*next_aux)->rm_next;
    }
    else {
      common_codecs++;
      *next = *left; next = &(*next)->rm_next;
    }
    *left = (*left)->rm_next;
  }

  /* Append common auxiliary codecs */
  if (aux)
    *next = aux, next = next_aux;

  /* Append leftover codecs */
  *next = *inout_list;

  *inout_list = sorted;

  return common_codecs;
}


/** Select rtpmaps in @a inout_list according to the values in @a rrm.
 *
 * @return Number of common codecs
 */
int soa_sdp_select_rtpmap(sdp_rtpmap_t **inout_list, 
			  sdp_rtpmap_t const *rrm,
			  char const *auxiliary,
			  int select_single)
{
  sdp_rtpmap_t **left;
  sdp_rtpmap_t *aux = NULL, **next_aux = &aux;

  int common_codecs = 0;

  assert(inout_list);
  if (!inout_list)
    return 0;

  for (left = inout_list; *left; ) {
    if (auxiliary && soa_sdp_is_auxiliary_codec(*left, auxiliary))
      /* Insert into list of auxiliary codecs */
      *next_aux = *left, *left = (*left)->rm_next, 
	next_aux = &(*next_aux)->rm_next;
    else if (!(select_single && common_codecs > 0)
	     && sdp_rtpmap_find_matching(rrm, (*left)))
      /* Select */
      left = &(*left)->rm_next, common_codecs++;
    else
      /* Remove */
      *left = (*left)->rm_next;
  }

  *left = aux, *next_aux = NULL;

  return common_codecs;
}

/** Sort and select rtpmaps within session */ 
int soa_sdp_upgrade_rtpmaps(soa_session_t *ss,
			    sdp_session_t *session,
			    sdp_session_t const *remote)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;
  sdp_media_t *sm;
  sdp_media_t const *rm;

  for (sm = session->sdp_media, rm = remote->sdp_media; 
       sm && rm; 
       sm = sm->m_next, rm = rm->m_next) {
    if (sm->m_rejected)
      continue;
    if (sdp_media_uses_rtp(sm)) {
      int common_codecs = soa_sdp_set_rtpmap_pt(sm, rm);

      char const *auxiliary =
	rm->m_type == sdp_media_audio ? sss->sss_audio_aux : NULL;

      if (ss->ss_rtp_sort == SOA_RTP_SORT_REMOTE || 
	  (ss->ss_rtp_sort == SOA_RTP_SORT_DEFAULT &&
	   rm->m_mode == sdp_recvonly)) {
	soa_sdp_sort_rtpmap(&sm->m_rtpmaps, rm->m_rtpmaps, auxiliary);
      }

      if (common_codecs == 0)
	;
      else if (ss->ss_rtp_select == SOA_RTP_SELECT_SINGLE) {
	soa_sdp_select_rtpmap(&sm->m_rtpmaps, rm->m_rtpmaps, auxiliary, 1);
      }
      else if (ss->ss_rtp_select == SOA_RTP_SELECT_COMMON) {
	soa_sdp_select_rtpmap(&sm->m_rtpmaps, rm->m_rtpmaps, auxiliary, 0);
      }
    }
  }

  return 0;
}


/** Upgrade m= lines within session */ 
int soa_sdp_upgrade(soa_session_t *ss,
		    su_home_t *home,
		    sdp_session_t *session,
		    sdp_session_t const *caps,
		    sdp_session_t const *upgrader)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;

  int Ns, Nc, Nu, size, i, j;
  sdp_media_t *m, **mm, *cm;
  sdp_media_t **s_media, **o_media, **c_media;
  sdp_media_t const **u_media;

  Ns = sdp_media_count(session, sdp_media_any, 0, 0, 0);
  Nc = sdp_media_count(caps, sdp_media_any, 0, 0, 0);
  Nu = sdp_media_count(upgrader, sdp_media_any, 0, 0, 0);

  if (caps == upgrader)
    size = Ns + Nc + 1;
  else if (Ns < Nu)
    size = Nu + 1;
  else
    size = Ns + 1;

  s_media = su_zalloc(home, size * (sizeof *s_media));
  o_media = su_zalloc(home, (Ns + 1) * (sizeof *o_media));
  c_media = su_zalloc(home, (Nc + 1) * (sizeof *c_media));
  u_media = su_zalloc(home, (Nu + 1) * (sizeof *u_media));

  cm = sdp_media_dup_all(home, caps->sdp_media, session); 

  if (!s_media || !c_media || !u_media || !cm)
    return -1;

  for (i = 0, m = session->sdp_media; m && i < Ns; m = m->m_next)
    o_media[i++] = m;
  assert(i == Ns);
  for (i = 0, m = cm; m && i < Nc; m = m->m_next)
    c_media[i++] = m;
  assert(i == Nc);
  for (i = 0, m = upgrader->sdp_media; m && i < Nu; m = m->m_next)
    u_media[i++] = m;
  assert(i == Nu);

  if (caps != upgrader) {
    /* Update session according to remote */
    for (i = 0; i < Nu; i++) {
      int common_codecs = 0;

      m = soa_sdp_matching(ss, c_media, u_media[i], &common_codecs);

      if (!m || u_media[i]->m_rejected) {
	m = soa_sdp_make_rejected_media(home, u_media[i], session, 0);
      }
      else if (sdp_media_uses_rtp(m)) {
	/* Process rtpmaps */
	char const *auxiliary =
	  m->m_type == sdp_media_audio ? sss->sss_audio_aux : NULL;

	if (!common_codecs && !ss->ss_rtp_mismatch)
	  m = soa_sdp_make_rejected_media(home, m, session, 1);
	soa_sdp_set_rtpmap_pt(m, u_media[i]);

	if (ss->ss_rtp_sort == SOA_RTP_SORT_REMOTE || 
	    (ss->ss_rtp_sort == SOA_RTP_SORT_DEFAULT &&
	     u_media[i]->m_mode == sdp_recvonly)) {
	  soa_sdp_sort_rtpmap(&m->m_rtpmaps, u_media[i]->m_rtpmaps, auxiliary);
	}

	if (common_codecs &&
	    (ss->ss_rtp_select == SOA_RTP_SELECT_SINGLE ||
	     ss->ss_rtp_select == SOA_RTP_SELECT_COMMON)) {
	  soa_sdp_select_rtpmap(&m->m_rtpmaps, u_media[i]->m_rtpmaps, auxiliary,
				ss->ss_rtp_select == SOA_RTP_SELECT_SINGLE);
	}
      }

      s_media[i] = m;
    }
  }
  else {
    /* Update session according to local */
    for (i = 0; i < Ns; i++) {
      m = soa_sdp_matching(ss, c_media, o_media[i], NULL);
      if (!m)
	m = soa_sdp_make_rejected_media(home, o_media[i], session, 0);
      s_media[i] = m;
    }
    /* Here we just append new media at the end */
    for (j = 0; c_media[j]; j++)
      s_media[i++] = c_media[j];
    assert(i <= size);
  }

  mm = &session->sdp_media;
  for (i = 0; s_media[i]; i++) {
    m = s_media[i]; *mm = m; mm = &m->m_next;
  }
  *mm = NULL;

  return 0;
}

/** Check if @a session contains media that are rejected by @a remote. */ 
int soa_sdp_reject_is_needed(sdp_session_t const *session,
			     sdp_session_t const *remote)
{
  sdp_media_t const *sm, *rm;

  if (!remote)
    return 1;
  if (!session)
    return 0;

  for (sm = session->sdp_media, rm = remote->sdp_media; 
       sm && rm; sm = sm->m_next, rm = rm->m_next) {
    if (rm->m_rejected) {
      if (!sm->m_rejected)
	return 1;
    }
    else {
      /* Mode bits do not match */
      if (((rm->m_mode & sdp_recvonly) == sdp_recvonly)
	  != ((sm->m_mode & sdp_sendonly) == sdp_sendonly))
	return 1;
    }
  }

  if (sm)
    return 1;

  return 0;
}

/** If m= line is rejected by remote mark m= line rejected within session */ 
int soa_sdp_reject(su_home_t *home,
		   sdp_session_t *session,
		   sdp_session_t const *remote)
{
  sdp_media_t *sm;
  sdp_media_t const *rm;

  if (!session || !session->sdp_media || !remote)
    return 0;

  rm = remote->sdp_media;

  for (sm = session->sdp_media; sm; sm = sm->m_next) {
    if (!rm || rm->m_rejected) {
      sm->m_rejected = 1;
      sm->m_mode = 0;
      sm->m_port = 0;
      sm->m_number_of_ports = 1;
      if (sm->m_format)
	sm->m_format->l_next = NULL;
      if (sm->m_rtpmaps)
	sm->m_rtpmaps->rm_next = NULL;
      sm->m_information = NULL;
      if (sm->m_connections)
	sm->m_connections->c_next = NULL;
      sm->m_bandwidths = NULL;
      sm->m_key = NULL;
      sm->m_attributes = NULL;
      sm->m_user = NULL;
    }

    if (rm)
      rm = rm->m_next;
  }

  return 0;
}

/** Check if @a session mode should be changed. */ 
int soa_sdp_mode_set_is_needed(sdp_session_t const *session,
			       sdp_session_t const *remote,
			       char const *hold)
{
  sdp_media_t const *sm, *rm, *rm_next;
  int hold_all;
  sdp_mode_t recv_mode;

  SU_DEBUG_7(("soa_sdp_mode_set_is_needed(%p, %p, \"%s\"): called\n",
	      (void *)session, (void *)remote, hold ? hold : ""));

  if (!session )
    return 0;

  hold_all = str0cmp(hold, "*") == 0;

  rm = remote ? remote->sdp_media : NULL, rm_next = NULL;

  for (sm = session->sdp_media; sm; sm = sm->m_next, rm = rm_next) {
    rm_next = rm ? rm->m_next : NULL;

    if (sm->m_rejected)
      continue;

    if (rm) {
      /* Mode bits do not match */
      if (((rm->m_mode & sdp_recvonly) == sdp_recvonly)
	  != ((sm->m_mode & sdp_sendonly) == sdp_sendonly))
	return 1;
    }

    recv_mode = sm->m_mode & sdp_recvonly;
    if (recv_mode && hold &&
	(hold_all || strcasestr(hold, sm->m_type_name)))
      return 1;
  }

  return 0;
}


/** Update mode within session */ 
int soa_sdp_mode_set(sdp_session_t *session,
		     sdp_session_t const *remote,
		     char const *hold)
{
  sdp_media_t *sm;
  sdp_media_t const *rm, *rm_next;
  int hold_all;
  sdp_mode_t send_mode, recv_mode;

  SU_DEBUG_7(("soa_sdp_mode_set(%p, %p, \"%s\"): called\n",
	      (void *)session, (void *)remote, hold ? hold : ""));

  if (!session || !session->sdp_media)
    return 0;

  rm = remote ? remote->sdp_media : NULL, rm_next = NULL;

  hold_all = str0cmp(hold, "*") == 0;

  for (sm = session->sdp_media; sm; sm = sm->m_next, rm = rm_next) {
    rm_next = rm ? rm->m_next : NULL;

    if (sm->m_rejected)
      continue;

    send_mode = sdp_sendonly;
    if (rm)
      send_mode = (rm->m_mode & sdp_recvonly) ? sdp_sendonly : 0;

    recv_mode = sm->m_mode & sdp_recvonly;
    if (recv_mode && hold && (hold_all || strcasestr(hold, sm->m_type_name)))
      recv_mode = 0;

    sm->m_mode = recv_mode | send_mode;
  }

  return 0;
}

enum offer_answer_action {
  generate_offer,
  generate_answer,
  process_answer
};

/**
 * Updates the modified copy of local SDP based
 * on application provided local SDP and remote SDP.
 */
static int offer_answer_step(soa_session_t *ss,
			     enum offer_answer_action action,
			     char const *by)
{
  char c_address[64];
  sdp_session_t *local = ss->ss_local->ssd_sdp;
  sdp_session_t local0[1];

  sdp_session_t *user = ss->ss_user->ssd_sdp;
  unsigned user_version = ss->ss_user_version;

  sdp_session_t *remote = ss->ss_remote->ssd_sdp;
  unsigned remote_version = ss->ss_remote_version;

  sdp_origin_t o[1] = {{ sizeof(o) }};
  sdp_connection_t *c, c0[1] = {{ sizeof(c0) }};
  sdp_time_t t[1] = {{ sizeof(t) }};

  char const *phrase = "Internal Media Error";

  su_home_t tmphome[SU_HOME_AUTO_SIZE(8192)];

  su_home_auto(tmphome, sizeof tmphome);

  SU_DEBUG_7(("soa_static_offer_answer_action(%p, %s): called\n",
	      (void *)ss, by));

  if (user == NULL)
    return soa_set_status(ss, 500, "No session set by user");

  if (action == generate_offer)
    remote = NULL;

  /* Pre-negotiation Step: Expand truncated remote SDP */
  if (local && remote) switch (action) {
  case generate_answer:
  case process_answer:
    if (sdp_media_count(remote, sdp_media_any, "*", 0, 0) < 
	sdp_media_count(local, sdp_media_any, "*", 0, 0)) {
      SU_DEBUG_5(("%s: remote %s is truncated: expanding\n",
		  by, action == generate_answer ? "offer" : "answer"));
      remote = soa_sdp_expand_media(tmphome, remote, local);
    }
  default:
    break;
  }
  
  /* Step A: Create local SDP session (based on user-supplied SDP) */
  if (local == NULL) switch (action) {
  case generate_offer:
  case generate_answer:
    SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by,
		"generating local description"));

    local = local0;
    *local = *user, local->sdp_media = NULL;

    if (local->sdp_origin) {
      o->o_username = local->sdp_origin->o_username;
      /* o->o_address = local->sdp_origin->o_address; */
    }
    if (!o->o_address)
      o->o_address = c0; 
    local->sdp_origin = o;

    if (soa_init_sdp_origin(ss, o, c_address) < 0) {
      phrase = "Cannot Get IP Address for Media";
      goto internal_error;
    }

    break;

  case process_answer:
  default:
    goto internal_error;
  }

  /* Step B: upgrade local SDP (add m= lines to it)  */
  switch (action) {
  case generate_offer:
    /* Upgrade local SDP based on user SDP */
    if (local != local0 && ss->ss_local_user_version == user_version)
      break;
    if (local != local0)
      *local0 = *local, local = local0;
    SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by, 
		"upgrade with local description"));
    soa_sdp_upgrade(ss, tmphome, local, user, user);
    break;
  case generate_answer:
    /* Upgrade local SDP based on remote SDP */
    if (ss->ss_local_user_version == user_version &&
	ss->ss_local_remote_version == remote_version)
      break;
    if (1) {
      if (local != local0)
	*local0 = *local, local = local0;
      SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by,
		  "upgrade with remote description"));
      soa_sdp_upgrade(ss, tmphome, local, user, remote);
    }
    break;
  case process_answer:
  default:
    break;
  }


  /* Step C: reject media */
  switch (action) {
  case generate_offer:
    /* Local media is marked as rejected already in upgrade phase */
    break;
  case generate_answer:
  case process_answer:
    if (ss->ss_local_remote_version == remote_version)
      break;
    if (soa_sdp_reject_is_needed(local, remote)) {
      if (local != local0) {
	*local0 = *local, local = local0;
#define DUP_LOCAL(local)					 \
	do {							 \
	  if (!local->sdp_media) break;				 \
	  local->sdp_media =					 \
	    sdp_media_dup_all(tmphome, local->sdp_media, local); \
	  if (!local->sdp_media)				 \
	    goto internal_error;				 \
	} while (0)
	DUP_LOCAL(local);
      }
      SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by, 
		  "marking rejected media"));
      soa_sdp_reject(tmphome, local, remote);
    }
    break;
  default:
    break;
  }

  /* Step D: Set media mode bits */
  switch (action) {
  case generate_offer:
  case generate_answer:
  case process_answer:
    if (soa_sdp_mode_set_is_needed(local, remote, ss->ss_hold)) {
      if (local != local0) {
	*local0 = *local, local = local0;
	DUP_LOCAL(local);
      }

      soa_sdp_mode_set(local, remote, ss->ss_hold);
    }
    break;
  default:
    break;
  }

  /* Step E: Upgrade codecs by answer. */
  switch (action) {
  case process_answer:
    /* Upgrade local SDP based on remote SDP */
    if (ss->ss_local_remote_version == remote_version)
      break;
    if (1 /* We don't have good test for codecs */) {
      SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by,
		  "upgrade codecs with remote description"));
      if (local != local0) {
	*local0 = *local, local = local0; 
	DUP_LOCAL(local);
      }
      soa_sdp_upgrade_rtpmaps(ss, local, remote);
    }
    break;
  case generate_offer:
  case generate_answer:
  default:
    break;
  }

  /* Step F: Update c= line */ 
  switch (action) {
  case generate_offer:
  case generate_answer:
    /* Upgrade local SDP based of user SDP */
    if (ss->ss_local_user_version == user_version &&
	local->sdp_connection)
      break;

    if (local->sdp_connection == NULL || 
	(user->sdp_connection != NULL && 
	 sdp_connection_cmp(local->sdp_connection, user->sdp_connection))) {
      sdp_media_t *m;

      /* Every m= line (even rejected one) must have a c= line 
       * or there must be a c= line at session level
       */
      if (user->sdp_connection)
	c = user->sdp_connection;
      else
	c = local->sdp_origin->o_address;

      for (m = local->sdp_media; m; m = m->m_next)
	if (m->m_connections == NULL)
	  break;

      if (m) {
	if (local != local0) {
	  *local0 = *local, local = local0; 
	  DUP_LOCAL(local);
	}
	local->sdp_connection = c;
      }
    }
    break;

  default:
    break;
  }

  soa_description_free(ss, ss->ss_previous);

  if (ss->ss_local->ssd_sdp != local &&
      sdp_session_cmp(ss->ss_local->ssd_sdp, local)) {
    /* We have modfied local session: update origin-line */
    if (local->sdp_origin != o)
      *o = *local->sdp_origin, local->sdp_origin = o;
    o->o_version++;

    /* Do sanity checks for the created SDP */
    if (!local->sdp_subject)	/* s= is mandatory */
      local->sdp_subject = "-";
    if (!local->sdp_time)	/* t= is mandatory */
      local->sdp_time = t;

    if (action == generate_offer) {
      /* Keep a copy of previous session state */
      *ss->ss_previous = *ss->ss_local;
      memset(ss->ss_local, 0, (sizeof *ss->ss_local));
      ss->ss_previous_user_version = ss->ss_local_user_version;
      ss->ss_previous_remote_version = ss->ss_local_remote_version;
    }

    SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by,
		"storing local description"));

    /* Update the unparsed and pretty-printed descriptions  */
    if (soa_description_set(ss, ss->ss_local, local, NULL, 0) < 0) {
      goto internal_error;
    }
  }

  /* Update version numbers */
  switch (action) {
  case generate_offer:
    ss->ss_local_user_version = user_version;
    break;
  case generate_answer:
    ss->ss_local_user_version = user_version;
    ss->ss_local_remote_version = remote_version;
    break;
  case process_answer:
    ss->ss_local_remote_version = remote_version;
  default:
    break;
  }

  su_home_deinit(tmphome);
  return 0;

 internal_error:
  su_home_deinit(tmphome);
  return soa_set_status(ss, 500, phrase);
}

/**
 * Generates offer based on local SDP.
 */
static int soa_static_generate_offer(soa_session_t *ss,
				     soa_callback_f *completed)
{
  if (!ss->ss_user->ssd_sdp)
    return soa_set_status(ss, 500, "No session set by user");

  if (offer_answer_step(ss, generate_offer, "soa_generate_offer") < 0)
    return -1;

  return soa_base_generate_offer(ss, NULL);
}

static int soa_static_generate_answer(soa_session_t *ss,
				      soa_callback_f *completed)
{
  /* NOTE:
   * - local SDP might have changed
   * - remote SDP might have been updated 
   */

  if (offer_answer_step(ss, generate_answer, "soa_generate_answer") < 0)
    return -1;

  return soa_base_generate_answer(ss, NULL);
}

static int soa_static_process_answer(soa_session_t *ss,
				     soa_callback_f *completed)
{
  /* NOTE:
   * - both local and remote information is available
   * - local SDP might have changed
   * - remote SDP might have been updated 
   */
  if (offer_answer_step(ss, process_answer, "soa_process_answer") < 0)
    return -1;

  return soa_base_process_answer(ss, NULL);
}

/** Process rejected offer */
static int soa_static_process_reject(soa_session_t *ss,
				     soa_callback_f *completed)
{
  struct soa_description d[1];

  *d = *ss->ss_local;
  *ss->ss_local = *ss->ss_previous;
  ss->ss_local_user_version = ss->ss_previous_user_version;
  ss->ss_local_remote_version = ss->ss_previous_remote_version;

  memset(ss->ss_previous, 0, (sizeof *ss->ss_previous));
  soa_description_free(ss, d);

  return soa_base_process_reject(ss, NULL);
}

static int soa_static_activate(soa_session_t *ss, char const *option)
{
  return soa_base_activate(ss, option);
}

static int soa_static_deactivate(soa_session_t *ss, char const *option)
{
  return soa_base_deactivate(ss, option);
}

static void soa_static_terminate(soa_session_t *ss, char const *option)
{
  soa_description_free(ss, ss->ss_user);
  soa_base_terminate(ss, option);
}
