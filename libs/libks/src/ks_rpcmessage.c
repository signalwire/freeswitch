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
#include <ks_buffer.h>

struct 
{

    ks_mutex_t *id_mutex;
    uint32_t   message_id;

    ks_pool_t  *pool;

} handle = {NULL, 0, NULL};

const char PROTOCOL[] = "jsonrpc";
const char PROTOCOL_VERSION[] = "2.0";
const char ID[]      = "id";
const char METHOD[]  = "method";
const char PARAMS[]  = "params";
const char ERROR[]   = "error";
const char RESULT[]  = "result";



KS_DECLARE(void*) ks_json_pool_alloc(ks_size_t size)
{
	return ks_pool_alloc(handle.pool, size);
}

KS_DECLARE(void) ks_json_pool_free(void *ptr)
{
    ks_pool_free(handle.pool, &ptr);
}


KS_DECLARE(void) ks_rpcmessage_init(ks_pool_t* pool)
{
	if (!handle.id_mutex) {
		ks_mutex_create(&handle.id_mutex, KS_MUTEX_FLAG_DEFAULT, pool);
	    handle.pool = pool;

		cJSON_Hooks hooks;
		hooks.malloc_fn = ks_json_pool_alloc;
		hooks.free_fn   = ks_json_pool_free;
		cJSON_InitHooks(&hooks);
	}
	return;
}

static uint32_t ks_rpcmessage_next_id()
{
    uint32_t message_id;

    ks_mutex_lock(handle.id_mutex);

    ++handle.message_id;

    if (!handle.message_id) {
		 ++handle.message_id;
	}

	message_id = handle.message_id;

    ks_mutex_unlock(handle.id_mutex);

    return message_id;
}


static cJSON *ks_rpcmessage_new(uint32_t id)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, PROTOCOL, cJSON_CreateString(PROTOCOL_VERSION));

    if (id) {
        cJSON_AddItemToObject(obj, ID, cJSON_CreateNumber(id));
    }

    return obj;
}

static cJSON *ks_rpcmessage_dup(cJSON *msgid)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, PROTOCOL, cJSON_CreateString(PROTOCOL_VERSION));

    if (msgid) {
        cJSON_AddItemToObject(obj, ID,  cJSON_Duplicate(msgid, 0));
    }

    return obj;
}

KS_DECLARE(ks_bool_t) ks_rpcmessage_isrequest(cJSON *msg)
{
    cJSON *result = cJSON_GetObjectItem(msg, RESULT);
    cJSON *error  = cJSON_GetObjectItem(msg, ERROR);

	if (result || error) {
		return  KS_FALSE;
	}

    return KS_TRUE;
}

KS_DECLARE(ks_bool_t) ks_rpcmessage_isrpc(cJSON *msg)
{
    cJSON *rpc = cJSON_GetObjectItem(msg, PROTOCOL);

    if (rpc) {
        return  KS_FALSE;
    }

    return KS_TRUE;
}




KS_DECLARE(ks_rpcmessageid_t) ks_rpcmessage_create_request(char *namespace,
												char *command,
												cJSON **paramsP,
												cJSON **requestP)
{
    cJSON *msg, *params = NULL;
	*requestP = NULL;

	ks_rpcmessageid_t msgid = ks_rpcmessage_next_id();
    msg = ks_rpcmessage_new(msgid);

    if (paramsP) {

		if (*paramsP) {   /* parameters have been passed */
			params = *paramsP;
		}
		else {
			params = cJSON_CreateObject();
			*paramsP = params;
		}

		cJSON_AddItemToObject(msg, PARAMS, params);
    }

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

	sprintf(fqcommand, "%s.%s", namespace, command);

    cJSON_AddItemToObject(msg, METHOD, cJSON_CreateString(fqcommand));

	*requestP = msg;
    return msgid;
}

KS_DECLARE(ks_size_t) ks_rpc_create_buffer(char *namespace,
                                            char *method,
                                            cJSON **params,
                                            ks_buffer_t *buffer)
{

	cJSON *message;

	ks_rpcmessageid_t msgid = ks_rpcmessage_create_request(namespace, method, params, &message);

	if (!msgid) {
		return 0;
	}

	if ( (*params)->child == NULL) {
		cJSON_AddNullToObject(*params, "bladenull");
	}

	const char* b = cJSON_PrintUnformatted(message);
	ks_size_t size = strlen(b);

	ks_buffer_write(buffer, b, size);
	cJSON_Delete(message);

	return size; 	
}


static ks_rpcmessageid_t ks_rpcmessage_get_messageid(const cJSON *msg, cJSON **cmsgidP)
{
	ks_rpcmessageid_t msgid = 0;

	cJSON *cmsgid = cJSON_GetObjectItem(msg, ID);

	if (cmsgid->type == cJSON_Number) {
		msgid = (ks_rpcmessageid_t) cmsgid->valueint;
	}
 	
	*cmsgidP = cmsgid;	

	return msgid;
} 


static ks_rpcmessageid_t ks_rpcmessage_new_response(
                                                const cJSON *request,
                                                cJSON **result,
                                                cJSON **pmsg)
{
    cJSON *respmsg = NULL;
    cJSON *cmsgid  = NULL;

    cJSON *command = cJSON_GetObjectItem(request, METHOD);

	ks_rpcmessageid_t msgid = ks_rpcmessage_get_messageid(request, &cmsgid );

    if (!msgid || !command) {
        return 0;
    }

    *pmsg = respmsg = ks_rpcmessage_dup(cmsgid);

    cJSON_AddItemToObject(respmsg, METHOD, cJSON_Duplicate(command, 0));

    if (result && *result) {
        cJSON_AddItemToObject(respmsg, RESULT, *result);
    }

    return msgid;
}


KS_DECLARE(ks_rpcmessageid_t) ks_rpcmessage_create_response(
												const cJSON *request,
												cJSON **resultP,
												cJSON **responseP)
{
	ks_rpcmessageid_t msgid = ks_rpcmessage_new_response(request, resultP, responseP);

	cJSON *respmsg = *responseP;

    if (msgid) {

		if (resultP && *resultP == NULL) {
			cJSON *result = cJSON_CreateObject();
			*resultP = result;
			cJSON_AddItemToObject(respmsg, RESULT, result);
		}
	}

    return msgid;
}

KS_DECLARE(ks_rpcmessageid_t) ks_rpcmessage_create_errorresponse( 
												const cJSON *request, 
												cJSON **errorP, 
												cJSON **responseP)
{
	ks_rpcmessageid_t msgid = ks_rpcmessage_new_response(request, NULL, responseP);
	cJSON *respmsg = *responseP;

	if (msgid) { 

		if (errorP && *errorP == NULL) {
			cJSON *error = cJSON_CreateObject();
			*errorP = error;
			cJSON_AddItemToObject(respmsg, ERROR, error);
		}
		else if (errorP && *errorP) {
			cJSON_AddItemToObject(*responseP, ERROR, *errorP);	
		}
	}

    return msgid;
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
