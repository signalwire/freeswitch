/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30_api.c - ITU T.30 FAX transfer processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Steve Underwood
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
 * $Id: t30_api.c,v 1.13.4.2 2009/12/19 14:18:13 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_api.h"
#include "spandsp/t30_logging.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"
#include "spandsp/private/t30.h"

#include "t30_local.h"

SPAN_DECLARE(int) t30_set_tx_ident(t30_state_t *s, const char *id)
{
    if (id == NULL)
    {
        s->tx_info.ident[0] = '\0';
        return 0;
    }
    if (strlen(id) > T30_MAX_IDENT_LEN)
        return -1;
    strcpy(s->tx_info.ident, id);
    t4_tx_set_local_ident(&s->t4, s->tx_info.ident);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_ident(t30_state_t *s)
{
    if (s->tx_info.ident[0] == '\0')
        return NULL;
    return s->tx_info.ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_ident(t30_state_t *s)
{
    if (s->rx_info.ident[0] == '\0')
        return NULL;
    return s->rx_info.ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_sub_address(t30_state_t *s, const char *sub_address)
{
    if (sub_address == NULL)
    {
        s->tx_info.sub_address[0] = '\0';
        return 0;
    }
    if (strlen(sub_address) > T30_MAX_IDENT_LEN)
        return -1;
    strcpy(s->tx_info.sub_address, sub_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_sub_address(t30_state_t *s)
{
    if (s->tx_info.sub_address[0] == '\0')
        return NULL;
    return s->tx_info.sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_sub_address(t30_state_t *s)
{
    if (s->rx_info.sub_address[0] == '\0')
        return NULL;
    return s->rx_info.sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_selective_polling_address(t30_state_t *s, const char *selective_polling_address)
{
    if (selective_polling_address == NULL)
    {
        s->tx_info.selective_polling_address[0] = '\0';
        return 0;
    }
    if (strlen(selective_polling_address) > T30_MAX_IDENT_LEN)
        return -1;
    strcpy(s->tx_info.selective_polling_address, selective_polling_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_selective_polling_address(t30_state_t *s)
{
    if (s->tx_info.selective_polling_address[0] == '\0')
        return NULL;
    return s->tx_info.selective_polling_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_selective_polling_address(t30_state_t *s)
{
    if (s->rx_info.selective_polling_address[0] == '\0')
        return NULL;
    return s->rx_info.selective_polling_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_polled_sub_address(t30_state_t *s, const char *polled_sub_address)
{
    if (polled_sub_address == NULL)
    {
        s->tx_info.polled_sub_address[0] = '\0';
        return 0;
    }
    if (strlen(polled_sub_address) > T30_MAX_IDENT_LEN)
        return -1;
    strcpy(s->tx_info.polled_sub_address, polled_sub_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_polled_sub_address(t30_state_t *s)
{
    if (s->tx_info.polled_sub_address[0] == '\0')
        return NULL;
    return s->tx_info.polled_sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_polled_sub_address(t30_state_t *s)
{
    if (s->rx_info.polled_sub_address[0] == '\0')
        return NULL;
    return s->rx_info.polled_sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_sender_ident(t30_state_t *s, const char *sender_ident)
{
    if (sender_ident == NULL)
    {
        s->tx_info.sender_ident[0] = '\0';
        return 0;
    }
    if (strlen(sender_ident) > T30_MAX_IDENT_LEN)
        return -1;
    strcpy(s->tx_info.sender_ident, sender_ident);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_sender_ident(t30_state_t *s)
{
    if (s->tx_info.sender_ident[0] == '\0')
        return NULL;
    return s->tx_info.sender_ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_sender_ident(t30_state_t *s)
{
    if (s->rx_info.sender_ident[0] == '\0')
        return NULL;
    return s->rx_info.sender_ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_password(t30_state_t *s, const char *password)
{
    if (password == NULL)
    {
        s->tx_info.password[0] = '\0';
        return 0;
    }
    if (strlen(password) > T30_MAX_IDENT_LEN)
        return -1;
    strcpy(s->tx_info.password, password);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_password(t30_state_t *s)
{
    if (s->tx_info.password[0] == '\0')
        return NULL;
    return s->tx_info.password;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_password(t30_state_t *s)
{
    if (s->rx_info.password[0] == '\0')
        return NULL;
    return s->rx_info.password;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_nsf(t30_state_t *s, const uint8_t *nsf, int len)
{
    if (s->tx_info.nsf)
        free(s->tx_info.nsf);
    if (nsf  &&  len > 0  &&  (s->tx_info.nsf = malloc(len + 3)))
    {
        memcpy(s->tx_info.nsf + 3, nsf, len);
        s->tx_info.nsf_len = len;
    }
    else
    {
        s->tx_info.nsf = NULL;
        s->tx_info.nsf_len = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_nsf(t30_state_t *s, const uint8_t *nsf[])
{
    if (nsf)
        *nsf = s->tx_info.nsf;
    return s->tx_info.nsf_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_nsf(t30_state_t *s, const uint8_t *nsf[])
{
    if (nsf)
        *nsf = s->rx_info.nsf;
    return s->rx_info.nsf_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_nsc(t30_state_t *s, const uint8_t *nsc, int len)
{
    if (s->tx_info.nsc)
        free(s->tx_info.nsc);
    if (nsc  &&  len > 0  &&  (s->tx_info.nsc = malloc(len + 3)))
    {
        memcpy(s->tx_info.nsc + 3, nsc, len);
        s->tx_info.nsc_len = len;
    }
    else
    {
        s->tx_info.nsc = NULL;
        s->tx_info.nsc_len = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_nsc(t30_state_t *s, const uint8_t *nsc[])
{
    if (nsc)
        *nsc = s->tx_info.nsc;
    return s->tx_info.nsc_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_nsc(t30_state_t *s, const uint8_t *nsc[])
{
    if (nsc)
        *nsc = s->rx_info.nsc;
    return s->rx_info.nsc_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_nss(t30_state_t *s, const uint8_t *nss, int len)
{
    if (s->tx_info.nss)
        free(s->tx_info.nss);
    if (nss  &&  len > 0  &&  (s->tx_info.nss = malloc(len + 3)))
    {
        memcpy(s->tx_info.nss + 3, nss, len);
        s->tx_info.nss_len = len;
    }
    else
    {
        s->tx_info.nss = NULL;
        s->tx_info.nss_len = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_nss(t30_state_t *s, const uint8_t *nss[])
{
    if (nss)
        *nss = s->tx_info.nss;
    return s->tx_info.nss_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_nss(t30_state_t *s, const uint8_t *nss[])
{
    if (nss)
        *nss = s->rx_info.nss;
    return s->rx_info.nss_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_tsa(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.tsa)
        free(s->tx_info.tsa);
    if (address == NULL  ||  len == 0)
    {
        s->tx_info.tsa = NULL;
        s->tx_info.tsa_len = 0;
        return 0;
    }
    s->tx_info.tsa_type = type;
    if (len < 0)
        len = strlen(address);
    if ((s->tx_info.tsa = malloc(len)))
    {
        memcpy(s->tx_info.tsa, address, len);
        s->tx_info.tsa_len = len;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_tsa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.tsa_type;
    if (address)
        *address = s->tx_info.tsa;
    return s->tx_info.tsa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_tsa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.tsa_type;
    if (address)
        *address = s->rx_info.tsa;
    return s->rx_info.tsa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_ira(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.ira)
        free(s->tx_info.ira);
    if (address == NULL)
    {
        s->tx_info.ira = NULL;
        return 0;
    }
    s->tx_info.ira = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_ira(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.ira_type;
    if (address)
        *address = s->tx_info.ira;
    return s->tx_info.ira_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_ira(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.ira_type;
    if (address)
        *address = s->rx_info.ira;
    return s->rx_info.ira_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_cia(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.cia)
        free(s->tx_info.cia);
    if (address == NULL)
    {
        s->tx_info.cia = NULL;
        return 0;
    }
    s->tx_info.cia = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_cia(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.cia_type;
    if (address)
        *address = s->tx_info.cia;
    return s->tx_info.cia_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_cia(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.cia_type;
    if (address)
        *address = s->rx_info.cia;
    return s->rx_info.cia_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_isp(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.isp)
        free(s->tx_info.isp);
    if (address == NULL)
    {
        s->tx_info.isp = NULL;
        return 0;
    }
    s->tx_info.isp = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_isp(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.isp_type;
    if (address)
        *address = s->tx_info.isp;
    return s->tx_info.isp_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_isp(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.isp_type;
    if (address)
        *address = s->rx_info.isp;
    return s->rx_info.isp_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_csa(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.csa)
        free(s->tx_info.csa);
    if (address == NULL)
    {
        s->tx_info.csa = NULL;
        return 0;
    }
    s->tx_info.csa = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_csa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.csa_type;
    if (address)
        *address = s->tx_info.csa;
    return s->tx_info.csa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_csa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.csa_type;
    if (address)
        *address = s->rx_info.csa;
    return s->rx_info.csa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_page_header_info(t30_state_t *s, const char *info)
{
    if (info == NULL)
    {
        s->header_info[0] = '\0';
        return 0;
    }
    if (strlen(info) > T30_MAX_PAGE_HEADER_INFO)
        return -1;
    strcpy(s->header_info, info);
    t4_tx_set_header_info(&s->t4, s->header_info);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_page_header_info(t30_state_t *s, char *info)
{
    if (info)
        strcpy(info, s->header_info);
    return strlen(s->header_info);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_country(t30_state_t *s)
{
    return s->country;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_vendor(t30_state_t *s)
{
    return s->vendor;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_model(t30_state_t *s)
{
    return s->model;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_rx_file(t30_state_t *s, const char *file, int stop_page)
{
    strncpy(s->rx_file, file, sizeof(s->rx_file));
    s->rx_file[sizeof(s->rx_file) - 1] = '\0';
    s->rx_stop_page = stop_page;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_tx_file(t30_state_t *s, const char *file, int start_page, int stop_page)
{
    strncpy(s->tx_file, file, sizeof(s->tx_file));
    s->tx_file[sizeof(s->tx_file) - 1] = '\0';
    s->tx_start_page = start_page;
    s->tx_stop_page = stop_page;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_iaf_mode(t30_state_t *s, int iaf)
{
    s->iaf = iaf;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_ecm_capability(t30_state_t *s, int enabled)
{
    s->ecm_allowed = enabled;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_rx_encoding(t30_state_t *s, int encoding)
{
    switch (encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        s->output_encoding = encoding;
        return 0;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_minimum_scan_line_time(t30_state_t *s, int min_time)
{
    /* There are only certain possible times supported, so we need to select
       the code which best matches the request. */
    if (min_time == 0)
        s->local_min_scan_time_code = 7;
    else if (min_time <= 5)
        s->local_min_scan_time_code = 1;
    else if (min_time <= 10)
        s->local_min_scan_time_code = 2;
    else if (min_time <= 20)
        s->local_min_scan_time_code = 0;
    else if (min_time <= 40)
        s->local_min_scan_time_code = 4;
    else
        return -1;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_modems(t30_state_t *s, int supported_modems)
{
    s->supported_modems = supported_modems;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_compressions(t30_state_t *s, int supported_compressions)
{
    s->supported_compressions = supported_compressions;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_resolutions(t30_state_t *s, int supported_resolutions)
{
    s->supported_resolutions = supported_resolutions;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_image_sizes(t30_state_t *s, int supported_image_sizes)
{
    s->supported_image_sizes = supported_image_sizes;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_t30_features(t30_state_t *s, int supported_t30_features)
{
    s->supported_t30_features = supported_t30_features;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_status(t30_state_t *s, int status)
{
    s->current_status = status;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_receiver_not_ready(t30_state_t *s, int count)
{
    s->receiver_not_ready_count = count;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_phase_b_handler(t30_state_t *s, t30_phase_b_handler_t *handler, void *user_data)
{
    s->phase_b_handler = handler;
    s->phase_b_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_phase_d_handler(t30_state_t *s, t30_phase_d_handler_t *handler, void *user_data)
{
    s->phase_d_handler = handler;
    s->phase_d_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_phase_e_handler(t30_state_t *s, t30_phase_e_handler_t *handler, void *user_data)
{
    s->phase_e_handler = handler;
    s->phase_e_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_document_handler(t30_state_t *s, t30_document_handler_t *handler, void *user_data)
{
    s->document_handler = handler;
    s->document_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_real_time_frame_handler(t30_state_t *s, t30_real_time_frame_handler_t *handler, void *user_data)
{
    s->real_time_frame_handler = handler;
    s->real_time_frame_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t30_get_logging_state(t30_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
