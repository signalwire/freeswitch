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
#include <string.h>
#include <time.h>
#include <pulse/simple.h>
#include "pablio.h"

/************************************************************
 * Write data to ring buffer.
 * Will not return until all the data has been written.
 */
long WriteAudioStream(PABLIO_Stream * aStream, void *data, size_t datalen, int chan, switch_timer_t *timer)
{
	switch_core_timer_next(timer);

	pa_simple_write(aStream->ostream, data, datalen, NULL);
	//pa_simple_drain(aStream->ostream, NULL);

	return datalen;
}

/************************************************************
 * Read data from ring buffer.
 * Will not return until all the data has been read.
 */
long ReadAudioStream(PABLIO_Stream * aStream, void *data, size_t datalen, int chan, switch_timer_t *timer)
{
	switch_core_timer_next(timer);

	//printf("latency-a: %lu\n", pa_simple_get_latency(aStream->istream, NULL));
	//pa_simple_flush(aStream->istream, NULL);
	pa_simple_read(aStream->istream, data, datalen, NULL);

	return datalen;
}

/************************************************************
 * Opens a PortAudio stream with default characteristics.
 * Allocates PABLIO_Stream structure.
 *
 */
pa_error OpenAudioStream(PABLIO_Stream ** rwblPtr, const char * channelName,
						const pa_sample_spec * inputParameters,
						const pa_sample_spec * outputParameters, double sampleRate, long samples_per_packet)
{
	long bytesPerSample = 2;
	pa_error err;
	PABLIO_Stream *aStream;
	int channels = 1;

	if (!(inputParameters || outputParameters)) {
		return -1;
	}

	/* Allocate PABLIO_Stream structure for caller. */
	aStream = (PABLIO_Stream *) malloc(sizeof(PABLIO_Stream));
	switch_assert(aStream);
	memset(aStream, 0, sizeof(PABLIO_Stream));

	/* Open a PulseAudio stream that we will use to communicate with the underlying
	 * audio drivers. */
	if (inputParameters) {
		channels = inputParameters->channels;
		//inputParameters = sampleRate;
		aStream->has_in = 1;
		aStream->istream = pa_simple_new(NULL, "FreeSwitch", PA_STREAM_RECORD, NULL, channelName, inputParameters, NULL, NULL, &err);
		if (!aStream->istream) {
			goto error;
		}
	}

	if (outputParameters) {
		channels = outputParameters->channels;
		//outputParameters->rate = sampleRate;
		aStream->has_out = 1;
		aStream->ostream = pa_simple_new(NULL, "FreeSwitch", PA_STREAM_PLAYBACK, NULL, channelName, outputParameters, NULL, NULL, &err);
		if (!aStream->ostream) {
			goto error;
		}
	}

	aStream->bytesPerFrame = bytesPerSample;
	aStream->channelCount = channels;

	*rwblPtr = aStream;

	return 0;

  error:

	CloseAudioStream(aStream);

	*rwblPtr = NULL;

	return err;
}

/************************************************************/
void CloseAudioStream(PABLIO_Stream * aStream)
{
	if (aStream->has_out) {
		/* If we are writing data, make sure we play everything written. */
		pa_simple_drain(aStream->ostream, NULL);
	}

	if (aStream->has_in && aStream->istream) {
		pa_simple_free(aStream->istream);
		aStream->istream = NULL;
	}

	if (aStream->has_out && aStream->ostream) {
		pa_simple_free(aStream->ostream);
		aStream->ostream = NULL;
	}

	free(aStream);
}

