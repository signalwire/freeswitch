#ifndef UNISTDX_H_INCLUDED
#define UNISTDX_H_INCLUDED

/* Xmlrpc-c code #includes "unistdx.h" instead of <unistd.h> because
   <unistd.h> does not exist on WIN32.
*/

#ifndef WIN32
#  include <unistd.h>
#else

#endif  /* WIN32 */

#endif
