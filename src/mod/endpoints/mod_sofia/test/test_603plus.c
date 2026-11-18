/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2026, Anthony Minessale II <anthm@freeswitch.org>
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
 * Dmitry Verenitsin <dmitry.verenitsin@signalwire.com>
 *
 *
 * test_603plus.c -- Tests for SIP 603+ (ATIS-1000099) detection and passthrough
 *
 * Detection requires BOTH:
 *   1. SIP status 603 with phrase "Network Blocked" (case-insensitive)
 *   2. Reason header text starts with "v=analytics1;" (ATIS version AVP)
 *
 * Test approach: originate via loopback gateway (same FS instance).
 * The responding extension sends a crafted 603 with/without Reason header.
 * We bind to CHANNEL_HANGUP_COMPLETE to capture sip_603plus_reason from
 * the outbound leg before it is destroyed.
 *
 * Passthrough tests use a bridge scenario: originate -> middle extension
 * (sets passthrough) -> bridges to 603+ target. The originate leg receives
 * the response FROM the middle box, letting us verify what was actually sent.
 */

#include <switch.h>
#include <test/switch_test.h>

/* Event capture state */

static struct {
	char sip_603plus_reason[1024];
	char sip_invite_failure_phrase[256];
	char sip_reason[1024];
	switch_bool_t received;
} capture;

static void reset_capture(void)
{
	memset(capture.sip_603plus_reason, 0, sizeof(capture.sip_603plus_reason));
	memset(capture.sip_invite_failure_phrase, 0, sizeof(capture.sip_invite_failure_phrase));
	memset(capture.sip_reason, 0, sizeof(capture.sip_reason));
	capture.received = SWITCH_FALSE;
}

static void on_hangup_complete(switch_event_t *event)
{
	const char *direction, *val;

	/* Only capture from outbound legs (the originating call, not the responder).
	 * In bridge tests, multiple outbound legs hang up (bridge B-leg, then originate O-leg).
	 * Reset on every outbound event so the last one (O-leg) wins cleanly. */
	direction = switch_event_get_header(event, "Call-Direction");
	if (zstr(direction) || strcmp(direction, "outbound")) return;

	reset_capture();

	val = switch_event_get_header(event, "variable_sip_603plus_reason");
	if (!zstr(val)) {
		switch_snprintf(capture.sip_603plus_reason, sizeof(capture.sip_603plus_reason), "%s", val);
	}

	val = switch_event_get_header(event, "variable_sip_invite_failure_phrase");
	if (!zstr(val)) {
		switch_snprintf(capture.sip_invite_failure_phrase, sizeof(capture.sip_invite_failure_phrase), "%s", val);
	}

	val = switch_event_get_header(event, "variable_sip_reason");
	if (!zstr(val)) {
		switch_snprintf(capture.sip_reason, sizeof(capture.sip_reason), "%s", val);
	}

	capture.received = SWITCH_TRUE;
}

static void originate_and_wait(const char *dest, switch_call_cause_t *cause)
{
	switch_core_session_t *session = NULL;

	switch_ivr_originate(NULL, &session, cause,
		dest, 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);

	if (session) {
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
		switch_core_session_rwunlock(session);
	}

	/* Let event dispatch thread deliver CHANNEL_HANGUP_COMPLETE */
	switch_yield(1000000);
}

/* Test suite */

FST_CORE_EX_BEGIN("./conf", SCF_VG | SCF_USE_SQL)
{
FST_MODULE_BEGIN(mod_sofia, sofia)
{
	FST_SETUP_BEGIN()
	{
	}
	FST_SETUP_END()

	FST_TEARDOWN_BEGIN()
	{
	}
	FST_TEARDOWN_END()

	/* Detection: positive cases */

	FST_TEST_BEGIN(detect_valid_603plus_sip)
	{
		/*
		 * Extension +15553336050 sends:
		 *   603 Network Blocked
		 *   Reason: SIP;cause=603;text="v=analytics1;url=https://example.com/redress";location=TN
		 *
		 * Both conditions met -> sip_603plus_reason MUST be set.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336050", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(!zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must be set for valid 603+");
		fst_xcheck(!!strstr(capture.sip_603plus_reason, "v=analytics1"), "sip_603plus_reason must contain v=analytics1");
		fst_xcheck(!strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked"), "Failure phrase must be 'Network Blocked'");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(detect_valid_603plus_q850)
	{
		/*
		 * Extension +15553336051 sends:
		 *   603 Network Blocked
		 *   Reason: Q.850;cause=21;text="v=analytics1;url=https://example.com/redress";location=LN
		 *
		 * Q.850 protocol is equally valid per ATIS-1000099 section 4.1.1.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336051", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(!zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must be set for Q.850 ATIS Reason");
		fst_xcheck(!!strstr(capture.sip_603plus_reason, "v=analytics1"), "sip_603plus_reason must contain v=analytics1");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(detect_603plus_after_180)
	{
		/*
		 * Extension +15553336056 sends 180 Ringing, waits 500ms, then:
		 *   603 Network Blocked
		 *   Reason: SIP;cause=603;text="v=analytics1;url=https://example.com/redress";location=TN
		 *
		 * Detection must work after provisional responses.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336056", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(!zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must be set after 180+603");
		fst_xcheck(!!strstr(capture.sip_603plus_reason, "v=analytics1"), "sip_603plus_reason must contain v=analytics1");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	/* Detection: negative cases */

	FST_TEST_BEGIN(detect_wrong_phrase)
	{
		/*
		 * Extension +15553336052 sends:
		 *   603 Decline            <- wrong phrase
		 *   Reason: SIP;cause=603;text="v=analytics1;url=https://example.com/redress";location=TN
		 *
		 * Phrase is "Decline", not "Network Blocked". Detection must NOT fire.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336052", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must NOT be set when phrase is 'Decline'");
		/* sip_reason should still be set (existing behavior for any Reason header) */
		fst_xcheck(!zstr_buf(capture.sip_reason), "sip_reason should be set regardless of phrase");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(detect_no_analytics_in_reason)
	{
		/*
		 * Extension +15553336053 sends:
		 *   603 Network Blocked
		 *   Reason: Q.850;cause=21;text="Call Rejected"   <- no v=analytics1
		 *
		 * Reason header lacks v=analytics1. Detection must NOT fire.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336053", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must NOT be set without v=analytics1");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(detect_no_reason_header)
	{
		/*
		 * Extension +15553336054 sends:
		 *   603 Network Blocked
		 *   (no Reason header -- disable_q850_reason=true suppresses it)
		 *
		 * No Reason header -> sip->sip_reason is NULL. Detection must NOT fire.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336054", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must NOT be set without Reason header");
		fst_xcheck(zstr_buf(capture.sip_reason), "sip_reason should not be set when Reason header is suppressed");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(detect_non_603_status)
	{
		/*
		 * Extension +15553336055 sends:
		 *   486 Busy Here           <- not 603
		 *   Reason: SIP;cause=603;text="v=analytics1;url=https://example.com/redress";location=TN
		 *
		 * Status code is 486, not 603. Detection must NOT fire.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336055", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_USER_BUSY, "Expected USER_BUSY for 486");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(zstr_buf(capture.sip_603plus_reason), "sip_603plus_reason must NOT be set for non-603 status");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	/*
	 * Passthrough behavior.
	 *
	 * Bridge scenario: originate -> middle extension (sets passthrough) -> bridges to 603+ target.
	 * The originate leg receives the response FROM the middle box. We capture its
	 * sip_invite_failure_phrase and sip_reason to verify what was actually sent.
	 */

	FST_TEST_BEGIN(passthrough_true)
	{
		/*
		 * Extension +15553336060 sets sip_603plus_passthrough=true, bridges to 603+ target.
		 * The middle box should forward both "Network Blocked" phrase and ATIS Reason header.
		 * Our originate leg should see a valid 603+.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336060", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(!strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked"),
			"passthrough=true must preserve 'Network Blocked' phrase");
		fst_xcheck(!zstr_buf(capture.sip_603plus_reason),
			"passthrough=true must result in valid 603+ on originate leg");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(passthrough_false)
	{
		/*
		 * Extension +15553336061 sets sip_603plus_passthrough=false, bridges to 603+ target.
		 * The middle box should strip the ATIS Reason and use default phrase "Decline".
		 * Our originate leg should NOT see a 603+.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336061", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked") != 0,
			"passthrough=false must NOT send 'Network Blocked' phrase");
		fst_xcheck(zstr_buf(capture.sip_603plus_reason),
			"passthrough=false must strip ATIS Reason (no 603+ on originate leg)");
		fst_xcheck(zstr_buf(capture.sip_reason),
			"passthrough=false must suppress Reason header entirely");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(passthrough_default)
	{
		/*
		 * Extension +15553336062 does NOT set sip_603plus_passthrough, bridges to 603+ target.
		 * Default: phrase is "Decline" (existing behavior), but ATIS Reason leaks through.
		 * This is the backward-compatible state.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336062", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked") != 0,
			"default passthrough must NOT change phrase (stays 'Decline')");
		/* ATIS Reason leaks through via sip_reason -- this is existing behavior */
		fst_xcheck(!zstr_buf(capture.sip_reason),
			"default passthrough: sip_reason should still be set (existing behavior)");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	/*
	 * disable_q850_reason + passthrough combinations.
	 *
	 * Tests that disable_q850_reason and sip_603plus_passthrough work independently.
	 * disable_q850_reason suppresses standard Reason headers;
	 * sip_603plus_passthrough controls 603+ ATIS Reason forwarding.
	 */

	FST_TEST_BEGIN(disable_reason_passthrough_true)
	{
		/*
		 * Extension +15553336063: disable_q850_reason=true + sip_603plus_passthrough=true.
		 * Standard Reason suppressed, but ATIS 603+ Reason restored.
		 * The customer use case: suppress all Reason headers except FCC-required 603+.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336063", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(!strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked"),
			"disable_q850+passthrough=true must preserve 'Network Blocked' phrase");
		fst_xcheck(!zstr_buf(capture.sip_603plus_reason),
			"disable_q850+passthrough=true must restore ATIS Reason");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(disable_reason_passthrough_false)
	{
		/*
		 * Extension +15553336064: disable_q850_reason=true + sip_603plus_passthrough=false.
		 * Both suppress -- no Reason header at all.
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336064", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked") != 0,
			"disable_q850+passthrough=false must NOT send 'Network Blocked' phrase");
		fst_xcheck(zstr_buf(capture.sip_reason),
			"disable_q850+passthrough=false must suppress Reason header entirely");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(disable_reason_passthrough_default)
	{
		/*
		 * Extension +15553336065: disable_q850_reason=true, passthrough not set.
		 * disable_q850_reason suppresses everything, passthrough not set = no override.
		 * No Reason header, phrase is "Decline".
		 */
		switch_call_cause_t cause;

		switch_event_bind("test_603plus", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
			SWITCH_EVENT_SUBCLASS_ANY, on_hangup_complete, NULL);

		reset_capture();
		originate_and_wait("sofia/gateway/test/+15553336065", &cause);

		fst_xcheck(cause == SWITCH_CAUSE_CALL_REJECTED, "Expected CALL_REJECTED for 603");
		fst_xcheck(capture.received == SWITCH_TRUE, "Should have received outbound hangup event");
		fst_xcheck(strcasecmp(capture.sip_invite_failure_phrase, "Network Blocked") != 0,
			"disable_q850+default must NOT send 'Network Blocked' phrase");
		fst_xcheck(zstr_buf(capture.sip_reason),
			"disable_q850+default must suppress Reason header");

		switch_event_unbind_callback(on_hangup_complete);
	}
	FST_TEST_END()

}
FST_MODULE_END()
}
FST_CORE_END()
