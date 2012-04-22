#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"


static size_t
nextLineSize(const char * const string,
             size_t       const startPos,
             size_t       const stringSize) {
/*----------------------------------------------------------------------------
   Return the length of the line that starts at offset 'startPos' in the
   string 'string', which is 'stringSize' characters long.

   'string' in not NUL-terminated.
   
   A line begins at beginning of string or after a newline character and
   runs through the next newline character or end of string.  The line
   includes the newline character at the end, if any.
-----------------------------------------------------------------------------*/
    size_t i;

    for (i = startPos; i < stringSize && string[i] != '\n'; ++i);

    if (i < stringSize)
        ++i;  /* Include the newline */

    return i - startPos;
}



void
xmlrpc_traceXml(const char * const label, 
                const char * const xml,
                unsigned int const xmlLength) {

    if (getenv("XMLRPC_TRACE_XML")) {
        size_t cursor;  /* Index into xml[] */

        fprintf(stderr, "%s:\n\n", label);

        for (cursor = 0; cursor < xmlLength; ) {
            /* Print one line of XML */

            size_t const lineSize = nextLineSize(xml, cursor, xmlLength);
            const char * const xmlPrintableLine =
                xmlrpc_makePrintable_lp(&xml[cursor], lineSize);

            fprintf(stderr, "%s\n", xmlPrintableLine);

            cursor += lineSize;

            xmlrpc_strfree(xmlPrintableLine);
        }
        fprintf(stderr, "\n");
    }
}

