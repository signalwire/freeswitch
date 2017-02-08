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

#include <blade_rpcproto.h>
#include <blade_message.h>

/* 
 * internal shared structure grounded in global 
 */
typedef struct blade_rpc_handle_ex {

    ks_hash_t  *namespace_hash;      /* hash to namespace methods */
	ks_hash_t  *template_hash;		 /* hash to template methods */

    ks_hash_t  *peer_hash;           /* hash to peer structure */
	
	ks_q_t     *event_queue;
	ks_bool_t   isactive;
	ks_pool_t  *pool; 

} blade_rpc_handle_ex_t;


typedef struct blade_rpc_namespace_s {

	char	   name[KS_RPCMESSAGE_NAMESPACE_LENGTH+1];
	char       version[KS_RPCMESSAGE_VERSION_LENGTH+1];   /* nnn.nn.nn */
    ks_hash_t *method_hash;      /* hash to namespace methods */

} blade_rpc_namespace_t;




blade_rpc_handle_ex_t *g_handle = NULL;


/*
 * callbacks - from blade_rpc_handle_ex->method_hash
 */

typedef struct blade_rpc_custom_callbackpair_s
{
    jrpc_prefix_func_t       prefix_request_func;
    jrpc_postfix_func_t      postfix_request_func;

    jrpc_prefix_resp_func_t  prefix_response_func;
    jrpc_postfix_resp_func_t postfix_response_func;

} blade_rpc_custom_callbackpair_t;



typedef struct blade_rpc_callbackpair_s
{
    jrpc_func_t              request_func;

    jrpc_resp_func_t         response_func;

	blade_rpc_custom_callbackpair_t* custom;

	ks_mutex_t		 *lock;	

    uint16_t         command_length;
    char             command[1];

} blade_rpc_callbackpair_t;





static void blade_rpc_make_fqcommand(const char* namespace, const char *command, char *fscommand)
{
    memset(fscommand, 0, KS_RPCMESSAGE_FQCOMMAND_LENGTH);
	sprintf(fscommand, "%s.%s", namespace, command);

	return;
}

static void blade_rpc_parse_fqcommand(const char* fscommand, char *namespace, char *command)
{
    memset(command, 0, KS_RPCMESSAGE_COMMAND_LENGTH);
    memset(namespace, 0, KS_RPCMESSAGE_NAMESPACE_LENGTH);

	uint32_t len = strlen(fscommand);

	assert(len <=  KS_RPCMESSAGE_FQCOMMAND_LENGTH);	
	ks_bool_t dotfound = KS_FALSE;

	for(int i=0, x=0; i<len; ++i, ++x)  {

		if (fscommand[i] == '.' && dotfound == KS_FALSE) {
			dotfound = KS_TRUE;
			x = 0;
		}
		else if (dotfound == KS_FALSE) {
			namespace[x] = fscommand[i];	
		}
		else {
			command[x] = fscommand[i];
		}

	}

	return;
}
 


KS_DECLARE(ks_status_t) blade_rpc_init(ks_pool_t *pool)
{

	if (g_handle == NULL) {
		g_handle = ks_pool_alloc(pool, sizeof(blade_rpc_handle_ex_t));
	    ks_hash_create(&g_handle->namespace_hash,
                    KS_HASH_MODE_CASE_SENSITIVE,
                    KS_HASH_FLAG_RWLOCK + KS_HASH_FLAG_DUP_CHECK,       // + KS_HASH_FLAG_FREE_VALUE,
                    pool);

        ks_hash_create(&g_handle->template_hash,
                    KS_HASH_MODE_CASE_SENSITIVE,
                    KS_HASH_FLAG_RWLOCK + KS_HASH_FLAG_DUP_CHECK,       // + KS_HASH_FLAG_FREE_VALUE,
                    pool);
					
		ks_q_create(&g_handle->event_queue, pool, 1024);	

		g_handle->pool = pool;

		/* initialize rpc messaging mechanism */
		ks_rpcmessage_init(pool);

		g_handle->isactive = KS_TRUE;
	}	
	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_rpc_onconnect(ks_pool_t *pool, blade_peer_t* peer)
{


	return KS_STATUS_FAIL;
}

KS_DECLARE(ks_status_t) blade_rpc_disconnect(blade_peer_t* peer)
{

	return KS_STATUS_FAIL;
}



/*
 * namespace setup
 */

/*
 *  function/callback functions
 */
static blade_rpc_callbackpair_t *blade_rpc_find_callbacks_locked(char *namespace, char *command)
{
	blade_rpc_callbackpair_t *callbacks = NULL;

    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, namespace, KS_UNLOCKED);
	if (n) {
		char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH+1];
		blade_rpc_make_fqcommand(namespace, command, fqcommand);

		ks_hash_read_lock(n->method_hash);

        callbacks = ks_hash_search(n->method_hash, fqcommand, KS_UNLOCKED);
		ks_mutex_lock(callbacks->lock);

		ks_hash_read_lock(n->method_hash);
	}

    return callbacks;
}

static blade_rpc_callbackpair_t *blade_rpc_find_template_locked(char *name, char *command)
{
    blade_rpc_callbackpair_t *callbacks = NULL;

    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->template_hash, name, KS_UNLOCKED);
    if (n) {

        ks_hash_read_lock(n->method_hash);
        callbacks = ks_hash_search(n->method_hash, command, KS_UNLOCKED);
        ks_mutex_lock(callbacks->lock);

        ks_hash_read_lock(n->method_hash);
    }

    return callbacks;
}




static blade_rpc_callbackpair_t *blade_rpc_find_callbacks_locked_fq(char *fqcommand)
{
    blade_rpc_callbackpair_t *callbacks = NULL;

	char command[KS_RPCMESSAGE_COMMAND_LENGTH+1];
    char namespace[KS_RPCMESSAGE_NAMESPACE_LENGTH+1];

	blade_rpc_parse_fqcommand(fqcommand, namespace, command);

    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, namespace, KS_UNLOCKED);
    if (n) {
        blade_rpc_make_fqcommand(namespace, command, fqcommand);
		ks_hash_read_lock(n->method_hash);

        callbacks = ks_hash_search(n->method_hash, fqcommand, KS_UNLOCKED);
		ks_mutex_lock(callbacks->lock);

		ks_hash_read_unlock(n->method_hash);
    }

    return callbacks;
}


KS_DECLARE(jrpc_func_t) blade_rpc_find_request_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

    if (!callbacks) {
        return NULL;
    }

    jrpc_func_t f = callbacks->request_func;

    ks_mutex_unlock(callbacks->lock);

    return f;
}

KS_DECLARE(jrpc_resp_func_t) blade_rpc_find_requestprefix_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

    if (!callbacks || !callbacks->custom) {
        return NULL;
    }

    jrpc_resp_func_t f = callbacks->custom->prefix_request_func;

    ks_mutex_unlock(callbacks->lock);

    return f;
}

KS_DECLARE(jrpc_resp_func_t) blade_rpc_find_response_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

	if (!callbacks) {
		return NULL;
	}
		
    jrpc_resp_func_t f = callbacks->response_func;

	ks_mutex_unlock(callbacks->lock);

	return f;
}

KS_DECLARE(jrpc_resp_func_t) blade_rpc_find_responseprefix_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

    if (!callbacks || !callbacks->custom) {
        return NULL;
    }

    jrpc_resp_func_t f = callbacks->custom->prefix_response_func;

    ks_mutex_unlock(callbacks->lock);

    return f;
}


KS_DECLARE(ks_status_t) blade_rpc_declare_namespace(char* namespace, const char* version)
{

	/* find/insert to namespace hash as needed */
	ks_hash_write_lock(g_handle->namespace_hash);
	blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, namespace, KS_UNLOCKED);
	if (n == NULL) {
		n = ks_pool_alloc(g_handle->pool, sizeof (blade_rpc_namespace_t) + strlen(namespace) + 1);
		strncpy(n->name, namespace, KS_RPCMESSAGE_NAMESPACE_LENGTH);
		strncpy(n->version, version, KS_RPCMESSAGE_VERSION_LENGTH);
		ks_hash_insert(g_handle->namespace_hash, n->name, n);
	}
	ks_hash_write_unlock(g_handle->namespace_hash);

    ks_log(KS_LOG_DEBUG, "Setting message namespace value %s, version %s", namespace, version);

    return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t)blade_rpc_register_namespace_function(char *namespace, 
                                                char *command,
                                                jrpc_func_t func,
                                                jrpc_resp_func_t respfunc)
{
    if (!func && !respfunc) {
        return KS_STATUS_FAIL;
    }

	char nskey[KS_RPCMESSAGE_NAMESPACE_LENGTH+1];
	memset(nskey, 0, sizeof(nskey));
	strcpy(nskey, namespace);

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

    strcpy(fqcommand, namespace);
	strcpy(fqcommand, ".");
    strcat(fqcommand, command);

    int lkey = strlen(fqcommand)+1;

    if (lkey < 16) {
        lkey = 16;
    }

    ks_hash_read_lock(g_handle->namespace_hash);    /* lock namespace hash */

    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, nskey, KS_UNLOCKED);

    if (n == NULL) {
        ks_hash_read_unlock(g_handle->namespace_hash);
        ks_log(KS_LOG_ERROR, "Unable to find %s namespace\n", namespace);
        return KS_STATUS_FAIL;
    }

	blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked(nskey, command);

	/* just ignore attempt to re register callbacks */
	/* @todo :  should this be smarter, allow override ? */
	if (callbacks != NULL) {
		ks_mutex_unlock(callbacks->lock);
        ks_hash_read_unlock(g_handle->namespace_hash);
        ks_log(KS_LOG_ERROR, "Callbacks already registered for %s namespace\n", namespace);
        return KS_STATUS_FAIL;
	}

    callbacks =
            (blade_rpc_callbackpair_t*)ks_pool_alloc(g_handle->pool, lkey + sizeof(blade_rpc_callbackpair_t));

    strcpy(callbacks->command, command);
    callbacks->command_length = lkey;
    callbacks->request_func   = func;
    callbacks->response_func  = respfunc;
	ks_mutex_create(&callbacks->lock, KS_MUTEX_FLAG_DEFAULT, g_handle->pool);

    ks_hash_write_lock(n->method_hash);             /* lock method hash */

    ks_hash_insert(n->method_hash, callbacks->command, (void *) callbacks);

    ks_hash_write_unlock(n->method_hash);           /* unlock method hash */
	ks_hash_read_unlock(g_handle->namespace_hash);  /* unlock namespace hash */

    ks_log(KS_LOG_DEBUG, "Message %s %s registered\n", namespace, command);

    return KS_STATUS_SUCCESS;

}


KS_DECLARE(ks_status_t)blade_rpc_register_prefix_request_function(char *namespace, 
													char *command,
													jrpc_prefix_func_t prefix_func,
													jrpc_postfix_func_t postfix_func)
{
	ks_status_t s = KS_STATUS_FAIL;

    ks_hash_write_lock(g_handle->namespace_hash);
	blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked(namespace, command);

    if (callbacks) {

		if (!callbacks->custom) {
			callbacks->custom = 
				(blade_rpc_custom_callbackpair_t *)ks_pool_alloc(g_handle->pool, sizeof(blade_rpc_custom_callbackpair_t));
		}

		callbacks->custom->prefix_request_func  = prefix_func;
		callbacks->custom->postfix_request_func = postfix_func;
		ks_mutex_unlock(callbacks->lock);
		s = KS_STATUS_SUCCESS;
	}

	ks_hash_write_unlock(g_handle->namespace_hash);
	return	s;
}

KS_DECLARE(ks_status_t)blade_rpc_register_prefix_response_function(char* namespace, 
													char *command,
													jrpc_prefix_resp_func_t prefix_func,
													jrpc_postfix_resp_func_t postfix_func)
{
	ks_status_t s = KS_STATUS_FAIL;
	
	ks_hash_write_lock(g_handle->namespace_hash);
    blade_rpc_callbackpair_t *callbacks = blade_rpc_find_callbacks_locked(namespace, command);

    if (callbacks) {

		if (!callbacks->custom) {
			callbacks->custom = 
				(blade_rpc_custom_callbackpair_t *)ks_pool_alloc(g_handle->pool, sizeof(blade_rpc_custom_callbackpair_t));
		}

        callbacks->custom->prefix_response_func  = prefix_func;
        callbacks->custom->postfix_response_func = postfix_func;
		ks_mutex_unlock(callbacks->lock);
        s = KS_STATUS_SUCCESS;
    }

    ks_hash_write_unlock(g_handle->namespace_hash);
    return  s;
}

KS_DECLARE(void) blade_rpc_remove_namespace(char* namespace)
{

    ks_hash_write_lock(g_handle->namespace_hash);

	blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, namespace, KS_UNLOCKED);

	ks_hash_iterator_t* it = ks_hash_first(n->method_hash, KS_HASH_FLAG_RWLOCK);

	while (it) {

		const void *key; 
		void *value;
		ks_ssize_t len = strlen(key);

		ks_hash_this(it, &key, &len, &value);
		blade_rpc_callbackpair_t *callbacks = (blade_rpc_callbackpair_t *)value;

		ks_mutex_lock(callbacks->lock);

		if (callbacks->custom) {
			ks_pool_free(g_handle->pool, callbacks->custom);
		}

		it = ks_hash_next(&it);
		ks_hash_remove(n->method_hash, (void *)key);
	}		

	ks_hash_write_unlock(g_handle->namespace_hash);

	return;
}


/*
 * template functions
 *
 */

KS_DECLARE(ks_status_t) blade_rpc_declare_template(char* templatename, const char* version)
{

    /* find/insert to namespace hash as needed */
    ks_hash_write_lock(g_handle->template_hash);
    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->template_hash, templatename, KS_UNLOCKED);
    if (n == NULL) {
        n = ks_pool_alloc(g_handle->pool, sizeof (blade_rpc_namespace_t) + strlen(templatename) + 1);
        strncpy(n->name, templatename, KS_RPCMESSAGE_NAMESPACE_LENGTH);
		strncpy(n->version, version, KS_RPCMESSAGE_VERSION_LENGTH);
        ks_hash_insert(g_handle->template_hash, n->name, n);
    }
    ks_hash_write_unlock(g_handle->template_hash);

    ks_log(KS_LOG_DEBUG, "Declaring application template namespace %s, version %s", templatename, version);

    return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t)blade_rpc_register_template_function(char *name,
                                                char *command,
                                                jrpc_func_t func,
                                                jrpc_resp_func_t respfunc)
{
    if (!func && !respfunc) {
        return KS_STATUS_FAIL;
    }

    int lkey = strlen(command)+1;

    if (lkey < 16) {
        lkey = 16;
    }

    ks_hash_read_lock(g_handle->template_hash);    /* lock template hash */

    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->template_hash, name, KS_UNLOCKED);

    if (n == NULL) {
        ks_hash_read_unlock(g_handle->template_hash);
        ks_log(KS_LOG_ERROR, "Unable to find %s template\n", name);
        return KS_STATUS_FAIL;
    }

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_template_locked(name, command);

    /* just ignore attempt to re register callbacks */
    /* as the template may already be in use leading to confusion */

    if (callbacks != NULL) {
        ks_mutex_unlock(callbacks->lock);
        ks_hash_read_unlock(g_handle->template_hash);
        ks_log(KS_LOG_ERROR, "Callbacks already registered for %s template\n", name);
        return KS_STATUS_FAIL;
    }

    callbacks =
            (blade_rpc_callbackpair_t*)ks_pool_alloc(g_handle->pool, lkey + sizeof(blade_rpc_callbackpair_t));

    strcpy(callbacks->command, command);
    callbacks->command_length = lkey;
    callbacks->request_func   = func;
    callbacks->response_func  = respfunc;

    ks_mutex_create(&callbacks->lock, KS_MUTEX_FLAG_DEFAULT, g_handle->pool);

    ks_hash_write_lock(n->method_hash);             /* lock method hash */

    ks_hash_insert(n->method_hash, callbacks->command, (void *) callbacks);

    ks_hash_write_unlock(n->method_hash);           /* unlock method hash */
    ks_hash_read_unlock(g_handle->template_hash);  /* unlock namespace hash */

    ks_log(KS_LOG_DEBUG, "Template message %s %s registered\n", name, command);

    return KS_STATUS_SUCCESS;

}

KS_DECLARE(ks_status_t)blade_rpc_inherit_template(char *namespace, char* template)
{
	ks_hash_read_lock(g_handle->template_hash);

	char tkey[KS_RPCMESSAGE_NAMESPACE_LENGTH+1];
	memset(tkey, 0, sizeof(tkey));
	strcpy(tkey, template);

    char nskey[KS_RPCMESSAGE_NAMESPACE_LENGTH+1];
    memset(nskey, 0, sizeof(tkey));
    strcpy(nskey, namespace);

	blade_rpc_namespace_t *n =  ks_hash_search(g_handle->template_hash, tkey, KS_UNLOCKED);

	if (!n) {
		ks_hash_read_unlock(g_handle->template_hash);
		ks_log(KS_LOG_ERROR, "Unable to locate template %s\n", template);
		return KS_STATUS_FAIL;
	}

	ks_hash_read_lock(g_handle->namespace_hash);

	blade_rpc_namespace_t *ns =  ks_hash_search(g_handle->namespace_hash, nskey, KS_UNLOCKED);

    if (!ns) {
        ks_hash_read_unlock(g_handle->template_hash);
		ks_hash_read_unlock(g_handle->namespace_hash);
        ks_log(KS_LOG_ERROR, "Unable to locate namespace %s\n", namespace);
        return KS_STATUS_FAIL;
    }

	ks_hash_write_lock(ns->method_hash);

    ks_hash_iterator_t* it = ks_hash_first(n->method_hash, KS_HASH_FLAG_RWLOCK);

	ks_hash_iterator_t* itfirst = it;

	/* first check that there are no name conflicts */
    while (it) {

        const void *key;
        void *value;
        ks_ssize_t len = strlen(key);

        ks_hash_this(it, &key, &len, &value);
        blade_rpc_callbackpair_t *t_callback = (blade_rpc_callbackpair_t *)value;

        ks_mutex_lock(t_callback->lock);

        char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH+1];
        blade_rpc_make_fqcommand(namespace, t_callback->command, fqcommand);
        blade_rpc_callbackpair_t *ns_callbacks = ks_hash_search(ns->method_hash, fqcommand, KS_UNLOCKED);

		if (ns_callbacks) {   /* if something already registered for this function kick the entire inherit */
			ks_hash_read_unlock(g_handle->template_hash);
			ks_hash_read_unlock(g_handle->namespace_hash);
			ks_hash_read_unlock(ns->method_hash);
			ks_mutex_unlock(t_callback->lock);
			ks_log(KS_LOG_ERROR, "Implementing template %s in namespace %s rejected. Command %s is ambiguous\n", 
											template, namespace, t_callback->command);
			return KS_STATUS_FAIL;
		}

		ks_mutex_unlock(t_callback->lock);

        it = ks_hash_next(&it);
    }

	/* ok - if we have got this far then the inherit is problem free */

	it = itfirst;

	while (it) {

        const void *key;
        void *value;
        ks_ssize_t len = strlen(key);

        ks_hash_this(it, &key, &len, &value);
        blade_rpc_callbackpair_t *t_callback = (blade_rpc_callbackpair_t *)value;

        ks_mutex_lock(t_callback->lock);

		int lkey = t_callback->command_length;

		blade_rpc_callbackpair_t *callbacks =
				(blade_rpc_callbackpair_t*)ks_pool_alloc(g_handle->pool, lkey + sizeof(blade_rpc_callbackpair_t));

		strcpy(callbacks->command, t_callback->command);
		callbacks->command_length = lkey;
		callbacks->request_func   = t_callback->request_func;
		callbacks->response_func  = t_callback->response_func;
		ks_mutex_create(&callbacks->lock, KS_MUTEX_FLAG_DEFAULT, g_handle->pool);

		ks_hash_insert(ns->method_hash, callbacks->command, (void *) callbacks);		

		ks_mutex_unlock(t_callback->lock);

		it = ks_hash_next(&it);
	}
	

	ks_hash_write_lock(ns->method_hash);

	ks_hash_read_unlock(g_handle->namespace_hash);
	ks_hash_read_unlock(g_handle->template_hash);

	return KS_STATUS_SUCCESS;
}
		


/*
 * send message
 * ------------
 */

KS_DECLARE(ks_status_t) blade_rpc_write_data(char *sessionid, char* data, uint32_t size)
{

	ks_status_t s = KS_STATUS_FAIL;

    // convert to json
	cJSON *msg = cJSON_Parse(data);

	if (msg) {

		// ks_status_t blade_peer_message_push(blade_peer_t *peer, void *data, int32_t data_length);

		s = KS_STATUS_SUCCESS;
	}
	else {
		ks_log(KS_LOG_ERROR, "Unable to format outbound message\n");
	}
	

    // ks_rpc_write_json
	// ks_status_t blade_peer_message_push(blade_peer_t *peer, void *data, int32_t data_length);
	return s;
}


KS_DECLARE(ks_status_t) blade_rpc_write_json(cJSON* json)
{
    // just push the messages onto the communication manager
    // synchronization etc, taken care of by the transport api'
	char *data = cJSON_PrintUnformatted(json);
	if (data) {
		ks_log(KS_LOG_DEBUG, "%s\n", data);
	    //return blade_rpc_write_data(sessionid, data, strlen(data));
	}
	ks_log(KS_LOG_ERROR, "Unable to parse json\n");
	return KS_STATUS_FAIL;
}




/*
 * Transport layer callbacks follow below
 *
*/




/*
 * rpc message processing
 */
static ks_status_t blade_rpc_process_jsonmessage_all(cJSON *request, cJSON **responseP)
{
	const char *fqcommand = cJSON_GetObjectCstr(request, "method");
	cJSON *error = NULL;
	cJSON *response = NULL;
	*responseP = NULL;

	if (!fqcommand) {
		error = cJSON_CreateObject();
		cJSON_AddStringToObject(error, "errormessage", "Command not specified");
        ks_rpcmessage_create_request("rpcprotocol", "unknowncommand", NULL, NULL,  &error, responseP);
        return KS_STATUS_FAIL;
	}


	char namespace[KS_RPCMESSAGE_NAMESPACE_LENGTH];
	char command[KS_RPCMESSAGE_COMMAND_LENGTH];

    blade_rpc_parse_fqcommand(fqcommand, namespace, command);
    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked(namespace, command);

    if (!callbacks) {
		error = cJSON_CreateObject();
		cJSON_AddStringToObject(error, "errormessage", "Command not supported");
        ks_rpcmessage_create_response(request, &error, responseP);
        return  KS_STATUS_FAIL;
    }

	//todo - add more checks ? 

	ks_bool_t isrequest = ks_rpcmessage_isrequest(request);

	ks_status_t s = KS_STATUS_SUCCESS;
	
	if (isrequest && callbacks->request_func) {

		if (callbacks->custom && callbacks->custom->prefix_request_func) {
			s = callbacks->custom->prefix_request_func(request);
		}

        if (s == KS_STATUS_SUCCESS) {
			s =  callbacks->request_func(request, responseP);
		}

		if (s == KS_STATUS_SUCCESS && callbacks->custom && callbacks->custom->postfix_request_func) {
			s =  callbacks->custom->postfix_request_func(request, responseP);
		}

		ks_mutex_unlock(callbacks->lock);

		return s;
	}
    else if (!isrequest && callbacks->response_func) {

        if (callbacks->custom && callbacks->custom->prefix_response_func) {
            s = callbacks->custom->prefix_response_func(response);
		}

		if (s == KS_STATUS_SUCCESS) {
			s =  callbacks->response_func(response);
		}

		if (s == KS_STATUS_SUCCESS && callbacks->custom &&  callbacks->custom->postfix_response_func) {
			s =  callbacks->custom->postfix_response_func(response);
		}

		ks_mutex_unlock(callbacks->lock);

		return s;
	}

	ks_log(KS_LOG_ERROR, "Unable to find message handler for %s\n", command);
	
	return KS_STATUS_FAIL;
}

/*
 *
*/
KS_DECLARE(ks_status_t) blade_rpc_process_jsonmessage(cJSON *request, cJSON **responseP)
{
	ks_status_t respstatus = blade_rpc_process_jsonmessage_all(request, responseP);
	cJSON *response = *responseP;
	if (respstatus == KS_STATUS_SUCCESS && response != NULL) {
		blade_rpc_write_json(response);
	}
	return respstatus;
}

KS_DECLARE(ks_status_t) blade_rpc_process_data(const uint8_t *data,
                                                        ks_size_t size)
{
	
	cJSON *json = cJSON_Parse((const char*)data);
	if (json != NULL) {
		ks_log(	KS_LOG_ERROR, "Unable to parse message\n");
		return KS_STATUS_FAIL;
	}

	/* deal with rpc message */
	if (ks_rpcmessage_isrpc(json)) {
		cJSON *response = NULL;
		ks_status_t respstatus = blade_rpc_process_jsonmessage_all(json, &response);
		if (respstatus == KS_STATUS_SUCCESS && response != NULL) {
			blade_rpc_write_json(response);
			cJSON_Delete(response);
		}
		return respstatus;
	}
	
	ks_log(KS_LOG_ERROR, "Unable to identify message type\n");
	
	return KS_STATUS_FAIL;
}

KS_DECLARE(ks_status_t) blade_rpc_process_blademessage(blade_message_t *message)
{
	uint8_t* data = NULL;
	ks_size_t size = 0;

	blade_message_get(message, (void **)&data, &size);

	if (data && size>0) {
		ks_status_t s = blade_rpc_process_data(data, size);
		blade_message_discard(&message);
		return s;
	} 
	
	ks_log(KS_LOG_ERROR, "Message read failed\n");
	return KS_STATUS_FAIL;

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

