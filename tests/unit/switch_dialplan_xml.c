/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2026, Walter Fan <walterfan@ustc.edu>
 *
 * Version: MPL 1.1
 *
 * switch_dialplan_xml.c -- XML dialplan hunt tests (GitHub #3141)
 *
 */
#include <switch.h>
#include <test/switch_test.h>

#ifndef SWITCH_MOD_BUILDDIR
#define SWITCH_MOD_BUILDDIR "../../src/mod"
#endif

/* Issue #3141 dialplan: empty DID plus Diversion sip:1(\d+)@ → $1 is the 10-digit DID. */
static const char dp_regex_all[] =
	"          <condition regex=\"all\">\n"
	"            <regex field=\"${did_number}\" expression=\"^$\"/>\n"
	"            <regex field=\"${sip_h_Diversion}\" expression=\"sip:1(\\d+)@.+$\"/>\n"
	"            <action application=\"log\" data=\"transfer to $1\"/>\n"
	"          </condition>\n";

/* Same capture without nested <regex>; used as a control that never hit the UAF. */
static const char dp_classic[] =
	"          <condition field=\"${sip_h_Diversion}\" expression=\"sip:1(\\d+)@.+$\">\n"
	"            <action application=\"log\" data=\"transfer to $1\"/>\n"
	"          </condition>\n";

static switch_status_t load_built_module(const char *rel_dir, const char *modname)
{
	const char *err = NULL;
	char path[1024];

	switch_snprintf(path, sizeof(path), "%s%s%s%s.libs", SWITCH_MOD_BUILDDIR, SWITCH_PATH_SEPARATOR, rel_dir, SWITCH_PATH_SEPARATOR);
	return switch_loadable_module_load_module(path, (char *) modname, SWITCH_TRUE, &err);
}

static char *write_alt_dialplan(switch_memory_pool_t *pool, const char *name, const char *condition_xml)
{
	char *path;
	FILE *fp;

	path = switch_core_sprintf(pool, "%s%s%s.xml", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, name);
	fp = fopen(path, "w");
	if (!fp) {
		return NULL;
	}

	/* alt_path hunt uses switch_xml_find_child(..., "dialplan", NULL, NULL),
	 * which returns the section itself, so <context> must hang off <section>. */
	fprintf(fp,
			"<?xml version=\"1.0\"?>\n"
			"<document type=\"freeswitch/xml\">\n"
			"  <section name=\"dialplan\">\n"
			"    <context name=\"default\">\n"
			"      <extension name=\"%s\">\n"
			"%s"
			"      </extension>\n"
			"    </context>\n"
			"  </section>\n"
			"</document>\n", name, condition_xml);
	fclose(fp);
	return path;
}

static switch_caller_extension_t *hunt_alt_xml(switch_core_session_t *session, const char *xml_path)
{
	switch_dialplan_interface_t *dp_interface;
	switch_caller_extension_t *extension;

	dp_interface = switch_loadable_module_get_dialplan_interface("XML");
	if (!dp_interface || !dp_interface->hunt_function) {
		return NULL;
	}

	extension = dp_interface->hunt_function(session, (void *) xml_path, NULL);
	UNPROTECT_INTERFACE(dp_interface);
	return extension;
}

static switch_caller_extension_t *hunt_condition(switch_core_session_t *session, switch_memory_pool_t *pool,
												 const char *name, const char *condition)
{
	char *xml_path;
	switch_caller_extension_t *extension;

	xml_path = write_alt_dialplan(pool, name, condition);
	if (!xml_path) {
		return NULL;
	}
	extension = hunt_alt_xml(session, xml_path);
	unlink(xml_path);
	return extension;
}

static switch_core_session_t *originate_null_session(void)
{
	switch_core_session_t *session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}
	return session;
}

static void hangup_session(switch_core_session_t *session)
{
	switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
	switch_core_session_rwunlock(session);
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_dialplan_xml)
	{
		FST_SETUP_BEGIN()
		{
			if (switch_loadable_module_exists("mod_loopback") != SWITCH_STATUS_SUCCESS) {
				fst_requires(load_built_module("endpoints/mod_loopback", "mod_loopback") == SWITCH_STATUS_SUCCESS);
			}
			if (switch_loadable_module_exists("mod_dialplan_xml") != SWITCH_STATUS_SUCCESS) {
				fst_requires(load_built_module("dialplans/mod_dialplan_xml", "mod_dialplan_xml") == SWITCH_STATUS_SUCCESS);
			}
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		/*
		 * Given: regex="all", empty did_number, Diversion sip:17001234567@...
		 * When: XML dialplan hunt runs
		 * Then: action data is "transfer to 7001234567" ($1 from the second regex)
		 */
		FST_TEST_BEGIN(regex_all_var_field_keeps_capture)
		{
			switch_core_session_t *session;
			switch_channel_t *channel;
			switch_caller_extension_t *extension;
			const char *expected[] = { "log", "transfer to 7001234567", NULL };

			session = originate_null_session();
			fst_requires(session);
			channel = switch_core_session_get_channel(session);
			switch_channel_set_variable(channel, "did_number", "");
			switch_channel_set_variable(channel, "sip_h_Diversion", "sip:17001234567@192.0.2.1;reason=unconditional");

			extension = hunt_condition(session, fst_pool, "issue_3141_regex_all", dp_regex_all);
			fst_check_extension_apps(expected, extension);
			hangup_session(session);
		}
		FST_TEST_END()

		/*
		 * Given: a classic <condition> on ${sip_h_Diversion} with the same capture
		 * When: XML dialplan hunt runs
		 * Then: action data is "transfer to 7001234567"
		 */
		FST_TEST_BEGIN(classic_var_field_keeps_capture)
		{
			switch_core_session_t *session;
			switch_channel_t *channel;
			switch_caller_extension_t *extension;
			const char *expected[] = { "log", "transfer to 7001234567", NULL };

			session = originate_null_session();
			fst_requires(session);
			channel = switch_core_session_get_channel(session);
			switch_channel_set_variable(channel, "did_number", "");
			switch_channel_set_variable(channel, "sip_h_Diversion", "sip:17001234567@192.0.2.1;reason=unconditional");

			extension = hunt_condition(session, fst_pool, "issue_3141_classic", dp_classic);
			fst_check_extension_apps(expected, extension);
			hangup_session(session);
		}
		FST_TEST_END()

		/*
		 * Given: regex="all", empty did_number, Diversion not matching sip:1(\d+)@
		 * When: XML dialplan hunt runs
		 * Then: no extension is queued (AND failed on the second regex)
		 */
		FST_TEST_BEGIN(regex_all_second_regex_fail_skips_action)
		{
			switch_core_session_t *session;
			switch_channel_t *channel;
			switch_caller_extension_t *extension;

			session = originate_null_session();
			fst_requires(session);
			channel = switch_core_session_get_channel(session);
			switch_channel_set_variable(channel, "did_number", "");
			switch_channel_set_variable(channel, "sip_h_Diversion", "sip:not-a-did@192.0.2.1;reason=unconditional");

			extension = hunt_condition(session, fst_pool, "issue_3141_regex_all_no_did", dp_regex_all);
			fst_check(extension == NULL);
			hangup_session(session);
		}
		FST_TEST_END()

		/*
		 * Given: regex="all", did_number already set, Diversion would otherwise match
		 * When: XML dialplan hunt runs
		 * Then: no extension is queued (AND failed on ${did_number} =~ /^$/)
		 */
		FST_TEST_BEGIN(regex_all_did_already_set_skips_action)
		{
			switch_core_session_t *session;
			switch_channel_t *channel;
			switch_caller_extension_t *extension;

			session = originate_null_session();
			fst_requires(session);
			channel = switch_core_session_get_channel(session);
			switch_channel_set_variable(channel, "did_number", "999");
			switch_channel_set_variable(channel, "sip_h_Diversion", "sip:17001234567@192.0.2.1;reason=unconditional");

			extension = hunt_condition(session, fst_pool, "issue_3141_regex_all_did_set", dp_regex_all);
			fst_check(extension == NULL);
			hangup_session(session);
		}
		FST_TEST_END()

		/*
		 * Given: a classic <condition> and a Diversion that does not match sip:1(\d+)@
		 * When: XML dialplan hunt runs
		 * Then: no extension is queued (no anti-action in the XML)
		 */
		FST_TEST_BEGIN(classic_var_field_mismatch_skips_action)
		{
			switch_core_session_t *session;
			switch_channel_t *channel;
			switch_caller_extension_t *extension;

			session = originate_null_session();
			fst_requires(session);
			channel = switch_core_session_get_channel(session);
			switch_channel_set_variable(channel, "sip_h_Diversion", "sip:not-a-did@192.0.2.1;reason=unconditional");

			extension = hunt_condition(session, fst_pool, "issue_3141_classic_mismatch", dp_classic);
			fst_check(extension == NULL);
			hangup_session(session);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
