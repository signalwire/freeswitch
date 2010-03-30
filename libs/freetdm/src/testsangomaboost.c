/*
 * Copyright (c) 2010, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
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

/**
 * Sample program for the boost signaling absraction.
 * Usage: boostsample <span name>
 * The span name must be a valid span defined in freetdm.conf
 * compile this program linking to the freetdm library (ie -lfreetdm)
 **/

#ifndef __linux__
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <signal.h>

#include "freetdm.h"


/* arbitrary limit for max calls in this sample program */
#define MAX_CALLS 255

/* some timers (in seconds) to fake responses in incoming calls */
#define PROGRESS_TIMER 1
#define ANSWER_TIMER 5
#define HANGUP_TIMER 15

/* simple variable used to stop the application */
static int app_running = 0;

typedef void (*expired_function_t)(ftdm_channel_t *channel);
typedef struct dummy_timer_s {
	int time;
	ftdm_channel_t *channel;
	expired_function_t expired;
} dummy_timer_t;

/* dummy second resolution timers */
static dummy_timer_t g_timers[MAX_CALLS];

/* mutex to protect the timers (both, the test thread and the signaling thread may modify them) */
static ftdm_mutex_t *g_schedule_mutex;

/* unique outgoing channel */
static ftdm_channel_t *g_outgoing_channel = NULL;

static void interrupt_requested(int signal)
{
	app_running = 0;
}

static void schedule_timer(ftdm_channel_t *channel, int sec, expired_function_t expired)
{
	int i;
	ftdm_mutex_lock(g_schedule_mutex);
	for (i = 0; i < sizeof(g_timers)/sizeof(g_timers[0]); i++) {
		/* check the timer slot is free to use */
		if (!g_timers[i].time) {
			g_timers[i].time = sec;
			g_timers[i].channel = channel;
			g_timers[i].expired = expired;
			ftdm_mutex_unlock(g_schedule_mutex);
			return;
		}
	}
	ftdm_log(FTDM_LOG_ERROR, "Failed to schedule timer\n");
	ftdm_mutex_unlock(g_schedule_mutex);
}

static void run_timers(void)
{
	int i;
	void *channel;
	expired_function_t expired_func = NULL;
	ftdm_mutex_lock(g_schedule_mutex);
	for (i = 0; i < sizeof(g_timers)/sizeof(g_timers[0]); i++) {
		/* if there's time left, decrement */
		if (g_timers[i].time) {
			g_timers[i].time--;
		}

		/* if time expired and we have an expired function, call it */
		if (!g_timers[i].time && g_timers[i].expired) {
			expired_func = g_timers[i].expired;
			channel = g_timers[i].channel;
			memset(&g_timers[i], 0, sizeof(g_timers[i]));
			expired_func(channel);
		}
	}
	ftdm_mutex_unlock(g_schedule_mutex);
}

static void release_timers(ftdm_channel_t *channel)
{
	int i;
	ftdm_mutex_lock(g_schedule_mutex);
	for (i = 0; i < sizeof(g_timers)/sizeof(g_timers[0]); i++) {
		/* clear any timer belonging to the given channel */
		if (g_timers[i].channel == channel) {
			memset(&g_timers[i], 0, sizeof(g_timers[i]));
		}
	}
	ftdm_mutex_unlock(g_schedule_mutex);
}

/*  hangup the call */ 
static void send_hangup(ftdm_channel_t *channel)
{
	ftdm_log(FTDM_LOG_NOTICE, "-- Requesting hangup in channel %d:%d\n", channel->span_id, channel->chan_id);
	ftdm_set_state_locked(channel, FTDM_CHANNEL_STATE_HANGUP);
}

/*  send answer for an incoming call */ 
static void send_answer(ftdm_channel_t *channel)
{
	 /* we move the channel signaling state machine to UP (answered) */
	ftdm_log(FTDM_LOG_NOTICE, "-- Requesting answer in channel %d:%d\n", channel->span_id, channel->chan_id);
	ftdm_set_state_locked(channel, FTDM_CHANNEL_STATE_UP);
	schedule_timer(channel, HANGUP_TIMER, send_hangup);
}

/* send progress for an incoming */
static void send_progress(ftdm_channel_t *channel)
{
	 /* we move the channel signaling state machine to UP (answered) */
	ftdm_log(FTDM_LOG_NOTICE, "-- Requesting progress\n", channel->span_id, channel->chan_id);
	ftdm_set_state_locked(channel, FTDM_CHANNEL_STATE_PROGRESS);
	schedule_timer(channel, ANSWER_TIMER, send_answer);
}

/* This function will be called in an undetermined signaling thread, you must not do 
 * any blocking operations here or the signaling stack may delay other call event processing 
 * The arguments for this function are defined in FIO_SIGNAL_CB_FUNCTION prototype, I just
 * name them here for your convenience:
 * ftdm_sigmsg_t *sigmsg
 * - The sigmsg structure contains the ftdm_channel structure that represents the channel where
 * the event occurred and the event_id of the signaling event that just occurred.
 * */
static FIO_SIGNAL_CB_FUNCTION(on_signaling_event)
{
	switch (sigmsg->event_id) {
	/* This event signals the start of an incoming call */
	case FTDM_SIGEVENT_START:
		ftdm_log(FTDM_LOG_NOTICE, "Incoming call received in channel %d:%d\n", sigmsg->span_id, sigmsg->chan_id);
		schedule_timer(sigmsg->channel, PROGRESS_TIMER, send_progress);
		break;
	/* This event signals progress on an outgoing call */
	case FTDM_SIGEVENT_PROGRESS_MEDIA:
		ftdm_log(FTDM_LOG_NOTICE, "Progress message received in channel %d:%d\n", sigmsg->span_id, sigmsg->chan_id);
		break;
	/* This event signals answer in an outgoing call */
	case FTDM_SIGEVENT_UP:
		ftdm_log(FTDM_LOG_NOTICE, "Answer received in channel %d:%d\n", sigmsg->span_id, sigmsg->chan_id);
		/* now the channel is answered and we can use 
		 * ftdm_channel_wait() to wait for input/output in a channel (equivalent to poll() or select())
		 * ftdm_channel_read() to read available data in a channel
		 * ftdm_channel_write() to write to the channel */
		break;
	/* This event signals hangup from the other end */
	case FTDM_SIGEVENT_STOP:
		ftdm_log(FTDM_LOG_NOTICE, "Hangup received in channel %d:%d\n", sigmsg->span_id, sigmsg->chan_id);
		if (g_outgoing_channel == sigmsg->channel) {
			g_outgoing_channel = NULL;
		}
		/* release any timer for this channel */
		release_timers(sigmsg->channel);
		/* acknowledge the hangup */
		ftdm_set_state_locked(sigmsg->channel, FTDM_CHANNEL_STATE_HANGUP);
		break;
	default:
		ftdm_log(FTDM_LOG_WARNING, "Unhandled event %s in channel %d:%d\n", ftdm_signal_event2str(sigmsg->event_id), 
				sigmsg->span_id, sigmsg->chan_id);
		break;
	}
	return FTDM_SUCCESS;
}

static void place_call(const ftdm_span_t *span, const char *number)
{
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_caller_data_t caller_data = {{ 0 }};
	ftdm_status_t status = FTDM_FAIL;

	/* set destiny number */
	ftdm_set_string(caller_data.dnis.digits, number);

	/* set callerid */
	ftdm_set_string(caller_data.cid_name, "testsangomaboost");
	ftdm_set_string(caller_data.cid_num.digits, "1234");

	/* request to search for an outgoing channel top down with the given caller data.
	 * it is also an option to use ftdm_channel_open_by_group to let freetdm hunt
	 * an available channel in a given group instead of per span
	 * */
	status = ftdm_channel_open_by_span(span->span_id, FTDM_TOP_DOWN, &caller_data, &ftdmchan);
	if (status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to originate call\n");
		return;
	}

	g_outgoing_channel = ftdmchan;

	/* set the caller data for the outgoing channel */
	memcpy(&ftdmchan->caller_data, &caller_data, sizeof(caller_data));

	status = ftdm_channel_outgoing_call(ftdmchan);
	if (status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to originate call\n");
		return;
	}

	/* this is required to initialize the outgoing channel */
	ftdm_channel_init(ftdmchan);
}

int main(int argc, char *argv[])
{
	ftdm_conf_parameter_t parameters[20];
	ftdm_span_t *span;
	char *todial = NULL;
	int32_t ticks = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <span name> [number to dial if any]\n", argv[0]);
		exit(-1);
	}

	/* register a handler to shutdown things properly */
	if (signal(SIGINT, interrupt_requested) == SIG_ERR) {
		fprintf(stderr, "Could not set the SIGINT signal handler: %s\n", strerror(errno));
		exit(-1);
	}

	if (argc >= 3) {
		todial = argv[2];
		if (!strlen(todial)) {
			todial = NULL;
		}
	}

	/* clear any outstanding timers */
	memset(&g_timers, 0, sizeof(g_timers));

	/* set the logging level to use */
	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	/* Initialize the FTDM library */
	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading FreeTDM\n");
		exit(-1);
	}

	/* create the schedule mutex */
	ftdm_mutex_create(&g_schedule_mutex);

	/* Load the FreeTDM configuration */
	if (ftdm_global_configuration() != FTDM_SUCCESS) {
		fprintf(stderr, "Error configuring FreeTDM\n");
		exit(-1);
	}

	/* At this point FreeTDM is ready to be used, the spans defined in freetdm.conf have the basic I/O board configuration
	 * but no telephony signaling configuration at all. */
	printf("FreeTDM loaded ...\n");

	/* Retrieve a span by name (according to freetdm.conf) */
	if (ftdm_span_find_by_name(argv[1], &span) != FTDM_SUCCESS) {
		fprintf(stderr, "Error finding FreeTDM span %s\n", argv[1]);
		goto done;
	}

	/* prepare the configuration parameters that will be sent down to the signaling stack, the array of paramters must be terminated by an 
	 * array element with a null .var member */

	/* for sangoma_boost signaling (abstraction signaling used by Sangoma for PRI, BRI and SS7) the first parameter you must send
	 * is sigmod, which must be either sangoma_prid, if you have the PRI stack available, or sangoma_brid for the BRI stack */
	parameters[0].var = "sigmod";	
	parameters[0].val = "sangoma_prid";	

	/* following parameters are signaling stack specific, this ones are for PRI */
	parameters[1].var = "switchtype";
	parameters[1].val = "national";

	parameters[2].var = "signalling";
	parameters[2].val = "pri_cpe";

	/*
	 * parameters[3].var = "nfas_primary";
	 * parameters[3].val = "4"; //span number
	 *
	 * parameters[4].var = "nfas_secondary";
	 * parameters[4].val = "2"; //span number
	 *
	 * parameters[5].var = "nfas_group";
	 * parameters[5].val = "1";
	 * */

	/* the last parameter .var member must be NULL! */
	parameters[3].var = NULL;

	/* send the configuration values down to the stack */
	if (ftdm_configure_span_signaling("sangoma_boost", span, on_signaling_event, parameters) != FTDM_SUCCESS) {
		fprintf(stderr, "Error configuring sangoma_boost signaling abstraction in span %s\n", span->name);
		goto done;
	}

	/* configuration succeeded, we can proceed now to start the span
	 * This step will launch at least 1 background (may be more, depending on the signaling stack used)
	 * to handle *ALL* signaling events for this span, your on_signaling_event callback will be called always
	 * in one of those infraestructure threads and you MUST NOT block in that handler to avoid delays and errors 
	 * in the signaling processing for any call.
	 * */
	ftdm_span_start(span);

	app_running = 1;

	/* The application thread can go on and do anything else, like waiting for a shutdown signal */
	while(ftdm_running() && app_running) {
		ftdm_sleep(1000);
		run_timers();
		ticks++;
		if (!(ticks % 10) && todial && !g_outgoing_channel) {
			ftdm_log(FTDM_LOG_NOTICE, "Originating call to number %s\n", todial);
			place_call(span, todial);
		}
	}
	printf("Shutting down FreeTDM ...\n");
 done:

	ftdm_mutex_destroy(&g_schedule_mutex);

	/* whenever you're done, this function will shutdown the signaling threads in any span that was started */
	ftdm_global_destroy();

	return 0;
}

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
