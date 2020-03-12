#ifndef PULSEAUDIO_PABLIO_H
#define PULSEAUDIO_PABLIO_H

#ifdef __cplusplus
extern "C" {
#endif							/* __cplusplus */

/*
 * $Id: pablio.h 1083 2006-08-23 07:30:49Z rossb $
 * PABLIO.h
 * PulseAudio abstraction
 *
 * Contributor(s):
 *
 * Phil Burk, http://www.softsynth.com
 * Jérôme Poulin <jeromepoulin@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>

/*! Maximum number of channels per stream */
#define MAX_IO_CHANNELS 2

/*! Maximum numer of milliseconds per packet */
#define MAX_IO_MS 100

/*! Maximum sampling rate (48Khz) */
#define MAX_SAMPLING_RATE  48000
#define SAMPLE_TYPE PA_SAMPLE_S16LE
typedef int16_t SAMPLE;

/* Maximum size of a read */
#define MAX_IO_BUFFER (((MAX_IO_MS * MAX_SAMPLING_RATE)/1000)*sizeof(int16_t))
typedef struct {
	pa_simple *istream;
	pa_simple *ostream;
	int bytesPerFrame;
	int has_in;
	int has_out;
	int channelCount;
	char iobuff[MAX_IO_BUFFER];
} PABLIO_Stream;

/* PulseAudio error */
typedef int pa_error;

long WriteAudioStream(PABLIO_Stream * aStream, void *data, size_t datalen, int chan, switch_timer_t *timer);
long ReadAudioStream(PABLIO_Stream * aStream, void *data, size_t datalen, int chan, switch_timer_t *timer);

/************************************************************
 * Opens a PulseAudio stream with default characteristics.
 * Allocates PABLIO_Stream structure.
 */
pa_error OpenAudioStream(PABLIO_Stream ** rwblPtr,
			const char * channelName,
			const pa_sample_spec * inputParameters,
			const pa_sample_spec * outputParameters,
			double sampleRate, long samples_per_packet);

void FlushAudioStream(PABLIO_Stream * aStream);
void CloseAudioStream(PABLIO_Stream * aStream);

#ifdef __cplusplus
}
#endif							/* __cplusplus */
#endif							/* _PABLIO_H */
