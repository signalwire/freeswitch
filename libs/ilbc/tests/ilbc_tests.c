/*
 * iLBC - a library for the iLBC codec
 *
 * ilbc_tests.c - Test the iLBC low bit rate speech codec.
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * iLBC code supplied in RFC3951.
 *
 * Copyright (C) The Internet Society (2004).
 * All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: ilbc_tests.c,v 1.1.1.1 2008/02/15 12:15:55 steveu Exp $
 */

/*! \file */

/*! \page ilbc_tests_page iLBC codec tests
\section ilbc_tests_page_sec_1 What does it do?

\section ilbc_tests_page_sec_2 How is it used?
To perform a general audio quality test, ilbc_tests should be run. The file ../localtests/short_nb_voice.wav
will be compressed to iLBC data, decompressed, and the resulting audio stored in post_ilbc.wav.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <audiofile.h>

#include "ilbc.h"

#define IN_FILE_NAME            "../localtests/dam9.wav"
#define REF_FILE_NAME           "../localtests/dam9_lpc55.wav"
#define COMPRESS_FILE_NAME      "lpc10_out.lpc10"
#define DECOMPRESS_FILE_NAME    "lpc10_in.lpc10"
#define OUT_FILE_NAME           "post_lpc10.wav"

#define SAMPLE_RATE     8000

/*---------------------------------------------------------------*
 *  Main program to test iLBC encoding and decoding
 *
 *  Usage:
 *    exefile_name.exe <infile> <bytefile> <outfile> <channel>
 *
 *    <infile>   : Input file, speech for encoder (16-bit PCM file)
 *    <bytefile> : Bit stream output from the encoder
 *    <outfile>  : Output file, decoded speech (16-bit PCM file)
 *    <channel>  : Bit error file, optional (16-bit)
 *                     1 - Packet received correctly
 *                     0 - Packet Lost
 *--------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    AFfilehandle inhandle;
    AFfilehandle refhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int outframes;
    int compress_file;
    int decompress_file;
    float x;

    float starttime;
    float runtime;
    float outtime;
    FILE *ifileid;
    FILE *efileid;
    FILE *ofileid;
    FILE *cfileid;
    int16_t data[ILBC_BLOCK_LEN_MAX];
    uint8_t encoded_data[ILBC_NO_OF_BYTES_MAX];
    int16_t decoded_data[ILBC_BLOCK_LEN_MAX];
    int len;
    int16_t pli;
    int16_t mode;
    int blockcount = 0;
    int packetlosscount = 0;

    /* Create structs */
    ilbc_encode_state_t Enc_Inst;
    ilbc_decode_state_t Dec_Inst;

    compress_file = -1;
    decompress_file = -1;
    inhandle = AF_NULL_FILEHANDLE;
    refhandle = AF_NULL_FILEHANDLE;
    outhandle = AF_NULL_FILEHANDLE;
#if 0
    if (!decompress)
    {
        if ((inhandle = afOpenFile(in_file_name, "r", 0)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
        {
            fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
        {
            fprintf(stderr, "    Unexpected sample rate in wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
        {
            fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
        {
            fprintf(stderr, "    Failed to create file setup\n");
            exit(2);
        }

        if ((refhandle = afOpenFile(REF_FILE_NAME, "r", 0)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
        if ((x = afGetFrameSize(refhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
        {
            fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
        if ((x = afGetRate(refhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
        {
            fprintf(stderr, "    Unexpected sample rate in wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
        if ((x = afGetChannels(refhandle, AF_DEFAULT_TRACK)) != 1.0)
        {
            fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
    }
    else
    {
        if ((decompress_file = open(DECOMPRESS_FILE_NAME, O_RDONLY)) < 0)
        {
            fprintf(stderr, "    Cannot open decompressed data file '%s'\n", DECOMPRESS_FILE_NAME);
            exit(2);
        }
    }

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
#endif

    if ((argc != 5)  &&  (argc != 6))
    {
        fprintf(stderr,
                "\n*-----------------------------------------------*\n");
        fprintf(stderr,
                "   %s <20,30> input encoded decoded (channel)\n\n",
                argv[0]);
        fprintf(stderr,
                "   mode    : Frame size for the encoding/decoding\n");
        fprintf(stderr,
                "                 20 - 20 ms\n");
        fprintf(stderr,
                "                 30 - 30 ms\n");
        fprintf(stderr,
                "   input   : Speech for encoder (16-bit pcm file)\n");
        fprintf(stderr,
                "   encoded : Encoded bit stream\n");
        fprintf(stderr,
                "   decoded : Decoded speech (16-bit pcm file)\n");
        fprintf(stderr,
                "   channel : Packet loss pattern, optional (16-bit)\n");
        fprintf(stderr,
                "                  1 - Packet received correctly\n");
        fprintf(stderr,
                "                  0 - Packet Lost\n");
        fprintf(stderr,
                "*-----------------------------------------------*\n\n");
        exit(1);
    }
    mode = atoi(argv[1]);
    if (mode != 20  &&  mode != 30)
    {
        fprintf(stderr,"Wrong mode %s, must be 20, or 30\n",
                argv[1]);
        exit(2);
    }
    if ((ifileid = fopen(argv[2],"rb")) == NULL)
    {
        fprintf(stderr,"Cannot open input file %s\n", argv[2]);
        exit(2);
    }
    if ((efileid = fopen(argv[3],"wb")) == NULL)
    {
        fprintf(stderr, "Cannot open encoded file %s\n",
                argv[3]);
        exit(1);
    }
    if ((ofileid = fopen(argv[4],"wb")) == NULL)
    {
        fprintf(stderr, "Cannot open decoded file %s\n",
                argv[4]);
        exit(1);
    }
    if (argc == 6)
    {
        if( (cfileid=fopen(argv[5],"rb")) == NULL)
        {
            fprintf(stderr, "Cannot open channel file %s\n",
                    argv[5]);
            exit(1);
        }
    }
    else
    {
        cfileid=NULL;
    }

    /* print info */

    fprintf(stderr, "\n");
    fprintf(stderr,
            "*---------------------------------------------------*\n");
    fprintf(stderr,
            "*                                                   *\n");
    fprintf(stderr,
            "*      iLBC test program                            *\n");
    fprintf(stderr,
            "*                                                   *\n");
    fprintf(stderr,
            "*                                                   *\n");
    fprintf(stderr,
            "*---------------------------------------------------*\n");
    fprintf(stderr, "\nMode           : %2d ms\n", mode);
    fprintf(stderr, "Input file     : %s\n", argv[2]);
    fprintf(stderr, "Encoded file   : %s\n", argv[3]);
    fprintf(stderr, "Output file    : %s\n", argv[4]);
    if (argc == 6)
        fprintf(stderr,"Channel file   : %s\n", argv[5]);
    fprintf(stderr, "\n");

    /* Initialization */

    ilbc_encode_init(&Enc_Inst, mode);
    ilbc_decode_init(&Dec_Inst, mode, 1);

    /* Runtime statistics */
    starttime = clock()/(float)CLOCKS_PER_SEC;

    /* Loop over input blocks */
    while (fread(data, sizeof(int16_t), Enc_Inst.blockl, ifileid) == Enc_Inst.blockl)
    {
        blockcount++;

        /* Encoding */
        fprintf(stderr, "--- Encoding block %i --- ",blockcount);
        len = ilbc_encode(&Enc_Inst, encoded_data, data, Enc_Inst.blockl);
        fprintf(stderr, "\r");

        /* Write byte file */
        fwrite(encoded_data, sizeof(uint8_t), len, efileid);

        /* Get channel data if provided */
        if (argc == 6)
        {
            if (fread(&pli, sizeof(int16_t), 1, cfileid))
            {
                if ((pli != 0)  &&  (pli != 1))
                {
                    fprintf(stderr, "Error in channel file\n");
                    exit(0);
                }
                if (pli == 0)
                {
                    /* Packet loss -> remove info from frame */
                    memset(encoded_data, 0, len);
                    packetlosscount++;
                }
            }
            else
            {
                fprintf(stderr, "Error. Channel file too int16_t\n");
                exit(0);
            }
        }
        else
        {
            pli = 1;
        }

        /* Decoding */
        fprintf(stderr, "--- Decoding block %i --- ", blockcount);

        if (pli)
            len = ilbc_decode(&Dec_Inst, decoded_data, encoded_data, len);
        else
            len = ilbc_fillin(&Dec_Inst, decoded_data, len);
        fprintf(stderr, "\r");

        /* Write output file */
        fwrite(decoded_data, sizeof(int16_t), len, ofileid);
    }

    /* Runtime statistics */
    runtime = (float) (clock()/(float) CLOCKS_PER_SEC - starttime);
    outtime = (float) ((float) blockcount*(float) mode/1000.0f);
    printf("\n\nLength of speech file: %.1f s\n", outtime);
    printf("Packet loss          : %.1f%%\n",
           100.0f*(float) packetlosscount/(float) blockcount);
    printf("Time to run iLBC     :");
    printf(" %.1f s (%.1f%% of realtime)\n\n", runtime, 100.0f*runtime/outtime);

    /* close files */
    fclose(ifileid);
    fclose(efileid);
    fclose(ofileid);
    if (argc == 6)
        fclose(cfileid);
    return(0);
}
