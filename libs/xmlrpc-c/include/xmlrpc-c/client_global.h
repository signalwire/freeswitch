#ifndef CLIENT_GLOBAL_H_INCLUDED
#define CLIENT_GLOBAL_H_INCLUDED

#include <xmlrpc-c/c_util.h>
#include <xmlrpc-c/client.h>

/*=========================================================================
**  Initialization and Shutdown
**=========================================================================
**  These routines initialize and terminate the XML-RPC client. If you're
**  already using libwww on your own, you can pass
**  XMLRPC_CLIENT_SKIP_LIBWWW_INIT to avoid initializing it twice.
*/

#define XMLRPC_CLIENT_NO_FLAGS         (0)
#define XMLRPC_CLIENT_SKIP_LIBWWW_INIT (1)

XMLRPC_DLLEXPORT
extern void
xmlrpc_client_init(int          const flags,
                   const char * const appname,
                   const char * const appversion);

XMLRPC_DLLEXPORT
void 
xmlrpc_client_init2(xmlrpc_env *                      const env,
                    int                               const flags,
                    const char *                      const appname,
                    const char *                      const appversion,
                    const struct xmlrpc_clientparms * const clientparms,
                    unsigned int                      const parm_size);

XMLRPC_DLLEXPORT
extern void
xmlrpc_client_cleanup(void);

/*=========================================================================
**  xmlrpc_client_call
**=========================================================================
**  A synchronous XML-RPC client. Do not attempt to call any of these
**  functions from inside an asynchronous callback!
*/

XMLRPC_DLLEXPORT
xmlrpc_value * 
xmlrpc_client_call(xmlrpc_env * const envP,
                   const char * const server_url,
                   const char * const method_name,
                   const char * const format,
                   ...);

XMLRPC_DLLEXPORT
xmlrpc_value * 
xmlrpc_client_call_params(xmlrpc_env *   const envP,
                          const char *   const serverUrl,
                          const char *   const methodName,
                          xmlrpc_value * const paramArrayP);

XMLRPC_DLLEXPORT
xmlrpc_value * 
xmlrpc_client_call_server(xmlrpc_env *               const envP,
                          const xmlrpc_server_info * const server,
                          const char *               const method_name,
                          const char *               const format, 
                          ...);

XMLRPC_DLLEXPORT
xmlrpc_value *
xmlrpc_client_call_server_params(
    xmlrpc_env *               const envP,
    const xmlrpc_server_info * const serverP,
    const char *               const method_name,
    xmlrpc_value *             const paramArrayP);

XMLRPC_DLLEXPORT
void
xmlrpc_client_transport_call(
    xmlrpc_env *               const envP,
    void *                     const reserved,  /* for client handle */
    const xmlrpc_server_info * const serverP,
    xmlrpc_mem_block *         const callXmlP,
    xmlrpc_mem_block **        const respXmlPP);


/*=========================================================================
**  xmlrpc_client_call_asynch
**=========================================================================
**  An asynchronous XML-RPC client.
*/

/* Make an asynchronous XML-RPC call. We make internal copies of all
** arguments except user_data, so you can deallocate them safely as soon
** as you return. Errors will be passed to the callback. You will need
** to run the event loop somehow; see below.
** WARNING: If an error occurs while building the argument, the
** response handler will be called with a NULL param_array. */
XMLRPC_DLLEXPORT
void 
xmlrpc_client_call_asynch(const char * const server_url,
                          const char * const method_name,
                          xmlrpc_response_handler callback,
                          void *       const user_data,
                          const char * const format,
                          ...);

/* As above, but use an xmlrpc_server_info object. The server object can be
** safely destroyed as soon as this function returns. */
XMLRPC_DLLEXPORT
void 
xmlrpc_client_call_server_asynch(xmlrpc_server_info * const server,
                                 const char *         const method_name,
                                 xmlrpc_response_handler callback,
                                 void *               const user_data,
                                 const char *         const format,
                                 ...);

/* As above, but the parameter list is supplied as an xmlrpc_value
** containing an array.
*/
XMLRPC_DLLEXPORT
void
xmlrpc_client_call_asynch_params(const char *   const server_url,
                                 const char *   const method_name,
                                 xmlrpc_response_handler callback,
                                 void *         const user_data,
                                 xmlrpc_value * const paramArrayP);
    
/* As above, but use an xmlrpc_server_info object. The server object can be
** safely destroyed as soon as this function returns. */
XMLRPC_DLLEXPORT
void 
xmlrpc_client_call_server_asynch_params(
    xmlrpc_server_info * const server,
    const char *         const method_name,
    xmlrpc_response_handler callback,
    void *               const user_data,
    xmlrpc_value *       const paramArrayP);
    
/*=========================================================================
**  Event Loop Interface
**=========================================================================
**  These functions can be used to run the XML-RPC event loop. If you
**  don't like these, you can also run the libwww event loop directly.
*/

/* Finish all outstanding asynchronous calls. Alternatively, the loop
** will exit if someone calls xmlrpc_client_event_loop_end. */
XMLRPC_DLLEXPORT
extern void
xmlrpc_client_event_loop_finish_asynch(void);


/* Finish all outstanding asynchronous calls. */
XMLRPC_DLLEXPORT
extern void
xmlrpc_client_event_loop_finish_asynch_timeout(unsigned long const milliseconds);

#endif
