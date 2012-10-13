#include <stdlib.h>
#include <stdio.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/util_int.h"

#include "testtool.h"
     /* Defines global variable, 'total_tests', 'total_failures' */


/* This is a good place to set a breakpoint. */
void
test_failure(const char * const file,
             unsigned int const line,
             const char * const label,
             const char * const statement) {

    ++total_failures;
    printf("\n%s:%u: test failure: %s (%s)\n", file, line, label, statement);

    /* We abort rather than exit so one can tell from a dump or debug session
       who called us.
    */
    abort();
}



void
test_fault(xmlrpc_env * const envP,
           int          const expectedCode,
           const char * const fileName,
           unsigned int const lineNumber) {

    ++total_tests;

    if (!envP->fault_occurred)
        test_failure(fileName, lineNumber, "no fault occurred", "");
    else if (envP->fault_code != expectedCode)
        test_failure(fileName, lineNumber, "wrong fault occurred",
                     envP->fault_string);
    else
        printf(".");

    xmlrpc_env_clean(envP);
    xmlrpc_env_init(envP);
}



void
test_null_string(const char * const string,
                 const char * const fileName,
                 unsigned int const lineNumber) {

    ++total_tests;

    if (string != NULL)
        test_failure(fileName, lineNumber, "string not null", string);
    else
        printf(".");
}


