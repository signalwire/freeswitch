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
#include <sofia-sip/su_string.h>
#include <sofia-sip/bnf.h>

#include "sofia-sip/soa.h"
#include <sofia-sip/sdp.h>
#include "sofia-sip/soa_session.h"

#define NONE ((void *)-1)
#define XXX assert(!"implemented")

typedef struct soa_static_session
{
  soa_session_t sss_session[1];
  char *sss_audio_aux;
  int sss_ordered_user;  /**< User SDP is ordered */
  int sss_reuse_rejected; /**< Try to reuse rejected media line slots */

  /** Mapping from user SDP m= lines to session SDP m= lines */
  int  *sss_u2s;
  /** Mapping from session SDP m= lines to user SDP m= lines */
  int *sss_s2u;

  /** State kept from SDP before current offer */
  struct {
    int  *u2s, *s2u;
  } sss_previous;

  /** Our latest offer or answer */
  sdp_session_t *sss_latest;
}
soa_static_session_t;

#define U2S_NOT_USED (-1)
#define U2S_SENTINEL (-2)

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
  int ordered_user = sss->sss_ordered_user;
  int reuse_rejected = sss->sss_reuse_rejected;
  int n, m;

  n = tl_gets(tags,
	      SOATAG_AUDIO_AUX_REF(audio_aux),
	      SOATAG_ORDERED_USER_REF(ordered_user),
	      SOATAG_REUSE_REJECTED_REF(reuse_rejected),
	      TAG_END());

  if (n > 0 && !su_casematch(audio_aux, sss->sss_audio_aux)) {
    char *s = su_strdup(ss->ss_home, audio_aux), *tbf = sss->sss_audio_aux;
    if (s == NULL && audio_aux != NULL)
      return -1;
    sss->sss_audio_aux = s;
    if (tbf)
      su_free(ss->ss_home, tbf);
  }

  sss->sss_ordered_user = ordered_user != 0;
  sss->sss_reuse_rejected = reuse_rejected != 0;

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
	       SOATAG_ORDERED_USER(sss->sss_ordered_user),
	       SOATAG_REUSE_REJECTED(sss->sss_reuse_rejected),
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
			      TAG_IF(sss->sss_ordered_user,
				     SOATAG_ORDERED_USER(1)),
			      TAG_IF(sss->sss_reuse_rejected,
				     SOATAG_REUSE_REJECTED(1)),
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
static
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
    if (m->m_rtpmaps) {
      rejected->m_rtpmaps = m->m_rtpmaps;
    }
    else if (m->m_format) {
      rejected->m_format = m->m_format;
    }
  }

  rejected->m_rejected = 1;

  return sdp_media_dup(home, rejected, sdp);
}

/** Expand a @a truncated SDP.
 */
static
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
static
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
       (match = su_strcasestr(match, codec));
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

static
sdp_rtpmap_t *soa_sdp_media_matching_rtpmap(sdp_rtpmap_t const *from,
					    sdp_rtpmap_t const *anylist,
					    char const *auxiliary)
{
  sdp_rtpmap_t const *rm;

  for (rm = anylist; rm; rm = rm->rm_next) {
    /* Ignore auxiliary codecs */
    if (auxiliary && soa_sdp_is_auxiliary_codec(rm, auxiliary))
      continue;

    if (sdp_rtpmap_find_matching(from, rm))
      return (sdp_rtpmap_t *)rm;
  }

  return NULL;
}

#ifndef _MSC_VER
#define SDP_MEDIA_NONE ((sdp_media_t *)-1)
#else
#define SDP_MEDIA_NONE ((sdp_media_t *)(INT_PTR)-1)
#endif

/** Find first matching media in table @a mm.
 *
 * - if allow_rtp_mismatch == 0, search for a matching codec
 * - if allow_rtp_mismatch == 1, prefer m=line with matching codec
 * - if allow_rtp_mismatch > 1, ignore codecs
 */
static
int soa_sdp_matching_mindex(soa_session_t *ss,
			    sdp_media_t *mm[],
			    sdp_media_t const *with,
			    int *return_codec_mismatch)
{
  int i, j = -1;
  soa_static_session_t *sss = (soa_static_session_t *)ss;
  int rtp = sdp_media_uses_rtp(with), dummy;
  char const *auxiliary = NULL;

  if (return_codec_mismatch == NULL)
    return_codec_mismatch = &dummy;

  if (with->m_type == sdp_media_audio) {
    auxiliary = sss->sss_audio_aux;
    /* Looking for a single codec */
    if (with->m_rtpmaps && with->m_rtpmaps->rm_next == NULL)
      auxiliary = NULL;
  }

  for (i = 0; mm[i]; i++) {
    if (mm[i] == SDP_MEDIA_NONE)
      continue;

    if (!sdp_media_match_with(mm[i], with))
      continue;

    if (!rtp)
      break;

    if (soa_sdp_media_matching_rtpmap(with->m_rtpmaps,
				      mm[i]->m_rtpmaps,
				      auxiliary))
      break;

    if (j == -1)
      j = i;
  }

  if (mm[i])
    return *return_codec_mismatch = 0, i;
  else
    return *return_codec_mismatch = 1, j;
}

/** Set payload types in @a l_m according to the values in @a r_m.
 *
 * @retval number of common codecs
 */
static
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
#if 0
      /* Use same payload type as remote */
      if (lrm->rm_pt != rrm->rm_pt) {
		  lrm->rm_predef = 0;
		  lrm->rm_pt = rrm->rm_pt;

      }
#endif
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
static
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
static
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


/** Sort and select rtpmaps  */
static
int soa_sdp_media_upgrade_rtpmaps(soa_session_t *ss,
				  sdp_media_t *sm,
				  sdp_media_t const *rm)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;
  char const *auxiliary = NULL;
  int common_codecs;

  common_codecs = soa_sdp_set_rtpmap_pt(sm, rm);

  if (rm->m_type == sdp_media_audio)
    auxiliary = sss->sss_audio_aux;

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

  return common_codecs;
}


/** Sort and select rtpmaps within session */
static
int soa_sdp_session_upgrade_rtpmaps(soa_session_t *ss,
				    sdp_session_t *session,
				    sdp_session_t const *remote)
{
  sdp_media_t *sm;
  sdp_media_t const *rm;

  for (sm = session->sdp_media, rm = remote->sdp_media;
       sm && rm;
       sm = sm->m_next, rm = rm->m_next) {
    if (!sm->m_rejected && sdp_media_uses_rtp(sm))
      soa_sdp_media_upgrade_rtpmaps(ss, sm, rm);
  }

  return 0;
}

/** Upgrade m= lines within session */
static
int soa_sdp_upgrade(soa_session_t *ss,
		    su_home_t *home,
		    sdp_session_t *session,
		    sdp_session_t const *user,
		    sdp_session_t const *remote,
		    int **return_u2s,
		    int **return_s2u)
{
  soa_static_session_t *sss = (soa_static_session_t *)ss;

  int Ns, Nu, Nr, Nmax, n, i, j;
  sdp_media_t *m, **mm, *um;
  sdp_media_t **s_media, **o_media, **u_media;
  sdp_media_t const *rm, **r_media;
  int *u2s = NULL, *s2u = NULL;

  if (session == NULL || user == NULL)
    return (errno = EFAULT), -1;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-literal-null-conversion"
#endif

  Ns = sdp_media_count(session, sdp_media_any, (sdp_text_t)0, (sdp_proto_e)0, (sdp_text_t)0);
  Nu = sdp_media_count(user, sdp_media_any, (sdp_text_t)0, (sdp_proto_e)0, (sdp_text_t)0);
  Nr = sdp_media_count(remote, sdp_media_any, (sdp_text_t)0, (sdp_proto_e)0, (sdp_text_t)0);

#ifdef __clang__
#pragma clang diagnostic pop
#endif

  if (remote == NULL)
    Nmax = Ns + Nu;
  else if (Ns < Nr)
    Nmax = Nr;
  else
    Nmax = Ns;

  s_media = su_zalloc(home, (Nmax + 1) * (sizeof *s_media));
  o_media = su_zalloc(home, (Ns + 1) * (sizeof *o_media));
  u_media = su_zalloc(home, (Nu + 1) * (sizeof *u_media));
  r_media = su_zalloc(home, (Nr + 1) * (sizeof *r_media));
  if (!s_media || !o_media || !u_media || !r_media)
    return -1;

  um = sdp_media_dup_all(home, user->sdp_media, session);
  if (!um && user->sdp_media)
    return -1;

  u2s = su_alloc(home, (Nu + 1) * sizeof(*u2s));
  s2u = su_alloc(home, (Nmax + 1) * sizeof(*s2u));
  if (!u2s || !s2u)
    return -1;

  for (i = 0; i < Nu; i++)
    u2s[i] = U2S_NOT_USED;
  u2s[Nu] = U2S_SENTINEL;

  for (i = 0; i < Nmax; i++)
    s2u[i] = U2S_NOT_USED;
  s2u[Nmax] = U2S_SENTINEL;

  for (i = 0, m = session->sdp_media; m && i < Ns; m = m->m_next)
    o_media[i++] = m;
  assert(i == Ns);
  for (i = 0, m = um; m && i < Nu; m = m->m_next)
    u_media[i++] = m;
  assert(i == Nu);
  m = remote ? remote->sdp_media : NULL;
  for (i = 0; m && i < Nr; m = m->m_next)
      r_media[i++] = m;
  assert(i == Nr);

  if (sss->sss_ordered_user && sss->sss_u2s) {     /* User SDP is ordered */
    for (j = 0; sss->sss_u2s[j] != U2S_SENTINEL; j++) {
      i = sss->sss_u2s[j];
      if (i == U2S_NOT_USED)
	continue;
      if (j >= Nu) /* lines removed from user SDP */
	continue;
	  if (i >= Ns) /* I should never be called but somehow i and Ns are 0 here sometimes */
    continue;
      s_media[i] = u_media[j], u_media[j] = SDP_MEDIA_NONE;
      u2s[j] = i, s2u[i] = j;
    }
  }

  if (remote) {
    /* Update session according to remote */
    for (i = 0; i < Nr; i++) {
      rm = r_media[i];
      m = s_media[i];

      if (!m) {
	int codec_mismatch = 0;

	if (!rm->m_rejected)
	  j = soa_sdp_matching_mindex(ss, u_media, rm, &codec_mismatch);
	else
	  j = -1;

	if (j == -1) {
	  s_media[i] = soa_sdp_make_rejected_media(home, rm, session, 0);
	  continue;
	}
	else if (codec_mismatch && !ss->ss_rtp_mismatch) {
	  m = soa_sdp_make_rejected_media(home, u_media[j], session, 1);
	  soa_sdp_set_rtpmap_pt(s_media[i] = m, rm);
	  continue;
	}

	s_media[i] = m = u_media[j]; u_media[j] = SDP_MEDIA_NONE;
	u2s[j] = i, s2u[i] = j;
      }

      if (sdp_media_uses_rtp(rm))
	soa_sdp_media_upgrade_rtpmaps(ss, m, rm);
    }
  }
  else {

    if (sss->sss_ordered_user) {
      /* Update session with unused media in u_media */

      if (!sss->sss_reuse_rejected) {
	/* Mark previously used slots */
	for (i = 0; i < Ns; i++) {
	  if (s_media[i])
	    continue;
	  s_media[i] =
	    soa_sdp_make_rejected_media(home, o_media[i], session, 0);
	}
      }

      for (j = 0; j < Nu; j++) {
	if (u_media[j] == SDP_MEDIA_NONE)
	  continue;

	for (i = 0; i < Nmax; i++) {
	  if (s_media[i] == NULL) {
	    s_media[i] = u_media[j], u_media[j] = SDP_MEDIA_NONE;
	    u2s[j] = i, s2u[i] = j;
	    break;
	  }
	}

	assert(i != Nmax);
      }
    }

    /* Match unused user media by media types with the existing session */
    for (i = 0; i < Ns; i++) {
      if (s_media[i])
	continue;

      j = soa_sdp_matching_mindex(ss, u_media, o_media[i], NULL);
      if (j == -1) {
	s_media[i] = soa_sdp_make_rejected_media(home, o_media[i], session, 0);
	continue;
      }

      s_media[i] = u_media[j], u_media[j] = SDP_MEDIA_NONE;
      u2s[j] = i, s2u[i] = j;
    }

    /* Here we just append new media at the end */
    for (j = 0; j < Nu; j++) {
      if (u_media[j] != SDP_MEDIA_NONE) {
	s_media[i] = u_media[j], u_media[j] = SDP_MEDIA_NONE;
	u2s[j] = i, s2u[i] = j;
	i++;
      }
    }
    assert(i <= Nmax);
  }

  mm = &session->sdp_media;
  for (i = 0; s_media[i]; i++) {
    m = s_media[i]; *mm = m; mm = &m->m_next;
  }
  *mm = NULL;

  s2u[n = i] = U2S_SENTINEL;
  *return_u2s = u2s;
  *return_s2u = s2u;

#ifndef NDEBUG			/* X check */
  for (j = 0; j < Nu; j++) {
    i = u2s[j];
    assert(i == U2S_NOT_USED || s2u[i] == j);
  }
  for (i = 0; i < n; i++) {
    j = s2u[i];
    assert(j == U2S_NOT_USED || u2s[j] == i);
  }
#endif

  return 0;
}

static
int *u2s_alloc(su_home_t *home, int const *u2s)
{
  if (u2s) {
    int i, *a;
    for (i = 0; u2s[i] != U2S_SENTINEL; i++)
      ;
    a = su_alloc(home, (i + 1) * (sizeof *u2s));
    if (a)
      memcpy(a, u2s, (i + 1) * (sizeof *u2s));
    return a;
  }

  return NULL;
}

/** Check if @a session contains media that are rejected by @a remote. */
static
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
static
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


/** Update mode within session.
 *
 * @sa soatag_hold
 *
 * @retval 1 if session was changed (or to be changed, if @a dryrun is nonzero)
 */
static
int soa_sdp_mode_set(sdp_session_t const *user,
		     int const *s2u,
		     sdp_session_t *session,
		     sdp_session_t const *remote,
		     char const *hold,
		     int dryrun)
{
  sdp_media_t *sm;
  sdp_media_t const *rm, *rm_next, *um;
  int retval = 0, i, j;
  int hold_all;
  int inactive_all;
  char const *hold_media = NULL;
  sdp_mode_t send_mode, recv_mode;

  SU_DEBUG_7(("soa_sdp_mode_set(%p, %p, \"%s\"): called\n",
	      (void *)session, (void *)remote, hold ? hold : ""));

  if (!session || !session->sdp_media)
    return 0;

  rm = remote ? remote->sdp_media : NULL, rm_next = NULL;

  hold_all = su_strmatch(hold, "*");
  inactive_all = su_strmatch(hold, "#");

  i = 0;

  for (sm = session->sdp_media; sm; sm = sm->m_next, rm = rm_next, i++) {
    rm_next = rm ? rm->m_next : NULL;

    if (sm->m_rejected)
      continue;

    assert(s2u);

    for (j = 0, um = user->sdp_media; j != s2u[i]; um = um->m_next, j++) {
		if (!um) break;
	}

    if (um == NULL) {
      if (dryrun)
		  return 1;
      else
		  retval = 1;
      sm->m_rejected = 1;
      sm->m_mode = sdp_inactive;
      sm->m_port = 0;
      continue;
    }

    send_mode = (sdp_mode_t)(um->m_mode & sdp_sendonly);
    if (rm)
      send_mode = (rm->m_mode & sdp_recvonly) ? sdp_sendonly : 0;

    recv_mode = (sdp_mode_t)(um->m_mode & sdp_recvonly);

    if (rm && rm->m_mode == sdp_inactive) {
      send_mode = recv_mode = (sdp_mode_t)0;
    }
    else if (inactive_all) {
      send_mode = recv_mode = (sdp_mode_t)0;
    }
    else if (hold_all) {
      recv_mode = (sdp_mode_t)0;
    }
    else if (hold && (hold_media = su_strcasestr(hold, sm->m_type_name))) {
      recv_mode = (sdp_mode_t)0;
      hold_media += strlen(sm->m_type_name);
      hold_media += strspn(hold_media, " \t");
      if (hold_media[0] == '=') {
	hold_media += strspn(hold, " \t");
	if (su_casenmatch(hold_media, "inactive", strlen("inactive")))
	  recv_mode = send_mode = (sdp_mode_t)0;
      }
    }

    if (sm->m_mode != (unsigned)(recv_mode | send_mode)) {
      if (dryrun)
	return 1;
      else
	retval = 1;
      sm->m_mode = recv_mode | send_mode;
    }
  }

  return retval;
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
  soa_static_session_t *sss = (soa_static_session_t *)ss;

  sdp_session_t *local = ss->ss_local->ssd_sdp;
  sdp_session_t local0[1];

  sdp_session_t *user = ss->ss_user->ssd_sdp;
  unsigned user_version = ss->ss_user_version;

  sdp_session_t *remote = ss->ss_remote->ssd_sdp;
  unsigned remote_version = ss->ss_remote_version;

  int fresh = 0;

  sdp_origin_t o[1] = {{ sizeof(o) }};
  sdp_connection_t *c, c0[1] = {{ sizeof(c0) }};
  char c0_buffer[64];

  sdp_time_t t[1] = {{ sizeof(t) }};

  int *u2s = NULL, *s2u = NULL, *tbf;

  sdp_session_t *latest = NULL, *previous = NULL;

  char const *phrase = "Internal Media Error";

  su_home_t tmphome[SU_HOME_AUTO_SIZE(8192)];

  su_home_auto(tmphome, sizeof tmphome);

  SU_DEBUG_7(("soa_static_offer_answer_action(%p, %s): called\n",
	      (void *)ss, by));

  if (user == NULL)
    return soa_set_status(ss, 500, "No session set by user");

  if (action == generate_offer)
    remote = NULL;
  else if (remote == NULL)
    return soa_set_status(ss, 500, "No remote SDP");

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-literal-null-conversion"
#endif

  /* Pre-negotiation Step: Expand truncated remote SDP */
  if (local && remote) switch (action) {
  case generate_answer:
  case process_answer:
    if (sdp_media_count(remote, sdp_media_any, "*", (sdp_proto_e)0, (sdp_text_t)0) <
	sdp_media_count(local, sdp_media_any, "*", (sdp_proto_e)0, (sdp_text_t)0)) {
      SU_DEBUG_5(("%s: remote %s is truncated: expanding\n",
		  by, action == generate_answer ? "offer" : "answer"));
      remote = soa_sdp_expand_media(tmphome, remote, local);
      if (remote == NULL)
	return soa_set_status(ss, 500, "Cannot expand remote session");
    }
  default:
    break;
  }

#ifdef __clang__
#pragma clang diagnostic pop
#endif


  /* Step A: Create local SDP session (based on user-supplied SDP) */
  if (local == NULL) switch (action) {
  case generate_offer:
  case generate_answer:
    SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by,
		"generating local description"));

    fresh = 1;
    local = local0;
    *local = *user, local->sdp_media = NULL;

    o->o_username = "-";
    o->o_address = c0;
    c0->c_address = c0_buffer;

    if (!local->sdp_origin)
      local->sdp_origin = o;
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
    if (soa_sdp_upgrade(ss, tmphome, local, user, NULL, &u2s, &s2u) < 0)
      goto internal_error;
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
      if (soa_sdp_upgrade(ss, tmphome, local, user, remote, &u2s, &s2u) < 0)
	goto internal_error;
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
    int const *s2u_;

  case generate_offer:
  case generate_answer:
  case process_answer:
    s2u_ = s2u;

    if (!s2u_) s2u_ = sss->sss_s2u;

    if (soa_sdp_mode_set(user, s2u_, local, remote, ss->ss_hold, 1)) {
      if (local != local0) {
	*local0 = *local, local = local0;
	DUP_LOCAL(local);
      }

      soa_sdp_mode_set(user, s2u_, local, remote, ss->ss_hold, 0);
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
      soa_sdp_session_upgrade_rtpmaps(ss, local, remote);
    }
    break;
  case generate_offer:
  case generate_answer:
  default:
    break;
  }

  /* Step F0: Initialize o= line */
  if (fresh) {
	if (user->sdp_origin) {
      o->o_username = user->sdp_origin->o_username;

	  if (user->sdp_origin->o_address)
	    o->o_address = user->sdp_origin->o_address;

      if (user->sdp_origin->o_id)
		  o->o_id = user->sdp_origin->o_id;

	  if (user->sdp_origin->o_version && user->sdp_origin->o_version != o->o_version) {
		  o->o_version = user->sdp_origin->o_version;
		  o->o_version--;
	  }
	}

    if (soa_init_sdp_origin_with_session(ss, o, c0_buffer, local) < 0) {
      phrase = "Cannot Get IP Address for Session Description";
      goto internal_error;
    }

    local->sdp_origin = o;
  }

  /* Step F: Update c= line(s) */
  switch (action) {
    sdp_connection_t *user_c, *local_c;

  case generate_offer:
  case generate_answer:
    user_c = user->sdp_connection;
    if (!soa_check_sdp_connection(user_c))
      user_c = NULL;

    local_c = local->sdp_connection;
    if (!soa_check_sdp_connection(local_c))
      local_c = NULL;

    if (ss->ss_local_user_version != user_version ||
	local_c == NULL ||
	(user_c != NULL && sdp_connection_cmp(local_c, user_c))) {
      sdp_media_t *m;

      if (user_c)
	c = user_c;
      else
	c = local->sdp_origin->o_address;

      /* Every m= line (even rejected one) must have a c= line
       * or there must be a c= line at session level
       */
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
  su_free(ss->ss_home, sss->sss_previous.u2s), sss->sss_previous.u2s = NULL;
  su_free(ss->ss_home, sss->sss_previous.s2u), sss->sss_previous.s2u = NULL;

  if (u2s) {
    u2s = u2s_alloc(ss->ss_home, u2s);
    s2u = u2s_alloc(ss->ss_home, s2u);
    if (!u2s || !s2u)
      goto internal_error;
  }

  if (ss->ss_local->ssd_sdp != local &&
      sdp_session_cmp(ss->ss_local->ssd_sdp, local)) {
    int bump;

    switch (action) {
    case generate_offer:
      bump = sdp_session_cmp(local, sss->sss_latest);
      break;
    case generate_answer:
      bump = 1;
      break;
    case process_answer:
    default:
      bump = 0;
      break;
    }

    if (bump) {
      /* Upgrade the version number */
      if (local->sdp_origin != o)
	*o = *local->sdp_origin, local->sdp_origin = o;
      o->o_version++;
    }

    /* Do sanity checks for the created SDP */
    if (!local->sdp_subject)	/* s= is mandatory */
      local->sdp_subject = "-";
    if (!local->sdp_time)	/* t= is mandatory */
      local->sdp_time = t;

    if (action == generate_offer) {
      /* Keep a copy of previous session state */
      int *previous_u2s = u2s_alloc(ss->ss_home, sss->sss_u2s);
      int *previous_s2u = u2s_alloc(ss->ss_home, sss->sss_s2u);

      if ((sss->sss_u2s && !previous_u2s) || (sss->sss_s2u && !previous_s2u))
	goto internal_error;

      *ss->ss_previous = *ss->ss_local;
      memset(ss->ss_local, 0, (sizeof *ss->ss_local));
      ss->ss_previous_user_version = ss->ss_local_user_version;
      ss->ss_previous_remote_version = ss->ss_local_remote_version;
      sss->sss_previous.u2s = previous_u2s;
      sss->sss_previous.s2u = previous_s2u;
    }

    SU_DEBUG_7(("soa_static(%p, %s): %s\n", (void *)ss, by,
		"storing local description"));

    /* Update the unparsed and pretty-printed descriptions  */
    if (soa_description_set(ss, ss->ss_local, local, NULL, 0) < 0) {
      if (action == generate_offer) {
	/* Remove 2nd reference to local session state */
	memset(ss->ss_previous, 0, (sizeof *ss->ss_previous));
	ss->ss_previous_user_version = 0;
	ss->ss_previous_remote_version = 0;
	su_free(ss->ss_home, sss->sss_previous.u2s), sss->sss_previous.u2s = NULL;
	su_free(ss->ss_home, sss->sss_previous.s2u), sss->sss_previous.s2u = NULL;
      }

      su_free(ss->ss_home, u2s), su_free(ss->ss_home, s2u);

      goto internal_error;
    }

    if (bump) {
      latest = sdp_session_dup(ss->ss_home, ss->ss_local->ssd_sdp);
      previous = sss->sss_latest;
    }
  }

  if (u2s) {
    tbf = sss->sss_u2s, sss->sss_u2s = u2s, su_free(ss->ss_home, tbf);
    tbf = sss->sss_s2u, sss->sss_s2u = s2u, su_free(ss->ss_home, tbf);
  }

  /* Update version numbers */
  switch (action) {
  case generate_offer:
    ss->ss_local_user_version = user_version;
    sss->sss_latest = latest;
    break;
  case generate_answer:
    ss->ss_local_user_version = user_version;
    ss->ss_local_remote_version = remote_version;
    sss->sss_latest = latest;
    break;
  case process_answer:
    ss->ss_local_remote_version = remote_version;
  default:
    break;
  }

  if (previous)
    su_free(ss->ss_home, previous);

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
  soa_static_session_t *sss = (soa_static_session_t *)ss;
  struct soa_description d[1];

  soa_base_process_reject(ss, NULL);

  *d = *ss->ss_local;
  *ss->ss_local = *ss->ss_previous;
  ss->ss_local_user_version = ss->ss_previous_user_version;
  ss->ss_local_remote_version = ss->ss_previous_remote_version;

  memset(ss->ss_previous, 0, (sizeof *ss->ss_previous));
  soa_description_free(ss, d);
  su_free(ss->ss_home, sss->sss_previous.u2s), sss->sss_previous.u2s = NULL;
  su_free(ss->ss_home, sss->sss_previous.s2u), sss->sss_previous.s2u = NULL;
  ss->ss_previous_user_version = 0;
  ss->ss_previous_remote_version = 0;

  su_free(ss->ss_home, sss->sss_latest), sss->sss_latest = NULL;

  return 0;
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
  soa_static_session_t *sss = (soa_static_session_t *)ss;

  soa_description_free(ss, ss->ss_local);
  su_free(ss->ss_home, sss->sss_u2s), sss->sss_u2s = NULL;
  su_free(ss->ss_home, sss->sss_s2u), sss->sss_s2u = NULL;

  soa_description_free(ss, ss->ss_previous);
  ss->ss_previous_user_version = 0;
  ss->ss_previous_remote_version = 0;
  su_free(ss->ss_home, sss->sss_previous.u2s), sss->sss_previous.u2s = NULL;
  su_free(ss->ss_home, sss->sss_previous.s2u), sss->sss_previous.s2u = NULL;

  su_free(ss->ss_home, sss->sss_latest), sss->sss_latest = NULL;

  soa_base_terminate(ss, option);
}
