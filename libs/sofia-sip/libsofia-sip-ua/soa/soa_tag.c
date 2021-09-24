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

/**@CFILE soa_tag.c  Tags and tag lists for Offer/Answer Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Aug  3 20:28:17 EEST 2005
 */

#include "config.h"

#include <sofia-sip/su.h>

#if 1
#define TAG_NAMESPACE soa_tag_namespace
#else
/* Definition used by tag_dll.awk */
#define TAG_NAMESPACE "soa"
#endif

#include <sofia-sip/soa.h>
#include <sofia-sip/soa_tag.h>

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/sdp_tag.h>

#include <string.h>

char const soa_tag_namespace[] = "soa";

/** Filter soa tags. */
int soa_tag_filter(tagi_t const *f, tagi_t const *t)
{
  char const *ns;

  if (!t || !t->t_tag)
    return 0;

  ns = t->t_tag->tt_ns;
  if (!ns)
    return 0;

  return ns == soa_tag_namespace || strcmp(ns, soa_tag_namespace) == 0;
}

/**@def SOATAG_ANY()
 *
 * Filter tag matching any SOATAG_*() item.
 */
tag_typedef_t soatag_any = NSTAG_TYPEDEF(*);

/**@def SOATAG_CAPS_SDP(x)
 *  Pass parsed capability description to soa session object.
 *
 * @par Used with
 *    soa_set_params() \n
 *    soa_get_params() \n
 *
 * @par Parameter type
 *    #sdp_session_t *
 *
 * @par Values
 *    #sdp_session_t describing @soa capabilities
 *
 * Corresponding tag taking reference parameter is SOATAG_CAPS_SDP_REF()
 */
tag_typedef_t soatag_caps_sdp = SDPTAG_TYPEDEF(caps_sdp);

/**@def SOATAG_CAPS_SDP_STR(x)
 *  Pass capability description to @soa session object.
 *
 * @par Used with
 *    soa_set_param() \n
 *    soa_get_params() \n
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    String containing SDP description of @soa capabilities
 *
 * Corresponding tag taking reference parameter is SOATAG_CAPS_SDP_STR_REF()
 */
tag_typedef_t soatag_caps_sdp_str = STRTAG_TYPEDEF(caps_sdp_str);

/**@def SOATAG_LOCAL_SDP(x)
 *  Get parsed local session description from soa session object.
 *
 * @par Used with
 *    soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    #sdp_session_t *
 *
 * @par Values
 *    pointer to #sdp_session_t.
 *
 * Corresponding tag taking reference parameter is SOATAG_LOCAL_SDP_REF()
 *
 * @sa soa_get_local_sdp(), SOATAG_LOCAL_SDP_STR(), SOATAG_USER_SDP(),
 * SOATAG_USER_SDP_STR().
 */
tag_typedef_t soatag_local_sdp = SDPTAG_TYPEDEF(local_sdp);

/**@def SOATAG_LOCAL_SDP_STR(x)
 * Get local session description as a string from soa session object.
 *
 * @par Used with
 *    soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 * char const *
 *
 * @par Values
 *  String containing SDP offer or answer.
 *
 * Corresponding tag taking reference parameter is SOATAG_LOCAL_SDP_STR_REF()..
 *
 * @sa soa_get_local_sdp(), SOATAG_LOCAL_SDP(),
 * SOATAG_USER_SDP(), SOATAG_USER_SDP_STR().
 */
tag_typedef_t soatag_local_sdp_str = STRTAG_TYPEDEF(local_sdp_str);

/**@def SOATAG_REMOTE_SDP(x)
 *  Pass parsed remote session description to soa session object.
 *
 * @par Used with
 *    soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    #sdp_session_t *
 *
 * @par Values
 *    pointer to #sdp_session_t.
 *
 * Corresponding tag taking reference parameter is SOATAG_REMOTE_SDP_REF()
 *
 * @sa soa_set_remote_sdp(), soa_get_remote_sdp(), SOATAG_REMOTE_SDP_STR(),
 * SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR().
 */
tag_typedef_t soatag_remote_sdp = SDPTAG_TYPEDEF(remote_sdp);

/**@def SOATAG_REMOTE_SDP_STR(x)
 *  Pass media description file name to the NUA stack.
 *
 * Pass name of media description file that contains media templates
 * (normally mss.sdp) to the NUA stack.
 *
 * @par Used with
 *    soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    String containing SDP description received from remote end.
 *
 * Corresponding tag taking reference parameter is SOATAG_REMOTE_SDP_STR_REF()
 *
 * @sa soa_set_remote_sdp(), soa_get_remote_sdp(), SOATAG_REMOTE_SDP(),
 * SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR().
 */
tag_typedef_t soatag_remote_sdp_str = STRTAG_TYPEDEF(remote_sdp_str);

/**@def SOATAG_USER_SDP(x)
 *  Pass parsed user session description to soa session object.
 *
 * User SDP is used as basis for SDP Offer/Answer negotiation. It can be
 * very minimal, consisting just sdp_session_t structures, sdp_media_t
 * structures and sdp_rtpmap_t structures listing te supported media, used
 * RTP port number, and RTP payload descriptions of supported codecs.
 *
 * When generating the offer or answer the user SDP is augmented with the
 * required SDP lines (v=, o=, t=, c=, a=rtpmap, etc.) as required. The
 * complete offer or answer generated by @soa is passed in
 * SOATAG_LOCAL_SDP() (SOATAG_LOCAL_SDP_STR() contains same in text format).
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    #sdp_session_t *
 *
 * @par Values
 *    pointer to #sdp_session_t.
 *
 * Corresponding tag taking reference parameter is SOATAG_USER_SDP_REF()
 *
 * @sa soa_set_user_sdp(), soa_get_user_sdp(), SOATAG_USER_SDP_STR(),
 * SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR().
 */
tag_typedef_t soatag_user_sdp = SDPTAG_TYPEDEF(user_sdp);

/**@def SOATAG_USER_SDP_STR(x)
 * Pass unparsed user session description to soa session object.
 *
 * User SDP is used as basis for SDP Offer/Answer negotiation. It can be
 * very minimal, listing just m= lines with the port numbers and RTP payload
 * numbers of supported codecs, like
 * @code
 *   SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8")
 * @endcode
 * When generating the offer or answer the user SDP is augmented with the
 * required SDP lines (v=, o=, t=, c=, a=rtpmap, etc.) as required. The
 * complete offer or answer generated by @soa is passed in
 * SOATAG_LOCAL_SDP_STR() (SOATAG_LOCAL_SDP() contains session in parsed
 * format).
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    String containing minimal SDP description.
 *
 * Corresponding tag taking reference parameter is SOATAG_USER_SDP_STR_REF()
 *
 * @sa soa_set_user_sdp(), soa_get_user_sdp(), SOATAG_USER_SDP(),
 * SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR()
 */
tag_typedef_t soatag_user_sdp_str = STRTAG_TYPEDEF(user_sdp_str);

/**@def SOATAG_AF(x)
 *
 * Preferred address family for media.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    enum #soa_af {
 *      #SOA_AF_ANY,
 *      #SOA_AF_IP4_ONLY, #SOA_AF_IP6_ONLY,
 *      #SOA_AF_IP4_IP6,  #SOA_AF_IP6_IP4
 *    }
 *
 * @par Values
 *    - #SOA_AF_ANY       (0) any address family (default)
 *    - #SOA_AF_IP4_ONLY  (1) only IP version 4
 *    - #SOA_AF_IP6_ONLY  (2) only IP version 6
 *    - #SOA_AF_IP4_IP6   (3) either IP version 4 or 6, version 4 preferred
 *    - #SOA_AF_IP6_IP4   (4) either IP version 4 or 6, version 6 preferred
 *
 * Corresponding tag taking reference parameter is SOATAG_AF_REF()
 *
 * @sa SOATAG_ADDRESS()
 */
tag_typedef_t soatag_af = INTTAG_TYPEDEF(af);


/**@def SOATAG_ADDRESS(x)
 *
 * Pass media address.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    NUL-terminated C string containing a domain name,
 *    IPv4 address, or IPv6 address.
 *
 * Corresponding tag taking reference parameter is SOATAG_ADDRESS_REF()
 *
 * @sa SOATAG_AF()
 */
tag_typedef_t soatag_address = STRTAG_TYPEDEF(address);


/**@def SOATAG_RTP_SELECT(x)
 *
 * When generating answer or second offer, @soa can include all the supported
 * codecs, only one codec, or only the codecs supported by both ends in the
 * list of payload types on the m= line.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    enum {
 *      #SOA_RTP_SELECT_SINGLE, #SOA_RTP_SELECT_COMMON, #SOA_RTP_SELECT_ALL
 *    } \n
 *    (int in range 0..2)
 *
 * @par Values
 *    - #SOA_RTP_SELECT_SINGLE (0) - select the best common codec
 *    - #SOA_RTP_SELECT_COMMON (1) - select all common codecs
 *    - #SOA_RTP_SELECT_ALL (2) - select all local codecs
 *
 * The default value is 0, only one RTP codec is selected. Note, however,
 * that if there is no common codec (no local codec is supported by remote
 * end), all the codecs are included in the list. In that case the media
 * line is rejected, too, unless SOATAG_RTP_MISMATCH(1) has been used.
 *
 * Corresponding tag taking a reference parameter is SOATAG_RTP_SELECT_REF().
 *
 * @sa SOATAG_RTP_MISMATCH(), SOATAG_RTP_SORT(), SOATAG_AUDIO_AUX()
 */
tag_typedef_t soatag_rtp_select = INTTAG_TYPEDEF(rtp_select);


/**@def SOATAG_AUDIO_AUX(x)
 *
 * The named audio codecs are considered auxiliary, that is, they are
 * considered as common codec only when they are the only codec listed on
 * the media line.
 *
 * When generating answer or second offer soa includes auxiliary audio
 * codecs in the list of codecs even if it is selecting only one codec or
 * common codecs.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    A string with whitespace separated list of codec names.
 *
 * @par Values
 *    E.g., "telephone-event cn".
 *
 * By default, there are no auxiliary audio codecs.
 *
 * Corresponding tag taking a reference parameter is
 * SOATAG_AUDIO_AUX_REF().
 *
 * @since New in @VERSION_1_12_2.
 *
 * @sa SOATAG_RTP_SELECT(), SOATAG_RTP_MISMATCH(), SOATAG_RTP_SORT()
 */
tag_typedef_t soatag_audio_aux = STRTAG_TYPEDEF(audio_aux);

/**@def SOATAG_RTP_SORT(x)
 *
 * When selecting the common codecs, soa can either select first local codec
 * supported by remote end, or first remote codec supported by local codecs.
 * The preference is indicated with ordering: the preferred codec is
 * first and so on.
 *
 * The auxiliary audio codecs (specified with SOATAG_AUDIO_AUX()) are listed
 * after other codecs.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    enum {
 *      #SOA_RTP_SORT_DEFAULT, #SOA_RTP_SORT_LOCAL, #SOA_RTP_SORT_REMOTE
 *    } \n
 *    (int in range 0..2)
 *
 * @par Values
 *    - #SOA_RTP_SORT_DEFAULT (0) - select by local preference
 *            if media is recvonly, remote preference othewise
 *    - #SOA_RTP_SORT_LOCAL   (1) - always select by local preference
 *    - #SOA_RTP_SORT_REMOTE  (2) - always select by remote preference
 *
 * The default value is #SOA_RTP_SORT_DEFAULT (0).
 *
 * Corresponding tag taking reference parameter is SOATAG_RTP_SORT_REF()
 *
 * @sa SOATAG_RTP_SELECT(), SOATAG_RTP_MISMATCH(), SOATAG_AUDIO_AUX()
*/
tag_typedef_t soatag_rtp_sort = INTTAG_TYPEDEF(rtp_sort);


/**@def SOATAG_RTP_MISMATCH(x)
 *
 * Accept media line even if the SDP negotation code determines that there
 * are no common codecs between local and remote media. Normally, if the soa
 * determines there are no common codecs, the media line is rejected.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    Boolean (int)
 *
 * @par Values
 *    0 - reject media if there are no common codecs \n
 *    1 (!= 0) - accept media even if there are no common codecs \n
 *
 * Default value is 0.
 *
 * Corresponding tag taking reference parameter is SOATAG_RTP_MISMATCH_REF()
 *
 * @sa SOATAG_RTP_SELECT(), SOATAG_RTP_MISMATCH(), SOATAG_AUDIO_AUX()
 */
tag_typedef_t soatag_rtp_mismatch = BOOLTAG_TYPEDEF(rtp_mismatch);


/**@def SOATAG_ACTIVE_AUDIO(x)
 *
 * Audio session status.
 *
 * @par Used with
 *
 * @par Parameter type
 *    enum { #SOA_ACTIVE_DISABLED, #SOA_ACTIVE_REJECTED,
 *           #SOA_ACTIVE_INACTIVE, #SOA_ACTIVE_SENDONLY,
 *           #SOA_ACTIVE_RECVONLY, #SOA_ACTIVE_SENDRECV }
 *
 * @par Values
 *    - #SOA_ACTIVE_REJECTED  (-8)
 *    - #SOA_ACTIVE_INACTIVE  (0)
 *    - #SOA_ACTIVE_SENDONLY  (1)
 *    - #SOA_ACTIVE_RECVONLY  (2)
 *    - #SOA_ACTIVE_SENDRECV  (3)
 *
 *  Corresponding tag taking reference parameter is SOATAG_ACTIVE_AUDIO_REF()
 *
 */
tag_typedef_t soatag_active_audio = INTTAG_TYPEDEF(active_audio);

/**@def SOATAG_ACTIVE_VIDEO(x)
 *
 * Video session status
 *
 * @par Used with
 *
 * @par Parameter type
 *    enum { #SOA_ACTIVE_DISABLED, #SOA_ACTIVE_REJECTED,
 *           #SOA_ACTIVE_INACTIVE, #SOA_ACTIVE_SENDONLY,
 *           #SOA_ACTIVE_RECVONLY, #SOA_ACTIVE_SENDRECV }
 *
 * @par Values
 *    - #SOA_ACTIVE_REJECTED  (-8)
 *    - #SOA_ACTIVE_INACTIVE  (0)
 *    - #SOA_ACTIVE_SENDONLY  (1)
 *    - #SOA_ACTIVE_RECVONLY  (2)
 *    - #SOA_ACTIVE_SENDRECV  (3)
 *
 * Corresponding tag taking reference parameter is SOATAG_ACTIVE_VIDEO_REF()
 */
tag_typedef_t soatag_active_video = INTTAG_TYPEDEF(active_video);

/**@def SOATAG_ACTIVE_IMAGE(x)
 *
 * Active image session status
 *
 * @par Used with
 *    #nua_i_active \n
 *    #nua_i_state \n
 *
 * @par Parameter type
 *    enum { #SOA_ACTIVE_DISABLED, #SOA_ACTIVE_REJECTED,
 *           #SOA_ACTIVE_INACTIVE, #SOA_ACTIVE_SENDONLY,
 *           #SOA_ACTIVE_RECVONLY, #SOA_ACTIVE_SENDRECV }
 *
 * @par Values
 *    - #SOA_ACTIVE_REJECTED  (-8)
 *    - #SOA_ACTIVE_INACTIVE  (0)
 *    - #SOA_ACTIVE_SENDONLY  (1)
 *    - #SOA_ACTIVE_RECVONLY  (2)
 *    - #SOA_ACTIVE_SENDRECV  (3)
 *
 * @par Parameter type
 *    enum { #SOA_ACTIVE_DISABLED, #SOA_ACTIVE_REJECTED,
 *           #SOA_ACTIVE_INACTIVE, #SOA_ACTIVE_SENDONLY,
 *           #SOA_ACTIVE_RECVONLY, #SOA_ACTIVE_SENDRECV }
 *
 * @par Values
 *    - #SOA_ACTIVE_REJECTED  (-8)
 *    - #SOA_ACTIVE_INACTIVE  (0)
 *    - #SOA_ACTIVE_SENDONLY  (1)
 *    - #SOA_ACTIVE_RECVONLY  (2)
 *    - #SOA_ACTIVE_SENDRECV  (3)
 *
 * Corresponding tag taking reference parameter is SOATAG_ACTIVE_IMAGE_REF()
 */
tag_typedef_t soatag_active_image = INTTAG_TYPEDEF(active_image);

/**@def SOATAG_ACTIVE_CHAT(x)
 *
 * Active chat session status.
 *
 * @par Used with
 *    #nua_i_active \n
 *    #nua_i_state \n
 *
 * @par Parameter type
 *    enum { #SOA_ACTIVE_DISABLED, #SOA_ACTIVE_REJECTED,
 *           #SOA_ACTIVE_INACTIVE, #SOA_ACTIVE_SENDONLY,
 *           #SOA_ACTIVE_RECVONLY, #SOA_ACTIVE_SENDRECV }
 *
 * @par Values
 *    - #SOA_ACTIVE_REJECTED  (-8)
 *    - #SOA_ACTIVE_INACTIVE  (0)
 *    - #SOA_ACTIVE_SENDONLY  (1)
 *    - #SOA_ACTIVE_RECVONLY  (2)
 *    - #SOA_ACTIVE_SENDRECV  (3)
 *
 * Corresponding tag taking reference parameter is SOATAG_ACTIVE_CHAT_REF()
 */
tag_typedef_t soatag_active_chat = INTTAG_TYPEDEF(active_chat);

/**@def SOATAG_SRTP_ENABLE(x)
 *
 * Enable SRTP
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    boolean (int)
 *
 * @par Values
 *    @c !=0 enable \n
 *    @c 0 disable
 *
 * Corresponding tag taking reference parameter is
 * SOATAG_SRTP_ENABLE_REF()
 *
 * @todo SRTP functionality is not implemented.
 */
tag_typedef_t soatag_srtp_enable = BOOLTAG_TYPEDEF(srtp_enable);

/**@def SOATAG_SRTP_CONFIDENTIALITY(x)
 *
 * Enable SRTP confidentiality negotiation.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    boolean (int)
 *
 * @par Values
 *    @c != 0 enable SRTP confidentiality \n
 *    @c 0 disable SRTP conidentiality
 *
 * Corresponding tag taking reference parameter is
 * SOATAG_SRTP_CONFIDENTIALITY_REF()
 *
 * @todo SRTP functionality is not implemented.
 */
tag_typedef_t soatag_srtp_confidentiality =
  BOOLTAG_TYPEDEF(srtp_confidentiality);

/**@def SOATAG_SRTP_INTEGRITY(x)
 *
 * Enable SRTP integrity protection
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    boolean (int)
 *
 * @par Values
 *    @c !=0 enable \n
 *    @c 0 disable
 *
 * Corresponding tag taking reference parameter is
 * SOATAG_SRTP_INTEGRITY_REF()
 *
 * @todo SRTP functionality is not implemented.
 */
tag_typedef_t soatag_srtp_integrity = BOOLTAG_TYPEDEF(srtp_integrity);

/**@def SOATAG_HOLD(x)
 *
 * Hold media stream or streams.
 *
 * The hold media stream will have the attribute a=sendonly (meaning that
 * some hold announcements or pause music is sent to the held party but that
 * the held party should not generate any media) or a=inactive (meaning that
 * no media is sent).
 *
 * When putting a SIP session on hold with sendonly, the application can
 * include, e.g., SOATAG_HOLD("audio") or SOATAG_HOLD("video") or
 * SOATAG_HOLD("audio, video") or SOATAG_HOLD("*") as @soa parameters. When
 * using inactive instead, the application should use "#" or
 * "audio=inactive" instead. When resuming the session, application should
 * include the tag SOATAG_HOLD(NULL).
 *
 * Note that last SOATAG_HOLD() in the tag list will override the
 * SOATAG_HOLD() tags before it.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    character string
 *
 * @par Values
 *    Comma-separated media name ("audio", "video") or wildcard ("*").
 *
 * Corresponding tag taking reference parameter is SOATAG_HOLD_REF()
 *
 * @sa soa_set_params(), nua_invite(), @ref nua_event_diagram_call_hold
 */
tag_typedef_t soatag_hold = STRTAG_TYPEDEF(hold);


/**@def SOATAG_ORDERED_USER(x)
 *
 * Take account strict ordering of user SDP m=lines. If user SDP has been
 * updated, the new media lines replace old ones even if the media type has
 * been changed. This allows the application to replace @b m=audio with
 * @b m=image/t38, for instance.
 *
 * @par Used with
 *    soa_set_params(), soa_get_params(), soa_get_paramlist() \n
 *
 * @par Parameter type
 *    boolean
 *
 * @par Values
 *   - false (0) - update session with user SDP based on media type
 *   - true (1) - update session with m= line in user SDP based on their order
 *
 * The default value is false and session are updated based on media types.
 *
 *
 * Corresponding tag taking a reference parameter is SOATAG_RTP_SELECT_REF().
 *
 * @sa @RFC3264 section 8.3.3, T.38
 *
 * @NEW_1_12_7.
 */
tag_typedef_t soatag_ordered_user = BOOLTAG_TYPEDEF(ordered_user);

tag_typedef_t soatag_reuse_rejected = BOOLTAG_TYPEDEF(reuse_rejected);
