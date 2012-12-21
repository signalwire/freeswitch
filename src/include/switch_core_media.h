/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * switch_core_media.c -- Core Media
 *
 */

#ifndef SWITCH_CORE_MEDIA_H
#define SWITCH_CORE_MEDIA_H


#include <sdp.h>
#include <switch.h>

SWITCH_BEGIN_EXTERN_C

typedef enum {
	DTMF_2833,
	DTMF_INFO,
	DTMF_NONE
} switch_core_media_dtmf_t;

typedef enum {
	SM_NDLB_ALLOW_BAD_IANANAME = (1 << 0),
	SM_NDLB_ALLOW_NONDUP_SDP = (1 << 1),
	SM_NDLB_ALLOW_CRYPTO_IN_AVP = (1 << 2),
	SM_NDLB_DISABLE_SRTP_AUTH = (1 << 3)
} switch_core_media_NDLB_t;

typedef enum {
	SCMF_RUNNING,
	SCMF_DISABLE_TRANSCODING,
	SCMF_AUTOFIX_TIMING,
	SCMF_CODEC_GREEDY,
	SCMF_CODEC_SCROOGE,
	SCMF_DISABLE_HOLD,
	SCMF_RENEG_ON_HOLD,
	SCMF_RENEG_ON_REINVITE,
	SCMF_T38_PASSTHRU,
	SCMF_LIBERAL_DTMF,
	SCMF_SUPPRESS_CNG,
	SCMF_DISABLE_RTP_AUTOADJ,
	SCMF_PASS_RFC2833,
	SCMF_AUTOFLUSH,
	SCMF_REWRITE_TIMESTAMPS,
	SCMF_MAX
} switch_core_media_flag_t;

struct switch_media_handle_s;

typedef enum {
	STUN_FLAG_SET = (1 << 0),
	STUN_FLAG_PING = (1 << 1),
	STUN_FLAG_FUNNY = (1 << 2)
} STUNFLAGS;

typedef enum {
	VAD_IN = (1 << 0),
	VAD_OUT = (1 << 1)
} switch_core_media_vflag_t;

typedef struct switch_core_media_params_s {
	uint32_t rtp_timeout_sec;
	uint32_t rtp_hold_timeout_sec;
	uint32_t dtmf_delay;
	uint32_t codec_flags;
	int reinvite;
	switch_core_media_NDLB_t ndlb;
	switch_rtp_bug_flag_t auto_rtp_bugs;

	char *inbound_codec_string;
	char *outbound_codec_string;

	char *timer_name;

	char *remote_sdp_str;
	char *early_sdp;
	char *local_sdp_str;
	char *last_sdp_str;

	char *stun_ip;
	switch_port_t stun_port;
	uint32_t stun_flags;

	char *jb_msec;

	switch_core_media_vflag_t vflags;

	switch_rtp_bug_flag_t manual_rtp_bugs;
	switch_rtp_bug_flag_t manual_video_rtp_bugs;

	char *rtcp_audio_interval_msec;
	char *rtcp_video_interval_msec;


	char *extrtpip;
	char *rtpip;

	char *remote_ip;
	int remote_port;

	char *extsipip;
	char *local_network;
	

} switch_core_media_params_t;



SWITCH_DECLARE(switch_status_t) switch_media_handle_create(switch_media_handle_t **smhp, switch_core_session_t *session, switch_core_media_params_t *params);
SWITCH_DECLARE(switch_media_handle_t *) switch_core_session_get_media_handle(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_session_clear_media_handle(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_session_media_handle_ready(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_media_handle_set_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag);
SWITCH_DECLARE(void) switch_media_handle_clear_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag);
SWITCH_DECLARE(int32_t) switch_media_handle_test_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag);
SWITCH_DECLARE(void) switch_media_handle_set_media_flags(switch_media_handle_t *smh, switch_core_media_flag_t flags[]);
SWITCH_DECLARE(void) switch_core_session_check_outgoing_crypto(switch_core_session_t *session, const char *sec_var);
SWITCH_DECLARE(const char *) switch_core_session_local_crypto_key(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(int) switch_core_session_check_incoming_crypto(switch_core_session_t *session, 
															  const char *varname,
															  switch_media_type_t type, const char *crypto, int crypto_tag);

SWITCH_DECLARE(void) switch_core_session_apply_crypto(switch_core_session_t *session, switch_media_type_t type, const char *varname);
SWITCH_DECLARE(void) switch_core_session_get_recovery_crypto_key(switch_core_session_t *session, switch_media_type_t type, const char *varname);

SWITCH_DECLARE(void) switch_core_media_set_rtp_session(switch_core_session_t *session, switch_media_type_t type, switch_rtp_t *rtp_session);

SWITCH_DECLARE(const char *)switch_core_media_get_codec_string(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_parse_rtp_bugs(switch_rtp_bug_flag_t *flag_pole, const char *str);
SWITCH_DECLARE(switch_t38_options_t *) switch_core_media_process_udptl(switch_core_session_t *session, sdp_session_t *sdp, sdp_media_t *m);
SWITCH_DECLARE(switch_t38_options_t *) switch_core_media_extract_t38_options(switch_core_session_t *session, const char *r_sdp);
SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_find_zrtp_hash(switch_core_session_t *session, sdp_session_t *sdp);
SWITCH_DECLARE(const char *) switch_core_media_get_zrtp_hash(switch_core_session_t *session, switch_media_type_t type, switch_bool_t local);
SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session);
SWITCH_DECLARE(int) switch_core_media_toggle_hold(switch_core_session_t *session, int sendonly);
SWITCH_DECLARE(void) switch_core_media_copy_t38_options(switch_t38_options_t *t38_options, switch_core_session_t *session);
SWITCH_DECLARE(uint8_t) switch_core_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, uint8_t *proceed, int reinvite, int codec_flags, switch_payload_t default_te);
SWITCH_DECLARE(switch_status_t) switch_core_media_set_video_codec(switch_core_session_t *session, int force);
SWITCH_DECLARE(switch_status_t) switch_core_media_set_codec(switch_core_session_t *session, int force, uint32_t codec_flags);
SWITCH_DECLARE(void) switch_core_media_check_video_codecs(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_read_frame(switch_core_session_t *session, switch_frame_t **frame,
															 switch_io_flag_t flags, int stream_id, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_media_write_frame(switch_core_session_t *session, 
															  switch_frame_t *frame, switch_io_flag_t flags, int stream_id, switch_media_type_t type);
SWITCH_DECLARE(int) switch_core_media_check_nat(switch_media_handle_t *smh, const char *network_ip);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
