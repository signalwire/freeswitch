/* A simple news-searcher, written in C to demonstrate how to use the
   xmplrpc-c client library.

   This program connects to an XMLRPC server that O'Reilly runs on the
   Internet, gets some information, and displays it on Standard Output.
   
   Note that that server is not in any way designed specifically for xmlrpc-c.
   It simply implements the XMLRPC protocol, and works with any client that
   implements XMLRPC.
   
   The service that the aforementioned server provides is that it gives you
   a list of news articles that match a certain regular expression.  You give
   that regular expression an argument to this client program.

   For more details about O'Reilly's excellent Meerkat news service, see:
   http://www.oreillynet.com/pub/a/rss/2000/11/14/meerkat_xmlrpc.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME        "XML-RPC C Meerkat Query Demo"
#define VERSION     "1.0"
#define MEERKAT_URL "http://www.oreillynet.com/meerkat/xml-rpc/server.php"

struct cmdline {
    const char * searchArg;
    int hours;
};


static void
parseCommandLine(int              const argc, 
                 const char **    const argv,
                 struct cmdline * const cmdlineP) {

    if (argc-1 < 1) {
        fprintf(stderr, "Need at least one argument:  "
                "A mysql regular expression "
                "search pattern.  Try 'query-meerkat Linux'\n");
        exit(1);
    } else {
        cmdlineP->searchArg = argv[1];

        if (argc-1 < 2) {
            cmdlineP->hours = 24;
        } else {
            cmdlineP->hours = atoi(argv[2]);
            if (cmdlineP->hours > 49) {
                fprintf(stderr, "It's not nice to ask for > 49 hours "
                        "at once.\n");
                exit(1);    
            }
            if (argc-1 > 2) {
                fprintf(stderr, "There are at most 2 arguments: "
                        "search pattern "
                        "and number of hours.");
                exit(1);
            }
        }
    }
}



static void 
die_if_fault_occurred(xmlrpc_env * const env) {
    /* We're a command-line utility, so we abort if an error occurs. */
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault #%d: %s\n",
                env->fault_code, env->fault_string);
        exit(1);
    }
}



/* Hey! We fit in one function. */
int 
main(int          const argc, 
     const char** const argv) {

    struct cmdline cmdline;
    char time_period[16];
    xmlrpc_env env;
    xmlrpc_value *stories, *story;
    size_t size, i;
    int first;

    parseCommandLine(argc, argv, &cmdline);

    snprintf(time_period, sizeof(time_period), "%dHOUR", cmdline.hours);

    xmlrpc_env_init(&env);

    /* Set up our client. */
    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);

    die_if_fault_occurred(&env);

    /* Ask Meerkat to look for matching stories. */
    stories = xmlrpc_client_call(&env, MEERKAT_URL,
                                 "meerkat.getItems", "({s:s,s:i,s:s})",
                                 "search", cmdline.searchArg,
                                 "descriptions", (xmlrpc_int32) 76,
                                 "time_period", time_period);
    die_if_fault_occurred(&env);
    
    /* Loop over the stories. */
    size = xmlrpc_array_size(&env, stories);
    die_if_fault_occurred(&env);
    first = 1;
    for (i = 0; i < size; i++) {
        const char * title;
        const char * link;
        const char * description;

        /* Extract the useful information from our story. */
        story = xmlrpc_array_get_item(&env, stories, i);
        die_if_fault_occurred(&env);
        xmlrpc_decompose_value(&env, story, "{s:s,s:s,s:s,*}",
                               "title", &title,
                               "link", &link,
                               "description", &description);
        die_if_fault_occurred(&env);

        /* Print a separator line if necessary. */
        if (first)
            first = 0;
        else
            printf("\n");

        /* Print the story. */
        if (strlen(description) > 0) {
            printf("%s\n%s\n%s\n", title, description, link);
        } else {
            printf("%s\n%s\n", title, link);
        }
        free((char*)title);
        free((char*)link);
        free((char*)description);
    }
    
    /* Shut down our client. */
    xmlrpc_DECREF(stories);
    xmlrpc_env_clean(&env);
    xmlrpc_client_cleanup();

    return 0;
}
