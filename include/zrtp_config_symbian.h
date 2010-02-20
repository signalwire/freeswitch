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


/*
 * Used to map system integer types to zrtp integer definitions.
 * Define to 1 if you have the <inttypes.h> header file.
 */
#undef ZRTP_HAVE_INTTYPES_H

/*
 * This header is needed for operations with binary file in deefault realization
 * of the secrets' cache. Can be eliminated if default cache isn't used.
 * Define to 1 if you have the <stdio.h> header file.
 */
#ifndef ZRTP_HAVE_STDIO_H
#	define ZRTP_HAVE_STDIO_H 1
#endif

#ifndef ZRTP_HAVE_STDARG_H
#	define ZRTP_HAVE_STDARG_H 1
#endif

/*
 * Used by bnlib, but we don't need this on Symbian platform.
 */
#ifndef NO_ASSERT_H
#	define NO_ASSERT_H 1
#endif

/*
 * Used by bnlib. We have stdlib in Symbian platform - set it to 1.
 */
#ifndef NO_STDLIB_H
#	define NO_STDLIB_H 0
#endif

#ifndef ZRTP_HAVE_SYS_TIME_H
#	define ZRTP_HAVE_SYS_TIME_H 1
#endif


#define ZRTP_HAVE_INT64_T 0
#define ZRTP_HAVE_INT32_T 0
#define ZRTP_HAVE_INT16_T 0
#define ZRTP_HAVE_INT8_T  0

#define ZRTP_HAVE_UINT64_T 0
#define ZRTP_HAVE_UINT32_T 0
#define ZRTP_HAVE_UINT16_T 0
#define ZRTP_HAVE_UINT8_T  0

#define ZRTP_BYTE_ORDER ZBO_BIG_ENDIAN

#define SIZEOF_UNSIGNED_LONG 4
#define SIZEOF_UNSIGNED_LONG_LONG 8

#define ZRTP_INLINE static __inline

#endif /* ZRTP_WIN_CONFIG_H__ */
