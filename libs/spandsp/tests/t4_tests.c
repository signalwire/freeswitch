/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_tests.c - ITU T.4 FAX image to and from TIFF file tests
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
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
 * $Id: t4_tests.c,v 1.69.4.1 2009/12/19 09:47:57 steveu Exp $
 */

/*! \file */

/*! \page t4_tests_page T.4 tests
\section t4_tests_page_sec_1 What does it do
These tests exercise the image compression and decompression methods defined
in ITU specifications T.4 and T.6.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

#define IN_FILE_NAME    "../test-data/itu/fax/itutests.tif"
#define OUT_FILE_NAME   "t4_tests_receive.tif"

#define XSIZE           1728

t4_state_t send_state;
t4_state_t receive_state;

/* The following are some test cases from T.4 */
#define FILL_70      "                                                                      "
#define FILL_80      "                                                                                "
#define FILL_100     "                                                                                                    "
#define FILL_670     FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_70
#define FILL_980     FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_80

static const char t4_test_patterns[][1728 + 1] =
{
    "XXXXXX              " FILL_980 "  XXX  XXX X                  " FILL_670 "                        XXXX",
    "XXXXXX              " FILL_980 "     XXX   X                  " FILL_670 "                        XXXX",
    /* Line start should code to V(0). Line middle codes to VR(3) VL(2) V(0). Line end should code to V(0) V(0). */

    " XXXX               " FILL_980 "    XXXXXXX                   " FILL_670 "                        XX  ",
    "XXXXX               " FILL_980 "XX       XX                   " FILL_670 "                        XXXX",
    /* Line start should code to VL(1). Line middle codes to H(7,2). Line end should code to V(0) VR(2) */

    "XXX                 " FILL_980 " XX  XX   XX        XXX       " FILL_670 "                      X     ",
    "                    " FILL_980 " X       XXX   XXXX           " FILL_670 "                      X   XX",
    /* Line start should code to P. Line middle codes to P VL(1) V(0) H(3,4) P. Line end codes to V(0) VL(2) V(0). */

    "XXXXX               " FILL_980 "                              " FILL_670 "                        XXXX",
    "  XXX               " FILL_980 "                              " FILL_670 "                        XX  ",
    /* Line start should code to VR(2). Line end codes to V(0) VL(2) V(0). */

    "         XX         " FILL_980 "                              " FILL_670 "                      X  XXX",
    "XXX       X         " FILL_980 "                              " FILL_670 "                      X     ",
    /* Line start should code to H(0,3) VR(1). Line end codes to V(0) VR(3). */

    "                    " FILL_980 "                              " FILL_670 "                         XX ",
    "                    " FILL_980 "                              " FILL_670 "                            ",
    /* Line end codes to P V(0) a'0. */

    "                    " FILL_980 "                              " FILL_670 "                  XXXXXXXXXX",
    "                    " FILL_980 "                              " FILL_670 "              XXXXXX  XXXXXX",
    /* Line end codes to H(2,6). */

    "                    " FILL_980 "                              " FILL_670 "                   XX  XXXXX",
    "                    " FILL_980 "                              " FILL_670 "                   XX       ",
    /* Line end codes to V(0) H(7,0). */
};

static void dump_image_as_xxx(t4_state_t *state)
{
    uint8_t *s;
    int i;
    int j;
    int k;

    /* Dump the entire image as text 'X's and spaces */
    printf("Image (%d x %d):\n", receive_state.image_width, receive_state.image_length);
    s = state->image_buffer;
    for (i = 0;  i < state->image_length;  i++)
    {
        for (j = 0;  j < state->bytes_per_row;  j++)
        {
            for (k = 0;  k < 8;  k++)
            {
                printf((state->image_buffer[i*state->bytes_per_row + j] & (0x80 >> k))  ?  "X"  :  " ");
            }
        }
        printf("\n");
    }
}
/*- End of function --------------------------------------------------------*/

static void display_page_stats(t4_state_t *s)
{
    t4_stats_t stats;

    t4_get_transfer_statistics(s, &stats);
    printf("Pages = %d\n", stats.pages_transferred);
    printf("Image size = %d pels x %d pels\n", stats.width, stats.length);
    printf("Image resolution = %d pels/m x %d pels/m\n", stats.x_resolution, stats.y_resolution);
    printf("Bad rows = %d\n", stats.bad_rows);
    printf("Longest bad row run = %d\n", stats.longest_bad_row_run);
    printf("Bits per row - min %d, max %d\n", s->min_row_bits, s->max_row_bits);
}
/*- End of function --------------------------------------------------------*/

static int row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    int i;
    int j;
    const char *s;
    static int row = 0;

    /* Send the test pattern. */
    s = t4_test_patterns[row++];
    if (row >= 16)
        return 0;
    memset(buf, 0, len);
    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < 8;  j++)
        {
            if (*s++ != ' ')
                buf[i] |= (0x80 >> j);
        }
    }
    if (*s)
        printf("Oops - '%c' at end of row %d\n", *s, row);
    return len;
}
/*- End of function --------------------------------------------------------*/

static int row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    int i;
    int j;
    const char *s;
    static int row = 0;
    uint8_t ref[8192];

    /* Verify that what is received matches the test pattern. */
    if (len == 0)
        return 0;
    s = t4_test_patterns[row++];
    if (row >= 16)
        row = 0;
    memset(ref, 0, len);
    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < 8;  j++)
        {
            if (*s++ != ' ')
                ref[i] |= (0x80 >> j);
        }
    }
    if (*s)
        printf("Oops - '%c' at end of row %d\n", *s, row);
    if (memcmp(buf, ref, len))
    {
        printf("Test failed at row %d\n", row);
        exit(2);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int detect_page_end(int bit, int page_ended)
{
    static int consecutive_eols;
    static int max_consecutive_eols;
    static int consecutive_zeros;
    static int consecutive_ones;
    static int eol_zeros;
    static int eol_ones;
    static int expected_eols;
    static int end_marks;

    /* Check the EOLs are added properly to the end of an image. We can't rely on the
       decoder giving the right answer, as a full set of EOLs is not needed for the
       decoder to work. */
    if (bit == -1000000)
    {
        /* Reset */
        consecutive_eols = 0;
        max_consecutive_eols = 0;
        consecutive_zeros = 0;
        consecutive_ones = 0;
        end_marks = 0;

        eol_zeros = 11;
        eol_ones = (page_ended == T4_COMPRESSION_ITU_T4_2D)  ?  2  :  1;
        expected_eols = (page_ended == T4_COMPRESSION_ITU_T6)  ?  2  :  6;
        return FALSE;
    }

    /* Monitor whether the EOLs are there in the correct amount */
    if (bit == 0)
    {
        consecutive_zeros++;
        consecutive_ones = 0;
    }
    else if (bit == 1)
    {
        if (++consecutive_ones == eol_ones)
        {
            if (consecutive_eols == 0  &&  consecutive_zeros >= eol_zeros)
                consecutive_eols++;
            else if (consecutive_zeros == eol_zeros)
                consecutive_eols++;
            else
                consecutive_eols = 0;
            consecutive_zeros = 0;
            consecutive_ones = 0;
        }
        if (max_consecutive_eols < consecutive_eols)
            max_consecutive_eols = consecutive_eols;
    }
    else if (bit == SIG_STATUS_END_OF_DATA)
    {
        if (end_marks == 0)
        {
            if (max_consecutive_eols != expected_eols)
            {
                printf("Only %d EOLs (should be %d)\n", max_consecutive_eols, expected_eols);
                return 2;
            }
            consecutive_zeros = 0;
            consecutive_eols = 0;
            max_consecutive_eols = 0;
        }
        if (!page_ended)
        {
            /* We might need to push a few bits to get the receiver to report the
                end of page condition (at least with T.6). */
            if (++end_marks > 50)
            {
                printf("Receiver missed the end of page mark\n");
                return 2;
            }
            return 0;
        }
        return 1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    static const int compression_sequence[] =
    {
        //T4_COMPRESSION_NONE,
        T4_COMPRESSION_ITU_T4_1D,
        T4_COMPRESSION_ITU_T4_2D,
        T4_COMPRESSION_ITU_T6,
        //T4_COMPRESSION_ITU_T85,
        //T4_COMPRESSION_ITU_T43,
        //T4_COMPRESSION_ITU_T45,
        //T4_COMPRESSION_ITU_T81,
        //T4_COMPRESSION_ITU_SYCC_T81
    };
    int sends;
    int page_no;
    int bit;
    int end_of_page;
    int end_marks;
    int res;
    int compression;
    int compression_step;
    int add_page_headers;
    int min_row_bits;
    int restart_pages;
    int block_size;
    char buf[1024];
    uint8_t block[1024];
    const char *in_file_name;
    const char *decode_file_name;
    int opt;
    int i;
    int bit_error_rate;
    int dump_as_xxx;
    int tests_failed;
    unsigned int last_pkt_no;
    unsigned int pkt_no;
    int page_ended;
    FILE *file;

    tests_failed = 0;
    compression = -1;
    compression_step = 0;
    add_page_headers = FALSE;
    restart_pages = FALSE;
    in_file_name = IN_FILE_NAME;
    decode_file_name = NULL;
    /* Use a non-zero default minimum row length to ensure we test the consecutive EOLs part
       properly. */
    min_row_bits = 50;
    block_size = 0;
    bit_error_rate = 0;
    dump_as_xxx = FALSE;
    while ((opt = getopt(argc, argv, "126b:d:ehri:m:x")) != -1)
    {
        switch (opt)
        {
        case '1':
            compression = T4_COMPRESSION_ITU_T4_1D;
            compression_step = -1;
            break;
        case '2':
            compression = T4_COMPRESSION_ITU_T4_2D;
            compression_step = -1;
            break;
        case '6':
            compression = T4_COMPRESSION_ITU_T6;
            compression_step = -1;
            break;
        case 'b':
            block_size = atoi(optarg);
            if (block_size > 1024)
                block_size = 1024;
            break;
        case 'd':
            decode_file_name = optarg;
            break;
        case 'e':
            bit_error_rate = 0x3FF;
            break;
        case 'h':
            add_page_headers = TRUE;
            break;
        case 'r':
            restart_pages = TRUE;
            break;
        case 'i':
            in_file_name = optarg;
            break;
        case 'm':
            min_row_bits = atoi(optarg);
            break;
        case 'x':
            dump_as_xxx = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    /* Create a send and a receive end */
    memset(&send_state, 0, sizeof(send_state));
    memset(&receive_state, 0, sizeof(receive_state));

    end_of_page = FALSE;
    if (decode_file_name)
    {
        if (compression < 0)
            compression = T4_COMPRESSION_ITU_T4_1D;
        /* Receive end puts TIFF to a new file. We assume the receive width here. */
        if (t4_rx_init(&receive_state, OUT_FILE_NAME, T4_COMPRESSION_ITU_T4_2D) == NULL)
        {
            printf("Failed to init T.4 rx\n");
            exit(2);
        }
        span_log_set_level(&receive_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        t4_rx_set_rx_encoding(&receive_state, compression);
        t4_rx_set_x_resolution(&receive_state, T4_X_RESOLUTION_R8);
        //t4_rx_set_y_resolution(&receive_state, T4_Y_RESOLUTION_FINE);
        t4_rx_set_y_resolution(&receive_state, T4_Y_RESOLUTION_STANDARD);
        t4_rx_set_image_width(&receive_state, XSIZE);

        page_no = 1;
        t4_rx_start_page(&receive_state);
        last_pkt_no = 0;
        file = fopen(decode_file_name, "r");
        while (fgets(buf, 1024, file))
        {
            if (sscanf(buf, "HDLC:  FCD: 06 %x", &pkt_no) == 1)
            {
                /* Useful for breaking up T.38 ECM logs */
                for (i = 0;  i < 256;  i++)
                {
                    if (sscanf(&buf[18 + 3*i], "%x", (unsigned int *) &bit) != 1)
                        break;
                    if ((end_of_page = t4_rx_put_byte(&receive_state, bit)))
                        break;
                }
            }
            else if (sscanf(buf, "HDLC:  %x", &pkt_no) == 1)
            {
                /* Useful for breaking up HDLC decodes of ECM logs */
                for (i = 0;  i < 256;  i++)
                {
                    if (sscanf(&buf[19 + 3*i], "%x", (unsigned int *) &bit) != 1)
                        break;
                    if ((end_of_page = t4_rx_put_byte(&receive_state, bit)))
                        break;
                }
            }
            else if (strlen(buf) > 62  &&  sscanf(buf + 57, "Rx %d: IFP %x %x", &pkt_no, (unsigned int *) &bit, (unsigned int *) &bit) == 3)
            {
                /* Useful for breaking up T.38 non-ECM logs */
                if (pkt_no != last_pkt_no + 1)
                    printf("Packet %u\n", pkt_no);
                last_pkt_no = pkt_no;
                for (i = 0;  i < 256;  i++)
                {
                    if (sscanf(&buf[57 + 29 + 3*i], "%x", (unsigned int *) &bit) != 1)
                        break;
                    bit = bit_reverse8(bit);
                    if ((end_of_page = t4_rx_put_byte(&receive_state, bit)))
                        break;
                }
            }
            else if (sscanf(buf, "%08x  %02x %02x %02x", (unsigned int *) &bit, (unsigned int *) &bit, (unsigned int *) &bit, (unsigned int *) &bit) == 4)
            {
                for (i = 0;  i < 16;  i++)
                {
                    if (sscanf(&buf[10 + 3*i], "%x", (unsigned int *) &bit) != 1)
                        break;
                    bit = bit_reverse8(bit);
                    if ((end_of_page = t4_rx_put_byte(&receive_state, bit)))
                        break;
                }
            }
            else if (sscanf(buf, "Rx bit %*d - %d", &bit) == 1)
            {
                if ((end_of_page = t4_rx_put_bit(&receive_state, bit)))
                {
                    printf("End of page detected\n");
                    break;
                }
            }
        }
        fclose(file);
        if (dump_as_xxx)
            dump_image_as_xxx(&receive_state);
        t4_rx_end_page(&receive_state);
        display_page_stats(&receive_state);
        t4_rx_release(&receive_state);
    }
    else
    {
#if 1
        printf("Testing image_function->compress->decompress->image_function\n");
        /* Send end gets image from a function */
        if (t4_tx_init(&send_state, in_file_name, -1, -1) == NULL)
        {
            printf("Failed to init T.4 tx\n");
            exit(2);
        }
        span_log_set_level(&send_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        t4_tx_set_row_read_handler(&send_state, row_read_handler, NULL);
        t4_tx_set_min_row_bits(&send_state, min_row_bits);
        t4_tx_set_local_ident(&send_state, "111 2222 3333");

        /* Receive end puts TIFF to a function. */
        if (t4_rx_init(&receive_state, OUT_FILE_NAME, T4_COMPRESSION_ITU_T4_2D) == NULL)
        {
            printf("Failed to init T.4 rx\n");
            exit(2);
        }
        span_log_set_level(&receive_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        t4_rx_set_row_write_handler(&receive_state, row_write_handler, NULL);
        t4_rx_set_image_width(&receive_state, t4_tx_get_image_width(&send_state));
        t4_rx_set_x_resolution(&receive_state, t4_tx_get_x_resolution(&send_state));
        t4_rx_set_y_resolution(&receive_state, t4_tx_get_y_resolution(&send_state));

        /* Now send and receive the test data with all compression modes. */
        page_no = 1;
        /* If we are stepping around the compression schemes, reset to the start of the sequence. */
        if (compression_step > 0)
            compression_step = 0;
        for (;;)
        {
            end_marks = 0;
            if (compression_step >= 0)
            {
                compression = compression_sequence[compression_step++];
                if (compression_step > 3)
                    break;
            }
            t4_tx_set_tx_encoding(&send_state, compression);
            t4_rx_set_rx_encoding(&receive_state, compression);

            if (t4_tx_start_page(&send_state))
                break;
            t4_rx_start_page(&receive_state);
            do
            {
                bit = t4_tx_get_bit(&send_state);
                if (bit == SIG_STATUS_END_OF_DATA)
                {
                    if (++end_marks > 50)
                    {
                        printf("Receiver missed the end of page mark\n");
                        tests_failed++;
                        break;
                    }
                }
                end_of_page = t4_rx_put_bit(&receive_state, bit & 1);
            }
            while (!end_of_page);
            t4_tx_end_page(&send_state);
            t4_rx_end_page(&receive_state);
            if (compression_step < 0)
                break;
        }
        t4_tx_release(&send_state);
        t4_rx_release(&receive_state);
#endif
#if 1
        printf("Testing TIFF->compress->decompress->TIFF cycle\n");
        /* Send end gets TIFF from a file */
        if (t4_tx_init(&send_state, in_file_name, -1, -1) == NULL)
        {
            printf("Failed to init T.4 send\n");
            exit(2);
        }
        span_log_set_level(&send_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        t4_tx_set_min_row_bits(&send_state, min_row_bits);
        t4_tx_set_local_ident(&send_state, "111 2222 3333");

        /* Receive end puts TIFF to a new file. */
        if (t4_rx_init(&receive_state, OUT_FILE_NAME, T4_COMPRESSION_ITU_T4_2D) == NULL)
        {
            printf("Failed to init T.4 rx for '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        span_log_set_level(&receive_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        t4_rx_set_x_resolution(&receive_state, t4_tx_get_x_resolution(&send_state));
        t4_rx_set_y_resolution(&receive_state, t4_tx_get_y_resolution(&send_state));
        t4_rx_set_image_width(&receive_state, t4_tx_get_image_width(&send_state));

        /* Now send and receive all the pages in the source TIFF file */
        page_no = 1;
        sends = 0;
        /* If we are stepping around the compression schemes, reset to the start of the sequence. */
        if (compression_step > 0)
            compression_step = 0;
        for (;;)
        {
            end_marks = 0;
            /* Add a header line to alternate pages, if required */
            if (add_page_headers  &&  (sends & 2))
                t4_tx_set_header_info(&send_state, "Header");
            else
                t4_tx_set_header_info(&send_state, NULL);
            if (restart_pages  &&  (sends & 1))
            {
                /* Use restart, to send the page a second time */
                if (t4_tx_restart_page(&send_state))
                    break;
            }
            else
            {
                if (compression_step >= 0)
                {
                    compression = compression_sequence[compression_step++];
                    if (compression_step > 2)
                        compression_step = 0;
                }
                t4_tx_set_tx_encoding(&send_state, compression);
                t4_rx_set_rx_encoding(&receive_state, compression);

                if (t4_tx_start_page(&send_state))
                    break;
            }
            t4_rx_start_page(&receive_state);
            detect_page_end(-1000000, compression);
            page_ended = FALSE;
            if (block_size == 0)
            {
                for (;;)
                {
                    bit = t4_tx_get_bit(&send_state);
                    /* Monitor whether the EOLs are there in the correct amount */
                    if ((res = detect_page_end(bit, page_ended)))
                    {
                        tests_failed += (res - 1);
                        break;
                    }
                    if (!page_ended)
                    {
                        if (bit_error_rate)
                        {
                            if ((rand() % bit_error_rate) == 0)
                                bit ^= 1;
                        }
                        if (t4_rx_put_bit(&receive_state, bit & 1))
                            page_ended = TRUE;
                    }
                }
                /* Now throw junk at the receive context, to ensure stuff occuring
                   after the end of page condition has no bad effect. */
                for (i = 0;  i < 1000;  i++)
                {
                    t4_rx_put_bit(&receive_state, (rand() >> 10) & 1);
                }
            }
            else if (block_size == 1)
            {
                do
                {
                    bit = t4_tx_get_byte(&send_state);
                    if ((bit & 0x100))
                    {
                        if (++end_marks > 50)
                        {
                            printf("Receiver missed the end of page mark\n");
                            tests_failed++;
                            break;
                        }
                    }
                    end_of_page = t4_rx_put_byte(&receive_state, bit & 0xFF);
                }
                while (!end_of_page);
            }
            else
            {
                do
                {
                    bit = t4_tx_get_chunk(&send_state, block, block_size);
                    if (bit > 0)
                        end_of_page = t4_rx_put_chunk(&receive_state, block, bit);
                    if (bit < block_size)
                    {
                        if (++end_marks > 50)
                        {
                            printf("Receiver missed the end of page mark\n");
                            tests_failed++;
                            break;
                        }
                    }
                }
                while (!end_of_page);
            }
            if (dump_as_xxx)
                dump_image_as_xxx(&receive_state);
            display_page_stats(&receive_state);
            if (!restart_pages  ||  (sends & 1))
                t4_tx_end_page(&send_state);
            t4_rx_end_page(&receive_state);
            sends++;
        }
        t4_tx_release(&send_state);
        t4_rx_release(&receive_state);
        /* And we should now have a matching received TIFF file. Note this will only match
           at the image level. TIFF files allow a lot of ways to express the same thing,
           so bit matching of the files is not the normal case. */
        fflush(stdout);
        sprintf(buf, "tiffcmp -t %s %s", in_file_name, OUT_FILE_NAME);
        if (tests_failed  ||  system(buf))
        {
            printf("Tests failed\n");
            exit(2);
        }
#endif
        printf("Tests passed\n");
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
