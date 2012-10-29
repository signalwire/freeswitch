#include <ctype.h>

#include "bool.h"
#include "girstring.h"



bool
stripcaseeq(const char * const comparand,
            const char * const comparator) {
/*----------------------------------------------------------------------------
  Compare two strings, ignoring leading and trailing blanks and case.

  Return true if the strings are identical, false otherwise.
-----------------------------------------------------------------------------*/
    const char *p, *q, *px, *qx;
    bool equal;
  
    /* Make p and q point to the first non-blank character in each string.
       If there are no non-blank characters, make them point to the terminating
       NULL.
    */

    p = &comparand[0];
    while (*p == ' ')
        ++p;
    q = &comparator[0];
    while (*q == ' ')
        ++q;

    /* Make px and qx point to the last non-blank character in each string.
       If there are no nonblank characters (which implies the string is
       null), make them point to the terminating NULL.
    */

    if (*p == '\0')
        px = p;
    else {
        px = p + strlen(p) - 1;
        while (*px == ' ')
            --px;
    }

    if (*q == '\0')
        qx = q;
    else {
        qx = q + strlen(q) - 1;
        while (*qx == ' ')
            --qx;
    }

    equal = true;   /* initial assumption */
  
    /* If the stripped strings aren't the same length, 
       we know they aren't equal 
    */
    if (px - p != qx - q)
        equal = false;


    while (p <= px) {
        if (toupper(*p) != toupper(*q))
            equal = false;
        ++p; ++q;
    }
    return equal;
}

