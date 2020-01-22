
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
 * switch_core_media.c -- Core Media
 *
 */

#ifndef SWITCH_CORE_MEDIA_H
#define SWITCH_CORE_MEDIA_H


#include <switch.h>
#include <switch_msrp.h>

SWITCH_BEGIN_EXTERN_C

#define SWITCH_MAX_CAND_ACL 25
#define SWITCH_NO_CRYPTO_TAG -1

typedef enum {
	DTMF_AUTO,
	DTMF_2833,
	DTMF_INFO,
	DTMF_NONE
} switch_core_media_dtmf_t;

typedef enum {
	SM_NDLB_ALLOW_BAD_IANANAME = (1 << 0),
	SM_NDLB_ALLOW_NONDUP_SDP = (1 << 1),
	SM_NDLB_ALLOW_CRYPTO_IN_AVP = (1 << 2),
	SM_NDLB_DISABLE_SRTP_AUTH = (1 << 3),
	SM_NDLB_SENDRECV_IN_SESSION = (1 << 4),
	SM_NDLB_NEVER_PATCH_REINVITE = (1 << 5)
} switch_core_media_NDLB_t;

typedef enum {
	SCMF_RUNNING,
	SCMF_DISABLE_TRANSCODING,
	SCMF_AUTOFIX_TIMING,
	SCMF_CODEC_GREEDY,
	SCMF_CODEC_SCROOGE,
	SCMF_DISABLE_HOLD,
	SCMF_SUPPRESS_CNG,
	SCMF_DISABLE_RTP_AUTOADJ,
	SCMF_PASS_RFC2833,
	SCMF_AUTOFLUSH,
	SCMF_REWRITE_TIMESTAMPS,
	SCMF_RTP_AUTOFLUSH_DURING_BRIDGE,
	SCMF_MULTI_ANSWER_AUDIO,
	SCMF_MULTI_ANSWER_VIDEO,
	SCMF_RECV_SDP,
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

	switch_core_media_NDLB_t ndlb;
	switch_rtp_bug_flag_t auto_rtp_bugs;

	char *inbound_codec_string;
	char *outbound_codec_string;

	char *timer_name;

	char *remote_sdp_str;
	char *early_sdp;
	char *local_sdp_str;
	char *last_sdp_str;
	char *last_sdp_response;
	char *prev_sdp_str;
	char *prev_sdp_response;

	char *stun_ip;
	switch_port_t stun_port;
	uint32_t stun_flags;

	char *jb_msec;

	switch_core_media_vflag_t vflags;

	switch_rtp_bug_flag_t manual_rtp_bugs;
	switch_rtp_bug_flag_t manual_video_rtp_bugs;
	switch_rtp_bug_flag_t manual_text_rtp_bugs;

	char *rtcp_audio_interval_msec;
	char *rtcp_video_interval_msec;
	char *rtcp_text_interval_msec;


	char *extrtpip;
	char *rtpip;
	char *rtpip4;
	char *rtpip6;


	char *remote_ip;
	int remote_port;

	char *extsipip;
	char *local_network;

	char *sipip;

	char *sdp_username;

	switch_payload_t te;
	switch_payload_t recv_te;
	unsigned long te_rate;
	unsigned long cng_rate;

	char *adv_sdp_audio_ip;

    int num_codecs;
	int hold_laps;
	switch_core_media_dtmf_t dtmf_type;
	switch_payload_t cng_pt;


	/* a core_video_thread will be started automatically
	   when uses rtp based media,
	   external_video_source should be set to SWITCH_TRUE and
	   switch_core_session_start_video_thread()
	   should be explicitly called to start the video thread
	   if uses the media handle for non-rtp based media
	*/

	switch_bool_t external_video_source;

	uint32_t video_key_freq;
	uint32_t video_key_first;

	switch_thread_t *video_write_thread;

} switch_core_media_params_t;

static inline const char *switch_media_type2str(switch_media_type_t type)
{
	switch(type) {
	case SWITCH_MEDIA_TYPE_AUDIO:
		return "audio";
	case SWITCH_MEDIA_TYPE_VIDEO:
		return "video";
	default:
		return "!ERR";

	}
}


SWITCH_DECLARE(switch_status_t) switch_media_handle_create(switch_media_handle_t **smhp, switch_core_session_t *session, switch_core_media_params_t *params);
SWITCH_DECLARE(void) switch_media_handle_destroy(switch_core_session_t *session);
SWITCH_DECLARE(switch_media_handle_t *) switch_core_session_get_media_handle(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_session_clear_media_handle(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_session_media_handle_ready(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_media_handle_set_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag);
SWITCH_DECLARE(void) switch_media_handle_clear_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag);
SWITCH_DECLARE(int32_t) switch_media_handle_test_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag);
SWITCH_DECLARE(void) switch_media_handle_set_media_flags(switch_media_handle_t *smh, switch_core_media_flag_t flags[]);
SWITCH_DECLARE(void) switch_core_session_check_outgoing_crypto(switch_core_session_t *session);
SWITCH_DECLARE(const char *) switch_core_session_local_crypto_key(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(int) switch_core_session_check_incoming_crypto(switch_core_session_t *session,
															  const char *varname,
															  switch_media_type_t type, const char *crypto, int crypto_tag, switch_sdp_type_t sdp_type);


SWITCH_DECLARE(uint32_t) switch_core_media_get_video_fps(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_set_rtp_session(switch_core_session_t *session, switch_media_type_t type, switch_rtp_t *rtp_session);

SWITCH_DECLARE(const char *)switch_core_media_get_codec_string(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_parse_rtp_bugs(switch_rtp_bug_flag_t *flag_pole, const char *str);
SWITCH_DECLARE(switch_status_t) switch_core_media_add_crypto(switch_core_session_t *session, switch_secure_settings_t *ssec, switch_rtp_crypto_direction_t direction);
SWITCH_DECLARE(switch_t38_options_t *) switch_core_media_extract_t38_options(switch_core_session_t *session, const char *r_sdp);
SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash(switch_core_session_t *session);
SWITCH_DECLARE(const char *) switch_core_media_get_zrtp_hash(switch_core_session_t *session, switch_media_type_t type, switch_bool_t local);
SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session);
SWITCH_DECLARE(int) switch_core_media_toggle_hold(switch_core_session_t *session, int sendonly);
SWITCH_DECLARE(void) switch_core_media_reset_t38(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_copy_t38_options(switch_t38_options_t *t38_options, switch_core_session_t *session);
SWITCH_DECLARE(uint8_t) switch_core_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, uint8_t *proceed, switch_sdp_type_t sdp_type);
SWITCH_DECLARE(switch_status_t) switch_core_media_set_video_codec(switch_core_session_t *session, int force);
SWITCH_DECLARE(switch_status_t) switch_core_media_set_codec(switch_core_session_t *session, int force, uint32_t codec_flags);
SWITCH_DECLARE(void) switch_core_media_check_video_codecs(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_read_frame(switch_core_session_t *session, switch_frame_t **frame,
															 switch_io_flag_t flags, int stream_id, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_media_write_frame(switch_core_session_t *session,
															  switch_frame_t *frame, switch_io_flag_t flags, int stream_id, switch_media_type_t type);
SWITCH_DECLARE(int) switch_core_media_check_nat(switch_media_handle_t *smh, const char *network_ip);

SWITCH_DECLARE(switch_status_t) switch_core_media_choose_port(switch_core_session_t *session, switch_media_type_t type, int force);
SWITCH_DECLARE(switch_status_t) switch_core_media_choose_ports(switch_core_session_t *session, switch_bool_t audio, switch_bool_t video);
SWITCH_DECLARE(void) switch_core_media_check_dtmf_type(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_absorb_sdp(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_proxy_remote_addr(switch_core_session_t *session, const char *sdp_str);
SWITCH_DECLARE(void) switch_core_media_parse_media_flags(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_deactivate_rtp(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_activate_rtp(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_ext_address_lookup(switch_core_session_t *session, char **ip, switch_port_t *port, const char *sourceip);
SWITCH_DECLARE(switch_status_t) switch_core_media_process_t38_passthru(switch_core_session_t *session,
																	   switch_core_session_t *other_session, switch_t38_options_t *t38_options);
SWITCH_DECLARE(void) switch_core_media_gen_local_sdp(switch_core_session_t *session, switch_sdp_type_t sdp_type,
													 const char *ip, switch_port_t port, const char *sr, int force);
SWITCH_DECLARE(void)switch_core_media_set_local_sdp(switch_core_session_t *session, const char *sdp_str, switch_bool_t dup);
SWITCH_DECLARE(void) switch_core_media_patch_sdp(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_set_udptl_image_sdp(switch_core_session_t *session, switch_t38_options_t *t38_options, int insist);
SWITCH_DECLARE(switch_core_media_params_t *) switch_core_media_get_mparams(switch_media_handle_t *smh);
SWITCH_DECLARE(void) switch_core_media_prepare_codecs(switch_core_session_t *session, switch_bool_t force);
SWITCH_DECLARE(void) switch_core_media_start_udptl(switch_core_session_t *session, switch_t38_options_t *t38_options);
SWITCH_DECLARE(void) switch_core_media_hard_mute(switch_core_session_t *session, switch_bool_t on);
SWITCH_DECLARE(switch_status_t) switch_core_media_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);



SWITCH_DECLARE(void) switch_core_media_break(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(void) switch_core_media_kill_socket(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_media_queue_rfc2833(switch_core_session_t *session, switch_media_type_t type, const switch_dtmf_t *dtmf);
SWITCH_DECLARE(switch_status_t) switch_core_media_queue_rfc2833_in(switch_core_session_t *session, switch_media_type_t type, const switch_dtmf_t *dtmf);
SWITCH_DECLARE(uint8_t) switch_core_media_ready(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(void) switch_core_media_set_telephony_event(switch_core_session_t *session, switch_media_type_t type, switch_payload_t te);
SWITCH_DECLARE(void) switch_core_media_set_telephony_recv_event(switch_core_session_t *session, switch_media_type_t type, switch_payload_t te);
SWITCH_DECLARE(switch_rtp_stats_t *) switch_core_media_stats(switch_core_session_t *session, switch_media_type_t type, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_core_media_udptl_mode(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_bool_t) switch_core_media_check_udptl_mode(switch_core_session_t *session, switch_media_type_t type);

SWITCH_DECLARE(void) switch_core_media_set_rtp_flag(switch_core_session_t *session, switch_media_type_t type, switch_rtp_flag_t flag);
SWITCH_DECLARE(void) switch_core_media_clear_rtp_flag(switch_core_session_t *session, switch_media_type_t type, switch_rtp_flag_t flag);
SWITCH_DECLARE(switch_jb_t *) switch_core_media_get_jb(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_rtp_stats_t *) switch_core_media_get_stats(switch_core_session_t *session, switch_media_type_t type, switch_memory_pool_t *pool);


SWITCH_DECLARE(void) switch_core_media_set_sdp_codec_string(switch_core_session_t *session, const char *r_sdp, switch_sdp_type_t sdp_type);
SWITCH_DECLARE(void) switch_core_media_merge_sdp_codec_string(switch_core_session_t *session, const char *r_sdp,
															  switch_sdp_type_t sdp_type, const char *codec_string);
							  		

SWITCH_DECLARE(void) switch_core_media_reset_autofix(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(void) switch_core_media_check_outgoing_proxy(switch_core_session_t *session, switch_core_session_t *o_session);
SWITCH_DECLARE(switch_status_t) switch_core_media_codec_chosen(switch_core_session_t *session, switch_media_type_t media);
SWITCH_DECLARE (void) switch_core_media_recover_session(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_add_ice_acl(switch_core_session_t *session, switch_media_type_t type, const char *acl_name);
SWITCH_DECLARE(void) switch_core_session_set_ice(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_clear_ice(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_pause(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_resume(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_init(void);
SWITCH_DECLARE(void) switch_core_media_deinit(void);
SWITCH_DECLARE(void) switch_core_media_set_stats(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_sync_stats(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_session_wake_video_thread(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_session_clear_crypto(switch_core_session_t *session);

SWITCH_DECLARE(switch_status_t) switch_core_session_get_payload_code(switch_core_session_t *session,
																	 switch_media_type_t type,
																	 const char *iananame,
																	 uint32_t rate,
																	 const char *fmtp_in,
																	 switch_payload_t *ptP,
																	 switch_payload_t *recv_ptP,
																	 char **fmtpP);

SWITCH_DECLARE(payload_map_t *) switch_core_media_add_payload_map(switch_core_session_t *session,
																  switch_media_type_t type,
																  const char *name,
																  const char *modname,
																  const char *fmtp,
																  switch_sdp_type_t sdp_type,
																  uint32_t pt,
																  uint32_t rate,
																  uint32_t ptime,
																  uint32_t channels,
																  uint8_t negotiated);

SWITCH_DECLARE(switch_status_t) switch_core_media_check_autoadj(switch_core_session_t *session);
SWITCH_DECLARE(switch_rtp_crypto_key_type_t) switch_core_media_crypto_str2type(const char *str);
SWITCH_DECLARE(const char *) switch_core_media_crypto_type2str(switch_rtp_crypto_key_type_t type);
SWITCH_DECLARE(int) switch_core_media_crypto_keysalt_len(switch_rtp_crypto_key_type_t type);
SWITCH_DECLARE(int) switch_core_media_crypto_salt_len(switch_rtp_crypto_key_type_t type);
SWITCH_DECLARE(char *) switch_core_media_filter_sdp(const char *sdp, const char *cmd, const char *arg);
SWITCH_DECLARE(char *) switch_core_media_process_sdp_filter(const char *sdp, const char *cmd_buf, switch_core_session_t *session);


SWITCH_DECLARE(switch_status_t) switch_core_media_codec_control(switch_core_session_t *session,
																switch_media_type_t mtype,
																switch_io_type_t iotype,
																switch_codec_control_command_t cmd,
																switch_codec_control_type_t ctype,
																void *cmd_data,
																switch_codec_control_type_t atype,
																void *cmd_arg,
																switch_codec_control_type_t *rtype,
																void **ret_data);

SWITCH_DECLARE(switch_bool_t) switch_core_media_codec_get_cap(switch_core_session_t *session,
																switch_media_type_t mtype,
																switch_codec_flag_t flag);


#define switch_core_media_gen_key_frame(_session) switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(_session), SWITCH_LOG_DEBUG1, "%s Send KeyFrame\n", switch_core_session_get_name(_session)); \
	switch_core_media_codec_control(_session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_IO_WRITE, SCC_VIDEO_GEN_KEYFRAME, SCCT_NONE, NULL, SCCT_NONE, NULL, NULL, NULL)

#define switch_core_media_write_bandwidth(_session, _val) switch_core_media_codec_control(_session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_IO_WRITE, \
																						  SCC_VIDEO_BANDWIDTH, SCCT_STRING, _val, SCCT_NONE, NULL, NULL, NULL)


SWITCH_DECLARE(switch_timer_t *) switch_core_media_get_timer(switch_core_session_t *session, switch_media_type_t mtype);
SWITCH_DECLARE(void) switch_core_media_start_engine_function(switch_core_session_t *session, switch_media_type_t type, switch_engine_function_t engine_function, void *user_data);
SWITCH_DECLARE(void) switch_core_media_end_engine_function(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_session_start_video_thread(switch_core_session_t *session);
SWITCH_DECLARE(int) switch_core_media_check_engine_function(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(void) switch_core_session_video_reinit(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_read_lock_unlock(switch_core_session_t *session, switch_media_type_t type, switch_bool_t lock);

#define switch_core_media_read_lock(_s, _t) switch_core_media_read_lock_unlock(_s, _t, SWITCH_TRUE)
#define switch_core_media_read_unlock(_s, _t) switch_core_media_read_lock_unlock(_s, _t, SWITCH_FALSE)

SWITCH_DECLARE(void) switch_core_session_stop_media(switch_core_session_t *session);
SWITCH_DECLARE(switch_media_flow_t) switch_core_session_media_flow(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_media_get_vid_params(switch_core_session_t *session, switch_vid_params_t *vid_params);
SWITCH_DECLARE(void) switch_core_session_write_blank_video(switch_core_session_t *session, uint32_t ms);
SWITCH_DECLARE(switch_status_t) switch_core_media_lock_video_file(switch_core_session_t *session, switch_rw_t rw);
SWITCH_DECLARE(switch_status_t) switch_core_media_unlock_video_file(switch_core_session_t *session, switch_rw_t rw);
SWITCH_DECLARE(switch_status_t) switch_core_media_set_video_file(switch_core_session_t *session, switch_file_handle_t *fh, switch_rw_t rw);
SWITCH_DECLARE(switch_file_handle_t *) switch_core_media_get_video_file(switch_core_session_t *session, switch_rw_t rw);
SWITCH_DECLARE(switch_bool_t) switch_core_session_in_video_thread(switch_core_session_t *session);
SWITCH_DECLARE(switch_bool_t) switch_core_media_check_dtls(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_media_set_outgoing_bitrate(switch_core_session_t *session, switch_media_type_t type, uint32_t bitrate);
SWITCH_DECLARE(uint32_t) switch_core_media_get_orig_bitrate(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(void) switch_core_media_set_media_bw_mult(switch_core_session_t *session, float mult);
SWITCH_DECLARE(float) switch_core_media_get_media_bw_mult(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_core_media_reset_jb(switch_core_session_t *session, switch_media_type_t type);
SWITCH_DECLARE(switch_status_t) switch_core_session_wait_for_video_input_params(switch_core_session_t *session, uint32_t timeout_ms);


SWITCH_DECLARE(switch_status_t) switch_core_session_set_text_read_callback(switch_core_session_t *session,
																		   switch_core_text_thread_callback_func_t func, void *user_data);
SWITCH_DECLARE(switch_status_t) switch_core_session_text_read_callback(switch_core_session_t *session, switch_frame_t *frame);
SWITCH_DECLARE(switch_status_t) switch_core_session_read_text_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags,
																	int stream_id);

SWITCH_DECLARE(switch_status_t) switch_core_session_write_text_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags,
																	 int stream_id);

SWITCH_DECLARE(switch_status_t) switch_rtp_text_factory_create(switch_rtp_text_factory_t **tfP, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_rtp_text_factory_destroy(switch_rtp_text_factory_t **tfP);

SWITCH_DECLARE(switch_status_t) switch_core_session_print(switch_core_session_t *session, const char *data);
SWITCH_DECLARE(switch_status_t) switch_core_session_printf(switch_core_session_t *session, const char *fmt, ...);

SWITCH_DECLARE(switch_msrp_session_t *) switch_core_media_get_msrp_session(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_core_media_set_smode(switch_core_session_t *session, switch_media_type_t type, switch_media_flow_t smode, switch_sdp_type_t sdp_type);
																		
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

