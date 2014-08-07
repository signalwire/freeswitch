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

#ifndef __WINDOWS__
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#endif

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
/*========================*/ 

#include <stdio.h>
#include <libwat.h>
#include <freetdm.h>

#include <private/ftdm_core.h>

/********************************************************************************/
/*                                                                              */
/*                                  MACROS                                      */
/*                                                                              */
/********************************************************************************/
// Macro to send signals
#define SEND_STATE_SIGNAL(sig) \
	{ \
		ftdm_sigmsg_t sigev; \
		memset(&sigev, 0, sizeof(sigev)); \
		sigev.event_id = sig; \
		sigev.channel = ftdmchan; \
		ftdm_span_send_signal(ftdmchan->span, &sigev); \
	}

// Syntax message
#define FT_SYNTAX "USAGE:\n" \
"--------------------------------------------------------------------------------\n" \
"ftdm gsm version \n" \
"ftdm gsm status <span_id|span_name>\n" \
"ftdm gsm sms <span_id|span_name> <destination> <text>\n" \
"ftdm gsm exec <span_id|span_name> <command string>\n" \
"ftdm gsm call <span_id|span_name> [number]\n" \
"--------------------------------------------------------------------------------\n"

// Used to declare command handler
#define COMMAND_HANDLER(name) \
	ftdm_status_t fCMD_##name(ftdm_stream_handle_t *stream, char *argv[], int argc); \
	ftdm_status_t fCMD_##name(ftdm_stream_handle_t *stream, char *argv[], int argc)

// Used to define command entry in the command map.
#define COMMAND(name, argc)  {#name, argc, fCMD_##name}

/********************************************************************************/
/*                                                                              */
/*                                  types                                       */
/*                                                                              */
/********************************************************************************/

// private data
typedef struct ftdm_gsm_span_data_s {
	ftdm_span_t *span;
	ftdm_channel_t *dchan;
	ftdm_channel_t *bchan;
	int32_t call_id;
	uint32_t sms_id;
	char conditional_forward_number[255];
	ftdm_sched_t *sched;
	ftdm_timer_id_t conditional_forwarding_timer;
	ftdm_bool_t init_forwarding;
} ftdm_gsm_span_data_t;

// command handler function type.
typedef ftdm_status_t (*fCMD)(ftdm_stream_handle_t *stream, char *argv[], int argc);

typedef struct ftdm_gsm_exec_helper {
	ftdm_span_t *span;
	ftdm_stream_handle_t *stream;
	uint8_t cmd_pending;
} ftdm_gsm_exec_helper_t;

/********************************************************************************/
/*                                                                              */
/*                           function declaration                               */
/*                                                                              */
/********************************************************************************/
static ftdm_status_t init_wat_lib(void);
static int wat_lib_initialized = 0;
static FIO_API_FUNCTION(ftdm_gsm_api);



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


ftdm_span_t *GetSpanByID(unsigned char span_id, ftdm_gsm_span_data_t **gsm_data);

static void *ftdm_gsm_run(ftdm_thread_t *me, void *obj);

/********************************************************************************/
/*                                                                              */
/*                           static & global data                               */
/*                                                                              */
/********************************************************************************/

/* At the moment we support only one concurrent call per span, so no need to have different ids */
#define GSM_OUTBOUND_CALL_ID 8

/* IO interface for the command API */
static ftdm_io_interface_t g_ftdm_gsm_interface;

/********************************************************************************/
/*                                                                              */
/*                              implementation                                  */
/*                                                                              */
/********************************************************************************/
static int read_channel(ftdm_channel_t *ftdm_chan , const void *buf, int size)
{
	ftdm_size_t outsize = size;
	ftdm_status_t status = ftdm_channel_read(ftdm_chan, (void *)buf, &outsize);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return (int)outsize;
}


int on_wat_span_write(unsigned char span_id, void *buffer, unsigned len)
{
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
		char errbuf[255];
		ftdm_log(FTDM_LOG_ERROR, "Failed to write %d bytes to d-channel in span %s: %s\n", len, span->name, strerror_r(errno, errbuf, sizeof(errbuf)));
		return -1;
	}
	return len;
}

static void ftdm_gsm_make_raw_call(ftdm_gsm_span_data_t *gsm_data, const char *number)
{
	wat_con_event_t con_event;

	ftdm_channel_lock(gsm_data->bchan);

	if (ftdm_test_flag(gsm_data->bchan, FTDM_CHANNEL_INUSE)) {
		ftdm_log_chan(gsm_data->bchan, FTDM_LOG_ERROR, "Failed to place raw call to %s: channel busy\n", number);
		goto done;
	}

	ftdm_log_chan(gsm_data->bchan, FTDM_LOG_INFO, "Placing raw call to %s\n", number);
	ftdm_set_flag(gsm_data->bchan, FTDM_CHANNEL_INUSE);

	gsm_data->call_id = g_outbound_call_id++;
	memset(&con_event, 0, sizeof(con_event));
	ftdm_set_string(con_event.called_num.digits, number);
	wat_con_req(gsm_data->span->span_id, gsm_data->call_id , &con_event);

done:
	ftdm_channel_unlock(gsm_data->bchan);
}

static void ftdm_gsm_enable_conditional_forwarding(void *data)
{
	ftdm_gsm_span_data_t *gsm_data = data;
	ftdm_log_chan(gsm_data->bchan, FTDM_LOG_NOTICE, "Enabling conditional forwarding to %s\n", gsm_data->conditional_forward_number);
	ftdm_gsm_make_raw_call(data, gsm_data->conditional_forward_number);
}

static void on_wat_span_status(unsigned char span_id, wat_span_status_t *status)
{
	ftdm_span_t *span = NULL;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	if (!(span = GetSpanByID(span_id, &gsm_data))) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to get span from id %d\n", span_id);
		return;
	}
	gsm_data = span->signal_data;

	switch (status->type) {
	case WAT_SPAN_STS_READY:
		{
			ftdm_log(FTDM_LOG_INFO, "span %d: Ready\n", span_id);
		}
		break;
	case WAT_SPAN_STS_SIGSTATUS:
		{
			if (status->sts.sigstatus == WAT_SIGSTATUS_UP) {
				ftdm_log_chan_msg(gsm_data->bchan, FTDM_LOG_INFO, "Signaling is now up\n");
			} else {
				ftdm_log_chan_msg(gsm_data->bchan, FTDM_LOG_INFO, "Signaling is now down\n");
			}
			if (gsm_data->init_forwarding == FTDM_TRUE && !ftdm_strlen_zero_buf(gsm_data->conditional_forward_number)) {
				ftdm_sched_timer(gsm_data->sched, "conditional_forwarding_delay", 500,
						ftdm_gsm_enable_conditional_forwarding,
						gsm_data,
						&gsm_data->conditional_forwarding_timer);
				gsm_data->init_forwarding = FTDM_FALSE;
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
	ftdm_span_t *span = NULL;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	
	ftdm_log(FTDM_LOG_INFO, "s%d: Incoming call (id:%d) Calling Number:%s  Calling Name:\"%s\" type:%d plan:%d\n", span_id, call_id, con_event->calling_num.digits, con_event->calling_name, con_event->calling_num.type, con_event->calling_num.plan);
	
	if(!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}	  

	gsm_data->call_id = call_id;			
	
	#define ZERO_ARRAY(arr) memset(arr, 0, sizeof(arr))
	// cid date
	{
		time_t t;
		struct tm *tmp;
		t = time(NULL);
		tmp = localtime(&t);
		if (tmp == NULL) {
			ZERO_ARRAY(gsm_data->bchan->caller_data.cid_date);
			strftime(gsm_data->bchan->caller_data.cid_date, sizeof(gsm_data->bchan->caller_data.cid_date), "%y/%m/%d", tmp);
		}

	}

	// cid name
	ZERO_ARRAY(gsm_data->bchan->caller_data.cid_name);
	strncpy(gsm_data->bchan->caller_data.cid_name, con_event->calling_name,sizeof(gsm_data->bchan->caller_data.cid_name));

	// dnis
	ZERO_ARRAY(gsm_data->bchan->caller_data.dnis.digits);
	strncpy(gsm_data->bchan->caller_data.dnis.digits, con_event->calling_num.digits, FTDM_DIGITS_LIMIT);
	
	//collected
	ZERO_ARRAY(gsm_data->bchan->caller_data.collected);
	strncpy(gsm_data->bchan->caller_data.collected, con_event->calling_num.digits, FTDM_DIGITS_LIMIT);
	
	ftdm_set_state(gsm_data->bchan, FTDM_CHANNEL_STATE_RING);

	if (ftdm_channel_open_chan(gsm_data->bchan) != FTDM_SUCCESS) {
		ftdm_log_chan(gsm_data->bchan, FTDM_LOG_ERROR, "Failed to open GSM b-channel of span %s!\n", span->name);
	}
	
}


ftdm_span_t *GetSpanByID(unsigned char span_id, ftdm_gsm_span_data_t **gsm_data)
{
	ftdm_status_t ftdm_status = FTDM_FAIL;
	ftdm_span_t *span = NULL;
	if(gsm_data) {
		(*gsm_data) = NULL;
	}

	ftdm_status = ftdm_span_find(span_id, &span);
	if (ftdm_status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "GetSpanByID - Failed to find span %d\n", span_id);
		return NULL;
	}

	if(gsm_data) {
		(*gsm_data) = span->signal_data;
	}

	return span;

}

void on_wat_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status)
{

	ftdm_span_t *span = NULL;
	ftdm_channel_state_t state =  FTDM_CHANNEL_STATE_END;
	//ftdm_status_t ftdm_status = FTDM_FAIL;
	ftdm_gsm_span_data_t *gsm_data = NULL;

	if (!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}

	switch (status->type) {
		case WAT_CON_STATUS_TYPE_RINGING:
			ftdm_log_chan_msg(gsm_data->bchan, FTDM_LOG_INFO, "Received ringing indication\n");
			state = FTDM_CHANNEL_STATE_RINGING;
		break;		
	
		case WAT_CON_STATUS_TYPE_ANSWER:
			ftdm_log_chan_msg(gsm_data->bchan, FTDM_LOG_INFO, "Received answer indication\n");
			state = FTDM_CHANNEL_STATE_PROGRESS_MEDIA;
		break;

		default:
			ftdm_log_chan(gsm_data->bchan, FTDM_LOG_WARNING, "Unhandled indication status %d\n", status->type);
		break;

	};

	if (state != FTDM_CHANNEL_STATE_END && gsm_data->bchan->state != FTDM_CHANNEL_STATE_DOWN) {
		ftdm_set_state(gsm_data->bchan, state);
	}
}

void on_wat_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event)
{
	ftdm_span_t *span = NULL;
	ftdm_gsm_span_data_t *gsm_data = NULL;

	ftdm_log(FTDM_LOG_INFO, "s%d: Call hangup (id:%d) cause:%d\n", span_id, call_id, rel_event->cause);

	if (!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}

	if (gsm_data->bchan->state == FTDM_CHANNEL_STATE_DOWN) {
		/* This is most likely due to a call to enable call
		 * forwarding, which does not run the state machine */
		ftdm_clear_flag(gsm_data->bchan, FTDM_CHANNEL_INUSE);
		wat_rel_req(span_id, call_id);
		return;
	}

	if (gsm_data->bchan->state > FTDM_CHANNEL_STATE_DOWN &&
		gsm_data->bchan->state < FTDM_CHANNEL_STATE_HANGUP) {
		ftdm_set_state(gsm_data->bchan, FTDM_CHANNEL_STATE_HANGUP);
	}
}

void on_wat_rel_cfm(unsigned char span_id, uint8_t call_id)
{
	ftdm_span_t *span = NULL;
	//ftdm_status_t ftdm_status = FTDM_FAIL;
	ftdm_gsm_span_data_t *gsm_data = NULL;

	ftdm_log(FTDM_LOG_INFO, "s%d: Call hangup complete (id:%d)\n", span_id, call_id);
  
	if (!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}

	if (gsm_data->bchan->state == FTDM_CHANNEL_STATE_DOWN) {
		/*  this is most likely a call forwarding enable call, which do not run the state machine */
		ftdm_clear_flag(gsm_data->bchan, FTDM_CHANNEL_INUSE);
		return;
	}

	switch(gsm_data->dchan->state) {
	case FTDM_CHANNEL_STATE_UP:
		ftdm_set_state(gsm_data->bchan, FTDM_CHANNEL_STATE_HANGUP);
		break;
	default:
		ftdm_set_state(gsm_data->bchan, FTDM_CHANNEL_STATE_DOWN);
		break;
	}
}

void on_wat_sms_ind(unsigned char span_id, wat_sms_event_t *sms_event)
{
	ftdm_span_t *span = NULL;
	ftdm_channel_t *ftdmchan;

	ftdm_gsm_span_data_t *gsm_data = NULL;

	if(!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}

	ftdmchan = gsm_data->dchan;

	{
		ftdm_sms_data_t sms_data;
		ftdm_sigmsg_t sigev;
		memset(&sms_data, 0, sizeof(sms_data));

		strncpy(sms_data.from, sms_event->from.digits, sizeof(sms_data.from));
		strncpy(sms_data.body, sms_event->content.data, sizeof(sms_data.body));

		memset(&sigev, 0, sizeof(sigev));
		sigev.event_id = FTDM_SIGEVENT_SMS;
		sigev.channel = ftdmchan ;
		gsm_data->dchan->caller_data.priv = (void *)&sms_data;
		ftdm_span_send_signal(span, &sigev);
	}
	return;
}

void on_wat_sms_sts(unsigned char span_id, uint8_t sms_id, wat_sms_status_t *status)
{

	if(status->success) {
		ftdm_log(FTDM_LOG_INFO, "Span %d SMS Send - OK\n", span_id );
	}
	else {
		ftdm_log(FTDM_LOG_CRIT, "Span %d SMS Send - FAIL (%s)\n", span_id, status->error);

	}
	
	
	return;
}

static void on_wat_dtmf_ind(unsigned char span_id, const char *dtmf)
{
	ftdm_span_t *span = NULL;
	ftdm_gsm_span_data_t *gsm_data = NULL;

	if (!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}

	ftdm_channel_queue_dtmf(gsm_data->bchan, dtmf);
}


void on_wat_log(uint8_t level, char *fmt, ...)
{
	int ftdm_level;
	char buff[4096];

	va_list argptr;
	va_start(argptr, fmt);
	
	switch(level) {
		case WAT_LOG_CRIT:	ftdm_level = FTDM_LOG_LEVEL_CRIT; break;
		case WAT_LOG_ERROR:	ftdm_level = FTDM_LOG_LEVEL_ERROR; break;
		case WAT_LOG_WARNING:   ftdm_level = FTDM_LOG_LEVEL_WARNING; break;
		case WAT_LOG_INFO:	ftdm_level = FTDM_LOG_LEVEL_INFO; break;
		case WAT_LOG_NOTICE:	ftdm_level = FTDM_LOG_LEVEL_NOTICE; break;
		case WAT_LOG_DEBUG:	ftdm_level = FTDM_LOG_LEVEL_DEBUG; break;
		default:		ftdm_level = FTDM_LOG_LEVEL_ERROR; break;
	};
	
	
	vsprintf(buff, fmt, argptr);

	ftdm_log(FTDM_PRE, ftdm_level, "%s", buff);

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
	ftdm_span_t *span = NULL;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	int ftdm_level;
	char buff[4096];
	va_list argptr;

	if (!(span = GetSpanByID(span_id, &gsm_data))) {
		return;
	}

	va_start(argptr, fmt);
	
	switch(level) {
		case WAT_LOG_CRIT:	ftdm_level = FTDM_LOG_LEVEL_CRIT; break;
		case WAT_LOG_ERROR:	ftdm_level = FTDM_LOG_LEVEL_ERROR; break;
		case WAT_LOG_WARNING:   ftdm_level = FTDM_LOG_LEVEL_WARNING; break;
		case WAT_LOG_INFO:	ftdm_level = FTDM_LOG_LEVEL_INFO; break;
		case WAT_LOG_NOTICE:	ftdm_level = FTDM_LOG_LEVEL_NOTICE; break;
		case WAT_LOG_DEBUG:	ftdm_level = FTDM_LOG_LEVEL_DEBUG; break;
		default:		ftdm_level = FTDM_LOG_LEVEL_ERROR; break;
	};
	
	vsprintf(buff, fmt, argptr);

	ftdm_log_chan_ex(gsm_data->bchan, __FILE__, __FTDM_FUNC__, __LINE__, ftdm_level, "%s", buff);

	va_end(argptr);
}


/* END wat callbacks */

/* span monitor thread */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(gsm_outgoing_call)
{

	return FTDM_SUCCESS;
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
	if (wat_span_stop(span->span_id)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to stop span %s!\n", span->name);
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_gsm_destroy(ftdm_span_t *span)
{
	ftdm_gsm_span_data_t *gsm_data = span->signal_data;
	ftdm_assert_return(gsm_data != NULL, FTDM_FAIL, "Span does not have GSM data!\n");
	if (gsm_data->sched) {
		ftdm_sched_destroy(&gsm_data->sched);
	}
	ftdm_free(gsm_data);
	return FTDM_SUCCESS;
}

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(ftdm_gsm_get_channel_sig_status)
{
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
		*status = FTDM_SIG_STATE_UP;
	}
	else {
		*status = FTDM_SIG_STATE_DOWN;
	}
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;

}

static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(ftdm_gsm_set_channel_sig_status)
{
	return FTDM_SUCCESS;
}

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(ftdm_gsm_get_span_sig_status)
{
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}

static FIO_SPAN_SET_SIG_STATUS_FUNCTION(ftdm_gsm_set_span_sig_status)
{
	return FTDM_SUCCESS;
}

static ftdm_state_map_t gsm_state_map = {
	{
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_ANY, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_RESET, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESET, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
		},
		
		/* Outbound states */
		
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_ANY, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_RESET, FTDM_CHANNEL_STATE_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESET, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_RINGING,  FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_END}
		},



		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},

		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END}
		},
	}
};

static ftdm_status_t ftdm_gsm_state_advance(ftdm_channel_t *ftdmchan)
{

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Executing state handler for %s\n", ftdm_channel_state2str(ftdmchan->state));
	
	ftdm_channel_complete_state(ftdmchan);

		switch (ftdmchan->state) {

		/* starting an incoming call */
		case FTDM_CHANNEL_STATE_COLLECT: 
			{
				
				
			}
			break;

			/* starting an outgoing call */
		case FTDM_CHANNEL_STATE_DIALING:
			{
				uint32_t interval = 0;
				
				ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Starting outgoing call with interval %d\n", interval);

				{

					ftdm_gsm_span_data_t *gsm_data;
					wat_con_event_t con_event;
					gsm_data = ftdmchan->span->signal_data;
					gsm_data->call_id = GSM_OUTBOUND_CALL_ID;
					memset(&con_event, 0, sizeof(con_event));
					ftdm_set_string(con_event.called_num.digits, ftdmchan->caller_data.dnis.digits);
					ftdm_log(FTDM_LOG_DEBUG, "Dialing number %s\n", con_event.called_num.digits);
					wat_con_req(ftdmchan->span->span_id, gsm_data->call_id , &con_event);
 
					SEND_STATE_SIGNAL(FTDM_SIGEVENT_DIALING);
					
					
				}

				
			}
			break;

			/* incoming call was offered */
		case FTDM_CHANNEL_STATE_RING:

			/* notify the user about the new call */

			ftdm_log(FTDM_LOG_INFO, "Answering Incomming Call\r\n");
			SEND_STATE_SIGNAL(FTDM_SIGEVENT_START);
			
			break;

			/* the call is making progress */
		case FTDM_CHANNEL_STATE_PROGRESS:
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			{
				SEND_STATE_SIGNAL(FTDM_SIGEVENT_PROGRESS_MEDIA);
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_UP);
			}
			break;

			/* the call was answered */
		case FTDM_CHANNEL_STATE_UP:
			{
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				
					SEND_STATE_SIGNAL(FTDM_SIGEVENT_UP);
				}
				else {
						ftdm_gsm_span_data_t *gsm_data;
						gsm_data = ftdmchan->span->signal_data;
						wat_con_cfm(ftdmchan->span->span_id, gsm_data->call_id); 
				}
				
				
			}
			break;

			/* just got hangup */
		case FTDM_CHANNEL_STATE_HANGUP:
			{
				ftdm_gsm_span_data_t *gsm_data;
				gsm_data = ftdmchan->span->signal_data;
				wat_rel_req(ftdmchan->span->span_id, gsm_data->call_id);
				gsm_data->call_id = 0;
				SEND_STATE_SIGNAL(FTDM_SIGEVENT_STOP);
			}
			break;

		case FTDM_CHANNEL_STATE_TERMINATING:
			{
				SEND_STATE_SIGNAL(FTDM_SIGEVENT_STOP);
			}
			break;

			/* finished call for good */
		case FTDM_CHANNEL_STATE_DOWN: 
			{
				ftdm_channel_t *closed_chan;
				closed_chan = ftdmchan;
				ftdm_channel_close(&closed_chan);
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "State processing ended.\n");
				SEND_STATE_SIGNAL(FTDM_SIGEVENT_STOP);
			}
			break;

			/* INDICATE_RINGING doesn't apply to MFC/R2. maybe we could generate a tone */
		case FTDM_CHANNEL_STATE_RINGING: 
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "RINGING indicated, ignoring it as it doesn't apply to MFC/R2\n");
			SEND_STATE_SIGNAL(FTDM_SIGEVENT_RINGING);
				
			break;

			/* put the r2 channel back to IDLE, close ftdmchan and set it's state as DOWN */
		case FTDM_CHANNEL_STATE_RESET:
			{
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "RESET indicated, putting the R2 channel back to IDLE\n");
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			}
			break;

		default:
			{
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Unhandled channel state change: %s\n", ftdm_channel_state2str(ftdmchan->state));
			}
			break;
	}

	
	return FTDM_SUCCESS;
}



static ftdm_status_t init_wat_lib(void)
{
	wat_interface_t wat_interface;

	if (wat_lib_initialized) {
		return FTDM_SUCCESS;
	}

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
	wat_interface.wat_dtmf_ind = on_wat_dtmf_ind;
	
	if (wat_register(&wat_interface)) {
		ftdm_log(FTDM_LOG_DEBUG, "Failed registering interface to WAT library ...\n");
		return FTDM_FAIL;
	}

	ftdm_log(FTDM_LOG_DEBUG, "Registered interface to WAT library\n");

	wat_lib_initialized = 1;
	return FTDM_SUCCESS;
}

WAT_AT_CMD_RESPONSE_FUNC(on_dtmf_sent)
{
	ftdm_channel_t *ftdmchan = obj;
	ftdm_span_t *span = ftdmchan->span;
	int i = 0;

	if (success == WAT_TRUE) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "DTMF successfully transmitted on span %s\n", span->name);
	} else {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Command execution failed on span %s. Err: %s\n", span->name, error);
	}

	for (i = 0; tokens[i]; i++) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "%s\n", tokens[i]);
	}
	return i;
}

static ftdm_status_t ftdm_gsm_send_dtmf(ftdm_channel_t *ftdmchan, const char* dtmf)
{
	ftdm_gsm_span_data_t *gsm_data = ftdmchan->span->signal_data;
	wat_send_dtmf(ftdmchan->span->span_id, gsm_data->call_id, dtmf, on_dtmf_sent, ftdmchan);
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
	ftdm_channel_t *bchan = NULL;
	ftdm_bool_t hwdtmf_detect = FTDM_FALSE;
	ftdm_bool_t hwdtmf_generate = FTDM_FALSE;

	unsigned paramindex = 0;
	const char *var = NULL;
	const char *val = NULL;
	char schedname[255];

	int codec = FTDM_CODEC_SLIN;
	int interval = 20;

	/* libwat is smart enough to set good default values for the timers if they are set to 0 */
	memset(&span_config, 0, sizeof(span_config));

	/* set some span defaults */
	span_config.moduletype = WAT_MODULE_TELIT;
	span_config.hardware_dtmf = WAT_FALSE;

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
		
		if ((NULL == dchan) && FTDM_IS_DCHAN(ftdmchan)) {
			dchan = ftdmchan;
		}
		if ((NULL == bchan) && FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
			bchan = ftdmchan;
		}

	}
	ftdm_iterator_free(chaniter);

	if (!dchan) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find a d-channel for GSM span %s!\n", span->name);
		return FTDM_FAIL;
	}
	if (!bchan) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find a b-channel for GSM span %s!\n", span->name);
		return FTDM_FAIL;
	}

	gsm_data = ftdm_calloc(1, sizeof(*gsm_data));
	if (!gsm_data) {
		return FTDM_FAIL;
	}
	gsm_data->dchan = dchan;
	gsm_data->bchan = bchan;

	for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) {
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		if (ftdm_strlen_zero_buf(val)) {
			ftdm_log(FTDM_LOG_WARNING, "Ignoring empty GSM parameter %s for span %s\n", var, span->name);
			continue;
		}
		ftdm_log(FTDM_LOG_DEBUG, "Reading GSM parameter %s=%s for span %s\n", var, val, span->name);
		if (!strcasecmp(var, "moduletype")) {
			span_config.moduletype = wat_str2wat_moduletype(val);
			if (span_config.moduletype == WAT_MODULE_INVALID) {
				ftdm_log(FTDM_LOG_ERROR, "Unknown GSM module type %s for span %s\n", val, span->name);
				continue;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Configuring GSM span %s with moduletype %s\n", span->name, val);
		} else if (!strcasecmp(var, "debug")) {
			span_config.debug_mask = wat_str2debug(val);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring GSM span %s with debug mask %s == 0x%X\n", span->name, val, span_config.debug_mask);
		} else if (!strcasecmp(var, "hwdtmf")) {
			hwdtmf_detect = FTDM_FALSE;
			hwdtmf_generate = FTDM_FALSE;
			if (!strcasecmp(val, "generate")) {
				hwdtmf_generate = FTDM_TRUE;
			} else if (!strcasecmp(val, "detect")) {
				hwdtmf_detect = FTDM_TRUE;
			} else if (!strcasecmp(val, "both") || ftdm_true(val)) {
				hwdtmf_detect = FTDM_TRUE;
				hwdtmf_generate = FTDM_TRUE;
			} else {
				span_config.hardware_dtmf = WAT_FALSE;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Configuring GSM span %s with hardware dtmf %s\n", span->name, val);
		} else if (!strcasecmp(var, "conditional-forwarding-number")) {
			ftdm_set_string(gsm_data->conditional_forward_number, val);
			gsm_data->init_forwarding = FTDM_TRUE;
		} else {
			ftdm_log(FTDM_LOG_ERROR, "Ignoring unknown GSM parameter '%s'", var);
		}
	}

	/* Bind function pointers for control operations */
	span->start = ftdm_gsm_start;
	span->stop = ftdm_gsm_stop;
	span->destroy = ftdm_gsm_destroy;
	span->sig_read = NULL;
	span->sig_write = NULL;
	if (hwdtmf_detect || hwdtmf_generate) {
		span_config.hardware_dtmf = WAT_TRUE;
		if (hwdtmf_generate) {
			span->sig_send_dtmf = ftdm_gsm_send_dtmf;
		}
		if (hwdtmf_detect) {
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_SIG_DTMF_DETECTION);
		}
	}
	span->signal_cb = sig_cb;
	span->signal_type = FTDM_SIGTYPE_GSM;
	span->signal_data = gsm_data;
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

	/* Setup the scheduler */
	snprintf(schedname, sizeof(schedname), "ftmod_gsm_%s", span->name);
	if (ftdm_sched_create(&gsm_data->sched, schedname) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to setup scheduler for span %s!\n", span->name);
		ftdm_gsm_destroy(span);
		return FTDM_FAIL;
	}

	/* Start the signaling stack */
	if (wat_span_config(span->span_id, &span_config)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to configure span %s for GSM signaling!!\n", span->name);
		ftdm_gsm_destroy(span);
		return FTDM_FAIL;
	}

	ftdm_channel_command(gsm_data->bchan, FTDM_COMMAND_SET_NATIVE_CODEC,  &codec);
	ftdm_channel_command(gsm_data->bchan, FTDM_COMMAND_SET_CODEC,  &codec);
	ftdm_channel_command(gsm_data->bchan, FTDM_COMMAND_SET_INTERVAL,  &interval);

	return FTDM_SUCCESS;

}

#define GSM_POLL_INTERVAL_MS 20
static void *ftdm_gsm_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	ftdm_interrupt_t *data_sources[2] = {NULL, NULL};
	ftdm_wait_flag_t flags = FTDM_READ | FTDM_EVENTS;
	ftdm_status_t status = FTDM_SUCCESS;
	char buffer[1025] = { 0 };
	int waitms = 0;
	ftdm_wait_flag_t flags = 0;
	ftdm_status_t status;
	
	gsm_data = span->signal_data;
	ftdm_assert_return(gsm_data != NULL, NULL, "No gsm data attached to span\n");

	ftdm_log(FTDM_LOG_DEBUG, "GSM monitor thread for span %s started\n", span->name);
	if (!gsm_data->dchan || ftdm_channel_open_chan(gsm_data->dchan) != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to open GSM d-channel of span %s!\n", span->name);
		gsm_data->dchan = NULL;
		goto done;
	}

	while (ftdm_running()) {
		wat_span_run(span->span_id);
		ftdm_sched_run(gsm_data->sched);

		waitms = wat_span_schedule_next(span->span_id);
		if (waitms > GSM_POLL_INTERVAL_MS) {
			waitms = GSM_POLL_INTERVAL_MS;
		}

		flags = FTDM_READ | FTDM_EVENTS;
		status = ftdm_channel_wait(gsm_data->dchan, &flags, waitms);
		
		/* check if this channel has a state change pending and process it if needed */
		ftdm_channel_lock(gsm_data->bchan);
		ftdm_channel_advance_states(gsm_data->bchan);
		if (FTDM_SUCCESS == status && (flags & FTDM_READ)) {
			int n = 0;
			n = read_channel(gsm_data->dchan, buffer, sizeof(buffer) - 1);
			if (n > 0) {
				wat_span_process_read(span->span_id, buffer, n);
				buffer[0] = 0;
			}
		}
		ftdm_channel_advance_states(gsm_data->bchan);
		ftdm_channel_unlock(gsm_data->bchan);

		ftdm_span_trigger_signals(span);
	}

done:
	if (data_sources[0]) {
		ftdm_interrupt_destroy(&data_sources[0]);
	}

	ftdm_log(FTDM_LOG_DEBUG, "GSM thread ending.\n");

	return NULL;
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

EX_DECLARE_DATA ftdm_module_t ftdm_module = { 
	/* .name */ "gsm",
	/* .io_load */ ftdm_gsm_io_init,
	/* .io_unload */ NULL,
	/* .sig_load */ NULL,
	/* .sig_configure */ NULL,
	/* .sig_unload */ NULL,
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */


/********************************************************************************/
/*                                                                              */
/*                             COMMAND HANDLERS                                 */
/*                                                                              */
/********************************************************************************/


// Version Command Handler
COMMAND_HANDLER(version)
{
	uint8_t current = 0, revision = 0, age = 0;
	wat_version(&current, &revision, &age);
	stream->write_function(stream, "libwat version: %d.%d.%d\n", current, revision, age);
	stream->write_function(stream, "+OK.\n");
	return FTDM_SUCCESS;
}


// Status Command Handler
COMMAND_HANDLER(status)
{
	int span_id = 0;
	ftdm_span_t *span = NULL;
	const wat_chip_info_t *chip_info = NULL;
	const wat_sim_info_t *sim_info = NULL;
	const wat_net_info_t *net_info = NULL;
	const wat_sig_info_t *sig_info = NULL;
	wat_pin_stat_t pin_stat = 0;

	span_id = atoi(argv[0]);
	if (ftdm_span_find_by_name(argv[0], &span) != FTDM_SUCCESS && ftdm_span_find(span_id, &span) != FTDM_SUCCESS) {
		stream->write_function(stream, "-ERR Failed to find GSM span '%s'\n",  argv[1]);
		return FTDM_FAIL;
	}

	if (!span || !span->signal_data || (span->start != ftdm_gsm_start)) {
		stream->write_function(stream, "-ERR '%s' is not a valid GSM span\n",  argv[1]);
		return FTDM_FAIL;
	}

	chip_info = wat_span_get_chip_info(span->span_id);	
	sim_info = wat_span_get_sim_info(span->span_id);
	net_info = wat_span_get_net_info(span->span_id);
	sig_info = wat_span_get_sig_info(span->span_id);

	/* This is absolutely retarded and should be fixed in libwat
	 * why the hell would you return a pointer to an internal state enum instead of a copy?
	 * probably the same applies to the rest of the info (sim_info, chip_info, net_info, etc),
	 * but at least there you could have the lame excuse that you don't need to copy the whole struct */
	pin_stat = *wat_span_get_pin_info(span->span_id);
	
	stream->write_function(stream, "Span %d (%s):\n",  span->span_id, span->name);

	stream->write_function(stream, "CHIP type - %s (%s), revision %s, serial  %s \n", 
			chip_info->manufacturer,
			chip_info->model,
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

		
	stream->write_function(stream, "Sig Info: rssi(%d) ber(%d)\n", sig_info->rssi, sig_info->ber);

	stream->write_function(stream, "PIN Status: %s\n", wat_pin_stat2str(pin_stat));

	stream->write_function(stream, "\n");
	
	stream->write_function(stream, "+OK.\n");
	
	return FTDM_SUCCESS;
}

// SMS Command Handler
COMMAND_HANDLER(sms)
{
	int span_id = 0, i;
	uint32_t sms_id = 0;
	ftdm_span_t *span = NULL;
	wat_sms_event_t sms;
	ftdm_gsm_span_data_t *gsm_data = NULL;
	
	span_id = atoi(argv[0]);
	if (ftdm_span_find_by_name(argv[0], &span) != FTDM_SUCCESS && ftdm_span_find(span_id, &span) != FTDM_SUCCESS) {
		stream->write_function(stream, "-ERR Failed to find GSM span '%s'\n",  argv[1]);
		return FTDM_FAIL;
	}

	if (!span || !span->signal_data || (span->start != ftdm_gsm_start)) {
		stream->write_function(stream, "-ERR '%s' is not a valid GSM span\n",  argv[1]);
		return FTDM_FAIL;
	}
	gsm_data = span->signal_data;

	memset(&sms, 0, sizeof(sms));
	strcpy(sms.to.digits, argv[1]);
	sms.type = WAT_SMS_TXT;
	sms.content.data[0] = '\0';
	for(i=2;i<argc;i++) {
		strcat(sms.content.data, argv[i]);
		strcat(sms.content.data, " ");
	}
	sms.content.len = strlen(sms.content.data);
	
	ftdm_channel_lock(gsm_data->bchan);

	sms_id = gsm_data->sms_id >= WAT_MAX_SMSS_PER_SPAN ? 0 : gsm_data->sms_id;
	gsm_data->sms_id++;

	ftdm_channel_unlock(gsm_data->bchan);

	if (WAT_SUCCESS != wat_sms_req(span->span_id, sms_id, &sms)) {
		stream->write_function(stream, "Failed to Send SMS \n");
	} else {
		stream->write_function(stream, "SMS Sent.\n");
	}
	return FTDM_SUCCESS;
}

WAT_AT_CMD_RESPONSE_FUNC(gsm_exec_cb)
{
	ftdm_gsm_exec_helper_t *helper = (ftdm_gsm_exec_helper_t *)obj;
	ftdm_stream_handle_t *stream = helper->stream;
	ftdm_span_t *span = helper->span;
	int i = 0;

	if (success == WAT_TRUE) {
		stream->write_function(stream, "Command executed successfully on span %s\n", span->name);
	} else {
		stream->write_function(stream, "Command execution failed on span %s. Err: %s\n", span->name, error);
	}

	for (i = 0; tokens[i]; i++) {
		stream->write_function(stream, "%s\n", tokens[i]);
	}

	helper->cmd_pending = 0;
	return i;
}

// AT Command Handler
COMMAND_HANDLER(exec)
{
	int span_id = 0;
	int sanity = 100;
	ftdm_span_t *span = NULL;
	ftdm_gsm_exec_helper_t helper;

	span_id = atoi(argv[0]);
	if (ftdm_span_find_by_name(argv[0], &span) != FTDM_SUCCESS && ftdm_span_find(span_id, &span) != FTDM_SUCCESS) {
		stream->write_function(stream, "-ERR Failed to find GSM span '%s'\n",  argv[0]);
		return FTDM_FAIL;
	}

	if (!span || !span->signal_data || (span->start != ftdm_gsm_start)) {
		stream->write_function(stream, "-ERR '%s' is not a valid GSM span\n",  argv[0]);
		return FTDM_FAIL;
	}

	helper.stream = stream;
	helper.span = span;
	helper.cmd_pending = 1;
	if (WAT_SUCCESS != wat_cmd_req(span->span_id, argv[1], gsm_exec_cb, &helper)) {
		stream->write_function(stream, "Failed to send AT command on span %s\n", span->name);
	} else {
		stream->write_function(stream, "AT command sent on span %s\n", span->name);
	}

	while (helper.cmd_pending && (--sanity > 0)) {
		ftdm_sleep(100);
	}

	if (sanity < 0) {
		stream->write_function(stream, "Timed out waiting for respons for AT command on span %s\n", span->name);
	}
	return FTDM_SUCCESS;
}

// AT Command Handler
COMMAND_HANDLER(call)
{
	int span_id = 0;
	ftdm_span_t *span = NULL;

	span_id = atoi(argv[0]);
	if (ftdm_span_find_by_name(argv[0], &span) != FTDM_SUCCESS && ftdm_span_find(span_id, &span) != FTDM_SUCCESS) {
		stream->write_function(stream, "-ERR Failed to find GSM span '%s'\n",  argv[0]);
		return FTDM_FAIL;
	}

	if (!span || !span->signal_data || (span->start != ftdm_gsm_start)) {
		stream->write_function(stream, "-ERR '%s' is not a valid GSM span\n",  argv[0]);
		return FTDM_FAIL;
	}

	ftdm_gsm_make_raw_call(span->signal_data, argv[1]);
	stream->write_function(stream, "+OK\n");
	return FTDM_SUCCESS;
}

//  command map
struct {
	const char*CMD; // command
	int  Argc;      // minimum args
	fCMD Handler;   // handling function
}
	GSM_COMMANDS[] = {
		COMMAND(version, 0),
		COMMAND(status, 1),
		COMMAND(sms, 3),
		COMMAND(exec, 2),
		COMMAND(call, 2),
	};

// Commnand API entry point
static FIO_API_FUNCTION(ftdm_gsm_api)
{
	
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	int i;
	ftdm_status_t status = FTDM_FAIL;
	ftdm_status_t syntax = FTDM_FAIL;


	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if(argc > 0) {
		for(i=0;i<sizeof(GSM_COMMANDS)/sizeof(GSM_COMMANDS[0]);i++) {
			if(strcasecmp(argv[0],GSM_COMMANDS[i].CMD)==0) {
				
				if(argc -1 >= GSM_COMMANDS[i].Argc) {
					syntax = FTDM_SUCCESS;
					status = GSM_COMMANDS[i].Handler(stream, &argv[1], argc-1);
					
				}
				
				break;
			}

		}
	}

	if(FTDM_SUCCESS != syntax) {
		stream->write_function(stream, "%s", FT_SYNTAX);
	}
	else
	if(FTDM_SUCCESS != status) {
		stream->write_function(stream, "%s Command Failed\r\n", GSM_COMMANDS[i].CMD);
	}
	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;
}


