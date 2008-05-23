/* Transport some XML to a server and get the response back, as if doing
   an XML-RPC call.
*/

//#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmlrpc_config.h"  /* information about this build environment */
#include "casprintf.h"
#include "mallocvar.h"
#include "cmdline_parser.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"

#define NAME "xmlrpc_transport command line program"
#define VERSION "1.0"

struct cmdlineInfo {
    const char *  url;
    const char *  username;
    const char *  password;
    const char *  transport;
        /* Name of XML transport he wants to use.  NULL if he has no 
           preference.
        */
};



static void 
die_if_fault_occurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "Error: %s (%d)\n",
                envP->fault_string, envP->fault_code);
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

    if (cmd_argumentCount(cp) < 1)
        setError(envP, "Not enough arguments.  Need a URL.");
    else {
        cmdlineP->url = cmd_getArgument(cp, 0);
    }
}



static void
parseCommandLine(xmlrpc_env *         const envP,
                 int                  const argc,
                 const char **        const argv,
                 struct cmdlineInfo * const cmdlineP) {

    cmdlineParser const cp = cmd_createOptionParser();

    const char * error;

    cmd_defineOption(cp, "transport", OPTTYPE_STRING);
    cmd_defineOption(cp, "username",  OPTTYPE_STRING);
    cmd_defineOption(cp, "password",  OPTTYPE_STRING);

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
            cmdlineP->transport = cmd_getOptionValueString(cp, "transport");
            
            processArguments(envP, cp, cmdlineP);
        }
    }
    cmd_destroyOptionParser(cp);
}



static void
freeCmdline(struct cmdlineInfo const cmdline) {

    strfree(cmdline.url);
    if (cmdline.username)
        strfree(cmdline.username);
    if (cmdline.password)
        strfree(cmdline.password);
    if (cmdline.transport)
        strfree(cmdline.transport);
}



static void
computeUrl(const char *  const urlArg,
           const char ** const urlP) {

    if (strstr(urlArg, "://") != 0) {
        *urlP = strdup(urlArg);
    } else {
        casprintf(urlP, "http://%s/RPC2", urlArg);
    }        
}



static void
doCall(xmlrpc_env *               const envP,
       const char *               const transport,
       const xmlrpc_server_info * const serverInfoP,
       xmlrpc_mem_block *         const callXmlP,
       xmlrpc_mem_block **        const respXmlPP) {
    
    struct xmlrpc_clientparms clientparms;

    clientparms.transport = transport;

    clientparms.transportparmsP = NULL;
    clientparms.transportparm_size = 0;

    xmlrpc_client_init2(envP, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, 
                        &clientparms, XMLRPC_CPSIZE(transportparm_size));
    if (!envP->fault_occurred) {
        xmlrpc_client_transport_call(envP, NULL, serverInfoP,
                                     callXmlP, respXmlPP);
        
        xmlrpc_client_cleanup();
    }
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



static void
readFile(xmlrpc_env *        const envP,
         FILE *              const ifP,
         xmlrpc_mem_block ** const fileContentsPP) {

    xmlrpc_mem_block * fileContentsP;

    fileContentsP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);

    while (!envP->fault_occurred && !feof(ifP)) {
        char buffer[4096];
        size_t bytesRead;
        
        bytesRead = fread(buffer, 1, sizeof(buffer), ifP);
        XMLRPC_MEMBLOCK_APPEND(char, envP, 
                               fileContentsP, buffer, bytesRead);
    }
    if (envP->fault_occurred)
        XMLRPC_MEMBLOCK_FREE(char, fileContentsP);

    *fileContentsPP = fileContentsP;
}



static void
writeFile(xmlrpc_env *       const envP,
          FILE *             const ofP,
          xmlrpc_mem_block * const fileContentsP) {

    size_t totalWritten;

    totalWritten = 0;

    while (!envP->fault_occurred &&
           totalWritten < XMLRPC_MEMBLOCK_SIZE(char, fileContentsP)) {
        size_t bytesWritten;

        bytesWritten = fwrite(
            XMLRPC_MEMBLOCK_CONTENTS(char, fileContentsP) + totalWritten,
            1,
            XMLRPC_MEMBLOCK_SIZE(char, fileContentsP) - totalWritten,
            ofP);

        if (bytesWritten < 1)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR,
                "Error writing output");

        totalWritten -= bytesWritten;
    }
}



int 
main(int           const argc, 
     const char ** const argv) {

    struct cmdlineInfo cmdline;
    xmlrpc_env env;
    xmlrpc_mem_block * callXmlP;
    xmlrpc_mem_block * respXmlP;
    const char * url;
    xmlrpc_server_info * serverInfoP;

    xmlrpc_env_init(&env);

    parseCommandLine(&env, argc, argv, &cmdline);
    die_if_fault_occurred(&env);

    computeUrl(cmdline.url, &url);

    createServerInfo(&env, url, cmdline.username, cmdline.password,
                     &serverInfoP);
    die_if_fault_occurred(&env);

    fprintf(stderr, "Reading call data from Standard Input...\n");

    readFile(&env, stdin, &callXmlP);
    die_if_fault_occurred(&env);

    fprintf(stderr, "Making call...\n");

    doCall(&env, cmdline.transport, serverInfoP, callXmlP,
           &respXmlP);
    die_if_fault_occurred(&env);

    fprintf(stderr, "Writing response data to Standard Output\n");
    writeFile(&env, stdout, respXmlP);
    die_if_fault_occurred(&env);

    XMLRPC_MEMBLOCK_FREE(char, callXmlP);
    XMLRPC_MEMBLOCK_FREE(char, respXmlP);
    
    strfree(url);

    freeCmdline(cmdline);

    xmlrpc_env_clean(&env);
    
    return 0;
}
