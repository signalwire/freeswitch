/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16_tests.c - 
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from code which is
 * Copyright 2000-2009 Broadcom Corporation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: bv16_tests.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BROADVOICE_EXPOSE_INTERNAL_STRUCTURES
#include "broadvoice.h"
#include "g192_bit_stream.h"

#define G192BITSTREAM

int frame;
int16_t bfi = 0;

static void usage(const char *name)
{
    fprintf(stderr, "usage: %s enc|dec input output\n", name);
    fprintf(stderr, "\nFormat for speech_file:\n    Binary file of 8 kHz sampled 16-bit PCM data.\n");
#if defined(G192BITSTREAM)
    fprintf(stderr, "\nFormat for bitstream_file per frame: ITU-T G.192 format\n\
                       One (2-byte) synchronization word [0x6B21],\n\
                       One (2-byte) size word,\n\
                       160 words (2-byte) containing 160 bits.\n\n");
#else
    fprintf(stderr, "\nFormat for bitstream_file per frame: Packed Bits\n");
#endif
    exit(1);
}

int main(int argc, char **argv)
{
    FILE *fi;
    FILE *fo;
    FILE *fbdi = NULL;
    int enc = 1;
    int nread;
    int i;
    int len;
    int16_t x[BV16_FRAME_LEN];
    bv16_encode_state_t *cs;
    bv16_decode_state_t *ds;
    uint8_t PackedStream[10];
    int next_bad_frame = -1;
    int packing;

    if ((argc != 4)  &&  (argc != 5))
        usage(argv[0]);
    if (!strcmp(argv[1], "enc"))
        enc = 1;
    else if (!strcmp(argv[1], "dec"))
        enc = 0;
    else
        usage(argv[0]);

    if (!(fi = fopen(argv[2], "rb")))
    {
        fprintf(stderr, "error: can't read %s\n", argv[2]);
        exit(2);
    }
    if (!(fo = fopen(argv[3], "wb")))
    {
        fprintf(stderr, "error: can't write to %s\n", argv[3]);
        exit(3);
    }
    if (argc == 5)
    {
        if (!(fbdi = fopen(argv[4], "rb")))
        {
            fprintf(stderr, "error: can't read %s\n", argv[4]);
            exit(3);
        }
    }

    if (enc)
    {
        fprintf(stderr, " BroadVoice16 Encoder V1.0 with ITU-T G.192\n");
        fprintf(stderr, " Input speech file     : %s\n", argv[2]);
        fprintf(stderr, " Output bit-stream file: %s\n", argv[3]);
    }
    else
    {
        fprintf(stderr, " BroadVoice16 Decoder V1.0 with ITU-T G.192\n");
        fprintf(stderr, " Input bit-stream file : %s\n", argv[2]);
        fprintf(stderr, " Output speech file    : %s\n", argv[3]);
    }

#if defined(G192BITSTREAM)
    packing = ITU_CODEC_BITSTREAM_G192;
#else
    packing = ITU_CODEC_BITSTREAM_PACKED;
#endif

    cs = NULL;
    ds = NULL;
    if (enc)
        cs = bv16_encode_init(NULL);
    else
        ds = bv16_decode_init(NULL);

    frame = 0;
    /* Read for the 1st bad frame */
    if (fbdi != NULL)
        fscanf(fbdi, "%d", &next_bad_frame);

    for (;;)
    {
        frame++;

        /* Read one speech frame */
        if (enc == 1)
        {
            nread = fread(x, sizeof(int16_t), BV16_FRAME_LEN, fi);
            if (nread <= 0)
                break;
            for (i = nread;  i < BV16_FRAME_LEN;  i++)
                x[i] = 0;

            len = bv16_encode(cs, PackedStream, x, BV16_FRAME_LEN);
            itu_codec_bitstream_write(PackedStream, 8*len, packing, fo);
        }
        else
        {
            nread = itu_codec_bitstream_read(PackedStream, &bfi, 80, packing, fi);
            if (nread <= 0)
                break;
            if (frame == next_bad_frame)
            {
                fscanf(fbdi, "%d", &next_bad_frame);
                bfi = 1;
            }

            if (bfi)
                len = bv16_fillin(ds, x, BV16_FRAME_LEN);
            else
                len = bv16_decode(ds, x, PackedStream, 10);
            fwrite(x, sizeof(int16_t), len, fo);
        }
    }

    if (enc)
        bv16_encode_free(cs);
    else
        bv16_decode_free(ds);

    fprintf(stderr, "\r %d %d-sample frames processed.\n", --frame, BV16_FRAME_LEN);

    fclose(fi);
    fclose(fo);

    if (fbdi != NULL)
        fclose(fbdi);

    fprintf(stderr, "\n\n");

    return 0;
}
