/*
 * $Id: pablio.c 1151 2006-11-29 02:11:16Z leland_lucius $
 * pablio.c
 * Portable Audio Blocking Input/Output utility.
 *
 * Author: Phil Burk, http://www.softsynth.com
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

#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portaudio.h"
#include "pa_ringbuffer.h"
#include "pablio.h"
#include <string.h>
#include <time.h>


/************************************************************************/
/******** Prototypes ****************************************************/
/************************************************************************/

static int blockingIOCallback(const void *inputBuffer, void *outputBuffer,
							  unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo * timeInfo, PaStreamCallbackFlags statusFlags, void *userData);
static PaError PABLIO_InitFIFO(PaUtilRingBuffer * rbuf, long numFrames, long bytesPerFrame);
static PaError PABLIO_TermFIFO(PaUtilRingBuffer * rbuf);

/************************************************************************/
/******** Functions *****************************************************/
/************************************************************************/

/* Called from PortAudio.
 * Read and write data 
 */
static int blockingIOCallback(const void *inputBuffer, void *outputBuffer,
							  unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo * timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	PABLIO_Stream *data = (PABLIO_Stream *) userData;
	long numBytes = data->bytesPerFrame * framesPerBuffer;
	
	/* This may get called with NULL inputBuffer during initial setup. */
	if (inputBuffer != NULL) {
		if (PaUtil_WriteRingBuffer(&data->inFIFO, inputBuffer, numBytes) != numBytes) {
			PaUtil_FlushRingBuffer(&data->inFIFO);
			PaUtil_WriteRingBuffer(&data->inFIFO, inputBuffer, numBytes);
		}
	}
	if (outputBuffer != NULL) {
		int i;
		int numRead = PaUtil_ReadRingBuffer(&data->outFIFO, outputBuffer, numBytes);
		/* Zero out remainder of buffer if we run out of data. */
		for (i = numRead; i < numBytes; i++) {
			((char *) outputBuffer)[i] = 0;
		}
	}

	return 0;
}

/* Allocate buffer. */
static PaError PABLIO_InitFIFO(PaUtilRingBuffer * rbuf, long numFrames, long bytesPerFrame)
{
	long numBytes = numFrames * bytesPerFrame;
	char *buffer = (char *) malloc(numBytes);
	if (buffer == NULL)
		return paInsufficientMemory;
	memset(buffer, 0, numBytes);
	return (PaError) PaUtil_InitializeRingBuffer(rbuf, numBytes, buffer);
}

/* Free buffer. */
static PaError PABLIO_TermFIFO(PaUtilRingBuffer * rbuf)
{
	if (rbuf->buffer)
		free(rbuf->buffer);
	rbuf->buffer = NULL;
	return paNoError;
}

/************************************************************
 * Write data to ring buffer.
 * Will not return until all the data has been written.
 */
long WriteAudioStream(PABLIO_Stream * aStream, void *data, long numFrames, switch_timer_t *timer)
{
	long bytesWritten;
	char *p = (char *) data;
	long numBytes = aStream->bytesPerFrame * numFrames;

	while (numBytes > 0) {
		bytesWritten = PaUtil_WriteRingBuffer(&aStream->outFIFO, p, numBytes);
		numBytes -= bytesWritten;
		p += bytesWritten;
		if (numBytes > 0) {
			if (switch_core_timer_check(timer, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
				PaUtil_FlushRingBuffer(&aStream->outFIFO);
				return 0;
			}
			switch_yield(1000);
		}
	}
	return numFrames;
}

/************************************************************
 * Read data from ring buffer.
 * Will not return until all the data has been read.
 */
long ReadAudioStream(PABLIO_Stream * aStream, void *data, long numFrames, switch_timer_t *timer)
{
	long bytesRead = 0;
	char *p = (char *) data;
	long avail, readBytes = 0, numBytes = aStream->bytesPerFrame * numFrames;
	
	while (numBytes > 0) {
		avail = PaUtil_GetRingBufferReadAvailable(&aStream->inFIFO);

		if (avail >= numBytes * 10) {
			PaUtil_FlushRingBuffer(&aStream->inFIFO);
			avail = 0;
		}
		
		if (avail >= numBytes * 2) {
			bytesRead = PaUtil_ReadRingBuffer(&aStream->inFIFO, p, numBytes);
			numBytes -= bytesRead;
			readBytes += bytesRead;
		}
		
		if (numBytes > 0) {
			if (switch_core_timer_check(timer, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
				break;
			}
			switch_yield(1000);
			p += bytesRead;
		}
	}

	if (readBytes) {
		switch_core_timer_sync(timer);
	}

	return readBytes / aStream->bytesPerFrame;
}

/************************************************************
 * Return the number of frames that could be written to the stream without
 * having to wait.
 */
long GetAudioStreamWriteable(PABLIO_Stream * aStream)
{
	int bytesEmpty = PaUtil_GetRingBufferWriteAvailable(&aStream->outFIFO);
	return bytesEmpty / aStream->bytesPerFrame;
}

/************************************************************
 * Return the number of frames that are available to be read from the
 * stream without having to wait.
 */
long GetAudioStreamReadable(PABLIO_Stream * aStream)
{
	int bytesFull = PaUtil_GetRingBufferReadAvailable(&aStream->inFIFO);
	return bytesFull / aStream->bytesPerFrame;
}

/***********************************************************/
static unsigned long RoundUpToNextPowerOf2(unsigned long n)
{
	long numBits = 0;
	if (((n - 1) & n) == 0)
		return n;				
	while (n > 0) {
		n = n >> 1;
		numBits++;
	}
	return (1 << numBits);
}



/************************************************************
 * Opens a PortAudio stream with default characteristics.
 * Allocates PABLIO_Stream structure.
 *
 */
PaError OpenAudioStream(PABLIO_Stream ** rwblPtr,
						const PaStreamParameters *inputParameters,
						const PaStreamParameters *outputParameters,
						double sampleRate,
						PaStreamFlags streamFlags,
						long samples_per_frame)
{
	long bytesPerSample = 2;
	PaError err;
	PABLIO_Stream *aStream;
	long numFrames;
	//long numBytes;
	int channels = 1;

	/* Allocate PABLIO_Stream structure for caller. */
	aStream = (PABLIO_Stream *) malloc(sizeof(PABLIO_Stream));
	if (aStream == NULL)
		return paInsufficientMemory;
	memset(aStream, 0, sizeof(PABLIO_Stream));

	/* Initialize PortAudio  */
	err = Pa_Initialize();
	if (err != paNoError)
		goto error;

	if (inputParameters) {
		channels = inputParameters->channelCount;
	} else if (outputParameters) {
		channels = outputParameters->channelCount;
	}

	numFrames = RoundUpToNextPowerOf2(samples_per_frame * 5);
	aStream->bytesPerFrame = bytesPerSample;

	/* Initialize Ring Buffers */

	if (inputParameters) {
		err = PABLIO_InitFIFO(&aStream->inFIFO, numFrames, aStream->bytesPerFrame);
		if (err != paNoError)
			goto error;
	}

	if (outputParameters) {
		err = PABLIO_InitFIFO(&aStream->outFIFO, numFrames, aStream->bytesPerFrame);
		if (err != paNoError)
			goto error;
	}

	/* Make Write FIFO appear full initially. */
	//numBytes = PaUtil_GetRingBufferWriteAvailable(&aStream->outFIFO);
	//PaUtil_AdvanceRingBufferWriteIndex(&aStream->outFIFO, numBytes);


	/* Open a PortAudio stream that we will use to communicate with the underlying
	 * audio drivers. */
	err = Pa_OpenStream(&aStream->stream, inputParameters, outputParameters, sampleRate, samples_per_frame, streamFlags, blockingIOCallback, aStream);

	if (err != paNoError)
		goto error;

	err = Pa_StartStream(aStream->stream);
	if (err != paNoError)
		goto error;
	*rwblPtr = aStream;
	return paNoError;

  error:
	CloseAudioStream(aStream);
	*rwblPtr = NULL;
	return err;
}

/************************************************************/
PaError CloseAudioStream(PABLIO_Stream * aStream)
{
	PaError err;
	int bytesEmpty;
	int byteSize = aStream->outFIFO.bufferSize;

	/* If we are writing data, make sure we play everything written. */
	if (byteSize > 0) {
		bytesEmpty = PaUtil_GetRingBufferWriteAvailable(&aStream->outFIFO);
		while (bytesEmpty < byteSize) {
			Pa_Sleep(10);
			bytesEmpty = PaUtil_GetRingBufferWriteAvailable(&aStream->outFIFO);
		}
	}

	err = Pa_StopStream(aStream->stream);
	if (err != paNoError)
		goto error;
	err = Pa_CloseStream(aStream->stream);
	if (err != paNoError)
		goto error;
	Pa_Terminate();

  error:
	PABLIO_TermFIFO(&aStream->inFIFO);
	PABLIO_TermFIFO(&aStream->outFIFO);
	free(aStream);
	return err;
}
