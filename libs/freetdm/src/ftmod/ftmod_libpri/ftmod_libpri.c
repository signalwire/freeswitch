/*
 * Copyright (c) 2007-2014, Anthony Minessale II
 * Copyright (c) 2010, Stefan Knoblich <s.knoblich@axsentis.de>
 * Copyright (c) 2012-2013, Stefan Knoblich <stkn@openisdn.net>
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


static ftdm_status_t ftdm_libpri_start(ftdm_span_t *span);
static ftdm_io_interface_t ftdm_libpri_interface;

static int on_timeout_t302(struct lpwrap_pri *spri, struct lpwrap_timer *timer);
static int on_timeout_t316(struct lpwrap_pri *spri, struct lpwrap_timer *timer);
static int on_timeout_t3xx(struct lpwrap_pri *spri, struct lpwrap_timer *timer);


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
	ftdm_unused_arg(ftdmchan);
	return FTDM_SUCCESS;
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
	ftdm_unused_arg(span);
	ftdm_unused_arg(chan_id);
	ftdm_unused_arg(direction);
	ftdm_unused_arg(caller_data);
	ftdm_unused_arg(ftdmchan);
	return FTDM_FAIL;
}


/**
 * \brief Logs a libpri message
 * \param pri	libpri structure
 * \param s	Message string
 */
static void s_pri_message(struct pri *pri, char *s)
{
	struct lpwrap_pri *spri = pri_get_userdata(pri);

	if (spri && spri->dchan) {
		ftdm_log_chan(spri->dchan, FTDM_LOG_DEBUG, "%s", s);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "%s", s);
	}
}

/**
 * \brief Logs a libpri error
 * \param pri	libpri structure
 * \param s	Error string
 */
static void s_pri_error(struct pri *pri, char *s)
{
	struct lpwrap_pri *spri = pri_get_userdata(pri);

	if (spri && spri->dchan) {
		ftdm_log_chan(spri->dchan, FTDM_LOG_ERROR, "%s", s);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "%s", s);
	}
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

#ifdef HAVE_LIBPRI_MAINT_SERVICE
/**
 * \brief Parses a change status string to flags
 * \param in change status string to parse for
 * \return Flags
 */
static int parse_change_status(const char *in)
{
	int flags = 0;
	if (!in) {
		return 0;
	}

	if (strstr(in, "in_service") || strstr(in, "in")) {
		flags = SERVICE_CHANGE_STATUS_INSERVICE;
	}
	if (strstr(in, "maintenance") || strstr(in, "maint")) {
		flags = SERVICE_CHANGE_STATUS_MAINTENANCE;
	}
	if (strstr(in, "out_of_service") || strstr(in, "out")) {
		flags = SERVICE_CHANGE_STATUS_OUTOFSERVICE;
	}


	return flags;
}
#endif


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


/***************************************************************
 * MSN filter
 ***************************************************************/

/**
 * Initialize MSN filter data structures
 * \param[in]	isdn_data	Span private data
 * \return	FTDM_SUCCESS, FTDM_FAIL
 */
static int msn_filter_init(ftdm_libpri_data_t *isdn_data)
{
	if (!isdn_data)
		return FTDM_FAIL;

	isdn_data->msn_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	if (!isdn_data->msn_hash)
		return FTDM_FAIL;

	if (ftdm_mutex_create(&isdn_data->msn_mutex)) {
		hashtable_destroy(isdn_data->msn_hash);
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

/**
 * Destroy MSN filter data structures
 * \param[in]	isdn_data	Span private data
 * \return	FTDM_SUCCESS, FTDM_FAIL
 */
static int msn_filter_destroy(ftdm_libpri_data_t *isdn_data)
{
	if (!isdn_data)
		return FTDM_FAIL;

	if (isdn_data->msn_hash)
		hashtable_destroy(isdn_data->msn_hash);
	if (isdn_data->msn_mutex)
		ftdm_mutex_destroy(&isdn_data->msn_mutex);

	return FTDM_SUCCESS;
}

/**
 * Check if the given string is a valid MSN/DDI
 * (i.e.: Not empty, not longer than FDM_DIGITS_LIMIT and all numbers)
 * \param[in]	str	String to check
 * \return	FTDM_SUCCESS, FTDM_FAIL
 */
static int msn_filter_verify(const char *str)
{
	if (ftdm_strlen_zero(str) || strlen(str) >= FTDM_DIGITS_LIMIT)
		return FTDM_FALSE;

	if (ftdm_is_number(str) != FTDM_SUCCESS)
		return FTDM_FALSE;

	return FTDM_TRUE;
}

/**
 * Add a new MSN/DDI to the filter
 * \param[in]	isdn_data	Span private data
 * \param[in]	msn		New MSN/DDI to add
 * \return	FTDM_SUCCESS, FTDM_FAIL
 */
static int msn_filter_add(ftdm_libpri_data_t *isdn_data, const char *msn)
{
	static const int value = 0xdeadbeef;
	char *key = NULL;
	int ret = FTDM_SUCCESS;

	if (!isdn_data || !msn_filter_verify(msn))
		return FTDM_FAIL;

	ftdm_mutex_lock(isdn_data->msn_mutex);

	/* check for duplicates (ignore if already in set) */
	if (hashtable_search(isdn_data->msn_hash, (void *)msn)) {
		ret = FTDM_SUCCESS;
		goto out;
	}

	/* Copy MSN (transient string), hashtable will free it in hashtable_destroy() */
	key = ftdm_strdup(msn);
	if (!key) {
		ret = FTDM_FAIL;
		goto out;
	}

	/* add MSN to list/hash */
	if (!hashtable_insert(isdn_data->msn_hash, (void *)key, (void *)&value, HASHTABLE_FLAG_FREE_KEY)) {
		ftdm_safe_free(key);
		ret = FTDM_FAIL;
	}
out:
	ftdm_mutex_unlock(isdn_data->msn_mutex);
	return ret;
}


/**
 * Check if a DNIS (destination number) is a valid MSN/DDI
 * \param[in]	isdn_data	Span private data
 * \param[in]	msn		Number to check
 * \retval	FTDM_TRUE	\p msn is a valid MSN/DDI or filter list is empty
 * \retval	FTDM_FALSE	\p msn is not a valid MSN/DDI
 */
static int msn_filter_match(ftdm_libpri_data_t *isdn_data, const char *msn)
{
	int ret = FTDM_FALSE;

	if (!isdn_data)
		return FTDM_FALSE;
	/* No number? return match found */
	if (ftdm_strlen_zero(msn))
		return FTDM_TRUE;

	ftdm_mutex_lock(isdn_data->msn_mutex);

	/* No MSN configured? */
	if (hashtable_count(isdn_data->msn_hash) <= 0) {
		ret = FTDM_TRUE;
		goto out;
	}
	/* Search for a matching MSN */
	if (hashtable_search(isdn_data->msn_hash, (void *)msn))
		ret = FTDM_TRUE;
out:
	ftdm_mutex_unlock(isdn_data->msn_mutex);
	return ret;
}

/**
 * Helper function to iterate over MSNs in the filter hash (handles locking)
 * \param[in]	isdn_data	Span private data
 * \param[in]	func		Callback function that is invoked for each entry
 * \param[in]	data		Private data passed to callback
 * \return	FTDM_SUCCESS, FTDM_FAIL
 */
static int msn_filter_foreach(ftdm_libpri_data_t *isdn_data, int (* func)(const char *, void *), void *data)
{
	ftdm_hash_iterator_t *iter = NULL;
	int ret = FTDM_SUCCESS;

	if (!isdn_data || !func)
		return FTDM_FAIL;

	/* iterate over MSNs */
	ftdm_mutex_lock(isdn_data->msn_mutex);

	for (iter = hashtable_first(isdn_data->msn_hash); iter; iter = hashtable_next(iter)) {
		const void *msn = NULL;

		hashtable_this(iter, &msn, NULL, NULL);

		if (ftdm_strlen_zero((const char *)msn))
			break;
		if ((ret = func(msn, data)) != FTDM_SUCCESS)
			break;
	}

	ftdm_mutex_unlock(isdn_data->msn_mutex);
	return ret;
}


/***************************************************************
 * Module API (CLI) interface
 ***************************************************************/

static const char *ftdm_libpri_usage =
	"Usage:\n"
	"libpri kill <span>\n"
	"libpri reset <span>\n"
	"libpri restart <span> <channel/all>\n"
#ifdef HAVE_LIBPRI_MAINT_SERVICE
	"libpri maintenance <span> <channel/all> <in/maint/out>\n"
#endif
	"libpri debug <span> [all|none|flag,...flagN]\n"
	"libpri msn <span>\n"
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
 * Custom data handle for list iterator functions
 */
struct msn_list_cb_private {
	ftdm_stream_handle_t *stream;
	unsigned int count;
};

/**
 * "ftdm libpri msn <span>" API command callback
 * function for msn_filter_foreach()
 */
static int msn_list_cb(const char *msn, void *priv)
{
	struct msn_list_cb_private *data = priv;
	ftdm_stream_handle_t *stream = data->stream;

	if (!stream || ftdm_strlen_zero(msn))
		return FTDM_FAIL;

	stream->write_function(stream, "\t%s\n", msn);
	data->count++;
	return FTDM_SUCCESS;
}


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
	} else if (argc == 2) {
		if (!strcasecmp(argv[0], "kill")) {
			int span_id = atoi(argv[1]);
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR '%s' is not a libpri span.\n",
						__FILE__, ftdm_span_get_name(span));
					goto done;
				}

				ftdm_clear_flag(&(isdn_data->spri), LPWRAP_PRI_READY);
				stream->write_function(stream, "%s: +OK killed.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR span '%s' not found.\n",
					__FILE__, argv[0]);
				goto done;
			}
		}
		if (!strcasecmp(argv[0], "msn")) {
			ftdm_span_t *span = NULL;
			struct msn_list_cb_private data;
			data.stream = stream;
			data.count  = 0;

			if (ftdm_span_find_by_name(argv[1], &span) != FTDM_SUCCESS) {
				stream->write_function(stream, "%s: -ERR span '%s' not found.\n",
					__FILE__, argv[1]);
				goto done;
			}
			if (span->start != ftdm_libpri_start) {
				stream->write_function(stream, "%s: -ERR '%s' is not a libpri span.\n",
					__FILE__, ftdm_span_get_name(span));
				goto done;
			}

			/* header */
			stream->write_function(stream, "------------------------------------------------------------------------------\n");

			if (msn_filter_foreach(span->signal_data, msn_list_cb, &data)) {
				stream->write_function(stream, "-ERR: Failed to list MSN(s)\n");
				goto done;
			}
			if (data.count == 0) {
				stream->write_function(stream, "\t\t\t -- no entries --\n");
			}

			/* footer */
			stream->write_function(stream, "---------------------------------------------------------------[ %02d MSN(s) ]--\n",
				data.count);
			stream->write_function(stream, "+OK");
			goto done;
		}
	} else if (argc >= 2) {
		if (!strcasecmp(argv[0], "debug")) {
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;
				uint32_t flags = 0;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR '%s' is not a libpri span.\n",
						__FILE__, ftdm_span_get_name(span));
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
				stream->write_function(stream, "%s: -ERR span '%s' not found.\n",
					__FILE__, argv[0]);
				goto done;
			}
		}
		if (!strcasecmp(argv[0], "reset")) {
			ftdm_span_t *span = NULL;
			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR '%s' is not a libpri span.\n",
						__FILE__, ftdm_span_get_name(span));
					goto done;
				}

				pri_restart(isdn_data->spri.pri);
				stream->write_function(stream, "%s: +OK reset.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR span '%s' not found.\n",
					__FILE__, argv[0]);
				goto done;
			}
		}
		if (!strcasecmp(argv[0], "restart") && argc == 3) {
			ftdm_span_t *span = NULL;
			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR '%s' is not a libpri span.\n",
						__FILE__, ftdm_span_get_name(span));
					goto done;
				}
				if (!strcasecmp(argv[2], "all")) {
					int j;
					for(j = 1; j <= span->chan_count; j++) {
						pri_reset(isdn_data->spri.pri, j);
						ftdm_sleep(50);
					}
				} else {
					pri_reset(isdn_data->spri.pri, atoi(argv[2]));
				}
				stream->write_function(stream, "%s: +OK restart set.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR span '%s' not found.\n",
					__FILE__, argv[0]);
				goto done;
			}
		}
#ifdef HAVE_LIBPRI_MAINT_SERVICE
		if (!strcasecmp(argv[0], "maintenance") && argc > 3) {
			ftdm_span_t *span = NULL;
			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				ftdm_libpri_data_t *isdn_data = span->signal_data;

				if (span->start != ftdm_libpri_start) {
					stream->write_function(stream, "%s: -ERR '%s' is not a libpri span.\n",
						__FILE__, ftdm_span_get_name(span));
					goto done;
				}
				if (!isdn_data->service_message_support) {
					stream->write_function(stream, "%s: -ERR service message support is disabled\n", __FILE__);
					goto done;
				}
				if (!strcasecmp(argv[2], "all")) {
					int j;
					for(j = 1; j <= span->chan_count; j++) {
						pri_maintenance_service(isdn_data->spri.pri, atoi(argv[1]), j, parse_change_status(argv[3]));
						ftdm_sleep(50);
					}
				} else {
					pri_maintenance_service(isdn_data->spri.pri, atoi(argv[1]), atoi(argv[2]), parse_change_status(argv[3]));
				}
				stream->write_function(stream, "%s: +OK change status set.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR span '%s' not found.\n",
					__FILE__, argv[0]);
				goto done;
			}
		}
#endif
	} else {
		/* zero args print usage */
		stream->write_function(stream, ftdm_libpri_usage);
		goto done;
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
			{FTDM_CHANNEL_STATE_ANY},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_END}
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
			{FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
			 FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS,
			 FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
			 FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
			 FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
			 FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
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
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
		},

		/****************************************/
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_ANY},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DIALTONE, FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DIALTONE, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS,
			 FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA,
			 FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
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
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
		},
	}
};

/**
 * \brief Handler for channel state change
 * \param ftdmchan Channel to handle
 * \note This function MUST be called with the channel locked
 */
static ftdm_status_t state_advance(ftdm_channel_t *chan)
{
	ftdm_span_t *span = ftdm_channel_get_span(chan);
	ftdm_libpri_data_t *isdn_data = span->signal_data;
	ftdm_libpri_b_chan_t *chan_priv = chan->call_data;
	q931_call *call = chan_priv->call;
	ftdm_status_t status;
	ftdm_sigmsg_t sig;

	ftdm_log(FTDM_LOG_DEBUG, "-- %d:%d STATE [%s]\n",
			ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan), ftdm_channel_get_state_str(chan));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdm_channel_get_id(chan);
	sig.span_id = ftdm_channel_get_span_id(chan);
	sig.channel = chan;

	ftdm_channel_complete_state(chan);

	switch (ftdm_channel_get_state(chan)) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
				ftdm_channel_t *chtmp = chan;

				if (call) {
					/* pri call destroy is done by libpri itself (on release_ack) */
					chan_priv->call = NULL;
				}

				/* Stop T302 */
				lpwrap_stop_timer(&isdn_data->spri, &chan_priv->t302);

				/* Stop T316 and reset counter */
				lpwrap_stop_timer(&isdn_data->spri, &chan_priv->t316);
				chan_priv->t316_timeout_cnt = 0;

				/* Unset remote hangup */
				chan_priv->peerhangup = 0;

				if (ftdm_channel_close(&chtmp) != FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_WARNING, "-- Failed to close channel %d:%d\n",
						ftdm_channel_get_span_id(chan),
						ftdm_channel_get_id(chan));
				} else {
					ftdm_log(FTDM_LOG_DEBUG, "-- Closed channel %d:%d\n",
						ftdm_channel_get_span_id(chan),
						ftdm_channel_get_id(chan));
				}
			}
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				/*
				 * Even if we have no media, sending progress without PI is forbidden
				 * by Q.931 3.1.8, so a protocol error will be issued from libpri
				 * and from remote equipment.
				 * So just pretend we have PI.
				 */
				pri_progress(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 1);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_RINGING:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_RINGING;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
//				pri_progress(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 1);
				pri_acknowledge(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 0);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				/* make sure channel is open in this state (outbound handled in on_proceeding()) */
				if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN)) {
					ftdm_channel_open_chan(chan);
				}
				pri_progress(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 1);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_PROCEED:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				/* PROCEED from other end, notify user */
				sig.event_id = FTDM_SIGEVENT_PROCEED;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_log(FTDM_LOG_ERROR, "Failed to send PROCEED sigevent on Channel %d:%d\n",
						ftdm_channel_get_span_id(chan),
						ftdm_channel_get_id(chan));
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_proceeding(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 0);
			} else {
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_COLLECT:	/* Overlap receive */
		{
			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				if (!call) {
					ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "No call handle\n");
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
				}
				else if (pri_need_more_info(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 0)) {
					ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

					ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "Failed to send INFORMATION request\n");

					/* hangup call */
					caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
				else {
					/* Start T302 */
					lpwrap_start_timer(&isdn_data->spri, &chan_priv->t302,
						isdn_data->overlap_timeout_ms, &on_timeout_t302);
				}
			} else {
				ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "Overlap receiving on outbound call?\n");
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_RING:
		{
			/*
			 * This needs more auditing for BRI PTMP:
			 * does pri_acknowledge() steal the call from other devices? (yes, it does)
			 */
			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				if (call) {
					pri_proceeding(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 0);
//					pri_acknowledge(isdn_data->spri.pri, call, ftdm_channel_get_id(chan), 0);
					sig.event_id = FTDM_SIGEVENT_START;
					if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
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
			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
				chan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_UNSPECIFIED;
				sig.event_id = FTDM_SIGEVENT_RESTART;
				status = ftdm_span_send_signal(span, &sig);

				if (ftdm_span_get_trunk_type(span) == FTDM_TRUNK_BRI_PTMP) {
					/* Just put the channel into DOWN state, libpri won't send RESTART on BRI PTMP */
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);

				} else if (!(chan_priv->flags & FTDM_LIBPRI_B_REMOTE_RESTART)) {
					/* Locally triggered restart, send RESTART to remote, wait for ACK */
					pri_reset(isdn_data->spri.pri, ftdm_channel_get_id(chan));
					/* Start T316 */
					lpwrap_start_timer(&isdn_data->spri, &chan_priv->t316, isdn_data->t316_timeout_ms, &on_timeout_t316);
				} else {
					/* Remote restart complete, clear flag (RESTART ACK already sent by libpri) */
					chan_priv->flags &= ~FTDM_LIBPRI_B_REMOTE_RESTART;
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);
				}
			}
		}
		break;

	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_flag(chan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_UP;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				/* make sure channel is open in this state (outbound handled in on_answer()) */
				if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN)) {
					ftdm_channel_open_chan(chan);
				}
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
			int caller_ton;
			int called_ton;

			if (!(call = pri_new_call(isdn_data->spri.pri))) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create new call on channel %d:%d\n",
					ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
				/* TODO: set hangup cause? */
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
				return FTDM_SUCCESS;
			}

			caller_ton = caller_data->cid_num.type;
			switch (caller_ton) {
			case FTDM_TON_NATIONAL:
				caller_ton = PRI_NATIONAL_ISDN;
				break;
			case FTDM_TON_INTERNATIONAL:
				caller_ton = PRI_INTERNATIONAL_ISDN;
				break;
			case FTDM_TON_SUBSCRIBER_NUMBER:
				caller_ton = PRI_LOCAL_ISDN;
				break;
			default:
				caller_ton = isdn_data->ton;
			}

			called_ton = caller_data->dnis.type;
			switch (called_ton) {
			case FTDM_TON_NATIONAL:
				called_ton = PRI_NATIONAL_ISDN;
				break;
			case FTDM_TON_INTERNATIONAL:
				called_ton = PRI_INTERNATIONAL_ISDN;
				break;
			case FTDM_TON_SUBSCRIBER_NUMBER:
				called_ton = PRI_LOCAL_ISDN;
				break;
			default:
				called_ton = isdn_data->ton;
			}

			chan_priv->call = call;

			sr = pri_sr_new();
			if (!sr) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create new setup request on channel %d:%d\n",
					ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
				/* TODO: handle error properly */
			}
			assert(sr);

			pri_sr_set_channel(sr, ftdm_channel_get_id(chan), 1, 0);
			pri_sr_set_bearer(sr, PRI_TRANS_CAP_SPEECH, isdn_data->layer1);

			pri_sr_set_called(sr, caller_data->dnis.digits, called_ton, 1);
			pri_sr_set_caller(sr, caller_data->cid_num.digits,
					((isdn_data->opts & FTMOD_LIBPRI_OPT_OMIT_DISPLAY_IE) ? NULL : caller_data->cid_name),
					caller_ton,
					((caller_data->pres != 1) ? PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN : PRES_PROHIB_USER_NUMBER_NOT_SCREENED));

			if (!(isdn_data->opts & FTMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE)) {
				pri_sr_set_redirecting(sr, caller_data->cid_num.digits, caller_ton,
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

				if (chan_priv->peerhangup) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
				}
			}
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		{
//			if (call) {
//				pri_destroycall(isdn_data->spri.pri, call);
//				chan_priv->call = NULL;
//			}
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;

	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = FTDM_SIGEVENT_STOP;
			status = ftdm_span_send_signal(span, &sig);
			/* user moves us to HANGUP and from there we go to DOWN */
		}
	default:
		break;
	}
	return FTDM_SUCCESS;
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
			ftdm_channel_lock(chan);
			ftdm_channel_advance_states(chan);
			ftdm_channel_unlock(chan);
		}
	}
}


/**
 * \brief Handler for libpri keypad digit event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_keypad_digit(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->ring.channel);

	ftdm_unused_arg(event_type);

	if (!chan) {
		ftdm_log(FTDM_LOG_ERROR, "-- Keypad event on invalid channel %d:%d\n",
			ftdm_span_get_id(span), pevent->ring.channel);
		return 0;
	}

	ftdm_log_chan(chan, FTDM_LOG_DEBUG, "-- Keypad event received, incoming digits: '%s'\n",
		pevent->digit.digits);

	/* Enqueue DTMF digits on channel */
	ftdm_channel_queue_dtmf(chan, pevent->digit.digits);
	return 0;
}


/**
 * \brief Handler for libpri information event (overlap receiving)
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_information(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->ring.channel);
	ftdm_libpri_b_chan_t *chan_priv = NULL;
	ftdm_caller_data_t *caller_data = NULL;
	ftdm_libpri_data_t *isdn_data = span->signal_data;

	ftdm_unused_arg(event_type);

	if (!chan) {
		ftdm_log(FTDM_LOG_CRIT, "-- Info on channel %d:%d but it's not in use?\n", ftdm_span_get_id(span), pevent->ring.channel);
		return 0;
	}

	caller_data = ftdm_channel_get_caller_data(chan);
	chan_priv   = chan->call_data;

	switch (ftdm_channel_get_state(chan)) {
	case FTDM_CHANNEL_STATE_COLLECT:	/* TE-mode overlap receiving */
	case FTDM_CHANNEL_STATE_DIALTONE:	/* NT-mode overlap receiving */

		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "-- Incoming INFORMATION indication, received digits: '%s', number complete: %c, collected digits: '%s'\n",
			pevent->ring.callednum,
			pevent->ring.complete ? 'Y' : 'N',
			caller_data->dnis.digits);

		/* Stop T302 */
		lpwrap_stop_timer(spri, &chan_priv->t302);

		/* append digits to dnis */
		if (!ftdm_strlen_zero(pevent->ring.callednum)) {
			int digits = strlen(pevent->ring.callednum);
			int offset = strlen(caller_data->dnis.digits);
			int len    = 0;

			if (strchr(pevent->ring.callednum, '#')) {
				pevent->ring.complete = 1;
				digits--;
			}

			len = ftdm_min(sizeof(caller_data->dnis.digits) - 1 - offset, digits);	/* max. length without terminator */
			if (len < digits) {
				ftdm_log_chan(chan, FTDM_LOG_WARNING, "Digit string of length %d exceeds available space %d of DNIS, truncating!\n",
					digits, len);
			}
			if (len) {
				ftdm_copy_string(&caller_data->dnis.digits[offset], (char *)pevent->ring.callednum, len + 1);	/* max. length with terminator */
				caller_data->dnis.digits[offset + len] = '\0';
			}
		}
		if (pevent->ring.complete) {
			ftdm_log_chan_msg(chan, FTDM_LOG_DEBUG, "Number complete indication received, moving channel to RING state\n");
			/* notify switch */
			ftdm_set_state(chan, FTDM_CHANNEL_STATE_RING);
		} else {
			/* Restart T302 */
			lpwrap_start_timer(spri, &chan_priv->t302, isdn_data->overlap_timeout_ms, &on_timeout_t302);
		}
		break;
	default:
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "-- INFORMATION indication in invalid state '%s'\n",
			ftdm_channel_get_state_str(chan));
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
	ftdm_libpri_b_chan_t *chan_priv = chan->call_data;

	if (!chan) {
		ftdm_log(FTDM_LOG_CRIT, "-- Hangup on channel %d:%d but it's not in use?\n", ftdm_span_get_id(spri->span), pevent->hangup.channel);
		return 0;
	}

	ftdm_channel_lock(chan);

	switch (event_type) {
	case LPWRAP_PRI_EVENT_HANGUP_REQ:	/* DISCONNECT */
		if (ftdm_channel_get_state(chan) >= FTDM_CHANNEL_STATE_TERMINATING) {
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Ignoring remote hangup in state %s\n",
				ftdm_channel_get_state_str(chan));
			goto done;
		}
		ftdm_log(FTDM_LOG_DEBUG, "-- Hangup REQ on channel %d:%d\n",
			ftdm_span_get_id(spri->span), pevent->hangup.channel);

		chan->caller_data.hangup_cause = pevent->hangup.cause;

		switch (ftdm_channel_get_state(chan)) {
		case FTDM_CHANNEL_STATE_DIALTONE:
		case FTDM_CHANNEL_STATE_COLLECT:
			ftdm_set_state(chan, FTDM_CHANNEL_STATE_HANGUP);
			break;
		default:
			ftdm_set_state(chan, FTDM_CHANNEL_STATE_TERMINATING);
		}
		break;

	case LPWRAP_PRI_EVENT_HANGUP_ACK:	/* RELEASE_COMPLETE */
		ftdm_log(FTDM_LOG_DEBUG, "-- Hangup ACK on channel %d:%d\n",
			ftdm_span_get_id(spri->span), pevent->hangup.channel);

		switch (ftdm_channel_get_state(chan)) {
			case FTDM_CHANNEL_STATE_RESTART:
				/* ACK caused by DL FAILURE in DISC REQ */
				ftdm_set_state(chan, FTDM_CHANNEL_STATE_DOWN);
				break;
			default:
				ftdm_set_state(chan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
				break;
		}
		break;

	case LPWRAP_PRI_EVENT_HANGUP:	/* "RELEASE/RELEASE_COMPLETE/other" */
		ftdm_log(FTDM_LOG_DEBUG, "-- Hangup on channel %d:%d\n",
			ftdm_span_get_id(spri->span), pevent->hangup.channel);

		chan_priv->peerhangup = 1;

		switch (ftdm_channel_get_state(chan)) {
		case FTDM_CHANNEL_STATE_DIALING:
		case FTDM_CHANNEL_STATE_RINGING:
		case FTDM_CHANNEL_STATE_PROGRESS:
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		case FTDM_CHANNEL_STATE_PROCEED:
		case FTDM_CHANNEL_STATE_UP:
			chan->caller_data.hangup_cause = pevent->hangup.cause;
			ftdm_set_state(chan, FTDM_CHANNEL_STATE_TERMINATING);
			break;
		case FTDM_CHANNEL_STATE_HANGUP:
			/* this will send "RELEASE_COMPLETE", eventually */
			pri_hangup(spri->pri, pevent->hangup.call, chan->caller_data.hangup_cause);
			chan->caller_data.hangup_cause = pevent->hangup.cause;
			ftdm_set_state(chan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
			break;
		case FTDM_CHANNEL_STATE_RESTART:
			/*
			 * We got an hungup doing a restart, normally beacause link has been lost during
			 * a call and the T309 timer has expired. So destroy it :) (DL_RELEASE_IND)
			 */
			pri_destroycall(spri->pri, pevent->hangup.call);
			ftdm_set_state(chan, FTDM_CHANNEL_STATE_DOWN);
			break;
//		case FTDM_CHANNEL_STATE_TERMINATING:
//			ftdm_set_state(chan, FTDM_CHANNEL_STATE_HANGUP);
//			break;
//		case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
//			ftdm_set_state(chan, FTDM_CHANNEL_STATE_DOWN);
//			break;
		}
		break;
	default:
		break;
	}

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

	ftdm_unused_arg(event_type);

	if (chan) {
		if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN)) {
			ftdm_log(FTDM_LOG_DEBUG, "-- Call answered, opening B-Channel %d:%d\n",
				ftdm_channel_get_span_id(chan),
				ftdm_channel_get_id(chan));

			if (ftdm_channel_open_chan(chan) != FTDM_SUCCESS) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

				ftdm_log(FTDM_LOG_ERROR, "-- Error opening channel %d:%d\n",
					ftdm_channel_get_span_id(chan),
					ftdm_channel_get_id(chan));

				caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_TERMINATING);
				goto out;
			}
		}
		ftdm_log(FTDM_LOG_DEBUG, "-- Answer on channel %d:%d\n", ftdm_span_get_id(span), pevent->answer.channel);
		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_UP);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Answer on channel %d:%d but it's not in the span?\n",
			ftdm_span_get_id(span), pevent->answer.channel);
	}
out:
	return 0;
}

/**
 * \brief Handler for libpri proceeding event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_proceeding(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->proceeding.channel);

	ftdm_unused_arg(event_type);

	if (chan) {
		/* Open channel if inband information is available */
		if (pevent->proceeding.progressmask & PRI_PROG_INBAND_AVAILABLE || pevent->proceeding.progressmask & PRI_PROG_CALL_NOT_E2E_ISDN) {
			ftdm_log(FTDM_LOG_DEBUG, "-- In-band information available, B-Channel %d:%d\n",
				ftdm_channel_get_span_id(chan),
				ftdm_channel_get_id(chan));

			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN) && (ftdm_channel_open_chan(chan) != FTDM_SUCCESS)) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

				ftdm_log(FTDM_LOG_ERROR, "-- Error opening channel %d:%d\n",
					ftdm_channel_get_span_id(chan),
					ftdm_channel_get_id(chan));

				caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_TERMINATING);
				goto out;
			}
		}
		ftdm_log(FTDM_LOG_DEBUG, "-- Proceeding on channel %d:%d\n", ftdm_span_get_id(span), pevent->proceeding.channel);
		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROCEED);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Proceeding on channel %d:%d but it's not in the span?\n",
						ftdm_span_get_id(span), pevent->proceeding.channel);
	}
out:
	return 0;
}

/**
 * \brief Handler for libpri progress event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 * \note also uses pri_event->proceeding
 */
static int on_progress(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_span_t *span = spri->span;
	ftdm_channel_t *chan = ftdm_span_get_channel(span, pevent->proceeding.channel);

	ftdm_unused_arg(event_type);

	if (chan) {
		/* Open channel if inband information is available */
		if (pevent->proceeding.progressmask & PRI_PROG_INBAND_AVAILABLE || pevent->proceeding.progressmask & PRI_PROG_CALL_NOT_E2E_ISDN) {
			ftdm_log(FTDM_LOG_DEBUG, "-- In-band information available, B-Channel %d:%d\n",
				ftdm_channel_get_span_id(chan),
				ftdm_channel_get_id(chan));

			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN) && (ftdm_channel_open_chan(chan) != FTDM_SUCCESS)) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

				ftdm_log(FTDM_LOG_ERROR, "-- Error opening channel %d:%d\n",
					ftdm_channel_get_span_id(chan),
					ftdm_channel_get_id(chan));

				caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_TERMINATING);
				goto out;
			}
			ftdm_log(FTDM_LOG_DEBUG, "-- Progress on channel %d:%d with media\n", ftdm_span_get_id(span), pevent->proceeding.channel);
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
		} else {
			ftdm_log(FTDM_LOG_DEBUG, "-- Progress on channel %d:%d\n", ftdm_span_get_id(span), pevent->proceeding.channel);
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROGRESS);
		}
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Progress on channel %d:%d but it's not in the span?\n",
						ftdm_span_get_id(span), pevent->proceeding.channel);
	}
out:
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

	ftdm_unused_arg(event_type);

	if (chan) {
		/* we may get on_ringing even when we're already in FTDM_CHANNEL_STATE_PROGRESS_MEDIA */
//		if (ftdm_channel_get_state(chan) == FTDM_CHANNEL_STATE_PROGRESS_MEDIA) {
//			/* dont try to move to STATE_PROGRESS to avoid annoying veto warning */
//			return 0;
//		}

		/* Open channel if inband information is available */
		if ((pevent->ringing.progressmask & PRI_PROG_INBAND_AVAILABLE)) {
			ftdm_log(FTDM_LOG_DEBUG, "-- In-band information available, B-Channel %d:%d\n",
				ftdm_channel_get_span_id(chan),
				ftdm_channel_get_id(chan));

			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN) && (ftdm_channel_open_chan(chan) != FTDM_SUCCESS)) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

				ftdm_log(FTDM_LOG_ERROR, "-- Error opening channel %d:%d\n",
					ftdm_channel_get_span_id(chan),
					ftdm_channel_get_id(chan));

				caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_TERMINATING);
				goto out;
			}
			ftdm_log(FTDM_LOG_DEBUG, "-- Ringing on channel %d:%d with media\n", ftdm_span_get_id(span), pevent->proceeding.channel);
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
		} else {
//			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_PROGRESS);
			ftdm_log(FTDM_LOG_DEBUG, "-- Ringing on channel %d:%d\n", ftdm_span_get_id(span), pevent->proceeding.channel);
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RINGING);
		}
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "-- Ringing on channel %d:%d but it's not in the span?\n",
			ftdm_span_get_id(span), pevent->ringing.channel);
	}
out:
	return 0;
}


/**
 * Look up FreeTDM channel by call reference value
 * \param[in]	span	Span object
 * \param[in]	crv	CRV to search for
 * \return	Channel on success, NULL otherwise
 */
static ftdm_channel_t *find_channel_by_cref(ftdm_span_t *span, const int cref)
{
	ftdm_iterator_t *c_iter, *c_cur;
	ftdm_channel_t *chan = NULL;

	if (!span || cref <= 0)
		return NULL;

	ftdm_mutex_lock(span->mutex);

	c_iter = ftdm_span_get_chan_iterator(span, NULL);

	/* Iterate over all channels on this span */
	for (c_cur = c_iter; c_cur; c_cur = ftdm_iterator_next(c_cur)) {
		ftdm_channel_t *cur = ftdm_iterator_current(c_cur);
		ftdm_caller_data_t *caller_data = NULL;

		if (ftdm_channel_get_type(cur) != FTDM_CHAN_TYPE_B)
			continue;

		caller_data = ftdm_channel_get_caller_data(cur);

		if (caller_data->call_reference == cref) {
			chan = cur;
			break;
		}
	}

	ftdm_iterator_free(c_iter);
	ftdm_mutex_unlock(span->mutex);
	return chan;
}


/**
 * Hunt for free channel (NT-mode only)
 * \param[in]	span	Span to hunt on
 * \param[in]	hint	Channel ID hint (preferred by remote end)
 * \param[in]	excl	Is the hint exclusive (or preferred)?
 * \param[out]	chan	Selected channel
 * \retval	FTDM_SUCCESS	A free channel has been found
 * \retval	FTDM_FAIL	No free channels could be found on the span
 * \retval	FTDM_EBUSY	The channel indicated in the exclusive hint is already in use
 */
static ftdm_status_t hunt_channel(ftdm_span_t *span, const int hint, const ftdm_bool_t excl, ftdm_channel_t **chan)
{
	ftdm_iterator_t *c_iter, *c_cur;
	ftdm_channel_t *tmp = NULL;
	int ret = FTDM_FAIL;

	/* lock span */
	ftdm_mutex_lock(span->mutex);

	/* Check hint */
	if (hint > 0) {
		tmp = ftdm_span_get_channel(span, hint);
		if (!tmp) {
			ftdm_log(FTDM_LOG_NOTICE, "Invalid channel hint '%d' given (out of bounds)\n", hint);
		}
		else if (!ftdm_test_flag(tmp, FTDM_CHANNEL_INUSE) && ftdm_channel_get_type(tmp) == FTDM_CHAN_TYPE_B) {
			ftdm_log(FTDM_LOG_DEBUG, "Using channel '%d' from hint\n", ftdm_channel_get_id(tmp));
			ftdm_channel_use(tmp);
			ret = FTDM_SUCCESS;
			*chan = tmp;
			goto out;
		}
		else if (excl) {
			ftdm_log(FTDM_LOG_NOTICE, "Channel '%d' in exclusive hint is not available\n",
				ftdm_channel_get_id(tmp));
			ret = FTDM_EBUSY;
			goto out;
		}
	}

	c_iter = ftdm_span_get_chan_iterator(span, NULL);

	/* Iterate over all channels on this span */
	for (c_cur = c_iter; c_cur; c_cur = ftdm_iterator_next(c_cur)) {
		tmp = ftdm_iterator_current(c_cur);

		if (ftdm_channel_get_type(tmp) != FTDM_CHAN_TYPE_B)
			continue;

		if (!ftdm_test_flag(tmp, FTDM_CHANNEL_INUSE)) {
			ftdm_channel_use(tmp);
			ret = FTDM_SUCCESS;
			*chan = tmp;
			break;
		}
	}

	ftdm_iterator_free(c_iter);
out:
	ftdm_mutex_unlock(span->mutex);
	return ret;
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
	ftdm_libpri_data_t *isdn_data = span->signal_data;
	ftdm_libpri_b_chan_t *chan_priv = NULL;
	ftdm_channel_t *chan = NULL;
	ftdm_caller_data_t *caller_data = NULL;
	int ret = 0;

	ftdm_unused_arg(event_type);

	/*
	 * Check if call has an associated channel (duplicate ring event)
	 */
	if ((chan = find_channel_by_cref(span, pevent->ring.cref))) {
		ftdm_log_chan_msg(chan, FTDM_LOG_NOTICE, "-- Duplicate ring received (ignored)\n");
		return ret;
	}

	if (isdn_data->mode == PRI_NETWORK) {
		/*
		 * Always hunt for a free channel in NT-mode,
		 * but use the pre-selected one as hint
		 */
		switch (hunt_channel(span, pevent->ring.channel, !pevent->ring.flexible, &chan)) {
		case FTDM_SUCCESS:	/* OK channel found */
			break;
		case FTDM_EBUSY:	/* Exclusive channel hint is not available */
			ftdm_log(FTDM_LOG_ERROR, "-- New call without channel on span '%s' [NOTE: Initial SETUP w/o channel selection is not supported by FreeTDM]\n",
				ftdm_span_get_name(span));
			pri_hangup(spri->pri, pevent->ring.call, PRI_CAUSE_CHANNEL_UNACCEPTABLE);
			return ret;
		default:
			ftdm_log(FTDM_LOG_ERROR, "-- New call without channel on span '%s' [NOTE: Initial SETUP w/o channel selection is not supported by FreeTDM]\n",
				ftdm_span_get_name(span));
			pri_hangup(spri->pri, pevent->ring.call, PRI_CAUSE_NORMAL_CIRCUIT_CONGESTION);
			return ret;
		}

		ftdm_channel_lock(chan);

	} else if (pevent->ring.channel == -1) {
		/*
		 * TE-mode incoming call without channel selection (not supported)
		 */
		ftdm_log(FTDM_LOG_ERROR, "-- New call without channel on span '%s' [NOTE: Initial SETUP w/o channel selection is not supported by FreeTDM]\n",
			ftdm_span_get_name(span));
		pri_destroycall(spri->pri, pevent->ring.call);
		return ret;
	} else {
		/*
		 * TE-mode, check MSN filter, ignore calls that aren't for this PTMP terminal
		 */
		if (!msn_filter_match(isdn_data, pevent->ring.callednum)) {
			ftdm_log(FTDM_LOG_INFO, "-- MSN filter not matching incoming DNIS '%s', ignoring call\n",
				pevent->ring.callednum);
			pri_destroycall(spri->pri, pevent->ring.call);
			return ret;
		}

		/*
		 * TE-mode channel selection, use whatever the NT tells us to
		 */
		chan = ftdm_span_get_channel(span, pevent->ring.channel);
		if (!chan) {
			ftdm_log(FTDM_LOG_ERROR, "-- Unable to get channel %d:%d\n",
				ftdm_span_get_id(span), pevent->ring.channel);
			pri_hangup(spri->pri, pevent->ring.call, PRI_CAUSE_DESTINATION_OUT_OF_ORDER);
			return ret;
		}

		ftdm_channel_lock(chan);

		if (ftdm_channel_get_state(chan) != FTDM_CHANNEL_STATE_DOWN || ftdm_test_flag(chan, FTDM_CHANNEL_INUSE)) {
			ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "-- Selected channel is already in use\n");
			pri_hangup(spri->pri, pevent->ring.call, PRI_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto done;
		}

		/* Reserve channel */
		if (ftdm_channel_use(chan) != FTDM_SUCCESS) {
			ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "-- Error reserving channel\n");
			pri_hangup(spri->pri, pevent->ring.call, PRI_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto done;
		}
	}

	/* Get per-channel private data */
	chan_priv = chan->call_data;

	if (chan_priv->call) {
		/* we could drop the incoming call, but most likely the pointer is just a ghost of the past,
		 * this check is just to detect potentially unreleased pointers */
		ftdm_log_chan(chan, FTDM_LOG_WARNING, "Channel already has call %p!\n", chan_priv->call);
		chan_priv->call = NULL;
	}

	caller_data = ftdm_channel_get_caller_data(chan);

	memset(caller_data, 0, sizeof(*caller_data));

	/* Save CRV, so we can do proper duplicate RING detection */
	caller_data->call_reference = pevent->ring.cref;

	ftdm_set_string(caller_data->cid_num.digits, (char *)pevent->ring.callingnum);
	ftdm_set_string(caller_data->ani.digits, (char *)pevent->ring.callingani);
	ftdm_set_string(caller_data->dnis.digits, (char *)pevent->ring.callednum);
	ftdm_set_string(caller_data->rdnis.digits, (char *)pevent->ring.redirectingnum);

	if (pevent->ring.callingpres == PRES_ALLOWED_USER_NUMBER_NOT_SCREENED) {
		caller_data->pres = FTDM_PRES_ALLOWED;
		caller_data->screen = FTDM_SCREENING_NOT_SCREENED;
	} else if (pevent->ring.callingpres == PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN) {
		caller_data->pres = FTDM_PRES_ALLOWED;
		caller_data->screen = FTDM_SCREENING_VERIFIED_PASSED;
	} else if (pevent->ring.callingpres == PRES_ALLOWED_USER_NUMBER_FAILED_SCREEN) {
		caller_data->pres = FTDM_PRES_ALLOWED;
		caller_data->screen = FTDM_SCREENING_VERIFIED_FAILED;
	} else if (pevent->ring.callingpres == PRES_ALLOWED_NETWORK_NUMBER) {
		caller_data->pres = FTDM_PRES_ALLOWED;
		caller_data->screen = FTDM_SCREENING_NETWORK_PROVIDED;
	} else if (pevent->ring.callingpres == PRES_PROHIB_USER_NUMBER_NOT_SCREENED) {
		caller_data->pres = FTDM_PRES_RESTRICTED;
		caller_data->screen = FTDM_SCREENING_NOT_SCREENED;
	} else if (pevent->ring.callingpres == PRES_PROHIB_USER_NUMBER_PASSED_SCREEN) {
		caller_data->pres = FTDM_PRES_RESTRICTED;
		caller_data->screen = FTDM_SCREENING_VERIFIED_PASSED;
	} else if (pevent->ring.callingpres == PRES_PROHIB_USER_NUMBER_FAILED_SCREEN) {
		caller_data->pres = FTDM_PRES_RESTRICTED;
		caller_data->screen = FTDM_SCREENING_VERIFIED_FAILED;
	} else if (pevent->ring.callingpres == PRES_PROHIB_NETWORK_NUMBER) {
		caller_data->pres = FTDM_PRES_RESTRICTED;
		caller_data->screen = FTDM_SCREENING_NETWORK_PROVIDED;
	} else if (pevent->ring.callingpres == PRES_NUMBER_NOT_AVAILABLE) {
		caller_data->pres = FTDM_PRES_NOT_AVAILABLE;
		caller_data->screen = FTDM_SCREENING_NETWORK_PROVIDED;
	} else {
		caller_data->pres = FTDM_PRES_INVALID;
		caller_data->screen = FTDM_SCREENING_INVALID;
	}

	if (pevent->ring.callingplanani != -1) {
		caller_data->ani.type = pevent->ring.callingplanani >> 4;
		caller_data->ani.plan = pevent->ring.callingplanani & 0x0F;
	} else {
		/* the remote party did not sent a valid (according to libpri) ANI ton,
 		 * so let's use the callingplan ton/type and hope is correct.
 		 */
		caller_data->ani.type = pevent->ring.callingplan >> 4;
		caller_data->ani.plan = pevent->ring.callingplan & 0x0F;
	}
	
	caller_data->cid_num.type = pevent->ring.callingplan >> 4;
	caller_data->cid_num.plan = pevent->ring.callingplan & 0x0F;

	caller_data->dnis.type = pevent->ring.calledplan >> 4;
	caller_data->dnis.plan = pevent->ring.calledplan & 0x0F;

	if (!ftdm_strlen_zero((char *)pevent->ring.callingname)) {
		ftdm_set_string(caller_data->cid_name, (char *)pevent->ring.callingname);
	} else {
		ftdm_set_string(caller_data->cid_name, (char *)pevent->ring.callingnum);
	}

	if (pevent->ring.ani2 >= 0) {
		snprintf(caller_data->aniII, 5, "%.2d", pevent->ring.ani2);
	}

	// scary to trust this pointer, you'd think they would give you a copy of the call data so you own it......
	/* hurr, this is valid as along as nobody releases the call */
	chan_priv->call = pevent->ring.call;

	/* Open Channel if inband information is available */
	if ((pevent->ring.progressmask & PRI_PROG_INBAND_AVAILABLE)) {
		/* Open channel if inband information is available */
		ftdm_log(FTDM_LOG_DEBUG, "-- In-band information available, opening B-Channel %d:%d\n",
			ftdm_channel_get_span_id(chan),
			ftdm_channel_get_id(chan));

		if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN) && ftdm_channel_open_chan(chan) != FTDM_SUCCESS) {
//			ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

			ftdm_log(FTDM_LOG_WARNING, "-- Error opening channel %d:%d (ignored)\n",
				ftdm_channel_get_span_id(chan),
				ftdm_channel_get_id(chan));

//			caller_data->hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
//			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_TERMINATING);
//			goto done;
		}
	}

	ftdm_log(FTDM_LOG_NOTICE, "-- Ring on channel %d:%d (from %s to %s)\n", ftdm_span_get_id(span), pevent->ring.channel,
					  pevent->ring.callingnum, pevent->ring.callednum);

	/* Only go to RING state if we have the complete called number (indicated via pevent->complete flag) */
	if (!pevent->ring.complete && (isdn_data->overlap & FTMOD_LIBPRI_OVERLAP_RECEIVE)) {
		ftdm_log(FTDM_LOG_DEBUG, "RING event without complete indicator, waiting for more digits\n");
		ftdm_set_state(chan, FTDM_CHANNEL_STATE_COLLECT);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "RING event with complete indicator (or overlap receive disabled)\n");
		ftdm_set_state(chan, FTDM_CHANNEL_STATE_RING);
	}
done:
	ftdm_channel_unlock(chan);
	return ret;
}


/**
 * Timeout handler for T302 (overlap receiving)
 */
static int on_timeout_t302(struct lpwrap_pri *spri, struct lpwrap_timer *timer)
{
	ftdm_libpri_b_chan_t *chan_priv = ftdm_container_of(timer, ftdm_libpri_b_chan_t, t302);
	ftdm_channel_t *chan = chan_priv->channel;

	ftdm_unused_arg(spri);

	ftdm_log_chan_msg(chan, FTDM_LOG_INFO, "-- T302 timed out, going to state RING\n");
	ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RING);
	return 0;
}

/**
 * Timeout handler for T316 (RESTART ACK timer)
 */
static int on_timeout_t316(struct lpwrap_pri *spri, struct lpwrap_timer *timer)
{
	ftdm_libpri_b_chan_t *chan_priv = ftdm_container_of(timer, ftdm_libpri_b_chan_t, t316);
	ftdm_libpri_data_t *isdn_data = ftdm_container_of(spri, ftdm_libpri_data_t, spri);
	ftdm_channel_t *chan = chan_priv->channel;

	if (++chan_priv->t316_timeout_cnt > isdn_data->t316_max_attempts) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "-- T316 timed out, channel reached restart attempt limit '%d' and is suspended\n",
			isdn_data->t316_max_attempts);

		ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_SUSPENDED);
	} else {
		ftdm_log_chan_msg(chan, FTDM_LOG_WARNING, "-- T316 timed out, resending RESTART request\n");
		pri_reset(spri->pri, ftdm_channel_get_id(chan));

		/* Restart T316 */
		lpwrap_start_timer(spri, timer, isdn_data->t316_timeout_ms, &on_timeout_t316);
	}
	return 0;
}


/**
 * Timeout handler for T3xx (NT-mode idle restart)
 */
static int on_timeout_t3xx(struct lpwrap_pri *spri, struct lpwrap_timer *timer)
{
	ftdm_span_t *span = spri->span;
	ftdm_libpri_data_t *isdn_data = span->signal_data;
	ftdm_iterator_t *c_iter, *c_cur;

	ftdm_log_chan_msg(isdn_data->dchan, FTDM_LOG_INFO, "-- T3xx timed out, restarting idle b-channels\n");
	ftdm_mutex_lock(span->mutex);

	c_iter = ftdm_span_get_chan_iterator(span, NULL);

	/* Iterate b-channels */
	for (c_cur = c_iter; c_cur; c_cur = ftdm_iterator_next(c_cur)) {
		ftdm_channel_t *cur = ftdm_iterator_current(c_cur);
		/* Skip non-b-channels */
		if (ftdm_channel_get_type(cur) != FTDM_CHAN_TYPE_B)
			continue;
		/* Restart idle b-channels */
		if (ftdm_channel_get_state(cur) == FTDM_CHANNEL_STATE_DOWN && !ftdm_test_flag(cur, FTDM_CHANNEL_INUSE)) {
			ftdm_set_state_locked(cur, FTDM_CHANNEL_STATE_RESTART);
		}
	}
	ftdm_iterator_free(c_iter);
	ftdm_mutex_unlock(span->mutex);

	/* Start timer again */
	lpwrap_start_timer(spri, timer, isdn_data->idle_restart_timeout_ms, &on_timeout_t3xx);
	return 0;
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

	ftdm_unused_arg(span);

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
			ftdm_log_chan_msg(event->channel, FTDM_LOG_WARNING, "channel has alarms!\n");
		}
		break;
	case FTDM_OOB_ALARM_CLEAR:
		{
			ftdm_clear_flag(event->channel, FTDM_CHANNEL_SUSPENDED);
			ftdm_channel_get_alarms(event->channel, &alarmbits);
			ftdm_log_chan_msg(event->channel, FTDM_LOG_WARNING, "channel alarms cleared!\n");
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
	ftdm_channel_t *chan = NULL;
	ftdm_span_t *span = spri->span;
	int i;

	ftdm_unused_arg(event_type);

	if (pevent->restart.channel < 1) {
		ftdm_log_chan_msg(spri->dchan, FTDM_LOG_DEBUG, "-- Restarting interface\n");

		for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
			chan = ftdm_span_get_channel(span, i);
			if (!chan)
				continue;
			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
				ftdm_libpri_b_chan_t *chan_priv = chan->call_data;
				chan_priv->flags |= FTDM_LIBPRI_B_REMOTE_RESTART;		/* Remote triggered RESTART, set flag */
				ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
	}
	else if ((chan = ftdm_span_get_channel(span, pevent->restart.channel))) {
		if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
			ftdm_libpri_b_chan_t *chan_priv = chan->call_data;

			ftdm_log_chan_msg(chan, FTDM_LOG_DEBUG, "-- Restarting single channel\n");
			chan_priv->flags |= FTDM_LIBPRI_B_REMOTE_RESTART;
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RESTART);
		} else {
			ftdm_log_chan_msg(chan, FTDM_LOG_NOTICE, "Ignoring RESTART on D-Channel\n");
		}
	}
	else {
		ftdm_log(FTDM_LOG_ERROR, "Invalid restart indicator / channel id '%d' received\n",
			pevent->restart.channel);
	}

	_ftdm_channel_set_state_force(spri->dchan, FTDM_CHANNEL_STATE_UP);
	return 0;
}

/**
 * \brief Handler for libpri restart acknowledge event
 * \param spri Pri wrapper structure (libpri, span, dchan)
 * \param event_type Event type (unused)
 * \param pevent Event
 * \return 0
 */
static int on_restart_ack(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	ftdm_channel_t *chan = NULL;
	ftdm_span_t *span = spri->span;
	int i;

	ftdm_unused_arg(event_type);

	if (pevent->restartack.channel < 1) {
		ftdm_log_chan_msg(spri->dchan, FTDM_LOG_DEBUG, "-- Restart of interface completed\n");

		for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
			chan = ftdm_span_get_channel(span, i);
			if (!chan)
				continue;
			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
				ftdm_libpri_b_chan_t *chan_priv = chan->call_data;
				if (!(chan_priv->flags & FTDM_LIBPRI_B_REMOTE_RESTART)) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);
				}
			}
		}
	}
	else if ((chan = ftdm_span_get_channel(span, pevent->restart.channel))) {
		if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
			ftdm_log_chan_msg(chan, FTDM_LOG_DEBUG, "-- Restart of channel completed\n");
			ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_DOWN);
		} else {
			ftdm_log_chan_msg(chan, FTDM_LOG_NOTICE, "Ignoring RESTART ACK on D-Channel\n");
		}
	}
	else {
		ftdm_log(FTDM_LOG_ERROR, "Invalid restart indicator / channel id '%d' received\n",
			pevent->restartack.channel);
	}

	_ftdm_channel_set_state_force(spri->dchan, FTDM_CHANNEL_STATE_UP);
	return 0;
}




/*
 * FACILITY Advice-On-Charge handler
 */
#if defined(HAVE_LIBPRI_AOC) && defined(PRI_EVENT_FACILITY)
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
	ftdm_unused_arg(aoc_s);
	return 0;
}

static int handle_facility_aoc_d(const struct pri_subcmd_aoc_d *aoc_d)
{
	/* Left as an excercise to the reader */
	ftdm_unused_arg(aoc_d);
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
#endif

#ifdef PRI_EVENT_FACILITY
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
#ifdef HAVE_LIBPRI_AOC
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
#endif
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
	ftdm_unused_arg(event_type);
	ftdm_unused_arg(pevent);

	if (!ftdm_test_flag(spri, LPWRAP_PRI_READY)) {
		ftdm_signaling_status_t status = FTDM_SIG_STATE_UP;
		ftdm_span_t *span = spri->span;
		ftdm_libpri_data_t *isdn_data = span->signal_data;
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
			sig.ev_data.sigstatus.status = status;
			ftdm_span_send_signal(span, &sig);
		}

		/* NT-mode idle b-channel restart timer */
		if (ftdm_span_get_trunk_type(span) != FTDM_TRUNK_BRI_PTMP &&
		    isdn_data->mode == PRI_NETWORK && isdn_data->idle_restart_timeout_ms > 0)
		{
			ftdm_log_chan(isdn_data->dchan, FTDM_LOG_INFO, "Starting NT-mode idle b-channel restart timer (%d ms)\n",
				isdn_data->idle_restart_timeout_ms);
			lpwrap_start_timer(&isdn_data->spri, &isdn_data->t3xx, isdn_data->idle_restart_timeout_ms, &on_timeout_t3xx);
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
	ftdm_unused_arg(event_type);
	ftdm_unused_arg(pevent);

	if (ftdm_test_flag(spri, LPWRAP_PRI_READY)) {
		ftdm_signaling_status_t status = FTDM_SIG_STATE_DOWN;
		ftdm_span_t *span = spri->span;
		ftdm_libpri_data_t *isdn_data = span->signal_data;
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
			sig.ev_data.sigstatus.status = status;

			ftdm_span_send_signal(span, &sig);

			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
				ftdm_libpri_b_chan_t *chan_priv = chan->call_data;
				/* Stop T316 and reset counter */
				lpwrap_stop_timer(spri, &chan_priv->t316);
				chan_priv->t316_timeout_cnt = 0;
			}
		}

		/* NT-mode idle b-channel restart timer */
		ftdm_log_chan_msg(isdn_data->dchan, FTDM_LOG_INFO, "Stopping NT-mode idle b-channel restart timer\n");
		lpwrap_stop_timer(&isdn_data->spri, &isdn_data->t3xx);
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
	ftdm_log(FTDM_LOG_DEBUG, "-- Caught Event span %d %u (%s)\n", ftdm_span_get_id(spri->span), event_type, lpwrap_pri_event_str(event_type));
	switch (pevent->e) {
	case PRI_EVENT_CONFIG_ERR:
		{
			ftdm_log(FTDM_LOG_WARNING, "-- PRI error event: %s\n", pevent->err.err);
		}
		break;
	}
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
	ftdm_unused_arg(pevent);

	ftdm_log(FTDM_LOG_DEBUG, "-- Caught Event span %d %u (%s)\n", ftdm_span_get_id(spri->span), event_type, lpwrap_pri_event_str(event_type));
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
	int res = 0;
	int i;

	ftdm_unused_arg(me);

	ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);
	isdn_data->dchan = NULL;

	/*
	 * Open D-Channel
	 */
	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

		if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_DQ921) {
			if (ftdm_channel_open(ftdm_span_get_id(span), i, &isdn_data->dchan) == FTDM_SUCCESS) {
				ftdm_log_chan_msg(chan, FTDM_LOG_DEBUG, "Opened D-Channel\n");
				break;
			} else {
				ftdm_log_chan_msg(chan, FTDM_LOG_CRIT, "Failed to open D-Channel\n");
				goto out;
			}
		}
	}

	/*
	 * Initialize BRI/PRI context
	 */
	res = lpwrap_init_pri(&isdn_data->spri, span, isdn_data->dchan,
		isdn_data->dialect, isdn_data->mode, isdn_data->debug_mask);

	if (res) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to initialize BRI/PRI on span %d\n",
			ftdm_span_get_id(span));
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
#ifdef HAVE_LIBPRI_MAINT_SERVICE
	/* Support the different switch of service status */
	if (isdn_data->service_message_support) {
		pri_set_service_message_support(isdn_data->spri.pri, 1);
	}
#endif

	/* Callbacks for libpri events */
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_ANY, on_anything);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RING, on_ring);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RINGING, on_ringing);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_PROCEEDING, on_proceeding);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_PROGRESS, on_progress);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_ANSWER, on_answer);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_DCHAN_UP, on_dchan_up);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_DCHAN_DOWN, on_dchan_down);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP_REQ, on_hangup);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP_ACK, on_hangup);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP, on_hangup);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_INFO_RECEIVED, on_information);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_KEYPAD_DIGIT, on_keypad_digit);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RESTART, on_restart);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RESTART_ACK, on_restart_ack);
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_IO_FAIL, on_io_fail);
#ifdef PRI_EVENT_FACILITY
	LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_FACILITY, on_facility);
#endif

	/* Callback invoked on each iteration of the lpwrap_run_pri() event loop */
	isdn_data->spri.on_loop = check_flags;

	/*
	 * Event loop
	 */
	while (ftdm_running() && !ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {
		if (down) {
			ftdm_log(FTDM_LOG_INFO, "PRI back up on span %d\n", ftdm_span_get_id(span));
			ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);
			down = 0;
		}

		lpwrap_run_pri(&isdn_data->spri);

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
	/* close d-channel, if set */
	if (isdn_data->dchan) {
		if (ftdm_channel_close(&isdn_data->dchan) != FTDM_SUCCESS) {
			ftdm_log_chan_msg(isdn_data->dchan, FTDM_LOG_ERROR, "Failed to close D-Channel\n");
		}
	}

	ftdm_log(FTDM_LOG_DEBUG, "PRI thread ended on span %d\n", ftdm_span_get_id(span));

	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);
	ftdm_clear_flag(isdn_data, FTMOD_LIBPRI_RUNNING);

	lpwrap_destroy_pri(&isdn_data->spri);
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
		ftdm_log(FTDM_LOG_DEBUG, "Span %d already stopped, continuing anyway...\n", ftdm_span_get_id(span));
		return FTDM_SUCCESS;
	}

	ftdm_log(FTDM_LOG_INFO, "Stopping span [s%d][%s]\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	ftdm_set_state_all(span, FTDM_CHANNEL_STATE_RESTART);
	check_state(span);

	ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);
	lpwrap_stop_pri(&isdn_data->spri);

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

	ftdm_log(FTDM_LOG_INFO, "Starting span [s%d][%s]\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

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
 * \brief Parse overlap string to value
 * \param	val	String to parse
 * \return	Overlap flags
 */
static int parse_overlap_dial(const char *val)
{
	if (!strcasecmp(val, "yes") || !strcasecmp(val, "both"))
		return FTMOD_LIBPRI_OVERLAP_BOTH;
	if (!strcasecmp(val, "incoming") || !strcasecmp(val, "receive"))
		return FTMOD_LIBPRI_OVERLAP_RECEIVE;
	if (!strcasecmp(val, "outgoing") || !strcasecmp(val, "send"))
		return FTMOD_LIBPRI_OVERLAP_SEND;
	if (!strcasecmp(val, "no"))
		return FTMOD_LIBPRI_OVERLAP_NONE;

	return -1;
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
 * Parse timeout value with (convenience) modifier suffix
 * \param[in]	in	Input string, e.g. '1d' = 1 day, '7w' = 7 weeks, '3s' = 3 seconds
 * \todo	Could be simplified by using strtol() instead of atoi()
 */
static int parse_timeout(const char *in)
{
	const char *p_end = NULL, *p_start = in;
	int msec = 0;

	if (ftdm_strlen_zero(in))
		return 0;

	p_end = in + strlen(in);

	/* skip whitespace at start */
	while (p_start != p_end && *p_start == ' ')
		p_start++;

	/* skip whitespace at end */
	while (p_end != p_start && (*p_end == ' ' || *p_end == '\0'))
		p_end--;

	msec = atoi(p_start);

	switch (p_end[0]) {
	case 's':	/* seconds */
		msec *= 1000;
		break;
	case 'm':	/* minutes */
		if (p_end[1] != 's') msec *= 60 * 1000;
		break;
	case 'h':	/* hours */
		msec *= 3600 * 1000;
		break;
	case 'd':	/* days */
		msec *= 86400 * 1000;
		break;
	case 'w':	/* weeks */
		msec *= 604800 * 1000;
		break;
	default:	/* miliseconds */
		break;
	}
	return msec;
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
				return FTDM_FAIL;
			}
			dchan_count++;
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
		return FTDM_FAIL;
	}
	if (!bchan_count) {
		ftdm_log(FTDM_LOG_ERROR, "Span has no B-Channels!\n");
		return FTDM_FAIL;
	}

	isdn_data = ftdm_malloc(sizeof(*isdn_data));
	assert(isdn_data != NULL);
	memset(isdn_data, 0, sizeof(*isdn_data));

	/* set some default values */
	isdn_data->ton = PRI_UNKNOWN;
	isdn_data->overlap_timeout_ms = OVERLAP_TIMEOUT_MS_DEFAULT;
	isdn_data->idle_restart_timeout_ms = IDLE_RESTART_TIMEOUT_MS_DEFAULT;

	/*
	 * T316 restart ack timeout and retry limit
	 * (ITU-T Q.931 05/98 Paragraph 5.5.1 and Table 9-1)
	 */
	isdn_data->t316_timeout_ms   = T316_TIMEOUT_MS_DEFAULT;
	isdn_data->t316_max_attempts = T316_ATTEMPT_LIMIT_DEFAULT;


	/* Use span's trunk_mode as a reference for the default libpri mode */
	if (ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_NET) {
		isdn_data->mode = PRI_NETWORK;
	} else {
		isdn_data->mode = PRI_CPE;
	}

	switch (ftdm_span_get_trunk_type(span)) {
	case FTDM_TRUNK_BRI:
	case FTDM_TRUNK_BRI_PTMP:
#ifndef HAVE_LIBPRI_BRI
		ftdm_log(FTDM_LOG_ERROR, "Unsupported trunk type: '%s', libpri too old\n", ftdm_span_get_trunk_type_str(span));
		goto error;
#endif
	case FTDM_TRUNK_E1:
		ftdm_log(FTDM_LOG_NOTICE, "Setting default Layer 1 to ALAW since this is an E1/BRI/BRI PTMP trunk\n");
		isdn_data->layer1  = PRI_LAYER_1_ALAW;
		isdn_data->dialect = PRI_SWITCH_EUROISDN_E1;
		break;
	case FTDM_TRUNK_T1:
	case FTDM_TRUNK_J1:
		ftdm_log(FTDM_LOG_NOTICE, "Setting default Layer 1 to ULAW since this is a T1/J1 trunk\n");
		isdn_data->layer1  = PRI_LAYER_1_ULAW;
		isdn_data->dialect = PRI_SWITCH_LUCENT5E;
		break;
	default:
		ftdm_log(FTDM_LOG_ERROR, "Invalid trunk type: '%s'\n", ftdm_span_get_trunk_type_str(span));
		goto error;
	}

	/*
	 * Init MSN filter
	 */
	if (msn_filter_init(isdn_data) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to init MSN filter\n");
		goto error;
	}

	for (i = 0; ftdm_parameters[i].var; i++) {
		const char *var = ftdm_parameters[i].var;
		const char *val = ftdm_parameters[i].val;

		if (ftdm_strlen_zero(var)) {
			ftdm_log(FTDM_LOG_WARNING, "Skipping parameter with no name\n");
			continue;
		}

		if (ftdm_strlen_zero(val)) {
			ftdm_log(FTDM_LOG_ERROR, "Parameter '%s' has no value\n", var);
			goto error;
		}

		if (!strcasecmp(var, "node") || !strcasecmp(var, "mode")) {
			if ((isdn_data->mode = parse_mode(val)) == -1) {
				ftdm_log(FTDM_LOG_ERROR, "Unknown node type '%s'\n", val);
				goto error;
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
		else if (!strcasecmp(var, "overlapdial")) {
			if ((isdn_data->overlap = parse_overlap_dial(val)) == -1) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid overlap flag, ignoring parameter\n");
				isdn_data->overlap = FTMOD_LIBPRI_OVERLAP_NONE;
			}
		}
		else if (!strcasecmp(var, "digit_timeout") || !strcasecmp(var, "t302")) {
			int tmp = parse_timeout(val);
			if (!tmp) {
				isdn_data->overlap_timeout_ms = 0; /* disabled */
			}
			else if ((isdn_data->overlap_timeout_ms = ftdm_clamp(tmp, OVERLAP_TIMEOUT_MS_MIN, OVERLAP_TIMEOUT_MS_MAX)) != tmp) {
				ftdm_log(FTDM_LOG_WARNING, "'%s' value %d ms ('%s') outside of range [%d:%d] ms, using %d ms instead\n",
					var, tmp, val, OVERLAP_TIMEOUT_MS_MIN, OVERLAP_TIMEOUT_MS_MAX,
					isdn_data->overlap_timeout_ms);
			}
		}
		else if (!strcasecmp(var, "idle_restart_interval")) {
			int tmp = parse_timeout(val);
			if (!tmp) {
				isdn_data->idle_restart_timeout_ms = 0; /* disabled */
			}
			else if ((isdn_data->idle_restart_timeout_ms = ftdm_clamp(tmp, IDLE_RESTART_TIMEOUT_MS_MIN, IDLE_RESTART_TIMEOUT_MS_MAX)) != tmp) {
				ftdm_log(FTDM_LOG_WARNING, "'%s' value %d ms ('%s') outside of range [%d:%d] ms, using %d ms instead\n",
					var, tmp, val, IDLE_RESTART_TIMEOUT_MS_MIN, IDLE_RESTART_TIMEOUT_MS_MAX,
					isdn_data->idle_restart_timeout_ms);
			}
		}
		else if (!strcasecmp(var, "restart_timeout") || !strcasecmp(var, "t316")) {
			int tmp = parse_timeout(val);
			if (tmp <= 0) {
				ftdm_log(FTDM_LOG_ERROR, "'%s' value '%s' is invalid\n", var, val);
				goto error;
			}
			else if ((isdn_data->t316_timeout_ms = ftdm_clamp(tmp, T316_TIMEOUT_MS_MIN, T316_TIMEOUT_MS_MAX)) != tmp) {
				ftdm_log(FTDM_LOG_WARNING, "'%s' value %d ms ('%s') outside of range [%d:%d] ms, using %d ms instead\n",
					var, tmp, val, T316_TIMEOUT_MS_MIN, T316_TIMEOUT_MS_MAX,
					isdn_data->t316_timeout_ms);
			}
		}
		else if (!strcasecmp(var, "restart_attempts") || !strcasecmp(var, "t316_limit")) {
			int tmp = atoi(val);
			if (tmp <= 0) {
				ftdm_log(FTDM_LOG_ERROR, "'%s' value '%s' is invalid\n", var, val);
				goto error;
			}
			else if ((isdn_data->t316_max_attempts = ftdm_clamp(tmp, T316_ATTEMPT_LIMIT_MIN, T316_ATTEMPT_LIMIT_MAX)) != tmp) {
				ftdm_log(FTDM_LOG_WARNING, "'%s' value %d ('%s') outside of range [%d:%d], using %d instead\n",
					var, tmp, val, T316_ATTEMPT_LIMIT_MIN, T316_ATTEMPT_LIMIT_MAX,
					isdn_data->t316_max_attempts);
			}
		}
		else if (!strcasecmp(var, "debug")) {
			if (parse_debug(val, &isdn_data->debug_mask) == -1) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid debug flag, ignoring parameter\n");
				isdn_data->debug_mask = 0;
			}
		}
		else if (!strcasecmp(var, "service_message_support")) {
			if (ftdm_true(val)) {
				isdn_data->service_message_support = 1;
			}
		}
		else if (!strcasecmp(var, "local-number") || !strcasecmp(var, "msn")) {
			if (msn_filter_add(isdn_data, val) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid MSN/DDI(s) '%s' specified\n", val);
				goto error;
			}
		}
		else {
			ftdm_log(FTDM_LOG_ERROR, "Unknown parameter '%s', aborting configuration\n", var);
			goto error;
		}
	}

	/* Check if modes match and log a message if they do not. Just to be on the safe side. */
	if (isdn_data->mode == PRI_CPE && ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_NET) {
		ftdm_log(FTDM_LOG_WARNING, "Span '%s' signalling set up for TE/CPE/USER mode, while port is running in NT/NET mode. You may want to check your 'trunk_mode' settings.\n",
			ftdm_span_get_name(span));
	}
	else if (isdn_data->mode == PRI_NETWORK && ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_CPE) {
		ftdm_log(FTDM_LOG_WARNING, "Span '%s' signalling set up for NT/NET mode, while port is running in TE/CPE/USER mode. You may want to check your 'trunk_mode' settings.\n",
			ftdm_span_get_name(span));
	}

	span->start = ftdm_libpri_start;
	span->stop  = ftdm_libpri_stop;
	span->signal_cb = sig_cb;

	span->signal_data = isdn_data;
	span->signal_type = FTDM_SIGTYPE_ISDN;
	span->outgoing_call = isdn_outgoing_call;

	span->state_map = &isdn_state_map;
	span->state_processor = state_advance;

	span->get_channel_sig_status = isdn_get_channel_sig_status;
	span->get_span_sig_status = isdn_get_span_sig_status;

	/* move calls to PROCEED state when they hit dialplan (ROUTING state in FreeSWITCH) */
	ftdm_set_flag(span, FTDM_SPAN_USE_PROCEED_STATE);

	if ((isdn_data->opts & FTMOD_LIBPRI_OPT_SUGGEST_CHANNEL)) {
		span->channel_request = isdn_channel_request;
		ftdm_set_flag(span, FTDM_SPAN_SUGGEST_CHAN_ID);
	}

	/* Allocate per-channel private data */
	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

		if (!chan)
			continue;

		if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
			ftdm_libpri_b_chan_t *priv = NULL;

			priv = calloc(1, sizeof(*priv));
			if (!priv) {
				ftdm_log_chan_msg(chan, FTDM_LOG_CRIT, "Failed to allocate per-channel private data\n");
				goto error;
			}
			priv->channel   = chan;
			chan->call_data = priv;
		}
	}
	return FTDM_SUCCESS;
error:
	/* TODO: free per-channel private data */
	msn_filter_destroy(isdn_data);
	ftdm_safe_free(isdn_data);
	return FTDM_FAIL;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
