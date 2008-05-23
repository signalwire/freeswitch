/* This takes the place of C99 stdbool.h, which at least some Windows
   compilers don't have.  (October 2005).

   One must not also include <stdbool.h>, because it might cause a name
   collision.
*/

#ifndef __cplusplus
/* At least the GNU compiler defines __bool_true_false_are_defined */
#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined
typedef enum {
    false = 0,
    true = 1
} bool;
#endif
#endif

