/*============================================================================
                         xmlrpc_client_int.h
==============================================================================
  This header file defines the interface between client modules inside
  xmlrpc-c.

  Use this in addition to xmlrpc_client.h, which defines the external
  interface.

  Copyright information is at the end of the file.
============================================================================*/


#ifndef  XMLRPC_CLIENT_INT_H_INCLUDED
#define  XMLRPC_CLIENT_INT_H_INCLUDED

#include "xmlrpc-c/util.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _xmlrpc_server_info {
    const char * serverUrl;
    struct {
        bool basic;
        bool digest;
        bool gssnegotiate;
        bool ntlm;
    } allowedAuth;
    const char * userNamePw;
        /* The username/password value for HTTP, i.e. in
           "user:password" form

           This can be NULL to indicate "none", but only if 'allowedAuth'
           doesn't allow any form of authentication.
        */
    const char * basicAuthHdrValue;
        /* A complete value for an HTTP Authorization: header that
           requests HTTP basic authentication.  This exists whether
           or not 'allowedAuth' allows basic authentication, and is
           completely redundant with 'userNamePw'.  It exists mainly
           for historical reasons, and may also save some computation
           when the same xmrpc_server_info is used for multiple
           HTTP connections.

           This is NULL exactly when 'userNamePw' is NULL.
        */
};

/*=========================================================================
** Transport Implementation functions.
**========================================================================= */
#include "xmlrpc-c/transport.h"

/* The generalized event loop. This uses the above flags. For more details,
** see the wrapper functions below. If you're not using the timeout, the
** 'milliseconds' parameter will be ignored.
** Note that ANY event loop call will return immediately if there are
** no outstanding XML-RPC calls. */
extern void
xmlrpc_client_event_loop_run_general (int flags, xmlrpc_timeout milliseconds);

/* Run the event loop forever. The loop will exit if someone calls
** xmlrpc_client_event_loop_end. */
extern void
xmlrpc_client_event_loop_run (void);

/* Run the event loop forever. The loop will exit if someone calls
** xmlrpc_client_event_loop_end or the timeout expires.
** (Note that ANY event loop call will return immediately if there are
** no outstanding XML-RPC calls.) */
extern void
xmlrpc_client_event_loop_run_timeout (xmlrpc_timeout milliseconds);

/* End the running event loop immediately. This can also be accomplished
** by calling the corresponding function in libwww.
** (Note that ANY event loop call will return immediately if there are
** no outstanding XML-RPC calls.) */
extern void
xmlrpc_client_event_loop_end (void);


/* Return true if there are uncompleted asynchronous calls.
** The exact value of this during a response callback is undefined. */
extern int
xmlrpc_client_asynch_calls_are_unfinished (void);



/*=========================================================================
** Interface between global client and general client functions.
** (These are necessary because there are some global client functions
** that don't have exported private client versions because we don't like
** them and have them for global functions only for backward compatibility.
** The global client functions existed before any private client ones did).
**========================================================================= */
void
xmlrpc_client_call_server2_va(xmlrpc_env *               const envP,
                              struct xmlrpc_client *     const clientP,
                              const xmlrpc_server_info * const serverInfoP,
                              const char *               const methodName,
                              const char *               const format,
                              va_list                          args,
                              xmlrpc_value **            const resultPP);

void
xmlrpc_client_start_rpcf_server_va(
    xmlrpc_env *               const envP,
    struct xmlrpc_client *     const clientP,
    const xmlrpc_server_info * const serverInfoP,
    const char *               const methodName,
    xmlrpc_response_handler responseHandler,
    void *                     const userData,
    const char *               const format,
    va_list                    args);

/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif



