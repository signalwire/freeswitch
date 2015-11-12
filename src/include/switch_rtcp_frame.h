/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Sherwin Sim
 *
 *
 * switch_rtcp_frame.h -- RTCP Frame Structure
 *
 */
/*! \file switch_rtcp_frame.h
  \brief RTCP Frame Structure
*/

#ifndef SWITCH_RTCP_FRAME_H
#define SWITCH_RTCP_FRAME_H

#include <switch.h>

#define MAX_REPORT_BLOCKS 5

SWITCH_BEGIN_EXTERN_C

struct switch_rtcp_report_block_frame {
	uint32_t ssrc; /* The SSRC identifier of the source to which the information in this reception report block pertains. */
	uint8_t fraction; /* The fraction of RTP data packets from source SSRC_n lost since the previous SR or RR packet was sent */
	uint32_t lost;  /* The total number of RTP data packets from source SSRC_n that have been lost since the beginning of reception */
	uint32_t highest_sequence_number_received;
	uint32_t jitter; /* An estimate of the statistical variance of the RTP data packet interarrival time, measured in timestamp units and expressed as an unsigned integer. */
	uint32_t lsr; /* The middle 32 bits out of 64 in the NTP timestamp */
	uint32_t dlsr; /* The delay, expressed in units of 1/65536 seconds, between receiving the last SR packet from source SSRC_n and sending this reception report block */
	uint32_t loss_avg;
	double rtt_avg;
};

/*! \brief An abstraction of a rtcp frame */
	struct switch_rtcp_frame {

	uint16_t report_count;

	uint16_t packet_type;

	uint32_t ssrc;

	uint32_t ntp_msw;

	uint32_t ntp_lsw;

	uint32_t timestamp;

	uint32_t packet_count;

	uint32_t octect_count;

	uint32_t nb_reports;

	struct switch_rtcp_report_block_frame reports[MAX_REPORT_BLOCKS];

};

SWITCH_END_EXTERN_C
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
