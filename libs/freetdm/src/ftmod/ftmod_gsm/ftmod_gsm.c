/*
 * Copyright (c) 2011, Sangoma Technologies 
 * All rights reserved.
 * 
  Redistribution and use in source and binary forms, with or without
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
 *
 * Contributors: 
 *
 * Gideon Sadan <gsadan@sangoma.com>
 * Moises Silva <moy@sangoma.com>
 *
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>
/*========================*/

#include <stdio.h>
#include <libwat.h>
#include <freetdm.h>
#include <private/ftdm_core.h>

#define MAX_SPANS 32
typedef struct ftdm_gsm_span_data_s {
	ftdm_span_t *span;
	fio_signal_cb_t sig_cb;
	ftdm_conf_parameter_t *ftdm_parameters;
}ftdm_gsm_span_data_t;


static ftdm_gsm_span_data_t spans_info[MAX_SPANS];
static int n_spans_info = 0;

typedef struct ftdm_gsm_data_s {

	wat_interface_t wat_interface;	

	

} ftdm_gsm_data_t;

ftdm_span_t *get_span(int span_id);

ftdm_span_t *get_span(int span_id)
{
	int i;
	for(i=0; i< n_spans_info;i++)
	{
		if(spans_info[i].span->span_id == span_id) {
			
			return spans_info[i].span;
		}

	}
	
	return NULL;
}

ftdm_channel_t *get_channel(int span_id, int channel_id);
ftdm_channel_t *get_channel(int span_id, int channel_id)
{
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_span_t * span = get_span(span_id);

	if(!span){
		return NULL;
	}


		
	
	ftdm_iterator_t *citer = ftdm_span_get_chan_iterator(span, NULL);
	
		for ( ; citer; citer = ftdm_iterator_next(citer)) {
			ftdmchan = ftdm_iterator_current(citer);
			if(ftdmchan->chan_id == channel_id) {
				ftdm_iterator_free(citer);
				return ftdmchan;
			}
			
			

			
		}

	ftdm_iterator_free(citer);
	return NULL;
}


static int read_channel(ftdm_channel_t *ftdm_chan , const void *buf, int size)
{
	
	ftdm_size_t outsize = size;
	ftdm_status_t status = ftdm_channel_read(ftdm_chan, (void *)buf, &outsize);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return (int)outsize;
}

/* wat callbacks */
void on_wat_sigstatus_change(unsigned char span_id, wat_sigstatus_t sigstatus);
void on_wat_span_alarm(unsigned char span_id, wat_alarm_t alarm);
int	 on_wat_span_write(unsigned char span_id, void *buffer, unsigned len);

void on_wat_con_ind(unsigned char span_id, uint8_t call_id, wat_con_event_t *con_event);
void on_wat_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status);
void on_wat_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event);
void on_wat_rel_cfm(unsigned char span_id, uint8_t call_id);
void on_wat_sms_ind(unsigned char span_id, wat_sms_event_t *sms_event);
void on_wat_sms_sts(unsigned char span_id, uint8_t sms_id, wat_sms_status_t *status);


void on_wat_log(uint8_t level, char *fmt, ...);
void *on_wat_malloc(size_t size);
void *on_wat_calloc(size_t nmemb, size_t size);	
void on_wat_free(void *ptr);
void on_wat_log_span(uint8_t span_id, uint8_t level, char *fmt, ...);


/*	gsm_data->wat_interface.wat_log = on_log; */
	
/*	gsm_data->wat_interface.wat
_log_span = on_log_span; */

/*		gsm_data->wat_interface.wat_malloc = on_wat_malloc;*/
/*		gsm_data->wat_interface.wat_calloc = on_wat_calloc;*/
/*	gsm_data->wat_interface.wat_free = on_wat_frspanee;*/


int on_wat_span_write(unsigned char span_id, void *buffer, unsigned len)
{
/*
	ftdm_log(FTDM_LOG_DEBUG, "!!! on_wat_span_write(%d, %s, int)\n", span_id, buffer, len);
*/
	ftdm_channel_t * ftdm_chan = get_channel(span_id, 2);
	ftdm_size_t outsize = len;
	ftdm_channel_lock(ftdm_chan);
	ftdm_status_t status = ftdm_channel_write(ftdm_chan, (void *)buffer, len, &outsize);
	ftdm_channel_unlock(ftdm_chan);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return (int)outsize;


}

void on_wat_sigstatus_change(unsigned char span_id, wat_sigstatus_t sigstatus)
{
	fprintf(stdout, "span:%d Signalling status changed %d\n", span_id, sigstatus);
	
	return;
}

void on_wat_span_alarm(unsigned char span_id, wat_alarm_t alrm)
{
	fprintf(stdout, "span:%d Alarm received\n", span_id);
	return;
}

void on_wat_con_ind(unsigned char span_id, uint8_t call_id, wat_con_event_t *con_event)
{
	fprintf(stdout, "s%d: Incoming call (id:%d) Calling Number:%s type:%d plan:%d\n", span_id, call_id, con_event->calling_num.digits, con_event->calling_num.type, con_event->calling_num.plan);

		
	return;
}

void on_wat_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status)
{
	return;
}

void on_wat_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event)
{
	fprintf(stdout, "s%d: Call hangup (id:%d) cause:%d\n", span_id, call_id, rel_event->cause);

	return;
}

void on_wat_rel_cfm(unsigned char span_id, uint8_t call_id)
{
	fprintf(stdout, "s%d: Call hangup complete (id:%d)\n", span_id, call_id);
	return;
}

void on_wat_sms_ind(unsigned char span_id, wat_sms_event_t *sms_event)
{
	return;
}

void on_wat_sms_sts(unsigned char span_id, uint8_t sms_id, wat_sms_status_t *status)
{
	return;
}



void on_wat_log(uint8_t level, char *fmt, ...)
{

	int ftdm_level;

	va_list argptr;
	va_start(argptr, fmt);
	
	char buff[10001];
	switch(level)
	{
		case WAT_LOG_CRIT:		ftdm_level = FTDM_LOG_LEVEL_CRIT; break;
		case WAT_LOG_ERROR:		ftdm_level = FTDM_LOG_LEVEL_ERROR; break;
		default:
		case WAT_LOG_WARNING:   ftdm_level = FTDM_LOG_LEVEL_WARNING; break;
		case WAT_LOG_INFO:		ftdm_level = FTDM_LOG_LEVEL_INFO; break;
		case WAT_LOG_NOTICE:	ftdm_level = FTDM_LOG_LEVEL_NOTICE; break;
		case WAT_LOG_DEBUG:		ftdm_level = FTDM_LOG_LEVEL_DEBUG; break;

	};
	
	
	vsprintf(buff, fmt, argptr);

	ftdm_log(FTDM_PRE, ftdm_level, "WAT :%s", buff);

	va_end(argptr);
}


void *on_wat_malloc(size_t size)
{
	return ftdm_malloc(size);
}
void *on_wat_calloc(size_t nmemb, size_t size)
{
	return ftdm_calloc(nmemb, size);
}	
void on_wat_free(void *ptr)
{
	ftdm_free(ptr);
}
void on_wat_log_span(uint8_t span_id, uint8_t level, char *fmt, ...)
{
	int ftdm_level;

	va_list argptr;
	va_start(argptr, fmt);
	
	char buff[10001];
	switch(level)
	{
		case WAT_LOG_CRIT:		ftdm_level = FTDM_LOG_LEVEL_CRIT; break;
		case WAT_LOG_ERROR:		ftdm_level = FTDM_LOG_LEVEL_ERROR; break;
		default:
		case WAT_LOG_WARNING:   ftdm_level = FTDM_LOG_LEVEL_WARNING; break;
		case WAT_LOG_INFO:		ftdm_level = FTDM_LOG_LEVEL_INFO; break;
		case WAT_LOG_NOTICE:	ftdm_level = FTDM_LOG_LEVEL_NOTICE; break;
		case WAT_LOG_DEBUG:		ftdm_level = FTDM_LOG_LEVEL_DEBUG; break;

	};
	
	
	vsprintf(buff, fmt, argptr);

	ftdm_log(FTDM_PRE, ftdm_level, "WAT span %d:%s", span_id, buff);

	va_end(argptr);

	
}


/* END wat callbacks */

/* span monitor thread */
static void *ftdm_gsm_run(ftdm_thread_t *me, void *obj);

/* IO interface for the command API */
static ftdm_io_interface_t g_ftdm_gsm_interface;

static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(gsm_outgoing_call)
{
	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "GSM place call not implemented yet!\n");
	return FTDM_FAIL;
}

static ftdm_status_t ftdm_gsm_start(ftdm_span_t *span)
{
	return ftdm_thread_create_detached(ftdm_gsm_run, span);
}

static ftdm_status_t ftdm_gsm_stop(ftdm_span_t *span)
{
	ftdm_log(FTDM_LOG_CRIT, "STOP not implemented yet!\n");
	return FTDM_FAIL;
}

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(ftdm_gsm_get_channel_sig_status)
{
	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "get sig status not implemented yet!\n");
	return FTDM_FAIL;
}

static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(ftdm_gsm_set_channel_sig_status)
{
	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "set sig status not implemented yet!\n");
	return FTDM_FAIL;
}

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(ftdm_gsm_get_span_sig_status)
{
	ftdm_log(FTDM_LOG_CRIT, "span get sig status not implemented yet!\n");
	return FTDM_FAIL;
}

static FIO_SPAN_SET_SIG_STATUS_FUNCTION(ftdm_gsm_set_span_sig_status)
{
	ftdm_log(FTDM_LOG_CRIT, "span set sig status not implemented yet!\n");
	return FTDM_FAIL;
}

static ftdm_state_map_t gsm_state_map = {
	{
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE, FTDM_END},
			{FTDM_CHANNEL_STATE_RESET, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESET, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
		},
		
		/* Outbound states */
		
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE, FTDM_END},
			{FTDM_CHANNEL_STATE_RESET, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESET, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_DIALING, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DIALING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},
	}
};

static ftdm_status_t ftdm_gsm_state_advance(ftdm_channel_t *ftdmchan)
{
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Executing state handler for %s\n", ftdm_channel_state2str(ftdmchan->state));
	return FTDM_SUCCESS;
}

static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_gsm_configure_span_signaling)
{
	unsigned paramindex = 0;
	const char *var = NULL;
	const char *val = NULL;


	if (n_spans_info >= MAX_SPANS) {
		snprintf(span->last_error, sizeof(span->last_error), "MAX_SPANS Exceeded !!!\n");
		ftdm_log(FTDM_LOG_DEBUG, span->last_error);
		return FTDM_FAIL;

	}

	memset(&spans_info[n_spans_info], 0 ,sizeof(spans_info[n_spans_info]));

	spans_info[n_spans_info].span = span;
	spans_info[n_spans_info].sig_cb = sig_cb;
	spans_info[n_spans_info].ftdm_parameters = ftdm_parameters;
	 n_spans_info ++;



	ftdm_gsm_data_t *gsm_data = malloc(sizeof(*gsm_data));
	if (!gsm_data) {

		snprintf(span->last_error, sizeof(span->last_error), "Failed to allocate GSM data.");
		return FTDM_FAIL;
	}
	memset(gsm_data,0, sizeof(*gsm_data));


  /* */



	ftdm_assert_return(sig_cb != NULL, FTDM_FAIL, "No signaling cb provided\n");

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return FTDM_FAIL;
	}

		
	

	for (; ftdm_parameters[paramindex].var; paramindex++) {
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		ftdm_log(FTDM_LOG_DEBUG, "Reading GSM parameter %s for span %d\n", var, span->span_id);
		if (!strcasecmp(var, "moduletype")) {
			if (!val) {
				break;
			}
			if (ftdm_strlen_zero_buf(val)) {
				ftdm_log(FTDM_LOG_NOTICE, "Ignoring empty moduletype parameter\n");
				continue;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Configuring GSM span %d for moduletype %s\n", span->span_id, val);
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown GSM parameter [%s]", var);
			return FTDM_FAIL;
		}
	}

	/* Bind function pointers for control operations */
	span->start = ftdm_gsm_start;
	span->stop = ftdm_gsm_stop;
	span->sig_read = NULL;
	span->sig_write = NULL;

	span->signal_cb = sig_cb;
	span->signal_type = FTDM_SIGTYPE_GSM;
	span->signal_data = gsm_data; /* Gideon, plz fill me with gsm span specific data (you allocate and free) */
	span->outgoing_call = gsm_outgoing_call;
	span->get_span_sig_status = ftdm_gsm_get_span_sig_status;
	span->set_span_sig_status = ftdm_gsm_set_span_sig_status;
	span->get_channel_sig_status = ftdm_gsm_get_channel_sig_status;
	span->set_channel_sig_status = ftdm_gsm_set_channel_sig_status;

	span->state_map = &gsm_state_map;
	span->state_processor = ftdm_gsm_state_advance;

	/* use signals queue */
	ftdm_set_flag(span, FTDM_SPAN_USE_SIGNALS_QUEUE);

	/* we can skip states (going straight from RING to UP) */
	ftdm_set_flag(span, FTDM_SPAN_USE_SKIP_STATES);

	

#if 0
	/* setup the scheduler (create if needed) */
	snprintf(schedname, sizeof(schedname), "ftmod_r2_%s", span->name);
	ftdm_assert(ftdm_sched_create(&r2data->sched, schedname) == FTDM_SUCCESS, "Failed to create schedule!\n");
	spanpvt->sched = r2data->sched;
#endif



ftdm_log(FTDM_LOG_DEBUG, "Registering interface to WAT Library...\n");

	gsm_data->wat_interface.wat_sigstatus_change = on_wat_sigstatus_change;
	gsm_data->wat_interface.wat_span_write = on_wat_span_write;
	
	gsm_data->wat_interface.wat_log = on_wat_log;
	gsm_data->wat_interface.wat_log_span = on_wat_log_span; 
	gsm_data->wat_interface.wat_malloc = on_wat_malloc;
	gsm_data->wat_interface.wat_calloc = on_wat_calloc;
	gsm_data->wat_interface.wat_free = on_wat_free;
	
	gsm_data->wat_interface.wat_alarm   = on_wat_span_alarm;
	gsm_data->wat_interface.wat_con_ind = on_wat_con_ind;
	gsm_data->wat_interface.wat_con_sts = on_wat_con_sts;
	gsm_data->wat_interface.wat_rel_ind = on_wat_rel_ind;
	gsm_data->wat_interface.wat_rel_cfm = on_wat_rel_cfm;
	gsm_data->wat_interface.wat_sms_ind = on_wat_sms_ind;
	gsm_data->wat_interface.wat_sms_sts = on_wat_sms_sts;
	
	if (wat_register(&gsm_data->wat_interface)) {
		snprintf(span->last_error, sizeof(span->last_error), "Failed to register WAT Library !!!\n");
		ftdm_log(FTDM_LOG_DEBUG, "FAILED Registering interface to WAT Library...\n");
		return FTDM_FAIL;

	}
	ftdm_log(FTDM_LOG_DEBUG, "Registered interface to WAT Library\n");


 

	ftdm_log(FTDM_LOG_DEBUG, "Configuring span\n");

	//sng_fd_t dev;
	//sangoma_wait_obj_t *waitable;
	//unsigned char wat_span_id;


	wat_span_config_t _wat_span_config;


	_wat_span_config.moduletype = WAT_MODULE_TELIT;
	_wat_span_config.timeout_cid_num = 10;
	
	if (wat_span_config(span->span_id, &_wat_span_config)) {
		fprintf(stderr, "Failed to configure span!!\n");
		return FTDM_FAIL;
	}

	fprintf(stdout, "Starting span\n");
	if (wat_span_start(span->span_id)) {
		fprintf(stderr, "Failed to start span!!\n");
		return FTDM_FAIL;
	}




	



	return FTDM_SUCCESS;

}

static void *ftdm_gsm_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *citer = NULL;
	int waitms = 10, i;
	ftdm_status_t status;

	
	short *poll_events = ftdm_malloc(sizeof(short) * span->chan_count);
	
	unsigned next;
	ftdm_log(FTDM_LOG_DEBUG, "GSM monitor thread for span %s started\n", span->name);

	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	if (!chaniter) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to allocate channel iterator for span %s!\n", span->name);
		goto done;
	}

  ftdmchan = get_channel(span->span_id, 2);

	if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to open channel during incoming call! [%s]\n", ftdmchan->last_error);
		return NULL;
	}

	while (ftdm_running()) {


	wat_span_run(span->span_id);
	next = wat_span_schedule_next(span->span_id);
	if(next < waitms) {
		next = waitms;
	}

#if 0
		/* run any span timers */
		ftdm_sched_run(r2data->sched);
			
#endif
		/* deliver the actual channel events to the user now without any channel locking */
		ftdm_span_trigger_signals(span);
#if 0
		 /* figure out what event to poll each channel for. POLLPRI when the channel is down,
		  * POLLPRI|POLLIN|POLLOUT otherwise */
		memset(poll_events, 0, sizeof(short)*span->chan_count);
		citer = ftdm_span_get_chan_iterator(span, chaniter);
		if (!citer) {
			ftdm_log(Fshort *poll_events = ftdm_malloc(sizeof(short) * span->chan_count);TDM_LOG_CRIT, "Failed to allocate channel iterator for span %s!\n", span->name);
			goto done;short *poll_events = ftdm_malloc(sizeof(short) * span->chan_count);
		}
		for (i = 0; citer; citer = ftdm_iterator_next(citer), i++) {
			ftdmchan = ftdm_iterator_current(citer);
			r2chan = R2CALL(ftdmchan)->r2chan;
			poll_events[i] = FTDM_EVENTS;
			if (openr2_chan_get_read_enabled(r2chan)) {
				poll_events[i] |= FTDM_READ;
			}
		}
		status = ftdm_span_poll_event(span, waitms, poll_events);

		/* run any span timers */
		ftdm_sched_runshort *poll_events = ftdm_malloc(sizeof(short) * span->chan_count);(r2data->sched);
#endif
		ftdm_sleep(waitms);


		/* this main loop takes care of MF and CAS signaling during call setup and tear down
		 * for every single channel in the span, do not perform blocking operations here! */
		citer = ftdm_span_get_chan_iterator(span, chaniter);
		for ( ; citer; citer = ftdm_iterator_next(citer)) {
			ftdmchan = ftdm_iterator_current(citer);

			ftdm_channel_lock(ftdmchan);

			ftdm_channel_advance_states(ftdmchan);

			ftdm_channel_unlock(ftdmchan);

		}

		for(i=0;i< span->chan_count; i++)
			poll_events[i] = FTDM_EVENTS;

		poll_events[1]  |= FTDM_READ;
		status = ftdm_span_poll_event(span, next, poll_events);

		if(FTDM_SUCCESS == status)
		{
			ftdm_channel_lock(ftdmchan);
			ftdm_channel_t * ftdm_chan = get_channel(span->span_id, 2);
			char buffer[11];
			int n = read_channel(ftdm_chan , buffer, sizeof(buffer));
			ftdm_channel_unlock(ftdmchan);
			if(n > 0) {
		
				wat_span_process_read(span->span_id, buffer, n);
				/*
				ftdm_log(FTDM_LOG_DEBUG, "!!! read_channel got %d bytes\n", n);
				*/
			}
			else	{
				ftdm_sleep(waitms);
			}

		}
	}

done:
	ftdm_iterator_free(chaniter);

	ftdm_log(FTDM_LOG_DEBUG, "GSM thread ending.\n");

	return NULL;
}

#define FT_SYNTAX "USAGE:\n" \
"--------------------------------------------------------------------------------\n" \
"ftdm gsm status <span_id|span_name>\n" \
"--------------------------------------------------------------------------------\n"
static FIO_API_FUNCTION(ftdm_gsm_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (data) {
		mycmd = ftdm_strdup(data);

		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc == 1) {
		if (!strcasecmp(argv[0], "version")) {
			uint8_t current = 0, revision = 0, age = 0;
			wat_version(&current, &revision, &age);
			stream->write_function(stream, "libwat GSM VERSION: %d.%d.%d\n", current, revision, age);
			stream->write_function(stream, "+OK.\n");
			goto done;
		}

		if (!strcasecmp(argv[0], "status")) {
			
			/*wat_chip_info_t* chip_info =  wat_span_get_chip_info(span->span_id);		*/
			

			
			stream->write_function(stream, "+OK.\n");
			goto done;
		}
	}

	stream->write_function(stream, "%s", FT_SYNTAX);

done:

	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;

}

static FIO_IO_LOAD_FUNCTION(ftdm_gsm_io_init)
{
	assert(fio != NULL);
	memset(&g_ftdm_gsm_interface, 0, sizeof(g_ftdm_gsm_interface));

	g_ftdm_gsm_interface.name = "gsm";
	g_ftdm_gsm_interface.api = ftdm_gsm_api;

	*fio = &g_ftdm_gsm_interface;

	return FTDM_SUCCESS;
}
static FIO_SIG_LOAD_FUNCTION(ftdm_gsm_init)
{
	/* this is called on module load */
	return FTDM_SUCCESS;
}

static FIO_SIG_UNLOAD_FUNCTION(ftdm_gsm_destroy)
{
	/* this is called on module unload */
	return FTDM_SUCCESS;
}

EX_DECLARE_DATA ftdm_module_t ftdm_module = { 
	/* .name */ "gsm",
	/* .io_load */ ftdm_gsm_io_init,
	/* .io_unload */ NULL,
	/* .sig_load */ ftdm_gsm_init,
	/* .sig_configure */ NULL,
	/* .sig_unload */ ftdm_gsm_destroy,
	/* .configure_span_signaling */ ftdm_gsm_configure_span_signaling
};
	

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
