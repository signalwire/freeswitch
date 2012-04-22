/*
 * Copyright (c) 2009, Anthony Minessale II
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


#ifndef OZMOD_LIBPRI_H
#define OZMOD_LIBPRI_H
#include "openzap.h"
#include "lpwrap_pri.h"

typedef enum {
	OZMOD_LIBPRI_OPT_NONE = 0,
	OZMOD_LIBPRI_OPT_SUGGEST_CHANNEL = (1 << 0),
	OZMOD_LIBPRI_OPT_OMIT_DISPLAY_IE = (1 << 1),
	OZMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE = (1 << 2),
		
	OZMOD_LIBPRI_OPT_MAX = (1 << 3)
} zap_isdn_opts_t;

typedef enum {
	OZMOD_LIBPRI_RUNNING = (1 << 0)
} zap_isdn_flag_t;


struct zap_libpri_data {
	zap_channel_t *dchan;
	zap_channel_t *dchans[2];
	struct zap_sigmsg sigmsg;
	uint32_t flags;
	int32_t mode;
	zap_isdn_opts_t opts;

	int node;
	int pswitch;
	char *dialplan;
	unsigned int l1;
	unsigned int dp;

	int debug;

	lpwrap_pri_t spri;
};

typedef struct zap_libpri_data zap_libpri_data_t;


/* b-channel private data */
struct zap_isdn_bchan_data
{
	int32_t digit_timeout;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

