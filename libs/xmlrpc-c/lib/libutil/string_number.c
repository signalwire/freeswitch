/*============================================================================
                                string_number
==============================================================================
  This file contains utilities for dealing with text string representation
  of numbers.
============================================================================*/
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/util.h>
#include <xmlrpc-c/string_int.h>
#include "xmlrpc_config.h"
#include "int.h"

#include <xmlrpc-c/string_number.h>



void
xmlrpc_parse_int64(xmlrpc_env *   const envP,
                   const char *   const str,
                   xmlrpc_int64 * const i64P) {

    xmlrpc_int64 i64val;

    char * tail;

    errno = 0;
    i64val = XMLRPC_STRTOLL(str, &tail, 10);

    if (errno == ERANGE)
        xmlrpc_faultf(envP, "Number cannot be represented in 64 bits.  "
                      "Must be in the range "
                      "[%" XMLRPC_PRId64 " - %" XMLRPC_PRId64 "]",
                      XMLRPC_INT64_MIN, XMLRPC_INT64_MAX);
    else if (errno != 0)
        xmlrpc_faultf(envP, "unexpected error: "
                      "strtoll() failed with errno %d (%s)",
                      errno, strerror(errno));
    else if (tail[0] != '\0')
        xmlrpc_faultf(envP, "contains non-numerical junk: '%s'", tail);
    else
        *i64P = i64val;
}
