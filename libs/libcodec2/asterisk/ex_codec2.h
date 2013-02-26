/*! \file
 * \brief 8-bit raw data
 *
 * Copyright (C) 2012, 2012 Ed W and David Rowe
 *
 * Distributed under the terms of the GNU General Public License
 *
 */

static uint8_t ex_codec2[] = {
    0x3e,0x06,0x4a,0xbb,0x9e,0x40,0xc0
};

static struct ast_frame *codec2_sample(void)
{
	static struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.subclass.codec = AST_FORMAT_CODEC2,
		.datalen = sizeof(ex_codec2),
		.samples = CODEC2_SAMPLES,
		.mallocd = 0,
		.offset = 0,
		.src = __PRETTY_FUNCTION__,
		.data.ptr = ex_codec2,
	};

	return &f;
}
