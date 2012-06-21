/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#if ZRTP_HAVE_STRING_H == 1
#	include <string.h>
#endif


/*----------------------------------------------------------------------------*/
int zrtp_zstrcmp(const zrtp_stringn_t *left, const zrtp_stringn_t *right)
{
    if (left->length == right->length) {
		return zrtp_memcmp(left->buffer, right->buffer, left->length);
	} else {
		return left->length - right->length;
	}
}

void zrtp_zstrcpy(zrtp_stringn_t *dst, const zrtp_stringn_t *src)
{
	dst->length = ZRTP_MIN(dst->max_length, src->length);
	zrtp_memcpy(dst->buffer, src->buffer, dst->length);
	if (dst->length < dst->max_length) {
		dst->buffer[dst->length] = 0;
	}
}

void zrtp_zstrcpyc(zrtp_stringn_t *dst, const char *src)
{
	dst->length = ZRTP_MIN(dst->max_length, strlen(src));
	zrtp_memcpy(dst->buffer, src, dst->length);
	if (dst->length < dst->max_length) {
		dst->buffer[dst->length] = 0;
	}
}

void zrtp_zstrncpy(zrtp_stringn_t *dst, const zrtp_stringn_t *src, uint16_t size)
{
	dst->length = ZRTP_MIN(dst->max_length, size);
	zrtp_memcpy(dst->buffer, src->buffer, dst->length);
	if (dst->length < dst->max_length) {
		dst->buffer[dst->length] = 0;
	}
}

void zrtp_zstrncpyc(zrtp_stringn_t *dst, const char *src, uint16_t size)
{
	dst->length = ZRTP_MIN(dst->max_length, size);
	zrtp_memcpy(dst->buffer, src, dst->length);
	if (dst->length < dst->max_length) {
		dst->buffer[dst->length] = 0;
	}
}

void zrtp_zstrcat(zrtp_stringn_t *dst, const zrtp_stringn_t *src)
{
	uint16_t count = ZRTP_MIN((dst->max_length - dst->length), src->length);
	zrtp_memcpy(dst->buffer + dst->length, src->buffer, count);
	dst->length += count;
	if (dst->length < dst->max_length) {
		dst->buffer[dst->length] = 0;
	}
}

void zrtp_wipe_zstring(zrtp_stringn_t *zstr)
{
	if (zstr && zstr->length) {
		zrtp_memset(zstr->buffer, 0, zstr->max_length);
		zstr->length = 0;
	}
}

int zrtp_memcmp(const void* s1, const void* s2, uint32_t n)
{
	uint32_t i = 0;
	uint8_t* s1uc = (uint8_t*) s1;
	uint8_t* s2uc = (uint8_t*) s2;
	
	for (i=0; i<n; i++) {
		if (s1uc[i] < s2uc[i]) {
			return -1;
		} else if (s1uc[i] > s2uc[i]) {
			return 1;
		}
	}
	
	return 0;
}

/*----------------------------------------------------------------------------*/
static char* hex2char(char *dst, unsigned char b)
{
	unsigned char v = b >> 4;
	*dst++ = (v<=9) ? '0'+v : 'a'+ (v-10);
	v = b & 0x0f;
	*dst++ = (v<=9) ? '0'+v : 'a'+ (v-10);
	
	return dst;
}

const char* hex2str(const char* bin, int bin_size, char* buff, int buff_size)
{
	char* nptr = buff;
	
	if (NULL == buff) {
		return "buffer is NULL";
	}		
	if (buff_size < bin_size*2) {
		return "buffer too small";
	}
	
	while (bin_size--) {
		nptr = hex2char(nptr, *bin++);
	}
	
	if (buff_size >= bin_size*2+1)
		*nptr = 0;
	
	return buff;
}

/*----------------------------------------------------------------------------*/
static int char2hex(char v)
{
	if (v >= 'a' && v <= 'f') {
		return v - 'a' + 10;
	}
	if (v >= 'A' && v <= 'F') {
		return v - 'A' + 10;
	}
	if (v >= '0' && v <= '9') {
		return v - '0';
	}
	return 0x10;
}

char *str2hex(const char* buff, int buff_size, char* bin, int bin_size)
{
	char tmp = 0;
	
	if (NULL == buff || !buff_size) {
		return "buffer is NULL || !buf_size";
	}	
	if (buff_size % 2) {
		return "buff_size has to be even";
	}	
	if (buff_size > bin_size*2) {
		return "buffer too small";
	}
	
	while (buff_size--)
	{
		int value = char2hex(*buff++);
		if (value > 0x0F) {
			return "wrong symbol in buffer";
		}
		if (buff_size % 2) {
			tmp = (char)value;
		} else {
			value |= (char)(tmp << 4);
			*bin++ = value;
		}
	}
	
	return bin;
}
