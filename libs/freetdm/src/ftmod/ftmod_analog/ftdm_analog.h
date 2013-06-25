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

#ifndef FTDM_ANALOG_H
#define FTDM_ANALOG_H
#include "freetdm.h"

typedef enum {
	FTDM_ANALOG_RUNNING = (1 << 0),
	FTDM_ANALOG_CALLERID = (1 << 1),
	FTDM_ANALOG_ANSWER_POLARITY_REVERSE = (1 << 2),
	FTDM_ANALOG_HANGUP_POLARITY_REVERSE = (1 << 3),
	FTDM_ANALOG_POLARITY_CALLERID = (1 << 4)
} ftdm_analog_flag_t;

#define FTDM_MAX_HOTLINE_STR		20
#define MAX_DTMF 256

struct ftdm_analog_data {
	uint32_t flags;
	uint32_t max_dialstr;
	uint32_t wait_dialtone_timeout;
	uint32_t polarity_delay;
	uint32_t digit_timeout;
	char hotline[FTDM_MAX_HOTLINE_STR];
};

/* Analog flags to be set in the sflags (signaling flags) channel memeber */
#define AF_POLARITY_REVERSE (1 << 0)

static void *ftdm_analog_run(ftdm_thread_t *me, void *obj);
typedef struct ftdm_analog_data ftdm_analog_data_t;

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
