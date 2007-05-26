/*****************************************************************************
 * libsangoma.c	AFT T1/E1: HDLC API Code Library
 *
 * Author(s):	Anthony Minessale II <anthmct@yahoo.com>
 *              Nenad Corbic <ncorbic@sangoma.com>
 *
 * Copyright:	(c) 2005 Anthony Minessale II
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * ============================================================================
 */

#ifndef _SANGOMA_PRI_H
#define _SANGOMA_PRI_H
#include <libpri.h>
#include <pri_internal.h>


#define SANGOMA_MAX_CHAN_PER_SPAN 32

typedef enum {
	SANGOMA_PRI_EVENT_ANY = 0,
	SANGOMA_PRI_EVENT_DCHAN_UP = PRI_EVENT_DCHAN_UP,
	SANGOMA_PRI_EVENT_DCHAN_DOWN = PRI_EVENT_DCHAN_DOWN,
	SANGOMA_PRI_EVENT_RESTART = PRI_EVENT_RESTART,
	SANGOMA_PRI_EVENT_CONFIG_ERR = PRI_EVENT_CONFIG_ERR,
	SANGOMA_PRI_EVENT_RING = PRI_EVENT_RING,
	SANGOMA_PRI_EVENT_HANGUP = PRI_EVENT_HANGUP,
	SANGOMA_PRI_EVENT_RINGING = PRI_EVENT_RINGING,
	SANGOMA_PRI_EVENT_ANSWER = PRI_EVENT_ANSWER,
	SANGOMA_PRI_EVENT_HANGUP_ACK = PRI_EVENT_HANGUP_ACK,
	SANGOMA_PRI_EVENT_RESTART_ACK = PRI_EVENT_RESTART_ACK,
	SANGOMA_PRI_EVENT_FACNAME = PRI_EVENT_FACNAME,
	SANGOMA_PRI_EVENT_INFO_RECEIVED = PRI_EVENT_INFO_RECEIVED,
	SANGOMA_PRI_EVENT_PROCEEDING = PRI_EVENT_PROCEEDING,
	SANGOMA_PRI_EVENT_SETUP_ACK = PRI_EVENT_SETUP_ACK,
	SANGOMA_PRI_EVENT_HANGUP_REQ = PRI_EVENT_HANGUP_REQ,
	SANGOMA_PRI_EVENT_NOTIFY = PRI_EVENT_NOTIFY,
	SANGOMA_PRI_EVENT_PROGRESS = PRI_EVENT_PROGRESS,
	SANGOMA_PRI_EVENT_KEYPAD_DIGIT = PRI_EVENT_KEYPAD_DIGIT
} sangoma_pri_event_t;

typedef enum {
	SANGOMA_PRI_NETWORK = PRI_NETWORK,
	SANGOMA_PRI_CPE = PRI_CPE
} sangoma_pri_node_t;

typedef enum {
	SANGOMA_PRI_SWITCH_UNKNOWN = PRI_SWITCH_UNKNOWN,
	SANGOMA_PRI_SWITCH_NI2 = PRI_SWITCH_NI2,	   		
	SANGOMA_PRI_SWITCH_DMS100 = PRI_SWITCH_DMS100,
	SANGOMA_PRI_SWITCH_LUCENT5E = PRI_SWITCH_LUCENT5E,
	SANGOMA_PRI_SWITCH_ATT4ESS = PRI_SWITCH_ATT4ESS,
	SANGOMA_PRI_SWITCH_EUROISDN_E1 = PRI_SWITCH_EUROISDN_E1,
	SANGOMA_PRI_SWITCH_EUROISDN_T1 = PRI_SWITCH_EUROISDN_T1,
	SANGOMA_PRI_SWITCH_NI1 = PRI_SWITCH_NI1,
	SANGOMA_PRI_SWITCH_GR303_EOC = PRI_SWITCH_GR303_EOC,
	SANGOMA_PRI_SWITCH_GR303_TMC = PRI_SWITCH_GR303_TMC,
	SANGOMA_PRI_SWITCH_QSIG = PRI_SWITCH_QSIG
} sangoma_pri_switch_t;

typedef enum {
	SANGOMA_PRI_READY = (1 << 0)
} sangoma_pri_flag_t;

struct sangoma_pri;
typedef int (*event_handler)(struct sangoma_pri *, sangoma_pri_event_t, pri_event *);
typedef int (*loop_handler)(struct sangoma_pri *);
#define MAX_EVENT 18

struct sangoma_pri {
	struct pri *pri;
	int span;
	int dchan;
	unsigned int flags;
	void *private_info;
	event_handler eventmap[MAX_EVENT+1];
	loop_handler on_loop;
	zap_channel_t *zdchan;
};

struct sangoma_pri_event_list {
	int event_id;
	int pri_event;
	char *name;
};



#define SANGOMA_MAP_PRI_EVENT(spri, event, func) spri.eventmap[event] = func;

char *sangoma_pri_event_str(sangoma_pri_event_t event_id);
int sangoma_one_loop(struct sangoma_pri *spri);
int sangoma_init_pri(struct sangoma_pri *spri, int span, int dchan, int swtype, int node, int debug);
int sangoma_run_pri(struct sangoma_pri *spri);

#endif
