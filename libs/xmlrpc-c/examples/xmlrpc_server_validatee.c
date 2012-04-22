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


/*============================================================================
                              xmlrpc_server_validatee
==============================================================================

  This program runs an XMLRPC server, using the Xmlrpc-c libraries.

  The server implements the methods that the Userland Validator1 test suite
  invokes, which are supposed to exercise a broad range of XMLRPC server
  function.

  Coments here used to say you could get information about Validator1
  from <http://validator.xmlrpc.com/>, but as of 2004.09.25, there's nothing
  there (there's a web server, but it is not configured to serve that
  particular URL).

============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */

#define RETURN_IF_FAULT(envP) \
    do { \
        if ((envP)->fault_occurred) \
            return NULL; \
    } while (0)
        

/*=========================================================================
**  validator1.arrayOfStructsTest
**=========================================================================
*/

static xmlrpc_value *
array_of_structs(xmlrpc_env *   const envP, 
                 xmlrpc_value * const paramArrayP, 
                 void *         const user_data ATTR_UNUSED) {

    xmlrpc_value * arrayP;
    xmlrpc_value * retval;

    xmlrpc_decompose_value(envP, paramArrayP, "(A)", &arrayP);
    if (envP->fault_occurred)
        retval = NULL;
    else {
        /* Add up all the struct elements named "curly". */
        size_t size;
        size = xmlrpc_array_size(envP, arrayP);
        if (envP->fault_occurred)
            retval = NULL;
        else {
            unsigned int sum;
            unsigned int i;
            sum = 0;
            for (i = 0; i < size && !envP->fault_occurred; ++i) {
                xmlrpc_value * strctP;
                strctP = xmlrpc_array_get_item(envP, arrayP, i);
                if (!envP->fault_occurred) {
                    xmlrpc_int32 curly;
                    xmlrpc_decompose_value(envP, strctP, "{s:i,*}", 
                                           "curly", &curly);
                    if (!envP->fault_occurred)
                        sum += curly;
                }
            }
            xmlrpc_DECREF(arrayP);
            if (envP->fault_occurred)
                retval = NULL;
            else
                retval = xmlrpc_build_value(envP, "i", sum);
        }
    }
    return retval;
}


/*=========================================================================
**  validator1.countTheEntities
**=========================================================================
*/

static xmlrpc_value *
count_entities(xmlrpc_env *   const envP,
               xmlrpc_value * const paramArrayP, 
               void *         const user_data ATTR_UNUSED) {

    const char * str;
    size_t len, i;
    xmlrpc_int32 left, right, amp, apos, quote;

    xmlrpc_decompose_value(envP, paramArrayP, "(s#)", &str, &len);
    RETURN_IF_FAULT(envP);

    left = right = amp = apos = quote = 0;
    for (i = 0; i < len; ++i) {
        switch (str[i]) {
        case '<':  ++left;  break;
        case '>':  ++right; break;
        case '&':  ++amp;   break;
        case '\'': ++apos;  break;
        case '\"': ++quote; break;
        default: break;
        }
    }
    free((void*)str);

    return xmlrpc_build_value(envP, "{s:i,s:i,s:i,s:i,s:i}",
                              "ctLeftAngleBrackets", left,
                              "ctRightAngleBrackets", right,
                              "ctAmpersands", amp,
                              "ctApostrophes", apos,
                              "ctQuotes", quote);
}



/*=========================================================================
**  validator1.easyStructTest
**=========================================================================
*/

static xmlrpc_value *
easy_struct(xmlrpc_env *   const envP,
            xmlrpc_value * const paramArrayP,
            void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 larry, moe, curly;

    /* Parse our argument array and get the stooges. */
    xmlrpc_decompose_value(envP, paramArrayP, "({s:i,s:i,s:i,*})",
                           "larry", &larry,
                           "moe", &moe,
                           "curly", &curly);
    RETURN_IF_FAULT(envP);

    /* Return our result. */
    return xmlrpc_build_value(envP, "i", larry + moe + curly);
}



/*=========================================================================
**  validator1.echoStructTest
**=========================================================================
*/

static xmlrpc_value *
echo_struct(xmlrpc_env *   const envP,
            xmlrpc_value * const paramArrayP, 
            void *         const user_data ATTR_UNUSED) {

    xmlrpc_value * sP;

    /* Parse our argument array. */
    xmlrpc_decompose_value(envP, paramArrayP, "(S)", &sP);
    RETURN_IF_FAULT(envP);
    
    return sP;  /* We transfer our reference on '*sP' to Caller */
}



/*=========================================================================
**  validator1.manyTypesTest
**=========================================================================
*/

static xmlrpc_value *
many_types(xmlrpc_env *   const env ATTR_UNUSED, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

    /* Create another reference to our argument array and return it as is. */
    xmlrpc_INCREF(param_array);
    return param_array;
}



/*=========================================================================
**  validator1.moderateSizeArrayCheck
**=========================================================================
*/

static void
concatenate(xmlrpc_env *    const envP,
            const char *    const str1,
            size_t          const str1_len,
            const char *    const str2,
            size_t          const str2_len,
            xmlrpc_value ** const resultPP) {

    /* Concatenate the two strings. */

    char * buffer;

    buffer = (char*) malloc(str1_len + str2_len);
    if (!buffer) {
        xmlrpc_env_set_fault(envP, 1, 
                             "Couldn't allocate concatenated string");
    } else {
        memcpy(buffer, str1, str1_len);
        memcpy(&buffer[str1_len], str2, str2_len);
        *resultPP = xmlrpc_build_value(envP, "s#", 
                                       buffer, str1_len + str2_len);
        free(buffer);
    }
}



static xmlrpc_value *
moderate_array(xmlrpc_env *   const envP,
               xmlrpc_value * const paramArrayP,
               void *         const user_data ATTR_UNUSED) {

    xmlrpc_value * retval;
    xmlrpc_value * arrayP;

    /* Parse our argument array. */
    xmlrpc_decompose_value(envP, paramArrayP, "(A)", &arrayP);
    if (!envP->fault_occurred) {
        int const size = xmlrpc_array_size(envP, arrayP);
        if (!envP->fault_occurred) {
            /* Get our first string. */
            xmlrpc_value * const firstItemP =
                xmlrpc_array_get_item(envP, arrayP, 0);
            if (!envP->fault_occurred) {
                const char * str1;
                size_t str1_len;
                xmlrpc_read_string_lp(envP, firstItemP, &str1_len, &str1);
                if (!envP->fault_occurred) {
                    /* Get our last string. */
                    xmlrpc_value * const lastItemP =
                        xmlrpc_array_get_item(envP, arrayP, size - 1);
                    if (!envP->fault_occurred) {
                        const char * str2;
                        size_t str2_len;
                        xmlrpc_read_string_lp(envP, lastItemP,
                                              &str2_len, &str2);
                        if (!envP->fault_occurred) {
                            concatenate(envP, str1, str1_len, str2, str2_len,
                                        &retval);
                            free((char*)str2);
                        }
                    }
                    free((char*)str1);
                }
            }
        }
        xmlrpc_DECREF(arrayP);
    }
    return retval;
}



/*=========================================================================
**  validator1.nestedStructTest
**=========================================================================
*/

static xmlrpc_value *
nested_struct(xmlrpc_env *   const envP,
              xmlrpc_value * const paramArrayP,
              void *         const user_data ATTR_UNUSED) {

    xmlrpc_value * yearsP;
    xmlrpc_value * retval;

    /* Parse our argument array. */
    xmlrpc_decompose_value(envP, paramArrayP, "(S)", &yearsP);
    if (envP->fault_occurred)
        retval = NULL;
    else {
        /* Get values of larry, moe and curly for 2000-04-01. */
        xmlrpc_int32 larry, moe, curly;
        xmlrpc_decompose_value(envP, yearsP,
                               "{s:{s:{s:{s:i,s:i,s:i,*},*},*},*}",
                               "2000", "04", "01",
                               "larry", &larry,
                               "moe", &moe,
                               "curly", &curly);               
        if (envP->fault_occurred)
            retval = NULL;
        else
            retval = xmlrpc_build_value(envP, "i", larry + moe + curly);

        xmlrpc_DECREF(yearsP);
    }
    return retval;
}



/*=========================================================================
**  validator1.simpleStructReturnTest
**=========================================================================
*/

static xmlrpc_value *
struct_return(xmlrpc_env *   const envP,
              xmlrpc_value * const paramArrayP,
              void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 i;

    xmlrpc_decompose_value(envP, paramArrayP, "(i)", &i);
    RETURN_IF_FAULT(envP);

    return xmlrpc_build_value(envP, "{s:i,s:i,s:i}",
                              "times10", (xmlrpc_int32) i * 10,
                              "times100", (xmlrpc_int32) i * 100,
                              "times1000", (xmlrpc_int32) i * 1000);
}



/*=========================================================================
**  main
**=========================================================================
*/

int main(int           const argc, 
         const char ** const argv) {

    xmlrpc_server_abyss_parms serverparm;
    xmlrpc_registry * registryP;
    xmlrpc_env env;

    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The TCP port "
                "number on which the server will accept connections "
                "for RPCs.  You specified %d arguments.\n",  argc-1);
        exit(1);
    }

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.arrayOfStructsTest", 
        &array_of_structs, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.countTheEntities", 
        &count_entities, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.easyStructTest", 
        &easy_struct, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.echoStructTest", 
        &echo_struct, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.manyTypesTest", 
        &many_types, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.moderateSizeArrayCheck", 
        &moderate_array, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.nestedStructTest", 
        &nested_struct, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "validator1.simpleStructReturnTest", 
        &struct_return, NULL);

    serverparm.config_file_name = NULL;
    serverparm.registryP = registryP;
    serverparm.port_number = atoi(argv[1]);
    serverparm.log_file_name = NULL;

    printf("Running XML-RPC server...\n");

    xmlrpc_server_abyss(&env, &serverparm, XMLRPC_APSIZE(log_file_name));

    /* This never gets executed. */
    return 0;
}
