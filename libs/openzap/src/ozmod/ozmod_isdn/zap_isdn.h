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

#ifndef ZAP_ISDN_H
#define ZAP_ISDN_H
#include "openzap.h"

#define DEFAULT_DIGIT_TIMEOUT	10000		/* default overlap timeout: 10 seconds */


typedef enum {
	ZAP_ISDN_OPT_NONE = 0,
	ZAP_ISDN_OPT_SUGGEST_CHANNEL = (1 << 0),
	ZAP_ISDN_OPT_OMIT_DISPLAY_IE = (1 << 1),	/*!< Do not send Caller name in outgoing SETUP message (= Display IE) */
	ZAP_ISDN_OPT_DISABLE_TONES = (1 << 2),		/*!< Disable tone generating thread (NT mode) */

	ZAP_ISDN_OPT_MAX = (2 << 0)
} zap_isdn_opts_t;

typedef enum {
	ZAP_ISDN_RUNNING = (1 << 0),
	ZAP_ISDN_TONES_RUNNING = (1 << 1),
	ZAP_ISDN_STOP = (1 << 2)
} zap_isdn_flag_t;


struct zap_isdn_data {
	Q921Data_t q921;
	Q931_TrunkInfo_t q931;
	zap_channel_t *dchan;
	zap_channel_t *dchans[2];
	struct zap_sigmsg sigmsg;
	uint32_t flags;
	int32_t mode;
	int32_t digit_timeout;
	zap_isdn_opts_t opts;
	zap_caller_data_t *outbound_crv[32768];
	zap_channel_t *channels_local_crv[32768];
	zap_channel_t *channels_remote_crv[32768];
};

typedef struct zap_isdn_data zap_isdn_data_t;


/* b-channel private data */
struct zap_isdn_bchan_data
{
	L2ULONG digit_timeout;
};

typedef struct zap_isdn_bchan_data zap_isdn_bchan_data_t;


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

