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
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_skel.c -- Framework Demo Module
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skel_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_skel_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_skel_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_skel, mod_skel_load, mod_skel_shutdown, NULL);

typedef enum {
	CODEC_NEGOTIATION_GREEDY = 1,
	CODEC_NEGOTIATION_GENEROUS = 2,
	CODEC_NEGOTIATION_EVIL = 3
} codec_negotiation_t;

static struct {
	char *codec_negotiation_str;
	codec_negotiation_t codec_negotiation;
	switch_bool_t sip_trace;
	int integer;
} globals;

static switch_status_t config_callback_siptrace(switch_xml_config_item_t *data, switch_config_callback_type_t callback_type, switch_bool_t changed)
{
	switch_bool_t value = *(switch_bool_t *) data->ptr;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "In siptrace callback: value %s changed %s\n",
					  value ? "true" : "false", changed ? "true" : "false");


	/*
	   if ((callback_type == CONFIG_LOG || callback_type == CONFIG_RELOAD) && changed) {
	   nua_set_params(((sofia_profile_t*)data->functiondata)->nua, TPTAG_LOG(value), TAG_END());
	   }
	 */

	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_config_string_options_t config_opt_codec_negotiation = { NULL, 0, "greedy|generous|evil" };

/* enforce_min, min, enforce_max, max */
static switch_xml_config_int_options_t config_opt_integer = { SWITCH_TRUE, 0, SWITCH_TRUE, 10 };
static switch_xml_config_enum_item_t config_opt_codec_negotiation_enum[] = {
	{"greedy", CODEC_NEGOTIATION_GREEDY},
	{"generous", CODEC_NEGOTIATION_GENEROUS},
	{"evil", CODEC_NEGOTIATION_EVIL},
	{NULL, 0}
};

static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM("codec-negotiation-str", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &globals.codec_negotiation_str, "greedy",
					   &config_opt_codec_negotiation,
					   "greedy|generous|evil", "Specifies the codec negotiation scheme to be used."),
	SWITCH_CONFIG_ITEM("codec-negotiation", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &globals.codec_negotiation, (void *) CODEC_NEGOTIATION_GREEDY,
					   &config_opt_codec_negotiation_enum,
					   "greedy|generous|evil", "Specifies the codec negotiation scheme to be used."),
	SWITCH_CONFIG_ITEM_CALLBACK("sip-trace", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &globals.sip_trace, (void *) SWITCH_FALSE,
								(switch_xml_config_callback_t) config_callback_siptrace, NULL,
								"yes|no", "If enabled, print out sip messages on the console."),
	SWITCH_CONFIG_ITEM("integer", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &globals.integer, (void *) 100, &config_opt_integer,
					   NULL, NULL),
	SWITCH_CONFIG_ITEM_END()
};

static switch_status_t do_config(switch_bool_t reload)
{
	memset(&globals, 0, sizeof(globals));

	if (switch_xml_config_parse_module_settings("skel.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not open skel.conf\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}
#include "switch_stun.h"

#define _switch_stun_packet_next_attribute(attribute, end) (attribute && (attribute = (switch_stun_packet_attribute_t *) (attribute->value + ntohs(attribute->length))) && ((void *)attribute < end) && ntohs(attribute->length) && ((void *)(attribute + ntohs(attribute->length)) < end))

#define _switch_stun_attribute_padded_length(attribute) ((uint16_t)(ntohs(attribute->length) + (sizeof(uint32_t)-1)) & ~sizeof(uint32_t))

//#define _switch_stun_packet_next_attribute(attribute, end) (attribute && (attribute = (switch_stun_packet_attribute_t *) (attribute->value +  _switch_stun_attribute_padded_length(attribute))) && ((void *)attribute < end) && ((void *)(attribute +  _switch_stun_attribute_padded_length(attribute)) < end))

#define MAX_PEERS 128
SWITCH_STANDARD_API(skel_function)
{
	switch_dial_handle_t *dh;
	switch_dial_leg_list_t *ll;
	switch_dial_leg_t *leg = NULL;
	int timeout = 0;
	char *peer_names[MAX_PEERS] = { 0 };
	switch_event_t *peer_vars[MAX_PEERS] = { 0 };
	int i;
	switch_core_session_t *peer_session = NULL;
	switch_call_cause_t cause;
	
	switch_dial_handle_create(&dh);


	switch_dial_handle_add_global_var(dh, "ignore_early_media", "true");
	switch_dial_handle_add_global_var_printf(dh, "coolness_count", "%d", 12);


	//// SET TO 1 FOR AND LIST example or to 0 for OR LIST example
#if 0
	switch_dial_handle_add_leg_list(dh, &ll);

	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/foo1@bar1.com");
	timeout += 10;
	switch_dial_handle_add_leg_var_printf(leg, "leg_timeout", "%d", timeout);

	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/foo2@bar2.com");
	timeout += 10;
	switch_dial_handle_add_leg_var_printf(leg, "leg_timeout", "%d", timeout);

	
	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/foo3@bar3.com");
	timeout += 10;
	switch_dial_handle_add_leg_var_printf(leg, "leg_timeout", "%d", timeout);


	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/3000@cantina.freeswitch.org");


#else

	switch_dial_handle_add_leg_list(dh, &ll); 
	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/foo1@bar1.com");
	timeout += 10;
	switch_dial_handle_add_leg_var_printf(leg, "leg_timeout", "%d", timeout);

	switch_dial_handle_add_leg_list(dh, &ll); 
	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/foo2@bar2.com");
	timeout += 10;
	switch_dial_handle_add_leg_var_printf(leg, "leg_timeout", "%d", timeout);

	switch_dial_handle_add_leg_list(dh, &ll); 
	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/foo3@bar3.com");
	timeout += 10;
	switch_dial_handle_add_leg_var_printf(leg, "leg_timeout", "%d", timeout);

	
	switch_dial_handle_add_leg_list(dh, &ll); 
	switch_dial_leg_list_add_leg(ll, &leg, "sofia/internal/3000@cantina.freeswitch.org");
#endif
	

	
	/////// JUST DUMP SOME OF IT TO SEE FIRST 

	switch_dial_handle_get_peers(dh, 0, peer_names, MAX_PEERS);
	switch_dial_handle_get_vars(dh, 0, peer_vars, MAX_PEERS);



	
	for(i = 0; i < MAX_PEERS; i++) {
		if (peer_names[i]) {
			char *foo;
			
			printf("peer: [%s]\n", peer_names[i]);

			if (peer_vars[i]) {
				if (switch_event_serialize(peer_vars[i], &foo, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
					printf("%s\n", foo);
				}
			}
			printf("\n\n");
			switch_safe_free(foo);
		}
	}


	switch_ivr_originate(NULL, &peer_session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);

	if (peer_session) {
		switch_ivr_session_transfer(peer_session, "3500", "XML", NULL);
		switch_core_session_rwunlock(peer_session);
	}

	
	switch_dial_handle_destroy(&dh);
	


		
	return SWITCH_STATUS_SUCCESS;
}

static void mycb(switch_core_session_t *session, switch_channel_callstate_t callstate, switch_device_record_t *drec)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_CRIT,
					  "%s device: %s\nState: %s Dev State: %s/%s Total:%u Offhook:%u Active:%u Held:%u Hungup:%u Dur: %u %s\n",
					  switch_channel_get_name(channel),
					  drec->device_id,
					  switch_channel_callstate2str(callstate),
					  switch_channel_device_state2str(drec->last_state),
					  switch_channel_device_state2str(drec->state),
					  drec->stats.total,
					  drec->stats.offhook,
					  drec->stats.active,
					  drec->stats.held,
					  drec->stats.hup,
					  drec->active_stop ? (uint32_t)(drec->active_stop - drec->active_start) / 1000 : 0,
					  switch_channel_test_flag(channel, CF_FINAL_DEVICE_LEG) ? "FINAL LEG" : "");

}


/* Macro expands to: switch_status_t mod_skel_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_skel_load)
{
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	do_config(SWITCH_FALSE);

	SWITCH_ADD_API(api_interface, "skel", "Skel API", skel_function, "syntax");

	switch_channel_bind_device_state_handler(mycb, NULL);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_skel_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skel_shutdown)
{
	/* Cleanup dynamically allocated config settings */
	switch_channel_unbind_device_state_handler(mycb);
	switch_xml_config_cleanup(instructions);
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_skel_runtime()
SWITCH_MODULE_RUNTIME_FUNCTION(mod_skel_runtime)
{
	while(looping)
	{
		switch_cond_next();
	}
	return SWITCH_STATUS_TERM;
}
*/

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
