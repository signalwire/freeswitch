#ifndef STDARGX_H_INCLUDED
#define STDARGX_H_INCLUDED

#include "xmlrpc_config.h"
#include <stdarg.h>
#include <string.h>

/*----------------------------------------------------------------------------
   We need a special version of va_list in order to pass around the
   variable argument heap by reference, thus allowing a subroutine to
   advance the heap's pointer.

   On some systems (e.g. Gcc for PPC or AMD64), va_list is an array.
   That invites the scourge of array-to-pointer degeneration if you try
   to take its address.  Burying it inside a struct as we do with out
   va_listx type makes it immune.

   Example of what would happen if we used va_list instead of va_listx,
   on a system where va_list is an array:

      void sub2(va_list * argsP) [
          ...
      }

      void sub1(va_list args) {
          sub2(&args);
      }

      This doesn't work.  '&args' is the same thing as 'args', so is
      va_list, not va_list *.  The compiler will even warn you about the
      pointer type mismatch.

  To use va_listx:

      void sub1_va(char * format, va_list args) {
          va_listx argsx;
          init_va_listx(&argsx, args);
          sub2(format, &argsx);
      }

-----------------------------------------------------------------------------*/


typedef struct {
/*----------------------------------------------------------------------------
   Same thing as va_list, but in a form that works everywhere.  See above.
-----------------------------------------------------------------------------*/
    va_list v;
} va_listx;



static __inline__ void
init_va_listx(va_listx * const argsxP,
              va_list    const args) {
#if VA_LIST_IS_ARRAY
    /* 'args' is NOT a va_list.  It is a pointer to the first element of a
       'va_list', which is the same address as a pointer to the va_list
       itself.
    */
    memcpy(&argsxP->v, args, sizeof(argsxP->v));
#else
    argsxP->v = args;
#endif
}

#endif
