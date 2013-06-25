/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_bitpack.h -- BITPACKING code for RFC3551 and AAL2 packing
 *
 */
/*! \file switch_bitpack.h
    \brief BITPACKING code for RFC3551 and AAL2 packing

*/
#ifndef SWITCH_BITPACK_H
#define SWITCH_BITPACK_H
SWITCH_BEGIN_EXTERN_C
#include <switch.h>
#ifdef DEBUG_BITS
static char bb[80] = "";
static inline char *print_bits(switch_byte_t byte, char *x)
{

	int i, j = 0;
	x[j++] = '[';
	for (i = 7; i >= 0; i--) {
		x[j++] = (byte & (1 << i)) ? '1' : '0';
	}
	x[j++] = ']';
	x[j++] = '\0';
	return x;
}
#endif



/*!
  \defgroup bp1 Bitpacking 
  \ingroup core1
  \{ 
*/

static const uint8_t SWITCH_BITPACKED_MASKS[] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };
static const uint8_t SWITCH_REVERSE_BITPACKED_MASKS[] = { 255, 254, 252, 248, 240, 224, 192, 128 };

/*!
  \brief Initialize a bitpack object
  \param pack the pack object
  \param bitlen the number of bits per packet
  \param buf the buffer to use for storage
  \param buflen the length of the storage buffer
  \param mode RFC3551 or AAL2 mode (curse you backwards folks) 
*/
DoxyDefine(void switch_bitpack_init(switch_bitpack_t *pack, int32_t bitlen, switch_byte_t *buf, uint32_t buflen, switch_bitpack_mode_t mode))
	 static inline int8_t switch_bitpack_init(switch_bitpack_t *pack, switch_byte_t bitlen, switch_byte_t *buf, uint32_t buflen,
											  switch_bitpack_mode_t mode)
{
	if (!pack || (bitlen > SWITCH_BITS_PER_BYTE) || !buf || !buflen) {
		return -1;
	}

	memset(pack, 0, sizeof(*pack));
	memset(buf, 0, buflen);
	pack->frame_bits = bitlen;
	pack->buf = buf;
	pack->buflen = buflen;
	pack->cur = pack->buf;
	pack->mode = mode;

	return 0;
}

static inline void pack_check_over(switch_bitpack_t *pack)
{
	switch_byte_t this_byte = pack->this_byte;

	if (pack->over) {
		pack->bits_cur = pack->over;

		if (pack->mode == SWITCH_BITPACK_MODE_RFC3551) {
			switch_assert(pack->over <= 8);
			this_byte &= SWITCH_BITPACKED_MASKS[pack->over];
			this_byte <<= pack->under;
			*pack->cur |= this_byte;
			pack->cur++;
		} else {
			switch_byte_t mask = SWITCH_BITS_PER_BYTE - pack->over;
			switch_assert(mask < 8);	/* if pack->over this will allways be true */
			this_byte &= SWITCH_REVERSE_BITPACKED_MASKS[mask];
			this_byte >>= mask;

			*pack->cur <<= pack->over;
			*pack->cur |= this_byte;
			pack->cur++;
		}


		pack->bytes++;
		pack->over = pack->under = 0;
	}
}

/*!
  \brief finalize a bitpack object
  \param pack the pack/unpack object
*/
DoxyDefine(int8_t switch_bitpack_done(switch_bitpack_t *pack))
	 static inline int8_t switch_bitpack_done(switch_bitpack_t *pack)
{

	if (pack->bits_cur && pack->bits_cur < SWITCH_BITS_PER_BYTE) {
		pack->bytes++;
		if (pack->mode == SWITCH_BITPACK_MODE_AAL2) {
			*pack->cur <<= SWITCH_BITS_PER_BYTE - pack->bits_cur;
		}
	}

	if (pack->over) {
		pack_check_over(pack);
	}
	return 0;
}


/*!
  \brief pull data out of a bitpack object into it's buffer
  \param unpack the pack/unpack object
  \param in a 1 byte int packed with bits
  \return -1 if the buffer is full otherwise 0
*/
DoxyDefine(int8_t switch_bitpack_out(switch_bitpack_t *unpack, switch_byte_t in))
	 static inline int8_t switch_bitpack_out(switch_bitpack_t *unpack, switch_byte_t in)
{
	switch_byte_t this_byte;

	if ((uint32_t) (unpack->cur - unpack->buf) > unpack->buflen) {
		return -1;
	}

	unpack->bits_cur = 0;
	unpack->this_byte = this_byte = in;



	pack_check_over(unpack);
	while (unpack->bits_cur <= SWITCH_BITS_PER_BYTE) {
		switch_byte_t next = unpack->bits_cur + unpack->frame_bits;
		switch_byte_t under_in;
		switch_byte_t mask;
		this_byte = unpack->this_byte;

		if (next > SWITCH_BITS_PER_BYTE) {
			unpack->over = next - SWITCH_BITS_PER_BYTE;
			unpack->under = unpack->frame_bits - unpack->over;

			if (unpack->mode == SWITCH_BITPACK_MODE_RFC3551) {
				mask = SWITCH_BITS_PER_BYTE - unpack->under;

				under_in = this_byte & SWITCH_REVERSE_BITPACKED_MASKS[mask];
				under_in >>= mask;
				*unpack->cur |= under_in;
			} else {
				mask = unpack->under;
				under_in = this_byte & SWITCH_BITPACKED_MASKS[mask];
				*unpack->cur <<= mask;
				*unpack->cur |= under_in;
			}

			break;
		}

		if (unpack->mode == SWITCH_BITPACK_MODE_RFC3551) {
			this_byte >>= unpack->bits_cur;
			this_byte &= SWITCH_BITPACKED_MASKS[unpack->frame_bits];
			*unpack->cur |= this_byte;
			unpack->cur++;
		} else {
			this_byte >>= (SWITCH_BITS_PER_BYTE - next);
			this_byte &= SWITCH_BITPACKED_MASKS[unpack->frame_bits];

			*unpack->cur |= this_byte;
			unpack->cur++;
		}

		unpack->bits_cur = next;
		unpack->bytes++;


	}


	return 0;
}


/*!
  \brief pack data into a bitpack object's buffer
  \param pack the pack/unpack object
  \param in a 1 byte int with 1 packet worth of bits
  \return -1 if the buffer is full otherwise 0
*/
DoxyDefine(int8_t switch_bitpack_in(switch_bitpack_t *pack, switch_byte_t in))
	 static inline int8_t switch_bitpack_in(switch_bitpack_t *pack, switch_byte_t in)
{
	int next = pack->bits_cur + pack->frame_bits;

	if ((uint32_t) (pack->cur - pack->buf) > pack->buflen) {
		return -1;
	}

	pack->bits_tot += pack->frame_bits;

	if (next > SWITCH_BITS_PER_BYTE) {
		switch_byte_t a = 0, b = 0, rem, nxt;
		rem = SWITCH_BITS_PER_BYTE - pack->bits_cur;
		nxt = pack->frame_bits - rem;
		if (pack->mode == SWITCH_BITPACK_MODE_RFC3551) {
			a = in & SWITCH_BITPACKED_MASKS[rem];
			b = in >> rem;
			a <<= pack->shiftby;
			*pack->cur |= a;
			pack->cur++;
			*pack->cur |= b;
			pack->bits_cur = pack->shiftby = nxt;
		} else {
			a = in >> nxt;
			b = in & SWITCH_BITPACKED_MASKS[nxt];
			*pack->cur <<= rem;
			*pack->cur |= a;
			pack->cur++;
			*pack->cur |= b;
			pack->bits_cur = nxt;

		}
		pack->bytes++;

	} else {

		if (pack->mode == SWITCH_BITPACK_MODE_RFC3551) {
			in <<= pack->shiftby;
			*pack->cur |= in;
			pack->shiftby = (switch_byte_t) (pack->shiftby + pack->frame_bits);
		} else {
			*pack->cur <<= pack->frame_bits;
			*pack->cur |= in;
		}

		if (next == SWITCH_BITS_PER_BYTE) {
			pack->cur++;
			pack->bytes++;
			pack->bits_cur = pack->shiftby = 0;
		} else {
			pack->bits_cur = (switch_byte_t) next;
		}
	}

	return 0;
}

///\}

SWITCH_END_EXTERN_C
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
