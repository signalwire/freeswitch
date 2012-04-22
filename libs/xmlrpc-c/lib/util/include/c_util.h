#ifndef C_UTIL_H_INCLUDED
#define C_UTIL_H_INCLUDED

/* C language stuff.  Doesn't involve any libraries that aren't part of
   the compiler.
*/

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

/* GNU_PRINTF_ATTR lets the GNU compiler check printf-type
   calls to be sure the arguments match the format string, thus preventing
   runtime segmentation faults and incorrect messages.
*/
#ifdef __GNUC__
#define GNU_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#else
#define GNU_PRINTF_ATTR(a,b)
#endif
#endif
