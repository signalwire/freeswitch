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

#ifndef _LPWRAP_PRI_H
#define _LPWRAP_PRI_H
#include <libpri.h>
#include <freetdm.h>


#define LPWRAP_MAX_CHAN_PER_SPAN 32

typedef enum {
	LPWRAP_PRI_EVENT_ANY           = 0,
	LPWRAP_PRI_EVENT_DCHAN_UP      = PRI_EVENT_DCHAN_UP,
	LPWRAP_PRI_EVENT_DCHAN_DOWN    = PRI_EVENT_DCHAN_DOWN,
	LPWRAP_PRI_EVENT_RESTART       = PRI_EVENT_RESTART,
	LPWRAP_PRI_EVENT_CONFIG_ERR    = PRI_EVENT_CONFIG_ERR,
	LPWRAP_PRI_EVENT_RING          = PRI_EVENT_RING,
	LPWRAP_PRI_EVENT_HANGUP        = PRI_EVENT_HANGUP,
	LPWRAP_PRI_EVENT_RINGING       = PRI_EVENT_RINGING,
	LPWRAP_PRI_EVENT_ANSWER        = PRI_EVENT_ANSWER,
	LPWRAP_PRI_EVENT_HANGUP_ACK    = PRI_EVENT_HANGUP_ACK,
	LPWRAP_PRI_EVENT_RESTART_ACK   = PRI_EVENT_RESTART_ACK,
	LPWRAP_PRI_EVENT_FACNAME       = PRI_EVENT_FACNAME,
	LPWRAP_PRI_EVENT_INFO_RECEIVED = PRI_EVENT_INFO_RECEIVED,
	LPWRAP_PRI_EVENT_PROCEEDING    = PRI_EVENT_PROCEEDING,
	LPWRAP_PRI_EVENT_SETUP_ACK     = PRI_EVENT_SETUP_ACK,
	LPWRAP_PRI_EVENT_HANGUP_REQ    = PRI_EVENT_HANGUP_REQ,
	LPWRAP_PRI_EVENT_NOTIFY        = PRI_EVENT_NOTIFY,
	LPWRAP_PRI_EVENT_PROGRESS      = PRI_EVENT_PROGRESS,
	LPWRAP_PRI_EVENT_KEYPAD_DIGIT  = PRI_EVENT_KEYPAD_DIGIT,
	LPWRAP_PRI_EVENT_IO_FAIL       = 19,

	/* don't touch */
	LPWRAP_PRI_EVENT_MAX
} lpwrap_pri_event_t;

typedef enum {
	LPWRAP_PRI_NETWORK = PRI_NETWORK,
	LPWRAP_PRI_CPE = PRI_CPE
} lpwrap_pri_node_t;

typedef enum {
	LPWRAP_PRI_SWITCH_UNKNOWN     = PRI_SWITCH_UNKNOWN,
	LPWRAP_PRI_SWITCH_NI2         = PRI_SWITCH_NI2,
	LPWRAP_PRI_SWITCH_DMS100      = PRI_SWITCH_DMS100,
	LPWRAP_PRI_SWITCH_LUCENT5E    = PRI_SWITCH_LUCENT5E,
	LPWRAP_PRI_SWITCH_ATT4ESS     = PRI_SWITCH_ATT4ESS,
	LPWRAP_PRI_SWITCH_EUROISDN_E1 = PRI_SWITCH_EUROISDN_E1,
	LPWRAP_PRI_SWITCH_EUROISDN_T1 = PRI_SWITCH_EUROISDN_T1,
	LPWRAP_PRI_SWITCH_NI1         = PRI_SWITCH_NI1,
	LPWRAP_PRI_SWITCH_GR303_EOC   = PRI_SWITCH_GR303_EOC,
	LPWRAP_PRI_SWITCH_GR303_TMC   = PRI_SWITCH_GR303_TMC,
	LPWRAP_PRI_SWITCH_QSIG        = PRI_SWITCH_QSIG,

	/* don't touch */
	LPWRAP_PRI_SWITCH_MAX
} lpwrap_pri_switch_t;

typedef enum {
	LPWRAP_PRI_READY = (1 << 0)
} lpwrap_pri_flag_t;

struct lpwrap_pri;
typedef int (*event_handler)(struct lpwrap_pri *, lpwrap_pri_event_t, pri_event *);
typedef int (*loop_handler)(struct lpwrap_pri *);

struct lpwrap_pri {
	struct pri *pri;
	ftdm_span_t *span;
	ftdm_channel_t *dchan;
	unsigned int flags;
	void *private_info;
	event_handler eventmap[LPWRAP_PRI_EVENT_MAX];
	loop_handler on_loop;
	int errs;
};

typedef struct lpwrap_pri lpwrap_pri_t;

struct lpwrap_pri_event_list {
	int event_id;
	int pri_event;
	const char *name;
};



#define LPWRAP_MAP_PRI_EVENT(spri, event, func) spri.eventmap[event] = func;

const char *lpwrap_pri_event_str(lpwrap_pri_event_t event_id);
int lpwrap_one_loop(struct lpwrap_pri *spri);
int lpwrap_init_pri(struct lpwrap_pri *spri, ftdm_span_t *span, ftdm_channel_t *dchan, int swtype, int node, int debug);
int lpwrap_run_pri(struct lpwrap_pri *spri);

#endif
