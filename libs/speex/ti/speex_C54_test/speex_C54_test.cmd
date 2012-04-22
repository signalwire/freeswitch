/* Copyright (C) 2005 Psi Systems, Inc.
   File: speex_C54_test.cmd
   Linker command file with memory allocation for TI TMS320VC5416 processor
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

-c
-stack 0x2000
-heap 0x1000	/* If private memory allocation is used for Speex */
/*-heap 0x6000	/* If calloc is used for Speex */
-lrts_ext.lib

MEMORY
{
/*   PAGE 0:   P_DARAM03:  origin = 0x80,          len = 0x7f00*/
   PAGE 0:   P_DARAM03:  origin = 0x5000,          len = 0x2f80
   PAGE 0:   VECT:       origin = 0x7f80,        len = 0x80
   PAGE 0:   P_DARAM47:  origin = 0x18000,       len = 0x8000
   PAGE 0:   SARAM03:    origin = 0x28000,       len = 0x8000
   PAGE 0:   SARAM47:    origin = 0x38000,       len = 0x8000

   PAGE 1:   USERREGS:   origin = 0x60,          len = 0x1a
   PAGE 1:   BIOSREGS:   origin = 0x7c,          len = 0x4
   PAGE 1:   CSLREGS:    origin = 0x7a,          len = 0x2
            D_DARAM03:  origin = 0x80,          len = 0x4f80
            D_DARAM47:  origin = 0x8000,        len = 0x8000
}

SECTIONS
{
    .vectors: {} > VECT PAGE 0
    .bootmem: {rts_ext.lib (.text)} > P_DARAM03 PAGE 0 
/*    .bootmem: {} > P_DARAM03 PAGE 0 */
    .text:    {} > SARAM03 PAGE 0
    .cinit:   {} > SARAM03 PAGE 0
    .switch:  {} > SARAM03 PAGE 0
    .bss:     {} > D_DARAM03 | D_DARAM47 PAGE 1
    .far:     {} > D_DARAM03 | D_DARAM47  PAGE 1
    .const:   {} > D_DARAM03 | D_DARAM47  PAGE 1
    .sysmem:  {} > D_DARAM47 PAGE 1
    .cio:     {} > D_DARAM03 | D_DARAM47  PAGE 1
    .stack:   {} > D_DARAM03 | D_DARAM47  PAGE 1
    .myheap:   {} > D_DARAM47 PAGE 1
}
