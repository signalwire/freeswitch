/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
* William King <william.king@quentustech.com>
*
* mod_smpp.c -- smpp client and server implementation using libsmpp
*
* using libsmpp from: http://cgit.osmocom.org/libsmpp/
*
*/

#include <mod_smpp.h>

switch_status_t mod_smpp_message_encode_body(char *body, int length, unsigned char *bin, uint8_t *enc_length)
{
	int i = 0;
	
	for ( i = 0; i < length; i++ ) {
		bin[i*2] = body[i] / 16;
		bin[i*2 + 1] = body[i] % 16;
	}

    *enc_length = (i * 2) + 1;
	return SWITCH_STATUS_SUCCESS;
}

/* 
   Scratch notes taken during development/interop:

	char *message = "5361792048656c6c6f20746f204d79204c6974746c6520467269656e64";
	''.join('%02x' % ord(c) for c in  u'Скажите привет моему маленькому другу'.encode('utf16'))

	Variable length UTF-16 russian text: 

	char *message = "fffe21043a043004360438044204350420003f044004380432043504420420003c043e0435043c04430420003c0430043b0435043d044c043a043e043c044304200034044004430433044304";
	char *mesg_txt = "This is a test SMS message from FreeSWITCH over SMPP";
	*/

switch_status_t mod_smpp_message_create(mod_smpp_gateway_t *gateway, switch_event_t *event, mod_smpp_message_t **message)
{
	mod_smpp_message_t *msg = calloc(1, sizeof(mod_smpp_message_t));
	char *body = switch_event_get_body(event);
									   
	assert(*message == NULL);

	if ( !body ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to encode message missing body\n");
		goto err;
	}

	if ( mod_smpp_globals.debug || gateway->debug ) {
		char *str = NULL;
		switch_event_serialize(event, &str, 0);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Creating message from event:\n%s\n", str);
		switch_safe_free(str);
	}

    msg->req.command_id              = SUBMIT_SM;
    msg->req.command_status          = ESME_ROK;
    msg->req.command_length          = 0;
    msg->req.protocol_id             = 0;
    msg->req.priority_flag           = 0;
    msg->req.registered_delivery     = 0;
    msg->req.replace_if_present_flag = 0;
    msg->req.sm_default_msg_id       = 0;
    msg->req.data_coding             = 0;
    msg->req.source_addr_ton         = 1;
    msg->req.source_addr_npi         = 1;	
    msg->req.dest_addr_ton           = 1;
	msg->req.dest_addr_npi           = 1;
    msg->req.esm_class               = 1; /* 0 => default, 1 => datagram, 2 => forward(transaction), 3 => store and forward
									         2 is endpoint delivery, all others are into a DB first.
								          */

    mod_smpp_gateway_get_next_sequence(gateway, &(msg->req.sequence_number));

	snprintf((char *)msg->req.service_type, sizeof(msg->req.service_type), "%s", "SMS");
	snprintf((char *)msg->req.source_addr, sizeof(msg->req.source_addr), "%s", switch_event_get_header(event, "from_user"));
	snprintf((char *)msg->req.destination_addr, sizeof(msg->req.destination_addr), "%s", switch_event_get_header(event, "to_user"));
	snprintf((char *)msg->req.schedule_delivery_time, sizeof(msg->req.schedule_delivery_time), "%s", "");
	snprintf((char *)msg->req.validity_period, sizeof(msg->req.validity_period), "%s", "");
	snprintf((char *)msg->req.short_message, sizeof(msg->req.short_message), "%s", body);
	msg->req.sm_length = strlen(body);

	if ( 0 && mod_smpp_message_encode_body(body, strlen(body), 
											 (unsigned char *) &(msg->req.short_message), &(msg->req.sm_length)) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to encode message body\n");
		goto err;
	}

	*message = msg;
	return SWITCH_STATUS_SUCCESS;

 err:
	switch_safe_free(msg);
	return SWITCH_STATUS_GENERR;
}

switch_status_t mod_smpp_message_decode(mod_smpp_gateway_t *gateway, deliver_sm_t *res, switch_event_t **event)
{
	switch_event_t *evt = NULL;
	char *str = NULL;

	if (switch_event_create(&evt, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new event\n"); 
	}

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "endpoint", "mod_smpp");

	str = switch_mprintf("%d", res->sequence_number);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "sequence_number", str);

	str = switch_mprintf("%d", res->command_status);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "command_status", str);

	str = switch_mprintf("%d", res->command_id);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "command_id", str);

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "smpp_gateway", gateway->name);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "proto", "smpp");

	str = switch_mprintf("%d", res->source_addr_ton);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "source_addr_ton", str);

	str = switch_mprintf("%d", res->source_addr_npi);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "source_addr_npi", str);

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "from_user", (const char *) res->source_addr);

	str = switch_mprintf("%d", res->dest_addr_ton);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "dest_addr_ton", str);

	str = switch_mprintf("%d", res->dest_addr_npi);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "dest_addr_npi", str);

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "to_user", (const char *) res->destination_addr);

	str = switch_mprintf("%d", res->data_coding);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "data_coding", str);
	str = NULL;

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "profile", gateway->profile);

	switch_event_add_body(evt, "%s", (const char *) res->short_message);

	*event = evt;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_message_destroy(mod_smpp_message_t **msg)
{
	if ( msg ) {
		switch_safe_free(*msg);		
	}

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
