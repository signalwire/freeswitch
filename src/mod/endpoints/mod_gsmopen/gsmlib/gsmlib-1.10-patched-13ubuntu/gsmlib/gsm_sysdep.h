// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sysdep.h
// *
// * Purpose: Some magic to make alloca work on different platforms plus
// *          other system-dependent stuff
// *
// * Warning: Only include this header from gsmlib .cc-files
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 28.10.1999
// *************************************************************************

#ifndef GSM_SYSDEP_H
#define GSM_SYSDEP_H

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif

extern "C" {

  // this is mostly taken from the autoconf documentation (WIN32 added)

#ifdef __GNUC__
# define alloca __builtin_alloca
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifdef WIN32
#     include <malloc.h>
#     define alloca _alloca
#   else
#     ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#     endif
#   endif
#  endif
# endif
#endif

}

// Windows-specific stuff
#if defined(WIN32) && ! defined(__GNUC__)
#define NOMINMAX
#include <winsock.h>
#include <io.h>

#ifdef _MSC_VER
#define min __min
#endif

#define S_ISREG(mode) (((mode) & _S_IFREG) == _S_IFREG)
#define S_ISCHR(mode) (((mode) & _S_IFCHR) == _S_IFCHR)

#define read _read
#endif

// define common data types with fixed sizes

#if SIZEOF_UNSIGNED_SHORT_INT == 2
  typedef unsigned short int unsigned_int_2;
#else
#error "no suitable 2 byte unsigned int available"
#endif
#if SIZEOF_UNSIGNED_LONG_INT == 4
  typedef unsigned long int unsigned_int_4;
#else
#if SIZEOF_UNSIGNED_INT == 4
  typedef unsigned int unsigned_int_4;
#else
#error "no suitable 4 byte unsigned int available"
#endif
#endif

#endif // GSM_SYSDEP_H
