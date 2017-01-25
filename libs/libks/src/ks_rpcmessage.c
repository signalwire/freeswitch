/*
 * Copyright (c) 2017 FreeSWITCH Solutions LLC
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

#pragma GCC optimize ("O0")


#include <ks.h>
#include <ks_rpcmessage.h>

#define KS_RPCMESSAGE_NAMESPACE_LENGTH 16
#define KS_PRCMESSAGE_COMMAND_LENGTH  238
#define KS_PRCMESSAGE_FQCOMMAND_LENGTH KS_RPCMESSAGE_NAMESPACE_LENGTH + 1 + KS_PRCMESSAGE_COMMAND_LENGTH
#define KS_RPCMESSAGE_VERSION_LENGTH 9

struct ks_rpcmessaging_handle_s
{
	ks_hash_t  *method_hash;

	ks_mutex_t *id_mutex;
	uint32_t   message_id;
	
	ks_pool_t  *pool;

	char namespace[KS_RPCMESSAGE_NAMESPACE_LENGTH+2];
    char version[KS_RPCMESSAGE_VERSION_LENGTH+1];   /* nnn.nn.nn */
};

typedef struct ks_rpcmessage_callbackpair_s 
{
	jrpc_func_t	     request_func;
	jrpc_resp_func_t response_func;
	uint16_t         command_length;
	char             command[1]; 
} ks_rpcmessage_callbackpair_t;


static uint32_t ks_rpcmessage_next_id(ks_rpcmessaging_handle_t* handle)
{
    uint32_t message_id;

    ks_mutex_lock(handle->id_mutex);

    ++handle->message_id;

    if (!handle->message_id) {
		 ++handle->message_id;
	}

	message_id = handle->message_id;

    ks_mutex_unlock(handle->id_mutex);

    return message_id;
}


KS_DECLARE(ks_rpcmessaging_handle_t*) ks_rpcmessage_init(ks_pool_t* pool, ks_rpcmessaging_handle_t** handleP)
{
	ks_rpcmessaging_handle_t *handle = (ks_rpcmessaging_handle_t *)ks_pool_alloc(pool, sizeof(ks_rpcmessaging_handle_t));
	*handleP = handle;

	ks_hash_create(&handle->method_hash, 
					KS_HASH_MODE_CASE_SENSITIVE, 
					KS_HASH_FLAG_RWLOCK + KS_HASH_FLAG_DUP_CHECK + KS_HASH_FLAG_FREE_VALUE,
					pool);

    ks_mutex_create(&handle->id_mutex, KS_MUTEX_FLAG_DEFAULT, pool);

	strcpy(handle->namespace, "global.");

	handle->pool = pool;
	return handle;
}


KS_DECLARE(void) ks_rpcmessage_deinit(ks_rpcmessaging_handle_t** handleP)
{
	ks_rpcmessaging_handle_t *handle = *handleP;
	ks_hash_destroy(&handle->method_hash);
    ks_pool_free(handle->pool, handleP);
    return;
}

static cJSON *ks_rpcmessage_new(uint32_t id)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "jsonrpc", cJSON_CreateString("2.0"));

    if (id) {
        cJSON_AddItemToObject(obj, "id", cJSON_CreateNumber(id));
    }

    return obj;
}

static cJSON *ks_rpcmessage_dup(cJSON *msgid)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "jsonrpc", cJSON_CreateString("2.0"));

    if (msgid) {
        cJSON_AddItemToObject(obj, "id",  cJSON_Duplicate(msgid, 0));
    }

    return obj;
}

static ks_bool_t ks_rpcmessage_isrequest(cJSON *msg)
{
	//cJSON *params = cJSON_GetObjectItem(msg, "param");
    cJSON *result = cJSON_GetObjectItem(msg, "result");
    cJSON *error  = cJSON_GetObjectItem(msg, "error");

	if (result || error) {
		return  KS_FALSE;
	}

    return KS_TRUE;
}



KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_request(ks_rpcmessaging_handle_t* handle, 
												const char *command,
												cJSON **paramsP,
												cJSON **requestP)
{
    cJSON *msg, *params = NULL;
	*requestP = NULL;

	if (!ks_rpcmessage_find_function(handle, command)) {
		ks_log(KS_LOG_ERROR, "Attempt to create unknown message type : %s, namespace %s\n", command, handle->namespace);
		return 0;
	}

	ks_rpcmessage_id msgid = ks_rpcmessage_next_id(handle);
    msg = ks_rpcmessage_new(msgid);

    if (paramsP && *paramsP) {
        params = *paramsP;
    }

    if (!params) {
        params = cJSON_CreateObject();
    }

    char fqcommand[KS_PRCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

    if (handle->namespace[0] != 0) {
        strcpy(fqcommand, handle->namespace);
    }

    strcat(fqcommand, command);

    cJSON_AddItemToObject(msg, "method", cJSON_CreateString(fqcommand));
    cJSON_AddItemToObject(msg, "params", params);

    if (handle->version[0] != 0) {
        cJSON_AddItemToObject(msg, "version", cJSON_CreateString(handle->version));
    }

    if (paramsP) {
        *paramsP = params;
    }

	*requestP = msg;
    return msgid;
}

static ks_rpcmessage_id ks_rpcmessage_get_messageid(const cJSON *msg, cJSON **cmsgidP)
{
	uint32_t msgid = 0;

	cJSON *cmsgid = cJSON_GetObjectItem(msg, "id");

	if (cmsgid->type == cJSON_Number) {
		msgid = (uint32_t) cmsgid->valueint;
	}
 	
	*cmsgidP = cmsgid;	

	return msgid;
} 


static ks_rpcmessage_id ks_rpcmessage_new_response(ks_rpcmessaging_handle_t* handle,
                                                const cJSON *request,
                                                cJSON *result,
                                                cJSON **pmsg)
{
    cJSON *respmsg = NULL;
    cJSON *cmsgid  = NULL;
    cJSON *command = cJSON_GetObjectItem(request, "method");

	ks_rpcmessage_id msgid = ks_rpcmessage_get_messageid(request, &cmsgid );

    if (!msgid || !command) {
        return 0;
    }

    *pmsg = respmsg = ks_rpcmessage_dup(cmsgid);

    cJSON_AddItemToObject(respmsg, "method", cJSON_Duplicate(command, 0));

    if (handle->version[0] != 0) {
        cJSON_AddItemToObject(respmsg, "version", cJSON_CreateString(handle->version));
    }

    if (result) {
        cJSON_AddItemToObject(respmsg, "result", result);
    }

    return msgid;
}


KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_response(ks_rpcmessaging_handle_t* handle,
												const cJSON *request,
												cJSON **resultP,
												cJSON **responseP)
{
	ks_rpcmessage_id msgid = ks_rpcmessage_new_response(handle, request, *resultP, responseP);
	cJSON *respmsg = *responseP;

    if (msgid) {

		if (*resultP == NULL) {
			*resultP = cJSON_CreateObject();
			cJSON_AddItemToObject(respmsg, "result", *resultP);
		}
	}

    return msgid;
}

KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_errorresponse(ks_rpcmessaging_handle_t* handle, 
												const cJSON *request, 
												cJSON **errorP, 
												cJSON **responseP)
{
	ks_rpcmessage_id msgid = ks_rpcmessage_new_response(handle, request, *errorP, responseP);
	cJSON *respmsg = *responseP;

	if (msgid) { 
  
		if (*errorP == NULL) {
			*errorP = cJSON_CreateObject();
			cJSON_AddItemToObject(respmsg, "error", *errorP);
		}
	}

    return msgid;
}

KS_DECLARE(ks_status_t) ks_rpcmessage_namespace(ks_rpcmessaging_handle_t* handle, const char* namespace, const char* version)
{
	memset(handle->namespace, 0, sizeof(handle->namespace));
    memset(handle->version, 0, sizeof(handle->version)); 

	strncpy(handle->namespace, namespace, KS_RPCMESSAGE_NAMESPACE_LENGTH);
    strncpy(handle->version, version, KS_RPCMESSAGE_VERSION_LENGTH);
	handle->namespace[sizeof(handle->namespace) - 1] = 0;
	handle->version[sizeof(handle->version) -1] = 0;

    ks_log(KS_LOG_DEBUG, "Setting message namespace value %s, version %s", handle->namespace, handle->version);
	strcat( handle->namespace, ".");

    return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rpcmessage_register_function(ks_rpcmessaging_handle_t* handle, 
												const char *command, 
												jrpc_func_t func,
												jrpc_resp_func_t respfunc)
{
	if (!func && !respfunc) {
		return KS_STATUS_FAIL;
	}

	char fqcommand[KS_PRCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

	if (handle->namespace[0] != 0) {
		strcpy(fqcommand, handle->namespace);
	}	
	strcat(fqcommand, command);

	int lkey = strlen(fqcommand)+1;

	if (lkey < 16) {
		lkey = 16;
	}	

	ks_rpcmessage_callbackpair_t* callbacks =
			(ks_rpcmessage_callbackpair_t*)ks_pool_alloc(handle->pool, lkey + sizeof(ks_rpcmessage_callbackpair_t));

	strcpy(callbacks->command, fqcommand);
	callbacks->command_length = lkey;
	callbacks->request_func = func;
	callbacks->response_func = respfunc;

	ks_hash_write_lock(handle->method_hash);
	ks_hash_insert(handle->method_hash, callbacks->command, (void *) callbacks);

	ks_hash_write_unlock(handle->method_hash);

	ks_log(KS_LOG_DEBUG, "Message %s registered (%s)\n", command, fqcommand);

	return KS_STATUS_SUCCESS;
}

static ks_rpcmessage_callbackpair_t* ks_rpcmessage_find_function_ex(ks_rpcmessaging_handle_t* handle, char *command)
{
	ks_hash_read_lock(handle->method_hash);

	 ks_rpcmessage_callbackpair_t* callbacks = ks_hash_search(handle->method_hash, command, KS_UNLOCKED);

	ks_hash_read_unlock(handle->method_hash);

	return callbacks;
}

KS_DECLARE(jrpc_func_t) ks_rpcmessage_find_function(ks_rpcmessaging_handle_t* handle, const char *command)
{
    char fqcommand[KS_PRCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

    if (handle->namespace[0] != 0) {
        strcpy(fqcommand, handle->namespace);
		strcat(fqcommand, command);
    }
	else {
		strcpy(fqcommand, command);
	}
	

    ks_rpcmessage_callbackpair_t* callbacks = ks_rpcmessage_find_function_ex(handle, (char *)fqcommand);

	if (!callbacks) {
		return NULL;
	}

	return callbacks->request_func;
}

KS_DECLARE(jrpc_resp_func_t) ks_rpcmessage_find_response_function(ks_rpcmessaging_handle_t* handle, const char *command)
{
    char fqcommand[KS_PRCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

    if (handle->namespace[0] != 0) {
        strcpy(fqcommand, handle->namespace);
        strcat(fqcommand, command);
    }
    else {
        strcpy(fqcommand, command);
    }

    ks_rpcmessage_callbackpair_t* callbacks = ks_rpcmessage_find_function_ex(handle, (char *)fqcommand);

    return callbacks->response_func;
}


KS_DECLARE(ks_status_t) ks_rpcmessage_process_jsonmessage(ks_rpcmessaging_handle_t* handle, cJSON *request, cJSON **responseP)
{
	const char *command = cJSON_GetObjectCstr(request, "method");
	cJSON *error = NULL;
	cJSON *response = NULL;
	*responseP = NULL;

	if (!command) {
		error = cJSON_CreateString("Command not found");
	}

	//todo - add more checks ? 

	if (error) {
		ks_rpcmessage_create_request(handle, 0, &error, &response);
		return KS_STATUS_FAIL;
	}

	
	ks_rpcmessage_callbackpair_t* callbacks = ks_rpcmessage_find_function_ex(handle, (char *)command);

	if (!callbacks) {
		error = cJSON_CreateString("Command not supported");
		return  KS_STATUS_FAIL;
	}

	ks_bool_t isrequest = ks_rpcmessage_isrequest(request);

	if (isrequest && callbacks->request_func) {
		return callbacks->request_func(handle, request, responseP);
	}
    else if (!isrequest && callbacks->response_func) {
		return callbacks->response_func(handle, request);
	}

	return KS_STATUS_FAIL;
}



KS_DECLARE(ks_status_t) ks_rpcmessage_process_message(ks_rpcmessaging_handle_t* handle, 
														uint8_t *data, 
														ks_size_t size, 
														cJSON **responseP)
{
	cJSON *response = NULL;
	cJSON *error = NULL;

	cJSON *request = cJSON_Parse((char*)data);

	if (!request) {
		error = cJSON_CreateString("Invalid json format");
		ks_rpcmessage_create_request(handle, 0, &error, &response);
		return  KS_STATUS_FAIL; 
	}

	ks_status_t status = ks_rpcmessage_process_jsonmessage(handle, request, responseP);

	cJSON_Delete(request);

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
