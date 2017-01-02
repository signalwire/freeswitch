/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011-2017, Seven Du <dujinfang@gmail.com>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 *
 * msrp.h -- MSRP lib
 *
 */

#ifndef _MSRP_H
#define _MSRP_H

#include <switch.h>

#define MSRP_LISTEN_PORT 2855
#define MSRP_SSL_LISTEN_PORT 2856

enum {
	MSRP_ST_WAIT_HEADER,
	MSRP_ST_PARSE_HEADER,
	MSRP_ST_WAIT_BODY,
	MSRP_ST_DONE,
	MSRP_ST_ERROR,

	MSRP_METHOD_REPLY,
	MSRP_METHOD_SEND,
	MSRP_METHOD_AUTH,
	MSRP_METHOD_REPORT,
};

typedef enum {
	MSRP_H_FROM_PATH,
	MSRP_H_TO_PATH,
	MSRP_H_MESSAGE_ID,
	MSRP_H_CONTENT_TYPE,
	MSRP_H_SUCCESS_REPORT,
	MSRP_H_FAILURE_REPORT,
	MSRP_H_STATUS,
	MSRP_H_KEEPALIVE,

	/* Fake headers */
	MSRP_H_TRASACTION_ID,
	MSRP_H_DELIMITER,
	MSRP_H_CODE_DESCRIPTION,

	MSRP_H_UNKNOWN
} switch_msrp_header_type_t;

typedef struct switch_msrp_session_s switch_msrp_session_t;
typedef struct msrp_client_socket_s switch_msrp_client_socket_t;
typedef struct msrp_socket_s switch_msrp_socket_t;

typedef struct msrp_msg_s {
	int state;
	int method;
	switch_event_t *headers;
	const char *transaction_id;
	const char *delimiter;
	int code_number;
	const char *code_description;
	switch_size_t byte_start;
	switch_size_t byte_end;
	switch_size_t bytes;
	switch_size_t payload_bytes;
	switch_size_t accumulated_bytes;
	int range_star; /* range-end is '*' */
	char *last_p;
	char *payload;
	struct msrp_msg_s *next;
} switch_msrp_msg_t;

struct switch_msrp_session_s{
	switch_memory_pool_t *pool;
	int secure;
	int active;
	char *remote_path;
	char *remote_accept_types;
	char *remote_accept_wrapped_types;
	char *remote_setup;
	char *remote_file_selector;
	char *local_path;
	char *local_accept_types;
	char *local_accept_wrapped_types;
	char *local_setup;
	char *local_file_selector;
	int local_port;
	char *call_id;
	switch_msrp_msg_t *msrp_msg;
	switch_msrp_msg_t *last_msg;
	switch_mutex_t *mutex;
	switch_size_t msrp_msg_buffer_size;
	switch_size_t msrp_msg_count;
	switch_msrp_socket_t *msock;
	switch_msrp_client_socket_t *csock;
	switch_frame_t frame;
	uint8_t frame_data[SWITCH_RTP_MAX_BUF_LEN];
	int running;
	void *user_data;
};

SWITCH_DECLARE(switch_status_t) switch_msrp_init(void);
SWITCH_DECLARE(switch_status_t) switch_msrp_destroy(void);
SWITCH_DECLARE(switch_msrp_session_t *)switch_msrp_session_new(switch_memory_pool_t *pool, const char *call_id, switch_bool_t secure);
SWITCH_DECLARE(switch_status_t) switch_msrp_session_destroy(switch_msrp_session_t **ms);
// switch_status_t switch_msrp_session_push_msg(switch_msrp_session_t *ms, switch_msrp_msg_t *msg);
SWITCH_DECLARE(switch_msrp_msg_t *)switch_msrp_session_pop_msg(switch_msrp_session_t *ms);
SWITCH_DECLARE(switch_status_t) switch_msrp_perform_send(switch_msrp_session_t *ms, switch_msrp_msg_t *msg, const char *file, const char *func, int line);
SWITCH_DECLARE(switch_status_t) switch_msrp_start_client(switch_msrp_session_t *msrp_session);
SWITCH_DECLARE(const char *) switch_msrp_listen_ip(void);

SWITCH_DECLARE(switch_msrp_msg_t*) switch_msrp_msg_create();
SWITCH_DECLARE(void) switch_msrp_msg_destroy(switch_msrp_msg_t **msg);

SWITCH_DECLARE(void) switch_msrp_load_apis_and_applications(switch_loadable_module_interface_t **moudle_interface);
SWITCH_DECLARE(const char*) switch_msrp_msg_get_header(switch_msrp_msg_t *msrp_msg, switch_msrp_header_type_t htype);
SWITCH_DECLARE(switch_status_t) switch_msrp_msg_add_header(switch_msrp_msg_t *msrp_msg, switch_msrp_header_type_t htype, char *fmt, ...);
SWITCH_DECLARE(void) switch_msrp_msg_set_payload(switch_msrp_msg_t *msrp_msg, const char *buf, switch_size_t payload_bytes);
SWITCH_DECLARE(char*) switch_msrp_header_name(switch_msrp_header_type_t htype);
SWITCH_DECLARE(switch_msrp_msg_t *) switch_msrp_msg_dup(switch_msrp_msg_t *msg);

#define switch_msrp_send(ms, msg) switch_msrp_perform_send(ms, msg, __FILE__, __SWITCH_FUNC__, __LINE__)

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
