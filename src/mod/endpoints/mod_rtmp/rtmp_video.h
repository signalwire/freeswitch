/*
 * mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2015, Seven Du.
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is rtmp_video for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Seven Du,
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 * Da Xiong <wavecb@gmail.com>
 *
 * rtmp_video.h -- RTMP video
 *
 */


#include "amf0.h"
#include "mod_rtmp.h"

#define MAX_RTP_PAYLOAD_SIZE 1400

void rtmp2rtp_helper_init(rtmp2rtp_helper_t *helper);
void rtp2rtmp_helper_init(rtp2rtmp_helper_t *helper);
void rtmp2rtp_helper_destroy(rtmp2rtp_helper_t *helper);
void rtp2rtmp_helper_destroy(rtp2rtmp_helper_t *helper);
switch_status_t on_rtmp_tech_init(switch_core_session_t *session, rtmp_private_t *tech_pvt);
switch_status_t on_rtmp_destroy(rtmp_private_t *tech_pvt);

/*Rtmp packet to rtp frame*/
switch_status_t rtmp_rtmp2rtpH264(rtmp2rtp_helper_t  *read_helper, uint8_t* data, uint32_t len);
switch_status_t rtmp_rtp2rtmpH264(rtp2rtmp_helper_t *helper, switch_frame_t *frame);
switch_status_t rtmp_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
switch_status_t rtmp_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
