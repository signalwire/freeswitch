#ifndef PORTAUDIO_PABLIO_H
#define PORTAUDIO_PABLIO_H

#ifdef __cplusplus
extern "C" {
#endif							/* __cplusplus */

/*
 * $Id: pablio.h 1083 2006-08-23 07:30:49Z rossb $
 * PABLIO.h
 * Portable Audio Blocking read/write utility.
 *
 * Author: Phil Burk, http://www.softsynth.com/portaudio/
 *
 * Include file for PABLIO, the Portable Audio Blocking I/O Library.
 * PABLIO is built on top of PortAudio, the Portable Audio Library.
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <portaudio.h>
#include "pa_ringbuffer.h"

/*! Maximum number of channels per stream */
#define MAX_IO_CHANNELS 2

/*! Maximum numer of milliseconds per packet */
#define MAX_IO_MS 100

/*! Maximum sampling rate (48Khz) */
#define MAX_SAMPLING_RATE  48000

/* Maximum size of a read */
#define MAX_IO_BUFFER (((MAX_IO_MS * MAX_SAMPLING_RATE)/1000)*sizeof(int16_t))
typedef struct {
	PaStream *istream;
	PaStream *ostream;
	PaStream *iostream;
	int bytesPerFrame;
	int do_dual;
	int has_in;
	int has_out;
	PaUtilRingBuffer inFIFOs[MAX_IO_CHANNELS];
	PaUtilRingBuffer outFIFOs[MAX_IO_CHANNELS];
	int channelCount;
	char iobuff[MAX_IO_BUFFER];
} PABLIO_Stream;

/* Values for flags for OpenAudioStream(). */
#define PABLIO_READ     (1<<0)
#define PABLIO_WRITE    (1<<1)
#define PABLIO_READ_WRITE    (PABLIO_READ|PABLIO_WRITE)
#define PABLIO_MONO     (1<<2)
#define PABLIO_STEREO   (1<<3)

/************************************************************
 * Write data to ring buffer.
 * Will not return until all the data has been written.
 */
long WriteAudioStream(PABLIO_Stream * aStream, void *data, long numFrames, int chan, switch_timer_t *timer);

/************************************************************
 * Read data from ring buffer.
 * Will not return until all the data has been read.
 */
long ReadAudioStream(PABLIO_Stream * aStream, void *data, long numFrames, int chan, switch_timer_t *timer);

/************************************************************
 * Return the number of frames that could be written to the stream without
 * having to wait.
 */
long GetAudioStreamWriteable(PABLIO_Stream * aStream, int chan);

/************************************************************
 * Return the number of frames that are available to be read from the
 * stream without having to wait.
 */
long GetAudioStreamReadable(PABLIO_Stream * aStream, int chan);

/************************************************************
 * Opens a PortAudio stream with default characteristics.
 * Allocates PABLIO_Stream structure.
 *
 * flags parameter can be an ORed combination of:
 *    PABLIO_READ, PABLIO_WRITE, or PABLIO_READ_WRITE,
 *    and either PABLIO_MONO or PABLIO_STEREO
 */
PaError OpenAudioStream(PABLIO_Stream ** rwblPtr,
			const PaStreamParameters * inputParameters,
			const PaStreamParameters * outputParameters,
			double sampleRate, PaStreamCallbackFlags statusFlags, long samples_per_packet, int do_dual);

PaError CloseAudioStream(PABLIO_Stream * aStream);

#ifdef __cplusplus
}
#endif							/* __cplusplus */
#endif							/* _PABLIO_H */
