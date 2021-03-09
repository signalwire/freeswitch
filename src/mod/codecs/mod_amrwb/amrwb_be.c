/*
 * Copyright (c) 2016, Athonet (www.athonet.com)
 * Dragos Oancea  <dragos.oancea@athonet.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef __AMRWB_BE_H__
#include "bitshift.h"
#include "amrwb_be.h"

extern const int switch_amrwb_frame_sizes[];

/* Bandwidth Efficient AMR-WB */
/* https://tools.ietf.org/html/rfc4867#page-17 */

/* this works the same as in AMR NB*/
extern switch_bool_t switch_amrwb_pack_be(unsigned char *shift_buf, int n)
{
	uint8_t save_toc, ft;

	save_toc = shift_buf[1];

	/* we must convert OA TOC -> BE TOC */
	/* OA TOC
	0 1 2 3 4 5 6 7
	+-+-+-+-+-+-+-+-+
	|F|  FT   |Q|P1|P2|
	+-+-+-+-+-+-+-+-+
	F (1 bit): see definition in Section 4.3.2.

	FT (4 bits, unsigned integer): see definition in Section 4.3.2.

	Q (1 bit): see definition in Section 4.3.2.

	P bits: padding bits, MUST be set to zero, and MUST be ignored on reception.
	*/

	/* BE TOC:
	 0 1 2 3 4 5
	 +-+-+-+-+-+-+
	|F|  FT   |Q|
	+-+-+-+-+-+-+
	F = 0 , FT = XXXX , Q = 1
	eg: Frame Types (FT): ftp://www.3gpp.org/tsg_sa/TSG_SA/TSGS_04/Docs/PDF/SP-99253.pdf - table 1a
	*/

	ft = save_toc >> 3 ; /* drop Q, P1, P2  */
	ft &= ~(1 << 5); /* clear -  will mark just 1 frame - bit F */

	/* we only encode one frame, so bit 0 of TOC will be 0 */
	shift_buf[0] |= (ft >> 1); /* first 3 bits of FT */

	switch_amr_array_lshift(6, shift_buf+1, n);
	/*make sure we clear the bit - it will be used as padding of the trailing byte */
	shift_buf[1] |= 1 << 6; /* set bit Q instead of P1 */
	if (( ft >> 0 ) & 1) {
		/* set last bit of TOC instead of P2 */
		shift_buf[1] |= 1 << 7;
	} else {
		/* reset last bit of TOC instead of P2 */
		shift_buf[1] &= ~(1 << 7);
	}

	return SWITCH_TRUE;
}

extern switch_bool_t switch_amrwb_unpack_be(unsigned char *encoded_buf, uint8_t *tmp, int encoded_len)
{
	int framesz, index, ft;
	uint8_t shift_tocs[2] = {0x00, 0x00};
	uint8_t *shift_buf;

	memcpy(shift_tocs, encoded_buf, 2);
	/* shift for BE */
	switch_amr_array_lshift(4, shift_tocs, 2);
	ft = shift_tocs[0] >> 3;
	ft &= ~(1 << 5); /* Frame Type*/
	shift_buf = encoded_buf + 1; /* skip CMR */
	/* shift for BE */
	switch_amr_array_lshift(2, shift_buf, encoded_len - 1);
	/* get frame size */
	index = ((shift_tocs[0] >> 3) & 0x0f);
	if (index > 10 && index != 0xe && index != 0xf) {
		return SWITCH_FALSE;
	}
	framesz = switch_amrwb_frame_sizes[index];
	tmp[0] = shift_tocs[0]; /* save TOC */
	memcpy(&tmp[1], shift_buf, framesz);

	return SWITCH_TRUE;
}
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
