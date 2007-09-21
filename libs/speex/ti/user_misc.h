/* Copyright (C) 2005 Psi Systems, Inc.
   Author:  Jean-Marc Valin 
   File: user_misc.h
   Memory Allocation overrides to allow user control rather than C alloc/free.

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

#ifdef MANUAL_ALLOC

/* To avoid changing the Speex call model, this file relies on four static variables 
   The user main creates two linear buffers, and initializes spxGlobalHeap/ScratchPtr 
   to point to the start of the two buffers, and initializes spxGlobalHeap/ScratchEnd 
   to point to the first address following the last byte of the two buffers.
   
   This mechanism allows, for example, data caching for multichannel applications, 
   where the Speex state is swapped from a large slow memory to a small fast memory 
   each time the codec runs.
   
   Persistent data is allocated in spxGlobalHeap (instead of calloc), while scratch
   data is allocated in spxGlobalScratch.
*/

extern char *spxGlobalHeapPtr, *spxGlobalHeapEnd; 
extern char *spxGlobalScratchPtr, *spxGlobalScratchEnd;

/* Make sure that all structures are aligned to largest type */
#define BLOCK_MASK      (sizeof(long double)-1)

#define OVERRIDE_SPEEX_ALLOC
void *speex_alloc (int size)
{
    void *ptr;
    
    ptr = (void *) (((int)spxGlobalHeapPtr + BLOCK_MASK) & ~BLOCK_MASK);  //Start on 8 boundary

    spxGlobalHeapPtr = (char *)((int)ptr + size);	// Update pointer to next free location

    if (spxGlobalHeapPtr > spxGlobalHeapEnd ) 
    {
#ifdef VERBOSE_ALLOC
	    fprintf (stderr, "insufficient space for persistent alloc request %d bytes\n", size);
#endif
       return 0;
    }
   
#ifdef VERBOSE_ALLOC
    fprintf (stderr, "Persist Allocated %d chars at %x, %d remaining\n", size, ptr, ((int)spxGlobalHeapEnd - (int)spxGlobalHeapPtr));
#endif
    memset(ptr, 0, size);
    return ptr;
}

#define OVERRIDE_SPEEX_ALLOC_SCRATCH
void *speex_alloc_scratch (int size)
{
    void *ptr;

    ptr = (void *) (((int)spxGlobalScratchPtr + BLOCK_MASK) & ~BLOCK_MASK);  //Start on 8 boundary

    spxGlobalScratchPtr = (char *)((int)ptr + size);	// Update pointer to next free location

    if (spxGlobalScratchPtr > spxGlobalScratchEnd ) 
    {
#ifdef VERBOSE_ALLOC
	    fprintf (stderr, "insufficient space for scratch alloc request %d bytes\n", size);
#endif
       return 0;
    }
   
#ifdef VERBOSE_ALLOC
    fprintf (stderr, "Scratch Allocated %d chars at %x, %d remaining\n", size, ptr, ((int)spxGlobalScratchEnd - (int)spxGlobalScratchPtr));
#endif
    memset(ptr, 0, size);
    return ptr;
}

#define OVERRIDE_SPEEX_REALLOC
void *speex_realloc (void *ptr, int size)
{
#ifdef VERBOSE_ALLOC
   speex_warning("realloc attempted, not allowed");
#endif
   return 0;
}

#define OVERRIDE_SPEEX_FREE
void speex_free (void *ptr)
{
#ifdef VERBOSE_ALLOC
   speex_warning("at speex_free");
#endif
}
#define OVERRIDE_SPEEX_FREE_SCRATCH
void speex_free_scratch (void *ptr)
{
#ifdef VERBOSE_ALLOC
   speex_warning("at speex_free_scratch");
#endif
}

#endif    /* !MANUAL_ALLOC */
