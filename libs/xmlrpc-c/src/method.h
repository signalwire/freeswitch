#ifndef METHOD_H_INCLUDED
#define METHOD_H_INCLUDED

#include "xmlrpc-c/base.h"

struct xmlrpc_signature {
    struct xmlrpc_signature * nextP;
    const char * retType;
        /* Name of the XML-RPC element that represents the return value
           type, e.g. "int" or "dateTime.iso8601"
        */
    unsigned int argCount;
        /* Number of arguments method takes */
    unsigned int argListSpace;
        /* Number of slots that exist in the argList[] (i.e. memory is
           allocated)
        */
    const char ** argList;
        /* Array of size 'argCount'.  argList[i] is the name of the type
           of argument i.  Like 'retType', e.g. "string".

           The strings are constants, not malloc'ed.
        */
};

typedef struct xmlrpc_signatureList {
    /* A list of signatures for a method.  Each signature describes one
       alternative form of invoking the method (a
       single method might have multiple forms, e.g. one takes two integer
       arguments; another takes a single string).
    */
    struct xmlrpc_signature * firstSignatureP;
} xmlrpc_signatureList;

struct xmlrpc_registry {
    bool                        introspectionEnabled;
    struct xmlrpc_methodList *  methodListP;
    xmlrpc_default_method       defaultMethodFunction;
    void *                      defaultMethodUserData;
    xmlrpc_preinvoke_method     preinvokeFunction;
    void *                      preinvokeUserData;
    xmlrpc_server_shutdown_fn * shutdownServerFn;
        /* Function that can be called to shut down the server that is
           using this registry.  NULL if none.
        */
    void * shutdownContext;
        /* Context for _shutdown_server_fn -- understood only by
           that function, passed to it as argument.
        */
    xmlrpc_dialect dialect;
};

typedef struct {
/*----------------------------------------------------------------------------
   Everything a registry knows about one XML-RPC method
-----------------------------------------------------------------------------*/
    /* One of the methodTypeX fields is NULL and the other isn't.
       (The reason there are two is backward compatibility.  Old
       programs set up the registry with Type 1; modern ones set it up
       with Type 2.
    */
    xmlrpc_method1 methodFnType1;
        /* The method function, if it's type 1.  Null if it's not */
    xmlrpc_method2 methodFnType2;
        /* The method function, if it's type 2.  Null if it's not */
    void * userData;
        /* Passed to method function */
    size_t stackSize;
        /* Amount of stack space 'methodFnType1' or 'methodFnType2' uses.
           Zero means unspecified.
        */
    struct xmlrpc_signatureList * signatureListP;
        /* Stuff returned by system method system.methodSignature.
           Empty list doesn't mean there are no valid forms of calling the
           method -- just that the registry declines to state.
        */
    const char * helpText;
        /* Stuff returned by system method system.methodHelp */
} xmlrpc_methodInfo;

typedef struct xmlrpc_methodNode {
    struct xmlrpc_methodNode * nextP;
    const char * methodName;
    xmlrpc_methodInfo * methodP;
} xmlrpc_methodNode;

typedef struct xmlrpc_methodList {
    xmlrpc_methodNode * firstMethodP;
    xmlrpc_methodNode * lastMethodP;
} xmlrpc_methodList;

void
xmlrpc_methodCreate(xmlrpc_env *           const envP,
                    xmlrpc_method1               methodFnType1,
                    xmlrpc_method2               methodFnType2,
                    void *                 const userData,
                    const char *           const signatureString,
                    const char *           const helpText,
                    size_t                 const stackSize,
                    xmlrpc_methodInfo **   const methodPP);

void
xmlrpc_methodDestroy(xmlrpc_methodInfo * const methodP);

void
xmlrpc_methodListCreate(xmlrpc_env *         const envP,
                        xmlrpc_methodList ** const methodListPP);

void
xmlrpc_methodListDestroy(xmlrpc_methodList * methodListP);

void
xmlrpc_methodListLookupByName(xmlrpc_methodList *  const methodListP,
                              const char *         const methodName,
                              xmlrpc_methodInfo ** const methodPP);

void
xmlrpc_methodListAdd(xmlrpc_env *        const envP,
                     xmlrpc_methodList * const methodListP,
                     const char *        const methodName,
                     xmlrpc_methodInfo * const methodP);



#endif
