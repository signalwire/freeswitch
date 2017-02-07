/*
 * Copyright (c) 2017, FreeSWITCH Solutions LLC 
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


#ifndef _KS_RPCMESSAGE_H_
#define _KS_RPCMESSAGE_H_

#include "ks.h"

KS_BEGIN_EXTERN_C

#define KS_RPCMESSAGE_NAMESPACE_LENGTH 16
#define KS_RPCMESSAGE_COMMAND_LENGTH  238
#define KS_RPCMESSAGE_FQCOMMAND_LENGTH  (KS_RPCMESSAGE_NAMESPACE_LENGTH+KS_RPCMESSAGE_COMMAND_LENGTH+1)
#define KS_RPCMESSAGE_VERSION_LENGTH 9


typedef uint32_t ks_rpcmessage_id;


KS_DECLARE(void) ks_rpcmessage_init(ks_pool_t *pool);

KS_DECLARE(void*) ks_json_pool_alloc(ks_size_t size);
KS_DECLARE(void) ks_json_pool_free(void *ptr);


KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_request(char *namespace, 
											char *method,
											char *sessionid,
											char *version, 
											cJSON **parmsP,
											cJSON **requestP);

KS_DECLARE(ks_size_t) ks_rpc_create_buffer(char *namespace,
                                            char *method,
											char *sessionid,
											char *version,
                                            cJSON **parmsP,
                                            ks_buffer_t *buffer);

KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_response( 
											const cJSON *request, 
											cJSON **resultP, 
											cJSON **responseP);

KS_DECLARE(ks_rpcmessage_id) ks_rpcmessage_create_errorresponse(
                                            const cJSON *request,
                                            cJSON **errorP,
                                            cJSON **responseP);

KS_DECLARE(ks_bool_t) ks_rpcmessage_isrequest(cJSON *msg);

KS_DECLARE(ks_bool_t) ks_rpcmessage_isrpc(cJSON *msg);

KS_END_EXTERN_C

#endif							/* defined(_KS_RPCMESSAGE_H_) */

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
