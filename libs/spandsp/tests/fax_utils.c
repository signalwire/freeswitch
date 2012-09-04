/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_utils.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"
#include "fax_utils.h"

void fax_log_tx_parameters(t30_state_t *s, const char *tag)
{
    const char *u;
    
    if ((u = t30_get_tx_ident(s)))
        printf("%s: Local ident '%s'\n", tag, u);
    if ((u = t30_get_tx_sub_address(s)))
        printf("%s: Local sub-address '%s'\n", tag, u);
    if ((u = t30_get_tx_polled_sub_address(s)))
        printf("%s: Local polled sub-address '%s'\n", tag, u);
    if ((u = t30_get_tx_selective_polling_address(s)))
        printf("%s: Local selective polling address '%s'\n", tag, u);
    if ((u = t30_get_tx_sender_ident(s)))
        printf("%s: Local sender ident '%s'\n", tag, u);
    if ((u = t30_get_tx_password(s)))
        printf("%s: Local password '%s'\n", tag, u);
}
/*- End of function --------------------------------------------------------*/

void fax_log_rx_parameters(t30_state_t *s, const char *tag)
{
    const char *u;

    if ((u = t30_get_rx_ident(s)))
        printf("%s: Remote ident '%s'\n", tag, u);
    if ((u = t30_get_rx_sub_address(s)))
        printf("%s: Remote sub-address '%s'\n", tag, u);
    if ((u = t30_get_rx_polled_sub_address(s)))
        printf("%s: Remote polled sub-address '%s'\n", tag, u);
    if ((u = t30_get_rx_selective_polling_address(s)))
        printf("%s: Remote selective polling address '%s'\n", tag, u);
    if ((u = t30_get_rx_sender_ident(s)))
        printf("%s: Remote sender ident '%s'\n", tag, u);
    if ((u = t30_get_rx_password(s)))
        printf("%s: Remote password '%s'\n", tag, u);
    if ((u = t30_get_rx_country(s)))
        printf("%s: Remote was made in '%s'\n", tag, u);
    if ((u = t30_get_rx_vendor(s)))
        printf("%s: Remote was made by '%s'\n", tag, u);
    if ((u = t30_get_rx_model(s)))
        printf("%s: Remote is model '%s'\n", tag, u);
}
/*- End of function --------------------------------------------------------*/

void fax_log_final_transfer_statistics(t30_state_t *s, const char *tag)
{
    t30_stats_t t;

    t30_get_transfer_statistics(s, &t);
    printf("%s: Bit rate %d\n", tag, t.bit_rate);
    printf("%s: ECM %s\n", tag, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%s: RTP events %d. RTN events %d\n", tag, t.rtp_events, t.rtn_events);
    printf("%s: Tx pages %d, rx pages %d\n", tag, t.pages_tx, t.pages_rx);
}
/*- End of function --------------------------------------------------------*/

void fax_log_page_transfer_statistics(t30_state_t *s, const char *tag)
{
    t30_stats_t t;

    t30_get_transfer_statistics(s, &t);
    printf("%s: Page statistics\n", tag);
    printf("%s:   Pages in the file %d\n", tag, t.pages_in_file);
    printf("%s:   Bad rows %d, longest bad row run %d\n", tag, t.bad_rows, t.longest_bad_row_run);
    printf("%s:   Bad ECM frames %d\n", tag, t.error_correcting_mode_retries);
    printf("%s:   Compression type %s (%d)\n", tag, t4_encoding_to_str(t.encoding), t.encoding);
    printf("%s:   Compressed image size %d bytes\n", tag, t.image_size);
    printf("%s:   Image size %d pels x %d pels\n", tag, t.width, t.length);
    printf("%s:   Image resolution %d pels/m x %d pels/m\n", tag, t.x_resolution, t.y_resolution);
#if defined(WITH_SPANDSP_INTERNALS)
    printf("%s:   Bits per row - min %d, max %d\n", tag, s->t4.min_row_bits, s->t4.max_row_bits);
#endif

    fax_log_final_transfer_statistics(s, tag);
}
/*- End of function --------------------------------------------------------*/

int get_tiff_total_pages(const char *file)
{
    TIFF *tiff_file;
    int max;

    if ((tiff_file = TIFFOpen(file, "r")) == NULL)
        return -1;
    /* Each page *should* contain the total number of pages, but can this be
       trusted? Some files say 0. Actually searching for the last page is
       more reliable. */
    max = 0;
    while (TIFFSetDirectory(tiff_file, (tdir_t) max))
        max++;
    TIFFClose(tiff_file);
    return max;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
