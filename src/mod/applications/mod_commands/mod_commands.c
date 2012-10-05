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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Johny Kadarisman <jkr888@gmail.com>
 * Paul Tinsley <jackhammer@gmail.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Cesar Cepeda <cesar@auronix.com>
 * Massimo Cetra <devel@navynet.it>
 * Rupa Schomaker <rupa@rupa.com>
 * Joseph Sullivan <jossulli@amazon.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 *
 * 
 * mod_commands.c -- Misc. Command Module
 *
 */
#include <switch.h>
#include <switch_stun.h>
#include <switch_version.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_commands_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_commands_shutdown);
SWITCH_MODULE_DEFINITION(mod_commands, mod_commands_load, mod_commands_shutdown, NULL);

static switch_mutex_t *reload_mutex = NULL;

struct cb_helper {
	uint32_t row_process;
	switch_stream_handle_t *stream;
};

static int url_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct cb_helper *cb = (struct cb_helper *) pArg;

	cb->row_process++;

	if (!zstr(argv[0])) {
		cb->stream->write_function(cb->stream, "%s,", argv[0]);
	}

	return 0;
}

static switch_status_t select_url(const char *user,
					   const char *domain,
					   const char *concat,
					   const char *exclude_contact, 
					   switch_stream_handle_t *stream)
{
	struct cb_helper cb;
	char *sql, *errmsg = NULL;
	switch_core_flag_t cflags = switch_core_flags();
	switch_cache_db_handle_t *db = NULL;

	if (!(cflags & SCF_USE_SQL)) {
		stream->write_function(stream, "-ERR SQL DISABLED NO DATA AVAILABLE!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "%s", "-ERR Databse Error!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	cb.row_process = 0;
	cb.stream = stream;

	if (exclude_contact) {
		sql = switch_mprintf("select url, '%q' "
							 "from registrations where reg_user='%q' and realm='%q' "
							 "and url not like '%%%s%%'", (concat != NULL) ? concat : "", user, domain, exclude_contact);
	} else {
		sql = switch_mprintf("select url, '%q' "
							 "from registrations where reg_user='%q' and realm='%q'",
							 (concat != NULL) ? concat : "", user, domain);
	}

	switch_assert(sql);
	switch_cache_db_execute_sql_callback(db, sql, url_callback, &cb, &errmsg);

	if (errmsg) {
		stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
		free(errmsg);
		errmsg = NULL;
	}	

	switch_safe_free(sql);

	if (db) {
		switch_cache_db_release_db_handle(&db);
	}

	return SWITCH_STATUS_SUCCESS;
}


#define SAY_STRING_SYNTAX "<module_name>[.<ext>] <lang>[.<ext>] <say_type> <say_method> [<say_gender>] <text>"
SWITCH_STANDARD_API(say_string_function)
{
	char *argv[6] = { 0 };
	int argc;
	char *lbuf = NULL, *string = NULL;
	int err = 1, par = 0;
	char *p, *ext = "wav";
	char *tosay = NULL;
	int strip = 0;

	if (cmd) {
		lbuf = strdup(cmd);
	}

	if (lbuf && (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) && (argc == 5 || argc == 6)) {

		if ((p = strchr(argv[0], '.'))) {
			*p++ = '\0';
			ext = p;
			par++;
		}

		if (!par && (p = strchr(argv[1], '.'))) {
			*p++ = '\0';
			ext = p;
		}

		tosay = (argc == 5) ? argv[4] : argv[5];

		if (*tosay == '~') {
			tosay++;
			strip++;
		}

		switch_ivr_say_string(session,
							  argv[1],
							  ext,
							  tosay,
							  argv[0], 
							  argv[2], 
							  argv[3], 
							  (argc == 6) ? argv[4] : NULL , 
							  &string);
		if (string) {
			stream->write_function(stream, "%s", strip ? string + 14 : string);
			free(string);
			err = 0;
		}
	}

	if (err) {
		stream->write_function(stream, "-ERR Usage: %s\n", SAY_STRING_SYNTAX);
	}
	
	free(lbuf);

	return SWITCH_STATUS_SUCCESS;
	
}

struct user_struct {
	char *dname;
	char *gname;
	char *effective_caller_id_name;
	char *effective_caller_id_number;
	char *callgroup;
	switch_xml_t x_user_tag;
	switch_stream_handle_t *stream;
	char *search_context;
	char *context;
	switch_xml_t x_domain_tag;
};

static void dump_user(struct user_struct *us)
{
	switch_xml_t x_vars, x_var, ux, x_user_tag, x_domain_tag;
	switch_status_t status;
	switch_stream_handle_t apistream = { 0 }, *stream;
	char *user_context = NULL, *search_context = NULL, *context = NULL;
	char *effective_caller_id_name = NULL;
	char *effective_caller_id_number = NULL;
	char *dname = NULL, *gname = NULL, *callgroup = NULL;
	char *utype = NULL, *uname = NULL;
	char *apip = NULL;

	x_user_tag = us->x_user_tag;
	x_domain_tag = us->x_domain_tag;
	effective_caller_id_name = us->effective_caller_id_name;
	effective_caller_id_number = us->effective_caller_id_number;
	callgroup = us->callgroup;
	dname = us->dname;
	gname = us->gname;
	stream = us->stream;
	context = us->context;
	search_context = us->search_context;

	if (!x_user_tag) {
		return;
	}

	utype = (char *)switch_xml_attr_soft(us->x_user_tag, "type");
	uname = (char *)switch_xml_attr_soft(us->x_user_tag, "id");

	if (!strcasecmp(utype, "pointer")) {
		if (switch_xml_locate_user_in_domain(uname, x_domain_tag, &ux, NULL) == SWITCH_STATUS_SUCCESS) {
			x_user_tag = ux;
		}
	}

	user_context = (char *)context;

	if ((x_vars = switch_xml_child(x_user_tag, "variables"))) {
		for (x_var = switch_xml_child(x_vars, "variable"); x_var; x_var = x_var->next) {
			const char *key = switch_xml_attr_soft(x_var, "name");
			const char *val = switch_xml_attr_soft(x_var, "value");

			if (!strcasecmp(key, "user_context")) {
				user_context = (char*) val;
			} else if (!strcasecmp(key, "effective_caller_id_name")) {
				effective_caller_id_name = (char*) val;
			} else if (!strcasecmp(key, "effective_caller_id_number")) {
				effective_caller_id_number = (char*) val;
			} else if (!strcasecmp(key, "callgroup")) {
				callgroup = (char*) val;
			} else {
				continue;
			}
		}
	}

	if (search_context) {
		if (zstr(user_context) || strcasecmp(search_context, user_context)) {
			return;
		}
	}

	if(zstr(dname)) {
		apip = switch_mprintf("%s",switch_xml_attr_soft(x_user_tag, "id"));
	} else {
		apip = switch_mprintf("%s@%s",switch_xml_attr_soft(x_user_tag, "id"), dname);
	}

	SWITCH_STANDARD_STREAM(apistream);
	if ((status = switch_api_execute("sofia_contact", apip, NULL, &apistream)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sofia_contact '%s' failed. status: %d \n", apip, status );
		goto end;
	}
	stream->write_function(stream, "%s|%s|%s|%s|%s|%s|%s|%s\n", switch_xml_attr_soft(x_user_tag, "id"), user_context, dname, gname, apistream.data, callgroup, effective_caller_id_name, effective_caller_id_number);

end:
	switch_safe_free(apistream.data);
	switch_safe_free(apip);

	return;
}

#define LIST_USERS_SYNTAX "[group <group>] [domain <domain>] [user <user>] [context <context>]"
SWITCH_STANDARD_API(list_users_function)
{
	int argc;
	char *pdata, *argv[8];
	int32_t arg = 0;
	switch_xml_t xml_root, x_domains, x_domain_tag;
	switch_xml_t gts, gt, uts, ut;
	char *_user = NULL, *_domain = NULL, *_search_context = NULL, *_group = NULL;


	if ((pdata = strdup(cmd))) {
		argc = switch_separate_string(pdata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		if (argc >= 8) {
			stream->write_function(stream, "-USAGE: %s\n", LIST_USERS_SYNTAX);
			goto done;
		}

		for (arg = 0; arg < argc; arg++) {
			if (!strcasecmp(argv[arg], "user")) {
				_user = argv[arg + 1];
			}
			if (!strcasecmp(argv[arg], "domain")) {
				_domain = argv[arg + 1];
			}
			if (!strcasecmp(argv[arg], "context")) {
				_search_context = argv[arg + 1];
			}
			if (!strcasecmp(argv[arg], "group")) {
				_group = argv[arg + 1];
			}
		}
	}

	stream->write_function(stream, "userid|context|domain|group|contact|callgroup|effective_caller_id_name|effective_caller_id_number\n");

	if (switch_xml_locate("directory", NULL, NULL, NULL, &xml_root, &x_domains, NULL, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		struct user_struct us = { 0 };

		for (x_domain_tag = switch_xml_child(x_domains, "domain"); x_domain_tag; x_domain_tag = x_domain_tag->next) {
			switch_xml_t x_vars, x_var;

			us.dname = (char*)switch_xml_attr_soft(x_domain_tag, "name");

			if (_domain && strcasecmp(_domain, us.dname)) {
				continue;
			}

			if ((x_vars = switch_xml_child(x_domain_tag, "variables"))) {
				if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "user_context",  NULL))) {
					us.context = (char*)switch_xml_attr_soft(x_var, "value");
				}
				if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "callgroup",  NULL))) {
					us.callgroup = (char*)switch_xml_attr_soft(x_var, "value");
				}
				if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "effective_caller_id_name",  NULL))) {
					us.effective_caller_id_name = (char*)switch_xml_attr_soft(x_var, "value");
				}
				if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "effective_caller_id_number",  NULL))) {
					us.effective_caller_id_number = (char*)switch_xml_attr_soft(x_var, "value");
				}
			}

			if ((gts = switch_xml_child(x_domain_tag, "groups"))) {
				for (gt = switch_xml_child(gts, "group"); gt; gt = gt->next) {
					us.gname = (char*)switch_xml_attr_soft(gt, "name");

					if (_group && strcasecmp(_group, us.gname)) {
						continue;
					}

					if ((x_vars = switch_xml_child(gt, "variables"))) {
						if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "user_context",  NULL))) {
							us.context = (char*)switch_xml_attr_soft(x_var, "value");
						}
						if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "callgroup",  NULL))) {
							us.callgroup = (char*)switch_xml_attr_soft(x_var, "value");
						}
						if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "effective_caller_id_name",  NULL))) {
							us.effective_caller_id_name = (char*)switch_xml_attr_soft(x_var, "value");
						}
						if ((x_var = switch_xml_find_child_multi(x_vars, "variable", "name", "effective_caller_id_number",  NULL))) {
							us.effective_caller_id_number = (char*)switch_xml_attr_soft(x_var, "value");
						}
					}

					for (uts = switch_xml_child(gt, "users"); uts; uts = uts->next) {
						for (ut = switch_xml_child(uts, "user"); ut; ut = ut->next) {
							if (_user && strcasecmp(_user, switch_xml_attr_soft(ut, "id"))) {
								continue;
							}
							us.x_user_tag = ut;
							us.x_domain_tag = x_domain_tag;
							us.stream = stream;
							dump_user(&us);
						}
					}
				}
			} else {
				for (uts = switch_xml_child(x_domain_tag, "users"); uts; uts = uts->next) {
					for (ut = switch_xml_child(uts, "user"); ut; ut = ut->next) {
						if (_user && strcasecmp(_user, switch_xml_attr_soft(ut, "id"))) {
							continue;
						}
						us.x_user_tag = ut;
						us.x_domain_tag = x_domain_tag;
						us.stream = stream;
						dump_user(&us);
					}
				}
			}
		}
		switch_xml_free(xml_root);
	}

	stream->write_function(stream, "\n+OK\n");

done:
	switch_safe_free(pdata);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reg_url_function)
{
	char *data;
	char *user = NULL;
	char *domain = NULL, *dup_domain = NULL;
	char *concat = NULL;
	const char *exclude_contact = NULL;
	char *reply = "error/facility_not_subscribed";
	switch_stream_handle_t mystream = { 0 };


	if (!cmd) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}

	if (session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		exclude_contact = switch_channel_get_variable(channel, "sip_exclude_contact");
	}

	
	data = strdup(cmd);
	switch_assert(data);

	user = data;
	
	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
		if ((concat = strchr(domain, '/'))) {
			*concat++ = '\0';
		}
	} else {
		if ((concat = strchr(user, '/'))) {
			*concat++ = '\0';
		}
	}

	if (zstr(domain)) {
		dup_domain = switch_core_get_variable_dup("domain");
		domain = dup_domain;
	}

	if (!user) goto end;

	
	SWITCH_STANDARD_STREAM(mystream);
	switch_assert(mystream.data);

	select_url(user, domain, concat, exclude_contact, &mystream);
	reply = mystream.data;


 end:
	
	if (zstr(reply)) {
		reply = "error/user_not_registered";
	} else if (end_of(reply) == ',') {
		end_of(reply) = '\0';
	}

	stream->write_function(stream, "%s", reply);
	reply = NULL;

	switch_safe_free(mystream.data);					

	switch_safe_free(data);
	switch_safe_free(dup_domain);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(banner_function)
{
	stream->write_function(stream, "%s", switch_core_banner());
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(hostname_api_function)
{
	stream->write_function(stream, "%s", switch_core_get_hostname());
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(switchname_api_function)
{
	stream->write_function(stream, "%s", switch_core_get_switchname());
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(shutdown_function)
{
	switch_session_ctl_t command = SCSC_SHUTDOWN;
	int arg = 0;

	stream->write_function(stream, "+OK\n");
	switch_core_session_ctl(command, &arg);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(version_function)
{
	int argc;
	char *mydata = NULL, *argv[2];

	if (zstr(cmd)) {
		stream->write_function(stream, "FreeSWITCH Version %s (%s)\n", SWITCH_VERSION_FULL, SWITCH_VERSION_REVISION_HUMAN);
		goto end;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc > 0 && switch_stristr("short", argv[0])) {
		stream->write_function(stream, "%s.%s.%s\n", SWITCH_VERSION_MAJOR,SWITCH_VERSION_MINOR,SWITCH_VERSION_MICRO);
	} else {
		stream->write_function(stream, "FreeSWITCH Version %s (%s)\n", SWITCH_VERSION_FULL, SWITCH_VERSION_FULL_HUMAN);
	}

	switch_safe_free(mydata);

end:
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(db_cache_function)
{
	int argc;
	char *mydata = NULL, *argv[2];

	if (zstr(cmd)) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 1) {
		goto error;
	}
	if (argv[0] && switch_stristr("status", argv[0])) {
		switch_cache_db_status(stream);
		goto ok;
	} else {
		goto error;
	}

  error:
	stream->write_function(stream, "%s", "parameter missing\n");
  ok:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(host_lookup_function)
{
	char host[256] = "";

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", "parameter missing\n");
	} else {
		if (switch_resolve_host(cmd, host, sizeof(host)) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "%s", host);
		} else {
			stream->write_function(stream, "%s", "!err!");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(nat_map_function)
{
	int argc;
	char *mydata = NULL, *argv[5];
	switch_nat_ip_proto_t proto = SWITCH_NAT_UDP;
	switch_port_t external_port = 0;
	char *tmp = NULL;
	switch_bool_t sticky = SWITCH_FALSE;
	switch_bool_t mapping = SWITCH_TRUE;

	if (!cmd) {
		goto usage;
	}

	if (!switch_nat_is_initialized()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "nat_map API called while NAT not initialized\n");
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	if (argc < 1) {
		goto usage;
	}
	if (argv[0] && switch_stristr("status", argv[0])) {
		tmp = switch_nat_status();
		stream->write_function(stream, tmp);
		switch_safe_free(tmp);
		goto ok;
	} else if (argv[0] && switch_stristr("republish", argv[0])) {
		switch_nat_republish();
		stream->write_function(stream, "true");
		goto ok;
	} else if (argv[0] && switch_stristr("reinit", argv[0])) {
		switch_nat_reinit();
		tmp = switch_nat_status();
		stream->write_function(stream, tmp);
		switch_safe_free(tmp);
		goto ok;
	}

	if (argc < 2) {
		goto usage;
	}

	if (argv[0] && switch_stristr("mapping", argv[0])) {
		if (argv[1] && switch_stristr("enable", argv[1])) {
			mapping = SWITCH_TRUE;
		} else if (argv[1] && switch_stristr("disable", argv[1])) {
			mapping = SWITCH_FALSE;
		}

		switch_nat_set_mapping(mapping);
		tmp = switch_nat_status();
		stream->write_function(stream, tmp);
		switch_safe_free(tmp);
		goto ok;
	}

	if (argc < 3) {
		goto error;
	}

	if (argv[2] && switch_stristr("tcp", argv[2])) {
		proto = SWITCH_NAT_TCP;
	} else if (argv[2] && switch_stristr("udp", argv[2])) {
		proto = SWITCH_NAT_UDP;
	}

	if (argv[3] && switch_stristr("sticky", argv[3])) {
		sticky = SWITCH_TRUE;
	}

	if (argv[0] && switch_stristr("add", argv[0])) {
		if (switch_nat_add_mapping((switch_port_t) atoi(argv[1]), proto, &external_port, sticky) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "true");	/* still return true */
			goto ok;
		}
	} else if (argv[0] && switch_stristr("del", argv[0])) {
		if (switch_nat_del_mapping((switch_port_t) atoi(argv[1]), proto) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "true");
			goto ok;
		}
	}

  error:

	stream->write_function(stream, "false");
	goto ok;

 usage:
	stream->write_function(stream, "USAGE: nat_map [status|reinit|republish] | [add|del] <port> [tcp|udp] [sticky] | [mapping] <enable|disable>");

  ok:

	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(time_test_function)
{
	switch_time_t now, then;
	int x;
	long mss;
	uint32_t total = 0;
	int diff;
	int max = 10, a = 0;
	char *p;
	if (zstr(cmd)) {
		stream->write_function(stream, "parameter missing\n");
		return SWITCH_STATUS_SUCCESS;
	}

	mss = atol(cmd);

	if (mss > 1000000) {
		mss = 1000000;
	}

	if ((p = strchr(cmd, ' '))) {
		if ((a = atoi(p + 1)) > 0) {
			max = a;
			if (max > 100) {
				max = 100;
			}
		}
	}

	for (x = 1; x <= max; x++) {
		then = switch_time_ref();
		switch_yield(mss);
		now = switch_time_ref();
		diff = (int) (now - then);
		stream->write_function(stream, "test %d sleep %ld %d\n", x, mss, diff);
		total += diff;
	}
	stream->write_function(stream, "avg %d\n", total / (x > 1 ? x - 1 : 1));

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(msleep_function)
{
	if (cmd) {
		long ms = atol(cmd);
		switch_yield(ms * 1000);
	}

	stream->write_function(stream, "+OK");

	return SWITCH_STATUS_SUCCESS;
}


#define TIMER_TEST_SYNTAX "<10|20|40|60|120> [<1..200>] [<timer_name>]"

SWITCH_STANDARD_API(timer_test_function)
{
	switch_time_t now, then, start, end;
	int x;
	int mss = 20;
	uint32_t total = 0;
	int diff;
	int max = 50;
	switch_timer_t timer = { 0 };
	int argc = 0;
	char *argv[5] = { 0 };
	const char *timer_name = "soft";
	switch_memory_pool_t *pool;
	char *mycmd = NULL;

	switch_core_new_memory_pool(&pool);

	if (zstr(cmd)) {
		mycmd = "";
	} else {
		mycmd = switch_core_strdup(pool, cmd);
	}

	argc = switch_split(mycmd, ' ', argv);

	if (argc > 0) {
		mss = atoi(argv[0]);
	}

	if (argc > 1) {
		int tmp = atoi(argv[1]);
		if (tmp > 0 && tmp <= 400) {
			max = tmp;
		}
	}

	if (argc > 2) {
		timer_name = argv[2];
	}

	if (mss != 10 && mss != 20 && mss != 30 && mss != 32 && mss != 40 && mss != 60 && mss != 120) {
		stream->write_function(stream, "parameter missing: %s\n", TIMER_TEST_SYNTAX);
		goto end;
	}

	if (switch_core_timer_init(&timer, timer_name, mss, 1, pool) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "Timer Error!\n");
		goto end;
	}

	switch_core_timer_next(&timer); /* Step timer once before testing results below, to get first timestamp as accurate as possible */

	start = then = switch_time_ref();

	for (x = 1; x <= max; x++) {
		switch_core_timer_next(&timer);
		now = switch_time_ref();
		diff = (int) (now - then);
		total += diff;
		then = now;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Timer Test: %d sleep %d %d\n", x, mss, diff);
	}
	end = then;

	switch_yield(250000);

	stream->write_function(stream, "Avg: %0.3fms Total Time: %0.3fms\n", (float) ((float) (total / (x > 1 ? x - 1 : 1)) / 1000),
						   (float) ((float) (end - start) / 1000));

  end:

	switch_core_destroy_memory_pool(&pool);


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(group_call_function)
{
	char *domain, *dup_domain = NULL;
	char *group_name = NULL;
	char *flags;
	int ok = 0;
	switch_channel_t *channel = NULL;
	char *fp = NULL;
	const char *call_delim = ",";

	if (zstr(cmd)) {
		goto end;
	}

	if (session) {
		channel = switch_core_session_get_channel(session);
	}

	group_name = strdup(cmd);
	switch_assert(group_name);

	if ((flags = strchr(group_name, '+'))) {
		*flags++ = '\0';
		for (fp = flags; fp && *fp; fp++) {
			switch (*fp) {
			case 'F':
				call_delim = "|";
				break;
			case 'A':
				call_delim = ",";
				break;
			case 'E':
				call_delim = SWITCH_ENT_ORIGINATE_DELIM;
				break;
			default:
				break;
			}
		}
	}

	domain = strchr(group_name, '@');

	if (domain) {
		*domain++ = '\0';
	} else {
		if ((dup_domain = switch_core_get_variable_dup("domain"))) {
			domain = dup_domain;
		}
	}

	if (!zstr(domain)) {
		switch_xml_t xml, x_domain, x_group;
		switch_event_t *params;
		switch_stream_handle_t dstream = { 0 };

		SWITCH_STANDARD_STREAM(dstream);

		switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "group", group_name);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "domain", domain);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "group_call");

		if (switch_xml_locate_group(group_name, domain, &xml, &x_domain, &x_group, params) == SWITCH_STATUS_SUCCESS) {
			switch_xml_t x_user, x_users, x_param, x_params, my_x_user;

			if ((x_users = switch_xml_child(x_group, "users"))) {
				ok++;

				for (x_user = switch_xml_child(x_users, "user"); x_user; x_user = x_user->next) {
					const char *id = switch_xml_attr_soft(x_user, "id");
					const char *x_user_type = switch_xml_attr_soft(x_user, "type");
					const char *dest = NULL;
					char *d_dest = NULL;
					switch_xml_t xml_for_pointer = NULL, x_domain_for_pointer = NULL, x_group_for_pointer = NULL, x_user_for_pointer = NULL;

					my_x_user = x_user;

					if (!strcmp(x_user_type, "pointer")) {
						if (switch_xml_locate_user("id", id, domain, NULL,
												   &xml_for_pointer, &x_domain_for_pointer,
												   &x_user_for_pointer, &x_group_for_pointer, params) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", id, domain);
							goto done_x_user;
						}
						my_x_user = x_user_for_pointer;
					}

					if ((x_params = switch_xml_child(x_domain, "params"))) {
						for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
							const char *var = switch_xml_attr(x_param, "name");
							const char *val = switch_xml_attr(x_param, "value");

							if (!strcasecmp(var, "group-dial-string")) {
								dest = val;
								break;
							}

							if (!strcasecmp(var, "dial-string")) {
								dest = val;
							}
						}
					}

					if ((x_params = switch_xml_child(x_group, "params"))) {
						for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
							const char *var = switch_xml_attr(x_param, "name");
							const char *val = switch_xml_attr(x_param, "value");

							if (!strcasecmp(var, "group-dial-string")) {
								dest = val;
								break;
							}

							if (!strcasecmp(var, "dial-string")) {
								dest = val;
							}
						}
					}

					if ((x_params = switch_xml_child(my_x_user, "params"))) {
						for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
							const char *var = switch_xml_attr(x_param, "name");
							const char *val = switch_xml_attr(x_param, "value");

							if (!strcasecmp(var, "group-dial-string")) {
								dest = val;
								break;
							}

							if (!strcasecmp(var, "dial-string")) {
								dest = val;
							}
						}
					}

					if (dest) {
						if (channel) {
							switch_channel_set_variable(channel, "dialed_group", group_name);
							switch_channel_set_variable(channel, "dialed_user", id);
							switch_channel_set_variable(channel, "dialed_domain", domain);
							d_dest = switch_channel_expand_variables(channel, dest);
						} else {
							switch_event_del_header(params, "dialed_user");
							switch_event_del_header(params, "dialed_group");
							switch_event_del_header(params, "dialed_domain");
							switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "dialed_user", id);
							switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "dialed_group", group_name);
							switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
							d_dest = switch_event_expand_headers(params, dest);
						}
					} else {
						d_dest = switch_mprintf("user/%s@%s", id, domain);
					}

					if (d_dest) {
						dstream.write_function(&dstream, "%s%s", d_dest, call_delim);

						if (d_dest != dest) {
							free(d_dest);
						}
					}

				  done_x_user:
					if (xml_for_pointer) {
						switch_xml_free(xml_for_pointer);
						xml_for_pointer = NULL;
					}
				}

				if (ok && dstream.data) {
					char *data = (char *) dstream.data;
					char *p;

					if ((p = strstr(end_of_p(data) - 3, call_delim))) {
						*p = '\0';
					}

					for (p = data; p && *p; p++) {
						if (*p == '{') {
							*p = '[';
						} else if (*p == '}') {
							*p = ']';
						}
					}


					stream->write_function(stream, "%s", data);
					free(dstream.data);
				} else {
					ok = 0;
				}

			}
		}
		switch_xml_free(xml);
		switch_event_destroy(&params);
	}

  end:
	
	switch_safe_free(group_name);
	switch_safe_free(dup_domain);

	if (!ok) {
		stream->write_function(stream, "error/NO_ROUTE_DESTINATION");
	}
	
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(in_group_function)
{
	switch_xml_t x_domain, xml = NULL, x_user = NULL, x_group;
	int argc;
	char *mydata = NULL, *argv[2], *user, *domain, *dup_domain = NULL;
	char delim = ',';
	switch_event_t *params = NULL;
	const char *rval = "false";
	char *group;

	if (zstr(cmd) || !(mydata = strdup(cmd))) {
		goto end;
	}

	if ((argc = switch_separate_string(mydata, delim, argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		goto end;
	}

	user = argv[0];
	group = argv[1];

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	} else {
		if ((dup_domain = switch_core_get_variable_dup("domain"))) {
			domain = dup_domain;
		}
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "user", user);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "domain", domain);

	if (switch_xml_locate_group(group, domain, &xml, &x_domain, &x_group, params) == SWITCH_STATUS_SUCCESS) {
		switch_xml_t x_users;
		if ((x_users = switch_xml_child(x_group, "users"))) {
			if ((x_user = switch_xml_find_child(x_users, "user", "id", user))) {
				rval = "true";
			}
		}
	}

  end:

	stream->write_function(stream, "%s", rval);

	switch_xml_free(xml);
	switch_safe_free(mydata);
	switch_safe_free(dup_domain);
	switch_event_destroy(&params);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(user_data_function)
{
	switch_xml_t x_domain, xml = NULL, x_user = NULL, x_group = NULL, x_param, x_params;
	int argc;
	char *mydata = NULL, *argv[3], *key = NULL, *type = NULL, *user, *domain, *dup_domain = NULL;
	char delim = ' ';
	const char *container = "params", *elem = "param";
	const char *result = NULL;
	switch_event_t *params = NULL;

	if (zstr(cmd) || !(mydata = strdup(cmd))) {
		goto end;
	}

	if ((argc = switch_separate_string(mydata, delim, argv, (sizeof(argv) / sizeof(argv[0])))) < 3) {
		goto end;
	}

	user = argv[0];
	type = argv[1];
	key = argv[2];

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	} else {
		if ((dup_domain = switch_core_get_variable_dup("domain"))) {
			domain = dup_domain;
		} else {
			domain = "cluecon.com";
		}
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "user", user);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "domain", domain);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "type", type);

	if (key && type && switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, &x_group, params) == SWITCH_STATUS_SUCCESS) {
		if (!strcmp(type, "attr")) {
			const char *attr = switch_xml_attr_soft(x_user, key);
			result = attr;
			goto end;
		}

		if (!strcmp(type, "var")) {
			container = "variables";
			elem = "variable";
		}

		if ((x_params = switch_xml_child(x_domain, container))) {
			for (x_param = switch_xml_child(x_params, elem); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");

				if (var && val && !strcasecmp(var, key)) {
					result = val;
				}

			}
		}

		if (x_group && (x_params = switch_xml_child(x_group, container))) {
			for (x_param = switch_xml_child(x_params, elem); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");

				if (var && val && !strcasecmp(var, key)) {
					result = val;
				}
			}
		}

		if ((x_params = switch_xml_child(x_user, container))) {
			for (x_param = switch_xml_child(x_params, elem); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");

				if (var && val && !strcasecmp(var, key)) {
					result = val;
				}
			}
		}
	}

  end:
	if (result) {
		stream->write_function(stream, "%s", result);
	}
	switch_xml_free(xml);
	switch_safe_free(mydata);
	switch_safe_free(dup_domain);
	switch_event_destroy(&params);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t _find_user(const char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream, switch_bool_t tf)
{
	switch_xml_t x_domain = NULL, x_user = NULL, xml = NULL;
	int argc;
	char *mydata = NULL, *argv[3];
	char *key, *user, *domain;
	char *xmlstr;
	char *path_info = NULL;
	char delim = ' ';
	char *host = NULL;
	const char *err = NULL;

	if (stream->param_event && (host = switch_event_get_header(stream->param_event, "http-host"))) {
		stream->write_function(stream, "Content-Type: text/xml\r\n\r\n");
		if ((path_info = switch_event_get_header(stream->param_event, "http-path-info"))) {
			cmd = path_info;
			delim = '/';
		}
	}

	if (!cmd) {
		err = "bad args";
		goto end;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, delim, argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 3) {
		err = "bad args";
		goto end;
	}

	key = argv[0];
	user = argv[1];
	domain = argv[2];

	if (!(key && user && domain)) {
		err = "bad args";
		goto end;
	}

	if (switch_xml_locate_user(key, user, domain, NULL, &xml, &x_domain, &x_user, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		err = "can't find user";
		goto end;
	}

  end:
	if (session || tf) {
		stream->write_function(stream, err ? "false" : "true");
	} else {
		if (err) {
			if (host) {
				stream->write_function(stream, "<error>%s</error>\n", err);
			} else {
				stream->write_function(stream, "-Error %s\n", err);
			}
		}

		if (x_user) {
			xmlstr = switch_xml_toxml(x_user, SWITCH_FALSE);
			switch_assert(xmlstr);

			stream->write_function(stream, "%s", xmlstr);
			free(xmlstr);
		}
	}

	switch_xml_free(xml);

	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}



SWITCH_STANDARD_API(md5_function)
{
	char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", "!err!");
	} else {
		switch_md5_string(digest, (void *) cmd, strlen(cmd));
		stream->write_function(stream, "%s", digest);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(url_decode_function)
{
	char *reply = "";
	char *data = NULL;

	if (!zstr(cmd)) {
		data = strdup(cmd);
		switch_url_decode(data);
		reply = data;
	}

	stream->write_function(stream, "%s", reply);

	switch_safe_free(data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(echo_function)
{
	stream->write_function(stream, "%s", cmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(stun_function)
{
	char *stun_ip = NULL;
	switch_port_t stun_port = (switch_port_t) SWITCH_STUN_DEFAULT_PORT;
	char *p;
	char ip_buf[256] = "";
	char *ip = NULL;
	char *pip = NULL;
	switch_port_t port = 0;
	switch_memory_pool_t *pool = NULL;
	char *error = "";

	ip = ip_buf;

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", "-STUN Failed! NO STUN SERVER\n");
		return SWITCH_STATUS_SUCCESS;
	}

	stun_ip = strdup(cmd);
	switch_assert(stun_ip);

	if ((p = strchr(stun_ip, ':'))) {
		int iport;
		*p++ = '\0';
		iport = atoi(p);
		if (iport > 0 && iport < 0xFFFF) {
			stun_port = (switch_port_t) iport;
		}
	} else {
		p = stun_ip;
	}

	if (p && (pip = strchr(p, ' '))) {
		*pip++ = '\0';
	}

	if (pip) {
		switch_copy_string(ip_buf, pip, sizeof(ip_buf));
	} else {
		switch_find_local_ip(ip_buf, sizeof(ip_buf), NULL, AF_INET);
	}

	switch_core_new_memory_pool(&pool);

	if (zstr(stun_ip)) {
		stream->write_function(stream, "%s", "-STUN Failed! NO STUN SERVER\n");
	} else {
		if ((switch_stun_lookup(&ip, &port, stun_ip, stun_port, &error, pool)) == SWITCH_STATUS_SUCCESS && ip && port) {
			stream->write_function(stream, "%s:%u\n", ip, port);
		} else {
			stream->write_function(stream, "-STUN Failed! [%s]\n", error);
		}
	}

	switch_core_destroy_memory_pool(&pool);
	free(stun_ip);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(expand_function)
{
	char *expanded;
	char *dup;
	char *arg;
	char *mycmd;
	switch_status_t status;
	const char *p;
	switch_core_session_t *xsession;
	char uuid[80] = "";

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR, no input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	dup = strdup(cmd);
	mycmd = dup;

	if (!strncasecmp(mycmd, "uuid:", 5)) {
		p = cmd + 5;
		if ((mycmd = strchr(p, ' ')) && *mycmd++) {
			switch_copy_string(uuid, p, mycmd - p);
		}
	}

	if (zstr(mycmd)) {
		stream->write_function(stream, "-ERR, no input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (*uuid) {
		if ((xsession = switch_core_session_locate(uuid))) {
			switch_channel_event_set_data(switch_core_session_get_channel(xsession), stream->param_event);
			switch_core_session_rwunlock(xsession);
		}
	}

	if ((arg = strchr(mycmd, ' '))) {
		*arg++ = '\0';
	}

	expanded = arg ? switch_event_expand_headers(stream->param_event, arg) : arg;
	if ((status = switch_api_execute(mycmd, expanded, session, stream)) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR, error executing command\n");
	}

	if (expanded != arg) {
		free(expanded);
		expanded = NULL;
	}

	free(dup);
	dup = NULL;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(console_complete_function)
{
	const char *p, *cursor = NULL;
	int c;

	if (zstr(cmd)) {
		cmd = " ";
	}

	if ((p = strstr(cmd, "c="))) {
		p += 2;
		c = atoi(p);
		if ((p = strchr(p, ';'))) {
			cmd = p + 1;
			cursor = cmd + c;
		}
	}

	switch_console_complete(cmd, cursor, NULL, stream, NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(console_complete_xml_function)
{
	const char *p, *cursor = NULL;
	int c;
	switch_xml_t xml = switch_xml_new("complete");
	char *sxml;

	if (zstr(cmd)) {
		cmd = " ";
	}

	if ((p = strstr(cmd, "c="))) {
		p += 2;
		c = atoi(p);
		if ((p = strchr(p, ';'))) {
			cmd = p + 1;
			cursor = cmd + c;
		}
	}

	switch_console_complete(cmd, cursor, NULL, NULL, xml);

	sxml = switch_xml_toxml(xml, SWITCH_TRUE);
	stream->write_function(stream, "%s", sxml);
	free(sxml);

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(eval_function)
{
	char *expanded;
	switch_event_t *event;
	char uuid[80] = "";
	const char *p, *input = cmd;

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strncasecmp(cmd, "uuid:", 5)) {
		p = cmd + 5;
		if ((input = strchr(p, ' ')) && *input++) {
			switch_copy_string(uuid, p, input - p);
		}
	}

	if (zstr(input)) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}


	switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA);
	if (*uuid) {
		if ((session = switch_core_session_locate(uuid))) {
			switch_channel_event_set_data(switch_core_session_get_channel(session), event);
			switch_core_session_rwunlock(session);
		}
	}

	expanded = switch_event_expand_headers(event, input);

	stream->write_function(stream, "%s", expanded);

	if (expanded != input) {
		free(expanded);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(module_exists_function)
{
	if (!zstr(cmd)) {
		if (switch_loadable_module_exists(cmd) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "true");
		} else {
			stream->write_function(stream, "false");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(domain_exists_function)
{
	switch_xml_t root = NULL, domain = NULL;

	if (!zstr(cmd)) {
		if (switch_xml_locate_domain(cmd, NULL, &root, &domain) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "true");
			switch_xml_free(root);
		} else {
			stream->write_function(stream, "false");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(url_encode_function)
{
	char *reply = "";
	char *data = NULL;
	int len = 0;

	if (!zstr(cmd)) {
		len = (int)(strlen(cmd) * 3) + 1;
		switch_zmalloc(data, len);
		switch_url_encode(cmd, data, len);
		reply = data;
	}

	stream->write_function(stream, "%s", reply);

	switch_safe_free(data);
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_STANDARD_API(user_exists_function)
{
	return _find_user(cmd, session, stream, SWITCH_TRUE);
}

SWITCH_STANDARD_API(find_user_function)
{
	return _find_user(cmd, session, stream, SWITCH_FALSE);
}

SWITCH_STANDARD_API(xml_locate_function)
{
	switch_xml_t xml = NULL, obj = NULL;
	int argc;
	char *mydata = NULL, *argv[4];
	char *section, *tag, *tag_attr_name, *tag_attr_val;
	switch_event_t *params = NULL;
	char *xmlstr;
	char *path_info, delim = ' ';
	char *host = NULL;
	const char *err = NULL;

	if (stream->param_event && (host = switch_event_get_header(stream->param_event, "http-host"))) {
		stream->write_function(stream, "Content-Type: text/xml\r\n\r\n");
		if ((path_info = switch_event_get_header(stream->param_event, "http-path-info"))) {
			cmd = path_info;
			delim = '/';
		}
	}

	if (!cmd) {
		err = "bad args";
		goto end;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, delim, argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc == 1 && !strcasecmp(argv[0], "root")) {
		const char *error;
		xml = switch_xml_open_root(0, &error);
		obj = xml;
		goto end;
	}

	if (argc != 1 && argc != 4) {
		err = "bad args";
		goto end;
	}

	section = argv[0];
	tag = argv[1];
	tag_attr_name = argv[2];
	tag_attr_val = argv[3];

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "section", section);

	if (tag) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "tag", tag);
	}

	if (tag_attr_name) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "tag_attr_name", tag_attr_name);
	}

	if (tag_attr_val) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "tag_attr_val", tag_attr_val);
	}

	if (switch_xml_locate(section, tag, tag_attr_name, tag_attr_val, &xml, &obj, params, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "can't find anything\n");
		goto end;
	}

  end:
	if (err) {
		if (host) {
			stream->write_function(stream, "<error>%s</error>\n", err);
		} else {
			stream->write_function(stream, "-Error %s\n", err);
		}
	}

	if (obj) {
		xmlstr = switch_xml_toxml(obj, SWITCH_FALSE);
		switch_assert(xmlstr);
		stream->write_function(stream, "%s", xmlstr);
		free(xmlstr);
	}

	switch_xml_free(xml);
	switch_event_destroy(&params);
	free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reload_acl_function)
{
	const char *err;

	if (cmd && !strcasecmp(cmd, "reloadxml")) {
		stream->write_function(stream, "This option is deprecated, we now always reloadxml.\n");
	}
	
	if (switch_xml_reload(&err) == SWITCH_STATUS_SUCCESS) {
		switch_load_network_lists(SWITCH_TRUE);
		stream->write_function(stream, "+OK acl reloaded\n");
	} else {
		stream->write_function(stream, "-Error [%s]\n", err);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(acl_function)
{
	int argc;
	char *mydata = NULL, *argv[3];

	if (!cmd) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		goto error;
	}

	if (switch_check_network_list_ip(argv[0], argv[1])) {
		stream->write_function(stream, "true");
		goto ok;
	}

  error:

	stream->write_function(stream, "false");

  ok:

	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(replace_function)
{
	char delim = '|';
	char *mydata = NULL, *argv[3], *d, *replace;
	int argc = 0;

	if (!cmd) {
        goto error;
    }

	mydata = strdup(cmd);
	d = mydata;

	if (*d == 'm' && *(d + 1) == ':' && *(d + 2)) {
		char t = *(d + 2);

		switch (t) {
		case '|':
		case '~':
		case '/':
			d += 3;
			delim = t;
			break;
		default:
			break;
		}
	}

	argc = switch_separate_string(d, delim, argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 3) {
		goto error;
	}

	replace = switch_string_replace(argv[0], argv[1], argv[2]);
	stream->write_function(stream, "%s", replace);
	free(replace);

	goto ok;


  error:
	stream->write_function(stream, "-ERR");
  ok:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
	

}

SWITCH_STANDARD_API(regex_function)
{
	switch_regex_t *re = NULL;
	int ovector[30];
	int argc;
	char *mydata = NULL, *argv[3];
	size_t len = 0;
	char *substituted = NULL;
	int proceed = 0;
	char *d;
	char delim = '|';

	if (!cmd) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	d = mydata;

	if (*d == 'm' && *(d + 1) == ':' && *(d + 2)) {
		char t = *(d + 2);

		switch (t) {
		case '|':
		case '~':
		case '/':
			d += 3;
			delim = t;
			break;
		default:
			break;
		}
	}


	argc = switch_separate_string(d, delim, argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		goto error;
	}

	proceed = switch_regex_perform(argv[0], argv[1], &re, ovector, sizeof(ovector) / sizeof(ovector[0]));

	if (argc > 2) {
		if (proceed) {
			len = (strlen(argv[0]) + strlen(argv[2]) + 10) * proceed;
			substituted = malloc(len);
			switch_assert(substituted);
			memset(substituted, 0, len);
			switch_replace_char(argv[2], '%', '$', SWITCH_FALSE);
			switch_perform_substitution(re, proceed, argv[2], argv[0], substituted, len, ovector);

			stream->write_function(stream, "%s", substituted);
			free(substituted);
		} else {
			stream->write_function(stream, "%s", argv[0]);
		}
	} else {
		stream->write_function(stream, proceed ? "true" : "false");
	}

	goto ok;

  error:
	stream->write_function(stream, "-ERR");
  ok:
	switch_regex_safe_free(re);
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

typedef enum {
	O_NONE,
	O_EQ,
	O_NE,
	O_GT,
	O_GE,
	O_LT,
	O_LE
} o_t;

SWITCH_STANDARD_API(cond_function)
{
	int argc;
	char *mydata = NULL, *argv[3];
	char *expr;
	char *a, *b;
	double a_f = 0.0, b_f = 0.0;
	o_t o = O_NONE;
	int is_true = 0;
	char *p;

	if (!cmd) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	if ((p = strchr(mydata, '?'))) {
		*p = ':';
	} else {
		goto error;
	}

	argc = switch_separate_string(mydata, ':', argv, (sizeof(argv) / sizeof(argv[0])));

	if (! (argc >= 2 && argc <= 3)) {
		goto error;
	}

	a = argv[0];

	if ((expr = strchr(a, '!'))) {
		*expr++ = '\0';
		if (*expr == '=') {
			o = O_NE;
		}
	} else if ((expr = strchr(a, '>'))) {
		if (*(expr + 1) == '=') {
			*expr++ = '\0';
			o = O_GE;
		} else {
			o = O_GT;
		}
	} else if ((expr = strchr(a, '<'))) {
		if (*(expr + 1) == '=') {
			*expr++ = '\0';
			o = O_LE;
		} else {
			o = O_LT;
		}
	} else if ((expr = strchr(a, '='))) {
		*expr++ = '\0';
		if (*expr == '=') {
			o = O_EQ;
		}
	}

	if (o) {
		char *s_a = NULL, *s_b = NULL;
		int a_is_num, b_is_num;
		*expr++ = '\0';
		b = expr;
		s_a = switch_strip_spaces(a, SWITCH_TRUE);
		s_b = switch_strip_spaces(b, SWITCH_TRUE);
		a_is_num = switch_is_number(s_a);
		b_is_num = switch_is_number(s_b);

		a_f = a_is_num ? atof(s_a) : (float) strlen(s_a);
		b_f = b_is_num ? atof(s_b) : (float) strlen(s_b);

		switch (o) {
		case O_EQ:
			if (!a_is_num && !b_is_num) {
				is_true = !strcmp(s_a, s_b);
			} else {
				is_true = a_f == b_f;
			}
			break;
		case O_NE:
			if (!a_is_num && !b_is_num) {
				is_true = strcmp(s_a, s_b);
			} else {
				is_true = a_f != b_f;
			}
			break;
		case O_GT:
			is_true = a_f > b_f;
			break;
		case O_GE:
			is_true = a_f >= b_f;
			break;
		case O_LT:
			is_true = a_f < b_f;
			break;
		case O_LE:
			is_true = a_f <= b_f;
			break;
		default:
			break;
		}
		switch_safe_free(s_a);
		switch_safe_free(s_b);

		if ((argc == 2 && !is_true)) {
			stream->write_function(stream, "");
		} else {
			stream->write_function(stream, "%s", is_true ? argv[1] : argv[2]);
		}
		goto ok;
	}

  error:
	stream->write_function(stream, "-ERR");
  ok:

	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(lan_addr_function)
{
	stream->write_function(stream, "%s", switch_is_lan_addr(cmd) ? "true" : "false");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(status_function)
{
	uint8_t html = 0;
	switch_core_time_duration_t duration = { 0 };
	char *http = NULL;
	int sps = 0, last_sps = 0;
	const char *var;
	switch_size_t cur = 0, max = 0;

	switch_core_measure_time(switch_core_uptime(), &duration);

	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
		if ((var = switch_event_get_header(stream->param_event, "content-type"))) {
			if (!strcasecmp(var, "text/plain")) {
				http = NULL;
			}
		}
	}

	if (http) {
		stream->write_function(stream, "%s", "Content-Type: text/html\n\n");
	}

	if (http || (cmd && strstr(cmd, "html"))) {
		html = 1;
		stream->write_function(stream, "<h1>FreeSWITCH Status</h1>\n<b>");
	}

	stream->write_function(stream,
						   "UP %u year%s, %u day%s, %u hour%s, %u minute%s, %u second%s, %u millisecond%s, %u microsecond%s\n",
						   duration.yr, duration.yr == 1 ? "" : "s", duration.day, duration.day == 1 ? "" : "s",
						   duration.hr, duration.hr == 1 ? "" : "s", duration.min, duration.min == 1 ? "" : "s",
						   duration.sec, duration.sec == 1 ? "" : "s", duration.ms, duration.ms == 1 ? "" : "s", duration.mms,
						   duration.mms == 1 ? "" : "s");

	stream->write_function(stream, "FreeSWITCH is %s\n", switch_core_ready() ? "ready" : "not ready");
	stream->write_function(stream, "%" SWITCH_SIZE_T_FMT " session(s) since startup\n", switch_core_session_id() - 1);
	switch_core_session_ctl(SCSC_LAST_SPS, &last_sps);
	switch_core_session_ctl(SCSC_SPS, &sps);
	stream->write_function(stream, "%d session(s) %d/%d\n", switch_core_session_count(), last_sps, sps);
	stream->write_function(stream, "%d session(s) max\n", switch_core_session_limit(0));
	stream->write_function(stream, "min idle cpu %0.2f/%0.2f\n", switch_core_min_idle_cpu(-1.0), switch_core_idle_cpu());


	if (switch_core_get_stacksizes(&cur, &max) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "Current Stack Size/Max %ldK/%ldK\n", cur / 1024, max / 1024);
	}

	if (html) {
		stream->write_function(stream, "</b>\n");
	}

	if (cmd && strstr(cmd, "refresh=")) {
		char *refresh = strchr(cmd, '=');
		if (refresh) {
			int r;
			refresh++;
			r = atoi(refresh);
			if (r > 0) {
				stream->write_function(stream, "<META HTTP-EQUIV=REFRESH CONTENT=\"%d; URL=/api/status?refresh=%d%s\">\n", r, r, html ? "html=1" : "");
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

#define CTL_SYNTAX "[recover|send_sighup|hupall|pause [inbound|outbound]|resume [inbound|outbound]|shutdown [cancel|elegant|asap|now|restart]|sps|sync_clock|sync_clock_when_idle|reclaim_mem|max_sessions|min_dtmf_duration [num]|max_dtmf_duration [num]|default_dtmf_duration [num]|min_idle_cpu|loglevel [level]|debug_level [level]]"
SWITCH_STANDARD_API(ctl_function)
{
	int argc;
	char *mydata, *argv[5];
	int32_t arg = 0;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", CTL_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((mydata = strdup(cmd))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		if (!strcasecmp(argv[0], "hupall")) {
			arg = 1;
			switch_core_session_ctl(SCSC_HUPALL, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "recover")) {
			int r = switch_core_session_ctl(SCSC_RECOVER, argv[1]);
			if (r < 0){
				stream->write_function(stream, "+OK flushed\n");
			} else {
				stream->write_function(stream, "+OK %d session(s) recovered in total\n", r);
			}
		} else if (!strcasecmp(argv[0], "flush_db_handles")) {
			switch_core_session_ctl(SCSC_FLUSH_DB_HANDLES, NULL);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "pause")) {
			switch_session_ctl_t command = SCSC_PAUSE_ALL;
			arg = 1;
			if (argv[1]) {
				if (!strcasecmp(argv[1], "inbound")) {
					command = SCSC_PAUSE_INBOUND;
				} else if (!strcasecmp(argv[1], "outbound")) {
					command = SCSC_PAUSE_OUTBOUND;
				}
			}
			switch_core_session_ctl(command, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "send_sighup")) {
			arg = 1;
			switch_core_session_ctl(SCSC_SEND_SIGHUP, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "resume")) {
			switch_session_ctl_t command = SCSC_PAUSE_ALL;
			arg = 0;
			if (argv[1]) {
				if (!strcasecmp(argv[1], "inbound")) {
					command = SCSC_PAUSE_INBOUND;
				} else if (!strcasecmp(argv[1], "outbound")) {
					command = SCSC_PAUSE_OUTBOUND;
				}
			}
			switch_core_session_ctl(command, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "calibrate_clock")) {
			switch_core_session_ctl(SCSC_CALIBRATE_CLOCK, NULL);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "crash")) {
			switch_core_session_ctl(SCSC_CRASH, NULL);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "verbose_events")) {
			arg = -1;
			if (argv[1]) {
				arg = switch_true(argv[1]);
			}

			switch_core_session_ctl(SCSC_VERBOSE_EVENTS, &arg);

			stream->write_function(stream, "+OK verbose_events is %s \n", arg ? "on" : "off");
		} else if (!strcasecmp(argv[0], "api_expansion")) {
			arg = -1;
			if (argv[1]) {
				arg = switch_true(argv[1]);
			}

			switch_core_session_ctl(SCSC_API_EXPANSION, &arg);

			stream->write_function(stream, "+OK api_expansion is %s \n", arg ? "on" : "off");
		} else if (!strcasecmp(argv[0], "threaded_system_exec")) {
			arg = -1;
			if (argv[1]) {
				arg = switch_true(argv[1]);
			}

			switch_core_session_ctl(SCSC_THREADED_SYSTEM_EXEC, &arg);

			stream->write_function(stream, "+OK threaded_system_exec is %s \n", arg ? "true" : "false");
			
		} else if (!strcasecmp(argv[0], "save_history")) {
			switch_core_session_ctl(SCSC_SAVE_HISTORY, NULL);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "pause_check")) {
			switch_session_ctl_t command = SCSC_PAUSE_CHECK;
			if (argv[1]) {
				if (!strcasecmp(argv[1], "inbound")) {
					command = SCSC_PAUSE_INBOUND_CHECK;
				} else if (!strcasecmp(argv[1], "outbound")) {
					command = SCSC_PAUSE_OUTBOUND_CHECK;
				}
			}
			switch_core_session_ctl(command, &arg);
			stream->write_function(stream, arg ? "true" : "false");
		} else if (!strcasecmp(argv[0], "ready_check")) {
			switch_core_session_ctl(SCSC_READY_CHECK, &arg);
			stream->write_function(stream, arg ? "true" : "false");
		} else if (!strcasecmp(argv[0], "shutdown_check")) {
			switch_core_session_ctl(SCSC_SHUTDOWN_CHECK, &arg);
			stream->write_function(stream, arg ? "true" : "false");
		} else if (!strcasecmp(argv[0], "shutdown")) {
			switch_session_ctl_t command = SCSC_SHUTDOWN;
			int x = 0;
			arg = 0;
			for (x = 1; x < 5; x++) {
				if (argv[x]) {
					if (!strcasecmp(argv[x], "cancel")) {
						arg = 0;
						command = SCSC_CANCEL_SHUTDOWN;
						break;
					} else if (!strcasecmp(argv[x], "elegant")) {
						command = SCSC_SHUTDOWN_ELEGANT;
					} else if (!strcasecmp(argv[x], "now")) {
						command = SCSC_SHUTDOWN_NOW;
					} else if (!strcasecmp(argv[x], "asap")) {
						command = SCSC_SHUTDOWN_ASAP;
					} else if (!strcasecmp(argv[x], "restart")) {
						arg = 1;
					}
				} else {
					break;
				}
			}
			switch_core_session_ctl(command, &arg);
			stream->write_function(stream, "+OK\n");

		} else if (!strcasecmp(argv[0], "debug_sql")) {
			int x = 0;
			switch_core_session_ctl(SCSC_DEBUG_SQL, &x);
			stream->write_function(stream, "+OK SQL DEBUG [%s]\n", x ? "on" : "off");			

		} else if (!strcasecmp(argv[0], "sql")) {
			if (argv[1]) {
				int x = 0;
				if (!strcasecmp(argv[1], "start")) {
					x = 1;
				}
				switch_core_session_ctl(SCSC_SQL, &x);
				stream->write_function(stream, "+OK\n");			
			}

		} else if (!strcasecmp(argv[0], "reclaim_mem")) {
			switch_core_session_ctl(SCSC_RECLAIM, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "max_sessions")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_MAX_SESSIONS, &arg);
			stream->write_function(stream, "+OK max sessions: %d\n", arg);
		} else if (!strcasecmp(argv[0], "min_idle_cpu")) {
			double d = -1;
			
			if (argc > 1) {
				d = atof(argv[1]);
			}

			switch_core_session_ctl(SCSC_MIN_IDLE_CPU, &d);

			if (d) {
				stream->write_function(stream, "+OK min idle cpu: %0.2f%\n", d);
			} else {
				stream->write_function(stream, "+OK min idle cpu: DISABLED\n", d);
			}

			
		} else if (!strcasecmp(argv[0], "max_dtmf_duration")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_MAX_DTMF_DURATION, &arg);
			stream->write_function(stream, "+OK max dtmf duration: %d\n", arg);
		} else if (!strcasecmp(argv[0], "min_dtmf_duration")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_MIN_DTMF_DURATION, &arg);
			stream->write_function(stream, "+OK min dtmf duration: %d\n", arg);
		} else if (!strcasecmp(argv[0], "default_dtmf_duration")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_DEFAULT_DTMF_DURATION, &arg);
			stream->write_function(stream, "+OK default dtmf duration: %d\n", arg);
		} else if (!strcasecmp(argv[0], "loglevel")) {
			if (argc > 1) {
				if (*argv[1] > 47 && *argv[1] < 58) {
					arg = atoi(argv[1]);
				} else {
					arg = switch_log_str2level(argv[1]);
				}
			} else {
				arg = -1;
			}

			if (arg == SWITCH_LOG_INVALID) {
				stream->write_function(stream, "-ERR syntax error, log level not set!\n");
			} else {
				switch_core_session_ctl(SCSC_LOGLEVEL, &arg);
				stream->write_function(stream, "+OK log level: %s [%d]\n", switch_log_level2str(arg), arg);
			}
		} else if (!strcasecmp(argv[0], "debug_level")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			} else {
				arg = -1;
			}

			switch_core_session_ctl(SCSC_DEBUG_LEVEL, &arg);
			stream->write_function(stream, "+OK DEBUG level: %d\n", arg);

		} else if (!strcasecmp(argv[0], "last_sps")) {
			switch_core_session_ctl(SCSC_LAST_SPS, &arg);
			stream->write_function(stream, "+OK last sessions per second: %d\n", arg);
		} else if (!strcasecmp(argv[0], "sps")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			} else {
				arg = 0;
			}
			switch_core_session_ctl(SCSC_SPS, &arg);
			stream->write_function(stream, "+OK sessions per second: %d\n", arg);
		} else if (!strcasecmp(argv[0], "sync_clock")) {
			arg = 0;
			switch_core_session_ctl(SCSC_SYNC_CLOCK, &arg);
			stream->write_function(stream, "+OK clock synchronized\n");
		} else if (!strcasecmp(argv[0], "sync_clock_when_idle")) {
			arg = 0;
			switch_core_session_ctl(SCSC_SYNC_CLOCK_WHEN_IDLE, &arg);
			if (arg) {
				stream->write_function(stream, "+OK clock synchronized\n");
			} else {
				stream->write_function(stream, "+OK clock will synchronize when there are no more calls\n");
			}
		} else {
			stream->write_function(stream, "-ERR INVALID COMMAND\nUSAGE: fsctl %s", CTL_SYNTAX);
			goto end;
		}

	  end:
		free(mydata);
	} else {
		stream->write_function(stream, "-ERR Memory Error\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define LOAD_SYNTAX "<mod_name>"
SWITCH_STANDARD_API(load_function)
{
	const char *err;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", LOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(reload_mutex);

	if (switch_xml_reload(&err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Reloading XML\n");
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR [%s]\n", err);
	}

	switch_mutex_unlock(reload_mutex);

	return SWITCH_STATUS_SUCCESS;
}

#define UNLOAD_SYNTAX "[-f] <mod_name>"
SWITCH_STANDARD_API(unload_function)
{
	const char *err;
	switch_bool_t force = SWITCH_FALSE;
	const char *p = cmd;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", UNLOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}


	if (*p == '-') {
		p++;
		while (p && *p) {
			switch (*p) {
			case ' ':
				cmd = p + 1;
				goto end;
			case 'f':
				force = SWITCH_TRUE;
				break;
			default:
				break;
			}
			p++;
		}
	}
  end:

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", UNLOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(reload_mutex);
	
	if (switch_loadable_module_unload_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, force, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR [%s]\n", err);
	}

	switch_mutex_unlock(reload_mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reload_function)
{
	const char *err;
	switch_bool_t force = SWITCH_FALSE;
	const char *p = cmd;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", UNLOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (*p == '-') {
		p++;
		while (p && *p) {
			switch (*p) {
			case ' ':
				cmd = p + 1;
				goto end;
			case 'f':
				force = SWITCH_TRUE;
				break;
			default:
				break;
			}
			p++;
		}
	}
  end:

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", UNLOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(reload_mutex);

	if (switch_loadable_module_unload_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, force, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK module unloaded\n");
	} else {
		stream->write_function(stream, "-ERR unloading module [%s]\n", err);
	}

	if (switch_xml_reload(&err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Reloading XML\n");
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK module loaded\n");
	} else {
		stream->write_function(stream, "-ERR loading module [%s]\n", err);
	}

	switch_mutex_unlock(reload_mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reload_xml_function)
{
	const char *err = "";

	switch_xml_reload(&err);
	stream->write_function(stream, "+OK [%s]\n", err);

	return SWITCH_STATUS_SUCCESS;
}

#define KILL_SYNTAX "<uuid> [cause]"
SWITCH_STANDARD_API(kill_function)
{
	char *mycmd = NULL, *kcause = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-USAGE: %s\n", KILL_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((kcause = strchr(mycmd, ' '))) {
		*kcause++ = '\0';
		if (!zstr(kcause)) {
			cause = switch_channel_str2cause(kcause);
		}		
	}

	if (switch_ivr_kill_uuid(mycmd, cause) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
	} else {
		stream->write_function(stream, "+OK\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define OUTGOING_ANSWER_SYNTAX "<uuid>"
SWITCH_STANDARD_API(outgoing_answer_function)
{
	switch_core_session_t *outgoing_session = NULL;
	char *mycmd = NULL;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-USAGE: %s\n", OUTGOING_ANSWER_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (zstr(mycmd) || !(outgoing_session = switch_core_session_locate(mycmd))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(outgoing_session);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_mark_answered(channel);
			stream->write_function(stream, "+OK\n");
		} else {
			stream->write_function(stream, "-ERR Not an outbound channel!\n");
		}
		switch_core_session_rwunlock(outgoing_session);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define PREPROCESS_SYNTAX "<>"
SWITCH_STANDARD_API(preprocess_function)
{
	switch_core_session_t *ksession = NULL;
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		goto usage;
	}

	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		goto usage;
	}

	if (!(ksession = switch_core_session_locate(argv[0]))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
		goto done;
	} else {
		switch_ivr_preprocess_session(ksession, (char *) argv[1]);
		switch_core_session_rwunlock(ksession);
		stream->write_function(stream, "+OK\n");
		goto done;
	}

  usage:
	stream->write_function(stream, "-USAGE: %s\n", PREPROCESS_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define PARK_SYNTAX "<uuid>"
SWITCH_STANDARD_API(park_function)
{
	switch_core_session_t *ksession = NULL;

	if (!cmd) {
		stream->write_function(stream, "-USAGE: %s\n", PARK_SYNTAX);
	} else if ((ksession = switch_core_session_locate(cmd))) {
		switch_ivr_park_session(ksession);
		switch_core_session_rwunlock(ksession);
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR No Such Channel!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define TRANSFER_SYNTAX "<uuid> [-bleg|-both] <dest-exten> [<dialplan>] [<context>]"
SWITCH_STANDARD_API(transfer_function)
{
	switch_core_session_t *tsession = NULL, *other_session = NULL;
	char *mycmd = NULL, *argv[5] = { 0 };
	int argc = 0;
	char *tuuid, *dest, *dp, *context, *arg = NULL;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-USAGE: %s\n", TRANSFER_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	if (argc < 2 || argc > 5) {
		stream->write_function(stream, "-USAGE: %s\n", TRANSFER_SYNTAX);
		goto done;
	}

	tuuid = argv[0];
	dest = argv[1];
	dp = argv[2];
	context = argv[3];

	if (zstr(tuuid) || !(tsession = switch_core_session_locate(tuuid))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
		goto done;
	}

	if (*dest == '-') {
		arg = dest;
		dest = argv[2];
		dp = argv[3];
		context = argv[4];
	}

	if (arg) {
		switch_channel_t *channel = switch_core_session_get_channel(tsession);
		const char *uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE);
		arg++;
		if (!strcasecmp(arg, "bleg")) {
			if (uuid && (other_session = switch_core_session_locate(uuid))) {
				switch_core_session_t *tmp = tsession;
				tsession = other_session;
				other_session = NULL;
				switch_core_session_rwunlock(tmp);
			}
		} else if (!strcasecmp(arg, "both")) {
			if (uuid && (other_session = switch_core_session_locate(uuid))) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
				switch_channel_set_flag(other_channel, CF_REDIRECT);
				switch_channel_set_flag(channel, CF_REDIRECT);
				switch_ivr_session_transfer(other_session, dest, dp, context);
				switch_core_session_rwunlock(other_session);
			}
		}
	}

	if (switch_ivr_session_transfer(tsession, dest, dp, context) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR\n");
	}

	switch_core_session_rwunlock(tsession);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


#define DUAL_TRANSFER_SYNTAX "<uuid> <A-dest-exten>[/<A-dialplan>][/<A-context>] <B-dest-exten>[/<B-dialplan>][/<B-context>]"
SWITCH_STANDARD_API(dual_transfer_function)
{
	switch_core_session_t *tsession = NULL, *other_session = NULL;
	char *mycmd = NULL, *argv[5] = { 0 };
	int argc = 0;
	char *tuuid, *dest1, *dest2, *dp1 = NULL, *dp2 = NULL, *context1 = NULL, *context2 = NULL;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-USAGE: %s\n", DUAL_TRANSFER_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc != 3) {
		stream->write_function(stream, "-USAGE: %s\n", DUAL_TRANSFER_SYNTAX);
		goto done;
	}

	tuuid = argv[0];
	dest1 = argv[1];
	dest2= argv[2];

	if ((dp1 = strstr(dest1, "/inline")) && *(dp1 + 7) == '\0') {
		*dp1++ = '\0';
	} else {
		if ((dp1 = strchr(dest1, '/'))) {
			*dp1++ = '\0';
			if ((context1 = strchr(dp1, '/'))) {
				*context1++ = '\0';
			}
		}
	}

	if ((dp2 = strstr(dest2, "/inline")) && *(dp2 + 7) == '\0') {
		*dp2++ = '\0';
	} else {
		if ((dp2 = strchr(dest2, '/'))) {
			*dp2++ = '\0';
			if ((context2 = strchr(dp2, '/'))) {
				*context2++ = '\0';
			}
		}
	}

	if (zstr(tuuid) || !(tsession = switch_core_session_locate(tuuid))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
		goto done;
	}
	
	if (switch_core_session_get_partner(tsession, &other_session) == SWITCH_STATUS_SUCCESS) {
		switch_ivr_session_transfer(other_session, dest2, dp2, context2);
		switch_core_session_rwunlock(other_session);
	}

	switch_ivr_session_transfer(tsession, dest1, dp1, context1);

	stream->write_function(stream, "+OK\n");

	switch_core_session_rwunlock(tsession);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define TONE_DETECT_SYNTAX "<uuid> <key> <tone_spec> [<flags> <timeout> <app> <args> <hits>]"
SWITCH_STANDARD_API(tone_detect_session_function)
{
	char *argv[8] = { 0 };
	int argc;
	char *mydata = NULL;
	time_t to = 0;
	switch_core_session_t *rsession;
	int hits = 1;

	if (!cmd) {
		stream->write_function(stream, "-USAGE: %s\n", TONE_DETECT_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	mydata = strdup(cmd);
	switch_assert(mydata != NULL);

	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "-ERR INVALID ARGS!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(rsession = switch_core_session_locate(argv[0]))) {
		stream->write_function(stream, "-ERR Error Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (argv[4]) {
		uint32_t mto;
		if (*argv[4] == '+') {
			if ((mto = atoi(argv[4] + 1)) > 0) {
				to = switch_epoch_time_now(NULL) + mto;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				goto done;
			}
		} else {
			if ((to = atoi(argv[4])) < switch_epoch_time_now(NULL)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				to = 0;
				goto done;
			}
		}
	}

	if (argv[7]) {
		hits = atoi(argv[7]);
		if (hits < 0) {
			hits = 1;
		}
	}

	switch_ivr_tone_detect_session(rsession, argv[1], argv[2], argv[3], to, hits, argv[5], argv[6], NULL);
	stream->write_function(stream, "+OK Enabling tone detection '%s' '%s' '%s'\n", argv[1], argv[2], argv[3]);

  done:

	free(mydata);
	switch_core_session_rwunlock(rsession);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_function)
{
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	switch_uuid_str(uuid_str, sizeof(uuid_str));
	
	stream->write_function(stream, "%s", uuid_str);	
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_CHAT_SYNTAX "<uuid> <text>"
SWITCH_STANDARD_API(uuid_chat)
{
	switch_core_session_t *tsession = NULL;
	char *uuid = NULL, *text = NULL;

	if (!zstr(cmd) && (uuid = strdup(cmd))) {
		if ((text = strchr(uuid, ' '))) {
			*text++ = '\0';
		}
	}

	if (zstr(uuid) || zstr(text)) {
		stream->write_function(stream, "-USAGE: %s\n", UUID_CHAT_SYNTAX);
	} else {
		if ((tsession = switch_core_session_locate(uuid))) {
			switch_event_t *event;
			if (switch_event_create(&event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_body(event, "%s", text);
				if (switch_core_session_receive_event(tsession, &event) != SWITCH_STATUS_SUCCESS) {
					switch_event_destroy(&event);
					stream->write_function(stream, "-ERR Send Failed\n");
				} else {
					stream->write_function(stream, "+OK\n");
				}
			}
			switch_core_session_rwunlock(tsession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel %s!\n", uuid);
		}
	}

	switch_safe_free(uuid);
	return SWITCH_STATUS_SUCCESS;
}



#define UUID_DEFLECT_SYNTAX "<uuid> <uri>"
SWITCH_STANDARD_API(uuid_deflect)
{
	switch_core_session_t *tsession = NULL;
	char *uuid = NULL, *text = NULL;

	if (!zstr(cmd) && (uuid = strdup(cmd))) {
		if ((text = strchr(uuid, ' '))) {
			*text++ = '\0';
		}
	}

	if (zstr(uuid) || zstr(text)) {
		stream->write_function(stream, "-USAGE: %s\n", UUID_DEFLECT_SYNTAX);
	} else {
		if ((tsession = switch_core_session_locate(uuid))) {
			switch_core_session_message_t msg = { 0 };

			/* Tell the channel to deflect the call */
			msg.from = __FILE__;
			msg.string_arg = text;
			msg.message_id = SWITCH_MESSAGE_INDICATE_DEFLECT;
			switch_core_session_receive_message(tsession, &msg);
			stream->write_function(stream, "+OK:%s\n", msg.string_reply);
			switch_core_session_rwunlock(tsession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel %s!\n", uuid);
		}
	}

	switch_safe_free(uuid);
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_RECOVERY_REFRESH_SYNTAX "<uuid> <uri>"
SWITCH_STANDARD_API(uuid_recovery_refresh)
{
	switch_core_session_t *tsession = NULL;
	char *uuid = NULL, *text = NULL;

	if (!zstr(cmd) && (uuid = strdup(cmd))) {
		if ((text = strchr(uuid, ' '))) {
			*text++ = '\0';
		}
	}

	if (zstr(uuid) || zstr(text)) {
		stream->write_function(stream, "-USAGE: %s\n", UUID_RECOVERY_REFRESH_SYNTAX);
	} else {
		if ((tsession = switch_core_session_locate(uuid))) {
			switch_core_session_message_t msg = { 0 };

			/* Tell the channel to recovery_refresh the call */
			msg.from = __FILE__;
			msg.string_arg = text;
			msg.message_id = SWITCH_MESSAGE_INDICATE_RECOVERY_REFRESH;
			switch_core_session_receive_message(tsession, &msg);
			stream->write_function(stream, "+OK:%s\n", msg.string_reply);
			switch_core_session_rwunlock(tsession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel %s!\n", uuid);
		}
	}

	switch_safe_free(uuid);
	return SWITCH_STATUS_SUCCESS;
}

#define SCHED_TRANSFER_SYNTAX "[+]<time> <uuid> <extension> [<dialplan>] [<context>]"
SWITCH_STANDARD_API(sched_transfer_function)
{
	switch_core_session_t *tsession = NULL;
	char *mycmd = NULL, *argv[6] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 2 || argc > 5 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_TRANSFER_SYNTAX);
	} else {
		char *uuid = argv[1];
		char *dest = argv[2];
		char *dp = argv[3];
		char *context = argv[4];
		time_t when;

		if (*argv[0] == '+') {
			when = switch_epoch_time_now(NULL) + atol(argv[0] + 1);
		} else {
			when = atol(argv[0]);
		}

		if ((tsession = switch_core_session_locate(uuid))) {
			switch_ivr_schedule_transfer(when, uuid, dest, dp, context);
			stream->write_function(stream, "+OK\n");
			switch_core_session_rwunlock(tsession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel!\n");
		}
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define SCHED_HANGUP_SYNTAX "[+]<time> <uuid> [<cause>]"
SWITCH_STANDARD_API(sched_hangup_function)
{
	switch_core_session_t *hsession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 1 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_HANGUP_SYNTAX);
	} else {
		char *uuid = argv[1];
		char *cause_str = argv[2];
		time_t when;
		switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;
		int sec = atol(argv[0] + 1);

		if (*argv[0] == '+') {
			when = switch_epoch_time_now(NULL) + sec;
		} else {
			when = atol(argv[0]);
		}

		if (cause_str) {
			cause = switch_channel_str2cause(cause_str);
		}

		if ((hsession = switch_core_session_locate(uuid))) {
			if (sec == 0) {
				switch_channel_t *hchannel = switch_core_session_get_channel(hsession);
				switch_channel_hangup(hchannel, cause);
			} else {
				switch_ivr_schedule_hangup(when, uuid, cause, SWITCH_FALSE);
			}

			stream->write_function(stream, "+OK\n");
			switch_core_session_rwunlock(hsession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel!\n");
		}
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define MEDIA_SYNTAX "[off] <uuid>"
SWITCH_STANDARD_API(uuid_media_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 1 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", MEDIA_SYNTAX);
	} else {
		if (!strcasecmp(argv[0], "off")) {
			status = switch_ivr_nomedia(argv[1], SMF_REBRIDGE);
		} else {
			status = switch_ivr_media(argv[0], SMF_REBRIDGE);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define MEDIA_RENEG_SYNTAX "<uuid>[ <codec_string>]"
SWITCH_STANDARD_API(uuid_media_neg_function)
{
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 1 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", MEDIA_RENEG_SYNTAX);
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;
		char *uuid = argv[0];

		msg.message_id = SWITCH_MESSAGE_INDICATE_MEDIA_RENEG;
		msg.string_arg = argv[1];
		msg.from = __FILE__;

		if (*uuid == '+') {
			msg.numeric_arg++;
			uuid++;
		}
		
		if ((lsession = switch_core_session_locate(uuid))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_early_ok_function)
{
	char *uuid = (char *) cmd;
	switch_core_session_t *xsession;

	if (uuid && (xsession = switch_core_session_locate(uuid))) {
		switch_channel_t *channel = switch_core_session_get_channel(xsession);
		switch_channel_set_flag(channel, CF_EARLY_OK);
		switch_core_session_rwunlock(xsession);
	} else {
		stream->write_function(stream, "-ERROR\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_pre_answer_function)
{
	char *uuid = (char *) cmd;
	switch_core_session_t *xsession;

	if (uuid && (xsession = switch_core_session_locate(uuid))) {
		switch_channel_t *channel = switch_core_session_get_channel(xsession);
		switch_channel_pre_answer(channel);
		switch_core_session_rwunlock(xsession);
	} else {
		stream->write_function(stream, "-ERROR\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_answer_function)
{
	char *uuid = (char *) cmd;
	switch_core_session_t *xsession;

	if (uuid && (xsession = switch_core_session_locate(uuid))) {
		switch_channel_t *channel = switch_core_session_get_channel(xsession);
		switch_channel_answer(channel);
		switch_core_session_rwunlock(xsession);
	} else {
		stream->write_function(stream, "-ERROR\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


#define BROADCAST_SYNTAX "<uuid> <path> [aleg|bleg|holdb|both]"
SWITCH_STANDARD_API(uuid_broadcast_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 2) {
		stream->write_function(stream, "-USAGE: %s\n", BROADCAST_SYNTAX);
	} else {
		switch_media_flag_t flags = SMF_NONE;

		if (argv[2]) {
			if (switch_stristr("both", (argv[2]))) {
				flags |= (SMF_ECHO_ALEG | SMF_ECHO_BLEG);
			}

			if (switch_stristr("aleg", argv[2])) {
				flags |= SMF_ECHO_ALEG;
			} 

			if (switch_stristr("bleg", argv[2])) {
				flags &= ~SMF_HOLD_BLEG;
				flags |= SMF_ECHO_BLEG;
			}

			if (switch_stristr("holdb", argv[2])) {
				flags &= ~SMF_ECHO_BLEG;
				flags |= SMF_HOLD_BLEG;
			}

		} else {
			flags = SMF_ECHO_ALEG | SMF_HOLD_BLEG;
		}

		switch_ivr_broadcast(argv[0], argv[1], flags);
		stream->write_function(stream, "+OK Message Sent\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define SCHED_BROADCAST_SYNTAX "[+]<time> <uuid> <path> [aleg|bleg|both]"
SWITCH_STANDARD_API(sched_broadcast_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 3 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_BROADCAST_SYNTAX);
	} else {
		switch_media_flag_t flags = SMF_NONE;
		time_t when;

		if (*argv[0] == '+') {
			when = switch_epoch_time_now(NULL) + atol(argv[0] + 1);
		} else {
			when = atol(argv[0]);
		}

		if (argv[3]) {
			if (!strcasecmp(argv[3], "both")) {
				flags |= (SMF_ECHO_ALEG | SMF_ECHO_BLEG);
			} else if (!strcasecmp(argv[3], "aleg")) {
				flags |= SMF_ECHO_ALEG;
			} else if (!strcasecmp(argv[3], "bleg")) {
				flags |= SMF_ECHO_BLEG;
			}
		} else {
			flags |= SMF_ECHO_ALEG;
		}

		switch_ivr_schedule_broadcast(when, argv[1], argv[2], flags);
		stream->write_function(stream, "+OK Message Scheduled\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define HOLD_SYNTAX "[off] <uuid> [<display>]"
SWITCH_STANDARD_API(uuid_hold_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 1 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", HOLD_SYNTAX);
	} else {
		if (!strcasecmp(argv[0], "off")) {
			status = switch_ivr_unhold_uuid(argv[1]);
		} else {
			status = switch_ivr_hold_uuid(argv[0], argv[1], 1);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define DISPLAY_SYNTAX "<uuid> <display>"
SWITCH_STANDARD_API(uuid_display_function)
{
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 2 || zstr(argv[0]) || zstr(argv[1])) {
		stream->write_function(stream, "-USAGE: %s\n", DISPLAY_SYNTAX);
		goto end;
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
		msg.string_arg = argv[1];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

 end:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define BUGLIST_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_buglist_function)
{
	char *mydata = NULL, *argv[2] = { 0 };
	int argc = 0;

	if (zstr(cmd)) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 1) {
		goto error;
	}
	if (argv[0]) {
		switch_core_session_t *lsession = NULL;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			switch_core_media_bug_enumerate(lsession, stream);
			switch_core_session_rwunlock(lsession);
		}
		goto ok;
	} else {
		goto error;
	}

  error:
	stream->write_function(stream, "-USAGE: %s\n", BUGLIST_SYNTAX);
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
  ok:
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

#define SIMPLIFY_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_simplify_function)
{
	char *mydata = NULL, *argv[2] = { 0 };
	int argc = 0;

	switch_status_t status = SWITCH_STATUS_FALSE;

	if (zstr(cmd)) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 1) {
		goto error;
	}
	if (argv[0]) {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_SIMPLIFY;
		msg.string_arg = argv[0];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
		goto ok;
	} else {
		goto error;
	}

  error:
	stream->write_function(stream, "-USAGE: %s\n", SIMPLIFY_SYNTAX);
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
  ok:
	switch_safe_free(mydata);

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define JITTERBUFFER_SYNTAX "<uuid> [0|<min_msec>[:<max_msec>]]"
SWITCH_STANDARD_API(uuid_jitterbuffer_function)
{
	char *mydata = NULL, *argv[2] = { 0 };
	int argc = 0;

	switch_status_t status = SWITCH_STATUS_FALSE;

	if (zstr(cmd)) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		goto error;
	}
	if (argv[1]) {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_JITTER_BUFFER;
		msg.string_arg = argv[1];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
		goto ok;
	} else {
		goto error;
	}

  error:
	stream->write_function(stream, "-USAGE: %s\n", JITTERBUFFER_SYNTAX);
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
  ok:
	switch_safe_free(mydata);

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


#define PHONE_EVENT_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_phone_event_function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		stream->write_function(stream, "-USAGE: %s\n", PHONE_EVENT_SYNTAX);
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_PHONE_EVENT;
		msg.string_arg = argv[1];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

#define SEND_MESSAGE_SYNTAX "<uuid> <message>"
SWITCH_STANDARD_API(uuid_send_message_function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 2) {
		stream->write_function(stream, "-USAGE: %s\n", SEND_MESSAGE_SYNTAX);
		goto end;
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_MESSAGE;
		msg.string_array_arg[2] = argv[1];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		} else {
			stream->write_function(stream, "-ERR Unable to find session for UUID\n");
			goto end;
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

 end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

#define INFO_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_send_info_function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		stream->write_function(stream, "-USAGE: %s\n", INFO_SYNTAX);
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_INFO;
		msg.string_array_arg[2] = argv[1];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}


#define VIDEO_REFRESH_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_video_refresh_function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		stream->write_function(stream, "-USAGE: %s\n", VIDEO_REFRESH_SYNTAX);
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;
		msg.string_array_arg[2] = argv[1];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}


#define DEBUG_AUDIO_SYNTAX "<uuid> <read|write|both> <on|off>"
SWITCH_STANDARD_API(uuid_debug_audio_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 3 || zstr(argv[0]) || zstr(argv[1]) || zstr(argv[2])) {
		stream->write_function(stream, "-USAGE: %s\n", DEBUG_AUDIO_SYNTAX);
		goto done;
	} else {
		switch_core_session_message_t msg = { 0 };
		switch_core_session_t *lsession = NULL;

		msg.message_id = SWITCH_MESSAGE_INDICATE_DEBUG_AUDIO;
		msg.string_array_arg[0] = argv[1];
		msg.string_array_arg[1] = argv[2];
		msg.from = __FILE__;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			status = switch_core_session_receive_message(lsession, &msg);
			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_SYNTAX "<uuid> <other_uuid>"
SWITCH_STANDARD_API(uuid_bridge_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 2) {
		stream->write_function(stream, "-USAGE: %s\n", UUID_SYNTAX);
	} else {
		switch_status_t status;
		char *who = NULL;

		if ((status = switch_ivr_uuid_bridge(argv[0], argv[1])) != SWITCH_STATUS_SUCCESS) {
			if (argv[2]) {
				if ((status = switch_ivr_uuid_bridge(argv[0], argv[2])) == SWITCH_STATUS_SUCCESS) {
					who = argv[2];
				}
			}
		} else {
			who = argv[1];
		}

		if (status == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "+OK %s\n", who);
		} else {
			stream->write_function(stream, "-ERR Invalid uuid\n");
		}
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define SESS_REC_SYNTAX "<uuid> [start|stop] <path> [<limit>]"
SWITCH_STANDARD_API(session_record_function)
{
	switch_core_session_t *rsession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	char *uuid = NULL, *action = NULL, *path = NULL;
	int argc = 0;
	uint32_t limit = 0;

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 3) {
		goto usage;
	}

	uuid = argv[0];
	action = argv[1];
	path = argv[2];
	limit = argv[3] ? atoi(argv[3]) : 0;

	if (zstr(uuid) || zstr(action) || zstr(path)) {
		goto usage;
	}

	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(action, "start")) {
		if (switch_ivr_record_session(rsession, path, limit, NULL) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "-ERR Cannot record session!\n");
		} else {
			stream->write_function(stream, "+OK Success\n");
		}
	} else if (!strcasecmp(action, "stop")) {
		if (switch_ivr_stop_record_session(rsession, path) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "-ERR Cannot stop record session!\n");
		} else {
			stream->write_function(stream, "+OK Success\n");
		}
	} else {
		goto usage;
	}

	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", SESS_REC_SYNTAX);

  done:
	if (rsession) {
		switch_core_session_rwunlock(rsession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define DISPLACE_SYNTAX "<uuid> [start|stop] <path> [<limit>] [mux]"
SWITCH_STANDARD_API(session_displace_function)
{
	switch_core_session_t *rsession = NULL;
	char *mycmd = NULL, *argv[5] = { 0 };
	char *uuid = NULL, *action = NULL, *path = NULL;
	int argc = 0;
	uint32_t limit = 0;
	char *flags = NULL;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 3) {
		goto usage;
	}

	uuid = argv[0];
	action = argv[1];
	path = argv[2];
	limit = argv[3] ? atoi(argv[3]) : 0;
	flags = argv[4];

	if (zstr(uuid) || zstr(action) || zstr(path)) {
		goto usage;
	}

	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(action, "start")) {
		if (switch_ivr_displace_session(rsession, path, limit, flags) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "-ERR Cannot displace session!\n");
		} else {
			stream->write_function(stream, "+OK Success\n");
		}
	} else if (!strcasecmp(action, "stop")) {
		if (switch_ivr_stop_displace_session(rsession, path) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "-ERR Cannot stop displace session!\n");
		} else {
			stream->write_function(stream, "+OK Success\n");
		}
	} else {
		goto usage;
	}

	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", DISPLACE_SYNTAX);

  done:
	if (rsession) {
		switch_core_session_rwunlock(rsession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


#define AUDIO_SYNTAX "<uuid> [start [read|write] [mute|level <level>]|stop]"
SWITCH_STANDARD_API(session_audio_function)
{
	switch_core_session_t *u_session = NULL;
	char *mycmd = NULL;
	int fail = 0;
	int argc = 0;
	char *argv[5] = { 0 };
	int level;

	if (zstr(cmd)) {
		fail++;
		goto done;
	}

	mycmd = strdup(cmd);
	argc = switch_split(mycmd, ' ', argv);

	if (argc < 2) {
		fail++;
		goto done;
	}

	if (!(u_session = switch_core_session_locate(argv[0]))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
		goto done;
	}

	if (!strcasecmp(argv[1], "stop")) {
		switch_ivr_stop_session_audio(u_session);
		goto done;
	}

	if (strcasecmp(argv[1], "start") || argc < 5 || (strcasecmp(argv[2], "read") && strcasecmp(argv[2], "write"))) {
		fail++;
		goto done;
	}

	level = atoi(argv[4]);

	if (!strcasecmp(argv[3], "mute")) {
		switch_ivr_session_audio(u_session, "mute", argv[2], level);
	} else if (!strcasecmp(argv[3], "level")) {
		switch_ivr_session_audio(u_session, "level", argv[2], level);
	} else {
		fail++;
	}

  done:

	if (u_session) {
		switch_core_session_rwunlock(u_session);
	}

	switch_safe_free(mycmd);

	if (fail) {
		stream->write_function(stream, "-USAGE: %s\n", AUDIO_SYNTAX);
	} else {
		stream->write_function(stream, "+OK\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define BREAK_SYNTAX "<uuid> [all]"
SWITCH_STANDARD_API(break_function)
{
	switch_core_session_t *psession = NULL, *qsession = NULL;
	char *mycmd = NULL, *flag;
	switch_channel_t *channel = NULL, *qchannel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int all = 0;
	int both = 0;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", BREAK_SYNTAX);
		goto done;
	}

	mycmd = strdup(cmd);
	switch_assert(mycmd);

	if ((flag = strchr(mycmd, ' '))) {
		*flag++ = '\0';
	}

	if (!(psession = switch_core_session_locate(mycmd))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
		goto done;
	}

	if (flag) {
		if (strstr(flag, "all")) {
			all++;
		}
		if (strstr(flag, "both")) {
			both++;
		}
	}

	channel = switch_core_session_get_channel(psession);

	if (both) {
		const char *quuid = switch_channel_get_partner_uuid(channel);
		if (quuid && (qsession = switch_core_session_locate(quuid))) {
			qchannel = switch_core_session_get_channel(qsession);
		}
	}

	if (all) {
		switch_core_session_flush_private_events(psession);
		if (qsession) {
			switch_core_session_flush_private_events(qsession);
		}
	}

	

	if (switch_channel_test_flag(channel, CF_BROADCAST)) {
		switch_channel_stop_broadcast(channel);
	} else {
		switch_channel_set_flag_value(channel, CF_BREAK, all ? 2 : 1);
	}

	if (qchannel) {
		if (switch_channel_test_flag(qchannel, CF_BROADCAST)) {
			switch_channel_stop_broadcast(qchannel);
		} else {
			switch_channel_set_flag_value(qchannel, CF_BREAK, all ? 2 : 1);
		}
	}

  done:

	if (psession) {
		switch_core_session_rwunlock(psession);
	}

	if (qsession) {
		switch_core_session_rwunlock(qsession);
	}

	switch_safe_free(mycmd);

	return status;
}

#define PAUSE_SYNTAX "<uuid> <on|off>"
SWITCH_STANDARD_API(pause_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 2 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", PAUSE_SYNTAX);
	} else {
		char *uuid = argv[0];
		char *dest = argv[1];

		if ((psession = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(psession);

			if (!strcasecmp(dest, "on")) {
				switch_channel_set_flag(channel, CF_HOLD);
			} else {
				switch_channel_clear_flag(channel, CF_HOLD);
			}

			switch_core_session_rwunlock(psession);

		} else {
			stream->write_function(stream, "-ERR No Such Channel!\n");
		}
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define ORIGINATE_SYNTAX "<call url> <exten>|&<application_name>(<app_args>) [<dialplan>] [<context>] [<cid_name>] [<cid_num>] [<timeout_sec>]"
SWITCH_STANDARD_API(originate_function)
{
	switch_channel_t *caller_channel;
	switch_core_session_t *caller_session = NULL;
	char *mycmd = NULL, *argv[10] = { 0 };
	int i = 0, x, argc = 0;
	char *aleg, *exten, *dp, *context, *cid_name, *cid_num;
	uint32_t timeout = 60;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	uint8_t machine = 1;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE %s\n", ORIGINATE_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	/* log warning if part of ongoing session, as we'll block the session */
	if (session){
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Originate can take 60 seconds to complete, and blocks the existing session. Do not confuse with a lockup.\n");
	}

	mycmd = strdup(cmd);
	switch_assert(mycmd);
	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2 || argc > 7) {
		stream->write_function(stream, "-USAGE: %s\n", ORIGINATE_SYNTAX);
		goto done;
	}

	for (x = 0; x < argc && argv[x]; x++) {
		if (!strcasecmp(argv[x], "undef")) {
			argv[x] = NULL;
		}
	}

	if (argv[0] && !strcasecmp(argv[0], "machine")) {
		machine = 1;
		i++;
	}

	aleg = argv[i++];
	exten = argv[i++];
	dp = argv[i++];
	context = argv[i++];
	cid_name = argv[i++];
	cid_num = argv[i++];

	if (!dp) {
		dp = "XML";
	}

	if (!context) {
		context = "default";
	}

	if (argv[6]) {
		timeout = atoi(argv[6]);
	}

	if (switch_ivr_originate(NULL, &caller_session, &cause, aleg, timeout, NULL, cid_name, cid_num, NULL, NULL, SOF_NONE, NULL) != SWITCH_STATUS_SUCCESS
		|| !caller_session) {
		if (machine) {
			stream->write_function(stream, "-ERR %s\n", switch_channel_cause2str(cause));
		} else {
			stream->write_function(stream, "-ERR Cannot Create Outgoing Channel! [%s] cause: %s\n", aleg, switch_channel_cause2str(cause));
		}
		goto done;
	}

	caller_channel = switch_core_session_get_channel(caller_session);

	if (*exten == '&' && *(exten + 1)) {
		switch_caller_extension_t *extension = NULL;
		char *app_name = switch_core_session_strdup(caller_session, (exten + 1));
		char *arg = NULL, *e;

		if ((e = strchr(app_name, ')'))) {
			*e = '\0';
		}

		if ((arg = strchr(app_name, '('))) {
			*arg++ = '\0';
		}

		if ((extension = switch_caller_extension_new(caller_session, app_name, arg)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
			abort();
		}
		switch_caller_extension_add_application(caller_session, extension, app_name, arg);
		switch_channel_set_caller_extension(caller_channel, extension);
		switch_channel_set_state(caller_channel, CS_EXECUTE);
	} else {
		switch_ivr_session_transfer(caller_session, exten, dp, context);
	}

	if (machine) {
		stream->write_function(stream, "+OK %s\n", switch_core_session_get_uuid(caller_session));
	} else {
		stream->write_function(stream, "+OK Created Session: %s\n", switch_core_session_get_uuid(caller_session));
	}

	switch_core_session_rwunlock(caller_session);

  done:
	switch_safe_free(mycmd);
	return status;
}

SWITCH_STANDARD_API(sched_del_function)
{
	uint32_t cnt = 0;

	if (!cmd) {
		stream->write_function(stream, "-ERR Invalid syntax\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_is_digit_string(cmd)) {
		int64_t tmp;
		tmp = (uint32_t) atoi(cmd);
		if (tmp > 0) {
			cnt = switch_scheduler_del_task_id((uint32_t) tmp);
		}
	} else {
		cnt = switch_scheduler_del_task_group(cmd);
	}

	stream->write_function(stream, "+OK Deleted: %u\n", cnt);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(xml_wrap_api_function)
{
	char *dcommand, *edata = NULL, *send = NULL, *command, *arg = NULL;
	switch_stream_handle_t mystream = { 0 };
	int encoded = 0, elen = 0;

	if (!cmd) {
		stream->write_function(stream, "-ERR Invalid syntax\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if ((dcommand = strdup(cmd))) {
		if (!strncasecmp(dcommand, "encoded ", 8)) {
			encoded++;
			command = dcommand + 8;
		} else {
			command = dcommand;
		}

		if ((arg = strchr(command, ' '))) {
			*arg++ = '\0';
		}
		SWITCH_STANDARD_STREAM(mystream);
		switch_api_execute(command, arg, NULL, &mystream);

		if (mystream.data) {
			if (encoded) {
				elen = (int) strlen(mystream.data) * 3 + 1;
				edata = malloc(elen);
				switch_assert(edata != NULL);
				memset(edata, 0, elen);
				switch_url_encode(mystream.data, edata, elen);
				send = edata;
			} else {
				send = mystream.data;
			}
		}

		stream->write_function(stream, "<result>\n" "  <row id=\"1\">\n" "    <data>%s</data>\n" "  </row>\n" "</result>\n", send ? send : "ERROR");
		switch_safe_free(mystream.data);
		switch_safe_free(edata);
		free(dcommand);
	}

	return SWITCH_STATUS_SUCCESS;
}

struct api_task {
	uint32_t recur;
	char cmd[];
};

static void sch_api_callback(switch_scheduler_task_t *task)
{
	char *cmd, *arg = NULL;
	switch_stream_handle_t stream = { 0 };
	struct api_task *api_task = (struct api_task *) task->cmd_arg;
	switch_assert(task);

	cmd = strdup(api_task->cmd);
	switch_assert(cmd);

	if ((arg = strchr(cmd, ' '))) {
		*arg++ = '\0';
	}

	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(cmd, arg, NULL, &stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Command %s(%s):\n%s\n", cmd, switch_str_nil(arg), switch_str_nil((char *) stream.data));
	switch_safe_free(stream.data);
	switch_safe_free(cmd);

	if (api_task->recur) {
		task->runtime = switch_epoch_time_now(NULL) + api_task->recur;
	}
}

#define UNSCHED_SYNTAX "<task_id>"
SWITCH_STANDARD_API(unsched_api_function)
{
	uint32_t id;

	if (!cmd) {
		stream->write_function(stream, "-ERR Invalid syntax. USAGE: %s\n", UNSCHED_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((id = (uint32_t) atol(cmd))) {
		stream->write_function(stream, "%s\n", switch_scheduler_del_task_id(id) ? "+OK" : "-ERR no such id");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define SCHED_SYNTAX "[+@]<time> <group_name> <command_string>[&]"
SWITCH_STANDARD_API(sched_api_function)
{
	char *tm = NULL, *dcmd, *group;
	time_t when;
	struct api_task *api_task = NULL;
	uint32_t recur = 0;
	int flags = SSHF_FREE_ARG;

	if (!cmd) {
		goto bad;
	}
	tm = strdup(cmd);
	switch_assert(tm != NULL);

	if ((group = strchr(tm, ' '))) {
		uint32_t id;

		*group++ = '\0';

		if ((dcmd = strchr(group, ' '))) {
			*dcmd++ = '\0';

			if (*tm == '+') {
				when = switch_epoch_time_now(NULL) + atol(tm + 1);
			} else if (*tm == '@') {
				recur = (uint32_t) atol(tm + 1);
				when = switch_epoch_time_now(NULL) + recur;
			} else {
				when = atol(tm);
			}

			switch_zmalloc(api_task, sizeof(*api_task) + strlen(dcmd) + 1);
			switch_copy_string(api_task->cmd, dcmd, strlen(dcmd) + 1);
			api_task->recur = recur;
			if (end_of(api_task->cmd) == '&') {
				end_of(api_task->cmd) = '\0';
				flags |= SSHF_OWN_THREAD;
			}


			id = switch_scheduler_add_task(when, sch_api_callback, (char *) __SWITCH_FUNC__, group, 0, api_task, flags);
			stream->write_function(stream, "+OK Added: %u\n", id);
			goto good;
		}
	}

  bad:

	stream->write_function(stream, "-ERR Invalid syntax. USAGE: %s\n", SCHED_SYNTAX);

  good:

	switch_safe_free(tm);
	return SWITCH_STATUS_SUCCESS;
}

static switch_thread_rwlock_t *bgapi_rwlock = NULL;

struct bg_job {
	char *cmd;
	char *arg;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC bgapi_exec(switch_thread_t *thread, void *obj)
{
	struct bg_job *job = (struct bg_job *) obj;
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	char *reply, *freply = NULL;
	switch_event_t *event;
	char *arg;
	switch_memory_pool_t *pool;

	if (!job) {
		return NULL;
	}

	switch_thread_rwlock_rdlock(bgapi_rwlock);

	pool = job->pool;

	SWITCH_STANDARD_STREAM(stream);

	if ((arg = strchr(job->cmd, ' '))) {
		*arg++ = '\0';
	}

	if ((status = switch_api_execute(job->cmd, arg, NULL, &stream)) == SWITCH_STATUS_SUCCESS) {
		reply = stream.data;
	} else {
		freply = switch_mprintf("%s: Command not found!\n", job->cmd);
		reply = freply;
	}

	if (!reply) {
		reply = "Command returned no output!";
	}

	if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", job->uuid_str);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command", job->cmd);
		if (arg) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", arg);
		}

		switch_event_add_body(event, "%s", reply);
		switch_event_fire(&event);
	}

	switch_safe_free(stream.data);
	switch_safe_free(freply);

	job = NULL;
	switch_core_destroy_memory_pool(&pool);
	pool = NULL;

	switch_thread_rwlock_unlock(bgapi_rwlock);

	return NULL;
}

SWITCH_STANDARD_API(bgapi_function)
{
	struct bg_job *job;
	switch_uuid_t uuid;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (!cmd) {
		stream->write_function(stream, "-ERR Invalid syntax\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_core_new_memory_pool(&pool);
	job = switch_core_alloc(pool, sizeof(*job));
	job->cmd = switch_core_strdup(pool, cmd);
	job->pool = pool;

	switch_uuid_get(&uuid);
	switch_uuid_format(job->uuid_str, &uuid);

	switch_threadattr_create(&thd_attr, job->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	stream->write_function(stream, "+OK Job-UUID: %s\n", job->uuid_str);
	switch_thread_create(&thread, thd_attr, bgapi_exec, job, job->pool);

	return SWITCH_STATUS_SUCCESS;
}

struct holder {
	switch_stream_handle_t *stream;
	char *http;
	char *delim;
	uint32_t count;
	int print_title;
	switch_xml_t xml;
	cJSON *json;
	int rows;
	int justcount;
};

static int show_as_json_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	cJSON *row;
	int x;

	if (holder->count == 0) {
		if (!(holder->json = cJSON_CreateArray())) {
			return -1;
		}
	}

	if (holder->justcount) {
		holder->count++;
		return 0;
	}

	if (!(row = cJSON_CreateObject())) {
		return -1;
	}

	cJSON_AddItemToArray(holder->json, row);

	for (x = 0; x < argc; x++) {
		char *name = columnNames[x];
		char *val = switch_str_nil(argv[x]);

		if (!name) {
			name = "undefined";
		}

		cJSON_AddItemToObject(row, name, cJSON_CreateString(val));
	}

	holder->count++;

	return 0;
}

static int show_as_xml_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	switch_xml_t row, field;
	int x, f_off = 0;
	char id[50];

	if (holder->count == 0) {
		if (!(holder->xml = switch_xml_new("result"))) {
			return -1;
		}
	}

	if (holder->justcount) {
		holder->count++;
		return 0;
	}

	if (!(row = switch_xml_add_child_d(holder->xml, "row", holder->rows++))) {
		return -1;
	}

	switch_snprintf(id, sizeof(id), "%d", holder->rows);

	switch_xml_set_attr(switch_xml_set_flag(row, SWITCH_XML_DUP), strdup("row_id"), strdup(id));

	for (x = 0; x < argc; x++) {
		char *name = columnNames[x];
		char *val = switch_str_nil(argv[x]);

		if (!name) {
			name = "undefined";
		}

		if ((field = switch_xml_add_child_d(row, name, f_off++))) {
			switch_xml_set_txt_d(field, val);
		} else {
			return -1;
		}
	}

	holder->count++;

	return 0;
}

static int show_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	int x;

	if (holder->justcount) {
		holder->count++;
		return 0;
	}

	if (holder->print_title && holder->count == 0) {
		if (holder->http) {
			holder->stream->write_function(holder->stream, "\n<tr>");
		}

		for (x = 0; x < argc; x++) {
			char *name = columnNames[x];
			if (!name) {
				name = "undefined";
			}

			if (holder->http) {
				holder->stream->write_function(holder->stream, "<td>");
				holder->stream->write_function(holder->stream, "<b>%s</b>%s", name, x == (argc - 1) ? "</td></tr>\n" : "</td><td>");
			} else {
				holder->stream->write_function(holder->stream, "%s%s", name, x == (argc - 1) ? "\n" : holder->delim);
			}
		}
	}

	if (holder->http) {
		holder->stream->write_function(holder->stream, "<tr bgcolor=%s>", holder->count % 2 == 0 ? "eeeeee" : "ffffff");
	}

	for (x = 0; x < argc; x++) {
		char *val = switch_str_nil(argv[x]);


		if (holder->http) {
			char aval[512];

			switch_amp_encode(val, aval, sizeof(aval));
			holder->stream->write_function(holder->stream, "<td>");
			holder->stream->write_function(holder->stream, "%s%s", aval, x == (argc - 1) ? "</td></tr>\n" : "</td><td>");
		} else {
			holder->stream->write_function(holder->stream, "%s%s", val, x == (argc - 1) ? "\n" : holder->delim);
		}
	}

	holder->count++;
	return 0;
}

#define COMPLETE_SYNTAX "add <word>|del [<word>|*]"
SWITCH_STANDARD_API(complete_function)
{
	switch_status_t status;

	if ((status = switch_console_set_complete(cmd)) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-USAGE: %s\n", COMPLETE_SYNTAX);
	}

	return SWITCH_STATUS_SUCCESS;
}

#define ALIAS_SYNTAX "[add|stickyadd] <alias> <command> | del [<alias>|*]"
SWITCH_STANDARD_API(alias_function)
{
	switch_status_t status;

	if ((status = switch_console_set_alias(cmd)) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-USAGE: %s\n", ALIAS_SYNTAX);
	}

	return SWITCH_STATUS_SUCCESS;
}

#define SHOW_SYNTAX "codec|endpoint|application|api|dialplan|file|timer|calls [count]|channels [count|like <match string>]|calls|detailed_calls|bridged_calls|detailed_bridged_calls|aliases|complete|chat|management|modules|nat_map|say|interfaces|interface_types|tasks|limits"
SWITCH_STANDARD_API(show_function)
{
	char sql[1024];
	char *errmsg;
	switch_cache_db_handle_t *db;
	struct holder holder = { 0 };
	int help = 0;
	char *mydata = NULL, *argv[6] = { 0 };
	char *command = NULL, *as = NULL;
	switch_core_flag_t cflags = switch_core_flags();
	switch_status_t status = SWITCH_STATUS_SUCCESS;
    const char *hostname = switch_core_get_switchname();

	if (!(cflags & SCF_USE_SQL)) {
		stream->write_function(stream, "-ERR SQL DISABLED NO DATA AVAILABLE!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "%s", "-ERR Databse Error!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	holder.justcount = 0;

	if (cmd && (mydata = strdup(cmd))) {
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		command = argv[0];
		if (argv[2] && !strcasecmp(argv[1], "as")) {
			as = argv[2];
		}

	}

	if (stream->param_event) {
		const char *var;
		holder.http = switch_event_get_header(stream->param_event, "http-host");

		if ((var = switch_event_get_header(stream->param_event, "content-type"))) {
			if (!strcasecmp(var, "text/plain")) {
				holder.http = NULL;
			}
		} else if (holder.http) {
			stream->write_function(stream, "%s", "Content-Type: text/html\n\n");
		}

	}

	holder.print_title = 1;

	/* If you change the field qty or order of any of these select */
	/* statements, you must also change show_callback and friends to match! */
	if (!command) {
		stream->write_function(stream, "-USAGE: %s\n", SHOW_SYNTAX);
		goto end;
	} else if (!strncasecmp(command, "codec", 5) ||
			   !strncasecmp(command, "dialplan", 8) ||
			   !strncasecmp(command, "file", 4) ||
			   !strncasecmp(command, "timer", 5) ||
			   !strncasecmp(command, "chat", 4) ||
			   !strncasecmp(command, "limit", 5) ||
			   !strncasecmp(command, "say", 3) || !strncasecmp(command, "management", 10) || !strncasecmp(command, "endpoint", 8)) {
		if (end_of(command) == 's') {
			end_of(command) = '\0';
		}
		sprintf(sql, "select type, name, ikey from interfaces where hostname='%s' and type = '%s' order by type,name", hostname, command);
	} else if (!strncasecmp(command, "module", 6)) {
		if (argv[1]) {
			sprintf(sql, "select distinct type, name, ikey, filename from interfaces where hostname='%s' and ikey = '%s' order by type,name", 
					hostname, argv[1]);
		} else {
			sprintf(sql, "select distinct type, name, ikey, filename from interfaces where hostname='%s' order by type,name", hostname);
		}
	} else if (!strcasecmp(command, "interfaces")) {
		sprintf(sql, "select type, name, ikey from interfaces where hostname='%s' order by type,name", hostname);
	} else if (!strcasecmp(command, "interface_types")) {
		sprintf(sql, "select type,count(type) as total from interfaces where hostname='%s' group by type order by type", hostname);
	} else if (!strcasecmp(command, "tasks")) {
		sprintf(sql, "select * from %s where hostname='%s'", command, hostname);
	} else if (!strcasecmp(command, "application") || !strcasecmp(command, "api")) {
		if (argv[1] && strcasecmp(argv[1], "as")) {
			sprintf(sql,
					"select name, description, syntax, ikey from interfaces where hostname='%s' and type = '%s' and description != '' and name = '%s' order by type,name",
					hostname, command, argv[1]);
		} else {
			sprintf(sql, "select name, description, syntax, ikey from interfaces where hostname='%s' and type = '%s' and description != '' order by type,name", hostname, command);
		}
	} else if (!strcasecmp(command, "calls")) {
		sprintf(sql, "select * from basic_calls where hostname='%s' order by call_created_epoch", hostname);
		if (argv[1] && !strcasecmp(argv[1], "count")) {
			holder.justcount = 1;
			if (argv[3] && !strcasecmp(argv[2], "as")) {
				as = argv[3];
			}
		}
	} else if (!strcasecmp(command, "registrations")) {
		sprintf(sql, "select * from registrations where hostname='%s'", hostname);
		if (argv[1] && !strcasecmp(argv[1], "count")) {
			holder.justcount = 1;
			if (argv[3] && !strcasecmp(argv[2], "as")) {
				as = argv[3];
			}
		}
	} else if (!strcasecmp(command, "channels") && argv[1] && !strcasecmp(argv[1], "like")) {
		if (argv[2]) {
			char *p;
			for (p = argv[2]; p && *p; p++) {
				if (*p == '\'' || *p == ';') {
					*p = ' ';
				}
			}
			if (strchr(argv[2], '%')) {
				sprintf(sql,
						"select * from channels where hostname='%s' and uuid like '%s' or name like '%s' or cid_name like '%s' or cid_num like '%s' or presence_data like '%s' order by created_epoch",
						hostname, argv[2], argv[2], argv[2], argv[2], argv[2]);
			} else {
				sprintf(sql,
						"select * from channels where hostname='%s' and uuid like '%%%s%%' or name like '%%%s%%' or cid_name like '%%%s%%' or cid_num like '%%%s%%' or presence_data like '%%%s%%' order by created_epoch",
						hostname, argv[2], argv[2], argv[2], argv[2], argv[2]);

			}

			if (argv[4] && !strcasecmp(argv[3], "as")) {
				as = argv[4];
			}
		} else {
			sprintf(sql, "select * from channels where hostname='%s' order by created_epoch", hostname);
		}
	} else if (!strcasecmp(command, "channels")) {
		sprintf(sql, "select * from channels where hostname='%s' order by created_epoch", hostname);
		if (argv[1] && !strcasecmp(argv[1], "count")) {
			holder.justcount = 1;
			if (argv[3] && !strcasecmp(argv[2], "as")) {
				as = argv[3];
			}
		}
	} else if (!strcasecmp(command, "detailed_calls")) {
		sprintf(sql, "select * from detailed_calls where hostname='%s' order by created_epoch", hostname);
		if (argv[2] && !strcasecmp(argv[1], "as")) {
			as = argv[2];
		}
	} else if (!strcasecmp(command, "bridged_calls")) {
		sprintf(sql, "select * from basic_calls where b_uuid is not null and hostname='%s' order by created_epoch", hostname);
		if (argv[2] && !strcasecmp(argv[1], "as")) {
			as = argv[2];
		}
	} else if (!strcasecmp(command, "detailed_bridged_calls")) {
		sprintf(sql, "select * from detailed_calls where b_uuid is not null and hostname='%s' order by created_epoch", hostname);
		if (argv[2] && !strcasecmp(argv[1], "as")) {
			as = argv[2];
		}
	} else if (!strcasecmp(command, "aliases")) {
		sprintf(sql, "select * from aliases where hostname='%s' order by alias", hostname);
	} else if (!strcasecmp(command, "complete")) {
		sprintf(sql, "select * from complete where hostname='%s' order by a1,a2,a3,a4,a5,a6,a7,a8,a9,a10", hostname);
	} else if (!strncasecmp(command, "help", 4)) {
		char *cmdname = NULL;

		help = 1;
		holder.print_title = 0;
		if ((cmdname = strchr(command, ' ')) && strcasecmp(cmdname, "as")) {
			*cmdname++ = '\0';
			switch_snprintfv(sql, sizeof(sql),
							"select name, syntax, description, ikey from interfaces where hostname='%s' and type = 'api' and name = '%q' order by name", 
							hostname, cmdname);
		} else {
			switch_snprintfv(sql, sizeof(sql), "select name, syntax, description, ikey from interfaces where hostname='%q' and type = 'api' order by name", hostname);
		}
	} else if (!strcasecmp(command, "nat_map")) {
		switch_snprintf(sql, sizeof(sql) - 1,
						"SELECT port, "
						"  CASE proto "
						"	WHEN 0 THEN 'udp' "
						"	WHEN 1 THEN 'tcp' "
						"	ELSE 'unknown' " "  END AS proto, " "  proto AS proto_num, " "  sticky " " FROM nat where hostname='%s' ORDER BY port, proto", hostname);
	} else {
		stream->write_function(stream, "-USAGE: %s\n", SHOW_SYNTAX);
		goto end;
	}

	holder.stream = stream;
	holder.count = 0;

	if (holder.http) {
		holder.stream->write_function(holder.stream, "<table cellpadding=1 cellspacing=4 border=1>\n");
	}

	if (!as) {
		as = "delim";
		holder.delim = ",";
	}

	/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SQL: %s.\n", sql); */

	if (!strcasecmp(as, "delim") || !strcasecmp(as, "csv")) {
		if (zstr(holder.delim)) {
			if (!(holder.delim = argv[3])) {
				holder.delim = ",";
			}
		}
		switch_cache_db_execute_sql_callback(db, sql, show_callback, &holder, &errmsg);
		if (holder.http) {
			holder.stream->write_function(holder.stream, "</table>");
		}

		if (errmsg) {
			stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
			free(errmsg);
			errmsg = NULL;
		} else if (help) {
			if (holder.count == 0)
				stream->write_function(stream, "-ERR No such command.\n");
		} else {
			stream->write_function(stream, "\n%u total.\n", holder.count);
		}
	} else if (!strcasecmp(as, "xml")) {
		switch_cache_db_execute_sql_callback(db, sql, show_as_xml_callback, &holder, &errmsg);

		if (errmsg) {
			stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
			free(errmsg);
			errmsg = NULL;
		}

		if (holder.xml) {
			char count[50];
			char *xmlstr;
			switch_snprintf(count, sizeof(count), "%d", holder.count);

			switch_xml_set_attr(switch_xml_set_flag(holder.xml, SWITCH_XML_DUP), strdup("row_count"), strdup(count));
			xmlstr = switch_xml_toxml(holder.xml, SWITCH_FALSE);
			switch_xml_free(holder.xml);

			if (xmlstr) {
				holder.stream->write_function(holder.stream, "%s", xmlstr);
				free(xmlstr);
			} else {
				holder.stream->write_function(holder.stream, "<result row_count=\"0\"/>\n");
			}
		} else {
			holder.stream->write_function(holder.stream, "<result row_count=\"0\"/>\n");
		}
	} else if (!strcasecmp(as, "json")) {

		switch_cache_db_execute_sql_callback(db, sql, show_as_json_callback, &holder, &errmsg);

		if (errmsg) {
			stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
			free(errmsg);
			errmsg = NULL;
		}

		if (holder.json) {
			cJSON *result;

			if (!(result = cJSON_CreateObject())) {
				cJSON_Delete(holder.json);
				holder.json = NULL;
				holder.stream->write_function(holder.stream, "-ERR Error creating json object!\n");
			} else {
				char *json_text;

				cJSON_AddItemToObject(result, "row_count", cJSON_CreateNumber(holder.count));
				cJSON_AddItemToObject(result, "rows", holder.json);

				json_text = cJSON_PrintUnformatted(result);

				if (!json_text) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
					holder.stream->write_function(holder.stream, "-ERR Memory Error!\n");
				} else {
					holder.stream->write_function(holder.stream, json_text);
				}
				cJSON_Delete(result);
				switch_safe_free(json_text);
			}

		} else {
			holder.stream->write_function(holder.stream, "{\"row_count\": 0}\n");
		}

	} else {
		holder.stream->write_function(holder.stream, "-ERR Cannot find format %s\n", as);
	}

  end:

	switch_safe_free(mydata);

	if (db) {
		switch_cache_db_release_db_handle(&db);
	}

	return status;
}

SWITCH_STANDARD_API(help_function)
{
	char showcmd[1024];
	int all = 0;
	if (zstr(cmd)) {
		sprintf(showcmd, "help");
		all = 1;
	} else {
		switch_snprintf(showcmd, sizeof(showcmd) - 1, "help %s", cmd);
	}

	if (all) {
		stream->write_function(stream, "\nValid Commands:\n\n");
	}

	show_function(showcmd, session, stream);

	return SWITCH_STATUS_SUCCESS;
}

#define HEARTBEAT_SYNTAX "<uuid> [sched] [0|<seconds>]"
SWITCH_STANDARD_API(uuid_session_heartbeat_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	uint32_t seconds = 60;
	int argc, tmp;
	switch_core_session_t *l_session = NULL;
	int x = 0, sched = 0;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		goto error;
	}

	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2 || !argv[0]) {
		goto error;
	}

	if (!(l_session = switch_core_session_locate(argv[0]))) {
		stream->write_function(stream, "-ERR Usage: cannot locate session.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(argv[1], "sched")) {
		x = 2;
		sched++;
	} else {
		x = 1;
	}

	if (switch_is_number(argv[x])) {
		tmp = atoi(argv[x]);
		if (tmp > 0) {
			seconds = tmp;
		}
	} else if (!switch_true(argv[x])) {
		seconds = 0;
	}

	if (seconds) {
		if (sched) {
			switch_core_session_sched_heartbeat(l_session, seconds);
		} else {
			switch_core_session_enable_heartbeat(l_session, seconds);
		}

	} else {
		switch_core_session_disable_heartbeat(l_session);
	}

	switch_core_session_rwunlock(l_session);

	switch_safe_free(mycmd);
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;

  error:
	switch_safe_free(mycmd);
	stream->write_function(stream, "-ERR Usage: uuid_session_heartbeat %s", HEARTBEAT_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_flush_dtmf_function)
{
	switch_core_session_t *fsession;

	if (!zstr(cmd) && (fsession = switch_core_session_locate(cmd))) {
		switch_channel_flush_dtmf(switch_core_session_get_channel(fsession));
		switch_core_session_rwunlock(fsession);
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR no such session\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_zombie_exec_function)
{
	switch_core_session_t *fsession;

	if (!zstr(cmd) && (fsession = switch_core_session_locate(cmd))) {
		switch_channel_set_flag(switch_core_session_get_channel(fsession), CF_ZOMBIE_EXEC);
		switch_core_session_rwunlock(fsession);
		stream->write_function(stream, "+OK MMM Brains...\n");
	} else {
		stream->write_function(stream, "-ERR no such session\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define SETVAR_SYNTAX "<uuid> <var> [value]"
SWITCH_STANDARD_API(uuid_setvar_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if ((argc == 2 || argc == 3) && !zstr(argv[0])) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			char *var_value = NULL;

			if (argc == 3) {
				var_value = argv[2];
			}

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				channel = switch_core_session_get_channel(psession);

				if (zstr(var_name)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
					stream->write_function(stream, "-ERR No variable specified\n");
				} else {
					switch_channel_set_variable(channel, var_name, var_value);
					stream->write_function(stream, "+OK\n");
				}

				switch_core_session_rwunlock(psession);

			} else {
				stream->write_function(stream, "-ERR No Such Channel!\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", SETVAR_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


#define SETVAR_MULTI_SYNTAX "<uuid> <var>=<value>;<var>=<value>..."
SWITCH_STANDARD_API(uuid_setvar_multi_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *vars, *argv[64] = { 0 };
	int argc = 0;
	char *var_name, *var_value = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		char *uuid = mycmd;
		if (!(vars = strchr(uuid, ' '))) {
			goto done;
		}
		*vars++ = '\0';

		if ((psession = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(psession);
			int x, y = 0;
			argc = switch_separate_string(vars, ';', argv, (sizeof(argv) / sizeof(argv[0])));

			for (x = 0; x < argc; x++) {
				var_name = argv[x];
				if (var_name && (var_value = strchr(var_name, '='))) {
					*var_value++ = '\0';
				}
				if (zstr(var_name)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
					stream->write_function(stream, "-ERR No variable specified\n");
				} else {
					switch_channel_set_variable(channel, var_name, var_value);
					y++;
				}
			}

			switch_core_session_rwunlock(psession);
			if (y) {
				stream->write_function(stream, "+OK\n");
				goto done;
			}
		} else {
			stream->write_function(stream, "-ERR No Such Channel!\n");
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", SETVAR_MULTI_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define EXISTS_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_exists_function)
{
	switch_bool_t exists = SWITCH_FALSE;

	if (cmd) {
		exists = switch_ivr_uuid_exists(cmd);
	}

	stream->write_function(stream, "%s", exists ? "true" : "false");

	return SWITCH_STATUS_SUCCESS;
}


#define GETVAR_SYNTAX "<uuid> <var>"
SWITCH_STANDARD_API(uuid_getvar_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 2 && !zstr(argv[0])) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			const char *var_value = NULL;

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				channel = switch_core_session_get_channel(psession);

				if (zstr(var_name)) {
					stream->write_function(stream, "-ERR No variable name specified!\n");
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
				} else {
					var_value = switch_channel_get_variable(channel, var_name);
					if (var_value != NULL) {
						stream->write_function(stream, "%s", var_value);
					} else {
						stream->write_function(stream, "_undef_");
					}
				}

				switch_core_session_rwunlock(psession);

			} else {
				stream->write_function(stream, "-ERR No Such Channel!\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", GETVAR_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


#define FILEMAN_SYNTAX "<uuid> <cmd>:<val>"
SWITCH_STANDARD_API(uuid_fileman_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 2 && !zstr(argv[0])) {
			char *uuid = argv[0];
			char *cmd = argv[1];

			if ((psession = switch_core_session_locate(uuid))) {
				//switch_channel_t *channel;
				switch_file_handle_t *fh = NULL;
				
				//channel = switch_core_session_get_channel(psession);
				
				if (switch_ivr_get_file_handle(psession, &fh) == SWITCH_STATUS_SUCCESS) {
					switch_ivr_process_fh(psession, cmd, fh);
					switch_ivr_release_file_handle(psession, &fh);
					stream->write_function(stream, "+OK\n");
				} else {
					stream->write_function(stream, "-ERR No File Handle!\n");
				}

				switch_core_session_rwunlock(psession);

			} else {
				stream->write_function(stream, "-ERR No Such Channel!\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", GETVAR_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_SEND_DTMF_SYNTAX "<uuid> <dtmf_data>"
SWITCH_STANDARD_API(uuid_send_dtmf_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[2] = { 0 };
	char *uuid = NULL, *dtmf_data = NULL;
	int argc = 0;

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		goto usage;
	}

	uuid = argv[0];
	dtmf_data = argv[1];
	if (zstr(uuid) || zstr(dtmf_data)) {
		goto usage;
	}

	if (!(psession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_core_session_send_dtmf_string(psession, (const char *) dtmf_data);
	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", UUID_SEND_DTMF_SYNTAX);

  done:
	if (psession) {
		switch_core_session_rwunlock(psession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}



#define UUID_RECV_DTMF_SYNTAX "<uuid> <dtmf_data>"
SWITCH_STANDARD_API(uuid_recv_dtmf_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[2] = { 0 };
	char *uuid = NULL, *dtmf_data = NULL;
	int argc = 0;

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		goto usage;
	}

	uuid = argv[0];
	dtmf_data = argv[1];
	if (zstr(uuid) || zstr(dtmf_data)) {
		goto usage;
	}

	if (!(psession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_channel_queue_dtmf_string(switch_core_session_get_channel(psession), dtmf_data);
	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", UUID_RECV_DTMF_SYNTAX);

  done:
	if (psession) {
		switch_core_session_rwunlock(psession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define DUMP_SYNTAX "<uuid> [format]"
SWITCH_STANDARD_API(uuid_dump_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 0 && !zstr(argv[0])) {
			char *uuid = argv[0];
			char *format = argv[1];

			if (!format) {
				format = "txt";
			}

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				switch_event_t *event;
				char *buf;

				channel = switch_core_session_get_channel(psession);

				if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					switch_xml_t xml;
					switch_channel_event_set_data(channel, event);
					if (!strcasecmp(format, "xml")) {
						if ((xml = switch_event_xmlize(event, SWITCH_VA_NONE))) {
							buf = switch_xml_toxml(xml, SWITCH_FALSE);
							switch_xml_free(xml);
						} else {
							stream->write_function(stream, "-ERR Unable to create xml!\n");
							switch_event_destroy(&event);
							switch_core_session_rwunlock(psession);
							goto done;
						}
					} else if (!strcasecmp(format, "json")) {
						switch_event_serialize_json(event, &buf);
					} else {
						switch_event_serialize(event, &buf, strcasecmp(format, "plain"));
					}

					switch_assert(buf);
					stream->raw_write_function(stream, (unsigned char *) buf, strlen(buf));
					switch_event_destroy(&event);
					free(buf);
				} else {
					stream->write_function(stream, "-ERR Allocation error\n");
				}

				switch_core_session_rwunlock(psession);

			} else {
				stream->write_function(stream, "-ERR No Such Channel!\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", DUMP_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define GLOBAL_SETVAR_SYNTAX "<var>=<value> [=<value2>]"
SWITCH_STANDARD_API(global_setvar_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, '=', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc > 0 && !zstr(argv[0])) {
			char *var_name = argv[0];
			char *var_value = argv[1];
			char *var_value2 = argv[2];

			if (zstr(var_value)) {
				var_value = NULL;
			}

			if (zstr(var_value2)) {
				var_value2 = NULL;
			}

			if (var_value2) {
				switch_core_set_var_conditional(var_name, var_value, var_value2);
			} else {
				switch_core_set_variable(var_name, var_value);
			}
			stream->write_function(stream, "+OK");
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", GLOBAL_SETVAR_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define GLOBAL_GETVAR_SYNTAX "<var>"
SWITCH_STANDARD_API(global_getvar_function)
{
	if (zstr(cmd)) {
		switch_core_dump_variables(stream);
	} else {
		char *var = switch_core_get_variable_dup(cmd);
		stream->write_function(stream, "%s", switch_str_nil(var));
		switch_safe_free(var);
	}
	return SWITCH_STATUS_SUCCESS;
}

#define SYSTEM_SYNTAX "<command>"
SWITCH_STANDARD_API(system_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", SYSTEM_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_stream_system(cmd, stream) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Failed to execute command: %s\n", cmd);
	}

	return SWITCH_STATUS_SUCCESS;
}


#define SYSTEM_SYNTAX "<command>"
SWITCH_STANDARD_API(bg_system_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", SYSTEM_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Executing command: %s\n", cmd);
	if (switch_system(cmd, SWITCH_FALSE) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Failed to execute command: %s\n", cmd);
	}
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(strftime_tz_api_function)
{
	char *format = NULL;
	const char *tz_name = NULL;
	char date[80] = "";
	char *mycmd = NULL, *p;
	switch_time_t when = 0;

	if (cmd) mycmd = strdup(cmd);

	if (!zstr(mycmd)) {
		tz_name = mycmd;

		if ((format = strchr(mycmd, ' '))) {
			*format++ = '\0';
		
			if (format && (p = strchr(format, '|'))) {
				*p++ = '\0';
				when = atol(format);
				format = p;
			}
		}
	}

	if (zstr(format)) {
		format = "%Y-%m-%d";
	}
	
	if (format && switch_strftime_tz(tz_name, format, date, sizeof(date), when * 1000000) == SWITCH_STATUS_SUCCESS) {	/* The lookup of the zone may fail. */
		stream->write_function(stream, "%s", date);
	} else {
		stream->write_function(stream, "-ERR Invalid Timezone/Format\n");
	}

	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(hupall_api_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	char *var = NULL;
	char *val = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_MANAGER_REQUEST;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		switch_assert(argv[0]);
		if ((cause = switch_channel_str2cause(argv[0])) == SWITCH_CAUSE_NONE) {
			cause = SWITCH_CAUSE_MANAGER_REQUEST;
		}
		var = argv[1];
		val = argv[2];
	}

	if (!val) {
		var = NULL;
	}

	if (zstr(var)) {
		switch_core_session_hupall(cause);
	} else {
		switch_core_session_hupall_matching_var(var, val, cause);
	}

	if (zstr(var)) {
		stream->write_function(stream, "+OK hangup all channels with cause %s\n", switch_channel_cause2str(cause));
	} else {
		stream->write_function(stream, "+OK hangup all channels matching [%s]=[%s] with cause: %s\n", var, val, switch_channel_cause2str(cause));
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(xml_flush_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;
	int r = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_split(mycmd, ' ', argv);
	}

	if (argc == 3) {
		r = switch_xml_clear_user_cache(argv[0], argv[1], argv[2]);
	} else {
		r = switch_xml_clear_user_cache(NULL, NULL, NULL);
	}


	stream->write_function(stream, "+OK cleared %u entr%s\n", r, r == 1 ? "y" : "ies");

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(escape_function)
{
	int len;
	char *mycmd;

	if (zstr(cmd)) {
		return SWITCH_STATUS_SUCCESS;
	}

	len = (int)strlen(cmd) * 2;
	mycmd = malloc(len);

	stream->write_function(stream, "%s", switch_escape_string(cmd, mycmd, len));

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_LOGLEVEL_SYNTAX "<uuid> <level>"
SWITCH_STANDARD_API(uuid_loglevel)
{
	switch_core_session_t *tsession = NULL;
	char *uuid = NULL, *text = NULL;

	if (!zstr(cmd) && (uuid = strdup(cmd))) {
		if ((text = strchr(uuid, ' '))) {
			*text++ = '\0';
		}
	}

	if (zstr(uuid) || zstr(text)) {
		stream->write_function(stream, "-USAGE: %s\n", UUID_LOGLEVEL_SYNTAX);
	} else {
		switch_log_level_t level = switch_log_str2level(text);

		if (level == SWITCH_LOG_INVALID) {
			stream->write_function(stream, "-ERR Invalid log level!\n");
		} else if ((tsession = switch_core_session_locate(uuid))) {

			switch_core_session_set_loglevel(tsession, level);
			stream->write_function(stream, "+OK\n");
			switch_core_session_rwunlock(tsession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel %s!\n", uuid);
		}
	}

	switch_safe_free(uuid);
	return SWITCH_STATUS_SUCCESS;
}

#define SQL_ESCAPE_SYNTAX "<string>"
SWITCH_STANDARD_API(sql_escape)
{
	if (!cmd) {
		stream->write_function(stream, "-USAGE: %s\n", SQL_ESCAPE_SYNTAX);
	} else {
		stream->write_function(stream, "%q", cmd);
	}

	return SWITCH_STATUS_SUCCESS;
}

/* LIMIT Stuff */
#define LIMIT_USAGE_SYNTAX "<backend> <realm> <id> [rate]"
SWITCH_STANDARD_API(limit_usage_function)
{
	int argc = 0;
	char *argv[5] = { 0 };
	char *mydata = NULL;
	uint32_t count = 0;
	uint32_t rcount = 0;
	switch_bool_t dorate = SWITCH_FALSE;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	/* backwards compat version */
	if (argc == 2) {
		switch_safe_free(mydata);
		/* allocate space for "db " */
		mydata = malloc(strlen(cmd) + 10);
		switch_assert(mydata);
		sprintf(mydata, "db %s", cmd);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Using deprecated limit api: Please specify backend.  Defaulting to 'db' backend.\n");
	}
	
	if (argc < 3) {
		stream->write_function(stream, "USAGE: limit_usage %s\n", LIMIT_USAGE_SYNTAX);
		goto end;
	}

	if (argc > 3) {
		if (!strcasecmp("rate", argv[3])) {
			dorate = SWITCH_TRUE;
		}
	}
	
	count = switch_limit_usage(argv[0], argv[1], argv[2], &rcount);
	
	if (dorate == SWITCH_TRUE) {
		stream->write_function(stream, "%d/%d", count, rcount);
	} else {
		stream->write_function(stream, "%d", count);
	}

end:
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

#define LIMIT_HASH_USAGE_SYNTAX "<realm> <id> [rate] (Using deprecated limit api, check limit_usage with backend param)"
SWITCH_STANDARD_API(limit_hash_usage_function)
{
	char *mydata = NULL;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	if (!zstr(cmd)) {
		mydata = switch_mprintf("hash %s", cmd);
		ret = limit_usage_function(mydata, session, stream);
		switch_safe_free(mydata);
		return ret;
	} else {
		stream->write_function(stream, "USAGE: limit_hash_usage %s\n", LIMIT_HASH_USAGE_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}
}

#define LIMIT_STATUS_SYNTAX "<backend>"
SWITCH_STANDARD_API(limit_status_function)
{
	int argc = 0;
	char *argv[2] = { 0 };
	char *mydata = NULL;
	char *ret = NULL;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		stream->write_function(stream, "USAGE: limit_status %s\n", LIMIT_STATUS_SYNTAX);
		goto end;
	}
	
	ret = switch_limit_status(argv[0]);

	stream->write_function(stream, "%s", ret);

end:
	switch_safe_free(mydata);
	switch_safe_free(ret);

	return SWITCH_STATUS_SUCCESS;
}

#define LIMIT_RESET_SYNTAX "<backend>"
SWITCH_STANDARD_API(limit_reset_function)
{
	int argc = 0;
	char *argv[2] = { 0 };
	char *mydata = NULL;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		stream->write_function(stream, "USAGE: limit_reset %s\n", LIMIT_RESET_SYNTAX);
		goto end;
	}
	
	ret = switch_limit_reset(argv[0]);

	stream->write_function(stream, "%s", (ret == SWITCH_STATUS_SUCCESS) ? "+OK" : "-ERR");

end:
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

#define LIMIT_SYNTAX "<uuid> <backend> <realm> <resource> [<max>[/interval]] [number [dialplan [context]]]"
SWITCH_STANDARD_API(uuid_limit_function)
{
	int argc = 0;
	char *argv[8] = { 0 };
	char *mydata = NULL;
	char *realm = NULL;
	char *resource = NULL;
	char *xfer_exten = NULL;
	int max = -1;
	int interval = 0;
	switch_core_session_t *sess = NULL;
	switch_status_t res = SWITCH_STATUS_SUCCESS;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 4) {
		stream->write_function(stream, "USAGE: uuid_limit %s\n", LIMIT_SYNTAX);
		goto end;
	}

	realm = argv[2];
	resource = argv[3];

	/* If max is omitted or negative, only act as a counter and skip maximum checks */
	if (argc > 4) {
		if (argv[4][0] == '-') {
			max = -1;
		} else {
			char *szinterval = NULL;
			if ((szinterval = strchr(argv[4], '/'))) {
				*szinterval++ = '\0';
				interval = atoi(szinterval);
			}

			max = atoi(argv[4]);

			if (max < 0) {
				max = 0;
			}
		}
	}

	if (argc > 5) {
		xfer_exten = argv[5];
	} else {
		xfer_exten = LIMIT_DEF_XFER_EXTEN;
	}

	sess = switch_core_session_locate(argv[0]);
	if (!sess) {
		stream->write_function(stream, "-ERR did not find a session with uuid %s\n", argv[0]);
		goto end;
	}

	res = switch_limit_incr(argv[1], sess, realm, resource, max, interval);

	if (res != SWITCH_STATUS_SUCCESS) {
		/* Limit exceeded */
		if (*xfer_exten == '!') {
			switch_channel_t *channel = switch_core_session_get_channel(sess);
			switch_channel_hangup(channel, switch_channel_str2cause(xfer_exten + 1));
		} else {
			switch_ivr_session_transfer(sess, xfer_exten, argv[6], argv[7]);
		}
	}

	switch_core_session_rwunlock(sess);

	stream->write_function(stream, "+OK");

end:
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

#define LIMIT_RELEASE_SYNTAX "<uuid> <backend> [realm] [resource]"
SWITCH_STANDARD_API(uuid_limit_release_function)
{
	int argc = 0;
	char *argv[5] = { 0 };
	char *mydata = NULL;
	char *realm = NULL;
	char *resource = NULL;
	switch_core_session_t *sess = NULL;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 2) {
		stream->write_function(stream, "USAGE: uuid_limit_release %s\n", LIMIT_RELEASE_SYNTAX);
		goto end;
	}

	if (argc > 2) {
		realm = argv[2];
	}

	if (argc > 3) {
		resource = argv[3];
	}

	sess = switch_core_session_locate(argv[0]);
	if (!sess) {
		stream->write_function(stream, "-ERR did not find a session with uuid %s\n", argv[0]);
		goto end;
	}

	switch_limit_release(argv[1], sess, realm, resource);

	switch_core_session_rwunlock(sess);
	
	stream->write_function(stream, "+OK");

end:
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

#define LIMIT_INTERVAL_RESET_SYNTAX "<backend> <realm> <resource>"
SWITCH_STANDARD_API(limit_interval_reset_function)
{
	int argc = 0;
	char *argv[5] = { 0 };
	char *mydata = NULL;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 3) {
		stream->write_function(stream, "USAGE: limit_interval_reset %s\n", LIMIT_INTERVAL_RESET_SYNTAX);
		goto end;
	}

	switch_limit_interval_reset(argv[0], argv[1], argv[2]);

	stream->write_function(stream, "+OK");

end:
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_commands_shutdown)
{
	int x;

	for (x = 30; x > 0; x--) {
		if (switch_thread_rwlock_trywrlock(bgapi_rwlock) == SWITCH_STATUS_SUCCESS) {
			switch_thread_rwlock_unlock(bgapi_rwlock);
			break;
		}
		if (x == 30) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for bgapi threads.\n");
		}
		switch_yield(1000000);
	}

	if (!x) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Giving up waiting for bgapi threads.\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define LOG_SYNTAX "<level> <message>"
SWITCH_STANDARD_API(log_function)
{
	char *level, *log_str;

	if (cmd && (level = strdup(cmd))) {
		switch_log_level_t ltype = SWITCH_LOG_DEBUG;

		if ((log_str = strchr(level, ' '))) {
			*log_str++ = '\0';
			ltype = switch_log_str2level(level);
		} else {
			log_str = level;
		}
		if (ltype == SWITCH_LOG_INVALID) {
			ltype = SWITCH_LOG_DEBUG;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), ltype, "%s\n", log_str);
		switch_safe_free(level);
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(file_exists_function)
{
	if (!zstr(cmd)) {
		switch_memory_pool_t *pool;

		switch_core_new_memory_pool(&pool);

		if (switch_file_exists(cmd, pool) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "true");
		} else {
			stream->write_function(stream, "false");
		}

		switch_core_destroy_memory_pool(&pool);
	} else {
		stream->write_function(stream, "false");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_commands_load)
{
	switch_api_interface_t *commands_api_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_thread_rwlock_create(&bgapi_rwlock, pool);
	switch_mutex_init(&reload_mutex, SWITCH_MUTEX_NESTED, pool);
	

	SWITCH_ADD_API(commands_api_interface, "acl", "compare an ip to an acl list", acl_function, "<ip> <list_name>");
	SWITCH_ADD_API(commands_api_interface, "alias", "Alias", alias_function, ALIAS_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "banner", "Returns the system banner", banner_function, "");
	SWITCH_ADD_API(commands_api_interface, "bgapi", "Execute an api command in a thread", bgapi_function, "<command>[ <arg>]");
	SWITCH_ADD_API(commands_api_interface, "bg_system", "Execute a system command in the background", bg_system_function, SYSTEM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "break", "Break", break_function, BREAK_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "complete", "Complete", complete_function, COMPLETE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "cond", "Eval a conditional", cond_function, "<expr> ? <true val> : <false val>");
	SWITCH_ADD_API(commands_api_interface, "console_complete", "", console_complete_function, "<line>");
	SWITCH_ADD_API(commands_api_interface, "console_complete_xml", "", console_complete_xml_function, "<line>");
	SWITCH_ADD_API(commands_api_interface, "create_uuid", "Create a uuid", uuid_function, UUID_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "db_cache", "db cache management", db_cache_function, "status");
	SWITCH_ADD_API(commands_api_interface, "domain_exists", "check if a domain exists", domain_exists_function, "<domain>");
	SWITCH_ADD_API(commands_api_interface, "echo", "echo", echo_function, "<data>");
	SWITCH_ADD_API(commands_api_interface, "escape", "escape a string", escape_function, "<data>");
	SWITCH_ADD_API(commands_api_interface, "eval", "eval (noop)", eval_function, "[uuid:<uuid> ]<expression>");
	SWITCH_ADD_API(commands_api_interface, "expand", "expand vars and execute", expand_function, "[uuid:<uuid> ]<cmd> <args>");
	SWITCH_ADD_API(commands_api_interface, "find_user_xml", "find a user", find_user_function, "<key> <user> <domain>");
	SWITCH_ADD_API(commands_api_interface, "fsctl", "control messages", ctl_function, CTL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "...", "shutdown", shutdown_function, "");
	SWITCH_ADD_API(commands_api_interface, "shutdown", "shutdown", shutdown_function, "");
	SWITCH_ADD_API(commands_api_interface, "version", "version", version_function, "[short]");
	SWITCH_ADD_API(commands_api_interface, "global_getvar", "global_getvar", global_getvar_function, GLOBAL_GETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "global_setvar", "global_setvar", global_setvar_function, GLOBAL_SETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "group_call", "Generate a dial string to call a group", group_call_function, "<group>[@<domain>]");
	SWITCH_ADD_API(commands_api_interface, "help", "Show help for all the api commands", help_function, "");
	SWITCH_ADD_API(commands_api_interface, "host_lookup", "host_lookup", host_lookup_function, "<hostname>");
	SWITCH_ADD_API(commands_api_interface, "hostname", "Returns the system hostname", hostname_api_function, "");
	SWITCH_ADD_API(commands_api_interface, "switchname", "Returns the switch name", switchname_api_function, "");
	SWITCH_ADD_API(commands_api_interface, "hupall", "hupall", hupall_api_function, "<cause> [<var> <value>]");
	SWITCH_ADD_API(commands_api_interface, "in_group", "determine if a user is in a group", in_group_function, "<user>[@<domain>] <group_name>");
	SWITCH_ADD_API(commands_api_interface, "is_lan_addr", "see if an ip is a lan addr", lan_addr_function, "<ip>");
	SWITCH_ADD_API(commands_api_interface, "limit_usage", "Gets the usage count of a limited resource", limit_usage_function, "<backend> <realm> <id>");
	SWITCH_ADD_API(commands_api_interface, "limit_hash_usage", "Deprecated: gets the usage count of a limited resource", limit_hash_usage_function, "<realm> <id>");
	SWITCH_ADD_API(commands_api_interface, "limit_status", "Gets the status of a limit backend", limit_status_function, "<backend>");
	SWITCH_ADD_API(commands_api_interface, "limit_reset", "Reset the counters of a limit backend", limit_reset_function, "<backend>");
	SWITCH_ADD_API(commands_api_interface, "limit_interval_reset", "Reset the interval counter for a limited resource", limit_interval_reset_function, LIMIT_INTERVAL_RESET_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "list_users", "List Users configured in Directory", list_users_function, LIST_USERS_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "load", "Load Module", load_function, LOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "log", "Log", log_function, LOG_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "md5", "md5", md5_function, "<data>");
	SWITCH_ADD_API(commands_api_interface, "module_exists", "check if module exists", module_exists_function, "<module>");
	SWITCH_ADD_API(commands_api_interface, "msleep", "sleep N milliseconds", msleep_function, "<milliseconds>");
	SWITCH_ADD_API(commands_api_interface, "nat_map", "nat_map", nat_map_function, "[status|republish|reinit] | [add|del] <port> [tcp|udp] [static]");
	SWITCH_ADD_API(commands_api_interface, "originate", "Originate a Call", originate_function, ORIGINATE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "pause", "Pause", pause_function, PAUSE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "regex", "Eval a regex", regex_function, "<data>|<pattern>[|<subst string>]");
	SWITCH_ADD_API(commands_api_interface, "reloadacl", "Reload ACL", reload_acl_function, "");
	SWITCH_ADD_API(commands_api_interface, "reload", "Reload Module", reload_function, UNLOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "reloadxml", "Reload XML", reload_xml_function, "");
	SWITCH_ADD_API(commands_api_interface, "replace", "replace a string", replace_function, "<data>|<string1>|<string2>");
	SWITCH_ADD_API(commands_api_interface, "say_string", "", say_string_function, SAY_STRING_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_api", "Schedule an api command", sched_api_function, SCHED_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_broadcast", "Schedule a broadcast event to a running call", sched_broadcast_function,
				   SCHED_BROADCAST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_del", "Delete a Scheduled task", sched_del_function, "<task_id>|<group_id>");
	SWITCH_ADD_API(commands_api_interface, "sched_hangup", "Schedule a running call to hangup", sched_hangup_function, SCHED_HANGUP_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_transfer", "Schedule a transfer for a running call", sched_transfer_function, SCHED_TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "show", "Show", show_function, SHOW_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sql_escape", "Escape a string to prevent sql injection", sql_escape, SQL_ESCAPE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "status", "status", status_function, "");
	SWITCH_ADD_API(commands_api_interface, "strftime_tz", "strftime_tz", strftime_tz_api_function, "<Timezone_name> [<epoch>|][format string]");
	SWITCH_ADD_API(commands_api_interface, "stun", "stun", stun_function, "<stun_server>[:port]");
	SWITCH_ADD_API(commands_api_interface, "system", "Execute a system command", system_function, SYSTEM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "time_test", "time_test", time_test_function, "<mss> [count]");
	SWITCH_ADD_API(commands_api_interface, "timer_test", "timer_test", timer_test_function, TIMER_TEST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "tone_detect", "Start Tone Detection on a channel", tone_detect_session_function, TONE_DETECT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "unload", "Unload Module", unload_function, UNLOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "unsched_api", "Unschedule an api command", unsched_api_function, UNSCHED_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "reg_url", "", reg_url_function, "<user>@<realm>");
	SWITCH_ADD_API(commands_api_interface, "url_decode", "url decode a string", url_decode_function, "<string>");
	SWITCH_ADD_API(commands_api_interface, "url_encode", "url encode a string", url_encode_function, "<string>");
	SWITCH_ADD_API(commands_api_interface, "user_data", "find user data", user_data_function, "<user>@<domain> [var|param|attr] <name>");
	SWITCH_ADD_API(commands_api_interface, "uuid_early_ok", "stop ignoring early media", uuid_early_ok_function, "<uuid>");
	SWITCH_ADD_API(commands_api_interface, "user_exists", "find a user", user_exists_function, "<key> <user> <domain>");
	SWITCH_ADD_API(commands_api_interface, "uuid_answer", "answer", uuid_answer_function, "<uuid>");
	SWITCH_ADD_API(commands_api_interface, "uuid_audio", "uuid_audio", session_audio_function, AUDIO_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_break", "Break", break_function, BREAK_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_bridge", "uuid_bridge", uuid_bridge_function, "");
	SWITCH_ADD_API(commands_api_interface, "uuid_broadcast", "broadcast", uuid_broadcast_function, BROADCAST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_buglist", "List media bugs on a session", uuid_buglist_function, BUGLIST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_chat", "Send a chat message", uuid_chat, UUID_CHAT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_debug_audio", "debug audio", uuid_debug_audio_function, DEBUG_AUDIO_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_deflect", "Send a deflect", uuid_deflect, UUID_DEFLECT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_displace", "session displace", session_displace_function, "<uuid> [start|stop] <path> [<limit>] [mux]");
	SWITCH_ADD_API(commands_api_interface, "uuid_display", "change display", uuid_display_function, DISPLAY_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_dump", "uuid_dump", uuid_dump_function, DUMP_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_exists", "see if a uuid exists", uuid_exists_function, EXISTS_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_fileman", "uuid_fileman", uuid_fileman_function, FILEMAN_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_flush_dtmf", "Flush dtmf on a given uuid", uuid_flush_dtmf_function, "<uuid>");
	SWITCH_ADD_API(commands_api_interface, "uuid_getvar", "uuid_getvar", uuid_getvar_function, GETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_hold", "hold", uuid_hold_function, HOLD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_kill", "Kill Channel", kill_function, KILL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_send_message", "Send MESSAGE to the endpoint", uuid_send_message_function, SEND_MESSAGE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_send_info", "Send info to the endpoint", uuid_send_info_function, INFO_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_video_refresh", "Send video refresh.", uuid_video_refresh_function, VIDEO_REFRESH_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_outgoing_answer", "Answer Outgoing Channel", outgoing_answer_function, OUTGOING_ANSWER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_limit", "Increase limit resource", uuid_limit_function, LIMIT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_limit_release", "Release limit resource", uuid_limit_release_function, LIMIT_RELEASE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_loglevel", "set loglevel on session", uuid_loglevel, UUID_LOGLEVEL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_media", "media", uuid_media_function, MEDIA_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_media_reneg", "media negotiation", uuid_media_neg_function, MEDIA_RENEG_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_park", "Park Channel", park_function, PARK_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_phone_event", "Send and event to the phone", uuid_phone_event_function, PHONE_EVENT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_pre_answer", "pre_answer", uuid_pre_answer_function, "<uuid>");
	SWITCH_ADD_API(commands_api_interface, "uuid_preprocess", "Pre-process Channel", preprocess_function, PREPROCESS_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_record", "session record", session_record_function, SESS_REC_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_recovery_refresh", "Send a recovery_refresh", uuid_recovery_refresh, UUID_RECOVERY_REFRESH_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_recv_dtmf", "receive dtmf digits", uuid_recv_dtmf_function, UUID_RECV_DTMF_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_send_dtmf", "send dtmf digits", uuid_send_dtmf_function, UUID_SEND_DTMF_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_session_heartbeat", "uuid_session_heartbeat", uuid_session_heartbeat_function, HEARTBEAT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_setvar_multi", "uuid_setvar_multi", uuid_setvar_multi_function, SETVAR_MULTI_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_setvar", "uuid_setvar", uuid_setvar_function, SETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_transfer", "Transfer a session", transfer_function, TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_dual_transfer", "Transfer a session and its partner", dual_transfer_function, DUAL_TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_simplify", "Try to cut out of a call path / attended xfer", uuid_simplify_function, SIMPLIFY_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_jitterbuffer", "Try to cut out of a call path / attended xfer", 
				   uuid_jitterbuffer_function, JITTERBUFFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_zombie_exec", "Set zombie_exec flag on the specified uuid", uuid_zombie_exec_function, "<uuid>");
	SWITCH_ADD_API(commands_api_interface, "xml_flush_cache", "clear xml cache", xml_flush_function, "<id> <key> <val>");
	SWITCH_ADD_API(commands_api_interface, "xml_locate", "find some xml", xml_locate_function, "[root | <section> <tag> <tag_attr_name> <tag_attr_val>]");
	SWITCH_ADD_API(commands_api_interface, "xml_wrap", "Wrap another api command in xml", xml_wrap_api_function, "<command> <args>");
	SWITCH_ADD_API(commands_api_interface, "file_exists", "check if a file exists on server", file_exists_function, "<file>");


	switch_console_set_complete("add alias add");
	switch_console_set_complete("add alias del");
	switch_console_set_complete("add complete add");
	switch_console_set_complete("add complete del");
	switch_console_set_complete("add db_cache status");
	switch_console_set_complete("add fsctl debug_level");
	switch_console_set_complete("add fsctl debug_sql");
	switch_console_set_complete("add fsctl last_sps");
	switch_console_set_complete("add fsctl default_dtmf_duration");
	switch_console_set_complete("add fsctl hupall");
	switch_console_set_complete("add fsctl loglevel");
	switch_console_set_complete("add fsctl loglevel console");
	switch_console_set_complete("add fsctl loglevel alert");
	switch_console_set_complete("add fsctl loglevel crit");
	switch_console_set_complete("add fsctl loglevel err");
	switch_console_set_complete("add fsctl loglevel warning");
	switch_console_set_complete("add fsctl loglevel notice");
	switch_console_set_complete("add fsctl loglevel info");
	switch_console_set_complete("add fsctl loglevel debug");
	switch_console_set_complete("add fsctl max_dtmf_duration");
	switch_console_set_complete("add fsctl max_sessions");
	switch_console_set_complete("add fsctl min_dtmf_duration");
	switch_console_set_complete("add fsctl pause");
	switch_console_set_complete("add fsctl pause inbound");
	switch_console_set_complete("add fsctl pause outbound");
	switch_console_set_complete("add fsctl reclaim_mem");
	switch_console_set_complete("add fsctl resume");
	switch_console_set_complete("add fsctl resume inbound");
	switch_console_set_complete("add fsctl resume outbound");
	switch_console_set_complete("add fsctl calibrate_clock");
	switch_console_set_complete("add fsctl crash");
	switch_console_set_complete("add fsctl verbose_events");
	switch_console_set_complete("add fsctl save_history");
	switch_console_set_complete("add fsctl pause_check");
	switch_console_set_complete("add fsctl pause_check inbound");
	switch_console_set_complete("add fsctl pause_check outbound");
	switch_console_set_complete("add fsctl ready_check");
	switch_console_set_complete("add fsctl recover");
	switch_console_set_complete("add fsctl shutdown_check");
	switch_console_set_complete("add fsctl shutdown");
	switch_console_set_complete("add fsctl shutdown asap");
	switch_console_set_complete("add fsctl shutdown now");
	switch_console_set_complete("add fsctl shutdown asap restart");
	switch_console_set_complete("add fsctl shutdown cancel");
	switch_console_set_complete("add fsctl shutdown elegant");
	switch_console_set_complete("add fsctl shutdown elegant restart");
	switch_console_set_complete("add fsctl shutdown restart");
	switch_console_set_complete("add fsctl shutdown restart asap");
	switch_console_set_complete("add fsctl shutdown restart elegant");
	switch_console_set_complete("add fsctl sps");
	switch_console_set_complete("add fsctl sync_clock");
	switch_console_set_complete("add fsctl flush_db_handles");
	switch_console_set_complete("add fsctl min_idle_cpu");
	switch_console_set_complete("add fsctl send_sighup");
	switch_console_set_complete("add load ::console::list_available_modules");
	switch_console_set_complete("add nat_map reinit");
	switch_console_set_complete("add nat_map republish");
	switch_console_set_complete("add nat_map status");
	switch_console_set_complete("add reload ::console::list_loaded_modules");
	switch_console_set_complete("add reloadacl reloadxml");
	switch_console_set_complete("add show aliases");
	switch_console_set_complete("add show api");
	switch_console_set_complete("add show application");
	switch_console_set_complete("add show calls");
	switch_console_set_complete("add show channels");
	switch_console_set_complete("add show channels count");
	switch_console_set_complete("add show chat");
	switch_console_set_complete("add show codec");
	switch_console_set_complete("add show complete");
	switch_console_set_complete("add show dialplan");
	switch_console_set_complete("add show detailed_calls");
	switch_console_set_complete("add show bridged_calls");
	switch_console_set_complete("add show detailed_bridged_calls");
	switch_console_set_complete("add show endpoint");
	switch_console_set_complete("add show file");
	switch_console_set_complete("add show interfaces");
	switch_console_set_complete("add show interface_types");
	switch_console_set_complete("add show tasks");
	switch_console_set_complete("add show management");
	switch_console_set_complete("add show modules");
	switch_console_set_complete("add show nat_map");
	switch_console_set_complete("add show registrations");
	switch_console_set_complete("add show say");
	switch_console_set_complete("add show timer");
	switch_console_set_complete("add shutdown");
	switch_console_set_complete("add sql_escape");
	switch_console_set_complete("add unload ::console::list_loaded_modules");
	switch_console_set_complete("add uuid_audio ::console::list_uuid start read mute");
	switch_console_set_complete("add uuid_audio ::console::list_uuid start read level");
	switch_console_set_complete("add uuid_audio ::console::list_uuid start write mute");
	switch_console_set_complete("add uuid_audio ::console::list_uuid start write level");
	switch_console_set_complete("add uuid_audio ::console::list_uuid stop");
	switch_console_set_complete("add uuid_break ::console::list_uuid all");
	switch_console_set_complete("add uuid_break ::console::list_uuid both");
	switch_console_set_complete("add uuid_bridge ::console::list_uuid ::console::list_uuid");
	switch_console_set_complete("add uuid_broadcast ::console::list_uuid");
	switch_console_set_complete("add uuid_buglist ::console::list_uuid");
	switch_console_set_complete("add uuid_chat ::console::list_uuid");
	switch_console_set_complete("add uuid_debug_audio ::console::list_uuid");
	switch_console_set_complete("add uuid_deflect ::console::list_uuid");
	switch_console_set_complete("add uuid_displace ::console::list_uuid");
	switch_console_set_complete("add uuid_display ::console::list_uuid");
	switch_console_set_complete("add uuid_dump ::console::list_uuid");
	switch_console_set_complete("add uuid_answer ::console::list_uuid");
	switch_console_set_complete("add uuid_pre_answer ::console::list_uuid");
	switch_console_set_complete("add uuid_early_ok ::console::list_uuid");
	switch_console_set_complete("add uuid_exists ::console::list_uuid");
	switch_console_set_complete("add uuid_fileman ::console::list_uuid");
	switch_console_set_complete("add uuid_flush_dtmf ::console::list_uuid");
	switch_console_set_complete("add uuid_getvar ::console::list_uuid");
	switch_console_set_complete("add uuid_hold ::console::list_uuid");
	switch_console_set_complete("add uuid_send_info ::console::list_uuid");
	switch_console_set_complete("add uuid_jitterbuffer ::console::list_uuid");
	switch_console_set_complete("add uuid_kill ::console::list_uuid");
	switch_console_set_complete("add uuid_outgoing_answer ::console::list_uuid");
	switch_console_set_complete("add uuid_limit ::console::list_uuid");
	switch_console_set_complete("add uuid_limit_release ::console::list_uuid");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid console");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid alert");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid crit");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid err");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid warning");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid notice");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid info");
	switch_console_set_complete("add uuid_loglevel ::console::list_uuid debug");
	switch_console_set_complete("add uuid_media ::console::list_uuid");
	switch_console_set_complete("add uuid_media off ::console::list_uuid");
	switch_console_set_complete("add uuid_park ::console::list_uuid");
	switch_console_set_complete("add uuid_media_reneg ::console::list_uuid");
	switch_console_set_complete("add uuid_phone_event ::console::list_uuid talk");
	switch_console_set_complete("add uuid_phone_event ::console::list_uuid hold");
	switch_console_set_complete("add uuid_preprocess ::console::list_uuid");
	switch_console_set_complete("add uuid_record ::console::list_uuid ::[start:stop");
	switch_console_set_complete("add uuid_recovery_refresh ::console::list_uuid");
	switch_console_set_complete("add uuid_recv_dtmf ::console::list_uuid");
	switch_console_set_complete("add uuid_send_dtmf ::console::list_uuid");
	switch_console_set_complete("add uuid_session_heartbeat ::console::list_uuid");
	switch_console_set_complete("add uuid_setvar_multi ::console::list_uuid");
	switch_console_set_complete("add uuid_setvar ::console::list_uuid");
	switch_console_set_complete("add uuid_simplify ::console::list_uuid");
	switch_console_set_complete("add uuid_transfer ::console::list_uuid");
	switch_console_set_complete("add uuid_dual_transfer ::console::list_uuid");
	switch_console_set_complete("add uuid_video_refresh ::console::list_uuid");
	switch_console_set_complete("add version");
	switch_console_set_complete("add uuid_warning ::console::list_uuid");
	switch_console_set_complete("add ...");
	switch_console_set_complete("add file_exists");


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
}

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
