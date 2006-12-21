/* Interface header file for libxmlrpc_server_cgi.

   By Bryan Henderson, 05.04.27.  Contributed to the public domain.
*/

#ifndef  XMLRPC_CGI_H_INCLUDED
#define  XMLRPC_CGI_H_INCLUDED

#include <xmlrpc-c/server.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void
xmlrpc_server_cgi_process_call(xmlrpc_registry * const registryP);

#define XMLRPC_CGI_NO_FLAGS (0)

extern void
xmlrpc_cgi_init (int flags);

extern xmlrpc_registry *
xmlrpc_cgi_registry (void);

void
xmlrpc_cgi_add_method(const char *  const method_name,
                      xmlrpc_method const method,
                      void *        const user_data);

void
xmlrpc_cgi_add_method_w_doc(const char *  const method_name,
                            xmlrpc_method const method,
                            void *        const user_data,
                            const char *  const signature,
                            const char *  const help);
extern void
xmlrpc_cgi_process_call (void);

extern void
xmlrpc_cgi_cleanup (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
