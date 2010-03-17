/***********************************************************************
Copyright (c) 2006-2010, Skype Limited. All rights reserved. 
Redistribution and use in source and binary forms, with or without 
modification, (subject to the limitations in the disclaimer below) 
are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright 
notice, this list of conditions and the following disclaimer in the 
documentation and/or other materials provided with the distribution.
- Neither the name of Skype Limited, nor the names of specific 
contributors, may be used to endorse or promote products derived from 
this software without specific prior written permission.
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED 
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
CONTRIBUTORS ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

/*****************************/
/* Silk encoder test program */
/*****************************/

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE    1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SKP_Silk_SDK_API.h"

/* Define codec specific settings */
#define MAX_BYTES_PER_FRAME     250 // Equals peak bitrate of 100 kbps 
#define MAX_INPUT_FRAMES        5
#define MAX_LBRR_DELAY          2
#define MAX_FRAME_LENGTH        480

static void print_usage( char* argv[] ) {
    printf( "\nusage: %s in.pcm out.bit [settings]\n", argv[ 0 ] );
    printf( "\nin.pcm              : Speech input to encoder" );
    printf( "\nout.bit             : Bitstream output from encoder" );
    printf( "\n   settings:" );
    printf( "\n-fs <kHz>           : Sampling rate in kHz, default: 24" );
    printf( "\n-packetlength <ms>  : Packet interval in ms, default: 20" );
    printf( "\n-rate <bps>         : Target bitrate; default: 25000" );
    printf( "\n-loss <perc>        : Uplink loss estimate, in percent (0-100); default: 0" );
    printf( "\n-inbandFEC <flag>   : Enable inband FEC usage (0/1); default: 0" );
    printf( "\n-complexity <comp>  : Set complexity, 0: low, 1: medium, 2: high; default: 2" );
    printf( "\n-DTX <flag>         : Enable DTX (0/1); default: 0" );
    printf( "\n-quiet              : Print only some basic values" );
    printf( "\n");
}

int main( int argc, char* argv[] )
{
    size_t    counter;
    SKP_int   k, args, totPackets, totActPackets, ret;
    SKP_int16 nBytes;
    double    sumBytes, sumWBytes, sumActBytes, avg_rate, act_rate, wght_rate, nrg;
    SKP_uint8 payload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES ];
    SKP_int16 in[ MAX_FRAME_LENGTH * MAX_INPUT_FRAMES ];
    char      speechInFileName[ 150 ], bitOutFileName[ 150 ];
    FILE      *bitOutFile, *speechInFile;
    SKP_int32 encSizeBytes;
    void      *psEnc;

    /* default settings */
    SKP_int   fs_kHz = 24;
    SKP_int   targetRate_bps = 25000;
    SKP_int   packetSize_ms = 20;
    SKP_int   frameSizeReadFromFile_ms = 20;
    SKP_int   packetLoss_perc = 0, complexity_mode = 2, smplsSinceLastPacket;
    SKP_int   INBandFec_enabled = 0, DTX_enabled = 0, quiet = 0;
    SKP_SILK_SDK_EncControlStruct encControl; // Struct for input to encoder
        
    if( argc < 3 ) {
        print_usage( argv );
        exit( 0 );
    } 
    
    /* get arguments */
    args = 1;
    strcpy( speechInFileName, argv[ args ] );
    args++;
    strcpy( bitOutFileName,   argv[ args ] );
    args++;
    while( args < argc ) {
        if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-fs" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &fs_kHz );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-packetlength" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &packetSize_ms );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-rate" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &targetRate_bps );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-loss" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &packetLoss_perc );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-complexity" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &complexity_mode );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-inbandFEC" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &INBandFec_enabled );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-DTX") == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &DTX_enabled );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-quiet" ) == 0 ) {
            quiet = 1;
            args++;
        } else {
            printf( "Error: unrecognized setting: %s\n\n", argv[ args ] );
            print_usage( argv );
            exit( 0 );
        }
    }

    /* Print options */
    if( !quiet ) {
        printf("******************* Silk Encoder v %s ****************\n", SKP_Silk_SDK_get_version());
        printf("******************* Compiled for %d bit cpu ********* \n", (int)sizeof(void*) * 8 );
        printf( "Input:                       %s\n",        speechInFileName );
        printf( "Output:                      %s\n",        bitOutFileName );
        printf( "Sampling rate:               %d kHz\n",    fs_kHz);
        printf( "Packet interval:             %d ms\n",     packetSize_ms);
        printf( "Inband FEC used:             %d\n",        INBandFec_enabled);
        printf( "DTX used:                    %d\n",        DTX_enabled);
        printf( "Complexity:                  %d\n",        complexity_mode);
        printf( "Target bitrate:              %d bps\n",    targetRate_bps);
    }

    /* Open files */
    speechInFile = fopen( speechInFileName, "rb" );
    if( speechInFile == NULL ) {
        printf( "Error: could not open input file %s\n", speechInFileName );
        exit( 0 );
    }
    bitOutFile = fopen( bitOutFileName, "wb" );
    if( bitOutFile == NULL ) {
        printf( "Error: could not open output file %s\n", bitOutFileName );
        exit( 0 );
    }

    /* Create Encoder */
    ret = SKP_Silk_SDK_Get_Encoder_Size( &encSizeBytes );
    if( ret ) {
        printf( "\nSKP_Silk_create_encoder returned %d", ret );
    }

    psEnc = malloc( encSizeBytes );
    
    /* Reset Encoder */
    ret = SKP_Silk_SDK_InitEncoder( psEnc, &encControl );
    if( ret ) {
        printf( "\nSKP_Silk_reset_encoder returned %d", ret );
    }
    
    /* Set Encoder parameters */
    encControl.sampleRate           = fs_kHz * 1000;
    encControl.packetSize           = packetSize_ms * fs_kHz;
    encControl.packetLossPercentage = packetLoss_perc;
    encControl.useInBandFEC         = INBandFec_enabled;
    encControl.useDTX               = DTX_enabled;
    encControl.complexity           = complexity_mode;
    encControl.bitRate              = targetRate_bps;

    if( fs_kHz > 24 || fs_kHz < 0 ) {
        printf( "\nError: Sampling rate = %d out of range, valid range 8 - 24 \n \n", fs_kHz );
        exit( 0 );
    }

    totPackets           = 0;
    totActPackets        = 0;
    smplsSinceLastPacket = 0;
    sumBytes             = 0.0;
    sumActBytes          = 0.0;
    sumWBytes            = 0.0;
    
    while( 1 ) {
        /* Read input to file */
        counter = fread( in, sizeof( SKP_int16 ), frameSizeReadFromFile_ms * fs_kHz, speechInFile );
        if( (SKP_int)counter < frameSizeReadFromFile_ms * fs_kHz ) {
            break;
        }

        /* max payload size */
        nBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;

        /* Silk Encoder */
        ret = SKP_Silk_SDK_Encode( psEnc, &encControl, in, (SKP_int16)counter, payload, &nBytes );
        if( ret ) {
            printf( "\nSKP_Silk_Encode returned %d", ret );
            break;
        }

        /* Get packet size */
        packetSize_ms = (SKP_int)( 1000 * encControl.packetSize ) / encControl.sampleRate;

        smplsSinceLastPacket += (SKP_int)counter;
        
        if( ( smplsSinceLastPacket / fs_kHz ) == packetSize_ms ) {
            /* Sends a dummy zero size packet in case of DTX period  */
            /* to make it work with the decoder test program.        */
            /* In practice should be handled by RTP sequence numbers */
            totPackets++;
            sumBytes  += nBytes;
            sumWBytes += pow( (double)nBytes, 10.0 );
            nrg = 0.0;
            for( k = 0; k < (SKP_int)counter; k++ ) {
                nrg += in[ k ] * (double)in[ k ];
            }
            if( ( nrg / (SKP_int)counter ) > 1e3 ) {
                sumActBytes += nBytes;
                totActPackets++;
            }

            /* Write payload size */
            fwrite( &nBytes, sizeof( SKP_int16 ), 1, bitOutFile );

            /* Write payload */
            fwrite( payload, sizeof( SKP_uint8 ), nBytes, bitOutFile );
        
            if( !quiet ) {
                fprintf( stderr, "\rPackets encoded:              %d", totPackets );
            }
            smplsSinceLastPacket = 0;
        }
    }

    /* Write dummy because it can not end with 0 bytes */
    nBytes = -1;

    /* Write payload size */
    fwrite( &nBytes, sizeof( SKP_int16 ), 1, bitOutFile );

    /* Query encoder */
    packetSize_ms = (SKP_int)( 1000 * encControl.packetSize ) / encControl.sampleRate;

    /* Free Encoder */
    free( psEnc );

    fclose( speechInFile );
    fclose( bitOutFile   );

    avg_rate  = 8.0 / packetSize_ms * sumBytes       / totPackets;
    act_rate  = 8.0 / packetSize_ms * sumActBytes    / totActPackets;
    wght_rate = 8.0 / packetSize_ms * pow( sumWBytes / totPackets, 0.1 );
    if( !quiet ) {
        printf( "\nAverage bitrate:             %.3f kbps", avg_rate  );
        printf( "\nActive bitrate:              %.3f kbps", act_rate  );
        printf( "\nWeighted bitrate:            %.3f kbps", wght_rate );
        printf( "\n\n" );
    } else {
        /* print average and weighted bitrates */
        printf( "%.3f %.3f %.3f \n", avg_rate, act_rate, wght_rate );
    }
    return 0;
}
