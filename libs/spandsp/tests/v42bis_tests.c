/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

/*! \page v42bis_tests_page V.42bis tests
\section v42bis_tests_page_sec_1 What does it do?
These tests compress the contents of a file specified on the command line, writing
the compressed data to v42bis_tests.v42bis. They then read back the contents of the
compressed file, decompress, and write the results to v42bis_tests.out. The contents
of this file should exactly match the original file.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "spandsp.h"

#define COMPRESSED_FILE_NAME        "v42bis_tests.v42bis"
#define DECOMPRESSED_FILE_NAME      "v42bis_tests.out"

int in_octets_to_date = 0;
int out_octets_to_date = 0;

static void frame_handler(void *user_data, const uint8_t *buf, int len)
{
    int ret;

    if ((ret = write((intptr_t) user_data, buf, len)) != len)
        fprintf(stderr, "Write error %d/%d\n", ret, errno);
    out_octets_to_date += len;
}

static void data_handler(void *user_data, const uint8_t *buf, int len)
{
    int ret;

    if ((ret = write((intptr_t) user_data, buf, len)) != len)
        fprintf(stderr, "Write error %d/%d\n", ret, errno);
    out_octets_to_date += len;
}

int main(int argc, char *argv[])
{
    int len;
    v42bis_state_t *state_a;
    v42bis_state_t *state_b;
    uint8_t buf[1024];
    int in_fd;
    int v42bis_fd;
    int out_fd;
    int do_compression;
    bool do_decompression;
    bool stutter_compression;
    int stutter_time;
    int seg;
    int opt;
    time_t now;
    const char *argv0;
    const char *original_file;
    const char *compressed_file;
    const char *decompressed_file;

    argv0 = argv[0];
    do_compression = false;
    do_decompression = false;
    stutter_compression = false;
    while ((opt = getopt(argc, argv, "cds")) != -1)
    {
        switch (opt)
        {
        case 'c':
            do_compression = true;
            break;
        case 'd':
            do_decompression = true;
            break;
        case 's':
            stutter_compression = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 1)
    {
        fprintf(stderr, "Usage: %s [-c] [-d] [-s] <in-file> [<out-file>]\n", argv0);
        exit(2);
    }
    if (do_compression)
    {
        original_file = argv[0];
        compressed_file = COMPRESSED_FILE_NAME;
    }
    else
    {
        original_file = NULL;
        compressed_file = argv[0];
    }
    decompressed_file = (argc > 1)  ?  argv[1]  :  DECOMPRESSED_FILE_NAME;
    if (do_compression)
    {
        stutter_time = rand() & 0x3FF;
        if ((in_fd = open(argv[0], O_RDONLY)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", original_file);
            exit(2);
        }
        if ((v42bis_fd = open(compressed_file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", compressed_file);
            exit(2);
        }

        time(&now);
        state_a = v42bis_init(NULL, 3, 512, 6, frame_handler, (void *) (intptr_t) v42bis_fd, 512, data_handler, NULL, 512);
        span_log_set_level(v42bis_get_logging_state(state_a), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
        span_log_set_tag(v42bis_get_logging_state(state_a), "V.42bis");
        //v42bis_compression_control(state_a, V42BIS_COMPRESSION_MODE_ALWAYS);
        in_octets_to_date = 0;
        out_octets_to_date = 0;
        while ((len = read(in_fd, buf, 1024)) > 0)
        {
            seg = 0;
            if (stutter_compression)
            {
                while ((len - seg) >= stutter_time)
                {
                    if (v42bis_compress(state_a, buf + seg, stutter_time))
                    {
                        fprintf(stderr, "Bad return code from compression\n");
                        exit(2);
                    }
                    v42bis_compress_flush(state_a);
                    seg += stutter_time;
                    stutter_time = rand() & 0x3FF;
                }
            }
            if (v42bis_compress(state_a, buf + seg, len - seg))
            {
                fprintf(stderr, "Bad return code from compression\n");
                exit(2);
            }
            in_octets_to_date += len;
        }
        v42bis_compress_flush(state_a);
        printf("%d bytes compressed to %d bytes in %lds\n", in_octets_to_date, out_octets_to_date, time(NULL) - now);
        close(in_fd);
        close(v42bis_fd);
    }

    if (do_decompression)
    {
        /* Now open the files for the decompression. */
        if ((v42bis_fd = open(compressed_file, O_RDONLY)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", compressed_file);
            exit(2);
        }
        if ((out_fd = open(decompressed_file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", decompressed_file);
            exit(2);
        }

        time(&now);
        state_b = v42bis_init(NULL, 3, 512, 6, frame_handler, (void *) (intptr_t) v42bis_fd, 512, data_handler, (void *) (intptr_t) out_fd, 512);
        span_log_set_level(v42bis_get_logging_state(state_b), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
        span_log_set_tag(v42bis_get_logging_state(state_b), "V.42bis");
        in_octets_to_date = 0;
        out_octets_to_date = 0;
        while ((len = read(v42bis_fd, buf, 1024)) > 0)
        {
            if (v42bis_decompress(state_b, buf, len))
            {
                fprintf(stderr, "Bad return code from decompression\n");
                exit(2);
            }
            in_octets_to_date += len;
        }
        v42bis_decompress_flush(state_b);
        printf("%d bytes decompressed to %d bytes in %lds\n", in_octets_to_date, out_octets_to_date, time(NULL) - now);
        close(v42bis_fd);
        close(out_fd);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
