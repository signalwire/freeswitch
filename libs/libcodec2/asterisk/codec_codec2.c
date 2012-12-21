/*
 * Codec 2 module for Asterisk.
 *
 * Credit: codec_gsm.c used as a starting point.
 *
 * Copyright (C) 2012 Ed W and David Rowe
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Translate between signed linear and Codec 2
 *
 * \ingroup codecs
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/translate.h"
#include "asterisk/config.h"
#include "asterisk/module.h"
#include "asterisk/utils.h"

#include <codec2.h>

#define BUFFER_SAMPLES	  8000
#define CODEC2_SAMPLES    160
#define	CODEC2_FRAME_LEN  7

/* Sample frame data */

#include "asterisk/slin.h"
#include "ex_codec2.h"

struct codec2_translator_pvt {	        /* both codec2tolin and codec2togsm */
    struct CODEC2 *codec2;
    short  buf[BUFFER_SAMPLES];	/* lintocodec2, temporary storage */
};

static int codec2_new(struct ast_trans_pvt *pvt)
{
    struct codec2_translator_pvt *tmp = pvt->pvt;

    tmp->codec2 = codec2_create(CODEC2_MODE_2400);
	
    return 0;
}

/*! \brief decode and store in outbuf. */
static int codec2tolin_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
    struct codec2_translator_pvt *tmp = pvt->pvt;
    int x;
    int16_t *dst = pvt->outbuf.i16;
    int flen = CODEC2_FRAME_LEN;

    for (x=0; x < f->datalen; x += flen) {
	unsigned char *src;
	int len;
	len = CODEC2_SAMPLES;
	src = f->data.ptr + x;

	codec2_decode(tmp->codec2, dst + pvt->samples, src);

	pvt->samples += CODEC2_SAMPLES;
	pvt->datalen += 2 * CODEC2_SAMPLES;
    }
    return 0;
}

/*! \brief store samples into working buffer for later decode */
static int lintocodec2_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct codec2_translator_pvt *tmp = pvt->pvt;

	if (pvt->samples + f->samples > BUFFER_SAMPLES) {
		ast_log(LOG_WARNING, "Out of buffer space\n");
		return -1;
	}
	memcpy(tmp->buf + pvt->samples, f->data.ptr, f->datalen);
	pvt->samples += f->samples;
	return 0;
}

/*! \brief encode and produce a frame */
static struct ast_frame *lintocodec2_frameout(struct ast_trans_pvt *pvt)
{
	struct codec2_translator_pvt *tmp = pvt->pvt;
	int datalen = 0;
	int samples = 0;

	/* We can't work on anything less than a frame in size */
	if (pvt->samples < CODEC2_SAMPLES)
		return NULL;
	while (pvt->samples >= CODEC2_SAMPLES) {
	    /* Encode a frame of data */
	    codec2_encode(tmp->codec2, (unsigned char*)(pvt->outbuf.c + datalen), tmp->buf + samples);
	    datalen += CODEC2_FRAME_LEN;
	    samples += CODEC2_SAMPLES;
	    pvt->samples -= CODEC2_SAMPLES;
	}

	/* Move the data at the end of the buffer to the front */
	if (pvt->samples)
		memmove(tmp->buf, tmp->buf + samples, pvt->samples * 2);

	return ast_trans_frameout(pvt, datalen, samples);
}

static void codec2_destroy_stuff(struct ast_trans_pvt *pvt)
{
	struct codec2_translator_pvt *tmp = pvt->pvt;
	if (tmp->codec2)
		codec2_destroy(tmp->codec2);
}

static struct ast_translator codec2tolin = {
	.name = "codec2tolin", 
	.srcfmt = AST_FORMAT_CODEC2,
	.dstfmt = AST_FORMAT_SLINEAR,
	.newpvt = codec2_new,
	.framein = codec2tolin_framein,
	.destroy = codec2_destroy_stuff,
	.sample = codec2_sample,
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.desc_size = sizeof (struct codec2_translator_pvt ),
};

static struct ast_translator lintocodec2 = {
	.name = "lintocodec2", 
	.srcfmt = AST_FORMAT_SLINEAR,
	.dstfmt = AST_FORMAT_CODEC2,
	.newpvt = codec2_new,
	.framein = lintocodec2_framein,
	.frameout = lintocodec2_frameout,
	.destroy = codec2_destroy_stuff,
	.sample = slin8_sample,
	.desc_size = sizeof (struct codec2_translator_pvt ),
	.buf_size = (BUFFER_SAMPLES * CODEC2_FRAME_LEN + CODEC2_SAMPLES - 1)/CODEC2_SAMPLES,
};

/*! \brief standard module glue */
static int reload(void)
{
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_translator(&lintocodec2);
	if (!res)
		res = ast_unregister_translator(&codec2tolin);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_translator(&codec2tolin);
	if (!res) 
		res=ast_register_translator(&lintocodec2);
	else
		ast_unregister_translator(&codec2tolin);
	if (res) 
		return AST_MODULE_LOAD_FAILURE;
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Codec 2 Coder/Decoder",
		.load = load_module,
		.unload = unload_module,
		.reload = reload,
	       );
