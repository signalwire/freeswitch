/* 
   pa_rec.c
   David Rowe
   July 6 2012

   Records at 48000 Hz from default sound device, convertes to 8 kHz,
   and saves to raw file.  Used to get experience with Portaudio.

   Modified from paex_record.c Portaudio example. Original author
   author Phil Burk http://www.softsynth.com

   To Build:

     gcc paex_rec.c -o paex_rec -lm -lrt -lportaudio -pthread
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
#define NUM_CHANNELS 2             /* I think most sound cards prefer
				      stereo, we will convert to mono
				      as we sample */

/* state information passed to call back */

typedef struct {
    FILE               *fout;
    int                 framesLeft;
    float               in48k[FDMDV_OS_TAPS + N48];
} paTestData;


/* 
   This routine will be called by the PortAudio engine when audio is
   available.  It may be called at interrupt level on some machines so
   don't do anything that could mess up the system like calling
   malloc() or free().
*/

static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    FILE       *fout = data->fout;
    int         framesToCopy;
    int         i;
    int         finished;
    short      *rptr = (short*)inputBuffer;
    float       out8k[N8];
    short       out8k_short[N8];

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if (data->framesLeft < framesPerBuffer) {
        framesToCopy = data->framesLeft;
        finished = paComplete;
    } 
    else {
        framesToCopy = framesPerBuffer;
        finished = paContinue;
    }
    data->framesLeft -= framesToCopy;

    assert(inputBuffer != NULL);

    /* just use left channel */

    for(i=0; i<framesToCopy; i++,rptr+=2)
	data->in48k[i+FDMDV_OS_TAPS] = *rptr; 

    /* downsample and update filter memory */

    fdmdv_48_to_8(out8k, &data->in48k[FDMDV_OS_TAPS], N8);
    for(i=0; i<FDMDV_OS_TAPS; i++)
	data->in48k[i] = data->in48k[i+framesToCopy];

    /* save 8k to disk  */

    for(i=0; i<N8; i++)
	out8k_short[i] = (short)out8k[i];

    /* note Portaudio docs recs. against making systems calls like
       fwrite() in this callback but seems to work OK */
    
    fwrite(out8k_short, sizeof(short), N8, fout);

    return finished;
}

int main(int argc, char *argv[])
{
    PaStreamParameters  inputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;
    int                 numSecs;

    if (argc != 3) {
	printf("usage: %s rawFile time(s)\n", argv[0]);
	exit(0);
    }

    data.fout = fopen(argv[1], "wt");
    if (data.fout == NULL) {
	printf("Error opening output raw file %s\n", argv[1]);
	exit(1);
    }

    numSecs = atoi(argv[2]);
    data.framesLeft = numSecs * SAMPLE_RATE;

    for(i=0; i<FDMDV_OS_TAPS; i++)
	data.in48k[i] = 0.0;

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    printf( "PortAudio version number = %d\nPortAudio version text = '%s'\n",
            Pa_GetVersion(), Pa_GetVersionText() );

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = NUM_CHANNELS;         /* stereo input */
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio --------------------------------------------- */

    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              N48,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
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

    fclose(data.fout);


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

