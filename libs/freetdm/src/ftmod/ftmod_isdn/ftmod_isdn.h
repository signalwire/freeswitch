/*
 * Copyright (c) 2007-2012, Anthony Minessale II
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FTDM_ISDN_H
#define FTDM_ISDN_H

#define DEFAULT_DIGIT_TIMEOUT	10000		/* default overlap timeout: 10 seconds */


typedef enum {
	FTDM_ISDN_OPT_NONE = 0,
	FTDM_ISDN_OPT_SUGGEST_CHANNEL = (1 << 0),
	FTDM_ISDN_OPT_OMIT_DISPLAY_IE = (1 << 1),	/*!< Do not send Caller name in outgoing SETUP message (= Display IE) */
	FTDM_ISDN_OPT_DISABLE_TONES   = (1 << 2),	/*!< Disable tone generating thread (NT mode) */

	FTDM_ISDN_OPT_MAX = (2 << 0)
} ftdm_isdn_opts_t;

typedef enum {
	FTDM_ISDN_RUNNING        = (1 << 0),
	FTDM_ISDN_TONES_RUNNING  = (1 << 1),
	FTDM_ISDN_STOP           = (1 << 2),

	FTDM_ISDN_CAPTURE        = (1 << 3),
	FTDM_ISDN_CAPTURE_L3ONLY = (1 << 4)
} ftdm_isdn_flag_t;

#ifdef HAVE_PCAP
struct pcap_context;
#endif

struct ftdm_isdn_data {
	Q921Data_t q921;
	Q931_TrunkInfo_t q931;
	ftdm_channel_t *dchan;
	uint32_t flags;
	int32_t mode;
	int32_t digit_timeout;
	ftdm_isdn_opts_t opts;
#ifdef HAVE_PCAP
	struct pcap_context *pcap;
#endif
};

typedef struct ftdm_isdn_data ftdm_isdn_data_t;


/* b-channel private data */
struct ftdm_isdn_bchan_data
{
	ftdm_time_t digit_timeout;
	int offset;	/* offset in teletone buffer */
};

typedef struct ftdm_isdn_bchan_data ftdm_isdn_bchan_data_t;


#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet expandtab:
 */

