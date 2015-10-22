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
 *
 * mod_esf.c -- Extra SIP Functionality
 *
 */
#include <switch.h>
#include <g711.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_esf_load);
SWITCH_MODULE_DEFINITION(mod_esf, mod_esf_load, NULL, NULL);



#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#else
#pragma pack(1)
#endif


struct polycom_packet {
	switch_byte_t op;
	switch_byte_t channel;
	uint32_t serno;
	uint8_t cid_len;
	unsigned char cid[13];
};

typedef struct polycom_packet polycom_packet_t;

typedef struct {
	switch_byte_t codec;
	switch_byte_t flags;
	uint32_t seq;
} polycom_audio_header_t;


typedef struct polycom_alert_packet {
	polycom_packet_t header;
	polycom_audio_header_t audio_header;
	uint8_t data[];
} polycom_alert_packet_t;


#ifdef _MSC_VER
#pragma pack(pop, r1)
#else
#pragma pack()
#endif

struct ls_control_packet {
	uint32_t unique_id;
	uint32_t command;
	uint32_t ip;
	uint32_t port;
};

typedef struct ls_control_packet ls_control_packet_t;

typedef enum {
	LS_START_BCAST = 6,
	LS_STOP_BCAST = 7
} ls_command_t;

typedef enum {
	SEND_TYPE_UNKNOWN = 0,
	SEND_TYPE_RTP = 1,
	SEND_TYPE_NOMEDIA = 2
} ls_how_t;

static uint32_t SERNO = 0;

switch_mutex_t *MUTEX = NULL;

uint32_t get_serno(void)
{
	uint32_t r = 0;

	switch_mutex_lock(MUTEX);
	r = SERNO;
	switch_mutex_unlock(MUTEX);

	return r;
}


void inc_serno(void)
{
	switch_mutex_lock(MUTEX);
	SERNO++;
	switch_mutex_unlock(MUTEX);

}

void dec_serno(void)
{
	switch_mutex_lock(MUTEX);
	SERNO--;
	switch_mutex_unlock(MUTEX);

}


SWITCH_STANDARD_APP(bcast_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_socket_t *socket = NULL, *polycom_socket = NULL;
	switch_sockaddr_t *audio_addr = NULL, *control_packet_addr = NULL, *polycom_addr = NULL, *local_addr = NULL;
	switch_frame_t *read_frame = NULL;
	switch_status_t status;
	switch_size_t bytes;
	ls_control_packet_t control_packet;
	unsigned char polycom_buf[1024] = { 0 };
	unsigned char last_polycom_buf[1024] = { 0 };
	uint32_t last_polycom_len = 0;
	polycom_packet_t *polycom_packet = (polycom_packet_t *) polycom_buf;
	polycom_alert_packet_t *alert_packet = (polycom_alert_packet_t *) polycom_buf;
	switch_codec_t codec = { 0 };
	switch_codec_t write_codec = { 0 };
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	const char *err;
	switch_rtp_t *rtp_session = NULL;
	switch_port_t rtp_port = 0;
	ls_how_t ready = SEND_TYPE_UNKNOWN;
	char *mydata, *argv[5];
	char *mcast_ip = "224.168.168.168";
	switch_port_t mcast_port = 34567;
	switch_port_t mcast_control_port = 6061;
	char *mcast_port_str = "34567";
	char *polycom_ip = "224.0.1.116";
	const char *source_ip = NULL;
	switch_port_t polycom_port = 5001;
	const char *var;
	switch_codec_implementation_t read_impl = { 0 };
	int mcast_ttl = 1;
	const char *caller_id_name = NULL;
	int x = 0;
	uint32_t seq = 0;
	const char *codec_name = "PCMU";
	int read_rate = 8000;
	int need_transcode = 0;
	
	inc_serno();

	switch_core_session_get_read_impl(session, &read_impl);

	if (!zstr((char *) data)) {
		mydata = switch_core_session_strdup(session, data);
		assert(mydata != NULL);

		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		if ((var = switch_channel_get_variable(channel, "esf_multicast_ip"))) {
			mcast_ip = switch_core_session_strdup(session, var);
		}

		if (!zstr(argv[0])) {
			mcast_ip = argv[0];
		}

		if (!zstr(argv[1])) {
			mcast_port_str = argv[1];
			mcast_port = (switch_port_t) atoi(mcast_port_str);
		}

		if (!zstr(argv[2])) {
			mcast_control_port = (switch_port_t) atoi(argv[2]);
		}

		if (!zstr(argv[3])) {
			mcast_ttl = atoi(argv[3]);
			if (mcast_ttl < 1 || mcast_ttl > 255) {
				mcast_ttl = 1;
			}
		}
	}

	switch_channel_set_variable_printf(channel, "multicast_ttl", "%d", mcast_ttl);


	if (switch_true(switch_channel_get_variable(channel, SWITCH_BYPASS_MEDIA_VARIABLE))) {
		switch_core_session_message_t msg = { 0 };

		ready = SEND_TYPE_NOMEDIA;

		switch_channel_set_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, mcast_ip);
		switch_channel_set_variable(channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, mcast_port_str);

		/* special answer with the mcast addr */
		msg.from = __FILE__;
		msg.string_arg = "recvonly";
		msg.message_id = SWITCH_MESSAGE_INDICATE_BROADCAST;
		switch_core_session_receive_message(session, &msg);
	} else {
		switch_channel_answer(channel);
	}

	if (!(source_ip = switch_channel_get_variable(channel, "esf_multicast_bind_ip"))) {
		if (!(source_ip = switch_channel_get_variable(channel, "local_ip_v4"))) {
			source_ip = "127.0.0.1";
		}
	}

	/* everyone */

	if (switch_sockaddr_info_get(&local_addr, source_ip, SWITCH_UNSPEC,
								 0, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "address Error\n");
		goto fail;
	}

	if (switch_socket_create(&socket, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error 1\n");
		goto fail;
	}

	if (switch_socket_opt_set(socket, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Option Error\n");
		goto fail;
	}

	if (switch_sockaddr_info_get(&control_packet_addr, mcast_ip, SWITCH_UNSPEC,
								 mcast_control_port, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error 3\n");
		goto fail;
	}

	if (switch_socket_bind(socket, local_addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket bind Error\n");
		goto fail;
	}

	if (switch_mcast_interface(socket, local_addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket interface Error\n");
		goto fail;
	}

	if (switch_mcast_join(socket, control_packet_addr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
		goto fail;
	}


	if (switch_mcast_hops(socket, (uint8_t) mcast_ttl) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Mutlicast TTL set failed\n");
		goto fail;
	}

	/* polycom */


	if (switch_sockaddr_info_get(&polycom_addr, polycom_ip, SWITCH_UNSPEC,
								 polycom_port, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error 3\n");
		goto fail;
	}

	if (switch_socket_create(&polycom_socket, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error 1\n");
		goto fail;
	}

	if (switch_socket_opt_set(polycom_socket, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Option Error\n");
		goto fail;
	}

	if (switch_socket_bind(polycom_socket, local_addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket bind Error\n");
		goto fail;
	}


	if (switch_mcast_interface(polycom_socket, local_addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket interface Error\n");
		goto fail;
	}

	if (switch_mcast_join(polycom_socket, polycom_addr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
		goto fail;
	}


	if (switch_mcast_hops(polycom_socket, (uint8_t) mcast_ttl) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Mutlicast TTL set failed\n");
		goto fail;
	}

	
	while (!ready) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status) || !read_frame) {
			goto fail;
		}


		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}


		ready = SEND_TYPE_RTP;
	}


	alert_packet->audio_header.codec = 0x00;
	alert_packet->audio_header.flags = 0;
	
	if ((var = switch_channel_get_variable(channel, "esf_multicast_write_codec"))) {
		if (!strcasecmp(var, "PCMU")) {
			codec_name = var;
		} else if (!strcasecmp(var, "G722")) {
			codec_name = var;
			read_rate = 16000;
			alert_packet->audio_header.codec = 0x09;
		}
	}


	if (ready == SEND_TYPE_RTP) {
		if (strcasecmp(read_impl.iananame, codec_name)) {
			need_transcode = 1;

			if (switch_core_codec_init(&codec,
									   "L16",
									   NULL,
									   NULL,
									   read_rate,
									   20,
									   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_set_read_codec(session, &codec);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Codec Activation Success\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Activation Fail\n");
				goto fail;
			}

			if (switch_core_codec_init(&write_codec,
									   codec_name,
									   NULL,
									   NULL,
									   8000,
									   20,
									   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_set_read_codec(session, &codec);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Codec Activation Success\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Activation Fail\n");
				goto fail;
			}
		}
		

		if (!(rtp_port = switch_rtp_request_port(source_ip))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "RTP Port Error\n");
			goto fail;
		}

		rtp_session = switch_rtp_new(source_ip,
									 rtp_port,
									 mcast_ip,
									 mcast_port,
									 alert_packet->audio_header.codec,
									 160,
									 20000, flags, "soft", &err, switch_core_session_get_pool(session));
		
		if (!switch_rtp_ready(rtp_session)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "RTP Error\n");
			goto fail;
		}

	} else if (ready == SEND_TYPE_NOMEDIA) {
		switch_yield(10000);
	}

	control_packet.unique_id = htonl((u_long) switch_epoch_time_now(NULL));
	control_packet.command = htonl(LS_START_BCAST);
	control_packet.ip = inet_addr(mcast_ip);
	control_packet.port = htonl(mcast_port);

	bytes = 16;
	switch_socket_sendto(socket, control_packet_addr, 0, (void *) &control_packet, &bytes);
	bytes = 16;
	switch_socket_sendto(socket, control_packet_addr, 0, (void *) &control_packet, &bytes);


	if (!(caller_id_name = switch_channel_get_variable(channel, "caller_id_name"))) {
		caller_id_name = "FreeSWITCH";
	}

	strncpy((char *)polycom_packet->cid, caller_id_name, sizeof(polycom_packet->cid));
	polycom_packet->cid_len = 13;
	
	polycom_packet->op = 0x0F;
	polycom_packet->channel = 0x1a;
	polycom_packet->serno = htonl(get_serno());

	if ((var = switch_channel_get_variable(channel, "esf_multicast_channel"))) {
		int channel_no = atoi(var);

		if (channel_no > 0 && channel_no < 255) {
			polycom_packet->channel = (uint8_t) channel_no;
		}
	}

	for (x = 0; x < 32; x++) {
		bytes = sizeof(polycom_packet_t);
		switch_socket_sendto(socket, polycom_addr, 0, (void *) polycom_packet, &bytes);
		//switch_yield(30000);
	}

	polycom_packet->op = 0x10;

	if ((var = switch_channel_get_variable(channel, "esf_multicast_alert_sound"))) {
		switch_ivr_displace_session(session, var, 0, "mr");
	}

	while (switch_channel_ready(channel)) {

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (ready == SEND_TYPE_RTP) {
			unsigned char *ebuf;
			unsigned char encoded_data[4192];
			uint32_t encoded_datalen = sizeof(encoded_data);

			if (need_transcode) {
				uint32_t rate = codec.implementation->actual_samples_per_second;
				uint32_t flag = 0;

				ebuf = encoded_data;

				switch_core_codec_encode(&write_codec,
										 &codec,
										 read_frame->data,
										 read_frame->datalen,
										 read_impl.actual_samples_per_second,
										 ebuf, &encoded_datalen, &rate, &flag);

				if (read_frame->buflen >= encoded_datalen) {
					memcpy(read_frame->data, encoded_data, encoded_datalen);
				}

				read_frame->datalen = encoded_datalen;

			} else {
				ebuf = read_frame->data;
				encoded_datalen = read_frame->datalen;
			}
			
			switch_rtp_write_frame(rtp_session, read_frame);
			
			seq += 160;
			
			alert_packet->audio_header.seq = htonl(seq);
			
			if (last_polycom_len) {
				memcpy(alert_packet->data, last_polycom_buf, last_polycom_len);
				memcpy(alert_packet->data + last_polycom_len, ebuf, encoded_datalen);
			} else {
				memcpy(alert_packet->data, ebuf, encoded_datalen);
			}
			
			bytes = sizeof(*alert_packet) + encoded_datalen + last_polycom_len;

			switch_socket_sendto(socket, polycom_addr, 0, (void *) polycom_buf, &bytes);
			
			last_polycom_len = encoded_datalen;
			memcpy((void *)last_polycom_buf, (void *)ebuf, last_polycom_len);
			
		} else {
			bytes = read_frame->packetlen;
			switch_socket_sendto(socket, audio_addr, 0, read_frame->packet, &bytes);
		}
	}

	control_packet.unique_id = htonl((u_long) switch_epoch_time_now(NULL));
	control_packet.command = htonl(LS_STOP_BCAST);
	bytes = 8;
	switch_socket_sendto(socket, control_packet_addr, 0, (void *) &control_packet, &bytes);
	bytes = 8;
	switch_socket_sendto(socket, control_packet_addr, 0, (void *) &control_packet, &bytes);


	polycom_packet->op = 0xFF;
	//switch_yield(50000);

	for (x = 0; x < 12; x++) {
		bytes = sizeof(*polycom_packet);
		switch_socket_sendto(socket, polycom_addr, 0, (void *) polycom_packet, &bytes);
		//switch_yield(30000);
	}

  fail:

	switch_core_session_set_read_codec(session, NULL);
	if (switch_core_codec_ready(&codec)) {
		switch_core_codec_destroy(&codec);
	}

	if (rtp_session && ready == SEND_TYPE_RTP && switch_rtp_ready(rtp_session)) {
		switch_rtp_destroy(&rtp_session);
	}

	if (socket) {
		switch_socket_close(socket);
	}
	
	if (polycom_socket) {
		switch_socket_close(polycom_socket);
	}

	if (rtp_port) {
		switch_rtp_release_port(source_ip, rtp_port);
	}

	dec_serno();

	return;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_esf_load)
{
	switch_application_interface_t *app_interface;

	switch_mutex_init(&MUTEX, SWITCH_MUTEX_NESTED, pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "esf_page_group", NULL, NULL, bcast_function, NULL, SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

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
