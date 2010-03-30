/* gsm_config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if using alloca.c.  */
#undef C_ALLOCA

/* Define to empty if the keyword does not work.  */
#undef const

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
#undef CRAY_STACKSEG_END

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1
#define alloca _alloca // Microsoft

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
#undef HAVE_ALLOCA_H

/* Define if you have a working `mmap' system call.  */
#undef HAVE_MMAP

/* Define as __inline if that's what the C compiler calls it.  */
#undef inline

/* Define to `long' if <sys/types.h> doesn't define.  */
#undef off_t

/* Define if you need to in order for stat and other things to work.  */
#undef _POSIX_SOURCE

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
#undef size_t

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
#undef STACK_DIRECTION

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* used by libtool*/
#define PACKAGE 0

/* used by libtool*/
#define VERSION 0

/* Define if getopt_long() available */
#undef HAVE_GETOPT_LONG

/* Define if alarm() available */
#undef HAVE_ALARM

/* Define for NLS */
#undef ENABLE_NLS
#undef HAVE_CATGETS
#undef HAVE_GETTEXT
#undef HAVE_LC_MESSAGES
#undef HAVE_STPCPY

/* Define LOCALEDIR */
#define LOCALEDIR "/usr/share/locale"

/* Define if vsnprintf() function available */
#define HAVE_VSNPRINTF 1
#ifndef WIN32
#define vsnprintf _vsnprintf
#endif //WIN32

/* The number of bytes in a unsigned int.  */
#define SIZEOF_UNSIGNED_INT 4

/* The number of bytes in a unsigned long int.  */
#define SIZEOF_UNSIGNED_LONG_INT 4

/* The number of bytes in a unsigned short int.  */
#define SIZEOF_UNSIGNED_SHORT_INT 2

/* Define if you have the __argz_count function.  */
#undef HAVE___ARGZ_COUNT

/* Define if you have the __argz_next function.  */
#undef HAVE___ARGZ_NEXT

/* Define if you have the __argz_stringify function.  */
#undef HAVE___ARGZ_STRINGIFY

/* Define if you have the dcgettext function.  */
#undef HAVE_DCGETTEXT

/* Define if you have the getcwd function.  */
#undef HAVE_GETCWD

/* Define if you have the getpagesize function.  */
#undef HAVE_GETPAGESIZE

/* Define if you have the munmap function.  */
#undef HAVE_MUNMAP

/* Define if you have the putenv function.  */
#undef HAVE_PUTENV

/* Define if you have the setenv function.  */
#undef HAVE_SETENV

/* Define if you have the setlocale function.  */
#define HAVE_SETLOCALE 1

/* Define if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* Define if you have the strcasecmp function.  */
#undef HAVE_STRCASECMP
#define strcasecmp _strcmpi

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the <argz.h> header file.  */
#undef HAVE_ARGZ_H

/* Define if you have the <libintl.h> header file.  */
#undef HAVE_LIBINTL_H

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <malloc.h> header file.  */
#define HAVE_MALLOC_H 1

/* Define if you have the <netinet/in.h> header file.  */
#undef HAVE_NETINET_IN_H

/* Define if you have the <nl_types.h> header file.  */
#undef HAVE_NL_TYPES_H

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/param.h> header file.  */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the i library (-li).  */
#undef HAVE_LIBI

/* Define if you have the intl library (-lintl).  */
#undef HAVE_LIBINTL

// WIN32 specific defines
#pragma warning( disable : 4786 )  // Disable warning messages
                                   // 4786 (id too long)

// Win32 strftime() does not return length of output when passing
// NULL pointer
#define BROKEN_STRFTIME
// Win32 STL erase() for maps makes iterators invalid
#define BUGGY_MAP_ERASE
