/* config_in.h.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
#undef AC_APPLE_UNIVERSAL_BUILD

/* Define if building for a CISC machine (e.g. Intel). */
#undef CPU_CISC

/* Define if building for a RISC machine (assume slow byte access). */
#undef CPU_RISC

/* Enable extra debugging. */
#undef DEBUG

/* Path to random device */
#undef DEV_URANDOM

/* Define to compile in dynamic debugging system. */
#undef ENABLE_DEBUGGING

/* Report errors to this file. */
#undef ERR_REPORTING_FILE

/* Define to use logging to stdout. */
#undef ERR_REPORTING_STDOUT

/* Define this to use ISMAcryp code. */
#undef GENERIC_AESICM

/* Define to 1 if you have the <arpa/inet.h> header file. */
#undef HAVE_ARPA_INET_H

/* define if you have a usable bswap_64 in byteswap.h */
#undef HAVE_BYTESWAP_H

/* Define to 1 if you have the <dlfcn.h> header file. */
#undef HAVE_DLFCN_H

/* Define to 1 if you have the `inet_aton' function. */
#undef HAVE_INET_ATON

/* Define to 1 if the system has the type `int16_t'. */
#undef HAVE_INT16_T

/* Define to 1 if the system has the type `int32_t'. */
#undef HAVE_INT32_T

/* Define to 1 if the system has the type `int8_t'. */
#undef HAVE_INT8_T

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the `crypto' library (-lcrypto). */
#undef HAVE_LIBCRYPTO

/* Define to 1 if you have the `socket' library (-lsocket). */
#undef HAVE_LIBSOCKET

/* Define to 1 if you have the <machine/types.h> header file. */
#undef HAVE_MACHINE_TYPES_H

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have the <netinet/in.h> header file. */
#undef HAVE_NETINET_IN_H

/* Define to 1 if you have the `sigaction' function. */
#undef HAVE_SIGACTION

/* Define to 1 if you have the `socket' function. */
#undef HAVE_SOCKET

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if you have the <syslog.h> header file. */
#undef HAVE_SYSLOG_H

/* Define to 1 if you have the <sys/int_types.h> header file. */
#undef HAVE_SYS_INT_TYPES_H

/* Define to 1 if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <sys/uio.h> header file. */
#undef HAVE_SYS_UIO_H

/* Define to 1 if the system has the type `uint16_t'. */
#undef HAVE_UINT16_T

/* Define to 1 if the system has the type `uint32_t'. */
#undef HAVE_UINT32_T

/* Define to 1 if the system has the type `uint64_t'. */
#undef HAVE_UINT64_T

/* Define to 1 if the system has the type `uint8_t'. */
#undef HAVE_UINT8_T

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to 1 if you have the `usleep' function. */
#undef HAVE_USLEEP

/* Define to 1 if you have the <windows.h> header file. */
#undef HAVE_WINDOWS_H

/* Define to 1 if you have the <winsock2.h> header file. */
#undef HAVE_WINSOCK2_H

/* Define to use X86 inlined assembly code */
#undef HAVE_X86

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#undef LT_OBJDIR

/* Define this to use OpenSSL crypto. */
#undef OPENSSL

/* Enable Optimization. */
#undef OPTIMZER

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* The size of `unsigned long', as computed by sizeof. */
#undef SIZEOF_UNSIGNED_LONG

/* The size of `unsigned long long', as computed by sizeof. */
#undef SIZEOF_UNSIGNED_LONG_LONG

/* Define to compile for kernel contexts. */
#undef SRTP_KERNEL

/* Define to compile for Linux kernel context. */
#undef SRTP_KERNEL_LINUX

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Write errors to this file */
#undef USE_ERR_REPORTING_FILE

/* Define to use syslog logging. */
#undef USE_SYSLOG

/* Version number of package */
#undef VERSION

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* define it the right way ;) */
#undef __FUNCTION__

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t
