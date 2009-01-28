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
 *
 * $Id: v42bis_tests.c,v 1.24 2008/11/15 14:43:08 steveu Exp $
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
#include <config.h>
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

#include "spandsp/private/v42bis.h"

#define COMPRESSED_FILE_NAME        "v42bis_tests.v42bis"
#define OUTPUT_FILE_NAME            "v42bis_tests.out"

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
    v42bis_state_t state_a;
    v42bis_state_t state_b;
    uint8_t buf[1024];
    int in_fd;
    int v42bis_fd;
    int out_fd;
    int do_compression;
    int do_decompression;
    time_t now;

    do_compression = TRUE;
    do_decompression = TRUE;
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(2);
    }
    if (do_compression)
    {
        if ((in_fd = open(argv[1], O_RDONLY)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", argv[1]);
            exit(2);
        }
        if ((v42bis_fd = open(COMPRESSED_FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", COMPRESSED_FILE_NAME);
            exit(2);
        }

        time(&now);
        v42bis_init(&state_a, 3, 512, 6, frame_handler, (void *) (intptr_t) v42bis_fd, 512, data_handler, NULL, 512);
        v42bis_compression_control(&state_a, V42BIS_COMPRESSION_MODE_ALWAYS);
        in_octets_to_date = 0;
        out_octets_to_date = 0;
        while ((len = read(in_fd, buf, 1024)) > 0)
        {
            if (v42bis_compress(&state_a, buf, len))
            {
                fprintf(stderr, "Bad return code from compression\n");
                exit(2);
            }
            in_octets_to_date += len;
        }
        v42bis_compress_flush(&state_a);
        printf("%d bytes compressed to %d bytes in %lds\n", in_octets_to_date, out_octets_to_date, time(NULL) - now);
        close(in_fd);
        close(v42bis_fd);
    }

    if (do_decompression)
    {
        /* Now open the files for the decompression. */
        if ((v42bis_fd = open(COMPRESSED_FILE_NAME, O_RDONLY)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", COMPRESSED_FILE_NAME);
            exit(2);
        }
        if ((out_fd = open(OUTPUT_FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        {
            fprintf(stderr, "Error opening file '%s'.\n", OUTPUT_FILE_NAME);
            exit(2);
        }
    
        time(&now);
        v42bis_init(&state_b, 3, 512, 6, frame_handler, (void *) (intptr_t) v42bis_fd, 512, data_handler, (void *) (intptr_t) out_fd, 512);
        in_octets_to_date = 0;
        out_octets_to_date = 0;
        while ((len = read(v42bis_fd, buf, 1024)) > 0)
        {
            if (v42bis_decompress(&state_b, buf, len))
            {
                fprintf(stderr, "Bad return code from decompression\n");
                exit(2);
            }
            in_octets_to_date += len;
        }
        v42bis_decompress_flush(&state_b);
        printf("%d bytes decompressed to %d bytes in %lds\n", in_octets_to_date, out_octets_to_date, time(NULL) - now);
        close(v42bis_fd);
        close(out_fd);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
