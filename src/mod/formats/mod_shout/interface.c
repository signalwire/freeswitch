
#include <stdlib.h>
#include <stdio.h>

#include "mpg123.h"
#include "mpglib.h"

void InitMP3Constants(void)
{
	init_layer3_const();
	make_decode_tables_const();

}


BOOL InitMP3(struct mpstr *mp, long outscale, int samplerate)
{
	/* quiet 4096 med 8192 */

	memset(mp, 0, sizeof(struct mpstr));

	mp->framesize = 0;
	mp->fsizeold = -1;
	mp->bsize = 0;
	mp->head = mp->tail = NULL;
	mp->fr.single = 3;			/* force mono */
	mp->bsnum = 0;
	mp->synth_bo = 1;
	mp->outsamplerate = samplerate;
	mp->ntom_val[0] = NTOM_MUL >> 1;
	mp->ntom_val[1] = NTOM_MUL >> 1;
	mp->ntom_step = NTOM_MUL;

	make_decode_tables_scale(mp, outscale);

	init_layer3_sample_limits(mp, SBLIMIT);

	return !0;
}

void ExitMP3(struct mpstr *mp)
{
	struct buf *b, *bn;

	b = mp->tail;
	while (b) {
		free(b->pnt);
		bn = b->next;
		free(b);
		b = bn;
	}
}

static struct buf *addbuf(struct mpstr *mp, char *buf, int size)
{
	struct buf *nbuf;

	nbuf = malloc(sizeof(struct buf));
	if (!nbuf) {
		debug_printf("%d Out of memory!\n", __LINE__);
		return NULL;
	}
	nbuf->pnt = malloc(size);
	if (!nbuf->pnt) {
		free(nbuf);
		return NULL;
	}
	nbuf->size = size;
	memcpy(nbuf->pnt, buf, size);
	nbuf->next = NULL;
	nbuf->prev = mp->head;
	nbuf->pos = 0;

	if (!mp->tail) {
		mp->tail = nbuf;
	} else {
		mp->head->next = nbuf;
	}

	mp->head = nbuf;
	mp->bsize += size;

	return nbuf;
}

static void remove_buf(struct mpstr *mp)
{
	struct buf *buf = mp->tail;

	mp->tail = buf->next;
	if (mp->tail)
		mp->tail->prev = NULL;
	else {
		mp->tail = mp->head = NULL;
	}

	free(buf->pnt);
	free(buf);

}

static int read_buf_byte(int *error, struct mpstr *mp)
{
	unsigned int b;
	int pos;

	pos = mp->tail->pos;
	while (pos >= mp->tail->size) {
		remove_buf(mp);
		pos = mp->tail->pos;
		if (!mp->tail) {
			/* We may pick up this error a few times */
			/* But things have gone pear shaped */
			debug_printf("%d Fatal Buffer error!\n", __LINE__);
			*error = 1;
			return (0);
		}
	}

	b = mp->tail->pnt[pos];
	mp->bsize--;
	mp->tail->pos++;


	return b;
}

static int read_head(struct mpstr *mp)
{
	unsigned long head;
	int error = 0;


	head = read_buf_byte(&error, mp);
	head <<= 8;
	head |= read_buf_byte(&error, mp);
	head <<= 8;
	head |= read_buf_byte(&error, mp);
	head <<= 8;
	head |= read_buf_byte(&error, mp);

	mp->header = head;

	if (error) {
		return (1);
	} else
		return (0);

}

static int head_check(unsigned long head)
{
	if ((head & 0xffe00000) != 0xffe00000)
		return FALSE;
	if (!((head >> 17) & 3))
		return FALSE;
	if (((head >> 12) & 0xf) == 0xf || ((head >> 12) & 0xf) == 0)
		return FALSE;
	if (((head >> 10) & 0x3) == 0x3)
		return FALSE;
	if ((head & 0xffff0000) == 0xfffe0000)
		return FALSE;

	return TRUE;
}

static int head_shift(struct mpstr *mp)
{
	unsigned long head;
	int error = 0;

	head = mp->header;

	head <<= 8;
	head |= read_buf_byte(&error, mp);

	mp->header = head;

	if (error) {
		return (1);
	} else
		return (0);

}


int decodeMP3(struct mpstr *mp, char *in, int isize, char *out, int osize, int *done)
{
	int len;
	long n, m;
	int down_sample_sblimit;

	if (osize < 4608) {
		debug_printf("%d To less out space\n", __LINE__);
		return MP3_TOOSMALL;
	}

	if (in) {
		if (addbuf(mp, in, isize) == NULL) {
			return MP3_ERR;
		}
	}

	/* First decode header */
	if (mp->framesize == 0) {
		if (mp->bsize < 4) {
			return MP3_NEED_MORE;
		}
		if (read_head(mp))
			return MP3_ERR;

		if (!head_check(mp->header)) {
			int i;

			debug_printf("Junk at the beginning of frame %08lx\n", mp->header);

			/* step in byte steps through next 64K */
			for (i = 0; i < 65536; i++) {
				if (!mp->bsize)
					return MP3_NEED_MORE;

				if (head_shift(mp))
					return MP3_ERR;

				if (head_check(mp->header))
					break;
			}
			if (i == 65536) {
				debug_printf("%d Giving up searching valid MPEG header\n", __LINE__);
				return MP3_ERR;
			}
		}

		decode_header(&mp->fr, mp->header);
		mp->framesize = mp->fr.framesize;

		if (!mp->initmp3) {
			mp->initmp3 = 1;

			n = freqs[mp->fr.sampling_frequency];
			if (mp->outsamplerate) {
				m = mp->outsamplerate;
			} else {
				m = n;
			}

			if (synth_ntom_set_step(mp, n, m))
				return MP3_ERR;


			if (n > m) {
				down_sample_sblimit = SBLIMIT * m;
				down_sample_sblimit /= n;
			} else {
				down_sample_sblimit = SBLIMIT;
			}

			init_layer3_sample_limits(mp, down_sample_sblimit);

		}
	}


	if (mp->fr.framesize > mp->bsize)
		return MP3_NEED_MORE;

	(mp->worksample).wordpointer = mp->bsspace[mp->bsnum] + 512;
	mp->bsnum = (mp->bsnum + 1) & 0x1;
	(mp->worksample).bitindex = 0;

	len = 0;
	while (len < mp->framesize) {
		int nlen;
		int blen = mp->tail->size - mp->tail->pos;
		if ((mp->framesize - len) <= blen) {
			nlen = mp->framesize - len;
		} else {
			nlen = blen;
		}
		memcpy((mp->worksample).wordpointer + len, mp->tail->pnt + mp->tail->pos, nlen);
		len += nlen;
		mp->tail->pos += nlen;
		mp->bsize -= nlen;
		if (mp->tail->pos == mp->tail->size) {
			remove_buf(mp);
		}
	}

	*done = 0;
	if (mp->fr.error_protection)
		getbits(mp, 16);

	if (do_layer3(mp, (unsigned char *) out, done))
		return MP3_ERR;

	mp->fsizeold = mp->framesize;
	mp->framesize = 0;

	return MP3_OK;
}

int set_pointer(struct mpstr *mp, long backstep)
{
	unsigned char *bsbufold;
	if (mp->fsizeold < 0 && backstep > 0) {
		debug_printf("Can't step back %ld!\n", backstep);
		return MP3_ERR;
	}
	bsbufold = mp->bsspace[mp->bsnum] + 512;
	(mp->worksample).wordpointer -= backstep;
	if (backstep)
		memcpy((mp->worksample).wordpointer, bsbufold + mp->fsizeold - backstep, backstep);
	(mp->worksample).bitindex = 0;
	return MP3_OK;
}
