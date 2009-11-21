/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * broadvoice_tests.c
 *
 * Copyright 2008-2009 Steve Underwood <steveu@coppice.org>
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
 * $Id: broadvoice_tests.c,v 1.2 2009/11/20 13:12:23 steveu Exp $
 */

/*! \file */

/*! \page broadvoice_tests_page BroadVoice 16 and 32 codec tests
\section broadvoice_tests_page_sec_1 What does it do?

\section broadvoice_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <audiofile.h>

#define BROADVOICE_EXPOSE_INTERNAL_STRUCTURES
#include <broadvoice.h>

#include "timing.h"
#include "g192_bit_stream.h"

#define MAX_FRAME_SIZE      80
#define MAX_BITS_PER_FRAME  160

typedef struct
{
    int encode;
    int encoded_format;
    int bit_rate;
    int sample_rate;
    int number_of_bits_per_frame;
    int frame_size;
    char *source_file;
    char *dest_file;
    FILE *fp;
    FILE *fp_bitstream;
} coder_control_t;

static int encode_test(coder_control_t *control, int frames)
{
    bv16_encode_state_t encode_state_16;
    bv16_encode_state_t *s16;
    bv32_encode_state_t encode_state_32;
    bv32_encode_state_t *s32;
    int16_t amp[frames*MAX_FRAME_SIZE];
    uint8_t bv_code[frames*MAX_BITS_PER_FRAME/8];
    int samples;
    int padded_samples;
    int frame_cnt;
    int bytes;
    int actual_frames;
    int i;
    int64_t start;
    int64_t end;
    int64_t total;

    if ((control->fp = fopen(control->source_file, "rb")) == NULL)
    {
        printf("Error opening %s.\n", control->source_file);
        exit(1);
    }
    if ((control->fp_bitstream = fopen(control->dest_file, "wb")) == NULL)
    {
        printf("Error opening %s.\n", control->dest_file);
        exit(1);
    }

    s16 = NULL;
    s32 = NULL;
    if (control->bit_rate == 16000)
    {
        if ((s16 = bv16_encode_init(&encode_state_16)) == NULL)
        {
            printf("Failed to initialise the encoder.\n");
            exit(2);
        }
    }
    else
    {
        if ((s32 = bv32_encode_init(&encode_state_32)) == NULL)
        {
            printf("Failed to initialise the encoder.\n");
            exit(2);
        }
    }
    frame_cnt = 0;
    total = 0;
    for (;;)
    {
        samples = fread(amp, sizeof(int16_t), frames*control->frame_size, control->fp);
        if (samples <= 0)
            break;
        if (samples%control->frame_size != 0)
        {
            padded_samples = samples - samples%control->frame_size + control->frame_size;
            /* Pad this fractional frame out to a full one with silence */
            for (i = samples;  i < padded_samples;  i++)
                amp[i] = 0;
            samples = padded_samples;
        }
        actual_frames = samples/control->frame_size;
        start = rdtscll();
        if (control->bit_rate == 16000)
            bytes = bv16_encode(s16, bv_code, amp, samples);
        else
            bytes = bv32_encode(s32, bv_code, amp, samples);
        end = rdtscll();
        frame_cnt += actual_frames;
        /* Write output bitstream to the output file */
        for (i = 0;  i < actual_frames;  i++)
            itu_codec_bitstream_write(&bv_code[i*bytes/actual_frames], 8*bytes/actual_frames, control->encoded_format, control->fp_bitstream);
        total += (end - start);
    }
    fclose(control->fp);
    fclose(control->fp_bitstream);
    printf("%d frames encoded\n", frame_cnt);
    if (frame_cnt == 0)
        frame_cnt = 1;
    printf("%" PRId64 " cycles. %" PRId64 " per frame\n", total, total/frame_cnt);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int decode_test(coder_control_t *control, int frames)
{
    bv16_decode_state_t decode_state_16;
    bv16_decode_state_t *s16;
    bv32_decode_state_t decode_state_32;
    bv32_decode_state_t *s32;
    int16_t amp[frames*MAX_FRAME_SIZE];
    uint8_t bv_code[frames*MAX_BITS_PER_FRAME/8];
    int bytes;
    int samples;
    int frame_cnt;
    int number_of_bytes_per_frame;
    int actual_frames;
    int i;
    int j;
    int k;
    int n;
    int16_t frame_error_flag;
    int64_t start;
    int64_t end;
    int64_t total;

    if ((control->fp_bitstream = fopen(control->source_file, "rb")) == NULL)
    {
        printf("Error opening %s.\n", control->source_file);
        exit(1);
    }
    if ((control->fp = fopen(control->dest_file, "wb")) == NULL)
    {
        printf("Error opening %s.\n", control->dest_file);
        exit(1);
    }

    number_of_bytes_per_frame = control->number_of_bits_per_frame/8;

    s16 = NULL;
    s32 = NULL;
    if (control->bit_rate == 16000)
    {
        if ((s16 = bv16_decode_init(&decode_state_16)) == NULL)
        {
            printf("Failed to initialise the decoder.\n");
            exit(2);
        }
    }
    else
    {
        if ((s32 = bv32_decode_init(&decode_state_32)) == NULL)
        {
            printf("Failed to initialise the decoder.\n");
            exit(2);
        }
    }

    frame_cnt = 0;
    total = 0;
    frame_error_flag = 0;
    n = 0;
    for (;;)
    {
        for (actual_frames = 0, bytes = 0, i = 0;  i < frames;  i++)
        {
            n = itu_codec_bitstream_read(&bv_code[i*number_of_bytes_per_frame],
                                         &frame_error_flag,
                                         number_of_bytes_per_frame*8,
                                         control->encoded_format,
                                         control->fp_bitstream)/8;
            bytes += n;
            if (n == number_of_bytes_per_frame)
                actual_frames++;
            if (frame_error_flag  ||  n != number_of_bytes_per_frame)
                break;
        }
        if (frame_error_flag  ||  bytes >= number_of_bytes_per_frame)
        {
            if (frame_error_flag)
            {
                samples = 0;
                if (actual_frames > 0)
                {
                    start = rdtscll();
                    if (control->bit_rate == 16000)
                        samples = bv16_decode(s16, amp, bv_code, bytes - number_of_bytes_per_frame);
                    else
                        samples = bv32_decode(s32, amp, bv_code, bytes - number_of_bytes_per_frame);
                    end = rdtscll();
                    total += (end - start);
                }
                j = bytes - number_of_bytes_per_frame;
                if (j < 0)
                    j = 0;
                k = (actual_frames - 1)*control->frame_size;
                if (k < 0)
                    k = 0;
#if 0
                if (control->bit_rate == 16000)
                    samples += bv16_fillin(s16, &amp[k], &bv_code[j], number_of_bytes_per_frame);
                else
                    samples += bv32_fillin(s32, &amp[k], &bv_code[j], number_of_bytes_per_frame);
#else
                if (control->bit_rate == 16000)
                    samples += bv16_fillin(s16, &amp[k], number_of_bytes_per_frame);
                else
                    samples += bv32_fillin(s32, &amp[k], number_of_bytes_per_frame);
#endif
            }
            else
            {
                start = rdtscll();
                if (control->bit_rate == 16000)
                    samples = bv16_decode(s16, amp, bv_code, bytes);
                else
                    samples = bv32_decode(s32, amp, bv_code, bytes);
                end = rdtscll();
                total += (end - start);
            }
            frame_cnt += actual_frames;
            /* For ITU testing, chop off the 2 LSBs. */
            //for (i = 0;  i < samples;  i++)
            //    amp[i] &= 0xFFFC;
            /* Write frame of output samples */
            fwrite(amp, sizeof(int16_t), samples, control->fp);
        }
        if (!frame_error_flag  &&  n != number_of_bytes_per_frame)
            break;
    }
    fclose(control->fp);
    fclose(control->fp_bitstream);
    printf("%d frames decoded\n", frame_cnt);
    if (frame_cnt == 0)
        frame_cnt = 1;
    printf("%" PRId64 " cycles. %" PRId64 " per frame\n", total, total/frame_cnt);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void parse_command_line(char *argv[], coder_control_t *control)
{
    control->encode = (strcasecmp(*++argv, "e") == 0);
    
    if (strcasecmp(*++argv, "p") == 0)
    {
        control->encoded_format = ITU_CODEC_BITSTREAM_PACKED;
        printf("Encoding format = packed bitstream\n");
    }
    else if (strcasecmp(*argv, "i") == 0)
    {
        control->encoded_format = ITU_CODEC_BITSTREAM_G192;
        printf("Encoding format = ITU G.192 format bitstream\n");
    }
    else
    {
        printf("Error. Encoded format must be P for packed, or I for ITU format\n");
        exit(1);
    }
    control->bit_rate = (int32_t) atoi(*++argv);
    control->number_of_bits_per_frame = (int16_t) (control->bit_rate/200);

    
    control->sample_rate = (control->bit_rate == 16000)  ?  8000  :  16000;
    if (control->sample_rate == 8000)
    {
        control->frame_size = MAX_FRAME_SIZE >> 1;

        printf("Sample rate = 8000 (BroadVoice16, 3.4kHz bandwidth)\n");
    }
    else if (control->sample_rate == 16000)
    {
        control->frame_size = MAX_FRAME_SIZE;

        printf("Sample rate = 16000 (BroadVoice32, 7.1kHz bandwidth)\n");
    }
    else
    {
        printf("Error. Sample rate must be 8000 or 16000\n");
        exit(1);
    }
    control->source_file = *++argv;
    control->dest_file = *++argv;

    printf("Bit rate = %d\n", control->bit_rate);
    printf("Framesize = %d samples\n", control->frame_size);
    printf("Number of bits per frame = %d bits\n", control->number_of_bits_per_frame);
    printf("\n");
    printf("\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    coder_control_t control;

    /* Check usage */
    if (argc < 6)
    {
        printf("Usage: %s <E/D> <P(packed)/I(ITU)> <bit rate> <input-file> <output-file>\n\n", argv[0]);
        printf("Sample rate: 3.5kHz  = 8000\n");
        printf("             7.1kHz  = 16000\n");
        printf("\n");
        exit(1);
    }

    parse_command_line(argv, &control);
    if (control.encode)
        encode_test(&control, 4);
    else
        decode_test(&control, 4);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
