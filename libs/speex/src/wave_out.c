/* Copyright (c) 2002, John Edwards

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Set TABS = 4 */
/********************************************************************

 function: To provide playback of 16 bit PCM wave data in Win32
           environments from decoded compressed files.

 ********************************************************************/

#if defined WIN32 || defined _WIN32

#include <string.h>
#include <errno.h>
#include "wave_out.h"

#define MAXWAVESIZE     4294967040LU
#define MAX_WAVEBLOCKS    32

// This is modified for USE_WIN_AUDIO - ONLY 2002-02-27


static CRITICAL_SECTION  cs;
static HWAVEOUT          dev                    = NULL;
static int               ScheduledBlocks        = 0;
static int               PlayedWaveHeadersCount = 0;          // free index
static WAVEHDR*          PlayedWaveHeaders [MAX_WAVEBLOCKS];

static int
Box ( const char* msg )
{
	MessageBox ( NULL, msg, " "VERSION_STRING": Error Message . . .", MB_OK | MB_ICONEXCLAMATION );
	return -1;
}


/*
 *  This function registers already played WAVE chunks. Freeing is done by free_memory(),
 */

static void CALLBACK
wave_callback ( HWAVE hWave, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 )
{
	if ( uMsg == WOM_DONE ) {
		EnterCriticalSection ( &cs );
		PlayedWaveHeaders [PlayedWaveHeadersCount++] = (WAVEHDR*) dwParam1;
		LeaveCriticalSection ( &cs );
	}
}


static void
free_memory ( void )
{
	WAVEHDR*  wh;
	HGLOBAL   hg;

	EnterCriticalSection ( &cs );
	wh = PlayedWaveHeaders [--PlayedWaveHeadersCount];
	ScheduledBlocks--;                               // decrease the number of USED blocks
	LeaveCriticalSection ( &cs );

	waveOutUnprepareHeader ( dev, wh, sizeof (WAVEHDR) );

	hg = GlobalHandle ( wh -> lpData );       // Deallocate the buffer memory
	GlobalUnlock (hg);
	GlobalFree   (hg);

	hg = GlobalHandle ( wh );                 // Deallocate the header memory
	GlobalUnlock (hg);
	GlobalFree   (hg);
}


Int
Set_WIN_Params ( FILE_T   dummyFile ,
                 Ldouble  SampleFreq,
                 Uint     BitsPerSample,
                 Uint     Channels )
{
	WAVEFORMATEX  outFormat;
	UINT          deviceID = WAVE_MAPPER;

	(void) dummyFile;

	if ( waveOutGetNumDevs () == 0 )
		return Box ( "No audio device present." );

	outFormat.wFormatTag      = WAVE_FORMAT_PCM;
	outFormat.wBitsPerSample  = BitsPerSample;
	outFormat.nChannels       = Channels;
	outFormat.nSamplesPerSec  = (unsigned long)(SampleFreq + 0.5);
	outFormat.nBlockAlign     = (outFormat.wBitsPerSample + 7) / 8 * outFormat.nChannels;
	outFormat.nAvgBytesPerSec = outFormat.nSamplesPerSec * outFormat.nBlockAlign;

	switch ( waveOutOpen ( &dev, deviceID, &outFormat, (DWORD)wave_callback, 0, CALLBACK_FUNCTION ) )
	{
		case MMSYSERR_ALLOCATED:   return Box ( "Device is already open." );
		case MMSYSERR_BADDEVICEID: return Box ( "The specified device is out of range." );
		case MMSYSERR_NODRIVER:    return Box ( "There is no audio driver in this system." );
		case MMSYSERR_NOMEM:       return Box ( "Unable to allocate sound memory." );
		case WAVERR_BADFORMAT:     return Box ( "This audio format is not supported." );
		case WAVERR_SYNC:          return Box ( "The device is synchronous." );
		default:                   return Box ( "Unknown media error." );
		case MMSYSERR_NOERROR:     break;
	}

	waveOutReset ( dev );
	InitializeCriticalSection ( &cs );
	SetPriorityClass ( GetCurrentProcess (), HIGH_PRIORITY_CLASS );
	return 0;
}


int
WIN_Play_Samples ( const void* data, size_t len )
{
	HGLOBAL    hg;
	HGLOBAL    hg2;
	LPWAVEHDR  wh;
	void*      allocptr;

	do {
		while ( PlayedWaveHeadersCount > 0 )                // free used blocks ...
			free_memory ();

		if ( ScheduledBlocks < sizeof(PlayedWaveHeaders)/sizeof(*PlayedWaveHeaders) ) // wait for a free block ...
			break;
		Sleep (26);
	} while (1);

	if ( (hg2 = GlobalAlloc ( GMEM_MOVEABLE, len )) == NULL )   // allocate some memory for a copy of the buffer
		return Box ( "GlobalAlloc failed." );

	allocptr = GlobalLock (hg2);
	CopyMemory ( allocptr, data, len );                         // Here we can call any modification output functions we want....

	if ( (hg = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof (WAVEHDR))) == NULL ) // now make a header and WRITE IT!
		return -1;

	wh                   = GlobalLock (hg);
	wh -> dwBufferLength = len;
	wh -> lpData         = allocptr;

	if ( waveOutPrepareHeader ( dev, wh, sizeof (WAVEHDR)) != MMSYSERR_NOERROR ) {
		GlobalUnlock (hg);
		GlobalFree   (hg);
		return -1;
	}

	if ( waveOutWrite ( dev, wh, sizeof (WAVEHDR)) != MMSYSERR_NOERROR ) {
		GlobalUnlock (hg);
		GlobalFree   (hg);
		return -1;
	}

	EnterCriticalSection ( &cs );
	ScheduledBlocks++;
	LeaveCriticalSection ( &cs );

	return len;
}


int
WIN_Audio_close ( void )
{
	if ( dev != NULL ) {

		while ( ScheduledBlocks > 0 ) {
			Sleep (ScheduledBlocks);
			while ( PlayedWaveHeadersCount > 0 )         // free used blocks ...
				free_memory ();
		}

		waveOutReset (dev);      // reset the device
		waveOutClose (dev);      // close the device
		dev = NULL;
	}

	DeleteCriticalSection ( &cs );
	ScheduledBlocks = 0;
	return 0;
}

#endif

/* end of wave_out.c */
