#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include "int.h"
#include "girstring.h"
#include "casprintf.h"

#include "string_parser.h"

static const char *
strippedSubstring(const char * const string) {

    const char * p;

    for (p = &string[0]; isspace(*p); ++p);

    return p;
}



void
interpretUll(const char *  const string,
             uint64_t *    const ullP,
             const char ** const errorP) {

    /* strtoull() has the same disappointing weaknesses of strtoul().
       See interpretUint().
    */

    const char * const strippedString = strippedSubstring(string);

    if (strippedString[0] == '\0')
        casprintf(errorP, "Null (or all whitespace) string.");
    else if (!isdigit(strippedString[0]))
        casprintf(errorP, "First non-blank character is '%c', not a digit.",
                  strippedString[0]);
    else {
        /* strtoull() does a bizarre thing where if the number is out
           of range, it returns a clamped value but tells you about it
           by setting errno = ERANGE.  If it is not out of range,
           strtoull() leaves errno alone.
        */
        char * tail;
        
        errno = 0;  /* So we can tell if strtoull() overflowed */

        *ullP = XMLRPC_STRTOULL(strippedString, &tail, 10);
        
        if (tail[0] != '\0')
            casprintf(errorP, "Non-digit stuff in string: %s", tail);
        else if (errno == ERANGE)
            casprintf(errorP, "Number too large");
        else
            *errorP = NULL;
    }
}



void
interpretLl(const char *  const string,
            int64_t *     const llP,
            const char ** const errorP) {

    if (string[0] == '\0')
        casprintf(errorP, "Null string.");
    else {
        /* strtoll() does a bizarre thing where if the number is out
           of range, it returns a clamped value but tells you about it
           by setting errno = ERANGE.  If it is not out of range,
           strtoll() leaves errno alone.
        */
        char * tail;
        
        errno = 0;  /* So we can tell if strtoll() overflowed */

        *llP = XMLRPC_STRTOLL(string, &tail, 10);
        
        if (tail[0] != '\0')
            casprintf(errorP, "Non-digit stuff in string: %s", tail);
        else if (errno == ERANGE)
            casprintf(errorP, "Number too large");
        else
            *errorP = NULL;
    }
}



void
interpretUint(const char *   const string,
              unsigned int * const uintP,
              const char **  const errorP) {

    /* strtoul() does a lousy job of dealing with invalid numbers.  A null
       string is just zero; a negative number is a large positive one; a
       positive (cf unsigned) number is accepted.  strtoul is inconsistent
       in its treatment of the tail; if there is no valid number at all,
       it returns the entire string as the tail, including leading white
       space and sign, which are not themselves invalid.
    */

    const char * const strippedString = strippedSubstring(string);

    if (strippedString[0] == '\0')
        casprintf(errorP, "Null (or all whitespace) string.");
    else if (!isdigit(strippedString[0]))
        casprintf(errorP, "First non-blank character is '%c', not a digit.",
                  strippedString[0]);
    else {
        /* strtoul() does a bizarre thing where if the number is out
           of range, it returns a clamped value but tells you about it
           by setting errno = ERANGE.  If it is not out of range,
           strtoul() leaves errno alone.
        */
        char * tail;
        unsigned long ulongValue;
        
        errno = 0;  /* So we can tell if strtoul() overflowed */

        ulongValue = strtoul(strippedString, &tail, 10);
        
        if (tail[0] != '\0')
            casprintf(errorP, "Non-digit stuff in string: %s", tail);
        else if (errno == ERANGE)
            casprintf(errorP, "Number too large");
        else if (ulongValue > UINT_MAX)
            casprintf(errorP, "Number too large");
        else {
            *uintP = ulongValue;
            *errorP = NULL;
        }
    }
}



void
interpretInt(const char *  const string,
             int *         const intP,
             const char ** const errorP) {

    if (string[0] == '\0')
        casprintf(errorP, "Null string.");
    else {
        /* strtol() does a bizarre thing where if the number is out
           of range, it returns a clamped value but tells you about it
           by setting errno = ERANGE.  If it is not out of range,
           strtol() leaves errno alone.
        */
        char * tail;
        long longValue;
        
        errno = 0;  /* So we can tell if strtol() overflowed */

        longValue = strtol(string, &tail, 10);
        
        if (tail[0] != '\0')
            casprintf(errorP, "Non-digit stuff in string: %s", tail);
        else if (errno == ERANGE)
            casprintf(errorP, "Number too large");
        else if (longValue > INT_MAX)
            casprintf(errorP, "Number too large");
        else if (longValue < INT_MIN)
            casprintf(errorP, "Number too negative");
        else {
            *intP = longValue;
            *errorP = NULL;
        }
    }
}



void
interpretBinUint(const char *  const string,
                 uint64_t *    const valueP,
                 const char ** const errorP) {

    char * tailptr;
    long const mantissa_long = strtol(string, &tailptr, 10);

    if (errno == ERANGE)
        casprintf(errorP,
                  "Numeric value out of range for computation: '%s'.  "
                  "Try a smaller number with a K, M, G, etc. suffix.",
                  string);
    else {
        int64_t const mantissa = mantissa_long;

        int64_t argNumber;

        *errorP = NULL;  /* initial assumption */

        if (*tailptr == '\0')
            /* There's no suffix.  A pure number */
            argNumber = mantissa * 1;
        else if (stripcaseeq(tailptr, "K"))
            argNumber = mantissa * 1024;
        else if (stripcaseeq(tailptr, "M"))
            argNumber = mantissa * 1024 * 1024;
        else if (stripcaseeq(tailptr, "G"))
            argNumber = mantissa * 1024 * 1024 * 1024;
        else if (stripcaseeq(tailptr, "T"))
            argNumber = mantissa * 1024 * 1024 * 1024 * 1024;
        else if (stripcaseeq(tailptr, "P"))
            argNumber = mantissa * 1024 * 1024 * 1024 * 1024 * 1024;
        else {
            argNumber = 0;  /* quiet compiler warning */
            casprintf(errorP, "Garbage suffix '%s' on number", tailptr);
        }
        if (!*errorP) {
            if (argNumber < 0)
                casprintf(errorP, "Unsigned numeric value is "
                          "negative: %" PRId64, argNumber);
            else
                *valueP = (uint64_t) argNumber;
        }
    }
}
