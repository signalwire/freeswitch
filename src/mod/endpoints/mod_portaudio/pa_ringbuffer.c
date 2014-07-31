/*
 * $Id: pa_ringbuffer.c 1164 2006-12-21 15:34:50Z bjornroche $
 * Portable Audio I/O Library
 * Ring Buffer utility.
 *
 * Author: Phil Burk, http://www.softsynth.com
 * modified for SMP safety on Mac OS X by Bjorn Roche
 * modified for SMP safety on Linux by Leland Lucius
 * also, allowed for const where possible
 * Note that this is safe only for a single-thread reader and a
 * single-thread writer.
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

/**
 @file
 @ingroup common_src
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pa_ringbuffer.h"

/****************
 * First, we'll define some memory barrier primitives based on the system.
 * right now only OS X, FreeBSD, and Linux are supported. In addition to providing
 * memory barriers, these functions should ensure that data cached in registers
 * is written out to cache where it can be snooped by other CPUs. (ie, the volatile
 * keyword should not be required)
 *
 * the primitives that must be defined are:
 *
 * PaUtil_FullMemoryBarrier()
 * PaUtil_ReadMemoryBarrier()
 * PaUtil_WriteMemoryBarrier()
 *
 ****************/
#define __VIA_HACK__
#if defined(__VIA_HACK__)
#define NO_BARRIER
#endif

#if defined(NO_BARRIER)
#   define PaUtil_FullMemoryBarrier()
#   define PaUtil_ReadMemoryBarrier()
#   define PaUtil_WriteMemoryBarrier()
#else

#if defined(__APPLE__)			//|| defined(__FreeBSD__)
#   include <libkern/OSAtomic.h>
	/* Here are the memory barrier functions. Mac OS X and FreeBSD only provide
	   full memory barriers, so the three types of barriers are the same. */
#   define PaUtil_FullMemoryBarrier()  OSMemoryBarrier()
#   define PaUtil_ReadMemoryBarrier()  OSMemoryBarrier()
#   define PaUtil_WriteMemoryBarrier() OSMemoryBarrier()
#elif defined(__GNUC__)

	/* GCC understands volatile asm and "memory" to mean it
	 * should not reorder memory read/writes */
#   if defined( __PPC__ )
#      define PaUtil_FullMemoryBarrier()  __asm__ volatile("sync":::"memory")
#      define PaUtil_ReadMemoryBarrier()  __asm__ volatile("sync":::"memory")
#      define PaUtil_WriteMemoryBarrier() __asm__ volatile("sync":::"memory")
#   elif defined( __i386__ ) || defined( __i486__ ) || defined( __i586__ ) || defined( __i686__ ) || defined(__x86_64__)
#      define PaUtil_FullMemoryBarrier()  __asm__ volatile("mfence":::"memory")
#      define PaUtil_ReadMemoryBarrier()  __asm__ volatile("lfence":::"memory")
#      define PaUtil_WriteMemoryBarrier() __asm__ volatile("sfence":::"memory")
#   else
#      define PaUtil_FullMemoryBarrier()
#      define PaUtil_ReadMemoryBarrier()
#      define PaUtil_WriteMemoryBarrier()
#   endif
#elif defined(_MSC_VER)
#   include <intrin.h>
#   pragma intrinsic(_ReadWriteBarrier)
#   pragma intrinsic(_ReadBarrier)
#   pragma intrinsic(_WriteBarrier)
#   define PaUtil_FullMemoryBarrier()  _ReadWriteBarrier()
#   define PaUtil_ReadMemoryBarrier()  _ReadBarrier()
#   define PaUtil_WriteMemoryBarrier() _WriteBarrier()
#else
#   define PaUtil_FullMemoryBarrier()
#   define PaUtil_ReadMemoryBarrier()
#   define PaUtil_WriteMemoryBarrier()
#endif
#endif
/***************************************************************************
 * Initialize FIFO.
 * numBytes must be power of 2, returns -1 if not.
 */
long PaUtil_InitializeRingBuffer(PaUtilRingBuffer * rbuf, long numBytes, void *dataPtr)
{
	if (((numBytes - 1) & numBytes) != 0)
		return -1;				/* Not Power of two. */
	rbuf->bufferSize = numBytes;
	rbuf->buffer = (char *) dataPtr;
	PaUtil_FlushRingBuffer(rbuf);
	rbuf->bigMask = (numBytes * 2) - 1;
	rbuf->smallMask = (numBytes) - 1;
	return 0;
}

/***************************************************************************
** Return number of bytes available for reading. */
long PaUtil_GetRingBufferReadAvailable(PaUtilRingBuffer * rbuf)
{
	PaUtil_ReadMemoryBarrier();
	return ((rbuf->writeIndex - rbuf->readIndex) & rbuf->bigMask);
}

/***************************************************************************
** Return number of bytes available for writing. */
long PaUtil_GetRingBufferWriteAvailable(PaUtilRingBuffer * rbuf)
{
	/* Since we are calling PaUtil_GetRingBufferReadAvailable, we don't need an aditional MB */
	return (rbuf->bufferSize - PaUtil_GetRingBufferReadAvailable(rbuf));
}

/***************************************************************************
** Clear buffer. Should only be called when buffer is NOT being read. */
void PaUtil_FlushRingBuffer(PaUtilRingBuffer * rbuf)
{
	rbuf->writeIndex = rbuf->readIndex = 0;
}

/***************************************************************************
** Get address of region(s) to which we can write data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
long PaUtil_GetRingBufferWriteRegions(PaUtilRingBuffer * rbuf, long numBytes, void **dataPtr1, long *sizePtr1, void **dataPtr2, long *sizePtr2)
{
	long index;
	long available = PaUtil_GetRingBufferWriteAvailable(rbuf);
	if (numBytes > available)
		numBytes = available;
	/* Check to see if write is not contiguous. */
	index = rbuf->writeIndex & rbuf->smallMask;
	if ((index + numBytes) > rbuf->bufferSize) {
		/* Write data in two blocks that wrap the buffer. */
		long firstHalf = rbuf->bufferSize - index;
		*dataPtr1 = &rbuf->buffer[index];
		*sizePtr1 = firstHalf;
		*dataPtr2 = &rbuf->buffer[0];
		*sizePtr2 = numBytes - firstHalf;
	} else {
		*dataPtr1 = &rbuf->buffer[index];
		*sizePtr1 = numBytes;
		*dataPtr2 = NULL;
		*sizePtr2 = 0;
	}
	return numBytes;
}


/***************************************************************************
*/
long PaUtil_AdvanceRingBufferWriteIndex(PaUtilRingBuffer * rbuf, long numBytes)
{
	/* we need to ensure that previous writes are seen before we update the write index */
	PaUtil_WriteMemoryBarrier();
	return rbuf->writeIndex = (rbuf->writeIndex + numBytes) & rbuf->bigMask;
}

/***************************************************************************
** Get address of region(s) from which we can read data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
long PaUtil_GetRingBufferReadRegions(PaUtilRingBuffer * rbuf, long numBytes, void **dataPtr1, long *sizePtr1, void **dataPtr2, long *sizePtr2)
{
	long index;
	long available = PaUtil_GetRingBufferReadAvailable(rbuf);
	if (numBytes > available)
		numBytes = available;
	/* Check to see if read is not contiguous. */
	index = rbuf->readIndex & rbuf->smallMask;
	if ((index + numBytes) > rbuf->bufferSize) {
		/* Write data in two blocks that wrap the buffer. */
		long firstHalf = rbuf->bufferSize - index;
		*dataPtr1 = &rbuf->buffer[index];
		*sizePtr1 = firstHalf;
		*dataPtr2 = &rbuf->buffer[0];
		*sizePtr2 = numBytes - firstHalf;
	} else {
		*dataPtr1 = &rbuf->buffer[index];
		*sizePtr1 = numBytes;
		*dataPtr2 = NULL;
		*sizePtr2 = 0;
	}
	return numBytes;
}

/***************************************************************************
*/
long PaUtil_AdvanceRingBufferReadIndex(PaUtilRingBuffer * rbuf, long numBytes)
{
	/* we need to ensure that previous writes are always seen before updating the index. */
	PaUtil_WriteMemoryBarrier();
	return rbuf->readIndex = (rbuf->readIndex + numBytes) & rbuf->bigMask;
}

/***************************************************************************
** Return bytes written. */
long PaUtil_WriteRingBuffer(PaUtilRingBuffer * rbuf, const void *data, long numBytes)
{
	long size1, size2, numWritten;
	void *data1, *data2;
	numWritten = PaUtil_GetRingBufferWriteRegions(rbuf, numBytes, &data1, &size1, &data2, &size2);
	if (size2 > 0) {

		memcpy(data1, data, size1);
		data = ((char *) data) + size1;
		memcpy(data2, data, size2);
	} else {
		memcpy(data1, data, size1);
	}
	PaUtil_AdvanceRingBufferWriteIndex(rbuf, numWritten);
	return numWritten;
}

/***************************************************************************
** Return bytes read. */
long PaUtil_ReadRingBuffer(PaUtilRingBuffer * rbuf, void *data, long numBytes)
{
	long size1, size2, numRead;
	void *data1, *data2;
	numRead = PaUtil_GetRingBufferReadRegions(rbuf, numBytes, &data1, &size1, &data2, &size2);
	if (size2 > 0) {
		memcpy(data, data1, size1);
		data = ((char *) data) + size1;
		memcpy(data, data2, size2);
	} else {
		memcpy(data, data1, size1);
	}
	PaUtil_AdvanceRingBufferReadIndex(rbuf, numRead);
	return numRead;
}
