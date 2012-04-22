/* This takes the place of C99 inttypes.h, which at least some Windows
   compilers don't have.  (October 2007).
*/

/* PRId64 is the printf-style format specifier for a long long type, as in
   long long mynumber = 5;
   printf("My number is %" PRId64 ".\n", mynumber);

   The LL/ULL macro is for 64 bit integer literals, like this:

   long long mask= ULL(1) << 33;
*/

#ifdef _MSC_VER
#  define PRId64 "I64d"
#  define PRIu64 "I64u"

#ifndef int16_t
typedef short             int16_t;
#endif
#ifndef uint16_t
typedef unsigned short    uint16_t;
#endif
#ifndef int32_t
typedef int               int32_t;
#endif
#ifndef uint32_t
typedef unsigned int      uint32_t;
#endif
#ifndef int64_t
typedef __int64           int64_t;
#endif
#ifndef uint64_t
typedef unsigned __int64  uint64_t;
#endif
#ifndef uint
typedef unsigned int      uint;
#endif
#ifndef uint8_t
typedef unsigned char     uint8_t;
#endif

/* Older Microsoft compilers don't know the standard ll/ull suffixes */
#define LL(x) x ## i64
#define ULL(x) x ## u64

#else
  /* Not Microsoft compiler */
  #include <inttypes.h>
  #define LL(x) x ## ll
  #define ULL(x) x ## ull
#endif

