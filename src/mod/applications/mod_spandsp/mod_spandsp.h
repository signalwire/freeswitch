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
 * The Original Code is FreeSWITCH mod_spandsp.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian West <brian@freeswitch.org>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 * Antonio Gallo <agx@linux.it>
 * mod_spandsp.h -- applications provided by SpanDSP
 *
 */

#include <switch.h>

#ifdef WIN32
#define FAX_INVALID_SOCKET INVALID_HANDLE_VALUE
typedef HANDLE zap_socket_t;
#else
#define FAX_INVALID_SOCKET -1
typedef int zap_socket_t;
#endif

#define MY_EVENT_TDD_SEND_MESSAGE "TDD::SEND_MESSAGE"
#define MY_EVENT_TDD_RECV_MESSAGE "TDD::RECV_MESSAGE"

#define MAX_MODEMS 1024
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>

/* The global stuff */
struct spandsp_globals {
	switch_memory_pool_t *pool;
	switch_memory_pool_t *config_pool;
	switch_mutex_t *mutex;

	uint32_t total_sessions;

	short int use_ecm;
	short int verbose;
	short int disable_v17;
	short int enable_t38;
	short int enable_t38_request;
	short int enable_t38_insist;
	char *ident;
	char *header;
	char *timezone;
	char *prepend_string;
	char *spool;
	switch_thread_cond_t *cond;
	switch_mutex_t *cond_mutex;
	int modem_count;
	int modem_verbose;
	char *modem_context;
	char *modem_dialplan;
	char *modem_directory;
	switch_hash_t *tones;
	int tonedebug;
};

extern struct spandsp_globals spandsp_globals;


typedef enum {
	FUNCTION_TX,
	FUNCTION_RX,
	FUNCTION_GW
} mod_spandsp_fax_application_mode_t;

/******************************************************************************
 * TONE DETECTION WITH CADENCE
 */

#define MAX_TONES 32
#define STRLEN 128
/**
 * Tone descriptor
 *
 * Defines a set of tones to look for
 */
struct tone_descriptor {
	/** The name of this descriptor set */
	const char *name;

	/** Describes the tones to watch */
	super_tone_rx_descriptor_t *spandsp_tone_descriptor;

	/** The mapping of tone id to key */
	char tone_keys[MAX_TONES][STRLEN];
	int idx;

};
typedef struct tone_descriptor tone_descriptor_t;


switch_status_t tone_descriptor_create(tone_descriptor_t **descriptor, const char *name, switch_memory_pool_t *memory_pool);
int tone_descriptor_add_tone(tone_descriptor_t *descriptor, const char *name);
switch_status_t tone_descriptor_add_tone_element(tone_descriptor_t *descriptor, int tone_id, int freq1, int freq2, int min, int max);


void mod_spandsp_fax_load(switch_memory_pool_t *pool);
switch_status_t mod_spandsp_codecs_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool);
switch_status_t mod_spandsp_dsp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool);

void mod_spandsp_fax_shutdown(void);
void mod_spandsp_dsp_shutdown(void);

void mod_spandsp_fax_event_handler(switch_event_t *event);
void mod_spandsp_fax_process_fax(switch_core_session_t *session, const char *data, mod_spandsp_fax_application_mode_t app_mode);
switch_bool_t t38_gateway_start(switch_core_session_t *session, const char *app, const char *data);

switch_status_t spandsp_stop_inband_dtmf_session(switch_core_session_t *session);
switch_status_t spandsp_inband_dtmf_session(switch_core_session_t *session);

switch_status_t callprogress_detector_start(switch_core_session_t *session, const char *name);
switch_status_t callprogress_detector_stop(switch_core_session_t *session);

switch_status_t spandsp_fax_detect_session(switch_core_session_t *session,
														   const char *flags, int timeout, int tone_type,
														   int hits, const char *app, const char *data, switch_tone_detect_callback_t callback);

switch_status_t spandsp_fax_stop_detect_session(switch_core_session_t *session);
void spanfax_log_message(void *user_data, int level, const char *msg);
switch_status_t load_configuration(switch_bool_t reload);
void mod_spandsp_indicate_data(switch_core_session_t *session, switch_bool_t self, switch_bool_t on);

switch_status_t spandsp_stop_tdd_encode_session(switch_core_session_t *session);
switch_status_t spandsp_tdd_encode_session(switch_core_session_t *session, const char *text);


switch_status_t spandsp_stop_tdd_decode_session(switch_core_session_t *session);
switch_status_t spandsp_tdd_decode_session(switch_core_session_t *session);
switch_status_t spandsp_tdd_send_session(switch_core_session_t *session, const char *text);

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
