/* 
   pa_play.c
   David Rowe
   July 8 2012

   Converts samples from a 16 bit short 8000 Hz rawfile to 480000Hz
   sample rate and plays them using the default sound device.  Used as
   an intermediate step in Portaudio integration.

   Modified from paex_record.c Portaudio example. Original author
   author Phil Burk http://www.softsynth.com
*/

/*
 * $Id: paex_record.c 1752 2011-09-08 03:21:55Z philburk $
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"
#include "fdmdv.h"

#define SAMPLE_RATE  48000         /* 48 kHz sampling rate rec. as we
				      can trust accuracy of sound
				      card */
#define N8           160           /* processing buffer size at 8 kHz */
#define N48          (N8*FDMDV_OS) /* processing buffer size at 48 kHz */
#define MEM8 (FDMDV_OS_TAPS/FDMDV_OS)
#define NUM_CHANNELS 2             /* I think most sound cards prefer
				      stereo, we will convert to mono
				      as we sample */

/* state information passed to call back */

typedef struct {
    FILE               *fin;
    float               in8k[MEM8 + N8];
} paTestData;


/* 
   This routine will be called by the PortAudio engine when audio is
   required.  It may be called at interrupt level on some machines so
   don't do anything that could mess up the system like calling
   malloc() or free().
*/

static int playCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    FILE       *fin = data->fin;
    int         i, nread;
    int         finished;
    short      *wptr = (short*)outputBuffer;
    float      *in8k = data->in8k;
    float       out48k[N48];
    short       out48k_short[N48];
    short       in8k_short[N8];

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    /* note Portaudio docs recs. against making systems calls like
       fwrite() in this callback but seems to work OK */
    
    nread = fread(in8k_short, sizeof(short), N8, fin);
    if (nread == N8)
	finished = paContinue;
    else
	finished = paComplete;

    for(i=0; i<N8; i++)
	in8k[MEM8+i] = in8k_short[i];

    /* upsample and update filter memory */

    fdmdv_8_to_48(out48k, &in8k[MEM8], N8);
    for(i=0; i<MEM8; i++)
	in8k[i] = in8k[i+N8];

    assert(outputBuffer != NULL);

    /* write signal to both channels */

    for(i=0; i<N48; i++)
	out48k_short[i] = (short)out48k[i];
    for(i=0; i<framesPerBuffer; i++,wptr+=2) {
	wptr[0] = out48k_short[i]; 
	wptr[1] = out48k_short[i]; 
    }

    return finished;
}

int main(int argc, char *argv[])
{
    PaStreamParameters  outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;

    if (argc != 2) {
	printf("usage: %s rawFile\n", argv[0]);
	exit(0);
    }

    data.fin = fopen(argv[1], "rt");
    if (data.fin == NULL) {
	printf("Error opening input raw file %s\n", argv[1]);
	exit(1);
    }

    for(i=0; i<MEM8; i++)
	data.in8k[i] = 0.0;

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default input device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        goto done;
    }
    outputParameters.channelCount = NUM_CHANNELS;         /* stereo input */
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    /* Play some audio --------------------------------------------- */

    err = Pa_OpenStream(
              &stream,
	      NULL,
              &outputParameters,
              SAMPLE_RATE,
              N48,
              paClipOff,      
              playCallback,
              &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(100);
    }
    if( err < 0 ) goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto done;

    fclose(data.fin);


done:
    Pa_Terminate();
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}

