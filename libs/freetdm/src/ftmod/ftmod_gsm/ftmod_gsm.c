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

typedef struct ftdm_gsm_span_data_s {
	ftdm_span_t *span;
	ftdm_channel_t *dchan;
} ftdm_gsm_span_data_t;

static ftdm_status_t init_wat_lib(void);
static int wat_lib_initialized = 0;

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
int on_wat_span_write(unsigned char span_id, void *buffer, unsigned len);

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

int on_wat_span_write(unsigned char span_id, void *buffer, unsigned len)
{
/*	ftdm_log(FTDM_LOG_DEBUG, "====================>>> %s (%d) - %d\n", buffer, len, (int) span_id);*/
	ftdm_span_t *span = NULL;
	ftdm_status_t status = FTDM_FAIL;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	ftdm_size_t outsize = len;

	status = ftdm_span_find(span_id, &span);
	if (status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to find span %d to write %d bytes\n", span_id, len);
		return -1;
	}

	gsm_data = span->signal_data;
	status = ftdm_channel_write(gsm_data->dchan, (void *)buffer, len, &outsize);
	if (status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to write %d bytes to d-channel in span %s\n", len, span->name);
		return -1;
	}
	return (int)outsize;


}

static void on_wat_span_status(unsigned char span_id, wat_span_status_t *status)
{
	switch (status->type) {
	case WAT_SPAN_STS_READY:
		{
			ftdm_log(FTDM_LOG_INFO, "span %d: Ready\n", span_id);
		}
		break;
	case WAT_SPAN_STS_SIGSTATUS:
		{
			if (status->sts.sigstatus == WAT_SIGSTATUS_UP) {
				ftdm_log(FTDM_LOG_INFO, "span %d: Signaling is now up\n", span_id);
			} else {
				ftdm_log(FTDM_LOG_INFO, "span %d: Signaling is now down\n", span_id);
			}
		}
		break;
	case WAT_SPAN_STS_SIM_INFO_READY:
		{
			ftdm_log(FTDM_LOG_INFO, "span %d: SIM information ready\n", span_id);
		}
		break;
	case WAT_SPAN_STS_ALARM:
		{
			ftdm_log(FTDM_LOG_INFO, "span %d: Alarm received\n", span_id);
		}
		break;
	default:
		{
			ftdm_log(FTDM_LOG_INFO, "span %d: Unhandled span status notification %d\n", span_id, status->type);
		}
		break;
	}
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
	if (wat_span_start(span->span_id)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to start span %s!\n", span->name);
		return FTDM_FAIL;
	}

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



static ftdm_status_t init_wat_lib(void)
{
	if (wat_lib_initialized) {
		return FTDM_SUCCESS;
	}

	wat_interface_t wat_interface;

	ftdm_log(FTDM_LOG_DEBUG, "Registering interface to WAT Library...\n");

	memset(&wat_interface, 0, sizeof(wat_interface));
	wat_interface.wat_span_write = on_wat_span_write;
	
	wat_interface.wat_log = on_wat_log;
	wat_interface.wat_log_span = on_wat_log_span; 
	wat_interface.wat_malloc = on_wat_malloc;
	wat_interface.wat_calloc = on_wat_calloc;
	wat_interface.wat_free = on_wat_free;
	
	wat_interface.wat_con_ind = on_wat_con_ind;
	wat_interface.wat_con_sts = on_wat_con_sts;
	wat_interface.wat_rel_ind = on_wat_rel_ind;
	wat_interface.wat_rel_cfm = on_wat_rel_cfm;
	wat_interface.wat_sms_ind = on_wat_sms_ind;
	wat_interface.wat_sms_sts = on_wat_sms_sts;
	wat_interface.wat_span_sts = on_wat_span_status;
	
	if (wat_register(&wat_interface)) {
		ftdm_log(FTDM_LOG_DEBUG, "Failed registering interface to WAT library ...\n");
		return FTDM_FAIL;

	}
	ftdm_log(FTDM_LOG_DEBUG, "Registered interface to WAT library\n");

	wat_lib_initialized = 1;
	return FTDM_SUCCESS;
}


static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_gsm_configure_span_signaling)
{
	wat_span_config_t span_config;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *citer = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_channel_t *dchan = NULL;
	unsigned paramindex = 0;
	const char *var = NULL;
	const char *val = NULL;

	/* libwat is smart enough to set good default values for the timers if they are set to 0 */
	memset(&span_config, 0, sizeof(span_config));
	
	/* set some span defaults */
	span_config.moduletype = WAT_MODULE_TELIT;

	if (FTDM_SUCCESS != init_wat_lib()) {
		return FTDM_FAIL;
	}

	if (!sig_cb) {
		ftdm_log(FTDM_LOG_ERROR, "No signaling callback provided\n");
		return FTDM_FAIL;
	}

	if (span->signal_type) {
		ftdm_log(FTDM_LOG_ERROR, "Span %s is already configured for another signaling\n", span->name);
		return FTDM_FAIL;
	}

	/* verify the span has one d-channel */
	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	if (!chaniter) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to allocate channel iterator for span %s!\n", span->name);
		return FTDM_FAIL;
	}

	citer = ftdm_span_get_chan_iterator(span, chaniter);
	for ( ; citer; citer = ftdm_iterator_next(citer)) {
		ftdmchan = ftdm_iterator_current(citer);
		if (FTDM_IS_DCHAN(ftdmchan)) {
			dchan = ftdmchan;
			break;
		}
	}
	ftdm_iterator_free(chaniter);

	if (!dchan) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find a d-channel for GSM span %s!\n", span->name);
		return FTDM_FAIL;
	}

	gsm_data = ftdm_calloc(1, sizeof(*gsm_data));
	if (!gsm_data) {
		return FTDM_FAIL;
	}
	gsm_data->dchan = dchan;

	for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) {
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		if (!ftdm_strlen_zero_buf(val)) {
			ftdm_log(FTDM_LOG_WARNING, "Ignoring empty GSM parameter %s for span %s\n", var, val, span->name);
			continue;
		}
		ftdm_log(FTDM_LOG_DEBUG, "Reading GSM parameter %s=%s for span %s\n", var, val, span->name);
		if (!strcasecmp(var, "moduletype")) {
			span_config.moduletype = wat_str2wat_moduletype(val);
			if (span_config.moduletype == WAT_MODULE_INVALID) {
				ftdm_log(FTDM_LOG_DEBUG, "Unknown GSM module type %s for span %s\n", val, span->name);
				continue;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Configuring GSM span %s with moduletype %s\n", span->name, val);
		} else {
			ftdm_log(FTDM_LOG_ERROR, "Ignoring unknown GSM parameter '%s'", var);
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
	ftdm_set_flag(span, FTDM_SPAN_USE_CHAN_QUEUE);

	/* we can skip states (going straight from RING to UP) */
	ftdm_set_flag(span, FTDM_SPAN_USE_SKIP_STATES);

	gsm_data->span = span;

#if 0
	/* setup the scheduler (create if needed) */
	snprintf(schedname, sizeof(schedname), "ftmod_r2_%s", span->name);
	ftdm_assert(ftdm_sched_create(&r2data->sched, schedname) == FTDM_SUCCESS, "Failed to create schedule!\n");
	spanpvt->sched = r2data->sched;
#endif

	
	if (wat_span_config(span->span_id, &span_config)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to configure span %s for GSM signaling!!\n", span->name);
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;

}

#define GSM_POLL_INTERVAL_MS 20
static void *ftdm_gsm_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_wait_flag_t ioflags = FTDM_NO_FLAGS;
	ftdm_interrupt_t *data_sources[2] = {NULL, NULL};
	int waitms = 0;
	
	gsm_data = span->signal_data;

	ftdm_assert_return(gsm_data != NULL, NULL, "No gsm data attached to span\n");

	ftdm_log(FTDM_LOG_DEBUG, "GSM monitor thread for span %s started\n", span->name);
	if (!gsm_data->dchan || ftdm_channel_open_chan(gsm_data->dchan) != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to open GSM d-channel of span %s!\n", span->name);
		gsm_data->dchan = NULL;
		goto done;
	}

	/* create an interrupt object to wait for data from the d-channel device */
	if (ftdm_interrupt_create(&data_sources[0], gsm_data->dchan->sockfd, FTDM_READ) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create GSM d-channel interrupt for span %s\n", span->name);
		goto done;
	}
	status = ftdm_queue_get_interrupt(span->pendingchans, &data_sources[1]);
	if (status != FTDM_SUCCESS || data_sources[1] == NULL) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to retrieve channel queue interrupt for span %s\n", span->name);
		goto done;
	}

	while (ftdm_running()) {

		wat_span_run(span->span_id);

		waitms = wat_span_schedule_next(span->span_id);
		if (waitms > GSM_POLL_INTERVAL_MS) {
			waitms = GSM_POLL_INTERVAL_MS;
		}

#if 0
		/* run any span timers */
		ftdm_sched_run(r2data->sched);
			
#endif
		status = ftdm_interrupt_multiple_wait(data_sources, ftdm_array_len(data_sources), waitms);
		switch (status) {
		case FTDM_ETIMEDOUT:
			break;
		case FTDM_SUCCESS:
			{
				/* process first the d-channel if ready */
				if ((ioflags = ftdm_interrupt_device_ready(data_sources[0])) != FTDM_NO_FLAGS) {
					char buffer[1024];
					unsigned int n = 0;
					n = read_channel(gsm_data->dchan, buffer, sizeof(buffer));
					/* this may trigger any of the callbacks registered through wat_register() */
					wat_span_process_read(span->span_id, buffer, n);
				}

				/* now process all channels with state changes pending */			
				while ((ftdmchan = ftdm_queue_dequeue(span->pendingchans))) {
					/* double check that this channel has a state change pending */
					ftdm_channel_lock(ftdmchan);
					ftdm_channel_advance_states(ftdmchan);
					ftdm_channel_unlock(ftdmchan);
				}

				/* deliver the actual channel events to the user now without any channel locking */
				ftdm_span_trigger_signals(span);
			}
			break;
		case FTDM_FAIL:
			ftdm_log(FTDM_LOG_ERROR, "%s: ftdm_interrupt_wait returned error!\n", span->name);
			break;

		default:
			ftdm_log(FTDM_LOG_ERROR, "%s: ftdm_interrupt_wait returned with unknown code\n", span->name);
			break;
		}

	}

done:
	if (data_sources[0]) {
		ftdm_interrupt_destroy(&data_sources[0]);
	}

	ftdm_log(FTDM_LOG_DEBUG, "GSM thread ending.\n");

	return NULL;
}

#define FT_SYNTAX "USAGE:\n" \
"--------------------------------------------------------------------------------\n" \
"ftdm gsm status <span_id|span_name>\n" \
"--------------------------------------------------------------------------------\n"
static FIO_API_FUNCTION(ftdm_gsm_api)
{
	ftdm_span_t *span = NULL;
	int span_id = 0;
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!strcasecmp(argv[0], "version")) {
		uint8_t current = 0, revision = 0, age = 0;
		wat_version(&current, &revision, &age);
		stream->write_function(stream, "libwat version: %d.%d.%d\n", current, revision, age);
		stream->write_function(stream, "+OK.\n");
		goto done;

	} else if (!strcasecmp(argv[0], "status")) {
		const wat_chip_info_t *chip_info = NULL;
		const wat_sim_info_t *sim_info = NULL;
		const wat_net_info_t *net_info = NULL;
		const wat_sig_info_t *sig_info = NULL;
		const wat_pin_stat_t *pin_stat = NULL;

		if (argc < 2) {
			goto syntax;
		}

		span_id = atoi(argv[1]);
		if (ftdm_span_find_by_name(argv[1], &span) != FTDM_SUCCESS && 
		    ftdm_span_find(span_id, &span) != FTDM_SUCCESS) {
			stream->write_function(stream, "-ERR Failed to find GSM span '%s'\n",  argv[1]);
			goto done;
		}

		if (!span || !span->signal_data || (span->start != ftdm_gsm_start)) {
			stream->write_function(stream, "-ERR '%s' is not a valid GSM span\n",  argv[1]);
			goto done;
		}

		chip_info = wat_span_get_chip_info(span->span_id);	
		sim_info = wat_span_get_sim_info(span->span_id);
		net_info = wat_span_get_net_info(span->span_id);
		sig_info = wat_span_get_sig_info(span->span_id);
		pin_stat = wat_span_get_pin_info(span->span_id);
		
		stream->write_function(stream, "Span %d (%s):\n",  span->span_id, span->name);

		stream->write_function(stream, "CHIP - %s (%s), revision %s, serial  %s \n", 
				chip_info->manufacturer_name,
				chip_info->manufacturer_id,
				chip_info->revision,
				chip_info->serial);
	
		stream->write_function(stream, "SIM - Subscriber type %s, imsi %s\n", sim_info->subscriber_type, sim_info->imsi);		

		stream->write_function(stream, "Subscriber - Number %s, Plan %s, validity %s\n", 
				sim_info->subscriber.digits,
				wat_number_type2str(sim_info->subscriber.type),
				wat_number_plan2str(sim_info->subscriber.plan),
				wat_number_validity2str(sim_info->subscriber.validity));		

		stream->write_function(stream, "Network - status %s, Area Code %d,  Cell ID %d, Operator %s\n", 
			wat_net_stat2str(net_info->stat), net_info->lac, net_info->ci, net_info->operator_name);		

			
		stream->write_function(stream, "\n");
		
		stream->write_function(stream, "+OK.\n");
		goto done;
	}

syntax:
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

	return (FTDM_SUCCESS);
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
