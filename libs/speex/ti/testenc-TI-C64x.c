/* Copyright (C) 2005 Psi Systems, Inc.
   Author:  Jean-Marc Valin 
   File: testenc-TI-C64x.c
   Encoder/Decoder Loop Main file for TI TMS320C64xx processor
   for use with TI Code Composer (TM) DSP development tools.
   Modified from speexlib/testenc.c


   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <speex/speex.h>
#include <stdio.h>
#include <stdlib.h>
#include <speex/speex_callbacks.h>

#undef DECODE_ONLY
#define CHECK_RESULT  /* Compares original file with encoded/decoder file */
#define TESTENC_BYTES_PER_FRAME 20  /* 8kbps */
#define TESTENC_QUALITY 4 /* 8kbps */
//#define TESTENC_BYTES_PER_FRAME 28  /* 11kbps */
//#define TESTENC_QUALITY 5	      /* 11 kbps */

/* For narrowband, QUALITY maps to these bit rates (see modes.c, manual.pdf) 
 *   {1, 8, 2, 3, 3, 4, 4, 5, 5, 6, 7}
 * 0 -> 2150
 * 1 -> 3950
 * 2 -> 5950
 * 3 -> 8000
 * 4 -> 8000
 * 5 -> 11000
 * 6 -> 11000
 * 7 -> 15000
 * 8 -> 15000
 * 9 -> 18200
 *10 -> 26400  */

#ifdef FIXED_DEBUG
extern long long spx_mips;
#endif

#ifdef MANUAL_ALLOC
/* Take all Speex space from this private heap */ 
/* This is useful for multichannel applications */
#pragma DATA_SECTION(spxHeap, ".myheap"); 
static char spxHeap[SPEEX_PERSIST_STACK_SIZE];

#pragma DATA_SECTION(spxScratch, ".myheap"); 
static char spxScratch[SPEEX_SCRATCH_STACK_SIZE];

char *spxGlobalHeapPtr, *spxGlobalHeapEnd;
char *spxGlobalScratchPtr, *spxGlobalScratchEnd;
#endif    /* MANUAL_ALLOC */

#include <math.h>
void main()
{
   char *outFile, *bitsFile;
   FILE *fout, *fbits=NULL;
#if !defined(DECODE_ONLY) || defined(CHECK_RESULT)
   char *inFile;
   FILE *fin;
   short in_short[FRAME_SIZE];
#endif
   short out_short[FRAME_SIZE];
#ifndef DECODE_ONLY
   int nbBits;
#endif
#ifdef CHECK_RESULT
   float sigpow,errpow,snr, seg_snr=0;
   int snr_frames = 0;
   int i;
#endif
   char cbits[200];
   void *st;
   void *dec;
   SpeexBits bits;
   spx_int32_t tmp;
   unsigned long bitCount=0;
   spx_int32_t skip_group_delay;
   SpeexCallback callback;

#ifdef CHECK_RESULT
   sigpow = 0;
   errpow = 0;
#endif

#ifdef MANUAL_ALLOC
	spxGlobalHeapPtr = spxHeap;
	spxGlobalHeapEnd = spxHeap + sizeof(spxHeap);

	spxGlobalScratchPtr = spxScratch;
	spxGlobalScratchEnd = spxScratch + sizeof(spxScratch);
#endif
   st = speex_encoder_init(&speex_nb_mode);
#ifdef MANUAL_ALLOC
	spxGlobalScratchPtr = spxScratch;		/* Reuse scratch for decoder */
#endif
   dec = speex_decoder_init(&speex_nb_mode);

   callback.callback_id = SPEEX_INBAND_CHAR;
   callback.func = speex_std_char_handler;
   callback.data = stderr;
   speex_decoder_ctl(dec, SPEEX_SET_HANDLER, &callback);

   callback.callback_id = SPEEX_INBAND_MODE_REQUEST;
   callback.func = speex_std_mode_request_handler;
   callback.data = st;
   speex_decoder_ctl(dec, SPEEX_SET_HANDLER, &callback);

   tmp=0;
   speex_decoder_ctl(dec, SPEEX_SET_ENH, &tmp);
   tmp=0;
   speex_encoder_ctl(st, SPEEX_SET_VBR, &tmp);
   tmp=TESTENC_QUALITY;
   speex_encoder_ctl(st, SPEEX_SET_QUALITY, &tmp);
   tmp=1;  /* Lowest */
   speex_encoder_ctl(st, SPEEX_SET_COMPLEXITY, &tmp);

#ifdef DISABLE_HIGHPASS
   /* Turn this off if you want to measure SNR (on by default) */
   tmp=0;
   speex_encoder_ctl(st, SPEEX_SET_HIGHPASS, &tmp);
   speex_decoder_ctl(dec, SPEEX_SET_HIGHPASS, &tmp);
#endif

   speex_encoder_ctl(st, SPEEX_GET_LOOKAHEAD, &skip_group_delay);
   speex_decoder_ctl(dec, SPEEX_GET_LOOKAHEAD, &tmp);
   skip_group_delay += tmp;
   fprintf (stderr, "decoder lookahead = %d\n", skip_group_delay);

#ifdef DECODE_ONLY
   bitsFile = "c:\\speextrunktest\\samples\\malebitsin.dat";
   fbits = fopen(bitsFile, "rb");
#else
   bitsFile = "c:\\speextrunktest\\samples\\malebits6x.dat";
   fbits = fopen(bitsFile, "wb");
#endif
#if !defined(DECODE_ONLY) || defined(CHECK_RESULT)
   inFile = "c:\\speextrunktest\\samples\\male.snd";
   fin = fopen(inFile, "rb");
#endif
   outFile = "c:\\speextrunktest\\samples\\maleout6x.snd";
   fout = fopen(outFile, "wb+");
 
   speex_bits_init(&bits);
#ifndef DECODE_ONLY
   while (!feof(fin))
   {
      fread(in_short, sizeof(short), FRAME_SIZE, fin);
      if (feof(fin))
         break;
      speex_bits_reset(&bits);

      speex_encode_int(st, in_short, &bits);
      nbBits = speex_bits_write(&bits, cbits, 200);
      bitCount+=bits.nbBits;

      fwrite(cbits, 1, nbBits, fbits);
      speex_bits_rewind(&bits);

#else /* DECODE_ONLY */
   while (!feof(fbits))
   {
      fread(cbits, 1, TESTENC_BYTES_PER_FRAME, fbits);

      if (feof(fbits))
         break;

      speex_bits_read_from(&bits, cbits, TESTENC_BYTES_PER_FRAME);
//      bitCount+=160;  /* only correct for 8kbps, but just for the printf */
      bitCount+=bits.nbBits;
#endif
      
      speex_decode_int(dec, &bits, out_short);
      speex_bits_reset(&bits);

      fwrite(&out_short[skip_group_delay], sizeof(short), FRAME_SIZE-skip_group_delay, fout);
      skip_group_delay = 0;
#if 1
   fprintf (stderr, "Bits so far: %lu \n", bitCount);
#endif
   }
   fprintf (stderr, "Total encoded size: %lu bits\n", bitCount);
   speex_encoder_destroy(st);
   speex_decoder_destroy(dec);

#ifdef CHECK_RESULT 
   rewind(fin);
   rewind(fout);

   while ( FRAME_SIZE == fread(in_short, sizeof(short), FRAME_SIZE, fin) 
           &&
           FRAME_SIZE ==  fread(out_short, sizeof(short), FRAME_SIZE,fout) )
   {
	float s=0, e=0;
        for (i=0;i<FRAME_SIZE;++i) {
            s += (float)in_short[i] * in_short[i];
            e += ((float)in_short[i]-out_short[i]) * ((float)in_short[i]-out_short[i]);
        }
	seg_snr += 10*log10((s+160)/(e+160));
	sigpow += s;
	errpow += e;
	snr_frames++;
   }

   snr = 10 * log10( sigpow / errpow );
   seg_snr /= snr_frames;
   fprintf(stderr,"SNR = %f\nsegmental SNR = %f\n",snr, seg_snr);

#ifdef FIXED_DEBUG
   printf ("Total: %f MIPS\n", (float)(1e-6*50*spx_mips/snr_frames));
#endif
#endif /* CHECK_RESULT */
#if !defined(DECODE_ONLY) || defined(CHECK_RESULT)
   fclose(fin);
#endif
   fclose(fout);
   fclose(fbits);
}
