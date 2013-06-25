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
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 * David Yat Sin <dyatsin@sangoma.com>
 *
 */
#define _GNU_SOURCE
#include "private/ftdm_core.h"
#include <stdarg.h>
#include <ctype.h>
#ifdef WIN32
#include <io.h>
#endif
#ifdef FTDM_PIKA_SUPPORT
#include "ftdm_pika.h"
#endif
#include "ftdm_cpu_monitor.h"

#ifndef localtime_r
struct tm *localtime_r(const time_t *clock, struct tm *result);
#endif

#define FORCE_HANGUP_TIMER 30000
#define FTDM_READ_TRACE_INDEX 0
#define FTDM_WRITE_TRACE_INDEX 1
#define MAX_CALLIDS 6000
#define FTDM_HALF_DTMF_PAUSE 500
#define FTDM_FULL_DTMF_PAUSE 1000

ftdm_time_t time_last_throttle_log = 0;
ftdm_time_t time_current_throttle_log = 0;

typedef struct val_str {
	const char *str;
	unsigned long long val;
} val_str_t;

static val_str_t channel_flag_strs[] =  {
	{ "configured" ,  FTDM_CHANNEL_CONFIGURED},
	{ "ready",  FTDM_CHANNEL_READY},
	{ "open",  FTDM_CHANNEL_OPEN},
	{ "dtmf-detect",  FTDM_CHANNEL_DTMF_DETECT},
	{ "suppress-dtmf",  FTDM_CHANNEL_SUPRESS_DTMF},
	{ "transcode",  FTDM_CHANNEL_TRANSCODE},
	{ "buffer",  FTDM_CHANNEL_BUFFER},
	{ "in-thread",  FTDM_CHANNEL_INTHREAD},
	{ "wink",  FTDM_CHANNEL_WINK},
	{ "flash",  FTDM_CHANNEL_FLASH},
	{ "state-change",  FTDM_CHANNEL_STATE_CHANGE},
	{ "hold",  FTDM_CHANNEL_HOLD},
	{ "in-use",  FTDM_CHANNEL_INUSE},
	{ "off-hook",  FTDM_CHANNEL_OFFHOOK},
	{ "ringing",  FTDM_CHANNEL_RINGING},
	{ "progress-detect",  FTDM_CHANNEL_PROGRESS_DETECT},
	{ "callerid-detect",  FTDM_CHANNEL_CALLERID_DETECT},
	{ "outbound",  FTDM_CHANNEL_OUTBOUND},
	{ "suspended",  FTDM_CHANNEL_SUSPENDED},
	{ "3-way",  FTDM_CHANNEL_3WAY},
	{ "progress",  FTDM_CHANNEL_PROGRESS},
	{ "media",  FTDM_CHANNEL_MEDIA},
	{ "answered",  FTDM_CHANNEL_ANSWERED},
	{ "mute",  FTDM_CHANNEL_MUTE},
	{ "use-rx-gain",  FTDM_CHANNEL_USE_RX_GAIN},
	{ "use-tx-gain",  FTDM_CHANNEL_USE_TX_GAIN},
	{ "in-alarm",  FTDM_CHANNEL_IN_ALARM},
	{ "sig-up",  FTDM_CHANNEL_SIG_UP},
	{ "user-hangup",  FTDM_CHANNEL_USER_HANGUP},
	{ "rx-disabled",  FTDM_CHANNEL_RX_DISABLED},
	{ "tx-disabled",  FTDM_CHANNEL_TX_DISABLED},
	{ "call-started",  FTDM_CHANNEL_CALL_STARTED},
	{ "non-block",  FTDM_CHANNEL_NONBLOCK},
	{ "ind-ack-pending",  FTDM_CHANNEL_IND_ACK_PENDING},
	{ "blocking",  FTDM_CHANNEL_BLOCKING},
	{ "media",  FTDM_CHANNEL_DIGITAL_MEDIA},
	{ "native-sigbridge",  FTDM_CHANNEL_NATIVE_SIGBRIDGE},
	{ "invalid",  FTDM_CHANNEL_MAX_FLAG},
};

static val_str_t span_flag_strs[] =  {
	{ "configured", FTDM_SPAN_CONFIGURED},
	{ "started", FTDM_SPAN_STARTED},
	{ "state-change", FTDM_SPAN_STATE_CHANGE},
	{ "suspended", FTDM_SPAN_SUSPENDED},
	{ "in-thread", FTDM_SPAN_IN_THREAD},
	{ "stop-thread", FTDM_SPAN_STOP_THREAD},
	{ "use-chan-queue", FTDM_SPAN_USE_CHAN_QUEUE},
	{ "suggest-chan-id", FTDM_SPAN_SUGGEST_CHAN_ID},
	{ "use-av-rate", FTDM_SPAN_USE_AV_RATE},
	{ "power-saving", FTDM_SPAN_PWR_SAVING},
	{ "signals-queue", FTDM_SPAN_USE_SIGNALS_QUEUE},
	{ "proceed-state", FTDM_SPAN_USE_PROCEED_STATE},
	{ "skip-state", FTDM_SPAN_USE_SKIP_STATES},
	{ "non-stoppable", FTDM_SPAN_NON_STOPPABLE},
	{ "use-transfer", FTDM_SPAN_USE_TRANSFER},
};

static ftdm_status_t ftdm_call_set_call_id(ftdm_channel_t *fchan, ftdm_caller_data_t *caller_data);
static ftdm_status_t ftdm_call_clear_call_id(ftdm_caller_data_t *caller_data);
static ftdm_status_t ftdm_channel_done(ftdm_channel_t *ftdmchan);
static ftdm_status_t ftdm_channel_sig_indicate(ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication, ftdm_usrmsg_t *usrmsg);

static const char *ftdm_val2str(unsigned long long val, val_str_t *val_str_table, ftdm_size_t array_size, const char *default_str);
static unsigned long long ftdm_str2val(const char *str, val_str_t *val_str_table, ftdm_size_t array_size, unsigned long long default_val);


static int time_is_init = 0;

static void time_init(void)
{
#ifdef WIN32
	timeBeginPeriod(1);
#endif
	time_is_init = 1;
}

static void time_end(void)
{
#ifdef WIN32
	timeEndPeriod(1);
#endif
	time_is_init = 0;
}

FT_DECLARE(ftdm_time_t) ftdm_current_time_in_ms(void)
{
#ifdef WIN32
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

static void write_chan_io_dump(ftdm_io_dump_t *dump, char *dataptr, int dlen)
{
	int windex = dump->windex;
	int avail = (int)dump->size - windex;

	if (!dump->buffer) {
		return;
	}

	if (dlen > avail) {
		int diff = dlen - avail;
		
		ftdm_assert(diff < (int)dump->size, "Very small buffer or very big IO chunk!\n");

		/* write only what we can and the rest at the beginning of the buffer */
		memcpy(&dump->buffer[windex], dataptr, avail);
		memcpy(&dump->buffer[0], &dataptr[avail], diff);
		windex = diff;

		/*ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "wrapping around dump buffer %p up to index %d\n\n", dump, windex);*/
		dump->wrapped = 1;
	} else {
		memcpy(&dump->buffer[windex], dataptr, dlen);
		windex += dlen;
	}

	if (windex == (int)dump->size) {
		/*ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "wrapping around dump buffer %p\n", dump);*/
		windex = 0;
		dump->wrapped = 1;
	}

	dump->windex = windex;
}

static void dump_chan_io_to_file(ftdm_channel_t *fchan, ftdm_io_dump_t *dump, FILE *file)
{
	/* write the saved audio buffer */
	ftdm_size_t rc = 0;
	ftdm_size_t towrite = 0;

	if (!dump->buffer) {
		return;
	}

	towrite = dump->size - dump->windex;

	if (dump->wrapped) {
		rc = fwrite(&dump->buffer[dump->windex], 1, towrite, file);
		if (rc != towrite) {
			ftdm_log_chan(fchan, FTDM_LOG_ERROR, "only wrote %"FTDM_SIZE_FMT" out of %"FTDM_SIZE_FMT" bytes in io dump buffer: %s\n",
					rc, towrite, strerror(errno));
		}
	}
	if (dump->windex) {
		towrite = dump->windex;
		rc = fwrite(&dump->buffer[0], 1, towrite, file);
		if (rc != towrite) {
			ftdm_log_chan(fchan, FTDM_LOG_ERROR, "only wrote %"FTDM_SIZE_FMT" out of %"FTDM_SIZE_FMT" bytes in io dump buffer: %s\n",
					rc, towrite, strerror(errno));
		}
	}
	dump->windex = 0;
	dump->wrapped = 0;
}

static void stop_chan_io_dump(ftdm_io_dump_t *dump)
{
	if (!dump->buffer) {
		return;
	}
	ftdm_safe_free(dump->buffer);
	memset(dump, 0, sizeof(*dump));
}

static ftdm_status_t start_chan_io_dump(ftdm_channel_t *chan, ftdm_io_dump_t *dump, ftdm_size_t size)
{
	if (dump->buffer) {
		ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "IO dump is already started\n");
		return FTDM_FAIL;
	}
	memset(dump, 0, sizeof(*dump));
	dump->buffer = ftdm_malloc(size);
	if (!dump->buffer) {
		return FTDM_FAIL;
	}
	dump->size = size;
	return FTDM_SUCCESS;
}


static void close_dtmf_debug_file(ftdm_channel_t *ftdmchan)
{
	if (ftdmchan->dtmfdbg.file) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "closing debug dtmf file\n");
		fclose(ftdmchan->dtmfdbg.file);
		ftdmchan->dtmfdbg.file = NULL;
	}
}

static ftdm_status_t disable_dtmf_debug(ftdm_channel_t *ftdmchan)
{
	if (!ftdmchan->dtmfdbg.enabled) {
		return FTDM_SUCCESS;
	}

	if (!ftdmchan->rxdump.buffer) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "DTMF debug enabled but no rx dump?\n");	
		return FTDM_FAIL;
	}

	close_dtmf_debug_file(ftdmchan);
	stop_chan_io_dump(&ftdmchan->rxdump);
	ftdmchan->dtmfdbg.enabled = 0;
	return FTDM_SUCCESS;
}

typedef struct {
	uint8_t 	enabled;
	uint8_t         running;
	uint8_t         alarm;
	uint32_t        interval;
	uint8_t         alarm_action_flags;
	uint8_t         set_alarm_threshold;
	uint8_t         clear_alarm_threshold;
	ftdm_interrupt_t *interrupt;
} cpu_monitor_t;

static struct {
	ftdm_hash_t *interface_hash;
	ftdm_hash_t *module_hash;
	ftdm_hash_t *span_hash;
	ftdm_hash_t *group_hash;
	ftdm_mutex_t *mutex;
	ftdm_mutex_t *span_mutex;
	ftdm_mutex_t *group_mutex;
	ftdm_sched_t *timingsched;
	uint32_t span_index;
	uint32_t group_index;
	uint32_t running;
	ftdm_span_t *spans;
	ftdm_group_t *groups;
	cpu_monitor_t cpu_monitor;
	
	ftdm_caller_data_t *call_ids[MAX_CALLIDS+1];
	ftdm_mutex_t *call_id_mutex;
	uint32_t last_call_id;
	char dtmfdebug_directory[1024];
} globals;

enum ftdm_enum_cpu_alarm_action_flags
{
	FTDM_CPU_ALARM_ACTION_WARN   = (1 << 0),
	FTDM_CPU_ALARM_ACTION_REJECT = (1 << 1)
};

/* enum lookup funcs */
FTDM_ENUM_NAMES(TONEMAP_NAMES, TONEMAP_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_tonemap, ftdm_tonemap2str, ftdm_tonemap_t, TONEMAP_NAMES, FTDM_TONEMAP_INVALID)

FTDM_ENUM_NAMES(OOB_NAMES, OOB_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_oob_event, ftdm_oob_event2str, ftdm_oob_event_t, OOB_NAMES, FTDM_OOB_INVALID)

FTDM_ENUM_NAMES(TRUNK_TYPE_NAMES, TRUNK_TYPE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_trunk_type, ftdm_trunk_type2str, ftdm_trunk_type_t, TRUNK_TYPE_NAMES, FTDM_TRUNK_NONE)

FTDM_ENUM_NAMES(TRUNK_MODE_NAMES, TRUNK_MODE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_trunk_mode, ftdm_trunk_mode2str, ftdm_trunk_mode_t, TRUNK_MODE_NAMES, FTDM_TRUNK_MODE_INVALID)

FTDM_ENUM_NAMES(START_TYPE_NAMES, START_TYPE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_analog_start_type, ftdm_analog_start_type2str, ftdm_analog_start_type_t, START_TYPE_NAMES, FTDM_ANALOG_START_NA)

FTDM_ENUM_NAMES(SIGNAL_NAMES, SIGNAL_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_signal_event, ftdm_signal_event2str, ftdm_signal_event_t, SIGNAL_NAMES, FTDM_SIGEVENT_INVALID)

FTDM_ENUM_NAMES(MDMF_TYPE_NAMES, MDMF_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_mdmf_type, ftdm_mdmf_type2str, ftdm_mdmf_type_t, MDMF_TYPE_NAMES, MDMF_INVALID)

FTDM_ENUM_NAMES(CHAN_TYPE_NAMES, CHAN_TYPE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_chan_type, ftdm_chan_type2str, ftdm_chan_type_t, CHAN_TYPE_NAMES, FTDM_CHAN_TYPE_COUNT)

FTDM_ENUM_NAMES(SIGNALING_STATUS_NAMES, SIGSTATUS_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_signaling_status, ftdm_signaling_status2str, ftdm_signaling_status_t, SIGNALING_STATUS_NAMES, FTDM_SIG_STATE_INVALID)

FTDM_ENUM_NAMES(TRACE_DIR_NAMES, TRACE_DIR_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_trace_dir, ftdm_trace_dir2str, ftdm_trace_dir_t, TRACE_DIR_NAMES, FTDM_TRACE_DIR_INVALID)

FTDM_ENUM_NAMES(TRACE_TYPE_NAMES, TRACE_TYPE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_trace_type, ftdm_trace_type2str, ftdm_trace_type_t, TRACE_TYPE_NAMES, FTDM_TRACE_TYPE_INVALID)

FTDM_ENUM_NAMES(TON_NAMES, TON_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_ton, ftdm_ton2str, ftdm_ton_t, TON_NAMES, FTDM_TON_INVALID)

FTDM_ENUM_NAMES(NPI_NAMES, NPI_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_npi, ftdm_npi2str, ftdm_npi_t, NPI_NAMES, FTDM_NPI_INVALID)

FTDM_ENUM_NAMES(PRESENTATION_NAMES, PRESENTATION_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_presentation, ftdm_presentation2str, ftdm_presentation_t, PRESENTATION_NAMES, FTDM_PRES_INVALID)

FTDM_ENUM_NAMES(SCREENING_NAMES, SCREENING_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_screening, ftdm_screening2str, ftdm_screening_t, SCREENING_NAMES, FTDM_SCREENING_INVALID)

FTDM_ENUM_NAMES(BEARER_CAP_NAMES, BEARER_CAP_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_bearer_cap, ftdm_bearer_cap2str, ftdm_bearer_cap_t, BEARER_CAP_NAMES, FTDM_BEARER_CAP_INVALID)

FTDM_ENUM_NAMES(USER_LAYER1_PROT_NAMES, USER_LAYER1_PROT_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_usr_layer1_prot, ftdm_user_layer1_prot2str, ftdm_user_layer1_prot_t, USER_LAYER1_PROT_NAMES, FTDM_USER_LAYER1_PROT_INVALID)

FTDM_ENUM_NAMES(CALLING_PARTY_CATEGORY_NAMES, CALLING_PARTY_CATEGORY_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_calling_party_category, ftdm_calling_party_category2str, ftdm_calling_party_category_t, CALLING_PARTY_CATEGORY_NAMES, FTDM_CPC_INVALID)

FTDM_ENUM_NAMES(INDICATION_NAMES, INDICATION_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_channel_indication, ftdm_channel_indication2str, ftdm_channel_indication_t, INDICATION_NAMES, FTDM_CHANNEL_INDICATE_INVALID)

FTDM_ENUM_NAMES(TRANSFER_RESPONSE_NAMES, TRANSFER_RESPONSE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_transfer_response, ftdm_transfer_response2str, ftdm_transfer_response_t, TRANSFER_RESPONSE_NAMES, FTDM_TRANSFER_RESPONSE_INVALID)

static ftdm_status_t ftdm_group_add_channels(ftdm_span_t* span, int currindex, const char* name);

static void null_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	ftdm_unused_arg(file);
	ftdm_unused_arg(func);
	ftdm_unused_arg(line);
	ftdm_unused_arg(level);
	ftdm_unused_arg(fmt);
}


const char *FTDM_LEVEL_NAMES[9] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	NULL
};

static int ftdm_log_level = FTDM_LOG_LEVEL_DEBUG;

static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	char data[1024];
	va_list ap;

	if (level < 0 || level > 7) {
		level = 7;
	}
	if (level > ftdm_log_level) {
		return;
	}

	va_start(ap, fmt);

	vsnprintf(data, sizeof(data), fmt, ap);

	fprintf(stderr, "[%s] %s:%d %s() %s", FTDM_LEVEL_NAMES[level], file, line, func, data);

	va_end(ap);

}

static __inline__ void *ftdm_std_malloc(void *pool, ftdm_size_t size)
{
	void *ptr = malloc(size);
	ftdm_unused_arg(pool);
	ftdm_assert_return(ptr != NULL, NULL, "Out of memory\n");
	return ptr;
}

static __inline__ void *ftdm_std_calloc(void *pool, ftdm_size_t elements, ftdm_size_t size)
{
	void *ptr = calloc(elements, size);
	ftdm_unused_arg(pool);
	ftdm_assert_return(ptr != NULL, NULL, "Out of memory\n");
	return ptr;
}

static __inline__ void *ftdm_std_realloc(void *pool, void *buff, ftdm_size_t size)
{
	buff = realloc(buff, size);
	ftdm_unused_arg(pool);
	ftdm_assert_return(buff != NULL, NULL, "Out of memory\n");
	return buff;
}

static __inline__ void ftdm_std_free(void *pool, void *ptr)
{
	ftdm_unused_arg(pool);
	ftdm_assert_return(ptr != NULL, , "Attempted to free null pointer");
	free(ptr);
}

FT_DECLARE(void) ftdm_set_echocancel_call_begin(ftdm_channel_t *chan)
{
	ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);
	if (ftdm_channel_test_feature(chan, FTDM_CHANNEL_FEATURE_HWEC)) {
		if (ftdm_channel_test_feature(chan, FTDM_CHANNEL_FEATURE_HWEC_DISABLED_ON_IDLE)) {
			/* If the ec is disabled on idle, we need to enable it unless is a digital call */
			if (caller_data->bearer_capability != FTDM_BEARER_CAP_UNRESTRICTED) {
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Enabling ec for call in channel state %s\n", ftdm_channel_state2str(chan->state));
				ftdm_channel_command(chan, FTDM_COMMAND_ENABLE_ECHOCANCEL, NULL);
			}
		} else {
			/* If the ec is enabled on idle, we do nothing unless is a digital call that needs it disabled */
			if (caller_data->bearer_capability == FTDM_BEARER_CAP_UNRESTRICTED) {
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Disabling ec for digital call in channel state %s\n", ftdm_channel_state2str(chan->state));
				ftdm_channel_command(chan, FTDM_COMMAND_DISABLE_ECHOCANCEL, NULL);
			}
		}
	}
}

FT_DECLARE(void) ftdm_set_echocancel_call_end(ftdm_channel_t *chan)
{
	if (ftdm_channel_test_feature(chan, FTDM_CHANNEL_FEATURE_HWEC)) {
		if (ftdm_channel_test_feature(chan, FTDM_CHANNEL_FEATURE_HWEC_DISABLED_ON_IDLE)) {
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Disabling ec on call end in channel state %s\n", ftdm_channel_state2str(chan->state));
			ftdm_channel_command(chan, FTDM_COMMAND_DISABLE_ECHOCANCEL, NULL);
		} else {
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Enabling ec back on call end in channel state %s\n", ftdm_channel_state2str(chan->state));
			ftdm_channel_command(chan, FTDM_COMMAND_ENABLE_ECHOCANCEL, NULL);
		}
	}
}

FT_DECLARE_DATA ftdm_memory_handler_t g_ftdm_mem_handler = 
{
	/*.pool =*/ NULL,
	/*.malloc =*/ ftdm_std_malloc,
	/*.calloc =*/ ftdm_std_calloc,
	/*.realloc =*/ ftdm_std_realloc,
	/*.free =*/ ftdm_std_free
};

FT_DECLARE_DATA ftdm_crash_policy_t g_ftdm_crash_policy = FTDM_CRASH_NEVER;

static ftdm_status_t ftdm_set_caller_data(ftdm_span_t *span, ftdm_caller_data_t *caller_data)
{
	if (!caller_data) {
		ftdm_log(FTDM_LOG_CRIT, "Error: trying to set caller data, but no caller_data!\n");
		return FTDM_FAIL;
	}

	if (caller_data->dnis.plan >= FTDM_NPI_INVALID) {
		caller_data->dnis.plan = span->default_caller_data.dnis.plan;
	}

	if (caller_data->dnis.type >= FTDM_TON_INVALID) {
		caller_data->dnis.type = span->default_caller_data.dnis.type;
	}

	if (caller_data->cid_num.plan >= FTDM_NPI_INVALID) {
		caller_data->cid_num.plan = span->default_caller_data.cid_num.plan;
	}

	if (caller_data->cid_num.type >= FTDM_TON_INVALID) {
		caller_data->cid_num.type = span->default_caller_data.cid_num.type;
	}

	if (caller_data->ani.plan >= FTDM_NPI_INVALID) {
		caller_data->ani.plan = span->default_caller_data.ani.plan;
	}

	if (caller_data->ani.type >= FTDM_TON_INVALID) {
		caller_data->ani.type = span->default_caller_data.ani.type;
	}

	if (caller_data->rdnis.plan >= FTDM_NPI_INVALID) {
		caller_data->rdnis.plan = span->default_caller_data.rdnis.plan;
	}

	if (caller_data->rdnis.type >= FTDM_NPI_INVALID) {
		caller_data->rdnis.type = span->default_caller_data.rdnis.type;
	}

	if (caller_data->bearer_capability >= FTDM_INVALID_INT_PARM) {
		caller_data->bearer_capability = span->default_caller_data.bearer_capability;
	}

	if (caller_data->bearer_layer1 >= FTDM_INVALID_INT_PARM) {
		caller_data->bearer_layer1 = span->default_caller_data.bearer_layer1;
	}

	if (FTDM_FAIL == ftdm_is_number(caller_data->cid_num.digits)) {
		ftdm_log(FTDM_LOG_DEBUG, "dropping caller id number %s since we only accept digits\n", caller_data->cid_num.digits);
		caller_data->cid_num.digits[0] = '\0';
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_set_caller_data(ftdm_channel_t *ftdmchan, ftdm_caller_data_t *caller_data)
{
	ftdm_status_t err = FTDM_SUCCESS;
	if (!ftdmchan) {
		ftdm_log(FTDM_LOG_CRIT, "trying to set caller data, but no ftdmchan!\n");
		return FTDM_FAIL;
	}
	if ((err = ftdm_set_caller_data(ftdmchan->span, caller_data)) != FTDM_SUCCESS) {
		return err; 
	}
	ftdmchan->caller_data = *caller_data;
	if (ftdmchan->caller_data.bearer_capability == FTDM_BEARER_CAP_UNRESTRICTED) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_DIGITAL_MEDIA);
	}
	return FTDM_SUCCESS;
}

FT_DECLARE_DATA ftdm_logger_t ftdm_log = null_logger;

FT_DECLARE(void) ftdm_global_set_crash_policy(ftdm_crash_policy_t policy)
{
	g_ftdm_crash_policy |= policy;
}

FT_DECLARE(ftdm_status_t) ftdm_global_set_memory_handler(ftdm_memory_handler_t *handler)
{
	if (!handler) {
		return FTDM_FAIL;
	}
	if (!handler->malloc) {
		return FTDM_FAIL;
	}
	if (!handler->calloc) {
		return FTDM_FAIL;
	}
	if (!handler->free) {
		return FTDM_FAIL;
	}
	memcpy(&g_ftdm_mem_handler, handler, sizeof(*handler));
	return FTDM_SUCCESS;
}

FT_DECLARE(void) ftdm_global_set_logger(ftdm_logger_t logger)
{
	if (logger) {
		ftdm_log = logger;
	} else {
		ftdm_log = null_logger;
	}
}

FT_DECLARE(void) ftdm_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	ftdm_log = default_logger;
	ftdm_log_level = level;
}

FT_DECLARE_NONSTD(int) ftdm_hash_equalkeys(void *k1, void *k2)
{
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

FT_DECLARE_NONSTD(uint32_t) ftdm_hash_hashfromstring(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	uint32_t hash = 0;
    int c;
	
	while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}

static ftdm_status_t ftdm_channel_destroy(ftdm_channel_t *ftdmchan)
{

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CONFIGURED)) {

		while (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
			ftdm_log(FTDM_LOG_INFO, "Waiting for thread to exit on channel %u:%u\n", ftdmchan->span_id, ftdmchan->chan_id);
			ftdm_sleep(500);
		}

		ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
		ftdm_buffer_destroy(&ftdmchan->pre_buffer);
		ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

		ftdm_buffer_destroy(&ftdmchan->digit_buffer);
		ftdm_buffer_destroy(&ftdmchan->gen_dtmf_buffer);
		ftdm_buffer_destroy(&ftdmchan->dtmf_buffer);
		ftdm_buffer_destroy(&ftdmchan->fsk_buffer);
		ftdmchan->pre_buffer_size = 0;

		ftdm_safe_free(ftdmchan->dtmf_hangup_buf);

		if (ftdmchan->tone_session.buffer) {
			teletone_destroy_session(&ftdmchan->tone_session);
			memset(&ftdmchan->tone_session, 0, sizeof(ftdmchan->tone_session));
		}

		
		if (ftdmchan->span->fio->channel_destroy) {
			ftdm_log(FTDM_LOG_INFO, "Closing channel %s:%u:%u fd:%d\n", ftdmchan->span->type, ftdmchan->span_id, ftdmchan->chan_id, ftdmchan->sockfd);
			if (ftdmchan->span->fio->channel_destroy(ftdmchan) == FTDM_SUCCESS) {
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_CONFIGURED);
			} else {
				ftdm_log(FTDM_LOG_ERROR, "Error Closing channel %u:%u fd:%d\n", ftdmchan->span_id, ftdmchan->chan_id, ftdmchan->sockfd);
			}
		}

		ftdm_mutex_destroy(&ftdmchan->mutex);
		ftdm_mutex_destroy(&ftdmchan->pre_buffer_mutex);
		if (ftdmchan->state_completed_interrupt) {
			ftdm_interrupt_destroy(&ftdmchan->state_completed_interrupt);
		}
	}
	
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_span_destroy(ftdm_span_t *span)
{
	ftdm_status_t status = FTDM_SUCCESS;
	unsigned j;

	ftdm_mutex_lock(span->mutex);

	/* stop the signaling */

	/* This is a forced stopped */
	ftdm_clear_flag(span, FTDM_SPAN_NON_STOPPABLE);
	
	ftdm_span_stop(span);

	/* destroy the channels */
	ftdm_clear_flag(span, FTDM_SPAN_CONFIGURED);
	for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
		ftdm_channel_t *cur_chan = span->channels[j];
		if (cur_chan) {
			if (ftdm_test_flag(cur_chan, FTDM_CHANNEL_CONFIGURED)) {
				ftdm_channel_destroy(cur_chan);
			}
			ftdm_safe_free(cur_chan);
			cur_chan = NULL;
		}
	}

	/* destroy the I/O for the span */
	if (span->fio && span->fio->span_destroy) {
		ftdm_log(FTDM_LOG_INFO, "Destroying span %u type (%s)\n", span->span_id, span->type);
		if (span->fio->span_destroy(span) != FTDM_SUCCESS) {
			status = FTDM_FAIL;
		}
	}

	/* destroy final basic resources of the span data structure */
	if (span->pendingchans) {
		ftdm_queue_destroy(&span->pendingchans);
	}
	if (span->pendingsignals) {
		ftdm_queue_destroy(&span->pendingsignals);
	}
	ftdm_mutex_unlock(span->mutex);
	ftdm_mutex_destroy(&span->mutex);
	ftdm_safe_free(span->signal_data);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_get_alarms(ftdm_channel_t *ftdmchan, ftdm_alarm_flag_t *alarmbits)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(alarmbits != NULL, FTDM_EINVAL, "null alarmbits argument\n");
	ftdm_assert_return(ftdmchan != NULL, FTDM_EINVAL, "null channel argument\n");
	ftdm_assert_return(ftdmchan->span != NULL, FTDM_EINVAL, "null span\n");
	ftdm_assert_return(ftdmchan->span->fio != NULL, FTDM_EINVAL, "null io\n");

	*alarmbits = FTDM_ALARM_NONE;

	if (!ftdmchan->span->fio->get_alarms) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "No get_alarms interface for this channel\n");
		return FTDM_ENOSYS;
	}

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CONFIGURED)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Cannot get alarms from an unconfigured channel\n");
		return FTDM_EINVAL;
	}

	ftdm_channel_lock(ftdmchan);

	if ((status = ftdmchan->span->fio->get_alarms(ftdmchan)) != FTDM_SUCCESS) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Failed to get alarms from channel\n");
		goto done;
	}

	*ftdmchan->last_error = '\0';
	*alarmbits = ftdmchan->alarm_flags;
	if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_RED)) {
		snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "RED/");
	}
	if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_YELLOW)) {
		snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "YELLOW/");
	}
	if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_RAI)) {
		snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "RAI/");
	}
	if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_BLUE)) {
		snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "BLUE/");
	}
	if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_AIS)) {
		snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "AIS/");
	}
	if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_GENERAL)) {
		snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "GENERAL");
	}
	*(ftdmchan->last_error + strlen(ftdmchan->last_error) - 1) = '\0';

done:

	ftdm_channel_unlock(ftdmchan);	

	return status;
}

static void ftdm_span_add(ftdm_span_t *span)
{
	ftdm_span_t *sp;
	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp && sp->next; sp = sp->next);
	if (sp) {
		sp->next = span;
	} else {
		globals.spans = span;
	}
	hashtable_insert(globals.span_hash, (void *)span->name, span, HASHTABLE_FLAG_FREE_VALUE);
	ftdm_mutex_unlock(globals.span_mutex);
}

FT_DECLARE(ftdm_status_t) ftdm_span_stop(ftdm_span_t *span)
{
	ftdm_status_t status =  FTDM_SUCCESS;
	
	ftdm_mutex_lock(span->mutex);
	
	if (ftdm_test_flag(span, FTDM_SPAN_NON_STOPPABLE)) {
		status = FTDM_NOTIMPL;
		goto done;
	}

	if (!ftdm_test_flag(span, FTDM_SPAN_STARTED)) {
		status = FTDM_EINVAL;
		goto done;
	}

	if (!span->stop) {
		status = FTDM_ENOSYS;
		goto done;
	}

	/* Stop SIG */
	status = span->stop(span);
	if (status == FTDM_SUCCESS) {
		ftdm_clear_flag(span, FTDM_SPAN_STARTED);
	}

	/* Stop I/O */
	if (span->fio && span->fio->span_stop) {
		status = span->fio->span_stop(span);
	}
done:
	ftdm_mutex_unlock(span->mutex);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_create(const char *iotype, const char *name, ftdm_span_t **span)
{
	ftdm_span_t *new_span = NULL;
	ftdm_io_interface_t *fio = NULL;
	ftdm_status_t status = FTDM_FAIL;
	char buf[128] = "";

	ftdm_assert_return(iotype != NULL, FTDM_FAIL, "No IO type provided\n");
	ftdm_assert_return(name != NULL, FTDM_FAIL, "No span name provided\n");
	
	*span = NULL;

	fio = ftdm_global_get_io_interface(iotype, FTDM_TRUE);
	if (!fio) {
		ftdm_log(FTDM_LOG_CRIT, "failure creating span, no such I/O type '%s'\n", iotype);
		return FTDM_FAIL;
	}

	if (!fio->configure_span) {
		ftdm_log(FTDM_LOG_CRIT, "failure creating span, no configure_span method for I/O type '%s'\n", iotype);
		return FTDM_FAIL;
	}

	ftdm_mutex_lock(globals.mutex);
	if (globals.span_index < FTDM_MAX_SPANS_INTERFACE) {
		new_span = ftdm_calloc(sizeof(*new_span), 1);
		
		ftdm_assert(new_span, "allocating span failed\n");

		status = ftdm_mutex_create(&new_span->mutex);
		ftdm_assert(status == FTDM_SUCCESS, "mutex creation failed\n");

		ftdm_set_flag(new_span, FTDM_SPAN_CONFIGURED);
		new_span->span_id = ++globals.span_index;
		new_span->fio = fio;
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_DIAL], "%(1000,0,350,440)", FTDM_TONEMAP_LEN);
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_RING], "%(2000,4000,440,480)", FTDM_TONEMAP_LEN);
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_BUSY], "%(500,500,480,620)", FTDM_TONEMAP_LEN);
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_ATTN], "%(100,100,1400,2060,2450,2600)", FTDM_TONEMAP_LEN);
		new_span->trunk_type = FTDM_TRUNK_NONE;
		new_span->trunk_mode = FTDM_TRUNK_MODE_CPE;
		new_span->data_type = FTDM_TYPE_SPAN;

		ftdm_mutex_lock(globals.span_mutex);
		if (!ftdm_strlen_zero(name) && hashtable_search(globals.span_hash, (void *)name)) {
			ftdm_log(FTDM_LOG_WARNING, "name %s is already used, substituting 'span%d' as the name\n", name, new_span->span_id);
			name = NULL;
		}
		ftdm_mutex_unlock(globals.span_mutex);
		
		if (!name) {
			snprintf(buf, sizeof(buf), "span%d", new_span->span_id);
			name = buf;
		}
		new_span->name = ftdm_strdup(name);
		new_span->type = ftdm_strdup(iotype);
		ftdm_span_add(new_span);
		*span = new_span;
		status = FTDM_SUCCESS;
	}
	ftdm_mutex_unlock(globals.mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_close_all(void)
{
	ftdm_span_t *span;
	uint32_t i = 0, j;

	ftdm_mutex_lock(globals.span_mutex);
	for (span = globals.spans; span; span = span->next) {
		if (ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
			for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
				ftdm_channel_t *toclose = span->channels[j];
				if (ftdm_test_flag(toclose, FTDM_CHANNEL_INUSE)) {
					ftdm_channel_close(&toclose);
				}
				i++;
			}
		} 
	}
	ftdm_mutex_unlock(globals.span_mutex);

	return i ? FTDM_SUCCESS : FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_load_tones(ftdm_span_t *span, const char *mapname)
{
	ftdm_config_t cfg;
	char *var, *val;
	int x = 0;

	if (!ftdm_config_open_file(&cfg, "tones.conf")) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return FTDM_FAIL;
	}
	
	while (ftdm_config_next_pair(&cfg, &var, &val)) {
		int detect = 0;

		if (!strcasecmp(cfg.category, mapname) && var && val) {
			uint32_t index;
			char *name = NULL;

			if (!strncasecmp(var, "detect-", 7)) {
				name = var + 7;
				detect = 1;
			} else if (!strncasecmp(var, "generate-", 9)) {
				name = var + 9;
			} else {
				ftdm_log(FTDM_LOG_WARNING, "Unknown tone name %s\n", var);
				continue;
			}

			index = ftdm_str2ftdm_tonemap(name);

			if (index >= FTDM_TONEMAP_INVALID || index == FTDM_TONEMAP_NONE) {
				ftdm_log(FTDM_LOG_WARNING, "Unknown tone name %s\n", name);
			} else {
				if (detect) {
					char *p = val, *next;
					int i = 0;
					do {
						teletone_process_t this;
						next = strchr(p, ',');
						this = (teletone_process_t)atof(p);
						span->tone_detect_map[index].freqs[i++] = this;
						if (next) {
							p = next + 1;
						}
					} while (next);
					ftdm_log(FTDM_LOG_DEBUG, "added tone detect [%s] = [%s]\n", name, val);
				}  else {
					ftdm_log(FTDM_LOG_DEBUG, "added tone generation [%s] = [%s]\n", name, val);
					ftdm_copy_string(span->tone_map[index], val, sizeof(span->tone_map[index]));
				}
				x++;
			}
		}
	}

	ftdm_config_close_file(&cfg);
	
	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
	
}

#define FTDM_SLINEAR_MAX_VALUE 32767
#define FTDM_SLINEAR_MIN_VALUE -32767
static void reset_gain_table(uint8_t *gain_table, float new_gain, ftdm_codec_t codec_gain)
{
	/* sample value */
	uint8_t sv = 0;
	/* linear gain factor */
	float lingain = 0;
	/* linear value for each table sample */
	float linvalue = 0;
	/* amplified (or attenuated in case of negative amplification) sample value */
	int ampvalue = 0;

	/* gain tables are only for alaw and ulaw */
	if (codec_gain != FTDM_CODEC_ALAW && codec_gain != FTDM_CODEC_ULAW) {
		ftdm_log(FTDM_LOG_WARNING, "Not resetting gain table because codec is not ALAW or ULAW but %d\n", codec_gain);
		return;
	}

	if (!new_gain) {
		/* for a 0.0db gain table, each alaw/ulaw sample value is left untouched (0 ==0, 1 == 1, 2 == 2 etc)*/
		sv = 0;
		while (1) {
			gain_table[sv] = sv;
			if (sv == (FTDM_GAINS_TABLE_SIZE-1)) {
				break;
			}
			sv++;
		}
		return;
	}

	/* use the 20log rule to increase the gain: http://en.wikipedia.org/wiki/Gain, http:/en.wipedia.org/wiki/20_log_rule#Definitions */
	lingain = (float)pow(10.0, new_gain/ 20.0);
	sv = 0;
	while (1) {
		/* get the linear value for this alaw/ulaw sample value */
		linvalue = codec_gain == FTDM_CODEC_ALAW ? (float)alaw_to_linear(sv) : (float)ulaw_to_linear(sv);

		/* multiply the linear value and the previously calculated linear gain */
		ampvalue = (int)(linvalue * lingain);

		/* chop it if goes beyond the limits */
		if (ampvalue > FTDM_SLINEAR_MAX_VALUE) {
			ampvalue = FTDM_SLINEAR_MAX_VALUE;
		}

		if (ampvalue < FTDM_SLINEAR_MIN_VALUE) {
			ampvalue = FTDM_SLINEAR_MIN_VALUE;
		}
		gain_table[sv] = codec_gain == FTDM_CODEC_ALAW ? linear_to_alaw(ampvalue) : linear_to_ulaw(ampvalue);
		if (sv == (FTDM_GAINS_TABLE_SIZE-1)) {
			break;
		}
		sv++;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_span_add_channel(ftdm_span_t *span, ftdm_socket_t sockfd, ftdm_chan_type_t type, ftdm_channel_t **chan)
{
	unsigned char i = 0;
	if (span->chan_count < FTDM_MAX_CHANNELS_SPAN) {
		ftdm_channel_t *new_chan = span->channels[++span->chan_count];

		if (!new_chan) {
#ifdef FTDM_DEBUG_CHAN_MEMORY
			void *chanmem = NULL;
			int pages = 1;
			int pagesize = sysconf(_SC_PAGE_SIZE);
			if (sizeof(*new_chan) > pagesize) {
				pages = sizeof(*new_chan)/pagesize;
				pages++;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Allocating %d pages of %d bytes for channel of size %d\n", pages, pagesize, sizeof(*new_chan));
			if (posix_memalign(&chanmem, pagesize, pagesize*pages)) {
				return FTDM_FAIL;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Channel pages allocated start at mem %p\n", chanmem);
			memset(chanmem, 0, sizeof(*new_chan));
			new_chan = chanmem;
#else
			if (!(new_chan = ftdm_calloc(1, sizeof(*new_chan)))) {
				return FTDM_FAIL;
			}
#endif
			span->channels[span->chan_count] = new_chan;
		}

		new_chan->type = type;
		new_chan->sockfd = sockfd;
		new_chan->fio = span->fio;
		new_chan->span_id = span->span_id;
		new_chan->chan_id = span->chan_count;
		new_chan->span = span;
		new_chan->fds[FTDM_READ_TRACE_INDEX] = -1;
		new_chan->fds[FTDM_WRITE_TRACE_INDEX] = -1;
		new_chan->data_type = FTDM_TYPE_CHANNEL;
		if (!new_chan->dtmf_on) {
			new_chan->dtmf_on = FTDM_DEFAULT_DTMF_ON;
		}

		if (!new_chan->dtmf_off) {
			new_chan->dtmf_off = FTDM_DEFAULT_DTMF_OFF;
		}

		ftdm_mutex_create(&new_chan->mutex);
		ftdm_mutex_create(&new_chan->pre_buffer_mutex);

		ftdm_buffer_create(&new_chan->digit_buffer, 128, 128, 0);
		ftdm_buffer_create(&new_chan->gen_dtmf_buffer, 128, 128, 0);

		new_chan->dtmf_hangup_buf = ftdm_calloc (span->dtmf_hangup_len + 1, sizeof (char));

		/* set 0.0db gain table */
		i = 0;
		while (1) {
			new_chan->txgain_table[i] = i;
			new_chan->rxgain_table[i] = i;
			if (i == (sizeof(new_chan->txgain_table)-1)) {
				break;
			}
			i++;
		}

		ftdm_set_flag(new_chan, FTDM_CHANNEL_CONFIGURED | FTDM_CHANNEL_READY);
		new_chan->state = FTDM_CHANNEL_STATE_DOWN;
		new_chan->state_status = FTDM_STATE_STATUS_COMPLETED;
		*chan = new_chan;
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_find_by_name(const char *name, ftdm_span_t **span)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_mutex_lock(globals.span_mutex);
	if (!ftdm_strlen_zero(name)) {
		if ((*span = hashtable_search(globals.span_hash, (void *)name))) {
			status = FTDM_SUCCESS;
		} else {
			int span_id = atoi(name);

			ftdm_span_find(span_id, span);
			if (*span) {
				status = FTDM_SUCCESS;
			}
		}
	}
	ftdm_mutex_unlock(globals.span_mutex);
	
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_find(uint32_t id, ftdm_span_t **span)
{
	ftdm_span_t *fspan = NULL, *sp;

	if (id > FTDM_MAX_SPANS_INTERFACE) {
		return FTDM_FAIL;
	}

	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp; sp = sp->next) {
		if (sp->span_id == id) {
			fspan = sp;
			break;
		}
	}
	ftdm_mutex_unlock(globals.span_mutex);

	if (!fspan || !ftdm_test_flag(fspan, FTDM_SPAN_CONFIGURED)) {
		return FTDM_FAIL;
	}

	*span = fspan;

	return FTDM_SUCCESS;
	
}

FT_DECLARE(ftdm_status_t) ftdm_span_poll_event(ftdm_span_t *span, uint32_t ms, short *poll_events)
{
	assert(span->fio != NULL);

	if (span->fio->poll_event) {
		return span->fio->poll_event(span, ms, poll_events);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "poll_event method not implemented in module %s!", span->fio->name);
	}

	return FTDM_NOTIMPL;
}

/* handle oob events and send the proper SIGEVENT signal to user, when applicable */
static __inline__ ftdm_status_t ftdm_event_handle_oob(ftdm_event_t *event)
{
	ftdm_sigmsg_t sigmsg;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_channel_t *fchan = event->channel;
	ftdm_span_t *span = fchan->span;

	memset(&sigmsg, 0, sizeof(sigmsg));
	sigmsg.span_id = span->span_id;
	sigmsg.chan_id = fchan->chan_id;
	sigmsg.channel = fchan;
	switch (event->enum_id) {
	case FTDM_OOB_ALARM_CLEAR:
		{
			sigmsg.event_id = FTDM_SIGEVENT_ALARM_CLEAR;
			ftdm_clear_flag_locked(fchan, FTDM_CHANNEL_IN_ALARM);
			status = ftdm_span_send_signal(span, &sigmsg);
		}
		break;
	case FTDM_OOB_ALARM_TRAP:
		{
			sigmsg.event_id = FTDM_SIGEVENT_ALARM_TRAP;
			ftdm_set_flag_locked(fchan, FTDM_CHANNEL_IN_ALARM);
			status = ftdm_span_send_signal(span, &sigmsg);
		}
		break;
	default:
		/* NOOP */
		break;
	}
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_next_event(ftdm_span_t *span, ftdm_event_t **event)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_assert_return(span->fio != NULL, FTDM_FAIL, "No I/O module attached to this span!\n");

	if (!span->fio->next_event) {
		ftdm_log(FTDM_LOG_ERROR, "next_event method not implemented in module %s!", span->fio->name);
		return FTDM_NOTIMPL;
	}

	status = span->fio->next_event(span, event);
	if (status != FTDM_SUCCESS) {
		return status;
	}

	status = ftdm_event_handle_oob(*event);
	if (status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "failed to handle event %d\n", (*event)->e_type);
	}
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_read_event(ftdm_channel_t *ftdmchan, ftdm_event_t **event)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_span_t *span = ftdmchan->span;
	ftdm_assert_return(span->fio != NULL, FTDM_FAIL, "No I/O module attached to this span!\n");

	ftdm_channel_lock(ftdmchan);

	if (!span->fio->channel_next_event) {
		ftdm_log(FTDM_LOG_ERROR, "channel_next_event method not implemented in module %s!\n", span->fio->name);
		status = FTDM_NOTIMPL;
		goto done;
	}

	if (ftdm_test_io_flag(ftdmchan, FTDM_CHANNEL_IO_EVENT)) {
		ftdm_clear_io_flag(ftdmchan, FTDM_CHANNEL_IO_EVENT);
	}

	status = span->fio->channel_next_event(ftdmchan, event);
	if (status != FTDM_SUCCESS) {
		goto done;
	}

	status = ftdm_event_handle_oob(*event);
	if (status != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "failed to handle event %d\n", (*event)->e_type);
	}

done:
	ftdm_channel_unlock(ftdmchan);
	return status;
}

static ftdm_status_t ftdmchan_fsk_write_sample(int16_t *buf, ftdm_size_t buflen, void *user_data)
{
	ftdm_channel_t *ftdmchan = (ftdm_channel_t *) user_data;
	ftdm_buffer_write(ftdmchan->fsk_buffer, buf, buflen * 2);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_send_fsk_data(ftdm_channel_t *ftdmchan, ftdm_fsk_data_state_t *fsk_data, float db_level)
{
	struct ftdm_fsk_modulator fsk_trans;

	if (!ftdmchan->fsk_buffer) {
		ftdm_buffer_create(&ftdmchan->fsk_buffer, 128, 128, 0);
	} else {
		ftdm_buffer_zero(ftdmchan->fsk_buffer);
	}

	if (ftdmchan->token_count > 1) {
		ftdm_fsk_modulator_init(&fsk_trans, FSK_BELL202, ftdmchan->rate, fsk_data, db_level, 80, 5, 0, ftdmchan_fsk_write_sample, ftdmchan);
		ftdm_fsk_modulator_send_all((&fsk_trans));
	} else {
		ftdm_fsk_modulator_init(&fsk_trans, FSK_BELL202, ftdmchan->rate, fsk_data, db_level, 180, 5, 300, ftdmchan_fsk_write_sample, ftdmchan);
		ftdm_fsk_modulator_send_all((&fsk_trans));
		ftdmchan->buffer_delay = 3500 / ftdmchan->effective_interval;
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_clear_token(ftdm_channel_t *ftdmchan, const char *token)
{
	ftdm_status_t status = FTDM_FAIL;
	
	ftdm_mutex_lock(ftdmchan->mutex);
	if (token == NULL) {
		memset(ftdmchan->tokens, 0, sizeof(ftdmchan->tokens));
		ftdmchan->token_count = 0;
	} else if (*token != '\0') {
		char tokens[FTDM_MAX_TOKENS][FTDM_TOKEN_STRLEN];
		int32_t i, count = ftdmchan->token_count;
		memcpy(tokens, ftdmchan->tokens, sizeof(tokens));
		memset(ftdmchan->tokens, 0, sizeof(ftdmchan->tokens));
		ftdmchan->token_count = 0;		

		for (i = 0; i < count; i++) {
			if (strcmp(tokens[i], token)) {
				ftdm_copy_string(ftdmchan->tokens[ftdmchan->token_count], tokens[i], sizeof(ftdmchan->tokens[ftdmchan->token_count]));
				ftdmchan->token_count++;
			}
		}

		status = FTDM_SUCCESS;
	}
	ftdm_mutex_unlock(ftdmchan->mutex);

	return status;
}

FT_DECLARE(void) ftdm_channel_rotate_tokens(ftdm_channel_t *ftdmchan)
{
	if (ftdmchan->token_count) {
		memmove(ftdmchan->tokens[1], ftdmchan->tokens[0], ftdmchan->token_count * FTDM_TOKEN_STRLEN);
		ftdm_copy_string(ftdmchan->tokens[0], ftdmchan->tokens[ftdmchan->token_count], FTDM_TOKEN_STRLEN);
		*ftdmchan->tokens[ftdmchan->token_count] = '\0';
	}
}

FT_DECLARE(void) ftdm_channel_replace_token(ftdm_channel_t *ftdmchan, const char *old_token, const char *new_token)
{
	unsigned int i;

	if (ftdmchan->token_count) {
		for(i = 0; i < ftdmchan->token_count; i++) {
			if (!strcmp(ftdmchan->tokens[i], old_token)) {
				ftdm_copy_string(ftdmchan->tokens[i], new_token, FTDM_TOKEN_STRLEN);
				break;
			}
		}
	}
}

FT_DECLARE(void) ftdm_channel_set_private(ftdm_channel_t *ftdmchan, void *pvt)
{
	ftdmchan->user_private = pvt;
}

FT_DECLARE(void *) ftdm_channel_get_private(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->user_private;
}

FT_DECLARE(uint32_t) ftdm_channel_get_token_count(const ftdm_channel_t *ftdmchan)
{
	uint32_t count;
	ftdm_mutex_lock(ftdmchan->mutex);
	count = ftdmchan->token_count;
	ftdm_mutex_unlock(ftdmchan->mutex);
	return count;
}

FT_DECLARE(uint32_t) ftdm_channel_get_io_interval(const ftdm_channel_t *ftdmchan)
{
	uint32_t count;
	ftdm_mutex_lock(ftdmchan->mutex);
	count = ftdmchan->effective_interval;
	ftdm_mutex_unlock(ftdmchan->mutex);
	return count;
}

FT_DECLARE(uint32_t) ftdm_channel_get_io_packet_len(const ftdm_channel_t *ftdmchan)
{
	uint32_t count;
	ftdm_mutex_lock(ftdmchan->mutex);
	count = ftdmchan->packet_len;
	ftdm_mutex_unlock(ftdmchan->mutex);
	return count;
}

FT_DECLARE(uint32_t) ftdm_channel_get_type(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->type;
}

FT_DECLARE(ftdm_codec_t) ftdm_channel_get_codec(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->effective_codec;
}

FT_DECLARE(const char *) ftdm_channel_get_token(const ftdm_channel_t *ftdmchan, uint32_t tokenid)
{
	const char *token = NULL;
	ftdm_mutex_lock(ftdmchan->mutex);

	if (ftdmchan->token_count <= tokenid) {
		ftdm_mutex_unlock(ftdmchan->mutex);
		return NULL;
	}

	token = ftdmchan->tokens[tokenid];
	ftdm_mutex_unlock(ftdmchan->mutex);
	return token;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_add_token(ftdm_channel_t *ftdmchan, char *token, int end)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_mutex_lock(ftdmchan->mutex);
	if (ftdmchan->token_count < FTDM_MAX_TOKENS) {
		if (end) {
			ftdm_copy_string(ftdmchan->tokens[ftdmchan->token_count++], token, FTDM_TOKEN_STRLEN);
		} else {
			memmove(ftdmchan->tokens[1], ftdmchan->tokens[0], ftdmchan->token_count * FTDM_TOKEN_STRLEN);
			ftdm_copy_string(ftdmchan->tokens[0], token, FTDM_TOKEN_STRLEN);
			ftdmchan->token_count++;
		}
		status = FTDM_SUCCESS;
	}
	ftdm_mutex_unlock(ftdmchan->mutex);

	return status;
}


FT_DECLARE(uint32_t) ftdm_group_get_id(const ftdm_group_t *group)
{
	return group->group_id;
}

FT_DECLARE(ftdm_status_t) ftdm_group_channel_use_count(ftdm_group_t *group, uint32_t *count)
{
	uint32_t j;

	*count = 0;
	
	if (!group) {
		return FTDM_FAIL;
	}
	
	for(j = 0; j < group->chan_count && group->channels[j]; j++) {
		if (group->channels[j]) {
			if (ftdm_test_flag(group->channels[j], FTDM_CHANNEL_INUSE)) {
				(*count)++;
			}
		}
	}
	
	return FTDM_SUCCESS;
}

static __inline__ int chan_is_avail(ftdm_channel_t *check)
{
	if ((check->span->signal_type == FTDM_SIGTYPE_M2UA) || 
			(check->span->signal_type == FTDM_SIGTYPE_NONE)) {
		if (!ftdm_test_flag(check, FTDM_CHANNEL_READY) ||
			ftdm_test_flag(check, FTDM_CHANNEL_INUSE) ||
			ftdm_test_flag(check, FTDM_CHANNEL_SUSPENDED) ||
			ftdm_test_flag(check, FTDM_CHANNEL_IN_ALARM) ||
			check->state != FTDM_CHANNEL_STATE_DOWN) {
			return 0;
		}
	} else {
		if (!ftdm_test_flag(check, FTDM_CHANNEL_READY) ||
			!ftdm_test_flag(check, FTDM_CHANNEL_SIG_UP) ||
			ftdm_test_flag(check, FTDM_CHANNEL_INUSE) ||
			ftdm_test_flag(check, FTDM_CHANNEL_SUSPENDED) ||
			ftdm_test_flag(check, FTDM_CHANNEL_IN_ALARM) ||
			check->state != FTDM_CHANNEL_STATE_DOWN) {
			return 0;
		}
	}
	return 1;
}

static __inline__ int chan_voice_is_avail(ftdm_channel_t *check)
{
	if (!FTDM_IS_VOICE_CHANNEL(check)) {
		return 0;
	}
	return chan_is_avail(check);
}

static __inline__ int request_voice_channel(ftdm_channel_t *check, ftdm_channel_t **ftdmchan, 
		ftdm_caller_data_t *caller_data, ftdm_direction_t direction)
{
	ftdm_status_t status;
	if (chan_voice_is_avail(check)) {
		/* unlocked testing passed, try again with the channel locked */
		ftdm_mutex_lock(check->mutex);
		if (chan_voice_is_avail(check)) {
			if (check->span && check->span->channel_request) {
				/* I am only unlocking here cuz this function is called
				 * sometimes with the group or span lock held and were
				 * blocking anyone hunting for channels available and
				 * I believe teh channel_request() function may take
				 * a bit of time. However channel_request is a callback
				 * used by boost and may be only a few other old sig mods
				 * and it should be deprecated */
				ftdm_mutex_unlock(check->mutex);
				ftdm_set_caller_data(check->span, caller_data);
				status = check->span->channel_request(check->span, check->chan_id, 
					direction, caller_data, ftdmchan);
				if (status == FTDM_SUCCESS) {
					return 1;
				}
			} else {
				status = ftdm_channel_open_chan(check);
				if (status == FTDM_SUCCESS) {
					*ftdmchan = check;
					ftdm_set_flag(check, FTDM_CHANNEL_OUTBOUND);
#if 0
					ftdm_mutex_unlock(check->mutex);
#endif
					return 1;
				}
			}
		}
		ftdm_mutex_unlock(check->mutex);
	} 
	return 0;
}

static void __inline__ calculate_best_rate(ftdm_channel_t *check, ftdm_channel_t **best_rated, int *best_rate)
{
	if (ftdm_test_flag(check->span, FTDM_SPAN_USE_AV_RATE)) {
		ftdm_mutex_lock(check->mutex);
		if (ftdm_test_flag(check, FTDM_CHANNEL_INUSE)) {
			/* twiddle */
		} else if (ftdm_test_flag(check, FTDM_CHANNEL_SIG_UP)) {
			/* twiddle */
		} else if (check->availability_rate > *best_rate){
			/* the channel is not in use and the signaling status is down, 
			 * it is a potential candidate to place a call */
			*best_rated = check;
			*best_rate = check->availability_rate;
		}
		ftdm_mutex_unlock(check->mutex);
	}
}

static ftdm_status_t __inline__ get_best_rated(ftdm_channel_t **fchan, ftdm_channel_t *best_rated)
{
	ftdm_status_t status;

	if (!best_rated) {
		return FTDM_FAIL;
	}

	ftdm_mutex_lock(best_rated->mutex);

	if (ftdm_test_flag(best_rated, FTDM_CHANNEL_INUSE)) {
		ftdm_mutex_unlock(best_rated->mutex);
		return FTDM_FAIL;
	}

	ftdm_log_chan_msg(best_rated, FTDM_LOG_DEBUG, "I may not be available but I had the best availability rate, trying to open I/O now\n");

	status = ftdm_channel_open_chan(best_rated);
	if (status != FTDM_SUCCESS) {
		ftdm_mutex_unlock(best_rated->mutex);
		return FTDM_FAIL;
	}
	*fchan = best_rated;
	ftdm_set_flag(best_rated, FTDM_CHANNEL_OUTBOUND);
#if 0	
	ftdm_mutex_unlock(best_rated->mutex);
#endif
	return FTDM_SUCCESS;
}

static uint32_t __inline__ rr_next(uint32_t last, uint32_t min, uint32_t max, ftdm_direction_t direction)
{
	uint32_t next = min;

	ftdm_log(FTDM_LOG_DEBUG, "last = %d, min = %d, max = %d\n", last, min, max);

	if (direction == FTDM_RR_DOWN) {
		next = (last >= max) ? min : ++last;
	} else {
		next = (last <= min) ? max : --last;
	}
	return next;
}


FT_DECLARE(int) ftdm_channel_get_availability(ftdm_channel_t *ftdmchan)
{
	int availability = -1;
	ftdm_channel_lock(ftdmchan);
	if (ftdm_test_flag(ftdmchan->span, FTDM_SPAN_USE_AV_RATE)) {
		availability = ftdmchan->availability_rate;
	}
	ftdm_channel_unlock(ftdmchan);
	return availability;
}

static ftdm_status_t _ftdm_channel_open_by_group(uint32_t group_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_channel_t *check = NULL;
	ftdm_channel_t *best_rated = NULL;
	ftdm_group_t *group = NULL;
	int best_rate = 0;
	uint32_t i = 0;
	uint32_t count = 0;
	uint32_t first_channel = 0;

	if (group_id) {
		ftdm_group_find(group_id, &group);
	}

	if (!group) {
		ftdm_log(FTDM_LOG_ERROR, "Group %d not defined!\n", group_id);
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}

	ftdm_group_channel_use_count(group, &count);

	if (count >= group->chan_count) {
		ftdm_log(FTDM_LOG_WARNING, "All circuits are busy (%d channels used out of %d available).\n", count, group->chan_count);
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}

	
	if (direction == FTDM_TOP_DOWN) {
		i = 0;
	} else if (direction == FTDM_RR_DOWN || direction == FTDM_RR_UP) {
		i = rr_next(group->last_used_index, 0, group->chan_count - 1, direction);
		first_channel = i;
	} else {
		i = group->chan_count-1;
	}

	ftdm_mutex_lock(group->mutex);
	for (;;) {
	
		if (!(check = group->channels[i])) {
			status = FTDM_FAIL;
			break;
		}

		if (request_voice_channel(check, ftdmchan, caller_data, direction)) {
			status = FTDM_SUCCESS;
			if (direction == FTDM_RR_UP || direction == FTDM_RR_DOWN) {
				group->last_used_index = i;
			}
			break;
		}

		calculate_best_rate(check, &best_rated, &best_rate);

		if (direction == FTDM_TOP_DOWN) {
			if (i >= (group->chan_count - 1)) {
				break;
			}
			i++;
		} else if (direction == FTDM_RR_DOWN || direction == FTDM_RR_UP) {
			if (check == best_rated) {
				group->last_used_index = i;
			}
			i = rr_next(i, 0, group->chan_count - 1, direction);
			if (first_channel == i) {
				break;
			}
		} else {
			if (i == 0) {
				break;
			}
			i--;
		}
	}

	if (status == FTDM_FAIL) {
		status = get_best_rated(ftdmchan, best_rated);
	}

	ftdm_mutex_unlock(group->mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_group(uint32_t group_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status;
	status = _ftdm_channel_open_by_group(group_id, direction, caller_data, ftdmchan);
	if (status == FTDM_SUCCESS) {
		ftdm_channel_t *fchan = *ftdmchan;
		ftdm_channel_unlock(fchan);
	}
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_channel_use_count(ftdm_span_t *span, uint32_t *count)
{
	uint32_t j;

	*count = 0;
	
	if (!span || !ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
		return FTDM_FAIL;
	}
	
	for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
		if (span->channels[j]) {
			if (ftdm_test_flag(span->channels[j], FTDM_CHANNEL_INUSE)) {
				(*count)++;
			}
		}
	}
	
	return FTDM_SUCCESS;
}

/* Hunt a channel by span, if successful the channel is returned locked */
static ftdm_status_t _ftdm_channel_open_by_span(uint32_t span_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_channel_t *check = NULL;
	ftdm_channel_t *best_rated = NULL;
	ftdm_span_t *span = NULL;
	int best_rate = 0;
	uint32_t i = 0;
	uint32_t count = 0;
	uint32_t first_channel = 0;

	*ftdmchan = NULL;

	if (!span_id) {
		ftdm_log(FTDM_LOG_CRIT, "No span supplied\n");
		return FTDM_FAIL;
	}

	ftdm_span_find(span_id, &span);

	if (!span || !ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
		ftdm_log(FTDM_LOG_CRIT, "span %d not defined or configured!\n", span_id);
		return FTDM_FAIL;
	}

	ftdm_span_channel_use_count(span, &count);

	if (count >= span->chan_count) {
		ftdm_log(FTDM_LOG_WARNING, "All circuits are busy: active=%i max=%i.\n", count, span->chan_count);
		return FTDM_FAIL;
	}

	if (span->channel_request && !ftdm_test_flag(span, FTDM_SPAN_SUGGEST_CHAN_ID)) {
		ftdm_set_caller_data(span, caller_data);
		return span->channel_request(span, 0, direction, caller_data, ftdmchan);
	}
		
	ftdm_mutex_lock(span->mutex);
	
	if (direction == FTDM_TOP_DOWN) {
		i = 1;
	} else if (direction == FTDM_RR_DOWN || direction == FTDM_RR_UP) {
		i = rr_next(span->last_used_index, 1, span->chan_count, direction);
		first_channel = i;
	} else {
		i = span->chan_count;
	}	
		
	for(;;) {

		if (direction == FTDM_TOP_DOWN) {
			if (i > span->chan_count) {
				break;
			}
		} else {
			if (i == 0) {
				break;
			}
		}
			
		if (!(check = span->channels[i])) {
			status = FTDM_FAIL;
			break;
		}

		if (request_voice_channel(check, ftdmchan, caller_data, direction)) {
			status = FTDM_SUCCESS;
			if (direction == FTDM_RR_UP || direction == FTDM_RR_DOWN) {
				span->last_used_index = i;
			}
			break;
		}
			
		calculate_best_rate(check, &best_rated, &best_rate);

		if (direction == FTDM_TOP_DOWN) {
			i++;
		} else if (direction == FTDM_RR_DOWN || direction == FTDM_RR_UP) {
			if (check == best_rated) {
				span->last_used_index = i;
			}
			i = rr_next(i, 1, span->chan_count, direction);
			if (first_channel == i) {
				break;
			}
		} else {
			i--;
		}
	}

	if (status == FTDM_FAIL) {
		status = get_best_rated(ftdmchan, best_rated);
	}

	ftdm_mutex_unlock(span->mutex);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_span(uint32_t span_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status;
	status = _ftdm_channel_open_by_span(span_id, direction, caller_data, ftdmchan);
	if (status == FTDM_SUCCESS) {
		ftdm_channel_t *fchan = *ftdmchan;
		ftdm_channel_unlock(fchan);
	}
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_chan(ftdm_channel_t *ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "invalid ftdmchan pointer\n");

	ftdm_mutex_lock(ftdmchan->mutex);

	if (FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SUSPENDED)) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "Channel is suspended\n");
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Cannot open channel when is suspended\n");
			goto done;
		}
		
		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_IN_ALARM) && !ftdm_test_flag(ftdmchan->span, FTDM_SPAN_PWR_SAVING)) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "Channel is alarmed\n");
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Cannot open channel when is alarmed\n");
			goto done;
		}
		
		if (globals.cpu_monitor.alarm &&
				  globals.cpu_monitor.alarm_action_flags & FTDM_CPU_ALARM_ACTION_REJECT) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "CPU usage alarm is on - refusing to open channel\n");
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "CPU usage alarm is on - refusing to open channel\n");
			ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_SWITCH_CONGESTION;
			goto done;
		}
	}

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Channel is not ready");
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Cannot open channel when is not ready\n");
		goto done;
	}

	status = ftdmchan->fio->open(ftdmchan);
	if (status == FTDM_SUCCESS) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OPEN | FTDM_CHANNEL_INUSE);
	} else {
		ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "IO open failed: %d\n", status);
	}

done:
	
	ftdm_mutex_unlock(ftdmchan->mutex);

	return status;
}

static ftdm_status_t _ftdm_channel_open(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan, uint8_t physical)
{
	ftdm_channel_t *check = NULL;
	ftdm_span_t *span = NULL;
	ftdm_channel_t *best_rated = NULL;
	ftdm_status_t status = FTDM_FAIL;
	int best_rate = 0;

	*ftdmchan = NULL;

	ftdm_mutex_lock(globals.mutex);

	ftdm_span_find(span_id, &span);

	if (!span) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find span!\n");
		goto done;
	}

	if (!ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
		ftdm_log(FTDM_LOG_CRIT, "Span %d is not configured\n", span_id);
		goto done;
	}

	if (span->channel_request) {
		ftdm_log(FTDM_LOG_ERROR, "Individual channel selection not implemented on this span.\n");
		goto done;
	}

	if (physical) { /* Open by physical */
		ftdm_channel_t *fchan = NULL;
		ftdm_iterator_t *citer = NULL;
		ftdm_iterator_t *curr = NULL;

		if (chan_id < 1) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid physical channel %d to open in span %d\n", chan_id, span_id);
			status = FTDM_FAIL;
			goto done;
		}

		citer = ftdm_span_get_chan_iterator(span, NULL);
		if (!citer) {
			status = ENOMEM;
			goto done;
		}

		for (curr = citer ; curr; curr = ftdm_iterator_next(curr)) {
			fchan = ftdm_iterator_current(curr);
			if (fchan->physical_chan_id == chan_id) {
				check = fchan;
				break;
			}
		}

		ftdm_iterator_free(citer);
		if (!check) {
			ftdm_log(FTDM_LOG_CRIT, "Wow, no physical channel %d in span %d\n", chan_id, span_id);
			goto done;
		}
	} else { /* Open by logical */
		if (chan_id < 1 || chan_id > span->chan_count) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid channel %d to open in span %d\n", chan_id, span_id);
			goto done;
		}

		if (!(check = span->channels[chan_id])) {
			ftdm_log(FTDM_LOG_CRIT, "Wow, no channel %d in span %d\n", chan_id, span_id);
			goto done;
		}
	}

	ftdm_channel_lock(check);

	if (ftdm_test_flag(check, FTDM_CHANNEL_OPEN)) {
		/* let them know is already open, but return the channel anyway */
		status = FTDM_EBUSY;
		*ftdmchan = check;
		goto unlockchan;
	}

	/* The following if's and gotos replace a big if (this || this || this || this) else { nothing; } */

	/* if it is not a voice channel, nothing else to check to open it */
	if (!FTDM_IS_VOICE_CHANNEL(check)) {
		goto openchan;
	}

	/* if it's an FXS device with a call active and has callwaiting enabled, we allow to open it twice */
	if (check->type == FTDM_CHAN_TYPE_FXS 
	    && check->token_count == 1 
	    && ftdm_channel_test_feature(check, FTDM_CHANNEL_FEATURE_CALLWAITING)) {
		goto openchan;
	}

	/* if channel is available, time to open it */
	if (chan_is_avail(check)) {
		goto openchan;
	}

	/* not available, but still might be available ... */
	calculate_best_rate(check, &best_rated, &best_rate);
	if (best_rated) {
		goto openchan;
	}

	/* channel is unavailable, do not open the channel */
	goto unlockchan;

openchan:
	if (!ftdm_test_flag(check, FTDM_CHANNEL_OPEN)) {
		status = check->fio->open(check);
		if (status == FTDM_SUCCESS) {
			ftdm_set_flag(check, FTDM_CHANNEL_OPEN);
		}
	} else {
		status = FTDM_SUCCESS;
	}
	ftdm_set_flag(check, FTDM_CHANNEL_INUSE);
	ftdm_set_flag(check, FTDM_CHANNEL_OUTBOUND);
	*ftdmchan = check;

	/* we've got the channel, do not unlock it */
	goto done;

unlockchan:
	ftdm_channel_unlock(check);

done:
	ftdm_mutex_unlock(globals.mutex);
	if (status != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to open channel %d:%d\n", span_id, chan_id);
	}

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status;
	status = _ftdm_channel_open(span_id, chan_id, ftdmchan, 0);
	if (status == FTDM_SUCCESS) {
		ftdm_channel_t *fchan = *ftdmchan;
		ftdm_channel_unlock(fchan);
	}
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_ph(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status;
	status = _ftdm_channel_open(span_id, chan_id, ftdmchan, 1);
	if (status == FTDM_SUCCESS) {
		ftdm_channel_t *fchan = *ftdmchan;
		ftdm_channel_unlock(fchan);
	}
	return status;
}

FT_DECLARE(uint32_t) ftdm_channel_get_id(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->chan_id;
}

FT_DECLARE(uint32_t) ftdm_channel_get_ph_id(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->physical_chan_id;
}

FT_DECLARE(uint32_t) ftdm_channel_get_span_id(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->span_id;
}

FT_DECLARE(ftdm_span_t *) ftdm_channel_get_span(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->span;
}

FT_DECLARE(const char *) ftdm_channel_get_span_name(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->span->name;
}

FT_DECLARE(void) ftdm_span_set_trunk_type(ftdm_span_t *span, ftdm_trunk_type_t type)
{
	span->trunk_type = type;
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_blocking_mode(const ftdm_span_t *span, ftdm_bool_t enabled)
{
	ftdm_channel_t *fchan = NULL;
	ftdm_iterator_t *citer = NULL;
	ftdm_iterator_t *curr = NULL;

	citer = ftdm_span_get_chan_iterator(span, NULL);
	if (!citer) {
		return FTDM_ENOMEM;
	}

	for (curr = citer ; curr; curr = ftdm_iterator_next(curr)) {
		fchan = ftdm_iterator_current(curr);
		if (enabled) {
			ftdm_clear_flag_locked(fchan, FTDM_CHANNEL_NONBLOCK);
		} else {
			ftdm_set_flag_locked(fchan, FTDM_CHANNEL_NONBLOCK);
		}
	}
	ftdm_iterator_free(citer);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_trunk_type_t) ftdm_span_get_trunk_type(const ftdm_span_t *span)
{
	return span->trunk_type;
}

FT_DECLARE(const char *) ftdm_span_get_trunk_type_str(const ftdm_span_t *span)
{
	return ftdm_trunk_type2str(span->trunk_type);
}

FT_DECLARE(void) ftdm_span_set_trunk_mode(ftdm_span_t *span, ftdm_trunk_mode_t mode)
{
	span->trunk_mode = mode;
}

FT_DECLARE(ftdm_trunk_mode_t) ftdm_span_get_trunk_mode(const ftdm_span_t *span)
{
	return span->trunk_mode;
}

FT_DECLARE(const char *) ftdm_span_get_trunk_mode_str(const ftdm_span_t *span)
{
	return ftdm_trunk_mode2str(span->trunk_mode);
}

FT_DECLARE(uint32_t) ftdm_span_get_id(const ftdm_span_t *span)
{
	return span->span_id;
}

FT_DECLARE(const char *) ftdm_span_get_name(const ftdm_span_t *span)
{
	return span->name;
}

FT_DECLARE(const char *) ftdm_channel_get_name(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->chan_name;
}

FT_DECLARE(const char *) ftdm_channel_get_number(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->chan_number;
}

FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_hold(const ftdm_channel_t *ftdmchan)
{
	ftdm_bool_t condition;
	ftdm_channel_lock(ftdmchan);
	condition = ftdm_test_flag(ftdmchan, FTDM_CHANNEL_HOLD) ? FTDM_TRUE : FTDM_FALSE;
	ftdm_channel_unlock(ftdmchan);
	return condition;
}

FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_answered(const ftdm_channel_t *ftdmchan)
{
	ftdm_bool_t condition = FTDM_FALSE;

	ftdm_channel_lock(ftdmchan);
	condition = (ftdmchan->state == FTDM_CHANNEL_STATE_UP) ? FTDM_TRUE : FTDM_FALSE;
	ftdm_channel_unlock(ftdmchan);

	return condition;
}

FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_busy(const ftdm_channel_t *ftdmchan)
{
	ftdm_bool_t condition = FTDM_FALSE;

	ftdm_channel_lock(ftdmchan);
	condition = (ftdmchan->state == FTDM_CHANNEL_STATE_BUSY) ? FTDM_TRUE : FTDM_FALSE;
	ftdm_channel_unlock(ftdmchan);

	return condition;
}

FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_hangup(const ftdm_channel_t *ftdmchan)
{
	ftdm_bool_t condition = FTDM_FALSE;

	ftdm_channel_lock(ftdmchan);
	condition = (ftdmchan->state == FTDM_CHANNEL_STATE_HANGUP || ftdmchan->state == FTDM_CHANNEL_STATE_TERMINATING) 
		? FTDM_TRUE : FTDM_FALSE;
	ftdm_channel_unlock(ftdmchan);

	return condition;
}

FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_done(const ftdm_channel_t *ftdmchan)
{
	ftdm_bool_t condition = FTDM_FALSE;

	ftdm_channel_lock(ftdmchan);
	condition = (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN) ? FTDM_TRUE : FTDM_FALSE;
	ftdm_channel_unlock(ftdmchan);

	return condition;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hold(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status;
	ftdm_channel_lock(ftdmchan);

	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_HOLD);
	status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_DIALTONE, 0, usrmsg);
	ftdm_channel_unlock(ftdmchan);

	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_unhold(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status;

	ftdm_channel_lock(ftdmchan);

	status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_UP, 0, usrmsg);

	ftdm_channel_unlock(ftdmchan);

	return status;
}

FT_DECLARE(void) ftdm_ack_indication(ftdm_channel_t *fchan, ftdm_channel_indication_t indication, ftdm_status_t status)
{
	ftdm_sigmsg_t msg;

	if (!ftdm_test_flag(fchan, FTDM_CHANNEL_IND_ACK_PENDING)) {
		return;
	}

	ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "Acknowledging indication %s in state %s (rc = %d)\n",
			ftdm_channel_indication2str(indication), ftdm_channel_state2str(fchan->state), status);
	ftdm_clear_flag(fchan, FTDM_CHANNEL_IND_ACK_PENDING);
	memset(&msg, 0, sizeof(msg));
	msg.channel = fchan;
	msg.event_id = FTDM_SIGEVENT_INDICATION_COMPLETED;
	msg.ev_data.indication_completed.indication = indication;
	msg.ev_data.indication_completed.status = status;
	ftdm_span_send_signal(fchan->span, &msg);
}

/*! Answer call without locking the channel. The caller must have locked first */
static ftdm_status_t _ftdm_channel_call_answer_nl(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_SUCCESS;

	if (!ftdm_test_flag(ftdmchan->span, FTDM_SPAN_USE_SKIP_STATES)) {
		/* We will fail RFC's if we not skip states, but some modules apart from ftmod_sangoma_isdn 
		* expect the call to always to go PROGRESS and PROGRESS MEDIA state before going to UP, so
		* use FTDM_SPAN_USE_SKIP_STATES for now while we update the sig modules */

		if (ftdmchan->state < FTDM_CHANNEL_STATE_PROGRESS) {
			status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_PROGRESS, 1, usrmsg);
			if (status != FTDM_SUCCESS) {
				status = FTDM_ECANCELED;
				goto done;
			}
		}

		/* set state unlocks the channel so we need to re-confirm that the channel hasn't gone to hell */
		if (ftdmchan->state == FTDM_CHANNEL_STATE_TERMINATING) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Ignoring answer because the call has moved to TERMINATING while we're moving to PROGRESS\n");
			status = FTDM_ECANCELED;
			goto done;
		}

		if (ftdmchan->state < FTDM_CHANNEL_STATE_PROGRESS_MEDIA) {
			status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, 1, usrmsg);
			if (status != FTDM_SUCCESS) {
				status = FTDM_ECANCELED;
				goto done;
			}
		}

		/* set state unlocks the channel so we need to re-confirm that the channel hasn't gone to hell */
		if (ftdmchan->state == FTDM_CHANNEL_STATE_TERMINATING) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Ignoring answer because the call has moved to TERMINATING while we're moving to UP\n");
			status = FTDM_ECANCELED;
			goto done;
		}
	}

	status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_UP, 1, usrmsg);
	if (status != FTDM_SUCCESS) {
		status = FTDM_ECANCELED;
		goto done;
	}

done:

	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_answer(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status;

	/* we leave the locking up to ftdm_channel_call_indicate, DO NOT lock here since ftdm_channel_call_indicate expects
	 * the lock recursivity to be 1 */
	status = _ftdm_channel_call_indicate(file, func, line, ftdmchan, FTDM_CHANNEL_INDICATE_ANSWER, usrmsg);

	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_transfer(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, const char* arg, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status;
	ftdm_usrmsg_t *msg = NULL;
	ftdm_bool_t free_msg = FTDM_FALSE;

	if (!usrmsg) {
		msg = ftdm_calloc(1, sizeof(*msg));
		ftdm_assert_return(msg, FTDM_FAIL, "Failed to allocate usr msg");
		memset(msg, 0, sizeof(*msg));
		free_msg = FTDM_TRUE;
	} else {
		msg = usrmsg;
	}

	ftdm_usrmsg_add_var(msg, "transfer_arg", arg);
	/* we leave the locking up to ftdm_channel_call_indicate, DO NOT lock here since ftdm_channel_call_indicate expects
	* the lock recursivity to be 1 */
	status = _ftdm_channel_call_indicate(file, func, line, ftdmchan, FTDM_CHANNEL_INDICATE_TRANSFER, msg);
	if (free_msg == FTDM_TRUE) {
		ftdm_safe_free(msg);
	}
	return status;
}

/* lock must be acquired by the caller! */
static ftdm_status_t _ftdm_channel_call_hangup_nl(const char *file, const char *func, int line, ftdm_channel_t *chan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_SUCCESS;

	/* In native sigbridge mode we ignore hangup requests from the user and hangup only when the signaling module decides it */
	if (ftdm_test_flag(chan, FTDM_CHANNEL_NATIVE_SIGBRIDGE) && chan->state != FTDM_CHANNEL_STATE_TERMINATING) {

		ftdm_log_chan_ex(chan, file, func, line, FTDM_LOG_LEVEL_DEBUG, 
				"Ignoring hangup in channel in state %s (native bridge enabled)\n", ftdm_channel_state2str(chan->state));
		ftdm_set_flag(chan, FTDM_CHANNEL_USER_HANGUP);
		goto done;
	}

	if (chan->state != FTDM_CHANNEL_STATE_DOWN) {
		if (chan->state == FTDM_CHANNEL_STATE_HANGUP) {
			/* make user's life easier, and just ignore double hangup requests */
			return FTDM_SUCCESS;
		}
		if (chan->hangup_timer) {
			ftdm_sched_cancel_timer(globals.timingsched, chan->hangup_timer);
		}
		ftdm_set_flag(chan, FTDM_CHANNEL_USER_HANGUP);
		/* if a state change requested by the user was pending, a hangup certainly cancels that request  */
		if (ftdm_test_flag(chan, FTDM_CHANNEL_STATE_CHANGE)) {
			ftdm_channel_cancel_state(file, func, line, chan);
		}
		status = ftdm_channel_set_state(file, func, line, chan, FTDM_CHANNEL_STATE_HANGUP, 1, usrmsg);
	} else {
		/* the signaling stack did not touch the state, 
		 * core is responsible from clearing flags and stuff, however, because ftmod_analog
		 * is a bitch in a serious need of refactoring, we also check whether the channel is open
		 * to avoid an spurious warning about the channel not being open. This is because ftmod_analog
		 * does not follow our convention of sending SIGEVENT_STOP and waiting for the user to move
		 * to HANGUP (implicitly through ftdm_channel_call_hangup(), as soon as ftmod_analog is fixed
		 * this check can be removed */
		if (ftdm_test_flag(chan, FTDM_CHANNEL_OPEN)) {
			ftdm_channel_close(&chan);
		}
	}

done:
	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hangup_with_cause(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_call_cause_t cause, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_channel_lock(ftdmchan);

	ftdmchan->caller_data.hangup_cause = cause;
	
	status = _ftdm_channel_call_hangup_nl(file, func, line, ftdmchan, usrmsg);

	ftdm_channel_unlock(ftdmchan);
	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hangup(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_SUCCESS;

	ftdm_channel_lock(ftdmchan);
	
	ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_CLEARING;

	status = _ftdm_channel_call_hangup_nl(file, func, line, ftdmchan, usrmsg);

	ftdm_channel_unlock(ftdmchan);
	return status;
}

FT_DECLARE(const char *) ftdm_channel_get_last_error(const ftdm_channel_t *ftdmchan)
{
	return ftdmchan->last_error;
}

FT_DECLARE(const char *) ftdm_span_get_last_error(const ftdm_span_t *span)
{
	return span->last_error;
}

FT_DECLARE(ftdm_caller_data_t *) ftdm_channel_get_caller_data(ftdm_channel_t *ftdmchan)
{
	return &ftdmchan->caller_data;
}

FT_DECLARE(ftdm_channel_t *) ftdm_span_get_channel(const ftdm_span_t *span, uint32_t chanid)
{
	ftdm_channel_t *chan;
	ftdm_mutex_lock(span->mutex);
	if (chanid == 0 || chanid > span->chan_count) {
		ftdm_mutex_unlock(span->mutex);
		return NULL;
	}
	chan = span->channels[chanid];
	ftdm_mutex_unlock(span->mutex);
	return chan;
}

FT_DECLARE(ftdm_channel_t *) ftdm_span_get_channel_ph(const ftdm_span_t *span, uint32_t chanid)
{
	ftdm_channel_t *chan = NULL;
	ftdm_channel_t *fchan = NULL;
	ftdm_iterator_t *citer = NULL;
	ftdm_iterator_t *curr = NULL;

	ftdm_mutex_lock(span->mutex);
	if (chanid == 0) {
		ftdm_mutex_unlock(span->mutex);
		return NULL;
	}

	citer = ftdm_span_get_chan_iterator(span, NULL);
	if (!citer) {
		ftdm_mutex_unlock(span->mutex);
		return NULL;
	}

	for (curr = citer ; curr; curr = ftdm_iterator_next(curr)) {
		fchan = ftdm_iterator_current(curr);
		if (fchan->physical_chan_id == chanid) {
			chan = fchan;
			break;
		}
	}

	ftdm_iterator_free(citer);

	ftdm_mutex_unlock(span->mutex);
	return chan;
}

FT_DECLARE(uint32_t) ftdm_span_get_chan_count(const ftdm_span_t *span)
{
	uint32_t count;
	ftdm_mutex_lock(span->mutex);
	count = span->chan_count;
	ftdm_mutex_unlock(span->mutex);
	return count;
}

FT_DECLARE(uint32_t) ftdm_channel_get_ph_span_id(const ftdm_channel_t *ftdmchan)
{
	uint32_t id;
	ftdm_channel_lock(ftdmchan);
	id = ftdmchan->physical_span_id;
	ftdm_channel_unlock(ftdmchan);
	return id;
}

/*
 * Every user requested indication *MUST* be acknowledged with the proper status (ftdm_status_t)
 * However, if the indication fails before we notify the signaling stack, we don't need to ack
 * but if we already notified the signaling stack about the indication, the signaling stack is
 * responsible for the acknowledge. Bottom line is, whenever this function returns FTDM_SUCCESS
 * someone *MUST* acknowledge the indication, either the signaling stack, this function or the core
 * at some later point
 * */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_indicate(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_SUCCESS;

	ftdm_assert_return(ftdmchan, FTDM_FAIL, "Null channel\n");

	ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, "Indicating %s in state %s\n",
			ftdm_channel_indication2str(indication), ftdm_channel_state2str(ftdmchan->state));

	ftdm_channel_lock(ftdmchan);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_NATIVE_SIGBRIDGE)) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, 
				"Ignoring indication %s in channel in state %s (native bridge enabled)\n",
				ftdm_channel_indication2str(indication), 
				ftdm_channel_state2str(ftdmchan->state));
		status = FTDM_SUCCESS;
		goto done;
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_IND_ACK_PENDING)) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_WARNING, "Cannot indicate %s in channel with indication %s still pending in state %s\n",
				ftdm_channel_indication2str(indication), 
				ftdm_channel_indication2str(ftdmchan->indication),
				ftdm_channel_state2str(ftdmchan->state));
		status = FTDM_EBUSY;
		goto done;
	}

	ftdmchan->indication = indication;
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_NONBLOCK)) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_IND_ACK_PENDING);
	}

	if (indication != FTDM_CHANNEL_INDICATE_FACILITY &&
	    ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {

		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_WARNING, "Cannot indicate %s in outgoing channel in state %s\n",
				ftdm_channel_indication2str(indication), ftdm_channel_state2str(ftdmchan->state));
		status = FTDM_EINVAL;
		goto done;
	}

	if (ftdmchan->state == FTDM_CHANNEL_STATE_TERMINATING) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, "Ignoring indication %s because the call is in %s state\n",
				ftdm_channel_indication2str(indication), ftdm_channel_state2str(ftdmchan->state));
		status = FTDM_ECANCELED;
		goto done;
	}

	switch (indication) {
	/* FIXME: ring and busy cannot be used with all signaling stacks 
	 * (particularly isdn stacks I think, we should emulate or just move to hangup with busy cause) */
	case FTDM_CHANNEL_INDICATE_RINGING:
		status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_RINGING, 1, usrmsg);
		break;
	case FTDM_CHANNEL_INDICATE_BUSY:
		status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_BUSY, 1, usrmsg);
		break;
	case FTDM_CHANNEL_INDICATE_PROCEED:
		if (!ftdm_test_flag(ftdmchan->span, FTDM_SPAN_USE_PROCEED_STATE) ||
		   	ftdmchan->state >= FTDM_CHANNEL_STATE_PROCEED) {
			ftdm_ack_indication(ftdmchan, indication, status);
			goto done;
		}
		status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_PROCEED, 1, usrmsg);
		break;
	case FTDM_CHANNEL_INDICATE_PROGRESS:
		status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_PROGRESS, 1, usrmsg);
		break;
	case FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA:
		if (!ftdm_test_flag(ftdmchan->span, FTDM_SPAN_USE_SKIP_STATES)) {
			if (ftdmchan->state < FTDM_CHANNEL_STATE_PROGRESS) {
				status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_PROGRESS, 1, usrmsg);
				if (status != FTDM_SUCCESS) {
					goto done;
				}
			}

			/* set state unlocks the channel so we need to re-confirm that the channel hasn't gone to hell */
			if (ftdmchan->state == FTDM_CHANNEL_STATE_TERMINATING) {
				ftdm_log_chan_ex_msg(ftdmchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, "Ignoring progress media because the call is terminating\n");
				goto done;
			}
		}
		status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, 1, usrmsg);
		break;
	case FTDM_CHANNEL_INDICATE_ANSWER:
		status = _ftdm_channel_call_answer_nl(file, func, line, ftdmchan, usrmsg);
		break;
	case FTDM_CHANNEL_INDICATE_TRANSFER:
		if (!ftdm_test_flag(ftdmchan->span, FTDM_SPAN_USE_TRANSFER)) {
			ftdm_log_chan_ex_msg(ftdmchan, file, func, line, FTDM_LOG_LEVEL_WARNING, "Transfer not supported\n");
			status = FTDM_EINVAL;
			goto done;
		}
		status = ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_TRANSFER, 1, usrmsg);
		break;
	default:
		/* See if signalling module can provide this indication */
		status = ftdm_channel_sig_indicate(ftdmchan, indication, usrmsg);
		break;
	}

done:
	ftdm_channel_unlock(ftdmchan);

	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_reset(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "null channel");

	ftdm_channel_lock(ftdmchan);
	ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_RESET, 1, usrmsg);
	ftdm_channel_unlock(ftdmchan);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_get_channel_from_string(const char *string_id, ftdm_span_t **out_span, ftdm_channel_t **out_channel)
{
	ftdm_status_t status = FTDM_SUCCESS;
	int rc = 0;
	ftdm_span_t *span = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	unsigned span_id = 0;
	unsigned chan_id = 0;

	*out_span = NULL;
	*out_channel = NULL;

	if (!string_id) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot parse NULL channel id string\n");
		status = FTDM_EINVAL;
		goto done;
	}

	rc = sscanf(string_id, "%u:%u", &span_id, &chan_id);
	if (rc != 2) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to parse channel id string '%s'\n", string_id);
		status = FTDM_EINVAL;
		goto done;
	} 

	status = ftdm_span_find(span_id, &span);
	if (status != FTDM_SUCCESS || !span) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to find span for channel id string '%s'\n", string_id);
		status = FTDM_EINVAL;
		goto done;
	} 

	if (chan_id > (FTDM_MAX_CHANNELS_SPAN+1) || !(ftdmchan = span->channels[chan_id])) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid channel id string '%s'\n", string_id);
		status = FTDM_EINVAL;
		goto done;
	}

	status = FTDM_SUCCESS;
	*out_span = span;
	*out_channel = ftdmchan;
done:
	return status;
}

/* this function MUST be called with the channel lock held with lock recursivity of 1 exactly, 
 * and the caller must be aware we might unlock the channel for a brief period of time and then lock it again */
static ftdm_status_t _ftdm_channel_call_place_nl(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	const char *var = NULL;
	ftdm_status_t status = FTDM_FAIL;
	
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "null channel");
	ftdm_assert_return(ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND), FTDM_FAIL, "Call place, but outbound flag not set\n");

	if (!ftdmchan->span->outgoing_call) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "outgoing_call method not implemented in this span!\n");
		status = FTDM_ENOSYS;
		goto done;
	}

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Cannot place call in channel that is not open!\n");
		goto done;
	}

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Cannot place call in non outbound channel in state %s!\n", ftdm_channel_state2str(ftdmchan->state));
		goto done;
	}

	status = ftdmchan->span->outgoing_call(ftdmchan);
	if (status == FTDM_BREAK) {
		/* the signaling module detected glare on time */
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Glare detected, you should hunt in another channel!\n");
		goto done;
	}
	
	if (status != FTDM_SUCCESS) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Failed to place call!\n");
		goto done;
	}

	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_CALL_STARTED);
	ftdm_call_set_call_id(ftdmchan, &ftdmchan->caller_data);
	var = ftdm_usrmsg_get_var(usrmsg, "sigbridge_peer");
	if (var) {
		ftdm_span_t *peer_span = NULL;
		ftdm_channel_t *peer_chan = NULL;
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_NATIVE_SIGBRIDGE);
		ftdm_get_channel_from_string(var, &peer_span, &peer_chan);
		if (peer_chan) {
			ftdm_set_flag(peer_chan, FTDM_CHANNEL_NATIVE_SIGBRIDGE);
		}
	}

	/* if the signaling stack left the channel in state down on success, is expecting us to move to DIALING */
	if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN) {
		if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_NONBLOCK)) {
			ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_DIALING, 1, usrmsg);	
		} else {
			ftdm_channel_set_state(file, func, line, ftdmchan, FTDM_CHANNEL_STATE_DIALING, 0, usrmsg);	
		}
	} else if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE) &&
		   !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_NONBLOCK)) {
		
		ftdm_channel_unlock(ftdmchan);

		ftdm_interrupt_wait(ftdmchan->state_completed_interrupt, 500);

		ftdm_channel_lock(ftdmchan);
	}

done:
	ftdm_unused_arg(file);
	ftdm_unused_arg(func);
	ftdm_unused_arg(line);
	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_channel_call_place(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status;
	ftdm_channel_lock(ftdmchan);

	/* be aware that _ftdm_channl_call_place_nl can unlock/lock the channel quickly if working in blocking mode  */
	status = _ftdm_channel_call_place_nl(file, func, line, ftdmchan, usrmsg);

	ftdm_channel_unlock(ftdmchan);
	return status;
}

FT_DECLARE(ftdm_status_t) _ftdm_call_place(const char *file, const char *func, int line, 
		   ftdm_caller_data_t *caller_data, ftdm_hunting_scheme_t *hunting, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_channel_t *fchan = NULL;

	ftdm_assert_return(caller_data, FTDM_EINVAL, "Invalid caller data\n");
	ftdm_assert_return(hunting, FTDM_EINVAL, "Invalid hunting scheme\n");

	if (hunting->mode == FTDM_HUNT_SPAN) {
		status = _ftdm_channel_open_by_span(hunting->mode_data.span.span_id, 
				hunting->mode_data.span.direction, caller_data, &fchan);
	} else if (hunting->mode == FTDM_HUNT_GROUP) {
		status = _ftdm_channel_open_by_group(hunting->mode_data.group.group_id, 
				hunting->mode_data.group.direction, caller_data, &fchan);
	} else if (hunting->mode == FTDM_HUNT_CHAN) {
		status = _ftdm_channel_open(hunting->mode_data.chan.span_id, hunting->mode_data.chan.chan_id, &fchan, 0);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Cannot make outbound call with invalid hunting mode %d\n", hunting->mode);
		return FTDM_EINVAL;
	}

	if (status != FTDM_SUCCESS) {
		return FTDM_EBUSY;
	}

	/* we have a locked channel and are not afraid of using it! */
	if (hunting->result_cb) {
		status = hunting->result_cb(fchan, caller_data);
		if (status != FTDM_SUCCESS) {
			status = FTDM_ECANCELED;
			goto done;
		}
	}

	ftdm_channel_set_caller_data(fchan, caller_data);

	/* be aware that _ftdm_channl_call_place_nl can unlock/lock the channel quickly if working in blocking mode  */
	status = _ftdm_channel_call_place_nl(file, func, line, fchan, usrmsg);
	if (status != FTDM_SUCCESS) {
		_ftdm_channel_call_hangup_nl(file, func, line, fchan, usrmsg);
		goto done;
	}

	/* let the user know which channel was picked and which call id was generated */
	caller_data->fchan = fchan;
	caller_data->call_id = fchan->caller_data.call_id;
done:
	ftdm_channel_unlock(fchan);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_set_sig_status(ftdm_channel_t *fchan, ftdm_signaling_status_t sigstatus)
{
	ftdm_status_t res;

	ftdm_assert_return(fchan != NULL, FTDM_FAIL, "Null channel\n");
	ftdm_assert_return(fchan->span != NULL, FTDM_FAIL, "Null span\n");
	ftdm_assert_return(fchan->span->set_channel_sig_status != NULL, FTDM_ENOSYS, "Not implemented\n");

	ftdm_channel_lock(fchan);

	res = fchan->span->set_channel_sig_status(fchan, sigstatus);

	ftdm_channel_unlock(fchan);

	return res;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_get_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *sigstatus)
{
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Null channel\n");
	ftdm_assert_return(ftdmchan->span != NULL, FTDM_FAIL, "Null span\n");
	ftdm_assert_return(sigstatus != NULL, FTDM_FAIL, "Null sig status parameter\n");
	
	if (ftdmchan->span->get_channel_sig_status) {
		ftdm_status_t res;
		ftdm_channel_lock(ftdmchan);
		res = ftdmchan->span->get_channel_sig_status(ftdmchan, sigstatus);
		ftdm_channel_unlock(ftdmchan);
		return res;
	} else {
		/* don't log error here, it can be called just to test if its supported */
		return FTDM_NOTIMPL;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_sig_status(ftdm_span_t *span, ftdm_signaling_status_t sigstatus)
{
	ftdm_assert_return(span != NULL, FTDM_FAIL, "Null span\n");
	
	if (sigstatus == FTDM_SIG_STATE_DOWN) {
		ftdm_log(FTDM_LOG_WARNING, "The user is not allowed to set the signaling status to DOWN, valid states are UP or SUSPENDED\n");
		return FTDM_FAIL;
	}
	
	if (span->set_span_sig_status) {
		return span->set_span_sig_status(span, sigstatus);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "set_span_sig_status method not implemented!\n");
		return FTDM_FAIL;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_span_get_sig_status(ftdm_span_t *span, ftdm_signaling_status_t *sigstatus)
{
	ftdm_assert_return(span != NULL, FTDM_FAIL, "Null span\n");
	ftdm_assert_return(sigstatus != NULL, FTDM_FAIL, "Null sig status parameter\n");
	
	if (span->get_span_sig_status) {
		return span->get_span_sig_status(span, sigstatus);
	} else {
		return FTDM_FAIL;
	}
}

static ftdm_status_t ftdm_channel_sig_indicate(ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication, ftdm_usrmsg_t *usrmsg)
{
	ftdm_status_t status = FTDM_FAIL;
	if (ftdmchan->span->indicate) {
		
		ftdm_channel_save_usrmsg(ftdmchan, usrmsg);
		
		status = ftdmchan->span->indicate(ftdmchan, indication);
		if (status == FTDM_NOTIMPL) {
			ftdm_log(FTDM_LOG_WARNING, "Do not know how to indicate %s\n", ftdm_channel_indication2str(indication));
		} else if (status != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_WARNING, "Failed to indicate %s\n", ftdm_channel_indication2str(indication));
		} else { /* SUCCESS */
			ftdm_ack_indication(ftdmchan, indication, FTDM_SUCCESS);
		}
		ftdm_usrmsg_free(&ftdmchan->usrmsg);
	} else {
		return FTDM_NOTIMPL;
	}
	return status;
}


/* this function must be called with the channel lock */
static ftdm_status_t ftdm_channel_done(ftdm_channel_t *ftdmchan)
{
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Null channel can't be done!\n");

	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OPEN);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_INUSE);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_WINK);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_FLASH);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_HOLD);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_RINGING);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_3WAY);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_PROGRESS);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_MEDIA);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_ANSWERED);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_USER_HANGUP);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_DIGITAL_MEDIA);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_NATIVE_SIGBRIDGE);
	ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
	ftdm_buffer_destroy(&ftdmchan->pre_buffer);
	ftdmchan->pre_buffer_size = 0;
	ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

	if (ftdmchan->hangup_timer) {
		ftdm_sched_cancel_timer(globals.timingsched, ftdmchan->hangup_timer);
	}

	ftdmchan->init_state = FTDM_CHANNEL_STATE_DOWN;
	ftdmchan->state = FTDM_CHANNEL_STATE_DOWN;
	ftdmchan->state_status = FTDM_STATE_STATUS_COMPLETED;

	ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_DEBUG_DTMF, NULL);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_INPUT_DUMP, NULL);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_OUTPUT_DUMP, NULL);

	if (FTDM_IS_VOICE_CHANNEL(ftdmchan) && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CALL_STARTED)) {
		ftdm_sigmsg_t sigmsg;
		memset(&sigmsg, 0, sizeof(sigmsg));
		sigmsg.span_id = ftdmchan->span_id;
		sigmsg.chan_id = ftdmchan->chan_id;
		sigmsg.channel = ftdmchan;
		sigmsg.event_id = FTDM_SIGEVENT_RELEASED;
		ftdm_span_send_signal(ftdmchan->span, &sigmsg);
		ftdm_call_clear_call_id(&ftdmchan->caller_data);
		ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_CALL_STARTED);
	}

	if (ftdmchan->txdrops || ftdmchan->rxdrops) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "channel dropped data: txdrops = %d, rxdrops = %d\n",
				ftdmchan->txdrops, ftdmchan->rxdrops);
	}
	
	memset(&ftdmchan->caller_data, 0, sizeof(ftdmchan->caller_data));

	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_HOLD);

	memset(ftdmchan->tokens, 0, sizeof(ftdmchan->tokens));
	ftdmchan->token_count = 0;

	ftdm_channel_flush_dtmf(ftdmchan);

	if (ftdmchan->gen_dtmf_buffer) {
		ftdm_buffer_zero(ftdmchan->gen_dtmf_buffer);
	}

	if (ftdmchan->dtmf_buffer) {
		ftdm_buffer_zero(ftdmchan->dtmf_buffer);
	}

	if (ftdmchan->digit_buffer) {
		ftdm_buffer_zero(ftdmchan->digit_buffer);
	}

	if (!ftdmchan->dtmf_on) {
		ftdmchan->dtmf_on = FTDM_DEFAULT_DTMF_ON;
	}

	if (!ftdmchan->dtmf_off) {
		ftdmchan->dtmf_off = FTDM_DEFAULT_DTMF_OFF;
	}

	memset(ftdmchan->dtmf_hangup_buf, '\0', ftdmchan->span->dtmf_hangup_len);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE)) {
		ftdmchan->effective_codec = ftdmchan->native_codec;
		ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
		ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
	}
	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "channel done\n");
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_use(ftdm_channel_t *ftdmchan)
{

	ftdm_assert(ftdmchan != NULL, "Null channel\n");

	ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INUSE);

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_close(ftdm_channel_t **ftdmchan)
{
	ftdm_channel_t *check;
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "null channel double pointer provided!\n");
	ftdm_assert_return(*ftdmchan != NULL, FTDM_FAIL, "null channel pointer provided!\n");

	check = *ftdmchan;
	*ftdmchan = NULL;

	if (ftdm_test_flag(check, FTDM_CHANNEL_CONFIGURED)) {
		ftdm_mutex_lock(check->mutex);
		if (!ftdm_test_flag(check, FTDM_CHANNEL_OPEN)) {
			ftdm_log_chan_msg(check, FTDM_LOG_WARNING, "Channel not opened, proceeding anyway\n");
		}
		status = check->fio->close(check);
		ftdm_assert(status == FTDM_SUCCESS, "Failed to close channel!\n");
		ftdm_channel_done(check);
		*ftdmchan = NULL;
		check->ring_count = 0;
		ftdm_mutex_unlock(check->mutex);
	}
	
	return status;
}

static ftdm_status_t ftdmchan_activate_dtmf_buffer(ftdm_channel_t *ftdmchan)
{
	if (!ftdmchan->dtmf_buffer) {
		if (ftdm_buffer_create(&ftdmchan->dtmf_buffer, 1024, 3192, 0) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to allocate DTMF Buffer!\n");
			return FTDM_FAIL;
		} else {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Created DTMF buffer\n");
		}
	}

	
	if (!ftdmchan->tone_session.buffer) {
		memset(&ftdmchan->tone_session, 0, sizeof(ftdmchan->tone_session));
		teletone_init_session(&ftdmchan->tone_session, 0, NULL, NULL);
	}

	ftdmchan->tone_session.rate = ftdmchan->rate;
	ftdmchan->tone_session.duration = ftdmchan->dtmf_on * (ftdmchan->tone_session.rate / 1000);
	ftdmchan->tone_session.wait = ftdmchan->dtmf_off * (ftdmchan->tone_session.rate / 1000);
	ftdmchan->tone_session.volume = -7;

	/*
	  ftdmchan->tone_session.debug = 1;
	  ftdmchan->tone_session.debug_stream = stdout;
	*/

	return FTDM_SUCCESS;
}

/*
 * ftdmchan_activate_dtmf_buffer to initialize ftdmchan->dtmf_buffer should be called prior to
 * calling ftdm_insert_dtmf_pause
 */
static ftdm_status_t ftdm_insert_dtmf_pause(ftdm_channel_t *ftdmchan, ftdm_size_t pausems)
{
	void *data = NULL;
	ftdm_size_t datalen = pausems * sizeof(uint16_t);

	data = ftdm_malloc(datalen);
	ftdm_assert(data, "Failed to allocate memory\n");

	memset(data, FTDM_SILENCE_VALUE(ftdmchan), datalen);

	ftdm_buffer_write(ftdmchan->dtmf_buffer, data, datalen);
	ftdm_safe_free(data);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_command(ftdm_channel_t *ftdmchan, ftdm_command_t command, void *obj)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "No channel\n");
	ftdm_assert_return(ftdmchan->fio != NULL, FTDM_FAIL, "No IO attached to channel\n");

	ftdm_channel_lock(ftdmchan);

	switch (command) {

	case FTDM_COMMAND_ENABLE_CALLERID_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CALLERID)) {
				if (ftdm_fsk_demod_init(&ftdmchan->fsk, ftdmchan->rate, ftdmchan->fsk_buf, sizeof(ftdmchan->fsk_buf)) != FTDM_SUCCESS) {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
					GOTO_STATUS(done, FTDM_FAIL);
				}
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_CALLERID_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CALLERID)) {
				ftdm_fsk_demod_destroy(&ftdmchan->fsk);
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_TRACE_INPUT:
		{
			char *path = FTDM_COMMAND_OBJ_CHAR_P;
			if (ftdmchan->fds[FTDM_READ_TRACE_INDEX] > 0) {
				close(ftdmchan->fds[FTDM_READ_TRACE_INDEX]);
				ftdmchan->fds[FTDM_READ_TRACE_INDEX] = -1;
			}
			if ((ftdmchan->fds[FTDM_READ_TRACE_INDEX] = open(path, O_WRONLY | O_CREAT | O_TRUNC 
							| FTDM_O_BINARY, S_IRUSR | S_IWUSR)) > -1) {
				ftdm_log(FTDM_LOG_DEBUG, "Tracing channel %u:%u input to [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, path);	
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
			GOTO_STATUS(done, FTDM_FAIL);
		}
		break;
	case FTDM_COMMAND_TRACE_OUTPUT:
		{
			char *path = (char *) obj;
			if (ftdmchan->fds[FTDM_WRITE_TRACE_INDEX] > 0) {
				close(ftdmchan->fds[FTDM_WRITE_TRACE_INDEX]);
				ftdmchan->fds[FTDM_WRITE_TRACE_INDEX] = -1;
			}
			if ((ftdmchan->fds[FTDM_WRITE_TRACE_INDEX] = open(path, O_WRONLY | O_CREAT | O_TRUNC
							| FTDM_O_BINARY, S_IRUSR | S_IWUSR)) > -1) {
				ftdm_log(FTDM_LOG_DEBUG, "Tracing channel %u:%u output to [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, path);	
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
			GOTO_STATUS(done, FTDM_FAIL);
		}
		break;
	case FTDM_COMMAND_TRACE_END_ALL:
		{
			if (ftdmchan->fds[FTDM_READ_TRACE_INDEX] > 0) {
				close(ftdmchan->fds[FTDM_READ_TRACE_INDEX]);
				ftdmchan->fds[FTDM_READ_TRACE_INDEX] = -1;
			}
			if (ftdmchan->fds[FTDM_WRITE_TRACE_INDEX] > 0) {
				close(ftdmchan->fds[FTDM_WRITE_TRACE_INDEX]);
				ftdmchan->fds[FTDM_WRITE_TRACE_INDEX] = -1;
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Enable DTMF debugging */
	case FTDM_COMMAND_ENABLE_DEBUG_DTMF:
		{
			if (ftdmchan->dtmfdbg.enabled) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Cannot enable debug DTMF again\n");	
				GOTO_STATUS(done, FTDM_FAIL);
			}
			if (ftdmchan->rxdump.buffer) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Cannot debug DTMF if Rx dumping is already enabled\n");	
				GOTO_STATUS(done, FTDM_FAIL);
			}
			if (start_chan_io_dump(ftdmchan, &ftdmchan->rxdump, FTDM_IO_DUMP_DEFAULT_BUFF_SIZE) != FTDM_SUCCESS) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Failed to enable rx dump for DTMF debugging\n");	
				GOTO_STATUS(done, FTDM_FAIL);
			}
			ftdmchan->dtmfdbg.enabled = 1;
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Enabled DTMF debugging\n");	
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Disable DTMF debugging (if not disabled explicitly, it is disabled automatically when calls hangup) */
	case FTDM_COMMAND_DISABLE_DEBUG_DTMF:
		{
			if (!ftdmchan->dtmfdbg.enabled) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "DTMF debug is already disabled\n");	
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			if (disable_dtmf_debug(ftdmchan) != FTDM_SUCCESS) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Failed to disable DTMF debug\n");	
				GOTO_STATUS(done, FTDM_FAIL);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Start dumping all input to a circular buffer. The size of the circular buffer can be specified, default used otherwise */
	case FTDM_COMMAND_ENABLE_INPUT_DUMP:
		{
			ftdm_size_t size = obj ? FTDM_COMMAND_OBJ_SIZE : FTDM_IO_DUMP_DEFAULT_BUFF_SIZE;
			if (ftdmchan->rxdump.buffer) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Input dump is already enabled\n");
				GOTO_STATUS(done, FTDM_FAIL);
			}
			if (start_chan_io_dump(ftdmchan, &ftdmchan->rxdump, size) != FTDM_SUCCESS) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to enable input dump of size %"FTDM_SIZE_FMT"\n", size);
				GOTO_STATUS(done, FTDM_FAIL);
			}
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Enabled input dump with size %"FTDM_SIZE_FMT"\n", size);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Stop dumping all input to a circular buffer. */
	case FTDM_COMMAND_DISABLE_INPUT_DUMP:
		{
			if (!ftdmchan->rxdump.buffer) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No need to disable input dump\n");
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Disabled input dump of size %"FTDM_SIZE_FMT"\n", 
					ftdmchan->rxdump.size);
			stop_chan_io_dump(&ftdmchan->rxdump);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Start dumping all output to a circular buffer. The size of the circular buffer can be specified, default used otherwise */
	case FTDM_COMMAND_ENABLE_OUTPUT_DUMP:
		{
			ftdm_size_t size = obj ? FTDM_COMMAND_OBJ_SIZE : FTDM_IO_DUMP_DEFAULT_BUFF_SIZE;
			if (ftdmchan->txdump.buffer) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Output dump is already enabled\n");
				GOTO_STATUS(done, FTDM_FAIL);
			}
			if (start_chan_io_dump(ftdmchan, &ftdmchan->txdump, size) != FTDM_SUCCESS) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to enable output dump of size %"FTDM_SIZE_FMT"\n", size);
				GOTO_STATUS(done, FTDM_FAIL);
			}
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Enabled output dump with size %"FTDM_SIZE_FMT"\n", size);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Stop dumping all output to a circular buffer. */
	case FTDM_COMMAND_DISABLE_OUTPUT_DUMP:
		{
			if (!ftdmchan->txdump.buffer) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No need to disable output dump\n");
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Disabled output dump of size %"FTDM_SIZE_FMT"\n", ftdmchan->rxdump.size);
			stop_chan_io_dump(&ftdmchan->txdump);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Dump the current input circular buffer to the specified FILE* structure */
	case FTDM_COMMAND_DUMP_INPUT:
		{
			if (!obj) {
				GOTO_STATUS(done, FTDM_FAIL);
			}
			if (!ftdmchan->rxdump.buffer) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Not dumped input to file %p, input dump is not enabled\n", obj);
				GOTO_STATUS(done, FTDM_FAIL);
			}
			dump_chan_io_to_file(ftdmchan, &ftdmchan->rxdump, obj);
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Dumped input of size %"FTDM_SIZE_FMT" to file %p\n", ftdmchan->rxdump.size, obj);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	/*!< Dump the current output circular buffer to the specified FILE* structure */
	case FTDM_COMMAND_DUMP_OUTPUT:
		{
			if (!obj) {
				GOTO_STATUS(done, FTDM_FAIL);
			}
			if (!ftdmchan->txdump.buffer) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Not dumped output to file %p, output dump is not enabled\n", obj);
				GOTO_STATUS(done, FTDM_FAIL);
			}
			dump_chan_io_to_file(ftdmchan, &ftdmchan->txdump, obj);
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Dumped input of size %"FTDM_SIZE_FMT" to file %p\n", ftdmchan->txdump.size, obj);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;

	case FTDM_COMMAND_SET_INTERVAL:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL)) {
				ftdmchan->effective_interval = FTDM_COMMAND_OBJ_INT;
				if (ftdmchan->effective_interval == ftdmchan->native_interval) {
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_BUFFER);
				} else {
					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_BUFFER);
				}
				ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_GET_INTERVAL:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->effective_interval;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_SET_CODEC:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				ftdmchan->effective_codec = FTDM_COMMAND_OBJ_INT;
				
				if (ftdmchan->effective_codec == ftdmchan->native_codec) {
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
				} else {
					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
				}
				ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;

	case FTDM_COMMAND_SET_NATIVE_CODEC:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				ftdmchan->effective_codec = ftdmchan->native_codec;
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
				ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;

	case FTDM_COMMAND_GET_CODEC: 
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->effective_codec;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_GET_NATIVE_CODEC: 
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->native_codec;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_ENABLE_PROGRESS_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_PROGRESS)) {
				/* if they don't have thier own, use ours */
				ftdm_channel_clear_detected_tones(ftdmchan);
				ftdm_channel_clear_needed_tones(ftdmchan);
				teletone_multi_tone_init(&ftdmchan->span->tone_finder[FTDM_TONEMAP_DIAL], &ftdmchan->span->tone_detect_map[FTDM_TONEMAP_DIAL]);
				teletone_multi_tone_init(&ftdmchan->span->tone_finder[FTDM_TONEMAP_RING], &ftdmchan->span->tone_detect_map[FTDM_TONEMAP_RING]);
				teletone_multi_tone_init(&ftdmchan->span->tone_finder[FTDM_TONEMAP_BUSY], &ftdmchan->span->tone_detect_map[FTDM_TONEMAP_BUSY]);
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_PROGRESS_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_PROGRESS)) {
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT);
				ftdm_channel_clear_detected_tones(ftdmchan);
				ftdm_channel_clear_needed_tones(ftdmchan);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_ENABLE_DTMF_DETECT:
		{
			/* if they don't have thier own, use ours */
			if (FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
				if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT)) {
					teletone_dtmf_detect_init (&ftdmchan->dtmf_detect, ftdmchan->rate);
					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT);
					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF);
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Enabled software DTMF detector\n");
					GOTO_STATUS(done, FTDM_SUCCESS);
				}
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_DTMF_DETECT:
		{
			if (FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
				if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT)) {
								teletone_dtmf_detect_init (&ftdmchan->dtmf_detect, ftdmchan->rate);
								ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT);
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF);
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Disabled software DTMF detector\n");
					GOTO_STATUS(done, FTDM_SUCCESS);
				}
			}
		}
		break;
	case FTDM_COMMAND_SET_PRE_BUFFER_SIZE:
		{
			int val = FTDM_COMMAND_OBJ_INT;

			if (val < 0) {
				val = 0;
			}

			ftdmchan->pre_buffer_size = val * 8;

			ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
			if (!ftdmchan->pre_buffer_size) {
				ftdm_buffer_destroy(&ftdmchan->pre_buffer);
			} else if (!ftdmchan->pre_buffer) {
				ftdm_buffer_create(&ftdmchan->pre_buffer, 1024, ftdmchan->pre_buffer_size, 0);
			}
			ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

			GOTO_STATUS(done, FTDM_SUCCESS);

		}
		break;
	case FTDM_COMMAND_GET_DTMF_ON_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->dtmf_on;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_GET_DTMF_OFF_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->dtmf_on;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_SET_DTMF_ON_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				int val = FTDM_COMMAND_OBJ_INT;
				if (val > 10 && val < 1000) {
					ftdmchan->dtmf_on = val;
					GOTO_STATUS(done, FTDM_SUCCESS);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid value %d range 10-1000", val);
					GOTO_STATUS(done, FTDM_FAIL);
				}
			}
		}
		break;
	case FTDM_COMMAND_SET_DTMF_OFF_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				int val = FTDM_COMMAND_OBJ_INT;
				if (val > 10 && val < 1000) {
					ftdmchan->dtmf_off = val;
					GOTO_STATUS(done, FTDM_SUCCESS);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid value %d range 10-1000", val);
					GOTO_STATUS(done, FTDM_FAIL);
				}
			}
		}
		break;
	case FTDM_COMMAND_SEND_DTMF:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				char *digits = FTDM_COMMAND_OBJ_CHAR_P;
				
				if ((status = ftdmchan_activate_dtmf_buffer(ftdmchan)) != FTDM_SUCCESS) {
					GOTO_STATUS(done, status);
				}
				
				ftdm_buffer_write(ftdmchan->gen_dtmf_buffer, digits, strlen(digits));
				
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;

	case FTDM_COMMAND_DISABLE_ECHOCANCEL:
		{
			ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
			ftdm_buffer_destroy(&ftdmchan->pre_buffer);
			ftdmchan->pre_buffer_size = 0;
			ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);
		}
		break;

	case FTDM_COMMAND_SET_RX_GAIN:
		{
			if (!FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
				ftdm_log(FTDM_LOG_ERROR, "Cannot set rx gain in non-voice channel of type: %s\n", ftdm_chan_type2str(ftdmchan->type));
				GOTO_STATUS(done, FTDM_FAIL);
			}
			ftdmchan->rxgain = FTDM_COMMAND_OBJ_FLOAT;
			reset_gain_table(ftdmchan->rxgain_table, ftdmchan->rxgain, ftdmchan->native_codec);
			if (ftdmchan->rxgain == 0.0) {
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_USE_RX_GAIN);
			} else {
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_USE_RX_GAIN);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_GET_RX_GAIN:
		{
			FTDM_COMMAND_OBJ_FLOAT = ftdmchan->rxgain;
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_SET_TX_GAIN:
		{
			if (!FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
				ftdm_log(FTDM_LOG_ERROR, "Cannot set tx gain in non-voice channel of type: %s\n", ftdm_chan_type2str(ftdmchan->type));
				GOTO_STATUS(done, FTDM_FAIL);
			}
			ftdmchan->txgain = FTDM_COMMAND_OBJ_FLOAT;
			reset_gain_table(ftdmchan->txgain_table, ftdmchan->txgain, ftdmchan->native_codec);
			if (ftdmchan->txgain == 0.0) {
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_USE_TX_GAIN);
			} else {
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_USE_TX_GAIN);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_GET_TX_GAIN:
		{
			FTDM_COMMAND_OBJ_FLOAT = ftdmchan->txgain;
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_GET_IOSTATS:
		{
			if (!obj) {
				GOTO_STATUS(done, FTDM_EINVAL);
			}
			memcpy(obj, &ftdmchan->iostats, sizeof(ftdmchan->iostats));
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_SWITCH_IOSTATS:
		{
			ftdm_bool_t enable = *(ftdm_bool_t *)obj;
			if (enable) {
				ftdm_channel_set_feature(ftdmchan, FTDM_CHANNEL_FEATURE_IO_STATS);
			} else {
				ftdm_channel_clear_feature(ftdmchan, FTDM_CHANNEL_FEATURE_IO_STATS);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	default:
		break;
	}

	if (!ftdmchan->fio->command) {
		ftdm_log(FTDM_LOG_ERROR, "no command function defined by the I/O freetdm module!\n");	
		GOTO_STATUS(done, FTDM_FAIL);
	}

    	status = ftdmchan->fio->command(ftdmchan, command, obj);

	if (status == FTDM_NOTIMPL) {
		ftdm_log(FTDM_LOG_ERROR, "I/O backend does not support command %d!\n", command);	
	}

done:
	ftdm_channel_unlock(ftdmchan);

	return status;

}

FT_DECLARE(ftdm_status_t) ftdm_channel_wait(ftdm_channel_t *ftdmchan, ftdm_wait_flag_t *flags, int32_t to)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Null channel\n");
	ftdm_assert_return(ftdmchan->fio != NULL, FTDM_FAIL, "Null io interface\n");
	ftdm_assert_return(ftdmchan->fio->wait != NULL, FTDM_NOTIMPL, "wait method not implemented\n");

	status = ftdmchan->fio->wait(ftdmchan, flags, to);
	if (status == FTDM_TIMEOUT) {
		/* make sure the flags are cleared on timeout */
		*flags = 0;
	}
	return status;
}

/*******************************/
FIO_CODEC_FUNCTION(fio_slin2ulaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	uint32_t i;
	ftdm_size_t len = *datalen;

	if (max > len) {
		max = len;
	}

	memcpy(sln, data, max);
	
	for(i = 0; i < max; i++) {
		*lp++ = linear_to_ulaw(*sln++);
	}

	*datalen = max / 2;

	return FTDM_SUCCESS;

}


FIO_CODEC_FUNCTION(fio_ulaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	uint32_t i;
	ftdm_size_t len = *datalen;
	
	if (max > len) {
		max = len;
	}

	memcpy(law, data, max);

	for(i = 0; i < max; i++) {
		*sln++ = ulaw_to_linear(*lp++);
	}
	
	*datalen = max * 2;

	return FTDM_SUCCESS;
}

FIO_CODEC_FUNCTION(fio_slin2alaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	uint32_t i;
	ftdm_size_t len = *datalen;

	if (max > len) {
		max = len;
	}

	memcpy(sln, data, max);
	
	for(i = 0; i < max; i++) {
		*lp++ = linear_to_alaw(*sln++);
	}

	*datalen = max / 2;

	return FTDM_SUCCESS;

}


FIO_CODEC_FUNCTION(fio_alaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	uint32_t i;
	ftdm_size_t len = *datalen;
	
	if (max > len) {
		max = len;
	}

	memcpy(law, data, max);

	for(i = 0; i < max; i++) {
		*sln++ = alaw_to_linear(*lp++);
	}

	*datalen = max * 2;

	return FTDM_SUCCESS;
}

FIO_CODEC_FUNCTION(fio_ulaw2alaw)
{
	ftdm_size_t len = *datalen;
	uint32_t i;
	uint8_t *lp = data;

	if (max > len) {
        max = len;
    }

	for(i = 0; i < max; i++) {
		*lp = ulaw_to_alaw(*lp);
		lp++;
	}

	return FTDM_SUCCESS;
}

FIO_CODEC_FUNCTION(fio_alaw2ulaw)
{
	ftdm_size_t len = *datalen;
	uint32_t i;
	uint8_t *lp = data;

	if (max > len) {
        max = len;
    }

	for(i = 0; i < max; i++) {
		*lp = alaw_to_ulaw(*lp);
		lp++;
	}

	return FTDM_SUCCESS;
}

/******************************/

FT_DECLARE(void) ftdm_channel_clear_detected_tones(ftdm_channel_t *ftdmchan)
{
	uint32_t i;

	memset(ftdmchan->detected_tones, 0, sizeof(ftdmchan->detected_tones[0]) * FTDM_TONEMAP_INVALID);
	
	for (i = 1; i < FTDM_TONEMAP_INVALID; i++) {
		ftdmchan->span->tone_finder[i].tone_count = 0;
	}
}

FT_DECLARE(void) ftdm_channel_clear_needed_tones(ftdm_channel_t *ftdmchan)
{
	memset(ftdmchan->needed_tones, 0, sizeof(ftdmchan->needed_tones[0]) * FTDM_TONEMAP_INVALID);
}

FT_DECLARE(ftdm_size_t) ftdm_channel_dequeue_dtmf(ftdm_channel_t *ftdmchan, char *dtmf, ftdm_size_t len)
{
	ftdm_size_t bytes = 0;

	assert(ftdmchan != NULL);

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY)) {
		return 0;
	}

	if (ftdmchan->digit_buffer && ftdm_buffer_inuse(ftdmchan->digit_buffer)) {
		ftdm_mutex_lock(ftdmchan->mutex);
		if ((bytes = ftdm_buffer_read(ftdmchan->digit_buffer, dtmf, len)) > 0) {
			*(dtmf + bytes) = '\0';
		}
		ftdm_mutex_unlock(ftdmchan->mutex);
	}

	return bytes;
}

FT_DECLARE(void) ftdm_channel_flush_dtmf(ftdm_channel_t *ftdmchan)
{
	if (ftdmchan->digit_buffer && ftdm_buffer_inuse(ftdmchan->digit_buffer)) {
		ftdm_mutex_lock(ftdmchan->mutex);
		ftdm_buffer_zero(ftdmchan->digit_buffer);
		ftdm_mutex_unlock(ftdmchan->mutex);
	}
}

FT_DECLARE(ftdm_status_t) ftdm_channel_queue_dtmf(ftdm_channel_t *ftdmchan, const char *dtmf)
{
	ftdm_status_t status;
	register ftdm_size_t len, inuse;
	ftdm_size_t wr = 0;
	const char *p;
	
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "No channel\n");

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Queuing DTMF %s (debug = %d)\n", dtmf, ftdmchan->dtmfdbg.enabled);

	if (ftdmchan->span->sig_dtmf && (ftdmchan->span->sig_dtmf(ftdmchan, dtmf) == FTDM_BREAK)) {
		/* Signalling module wants to absorb this DTMF event */
		return FTDM_SUCCESS;
	}

	if (!ftdmchan->dtmfdbg.enabled) {
		goto skipdebug;
	}

	if (!ftdmchan->dtmfdbg.file) {
		struct tm currtime;
		time_t currsec;
		char dfile[1024];

		currsec = time(NULL);

#ifdef WIN32
		_tzset();
		_localtime64_s(&currtime, &currsec);
#else
		localtime_r(&currsec, &currtime);
#endif

		if (ftdm_strlen_zero(globals.dtmfdebug_directory)) {
			snprintf(dfile, sizeof(dfile), "dtmf-s%dc%d-20%d-%d-%d-%d%d%d.%s", 
					ftdmchan->span_id, ftdmchan->chan_id, 
					currtime.tm_year-100, currtime.tm_mon+1, currtime.tm_mday,
					currtime.tm_hour, currtime.tm_min, currtime.tm_sec, ftdmchan->native_codec == FTDM_CODEC_ULAW ? "ulaw" : ftdmchan->native_codec == FTDM_CODEC_ALAW ? "alaw" : "sln");
		} else {
			snprintf(dfile, sizeof(dfile), "%s/dtmf-s%dc%d-20%d-%d-%d-%d%d%d.%s", 
					globals.dtmfdebug_directory,
					ftdmchan->span_id, ftdmchan->chan_id, 
					currtime.tm_year-100, currtime.tm_mon+1, currtime.tm_mday,
					currtime.tm_hour, currtime.tm_min, currtime.tm_sec, ftdmchan->native_codec == FTDM_CODEC_ULAW ? "ulaw" : ftdmchan->native_codec == FTDM_CODEC_ALAW ? "alaw" : "sln");
		}
		ftdmchan->dtmfdbg.file = fopen(dfile, "wb");	
		if (!ftdmchan->dtmfdbg.file) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "failed to open debug dtmf file %s\n", dfile);
		} else {
			ftdmchan->dtmfdbg.closetimeout = DTMF_DEBUG_TIMEOUT;
			ftdm_channel_command(ftdmchan, FTDM_COMMAND_DUMP_INPUT, ftdmchan->dtmfdbg.file);
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Dumped initial DTMF output to %s\n", dfile);
		}
	} else {
		ftdmchan->dtmfdbg.closetimeout = DTMF_DEBUG_TIMEOUT;
	}

skipdebug:

	if (ftdmchan->pre_buffer) {
		ftdm_buffer_zero(ftdmchan->pre_buffer);
	}

	ftdm_mutex_lock(ftdmchan->mutex);

	inuse = ftdm_buffer_inuse(ftdmchan->digit_buffer);
	len = strlen(dtmf);
	
	if (len + inuse > ftdm_buffer_len(ftdmchan->digit_buffer)) {
		ftdm_buffer_toss(ftdmchan->digit_buffer, strlen(dtmf));
	}

	if (ftdmchan->span->dtmf_hangup_len) {
		for (p = dtmf; ftdm_is_dtmf(*p); p++) {
			memmove (ftdmchan->dtmf_hangup_buf, ftdmchan->dtmf_hangup_buf + 1, ftdmchan->span->dtmf_hangup_len - 1);
			ftdmchan->dtmf_hangup_buf[ftdmchan->span->dtmf_hangup_len - 1] = *p;
			if (!strcmp(ftdmchan->dtmf_hangup_buf, ftdmchan->span->dtmf_hangup)) {
				ftdm_log(FTDM_LOG_DEBUG, "DTMF hangup detected.\n");

				ftdm_channel_set_state(__FILE__, __FUNCTION__, __LINE__, ftdmchan, FTDM_CHANNEL_STATE_HANGUP, 0, NULL);
				break;
			}
		}
	}

	p = dtmf;
	while (wr < len && p) {
		if (ftdm_is_dtmf(*p)) {
			wr++;
		} else {
			break;
		}
		p++;
	}

	status = ftdm_buffer_write(ftdmchan->digit_buffer, dtmf, wr) ? FTDM_SUCCESS : FTDM_FAIL;
	ftdm_mutex_unlock(ftdmchan->mutex);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_raw_write (ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
{
	int dlen = (int) *datalen;

	if (ftdm_test_io_flag(ftdmchan, FTDM_CHANNEL_IO_WRITE)) {
		ftdm_clear_io_flag(ftdmchan, FTDM_CHANNEL_IO_WRITE);
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TX_DISABLED)) {
		ftdmchan->txdrops++;
		if (ftdmchan->txdrops <= 10) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "cannot write in channel with tx disabled\n");
		} 
		if (ftdmchan->txdrops == 10) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Too many tx drops, not printing anymore\n");
		}
		return FTDM_FAIL;
	}
	if (ftdmchan->fds[FTDM_WRITE_TRACE_INDEX] > -1) {
		if ((write(ftdmchan->fds[FTDM_WRITE_TRACE_INDEX], data, dlen)) != dlen) {
			ftdm_log(FTDM_LOG_WARNING, "Raw output trace failed to write all of the %d bytes\n", dlen);
		}
	}
	write_chan_io_dump(&ftdmchan->txdump, data, dlen);
	return ftdmchan->fio->write(ftdmchan, data, datalen);
}

FT_DECLARE(ftdm_status_t) ftdm_raw_read (ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
{
	ftdm_status_t  status;
	
	if (ftdm_test_io_flag(ftdmchan, FTDM_CHANNEL_IO_READ)) {
		ftdm_clear_io_flag(ftdmchan, FTDM_CHANNEL_IO_READ);
	}
	status = ftdmchan->fio->read(ftdmchan, data, datalen);

	if (status == FTDM_SUCCESS && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_USE_RX_GAIN)
	   && (ftdmchan->native_codec == FTDM_CODEC_ALAW || ftdmchan->native_codec == FTDM_CODEC_ULAW)) {
		ftdm_size_t i = 0;
		unsigned char *rdata = data;
		for (i = 0; i < *datalen; i++) {
			rdata[i] = ftdmchan->rxgain_table[rdata[i]];
		}
	}

	if (status == FTDM_SUCCESS && ftdmchan->fds[FTDM_READ_TRACE_INDEX] > -1) {
		ftdm_size_t dlen = *datalen;
		if ((ftdm_size_t)write(ftdmchan->fds[FTDM_READ_TRACE_INDEX], data, (int)dlen) != dlen) {
			ftdm_log(FTDM_LOG_WARNING, "Raw input trace failed to write all of the %"FTDM_SIZE_FMT" bytes\n", dlen);
		}
	}

	if (status == FTDM_SUCCESS && ftdmchan->span->sig_read) {
		ftdmchan->span->sig_read(ftdmchan, data, *datalen);
	}

	if (status == FTDM_SUCCESS) {
		ftdm_size_t dlen = *datalen;
		ftdm_size_t rc = 0;

		write_chan_io_dump(&ftdmchan->rxdump, data, (int)dlen);

		/* if dtmf debug is enabled and initialized, write there too */
		if (ftdmchan->dtmfdbg.file) {
			rc = fwrite(data, 1, dlen, ftdmchan->dtmfdbg.file);
			if (rc != dlen) {
				ftdm_log(FTDM_LOG_WARNING, "DTMF debugger wrote only %"FTDM_SIZE_FMT" out of %"FTDM_SIZE_FMT" bytes: %s\n",
					rc, *datalen, strerror(errno));
			}
			ftdmchan->dtmfdbg.closetimeout--;
			if (!ftdmchan->dtmfdbg.closetimeout) {
				close_dtmf_debug_file(ftdmchan);
			}
		}
	}
	return status;
}

/* This function takes care of automatically generating DTMF or FSK tones when needed */
static ftdm_status_t handle_tone_generation(ftdm_channel_t *ftdmchan)
{
	/*
	 * datalen: size in bytes of the chunk of data the user requested to read (this function 
	 *          is called from the ftdm_channel_read function)
	 * dblen: size currently in use in any of the tone generation buffers (data available in the buffer)
	 * gen_dtmf_buffer: buffer holding the raw ASCII digits that the user requested to generate
	 * dtmf_buffer: raw linear tone data generated by teletone to be written to the devices
	 * fsk_buffer: raw linear FSK modulated data for caller id
	 */
	ftdm_buffer_t *buffer = NULL;
	ftdm_size_t dblen = 0;
	int wrote = 0;

	if (ftdmchan->gen_dtmf_buffer && (dblen = ftdm_buffer_inuse(ftdmchan->gen_dtmf_buffer))) {
		char digits[128] = "";
		char *cur;
		int x = 0;				 
		
		if (dblen > sizeof(digits) - 1) {
			dblen = sizeof(digits) - 1;
		}

		if (ftdm_buffer_read(ftdmchan->gen_dtmf_buffer, digits, dblen) && !ftdm_strlen_zero_buf(digits)) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generating DTMF [%s]\n", digits);

			cur = digits;

			for (; *cur; cur++) {
				if (*cur == 'F') {
					ftdm_channel_command(ftdmchan, FTDM_COMMAND_FLASH, NULL);
				} else if (*cur == 'w') {
					ftdm_insert_dtmf_pause(ftdmchan, FTDM_HALF_DTMF_PAUSE);
				} else if (*cur == 'W') {
					ftdm_insert_dtmf_pause(ftdmchan, FTDM_FULL_DTMF_PAUSE);
				} else {
					if ((wrote = teletone_mux_tones(&ftdmchan->tone_session, &ftdmchan->tone_session.TONES[(int)*cur]))) {
						ftdm_buffer_write(ftdmchan->dtmf_buffer, ftdmchan->tone_session.buffer, wrote * 2);
						x++;
					} else {
						ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Problem adding DTMF sequence [%s]\n", digits);
						return FTDM_FAIL;
					}
				}
				if (x) {
					ftdmchan->skip_read_frames = (wrote / (ftdmchan->effective_interval * 8)) + 4;
				}
			}
		}
	}

	if (!ftdmchan->buffer_delay || --ftdmchan->buffer_delay == 0) {
		/* time to pick a buffer, either the dtmf or fsk buffer */
		if (ftdmchan->dtmf_buffer && (dblen = ftdm_buffer_inuse(ftdmchan->dtmf_buffer))) {
			buffer = ftdmchan->dtmf_buffer;
		} else if (ftdmchan->fsk_buffer && (dblen = ftdm_buffer_inuse(ftdmchan->fsk_buffer))) {
			buffer = ftdmchan->fsk_buffer;			
		}
	}

	/* if we picked a buffer, time to read from it and write the linear data to the device */
	if (buffer) {
		uint8_t auxbuf[1024];
		ftdm_size_t dlen = ftdmchan->packet_len;
		ftdm_size_t len, br, max = sizeof(auxbuf);
		
		/* if the codec is not linear, then data is really twice as much cuz
		   tone generation is done in linear (we assume anything different than linear is G.711) */
		if (ftdmchan->native_codec != FTDM_CODEC_SLIN) {
			dlen *= 2;
		}

		/* we do not expect the user chunks to be bigger than auxbuf */
		ftdm_assert((dlen <= sizeof(auxbuf)), "Unexpected size for user data chunk size\n");

		/* dblen is the size in use for dtmf_buffer or fsk_buffer, and dlen is the size
		 * of the io chunks to write, we pick the smaller one */
		len = dblen > dlen ? dlen : dblen;

		/* we can't read more than the size of our auxiliary buffer */
		ftdm_assert((len <= sizeof(auxbuf)), "Unexpected size to read into auxbuf\n");

		br = ftdm_buffer_read(buffer, auxbuf, len);		

		/* the amount read can't possibly be bigger than what we requested */
		ftdm_assert((br <= len), "Unexpected size read from tone generation buffer\n");

		/* if we read less than the chunk size, we must fill in with silence the rest */
		if (br < dlen) {
			memset(auxbuf + br, 0, dlen - br);
		}

		/* finally we convert to the native format for the channel if necessary */
		if (ftdmchan->native_codec != FTDM_CODEC_SLIN) {
			if (ftdmchan->native_codec == FTDM_CODEC_ULAW) {
				fio_slin2ulaw(auxbuf, max, &dlen);
			} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW) {
				fio_slin2alaw(auxbuf, max, &dlen);
			}
		}
		
		/* write the tone to the channel */
		return ftdm_raw_write(ftdmchan, auxbuf, &dlen);
	} 

	return FTDM_SUCCESS;

}


FT_DECLARE(void) ftdm_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor)
{
    int16_t x;
    uint32_t i;
    int sum_rnd = 0;
    int16_t rnd2 = (int16_t) ftdm_current_time_in_ms() * (int16_t) (intptr_t) data;

    assert(divisor);

    for (i = 0; i < samples; i++, sum_rnd = 0) {
        for (x = 0; x < 6; x++) {
            rnd2 = rnd2 * 31821U + 13849U;
            sum_rnd += rnd2 ;
        }
        //switch_normalize_to_16bit(sum_rnd);
        *data = (int16_t) ((int16_t) sum_rnd / (int) divisor);

        data++;
    }
}

FT_DECLARE(ftdm_status_t) ftdm_channel_process_media(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
{
	fio_codec_t codec_func = NULL;
	ftdm_size_t max = *datalen;

	handle_tone_generation(ftdmchan);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_DIGITAL_MEDIA)) {
		goto done;
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE) && ftdmchan->effective_codec != ftdmchan->native_codec) {
		if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_ulaw2slin;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_ALAW) {
			codec_func = fio_ulaw2alaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_alaw2slin;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_ULAW) {
			codec_func = fio_alaw2ulaw;
		}

		if (codec_func) {
			codec_func(data, max, datalen);
		} else {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "no codec function to perform transcoding from %d to %d\n", ftdmchan->native_codec, ftdmchan->effective_codec);
		}
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT) ||
		ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT) ||
		ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT)) {

		uint8_t sln_buf[1024] = {0};
		int16_t *sln;
		ftdm_size_t slen = 0;

		if (ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			sln = data;
			slen = *datalen / 2;
		} else {
			ftdm_size_t len = *datalen;
			uint32_t i;
			uint8_t *lp = data;

			slen = sizeof(sln_buf) / 2;
			if (len > slen) {
				len = slen;
			}

			sln = (int16_t *) sln_buf;
			for(i = 0; i < len; i++) {
				if (ftdmchan->effective_codec == FTDM_CODEC_ULAW) {
					*sln++ = ulaw_to_linear(*lp++);
				} else if (ftdmchan->effective_codec == FTDM_CODEC_ALAW) {
					*sln++ = alaw_to_linear(*lp++);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
					ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "invalid effective codec %d\n", ftdmchan->effective_codec);
					goto done;
				}
			}
			sln = (int16_t *) sln_buf;
			slen = len;
		}

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT)) {
			if (ftdm_fsk_demod_feed(&ftdmchan->fsk, sln, slen) != FTDM_SUCCESS) {
				ftdm_size_t type, mlen;
				char str[128], *sp;
				
				while(ftdm_fsk_data_parse(&ftdmchan->fsk, &type, &sp, &mlen) == FTDM_SUCCESS) {
					*(str+mlen) = '\0';
					ftdm_copy_string(str, sp, ++mlen);
					ftdm_clean_string(str);

					ftdm_log(FTDM_LOG_DEBUG, "FSK: TYPE %s LEN %"FTDM_SIZE_FMT" VAL [%s]\n",
						ftdm_mdmf_type2str(type), mlen-1, str);
					
					switch(type) {
					case MDMF_DDN:
					case MDMF_PHONE_NUM:
						{
							if (mlen > sizeof(ftdmchan->caller_data.ani)) {
								mlen = sizeof(ftdmchan->caller_data.ani);
							}
							ftdm_set_string(ftdmchan->caller_data.ani.digits, str);
							ftdm_set_string(ftdmchan->caller_data.cid_num.digits, ftdmchan->caller_data.ani.digits);
						}
						break;
					case MDMF_NO_NUM:
						{
							ftdm_set_string(ftdmchan->caller_data.ani.digits, *str == 'P' ? "private" : "unknown");
							ftdm_set_string(ftdmchan->caller_data.cid_name, ftdmchan->caller_data.ani.digits);
						}
						break;
					case MDMF_PHONE_NAME:
						{
							if (mlen > sizeof(ftdmchan->caller_data.cid_name)) {
								mlen = sizeof(ftdmchan->caller_data.cid_name);
							}
							ftdm_set_string(ftdmchan->caller_data.cid_name, str);
						}
						break;
					case MDMF_NO_NAME:
						{
							ftdm_set_string(ftdmchan->caller_data.cid_name, *str == 'P' ? "private" : "unknown");
						}
					case MDMF_DATETIME:
						{
							if (mlen > sizeof(ftdmchan->caller_data.cid_date)) {
								mlen = sizeof(ftdmchan->caller_data.cid_date);
							}
							ftdm_set_string(ftdmchan->caller_data.cid_date, str);
						}
						break;
					}
				}
				ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_CALLERID_DETECT, NULL);
			}
		}

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT) && !ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_PROGRESS)) {
			uint32_t i;

			for (i = 1; i < FTDM_TONEMAP_INVALID; i++) {
				if (ftdmchan->span->tone_finder[i].tone_count) {
					if (ftdmchan->needed_tones[i] && teletone_multi_tone_detect(&ftdmchan->span->tone_finder[i], sln, (int)slen)) {
						if (++ftdmchan->detected_tones[i]) {
							ftdmchan->needed_tones[i] = 0;
							ftdmchan->detected_tones[0]++;
						}
					}
				}
			}
		}

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT) && !ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT)) {
			teletone_hit_type_t hit;
			char digit_char;
			uint32_t dur;

			if ((hit = teletone_dtmf_detect(&ftdmchan->dtmf_detect, sln, (int)slen)) == TT_HIT_END) {
				teletone_dtmf_get(&ftdmchan->dtmf_detect, &digit_char, &dur);

				if (ftdmchan->state == FTDM_CHANNEL_STATE_CALLWAITING && (digit_char == 'D' || digit_char == 'A')) {
					ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK]++;
				} else {
					char digit_str[2] = { 0 };

					digit_str[0] = digit_char;

					ftdm_channel_queue_dtmf(ftdmchan, digit_str);

					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF)) {
						ftdmchan->skip_read_frames = 20;
					}
				}
			}
		}
	}

	if (ftdmchan->skip_read_frames > 0 || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_MUTE)) {

		ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
		if (ftdmchan->pre_buffer && ftdm_buffer_inuse(ftdmchan->pre_buffer)) {
			ftdm_buffer_zero(ftdmchan->pre_buffer);
		}
		ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

		memset(data, FTDM_SILENCE_VALUE(ftdmchan), *datalen);

		if (ftdmchan->skip_read_frames > 0) {
			ftdmchan->skip_read_frames--;
		}
	} else {
		ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
		if (ftdmchan->pre_buffer_size && ftdmchan->pre_buffer) {
			ftdm_buffer_write(ftdmchan->pre_buffer, data, *datalen);
			if (ftdm_buffer_inuse(ftdmchan->pre_buffer) >= ftdmchan->pre_buffer_size) {
				ftdm_buffer_read(ftdmchan->pre_buffer, data, *datalen);
			} else {
				memset(data, FTDM_SILENCE_VALUE(ftdmchan), *datalen);
			}
		}
		ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);
	}

done:
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_channel_read(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
{

	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "ftdmchan is null\n");
	ftdm_assert_return(ftdmchan->fio != NULL, FTDM_FAIL, "No I/O module attached to ftdmchan\n");

	ftdm_channel_lock(ftdmchan);

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "cannot read from channel that is not open\n");
		status = FTDM_FAIL;
		goto done;
	}

	if (!ftdmchan->fio->read) {		
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "read method not implemented\n");
		status = FTDM_FAIL;
		goto done;
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RX_DISABLED)) {
		ftdmchan->rxdrops++;
		if (ftdmchan->rxdrops <= 10) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "cannot read from channel with rx disabled\n");
		}
		if (ftdmchan->rxdrops == 10) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "too many rx drops, not logging anymore\n");
		}
		status = FTDM_FAIL;
		goto done;
	}
	status = ftdm_raw_read(ftdmchan, data, datalen);
	if (status != FTDM_SUCCESS) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "raw I/O read filed\n");
		goto done;
	}

	status = ftdm_channel_process_media(ftdmchan, data, datalen);
	if (status != FTDM_SUCCESS) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Failed to process media\n");
	}
done:
	ftdm_channel_unlock(ftdmchan);
	return status;
}


FT_DECLARE(ftdm_status_t) ftdm_channel_write(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t datasize, ftdm_size_t *datalen)
{
	ftdm_status_t status = FTDM_SUCCESS;
	fio_codec_t codec_func = NULL;
	ftdm_size_t max = datasize;
	unsigned int i = 0;

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "null channel on write!\n");
	ftdm_assert_return(ftdmchan->fio != NULL, FTDM_FAIL, "null I/O on write!\n");

	ftdm_channel_lock(ftdmchan);

	if (!ftdmchan->buffer_delay && 
		((ftdmchan->dtmf_buffer && ftdm_buffer_inuse(ftdmchan->dtmf_buffer)) ||
		 (ftdmchan->fsk_buffer && ftdm_buffer_inuse(ftdmchan->fsk_buffer)))) {
		/* generating some kind of tone at the moment (see handle_tone_generation), 
		 * we ignore user data ... */
		goto done;
	}


	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "cannot write in channel not open\n");
		status = FTDM_FAIL;
		goto done;
	}

	if (!ftdmchan->fio->write) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "write method not implemented\n");
		status = FTDM_FAIL;
		goto done;
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_DIGITAL_MEDIA)) {
		goto do_write;
	}
	
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE) && ftdmchan->effective_codec != ftdmchan->native_codec) {
		if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_slin2ulaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_ALAW) {
			codec_func = fio_alaw2ulaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_slin2alaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_ULAW) {
			codec_func = fio_ulaw2alaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Do not know how to handle transcoding from %d to %d\n", 
					ftdmchan->effective_codec, ftdmchan->native_codec);			
			status = FTDM_FAIL;
			goto done;
		}
	}
	
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_USE_TX_GAIN) 
		&& (ftdmchan->native_codec == FTDM_CODEC_ALAW || ftdmchan->native_codec == FTDM_CODEC_ULAW)) {
		unsigned char *wdata = data;
		for (i = 0; i < *datalen; i++) {
			wdata[i] = ftdmchan->txgain_table[wdata[i]];
		}
	}

do_write:

	if (ftdmchan->span->sig_write) {
		status = ftdmchan->span->sig_write(ftdmchan, data, *datalen);
		if (status == FTDM_BREAK) {
			/* signaling module decided to drop user frame */
			status = FTDM_SUCCESS;
			goto done;
		}
	}

	status = ftdm_raw_write(ftdmchan, data, datalen);

done:
	ftdm_channel_unlock(ftdmchan);

	return status;
}

FT_DECLARE(ftdm_iterator_t *) ftdm_get_iterator(ftdm_iterator_type_t type, ftdm_iterator_t *iter)
{
	int allocated = 0;
	if (iter) {
		if (iter->type != type) {
			ftdm_log(FTDM_LOG_ERROR, "Cannot switch iterator types\n");
			return NULL;
		}
		allocated = iter->allocated;
		memset(iter, 0, sizeof(*iter));
		iter->type = type;
		iter->allocated = allocated;
		return iter;
	}

	iter = ftdm_calloc(1, sizeof(*iter));
	if (!iter) {
		return NULL;
	}
	iter->type = type;
	iter->allocated = 1;
	return iter;
}

FT_DECLARE(ftdm_iterator_t *) ftdm_get_span_iterator(ftdm_iterator_t *iter)
{
	if (!(iter = ftdm_get_iterator(FTDM_ITERATOR_SPANS, iter))) {
		return NULL;
	}

	iter->pvt.hashiter = hashtable_first(globals.span_hash);
	return iter;
}

FT_DECLARE(ftdm_iterator_t *) ftdm_span_get_chan_iterator(const ftdm_span_t *span, ftdm_iterator_t *iter)
{
	if (!span->chan_count) {
		return NULL;
	}
	if (!(iter = ftdm_get_iterator(FTDM_ITERATOR_CHANS, iter))) {
		return NULL;
	}
	iter->pvt.chaniter.index = 1;
	iter->pvt.chaniter.span = span;
	return iter;
}

FT_DECLARE(ftdm_iterator_t *) ftdm_iterator_next(ftdm_iterator_t *iter)
{
	ftdm_assert_return(iter && iter->type, NULL, "Invalid iterator\n");

	switch (iter->type) {
	case FTDM_ITERATOR_VARS:
	case FTDM_ITERATOR_SPANS:
		if (!iter->pvt.hashiter) {
			return NULL;
		}
		iter->pvt.hashiter = hashtable_next(iter->pvt.hashiter);
		if (!iter->pvt.hashiter) {
			return NULL;
		}
		return iter;
	case FTDM_ITERATOR_CHANS:
		ftdm_assert_return(iter->pvt.chaniter.index, NULL, "channel iterator index cannot be zero!\n");
		if (iter->pvt.chaniter.index == iter->pvt.chaniter.span->chan_count) {
			return NULL;
		}
		iter->pvt.chaniter.index++;
		return iter;
	default:
		break;
	}

	ftdm_assert_return(0, NULL, "Unknown iterator type\n");
	return NULL;
}

FT_DECLARE(void *) ftdm_iterator_current(ftdm_iterator_t *iter)
{
	const void *key = NULL;
	void *val = NULL;

	ftdm_assert_return(iter && iter->type, NULL, "Invalid iterator\n");

	switch (iter->type) {
	case FTDM_ITERATOR_VARS:
		hashtable_this(iter->pvt.hashiter, &key, NULL, &val);
		/* I decided to return the key instead of the value since the value can be retrieved using the key */
		return (void *)key;
	case FTDM_ITERATOR_SPANS:
		hashtable_this(iter->pvt.hashiter, &key, NULL, &val);
		return (void *)val;
	case FTDM_ITERATOR_CHANS:
		ftdm_assert_return(iter->pvt.chaniter.index, NULL, "channel iterator index cannot be zero!\n");
		ftdm_assert_return(iter->pvt.chaniter.index <= iter->pvt.chaniter.span->chan_count, NULL, "channel iterator index bigger than span chan count!\n");
		return iter->pvt.chaniter.span->channels[iter->pvt.chaniter.index];
	default:
		break;
	}

	ftdm_assert_return(0, NULL, "Unknown iterator type\n");
	return NULL;
}

FT_DECLARE(ftdm_status_t) ftdm_iterator_free(ftdm_iterator_t *iter)
{
	/* it's valid to pass a NULL iterator, do not return failure  */
	if (!iter) {
		return FTDM_SUCCESS;
	}

	if (!iter->allocated) {
		memset(iter, 0, sizeof(*iter));
		return FTDM_SUCCESS;
	}

	ftdm_assert_return(iter->type, FTDM_FAIL, "Cannot free invalid iterator\n");
	ftdm_safe_free(iter);

	return FTDM_SUCCESS;
}


static const char *print_neg_char[] = { "", "!" };
static const char *print_flag_state[] = { "OFF", "ON" };

static void print_channels_by_flag(ftdm_stream_handle_t *stream, ftdm_span_t *inspan, uint32_t inchan_id, uint64_t flagval, int not, int *count)
{
	ftdm_bool_t neg = !!not;
	const char *negind = print_neg_char[neg];
	const char *flagname;
	uint64_t flag = ((uint64_t)1 << flagval);
	int mycount = 0;

	flagname = ftdm_val2str(flag, channel_flag_strs, ftdm_array_len(channel_flag_strs), "invalid");

	ftdm_mutex_lock(globals.mutex);

	if (inspan) {
		ftdm_iterator_t *c_iter, *c_cur;

		c_iter = ftdm_span_get_chan_iterator(inspan, NULL);

		for (c_cur = c_iter; c_cur; c_cur = ftdm_iterator_next(c_cur)) {
			ftdm_channel_t *fchan;
			ftdm_bool_t cond;

			fchan = ftdm_iterator_current(c_cur);
			if (inchan_id && inchan_id != fchan->chan_id) {
				continue;
			}

			cond = !!ftdm_test_flag(fchan, flag);
			if (neg ^ cond) {
				mycount++;
			}

			stream->write_function(stream, "[s%dc%d][%d:%d] flag %s%"FTDM_UINT64_FMT"(%s%s) %s\n",
							fchan->span_id, fchan->chan_id,
							fchan->physical_span_id, fchan->physical_chan_id,
							negind, flagval, negind, flagname,
							print_flag_state[cond]);
		}

		ftdm_iterator_free(c_iter);

	} else {
		ftdm_iterator_t *s_iter, *s_cur;

		s_iter = ftdm_get_span_iterator(NULL);

		for (s_cur = s_iter; s_cur; s_cur = ftdm_iterator_next(s_cur)) {
			ftdm_iterator_t *c_iter, *c_cur;
			ftdm_span_t *span;

			span = ftdm_iterator_current(s_cur);
			if (!span) {
				break;
			}

			c_iter = ftdm_span_get_chan_iterator(span, NULL);

			for (c_cur = c_iter; c_cur; c_cur = ftdm_iterator_next(c_cur)) {
				ftdm_channel_t *fchan;

				fchan = ftdm_iterator_current(c_cur);

				if (neg ^ !!ftdm_test_flag(fchan, flag)) {
					stream->write_function(stream, "[s%dc%d][%d:%d] flag %s%"FTDM_UINT64_FMT"(%s%s)\n",
									fchan->span_id, fchan->chan_id,
									fchan->physical_span_id, fchan->physical_chan_id,
									negind, flagval, negind, flagname);
					mycount++;
				}
			}

			ftdm_iterator_free(c_iter);
		}

		ftdm_iterator_free(s_iter);
	}

	*count = mycount;
	ftdm_mutex_unlock(globals.mutex);
}

static void print_spans_by_flag(ftdm_stream_handle_t *stream, ftdm_span_t *inspan, uint64_t flagval, int not, int *count)
{
	ftdm_bool_t neg = !!not;
	const char *negind = print_neg_char[neg];
	const char *flagname;
	uint64_t flag = ((uint64_t)1 << flagval);
	int mycount = 0;

	flagname = ftdm_val2str(flag, span_flag_strs, ftdm_array_len(span_flag_strs), "invalid");

	ftdm_mutex_lock(globals.mutex);

	if (inspan) {
		ftdm_bool_t cond;

		cond = !!ftdm_test_flag(inspan, flag);
		if (neg ^ cond) {
			mycount++;
		}

		stream->write_function(stream, "[s%d] flag %s%"FTDM_UINT64_FMT"(%s%s) %s\n",
						inspan->span_id, negind, flagval, negind, flagname,
						print_flag_state[cond]);
	} else {
		ftdm_iterator_t *s_iter, *s_cur;

		s_iter = ftdm_get_span_iterator(NULL);

		for (s_cur = s_iter; s_cur; s_cur = ftdm_iterator_next(s_cur)) {
			ftdm_span_t *span;

			span = ftdm_iterator_current(s_cur);
			if (!span) {
				break;
			}

			if (neg ^ !!ftdm_test_flag(span, flag)) {
				stream->write_function(stream, "[s%d] flag %s%"FTDM_UINT64_FMT"(%s%s)\n",
								span->span_id, negind, flagval, negind, flagname);
				mycount++;
			}
		}

		ftdm_iterator_free(s_iter);
	}

	*count = mycount;
	ftdm_mutex_unlock(globals.mutex);
}

static void print_channels_by_state(ftdm_stream_handle_t *stream, ftdm_channel_state_t state, int not, int *count)
{
	ftdm_iterator_t *s_iter, *s_cur;
	ftdm_bool_t neg = !!not;
	int mycount = 0;

	s_iter = ftdm_get_span_iterator(NULL);

	ftdm_mutex_lock(globals.mutex);

	for (s_cur = s_iter; s_cur; s_cur = ftdm_iterator_next(s_cur)) {
		ftdm_iterator_t *c_iter, *c_cur;
		ftdm_span_t *span;

		span = ftdm_iterator_current(s_cur);
		if (!span) {
			break;
		}

		c_iter = ftdm_span_get_chan_iterator(span, NULL);

		for (c_cur = c_iter ; c_cur; c_cur = ftdm_iterator_next(c_cur)) {
			ftdm_channel_t *fchan = ftdm_iterator_current(c_cur);

			if (neg ^ (fchan->state == state)) {
				stream->write_function(stream, "[s%dc%d][%d:%d] in state %s\n",
						fchan->span_id, fchan->chan_id,
						fchan->physical_span_id, fchan->physical_chan_id, ftdm_channel_state2str(fchan->state));
				mycount++;
			}
		}

		ftdm_iterator_free(c_iter);
	}

	*count = mycount;
	ftdm_mutex_unlock(globals.mutex);

	ftdm_iterator_free(s_iter);
}

static void print_core_usage(ftdm_stream_handle_t *stream)
{
	stream->write_function(stream, 
	"--------------------------------------------------------------------------------\n"
	"ftdm core state [!]<state-name> - List all channels in or not in the given state\n"
	"ftdm core flag [!]<flag-int-value|flag-name> [<span_id|span_name>] [<chan_id>] - List all channels with the given flag value set\n"
	"ftdm core spanflag [!]<flag-int-value|flag-name> [<span_id|span_name>] - List all spans with the given span flag value set\n"
	"ftdm core calls - List all known calls to the FreeTDM core\n"
	"--------------------------------------------------------------------------------\n");
}


static unsigned long long ftdm_str2val(const char *str, val_str_t *val_str_table, ftdm_size_t array_size, unsigned long long default_val)
{
	ftdm_size_t i;
	for (i = 0; i < array_size; i++) {
		if (!strcasecmp(val_str_table[i].str, str)) {
			return val_str_table[i].val;
		}
	}
	return default_val;
}

static const char *ftdm_val2str(unsigned long long val, val_str_t *val_str_table, ftdm_size_t array_size, const char *default_str)
{
	ftdm_size_t i;
	for (i = 0; i < array_size; i++) {
		if (val_str_table[i].val == val) {
			return val_str_table[i].str;
		}
	}
	return default_str;
}

static void print_channel_flag_values(ftdm_stream_handle_t *stream)
{
	int i;
	for (i = 0; i < ftdm_array_len(channel_flag_strs); i++) {
		stream->write_function(stream, "%s\n", channel_flag_strs[i].str);
	}
}

static void print_span_flag_values(ftdm_stream_handle_t *stream)
{
	int i;
	for (i = 0; i < ftdm_array_len(span_flag_strs); i++) {
		stream->write_function(stream, "%s\n", span_flag_strs[i].str);
	}
}

/**
 * Compute log2 of 64bit integer v
 *
 * Bit Twiddling Hacks
 * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
 */
static int ftdm_log2_64(uint64_t v)
{
	unsigned int shift;
	uint64_t r;

	r =     (v > 0xFFFFFFFF) << 5; v >>= r;
	shift = (v > 0xFFFF    ) << 4; v >>= shift; r |= shift;
	shift = (v > 0xFF      ) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF       ) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3       ) << 1; v >>= shift; r |= shift;

	return ((int)(r | (v >> 1)));
}

static char *handle_core_command(const char *cmd)
{
	char *mycmd = NULL;
	int argc = 0;
	int count = 0;
	int not = 0;
	char *argv[10] = { 0 };
	char *flag = NULL;
	uint64_t flagval = 0;
	ftdm_channel_t *fchan = NULL;
	ftdm_span_t *fspan = NULL;
	ftdm_stream_handle_t stream = { 0 };

	FTDM_STANDARD_STREAM(stream);

	if (!ftdm_strlen_zero(cmd)) {
		mycmd = ftdm_strdup(cmd);
		argc = ftdm_separate_string(mycmd, ' ', argv, ftdm_array_len(argv));
	} else {
		print_core_usage(&stream);
		goto done;
	}

	if (!argc) {
		print_core_usage(&stream);
		goto done;
	}

	if (!strcasecmp(argv[0], "state")) {
		ftdm_channel_state_t st = FTDM_CHANNEL_STATE_INVALID;
		char *state = NULL;

		if (argc < 2) {
			stream.write_function(&stream, "core state command requires an argument\n");
			print_core_usage(&stream);
			goto done;
		}

		state = argv[1];
		if (state[0] == '!') {
			not = 1;
			state++;
		}

		for (st = FTDM_CHANNEL_STATE_DOWN; st < FTDM_CHANNEL_STATE_INVALID; st++) {
			if (!strcasecmp(state, ftdm_channel_state2str(st))) {
				break;
			}
		}
		if (st == FTDM_CHANNEL_STATE_INVALID) {
			stream.write_function(&stream, "invalid state %s\n", state);
			goto done;
		}
		print_channels_by_state(&stream, st, not, &count);
		stream.write_function(&stream, "\nTotal channels %s state %s: %d\n",
						not ? "not in" : "in", ftdm_channel_state2str(st), count);
	} else if (!strcasecmp(argv[0], "flag")) {
		uint32_t chan_id = 0;

		if (argc < 2) {
			stream.write_function(&stream, "core flag command requires an argument\n");
			print_core_usage(&stream);
			goto done;
		}

		flag = argv[1];
		if (flag[0] == '!') {
			not = 1;
			flag++;
		}

		if (isalpha(flag[0])) {
			flagval = ftdm_str2val(flag, channel_flag_strs, ftdm_array_len(channel_flag_strs), FTDM_CHANNEL_MAX_FLAG);
			if (flagval == FTDM_CHANNEL_MAX_FLAG) {
				stream.write_function(&stream, "\nInvalid channel flag value. Possible channel flags:\n");
				print_channel_flag_values(&stream);
				goto done;
			}
			flagval = ftdm_log2_64(flagval);
		} else {
			flagval = atoi(flag);
		}

		/* Specific span specified */
		if (argv[2]) {
			ftdm_span_find_by_name(argv[2], &fspan);
			if (!fspan) {
				stream.write_function(&stream, "-ERR span:%s not found\n", argv[2]);
				goto done;
			}
		}

		/* Specific channel specified */
		if (argv[3]) {
			chan_id = atoi(argv[3]);
			if (chan_id == 0 || chan_id >= ftdm_span_get_chan_count(fspan)) {
				stream.write_function(&stream, "-ERR invalid channel %u\n", chan_id);
				goto done;
			}
		}

		print_channels_by_flag(&stream, fspan, chan_id, flagval, not, &count);
		stream.write_function(&stream, "\nTotal channels %s flag %"FTDM_UINT64_FMT": %d\n", not ? "without" : "with", flagval, count);
	} else if (!strcasecmp(argv[0], "spanflag")) {
		if (argc < 2) {
			stream.write_function(&stream, "core spanflag command requires an argument\n");
			print_core_usage(&stream);
			goto done;
		}

		flag = argv[1];
		if (flag[0] == '!') {
			not = 1;
			flag++;
		}

		if (isalpha(flag[0])) {
			flagval = ftdm_str2val(flag, span_flag_strs, ftdm_array_len(span_flag_strs), FTDM_SPAN_MAX_FLAG);
			if (flagval == FTDM_SPAN_MAX_FLAG) {
				stream.write_function(&stream, "\nInvalid span flag value. Possible span flags\n");
				print_span_flag_values(&stream);
				goto done;
			}
			flagval = ftdm_log2_64(flagval);
		} else {
			flagval = atoi(flag);
		}

		/* Specific span specified */
		if (argv[2]) {
			ftdm_span_find_by_name(argv[2], &fspan);
			if (!fspan) {
				stream.write_function(&stream, "-ERR span:%s not found\n", argv[2]);
				goto done;
			}
		}

		print_spans_by_flag(&stream, fspan, flagval, not, &count);
		if (!fspan) {
			stream.write_function(&stream, "\nTotal spans %s flag %"FTDM_UINT64_FMT": %d\n", not ? "without" : "with", flagval, count);
		}
	} else if (!strcasecmp(argv[0], "calls")) {
		uint32_t current_call_id = 0;

		ftdm_mutex_lock(globals.call_id_mutex);
		for (current_call_id = 0; current_call_id <= MAX_CALLIDS; current_call_id++) {
			ftdm_caller_data_t *calldata = NULL;

			if (!globals.call_ids[current_call_id]) {
				continue;
			}

			calldata = globals.call_ids[current_call_id];
			fchan = calldata->fchan;
			if (fchan) {
				stream.write_function(&stream, "Call %u on channel %d:%d\n", current_call_id,
						fchan->span_id, fchan->chan_id);
			} else {
				stream.write_function(&stream, "Call %u without a channel?\n", current_call_id);
			}
			count++;
		}
		ftdm_mutex_unlock(globals.call_id_mutex);
		stream.write_function(&stream, "\nTotal calls: %d\n", count);
	} else {
		stream.write_function(&stream, "invalid core command %s\n", argv[0]);
		print_core_usage(&stream);
	}

done:
	ftdm_safe_free(mycmd);

	return stream.data;
}

FT_DECLARE(char *) ftdm_api_execute(const char *cmd)
{
	ftdm_io_interface_t *fio = NULL;
	char *dup = NULL, *p;
	char *rval = NULL;
	char *type = NULL;

	dup = ftdm_strdup(cmd);
	if ((p = strchr(dup, ' '))) {
		*p++ = '\0';
		cmd = p;
	} else {
		cmd = "";
	}

	type = dup;

	if (!strcasecmp(type, "core")) {
		return handle_core_command(cmd);
	}

	fio = ftdm_global_get_io_interface(type, FTDM_TRUE);
	if (fio && fio->api) {
		ftdm_stream_handle_t stream = { 0 };
		ftdm_status_t status;
		FTDM_STANDARD_STREAM(stream);

		status = fio->api(&stream, cmd);
		if (status != FTDM_SUCCESS) {
			ftdm_safe_free(stream.data);
		} else {
			rval = (char *) stream.data;
		}
	}

	ftdm_safe_free(dup);

	return rval;
}

static ftdm_status_t ftdm_set_channels_gains(ftdm_span_t *span, int currindex, float rxgain, float txgain)
{
	unsigned chan_index = 0;

	if (!span->chan_count) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to set channel gains because span %s has no channels\n", span->name);
		return FTDM_FAIL;
	}

	for (chan_index = currindex+1; chan_index <= span->chan_count; chan_index++) {
		if (!FTDM_IS_VOICE_CHANNEL(span->channels[chan_index])) {
			continue;
		}
		if (ftdm_channel_command(span->channels[chan_index], FTDM_COMMAND_SET_RX_GAIN, &rxgain) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
		if (ftdm_channel_command(span->channels[chan_index], FTDM_COMMAND_SET_TX_GAIN, &txgain) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
	}
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_report_initial_channels_alarms(ftdm_span_t *span) 
{
	ftdm_channel_t *fchan = NULL;
	ftdm_iterator_t *curr = NULL;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_alarm_flag_t alarmbits;
	ftdm_event_t fake_event;
	ftdm_iterator_t *citer = ftdm_span_get_chan_iterator(span, NULL);

	if (!citer) {
		status = FTDM_ENOMEM;
		goto done;
	}

	memset(&fake_event, 0, sizeof(fake_event));
	fake_event.e_type = FTDM_EVENT_OOB;

	for (curr = citer; curr; curr = ftdm_iterator_next(curr)) {
		fchan = ftdm_iterator_current(curr);
		status = ftdm_channel_get_alarms(fchan, &alarmbits);
		if (status != FTDM_SUCCESS) {
			ftdm_log_chan_msg(fchan, FTDM_LOG_ERROR, "Failed to initialize alarms\n");
			continue;
		}
		fake_event.channel = fchan;
		fake_event.enum_id = fchan->alarm_flags ? FTDM_OOB_ALARM_TRAP : FTDM_OOB_ALARM_CLEAR;
		ftdm_event_handle_oob(&fake_event);
	}

done:

	ftdm_iterator_free(citer);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_configure_span_channels(ftdm_span_t *span, const char* str, ftdm_channel_config_t *chan_config, unsigned *configured)
{
	int currindex;
	unsigned chan_index = 0;

	ftdm_assert_return(span != NULL, FTDM_EINVAL, "span is null\n");
	ftdm_assert_return(chan_config != NULL, FTDM_EINVAL, "config is null\n");
	ftdm_assert_return(configured != NULL, FTDM_EINVAL, "configured pointer is null\n");
	ftdm_assert_return(span->fio != NULL, FTDM_EINVAL, "span with no I/O configured\n");
	ftdm_assert_return(span->fio->configure_span != NULL, FTDM_NOTIMPL, "span I/O with no channel configuration implemented\n");

	currindex = span->chan_count;
	*configured = 0;
	*configured = span->fio->configure_span(span, str, chan_config->type, chan_config->name, chan_config->number);
	if (!*configured) {
		ftdm_log(FTDM_LOG_ERROR, "%d:Failed to configure span\n", span->span_id);
		return FTDM_FAIL;
	}

	if (chan_config->group_name[0]) {
		if (ftdm_group_add_channels(span, currindex, chan_config->group_name) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%d:Failed to add channels to group %s\n", span->span_id, chan_config->group_name);
			return FTDM_FAIL;
		}
	}

	if (ftdm_set_channels_gains(span, currindex, chan_config->rxgain, chan_config->txgain) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "%d:Failed to set channel gains\n", span->span_id);
		return FTDM_FAIL;
	}

	for (chan_index = currindex + 1; chan_index <= span->chan_count; chan_index++) {
		if (chan_config->iostats) {
			ftdm_channel_set_feature(span->channels[chan_index], FTDM_CHANNEL_FEATURE_IO_STATS);
		}

		if (!FTDM_IS_VOICE_CHANNEL(span->channels[chan_index])) {
			continue;
		}

		if (chan_config->debugdtmf) {
			span->channels[chan_index]->dtmfdbg.requested = 1;
		}

		span->channels[chan_index]->dtmfdetect.duration_ms = chan_config->dtmfdetect_ms;
		if (chan_config->dtmf_on_start) {
			span->channels[chan_index]->dtmfdetect.trigger_on_start = 1;
		}
	}

	return FTDM_SUCCESS;
}


static ftdm_status_t load_config(void)
{
	const char cfg_name[] = "freetdm.conf";
	ftdm_config_t cfg;
	char *var, *val;
	int catno = -1;
	int intparam = 0;
	ftdm_span_t *span = NULL;
	unsigned configured = 0, d = 0;
	ftdm_analog_start_type_t tmp;
	ftdm_size_t len = 0;
	ftdm_channel_config_t chan_config;
	ftdm_status_t ret = FTDM_SUCCESS;

	memset(&chan_config, 0, sizeof(chan_config));
	sprintf(chan_config.group_name, "__default");

	if (!ftdm_config_open_file(&cfg, cfg_name)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to open configuration file %s\n", cfg_name);
		return FTDM_FAIL;
	}

	ftdm_log(FTDM_LOG_DEBUG, "Reading FreeTDM configuration file\n");

	while (ftdm_config_next_pair(&cfg, &var, &val)) {
		if (*cfg.category == '#') {
			if (cfg.catno != catno) {
				ftdm_log(FTDM_LOG_DEBUG, "Skipping %s\n", cfg.category);
				catno = cfg.catno;
			}
		} else if (!strncasecmp(cfg.category, "span", 4)) {
			if (cfg.catno != catno) {
				char *type = cfg.category + 4;
				char *name;

				if (*type == ' ') {
					type++;
				}

				ftdm_log(FTDM_LOG_DEBUG, "found config for span\n");
				catno = cfg.catno;

				if (ftdm_strlen_zero(type)) {
					ftdm_log(FTDM_LOG_CRIT, "failure creating span, no type specified.\n");
					span = NULL;
					continue;
				}

				if ((name = strchr(type, ' '))) {
					*name++ = '\0';
				}

				/* Verify if trunk_type was specified for previous span */
				if (span && span->trunk_type == FTDM_TRUNK_NONE) {
					ftdm_log(FTDM_LOG_ERROR, "trunk_type not specified for span %d (%s)\n", span->span_id, span->name);
					ret = FTDM_FAIL;
					goto done;
				}

				if (ftdm_span_create(type, name, &span) == FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_DEBUG, "created span %d (%s) of type %s\n", span->span_id, span->name, type);
					d = 0;
					/* it is confusing that parameters from one span affect others, so let's clear them */
					memset(&chan_config, 0, sizeof(chan_config));
					sprintf(chan_config.group_name, "__default");
					/* default to storing iostats */
					chan_config.iostats = FTDM_TRUE;
				} else {
					ftdm_log(FTDM_LOG_CRIT, "failure creating span of type %s\n", type);
					span = NULL;
					continue;
				}
			}

			if (!span) {
				continue;
			}

			ftdm_log(FTDM_LOG_DEBUG, "span %d [%s]=[%s]\n", span->span_id, var, val);

			if (!strcasecmp(var, "trunk_type")) {
				ftdm_trunk_type_t trtype = ftdm_str2ftdm_trunk_type(val);
				ftdm_span_set_trunk_type(span, trtype);
				ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s'\n", ftdm_trunk_type2str(trtype));
			} else if (!strcasecmp(var, "trunk_mode")) {
				ftdm_trunk_mode_t trmode = ftdm_str2ftdm_trunk_mode(val);
				ftdm_span_set_trunk_mode(span, trmode);
				ftdm_log(FTDM_LOG_DEBUG, "setting trunk mode to '%s'\n", ftdm_trunk_mode2str(trmode));
			} else if (!strcasecmp(var, "name")) {
				if (!strcasecmp(val, "undef")) {
					chan_config.name[0] = '\0';
				} else {
					ftdm_copy_string(chan_config.name, val, FTDM_MAX_NAME_STR_SZ);
				}
			} else if (!strcasecmp(var, "number")) {
				if (!strcasecmp(val, "undef")) {
					chan_config.number[0] = '\0';
				} else {
					ftdm_copy_string(chan_config.number, val, FTDM_MAX_NUMBER_STR_SZ);
				}
			} else if (!strcasecmp(var, "analog-start-type")) {
				if (span->trunk_type == FTDM_TRUNK_FXS || span->trunk_type == FTDM_TRUNK_FXO || span->trunk_type == FTDM_TRUNK_EM) {
					if ((tmp = ftdm_str2ftdm_analog_start_type(val)) != FTDM_ANALOG_START_NA) {
						span->start_type = tmp;
						ftdm_log(FTDM_LOG_DEBUG, "changing start type to '%s'\n", ftdm_analog_start_type2str(span->start_type));
					}
				} else {
					ftdm_log(FTDM_LOG_ERROR, "This option is only valid on analog trunks!\n");
				}
			} else if (!strcasecmp(var, "fxo-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					span->trunk_type = FTDM_TRUNK_FXO;
					span->trunk_mode = FTDM_TRUNK_MODE_CPE;
					ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s' start(%s), mode(%s)\n", ftdm_trunk_type2str(span->trunk_type),
							ftdm_analog_start_type2str(span->start_type), ftdm_trunk_mode2str(span->trunk_mode));
				}
				if (span->trunk_type == FTDM_TRUNK_FXO) {
					unsigned chans_configured = 0;
					chan_config.type = FTDM_CHAN_TYPE_FXO;
					if (ftdm_configure_span_channels(span, val, &chan_config, &chans_configured) == FTDM_SUCCESS) {
						configured += chans_configured;
					}
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add FXO channels to a %s trunk!\n", ftdm_trunk_type2str(span->trunk_type));
				}
			} else if (!strcasecmp(var, "fxs-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					span->trunk_type = FTDM_TRUNK_FXS;
					span->trunk_mode = FTDM_TRUNK_MODE_NET;
					ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s' start(%s), mode(%s)\n", ftdm_trunk_type2str(span->trunk_type),
							ftdm_analog_start_type2str(span->start_type), ftdm_trunk_mode2str(span->trunk_mode));
				}
				if (span->trunk_type == FTDM_TRUNK_FXS) {
					unsigned chans_configured = 0;
					chan_config.type = FTDM_CHAN_TYPE_FXS;
					if (ftdm_configure_span_channels(span, val, &chan_config, &chans_configured) == FTDM_SUCCESS) {
						configured += chans_configured;
					}
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add FXS channels to a %s trunk!\n", ftdm_trunk_type2str(span->trunk_type));
				}
			} else if (!strcasecmp(var, "em-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					span->trunk_type = FTDM_TRUNK_EM;
					span->trunk_mode = FTDM_TRUNK_MODE_CPE;
					ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s' start(%s), mode(%s)\n", ftdm_trunk_type2str(span->trunk_type),
							ftdm_analog_start_type2str(span->start_type), ftdm_trunk_mode2str(span->trunk_mode));
				}
				if (span->trunk_type == FTDM_TRUNK_EM) {
					unsigned chans_configured = 0;
					chan_config.type = FTDM_CHAN_TYPE_EM;
					if (ftdm_configure_span_channels(span, val, &chan_config, &chans_configured) == FTDM_SUCCESS) {
						configured += chans_configured;
					}
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add EM channels to a %s trunk!\n", ftdm_trunk_type2str(span->trunk_type));
				}
			} else if (!strcasecmp(var, "b-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					ftdm_log(FTDM_LOG_ERROR, "No trunk type specified in configuration file\n");
					break;
				}
				if (FTDM_SPAN_IS_DIGITAL(span)) {
					unsigned chans_configured = 0;
					chan_config.type = FTDM_CHAN_TYPE_B;
					if (ftdm_configure_span_channels(span, val, &chan_config, &chans_configured) == FTDM_SUCCESS) {
						configured += chans_configured;
					}
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add B channels to a %s trunk!\n", ftdm_trunk_type2str(span->trunk_type));
				}
			} else if (!strcasecmp(var, "d-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					ftdm_log(FTDM_LOG_ERROR, "No trunk type specified in configuration file\n");
					break;
				}
				if (FTDM_SPAN_IS_DIGITAL(span)) {
					unsigned chans_configured = 0;
					if (d) {
						ftdm_log(FTDM_LOG_WARNING, "ignoring extra d-channel\n");
						continue;
					}
					if (!strncasecmp(val, "lapd:", 5)) {
						chan_config.type = FTDM_CHAN_TYPE_DQ931;
						val += 5;
					} else {
						chan_config.type = FTDM_CHAN_TYPE_DQ921;
					}
					if (ftdm_configure_span_channels(span, val, &chan_config, &chans_configured) == FTDM_SUCCESS) {
						configured += chans_configured;
					}
					d++;
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add D channels to a %s trunk!\n", ftdm_trunk_type2str(span->trunk_type));
				}
			} else if (!strcasecmp(var, "cas-channel")) {
				unsigned chans_configured = 0;
				chan_config.type = FTDM_CHAN_TYPE_CAS;
				if (ftdm_configure_span_channels(span, val, &chan_config, &chans_configured) == FTDM_SUCCESS) {
					configured += chans_configured;
				}
			} else if (!strcasecmp(var, "dtmf_hangup")) {
				span->dtmf_hangup = ftdm_strdup(val);
				span->dtmf_hangup_len = strlen(val);
			} else if (!strcasecmp(var, "txgain")) {
				if (sscanf(val, "%f", &(chan_config.txgain)) != 1) {
					ftdm_log(FTDM_LOG_ERROR, "invalid txgain: '%s'\n", val);
				}
			} else if (!strcasecmp(var, "rxgain")) {
				if (sscanf(val, "%f", &(chan_config.rxgain)) != 1) {
					ftdm_log(FTDM_LOG_ERROR, "invalid rxgain: '%s'\n", val);
				}
			} else if (!strcasecmp(var, "debugdtmf")) {
				chan_config.debugdtmf = ftdm_true(val);
				ftdm_log(FTDM_LOG_DEBUG, "Setting debugdtmf to '%s'\n", chan_config.debugdtmf ? "yes" : "no");
			} else if (!strncasecmp(var, "dtmfdetect_ms", sizeof("dtmfdetect_ms")-1)) {
				if (chan_config.dtmf_on_start == FTDM_TRUE) {
					chan_config.dtmf_on_start = FTDM_FALSE;
					ftdm_log(FTDM_LOG_WARNING, "dtmf_on_start parameter disabled because dtmfdetect_ms specified\n");
				}
				if (sscanf(val, "%d", &(chan_config.dtmfdetect_ms)) != 1) {
					ftdm_log(FTDM_LOG_ERROR, "invalid dtmfdetect_ms: '%s'\n", val);
				}
			} else if (!strncasecmp(var, "dtmf_on_start", sizeof("dtmf_on_start")-1)) {
				if (chan_config.dtmfdetect_ms) {
					ftdm_log(FTDM_LOG_WARNING, "dtmf_on_start parameter ignored because dtmf_detect_ms specified\n");
				} else {
					if (ftdm_true(val)) {
						chan_config.dtmf_on_start = FTDM_TRUE;
					} else {
						chan_config.dtmf_on_start = FTDM_FALSE;
					}
				}
			} else if (!strncasecmp(var, "iostats", sizeof("iostats")-1)) {
				if (ftdm_true(val)) {
					chan_config.iostats = FTDM_TRUE;
				} else {
					chan_config.iostats = FTDM_FALSE;
				}
				ftdm_log(FTDM_LOG_DEBUG, "Setting iostats to '%s'\n", chan_config.iostats ? "yes" : "no");
			} else if (!strcasecmp(var, "group")) {
				len = strlen(val);
				if (len >= FTDM_MAX_NAME_STR_SZ) {
					len = FTDM_MAX_NAME_STR_SZ - 1;
					ftdm_log(FTDM_LOG_WARNING, "Truncating group name %s to %"FTDM_SIZE_FMT" length\n", val, len);
				}
				memcpy(chan_config.group_name, val, len);
				chan_config.group_name[len] = '\0';
			} else {
				ftdm_log(FTDM_LOG_ERROR, "unknown span variable '%s'\n", var);
			}
		} else if (!strncasecmp(cfg.category, "general", 7)) {
			if (!strncasecmp(var, "cpu_monitor", sizeof("cpu_monitor")-1)) {
				if (!strncasecmp(val, "yes", 3)) {
					globals.cpu_monitor.enabled = 1;
					if (!globals.cpu_monitor.alarm_action_flags) {
						globals.cpu_monitor.alarm_action_flags |= FTDM_CPU_ALARM_ACTION_WARN;
					}
				}
			} else if (!strncasecmp(var, "debugdtmf_directory", sizeof("debugdtmf_directory")-1)) {
				ftdm_set_string(globals.dtmfdebug_directory, val);
				ftdm_log(FTDM_LOG_DEBUG, "Debug DTMF directory set to '%s'\n", globals.dtmfdebug_directory);
			} else if (!strncasecmp(var, "cpu_monitoring_interval", sizeof("cpu_monitoring_interval")-1)) {
				if (atoi(val) > 0) {
					globals.cpu_monitor.interval = atoi(val);
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid cpu monitoring interval %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_set_alarm_threshold", sizeof("cpu_set_alarm_threshold")-1)) {
				intparam = atoi(val);
				if (intparam > 0 && intparam < 100) {
					globals.cpu_monitor.set_alarm_threshold = (uint8_t)intparam;
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid cpu alarm set threshold %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_reset_alarm_threshold", sizeof("cpu_reset_alarm_threshold")-1) ||
			           !strncasecmp(var, "cpu_clear_alarm_threshold", sizeof("cpu_clear_alarm_threshold")-1)) {
				intparam = atoi(val);
				if (intparam > 0 && intparam < 100) {
					globals.cpu_monitor.clear_alarm_threshold = (uint8_t)intparam;
					if (globals.cpu_monitor.clear_alarm_threshold > globals.cpu_monitor.set_alarm_threshold) {
						globals.cpu_monitor.clear_alarm_threshold = globals.cpu_monitor.set_alarm_threshold - 10;
						ftdm_log(FTDM_LOG_ERROR, "Cpu alarm clear threshold must be lower than set threshold, "
								"setting clear threshold to %d\n", globals.cpu_monitor.clear_alarm_threshold);
					}
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid cpu alarm reset threshold %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_alarm_action", sizeof("cpu_alarm_action")-1)) {
				char* p = val;
				do {
					if (!strncasecmp(p, "reject", sizeof("reject")-1)) {
						globals.cpu_monitor.alarm_action_flags |= FTDM_CPU_ALARM_ACTION_REJECT;
					} else if (!strncasecmp(p, "warn", sizeof("warn")-1)) {
						globals.cpu_monitor.alarm_action_flags |= FTDM_CPU_ALARM_ACTION_WARN;
					}
					p = strchr(p, ',');
					if (p) {
						while(*p++) if (*p != 0x20) break;
					}
				} while (p);
			}
		} else {
			ftdm_log(FTDM_LOG_ERROR, "unknown param [%s] '%s' / '%s'\n", cfg.category, var, val);
		}
	}

	/* Verify is trunk_type was specified for the last span */
	if (span && span->trunk_type == FTDM_TRUNK_NONE) {
		ftdm_log(FTDM_LOG_ERROR, "trunk_type not specified for span %d (%s)\n", span->span_id, span->name);
		ret = FTDM_FAIL;
	}

done:
	ftdm_config_close_file(&cfg);

	ftdm_log(FTDM_LOG_INFO, "Configured %u channel(s)\n", configured);
	if (!configured) {
		ret = FTDM_FAIL;
	}

	return ret;
}

static ftdm_status_t process_module_config(ftdm_io_interface_t *fio)
{
	ftdm_config_t cfg;
	char *var, *val;
	char filename[256] = "";
	
	ftdm_assert_return(fio != NULL, FTDM_FAIL, "fio argument is null\n");

	snprintf(filename, sizeof(filename), "%s.conf", fio->name);

	if (!fio->configure) {
		ftdm_log(FTDM_LOG_DEBUG, "Module %s does not support configuration.\n", fio->name);	
		return FTDM_FAIL;
	}

	if (!ftdm_config_open_file(&cfg, filename)) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot open %s\n", filename);	
		return FTDM_FAIL;
	}

	while (ftdm_config_next_pair(&cfg, &var, &val)) {
		fio->configure(cfg.category, var, val, cfg.lineno);
	}

	ftdm_config_close_file(&cfg);	

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_global_add_io_interface(ftdm_io_interface_t *interface1)
{
	ftdm_status_t ret = FTDM_SUCCESS;
	ftdm_mutex_lock(globals.mutex);
	if (hashtable_search(globals.interface_hash, (void *)interface1->name)) {
		ftdm_log(FTDM_LOG_ERROR, "Interface %s already loaded!\n", interface1->name);
	} else {
		hashtable_insert(globals.interface_hash, (void *)interface1->name, interface1, HASHTABLE_FLAG_NONE);
	}
	ftdm_mutex_unlock(globals.mutex);
	return ret;
}

FT_DECLARE(ftdm_io_interface_t *) ftdm_global_get_io_interface(const char *iotype, ftdm_bool_t autoload)
{
	ftdm_io_interface_t *fio = NULL;

	ftdm_mutex_lock(globals.mutex);

	fio = (ftdm_io_interface_t *) hashtable_search(globals.interface_hash, (void *)iotype);
	if (!fio && autoload) {
		ftdm_load_module_assume(iotype);
		fio = (ftdm_io_interface_t *) hashtable_search(globals.interface_hash, (void *)iotype);
		if (fio) {
			ftdm_log(FTDM_LOG_INFO, "Auto-loaded I/O module '%s'\n", iotype);
		}
	}

	ftdm_mutex_unlock(globals.mutex);
	return fio;
}

FT_DECLARE(int) ftdm_load_module(const char *name)
{
	ftdm_dso_lib_t lib;
	int count = 0, x = 0;
	char path[512] = "";
	char *err;
	ftdm_module_t *mod;

	ftdm_build_dso_path(name, path, sizeof(path));

	if (!(lib = ftdm_dso_open(path, &err))) {
		ftdm_log(FTDM_LOG_ERROR, "Error loading %s [%s]\n", path, err);
		ftdm_safe_free(err);
		return 0;
	}
	
	if (!(mod = (ftdm_module_t *) ftdm_dso_func_sym(lib, "ftdm_module", &err))) {
		ftdm_log(FTDM_LOG_ERROR, "Error loading %s [%s]\n", path, err);
		ftdm_safe_free(err);
		return 0;
	}

	if (mod->io_load) {
		ftdm_io_interface_t *interface1 = NULL; /* name conflict w/windows here */

		if (mod->io_load(&interface1) != FTDM_SUCCESS || !interface1 || !interface1->name) {
			ftdm_log(FTDM_LOG_ERROR, "Error loading %s\n", path);
		} else {
			ftdm_log(FTDM_LOG_INFO, "Loading IO from %s [%s]\n", path, interface1->name);
			if (ftdm_global_add_io_interface(interface1) == FTDM_SUCCESS) {
				process_module_config(interface1);
				x++;
			}
		}
	}

	if (mod->sig_load) {
		if (mod->sig_load() != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error loading %s\n", path);
		} else {
			ftdm_log(FTDM_LOG_INFO, "Loading SIG from %s\n", path);
			x++;
		}
	}

	if (x) {
		char *p;
		mod->lib = lib;
		ftdm_set_string(mod->path, path);
		if (mod->name[0] == '\0') {
			if (!(p = strrchr(path, *FTDM_PATH_SEPARATOR))) {
				p = path;
			}
			ftdm_set_string(mod->name, p);
		}

		ftdm_mutex_lock(globals.mutex);
		if (hashtable_search(globals.module_hash, (void *)mod->name)) {
			ftdm_log(FTDM_LOG_ERROR, "Module %s already loaded!\n", mod->name);
			ftdm_dso_destroy(&lib);
		} else {
			hashtable_insert(globals.module_hash, (void *)mod->name, mod, HASHTABLE_FLAG_NONE);
			count++;
		}
		ftdm_mutex_unlock(globals.mutex);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Unloading %s\n", path);
		ftdm_dso_destroy(&lib);
	}
	
	return count;
}

FT_DECLARE(int) ftdm_load_module_assume(const char *name)
{
	char buf[256] = "";

	snprintf(buf, sizeof(buf), "ftmod_%s", name);
	return ftdm_load_module(buf);
}

FT_DECLARE(int) ftdm_load_modules(void)
{
	char cfg_name[] = "modules.conf";
	ftdm_config_t cfg;
	char *var, *val;
	int count = 0;

	if (!ftdm_config_open_file(&cfg, cfg_name)) {
        return FTDM_FAIL;
    }

	while (ftdm_config_next_pair(&cfg, &var, &val)) {
        if (!strcasecmp(cfg.category, "modules")) {
			if (!strcasecmp(var, "load")) {
				count += ftdm_load_module(val);
			}
		}
	}
			
	return count;
}

FT_DECLARE(ftdm_status_t) ftdm_unload_modules(void)
{
	ftdm_hash_iterator_t *i = NULL;
	ftdm_dso_lib_t lib = NULL;
	char modpath[255] = { 0 };

	/* stop signaling interfaces first as signaling depends on I/O and not the other way around */
	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		const void *key = NULL;
		void *val = NULL;
		ftdm_module_t *mod = NULL;

		hashtable_this(i, &key, NULL, &val);
		
		if (!key || !val) {
			continue;
		}
		
		mod = (ftdm_module_t *) val;

		if (!mod->sig_unload) {
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloading signaling interface %s\n", mod->name);
		
		if (mod->sig_unload() != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error unloading signaling interface %s\n", mod->name);
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloaded signaling interface %s\n", mod->name);
	}

	/* Now go ahead with I/O interfaces */
	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		const void *key = NULL;
		void *val = NULL;
		ftdm_module_t *mod = NULL;

		hashtable_this(i, &key, NULL, &val);
		
		if (!key || !val) {
			continue;
		}
		
		mod = (ftdm_module_t *) val;

		if (!mod->io_unload) {
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloading I/O interface %s\n", mod->name);
		
		if (mod->io_unload() != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error unloading I/O interface %s\n", mod->name);
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloaded I/O interface %s\n", mod->name);
	}

	/* Now unload the actual shared object/dll */
	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		ftdm_module_t *mod = NULL;
		const void *key = NULL;
		void *val = NULL;

		hashtable_this(i, &key, NULL, &val);

		if (!key || !val) {
			continue;
		}

		mod = (ftdm_module_t *) val;

		lib = mod->lib;
		snprintf(modpath, sizeof(modpath), "%s", mod->path);
		ftdm_log(FTDM_LOG_INFO, "Unloading module %s\n", modpath);
		ftdm_dso_destroy(&lib);
		ftdm_log(FTDM_LOG_INFO, "Unloaded module %s\n", modpath);
	}

	return FTDM_SUCCESS;
}

static ftdm_status_t post_configure_span_channels(ftdm_span_t *span)
{
	unsigned i = 0;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_signaling_status_t sigstatus = FTDM_SIG_STATE_DOWN;
	for (i = 1; i <= span->chan_count; i++) {
		sigstatus = FTDM_SIG_STATE_DOWN;
		ftdm_channel_get_sig_status(span->channels[i], &sigstatus);
		if (sigstatus == FTDM_SIG_STATE_UP) {
			ftdm_set_flag(span->channels[i], FTDM_CHANNEL_SIG_UP);
		}
	}
	if (ftdm_test_flag(span, FTDM_SPAN_USE_CHAN_QUEUE)) {
		status = ftdm_queue_create(&span->pendingchans, SPAN_PENDING_CHANS_QUEUE_SIZE);
	}
	if (status == FTDM_SUCCESS && ftdm_test_flag(span, FTDM_SPAN_USE_SIGNALS_QUEUE)) {
		status = ftdm_queue_create(&span->pendingsignals, SPAN_PENDING_SIGNALS_QUEUE_SIZE);
	}
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_configure_span(ftdm_span_t *span, const char *type, fio_signal_cb_t sig_cb, ...)
{
	ftdm_module_t *mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type);
	ftdm_status_t status = FTDM_FAIL;

	if (!span->chan_count) {
		ftdm_log(FTDM_LOG_WARNING, "Cannot configure signaling on span with no channels\n");
		return FTDM_FAIL;
	}

	if (!mod) {
		ftdm_load_module_assume(type);
		if ((mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type))) {
			ftdm_log(FTDM_LOG_INFO, "auto-loaded '%s'\n", type);
		} else {
			ftdm_log(FTDM_LOG_ERROR, "can't load '%s'\n", type);
			return FTDM_FAIL;
		}
	}

	if (mod->sig_configure) {
		va_list ap;
		va_start(ap, sig_cb);
		status = mod->sig_configure(span, sig_cb, ap);
		va_end(ap);
		if (status == FTDM_SUCCESS) {
			status = post_configure_span_channels(span);
		}
	} else {
		ftdm_log(FTDM_LOG_CRIT, "module '%s' did not implement the sig_configure method\n", type);
		status = FTDM_FAIL;
	}

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_configure_span_signaling(ftdm_span_t *span, const char *type, fio_signal_cb_t sig_cb, ftdm_conf_parameter_t *parameters) 
{
	ftdm_module_t *mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type);
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(type != NULL, FTDM_FAIL, "No signaling type");
	ftdm_assert_return(span != NULL, FTDM_FAIL, "No span");
	ftdm_assert_return(sig_cb != NULL, FTDM_FAIL, "No signaling callback");
	ftdm_assert_return(parameters != NULL, FTDM_FAIL, "No parameters");

	if (!span->chan_count) {
		ftdm_log(FTDM_LOG_WARNING, "Cannot configure signaling on span %s with no channels\n", span->name);
		return FTDM_FAIL;
	}

	if (!mod) {
		ftdm_load_module_assume(type);
		if ((mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type))) {
			ftdm_log(FTDM_LOG_INFO, "auto-loaded '%s'\n", type);
		}
	}

	if (!mod) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to load module type: %s\n", type);
		return FTDM_FAIL;
	}

	if (mod->configure_span_signaling) {
		status = mod->configure_span_signaling(span, sig_cb, parameters);
		if (status == FTDM_SUCCESS) {
			status = post_configure_span_channels(span);
		}
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Module %s did not implement the signaling configuration method\n", type);
	}

	return status;
}

static void *ftdm_span_service_events(ftdm_thread_t *me, void *obj)
{
	uint32_t i;
	unsigned waitms;
	ftdm_event_t *event;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_span_t *span = (ftdm_span_t*) obj;
	short *poll_events = ftdm_malloc(sizeof(short) * span->chan_count);

	if (me == 0) {};

	memset(poll_events, 0, sizeof(short) * span->chan_count);

	for(i = 1; i <= span->chan_count; i++) {
		poll_events[i] |= FTDM_EVENTS;
	}

	while (ftdm_running() && !(ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD))) {
		waitms = 1000;
		status = ftdm_span_poll_event(span, waitms, poll_events);
		switch (status) {
			case FTDM_FAIL:
				ftdm_log(FTDM_LOG_CRIT, "%s:Failed to poll span for events\n", span->name);
				break;
			case FTDM_TIMEOUT:
				break;
			case FTDM_SUCCESS:
				/* Check if there are any channels that have events available */
				while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS);
				break;
			default:
				ftdm_log(FTDM_LOG_CRIT, "%s:Unhandled IO event\n", span->name);
		}
	}
	return NULL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_register_signal_cb(ftdm_span_t *span, fio_signal_cb_t sig_cb)
{
	span->signal_cb = sig_cb;
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_span_start(ftdm_span_t *span)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_mutex_lock(span->mutex);

	if (ftdm_test_flag(span, FTDM_SPAN_STARTED)) {
		status = FTDM_EINVAL;
		goto done;
	}
	if (span->signal_type == FTDM_SIGTYPE_NONE) {
		/* If there is no signalling component, start a thread to poll events */
		status = ftdm_thread_create_detached(ftdm_span_service_events, span);
		if (status != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT,"Failed to start span event monitor thread!\n");
			goto done;
		}

		//ftdm_report_initial_channels_alarms(span);		
		ftdm_set_flag_locked(span, FTDM_SPAN_STARTED);
		goto done;
	}

	if (!span->start) {
		status = FTDM_ENOSYS;
		goto done;
	}

	/* Start I/O */
	if (span->fio && span->fio->span_start) {
		status = span->fio->span_start(span);
		if (status != FTDM_SUCCESS)
			goto done;
	}

	/* Start SIG */
	status = ftdm_report_initial_channels_alarms(span);
	if (status != FTDM_SUCCESS) {
		goto done;
	}

	status = span->start(span);
	if (status == FTDM_SUCCESS) {
		ftdm_set_flag_locked(span, FTDM_SPAN_STARTED);
	}
done:
	ftdm_mutex_unlock(span->mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_add_to_group(const char* name, ftdm_channel_t* ftdmchan)
{
	unsigned int i;
	ftdm_group_t* group = NULL;
	
	ftdm_mutex_lock(globals.group_mutex);

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Cannot add a null channel to a group\n");

	if (ftdm_group_find_by_name(name, &group) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_DEBUG, "Creating new group:%s\n", name);
		ftdm_group_create(&group, name);
	}

	/*verify that group does not already include this channel first */
	for(i = 0; i < group->chan_count; i++) {
		if (group->channels[i]->physical_span_id == ftdmchan->physical_span_id &&
				group->channels[i]->physical_chan_id == ftdmchan->physical_chan_id) {

			ftdm_mutex_unlock(globals.group_mutex);
			ftdm_log(FTDM_LOG_DEBUG, "Channel %d:%d is already added to group %s\n", 
					group->channels[i]->physical_span_id,
					group->channels[i]->physical_chan_id,
					name);
			return FTDM_SUCCESS;
		}
	}

	if (group->chan_count >= FTDM_MAX_CHANNELS_GROUP) {
		ftdm_log(FTDM_LOG_ERROR, "Max number of channels exceeded (max:%d)\n", FTDM_MAX_CHANNELS_GROUP);
		ftdm_mutex_unlock(globals.group_mutex);
		return FTDM_FAIL;
	}

	group->channels[group->chan_count++] = ftdmchan;
	ftdm_mutex_unlock(globals.group_mutex);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_remove_from_group(ftdm_group_t* group, ftdm_channel_t* ftdmchan)
{
	unsigned int i, j;
	//Need to test this function
	ftdm_mutex_lock(globals.group_mutex);

	for (i=0; i < group->chan_count; i++) {
			if (group->channels[i]->physical_span_id == ftdmchan->physical_span_id &&
					group->channels[i]->physical_chan_id == ftdmchan->physical_chan_id) {

				j=i;
				while(j < group->chan_count-1) {
					group->channels[j] = group->channels[j+1];
					j++;
				}
				group->channels[group->chan_count--] = NULL;
				if (group->chan_count <=0) {
					/* Delete group if it is empty */
					hashtable_remove(globals.group_hash, (void *)group->name);
				}
				ftdm_mutex_unlock(globals.group_mutex);
				return FTDM_SUCCESS;
			}
	}

	ftdm_mutex_unlock(globals.group_mutex);
	//Group does not contain this channel
	return FTDM_FAIL;
}

static ftdm_status_t ftdm_group_add_channels(ftdm_span_t* span, int currindex, const char* name)
{
	unsigned chan_index = 0;

	ftdm_assert_return(strlen(name) > 0, FTDM_FAIL, "Invalid group name provided\n");
	ftdm_assert_return(currindex >= 0, FTDM_FAIL, "Invalid current channel index provided\n");

	if (!span->chan_count) {
		return FTDM_SUCCESS;
	}

	for (chan_index = currindex+1; chan_index <= span->chan_count; chan_index++) {
		if (!FTDM_IS_VOICE_CHANNEL(span->channels[chan_index])) {
			continue;
		}
		if (ftdm_channel_add_to_group(name, span->channels[chan_index])) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to add chan:%d to group:%s\n", chan_index, name);
		}
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_group_find(uint32_t id, ftdm_group_t **group)
{
	ftdm_group_t *fgroup = NULL, *grp;

	if (id > FTDM_MAX_GROUPS_INTERFACE) {
		return FTDM_FAIL;
	}

	
	ftdm_mutex_lock(globals.group_mutex);
	for (grp = globals.groups; grp; grp = grp->next) {
		if (grp->group_id == id) {
			fgroup = grp;
			break;
		}
	}
	ftdm_mutex_unlock(globals.group_mutex);

	if (!fgroup) {
		return FTDM_FAIL;
	}

	*group = fgroup;

	return FTDM_SUCCESS;
	
}

FT_DECLARE(ftdm_status_t) ftdm_group_find_by_name(const char *name, ftdm_group_t **group)
{
	ftdm_status_t status = FTDM_FAIL;
	*group = NULL;
	ftdm_mutex_lock(globals.group_mutex);
	if (!ftdm_strlen_zero(name)) {
		if ((*group = hashtable_search(globals.group_hash, (void *) name))) {
			status = FTDM_SUCCESS;
		}
	}
	ftdm_mutex_unlock(globals.group_mutex);
	return status;
}

static void ftdm_group_add(ftdm_group_t *group)
{
	ftdm_group_t *grp;
	ftdm_mutex_lock(globals.group_mutex);
	
	for (grp = globals.groups; grp && grp->next; grp = grp->next);

	if (grp) {
		grp->next = group;
	} else {
		globals.groups = group;
	}
	hashtable_insert(globals.group_hash, (void *)group->name, group, HASHTABLE_FLAG_NONE);

	ftdm_mutex_unlock(globals.group_mutex);
}

FT_DECLARE(ftdm_status_t) ftdm_group_create(ftdm_group_t **group, const char *name)
{
	ftdm_group_t *new_group = NULL;
	ftdm_status_t status = FTDM_FAIL;

	ftdm_mutex_lock(globals.mutex);
	if (globals.group_index < FTDM_MAX_GROUPS_INTERFACE) {
		new_group = ftdm_calloc(1, sizeof(*new_group));
		
		ftdm_assert(new_group != NULL, "Failed to create new ftdm group, expect a crash\n");

		status = ftdm_mutex_create(&new_group->mutex);

		ftdm_assert(status == FTDM_SUCCESS, "Failed to create group mutex, expect a crash\n");

		new_group->group_id = ++globals.group_index;
		new_group->name = ftdm_strdup(name);
		ftdm_group_add(new_group);
		*group = new_group;
		status = FTDM_SUCCESS;
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Group %s was not added, we exceeded the max number of groups\n", name);
	}
	ftdm_mutex_unlock(globals.mutex);
	return status;
}

static ftdm_status_t ftdm_span_trigger_signal(const ftdm_span_t *span, ftdm_sigmsg_t *sigmsg)
{
	if (!span->signal_cb) {
		return FTDM_FAIL;
	}
	return span->signal_cb(sigmsg);
}

static ftdm_status_t ftdm_span_queue_signal(const ftdm_span_t *span, ftdm_sigmsg_t *sigmsg)
{
	ftdm_sigmsg_t *new_sigmsg = NULL;

	new_sigmsg = ftdm_calloc(1, sizeof(*sigmsg));
	if (!new_sigmsg) {
		return FTDM_FAIL;
	}
	memcpy(new_sigmsg, sigmsg, sizeof(*sigmsg));

	ftdm_queue_enqueue(span->pendingsignals, new_sigmsg);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_span_trigger_signals(const ftdm_span_t *span)
{
	ftdm_sigmsg_t *sigmsg = NULL;
	while ((sigmsg = ftdm_queue_dequeue(span->pendingsignals))) {
		ftdm_span_trigger_signal(span, sigmsg);
		ftdm_sigmsg_free(&sigmsg);
	}
	return FTDM_SUCCESS;
}


static void execute_safety_hangup(void *data)
{
	ftdm_channel_t *fchan = data;
	ftdm_channel_lock(fchan);
	fchan->hangup_timer = 0;
	if (fchan->state == FTDM_CHANNEL_STATE_TERMINATING) {
		ftdm_log_chan(fchan, FTDM_LOG_WARNING, "Forcing hangup since the user did not confirmed our hangup after %dms\n", FORCE_HANGUP_TIMER);
		_ftdm_channel_call_hangup_nl(__FILE__, __FUNCTION__, __LINE__, fchan, NULL);
	} else {
		ftdm_log_chan(fchan, FTDM_LOG_CRIT, "Not performing safety hangup, channel state is %s\n", ftdm_channel_state2str(fchan->state));
	}
	ftdm_channel_unlock(fchan);
}

FT_DECLARE(ftdm_status_t) ftdm_span_send_signal(ftdm_span_t *span, ftdm_sigmsg_t *sigmsg)
{
	ftdm_channel_t *fchan = NULL;
	if (sigmsg->channel) {
		fchan = sigmsg->channel;
		ftdm_channel_lock(fchan);
	}
	
	/* some core things to do on special events */
	switch (sigmsg->event_id) {

	case FTDM_SIGEVENT_SIGSTATUS_CHANGED:
		{
			if (sigmsg->ev_data.sigstatus.status == FTDM_SIG_STATE_UP) {
				ftdm_set_flag(fchan, FTDM_CHANNEL_SIG_UP);
				ftdm_clear_flag(fchan, FTDM_CHANNEL_SUSPENDED);
			} else {
				ftdm_clear_flag(fchan, FTDM_CHANNEL_SIG_UP);
				if (sigmsg->ev_data.sigstatus.status == FTDM_SIG_STATE_SUSPENDED) {
					ftdm_set_flag(fchan, FTDM_CHANNEL_SUSPENDED);
				} else {
					ftdm_clear_flag(fchan, FTDM_CHANNEL_SUSPENDED);
				}
			}
		}
		break;

	case FTDM_SIGEVENT_START:
		{
			ftdm_assert(!ftdm_test_flag(fchan, FTDM_CHANNEL_CALL_STARTED), "Started call twice!\n");

			if (ftdm_test_flag(fchan, FTDM_CHANNEL_OUTBOUND)) {
				ftdm_log_chan_msg(fchan, FTDM_LOG_WARNING, "Inbound call taking over outbound channel\n");
				ftdm_clear_flag(fchan, FTDM_CHANNEL_OUTBOUND);
			}
			ftdm_set_flag(fchan, FTDM_CHANNEL_CALL_STARTED);
			ftdm_call_set_call_id(fchan, &fchan->caller_data);
			/* when cleaning up the public API I added this because mod_freetdm.c on_fxs_signal was
			 * doing it during SIGEVENT_START, but now that flags are private they can't, wonder if
			 * is needed at all? */
			ftdm_clear_flag(sigmsg->channel, FTDM_CHANNEL_HOLD);
			if (sigmsg->channel->caller_data.bearer_capability == FTDM_BEARER_CAP_UNRESTRICTED) {
				ftdm_set_flag(sigmsg->channel, FTDM_CHANNEL_DIGITAL_MEDIA);
			}
		}
		break;

	case FTDM_SIGEVENT_PROGRESS_MEDIA:
		{
			/* test signaling module compliance */
			if (fchan->state != FTDM_CHANNEL_STATE_PROGRESS_MEDIA) {
				ftdm_log_chan(fchan, FTDM_LOG_WARNING, "FTDM_SIGEVENT_PROGRESS_MEDIA sent in state %s\n", ftdm_channel_state2str(fchan->state));
			}
		}
		break;

	case FTDM_SIGEVENT_UP:
		{
			/* test signaling module compliance */
			if (fchan->state != FTDM_CHANNEL_STATE_UP) {
				ftdm_log_chan(fchan, FTDM_LOG_WARNING, "FTDM_SIGEVENT_UP sent in state %s\n", ftdm_channel_state2str(fchan->state));
			}
		}
		break;

	case FTDM_SIGEVENT_STOP:
		{
			/* TODO: we could test for compliance here and check the state is FTDM_CHANNEL_STATE_TERMINATING
			 * but several modules need to be updated first */

			/* if the call was never started, do not send SIGEVENT_STOP
			   this happens for FXS devices in ftmod_analog which blindly send SIGEVENT_STOP, we should fix it there ... */
			if (!ftdm_test_flag(fchan, FTDM_CHANNEL_CALL_STARTED)) {
				ftdm_log_chan_msg(fchan, FTDM_LOG_DEBUG, "Ignoring SIGEVENT_STOP since user never knew about a call in this channel\n");
				goto done;
			}

			if (ftdm_test_flag(fchan, FTDM_CHANNEL_USER_HANGUP)) {
				ftdm_log_chan_msg(fchan, FTDM_LOG_DEBUG, "Ignoring SIGEVENT_STOP since user already requested hangup\n");
				goto done;
			}

			if (fchan->state == FTDM_CHANNEL_STATE_TERMINATING) {
				ftdm_log_chan_msg(fchan, FTDM_LOG_DEBUG, "Scheduling safety hangup timer\n");
				/* if the user does not move us to hangup in 2 seconds, we will do it ourselves */
				ftdm_sched_timer(globals.timingsched, "safety-hangup", FORCE_HANGUP_TIMER, execute_safety_hangup, fchan, &fchan->hangup_timer);
			}
		}
		break;

	default:
		break;	

	}

	if (fchan) {
		/* set members of the sigmsg that must be present for all events */
		sigmsg->chan_id = fchan->chan_id;
		sigmsg->span_id = fchan->span_id;
		sigmsg->call_id = fchan->caller_data.call_id;
		sigmsg->call_priv = fchan->caller_data.priv;
	}

	/* if the signaling module uses a queue for signaling notifications, then enqueue it */
	if (ftdm_test_flag(span, FTDM_SPAN_USE_SIGNALS_QUEUE)) {
		ftdm_span_queue_signal(span, sigmsg);
	} else {
		ftdm_span_trigger_signal(span, sigmsg);
	}

done:
	
	if (fchan) {
		ftdm_channel_unlock(fchan);
	}

	return FTDM_SUCCESS;
}

static void *ftdm_cpu_monitor_run(ftdm_thread_t *me, void *obj)
{
	cpu_monitor_t *monitor = (cpu_monitor_t *)obj;
	struct ftdm_cpu_monitor_stats *cpu_stats = ftdm_new_cpu_monitor();

	ftdm_log(FTDM_LOG_DEBUG, "CPU monitor thread is now running\n");
	if (!cpu_stats) {
		goto done;
	}
	monitor->running = 1;

	while (ftdm_running()) {
		double idle_time = 0.0;
		int cpu_usage = 0;

		if (ftdm_cpu_get_system_idle_time(cpu_stats, &idle_time)) {
			break;
		}

		cpu_usage = (int)(100 - idle_time);
		if (monitor->alarm) {
			if (cpu_usage <= monitor->clear_alarm_threshold) {
				ftdm_log(FTDM_LOG_DEBUG, "CPU alarm is now OFF (cpu usage: %d)\n", cpu_usage);
				monitor->alarm = 0;
			} else if (monitor->alarm_action_flags & FTDM_CPU_ALARM_ACTION_WARN) {
				ftdm_log(FTDM_LOG_WARNING, "CPU alarm is still ON (cpu usage: %d)\n", cpu_usage);
			}
		} else {
			if (cpu_usage >= monitor->set_alarm_threshold) {
				ftdm_log(FTDM_LOG_WARNING, "CPU alarm is now ON (cpu usage: %d)\n", cpu_usage);
				monitor->alarm = 1;
			}
		}
		ftdm_interrupt_wait(monitor->interrupt, monitor->interval);
	}

	ftdm_delete_cpu_monitor(cpu_stats);
	monitor->running = 0;

done:
	ftdm_unused_arg(me);
	ftdm_log(FTDM_LOG_DEBUG, "CPU monitor thread is now terminating\n");
	return NULL;
}

static ftdm_status_t ftdm_cpu_monitor_start(void)
{
	if (ftdm_interrupt_create(&globals.cpu_monitor.interrupt, FTDM_INVALID_SOCKET, FTDM_NO_FLAGS) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create CPU monitor interrupt\n");
		return FTDM_FAIL;
	}

	if (ftdm_thread_create_detached(ftdm_cpu_monitor_run, &globals.cpu_monitor) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create cpu monitor thread!!\n");
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

static void ftdm_cpu_monitor_stop(void)
{
	if (!globals.cpu_monitor.interrupt) {
		return;
	}

	if (!globals.cpu_monitor.running) {
		return;
	}

	if (ftdm_interrupt_signal(globals.cpu_monitor.interrupt) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to interrupt the CPU monitor\n");
		return;
	}

	while (globals.cpu_monitor.running) {
		ftdm_sleep(10);
	}

	ftdm_interrupt_destroy(&globals.cpu_monitor.interrupt);
}

FT_DECLARE(ftdm_status_t) ftdm_global_init(void)
{
	memset(&globals, 0, sizeof(globals));

	time_init();
	
	ftdm_thread_override_default_stacksize(FTDM_THREAD_STACKSIZE);

	globals.interface_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	globals.module_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	globals.span_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	globals.group_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	ftdm_mutex_create(&globals.mutex);
	ftdm_mutex_create(&globals.span_mutex);
	ftdm_mutex_create(&globals.group_mutex);
	ftdm_mutex_create(&globals.call_id_mutex);
	
	ftdm_sched_global_init();
	globals.running = 1;
	if (ftdm_sched_create(&globals.timingsched, "freetdm-master") != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create master timing schedule context\n");
		goto global_init_fail;
	}
	if (ftdm_sched_free_run(globals.timingsched) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to run master timing schedule context\n");
		goto global_init_fail;
	}
	
	return FTDM_SUCCESS;
	
global_init_fail:
	globals.running = 0;
	ftdm_mutex_destroy(&globals.mutex);
	ftdm_mutex_destroy(&globals.span_mutex);
	ftdm_mutex_destroy(&globals.group_mutex);
	ftdm_mutex_destroy(&globals.call_id_mutex);	
	hashtable_destroy(globals.interface_hash);
	hashtable_destroy(globals.module_hash);
	hashtable_destroy(globals.span_hash);
	hashtable_destroy(globals.group_hash);
	
	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_global_configuration(void)
{
	int modcount = 0;

	if (!globals.running) {
		return FTDM_FAIL;
	}
	
	modcount = ftdm_load_modules();

	ftdm_log(FTDM_LOG_NOTICE, "Modules configured: %d \n", modcount);

	globals.cpu_monitor.enabled = 0;
	globals.cpu_monitor.interval = 1000;
	globals.cpu_monitor.alarm_action_flags = 0;
	globals.cpu_monitor.set_alarm_threshold = 92;
	globals.cpu_monitor.clear_alarm_threshold = 82;

	if (load_config() != FTDM_SUCCESS) {
		globals.running = 0;
		ftdm_log(FTDM_LOG_ERROR, "FreeTDM global configuration failed!\n");
		return FTDM_FAIL;
	}

	if (globals.cpu_monitor.enabled) {
		ftdm_log(FTDM_LOG_INFO, "CPU Monitor is running interval:%d set-thres:%d clear-thres:%d\n", 
					globals.cpu_monitor.interval, 
					globals.cpu_monitor.set_alarm_threshold, 
					globals.cpu_monitor.clear_alarm_threshold);

		if (ftdm_cpu_monitor_start() != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
	}


	return FTDM_SUCCESS;
}

FT_DECLARE(uint32_t) ftdm_running(void)
{
	return globals.running;
}


FT_DECLARE(ftdm_status_t) ftdm_global_destroy(void)
{
	ftdm_span_t *sp;

	time_end();

	/* many freetdm event loops rely on this variable to decide when to stop, do this first */
	globals.running = 0;	

	/* stop the scheduling thread */
	ftdm_free_sched_stop();

	/* stop the cpu monitor thread */
	ftdm_cpu_monitor_stop();

	/* now destroy channels and spans */
	globals.span_index = 0;

	ftdm_span_close_all();
	
	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp;) {
		ftdm_span_t *cur_span = sp;
		sp = sp->next;

		if (cur_span) {
			if (ftdm_test_flag(cur_span, FTDM_SPAN_CONFIGURED)) {
				ftdm_span_destroy(cur_span);
			}

			hashtable_remove(globals.span_hash, (void *)cur_span->name);
			ftdm_safe_free(cur_span->dtmf_hangup);
			ftdm_safe_free(cur_span->type);
			ftdm_safe_free(cur_span->name);
			ftdm_safe_free(cur_span);
			cur_span = NULL;
		}
	}
	globals.spans = NULL;
	ftdm_mutex_unlock(globals.span_mutex);

	/* destroy signaling and io modules */
	ftdm_unload_modules();

	ftdm_global_set_logger( NULL );

	/* finally destroy the globals */
	ftdm_mutex_lock(globals.mutex);
	ftdm_sched_destroy(&globals.timingsched);
	hashtable_destroy(globals.interface_hash);
	hashtable_destroy(globals.module_hash);	
	hashtable_destroy(globals.span_hash);
	hashtable_destroy(globals.group_hash);
	ftdm_mutex_unlock(globals.mutex);
	ftdm_mutex_destroy(&globals.mutex);
	ftdm_mutex_destroy(&globals.span_mutex);
	ftdm_mutex_destroy(&globals.group_mutex);
	ftdm_mutex_destroy(&globals.call_id_mutex);

	memset(&globals, 0, sizeof(globals));
	return FTDM_SUCCESS;
}


FT_DECLARE(uint32_t) ftdm_separate_string(char *buf, char delim, char **array, int arraylen)
{
	int argc;
	char *ptr;
	int quot = 0;
	char qc = '\'';
	int x;

	if (!buf || !array || !arraylen) {
		return 0;
	}

	memset(array, 0, arraylen * sizeof(*array));

	ptr = buf;

	/* we swallow separators that are contiguous */
	while (*ptr == delim) ptr++;

	for (argc = 0; *ptr && (argc < arraylen - 1); argc++) {
		array[argc] = ptr;
		for (; *ptr; ptr++) {
			if (*ptr == qc) {
				if (quot) {
					quot--;
				} else {
					quot++;
				}
			} else if ((*ptr == delim) && !quot) {
				*ptr++ = '\0';
				/* we swallow separators that are contiguous */
				while (*ptr == delim) ptr++;
				break;
			}
		}
	}

	if (*ptr) {
		array[argc++] = ptr;
	}

	/* strip quotes */
	for (x = 0; x < argc; x++) {
		char *p = array[x];
		while((p = strchr(array[x], qc))) {
			memmove(p, p+1, strlen(p));
			p++;
		}
	}

	return argc;
}

FT_DECLARE(void) ftdm_bitstream_init(ftdm_bitstream_t *bsp, uint8_t *data, uint32_t datalen, ftdm_endian_t endian, uint8_t ss)
{
	memset(bsp, 0, sizeof(*bsp));
	bsp->data = data;
	bsp->datalen = datalen;
	bsp->endian = endian;
	bsp->ss = ss;
	
	if (endian < 0) {
		bsp->top = bsp->bit_index = 7;
		bsp->bot = 0;
	} else {
		bsp->top = bsp->bit_index = 0;
		bsp->bot = 7;
	}

}

FT_DECLARE(int8_t) ftdm_bitstream_get_bit(ftdm_bitstream_t *bsp)
{
	int8_t bit = -1;
	

	if (bsp->byte_index >= bsp->datalen) {
		goto done;
	}

	if (bsp->ss) {
		if (!bsp->ssv) {
			bsp->ssv = 1;
			return 0;
		} else if (bsp->ssv == 2) {
			bsp->byte_index++;
			bsp->ssv = 0;
			return 1;
		}
	}




	bit = (bsp->data[bsp->byte_index] >> (bsp->bit_index)) & 1;
	
	if (bsp->bit_index == bsp->bot) {
		bsp->bit_index = bsp->top;
		if (bsp->ss) {
			bsp->ssv = 2;
			goto done;
		} 

		if (++bsp->byte_index > bsp->datalen) {
			bit = -1;
			goto done;
		}
		
	} else {
		bsp->bit_index = bsp->bit_index + bsp->endian;
	}


 done:
	return bit;
}

FT_DECLARE(void) print_hex_bytes(uint8_t *data, ftdm_size_t dlen, char *buf, ftdm_size_t blen)
{
	char *bp = buf;
	uint8_t *byte = data;
	uint32_t i, j = 0;

	if (blen < (dlen * 3) + 2) {
        return;
    }

	*bp++ = '[';
	j++;

	for(i = 0; i < dlen; i++) {
		snprintf(bp, blen-j, "%02x ", *byte++);
		bp += 3;
		j += 3;
	}

	*--bp = ']';

}

FT_DECLARE(void) print_bits(uint8_t *b, int bl, char *buf, int blen, ftdm_endian_t e, uint8_t ss)
{
	ftdm_bitstream_t bs;
	int j = 0, c = 0;
	int8_t bit;
	uint32_t last;

	if (blen < (bl * 10) + 2) {
        return;
    }

	ftdm_bitstream_init(&bs, b, bl, e, ss);
	last = bs.byte_index;	
	while((bit = ftdm_bitstream_get_bit(&bs)) > -1) {
		buf[j++] = bit ? '1' : '0';
		if (bs.byte_index != last) {
			buf[j++] = ' ';
			last = bs.byte_index;
			if (++c == 8) {
				buf[j++] = '\n';
				c = 0;
			}
		}
	}

}



FT_DECLARE_NONSTD(ftdm_status_t) ftdm_console_stream_raw_write(ftdm_stream_handle_t *handle, uint8_t *data, ftdm_size_t datalen)
{
	ftdm_size_t need = handle->data_len + datalen;
	
	if (need >= handle->data_size) {
		void *new_data;
		need += handle->alloc_chunk;

		if (!(new_data = realloc(handle->data, need))) {
			return FTDM_MEMERR;
		}

		handle->data = new_data;
		handle->data_size = need;
	}

	memcpy((uint8_t *) (handle->data) + handle->data_len, data, datalen);
	handle->data_len += datalen;
	handle->end = (uint8_t *) (handle->data) + handle->data_len;
	*(uint8_t *)handle->end = '\0';

	return FTDM_SUCCESS;
}

FT_DECLARE(int) ftdm_vasprintf(char **ret, const char *fmt, va_list ap) /* code from switch_apr.c */
{
#ifdef HAVE_VASPRINTF
	return vasprintf(ret, fmt, ap);
#else
	char *buf;
	int len;
	size_t buflen;
	va_list ap2;
	char *tmp = NULL;

#ifdef _MSC_VER
#if _MSC_VER >= 1500
	/* hack for incorrect assumption in msvc header files for code analysis */
	__analysis_assume(tmp);
#endif
	ap2 = ap;
#else
	va_copy(ap2, ap);
#endif

	len = vsnprintf(tmp, 0, fmt, ap2);

	if (len > 0 && (buf = ftdm_malloc((buflen = (size_t) (len + 1)))) != NULL) {
		len = vsnprintf(buf, buflen, fmt, ap);
		*ret = buf;
	} else {
		*ret = NULL;
		len = -1;
	}

	va_end(ap2);
	return len;
#endif
}

FT_DECLARE_NONSTD(ftdm_status_t) ftdm_console_stream_write(ftdm_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	char *buf = handle->data;
	char *end = handle->end;
	int ret = 0;
	char *data = NULL;

	if (handle->data_len >= handle->data_size) {
		return FTDM_FAIL;
	}

	va_start(ap, fmt);
	ret = ftdm_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		ftdm_size_t remaining = handle->data_size - handle->data_len;
		ftdm_size_t need = strlen(data) + 1;

		if ((remaining < need) && handle->alloc_len) {
			ftdm_size_t new_len;
			void *new_data;

			new_len = handle->data_size + need + handle->alloc_chunk;
			if ((new_data = ftdm_realloc(handle->data, new_len))) {
				handle->data_size = handle->alloc_len = new_len;
				handle->data = new_data;
				buf = handle->data;
				remaining = handle->data_size - handle->data_len;
				handle->end = (uint8_t *) (handle->data) + handle->data_len;
				end = handle->end;
			} else {
				ftdm_log(FTDM_LOG_CRIT, "Memory Error!\n");
				ftdm_safe_free(data);
				return FTDM_FAIL;
			}
		}

		if (remaining < need) {
			ret = -1;
		} else {
			ret = 0;
			snprintf(end, remaining, "%s", data);
			handle->data_len = strlen(buf);
			handle->end = (uint8_t *) (handle->data) + handle->data_len;
		}
		ftdm_safe_free(data);
	}

	return ret ? FTDM_FAIL : FTDM_SUCCESS;
}

FT_DECLARE(char *) ftdm_strdup(const char *str)
{
	ftdm_size_t len = strlen(str) + 1;
	void *new = ftdm_malloc(len);

	if (!new) {
		return NULL;
	}

	return (char *)memcpy(new, str, len);
}

FT_DECLARE(char *) ftdm_strndup(const char *str, ftdm_size_t inlen)
{
	char *new = NULL;
	ftdm_size_t len = strlen(str) + 1;
	if (len > (inlen+1)) {
		len = inlen+1;
	}
	new = (char *)ftdm_malloc(len);

	if (!new) {
		return NULL;
	}
	
	memcpy(new, str, len-1);
	new[len-1] = 0;
	return new;
}

static ftdm_status_t ftdm_call_set_call_id(ftdm_channel_t *fchan, ftdm_caller_data_t *caller_data)
{
	uint32_t current_call_id;
	
	ftdm_assert_return(!caller_data->call_id, FTDM_FAIL, "Overwriting non-cleared call-id\n");

	ftdm_mutex_lock(globals.call_id_mutex);

	current_call_id = globals.last_call_id;

	for (current_call_id = globals.last_call_id + 1; 
	     current_call_id != globals.last_call_id; 
	     current_call_id++ ) {
		if (current_call_id > MAX_CALLIDS) {
			current_call_id = 1;
		}
		if (globals.call_ids[current_call_id] == NULL) {
			break;
		}
	}

	ftdm_assert_return(globals.call_ids[current_call_id] == NULL, FTDM_FAIL, "We ran out of call ids\n"); 

	globals.last_call_id = current_call_id;
	caller_data->call_id = current_call_id;

	globals.call_ids[current_call_id] = caller_data;
	caller_data->fchan = fchan;

	ftdm_mutex_unlock(globals.call_id_mutex);
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_call_clear_call_id(ftdm_caller_data_t *caller_data)
{
	if (caller_data->call_id) {
		ftdm_assert_return((caller_data->call_id <= MAX_CALLIDS), FTDM_FAIL, "Cannot clear call with invalid call-id\n");
	} else {
		/* there might not be a call at all */
		return FTDM_SUCCESS;
	}

	ftdm_mutex_lock(globals.call_id_mutex);
	if (globals.call_ids[caller_data->call_id]) {
		ftdm_log(FTDM_LOG_DEBUG, "Cleared call with id %u\n", caller_data->call_id);
		globals.call_ids[caller_data->call_id] = NULL;
		caller_data->call_id = 0;
	} else {
		ftdm_log(FTDM_LOG_CRIT, "call-id did not exist %u\n", caller_data->call_id);
	} 
	ftdm_mutex_unlock(globals.call_id_mutex);

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_sigmsg_get_raw_data(ftdm_sigmsg_t *sigmsg, void **data, ftdm_size_t *datalen)
{
	if (!sigmsg || !sigmsg->raw.len) {
		return FTDM_FAIL;
	}
	
	*data = sigmsg->raw.data;
	*datalen = sigmsg->raw.len;
	
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_sigmsg_get_raw_data_detached(ftdm_sigmsg_t *sigmsg, void **data, ftdm_size_t *datalen)
{
	if (!sigmsg || !sigmsg->raw.len) {
		return FTDM_FAIL;
	}

	*data = sigmsg->raw.data;
	*datalen = sigmsg->raw.len;

	sigmsg->raw.data = NULL;
	sigmsg->raw.len = 0;
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_sigmsg_set_raw_data(ftdm_sigmsg_t *sigmsg, void *data, ftdm_size_t datalen)
{
	ftdm_assert_return(sigmsg, FTDM_FAIL, "Trying to set raw data on a NULL event\n");
	ftdm_assert_return(!sigmsg->raw.len, FTDM_FAIL, "Overwriting existing raw data\n");
	ftdm_assert_return(datalen, FTDM_FAIL, "Data length not set\n");

	sigmsg->raw.data = data;
	sigmsg->raw.len = datalen;
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_usrmsg_get_raw_data(ftdm_usrmsg_t *usrmsg, void **data, ftdm_size_t *datalen)
{
	if (!usrmsg || !usrmsg->raw.len) {
		return FTDM_FAIL;
	}
	
	*data = usrmsg->raw.data;
	*datalen = usrmsg->raw.len;
	
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_usrmsg_set_raw_data(ftdm_usrmsg_t *usrmsg, void *data, ftdm_size_t datalen)
{
	ftdm_assert_return(usrmsg, FTDM_FAIL, "Trying to set raw data on a NULL event\n");
	ftdm_assert_return(!usrmsg->raw.len, FTDM_FAIL, "Overwriting existing raw data\n");
	ftdm_assert_return(datalen, FTDM_FAIL, "Data length not set\n");

	usrmsg->raw.data = data;
	usrmsg->raw.len = datalen;
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_save_usrmsg(ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg)
{
	ftdm_assert_return(!ftdmchan->usrmsg, FTDM_FAIL, "Info from previous event was not cleared\n");
	if (usrmsg) {
		/* Copy sigmsg from user to internal copy so user can set new variables without race condition */
		ftdmchan->usrmsg = ftdm_calloc(1, sizeof(ftdm_usrmsg_t));
		memcpy(ftdmchan->usrmsg, usrmsg, sizeof(ftdm_usrmsg_t));
		
		if (usrmsg->raw.data) {
			usrmsg->raw.data = NULL;
			usrmsg->raw.len = 0;
		}
		if (usrmsg->variables) {
			usrmsg->variables = NULL;
		}
	}
	return FTDM_SUCCESS;	
}

FT_DECLARE(ftdm_status_t) ftdm_sigmsg_free(ftdm_sigmsg_t **sigmsg)
{
	if (!*sigmsg) {
		return FTDM_SUCCESS;
	}

	if ((*sigmsg)->variables) {
		hashtable_destroy((*sigmsg)->variables);
		(*sigmsg)->variables = NULL;
	}

	if ((*sigmsg)->raw.data) {
		ftdm_safe_free((*sigmsg)->raw.data);
		(*sigmsg)->raw.data = NULL;
		(*sigmsg)->raw.len = 0;
	}

	ftdm_safe_free(*sigmsg);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_usrmsg_free(ftdm_usrmsg_t **usrmsg)
{
	if (!*usrmsg) {
		return FTDM_SUCCESS;
	}

	if ((*usrmsg)->variables) {
		hashtable_destroy((*usrmsg)->variables);
		(*usrmsg)->variables = NULL;
	}

	if ((*usrmsg)->raw.data) {
		ftdm_safe_free((*usrmsg)->raw.data);
		(*usrmsg)->raw.data = NULL;
		(*usrmsg)->raw.len = 0;
	}

	ftdm_safe_free(*usrmsg);
	return FTDM_SUCCESS;
}



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
