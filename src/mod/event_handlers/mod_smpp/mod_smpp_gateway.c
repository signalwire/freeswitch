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

static void *SWITCH_THREAD_FUNC mod_smpp_gateway_read_thread(switch_thread_t *thread, void *obj);

switch_status_t mod_smpp_gateway_create(mod_smpp_gateway_t **gw, char *name, char *host, int port, int debug, char *system_id, 
										char *password, char *system_type, char *profile) {
	mod_smpp_gateway_t *gateway = NULL;
	switch_memory_pool_t *pool = NULL;
	
  	switch_core_new_memory_pool(&pool);

	gateway = switch_core_alloc(pool, sizeof(mod_smpp_gateway_t));

	gateway->pool = pool;
	gateway->running = 1;
	gateway->sequence = 1;
	gateway->name = name ? switch_core_strdup(gateway->pool, name) : "default";
	gateway->host = host ? switch_core_strdup(gateway->pool, host) : "localhost";
	gateway->port = port ? port : 8000;
	gateway->debug = debug;
	gateway->system_id = system_id ? switch_core_strdup(gateway->pool, system_id) : "username";
	gateway->password = password ? switch_core_strdup(gateway->pool, password) : "password";
	gateway->system_type = system_type ? switch_core_strdup(gateway->pool, system_type) : "freeswitch_smpp";
	gateway->profile = profile ? switch_core_strdup(gateway->pool, profile) : "default";

	if ( switch_sockaddr_info_get(&(gateway->socketaddr), gateway->host, SWITCH_INET, 
								  gateway->port, 0, mod_smpp_globals.pool) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get socketaddr info\n");
		goto err;
	}

	if ( switch_socket_create(&(gateway->socket), switch_sockaddr_get_family(gateway->socketaddr), 
							  SOCK_STREAM, SWITCH_PROTO_TCP, mod_smpp_globals.pool) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create the socket\n");
		goto err;
	}

	switch_mutex_init(&(gateway->conn_mutex), SWITCH_MUTEX_UNNESTED, mod_smpp_globals.pool);
	if (mod_smpp_gateway_connect(gateway) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect to gateway[%s]\n", gateway->name);
		goto err;
	}

	/* Start gateway read thread */
	switch_threadattr_create(&(gateway->thd_attr), mod_smpp_globals.pool);
	switch_threadattr_detach_set(gateway->thd_attr, 1);
	switch_threadattr_stacksize_set(gateway->thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&(gateway->thread), gateway->thd_attr, mod_smpp_gateway_read_thread, (void *) gateway, mod_smpp_globals.pool);

	switch_core_hash_insert(mod_smpp_globals.gateways, name, (void *) gateway);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Gateway %s created\n", gateway->host);
	
	*gw = gateway;
	return SWITCH_STATUS_SUCCESS;

 err:
	mod_smpp_gateway_destroy(&gateway);
	return SWITCH_STATUS_GENERR;

}

switch_status_t mod_smpp_gateway_authenticate(mod_smpp_gateway_t *gateway) {
    bind_transceiver_t *req_b = calloc(sizeof(bind_transceiver_t), 1);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_event_t *event = NULL;
	switch_size_t write_len = 0;
	unsigned int command_id = 0;
	uint8_t local_buffer[1024] = {0};
	int local_buffer_len = 1024;

    req_b->command_length   = 0;
    req_b->command_id       = BIND_TRANSCEIVER;
    req_b->command_status   = ESME_ROK;
    req_b->addr_npi         = 1;
    req_b->addr_ton         = 1;

	strncpy( (char *)req_b->address_range, gateway->host, sizeof(req_b->address_range));
	
	if ( gateway->system_id ) {
		strncpy((char *)req_b->system_id, gateway->system_id, sizeof(req_b->system_id));
	}
	
	if ( gateway->password ) {
		strncpy((char *)req_b->password, gateway->password, sizeof(req_b->password));
	}
	
	if ( gateway->system_type ) {
		strncpy((char *)req_b->system_type, gateway->system_type, sizeof(req_b->system_type));
	}

    req_b->interface_version = SMPP_VERSION;

    mod_smpp_gateway_get_next_sequence(gateway, &(req_b->sequence_number));

	// Not thread safe due to smpp34_errno and smpp34_strerror since they are global to running process.
	// The risk here is that the errno and strerror variables will be corrupted due to race conditions if there are errors
    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)req_b) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		status = SWITCH_STATUS_GENERR;
		goto done;
	}

	write_len = local_buffer_len;

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) req_b);
	}

	if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	/* Receive the response */
	if ( mod_smpp_gateway_connection_read(gateway, &event, &command_id) != SWITCH_STATUS_SUCCESS){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Authentication failed for gateawy[%s]\n", gateway->name);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Authentication successful for gateway[%s]\n", gateway->name);
	}

 done:
	if ( (mod_smpp_globals.debug || gateway->debug ) && event ) {
		char *str = NULL;
		switch_event_serialize(event, &str, SWITCH_TRUE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Auth response: %s\n", str);
		switch_safe_free(str);
	}

	if ( req_b ) {
		switch_safe_free(req_b);
	}
	
	return status;
}

/* When connecting to a gateway, the first message must be an authentication attempt */
switch_status_t mod_smpp_gateway_connect(mod_smpp_gateway_t *gateway) {
	switch_status_t status;

	if ( (status = switch_socket_connect(gateway->socket, gateway->socketaddr)) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect the socket %d\n", status);
		return SWITCH_STATUS_GENERR;
	}
	
	if ( mod_smpp_gateway_authenticate(gateway) != SWITCH_STATUS_SUCCESS ) {
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;
}


/* Expects the gateway to be locked already */
switch_status_t mod_smpp_gateway_connection_read(mod_smpp_gateway_t *gateway, switch_event_t **event, unsigned int *command_id)
{
	switch_size_t read_len = 4; /* Default to reading only the first 4 bytes to read how large the PDU is */
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint8_t *local_buffer = calloc(2048, 1);
	uint32_t *local_buffer32 = (uint32_t *) local_buffer; /* TODO: Convert this into a union */
	switch_event_t *evt = NULL;
	generic_nack_t *gennack = NULL;
    char data[2048] = {0}; /* local buffer for unpacked PDU */

	/* Read from socket */
	/* TODO: Add/Expand support for partial reads */
	if ( switch_socket_recv(gateway->socket, (char *) local_buffer, &read_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to recv on the socket\n");
	    switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( read_len != 4 ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in recv(PEEK) %d\n", (unsigned int )read_len);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	read_len = ntohl(*local_buffer32 );

	if ( read_len > 1500 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Corrupted PDU size from gateway [%s]\n", gateway->name);		
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( ( status = switch_socket_recv(gateway->socket, (char *) local_buffer + 4, &read_len)) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to recv on the socket %d\n", status);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( smpp34_unpack2((void *)data, local_buffer, read_len + 4) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,  "smpp: error decoding the receive buffer:%d:%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) data);
	}

	gennack = (generic_nack_t *) data;
	*command_id = gennack->command_id;
	switch(*command_id) {
	case BIND_TRANSCEIVER_RESP:
		if ( gennack->command_status != ESME_ROK ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Authentication Failure %d\n", gennack->command_status);
		}
		break;
	case DELIVER_SM:
		if ( gennack->command_status == ESME_ROK ) {
			deliver_sm_t *res = (deliver_sm_t *) data;
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "New SMS received from[%s] to[%s] message[%s]\n", 
							  res->source_addr, res->destination_addr, res->short_message);
			
			mod_smpp_message_decode(gateway, res, &evt);
		}
		break;
	case ENQUIRE_LINK: 
	case ENQUIRE_LINK_RESP:
	case SUBMIT_SM_RESP:
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unrecognized Command ID: %u\n", *command_id);
	}

	/* Only need to create/expand the event if it is one of the PDU's that we care about */
	if (evt) {
		*event = evt;
	}

 done: 
	switch_safe_free(local_buffer);
	return status;
}

static void *SWITCH_THREAD_FUNC mod_smpp_gateway_read_thread(switch_thread_t *thread, void *obj)
{
	mod_smpp_gateway_t *gateway = (mod_smpp_gateway_t *) obj;

	while ( gateway->running ) {
		switch_event_t *event = NULL;
		unsigned int command_id = 0;

		if ( mod_smpp_gateway_connection_read(gateway, &event, &command_id) != SWITCH_STATUS_SUCCESS) {
			if ( gateway->running ) {
				if ( mod_smpp_gateway_connect(gateway) != SWITCH_STATUS_SUCCESS) {
					switch_sleep(1000 * 1000);
				}
			}
			continue;
		}

		if ( (mod_smpp_globals.debug || gateway->debug) && event) {
			char *str = NULL;
			switch_event_serialize(event, &str, 0);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Received message[%d]:\n%s\n\n", command_id, str);
			switch_safe_free(str);
		}

		switch(command_id) {
		case BIND_TRANSCEIVER_RESP:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Why did we get an unexpected authentication response?\n");
			break;
		case ENQUIRE_LINK:
			mod_smpp_gateway_send_enquire_link_response(gateway);
			break;
		case DELIVER_SM:
			if ( event ) {
				char *str = NULL;
				switch_event_serialize(event, &str, SWITCH_TRUE);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Inbound SMS packet event: \n%s\n\n", str);
				switch_safe_free(str);
			}

			mod_smpp_gateway_send_deliver_sm_response(gateway, event);
			
			switch_core_chat_send("smpp", event);
			switch_event_destroy(&event);
			/* Fire message to the chat plan, then respond */
			break;
		case SUBMIT_SM_RESP:
		case ENQUIRE_LINK_RESP:
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown command_id[%d]\n", command_id);
			if ( event ) {
				char *str = NULL;
				switch_event_serialize(event, &str, SWITCH_FALSE);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown packet event: %s\n", str);				
				switch_safe_free(str);
			}
		}
	}
	return 0;
}

switch_status_t mod_smpp_gateway_send_enquire_link_response(mod_smpp_gateway_t *gateway)
{
    enquire_link_resp_t *enquire_resp = calloc(sizeof(enquire_link_resp_t), 1);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t write_len = 0;
	uint8_t local_buffer[128] = {0};
	int local_buffer_len = sizeof(local_buffer);

    enquire_resp->command_length   = 0;
    enquire_resp->command_id       = ENQUIRE_LINK_RESP;
    enquire_resp->command_status   = ESME_ROK;

    mod_smpp_gateway_get_next_sequence(gateway, &(enquire_resp->sequence_number));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)enquire_resp) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	write_len = local_buffer_len;

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) enquire_resp);
	}

	if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(enquire_resp);

	return status;
}

switch_status_t mod_smpp_gateway_send_deliver_sm_response(mod_smpp_gateway_t *gateway, switch_event_t *event)
{
    deliver_sm_resp_t *deliver_sm_resp = calloc(sizeof(deliver_sm_resp_t), 1);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t write_len = 0;
	uint8_t local_buffer[128] = {0};
	int local_buffer_len = sizeof(local_buffer);

    deliver_sm_resp->command_length   = 0;
    deliver_sm_resp->command_id       = DELIVER_SM_RESP;
    deliver_sm_resp->command_status   = ESME_ROK;
/*	deliver_sm_resp->message_id       = 0;  Not used. calloc defaults to 0 */
	deliver_sm_resp->sequence_number  = atoi(switch_event_get_header(event, "sequence_number"));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)deliver_sm_resp) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	write_len = local_buffer_len;

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) deliver_sm_resp);
	}

	if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(deliver_sm_resp);
	return status;
}

switch_status_t mod_smpp_gateway_send_unbind(mod_smpp_gateway_t *gateway)
{
    unbind_t *unbind_req = calloc(sizeof(unbind_t), 1);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t write_len = 0;
	uint8_t local_buffer[128] = {0};
	int local_buffer_len = sizeof(local_buffer);

    unbind_req->command_length   = 0;
    unbind_req->command_id       = UNBIND;
    unbind_req->command_status   = ESME_ROK;

    mod_smpp_gateway_get_next_sequence(gateway, &(unbind_req->sequence_number));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)unbind_req) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	write_len = local_buffer_len;

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) unbind_req);
	}

	if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(unbind_req);

	return status;
}

switch_status_t mod_smpp_gateway_destroy(mod_smpp_gateway_t **gw) 
{
	mod_smpp_gateway_t *gateway = NULL;

	if ( !gw || !*gw ) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	gateway = *gw;
	
	switch_core_hash_delete(mod_smpp_globals.gateways, gateway->name);

	gateway->running = 0;

	mod_smpp_gateway_send_unbind(gateway);

	switch_socket_shutdown(gateway->socket, SWITCH_SHUTDOWN_READWRITE);
	switch_socket_close(gateway->socket);

	switch_core_destroy_memory_pool(&(gateway->pool));
	switch_mutex_destroy(gateway->conn_mutex);

	*gw = NULL;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_gateway_get_next_sequence(mod_smpp_gateway_t *gateway, uint32_t *seq)
{

	switch_mutex_lock(gateway->conn_mutex);
	gateway->sequence++;
	*seq = gateway->sequence;
	switch_mutex_unlock(gateway->conn_mutex);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_gateway_send_message(mod_smpp_gateway_t *gateway, switch_event_t *message) {
	mod_smpp_message_t *msg = NULL;
	uint8_t local_buffer[1024];
	int local_buffer_len = sizeof(local_buffer);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t write_len = 0;
	
	if ( mod_smpp_message_create(gateway, message, &msg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to send message due to message_create failure\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    memset(local_buffer, 0, sizeof(local_buffer));

    if( smpp34_pack2( local_buffer, sizeof(local_buffer),	&local_buffer_len, (void*)&(msg->req)) != 0 ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Unable to encode message:%d:\n%s\n", smpp34_errno, smpp34_strerror); 
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	write_len = local_buffer_len;

	if ( mod_smpp_gateway_get_next_sequence(gateway, &(msg->req.sequence_number)) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to fetch next gateway sequence number\n"); 
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) &(msg->req));
	}

	if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    if ( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Did not send all of message to gateway");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	mod_smpp_message_destroy(&msg);
	return status;
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
