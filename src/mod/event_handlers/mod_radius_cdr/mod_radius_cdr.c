/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Chris Parker <cparker@segv.org>
 * Mathieu Rene <mrene@avgs.ca>
 *
 *
 * mod_radius_cdr.c -- RADIUS CDR Module
 *
 */

#include <switch.h>
#include <sys/stat.h>
#include <freeradius-client.h>
#include "mod_radius_cdr.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_radius_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_radius_cdr_shutdown);
SWITCH_MODULE_DEFINITION(mod_radius_cdr, mod_radius_cdr_load, mod_radius_cdr_shutdown, NULL);

static struct {
	int shutdown;
	switch_thread_rwlock_t *rwlock;
} globals = {
0};

static char cf[] = "mod_radius_cdr.conf";
static char my_dictionary[PATH_MAX];
static char my_seqfile[PATH_MAX];
static char *my_deadtime;		/* 0 */
static char *my_timeout;		/* 5 */
static char *my_retries;		/* 3 */
static char my_servers[SERVER_MAX][255];

static rc_handle *my_radius_init(void)
{
	int i = 0;
	rc_handle *rad_config;

	rad_config = rc_new();

	if (rad_config == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_radius_cdr] Error initializing rc_handle!\n");
		return NULL;
	}

	rad_config = rc_config_init(rad_config);

	if (rad_config == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error initializing radius config!\n");
		rc_destroy(rad_config);
		return NULL;
	}

	/* Some hardcoded ( for now ) defaults needed to initialize radius */
	if (rc_add_config(rad_config, "auth_order", "radius", "mod_radius_cdr.c", 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting auth_order = radius failed\n");
		rc_destroy(rad_config);
		return NULL;
	}

	if (rc_add_config(rad_config, "seqfile", my_seqfile, "mod_radius_cdr.c", 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting seqfile = %s failed\n", my_seqfile);
		rc_destroy(rad_config);
		return NULL;
	}


	/* Add the module configs to initialize rad_config */

	for (i = 0; i < SERVER_MAX && my_servers[i][0] != '\0'; i++) {
		if (rc_add_config(rad_config, "acctserver", my_servers[i], cf, 0) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting acctserver = %s failed\n", my_servers[i]);
			rc_destroy(rad_config);
			return NULL;
		}
	}

	if (rc_add_config(rad_config, "dictionary", my_dictionary, cf, 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed setting dictionary = %s failed\n", my_dictionary);
		rc_destroy(rad_config);
		return NULL;
	}

	if (rc_add_config(rad_config, "radius_deadtime", my_deadtime, cf, 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting radius_deadtime = %s failed\n", my_deadtime);
		rc_destroy(rad_config);
		return NULL;
	}

	if (rc_add_config(rad_config, "radius_timeout", my_timeout, cf, 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting radius_timeout = %s failed\n", my_timeout);
		rc_destroy(rad_config);
		return NULL;
	}

	if (rc_add_config(rad_config, "radius_retries", my_retries, cf, 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting radius_retries = %s failed\n", my_retries);
		rc_destroy(rad_config);
		return NULL;
	}

	/* Read the dictionary file(s) */
	if (rc_read_dictionary(rad_config, rc_conf_str(rad_config, "dictionary")) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "reading dictionary file(s): %s\n", my_dictionary);
		rc_destroy(rad_config);
		return NULL;
	}

	return rad_config;
}

static switch_status_t my_on_routing(switch_core_session_t *session)
{
	switch_xml_t cdr = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	rc_handle *rad_config;
	switch_status_t retval = SWITCH_STATUS_TERM;
	VALUE_PAIR *send = NULL;
	uint32_t client_port = 0;
	uint32_t framed_addr = 0;
	uint32_t status_type = PW_STATUS_START;
	switch_time_t callstartdate = 0;
	switch_time_t callanswerdate = 0;
	switch_time_t callenddate = 0;
	switch_time_t calltransferdate = 0;
	const char *signal_bond = NULL;

	char *uuid_str;

	switch_time_exp_t tm;
	char buffer[32];

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] Entering my_on_routing\n");

	if (globals.shutdown) {
		return SWITCH_STATUS_FALSE;
	}

	if (channel) {
		const char *disable_flag = switch_channel_get_variable(channel, "disable_radius_start");
		if (switch_true(disable_flag)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] Not Sending RADIUS Start\n");
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_thread_rwlock_rdlock(globals.rwlock);

	rad_config = my_radius_init();

	if (rad_config == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Error initializing radius, Start packet not logged.\n");
		goto end;
	}

	if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
		uuid_str = switch_core_session_get_uuid(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Error Generating Data!\n");
		goto end;
	}

	/* Create the radius packet */

	/* Set Status Type */
	if (rc_avpair_add(rad_config, &send, PW_ACCT_STATUS_TYPE, &status_type, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Failed setting Acct-Status-Type: Start\n");
		rc_destroy(rad_config);
		goto end;
	}

	if (rc_avpair_add(rad_config, &send, PW_ACCT_SESSION_ID, uuid_str, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Failed adding Acct-Session-ID: %s\n", uuid_str);
		rc_destroy(rad_config);
		goto end;
	}

	/* Add VSAs */

	if (channel) {
		/*switch_call_cause_t   cause; */
		switch_caller_profile_t *profile;

		/*
		   cause = switch_channel_get_cause(channel);
		   if (rc_avpair_add(rad_config, &send, PW_FS_HANGUPCAUSE, &cause, -1, PW_FS_PEC) == NULL) {
		   switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Hangupcause: %d\n", cause);
		   rc_destroy(rad_config);
		   return SWITCH_STATUS_TERM;
		   }
		 */
		
		if ((signal_bond = switch_channel_get_partner_uuid(channel)) && !zstr(signal_bond)) {
			if (rc_avpair_add(rad_config, &send, PW_FS_OTHER_LEG_ID, (void*) signal_bond, -1, PW_FS_PEC) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Failed adding Freeswitch-Other-Leg-Id: %s\n", uuid_str);
				rc_destroy(rad_config);
				goto end;
			}
		}

		profile = switch_channel_get_caller_profile(channel);

		if (profile) {

			callstartdate = profile->times->created;
			callanswerdate = profile->times->answered;
			calltransferdate = profile->times->transferred;
			callenddate = profile->times->hungup;

			if (profile->username) {
				if (rc_avpair_add(rad_config, &send, PW_USER_NAME, (void *) profile->username, -1, 0) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding User-Name: %s\n", profile->username);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->caller_id_number) {
				if (rc_avpair_add(rad_config, &send, PW_FS_SRC, (void *) profile->caller_id_number, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Src: %s\n", profile->caller_id_number);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->caller_id_name) {
				if (rc_avpair_add(rad_config, &send, PW_FS_CLID, (void *) profile->caller_id_name, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-CLID: %s\n", profile->caller_id_name);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->destination_number) {
				if (rc_avpair_add(rad_config, &send, PW_FS_DST, (void *) profile->destination_number, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Dst: %s\n", profile->destination_number);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->dialplan) {
				if (rc_avpair_add(rad_config, &send, PW_FS_DIALPLAN, (void *) profile->dialplan, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Dialplan: %s\n", profile->dialplan);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->network_addr) {
				inet_pton(AF_INET, (void *) profile->network_addr, &framed_addr);
				framed_addr = htonl(framed_addr);
				if (rc_avpair_add(rad_config, &send, PW_FRAMED_IP_ADDRESS, &framed_addr, -1, 0) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Framed-IP-Address: %s\n", profile->network_addr);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->rdnis) {
				if (rc_avpair_add(rad_config, &send, PW_FS_RDNIS, (void *) profile->rdnis, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-RDNIS: %s\n", profile->rdnis);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->context) {
				if (rc_avpair_add(rad_config, &send, PW_FS_CONTEXT, (void *) profile->context, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Context: %s\n", profile->context);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->ani) {
				if (rc_avpair_add(rad_config, &send, PW_FS_ANI, (void *) profile->ani, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-ANI: %s\n", profile->ani);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->aniii) {
				if (rc_avpair_add(rad_config, &send, PW_FS_ANIII, (void *) profile->aniii, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-ANIII: %s\n", profile->aniii);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->source) {
				if (rc_avpair_add(rad_config, &send, PW_FS_SOURCE, (void *) profile->source, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Source: %s\n", profile->source);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (callstartdate > 0) {
				switch_time_exp_lt(&tm, callstartdate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLSTARTDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Callstartdate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (callanswerdate > 0) {
				switch_time_exp_lt(&tm, callanswerdate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLANSWERDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Callanswerdate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (calltransferdate > 0) {
				switch_time_exp_lt(&tm, calltransferdate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLTRANSFERDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Calltransferdate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (callenddate > 0) {
				switch_time_exp_lt(&tm, callenddate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLENDDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Callenddate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (profile->caller_extension && profile->caller_extension->last_application && profile->caller_extension->last_application->application_name) {
				if (rc_avpair_add(rad_config, &send, PW_FS_LASTAPP,
								  (void *) profile->caller_extension->last_application->application_name, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Lastapp: %s\n", profile->source);
					rc_destroy(rad_config);
					goto end;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "profile == NULL\n");
		}
	}

	if (rc_acct(rad_config, client_port, send) == OK_RC) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] RADIUS Accounting OK\n");
		retval = SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] RADIUS Accounting Failed\n");
		retval = SWITCH_STATUS_TERM;
	}
	rc_avpair_free(send);
	rc_destroy(rad_config);
  end:
	switch_xml_free(cdr);
	switch_thread_rwlock_unlock(globals.rwlock);
	return (retval);
}

static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_xml_t cdr = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	rc_handle *rad_config;
	switch_status_t retval = SWITCH_STATUS_TERM;
	VALUE_PAIR *send = NULL;
	uint32_t client_port = 0;
	uint32_t framed_addr = 0;
	uint32_t status_type = PW_STATUS_STOP;
	switch_time_t callstartdate = 0;
	switch_time_t callanswerdate = 0;
	switch_time_t callenddate = 0;
	switch_time_t calltransferdate = 0;
	switch_time_t billusec = 0;
	uint32_t billsec = 0;
	char *uuid_str;

	switch_time_exp_t tm;
	char buffer[32] = "";

	if (globals.shutdown) {
		return SWITCH_STATUS_FALSE;
	}


	if (channel) {
		const char *disable_flag = switch_channel_get_variable(channel, "disable_radius_stop");
		if (switch_true(disable_flag)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] Not Sending RADIUS Stop\n");
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_thread_rwlock_rdlock(globals.rwlock);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] Entering my_on_reporting\n");

	rad_config = my_radius_init();

	if (rad_config == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Error initializing radius, session not logged.\n");
		goto end;
	}

	if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
		uuid_str = switch_core_session_get_uuid(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[mod_radius_cdr] Error Generating Data!\n");
		goto end;
	}

	/* Create the radius packet */

	/* Set Status Type */
	if (rc_avpair_add(rad_config, &send, PW_ACCT_STATUS_TYPE, &status_type, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Acct-Session-ID: %s\n", uuid_str);
		rc_destroy(rad_config);
		goto end;
	}

	if (rc_avpair_add(rad_config, &send, PW_ACCT_SESSION_ID, uuid_str, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Acct-Session-ID: %s\n", uuid_str);
		rc_destroy(rad_config);
		goto end;
	}

	/* Add VSAs */

	if (channel) {
		switch_call_cause_t cause;
		switch_caller_profile_t *profile;

		cause = switch_channel_get_cause(channel);
		if (rc_avpair_add(rad_config, &send, PW_FS_HANGUPCAUSE, &cause, -1, PW_FS_PEC) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Hangupcause: %d\n", cause);
			rc_destroy(rad_config);
			goto end;
		}

		profile = switch_channel_get_caller_profile(channel);

		if (profile) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] Calculating billable time\n");

			/* calculate billable time */
			callstartdate = profile->times->created;
			callanswerdate = profile->times->answered;
			calltransferdate = profile->times->transferred;
			callenddate = profile->times->hungup;

			if (switch_channel_test_flag(channel, CF_ANSWERED)) {
				if (callstartdate && callanswerdate) {
					if (callenddate)
						billusec = callenddate - callanswerdate;
					else if (calltransferdate)
						billusec = calltransferdate - callanswerdate;
				}
			} else if (switch_channel_test_flag(channel, CF_TRANSFER)) {
				if (callanswerdate && calltransferdate)
					billusec = calltransferdate - callanswerdate;
			}
			billsec = (billusec / 1000000);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[mod_radius_cdr] Finished calculating billable time\n");

			if (profile->username) {
				if (rc_avpair_add(rad_config, &send, PW_USER_NAME, (void *) profile->username, -1, 0) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding User-Name: %s\n", profile->username);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->caller_id_number) {
				if (rc_avpair_add(rad_config, &send, PW_FS_SRC, (void *) profile->caller_id_number, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Src: %s\n", profile->caller_id_number);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->caller_id_name) {
				if (rc_avpair_add(rad_config, &send, PW_FS_CLID, (void *) profile->caller_id_name, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-CLID: %s\n", profile->caller_id_name);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->destination_number) {
				if (rc_avpair_add(rad_config, &send, PW_FS_DST, (void *) profile->destination_number, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Dst: %s\n", profile->destination_number);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->dialplan) {
				if (rc_avpair_add(rad_config, &send, PW_FS_DIALPLAN, (void *) profile->dialplan, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Dialplan: %s\n", profile->dialplan);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->network_addr) {
				inet_pton(AF_INET, (void *) profile->network_addr, &framed_addr);
				framed_addr = htonl(framed_addr);
				if (rc_avpair_add(rad_config, &send, PW_FRAMED_IP_ADDRESS, &framed_addr, -1, 0) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Framed-IP-Address: %s\n", profile->network_addr);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->rdnis) {
				if (rc_avpair_add(rad_config, &send, PW_FS_RDNIS, (void *) profile->rdnis, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-RDNIS: %s\n", profile->rdnis);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->context) {
				if (rc_avpair_add(rad_config, &send, PW_FS_CONTEXT, (void *) profile->context, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Context: %s\n", profile->context);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->ani) {
				if (rc_avpair_add(rad_config, &send, PW_FS_ANI, (void *) profile->ani, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-ANI: %s\n", profile->ani);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->aniii) {
				if (rc_avpair_add(rad_config, &send, PW_FS_ANIII, (void *) profile->aniii, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-ANIII: %s\n", profile->aniii);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->source) {
				if (rc_avpair_add(rad_config, &send, PW_FS_SOURCE, (void *) profile->source, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Source: %s\n", profile->source);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (profile->caller_extension && profile->caller_extension->last_application && profile->caller_extension->last_application->application_name) {
				if (rc_avpair_add(rad_config, &send, PW_FS_LASTAPP,
								  (void *) profile->caller_extension->last_application->application_name, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Lastapp: %s\n", profile->source);
					rc_destroy(rad_config);
					goto end;
				}
			}
			if (rc_avpair_add(rad_config, &send, PW_FS_BILLUSEC, &billusec, -1, PW_FS_PEC) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Billusec: %u\n", (uint32_t) billusec);
				rc_destroy(rad_config);
				goto end;
			}

			if (callstartdate > 0) {
				switch_time_exp_lt(&tm, callstartdate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLSTARTDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Callstartdate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (callanswerdate > 0) {
				switch_time_exp_lt(&tm, callanswerdate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLANSWERDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Callanswerdate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (calltransferdate > 0) {
				switch_time_exp_lt(&tm, calltransferdate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLTRANSFERDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Calltransferdate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (callenddate > 0) {
				switch_time_exp_lt(&tm, callenddate);
				switch_snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
				if (rc_avpair_add(rad_config, &send, PW_FS_CALLENDDATE, &buffer, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Callenddate: %s\n", buffer);
					rc_destroy(rad_config);
					goto end;
				}
			}

			if (rc_avpair_add(rad_config, &send, PW_ACCT_SESSION_TIME, &billsec, -1, 0) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Acct-Session-Time: %u\n", billsec);
				rc_destroy(rad_config);
				goto end;
			}
			
			{
				const char *direction_str = profile->direction == SWITCH_CALL_DIRECTION_INBOUND ? "inbound" : "outbound";
				
				if (rc_avpair_add(rad_config, &send, PW_FS_DIRECTION, (void *) direction_str, -1, PW_FS_PEC) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed adding Freeswitch-Direction: %s\n", direction_str);
					rc_destroy(rad_config);
					goto end;
				}
			}
			
		} else {				/* no profile, can't create data to send */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "profile == NULL\n");
		}
	}

	if (rc_acct(rad_config, client_port, send) == OK_RC) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RADIUS Accounting OK\n");
		retval = SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "RADIUS Accounting Failed\n");
		retval = SWITCH_STATUS_TERM;
	}
	rc_avpair_free(send);
	rc_destroy(rad_config);

  end:
	switch_xml_free(cdr);
	switch_thread_rwlock_unlock(globals.rwlock);
	return (retval);
}

static switch_status_t load_config(void)
{
	switch_xml_t cfg, xml, settings, param;

	int num_servers = 0;
	int i = 0;

	my_timeout = "5";
	my_retries = "3";
	my_deadtime = "0";
	strncpy(my_seqfile, "/var/run/radius.seq", PATH_MAX - 1);
	strncpy(my_dictionary, "/usr/local/freeswitch/conf/radius/dictionary", PATH_MAX - 1);

	for (i = 0; i < SERVER_MAX; i++) {
		my_servers[i][0] = '\0';
	}

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "acctserver")) {
				if (num_servers < SERVER_MAX) {
					strncpy(my_servers[num_servers], val, 255 - 1);
					num_servers++;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "you can only specify %d radius servers, ignoring excess server entry\n", SERVER_MAX);
				}
			} else if (!strcmp(var, "dictionary")) {
				strncpy(my_dictionary, val, PATH_MAX - 1);
			} else if (!strcmp(var, "seqfile")) {
				strncpy(my_seqfile, val, PATH_MAX - 1);
			} else if (!strcmp(var, "radius_timeout")) {
				my_timeout = strdup(val);
			} else if (!strcmp(var, "radius_retries")) {
				my_retries = strdup(val);
			} else if (!strcmp(var, "radius_deadtime")) {
				my_deadtime = strdup(val);
			}
		}
	}

	switch_xml_free(xml);

	if (num_servers < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "you must specify at least 1 radius server\n");
		return SWITCH_STATUS_TERM;
	}

	/* If we made it this far, we succeeded */
	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ my_on_routing,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ my_on_reporting
};

SWITCH_MODULE_LOAD_FUNCTION(mod_radius_cdr_load)
{

	switch_thread_rwlock_create(&globals.rwlock, pool);

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_radius_cdr_shutdown)
{

	globals.shutdown = 1;
	switch_core_remove_state_handler(&state_handlers);
	switch_thread_rwlock_wrlock(globals.rwlock);
	switch_thread_rwlock_unlock(globals.rwlock);

	return SWITCH_STATUS_SUCCESS;
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
