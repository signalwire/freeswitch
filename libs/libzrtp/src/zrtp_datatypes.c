/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

/*---------------------------------------------------------------------------*/
void zrtp_bitmap_right_shift(uint8_t *x, int width_bytes, int index)
{
	const int base_index = index >> 3;
	const int bit_index = index & 7;
	int i, from;
	uint8_t b;
    
	if (index > width_bytes*8) {
		for(i=0; i < width_bytes; i++) {
			x[i] = 0;
		}
		return;
	}
	
	if (bit_index == 0) {
		/* copy each word from left side to right side */
		x[width_bytes-1] = x[width_bytes-1-base_index];
		for (i=width_bytes-1; i > base_index; i--) {
			x[i-1] = x[i-1-base_index];
		}
	} else {
		/* set each word to the OR of the two bit-shifted words */
		for (i = width_bytes; i > base_index; i--) {
			from = i-1 - base_index;
			b = x[from] << bit_index;
			if (from > 0) {
				b |= x[from-1] >> (8-bit_index);
			}
			x[i-1] = b;
		}
	}
	
	/* now wrap up the final portion */
	for (i=0; i < base_index; i++) {
		x[i] = 0;
	}
}

/*---------------------------------------------------------------------------*/
void zrtp_bitmap_left_shift(uint8_t *x, int width_bytes, int index)
{
	int i;
	const int base_index = index >> 3;
	const int bit_index = index & 7;

	if (index > width_bytes*8) {
		for(i=0; i < width_bytes; i++) {
			x[i] = 0;
		}
		return;
	} 
    
	if (0 == bit_index) {
		for (i=0; i < width_bytes - base_index; i++) {
			x[i] = x[i+base_index];
		}
	} else {
		for (i=0; i < width_bytes - base_index - 1; i++) {
			x[i] = (x[i+base_index] >> bit_index) ^ (x[i+base_index+1] << (8 - bit_index));
		}
		
		x[width_bytes - base_index-1] = x[width_bytes-1] >> bit_index;
	}

	/* now wrap up the final portion */
	for (i = width_bytes - base_index; i < width_bytes; i++) {
		x[i] = 0;
	}
}

void zrtp_v128_xor(zrtp_v128_t *z, zrtp_v128_t *x, zrtp_v128_t *y)
{
  _zrtp_v128_xor(z, x, y);
}

/*---------------------------------------------------------------------------*/
uint16_t zrtp_swap16(uint16_t x) {
	return (x >> 8 | x << 8);
}

uint32_t zrtp_swap32(uint32_t x)
{
	uint32_t res = (x >> 8 & 0x0000ff00) | (x << 8 & 0x00ff0000);
	res |= (x >> 24 ) | (x << 24);
	return res;
}

#ifdef ZRTP_NO_64BIT_MATH
uint64_t zrtp_swap64(uint64_t x)
{
	uint8_t *p = &x;
	uint8_t tmp;
	int i;
	
	for(i=0; i<4; i++) {
		tmp = p[i];
		p[i] = p[7-i];
		p[7-i] = tmp;
	}
	return x;
}
#else
uint64_t zrtp_swap64(uint64_t x)
{
	uint64_t res;
	res =  (x >> 8  & 0x00000000ff000000ll) | (x << 8  & 0x000000ff00000000ll);
	res |= (x >> 24 & 0x0000000000ff0000ll) | (x << 24 & 0x0000ff0000000000ll);
	res |= (x >> 40 & 0x000000000000ff00ll) | (x << 40 & 0x00ff000000000000ll);
	res |= (x >> 56 & 0x00000000000000ffll) | (x << 56 & 0xff00000000000000ll);
	return res;
}
#endif /* ZRTP_NO_64BIT_MATH */
