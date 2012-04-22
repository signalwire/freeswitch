/*=========================================================================
  XML-RPC server method registry
  Method services
===========================================================================
  These are the functions that implement the method objects that
  the XML-RPC method registry uses.

  By Bryan Henderson, December 2006.

  Contributed to the public domain by its author.
=========================================================================*/

#include "xmlrpc_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base.h"
#include "registry.h"

#include "method.h"


static void
signatureDestroy(struct xmlrpc_signature * const signatureP) {

    if (signatureP->argList)
        free((void*)signatureP->argList);

    free(signatureP);
}



static void
translateTypeSpecifierToName(xmlrpc_env *  const envP,
                             char          const typeSpecifier,
                             const char ** const typeNameP) {

    switch (typeSpecifier) {
    case 'i': *typeNameP = "int";              break;
    case 'b': *typeNameP = "boolean";          break;
    case 'd': *typeNameP = "double";           break;
    case 's': *typeNameP = "string";           break;
    case '8': *typeNameP = "dateTime.iso8601"; break;
    case '6': *typeNameP = "base64";           break;
    case 'S': *typeNameP = "struct";           break;
    case 'A': *typeNameP = "array";            break;
    case 'n': *typeNameP = "nil";              break;
    default:
        xmlrpc_faultf(envP, 
                      "Method registry contains invalid signature "
                      "data.  It contains the type specifier '%c'",
                      typeSpecifier);
    }
}
                


#if defined(_MSC_VER)
/* MSVC 8 complains that const char ** is incompatible with void * in the
   REALLOCARRAY.  It's not.
*/
#pragma warning(push)
#pragma warning(disable:4090)
#endif

static void
makeRoomInArgList(xmlrpc_env *              const envP,
                  struct xmlrpc_signature * const signatureP,
                  unsigned int              const minArgCount) {

    if (signatureP->argListSpace < minArgCount) {
        REALLOCARRAY(signatureP->argList, minArgCount);
        if (signatureP->argList == NULL) {
            xmlrpc_faultf(envP, "Couldn't get memory for a argument list for "
                          "a method signature with %u arguments", minArgCount);
            signatureP->argListSpace = 0;
        }
    }
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif



static void
parseArgumentTypeSpecifiers(xmlrpc_env *              const envP,
                            const char *              const startP,
                            struct xmlrpc_signature * const signatureP,
                            const char **             const nextPP) {
    const char * cursorP;

    cursorP = startP;  /* start at the beginning */

    while (!envP->fault_occurred && *cursorP != ',' && *cursorP != '\0') {
        const char * typeName;

        translateTypeSpecifierToName(envP, *cursorP, &typeName);

        if (!envP->fault_occurred) {
            ++cursorP;

            makeRoomInArgList(envP, signatureP, signatureP->argCount + 1);

            signatureP->argList[signatureP->argCount++] = typeName;
        }
    }
    if (!envP->fault_occurred) {
        if (*cursorP) {
            XMLRPC_ASSERT(*cursorP == ',');
            ++cursorP;  /* Move past the signature and comma */
        }
    }
    if (envP->fault_occurred) 
        free((void*)signatureP->argList);

    *nextPP = cursorP;
}



static void
parseOneSignature(xmlrpc_env *               const envP,
                  const char *               const startP,
                  struct xmlrpc_signature ** const signaturePP,
                  const char **              const nextPP) {
/*----------------------------------------------------------------------------
   Parse one signature from the signature string that starts at 'startP'.

   Return that signature as a signature object *signaturePP.

   Return as *nextP the location in the signature string of the next
   signature (i.e. right after the next comma).  If there is no next
   signature (the string ends before any comma), make it point to the
   terminating NUL.
-----------------------------------------------------------------------------*/
    struct xmlrpc_signature * signatureP;

    MALLOCVAR(signatureP);
    if (signatureP == NULL)
        xmlrpc_faultf(envP, "Couldn't get memory for signature");
    else {
        const char * cursorP;

        signatureP->argListSpace = 0;  /* Start with no argument space */
        signatureP->argList = NULL;   /* Nothing allocated yet */
        signatureP->argCount = 0;  /* Start with no arguments */

        cursorP = startP;  /* start at the beginning */

        if (*cursorP == ',' || *cursorP == '\0')
            xmlrpc_faultf(envP, "empty signature (a signature "
                          "must have at least  return value type)");
        else {
            translateTypeSpecifierToName(envP, *cursorP, &signatureP->retType);

            ++cursorP;

            if (*cursorP != ':')
                xmlrpc_faultf(envP, "No colon (':') after "
                              "the result type specifier");
            else {
                ++cursorP;

                parseArgumentTypeSpecifiers(envP, cursorP, signatureP, nextPP);
            }
        }
        if (envP->fault_occurred)
            free(signatureP);
        else
            *signaturePP = signatureP;
    }
}    



static void
destroySignatures(struct xmlrpc_signature * const firstSignatureP) {

    struct xmlrpc_signature * p;
    struct xmlrpc_signature * nextP;

    for (p = firstSignatureP; p; p = nextP) {
        nextP = p->nextP;
        signatureDestroy(p);
    }
}



static void
listSignatures(xmlrpc_env *               const envP,
               const char *               const sigListString,
               struct xmlrpc_signature ** const firstSignaturePP) {
    
    struct xmlrpc_signature ** p;
    const char * cursorP;

    *firstSignaturePP = NULL;  /* Start with empty list */
    
    p = firstSignaturePP;
    cursorP = &sigListString[0];
    
    while (!envP->fault_occurred && *cursorP != '\0') {
        struct xmlrpc_signature * signatureP = NULL;
        
        parseOneSignature(envP, cursorP, &signatureP, &cursorP);
        
        /* cursorP now points at next signature in the list or the
           terminating NUL.
        */
        
        if (!envP->fault_occurred) {
            signatureP->nextP = NULL;
            *p = signatureP;
            p = &signatureP->nextP;
        }
    }
    if (envP->fault_occurred)
        destroySignatures(*firstSignaturePP);
}



static void
signatureListCreate(xmlrpc_env *            const envP,
                    const char *            const sigListString,
                    xmlrpc_signatureList ** const signatureListPP) {

    xmlrpc_signatureList * signatureListP;

    XMLRPC_ASSERT_ENV_OK(envP);
    
    MALLOCVAR(signatureListP);

    if (signatureListP == NULL)
        xmlrpc_faultf(envP, "Could not allocate memory for signature list");
    else {
        signatureListP->firstSignatureP = NULL;

        if (sigListString == NULL || xmlrpc_streq(sigListString, "?")) {
            /* No signatures -- leave the list empty */
        } else {
            listSignatures(envP, sigListString,
                           &signatureListP->firstSignatureP);

            if (!envP->fault_occurred) {
                if (!signatureListP->firstSignatureP)
                    xmlrpc_faultf(envP, "Signature string is empty.");

                if (envP->fault_occurred)
                    destroySignatures(signatureListP->firstSignatureP);
            }
        }
        if (envP->fault_occurred)
            free(signatureListP);

        *signatureListPP = signatureListP;
    }
}



static void
signatureListDestroy(xmlrpc_signatureList * const signatureListP) {

    destroySignatures(signatureListP->firstSignatureP);

    free(signatureListP);
}



static void
makeSignatureList(xmlrpc_env *            const envP,
                  const char *            const signatureString,
                  xmlrpc_signatureList ** const signatureListPP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    signatureListCreate(&env, signatureString, signatureListPP);

    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Can't interpret signature string '%s'.  %s",
                      signatureString, env.fault_string);
}



void
xmlrpc_methodCreate(xmlrpc_env *           const envP,
                    xmlrpc_method1               methodFnType1,
                    xmlrpc_method2               methodFnType2,
                    void *                 const userData,
                    const char *           const signatureString,
                    const char *           const helpText,
                    xmlrpc_methodInfo **   const methodPP) {

    xmlrpc_methodInfo * methodP;

    XMLRPC_ASSERT_ENV_OK(envP);

    MALLOCVAR(methodP);

    if (methodP == NULL)
        xmlrpc_faultf(envP, "Unable to allocate storage for a method "
                      "descriptor");
    else {
        methodP->methodFnType1  = methodFnType1;
        methodP->methodFnType2  = methodFnType2;
        methodP->userData       = userData;
        methodP->helpText       = strdup(helpText);

        makeSignatureList(envP, signatureString, &methodP->signatureListP);

        if (envP->fault_occurred)
            free(methodP);

        *methodPP = methodP;
    }
}



void
xmlrpc_methodDestroy(xmlrpc_methodInfo * const methodP) {
    
    signatureListDestroy(methodP->signatureListP);

    xmlrpc_strfree(methodP->helpText);

    free(methodP);
}



void
xmlrpc_methodListCreate(xmlrpc_env *         const envP,
                        xmlrpc_methodList ** const methodListPP) {

    xmlrpc_methodList * methodListP;

    XMLRPC_ASSERT_ENV_OK(envP);

    MALLOCVAR(methodListP);

    if (methodListP == NULL)
        xmlrpc_faultf(envP, "Couldn't allocate method list descriptor");
    else {
        methodListP->firstMethodP = NULL;
        methodListP->lastMethodP = NULL;

        *methodListPP = methodListP;
    }
}



void
xmlrpc_methodListDestroy(xmlrpc_methodList * methodListP) {

    xmlrpc_methodNode * p;
    xmlrpc_methodNode * nextP;

    for (p = methodListP->firstMethodP; p; p = nextP) {
        nextP = p->nextP;

        xmlrpc_methodDestroy(p->methodP);
        xmlrpc_strfree(p->methodName);
        free(p);
    }

    free(methodListP);
}



void
xmlrpc_methodListLookupByName(xmlrpc_methodList *  const methodListP,
                              const char *         const methodName,
                              xmlrpc_methodInfo ** const methodPP) {


    /* We do a simple linear lookup along a linked list.
       If speed is important, we can make this a binary tree instead.
    */

    xmlrpc_methodNode * p;
    xmlrpc_methodInfo * methodP;

    for (p = methodListP->firstMethodP, methodP = NULL;
         p && !methodP;
         p = p->nextP) {

        if (xmlrpc_streq(p->methodName, methodName))
            methodP = p->methodP;
    }
    *methodPP = methodP;
}



void
xmlrpc_methodListAdd(xmlrpc_env *        const envP,
                     xmlrpc_methodList * const methodListP,
                     const char *        const methodName,
                     xmlrpc_methodInfo * const methodP) {
    
    xmlrpc_methodInfo * existingMethodP;

    XMLRPC_ASSERT_ENV_OK(envP);

    xmlrpc_methodListLookupByName(methodListP, methodName, &existingMethodP);
    
    if (existingMethodP)
        xmlrpc_faultf(envP, "Method named '%s' already registered",
                      methodName);
    else {
        xmlrpc_methodNode * methodNodeP;

        MALLOCVAR(methodNodeP);
        
        if (methodNodeP == NULL)
            xmlrpc_faultf(envP, "Couldn't allocate method node");
        else {
            methodNodeP->methodName = strdup(methodName);
            methodNodeP->methodP = methodP;
            methodNodeP->nextP = NULL;
            
            if (!methodListP->firstMethodP)
                methodListP->firstMethodP = methodNodeP;

            if (methodListP->lastMethodP)
                methodListP->lastMethodP->nextP = methodNodeP;

            methodListP->lastMethodP = methodNodeP;
        }
    }
}

