/*
 * $Id: pablio.c 1151 2006-11-29 02:11:16Z leland_lucius $
 * pablio.c
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

#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <pulse/timeval.h>
#include <pulse/simple.h>
#include "pablio.h"

long WriteAudioStream(PABLIO_Stream * aStream, void *data, size_t datalen, switch_timer_t *timer)
{
	pa_simple_write(aStream->ostream, data, datalen, NULL);
	switch_core_timer_next(timer);

	return datalen;
}

long ReadAudioStream(PABLIO_Stream * aStream, void *data, size_t datalen, switch_timer_t *timer)
{
	pa_simple_read(aStream->istream, data, datalen, NULL);
	switch_core_timer_next(timer);

	return datalen;
}

/************************************************************
 * Opens a PulseAudio stream with default characteristics.
 * Allocates PABLIO_Stream structure.
 */
pa_error OpenAudioStream(PABLIO_Stream ** rwblPtr,
						const char *appName,
						const char *channelName,
						const pa_sample_spec * inputParameters,
						const pa_sample_spec * outputParameters)
{
	long bytesPerSample = 2;
	pa_error err;
	PABLIO_Stream *aStream;
	int channels = 1;
	int latency_msec = 40;
	pa_buffer_attr buffer_attr;

	if (!(inputParameters || outputParameters)) {
		return -1;
	}

	/* Allocate PABLIO_Stream structure for caller. */
	aStream = (PABLIO_Stream *) malloc(sizeof(PABLIO_Stream));
	switch_assert(aStream);
	memset(aStream, 0, sizeof(PABLIO_Stream));

	/* Open a PulseAudio stream that will be used to communicate with the underlying
	 * audio drivers. */
	bzero(&buffer_attr, sizeof(buffer_attr));
	buffer_attr.prebuf = (uint32_t) -1;
	buffer_attr.minreq = (uint32_t) -1;
	buffer_attr.maxlength = (uint32_t) -1;

	if (inputParameters) {
		buffer_attr.fragsize = pa_usec_to_bytes(latency_msec * PA_USEC_PER_MSEC, inputParameters);
		channels = inputParameters->channels;
		aStream->istream = pa_simple_new(NULL, appName, PA_STREAM_RECORD, NULL, channelName, inputParameters, NULL, &buffer_attr, &err);
		if (!aStream->istream) {
			goto error;
		}
	}

	if (outputParameters) {
		channels = outputParameters->channels;
		buffer_attr.tlength = pa_usec_to_bytes(latency_msec * PA_USEC_PER_MSEC, outputParameters);
		aStream->ostream = pa_simple_new(NULL, appName, PA_STREAM_PLAYBACK, NULL, channelName, outputParameters, NULL, &buffer_attr, &err);
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

void FlushAudioStream(PABLIO_Stream * aStream)
{
	if (aStream && aStream->istream) {
		pa_simple_flush(aStream->istream, NULL);
	}
}

void CloseAudioStream(PABLIO_Stream * aStream)
{
	if (aStream->ostream) {
		/* If we are writing data, make sure we play everything written. */
		pa_simple_drain(aStream->ostream, NULL);
	}

	if (aStream->istream) {
		pa_simple_free(aStream->istream);
		aStream->istream = NULL;
	}

	if (aStream->ostream) {
		pa_simple_free(aStream->ostream);
		aStream->ostream = NULL;
	}

	free(aStream);
}
