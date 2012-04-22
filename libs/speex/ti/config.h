/* Copyright (C) 2005 Psi Systems, Inc.
   File: config.h
   Main Speex option include file for TI C64xx, C54xx and C55xx processors
   for use with TI Code Composer (TM) DSP development tools.

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

#define FIXED_POINT
#define FRAME_SIZE 160
#define DISABLE_WIDEBAND

/* Disable DC block if doing SNR testing */
#define DISABLE_HIGHPASS 

/* Allow for 2 20ms narrowband blocks per frame, plus a couple of bytes */
#define MAX_CHARS_PER_FRAME (42/BYTES_PER_CHAR)

/* for debug */
#undef DECODE_ONLY
#define VERBOSE_ALLOC

/* EITHER    Allocate from fixed array (C heap not used) */
/*           Enable VERBOSE_ALLOC to see how much is used */
#define MANUAL_ALLOC
#define OS_SUPPORT_CUSTOM

/* OR        Use CALLOC (heap size must be increased in linker command file) */
//#undef MANUAL_ALLOC
//#undef OS_SUPPORT_CUSTOM

#if defined (CONFIG_TI_C54X) || defined (CONFIG_TI_C55X) 
//#define PRECISION16

// These values determined by analysis for 8kbps narrowband
#define SPEEXENC_PERSIST_STACK_SIZE 1000
#define SPEEXENC_SCRATCH_STACK_SIZE 3000
#define SPEEXDEC_PERSIST_STACK_SIZE 1000
#define SPEEXDEC_SCRATCH_STACK_SIZE 1000
#else /* C6X */
#define NO_LONGLONG

#define SPEEXENC_PERSIST_STACK_SIZE 2000
#define SPEEXENC_SCRATCH_STACK_SIZE 6000
#define SPEEXDEC_PERSIST_STACK_SIZE 2000
#define SPEEXDEC_SCRATCH_STACK_SIZE 2000
#endif
#define SPEEX_PERSIST_STACK_SIZE (SPEEXENC_PERSIST_STACK_SIZE + SPEEXDEC_PERSIST_STACK_SIZE)
#define SPEEX_SCRATCH_STACK_SIZE SPEEXENC_SCRATCH_STACK_SIZE
#define NB_ENC_STACK SPEEXENC_SCRATCH_STACK_SIZE
#define NB_DEC_STACK SPEEXDEC_SCRATCH_STACK_SIZE

