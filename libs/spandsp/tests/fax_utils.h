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
 *
 * $Id: fax_utils.h,v 1.1 2009/02/20 12:34:20 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_FAX_UTILS_H_)
#define _SPANDSP_FAX_UTILS_H_

#if defined(__cplusplus)
extern "C"
{
#endif

void log_tx_parameters(t30_state_t *s, const char *tag);

void log_rx_parameters(t30_state_t *s, const char *tag);

void log_transfer_statistics(t30_state_t *s, const char *tag);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
