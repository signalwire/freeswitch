/*
 * Copyright (c) 2017, FreeSWITCH LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BLADE_RPCPROTO_H_
#define _BLADE_RPCPROTO_H_

#include <ks_rpcmessage.h>
#include <blade_types.h>

// temp typedefs to get compile going 
//typedef struct blade_peer_s  blade_peer_t;
//typedef struct blade_event_s blade_event_t;

#define KS_RPCMESSAGE_NAMESPACE_LENGTH 16
#define KS_RPCMESSAGE_COMMAND_LENGTH  238
#define KS_RPCMESSAGE_FQCOMMAND_LENGTH  (KS_RPCMESSAGE_NAMESPACE_LENGTH+KS_RPCMESSAGE_COMMAND_LENGTH+1)
#define KS_RPCMESSAGE_VERSION_LENGTH 9


/* 
 *  contents to add to the "blade" field in rpc
 */

typedef struct blade_rpc_fields_s {

	const char *to;
	const char *from;
	const char *token;

}  blade_rpc_fields_t;



enum jrpc_status_t { 
	JRPC_PASS = (1 << 0), 
	JRPC_SEND = (1 << 1),
	JRPC_EXIT = (1 << 2),
	JRPC_SEND_EXIT = JRPC_SEND + JRPC_EXIT, 
	JRPC_ERROR =  (1 << 3)
};


typedef enum jrpc_status_t (*jrpc_func_t)  (cJSON *request, cJSON **replyP);


/*
 *  setup
 *  -----
 */

KS_DECLARE(ks_status_t) blade_rpc_init(ks_pool_t *pool);


/* 
 *  namespace and call back registration 
 *  ------------------------------------
 */
KS_DECLARE(ks_status_t) blade_rpc_declare_namespace(char* namespace, const char* version);
KS_DECLARE(ks_status_t) blade_rpc_register_function(char* namespace, 
														char *command,
														jrpc_func_t func,
														jrpc_func_t respfunc);
KS_DECLARE(ks_status_t) blade_rpc_register_custom_request_function(char* namespace,   
														char *command,
														jrpc_func_t prefix_func,
														jrpc_func_t postfix_func);
KS_DECLARE(ks_status_t) blade_rpc_register_custom_response_function(char *namespace,   
														char *command,
														jrpc_func_t prefix_func,
														jrpc_func_t postfix_func);
KS_DECLARE(void) blade_rpc_remove_namespace(char* namespace);

/*
 * template registration and inheritance
 * -------------------------------------
 */
KS_DECLARE(ks_status_t) blade_rpc_declare_template(char* templatename, const char* version);

KS_DECLARE(ks_status_t)blade_rpc_register_template_function(char *name,
                                                char *command,
                                                jrpc_func_t func,
                                                jrpc_func_t respfunc);

KS_DECLARE(ks_status_t)blade_rpc_inherit_template(char *namespace, char* template);


/*
 * create a request message
 */
KS_DECLARE(ks_rpcmessageid_t) blade_rpc_create_request(char *namespace,
													char *method,
													blade_rpc_fields_t* fields,
													cJSON **paramsP,
													cJSON **requestP);

KS_DECLARE(ks_rpcmessageid_t) blade_rpc_create_response(cJSON *request,
													cJSON **reply,
													cJSON **response);

KS_DECLARE(ks_status_t) blade_rpc_parse_message(cJSON *message,
													char **namespace,
													char **method,
													char **version,
													uint32_t *idP,	
													blade_rpc_fields_t **fieldsP);

/*
 * peer create/destroy
 * -------------------
 */
//KS_DECLARE(ks_status_t) blade_rpc_onconnect(ks_pool_t *pool, blade_peer_t* peer);
//KS_DECLARE(ks_status_t) blade_rpc_disconnect(blade_peer_t* peer);

/* 
 * send message
 * ------------
 */
KS_DECLARE(ks_status_t) blade_rpc_write(char *sessionid, char* data, uint32_t size);  //uuid_t ?
KS_DECLARE(ks_status_t) blade_rpc_write_json(cJSON* json);


/*
 * process inbound message
 * -----------------------
 */
KS_DECLARE(ks_status_t) blade_rpc_process_data(const uint8_t *data, ks_size_t size);

KS_DECLARE(ks_status_t) blade_rpc_process_jsonmessage(cJSON *request);


#endif

