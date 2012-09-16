/*
 * Copyright (c) 2009-2012, Anthony Minessale II
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
#ifndef FTMOD_LIBPRI_H
#define FTMOD_LIBPRI_H
#include "freetdm.h"
#include "lpwrap_pri.h"

/* T302 Overlap receiving inter-digit timeout */
#define OVERLAP_TIMEOUT_MS_DEFAULT	5000	/* 5 sec */
#define OVERLAP_TIMEOUT_MS_MIN		3000	/* 3 sec */
#define OVERLAP_TIMEOUT_MS_MAX		30000	/* 30 sec */

/* NT-mode idle b-channel restart timer */
#define IDLE_RESTART_TIMEOUT_MS_DEFAULT	0		/* disabled */
#define IDLE_RESTART_TIMEOUT_MS_MIN	10000		/* 10 sec */
#define IDLE_RESTART_TIMEOUT_MS_MAX	86400000	/* 1 day */

/* T316 RESTART ACK wait timer */
#define T316_TIMEOUT_MS_DEFAULT		30000	/* 30 sec */
#define T316_TIMEOUT_MS_MIN		10000	/* 10 sec */
#define T316_TIMEOUT_MS_MAX		300000	/* 5 min  */

/* T316 restart attempts until channel is suspended */
#define T316_ATTEMPT_LIMIT_DEFAULT	3
#define T316_ATTEMPT_LIMIT_MIN		1
#define T316_ATTEMPT_LIMIT_MAX		10


typedef enum {
        SERVICE_CHANGE_STATUS_INSERVICE = 0,
        SERVICE_CHANGE_STATUS_MAINTENANCE,
        SERVICE_CHANGE_STATUS_OUTOFSERVICE
} service_change_status_t;

typedef enum {
	FTMOD_LIBPRI_OPT_NONE = 0,
	FTMOD_LIBPRI_OPT_SUGGEST_CHANNEL = (1 << 0),
	FTMOD_LIBPRI_OPT_OMIT_DISPLAY_IE = (1 << 1),
	FTMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE = (1 << 2),
	FTMOD_LIBPRI_OPT_FACILITY_AOC = (1 << 3),

	FTMOD_LIBPRI_OPT_MAX = (1 << 4)
} ftdm_isdn_opts_t;

typedef enum {
	FTMOD_LIBPRI_RUNNING = (1 << 0)
} ftdm_isdn_flag_t;

typedef enum {
	FTMOD_LIBPRI_OVERLAP_NONE    = 0,
	FTMOD_LIBPRI_OVERLAP_RECEIVE = (1 << 0),
	FTMOD_LIBPRI_OVERLAP_SEND    = (1 << 1)
#define FTMOD_LIBPRI_OVERLAP_BOTH	(FTMOD_LIBPRI_OVERLAP_RECEIVE | FTMOD_LIBPRI_OVERLAP_SEND)
} ftdm_isdn_overlap_t;

struct ftdm_libpri_data {
	ftdm_channel_t *dchan;
	ftdm_isdn_opts_t opts;
	uint32_t flags;
	uint32_t debug_mask;

	int mode;
	int dialect;
	int overlap;		/*!< Overlap dial flags */
	int overlap_timeout_ms;	/*!< Overlap dial timeout */
	int idle_restart_timeout_ms;	/*!< NT-mode idle b-channel restart */
	int t316_timeout_ms;	/*!< T316 RESTART ACK timeout */
	int t316_max_attempts;	/*!< T316 timeout limit */
	unsigned int layer1;
	unsigned int ton;
	unsigned int service_message_support;

	lpwrap_pri_t spri;

	/* MSN filter */
	ftdm_hash_t *msn_hash;
	ftdm_mutex_t *msn_mutex;

	/* NT-mode idle restart timer */
	struct lpwrap_timer t3xx;
};

typedef struct ftdm_libpri_data ftdm_libpri_data_t;


/*
 * b-channel flags
 */
enum {
	FTDM_LIBPRI_B_NONE = 0,
	FTDM_LIBPRI_B_REMOTE_RESTART = (1 << 0),	/*!< Remote triggered channel restart */
};

/**
 * Per-b-channel private data
 */
struct ftdm_libpri_b_chan {
	struct lpwrap_timer t302;	/*!< T302 overlap receive timer */
	struct lpwrap_timer t316;	/*!< T316 restart ack timer */
	ftdm_channel_t *channel;	/*!< back-pointer to b-channel */
	q931_call *call;		/*!< libpri opaque call handle */
	uint32_t flags;			/*!< channel flags */
	uint32_t t316_timeout_cnt;	/*!< T316 timeout counter */
};

typedef struct ftdm_libpri_b_chan ftdm_libpri_b_chan_t;

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

