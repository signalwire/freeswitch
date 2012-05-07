/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_BASE_H__
#define __ZRTP_BASE_H__

#include "zrtp_config.h"

typedef double uint64_t_;

typedef uint8_t						zrtp_uchar4_t[4];
typedef uint8_t						zrtp_uchar8_t[8];
typedef uint8_t						zrtp_uchar12_t[12];
typedef uint8_t						zrtp_uchar16_t[16];
typedef uint8_t						zrtp_uchar32_t[32];
typedef uint8_t						zrtp_uchar64_t[64];
typedef uint8_t						zrtp_uchar128_t[128];
typedef uint8_t						zrtp_uchar256_t[256];
typedef uint8_t						zrtp_uchar1024_t[1024];

typedef uint32_t					zrtp_id_t;

typedef struct zrtp_profile_t		zrtp_profile_t;
typedef struct zrtp_stream_t		zrtp_stream_t;
typedef struct zrtp_session_t		zrtp_session_t;
typedef struct zrtp_global_t		zrtp_global_t;

typedef struct zrtp_protocol_t		zrtp_protocol_t;
typedef struct zrtp_srtp_ctx_t		zrtp_srtp_ctx_t;
typedef struct zrtp_shared_secret_t	zrtp_shared_secret_t;
typedef struct zrtp_retry_task_t	zrtp_retry_task_t;

typedef struct zrtp_hash_t			zrtp_hash_t;
typedef struct zrtp_cipher_t		zrtp_cipher_t;
typedef struct zrtp_auth_tag_length_t zrtp_auth_tag_length_t;
typedef struct zrtp_pk_scheme_t		zrtp_pk_scheme_t;
typedef struct zrtp_sas_scheme_t	zrtp_sas_scheme_t;
typedef struct zrtp_sig_scheme_t	zrtp_sig_scheme_t;

typedef struct zrtp_mutex_t			zrtp_mutex_t;
typedef struct zrtp_sem_t			zrtp_sem_t;

typedef struct zrtp_stream_info_t	zrtp_stream_info_t;
typedef struct zrtp_session_info_t	zrtp_session_info_t;

#include "sha2.h"
#define MD_CTX						sha512_ctx
#define MD_Update(a,b,c)			sha512_hash((const unsigned char *)(b),c,a)


/**
 * \brief Function computing minimum value
 *
 * This macro returns the lesser of two values. If the numbers are equal, either of them is returned.
 *
 * \param left - first value for comparison;
 * \param right - second value for comparison.
 * \return
 *  - lesser of compared numbers.
 */
#define ZRTP_MIN(left, right) ((left < right) ? left : right)


/*!
 * \brief zrtp_htonXX,  zrtp_ntohXX - convert values between host and network
 * byte order
 *
 * To avoid ambiguities and difficulties with compilation on various platforms,
 * we designed our own swap functions. Byte order detection is based on zrtp_system.h.
 *
 * On the i80x86 the host byte order is little-endian (least significant byte
 * first), whereas the network byte order, as used on the Internet, is
 * big-endian (most significant byte first).
 */

uint16_t zrtp_swap16(uint16_t x);
uint32_t zrtp_swap32(uint32_t x);
uint64_t zrtp_swap64(uint64_t x);

#if ZRTP_BYTE_ORDER == ZBO_BIG_ENDIAN
/*! Converts 16 bit unsigned integer to network byte order */
#define zrtp_hton16(x)    (x)
/*! Converts 32 bit unsigned integer to network byte order */
#define zrtp_hton32(x)    (x)
/*! Converts 64 bit unsigned integer to network byte order */
#define zrtp_hton64(x)    (x)

/*! Converts 16 bit unsigned integer to host byte order */
#define zrtp_ntoh16(x)    (x)
/*! Converts 32 bit unsigned integer to host byte order */
#define zrtp_ntoh32(x)    (x)
/*! Converts 64 bit unsigned integer to host byte order */
#define zrtp_ntoh64(x)    (x)
#else /* ZBO_BIG_ENDIAN    */
/*! Converts 16 bit unsigned integer to network byte order */
#define zrtp_hton16(x)    (zrtp_swap16(x))
/*! Converts 32 bit unsigned integer to network byte order */
#define zrtp_hton32(x)    (zrtp_swap32(x))
/*! Converts 64 bit unsigned integer to network byte order */
#define zrtp_hton64(x)    (zrtp_swap64(x))

/*! Converts 16 bit unsigned integer to host byte order */
#define zrtp_ntoh16(x)    (zrtp_swap16(x))
/*! Converts 32 bit unsigned integer to host byte order */
#define zrtp_ntoh32(x)    (zrtp_swap32(x))
/*! Converts 64 bit unsigned integer to host byte order */
#define zrtp_ntoh64(x)    (zrtp_swap64(x))
#endif


/*
 * 128 and 256-bit structures used in Ciphers and SRTP module
 */
typedef union
	{
		uint8_t  v8[16];
		uint16_t v16[8];
		uint32_t v32[4];
		uint64_t v64[2];
	} zrtp_v128_t;

typedef union
	{
		uint8_t  v8[32];
		uint16_t v16[16];
		uint32_t v32[8];
		uint64_t v64[4];
	} zrtp_v256_t;

/*
 * The following macros define the data manipulation functions.
 * 
 * If DATATYPES_USE_MACROS is defined, then these macros are used directly (and
 * function-call overhead is avoided).  Otherwise, the macros are used through
 * the functions defined in datatypes.c (and the compiler provides better
 * warnings).
 */

#define _zrtp_v128_xor(z, x, y)                        \
(                                                      \
(z)->v32[0] = (x)->v32[0] ^ (y)->v32[0],               \
(z)->v32[1] = (x)->v32[1] ^ (y)->v32[1],               \
(z)->v32[2] = (x)->v32[2] ^ (y)->v32[2],               \
(z)->v32[3] = (x)->v32[3] ^ (y)->v32[3]                \
)

#define _zrtp_v128_get_bit(x, bit)                     \
(                                                      \
( (((x)->v32[(bit) >> 5]) >> ((bit) & 31)) & 1)        \
)

#define zrtp_bitmap_get_bit(x, bit)                    \
(                                                      \
( (((x)[(bit) >> 3]) >> ((bit) & 7) ) & 1)             \
)

#define zrtp_bitmap_set_bit(x, bit)                     \
(                                                       \
( (((x)[(bit) >> 3])) |= ((uint8_t)1 << ((bit) & 7)) )  \
)

#define zrtp_bitmap_clear_bit(x, bit)                   \
(                                                       \
( (((x)[(bit) >> 3])) &= ~((uint8_t)1 << ((bit) & 7)) ) \
)

void zrtp_bitmap_left_shift(uint8_t *x, int width_bytes, int index);

void zrtp_v128_xor(zrtp_v128_t *z, zrtp_v128_t *x, zrtp_v128_t *y);



//WIN64 {
#if (ZRTP_PLATFORM == ZP_WIN32_KERNEL)

#ifdef WIN64 // For 64-bit apps

unsigned __int64 __rdtsc(void);
#pragma intrinsic(__rdtsc)
#define _RDTSC __rdtsc

#else // For 32-bit apps

#define _RDTSC_STACK(ts) \
__asm rdtsc \
__asm mov DWORD PTR [ts], eax \
__asm mov DWORD PTR [ts+4], edx

__inline unsigned __int64 _inl_rdtsc32() {
	unsigned __int64 t;
	_RDTSC_STACK(t);
	return t;
}
#define _RDTSC _inl_rdtsc32

#endif

#endif
//WIN64 }


#endif /*__ZRTP_BASE_H__*/
