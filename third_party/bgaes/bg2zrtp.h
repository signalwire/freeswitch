/*
 * Copyright (c) 2006-2008 Philip R. Zimmermann. All rights reserved.
 * Contact: http://www.philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 *
 * Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
 */

#ifndef __BG2ZRTP_H__
#define __BG2ZRTP_H__

/* Define platform byte order for Brian Gladman's AES */
#include "zrtp_config.h"

#define IS_BIG_ENDIAN      4321
#define IS_LITTLE_ENDIAN   1234


#if ZRTP_BYTE_ORDER == ZBO_LITTLE_ENDIAN
	#define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#elif ZRTP_BYTE_ORDER == ZBO_BIG_ENDIAN
	#define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#else
	#error "Can't define byte order for BG AES. Edit zrtp_system.h"
#endif


/* Define integers for Brian Gladman's AES */

#define BRG_UI8
typedef uint8_t uint_8t;

#define BRG_UI16
typedef uint16_t uint_16t;

#define BRG_UI32
//typedef uint32_t uint_32t;
typedef unsigned int uint_32t;

#define BRG_UI64
typedef uint64_t uint_64t;


#endif /*__BG2ZRTP_H__*/
