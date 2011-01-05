/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * g722_1_tests.c
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1, which is:
 *
 *   (C) 2004 Polycom, Inc.
 *   All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/*! \file */

/*! \page g722_1_tests_page G.722.1 codec tests
\section g722_1_tests_page_sec_1 What does it do?

\section g722_1_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <audiofile.h>

#include <g722_1.h>

#include "timing.h"
#include "g192_bit_stream.h"

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
    g722_1_encode_state_t encode_state;
    g722_1_encode_state_t *s;
    int16_t amp[frames*MAX_FRAME_SIZE];
    uint8_t g722_1_code[frames*MAX_BITS_PER_FRAME/8];
    int samples;
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

    if ((s = g722_1_encode_init(&encode_state, control->bit_rate, control->sample_rate)) == NULL)
    {
        printf("Failed to initialise the encoder.\n");
        exit(2);
    }

    frame_cnt = 0;
    total = 0;
    for (;;)
    {
        samples = fread(amp, sizeof(int16_t), frames*control->frame_size, control->fp);
        if (samples < control->frame_size)
            break;
        actual_frames = samples/control->frame_size;
        start = rdtscll();
        bytes = g722_1_encode(s, g722_1_code, amp, samples);
        end = rdtscll();
        frame_cnt += actual_frames;
        /* Write output bitstream to the output file */
        for (i = 0;  i < actual_frames;  i++)
            itu_codec_bitstream_write(&g722_1_code[i*bytes/actual_frames], 8*bytes/actual_frames, control->encoded_format, control->fp_bitstream);
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
    g722_1_decode_state_t decode_state;
    g722_1_decode_state_t *s;
    int16_t amp[frames*MAX_DCT_LENGTH];
    uint8_t g722_1_code[frames*MAX_BITS_PER_FRAME/8];
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

    if ((s = g722_1_decode_init(&decode_state, control->bit_rate, control->sample_rate)) == NULL)
    {
        printf("Failed to initialise the decoder.\n");
        exit(2);
    }

    frame_cnt = 0;
    total = 0;
    frame_error_flag = 0;
    n = 0;
    for (;;)
    {
        for (actual_frames = 0, bytes = 0, i = 0;  i < frames;  i++)
        {
            n = itu_codec_bitstream_read(&g722_1_code[i*number_of_bytes_per_frame],
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
                    samples = g722_1_decode(s, amp, g722_1_code, bytes - number_of_bytes_per_frame);
                    end = rdtscll();
                    total += (end - start);
                }
                j = bytes - number_of_bytes_per_frame;
                if (j < 0)
                    j = 0;
                k = (actual_frames - 1)*control->frame_size;
                if (k < 0)
                    k = 0;
                samples += g722_1_fillin(s, &amp[k], &g722_1_code[j], number_of_bytes_per_frame);
            }
            else
            {
                start = rdtscll();
                samples = g722_1_decode(s, amp, g722_1_code, bytes);
                end = rdtscll();
                total += (end - start);
            }
            frame_cnt += actual_frames;
            /* For ITU testing, chop off the 2 LSBs. */
            for (i = 0;  i < samples;  i++)
                amp[i] &= 0xFFFC;
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
        printf("Encoding format = ITU-format bitstream\n");
    }
    else
    {
        printf("Error. Encoded format must be P for packed, or I for ITU format\n");
        exit(1);
    }
    control->bit_rate = (int32_t) atoi(*++argv);
    control->number_of_bits_per_frame = (int16_t) ((control->bit_rate)/50);

    control->sample_rate = (int16_t) atoi(*++argv);
    if (control->sample_rate == 16000)
    {
        control->frame_size = MAX_FRAME_SIZE >> 1;

        printf("Sample rate = 16000 (G.722.1, 7kHz bandwidth)\n");
    }
    else if (control->sample_rate == 32000)
    {
        control->frame_size = MAX_FRAME_SIZE;

        printf("Sample rate = 32000 (G.722.1 Annex C, 14kHz bandwidth)\n");
    }
    else
    {
        printf("Error. Sample rate must be 16000 or 32000\n");
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
    if (argc < 7)
    {
        printf("Usage: %s <E/D> <P(packed)/I(ITU)> <bit-rate> <sample rate> <input-file> <output-file>\n\n", argv[0]);
        printf("Valid Rates: 24kbps = 24000\n");
        printf("             32kbps = 32000\n");
        printf("             48kbps = 48000\n");
        printf("\n");
        printf("Sample rate:  7kHz  = 16000\n");
        printf("             14kHz  = 32000\n");
        printf("\n");
        exit(1);
    }

    parse_command_line(argv, &control);
    if (control.encode)
        encode_test(&control, 2);
    else
        decode_test(&control, 2);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
