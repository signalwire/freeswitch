/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_utils.h
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

/*! \file */

#if !defined(_SPANDSP_FAX_UTILS_H_)
#define _SPANDSP_FAX_UTILS_H_

#if defined(__cplusplus)
extern "C"
{
#endif

void fax_log_tx_parameters(t30_state_t *s, const char *tag);

void fax_log_rx_parameters(t30_state_t *s, const char *tag);

void fax_log_page_transfer_statistics(t30_state_t *s, const char *tag);

void fax_log_final_transfer_statistics(t30_state_t *s, const char *tag);

int get_tiff_total_pages(const char *file);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
