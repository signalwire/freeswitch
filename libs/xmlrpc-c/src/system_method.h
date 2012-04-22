#ifndef SYSTEM_METHOD_H_INCLUDED
#define SYSTEM_METHOD_H_INCLUDED


void
xmlrpc_installSystemMethods(struct _xmlrpc_env *     const envP,
                            struct xmlrpc_registry * const registryP);

void
xmlrpc_buildSignatureArray(xmlrpc_env *    const envP,
                           const char *    const sigListString,
                           xmlrpc_value ** const resultPP);

#endif
