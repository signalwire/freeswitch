/* Copyright (C) 2003-2006 Jean-Marc Valin

   File: mdf.c
   Echo canceller based on the MDF algorithm (see below)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

/*
   The echo canceller is based on the MDF algorithm described in:

   J. S. Soo, K. K. Pang Multidelay block frequency adaptive filter, 
   IEEE Trans. Acoust. Speech Signal Process., Vol. ASSP-38, No. 2, 
   February 1990.
   
   We use the Alternatively Updated MDF (AUMDF) variant. Robustness to 
   double-talk is achieved using a variable learning rate as described in:
   
   Valin, J.-M., On Adjusting the Learning Rate in Frequency Domain Echo 
   Cancellation With Double-Talk. To appear in IEEE Transactions on Audio,
   Speech and Language Processing, 2006.
   http://people.xiph.org/~jm/papers/valin_taslp2006.pdf
   
   There is no explicit double-talk detection, but a continuous variation
   in the learning rate based on residual echo, double-talk and background
   noise.
   
   About the fixed-point version:
   All the signals are represented with 16-bit words. The filter weights 
   are represented with 32-bit words, but only the top 16 bits are used
   in most cases. The lower 16 bits are completely unreliable (due to the
   fact that the update is done only on the top bits), but help in the
   adaptation -- probably by removing a "threshold effect" due to
   quantization (rounding going to zero) when the gradient is small.
   
   Another kludge that seems to work good: when performing the weight
   update, we only move half the way toward the "goal" this seems to
   reduce the effect of quantization noise in the update phase. This
   can be seen as applying a gradient descent on a "soft constraint"
   instead of having a hard constraint.
   
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "misc.h"
#include "speex/speex_echo.h"
#include "fftwrap.h"
#include "pseudofloat.h"
#include "math_approx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))

#ifdef FIXED_POINT
#define WEIGHT_SHIFT 11
#define NORMALIZE_SCALEDOWN 5
#define NORMALIZE_SCALEUP 3
#else
#define WEIGHT_SHIFT 0
#endif

/* If enabled, the transition between blocks is smooth, so there isn't any blocking
aftifact when adapting. The cost is an extra FFT and a matrix-vector multiply */
#define SMOOTH_BLOCKS

#ifdef FIXED_POINT
static const spx_float_t MIN_LEAK = {16777, -19};
#define TOP16(x) ((x)>>16)
#else
static const spx_float_t MIN_LEAK = .032f;
#define TOP16(x) (x)
#endif


/** Speex echo cancellation state. */
struct SpeexEchoState_ {
   int frame_size;           /**< Number of samples processed each time */
   int window_size;
   int M;
   int cancel_count;
   int adapted;
   int saturated;
   spx_int32_t sampling_rate;
   spx_word16_t spec_average;
   spx_word16_t beta0;
   spx_word16_t beta_max;
   spx_word32_t sum_adapt;
   spx_word16_t *e;
   spx_word16_t *x;
   spx_word16_t *X;
   spx_word16_t *d;
   spx_word16_t *y;
   spx_word16_t *last_y;
   spx_word32_t *Yps;
   spx_word16_t *Y;
   spx_word16_t *E;
   spx_word32_t *PHI;
   spx_word32_t *W;
   spx_word32_t *power;
   spx_float_t *power_1;
   spx_word16_t *wtmp;
#ifdef FIXED_POINT
   spx_word16_t *wtmp2;
#endif
   spx_word32_t *Rf;
   spx_word32_t *Yf;
   spx_word32_t *Xf;
   spx_word32_t *Eh;
   spx_word32_t *Yh;
   spx_float_t Pey;
   spx_float_t Pyy;
   spx_word16_t *window;
   spx_word16_t *prop;
   void *fft_table;
   spx_word16_t memX, memD, memE;
   spx_word16_t preemph;
   spx_word16_t notch_radius;
   spx_mem_t notch_mem[2];

   /* NOTE: If you only use speex_echo_cancel() and want to save some memory, remove this */
   spx_int16_t *play_buf;
   int play_buf_pos;
};

static inline void filter_dc_notch16(const spx_int16_t *in, spx_word16_t radius, spx_word16_t *out, int len, spx_mem_t *mem)
{
   int i;
   spx_word16_t den2;
#ifdef FIXED_POINT
   den2 = MULT16_16_Q15(radius,radius) + MULT16_16_Q15(QCONST16(.7,15),MULT16_16_Q15(32767-radius,32767-radius));
#else
   den2 = radius*radius + .7*(1-radius)*(1-radius);
#endif   
   /*printf ("%d %d %d %d %d %d\n", num[0], num[1], num[2], den[0], den[1], den[2]);*/
   for (i=0;i<len;i++)
   {
      spx_word16_t vin = in[i];
      spx_word32_t vout = mem[0] + SHL32(EXTEND32(vin),15);
#ifdef FIXED_POINT
      mem[0] = mem[1] + SHL32(SHL32(-EXTEND32(vin),15) + MULT16_32_Q15(radius,vout),1);
#else
      mem[0] = mem[1] + 2*(-vin + radius*vout);
#endif
      mem[1] = SHL32(EXTEND32(vin),15) - MULT16_32_Q15(den2,vout);
      out[i] = SATURATE32(PSHR32(MULT16_32_Q15(radius,vout),15),32767);
   }
}

static inline spx_word32_t mdf_inner_prod(const spx_word16_t *x, const spx_word16_t *y, int len)
{
   spx_word32_t sum=0;
   len >>= 1;
   while(len--)
   {
      spx_word32_t part=0;
      part = MAC16_16(part,*x++,*y++);
      part = MAC16_16(part,*x++,*y++);
      /* HINT: If you had a 40-bit accumulator, you could shift only at the end */
      sum = ADD32(sum,SHR32(part,6));
   }
   return sum;
}

/** Compute power spectrum of a half-complex (packed) vector */
static inline void power_spectrum(const spx_word16_t *X, spx_word32_t *ps, int N)
{
   int i, j;
   ps[0]=MULT16_16(X[0],X[0]);
   for (i=1,j=1;i<N-1;i+=2,j++)
   {
      ps[j] =  MULT16_16(X[i],X[i]) + MULT16_16(X[i+1],X[i+1]);
   }
   ps[j]=MULT16_16(X[i],X[i]);
}

/** Compute cross-power spectrum of a half-complex (packed) vectors and add to acc */
#ifdef FIXED_POINT
static inline void spectral_mul_accum(const spx_word16_t *X, const spx_word32_t *Y, spx_word16_t *acc, int N, int M)
{
   int i,j;
   spx_word32_t tmp1=0,tmp2=0;
   for (j=0;j<M;j++)
   {
      tmp1 = MAC16_16(tmp1, X[j*N],TOP16(Y[j*N]));
   }
   acc[0] = PSHR32(tmp1,WEIGHT_SHIFT);
   for (i=1;i<N-1;i+=2)
   {
      tmp1 = tmp2 = 0;
      for (j=0;j<M;j++)
      {
         tmp1 = SUB32(MAC16_16(tmp1, X[j*N+i],TOP16(Y[j*N+i])), MULT16_16(X[j*N+i+1],TOP16(Y[j*N+i+1])));
         tmp2 = MAC16_16(MAC16_16(tmp2, X[j*N+i+1],TOP16(Y[j*N+i])), X[j*N+i], TOP16(Y[j*N+i+1]));
      }
      acc[i] = PSHR32(tmp1,WEIGHT_SHIFT);
      acc[i+1] = PSHR32(tmp2,WEIGHT_SHIFT);
   }
   tmp1 = tmp2 = 0;
   for (j=0;j<M;j++)
   {
      tmp1 = MAC16_16(tmp1, X[(j+1)*N-1],TOP16(Y[(j+1)*N-1]));
   }
   acc[N-1] = PSHR32(tmp1,WEIGHT_SHIFT);
}
#else
static inline void spectral_mul_accum(const spx_word16_t *X, const spx_word32_t *Y, spx_word16_t *acc, int N, int M)
{
   int i,j;
   for (i=0;i<N;i++)
      acc[i] = 0;
   for (j=0;j<M;j++)
   {
      acc[0] += X[0]*Y[0];
      for (i=1;i<N-1;i+=2)
      {
         acc[i] += (X[i]*Y[i] - X[i+1]*Y[i+1]);
         acc[i+1] += (X[i+1]*Y[i] + X[i]*Y[i+1]);
      }
      acc[i] += X[i]*Y[i];
      X += N;
      Y += N;
   }
}
#endif

/** Compute weighted cross-power spectrum of a half-complex (packed) vector with conjugate */
static inline void weighted_spectral_mul_conj(const spx_float_t *w, const spx_word16_t *X, const spx_word16_t *Y, spx_word32_t *prod, int N)
{
   int i, j;
   prod[0] = FLOAT_MUL32(w[0],MULT16_16(X[0],Y[0]));
   for (i=1,j=1;i<N-1;i+=2,j++)
   {
      prod[i] = FLOAT_MUL32(w[j],MAC16_16(MULT16_16(X[i],Y[i]), X[i+1],Y[i+1]));
      prod[i+1] = FLOAT_MUL32(w[j],MAC16_16(MULT16_16(-X[i+1],Y[i]), X[i],Y[i+1]));
   }
   prod[i] = FLOAT_MUL32(w[j],MULT16_16(X[i],Y[i]));
}


/** Creates a new echo canceller state */
SpeexEchoState *speex_echo_state_init(int frame_size, int filter_length)
{
   int i,N,M;
   SpeexEchoState *st = (SpeexEchoState *)speex_alloc(sizeof(SpeexEchoState));

   st->frame_size = frame_size;
   st->window_size = 2*frame_size;
   N = st->window_size;
   M = st->M = (filter_length+st->frame_size-1)/frame_size;
   st->cancel_count=0;
   st->sum_adapt = 0;
   st->saturated = 0;
   /* FIXME: Make that an init option (new API call?) */
   st->sampling_rate = 8000;
   st->spec_average = DIV32_16(SHL32(EXTEND32(st->frame_size), 15), st->sampling_rate);
#ifdef FIXED_POINT
   st->beta0 = DIV32_16(SHL32(EXTEND32(st->frame_size), 16), st->sampling_rate);
   st->beta_max = DIV32_16(SHL32(EXTEND32(st->frame_size), 14), st->sampling_rate);
#else
   st->beta0 = (2.0f*st->frame_size)/st->sampling_rate;
   st->beta_max = (.5f*st->frame_size)/st->sampling_rate;
#endif

   st->fft_table = spx_fft_init(N);
   
   st->e = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->x = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->d = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->y = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->Yps = (spx_word32_t*)speex_alloc(N*sizeof(spx_word32_t));
   st->last_y = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->Yf = (spx_word32_t*)speex_alloc((st->frame_size+1)*sizeof(spx_word32_t));
   st->Rf = (spx_word32_t*)speex_alloc((st->frame_size+1)*sizeof(spx_word32_t));
   st->Xf = (spx_word32_t*)speex_alloc((st->frame_size+1)*sizeof(spx_word32_t));
   st->Yh = (spx_word32_t*)speex_alloc((st->frame_size+1)*sizeof(spx_word32_t));
   st->Eh = (spx_word32_t*)speex_alloc((st->frame_size+1)*sizeof(spx_word32_t));

   st->X = (spx_word16_t*)speex_alloc((M+1)*N*sizeof(spx_word16_t));
   st->Y = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->E = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->W = (spx_word32_t*)speex_alloc(M*N*sizeof(spx_word32_t));
   st->PHI = (spx_word32_t*)speex_alloc(N*sizeof(spx_word32_t));
   st->power = (spx_word32_t*)speex_alloc((frame_size+1)*sizeof(spx_word32_t));
   st->power_1 = (spx_float_t*)speex_alloc((frame_size+1)*sizeof(spx_float_t));
   st->window = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   st->prop = (spx_word16_t*)speex_alloc(M*sizeof(spx_word16_t));
   st->wtmp = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
#ifdef FIXED_POINT
   st->wtmp2 = (spx_word16_t*)speex_alloc(N*sizeof(spx_word16_t));
   for (i=0;i<N>>1;i++)
   {
      st->window[i] = (16383-SHL16(spx_cos(DIV32_16(MULT16_16(25736,i<<1),N)),1));
      st->window[N-i-1] = st->window[i];
   }
#else
   for (i=0;i<N;i++)
      st->window[i] = .5-.5*cos(2*M_PI*i/N);
#endif
   for (i=0;i<=st->frame_size;i++)
      st->power_1[i] = FLOAT_ONE;
   for (i=0;i<N*M;i++)
      st->W[i] = 0;
   for (i=0;i<N;i++)
      st->PHI[i] = 0;
   {
      spx_word32_t sum = 0;
      /* Ratio of ~10 between adaptation rate of first and last block */
      spx_word16_t decay = QCONST16(exp(-2.4/M),15);
      st->prop[0] = QCONST16(.7, 15);
      sum = EXTEND32(st->prop[0]);
      for (i=1;i<M;i++)
      {
         st->prop[i] = MULT16_16_Q15(st->prop[i-1], decay);
         sum = ADD32(sum, EXTEND32(st->prop[i]));
      }
      for (i=M-1;i>=0;i--)
      {
         st->prop[i] = DIV32(MULT16_16(QCONST16(.8,15), st->prop[i]),sum);
      }
   }
   
   st->memX=st->memD=st->memE=0;
   st->preemph = QCONST16(.9,15);
   if (st->sampling_rate<12000)
      st->notch_radius = QCONST16(.9, 15);
   else if (st->sampling_rate<24000)
      st->notch_radius = QCONST16(.982, 15);
   else
      st->notch_radius = QCONST16(.992, 15);

   st->notch_mem[0] = st->notch_mem[1] = 0;
   st->adapted = 0;
   st->Pey = st->Pyy = FLOAT_ONE;
   
   st->play_buf = (spx_int16_t*)speex_alloc(2*st->frame_size*sizeof(spx_int16_t));
   st->play_buf_pos = 0;

   return st;
}

/** Resets echo canceller state */
void speex_echo_state_reset(SpeexEchoState *st)
{
   int i, M, N;
   st->cancel_count=0;
   N = st->window_size;
   M = st->M;
   for (i=0;i<N*M;i++)
      st->W[i] = 0;
   for (i=0;i<N*(M+1);i++)
      st->X[i] = 0;
   for (i=0;i<=st->frame_size;i++)
      st->power[i] = 0;
   for (i=0;i<N;i++)
      st->E[i] = 0;
   st->notch_mem[0] = st->notch_mem[1] = 0;
  
   st->saturated = 0;
   st->adapted = 0;
   st->sum_adapt = 0;
   st->Pey = st->Pyy = FLOAT_ONE;
   st->play_buf_pos = 0;

}

/** Destroys an echo canceller state */
void speex_echo_state_destroy(SpeexEchoState *st)
{
   spx_fft_destroy(st->fft_table);

   speex_free(st->e);
   speex_free(st->x);
   speex_free(st->d);
   speex_free(st->y);
   speex_free(st->last_y);
   speex_free(st->Yps);
   speex_free(st->Yf);
   speex_free(st->Rf);
   speex_free(st->Xf);
   speex_free(st->Yh);
   speex_free(st->Eh);

   speex_free(st->X);
   speex_free(st->Y);
   speex_free(st->E);
   speex_free(st->W);
   speex_free(st->PHI);
   speex_free(st->power);
   speex_free(st->power_1);
   speex_free(st->window);
   speex_free(st->prop);
   speex_free(st->wtmp);
#ifdef FIXED_POINT
   speex_free(st->wtmp2);
#endif
   speex_free(st->play_buf);
   speex_free(st);
}

void speex_echo_capture(SpeexEchoState *st, const spx_int16_t *rec, spx_int16_t *out, spx_int32_t *Yout)
{
   int i;
   if (st->play_buf_pos>=st->frame_size)
   {
      speex_echo_cancel(st, rec, st->play_buf, out, Yout);
      st->play_buf_pos -= st->frame_size;
      for (i=0;i<st->frame_size;i++)
         st->play_buf[i] = st->play_buf[i+st->frame_size];
   } else {
      speex_warning("no playback frame available");
      if (st->play_buf_pos!=0)
      {
         speex_warning("internal playback buffer corruption?");
         st->play_buf_pos = 0;
      }
      for (i=0;i<st->frame_size;i++)
         out[i] = rec[i];
   }
}

void speex_echo_playback(SpeexEchoState *st, const spx_int16_t *play)
{
   if (st->play_buf_pos<=st->frame_size)
   {
      int i;
      for (i=0;i<st->frame_size;i++)
         st->play_buf[st->play_buf_pos+i] = play[i];
      st->play_buf_pos += st->frame_size;
   } else {
      speex_warning("had to discard a playback frame");
   }
}

/** Performs echo cancellation on a frame */
void speex_echo_cancel(SpeexEchoState *st, const spx_int16_t *ref, const spx_int16_t *echo, spx_int16_t *out, spx_int32_t *Yout)
{
   int i,j;
   int N,M;
   spx_word32_t Syy,See,Sxx;
   spx_word32_t Sey;
   spx_word16_t leak_estimate;
   spx_word16_t ss, ss_1;
   spx_float_t Pey = FLOAT_ONE, Pyy=FLOAT_ONE;
   spx_float_t alpha, alpha_1;
   spx_word16_t RER;
   spx_word32_t tmp32;
   
   N = st->window_size;
   M = st->M;
   st->cancel_count++;
#ifdef FIXED_POINT
   ss=DIV32_16(11469,M);
   ss_1 = SUB16(32767,ss);
#else
   ss=.35/M;
   ss_1 = 1-ss;
#endif

   filter_dc_notch16(ref, st->notch_radius, st->d, st->frame_size, st->notch_mem);
   /* Copy input data to buffer */
   for (i=0;i<st->frame_size;i++)
   {
      spx_word16_t tmp;
      spx_word32_t tmp32;
      st->x[i] = st->x[i+st->frame_size];
      tmp32 = SUB32(EXTEND32(echo[i]), EXTEND32(MULT16_16_P15(st->preemph, st->memX)));
#ifdef FIXED_POINT
      /*FIXME: If saturation occurs here, we need to freeze adaptation for M frames (not just one) */
      if (tmp32 > 32767)
      {
         tmp32 = 32767;
         st->saturated = 1;
      }      
      if (tmp32 < -32767)
      {
         tmp32 = -32767;
         st->saturated = 1;
      }      
#endif
      st->x[i+st->frame_size] = EXTRACT16(tmp32);
      st->memX = echo[i];
      
      tmp = st->d[i];
      st->d[i] = st->d[i+st->frame_size];
      tmp32 = SUB32(EXTEND32(tmp), EXTEND32(MULT16_16_P15(st->preemph, st->memD)));
#ifdef FIXED_POINT
      if (tmp32 > 32767)
      {
         tmp32 = 32767;
         st->saturated = 1;
      }      
      if (tmp32 < -32767)
      {
         tmp32 = -32767;
         st->saturated = 1;
      }
#endif
      st->d[i+st->frame_size] = tmp32;
      st->memD = tmp;
   }

   /* Shift memory: this could be optimized eventually*/
   for (j=M-1;j>=0;j--)
   {
      for (i=0;i<N;i++)
         st->X[(j+1)*N+i] = st->X[j*N+i];
   }

   /* Convert x (echo input) to frequency domain */
   spx_fft(st->fft_table, st->x, &st->X[0]);
   
#ifdef SMOOTH_BLOCKS
   spectral_mul_accum(st->X, st->W, st->Y, N, M);   
   spx_ifft(st->fft_table, st->Y, st->e);
#endif

   /* Compute weight gradient */
   if (!st->saturated)
   {
      for (j=M-1;j>=0;j--)
      {
         weighted_spectral_mul_conj(st->power_1, &st->X[(j+1)*N], st->E, st->PHI, N);
         for (i=0;i<N;i++)
            st->W[j*N+i] += MULT16_32_Q15(st->prop[j], st->PHI[i]);
         
      }   
   }
   
   st->saturated = 0;
   
   /* Update weight to prevent circular convolution (MDF / AUMDF) */
   for (j=0;j<M;j++)
   {
      /* This is a variant of the Alternatively Updated MDF (AUMDF) */
      /* Remove the "if" to make this an MDF filter */
      if (j==0 || st->cancel_count%(M-1) == j-1)
      {
#ifdef FIXED_POINT
         for (i=0;i<N;i++)
            st->wtmp2[i] = EXTRACT16(PSHR32(st->W[j*N+i],NORMALIZE_SCALEDOWN+16));
         spx_ifft(st->fft_table, st->wtmp2, st->wtmp);
         for (i=0;i<st->frame_size;i++)
         {
            st->wtmp[i]=0;
         }
         for (i=st->frame_size;i<N;i++)
         {
            st->wtmp[i]=SHL16(st->wtmp[i],NORMALIZE_SCALEUP);
         }
         spx_fft(st->fft_table, st->wtmp, st->wtmp2);
         /* The "-1" in the shift is a sort of kludge that trades less efficient update speed for decrease noise */
         for (i=0;i<N;i++)
            st->W[j*N+i] -= SHL32(EXTEND32(st->wtmp2[i]),16+NORMALIZE_SCALEDOWN-NORMALIZE_SCALEUP-1);
#else
         spx_ifft(st->fft_table, &st->W[j*N], st->wtmp);
         for (i=st->frame_size;i<N;i++)
         {
            st->wtmp[i]=0;
         }
         spx_fft(st->fft_table, st->wtmp, &st->W[j*N]);
#endif
      }
   }

   /* Compute filter response Y */
   spectral_mul_accum(st->X, st->W, st->Y, N, M);
   spx_ifft(st->fft_table, st->Y, st->y);

   
   /* Compute error signal (for the output with de-emphasis) */ 
   for (i=0;i<st->frame_size;i++)
   {
      spx_word32_t tmp_out;
#ifdef SMOOTH_BLOCKS
      spx_word16_t y = MULT16_16_Q15(st->window[i+st->frame_size],st->e[i+st->frame_size]) + MULT16_16_Q15(st->window[i],st->y[i+st->frame_size]);
      tmp_out = SUB32(EXTEND32(st->d[i+st->frame_size]), EXTEND32(y));
#else
      tmp_out = SUB32(EXTEND32(st->d[i+st->frame_size]), EXTEND32(st->y[i+st->frame_size]));
#endif

      /* Saturation */
      if (tmp_out>32767)
         tmp_out = 32767;
      else if (tmp_out<-32768)
         tmp_out = -32768;
      tmp_out = ADD32(tmp_out, EXTEND32(MULT16_16_P15(st->preemph, st->memE)));
      /* This is an arbitrary test for saturation */
      if (ref[i] <= -32000 || ref[i] >= 32000)
      {
         tmp_out = 0;
         st->saturated = 1;
      }
      out[i] = (spx_int16_t)tmp_out;
      st->memE = tmp_out;
   }

   /* Compute error signal (filter update version) */ 
   for (i=0;i<st->frame_size;i++)
   {
      st->e[i] = 0;
      st->e[i+st->frame_size] = st->d[i+st->frame_size] - st->y[i+st->frame_size];
   }

   /* Compute a bunch of correlations */
   Sey = mdf_inner_prod(st->e+st->frame_size, st->y+st->frame_size, st->frame_size);
   See = mdf_inner_prod(st->e+st->frame_size, st->e+st->frame_size, st->frame_size);
   See = ADD32(See, SHR32(MULT16_16(N, 100),6));
   Syy = mdf_inner_prod(st->y+st->frame_size, st->y+st->frame_size, st->frame_size);
   Sxx = mdf_inner_prod(st->x+st->frame_size, st->x+st->frame_size, st->frame_size);

   /* Convert error to frequency domain */
   spx_fft(st->fft_table, st->e, st->E);
   for (i=0;i<st->frame_size;i++)
      st->y[i] = 0;
   spx_fft(st->fft_table, st->y, st->Y);

   /* Compute power spectrum of echo (X), error (E) and filter response (Y) */
   power_spectrum(st->E, st->Rf, N);
   power_spectrum(st->Y, st->Yf, N);
   power_spectrum(st->X, st->Xf, N);
   
   /* Smooth echo energy estimate over time */
   for (j=0;j<=st->frame_size;j++)
      st->power[j] = MULT16_32_Q15(ss_1,st->power[j]) + 1 + MULT16_32_Q15(ss,st->Xf[j]);
   
   /* Enable this to compute the power based only on the tail (would need to compute more 
      efficiently to make this really useful */
   if (0)
   {
      float scale2 = .5f/M;
      for (j=0;j<=st->frame_size;j++)
         st->power[j] = 100;
      for (i=0;i<M;i++)
      {
         power_spectrum(&st->X[i*N], st->Xf, N);
         for (j=0;j<=st->frame_size;j++)
            st->power[j] += scale2*st->Xf[j];
      }
   }

   /* Compute filtered spectra and (cross-)correlations */
   for (j=st->frame_size;j>=0;j--)
   {
      spx_float_t Eh, Yh;
      Eh = PSEUDOFLOAT(st->Rf[j] - st->Eh[j]);
      Yh = PSEUDOFLOAT(st->Yf[j] - st->Yh[j]);
      Pey = FLOAT_ADD(Pey,FLOAT_MULT(Eh,Yh));
      Pyy = FLOAT_ADD(Pyy,FLOAT_MULT(Yh,Yh));
#ifdef FIXED_POINT
      st->Eh[j] = MAC16_32_Q15(MULT16_32_Q15(SUB16(32767,st->spec_average),st->Eh[j]), st->spec_average, st->Rf[j]);
      st->Yh[j] = MAC16_32_Q15(MULT16_32_Q15(SUB16(32767,st->spec_average),st->Yh[j]), st->spec_average, st->Yf[j]);
#else
      st->Eh[j] = (1-st->spec_average)*st->Eh[j] + st->spec_average*st->Rf[j];
      st->Yh[j] = (1-st->spec_average)*st->Yh[j] + st->spec_average*st->Yf[j];
#endif
   }
   
   Pyy = FLOAT_SQRT(Pyy);
   Pey = FLOAT_DIVU(Pey,Pyy);

   /* Compute correlation updatete rate */
   tmp32 = MULT16_32_Q15(st->beta0,Syy);
   if (tmp32 > MULT16_32_Q15(st->beta_max,See))
      tmp32 = MULT16_32_Q15(st->beta_max,See);
   alpha = FLOAT_DIV32(tmp32, See);
   alpha_1 = FLOAT_SUB(FLOAT_ONE, alpha);
   /* Update correlations (recursive average) */
   st->Pey = FLOAT_ADD(FLOAT_MULT(alpha_1,st->Pey) , FLOAT_MULT(alpha,Pey));
   st->Pyy = FLOAT_ADD(FLOAT_MULT(alpha_1,st->Pyy) , FLOAT_MULT(alpha,Pyy));
   if (FLOAT_LT(st->Pyy, FLOAT_ONE))
      st->Pyy = FLOAT_ONE;
   /* We don't really hope to get better than 33 dB (MIN_LEAK-3dB) attenuation anyway */
   if (FLOAT_LT(st->Pey, FLOAT_MULT(MIN_LEAK,st->Pyy)))
      st->Pey = FLOAT_MULT(MIN_LEAK,st->Pyy);
   if (FLOAT_GT(st->Pey, st->Pyy))
      st->Pey = st->Pyy;
   /* leak_estimate is the linear regression result */
   leak_estimate = FLOAT_EXTRACT16(FLOAT_SHL(FLOAT_DIVU(st->Pey, st->Pyy),14));
   /* This looks like a stupid bug, but it's right (because we convert from Q14 to Q15) */
   if (leak_estimate > 16383)
      leak_estimate = 32767;
   else
      leak_estimate = SHL16(leak_estimate,1);
   /*printf ("%f\n", leak_estimate);*/
   
   /* Compute Residual to Error Ratio */
#ifdef FIXED_POINT
   tmp32 = MULT16_32_Q15(leak_estimate,Syy);
   tmp32 = ADD32(SHR32(Sxx,13), ADD32(tmp32, SHL32(tmp32,1)));
   /* Check for y in e (lower bound on RER) */
   {
      spx_float_t bound = PSEUDOFLOAT(Sey);
      bound = FLOAT_DIVU(FLOAT_MULT(bound, bound), PSEUDOFLOAT(ADD32(1,Syy)));
      if (FLOAT_GT(bound, PSEUDOFLOAT(See)))
         tmp32 = See;
      else if (tmp32 < FLOAT_EXTRACT32(bound))
         tmp32 = FLOAT_EXTRACT32(bound);
   }
   if (tmp32 > SHR32(See,1))
      tmp32 = SHR32(See,1);
   RER = FLOAT_EXTRACT16(FLOAT_SHL(FLOAT_DIV32(tmp32,See),15));
#else
   RER = (.0001*Sxx + 3.*MULT16_32_Q15(leak_estimate,Syy)) / See;
   /* Check for y in e (lower bound on RER) */
   if (RER < Sey*Sey/(1+See*Syy))
      RER = Sey*Sey/(1+See*Syy);
   if (RER > .5)
      RER = .5;
#endif

   /* We consider that the filter has had minimal adaptation if the following is true*/
   if (!st->adapted && st->sum_adapt > QCONST32(1,15))
   {
      st->adapted = 1;
   }

   if (st->adapted)
   {
      for (i=0;i<=st->frame_size;i++)
      {
         spx_word32_t r, e;
         /* Compute frequency-domain adaptation mask */
         r = MULT16_32_Q15(leak_estimate,SHL32(st->Yf[i],3));
         e = SHL32(st->Rf[i],3)+1;
#ifdef FIXED_POINT
         if (r>SHR32(e,1))
            r = SHR32(e,1);
#else
         if (r>.5*e)
            r = .5*e;
#endif
         r = MULT16_32_Q15(QCONST16(.7,15),r) + MULT16_32_Q15(QCONST16(.3,15),(spx_word32_t)(MULT16_32_Q15(RER,e)));
         /*st->power_1[i] = adapt_rate*r/(e*(1+st->power[i]));*/
         st->power_1[i] = FLOAT_SHL(FLOAT_DIV32_FLOAT(r,FLOAT_MUL32U(e,st->power[i]+10)),WEIGHT_SHIFT+16);
      }
   } else if (Sxx > SHR32(MULT16_16(N, 1000),6)) {
      /* Temporary adaption rate if filter is not yet adapted enough */
      spx_word16_t adapt_rate=0;

      tmp32 = MULT16_32_Q15(QCONST16(.25f, 15), Sxx);
#ifdef FIXED_POINT
      if (tmp32 > SHR32(See,2))
         tmp32 = SHR32(See,2);
#else
      if (tmp32 > .25*See)
         tmp32 = .25*See;
#endif
      adapt_rate = FLOAT_EXTRACT16(FLOAT_SHL(FLOAT_DIV32(tmp32, See),15));
      
      for (i=0;i<=st->frame_size;i++)
         st->power_1[i] = FLOAT_SHL(FLOAT_DIV32(EXTEND32(adapt_rate),ADD32(st->power[i],10)),WEIGHT_SHIFT+1);


      /* How much have we adapted so far? */
      st->sum_adapt = ADD32(st->sum_adapt,adapt_rate);
   }

   /* Compute spectrum of estimated echo for use in an echo post-filter (if necessary)*/
   if (Yout)
   {
      spx_word16_t leak2;
      if (st->adapted)
      {
         /* If the filter is adapted, take the filtered echo */
         for (i=0;i<st->frame_size;i++)
            st->last_y[i] = st->last_y[st->frame_size+i];
         for (i=0;i<st->frame_size;i++)
            st->last_y[st->frame_size+i] = ref[i]-out[i];
      } else {
         /* If filter isn't adapted yet, all we can do is take the echo signal directly */
         for (i=0;i<N;i++)
            st->last_y[i] = st->x[i];
      }
      
      /* Apply hanning window (should pre-compute it)*/
      for (i=0;i<N;i++)
         st->y[i] = MULT16_16_Q15(st->window[i],st->last_y[i]);
      
      /* Compute power spectrum of the echo */
      spx_fft(st->fft_table, st->y, st->Y);
      power_spectrum(st->Y, st->Yps, N);
      
#ifdef FIXED_POINT
      if (leak_estimate > 16383)
         leak2 = 32767;
      else
         leak2 = SHL16(leak_estimate, 1);
#else
      if (leak_estimate>.5)
         leak2 = 1;
      else
         leak2 = 2*leak_estimate;
#endif
      /* Estimate residual echo */
      for (i=0;i<=st->frame_size;i++)
         Yout[i] = (spx_int32_t)MULT16_32_Q15(leak2,st->Yps[i]);
   }
}


int speex_echo_ctl(SpeexEchoState *st, int request, void *ptr)
{
   switch(request)
   {
      
      case SPEEX_ECHO_GET_FRAME_SIZE:
         (*(int*)ptr) = st->frame_size;
         break;
      case SPEEX_ECHO_SET_SAMPLING_RATE:
         st->sampling_rate = (*(int*)ptr);
         st->spec_average = DIV32_16(SHL32(EXTEND32(st->frame_size), 15), st->sampling_rate);
#ifdef FIXED_POINT
         st->beta0 = DIV32_16(SHL32(EXTEND32(st->frame_size), 16), st->sampling_rate);
         st->beta_max = DIV32_16(SHL32(EXTEND32(st->frame_size), 14), st->sampling_rate);
#else
         st->beta0 = (2.0f*st->frame_size)/st->sampling_rate;
         st->beta_max = (.5f*st->frame_size)/st->sampling_rate;
#endif
         if (st->sampling_rate<12000)
            st->notch_radius = QCONST16(.9, 15);
         else if (st->sampling_rate<24000)
            st->notch_radius = QCONST16(.982, 15);
         else
            st->notch_radius = QCONST16(.992, 15);
         break;
      case SPEEX_ECHO_GET_SAMPLING_RATE:
         (*(int*)ptr) = st->sampling_rate;
         break;
      default:
         speex_warning_int("Unknown speex_echo_ctl request: ", request);
         return -1;
   }
   return 0;
}
