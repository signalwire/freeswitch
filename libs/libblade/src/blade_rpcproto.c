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
    jrpc_func_t  prefix_request_func;
    jrpc_func_t  postfix_request_func;

    jrpc_func_t  prefix_response_func;
    jrpc_func_t  postfix_response_func;

} blade_rpc_custom_callbackpair_t;



typedef struct blade_rpc_callbackpair_s
{
    jrpc_func_t         request_func;

    jrpc_func_t         response_func;

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
			x = -1;
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


//KS_DECLARE(ks_status_t) blade_rpc_onconnect(ks_pool_t *pool, blade_peer_t* peer)
//{
//
//
//	return KS_STATUS_FAIL;
//}
//
//KS_DECLARE(ks_status_t) blade_rpc_disconnect(blade_peer_t* peer)
//{
//
//	return KS_STATUS_FAIL;
//}



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

		ks_hash_read_lock(n->method_hash);

        callbacks = ks_hash_search(n->method_hash, command, KS_UNLOCKED);

		if (callbacks) {
			ks_mutex_lock(callbacks->lock);
		}

		ks_hash_read_unlock(n->method_hash);
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

		if (callbacks) {
			ks_mutex_lock(callbacks->lock);
		}

        ks_hash_read_unlock(n->method_hash);
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

		if (callbacks) {
			ks_mutex_lock(callbacks->lock);
		}

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

KS_DECLARE(jrpc_func_t) blade_rpc_find_requestprefix_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

    if (!callbacks || !callbacks->custom) {
        return NULL;
    }

    jrpc_func_t f = callbacks->custom->prefix_request_func;

    ks_mutex_unlock(callbacks->lock);

    return f;
}

KS_DECLARE(jrpc_func_t) blade_rpc_find_response_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

	if (!callbacks) {
		return NULL;
	}
		
    jrpc_func_t f = callbacks->response_func;

	ks_mutex_unlock(callbacks->lock);

	return f;
}

KS_DECLARE(jrpc_func_t) blade_rpc_find_responseprefix_function(char *fqcommand)
{

    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked_fq(fqcommand);

    if (!callbacks || !callbacks->custom) {
        return NULL;
    }

    jrpc_func_t f = callbacks->custom->prefix_response_func;

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
		ks_hash_create(&n->method_hash,
					KS_HASH_MODE_CASE_SENSITIVE,
					KS_HASH_FLAG_RWLOCK + KS_HASH_FLAG_DUP_CHECK + KS_HASH_FLAG_FREE_VALUE,
					g_handle->pool);
		ks_hash_insert(g_handle->namespace_hash, n->name, n);
	}
	ks_hash_write_unlock(g_handle->namespace_hash);

    ks_log(KS_LOG_DEBUG, "Setting message namespace value %s, version %s\n", namespace, version);

    return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t)blade_rpc_register_function(char *namespace, 
                                                char *command,
                                                jrpc_func_t func,
                                                jrpc_func_t respfunc)
{
    if (!func && !respfunc) {
        return KS_STATUS_FAIL;
    }

	char nskey[KS_RPCMESSAGE_NAMESPACE_LENGTH+1];
	memset(nskey, 0, sizeof(nskey));
	strcpy(nskey, namespace);

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));

//    strcpy(fqcommand, namespace);
//    strcpy(fqcommand, ".");
    strcat(fqcommand, command);

    int lkey = strlen(command)+1;

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


KS_DECLARE(ks_status_t)blade_rpc_register_custom_request_function(char *namespace, 
													char *command,
													jrpc_func_t prefix_func,
													jrpc_func_t postfix_func)
{
	ks_status_t s = KS_STATUS_FAIL;

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));
    strcat(fqcommand, command);

    ks_hash_write_lock(g_handle->namespace_hash);
	blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked(namespace, fqcommand);

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

KS_DECLARE(ks_status_t)blade_rpc_register_custom_response_function(char* namespace, 
													char *command,
													jrpc_func_t prefix_func,
													jrpc_func_t postfix_func)
{
	ks_status_t s = KS_STATUS_FAIL;

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));
    strcat(fqcommand, command);
	
	ks_hash_write_lock(g_handle->namespace_hash);
    blade_rpc_callbackpair_t *callbacks = blade_rpc_find_callbacks_locked(namespace, fqcommand);

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
    char nskey[KS_RPCMESSAGE_NAMESPACE_LENGTH];
    memset(nskey, 0, sizeof(nskey));
    strcat(nskey, namespace);

    ks_hash_write_lock(g_handle->namespace_hash);

	blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, nskey, KS_UNLOCKED);

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
    char nskey[KS_RPCMESSAGE_NAMESPACE_LENGTH];
    memset(nskey, 0, sizeof(nskey));
    strcat(nskey, templatename);


    /* find/insert to namespace hash as needed */
    ks_hash_write_lock(g_handle->template_hash);
    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->template_hash, nskey, KS_UNLOCKED);

    if (n == NULL) {
        n = ks_pool_alloc(g_handle->pool, sizeof (blade_rpc_namespace_t) + strlen(templatename) + 1);
        strncpy(n->name, templatename, KS_RPCMESSAGE_NAMESPACE_LENGTH);
		strncpy(n->version, version, KS_RPCMESSAGE_VERSION_LENGTH);
		ks_hash_create(&n->method_hash,
					KS_HASH_MODE_CASE_SENSITIVE,
                    KS_HASH_FLAG_RWLOCK + KS_HASH_FLAG_DUP_CHECK + KS_HASH_FLAG_FREE_VALUE,
                    g_handle->pool);
        ks_hash_insert(g_handle->template_hash, n->name, n);
    }
    ks_hash_write_unlock(g_handle->template_hash);

    ks_log(KS_LOG_DEBUG, "Declaring application template namespace %s, version %s\n", templatename, version);

    return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t)blade_rpc_register_template_function(char *name,
                                                char *command,
                                                jrpc_func_t func,
                                                jrpc_func_t respfunc)
{
	(void)blade_rpc_find_template_locked;   //remove

    if (!func && !respfunc) {
        return KS_STATUS_FAIL;
    }

    char nskey[KS_RPCMESSAGE_NAMESPACE_LENGTH];
    memset(nskey, 0, sizeof(nskey));
    strcat(nskey, name);

    char fqcommand[KS_RPCMESSAGE_FQCOMMAND_LENGTH];
    memset(fqcommand, 0, sizeof(fqcommand));
    strcat(fqcommand, command);

    int lkey = strlen(fqcommand)+1;

    if (lkey < 16) {
        lkey = 16;
    }

    ks_hash_read_lock(g_handle->template_hash);    /* lock template hash */

    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->template_hash, nskey, KS_UNLOCKED);

    if (n == NULL) {
        ks_hash_read_unlock(g_handle->template_hash);
        ks_log(KS_LOG_ERROR, "Unable to find %s template\n", name);
        return KS_STATUS_FAIL;
    }

	ks_hash_read_lock(n->method_hash);
    blade_rpc_callbackpair_t* callbacks = ks_hash_search(n->method_hash, fqcommand, KS_UNLOCKED);
    ks_hash_read_unlock(n->method_hash);

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

		strcat(callbacks->command, template);
		strcat(callbacks->command, ".");
		strcat(callbacks->command, t_callback->command);
		
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
 * create a request message
 */
KS_DECLARE(ks_rpcmessageid_t) blade_rpc_create_request(char *namespace,
                                                    char *method,
                                                    blade_rpc_fields_t* fields,
                                                    cJSON **paramsP,
                                                    cJSON **requestP)
{
	cJSON *jversion = NULL;
	blade_rpc_callbackpair_t* callbacks = NULL;

	*requestP = NULL;

    ks_hash_read_lock(g_handle->namespace_hash);
    blade_rpc_namespace_t *n =  ks_hash_search(g_handle->namespace_hash, namespace, KS_UNLOCKED);

	if (n) {
		ks_hash_read_lock(n->method_hash);
		callbacks = ks_hash_search(n->method_hash, method, KS_UNLOCKED);				
		if (callbacks) {
			jversion = cJSON_CreateString(n->version);
		}
		ks_hash_read_unlock(n->method_hash);
	}

	ks_hash_read_unlock(g_handle->namespace_hash);

	if (!n) {
		ks_log(KS_LOG_ERROR, "No namespace %s found\n", namespace);
		return 0;
	}	

	if (!callbacks) {
        ks_log(KS_LOG_ERROR, "No method %s.%s found\n", namespace, method);
        return 0;
	}

	ks_rpcmessageid_t msgid = ks_rpcmessage_create_request(namespace, method, paramsP, requestP);
	
	if (!msgid || *requestP == NULL) {
		ks_log(KS_LOG_ERROR, "Unable to create rpc message for method %s.%s\n", namespace, method);
		return 0;		
	}
  
	cJSON *jfields = cJSON_CreateObject();

	cJSON_AddItemToObject(jfields, "version", jversion);

	if (fields->to) {
		cJSON_AddStringToObject(jfields, "to", fields->to);
	}

	if (fields->from) {
		cJSON_AddStringToObject(jfields, "from", fields->from);
	}

    if (fields->token) {
        cJSON_AddStringToObject(jfields, "token", fields->token);
    }

	 cJSON_AddItemToObject(*requestP, "blade", jfields);

	return msgid;
}

KS_DECLARE(ks_rpcmessageid_t) blade_rpc_create_response(cJSON *request,
                                                    cJSON **replyP,
                                                    cJSON **responseP)
{
	cJSON *jfields = cJSON_GetObjectItem(request, "blade");

	if (!jfields) {
		ks_log(KS_LOG_ERROR, "No blade routing info found.  Unable to create response\n");
		return 0;	
	}

	ks_rpcmessageid_t msgid = ks_rpcmessage_create_response(request, replyP, responseP);

	if (!msgid || *responseP == NULL) {
		ks_log(KS_LOG_ERROR, "Unable to create rpc response message\n");  //TODO : Add namespace, method from request 
		return 0;
	}

	const char *to =    cJSON_GetObjectCstr(jfields, "to");
	const char *from =  cJSON_GetObjectCstr(jfields, "from");
	const char *token = cJSON_GetObjectCstr(jfields, "token");
	const char *version =  cJSON_GetObjectCstr(jfields, "version");

	cJSON *blade = cJSON_CreateObject(); 

	if (to) {
        cJSON_AddStringToObject(blade, "to", from);
    }

	if (from) {
		cJSON_AddStringToObject(blade, "from", to);
	}

	if (token) {
		cJSON_AddStringToObject(blade, "token", token);
	}

    if (version) {
        cJSON_AddStringToObject(blade, "version", version);
    }

	cJSON_AddItemToObject(*responseP, "blade", blade);

	return msgid;
}

KS_DECLARE(ks_rpcmessageid_t) blade_rpc_create_errorresponse(cJSON *request,
                                                    cJSON **errorP,
                                                    cJSON **responseP)
{
	ks_rpcmessageid_t msgid = blade_rpc_create_response(request, NULL, responseP);

	if (msgid) {

		if 	(errorP) {

			if (*errorP) {
				cJSON_AddItemToObject(*responseP, "error", *errorP);
			}
			else {
				cJSON *error = cJSON_CreateObject();
				cJSON_AddItemToObject(*responseP, "error", error);
				*errorP = error;
			}
		}	
	}	
	
	return msgid;	
}


const char BLADE_JRPC_METHOD[] = "method";
const char BLADE_JRPC_ID[]     = "id";
const char BLADE_JRPC_FIELDS[] = "blade";
const char BLADE_JRPC_TO[]     = "to";
const char BLADE_JRPC_FROM[]   = "from";
const char BLADE_JRPC_TOKEN[]  = "token";
const char BLADE_JRPC_VERSION[] = "version";

KS_DECLARE(ks_status_t) blade_rpc_parse_message(cJSON *message,
													char **namespaceP,
													char **methodP,
													char **versionP,
													uint32_t *idP,
													blade_rpc_fields_t **fieldsP)
{
	const char *m = cJSON_GetObjectCstr(message, BLADE_JRPC_METHOD);
	cJSON *blade  = cJSON_GetObjectItem(message, BLADE_JRPC_FIELDS);
	cJSON *jid    = cJSON_GetObjectItem(message, BLADE_JRPC_ID);

	*fieldsP    = NULL;
	*namespaceP = NULL;
	*versionP   = NULL;
	*methodP    = NULL;
	*idP        = 0;

	if (jid) {
		*idP = jid->valueint; 
	}

	if (!m || !blade) {
		const char *buffer = cJSON_PrintUnformatted(message);
		ks_log(KS_LOG_ERROR, "Unable to locate necessary fields in message:\n%s\n", buffer);
		ks_pool_free(g_handle->pool, buffer);
		return KS_STATUS_FAIL;	
	}

    cJSON *jto    = cJSON_GetObjectItem(blade, BLADE_JRPC_TO);
    cJSON *jfrom  = cJSON_GetObjectItem(blade, BLADE_JRPC_FROM);
    cJSON *jtoken = cJSON_GetObjectItem(blade, BLADE_JRPC_TOKEN);


	ks_size_t len = KS_RPCMESSAGE_COMMAND_LENGTH   + 1 + 
					KS_RPCMESSAGE_NAMESPACE_LENGTH + 1 +
					KS_RPCMESSAGE_VERSION_LENGTH   + 1 +
					sizeof(blade_rpc_fields_t) + 1;

	uint32_t lento = 0;
	uint32_t lenfrom = 0;
	uint32_t lentoken = 0;

	if (jto) {
		lento = strlen(jto->valuestring) + 1;
		len += lento;
	}

	if (jfrom) {
		lenfrom += strlen(jfrom->valuestring) + 1;
		len += lenfrom;
	}

	if (jtoken) {
		lentoken += strlen(jtoken->valuestring) + 1;
		len += lentoken;
	}

	blade_rpc_fields_t *fields =  (blade_rpc_fields_t *)ks_pool_alloc(g_handle->pool, len);

    char *namespace = (char*)fields + sizeof(blade_rpc_fields_t);
    char *command   = namespace + KS_RPCMESSAGE_NAMESPACE_LENGTH + 1;
    char *version   = command + KS_RPCMESSAGE_COMMAND_LENGTH + 1;
	
	char *ptr = version + KS_RPCMESSAGE_VERSION_LENGTH + 1;

	if (jto) {
		strcpy(ptr, jto->valuestring);
		fields->to = ptr;
		ptr += strlen(jto->valuestring) + 1;
	}

	if (jfrom) {
		strcpy(ptr, jfrom->valuestring);
		fields->from = ptr;
		ptr += strlen(jfrom->valuestring) + 1;
	}

	if (jtoken) {
		strcpy(ptr, jtoken->valuestring);
		fields->token = ptr;
		ptr += strlen(jtoken->valuestring) + 1;
    }

    blade_rpc_parse_fqcommand(m, namespace, command);
	
	strcpy(version, cJSON_GetObjectCstr(blade, BLADE_JRPC_VERSION));

	*fieldsP    = fields;	
	*namespaceP = namespace;
	*methodP    = command;
	*versionP   = version;

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
		return KS_STATUS_SUCCESS;
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
static ks_status_t blade_rpc_process_jsonmessage_all(cJSON *request)
{
	const char *fqcommand = cJSON_GetObjectCstr(request, "method");
	cJSON *error = NULL;
	cJSON *response = NULL;
	cJSON *responseP = NULL;

	if (!fqcommand) {
		error = cJSON_CreateObject();
		cJSON_AddStringToObject(error, "errormessage", "Command not specified");
        ks_rpcmessage_create_request("rpcprotocol", "unknowncommand", &error, &responseP);
		blade_rpc_write_json(responseP);
        return KS_STATUS_FAIL;
	}


	char namespace[KS_RPCMESSAGE_NAMESPACE_LENGTH];
	char command[KS_RPCMESSAGE_COMMAND_LENGTH];

    blade_rpc_parse_fqcommand(fqcommand, namespace, command);
    blade_rpc_callbackpair_t* callbacks = blade_rpc_find_callbacks_locked(namespace, command);

    if (!callbacks) {
		error = cJSON_CreateObject();
		cJSON_AddStringToObject(error, "errormessage", "Command not supported");
        ks_rpcmessage_create_response(request, &error, &responseP);
	    blade_rpc_write_json(responseP);
        return  KS_STATUS_FAIL;
    }

	//todo - add more checks ? 

	ks_bool_t isrequest = ks_rpcmessage_isrequest(request);

	enum jrpc_status_t jrcs = 0;
	
	if (isrequest && callbacks->request_func) {

		cJSON *responseP = NULL;
	
		if (callbacks->custom && callbacks->custom->prefix_request_func) {
			jrcs = callbacks->custom->prefix_request_func(request, &responseP);
			if ( (jrcs & JRPC_SEND) && responseP) {
				blade_rpc_write_json(responseP);
				cJSON_Delete(responseP);
				responseP = NULL;
			}
		}

		if ( !(jrcs & JRPC_EXIT) && jrcs != JRPC_ERROR) {
			jrcs =  callbacks->request_func(request, &responseP);
			if ((jrcs & JRPC_SEND) && responseP) {
				blade_rpc_write_json(responseP);
				cJSON_Delete(responseP);
				responseP = NULL;
			}
		}

		if (  !(jrcs & JRPC_EXIT) && jrcs != JRPC_ERROR && callbacks->custom && callbacks->custom->postfix_request_func) {
			jrcs =  callbacks->custom->postfix_request_func(request, &responseP);
			if ( (jrcs & JRPC_SEND) && responseP) {
				blade_rpc_write_json(responseP);
				cJSON_Delete(responseP);
				responseP = NULL;
			}
		}

		ks_mutex_unlock(callbacks->lock);

		if (jrcs == JRPC_ERROR) {
			return KS_STATUS_FAIL;
		}

		return KS_STATUS_SUCCESS;
	}
    else if (!isrequest && callbacks->response_func) {

        if (callbacks->custom && callbacks->custom->prefix_response_func) {
            jrcs = callbacks->custom->prefix_response_func(response, &responseP);
            if ( (jrcs & JRPC_SEND) && responseP) {
                blade_rpc_write_json(responseP);
                cJSON_Delete(responseP);
                responseP = NULL;
            }
		}

		if ( !(jrcs & JRPC_EXIT) && jrcs != JRPC_ERROR) {
			jrcs =  callbacks->response_func(response, &responseP);
            if ( (jrcs & JRPC_SEND) && responseP) {
                blade_rpc_write_json(responseP);
                cJSON_Delete(responseP);
                responseP = NULL;
            }
		}

		if ( !(jrcs & JRPC_EXIT) && jrcs != JRPC_ERROR && callbacks->custom &&  callbacks->custom->postfix_response_func) {
			jrcs =  callbacks->custom->postfix_response_func(response, &responseP);
			if ( (jrcs & JRPC_SEND) && responseP) {
				blade_rpc_write_json(responseP);
				cJSON_Delete(responseP);
				responseP = NULL;
			}
		}

		ks_mutex_unlock(callbacks->lock);

		if (jrcs == JRPC_ERROR) {
			return KS_STATUS_FAIL;
		}

        return KS_STATUS_SUCCESS;
	}

	ks_log(KS_LOG_ERROR, "Unable to find message handler for %s\n", command);
	
	return KS_STATUS_FAIL;
}

/*
 *
*/
KS_DECLARE(ks_status_t) blade_rpc_process_jsonmessage(cJSON *request)
{
	ks_status_t respstatus = blade_rpc_process_jsonmessage_all(request);
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
		ks_status_t respstatus = blade_rpc_process_jsonmessage_all(json);
		return respstatus;
	}
	
	ks_log(KS_LOG_ERROR, "Unable to identify message type\n");
	
	return KS_STATUS_FAIL;
}

//KS_DECLARE(ks_status_t) blade_rpc_process_blademessage(blade_message_t *message)
//{
//	uint8_t* data = NULL;
//	ks_size_t size = 0;
//
//	blade_message_get(message, (void **)&data, &size);
//
//	if (data && size>0) {
//		ks_status_t s = blade_rpc_process_data(data, size);
//		blade_message_discard(&message);
//		return s;
//	} 
//	
//	ks_log(KS_LOG_ERROR, "Message read failed\n");
//	return KS_STATUS_FAIL;
//
//}


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

