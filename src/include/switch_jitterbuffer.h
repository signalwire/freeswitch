/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_jitterbuffer.h -- Audio/Video Jitter Buffer
 *
 */


#ifndef SWITCH_VIDDERBUFFER_H
#define SWITCH_VIDDERBUFFER_H

typedef enum {
	SJB_QUEUE_ONLY = (1 << 0)
} switch_jb_flag_t;

typedef enum {
	SJB_VIDEO = 0,
	SJB_AUDIO
} switch_jb_type_t;


SWITCH_BEGIN_EXTERN_C
SWITCH_DECLARE(switch_status_t) switch_jb_create(switch_jb_t **jbp, switch_jb_type_t type,
												 uint32_t min_frame_len, uint32_t max_frame_len, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_jb_set_frames(switch_jb_t *jb, uint32_t min_frame_len, uint32_t max_frame_len);
SWITCH_DECLARE(switch_status_t) switch_jb_peek_frame(switch_jb_t *jb, uint32_t ts, uint16_t seq, int peek, switch_frame_t *frame);
SWITCH_DECLARE(switch_status_t) switch_jb_get_frames(switch_jb_t *jb, uint32_t *min_frame_len, uint32_t *max_frame_len, uint32_t *cur_frame_len, uint32_t *highest_frame_len);
SWITCH_DECLARE(switch_status_t) switch_jb_destroy(switch_jb_t **jbp);
SWITCH_DECLARE(void) switch_jb_reset(switch_jb_t *jb);
SWITCH_DECLARE(void) switch_jb_debug_level(switch_jb_t *jb, uint8_t level);
SWITCH_DECLARE(int) switch_jb_frame_count(switch_jb_t *jb);
SWITCH_DECLARE(int) switch_jb_poll(switch_jb_t *jb);
SWITCH_DECLARE(switch_status_t) switch_jb_put_packet(switch_jb_t *jb, switch_rtp_packet_t *packet, switch_size_t len);
SWITCH_DECLARE(switch_size_t) switch_jb_get_last_read_len(switch_jb_t *jb);
SWITCH_DECLARE(switch_status_t) switch_jb_get_packet(switch_jb_t *jb, switch_rtp_packet_t *packet, switch_size_t *len);
SWITCH_DECLARE(uint32_t) switch_jb_pop_nack(switch_jb_t *jb);
SWITCH_DECLARE(switch_status_t) switch_jb_get_packet_by_seq(switch_jb_t *jb, uint16_t seq, switch_rtp_packet_t *packet, switch_size_t *len);
SWITCH_DECLARE(void) switch_jb_set_session(switch_jb_t *jb, switch_core_session_t *session);
SWITCH_DECLARE(void) switch_jb_ts_mode(switch_jb_t *jb, uint32_t samples_per_frame, uint32_t samples_per_second);
SWITCH_DECLARE(void) switch_jb_set_flag(switch_jb_t *jb, switch_jb_flag_t flag);
SWITCH_DECLARE(void) switch_jb_clear_flag(switch_jb_t *jb, switch_jb_flag_t flag);

SWITCH_END_EXTERN_C
#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
