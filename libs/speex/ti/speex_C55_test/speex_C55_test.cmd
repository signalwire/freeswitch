/* Copyright (C) 2005 Psi Systems, Inc.
   File: speex_C55_test.cmd
   Linker command file with memory allocation for TI TMS320VC5509A processor
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
-stack 0x1c00
-heap 0x1000	/* If private memory allocation is used for Speex */
/*-heap 0x6000	/ * If calloc is used for Speex */
-sysstack 0x200
-lrts55.lib

MEMORY
{
   DARAM:      origin = 0x200,         len = 0x7e00
   DARAM_B:    origin = 0x8000,        len = 0x8000
   VECT:       origin = 0x100,         len = 0x100
   SARAM_A:    origin = 0x10000,       len = 0x10000
   SARAM_B:    origin = 0x20000,       len = 0x20000
}

SECTIONS
{
    .vectors: {} > VECT
    .bootmem: {} > DARAM
    .text:    {} > SARAM_B
    .cinit:   {} > SARAM_B
    .switch:  {} > SARAM_B
    .bss:     {} > DARAM
/*    .far:     {} > DARAM*/
    .const:   {} > DARAM
    .sysmem:  {} > DARAM_B
    .cio:     {} > DARAM
    .stack:   {} > DARAM
    .sysstack:   {} > DARAM
    .myheap:   {} > SARAM_A
}
