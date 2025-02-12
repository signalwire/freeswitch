/* Copyright (C) 2001 by Eric Kidd. All rights reserved.
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


#include "xmlrpc_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Windows NT stdout binary mode fix. */
#ifdef _WIN32 
#include <io.h> 
#include <fcntl.h> 
#endif 

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/server_cgi.h"


/*=========================================================================
**  Output Routines
**=========================================================================
**  These routines send various kinds of responses to the server.
*/

static void 
send_xml(const char * const xml_data,
         size_t       const xml_len) {
#ifdef _WIN32 
    _setmode(_fileno(stdout), _O_BINARY); 
#endif 
    /* Send our CGI headers back to the server.
    ** XXX - Coercing 'size_t' to 'unsigned long' might be unsafe under
    ** really weird circumstances. */
    fprintf(stdout, "Status: 200 OK\n");
    /* Handle authentication cookie being sent back. */
    if (getenv("HTTP_COOKIE_AUTH") != NULL)
        fprintf(stdout, "Set-Cookie: auth=%s\n", getenv("HTTP_COOKIE_AUTH"));
    fprintf(stdout, "Content-type: text/xml; charset=\"utf-8\"\n");
    fprintf(stdout, "Content-length: %ld\n\n", (unsigned long) xml_len);

    /* Blast out our data. */
    fwrite(xml_data, sizeof(char), xml_len, stdout);
}



static void
send_error(int          const code,
           const char * const message,
           xmlrpc_env * const env) {

#ifdef _WIN32 
    _setmode(_fileno(stdout), _O_BINARY); 
#endif 
    /* Send an error header. */
    fprintf(stdout, "Status: %d %s\n", code, message);
    fprintf(stdout, "Content-type: text/html\n\n");
    
    /* Send an error message. */
    fprintf(stdout, "<title>%d %s</title>\n", code, message);
    fprintf(stdout, "<h1>%d %s</h1>\n", code, message);
    fprintf(stdout, "<p>An error occurred processing your request.</p>\n");

    /* Print out the XML-RPC fault, if present. */
    if (env && env->fault_occurred)
        fprintf(stdout, "<p>XML-RPC Fault #%d: %s</p>\n",
                env->fault_code, env->fault_string);
}


/*=========================================================================
**  die_if_fault_occurred
**=========================================================================
**  Certain kinds of errors aren't worth the trouble of generating
**  an XML-RPC fault. For these, we just send status 500 to our web server
**  and log the fault to our server log.
*/

static void
die_if_fault_occurred(xmlrpc_env * const env) {
    if (env->fault_occurred) {
        fprintf(stderr, "Unexpected XML-RPC fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        send_error(500, "Internal Server Error", env);
        exit(1);
    }
}


/*=========================================================================
**  Initialization, Cleanup & Method Registry
**=========================================================================
**  These are all related, so we group them together.
*/

static xmlrpc_registry * globalRegistryP;

/*=========================================================================
**  get_body
**=========================================================================
**  Slurp the body of the request into an xmlrpc_mem_block.
*/

static xmlrpc_mem_block *
get_body(xmlrpc_env * const env,
         size_t       const length) {

    xmlrpc_mem_block *result;
    char *contents;
    size_t count;

    XMLRPC_ASSERT_ENV_OK(env);

    /* Error-handling preconditions. */
    result = NULL;

#ifdef _WIN32 
    /* Fix from Jeff Stewart: NT opens stdin and stdout in text mode
       by default, badly confusing our length calculations.  So we need
       to set the file handle to binary. 
    */
    _setmode(_fileno(stdin), _O_BINARY); 
#endif 
    /* XXX - Puke if length is too big. */

    /* Allocate our memory block. */
    result = xmlrpc_mem_block_new(env, length);
    XMLRPC_FAIL_IF_FAULT(env);
    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, result);

    /* Get our data off the network.
    ** XXX - Coercing 'size_t' to 'unsigned long' might be unsafe under
    ** really weird circumstances. */
    count = fread(contents, sizeof(char), length, stdin);
    if (count < length)
        XMLRPC_FAIL2(env, XMLRPC_INTERNAL_ERROR,
                     "Expected %ld bytes, received %ld",
                     (unsigned long) length, (unsigned long) count);

 cleanup:
    if (env->fault_occurred) {
        if (result)
            xmlrpc_mem_block_free(result);
        return NULL;
    }
    return result;
}



void
xmlrpc_server_cgi_process_call(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
  Get the XML-RPC call from Standard Input and environment variables,
  parse it, find the right method, call it, prepare an XML-RPC
  response with the result, and write it to Standard Output.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    char *method, *type, *length_str;
    int length;
    xmlrpc_mem_block *input, *output;
    char *input_data, *output_data;
    size_t input_size, output_size;
    int code;
    char *message;
    char *err = NULL;

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    input = output = NULL;

    /* Set up a default error message. */
    code = 500; message = "Internal Server Error";

    /* Get our HTTP information from the environment. */
    method = getenv("REQUEST_METHOD");
    type = getenv("CONTENT_TYPE");
    length_str = getenv("CONTENT_LENGTH");

    /* Perform some sanity checks. */
    if (!method || !xmlrpc_streq(method, "POST")) {
        code = 405; message = "Method Not Allowed";
        XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Expected HTTP method POST");
    }
    if (!type || !xmlrpc_strneq(type, "text/xml", strlen("text/xml"))) {
	char *template = "Expected content type: \"text/xml\", received: \"%s\"";
    size_t err_len = strlen(template) + (type ? strlen(type) : 0) + 1;

    err = malloc(err_len);

    (void)snprintf(err, err_len, template, (type ? type : ""));
        code = 400; message = "Bad Request";
        XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, err);
    }
    if (!length_str) {
        code = 411; message = "Length Required";
        XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Content-length required");
    }

    /* Get our content length. */
    length = atoi(length_str);
    if (length <= 0) {
        code = 400; message = "Bad Request";
        XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Content-length must be > 0");
    }

    /* SECURITY: Make sure our content length is legal.
    ** XXX - We can cast 'input_len' because we know it's >= 0, yes? */
    if ((size_t) length > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID)) {
        code = 400; message = "Bad Request";
        XMLRPC_FAIL(&env, XMLRPC_LIMIT_EXCEEDED_ERROR,
                    "XML-RPC request too large");
    }

    /* Get our body. */
    input = get_body(&env, length);
    XMLRPC_FAIL_IF_FAULT(&env);
    input_data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, input);
    input_size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, input);

    /* Process our call. */
    xmlrpc_registry_process_call2(&env, registryP,
                                  input_data, input_size, NULL, &output);
    XMLRPC_FAIL_IF_FAULT(&env);
    output_data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output);
    output_size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);

    /* Send our data. */
    send_xml(output_data, output_size);
    
 cleanup:
    if (err)
        free(err);
    if (input)
        xmlrpc_mem_block_free(input);
    if (output)
        xmlrpc_mem_block_free(output);
    
    if (env.fault_occurred)
        send_error(code, message, &env);

    xmlrpc_env_clean(&env);
}



void
xmlrpc_cgi_init(int const flags ATTR_UNUSED) {
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    globalRegistryP = xmlrpc_registry_new(&env);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}



void
xmlrpc_cgi_cleanup(void) {
    xmlrpc_registry_free(globalRegistryP);
}



xmlrpc_registry *
xmlrpc_cgi_registry(void) {
    return globalRegistryP;
}



void
xmlrpc_cgi_add_method(const char *  const method_name,
                      xmlrpc_method const method,
                      void *        const user_data) {
    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method(&env, globalRegistryP, NULL, method_name,
                               method, user_data);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}



void
xmlrpc_cgi_add_method_w_doc(const char *  const method_name,
                            xmlrpc_method const method,
                            void *        const user_data,
                            const char *  const signature,
                            const char *  const help) {
    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method_w_doc(&env, globalRegistryP, NULL, method_name,
                                     method, user_data, signature, help);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}



void
xmlrpc_cgi_process_call(void) {
    
    xmlrpc_server_cgi_process_call(globalRegistryP);
}
