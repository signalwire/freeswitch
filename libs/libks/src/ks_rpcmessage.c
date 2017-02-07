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

KS_DECLARE(ks_bool_t) ks_rpcmessage_isrequest(cJSON *msg)
{
    cJSON *result = cJSON_GetObjectItem(msg, "result");
    cJSON *error  = cJSON_GetObjectItem(msg, "error");

	if (result || error) {
		return  KS_FALSE;
	}

    return KS_TRUE;
}

KS_DECLARE(ks_bool_t) ks_rpcmessage_isrpc(cJSON *msg)
{
    cJSON *rpc = cJSON_GetObjectItem(msg, "json-rpc");

    if (rpc) {
        return  KS_FALSE;
    }

    return KS_TRUE;
}




KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_request(char *namespace,
												char *command,
												char *sessionid,
												char *version,
												cJSON **paramsP,
												cJSON **requestP)
{
    cJSON *msg, *params = NULL;
	*requestP = NULL;

	ks_rpcmessage_id msgid = ks_rpcmessage_next_id();
    msg = ks_rpcmessage_new(msgid);

    if (paramsP && *paramsP) {   /* parameters have been passed */

		cJSON *p = *paramsP;
		
		if (p->type != cJSON_Object) {    /* need to wrap this in a param field */
			params = cJSON_CreateObject();
			cJSON_AddItemToObject(params, "param", p);
		}
		else {
			params = *paramsP;
		}

		cJSON *v = cJSON_GetObjectItem(params, "version");

		if (!v) {                /* add version if needed  */
			 cJSON_AddStringToObject(params, "version", version);
		}
		else {
			cJSON_AddStringToObject(params, "version", "0");
		}
    }

    if (!params) {
        params = cJSON_CreateObject();

		if (version && version[0] != 0) {		
			cJSON_AddStringToObject(params, "version", version);
		}
		else {
			cJSON_AddStringToObject(params, "version", "0");
		}

    }

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

	sprintf(fqcommand, "%s.%s", namespace, command);

    cJSON_AddItemToObject(msg, "method", cJSON_CreateString(fqcommand));

	if (sessionid && sessionid[0] != 0) {
		cJSON_AddStringToObject(params, "sessionid", sessionid);
	}

    cJSON_AddItemToObject(msg, "params", params);

    if (paramsP) {
        *paramsP = params;
    }

	*requestP = msg;
    return msgid;
}

KS_DECLARE(ks_size_t) ks_rpc_create_buffer(char *namespace,
                                            char *method,
											char *sessionid,
											char *version,
                                            cJSON **parms,
                                            ks_buffer_t *buffer)
{
	cJSON *message;

	ks_rpcmessage_id msgid = ks_rpcmessage_create_request(namespace, method, sessionid, version, parms, &message);

	if (!msgid) {
		return 0;
	}

	const char* b = cJSON_PrintUnformatted(message);
	ks_size_t size = strlen(b);

	ks_buffer_write(buffer, b, size);
	cJSON_Delete(message);

	return size; 	
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


static ks_rpcmessage_id ks_rpcmessage_new_response(
                                                const cJSON *request,
                                                cJSON *result,
                                                cJSON **pmsg)
{
    cJSON *respmsg = NULL;
    cJSON *cmsgid  = NULL;
	cJSON *version = NULL;
	cJSON *sessionid = NULL;

    cJSON *command = cJSON_GetObjectItem(request, "method");
	cJSON *params =  cJSON_GetObjectItem(request, "params");

	if (params) {
		version = cJSON_GetObjectItem(request, "version");
	}

	ks_rpcmessage_id msgid = ks_rpcmessage_get_messageid(request, &cmsgid );

    if (!msgid || !command) {
        return 0;
    }

    *pmsg = respmsg = ks_rpcmessage_dup(cmsgid);

    cJSON_AddItemToObject(respmsg, "method", cJSON_Duplicate(command, 0));

    if (result) {

	    cJSON *params =  cJSON_GetObjectItem(request, "params");

		if (params) {
			version = cJSON_GetObjectItem(params, "version");

			if (version) {
				cJSON_AddItemToObject(result, "version", cJSON_Duplicate(version, 0));
			}
		
			sessionid = cJSON_GetObjectItem(params, "sessionid");

            if (sessionid) {
                cJSON_AddItemToObject(result, "sessionid", cJSON_Duplicate(sessionid, 0));
            }

		}
		
        cJSON_AddItemToObject(respmsg, "result", result);
    }

    return msgid;
}


KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_response(
												const cJSON *request,
												cJSON **resultP,
												cJSON **responseP)
{
	ks_rpcmessage_id msgid = ks_rpcmessage_new_response(request, *resultP, responseP);

	cJSON *respmsg = *responseP;

    if (msgid) {

		if (*resultP == NULL) {
			*resultP = cJSON_CreateObject();
			cJSON *result = *resultP;

		    cJSON *params =  cJSON_GetObjectItem(request, "params");

			if (params) {
				cJSON *version = cJSON_GetObjectItem(request, "version");
				cJSON *sessionid = cJSON_GetObjectItem(request, "sessionid");

				if (version) {
					cJSON_AddItemToObject(result, "version", cJSON_Duplicate(version, 0));
				}
				else {
					cJSON_AddStringToObject(result, "version", "0");
				}

				if (sessionid) {
					cJSON_AddItemToObject(result, "sessionid", cJSON_Duplicate(sessionid, 0));
				}
				
			}
			else {
				cJSON_AddStringToObject(result, "version", "0");
			}

			cJSON_AddItemToObject(respmsg, "result", result);
		}
	}

    return msgid;
}

KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_errorresponse( 
												const cJSON *request, 
												cJSON **errorP, 
												cJSON **responseP)
{
	ks_rpcmessage_id msgid = ks_rpcmessage_new_response(request, *errorP, responseP);
	cJSON *respmsg = *responseP;

	if (msgid) { 
  
		if (*errorP == NULL) {
			*errorP = cJSON_CreateObject();
			cJSON_AddItemToObject(respmsg, "error", *errorP);
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
