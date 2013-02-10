/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

/**
 * @file zrtp_config_user.h
 * @brief libzrtp user configuration routine 
 */

#ifndef __ZRTP_CONFIG_USER_H__
#define __ZRTP_CONFIG_USER_H__

/**
 * \defgroup zrtp_config Build Configuration
 * \{
 * 
 * As libzrtp based on few OS dependent components, target platform and byte-order must be defined 
 * on compilation level. libzrtp provides automatic platform and byte-order detection. Developer 
 * needs to define these parameters manually in very specific cases only.
 * 
 * libzrtp originaly supports folowwing platforms:
 *  - 32/64-bit Windows platform;
 *  - Windows kernel mode;
 *  - Apple Mac OS X and iPhone;
 *  - Linux and *nix platforms;
 *  - Symbian OS.
 * 
 * In order to specify platform manually, developer should define ZRTP_PLATFORM value. If 
 * ZRTP_PLATFORM  is not defined - libzrtp will try to detect it automatically (see  zrtp_config.h).
 *
 * In order to specify platform byte-order manually, developer should define ZRTP_BYTE_ORDER value. 
 * If ZRTP_BYTE_ORDER  is not defined - libzrtp will try to detect it automatically.
 */

/** \brief Constant to define ZRTP Windows 32-bit platform */
#define ZP_WIN32					100
/** \brief Constant to define ZRTP Windows 64-bit platform */
#define ZP_WIN64					106
/** \brief Constant to define ZRTP Windows Kernel mode */
#define ZP_WIN32_KERNEL				101
/** \brief Constant to define ZRTP Windows CE platform */
#define ZP_WINCE					102
/** \brief Constant to define Linux and *nux platforms */
#define ZP_LINUX					103
/** \brief Constant to define Mac OS X Platform */
#define ZP_DARWIN					104
/** \brief Constant to define Symbian OS */
#define ZP_SYMBIAN					105
/** \brief Constant to define ZRTP BSD platform */
#define ZP_BSD						107
/** \brief Constant to define ZRTP Android platform */
#define ZP_ANDROID					108

/** \brief Define Platform manually there */
//#undefine ZRTP_PLATFORM


/** \brief Constant to define Big Endian Platform */
#define ZBO_BIG_ENDIAN				0x4321
/** \brief Constant to define Little Endian Platform */
#define ZBO_LITTLE_ENDIAN			0x1234

/** \brief Define Platform Byte Order manually there */
//#define ZRTP_BYTE_ORDER

/** \brief Defines the max length in bytes of a binary SAS digest */
#ifndef ZRTP_SAS_DIGEST_LENGTH
#define ZRTP_SAS_DIGEST_LENGTH		32
#endif

/** \brief Defines maximum number of ZRTP streams within one session */
#ifndef ZRTP_MAX_STREAMS_PER_SESSION
#define ZRTP_MAX_STREAMS_PER_SESSION 2
#endif

/** 
 * \brief Allows to build libzrtp against external srtp encryption library
 *
 * The latest version of libzrtp, starting with 0.3.9, supplies a built-in mechanism for SRTP 
 * encryption. However, if for some reason during  development it is neccesary to use an external 
 * library, this flag must be set.
 */
#ifndef ZRTP_USE_EXTERN_SRTP
#define ZRTP_USE_EXTERN_SRTP		0
#endif

/**
 * \brief Build libzrtp with minimum stack usage
 *
 * Set to 1 you build libzrtp in environment with strong limitation of stack size (Mobile platforms 
 * or in kernel mode). When this flag is set, some static data allocation will be changed to 
 * dynamic. The size of these data doesn't matter in "regular" PC applications, but on mobile 
 * platforms and in kernel mode, where the stack size is critical, libzrtp must work with optimized 
 * data.
 */
#ifndef ZRTP_USE_STACK_MINIM
#define ZRTP_USE_STACK_MINIM		0
#endif

#ifndef ZRTP_USE_BUILTIN
#define ZRTP_USE_BUILTIN			1
#endif

#ifndef ZRTP_USE_BUILTIN_SCEHDULER
#define ZRTP_USE_BUILTIN_SCEHDULER	1
#endif

#ifndef ZRTP_USE_BUILTIN_CACHE
#	if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WIN32) || defined(__TOS_WIN__)
#		if defined(__BUILDMACHINE__) && (__BUILDMACHINE__ == WinDDK)
#			define ZRTP_USE_BUILTIN_CACHE		1
#		else
#			define ZRTP_USE_BUILTIN_CACHE		0
#		endif
#	else
#		define ZRTP_USE_BUILTIN_CACHE		1
#	endif
#endif

#ifndef ZRTP_DEBUG_WITH_PJSIP
#define ZRTP_DEBUG_WITH_PJSIP		0
#endif

/**
 * \brief Set to 1 if you build libzrtp against libzrtp-s.
 *
 * CSD-mode was implemented to support new ZRTP/S protocol designed by KHAMSA SA, Via Giacometti 1,
 * CH-6900, Lugano - info@khamsa.ch. and Phil Zimmermann. ZRTP/S allows to make secure ZRTP calls
 * over CSD channels. This option affect enterprise version of the library only.
 */
#ifndef ZRTP_BUILD_FOR_CSD
#define	ZRTP_BUILD_FOR_CSD			0
#endif

/**
 * \brief Maximum number of Preshared exchanges allowed since last retain secret update
 * 
 * Preshared key exchange mode has lot of weaknesses comparing to DH. And one of them - lack of key
 * continuity. Preshared mode is not recommended unless there is a strong necessity in using it
 * (slow CPU device, low-latency channel).
 * 
 * To minimize risk of using Preshared exchanges, libzrtp automatically limits number for preshared
 * connection available for the same instance of RS value. In other words, libzrtp forces DH exchange
 * every \c ZRTP_PRESHARED_MAX_ALLOWED calls.
 */
#define ZRTP_PRESHARED_MAX_ALLOWED	20

/**
 * \brief Defines libzrtp log-level
 *
 * Defines  maximum log level for libzrtp: log-level 3 contains debug messages, 2 - warnings and 
 * software errors, 1 - security issues. If you set this option to 0 - libzrtp will not debug 
 * output and will not even make a log function calls.
 */
#ifndef ZRTP_LOG_MAX_LEVEL
#define ZRTP_LOG_MAX_LEVEL			3
#endif

/**
 * \brief Enables SRTP debug output
 *
 * \warning! ZRTP crypto debug logs may include security sensitive information and cause security
 *  weakness in the system. Enable SRTP debug logging only when it necessary.
 */
#ifndef ZRTP_DEBUG_SRTP_KEYS
#define ZRTP_DEBUG_SRTP_KEYS		0
#endif

/**
 * \brief Enables ZRTP Crypto debug logging.
 *
 * \warning! ZRTP crypto debug logs may include security sensitive information and cause security
 *  weakness in the system. Enable ZRTP Protocol debug logging only when it necessary.
 */
#ifndef ZRTP_DEBUG_ZRTP_KEYS
#define ZRTP_DEBUG_ZRTP_KEYS		0
#endif


/* \} */

#endif /*__ZRTP_CONFIG_USER_H__*/
