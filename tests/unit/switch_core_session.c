/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * Chris Rienzo <chris@signalwire.com>
 *
 *
 * switch_core_session.c -- tests sessions
 *
 */
#include <switch.h>
#include <test/switch_test.h>

/* Looks up the session's partner uuid with switch_channel_get_partner_uuid_copy() the given number of times and
   returns the last result. *pool_moved is set when the session pool handed out memory during the lookups.
   The check relies on a 0-byte switch_core_session_alloc() returning the pool's next free address without using it up,
   so the same address comes back until something else is allocated. A debug pool (APR_POOL_DEBUG, enabled by
   --enable-pool-sanitizer) mallocs every allocation separately, so two 0-byte allocations in a row return different
   addresses. The check is skipped then, and *pool_moved stays 0. */
static const char *partner_uuid_copy_repeat(switch_core_session_t *session, char *buf, switch_size_t buflen, int lookups, int *pool_moved)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	void *pool_mark = switch_core_session_alloc(session, 0);
	const char *uuid = NULL;
	int i;

	if (switch_core_session_alloc(session, 0) != pool_mark) {
		pool_mark = NULL;
	}

	for (i = 0; i < lookups; i++) {
		uuid = switch_channel_get_partner_uuid_copy(channel, buf, buflen);
	}

	*pool_moved = pool_mark && switch_core_session_alloc(session, 0) != pool_mark;

	return uuid;
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core_session)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_SESSION_BEGIN(session_external_id)
		{
			switch_core_session_t *session;
			fst_check(switch_core_session_set_external_id(fst_session, switch_core_session_get_uuid(fst_session)) == SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(switch_core_session_get_external_id(fst_session), switch_core_session_get_uuid(fst_session));
			fst_check(switch_core_session_set_external_id(fst_session, "foo") == SWITCH_STATUS_SUCCESS);
			session = switch_core_session_locate("foo");
			fst_requires(session);
			fst_check_string_equals(switch_core_session_get_uuid(session), switch_core_session_get_uuid(fst_session));
			fst_check_string_equals(switch_core_session_get_external_id(session), "foo");
			fst_check(switch_core_session_set_external_id(fst_session, "bar") == SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(switch_core_session_get_external_id(session), "bar");
			fst_requires(switch_core_session_locate("foo") == NULL);
			switch_core_session_rwunlock(session);
			session = switch_core_session_locate("bar");
			fst_requires(session);
			switch_core_session_rwunlock(session);
			session = switch_core_session_locate(switch_core_session_get_uuid(fst_session));
			fst_requires(session);
			switch_core_session_rwunlock(session);
			switch_channel_hangup(fst_channel, SWITCH_CAUSE_NORMAL_CLEARING);
			session = switch_core_session_locate("bar");
			fst_check(session == NULL);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(partner_uuid_copy)
		{
			char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";
			char long_bond[SWITCH_UUID_FORMATTED_LENGTH + 10];
			const char *partner_uuid = switch_core_session_get_uuid(fst_session);
			const char *uuid = NULL;
			int pool_moved = 0;

			/* no bond variable: no partner */
			uuid = switch_channel_get_partner_uuid_copy(fst_channel, uuid_str, sizeof(uuid_str));
			fst_check(uuid == NULL);

			/* signal_bond: every lookup copies the uuid into uuid_str, and 1000 lookups take nothing from the session pool */
			switch_channel_set_variable(fst_channel, SWITCH_SIGNAL_BOND_VARIABLE, partner_uuid);
			uuid = partner_uuid_copy_repeat(fst_session, uuid_str, sizeof(uuid_str), 1000, &pool_moved);
			fst_check(uuid == uuid_str);
			fst_check_string_equals(uuid_str, partner_uuid);
			fst_check(!pool_moved);

			/* originate_signal_bond is used when signal_bond is unset, with the same outcome */
			switch_channel_set_variable(fst_channel, SWITCH_SIGNAL_BOND_VARIABLE, NULL);
			switch_channel_set_variable(fst_channel, SWITCH_ORIGINATE_SIGNAL_BOND_VARIABLE, partner_uuid);
			memset(uuid_str, 0, sizeof(uuid_str));
			uuid = partner_uuid_copy_repeat(fst_session, uuid_str, sizeof(uuid_str), 1000, &pool_moved);
			fst_check(uuid == uuid_str);
			fst_check_string_equals(uuid_str, partner_uuid);
			fst_check(!pool_moved);

			/* a signal_bond longer than uuid_str: the copy is cut to SWITCH_UUID_FORMATTED_LENGTH bytes and ends with a NUL */
			memset(long_bond, 'a', sizeof(long_bond) - 1);
			long_bond[sizeof(long_bond) - 1] = '\0';
			switch_channel_set_variable(fst_channel, SWITCH_SIGNAL_BOND_VARIABLE, long_bond);
			memset(uuid_str, 'x', sizeof(uuid_str));
			uuid = switch_channel_get_partner_uuid_copy(fst_channel, uuid_str, sizeof(uuid_str));
			fst_check(uuid == uuid_str);
			fst_check(!memcmp(uuid_str, long_bond, SWITCH_UUID_FORMATTED_LENGTH));
			fst_check(uuid_str[SWITCH_UUID_FORMATTED_LENGTH] == '\0');

			switch_channel_set_variable(fst_channel, SWITCH_SIGNAL_BOND_VARIABLE, NULL);
			switch_channel_set_variable(fst_channel, SWITCH_ORIGINATE_SIGNAL_BOND_VARIABLE, NULL);
		}
		FST_SESSION_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
