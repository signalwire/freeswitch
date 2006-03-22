/*
  The oSIP library implements the Session Initiation Protocol (SIP -rfc3261-)
  Copyright (C) 2001,2002,2003,2004  Aymeric MOIZARD jack@atosc.org
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __SDP_RFC3264_H__
#define __SDP_RFC3264_H__

#include <osipparser2/osip_list.h>
#include <osipparser2/sdp_message.h>

/**
 * @file osip_rfc3264.h
 * @brief oSIP sdp negotiation facility.
 */

/**
 * @defgroup oSIP_rfc3264 oSIP sdp negotiation facility.
 * @ingroup osip2_sdp
 * @{
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Structure to hold support for codecs.
 * @struct osip_rfc3264
 */
  struct osip_rfc3264;

/**
 * Maximum number of supported audio payload.
 * @def MAX_AUDIO_CODECS
 */
#define MAX_AUDIO_CODECS   100
/**
 * Maximum number of supported video payload.
 * @def MAX_VIDEO_CODECS
 */
#define MAX_VIDEO_CODECS   100
/**
 * Maximum number of supported t38 config.
 * @def MAX_T38_CODECS
 */
#define MAX_T38_CODECS       2
/**
 * Maximum number of supported application config.
 * @def MAX_APP_CODECS
 */
#define MAX_APP_CODECS     100

/**
 * Initialize negotiation facility..
 * @param config The element to work on.
 */
  int osip_rfc3264_init (struct osip_rfc3264 **config);

/**
 * Free negotiation facility.
 * @param config The element to work on.
 */
  void osip_rfc3264_free (struct osip_rfc3264 *config);

/**
 * Test if a media exist in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element.
 */
  int osip_rfc3264_endof_media (struct osip_rfc3264 *config, int pos);

/**
 * Get a media from the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to get.
 */
  sdp_media_t *osip_rfc3264_get (struct osip_rfc3264 *config, int pos);

/**
 * Remove a media from the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
  int osip_rfc3264_remove (struct osip_rfc3264 *config, int pos);

/**
 * Remove all medias from the configuration.
 * @param config The element to work on.
 */
  int osip_rfc3264_reset_media (struct osip_rfc3264 *config);

/**
 * Add a media in the configuration.
 * @param config The element to work on.
 * @param med The media element to add.
 * @param pos The index of the media element to add.
 */
  int osip_rfc3264_add_audio_media (struct osip_rfc3264 *config,
                                    sdp_media_t * med, int pos);

/**
 * Remove a media in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
  int osip_rfc3264_del_audio_media (struct osip_rfc3264 *config, int pos);

/**
 * Add a media (for T.38) in the configuration.
 * @param config The element to work on.
 * @param med The media element to add.
 * @param pos The index of the media element to add.
 */
  int osip_rfc3264_add_t38_media (struct osip_rfc3264 *config,
                                  sdp_media_t * med, int pos);

/**
 * Remove a media (for T.38) in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
  int osip_rfc3264_del_t38_media (struct osip_rfc3264 *config, int pos);

/**
 * Add a media (for video) in the configuration.
 * @param config The element to work on.
 * @param med The media element to add.
 * @param pos The index of the media element to add.
 */
  int osip_rfc3264_add_video_media (struct osip_rfc3264 *config,
                                    sdp_media_t * med, int pos);

/**
 * Remove a media in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
  int osip_rfc3264_del_video_media (struct osip_rfc3264 *config, int pos);


/**
 * Search for support of a special codec.
 * @param config The element to work on.
 */
  sdp_media_t *osip_rfc3264_find_audio (struct osip_rfc3264 *config,
                                        char *payload, char *rtpmap);

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 * @param rtpmap The rtpmap for the payload.
 */
  sdp_media_t *osip_rfc3264_find_video (struct osip_rfc3264 *config,
                                        char *payload, char *rtpmap);

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 */
  sdp_media_t *osip_rfc3264_find_t38 (struct osip_rfc3264 *config, char *payload);

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 */
  sdp_media_t *osip_rfc3264_find_app (struct osip_rfc3264 *config, char *payload);

/**
 * Compare remote sdp packet against local supported media.
 *    Only one media line is checked.
 *
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param audio_tab The local list of media supported by both side.
 * @param video_tab The local list of media supported by both side.
 * @param t38_tab The local list of media supported by both side.
 * @param app_tab The local list of media supported by both side.
 * @param pos_media The position of the media line to match.
 */
  int osip_rfc3264_match (struct osip_rfc3264 *config,
                          sdp_message_t * remote_sdp,
                          sdp_media_t * audio_tab[],
                          sdp_media_t * video_tab[],
                          sdp_media_t * t38_tab[],
                          sdp_media_t * app_tab[], int pos_media);

/**
 * Compare remote sdp packet against local supported media for audio.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param audio_tab The local list of media supported by both side.
 */
  int osip_rfc3264_match_audio (struct osip_rfc3264 *config,
                                sdp_message_t * remote_sdp,
                                sdp_media_t * remote_med,
                                sdp_media_t * audio_tab[]);

/**
 * Compare remote sdp packet against local supported media for video.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param video_tab The local list of media supported by both side.
 */
  int osip_rfc3264_match_video (struct osip_rfc3264 *config,
                                sdp_message_t * remote_sdp,
                                sdp_media_t * remote_med,
                                sdp_media_t * video_tab[]);

/**
 * Compare remote sdp packet against local supported media for t38.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param t38_tab The local list of media supported by both side.
 */
  int osip_rfc3264_match_t38 (struct osip_rfc3264 *config,
                              sdp_message_t * remote_sdp,
                              sdp_media_t * remote_med, sdp_media_t * t38_tab[]);

/**
 * Compare remote sdp packet against local supported media for application.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param app_tab The local list of media supported by both side.
 */
  int osip_rfc3264_match_app (struct osip_rfc3264 *config,
                              sdp_message_t * remote_sdp,
                              sdp_media_t * remote_med, sdp_media_t * app_tab[]);


/**
 * Prepare an uncomplete answer.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param local_sdp The local SDP packet to prepare.
 * @param length The local SDP packet's length.
 */
  int osip_rfc3264_prepare_answer (struct osip_rfc3264 *config,
                                   sdp_message_t * remote_sdp,
                                   char *local_sdp, int length);

/**
 * Agree to support a specific codec.
 *   This method should be called for each codec returned by
 *   osip_rfc3264_match(...) that the calle agree to support.
 *
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param local_sdp The local SDP packet to complete.
 * @param med One of the media returned by osip_rfc3264_match.
 * @param mline The position of the media line to complete.
 */
  int
    osip_rfc3264_complete_answer (struct osip_rfc3264 *config,
                                  sdp_message_t * remote_sdp,
                                  sdp_message_t * local_sdp,
                                  sdp_media_t * med, int mline);

/**
 * Agree to support a specific codec.
 *   This method should be called for each codec returned by
 *   osip_rfc3264_match(...)
 *
 * @param config The element to work on.
 * @param med One of the media returned by osip_rfc3264_match
 * @param remote_sdp The remote SDP packet.
 * @param local_sdp The local SDP packet to prepare.
 */
  int osip_rfc3264_accept_codec (struct osip_rfc3264 *config,
                                 sdp_media_t * med,
                                 sdp_message_t * remote_sdp,
                                 sdp_message_t * local_sdp);


/**
 * List supported codecs. (for debugging purpose only)
 *
 * @param config The element to work on.
 */
  int __osip_rfc3264_print_codecs (struct osip_rfc3264 *config);


#ifdef __cplusplus
}
#endif

#endif
