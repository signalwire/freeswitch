/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */
 
#ifndef ZRTP_SYMB_CONFIG_H__
#define ZRTP_SYMB_CONFIG_H__

#ifndef ZRTP_HAVE_STDIO_H
#	define ZRTP_HAVE_STDIO_H 1
#endif

#ifndef ZRTP_HAVE_STDARG_H
#	define ZRTP_HAVE_STDARG_H 1
#endif


#ifndef NO_ASSERT_H
#	define NO_ASSERT_H 1
#endif

#ifndef NO_STDLIB_H
#	define NO_STDLIB_H 0
#endif
//#define ZRTP_HAVE_INTTYPES_H 1
#ifndef ZRTP_HAVE_UNISTD_H
#	define ZRTP_HAVE_UNISTD_H 1
#endif

#ifndef ZRTP_HAVE_PTHREAD_H
#	define ZRTP_HAVE_PTHREAD_H 1
#endif

#ifndef ZRTP_HAVE_SEMAPHORE_H
#define ZRTP_HAVE_SEMAPHORE_H 1
#endif

#ifndef ZRTP_HAVE_ERRNO_H
#define ZRTP_HAVE_ERRNO_H 1
#endif

#ifndef ZRTP_HAVE_FCNTL_H
#define ZRTP_HAVE_FCNTL_H 1
#endif

#ifndef ZRTP_HAVE_SYS_TIME_H
#	define ZRTP_HAVE_SYS_TIME_H 1
#endif


#ifndef ZRTP_HAVE_SYS_TYPES_H
#	define ZRTP_HAVE_SYS_TYPES_H 1
#endif


#ifndef ZRTP_HAVE_INTTYPES_H
#	define ZRTP_HAVE_INTTYPES_H 1
#endif

#ifndef ZRTP_HAVE_STDINT_H
#	define ZRTP_HAVE_STDINT_H 1
#endif

#ifndef ZRTP_HAVE_LINUX_VERSION_H
#	define ZRTP_HAVE_LINUX_VERSION_H 0
#endif


// (ZRTP_PLATFORM == ZP_ANDROID)


#define ZRTP_HAVE_INT64_T 1
#define ZRTP_HAVE_INT32_T 1
#define ZRTP_HAVE_INT16_T 1
#define ZRTP_HAVE_INT8_T  1

#define ZRTP_HAVE_UINT64_T 1
#define ZRTP_HAVE_UINT32_T 1
#define ZRTP_HAVE_UINT16_T 1
#define ZRTP_HAVE_UINT8_T  1

#define ZRTP_BYTE_ORDER ZBO_LITTLE_ENDIAN

#define SIZEOF_UNSIGNED_LONG 4
#define SIZEOF_UNSIGNED_LONG_LONG 8

#define ZRTP_INLINE inline

#define ZRTP_USE_BUILTIN_CACHE 1
#define ZRTP_USE_BUILTIN_SCEHDULER 1
#undef ZRTP_USE_STACK_MINIM
#define ZRTP_USE_STACK_MINIM 1
#define ALIGNMENT_32BIT_REQUIRED

#endif /* ZRTP_WIN_CONFIG_H__ */
