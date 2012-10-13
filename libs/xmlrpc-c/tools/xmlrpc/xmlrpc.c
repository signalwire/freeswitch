/* Make an XML-RPC call.

   User specifies details of the call on the command line.

   We print the result on Standard Output.

   Example:

     $ xmlrpc http://localhost:8080/RPC2 sample.add i/3 i/5
     Result:
       Integer: 8

     $ xmlrpc localhost:8080 sample.add i/3 i/5
     Result:
       Integer: 8

   This is just the beginnings of this program.  It should be extended
   to deal with all types of parameters and results.

   An example of a good syntax for parameters would be:

     $ xmlrpc http://www.oreillynet.com/meerkat/xml-rpc/server.php \
         meerkat.getItems \
         struct/{search:linux,descriptions:i/76,time_period:12hour}
     Result:
       Array:
         Struct:
           title: String: DatabaseJournal: OpenEdge-Based Finance ...
           link: String: http://linuxtoday.com/news_story.php3?ltsn=...
           description: String: "Finance application with embedded ...
         Struct:
           title: ...
           link: ...
           description: ...

*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "xmlrpc_config.h"  /* information about this build environment */
#include "bool.h"
#include "int.h"
#include "mallocvar.h"
#include "girstring.h"
#include "casprintf.h"
#include "string_parser.h"
#include "cmdline_parser.h"
#include "dumpvalue.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/string_int.h"

#define NAME "xmlrpc command line program"
#define VERSION "1.0"

struct cmdlineInfo {
    const char *  url;
    const char *  username;
    const char *  password;
    const char *  methodName;
    unsigned int  paramCount;
    const char ** params;
        /* Array of parameters, in order.  Has 'paramCount' entries. */
    const char *  transport;
        /* Name of XML transport he wants to use.  NULL if he has no 
           preference.
        */
    const char *  curlinterface;
        /* "network interface" parameter for the Curl transport.  (Not
           valid if 'transport' names a non-Curl transport).
        */
    xmlrpc_bool   curlnoverifypeer;
    xmlrpc_bool   curlnoverifyhost;
    const char *  curluseragent;
};



static void 
die_if_fault_occurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "Failed.  %s\n", envP->fault_string);
        exit(1);
    }
}



static void GNU_PRINTF_ATTR(2,3)
setError(xmlrpc_env * const envP, const char format[], ...) {
    va_list args;
    const char * faultString;

    va_start(args, format);

    cvasprintf(&faultString, format, args);
    va_end(args);

    xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR, faultString);

    strfree(faultString);
}
      


static void
processArguments(xmlrpc_env *         const envP,
                 cmdlineParser        const cp,
                 struct cmdlineInfo * const cmdlineP) {

    if (cmd_argumentCount(cp) < 2)
        setError(envP, "Not enough arguments.  Need at least a URL and "
                 "method name.");
    else {
        unsigned int i;
        
        cmdlineP->url        = cmd_getArgument(cp, 0);
        cmdlineP->methodName = cmd_getArgument(cp, 1);
        cmdlineP->paramCount = cmd_argumentCount(cp) - 2;
        MALLOCARRAY(cmdlineP->params, cmdlineP->paramCount);
        for (i = 0; i < cmdlineP->paramCount; ++i)
            cmdlineP->params[i] = cmd_getArgument(cp, i+2);
    }
}



static void
chooseTransport(xmlrpc_env *  const envP ATTR_UNUSED,
                cmdlineParser const cp,
                const char ** const transportPP) {
    
    const char * transportOpt = cmd_getOptionValueString(cp, "transport");

    if (transportOpt) {
        *transportPP = transportOpt;
    } else {
        if (cmd_optionIsPresent(cp, "curlinterface") || 
            cmd_optionIsPresent(cp, "curlnoverifypeer") ||
            cmd_optionIsPresent(cp, "curlnoverifyhost") ||
            cmd_optionIsPresent(cp, "curluseragent"))

            *transportPP = strdup("curl");
        else
            *transportPP = NULL;
    }
}



static void
parseCommandLine(xmlrpc_env *         const envP,
                 int                  const argc,
                 const char **        const argv,
                 struct cmdlineInfo * const cmdlineP) {

    cmdlineParser const cp = cmd_createOptionParser();

    const char * error;

    cmd_defineOption(cp, "transport",        OPTTYPE_STRING);
    cmd_defineOption(cp, "username",         OPTTYPE_STRING);
    cmd_defineOption(cp, "password",         OPTTYPE_STRING);
    cmd_defineOption(cp, "curlinterface",    OPTTYPE_STRING);
    cmd_defineOption(cp, "curlnoverifypeer", OPTTYPE_STRING);
    cmd_defineOption(cp, "curlnoverifyhost", OPTTYPE_STRING);
    cmd_defineOption(cp, "curluseragent",    OPTTYPE_STRING);

    cmd_processOptions(cp, argc, argv, &error);

    if (error) {
        setError(envP, "Command syntax error.  %s", error);
        strfree(error);
    } else {
        cmdlineP->username  = cmd_getOptionValueString(cp, "username");
        cmdlineP->password  = cmd_getOptionValueString(cp, "password");

        if (cmdlineP->username && !cmdlineP->password)
            setError(envP, "When you specify -username, you must also "
                     "specify -password.");
        else {
            chooseTransport(envP, cp, &cmdlineP->transport);

            cmdlineP->curlinterface = 
                cmd_getOptionValueString(cp, "curlinterface");
            cmdlineP->curlnoverifypeer =
                cmd_optionIsPresent(cp, "curlnoverifypeer");
            cmdlineP->curlnoverifyhost =
                cmd_optionIsPresent(cp, "curlnoverifyhost");
            cmdlineP->curluseragent =
                cmd_getOptionValueString(cp, "curluseragent");

            if ((!cmdlineP->transport || 
                 !streq(cmdlineP->transport, "curl"))
                &&
                (cmdlineP->curlinterface ||
                 cmdlineP->curlnoverifypeer ||
                 cmdlineP->curlnoverifyhost ||
                 cmdlineP->curluseragent))
                setError(envP, "You may not specify a Curl transport "
                         "option unless you also specify -transport=curl");

            processArguments(envP, cp, cmdlineP);
        }
    }
    cmd_destroyOptionParser(cp);
}



static void
freeCmdline(struct cmdlineInfo const cmdline) {

    unsigned int i;
    
    strfree(cmdline.url);
    strfree(cmdline.methodName);
    if (cmdline.transport)
        strfree(cmdline.transport);
    if (cmdline.curlinterface)
        strfree(cmdline.curlinterface);
    if (cmdline.curluseragent)
        strfree(cmdline.curluseragent);
    if (cmdline.username)
        strfree(cmdline.username);
    if (cmdline.password)
        strfree(cmdline.password);
    for (i = 0; i < cmdline.paramCount; ++i)
        strfree(cmdline.params[i]);
}



static void
computeUrl(const char *  const urlArg,
           const char ** const urlP) {

    if (strstr(urlArg, "://") != 0)
        casprintf(urlP, "%s", urlArg);
    else
        casprintf(urlP, "http://%s/RPC2", urlArg);
}



static void
buildString(xmlrpc_env *    const envP,
            const char *    const valueString,
            xmlrpc_value ** const paramPP) {

    *paramPP = xmlrpc_string_new(envP, valueString);
}



static void
interpretHex(xmlrpc_env *    const envP,
             const char *    const valueString,
             size_t          const valueStringSize,
             unsigned char * const byteString) {

    size_t bsCursor;
    size_t strCursor;

    for (strCursor = 0, bsCursor = 0;
         strCursor < valueStringSize && !envP->fault_occurred;
        ) {
        int rc;

        rc = sscanf(&valueString[strCursor], "%2hhx",
                    &byteString[bsCursor++]);

        if (rc != 1)
            xmlrpc_faultf(envP, "Invalid hex data '%s'",
                          &valueString[strCursor]);
        else
            strCursor += 2;
    }
}



static void
buildBytestring(xmlrpc_env *    const envP,
                const char *    const valueString,
                xmlrpc_value ** const paramPP) {

    size_t const valueStringSize = strlen(valueString);

    if (valueStringSize / 2 * 2 != valueStringSize)
        xmlrpc_faultf(envP, "Hexadecimal text is not an even "
                      "number of characters (it is %u characters)",
                      (unsigned)strlen(valueString));
    else {
        size_t const byteStringSize = strlen(valueString)/2;
        
        unsigned char * byteString;

        MALLOCARRAY(byteString, byteStringSize);

        if (byteString == NULL)
            xmlrpc_faultf(envP, "Failed to allocate %u-byte buffer",
                          (unsigned)byteStringSize);
        else {
            interpretHex(envP, valueString, valueStringSize, byteString);

            if (!envP->fault_occurred)
                *paramPP = xmlrpc_base64_new(envP, byteStringSize, byteString);

            free(byteString);
        }
    }
}



static void
buildInt(xmlrpc_env *    const envP,
         const char *    const valueString,
         xmlrpc_value ** const paramPP) {

    if (strlen(valueString) < 1)
        setError(envP, "Integer argument has nothing after the 'i/'");
    else {
        int value;
        const char * error;

        interpretInt(valueString, &value, &error);

        if (error) {
            setError(envP, "'%s' is not a valid 32-bit integer.  %s",
                     valueString, error);
            strfree(error);
        } else
            *paramPP = xmlrpc_int_new(envP, value);
    }
}



static void
buildBool(xmlrpc_env *    const envP,
          const char *    const valueString,
          xmlrpc_value ** const paramPP) {

    if (streq(valueString, "t") || streq(valueString, "true"))
        *paramPP = xmlrpc_bool_new(envP, true);
    else if (streq(valueString, "f") == 0 || streq(valueString, "false"))
        *paramPP = xmlrpc_bool_new(envP, false);
    else
        setError(envP, "Boolean argument has unrecognized value '%s'.  "
                 "recognized values are 't', 'f', 'true', and 'false'.",
                 valueString);
} 



static void
buildDouble(xmlrpc_env *    const envP,
            const char *    const valueString,
            xmlrpc_value ** const paramPP) {

    if (strlen(valueString) < 1)
        setError(envP, "\"Double\" argument has nothing after the 'd/'");
    else {
        double value;
        char * tailptr;

        value = strtod(valueString, &tailptr);

        if (*tailptr != '\0')
            setError(envP, 
                     "\"Double\" argument has non-decimal crap in it: '%s'",
                     tailptr);
        else
            *paramPP = xmlrpc_double_new(envP, value);
    }
}



static void
buildNil(xmlrpc_env *    const envP,
         const char *    const valueString,
         xmlrpc_value ** const paramPP) {

    if (strlen(valueString) > 0)
        setError(envP, "Nil argument has something after the 'n/'");
    else {
        *paramPP = xmlrpc_nil_new(envP);
    }
}



static void
buildI8(xmlrpc_env *    const envP,
        const char *    const valueString,
        xmlrpc_value ** const paramPP) {

    if (strlen(valueString) < 1)
        setError(envP, "Integer argument has nothing after the 'I/'");
    else {
        int64_t value;
        const char * error;

        interpretLl(valueString, &value, &error);

        if (error) {
            setError(envP, "'%s' is not a valid 64-bit integer.  %s",
                     valueString, error);
            strfree(error);
        } else
            *paramPP = xmlrpc_i8_new(envP, value);
    }
}



static void
computeParameter(xmlrpc_env *    const envP,
                 const char *    const paramArg,
                 xmlrpc_value ** const paramPP) {

    if (xmlrpc_strneq(paramArg, "s/", 2))
        buildString(envP, &paramArg[2], paramPP);
    else if (xmlrpc_strneq(paramArg, "h/", 2))
        buildBytestring(envP, &paramArg[2], paramPP);
    else if (xmlrpc_strneq(paramArg, "i/", 2)) 
        buildInt(envP, &paramArg[2], paramPP);
    else if (xmlrpc_strneq(paramArg, "I/", 2)) 
        buildI8(envP, &paramArg[2], paramPP);
    else if (xmlrpc_strneq(paramArg, "d/", 2)) 
        buildDouble(envP, &paramArg[2], paramPP);
    else if (xmlrpc_strneq(paramArg, "b/", 2))
        buildBool(envP, &paramArg[2], paramPP);
    else if (xmlrpc_strneq(paramArg, "n/", 2))
        buildNil(envP, &paramArg[2], paramPP);
    else {
        /* It's not in normal type/value format, so we take it to be
           the shortcut string notation 
        */
        buildString(envP, paramArg, paramPP);
    }
}



static void
computeParamArray(xmlrpc_env *    const envP,
                  unsigned int    const paramCount,
                  const char **   const params,
                  xmlrpc_value ** const paramArrayPP) {
    
    unsigned int i;

    xmlrpc_value * paramArrayP;

    paramArrayP = xmlrpc_array_new(envP);

    for (i = 0; i < paramCount && !envP->fault_occurred; ++i) {
        xmlrpc_value * paramP;

        computeParameter(envP, params[i], &paramP);

        if (!envP->fault_occurred) {
            xmlrpc_array_append_item(envP, paramArrayP, paramP);

            xmlrpc_DECREF(paramP);
        }
    }
    *paramArrayPP = paramArrayP;
}



static void
dumpResult(xmlrpc_value * const resultP) {

    printf("Result:\n\n");

    dumpValue("", resultP);
}



static void
callWithClient(xmlrpc_env *               const envP,
               const xmlrpc_server_info * const serverInfoP,
               const char *               const methodName,
               xmlrpc_value *             const paramArrayP,
               xmlrpc_value **            const resultPP) {
               
    xmlrpc_env env;
    xmlrpc_env_init(&env);
    *resultPP = xmlrpc_client_call_server_params(
        &env, serverInfoP, methodName, paramArrayP);
    
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Call failed.  %s.  (XML-RPC fault code %d)",
                      env.fault_string, env.fault_code);
    xmlrpc_env_clean(&env);
}



static void
doCall(xmlrpc_env *               const envP,
       const char *               const transport,
       const char *               const curlinterface,
       xmlrpc_bool                const curlnoverifypeer,
       xmlrpc_bool                const curlnoverifyhost,
       const char *               const curluseragent,
       const xmlrpc_server_info * const serverInfoP,
       const char *               const methodName,
       xmlrpc_value *             const paramArrayP,
       xmlrpc_value **            const resultPP) {
    
    struct xmlrpc_clientparms clientparms;

    XMLRPC_ASSERT(xmlrpc_value_type(paramArrayP) == XMLRPC_TYPE_ARRAY);

    clientparms.transport = transport;

    if (transport && streq(transport, "curl")) {
        struct xmlrpc_curl_xportparms * curlXportParmsP;
        MALLOCVAR(curlXportParmsP);

        curlXportParmsP->network_interface = curlinterface;
        curlXportParmsP->no_ssl_verifypeer = curlnoverifypeer;
        curlXportParmsP->no_ssl_verifyhost = curlnoverifyhost;
        curlXportParmsP->user_agent        = curluseragent;
        
        clientparms.transportparmsP    = curlXportParmsP;
        clientparms.transportparm_size = XMLRPC_CXPSIZE(user_agent);
    } else {
        clientparms.transportparmsP = NULL;
        clientparms.transportparm_size = 0;
    }
    xmlrpc_client_init2(envP, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, 
                        &clientparms, XMLRPC_CPSIZE(transportparm_size));
    if (!envP->fault_occurred) {
        callWithClient(envP, serverInfoP, methodName, paramArrayP, resultPP);

        xmlrpc_client_cleanup();
    }
    if (clientparms.transportparmsP)
        free((void*)clientparms.transportparmsP);
}



static void
createServerInfo(xmlrpc_env *          const envP,
                 const char *          const serverUrl,
                 const char *          const userName,
                 const char *          const password,
                 xmlrpc_server_info ** const serverInfoPP) {

    xmlrpc_server_info * serverInfoP;

    serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
    if (!envP->fault_occurred) {
        if (userName) {
            xmlrpc_server_info_set_basic_auth(
                envP, serverInfoP, userName, password);
        }
    }
    *serverInfoPP = serverInfoP;
}



int 
main(int           const argc, 
     const char ** const argv) {

    struct cmdlineInfo cmdline;
    xmlrpc_env env;
    xmlrpc_value * paramArrayP;
    xmlrpc_value * resultP;
    const char * url;
    xmlrpc_server_info * serverInfoP;

    xmlrpc_env_init(&env);

    parseCommandLine(&env, argc, argv, &cmdline);
    die_if_fault_occurred(&env);

    computeUrl(cmdline.url, &url);

    computeParamArray(&env, cmdline.paramCount, cmdline.params, &paramArrayP);
    die_if_fault_occurred(&env);

    createServerInfo(&env, url, cmdline.username, cmdline.password,
                     &serverInfoP);
    die_if_fault_occurred(&env);

    doCall(&env, cmdline.transport, cmdline.curlinterface,
           cmdline.curlnoverifypeer, cmdline.curlnoverifyhost,
           cmdline.curluseragent,
           serverInfoP,
           cmdline.methodName, paramArrayP, 
           &resultP);
    die_if_fault_occurred(&env);

    dumpResult(resultP);
    
    strfree(url);

    xmlrpc_DECREF(resultP);

    freeCmdline(cmdline);

    xmlrpc_env_clean(&env);
    
    return 0;
}
