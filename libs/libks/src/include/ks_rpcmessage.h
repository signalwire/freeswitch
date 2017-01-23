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





typedef struct ks_rpcmessaging_handle_s ks_rpcmessaging_handle_t;



typedef  ks_status_t (*jrpc_func_t)(ks_rpcmessaging_handle_t* handle, cJSON *params, cJSON **responseP);




KS_DECLARE(ks_rpcmessaging_handle_t *) ks_rpcmessage_init(ks_pool_t* pool, ks_rpcmessaging_handle_t** handleP);
KS_DECLARE(void)                    ks_rpcmessage_deinit(ks_rpcmessaging_handle_t** handleP);

KS_DECLARE(cJSON *)ks_rpcmessage_new_request(ks_rpcmessaging_handle_t* handle, 
											const char *method, 
											cJSON **parmsP,
											cJSON **requestP);
KS_DECLARE(cJSON *)ks_rpcmessage_new_response(ks_rpcmessaging_handle_t* handle, 
											const cJSON *request, 
											cJSON *result, 
											cJSON **responseP);

KS_DECLARE(ks_status_t)ks_rpcmessage_namespace(ks_rpcmessaging_handle_t* handle, const char* namespace);

KS_DECLARE(ks_status_t)ks_rpcmessage_register_function(ks_rpcmessaging_handle_t* handle, const char *command, jrpc_func_t func);
KS_DECLARE(jrpc_func_t) ks_rpcmessage_find_function(ks_rpcmessaging_handle_t* handle, const char *command);

KS_DECLARE(ks_status_t) ks_rpcmessage_process_request(ks_rpcmessaging_handle_t* handle, 
														uint8_t *data, 
														ks_size_t size, 
														cJSON **responseP);
KS_DECLARE(ks_status_t) ks_rpcmessage_process_jsonrequest(ks_rpcmessaging_handle_t* handle, cJSON *request, cJSON **responseP);


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
