/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_STRING_H__
#define __ZRTP_STRING_H__

#include "zrtp_config.h"

/**
 * \file zrtp_strings.h
 * \brief libzrtp safe strings
 */

/*============================================================================*/
/*                       Libzrtp Strings                                      */
/*============================================================================*/

#define ZRTP_STRING8	12
#define ZRTP_STRING16	20
#define ZRTP_STRING32	36
#define ZRTP_STRING64	68
#define ZRTP_STRING128	132
#define ZRTP_STRING256	260
#define ZRTP_STRING1024	1028


#if ( ZRTP_PLATFORM != ZP_SYMBIAN )
#pragma	pack(push, 1)
#endif

typedef struct zrtp_stringn
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[0];	
} zrtp_stringn_t;

typedef struct zrtp_string8
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING8];
} zrtp_string8_t;


typedef struct zrtp_string16
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING16];
} zrtp_string16_t;

typedef struct zrtp_string32
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING32];
} zrtp_string32_t;

typedef struct zrtp_string64
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING64];
} zrtp_string64_t;

typedef struct zrtp_string128
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING128];
} zrtp_string128_t;

typedef struct zrtp_string256
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING256];
} zrtp_string256_t;

typedef struct zrtp_string1024
{
	uint16_t	length;
	uint16_t	max_length;
	char		buffer[ZRTP_STRING1024];
} zrtp_string1024_t;

#if ( ZRTP_PLATFORM != ZP_SYMBIAN )
#pragma	pack(pop)
#endif


/**
 * \defgroup zrtp_strings Libzrtp Safe Strings
 *
 * Using standard C-like strings is potentially dangerous in any program. All standard functions for 
 * working with c-strings rely on  zero-termination, since c-strings don't contain a representation 
 * of their length. This can cause many mistakes. Moreover, it is impossible to use these strings 
 * for storing binary data.
 *
 * To solve these problems libzrtp uses zstrings instead of normal c-strings. A zstring is just a 
 * wrapped c-string that stores its own length. Use the following data types, macros and utility 
 * functions for working with zstrings in your applications.
 * 
 * zstrings are easy to use, and at the same time light-weight and flexible. 
 * We use two groups of zstring types: 
 * \li zrtp_stringn_t - base type for all operations with zstrings;
 * \li zrtp_stringXX_t group - storage types.
 *
 * One can use any zrtp_stringXX_t type (big enough to store necessary data) esired and operate with 
 * it using global zstring functions. To cast zrtp_stringXX_t to zrtp_stringn_t, the \ref ZSTR_GV 
 * and \ref ZSTR_GVP macros can be used.
 *
 * The main principle of running zstrings is storing its current data size. So to avoid mistakes and 
 * mess it is advised to use preestablished initialization macros. The description of each follows.
 * \{
 */


/**
 * \brief Casts zrtp_stringXX_t to a pointer to zrtp_stringn_t.
 *
 * This macro prevents static casts caused by using zstring functions. Prevents mistakes and makes 
 * zstrings safer to use. 
 * \sa ZSTR_GVP
 */
#define ZSTR_GV(pstr) \
(zrtp_stringn_t*)((char*)pstr.buffer - sizeof(pstr.max_length) - sizeof(pstr.length))

/**
 * \brief Casts zrtp_stringXX_t* to a pointer to zrtp_stringn_t.
 *
 * This macro prevents static casts from using zstring functions.
 * \sa ZSTR_GV
 */
#define ZSTR_GVP(pstr) \
(zrtp_stringn_t*)((char*)pstr->buffer - sizeof(pstr->max_length) - sizeof(pstr->length))

/**
 * \brief Macro for empty zstring initialization
 * \warning Use this macro on every zrtp_string structure allocation.
 * usage: \code zrtp_string_t zstr = ZSTR_INIT_EMPTY(zstr); \endcode
 */
#define	ZSTR_INIT_EMPTY(a) { 0, sizeof(a.buffer) - 1, { 0 }}

/**
 * \brief Macro for zstring initialization from a constant C-string
 * usage: \code zrtp_string_t zstr = ZSTR_INIT_WITH_CONST_CSTRING("zstring use example"); \endcode
 */
#define	ZSTR_INIT_WITH_CONST_CSTRING(s) {sizeof(s) - 1, 0, s}

/**
 * \brief Macro for zstring clearing
 *
 * Use this macro for initializing already created zstrings
 * usage: \code ZSTR_SET_EMPTY(zstr); \endcode
 */
#define	ZSTR_SET_EMPTY(a)\
{ a.length = 0; a.max_length = sizeof(a.buffer) - 1; a.buffer[0] = 0; }


#if defined(__cplusplus)
extern "C"
{
#endif
	
/**
 * \brief compare two zstrings
 *
 * Function compares the two strings left and right.
 * \param left - one string for comparing;
 * \param right - the other string for comparing.
 * \return
 *  - -1 if left string less than right;
 *  - 0 if left string is equal to right;
 *  - 1 if left string greater than right.
 */
int zrtp_zstrcmp(const zrtp_stringn_t *left, const zrtp_stringn_t *right);

/**
 * \brief Copy a zstring
 *
 * The zrtp_zstrcpy function copies the string pointed by src to the  structure pointed to by dst.
 * \param src source string;
 * \param dst destination string.
 */
void zrtp_zstrcpy(zrtp_stringn_t *dst, const zrtp_stringn_t *src);

/**
 * \brief Copy first N bytes of zstring
 *
 * The zrtp_zstrncpy function copies the first N bytes from the string pointed to by src to the 
 * structure pointed by dst.
 * \param src - source string;
 * \param dst - destination string;
 * \param size - nuber of bytes to copy.
 */
void zrtp_zstrncpy(zrtp_stringn_t *dst, const zrtp_stringn_t *src, uint16_t size);

/**
 * @brief Copy a c-string into a z-string
 * \param dst - destination zsyring
 * \param src - source c-string to be copied. 
 */
void zrtp_zstrcpyc(zrtp_stringn_t *dst, const char *src);


/**
 * \brief Copy first N bytes of a c-string into a z-string
 * \param dst - destination zsyring
 * \param src - source c-string to be copied.
 * \param size - number of bytes to be copied from \c src to \c dst
 */
void zrtp_zstrncpyc(zrtp_stringn_t *dst, const char *src, uint16_t size);

/**
 * \brief Concatenate two strings
 *
 * The zrtp_zstrcat function  appends the src string to the dst string. If dst string doesn't have 
 * enough space it will be truncated.
 * \param src source string;
 * \param dst destination string.
 */
void zrtp_zstrcat(zrtp_stringn_t *dst, const zrtp_stringn_t *src);

/**
 * \brief Clear a zstring
 * \param zstr - string for clearing;
 */
void zrtp_wipe_zstring(zrtp_stringn_t *zstr);

/**
 * \brief Compare two binary strings
 *
 * This function is used to prevent errors caused by other, non byte-to-byte comparison 
 * implementations. The secret sorting function is sensitive to such things.
 *
 * \param s1 - first string for comparison
 * \param s2 - second string for comparison
 * \param n - number of bytes to be compared
 * \return - an integer less than, equal to, or greater than zero, if the first n bytes of s1 
 * is found, respectively, to be less than, to match, or to be greater than the first n bytes of s2.
 */
int zrtp_memcmp(const void* s1, const void* s2, uint32_t n);

/**
 * \brief Converts binary data to the hex string representation
 *
 * \param bin - pointer to the binary buffer for converting;
 * \param bin_size - binary data size;
 * \param buff - destination buffer;
 * \param buff_size - destination buffer size.
 * \return 
 *  - pointer to the buff with converted data;
 *  - "Buffer too small" in case of error.
 */
const char* hex2str(const char* bin, int bin_size, char* buff, int buff_size);

/**
 * \brief Converts hex string to the binary representation
 *
 * \param buff - source buffer for converting;
 * \param buff_size - source buffer size; 
 * \param bin - pointer to the destination binary buffer;
 * \param bin_size - binary data size;
 * \return 
 *  - pointer to the buff with converted data, or NULL in case of error.
 */
char *str2hex(const char* buff, int buff_size, char* bin, int bin_size);
	
#if defined(__cplusplus)
}
#endif

/** \} */

#endif /* __ZRTP_STRING_H__ */
