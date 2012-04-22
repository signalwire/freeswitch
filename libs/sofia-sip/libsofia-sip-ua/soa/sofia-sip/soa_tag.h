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

#ifndef SOA_TAG_H
#define SOA_TAG_H
/**@file sofia-sip/soa_tag.h  Tags for SDP Offer/Answer Application Interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Aug 1 15:43:53 EEST 2005 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SDP_TAG_H
#include <sofia-sip/sdp_tag.h>
#endif

SOFIA_BEGIN_DECLS

/** List of base SOA tags (defined in base SOA module). */
SOFIAPUBVAR tagi_t soa_tag_list[];

/** Filter tag matching any soa tag. */
#define SOATAG_ANY()         soatag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t soatag_any;

/**
 * Media states
 */
enum {
  SOA_ACTIVE_REJECTED = -8, /**< Media rejected in negotiation */
  SOA_ACTIVE_DISABLED = -4, /**< Media not negotiated */
  SOA_ACTIVE_INACTIVE = 0,  /**< Media is inactive: no RTP */
  SOA_ACTIVE_SENDONLY = 1,  /**< Media is sent only */
  SOA_ACTIVE_RECVONLY = 2,  /**< Media is received only */
  SOA_ACTIVE_SENDRECV = SOA_ACTIVE_SENDONLY | SOA_ACTIVE_RECVONLY
			    /**< Media is bidirectional */
};

#define SOA_ACTIVE_DISABLED SOA_ACTIVE_DISABLED
#define SOA_ACTIVE_REJECTED SOA_ACTIVE_REJECTED
#define SOA_ACTIVE_INACTIVE SOA_ACTIVE_INACTIVE
#define SOA_ACTIVE_SENDONLY SOA_ACTIVE_SENDONLY
#define SOA_ACTIVE_RECVONLY SOA_ACTIVE_RECVONLY
#define SOA_ACTIVE_SENDRECV SOA_ACTIVE_SENDRECV

/*
 * SOA engine and media parameters set by soa_set_params(), get by
 * soa_get_params() or soa_get_paramlist()
 */

#define SOATAG_LOCAL_SDP(x)  soatag_local_sdp, sdptag_session_v(x)
SOFIAPUBVAR tag_typedef_t soatag_local_sdp;
#define SOATAG_LOCAL_SDP_REF(x) \
  soatag_local_sdp_ref, sdptag_session_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_local_sdp_ref;

#define SOATAG_LOCAL_SDP_STR(x)  soatag_local_sdp_str, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_local_sdp_str;
#define SOATAG_LOCAL_SDP_STR_REF(x) \
  soatag_local_sdp_str_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_local_sdp_str_ref;

#define SOATAG_USER_SDP(x)  soatag_user_sdp, sdptag_session_v(x)
SOFIAPUBVAR tag_typedef_t soatag_user_sdp;
#define SOATAG_USER_SDP_REF(x) \
  soatag_user_sdp_ref, sdptag_session_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_user_sdp_ref;

#define SOATAG_USER_SDP_STR(x)  soatag_user_sdp_str, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_user_sdp_str;
#define SOATAG_USER_SDP_STR_REF(x) \
  soatag_user_sdp_str_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_user_sdp_str_ref;

#define SOATAG_CAPS_SDP(x)  soatag_caps_sdp, sdptag_session_v(x)
SOFIAPUBVAR tag_typedef_t soatag_caps_sdp;
#define SOATAG_CAPS_SDP_REF(x) \
  soatag_caps_sdp_ref, sdptag_session_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_caps_sdp_ref;

#define SOATAG_CAPS_SDP_STR(x)  soatag_caps_sdp_str, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_caps_sdp_str;
#define SOATAG_CAPS_SDP_STR_REF(x) \
  soatag_caps_sdp_str_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_caps_sdp_str_ref;

#define SOATAG_REMOTE_SDP(x)  soatag_remote_sdp, sdptag_session_v(x)
SOFIAPUBVAR tag_typedef_t soatag_remote_sdp;
#define SOATAG_REMOTE_SDP_REF(x) \
  soatag_remote_sdp_ref, sdptag_session_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_remote_sdp_ref;

#define SOATAG_REMOTE_SDP_STR(x)  soatag_remote_sdp_str, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_remote_sdp_str;
#define SOATAG_REMOTE_SDP_STR_REF(x) \
  soatag_remote_sdp_str_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_remote_sdp_str_ref;

#define SOATAG_LOCAL_SDP(x)  soatag_local_sdp, sdptag_session_v(x)
SOFIAPUBVAR tag_typedef_t soatag_local_sdp;
#define SOATAG_LOCAL_SDP_REF(x) \
  soatag_local_sdp_ref, sdptag_session_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_local_sdp_ref;

#define SOATAG_LOCAL_SDP_STR(x)  soatag_local_sdp_str, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_local_sdp_str;
#define SOATAG_LOCAL_SDP_STR_REF(x) \
  soatag_local_sdp_str_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_session_sdp_str_ref;

#define SOATAG_AF(x)             soatag_af, tag_int_v((x))
SOFIAPUBVAR tag_typedef_t soatag_af;

#define SOATAG_AF_REF(x)         soatag_af_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_af_ref;

/** SOATAG_AF() parameter type */
enum soa_af {
  SOA_AF_ANY,			/**< Use any address family. */
  SOA_AF_IP4_ONLY,		/**< Use IP version 4 only */
  SOA_AF_IP6_ONLY,		/**< Use IP version 6 only */
  SOA_AF_IP4_IP6,		/**< Prefer IP4 to IP6 */
  SOA_AF_IP6_IP4		/**< Prefer IP6 to IP4 */
};

#define SOA_AF_ANY      SOA_AF_ANY
#define SOA_AF_IP4_ONLY SOA_AF_IP4_ONLY
#define SOA_AF_IP6_ONLY SOA_AF_IP6_ONLY
#define SOA_AF_IP4_IP6  SOA_AF_IP4_IP6
#define SOA_AF_IP6_IP4  SOA_AF_IP6_IP4

#define SOATAG_ADDRESS(x)  soatag_address, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_address;
#define SOATAG_ADDRESS_REF(x) soatag_address_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_address_ref;

#define SOATAG_RTP_SELECT(x)  soatag_rtp_select, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t soatag_rtp_select;
#define SOATAG_RTP_SELECT_REF(x)  soatag_rtp_select_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_rtp_select_ref;

/** Parameter type for SOATAG_RTP_SELECT() */
enum {
  SOA_RTP_SELECT_SINGLE,	/**< Select the best common codec */
  SOA_RTP_SELECT_COMMON,	/**< Select all common codecs */
  SOA_RTP_SELECT_ALL		/**< Select all local codecs */
 };

#define SOATAG_AUDIO_AUX(x)      soatag_audio_aux, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_audio_aux;
#define SOATAG_AUDIO_AUX_REF(x)  soatag_audio_aux_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_audio_aux_ref;

#define SOATAG_RTP_SORT(x)  soatag_rtp_sort, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t soatag_rtp_sort;
#define SOATAG_RTP_SORT_REF(x) soatag_rtp_sort_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_rtp_sort_ref;

/** Parameter type for SOATAG_RTP_SORT() */
enum {
  SOA_RTP_SORT_DEFAULT,		/**< Select codecs by local preference
				 *  when media is recvonly,
				 * remote preference othewise.
				 */
  SOA_RTP_SORT_LOCAL,		/**< Select codecs by local preference. */
  SOA_RTP_SORT_REMOTE		/**< Select codecs by remote preference. */
 };

#define SOATAG_RTP_MISMATCH(x) soatag_rtp_mismatch, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t soatag_rtp_mismatch;
#define SOATAG_RTP_MISMATCH_REF(x) soatag_rtp_mismatch_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_rtp_mismatch_ref;

#define SOATAG_ACTIVE_AUDIO(x) soatag_active_audio, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t soatag_active_audio;

#define SOATAG_ACTIVE_AUDIO_REF(x) soatag_active_audio_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_active_audio_ref;

#define SOATAG_ACTIVE_VIDEO(x) soatag_active_video, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t soatag_active_video;

#define SOATAG_ACTIVE_VIDEO_REF(x) soatag_active_video_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_active_video_ref;

#define SOATAG_ACTIVE_IMAGE(x) soatag_active_image, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t soatag_active_image;

#define SOATAG_ACTIVE_IMAGE_REF(x) soatag_active_image_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_active_image_ref;

#define SOATAG_ACTIVE_CHAT(x) soatag_active_chat, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t soatag_active_chat;

#define SOATAG_ACTIVE_CHAT_REF(x) soatag_active_chat_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_active_chat_ref;

/** Enable SRTP */
#define SOATAG_SRTP_ENABLE(x)  soatag_srtp_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t soatag_srtp_enable;

#define SOATAG_SRTP_ENABLE_REF(x) soatag_srtp_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_srtp_enable_ref;

#define SOATAG_SRTP_CONFIDENTIALITY(x)  soatag_srtp_confidentiality, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t soatag_srtp_confidentiality;
#define SOATAG_SRTP_CONFIDENTIALITY_REF(x) soatag_srtp_confidentiality_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_srtp_confidentiality_ref;

/** Enable SRTP integrity protection */
#define SOATAG_SRTP_INTEGRITY(x)  soatag_srtp_integrity, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t soatag_srtp_integrity;

#define SOATAG_SRTP_INTEGRITY_REF(x) \
  soatag_srtp_integrity_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_srtp_integrity_ref;

#define SOATAG_HOLD(x)           soatag_hold, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t soatag_hold;
#define SOATAG_HOLD_REF(x)       soatag_hold_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_hold_ref;

#define SOATAG_ORDERED_USER(x) soatag_ordered_user, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t soatag_ordered_user;

#define SOATAG_ORDERED_USER_REF(x) \
  soatag_ordered_user_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_ordered_user_ref;

#define SOATAG_REUSE_REJECTED(x) soatag_reuse_rejected, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t soatag_reuse_rejected;

#define SOATAG_REUSE_REJECTED_REF(x) \
  soatag_reuse_rejected_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t soatag_reuse_rejected_ref;

SOFIA_END_DECLS

#endif /* SOA_TAG_H */
