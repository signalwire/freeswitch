/* 
 * Mpeg Layer-1,2,3 audio decoder 
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 *
 * N->M down/up sampling. Not optimized for speed.
 */


#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"
#include "mpglib.h"

#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < -32768.0) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = (short)sum; }


//static unsigned long ntom_val[2] = { NTOM_MUL >> 1, NTOM_MUL >> 1 };
//static unsigned long ntom_step = NTOM_MUL;


int synth_ntom_set_step(struct mpstr *mp, long m, long n)
{
	if (param.verbose > 1)
		debug_printf("Init rate converter: %ld->%ld\n", m, n);

	if (n >= 96000 || m >= 96000 || m == 0 || n == 0) {
		debug_printf("NtoM converter: %d illegal rates\n", __LINE__);
		return (1);
	}

	n *= NTOM_MUL;
	mp->ntom_step = n / m;

	if (mp->ntom_step > 8 * NTOM_MUL) {
		debug_printf("%d max. 1:8 conversion allowed!\n", __LINE__);
		return (1);
	}

	mp->ntom_val[0] = mp->ntom_val[1] = NTOM_MUL >> 1;

	return (0);

}


int synth_ntom_mono(struct mpstr *mp, real * bandPtr, unsigned char *samples, int *pnt)
{
	short samples_tmp[8 * 64];
	short *tmp1 = samples_tmp;
	int i, ret;
	int pnt1 = 0;

	ret = synth_ntom(mp, bandPtr, 0, (unsigned char *) samples_tmp, &pnt1);
	samples += *pnt;

	for (i = 0; i < (pnt1 >> 2); i++) {
		*((short *) samples) = *tmp1;
		samples += 2;
		tmp1 += 2;
	}
	*pnt += pnt1 >> 1;

	return ret;
}



int synth_ntom(struct mpstr *mp, real * bandPtr, int channel, unsigned char *out, int *pnt)
{
	static const int step = 2;
	int bo;
	short *samples = (short *) (out + *pnt);

	real *b0, (*buf)[0x110];
	int clip = 0;
	int bo1;
	int ntom;

	bo = mp->synth_bo;

	if (!channel) {
		bo--;
		bo &= 0xf;
		buf = mp->synth_buffs[0];
		ntom = mp->ntom_val[1] = mp->ntom_val[0];
	} else {
		samples++;
		out += 2;				/* to compute the right *pnt value */
		buf = mp->synth_buffs[1];
		ntom = mp->ntom_val[1];
	}

	if (bo & 0x1) {
		b0 = buf[0];
		bo1 = bo;
		dct64(buf[1] + ((bo + 1) & 0xf), buf[0] + bo, bandPtr);
	} else {
		b0 = buf[1];
		bo1 = bo + 1;
		dct64(buf[0] + bo, buf[1] + bo + 1, bandPtr);
	}

	mp->synth_bo = bo;

	{
		register int j;
		real *window = (mp->decwin) + 16 - bo1;

		for (j = 16; j; j--, window += 0x10) {
			real sum;

			ntom += mp->ntom_step;
			if (ntom < NTOM_MUL) {
				window += 16;
				b0 += 16;
				continue;
			}

			sum = *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;
			sum += *window++ * *b0++;
			sum -= *window++ * *b0++;

			while (ntom >= NTOM_MUL) {
				WRITE_SAMPLE(samples, sum, clip);
				samples += step;
				ntom -= NTOM_MUL;
			}
		}

		ntom += mp->ntom_step;
		if (ntom >= NTOM_MUL) {
			real sum;
			sum = window[0x0] * b0[0x0];
			sum += window[0x2] * b0[0x2];
			sum += window[0x4] * b0[0x4];
			sum += window[0x6] * b0[0x6];
			sum += window[0x8] * b0[0x8];
			sum += window[0xA] * b0[0xA];
			sum += window[0xC] * b0[0xC];
			sum += window[0xE] * b0[0xE];

			while (ntom >= NTOM_MUL) {
				WRITE_SAMPLE(samples, sum, clip);
				samples += step;
				ntom -= NTOM_MUL;
			}
		}

		b0 -= 0x10, window -= 0x20;
		window += bo1 << 1;

		for (j = 15; j; j--, b0 -= 0x20, window -= 0x10) {
			real sum;

			ntom += mp->ntom_step;
			if (ntom < NTOM_MUL) {
				window -= 16;
				b0 += 16;
				continue;
			}

			sum = -*(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;
			sum -= *(--window) * *b0++;

			while (ntom >= NTOM_MUL) {
				WRITE_SAMPLE(samples, sum, clip);
				samples += step;
				ntom -= NTOM_MUL;
			}
		}
	}

	mp->ntom_val[channel] = ntom;
	*pnt = ((unsigned char *) samples - out);

	return clip;
}
