/* Copyright (C) 2004 Jean-Marc Valin */
/**
   @file filters_arm4.h
   @brief Various analysis/synthesis filters (ARM4 version)
*/
/*
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

#define OVERRIDE_NORMALIZE16
int normalize16(const spx_sig_t *x, spx_word16_t *y, int max_scale, int len)
{
   int i;
   spx_sig_t max_val=1;
   int sig_shift;
   int dead1, dead2, dead3, dead4, dead5, dead6;

   __asm__ __volatile__ (
         "\tmov %1, #1 \n"
         "\tmov %3, #0 \n"

         ".normalize16loop1%=: \n"

         "\tldr %4, [%0], #4 \n"
         "\tcmps %4, %1 \n"
         "\tmovgt %1, %4 \n"
         "\tcmps %4, %3 \n"
         "\tmovlt %3, %4 \n"

         "\tsubs %2, %2, #1 \n"
         "\tbne .normalize16loop1%=\n"

         "\trsb %3, %3, #0 \n"
         "\tcmp %1, %3 \n"
         "\tmovlt %1, %3 \n"
   : "=r" (dead1), "=r" (max_val), "=r" (dead3), "=r" (dead4),
   "=r" (dead5), "=r" (dead6)
   : "0" (x), "2" (len)
   : "cc");

   sig_shift=0;
   while (max_val>max_scale)
   {
      sig_shift++;
      max_val >>= 1;
   }
   
   __asm__ __volatile__ (
         ".normalize16loop%=: \n"

         "\tldr %4, [%0], #4 \n"
         "\tldr %5, [%0], #4 \n"
         "\tmov %4, %4, asr %3 \n"
         "\tstrh %4, [%1], #2 \n"
         "\tldr %4, [%0], #4 \n"
         "\tmov %5, %5, asr %3 \n"
         "\tstrh %5, [%1], #2 \n"
         "\tldr %5, [%0], #4 \n"
         "\tmov %4, %4, asr %3 \n"
         "\tstrh %4, [%1], #2 \n"
         "\tsubs %2, %2, #1 \n"
         "\tmov %5, %5, asr %3 \n"
         "\tstrh %5, [%1], #2 \n"

         "\tbge .normalize16loop%=\n"
   : "=r" (dead1), "=r" (dead2), "=r" (dead3), "=r" (dead4),
   "=r" (dead5), "=r" (dead6)
   : "0" (x), "1" (y), "2" (len>>2), "3" (sig_shift)
   : "cc", "memory");
   return sig_shift;
}

#define OVERRIDE_FILTER_MEM2
void filter_mem2(const spx_sig_t *x, const spx_coef_t *num, const spx_coef_t *den, spx_sig_t *y, int N, int ord, spx_mem_t *mem)
{
   int i,j;
   spx_sig_t xi,yi,nyi;

   for (i=0;i<ord;i++)
      mem[i] = SHR32(mem[i],1);   
   for (i=0;i<N;i++)
   {
      int deadm, deadn, deadd, deadidx, x1, y1, dead1, dead2, dead3, dead4, dead5, dead6;
      xi=SATURATE(x[i],805306368);
      yi = SATURATE(ADD32(xi, SHL(mem[0],2)),805306368);
      nyi = -yi;
      y[i] = yi;
      __asm__ __volatile__ (
            "\tldrsh %6, [%1], #2\n"
            "\tsmull %8, %9, %4, %6\n"
#ifdef SHORTCUTS
            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"


#else
            ".filterloop%=: \n"
            "\tldrsh %6, [%2], #2\n"
            "\tldr %10, [%0, #4]\n"
            "\tmov %8, %8, lsr #15\n"
            "\tsmull %7, %11, %5, %6\n"
            "\tadd %8, %8, %9, lsl #17\n"
            "\tldrsh %6, [%1], #2\n"
            "\tadd %10, %10, %8\n"
            "\tsmull %8, %9, %4, %6\n"
            "\tadd %10, %10, %7, lsr #15\n"
            "\tsubs %3, %3, #1\n"
            "\tadd %10, %10, %11, lsl #17\n"
            "\tstr %10, [%0], #4 \n"
            "\t bne .filterloop%=\n"
#endif
            "\tmov %8, %8, lsr #15\n"
            "\tadd %10, %8, %9, lsl #17\n"
            "\tldrsh %6, [%2], #2\n"
            "\tsmull %8, %9, %5, %6\n"
            "\tadd %10, %10, %8, lsr #15\n"
            "\tadd %10, %10, %9, lsl #17\n"
            "\tstr %10, [%0], #4 \n"

         : "=r" (deadm), "=r" (deadn), "=r" (deadd), "=r" (deadidx),
      "=r" (xi), "=r" (nyi), "=r" (dead1), "=r" (dead2),
      "=r" (dead3), "=r" (dead4), "=r" (dead5), "=r" (dead6)
         : "0" (mem), "1" (num), "2" (den), "3" (ord-1), "4" (xi), "5" (nyi)
         : "cc", "memory");
   
   }
   for (i=0;i<ord;i++)
      mem[i] = SHL32(mem[i],1);   
}

#define OVERRIDE_IIR_MEM2
void iir_mem2(const spx_sig_t *x, const spx_coef_t *den, spx_sig_t *y, int N, int ord, spx_mem_t *mem)
{
   int i,j;
   spx_sig_t xi,yi,nyi;

   for (i=0;i<ord;i++)
      mem[i] = SHR32(mem[i],1);   

   for (i=0;i<N;i++)
   {
      int deadm, deadd, deadidx, dead1, dead2, dead3, dead4, dead5, dead6;
      xi=SATURATE(x[i],805306368);
      yi = SATURATE(ADD32(xi, SHL(mem[0],2)),805306368);
      nyi = -yi;
      y[i] = yi;
      __asm__ __volatile__ (
            "\tldrsh %4, [%1], #2\n"
            "\tsmull %5, %6, %3, %4\n"

#ifdef SHORTCUTS
                        
            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %7, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %7, %7, %8\n"
            "\tstr %7, [%0], #4 \n"

                 
            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %9, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %9, %9, %8\n"
            "\tstr %9, [%0], #4 \n"

            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %7, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %7, %7, %8\n"
            "\tstr %7, [%0], #4 \n"

            
            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %9, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %9, %9, %8\n"
            "\tstr %9, [%0], #4 \n"

            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %7, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %7, %7, %8\n"
            "\tstr %7, [%0], #4 \n"

            
            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %9, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %9, %9, %8\n"
            "\tstr %9, [%0], #4 \n"

            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %7, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %7, %7, %8\n"
            "\tstr %7, [%0], #4 \n"

            
            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %9, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %9, %9, %8\n"
            "\tstr %9, [%0], #4 \n"

            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tldr %7, [%0, #4]\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %7, %7, %8\n"
            "\tstr %7, [%0], #4 \n"

            
            
#else
            ".iirloop%=: \n"
            "\tldr %7, [%0, #4]\n"

            "\tldrsh %4, [%1], #2\n"
            "\tmov %5, %5, lsr #15\n"
            "\tadd %8, %5, %6, lsl #17\n"
            "\tsmull %5, %6, %3, %4\n"
            "\tadd %7, %7, %8\n"
            "\tstr %7, [%0], #4 \n"
            "\tsubs %2, %2, #1\n"
            "\t bne .iirloop%=\n"
            
#endif
            "\tmov %5, %5, lsr #15\n"
            "\tadd %7, %5, %6, lsl #17\n"
            "\tstr %7, [%0], #4 \n"

         : "=r" (deadm), "=r" (deadd), "=r" (deadidx), "=r" (nyi),
      "=r" (dead1), "=r" (dead2), "=r" (dead3), "=r" (dead4),
      "=r" (dead5), "=r" (dead6)
         : "0" (mem), "1" (den), "2" (ord-1), "3" (nyi)
         : "cc", "memory");
   
   }
   for (i=0;i<ord;i++)
      mem[i] = SHL32(mem[i],1);   

}
