/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_CONFIG_H__
#define __ZRTP_CONFIG_H__

#include "zrtp_config_user.h"

/*
 * ZRTP PLATFORM DETECTION                                                     
 * If platworm is not specified manually in zrtp_config_user.h - try to detect it aytomatically
 */
#if !defined(ZRTP_PLATFORM)
#	if defined(ANDROID_NDK)
#		define ZRTP_PLATFORM ZP_ANDROID
#	elif defined(__FreeBSD__)
#		define ZRTP_PLATFORM ZP_BSD
#	elif defined(linux) || defined(__linux)
#		include <linux/version.h>
#		define ZRTP_PLATFORM ZP_LINUX
#	elif defined(__MACOSX__) || defined (__APPLE__) || defined (__MACH__)
#		define ZRTP_PLATFORM ZP_DARWIN
#	elif defined(_WIN32_WCE) || defined(UNDER_CE)
#		include <windef.h>
#		define ZRTP_PLATFORM ZP_WINCE
#	elif defined(__SYMBIAN32__)
#		define ZRTP_PLATFORM ZP_SYMBIAN
#	elif defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WIN32) || defined(__TOS_WIN__)
#		if defined(__BUILDMACHINE__) && (__BUILDMACHINE__ == WinDDK)
#			define ZRTP_PLATFORM ZP_WIN32_KERNEL
#		elif defined(_WIN64)
#			define ZRTP_PLATFORM ZP_WIN32
#		else
#			define ZRTP_PLATFORM ZP_WIN32
#		endif
#	endif
#endif

#if ZRTP_PLATFORM == ZP_ANDROID
#	include "zrtp_config_android.h"
#elif (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_BSD) || defined(ZRTP_AUTOMAKE)
#	include "zrtp_config_unix.h"
#elif (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN32_KERNEL) || (ZRTP_PLATFORM == ZP_WINCE)
#	include "zrtp_config_win.h"
#elif (ZRTP_PLATFORM == ZP_SYMBIAN)
#	include "zrtp_config_symbian.h"
#endif

#if !defined(ZRTP_PLATFORM)
#    error "Libzrtp can't detect software platform: use manual setup in zrtp_config_user.h"
#endif

#if ZRTP_HAVE_LINUX_VERSION_H == 1
#include <linux/version.h>
#endif
#if ZRTP_HAVE_ASM_TYPES_H == 1
#include <asm/types.h>
#endif

/* 
 * ZRTP BYTEORDER DETECTION
 * If the byte order is not specified manually in zrtp_config_user.h - try to detect it automatically
 */
#if !defined(ZRTP_BYTE_ORDER)

#if defined(_i386_) || defined(i_386_) || defined(_X86_) || defined(x86) || defined(__i386__) || \
	defined(__i386) || defined(_M_IX86) || defined(__I86__)
/*
 * Generic i386 processor family, little-endian
 */
#define ZRTP_BYTE_ORDER ZBO_LITTLE_ENDIAN

#elif defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_AMD64_)
/*
 * AMD 64bit processor, little endian
 */
#define ZRTP_BYTE_ORDER ZBO_LITTLE_ENDIAN

#elif defined(	__sparc__) || defined(__sparc)
/*
 * Sun Sparc, big endian
 */
#define ZRTP_BYTE_ORDER ZBO_BIG_ENDIAN

#elif defined(ARM) || defined(_ARM_) || defined(ARMV4) || defined(__arm__)
/*
 * ARM, default to little endian
 */
#define ZRTP_BYTE_ORDER ZBO_LITTLE_ENDIAN

#elif defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__) || \
	  defined(_M_PPC) || defined(_ARCH_PPC)
/*
 * PowerPC, big endian
 */
#define ZRTP_BYTE_ORDER ZBO_BIG_ENDIAN

#elif defined(__MIPSEB__)
/*
 * mips, big endian
 */
#define ZRTP_BYTE_ORDER ZBO_BIG_ENDIAN

#elif defined(__MIPSEL__)
/*
 * mips, little endian
 */
#define ZRTP_BYTE_ORDER ZBO_LITTLE_ENDIAN

#endif /* Automatic byte order detection */

#endif

#if !defined(ZRTP_BYTE_ORDER)
#    error "Libzrtp can't detect byte order: use manual setup in zrtp_config_user.h"
#endif


/*
 * Define Unaligned structure for target platform
 */
#if (ZRTP_PLATFORM == ZP_WINCE)
#	define ZRTP_UNALIGNED(type)	UNALIGNED type
#else
#	define ZRTP_UNALIGNED(type)	type
#endif


/*
 * Define basic literal types for libzrtp
 * We use this definitions in SRTP, AES and Hash implementation
 */
#if (ZRTP_PLATFORM != ZP_WIN32_KERNEL)
#	if ZRTP_HAVE_STDLIB_H == 1
#		include <stdlib.h>
#	endif
#	if ZRTP_HAVE_STDINT_H == 1
#		include <stdint.h>
#	endif
#	if ZRTP_HAVE_INTTYPES_H == 1
#		include <inttypes.h>
#	endif
#	if ZRTP_HAVE_SYS_TYPES_H == 1
#		include <sys/types.h>
#	endif
#	if ZRTP_HAVE_SYS_INT_TYPES_H == 1
#		include <sys/int_types.h>
#	endif
#	if ZRTP_HAVE_MACHINE_TYPES_H == 1
#		include <machine/types.h>
#	endif
#endif

#if (ZRTP_PLATFORM == ZP_WINCE) || (ZRTP_PLATFORM == ZP_SYMBIAN) || (ZRTP_PLATFORM == ZP_ANDROID)
#	define ALIGNMENT_32BIT_REQUIRED
#endif

#ifdef ZRTP_HAVE_UINT64_T
#	if ZRTP_HAVE_UINT64_T == 0
#		if defined(WIN32) || defined(WIN64)
#			if defined(_MSC_VER) && (_MSC_VER < 1310)
				typedef __int64				uint64_t;
#			else
				typedef unsigned long long	uint64_t;
#			endif
#		else
#			if SIZEOF_UNSIGNED_LONG == 8
				typedef unsigned long		uint64_t;
#			elif SIZEOF_UNSIGNED_LONG_LONG == 8
				typedef unsigned long long	uint64_t;
#			else
#				define ZRTP_NO_64BIT_MATH 1
#			endif
#		endif /* WIN32 */
#	endif
#endif

#ifdef ZRTP_HAVE_INT64_T
#	if ZRTP_HAVE_INT64_T == 0
#		if defined(WIN32) || defined(WIN64)
#			if defined(_MSC_VER) && (_MSC_VER < 1310)		
				typedef __int64		int64_t;
#			else
				typedef long long	int64_t;
#			endif
#		else
#			if SIZEOF_UNSIGNED_LONG == 8
				typedef long		int64_t;
#			elif SIZEOF_UNSIGNED_LONG_LONG == 8
				typedef long long	int64_t;
#			else
#				define ZRTP_NO_64BIT_MATH 1
#			endif
#		endif /* WIN32 */
#	endif
#endif

#define SIZEOF_UNSIGNED_LONG_LONG 8

#if defined(WIN32) || defined(WIN64)
#	if defined(_MSC_VER) && (_MSC_VER < 1310)	
#		define li_64(h) 0x##h##ui64
#	else
#		define li_64(h) 0x##h##ull
#	endif
#else
#	if SIZEOF_UNSIGNED_LONG == 8
#		define li_64(h) 0x##h##ul
#	elif SIZEOF_UNSIGNED_LONG_LONG == 8
#		define li_64(h) 0x##h##ull
#	else
#		define ZRTP_NO_64BIT_MATH 1
#	endif
#endif /* WIN32 */


#ifdef ZRTP_HAVE_UINT8_T
#	if ZRTP_HAVE_UINT8_T == 0
		typedef unsigned char		uint8_t;
#	endif
#endif

#ifdef ZRTP_HAVE_UINT16_T
#	if ZRTP_HAVE_UINT16_T == 0
		typedef unsigned short int	uint16_t;
#	endif
#endif

#ifdef ZRTP_HAVE_UINT32_T
#	if ZRTP_HAVE_UINT32_T == 0
		typedef unsigned int		uint32_t;
#	endif
#endif

#ifdef ZRTP_HAVE_INT8_T
#	if ZRTP_HAVE_INT8_T == 0
		typedef char				int8_t;
#	endif
#endif

#ifdef ZRTP_HAVE_INT16_T
#	if ZRTP_HAVE_INT16_T == 0
		typedef short int			int16_t;
#	endif
#endif

#ifdef ZRTP_HAVE_INT32_T
#	if ZRTP_HAVE_INT32_T == 0
	typedef int						int32_t;
#	endif
#endif

#endif /*__ZRTP_CONFIG_H__ */
