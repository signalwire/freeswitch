/* Interface header file for libxmlrpc_server_cgi.

   By Bryan Henderson, 05.04.27.  Contributed to the public domain.
*/

#ifndef  XMLRPC_CGI_H_INCLUDED
#define  XMLRPC_CGI_H_INCLUDED

#include <xmlrpc-c/c_util.h>
#include <xmlrpc-c/server.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

XMLRPC_DLLEXPORT
void
xmlrpc_server_cgi_process_call(xmlrpc_registry * const registryP);

#define XMLRPC_CGI_NO_FLAGS (0)

XMLRPC_DLLEXPORT
extern void
xmlrpc_cgi_init(int const flags);

XMLRPC_DLLEXPORT
extern xmlrpc_registry *
xmlrpc_cgi_registry (void);

XMLRPC_DLLEXPORT
void
xmlrpc_cgi_add_method(const char *  const method_name,
                      xmlrpc_method const method,
                      void *        const user_data);

XMLRPC_DLLEXPORT
void
xmlrpc_cgi_add_method_w_doc(const char *  const method_name,
                            xmlrpc_method const method,
                            void *        const user_data,
                            const char *  const signature,
                            const char *  const help);
XMLRPC_DLLEXPORT
extern void
xmlrpc_cgi_process_call (void);

XMLRPC_DLLEXPORT
extern void
xmlrpc_cgi_cleanup (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
