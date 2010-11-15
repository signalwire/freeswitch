/*
 * Copyright (c) 2007, Anthony Minessale II
 * Copyright (c) 2010, Stefan Knoblich <s.knoblich@axsentis.de>
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
#include "private/ftdm_core.h"
#include "ftmod_libpri.h"

static void _ftdm_channel_set_state_force(ftdm_channel_t *chan, const ftdm_channel_state_t state)
{
	assert(chan);
	chan->state = state;
}

/**
 * \brief Unloads libpri IO module
 * \return Success
 */
static FIO_IO_UNLOAD_FUNCTION(ftdm_libpri_unload)
{
	return FTDM_SUCCESS;
}

/**
 * \brief Returns the signalling status on a channel
 * \param ftdmchan Channel to get status on
 * \param status	Pointer to set signalling status
 * \return Success or failure
 */

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(isdn_get_channel_sig_status)
{
	*status = FTDM_SIG_STATE_DOWN;

	ftdm_libpri_data_t *isdn_data = ftdmchan->span->signal_data;
	if (ftdm_test_flag(&(isdn_data->spri), LPWRAP_PRI_READY)) {
		*status = FTDM_SIG_STATE_UP;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Returns the signalling status on a span
 * \param span Span to get status on
 * \param status	Pointer to set signalling status
 * \return Success or failure
 */

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(isdn_get_span_sig_status)
{
	*status = FTDM_SIG_STATE_DOWN;

	ftdm_libpri_data_t *isdn_data = span->signal_data;
	if (ftdm_test_flag(&(isdn_data->spri), LPWRAP_PRI_READY)) {
		*status = FTDM_SIG_STATE_UP;
	}
	return FTDM_SUCCESS;
}


/**
 * \brief Starts a libpri channel (outgoing call)
 * \param ftdmchan Channel to initiate call on
 * \return Success or failure
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(isdn_outgoing_call)
{
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALING);
	return status;
}

/**
 * \brief Requests an libpri channel on a span (outgoing call)
 * \param span Span where to get a channel (unused)
 * \param chan_id Specific channel to get (0 for any) (unused)
 * \param direction Call direction (unused)
 * \param caller_data Caller information (unused)
 * \param ftdmchan Channel to initialise (unused)
 * \return Failure
 */
static FIO_CHANNEL_REQUEST_FUNCTION(isdn_channel_request)
{
	return FTDM_FAIL;
}

#ifdef WIN32
/**
 * \brief Logs a libpri error
 * \param s Error string
 */
static void s_pri_error(char *s)
#else
/**
 * \brief Logs a libpri error
 * \param pri libpri structure (unused)
 * \param s Error string
 */
static void s_pri_error(struct pri *pri, char *s)
#endif
{
	ftdm_log(FTDM_LOG_ERROR, "%s", s);
}

#ifdef WIN32
/**
 * \brief Logs a libpri message
 * \param s Message string
 */
static void s_pri_message(char *s)
#else
/**
 * \brief Logs a libpri message
 * \param pri libpri structure (unused)
 * \param s Message string
 */
static void s_pri_message(struct pri *pri, char *s)
#endif
{
	ftdm_log(FTDM_LOG_DEBUG, "%s", s);
}

#define PRI_DEBUG_Q921_ALL	(PRI_DEBUG_Q921_RAW | PRI_DEBUG_Q921_DUMP | PRI_DEBUG_Q921_STATE)
#define PRI_DEBUG_Q931_ALL	(PRI_DEBUG_Q931_DUMP | PRI_DEBUG_Q931_STATE | PRI_DEBUG_Q931_ANOMALY)

static const struct ftdm_libpri_debug {
	const char *name;
	const int   flags;
} ftdm_libpri_debug[] = {
	/* NOTE: order is important for print_debug() */
	{ "q921_all",     PRI_DEBUG_Q921_ALL   },
	{ "q921_raw",     PRI_DEBUG_Q921_RAW   },
	{ "q921_dump",    PRI_DEBUG_Q921_DUMP  },
	{ "q921_state",   PRI_DEBUG_Q921_STATE },

	{ "q931_all",     PRI_DEBUG_Q931_ALL     },
	{ "q931_dump",    PRI_DEBUG_Q931_DUMP    },
	{ "q931_state",   PRI_DEBUG_Q931_STATE   },
	{ "q931_anomaly", PRI_DEBUG_Q931_ANOMALY },

	{ "config",       PRI_DEBUG_CONFIG },
	{ "apdu",         PRI_DEBUG_APDU   },
	{ "aoc",          PRI_DEBUG_AOC    }
};

/**
 * \brief Parses a debug string to flags
 * \param in Debug string to parse for
 * \return Flags or -1 if nothing matched
 */
static int parse_debug(const char *in, uint32_t *flags)
{
	int res = -1;
	int i;

	if (!in || !flags)
		return -1;

	if (!strcmp(in, "all")) {
		*flags = PRI_DEBUG_ALL;
		return 0;
	}
	if (strstr(in, "none")) {
		*flags = 0;
		return 0;
	}

	for (i = 0; i < ftdm_array_len(ftdm_libpri_debug); i++) {
		if (strstr(in, ftdm_libpri_debug[i].name)) {
			*flags |= ftdm_libpri_debug[i].flags;
			res = 0;
		}
	}
	return res;
}

static int print_debug(uint32_t flags, char *tmp, const int size)
{
	int offset = 0;
	int res = 0;
	int i;

	if ((flags & PRI_DEBUG_ALL) == PRI_DEBUG_ALL) {
		strcat(tmp, "all");
		return 0;
	}
	else if (!flags) {
		strcat(tmp, "none");
		return 0;
	}

	for (i = 0; i < ftdm_array_len(ftdm_libpri_debug); i++) {
		if ((flags & ftdm_libpri_debug[i].flags) == ftdm_libpri_debug[i].flags) {
			res = snprintf(&tmp[offset], size - offset, "%s,", ftdm_libpri_debug[i].name);
			if (res <= 0 || res == (size - offset))
				goto out;
			offset += res;
			flags  &= ~ftdm_libpri_debug[i].flags;	/* remove detected flags to make *_all work correctly */
		}
	}

out:
	tmp[offset - 1] = '\0';
	return 0;
}

static ftdm_status_t ftdm_libpri_start(ftdm_span_t *span);
static ftdm_io_interface_t ftdm_libpri_interface;

static const char *ftdm_libpri_usage =
	"Usage:\n"
	"libpri kill <span>\n"
	"libpri debug <span> [all|none|flag,...flagN]\n"
	"\n"
	"Possible debug flags:\n"
	"\tq921_raw     - Q.921 Raw messages\n"
	"\tq921_dump    - Q.921 Decoded messages\n"
	"\tq921_state   - Q.921 State machine changes\n"
	"\tq921_all     - Enable all Q.921 debug options\n"
	"\n"
	"\tq931_dump    - Q.931 Messages\n"
	"\tq931_state   - Q.931 State machine changes\n"
	"\tq931_anomaly - Q.931 Anomalies\n"
	"\tq931_all     - Enable all Q.931 debug options\n"
	"\n"
	"\tapdu         - Application protocol data unit\n"
	"\taoc          - Advice of Charge messages\n"
	"\tconfig       - Configuration\n"
	"\n"
	"\tnone         - Disable debugging\n"
	"\tall          - Enable all debug options\n";

/**
 * \brief API function to kill or debug a libpri span
 * \param stream API stream handler
 * \param data String containing argurments
 * \return Flags
 */
static FIO_API_FUNCTION(ftdm_libpri_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc == 1) {
		if (!strcasecmp(argv[0], "help") || !strcasecmp(argv[0], "usage")) {
			stream->write_function(stream, ftdm_libpri_usage);
			goto done;
		}
	}

	if (argc == 2) {
		if (!strcasecmp(argv[0], "kill")) {
			int span_id = atoi(argv[1]);
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
					goto done;
				}

				ftdm_clear_flag(&(isdn_data->spri), LPWRAP_PRI_READY);
				stream->write_function(stream, "%s: +OK killed.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
				goto done;
			}
		}
	}

	if (argc >= 2) {
		if (!strcasecmp(argv[0], "debug")) {
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;
				uint32_t flags = 0;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
					goto done;
				}

				if (argc == 2) {
					char tmp[100] = { 0 };
					print_debug(pri_get_debug(isdn_data->spri.pri), tmp, sizeof(tmp));
					stream->write_function(stream, "%s: +OK current debug flags: '%s'\n", __FILE__, tmp);
					goto done;
				}

				if (parse_debug(argv[2], &flags) == -1) {
					stream->write_function(stream, "%s: -ERR invalid debug flags given\n", __FILE__);
					goto done;
				}

				pri_set_debug(isdn_data->spri.pri, flags);
				stream->write_function(stream, "%s: +OK debug %s.\n", __FILE__, (flags) ? "enabled" : "disabled");
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
				goto done;
			}
		}

	}
	stream->write_function(stream, "%s: -ERR invalid command.\n", __FILE__);

done:
	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;
}


/**
 * \brief Loads libpri IO module
 * \param fio FreeTDM IO interface
 * \return Success
 */
static FIO_IO_LOAD_FUNCTION(ftdm_libpri_io_init)
{
	assert(fio != NULL);

	memset(&ftdm_libpri_interface, 0, sizeof(ftdm_libpri_interface));
	ftdm_libpri_interface.name = "libpri";
	ftdm_libpri_interface.api  = &ftdm_libpri_api;

	*fio = &ftdm_libpri_interface;

	return FTDM_SUCCESS;
}

/**
 * \brief Loads libpri signaling module
 * \param fio FreeTDM IO interface
 * \return Success
 */
static FIO_SIG_LOAD_FUNCTION(ftdm_libpri_init)
{
	pri_set_error(s_pri_error);
	pri_set_message(s_pri_message);
	return FTDM_SUCCESS;
}

/**
 * \brief libpri state map
 */
static ftdm_state_map_t isdn_state_map = {
	{
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
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
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},

		/****************************************/
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_DIALTONE, FTDM_CHANNEL_STATE_RING, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DIALTONE, FTDM_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, 
			 FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_UP, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
		},
	}
};

/**
 * \brief Handler for channel state change
 * \param ftdmchan Channel to handle
 */
static __inline__ void state_advance(ftdm_channel_t *chan)
{
	ftdm_libpri_data_t *isdn_data = chan->span->signal_data;
	q931_call *call = (q931_call *)chan->call_data;
	ftdm_status_t status;
	ftdm_sigmsg_t sig;

	ftdm_log(FTDM_LOG_DEBUG, "%d:%d STATE [%s]\n",
			ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan), ftdm_channel_get_state_str(chan));

#if 0
	if (!ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND) && !call) {
		ftdm_log(FTDM_LOG_WARNING, "NO CALL!!!!\n");
	}
#endif

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdm_channel_get_id(chan);
	sig.span_id = ftdm_channel_get_span_id(chan);
	sig.channel = chan;

	switch (ftdm_channel_get_state(chan)) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			chan->call_data = NULL;
			ftdm_channel_done(chan);
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS;
				if ((status = ftdm_span_send_signal(ftdm_channel_get_span(chan), &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_progress(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 1);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				if ((status = ftdm_span_send_signal(ftdm_channel_get_span(chan), &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_proceeding(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 1);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_RING:
		{
			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				if (call) {
					pri_acknowledge(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 0);
					sig.event_id = FTDM_SIGEVENT_START;
					if ((status = ftdm_span_send_signal(ftdm_channel_get_span(chan), &sig) != FTDM_SUCCESS)) {
						ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
					}
				} else {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
				}
			}
		}
		break;

	case FTDM_CHANNEL_STATE_RESTART:
		{
			chan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_UNSPECIFIED;
			sig.event_id = FTDM_SIGEVENT_RESTART;
			status = ftdm_span_send_signal(ftdm_channel_get_span(chan), &sig);
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;

	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_UP;
				if ((status = ftdm_span_send_signal(ftdm_channel_get_span(chan), &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_answer(isdn_data->spri.pri, call, 0, 1);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_DIALING:
		if (isdn_data) {
			ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);
			struct pri_sr *sr;
			int ton;

			if (!(call = pri_new_call(isdn_data->spri.pri))) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create new call on channel %d:%d\n",
					ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
				/* TODO: set hangup cause? */
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
				return;
			}

			ton = caller_data->dnis.type;
			switch (ton) {
			case FTDM_TON_NATIONAL:
				ton = PRI_NATIONAL_ISDN;
				break;
			case FTDM_TON_INTERNATIONAL:
				ton = PRI_INTERNATIONAL_ISDN;
				break;
			case FTDM_TON_SUBSCRIBER_NUMBER:
				ton = PRI_LOCAL_ISDN;
				break;
			default:
				ton = isdn_data->ton;
			}

			chan->call_data = call;

			sr = pri_sr_new();
			if (!sr) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create new setup request on channel %d:%d\n",
					ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
				/* TODO: handle error properly */
			}
			assert(sr);

			pri_sr_set_channel(sr, ftdm_channel_get_id(chan), 0, 0);
			pri_sr_set_bearer(sr, PRI_TRANS_CAP_SPEECH, isdn_data->layer1);

			pri_sr_set_called(sr, caller_data->dnis.digits, ton, 1);
			pri_sr_set_caller(sr, caller_data->cid_num.digits,
					((isdn_data->opts & FTMOD_LIBPRI_OPT_OMIT_DISPLAY_IE) ? NULL : caller_data->cid_name),
					ton,
					((caller_data->pres != 1) ? PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN : PRES_PROHIB_USER_NUMBER_NOT_SCREENED));

			if (!(isdn_data->opts & FTMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE)) {
				pri_sr_set_redirecting(sr, caller_data->cid_num.digits, ton,
					PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN, PRI_REDIR_UNCONDITIONAL);
			}
#ifdef HAVE_LIBPRI_AOC
			if (isdn_data->opts & FTMOD_LIBPRI_OPT_FACILITY_AOC) {
				/* request AOC on call */
				pri_sr_set_aoc_charging_request(sr, (PRI_AOC_REQUEST_S | PRI_AOC_REQUEST_E | PRI_AOC_REQUEST_D));
				ftdm_log(FTDM_LOG_DEBUG, "Requesting AOC-S/D/E on call\n");
			}
#endif
			if (pri_setup(isdn_data->spri.pri, call, sr)) {
				caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
			}

			pri_sr_free(sr);
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP:
		{
			if (call) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

				pri_hangup(isdn_data->spri.pri, call, caller_data->hangup_cause);
				pri_destroycall(isdn_data->spri.pri, call);

				chan->call_data = NULL;
			}
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		break;

	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = FTDM_SIGEVENT_STOP;
			status = ftdm_span_send_signal(ftdm_channel_get_span(chan), &sig);
			/* user moves us to HANGUP and from there we go to DOWN */
		}
	default:
		break;
	}
}

/**
 * \brief Checks current state on a span
 * \param span Span to check status on
 */
static __inline__ void check_state(ftdm_span_t *span)
{
	if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE)) {
		uint32_t j;

		ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);

		for (j = 1; j <= ftdm_span_get_chan_count(span); j++) {
			ftdm_channel_t *chan = ftdm_span_get_channel(span, j);

			if (ftdm_test_flag(chan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_channel_lock(chan);

				ftdm_clear_flag(chan, FTDM_CHANNEL_STATE_CHANGE);
				state_advance(chan);
				ftdm_channel_complete_state(chan);

				ftdm_channel_unlock(chan);
			}
		}
	}
}

/**
 * \brief Handler for libpri information event (incoming call?)
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_info(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_log(FTDM_LOG_DEBUG, "number is: %s\n", pevent->ring.callednum);

	if (strlen(pevent->ring.callednum) > 3) {
		ftdm_log(FTDM_LOG_DEBUG, "final number is: %s\n", pevent->ring.callednum);
		pri_answer(spri->pri, pevent->ring.call, 0, 1);
	}
	return 0;
}

/**
 * \brief Handler for libpri hangup event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_hangup(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->hangup.channel);
	q931_call *call = NULL;

	if (!chan) {
		ftdm_log(FTDM_LOG_CRIT, "-- Hangup on channel %d:%d %s but it's not in use?\n", ftdm_span_get_id(spri->span), pevent->hangup.channel);
		return 0;
	}

	ftdm_channel_lock(chan);

	if (ftdm_channel_get_state(chan) >= FTDM_CHANNEL_STATE_TERMINATING) {
		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Ignoring remote hangup in state %s\n", ftdm_channel_get_state_str(chan));
		goto done;
	}

	if (!chan->call_data) {
		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Ignoring remote hangup in state %s with no call data\n", ftdm_channel_get_state_str(chan));
		goto done;
	}

	call = (q931_call *)chan->call_data;

	ftdm_log(FTDM_LOG_DEBUG, "-- Hangup on channel %d:%d\n", ftdm_span_get_id(spri->span), pevent->hangup.channel);

	pri_release(spri->pri, call, 0);
	pri_destroycall(spri->pri, call);

	chan->caller_data.hangup_cause = pevent->hangup.cause;
	chan->call_data = NULL;
	ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_TERMINATING);

done:
	ftdm_channel_unlock(chan);
	return 0;
}

/**
 * \brief Handler for libpri answer event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_answer(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->answer.channel);

	if (chan) {
		ftdm_log(FTDM_LOG_DEBUG, "-- Answer on channel %d:%d\n", ftdm_span_get_id(span), pevent->answer.channel);
		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_UP);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Answer on channel %d:%d but it's not in the span?\n",
			ftdm_span_get_id(span), pevent->answer.channel);
	}
	return 0;
}

/**
 * \brief Handler for libpri proceed event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_proceed(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->answer.channel);

	if (chan) {
		ftdm_log(FTDM_LOG_DEBUG, "-- Proceeding on channel %d:%d\n", ftdm_span_get_id(span), pevent->proceeding.channel);
		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Proceeding on channel %d:%d but it's not in the span?\n",
						ftdm_span_get_id(span), pevent->proceeding.channel);
	}
	return 0;
}

/**
 * \brief Handler for libpri ringing event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_ringing(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->ringing.channel);

	if (chan) {
		ftdm_log(FTDM_LOG_DEBUG, "-- Ringing on channel %d:%d\n", ftdm_span_get_id(span), pevent->ringing.channel);
		/* we may get on_ringing even when we're already in FTDM_CHANNEL_STATE_PROGRESS_MEDIA */
		if (ftdm_channel_get_state(chan) == FTDM_CHANNEL_STATE_PROGRESS_MEDIA) {
			/* dont try to move to STATE_PROGRESS to avoid annoying veto warning */
			return 0;
		}
		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROGRESS);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Ringing on channel %d:%d but it's not in the span?\n",
			ftdm_span_get_id(span), pevent->ringing.channel);
	}

	return 0;
}

/**
 * \brief Handler for libpri ring event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0 on success
 */
static int on_ring(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->ring.channel);
	ftdm_caller_data_t *caller_data = NULL;
	int ret = 0;

	if (!chan || ftdm_channel_get_state(chan) != FTDM_CHANNEL_STATE_DOWN || ftdm_test_flag(chan, FTDM_CHANNEL_INUSE)) {
		ftdm_log(FTDM_LOG_WARNING, "--Duplicate Ring on channel %d:%d (ignored)\n", ftdm_span_get_id(span), pevent->ring.channel);
		goto done;
	}

	if (ftdm_channel_open_chan(chan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_WARNING, "--Failure opening channel %d:%d (ignored)\n", ftdm_span_get_id(span), pevent->ring.channel);
		goto done;
	}

	ftdm_log(FTDM_LOG_NOTICE, "-- Ring on channel %d:%d (from %s to %s)\n", ftdm_span_get_id(span), pevent->ring.channel,
					  pevent->ring.callingnum, pevent->ring.callednum);

	caller_data = ftdm_channel_get_caller_data(chan);

	memset(caller_data, 0, sizeof(*caller_data));

	ftdm_set_string(caller_data->cid_num.digits, (char *)pevent->ring.callingnum);
	ftdm_set_string(caller_data->ani.digits, (char *)pevent->ring.callingani);
	ftdm_set_string(caller_data->dnis.digits, (char *)pevent->ring.callednum);
	ftdm_set_string(caller_data->rdnis.digits, (char *)pevent->ring.redirectingnum);

	if (!ftdm_strlen_zero((char *)pevent->ring.callingname)) {
		ftdm_set_string(caller_data->cid_name, (char *)pevent->ring.callingname);
	} else {
		ftdm_set_string(caller_data->cid_name, (char *)pevent->ring.callingnum);
	}

	if (pevent->ring.ani2 >= 0) {
		snprintf(caller_data->aniII, 5, "%.2d", pevent->ring.ani2);
	}

	// scary to trust this pointer, you'd think they would give you a copy of the call data so you own it......
	/* hurr, this valid as along as nobody releases the call */
	chan->call_data = pevent->ring.call;

	ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RING);

done:
	return ret;
}

/**
 * \brief Processes freetdm event
 * \param span Span on which the event was fired
 * \param event Event to be treated
 * \return Success or failure
 */
static __inline__ ftdm_status_t process_event(ftdm_span_t *span, ftdm_event_t *event)
{
	ftdm_alarm_flag_t alarmbits;

	ftdm_log(FTDM_LOG_DEBUG, "EVENT [%s][%d][%d:%d] STATE [%s]\n",
			ftdm_oob_event2str(event->enum_id),
			event->enum_id,
			ftdm_channel_get_span_id(event->channel),
			ftdm_channel_get_id(event->channel),
			ftdm_channel_get_state_str(event->channel));

	switch (event->enum_id) {
	case FTDM_OOB_ALARM_TRAP:
		{
			if (ftdm_channel_get_state(event->channel) != FTDM_CHANNEL_STATE_DOWN) {
				if (ftdm_channel_get_type(event->channel) == FTDM_CHAN_TYPE_B) {
					ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_RESTART);
				}
			}

			ftdm_set_flag(event->channel, FTDM_CHANNEL_SUSPENDED);

			ftdm_channel_get_alarms(event->channel, &alarmbits);
			ftdm_log(FTDM_LOG_WARNING, "channel %d:%d (%d:%d) has alarms! [%s]\n",
					ftdm_channel_get_span_id(event->channel), ftdm_channel_get_id(event->channel),
					ftdm_channel_get_ph_span_id(event->channel), ftdm_channel_get_ph_id(event->channel),
					ftdm_channel_get_last_error(event->channel));
		}
		break;
	case FTDM_OOB_ALARM_CLEAR:
		{
			ftdm_log(FTDM_LOG_WARNING, "channel %d:%d (%d:%d) alarms Cleared!\n",
					ftdm_channel_get_span_id(event->channel), ftdm_channel_get_id(event->channel),
					ftdm_channel_get_ph_span_id(event->channel), ftdm_channel_get_ph_id(event->channel));

			ftdm_clear_flag(event->channel, FTDM_CHANNEL_SUSPENDED);
			ftdm_channel_get_alarms(event->channel, &alarmbits);
		}
		break;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Checks for events on a span
 * \param span Span to check for events
 */
static __inline__ void check_events(ftdm_span_t *span)
{
	ftdm_status_t status;

	status = ftdm_span_poll_event(span, 5, NULL);

	switch (status) {
	case FTDM_SUCCESS:
		{
			ftdm_event_t *event;

			while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
				if (event->enum_id == FTDM_OOB_NOOP) {
					continue;
				}
				if (process_event(span, event) != FTDM_SUCCESS) {
					break;
				}
			}
		}
		break;

	case FTDM_FAIL:
		ftdm_log(FTDM_LOG_DEBUG, "Event Failure! %d\n", ftdm_running());
		ftdm_sleep(2000);
		break;

	default:
		break;
	}
}

/**
 * \brief Checks flags on a pri span
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \return 0 on success, -1 on error
 */
static int check_flags(lpwrap_pri_t *spri)
{
	ftdm_span_t *span = spri->span;

	if (!ftdm_running() || ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {
		return -1;
	}

	check_state(span);
	check_events(span);
	return 0;
}

/**
 * \brief Handler for libpri restart event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_restart(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->restart.channel);

	ftdm_log(FTDM_LOG_NOTICE, "-- Restarting %d:%d\n", ftdm_span_get_id(span), pevent->restart.channel);
	_ftdm_channel_set_state_force(spri->dchan, FTDM_CHANNEL_STATE_UP);

	if (!chan) {
		return 0;
	}

	if (pevent->restart.channel < 1) {
		ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);
	} else {
		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
	}
	return 0;
}

/*
 * FACILITY Advice-On-Charge handler
 */
#ifdef HAVE_LIBPRI_AOC
static const char *aoc_billing_id(const int id)
{
	switch (id) {
	case PRI_AOC_E_BILLING_ID_NOT_AVAILABLE:
		return "not available";
	case PRI_AOC_E_BILLING_ID_NORMAL:
		return "normal";
	case PRI_AOC_E_BILLING_ID_REVERSE:
		return "reverse";
	case PRI_AOC_E_BILLING_ID_CREDIT_CARD:
		return "credit card";
	case PRI_AOC_E_BILLING_ID_CALL_FORWARDING_UNCONDITIONAL:
		return "call forwarding unconditional";
	case PRI_AOC_E_BILLING_ID_CALL_FORWARDING_BUSY:
		return "call forwarding busy";
	case PRI_AOC_E_BILLING_ID_CALL_FORWARDING_NO_REPLY:
		return "call forwarding no reply";
	case PRI_AOC_E_BILLING_ID_CALL_DEFLECTION:
		return "call deflection";
	case PRI_AOC_E_BILLING_ID_CALL_TRANSFER:
		return "call transfer";
	default:
		return "unknown\n";
	}
}

static float aoc_money_amount(const struct pri_aoc_amount *amount)
{
	switch (amount->multiplier) {
	case PRI_AOC_MULTIPLIER_THOUSANDTH:
		return amount->cost * 0.001f;
	case PRI_AOC_MULTIPLIER_HUNDREDTH:
		return amount->cost * 0.01f;
	case PRI_AOC_MULTIPLIER_TENTH:
		return amount->cost * 0.1f;
	case PRI_AOC_MULTIPLIER_TEN:
		return amount->cost * 10.0f;
	case PRI_AOC_MULTIPLIER_HUNDRED:
		return amount->cost * 100.0f;
	case PRI_AOC_MULTIPLIER_THOUSAND:
		return amount->cost * 1000.0f;
	default:
		return amount->cost;
	}
}

static int handle_facility_aoc_s(const struct pri_subcmd_aoc_s *aoc_s)
{
	/* Left as an excercise to the reader */
	return 0;
}

static int handle_facility_aoc_d(const struct pri_subcmd_aoc_d *aoc_d)
{
	/* Left as an excercise to the reader */
	return 0;
}

static int handle_facility_aoc_e(const struct pri_subcmd_aoc_e *aoc_e)
{
	char tmp[1024] = { 0 };
	int x = 0, offset = 0;

	switch (aoc_e->charge) {
	case PRI_AOC_DE_CHARGE_FREE:
		strcat(tmp, "\tcharge-type: none\n");
		offset = strlen(tmp);
		break;

	case PRI_AOC_DE_CHARGE_CURRENCY:
		sprintf(tmp, "\tcharge-type: money\n\tcharge-amount: %.2f\n\tcharge-currency: %s\n",
				aoc_money_amount(&aoc_e->recorded.money.amount),
				aoc_e->recorded.money.currency);
		offset = strlen(tmp);
		break;

	case PRI_AOC_DE_CHARGE_UNITS:
		strcat(tmp, "\tcharge-type: units\n");
		offset = strlen(tmp);

		for (x = 0; x < aoc_e->recorded.unit.num_items; x++) {
			sprintf(&tmp[offset], "\tcharge-amount: %ld (type: %d)\n",
					aoc_e->recorded.unit.item[x].number,
					aoc_e->recorded.unit.item[x].type);
			offset += strlen(&tmp[offset]);
		}
		break;

	default:
		strcat(tmp, "\tcharge-type: not available\n");
		offset = strlen(tmp);
	}

	sprintf(&tmp[offset], "\tbilling-id: %s\n", aoc_billing_id(aoc_e->billing_id));
	offset += strlen(&tmp[offset]);

	strcat(&tmp[offset], "\tassociation-type: ");
	offset += strlen(&tmp[offset]);

	switch (aoc_e->associated.charging_type) {
	case PRI_AOC_E_CHARGING_ASSOCIATION_NOT_AVAILABLE:
		strcat(&tmp[offset], "not available\n");
		break;
	case PRI_AOC_E_CHARGING_ASSOCIATION_NUMBER:
		sprintf(&tmp[offset], "number\n\tassociation-number: %s\n", aoc_e->associated.charge.number.str);
		break;
	case PRI_AOC_E_CHARGING_ASSOCIATION_ID:
		sprintf(&tmp[offset], "id\n\tassociation-id: %d\n", aoc_e->associated.charge.id);
		break;
	default:
		strcat(&tmp[offset], "unknown\n");
	}

	ftdm_log(FTDM_LOG_INFO, "AOC-E:\n%s", tmp);
	return 0;
}

/**
 * \brief Handler for libpri facility events
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_facility(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	struct pri_event_facility *pfac = &pevent->facility;
	int i = 0;

	if (!pevent)
		return 0;

	ftdm_log(FTDM_LOG_DEBUG, "Got a FACILITY event on span %d:%d\n", ftdm_span_get_id(spri->span), pfac->channel);

	if (!pfac->subcmds || pfac->subcmds->counter_subcmd <= 0)
		return 0;

	for (i = 0; i < pfac->subcmds->counter_subcmd; i++) {
		struct pri_subcommand *sub = &pfac->subcmds->subcmd[i];
		int res = -1;

		switch (sub->cmd) {
		case PRI_SUBCMD_AOC_S:	/* AOC-S: Start of call */
			res = handle_facility_aoc_s(&sub->u.aoc_s);
			break;
		case PRI_SUBCMD_AOC_D:	/* AOC-D: During call */
			res = handle_facility_aoc_d(&sub->u.aoc_d);
			break;
		case PRI_SUBCMD_AOC_E:	/* AOC-E: End of call */
			res = handle_facility_aoc_e(&sub->u.aoc_e);
			break;
		case PRI_SUBCMD_AOC_CHARGING_REQ:
			ftdm_log(FTDM_LOG_NOTICE, "AOC Charging Request received\n");
			break;
		case PRI_SUBCMD_AOC_CHARGING_REQ_RSP:
			ftdm_log(FTDM_LOG_NOTICE, "AOC Charging Request Response received [aoc_s data: %s, req: %x, resp: %x]\n",
					sub->u.aoc_request_response.valid_aoc_s ? "yes" : "no",
					sub->u.aoc_request_response.charging_request,
					sub->u.aoc_request_response.charging_response);
			break;
		default:
			ftdm_log(FTDM_LOG_DEBUG, "FACILITY subcommand %d is not implemented, ignoring\n", sub->cmd);
		}

		ftdm_log(FTDM_LOG_DEBUG, "FACILITY subcommand %d handler returned %d\n", sub->cmd, res);
	}

	ftdm_log(FTDM_LOG_DEBUG, "Caught Event on span %d %u (%s)\n", ftdm_span_get_id(spri->span), event_type, lpwrap_pri_event_str(event_type));
	return 0;
}
#endif

/**
 * \brief Handler for libpri dchan up event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_dchan_up(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	if (!ftdm_test_flag(spri, LPWRAP_PRI_READY)) {
		ftdm_signaling_status_t status = FTDM_SIG_STATE_UP;
		ftdm_span_t *span = spri->span;
		ftdm_sigmsg_t sig;
		int i;

		ftdm_log(FTDM_LOG_INFO, "Span %d D-Channel UP!\n", ftdm_span_get_id(span));
		ftdm_set_flag(spri, LPWRAP_PRI_READY);
		ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);

		ftdm_log(FTDM_LOG_NOTICE, "%d:Signaling link status changed to %s\n", ftdm_span_get_id(span), ftdm_signaling_status2str(status));

		for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
			ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

			memset(&sig, 0, sizeof(sig));
			sig.span_id = ftdm_channel_get_span_id(chan);
			sig.chan_id = ftdm_channel_get_id(chan);
			sig.channel = chan;
			sig.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
			sig.raw_data = &status;

			ftdm_span_send_signal(span, &sig);
		}
	}
	return 0;
}

/**
 * \brief Handler for libpri dchan down event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_dchan_down(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	if (ftdm_test_flag(spri, LPWRAP_PRI_READY)) {
		ftdm_signaling_status_t status = FTDM_SIG_STATE_DOWN;
		ftdm_span_t *span = spri->span;
		ftdm_sigmsg_t sig;
		int i;

		ftdm_log(FTDM_LOG_INFO, "Span %d D-Channel DOWN!\n", ftdm_span_get_id(span));
		ftdm_clear_flag(spri, LPWRAP_PRI_READY);
		ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);

		ftdm_log(FTDM_LOG_NOTICE, "%d:Signaling link status changed to %s\n", ftdm_span_get_id(span), ftdm_signaling_status2str(status));

		for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
			ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

			memset(&sig, 0, sizeof(sig));
			sig.span_id = ftdm_channel_get_span_id(chan);
			sig.chan_id = ftdm_channel_get_id(chan);
			sig.channel = chan;
			sig.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
			sig.raw_data = &status;

			ftdm_span_send_signal(span, &sig);
		}
	}

	return 0;
}

/**
 * \brief Handler for any libpri event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_anything(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_log(FTDM_LOG_DEBUG, "Caught Event span %d %u (%s)\n", ftdm_span_get_id(spri->span), event_type, lpwrap_pri_event_str(event_type));
	return 0;
}

/**
 * \brief Handler for libpri io fail event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_io_fail(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_log(FTDM_LOG_DEBUG, "Caught Event span %d %u (%s)\n", ftdm_span_get_id(spri->span), event_type, lpwrap_pri_event_str(event_type));
	return 0;
}

/**
 * \brief Main thread function for libpri span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 *
 * \todo  Move all init stuff outside of loop or into ftdm_libpri_configure_span()
 */
static void *ftdm_libpri_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_libpri_data_t *isdn_data = span->signal_data;
	int down = 0;
	int got_d = 0;
	int res = 0;

	ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);

	while (ftdm_running() && !ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {
		if (!got_d) {
			int i, x;

			for (i = 1, x = 0; i <= ftdm_span_get_chan_count(span); i++) {
				ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

				if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_DQ921) {
					if (ftdm_channel_open(ftdm_span_get_id(span), i, &isdn_data->dchan) == FTDM_SUCCESS) {
						ftdm_log(FTDM_LOG_DEBUG, "opening D-Channel #%d %d:%d\n", x,
							ftdm_channel_get_span_id(isdn_data->dchan), ftdm_channel_get_id(isdn_data->dchan));
						got_d = 1;
						x++;
						break;
					} else {
					    ftdm_log(FTDM_LOG_ERROR, "failed to open D-Channel #%d %d:%d\n", x,
						ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
					}
				}
			}
		}
		if (!got_d || !isdn_data->dchan) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to get a D-Channel in span %d\n", ftdm_span_get_id(span));
			break;
		}

		/* Initialize libpri trunk */
		switch (ftdm_span_get_trunk_type(span)) {
		case FTDM_TRUNK_E1:
		case FTDM_TRUNK_T1:
		case FTDM_TRUNK_J1:
			res = lpwrap_init_pri(&isdn_data->spri, span, isdn_data->dchan,
					isdn_data->dialect, isdn_data->mode, isdn_data->debug_mask);
			break;
		case FTDM_TRUNK_BRI:
			res = lpwrap_init_bri(&isdn_data->spri, span, isdn_data->dchan,
					isdn_data->dialect, isdn_data->mode, 1, isdn_data->debug_mask);
#ifndef HAVE_LIBPRI_BRI
			goto out;
#endif
			break;
		case FTDM_TRUNK_BRI_PTMP:
			res = lpwrap_init_bri(&isdn_data->spri, span, isdn_data->dchan,
					isdn_data->dialect, isdn_data->mode, 0, isdn_data->debug_mask);
#ifndef HAVE_LIBPRI_BRI
			goto out;
#endif
			break;
		default:
			snprintf(span->last_error, sizeof(span->last_error), "Invalid trunk type");
			goto out;
		}

#ifdef HAVE_LIBPRI_AOC
		/*
		 * Only enable facility on trunk if really required,
		 * this may help avoid problems on troublesome lines.
		 */
		if (isdn_data->opts & FTMOD_LIBPRI_OPT_FACILITY_AOC) {
			pri_facility_enable(isdn_data->spri.pri);
		}
#endif

		if (res == 0) {
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_ANY, on_anything);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RING, on_ring);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RINGING, on_ringing);
			//LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_SETUP_ACK, on_proceed);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_PROCEEDING, on_proceed);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_ANSWER, on_answer);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_DCHAN_UP, on_dchan_up);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_DCHAN_DOWN, on_dchan_down);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP_REQ, on_hangup);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP, on_hangup);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_INFO_RECEIVED, on_info);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RESTART, on_restart);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_IO_FAIL, on_io_fail);
#ifdef HAVE_LIBPRI_AOC
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_FACILITY, on_facility);
#endif
			if (down) {
				ftdm_log(FTDM_LOG_INFO, "PRI back up on span %d\n", ftdm_span_get_id(span));
				ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);
				down = 0;
			}

			isdn_data->spri.on_loop = check_flags;

			lpwrap_run_pri(&isdn_data->spri);
		} else {
			ftdm_log(FTDM_LOG_CRIT, "PRI init failed!\n");
			snprintf(span->last_error, sizeof(span->last_error), "PRI init failed!");
			break;
		}

		if (!ftdm_running() || ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {
			break;
		}

		ftdm_log(FTDM_LOG_CRIT, "PRI down on span %d\n", ftdm_span_get_id(span));
		if (isdn_data->spri.dchan) {
			_ftdm_channel_set_state_force(isdn_data->spri.dchan, FTDM_CHANNEL_STATE_DOWN);
		}

		if (!down) {
			ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);
			check_state(span);
		}

		check_state(span);
		check_events(span);

		down = 1;
		ftdm_sleep(5000);
	}
out:
	ftdm_log(FTDM_LOG_DEBUG, "PRI thread ended on span %d\n", ftdm_span_get_id(span));

	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);
	ftdm_clear_flag(isdn_data, FTMOD_LIBPRI_RUNNING);

	return NULL;
}

/**
 * \brief Stops a libpri span
 * \param span Span to halt
 * \return Success
 *
 * Sets a stop flag and waits for the thread to end
 */
static ftdm_status_t ftdm_libpri_stop(ftdm_span_t *span)
{
	ftdm_libpri_data_t *isdn_data = span->signal_data;

	if (!ftdm_test_flag(isdn_data, FTMOD_LIBPRI_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);

	check_state(span);

	ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);

	while (ftdm_test_flag(span, FTDM_SPAN_IN_THREAD)) {
		ftdm_sleep(100);
	}

	check_state(span);

	return FTDM_SUCCESS;
}

/**
 * \brief Starts a libpri span
 * \param span Span to halt
 * \return Success or failure
 *
 * Launches a thread to monitor the span
 */
static ftdm_status_t ftdm_libpri_start(ftdm_span_t *span)
{
	ftdm_libpri_data_t *isdn_data = span->signal_data;

	if (ftdm_test_flag(isdn_data, FTMOD_LIBPRI_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_clear_flag(span, FTDM_SPAN_STOP_THREAD);
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	ftdm_set_flag(isdn_data, FTMOD_LIBPRI_RUNNING);

	return ftdm_thread_create_detached(ftdm_libpri_run, span);
}

/**
 * \brief Converts a node string to node value
 * \param node Node string to convert
 * \return -1 on failure, node value on success
 */
static int parse_mode(const char *mode)
{
	if (!strcasecmp(mode, "cpe") || !strcasecmp(mode, "user"))
		return PRI_CPE;
	if (!strcasecmp(mode, "network") || !strcasecmp(mode, "net"))
		return PRI_NETWORK;

	return -1;
}

/**
 * \brief Converts a switch string to switch value
 * \param swtype Swtype string to convert
 * \return Switch value
 */
static int parse_dialect(const char *dialect)
{
	if (!strcasecmp(dialect, "ni1"))
		return PRI_SWITCH_NI1;
	if (!strcasecmp(dialect, "ni2"))
		return PRI_SWITCH_NI2;
	if (!strcasecmp(dialect, "dms100"))
		return PRI_SWITCH_DMS100;
	if (!strcasecmp(dialect, "lucent5e") || !strcasecmp(dialect, "5ess"))
		return PRI_SWITCH_LUCENT5E;
	if (!strcasecmp(dialect, "att4ess") || !strcasecmp(dialect, "4ess"))
		return PRI_SWITCH_ATT4ESS;
	if (!strcasecmp(dialect, "euroisdn") || !strcasecmp(dialect, "q931"))
		return PRI_SWITCH_EUROISDN_E1;
	if (!strcasecmp(dialect, "gr303eoc"))
		return PRI_SWITCH_GR303_EOC;
	if (!strcasecmp(dialect, "gr303tmc"))
		return PRI_SWITCH_GR303_TMC;

	return PRI_SWITCH_DMS100;
}

/**
 * \brief Converts a L1 string to L1 value
 * \param l1 L1 string to convert
 * \return L1 value
 */
static int parse_layer1(const char *val)
{
	if (!strcasecmp(val, "alaw"))
		return PRI_LAYER_1_ALAW;

	return PRI_LAYER_1_ULAW;
}

/**
 * \brief Converts a DP string to DP value
 * \param dp DP string to convert
 * \return DP value
 */
static int parse_ton(const char *ton)
{
	if (!strcasecmp(ton, "international"))
		return PRI_INTERNATIONAL_ISDN;
	if (!strcasecmp(ton, "national"))
		return PRI_NATIONAL_ISDN;
	if (!strcasecmp(ton, "local"))
		return PRI_LOCAL_ISDN;
	if (!strcasecmp(ton, "private"))
		return PRI_PRIVATE;
	if (!strcasecmp(ton, "unknown"))
		return PRI_UNKNOWN;

	return PRI_UNKNOWN;
}

/**
 * \brief Parses an option string to flags
 * \param in String to parse for configuration options
 * \return Flags
 */
static uint32_t parse_opts(const char *in)
{
	uint32_t flags = 0;

	if (!in) {
		return 0;
	}

	if (strstr(in, "suggest_channel")) {
		flags |= FTMOD_LIBPRI_OPT_SUGGEST_CHANNEL;
	}
	if (strstr(in, "omit_display")) {
		flags |= FTMOD_LIBPRI_OPT_OMIT_DISPLAY_IE;
	}
	if (strstr(in, "omit_redirecting_number")) {
		flags |= FTMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE;
	}
	if (strstr(in, "aoc")) {
		flags |= FTMOD_LIBPRI_OPT_FACILITY_AOC;
	}
	return flags;
}

/**
 * \brief Initialises a libpri span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ftdm_parameters List of configuration variables
 * \return Success or failure
 */
static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_libpri_configure_span)
{
	ftdm_libpri_data_t *isdn_data = NULL;
	//ftdm_channel_t *dchan = NULL;
	uint32_t bchan_count = 0;
	uint32_t dchan_count = 0;
	uint32_t i;

	if (ftdm_span_get_trunk_type(span) >= FTDM_TRUNK_NONE) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1.\n", ftdm_span_get_trunk_type_str(span));
		ftdm_span_set_trunk_type(span, FTDM_TRUNK_T1);
	}

	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

		switch (ftdm_channel_get_type(chan)) {
		case FTDM_CHAN_TYPE_DQ921:
			if (dchan_count > 1) {
				ftdm_log(FTDM_LOG_ERROR, "Span has more than 2 D-Channels!\n");
				snprintf(span->last_error, sizeof(span->last_error), "Span has more than 2 D-Channels!");
				return FTDM_FAIL;
			} else {
#if 0
				if (ftdm_channel_open(ftdm_span_get_id(span), i, &dchan) == FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_DEBUG, "opening D-Channel %d:%d\n", ftdm_channel_get_span_id(dchan), ftdm_channel_get_id(dchan));
					_ftdm_channel_set_state_force(dchan, FTDM_CHANNEL_STATE_UP);
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Failed to open D-Channel %d:%d\n", ftdm_channel_get_span_id(chan), ftdm_channel_getid(chan));
					snprintf(span->last_error, sizeof(span->last_error), "Failed to open D-Channel %d:%d\n", ftdm_channel_get_span_id(chan), ftdm_channel_getid(chan));
					return FTDM_FAIL;
				}
#endif
				dchan_count++;
			}
			break;

		case FTDM_CHAN_TYPE_B:
			bchan_count++;
			break;
		default:		/* Ignore other channel types */
			break;
		}
	}
	if (!dchan_count) {
		ftdm_log(FTDM_LOG_ERROR, "Span has no D-Channel!\n");
		snprintf(span->last_error, sizeof(span->last_error), "Span has no D-Channel!");
		return FTDM_FAIL;
	}
	if (!bchan_count) {
		ftdm_log(FTDM_LOG_ERROR, "Span has no B-Channels!\n");
		snprintf(span->last_error, sizeof(span->last_error), "Span has no B-Channels!");
		return FTDM_FAIL;
	}

	isdn_data = ftdm_malloc(sizeof(*isdn_data));
	assert(isdn_data != NULL);
	memset(isdn_data, 0, sizeof(*isdn_data));

	switch (ftdm_span_get_trunk_type(span)) {
	case FTDM_TRUNK_BRI:
	case FTDM_TRUNK_BRI_PTMP:
#ifndef HAVE_LIBPRI_BRI
		ftdm_log(FTDM_LOG_ERROR, "Unsupported trunk type: '%s', libpri too old\n", ftdm_span_get_trunk_type_str(span));
		snprintf(span->last_error, sizeof(span->last_error), "Unsupported trunk type [%s], libpri too old", ftdm_span_get_trunk_type_str(span));
		return FTDM_FAIL;
#endif
	case FTDM_TRUNK_E1:
		ftdm_log(FTDM_LOG_NOTICE, "Setting default Layer 1 to ALAW since this is an E1/BRI/BRI PTMP trunk\n");
		isdn_data->layer1 = PRI_LAYER_1_ALAW;
		break;
	case FTDM_TRUNK_T1:
	case FTDM_TRUNK_J1:
		ftdm_log(FTDM_LOG_NOTICE, "Setting default Layer 1 to ULAW since this is a T1/J1 trunk\n");
		isdn_data->layer1 = PRI_LAYER_1_ULAW;
		break;
	default:
		ftdm_log(FTDM_LOG_ERROR, "Invalid trunk type: '%s'\n", ftdm_span_get_trunk_type_str(span));
		snprintf(span->last_error, sizeof(span->last_error), "Invalid trunk type [%s]", ftdm_span_get_trunk_type_str(span));
		return FTDM_FAIL;
	}

	for (i = 0; i < 10 && ftdm_parameters[i].var; i++) {
		const char *var = ftdm_parameters[i].var;
		const char *val = ftdm_parameters[i].val;

		if (!val) {
			ftdm_log(FTDM_LOG_ERROR, "Parameter '%s' has no value\n", var);
			snprintf(span->last_error, sizeof(span->last_error), "Parameter [%s] has no value", var);
			return FTDM_FAIL;
		}

		if (!strcasecmp(var, "node") || !strcasecmp(var, "mode")) {
			if ((isdn_data->mode = parse_mode(val)) == -1) {
				ftdm_log(FTDM_LOG_ERROR, "Unknown node type '%s', defaulting to CPE mode\n", val);
				isdn_data->mode = PRI_CPE;
			}
		}
		else if (!strcasecmp(var, "switch") || !strcasecmp(var, "dialect")) {
			isdn_data->dialect = parse_dialect(val);
		}
		else if (!strcasecmp(var, "opts")) {
			isdn_data->opts = parse_opts(val);
		}
		else if (!strcasecmp(var, "dp") || !strcasecmp(var, "ton")) {
			isdn_data->ton = parse_ton(val);
		}
		else if (!strcasecmp(var, "l1") || !strcasecmp(var, "layer1")) {
			isdn_data->layer1 = parse_layer1(val);
		}
		else if (!strcasecmp(var, "debug")) {
			if (parse_debug(val, &isdn_data->debug_mask) == -1) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid debug flag, ignoring parameter\n");
				isdn_data->debug_mask = 0;
			}
		}
		else {
			ftdm_log(FTDM_LOG_ERROR, "Unknown parameter '%s', aborting configuration\n", var);
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return FTDM_FAIL;
		}
	}

	span->start = ftdm_libpri_start;
	span->stop  = ftdm_libpri_stop;
	span->signal_cb = sig_cb;

	span->signal_data = isdn_data;
	span->signal_type = FTDM_SIGTYPE_ISDN;
	span->outgoing_call = isdn_outgoing_call;

	span->state_map = &isdn_state_map;

	span->get_channel_sig_status = isdn_get_channel_sig_status;
	span->get_span_sig_status = isdn_get_span_sig_status;

	if ((isdn_data->opts & FTMOD_LIBPRI_OPT_SUGGEST_CHANNEL)) {
		span->channel_request = isdn_channel_request;
		ftdm_set_flag(span, FTDM_SPAN_SUGGEST_CHAN_ID);
	}

	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM libpri signaling and IO module definition
 */
ftdm_module_t ftdm_module = {
	"libpri",
	ftdm_libpri_io_init,
	ftdm_libpri_unload,
	ftdm_libpri_init,
	NULL,
	NULL,
	ftdm_libpri_configure_span
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
