#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/string_int.h"



const char *
xmlrpc_makePrintable_lp(const char * const input,
                        size_t       const inputLength) {
/*----------------------------------------------------------------------------
   Convert an arbitrary string of characters in length-pointer form to
   printable ASCII.  E.g. convert newlines to "\n".

   Return the result in newly malloc'ed storage.  Return NULL if we can't
   get the storage.
-----------------------------------------------------------------------------*/
    char * output;

    output = malloc(inputLength*4+1);
        /* Worst case, we render a character like \x01 -- 4 characters */

    if (output != NULL) {
        unsigned int inputCursor, outputCursor;

        for (inputCursor = 0, outputCursor = 0; 
             inputCursor < inputLength; 
             ++inputCursor) {

            if (0) {
            } else if (input[inputCursor] == '\\') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = '\\';
            } else if (input[inputCursor] == '\n') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 'n';
            } else if (input[inputCursor] == '\t') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 't';
            } else if (input[inputCursor] == '\a') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 'a';
            } else if (input[inputCursor] == '\r') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 'r';
            } else if (isprint(input[inputCursor])) {
                output[outputCursor++] = input[inputCursor]; 
            } else {
                snprintf(&output[outputCursor], 5, "\\x%02x", 
                         input[inputCursor]);
                outputCursor += 4;
            }
        }
        output[outputCursor++] = '\0';
    }
    return output;
}



const char * 
xmlrpc_makePrintable(const char * const input) {
/*----------------------------------------------------------------------------
   Convert an arbitrary string of characters (NUL-terminated, though) to
   printable ASCII.  E.g. convert newlines to "\n".

   Return the result in newly malloc'ed storage.  Return NULL if we can't
   get the storage.
-----------------------------------------------------------------------------*/
    return xmlrpc_makePrintable_lp(input, strlen(input));
}



const char *
xmlrpc_makePrintableChar(char const input) {
/*----------------------------------------------------------------------------
   Return an ASCIIZ string consisting of the character 'input',
   properly escaped so as to be printable.  E.g., in C notation, '\n'
   turns into "\\n"
-----------------------------------------------------------------------------*/
    const char * retval;

    if (input == '\0')
        retval = strdup("\\0");
    else {
        char buffer[2];
        
        buffer[0] = input;
        buffer[1] = '\0';
        
        retval = xmlrpc_makePrintable(buffer);
    }
    return retval;
}
