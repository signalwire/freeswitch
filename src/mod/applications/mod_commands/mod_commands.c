/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 * Johny Kadarisman <jkr888@gmail.com>
 * Paul Tinsley <jackhammer@gmail.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Cesar Cepeda <cesar@auronix.com>
 * Massimo Cetra <devel@navynet.it>
 *
 * 
 * mod_commands.c -- Misc. Command Module
 *
 */
#include <switch.h>
#include <switch_version.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_commands_load);
SWITCH_MODULE_DEFINITION(mod_commands, mod_commands_load, NULL, NULL);

SWITCH_STANDARD_API(time_test_function)
{
	switch_time_t now, then;
	int x;
	long mss = atol(cmd);
	uint32_t total = 0;
	int diff;
	int max = 10;
	char *p;
	
	if ((p = strchr(cmd, ' '))) {
		max = atoi(p+1);
		if (max < 0) {
			max = 10;
		}
	}

	for (x = 0; x < max; x++) {
		then = switch_time_now();
		switch_yield(mss);
		now = switch_time_now();
		diff = (int) now - then;
		stream->write_function(stream, "test %d sleep %ld %d\n", x+1, mss, diff);
		total += diff;
	}
	stream->write_function(stream, "avg %d\n", total / x);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(user_data_function)
{
	switch_xml_t x_domain, xml = NULL, x_user = NULL, x_param, x_params;
	int argc;
	char *mydata = NULL, *argv[3], *key = NULL, *type = NULL, *user, *domain;
	char delim = ' ';
	const char *container = "params", *elem = "param";
	switch_event_t *params = NULL;

	if (switch_strlen_zero(cmd) || !(mydata = strdup(cmd))) {
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
		domain = "cluecon.com";
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "user", user);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "mailbox", user);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "domain", domain);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "type", type);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "key", key);

	if (key && type && switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, params) == SWITCH_STATUS_SUCCESS) {
		if (!strcmp(type, "var")) {
			container = "variables";
			elem = "variable";
		}

		if ((x_params = switch_xml_child(x_user, container))) {
			for (x_param = switch_xml_child(x_params, elem); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");

				if (var && val && !strcasecmp(var, key)) {
					stream->write_function(stream, "%s", val);
					goto end;
				}

			}
		}
		
		if ((x_params = switch_xml_child(x_domain, container))) {
			for (x_param = switch_xml_child(x_params, elem); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");

				if (var && val && !strcasecmp(var, key)) {
					stream->write_function(stream, "%s", val);
					goto end;
				}
			}
		}
	}

  end:
	switch_xml_free(xml);
	free(mydata);
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

	if (switch_xml_locate_user(key, user, domain, NULL, &xml, &x_domain, &x_user, NULL) != SWITCH_STATUS_SUCCESS) {
		err = "can't find user";
		goto end;
	}

  end:
	if (session || tf) {
		stream->write_function(stream, err ? "false" : "true");
		switch_xml_free(xml);
	} else {
		if (err) {
			if (host) {
				stream->write_function(stream, "<error>%s</error>\n", err);
			} else {
				stream->write_function(stream, "-Error %s\n", err);
			}
		}

		if (xml && x_user) {
			xmlstr = switch_xml_toxml(x_user, SWITCH_FALSE);
			switch_assert(xmlstr);

			stream->write_function(stream, "%s", xmlstr);
			free(xmlstr);
			switch_xml_free(xml);
		}
	}

	free(mydata);
	return SWITCH_STATUS_SUCCESS;
}



SWITCH_STANDARD_API(md5_function)
{
	char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };

	if (switch_strlen_zero(cmd)) {
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

	if (!switch_strlen_zero(cmd)) {
		data = strdup(cmd);
		switch_url_decode(data);
		reply = data;
	}
	
	stream->write_function(stream, "%s", reply);

	switch_safe_free(data);
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(eval_function)
{
	char *expanded;
	switch_event_t *event;
	char uuid[80] = "";
	const char *p, *input = cmd;

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!strncasecmp(cmd, "uuid:", 5)) {
		p = cmd + 5;
		if ((input = strchr(p, ' ')) && *input++) {
			switch_copy_string(uuid, p, input - p);
		}
	}
	
	if (switch_strlen_zero(input)) {
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
	if (!switch_strlen_zero(cmd)) {
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
	
	if (!switch_strlen_zero(cmd)) {	
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
	int len = 0 ;

	if (!switch_strlen_zero(cmd)) {
		len = (strlen(cmd) * 3) + 1;
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

	if (argc < 4) {
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
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "tag", tag);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "tag_attr_name", tag_attr_name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "tag_attr_val", tag_attr_val);

	if (switch_xml_locate(section, tag, tag_attr_name, tag_attr_val, &xml, &obj, params) != SWITCH_STATUS_SUCCESS) {
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

	if (xml && obj) {
		xmlstr = switch_xml_toxml(obj, SWITCH_FALSE);
		switch_assert(xmlstr);

		stream->write_function(stream, "%s", xmlstr);
		free(xmlstr);
		switch_xml_free(xml);

	}

	switch_event_destroy(&params);
	free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reload_acl_function)
{
	const char *err;
	switch_xml_t xml_root;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (cmd && !strcmp(cmd, "reloadxml")) {
		if ((xml_root = switch_xml_open_root(1, &err))) {
			switch_xml_free(xml_root);
		}
	}

	switch_load_network_lists(SWITCH_TRUE);

	stream->write_function(stream, "+OK acl reloaded\n");
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

SWITCH_STANDARD_API(regex_function)
{
	switch_regex_t *re = NULL;
	int ovector[30];
	int argc;
	char *mydata = NULL, *argv[3];
	size_t len = 0;
	char *substituted = NULL;
	int proceed = 0;

	if (!cmd) {
		goto error;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, '|', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		goto error;
	}

	if ((proceed = switch_regex_perform(argv[0], argv[1], &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
		if (argc > 2) {
			len = strlen(argv[0]) * 3;
			substituted = malloc(len);
			switch_assert(substituted);
			memset(substituted, 0, len);
			switch_replace_char(argv[2], '%', '$', SWITCH_FALSE);
			switch_perform_substitution(re, proceed, argv[2], argv[0], substituted, len, ovector);

			stream->write_function(stream, "%s", substituted);
			free(substituted);
		} else {
			stream->write_function(stream, "true");
		}
	} else {
		stream->write_function(stream, "false");
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

	if (argc != 3) {
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
		s_a = switch_strip_spaces(a);
		s_b = switch_strip_spaces(b);
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
		stream->write_function(stream, "%s", is_true ? argv[1] : argv[2]);
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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	switch_core_measure_time(switch_core_uptime(), &duration);

	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
	}

	if ((var = switch_event_get_header(stream->param_event, "content-type"))) {
		if (!strcasecmp(var, "text/plain")) {
			http = NULL;
		}
	} else {
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

	stream->write_function(stream, "%" SWITCH_SIZE_T_FMT " session(s) since startup\n", switch_core_session_id() - 1);
	switch_core_session_ctl(SCSC_LAST_SPS, &last_sps);
	switch_core_session_ctl(SCSC_SPS, &sps);
	stream->write_function(stream, "%d session(s) %d/%d\n", switch_core_session_count(), last_sps, sps);

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

#define CTL_SYNTAX "[send_sighup|hupall|pause|resume|shutdown [cancel|elegant|asap|restart]|sps|sync_clock|reclaim_mem|max_sessions|max_dtmf_duration [num]|loglevel [level]]"
SWITCH_STANDARD_API(ctl_function)
{
	int argc;
	char *mydata, *argv[5];
	int32_t arg = 0;

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", CTL_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((mydata = strdup(cmd))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		if (!strcasecmp(argv[0], "hupall")) {
			arg = 1;
			switch_core_session_ctl(SCSC_HUPALL, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "pause")) {
			arg = 1;
			switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "send_sighup")) {
			arg = 1;
			switch_core_session_ctl(SCSC_SEND_SIGHUP, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "resume")) {
			arg = 0;
			switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
			stream->write_function(stream, "+OK\n");
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
		} else if (!strcasecmp(argv[0], "reclaim_mem")) {
			switch_core_session_ctl(SCSC_RECLAIM, &arg);
			stream->write_function(stream, "+OK\n");
		} else if (!strcasecmp(argv[0], "max_sessions")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_MAX_SESSIONS, &arg);
			stream->write_function(stream, "+OK max sessions: %d\n", arg);
		} else if (!strcasecmp(argv[0], "max_dtmf_duration")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_MAX_DTMF_DURATION, &arg);
			stream->write_function(stream, "+OK max dtmf duration: %d\n", arg);
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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", LOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR [%s]\n", err);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(unload_function)
{
	const char *err;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", LOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_loadable_module_unload_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR [%s]\n", err);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reload_function)
{
	const char *err;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", LOAD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_loadable_module_unload_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK module unloaded\n");
	} else {
		stream->write_function(stream, "-ERR unloading module [%s]\n", err);
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) cmd, SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK module loaded\n");
	} else {
		stream->write_function(stream, "-ERR loading module [%s]\n", err);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(reload_xml_function)
{
	const char *err;
	switch_xml_t xml_root;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if ((xml_root = switch_xml_open_root(1, &err))) {
		switch_xml_free(xml_root);
	}

	stream->write_function(stream, "+OK [%s]\n", err);

	return SWITCH_STATUS_SUCCESS;
}

#define KILL_SYNTAX "<uuid> [cause]"
SWITCH_STANDARD_API(kill_function)
{
	switch_core_session_t *ksession = NULL;
	char *mycmd = NULL, *kcause = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd) || !(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-USAGE: %s\n", KILL_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((kcause = strchr(mycmd, ' '))) {
		*kcause++ = '\0';
	}

	if (switch_strlen_zero(mycmd) || !(ksession = switch_core_session_locate(mycmd))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(ksession);
		if (!switch_strlen_zero(kcause)){
			cause = switch_channel_str2cause(kcause);
		}  
		switch_channel_hangup(channel, cause);
		switch_core_session_rwunlock(ksession);
		stream->write_function(stream, "+OK\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define PARK_SYNTAX "<uuid>"
SWITCH_STANDARD_API(park_function)
{
	switch_core_session_t *ksession = NULL;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd) || !(mycmd = strdup(cmd))) {
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

	if (switch_strlen_zero(tuuid) || !(tsession = switch_core_session_locate(tuuid))) {
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
				switch_channel_set_flag(other_channel, CF_TRANSFER);
				switch_channel_set_flag(channel, CF_TRANSFER);
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

#define TONE_DETECT_SYNTAX "<uuid> <key> <tone_spec> [<flags> <timeout> <app> <args>]"
SWITCH_STANDARD_API(tone_detect_session_function)
{
	char *argv[7] = { 0 };
	int argc;
	char *mydata = NULL;
	time_t to = 0;
	switch_core_session_t *rsession;

	if (!cmd) {
		stream->write_function(stream, "-USAGE: %s\n", TONE_DETECT_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	mydata = strdup(cmd);
	switch_assert(mydata != NULL);

	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "-ERR INVALID ARGS!\n");
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
				to = switch_timestamp(NULL) + mto;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				goto done;
			}
		} else {
			if ((to = atoi(argv[4])) < switch_timestamp(NULL)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				to = 0;
				goto done;
			}
		}
	}

	switch_ivr_tone_detect_session(rsession, argv[1], argv[2], argv[3], to, argv[5], argv[6]);
	stream->write_function(stream, "+OK Enabling tone detection '%s' '%s' '%s'\n", argv[1], argv[2], argv[3]);

  done:

	free(mydata);
	switch_core_session_rwunlock(rsession);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_function)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);
	stream->write_function(stream, "%s", uuid_str);
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_CHAT_SYNTAX "<uuid> <text>"
SWITCH_STANDARD_API(uuid_chat)
{
	switch_core_session_t *tsession = NULL;
	char *uuid = NULL, *text = NULL;

	if (!switch_strlen_zero(cmd) && (uuid = strdup(cmd))) {
		if ((text = strchr(uuid, ' '))) {
			*text++ = '\0';
		}
	}

	if (switch_strlen_zero(uuid) || switch_strlen_zero(text)) {
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

#define SCHED_TRANSFER_SYNTAX "[+]<time> <uuid> <extension> [<dialplan>] [<context>]"
SWITCH_STANDARD_API(sched_transfer_function)
{
	switch_core_session_t *tsession = NULL;
	char *mycmd = NULL, *argv[6] = { 0 };
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 2 || argc > 5 || switch_strlen_zero(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_TRANSFER_SYNTAX);
	} else {
		char *uuid = argv[1];
		char *dest = argv[2];
		char *dp = argv[3];
		char *context = argv[4];
		time_t when;

		if (*argv[0] == '+') {
			when = switch_timestamp(NULL) + atol(argv[0] + 1);
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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 1 || switch_strlen_zero(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_HANGUP_SYNTAX);
	} else {
		char *uuid = argv[1];
		char *cause_str = argv[2];
		time_t when;
		switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;

		if (*argv[0] == '+') {
			when = switch_timestamp(NULL) + atol(argv[0] + 1);
		} else {
			when = atol(argv[0]);
		}

		if (cause_str) {
			cause = switch_channel_str2cause(cause_str);
		}

		if ((hsession = switch_core_session_locate(uuid))) {
			switch_ivr_schedule_hangup(when, uuid, cause, SWITCH_FALSE);
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

	if (session) {
		return status;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 1 || switch_strlen_zero(argv[0])) {
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

#define BROADCAST_SYNTAX "<uuid> <path> [aleg|bleg|both]"
SWITCH_STANDARD_API(uuid_broadcast_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session) {
		return status;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 2) {
		stream->write_function(stream, "-USAGE: %s\n", BROADCAST_SYNTAX);
	} else {
		switch_media_flag_t flags = SMF_NONE;

		if (argv[2]) {
			if (!strcasecmp(argv[2], "both")) {
				flags |= (SMF_ECHO_ALEG | SMF_ECHO_BLEG);
			} else if (!strcasecmp(argv[2], "aleg")) {
				flags |= SMF_ECHO_ALEG;
			} else if (!strcasecmp(argv[2], "bleg")) {
				flags |= SMF_ECHO_BLEG;
			}
		} else {
			flags |= SMF_ECHO_ALEG;
		}

		status = switch_ivr_broadcast(argv[0], argv[1], flags);
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
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session) {
		return status;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 3 || switch_strlen_zero(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_BROADCAST_SYNTAX);
	} else {
		switch_media_flag_t flags = SMF_NONE;
		time_t when;

		if (*argv[0] == '+') {
			when = switch_timestamp(NULL) + atol(argv[0] + 1);
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

		status = switch_ivr_schedule_broadcast(when, argv[1], argv[2], flags);
		stream->write_function(stream, "+OK Message Scheduled\n");
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define HOLD_SYNTAX "<uuid> [<display>]"
SWITCH_STANDARD_API(uuid_hold_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session) {
		return status;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 1 || switch_strlen_zero(argv[0])) {
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

	if (session) {
		return status;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 2 || switch_strlen_zero(argv[0]) || switch_strlen_zero(argv[1])) {
		stream->write_function(stream, "-USAGE: %s\n", HOLD_SYNTAX);
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

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define UUID_SYNTAX "<uuid> <other_uuid>"
SWITCH_STANDARD_API(uuid_bridge_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 2) {
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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
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

	if (switch_strlen_zero(uuid) || switch_strlen_zero(action) || switch_strlen_zero(path)) {
		goto usage;
	}

	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(action, "start")) {
		if (switch_ivr_record_session(rsession, path, limit, NULL)!= SWITCH_STATUS_SUCCESS) {
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
	switch_safe_free(mycmd);

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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd) || !(mycmd = strdup(cmd))) {
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

	if (switch_strlen_zero(uuid) || switch_strlen_zero(action) || switch_strlen_zero(path)) {
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
	switch_safe_free(mycmd);

  done:
	if (rsession) {
		switch_core_session_rwunlock(rsession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define BREAK_SYNTAX "<uuid> [all]"
SWITCH_STANDARD_API(break_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd, *flag;
	
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", BREAK_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	mycmd = strdup(cmd);
	switch_assert(mycmd);

	if ((flag = strchr(mycmd, ' '))) {
		*flag++ = '\0';
	}

	if (!(psession = switch_core_session_locate(mycmd))) {
		stream->write_function(stream, "-ERR No Such Channel!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (flag && !strcasecmp(flag, "all")) {
		switch_core_session_flush_private_events(psession);
	}

	switch_channel_set_flag(switch_core_session_get_channel(psession), CF_BREAK);
	switch_core_session_rwunlock(psession);

	return SWITCH_STATUS_SUCCESS;
}

#define PAUSE_SYNTAX "<uuid> <on|off>"
SWITCH_STANDARD_API(pause_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (switch_strlen_zero(cmd) || argc < 2 || switch_strlen_zero(argv[0])) {
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

	if (session || switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE %s\n", ORIGINATE_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
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

	if (switch_ivr_originate(NULL, &caller_session, &cause, aleg, timeout, NULL, cid_name, cid_num, NULL, SOF_NONE) != SWITCH_STATUS_SUCCESS
		|| !caller_session) {
		if (machine) {
			stream->write_function(stream, "-ERR %s\n", switch_channel_cause2str(cause));
		} else {
			stream->write_function(stream, "-ERR Cannot Create Outgoing Channel! [%s] cause: %s\n", aleg, switch_channel_cause2str(cause));
		}
		goto done;
	}

	caller_channel = switch_core_session_get_channel(caller_session);
	switch_channel_clear_state_handler(caller_channel, NULL);

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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
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
				elen = (int) strlen(mystream.data) * 3;
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
		task->runtime = switch_timestamp(NULL) + api_task->recur;
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

#define SCHED_SYNTAX "[+@]<time> <group_name> <command_string>"
SWITCH_STANDARD_API(sched_api_function)
{
	char *tm = NULL, *dcmd, *group;
	time_t when;
	struct api_task *api_task = NULL;
	uint32_t recur = 0;
	
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
				when = switch_timestamp(NULL) + atol(tm + 1);
			} else if (*tm == '@') {
				recur = (uint32_t) atol(tm + 1);
				when = switch_timestamp(NULL) + recur;
			} else {
				when = atol(tm);
			}

			switch_zmalloc(api_task, sizeof(*api_task) + strlen(dcmd) + 1);
			switch_copy_string(api_task->cmd, dcmd, strlen(dcmd));
			api_task->recur = recur;

			id = switch_scheduler_add_task(when, sch_api_callback, (char *) __SWITCH_FUNC__, group, 0, api_task, SSHF_FREE_ARG);
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

	if (!job)
		return NULL;

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
	int rows;
	int justcount;
};

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

#define ALIAS_SYNTAX "add <alias> <command> | del [<alias>|*]"
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

#define SHOW_SYNTAX "codec|application|api|dialplan|file|timer|calls [count]|channels [count]|aliases|complete|chat|endpoint|management|say|interfaces|interface_types"
SWITCH_STANDARD_API(show_function)
{
	char sql[1024];
	char *errmsg;
	switch_core_db_t *db;
	struct holder holder = { 0 };
	int help = 0;
	char *mydata = NULL, *argv[6] = { 0 };
	int argc;
	char *command = NULL, *as = NULL;
	switch_core_flag_t cflags = switch_core_flags();
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	db = switch_core_db_handle();
	
	holder.justcount = 0;
	
	if (cmd && (mydata = strdup(cmd))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
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

	if (!(cflags & SCF_USE_SQL) && command && !strcasecmp(command, "channels")) {
		stream->write_function(stream, "-ERR SQL DISABLED NO CHANNEL DATA AVAILABLE!\n");
		goto end;
	}

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
			   !strncasecmp(command, "say", 3) || 
			   !strncasecmp(command, "management", 10) || 
			   !strncasecmp(command, "endpoint", 8)) {
		if (end_of(command) == 's') {
			end_of(command) = '\0';
		}
		sprintf(sql, "select type, name from interfaces where type = '%s' order by type,name", command);
	} else if (!strcasecmp(command, "interfaces")) {
		sprintf(sql, "select type, name from interfaces order by type,name");
	} else if (!strcasecmp(command, "interface_types")) {
		sprintf(sql, "select type,count(type) as total from interfaces group by type order by type");
	} else if (!strcasecmp(command, "tasks")) {
		sprintf(sql, "select * from %s", command);
	} else if (!strcasecmp(command, "application") || !strcasecmp(command, "api")) {
		sprintf(sql, "select name, description, syntax from interfaces where type = '%s' and description != '' order by type,name", command);
	} else if (!strcasecmp(command, "calls")) {
		sprintf(sql, "select * from calls order by created_epoch");
		if (argv[1] && !strcasecmp(argv[1],"count")) {
		    holder.justcount = 1;
		    if (argv[3] && !strcasecmp(argv[2], "as")) {
			as = argv[3];
		    }
		}
	} else if (!strcasecmp(command, "channels")) {
		sprintf(sql, "select * from channels order by created_epoch");
		if (argv[1] && !strcasecmp(argv[1],"count")) {
		    holder.justcount = 1;
		    if (argv[3] && !strcasecmp(argv[2], "as")) {
			as = argv[3];
		    }
		}
	} else if (!strcasecmp(command, "aliases")) {
		sprintf(sql, "select * from aliases order by alias");
	} else if (!strcasecmp(command, "complete")) {
		sprintf(sql, "select * from complete order by a1,a2,a3,a4,a5,a6,a7,a8,a9,a10");
	} else if (!strncasecmp(command, "help", 4)) {
		char *cmdname = NULL;

		help = 1;
		holder.print_title = 0;
		if ((cmdname = strchr(command, ' ')) != 0) {
			*cmdname++ = '\0';
			switch_snprintf(sql, sizeof(sql) - 1, "select name, syntax, description from interfaces where type = 'api' and name = '%s' order by name",
							cmdname);
		} else {
			switch_snprintf(sql, sizeof(sql) - 1, "select name, syntax, description from interfaces where type = 'api' order by name");
		}
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

	if (!strcasecmp(as, "delim") || !strcasecmp(as, "csv")) {
		if (switch_strlen_zero(holder.delim)) {
			if (!(holder.delim = argv[3])) {
				holder.delim = ",";
			}
		}
		switch_core_db_exec(db, sql, show_callback, &holder, &errmsg);
		if (holder.http) {
			holder.stream->write_function(holder.stream, "</table>");
		}

		if (errmsg) {
			stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
			switch_core_db_free(errmsg);
			errmsg = NULL;
		} else if (help) {
			if (holder.count == 0)
				stream->write_function(stream, "-ERR No such command.\n");
		} else {
			stream->write_function(stream, "\n%u total.\n", holder.count);
		}
	} else if (!strcasecmp(as, "xml")) {
		switch_core_db_exec(db, sql, show_as_xml_callback, &holder, &errmsg);

		if (errmsg) {
			stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
			switch_core_db_free(errmsg);
			errmsg = NULL;
		}

		if (holder.xml) {
			char count[50];
			char *xmlstr;
			switch_snprintf(count, sizeof(count), "%d", holder.count);

			switch_xml_set_attr(switch_xml_set_flag(holder.xml, SWITCH_XML_DUP), strdup("row_count"), strdup(count));
			xmlstr = switch_xml_toxml(holder.xml, SWITCH_FALSE);

			if (xmlstr) {
				holder.stream->write_function(holder.stream, "%s", xmlstr);
				free(xmlstr);
			} else {
				holder.stream->write_function(holder.stream, "<result row_count=\"0\"/>\n");
			}
		} else {
			holder.stream->write_function(holder.stream, "<result row_count=\"0\"/>\n");
		}
	} else {
		holder.stream->write_function(holder.stream, "-ERR Cannot find format %s\n", as);
	}

 end:

	switch_safe_free(mydata);

	if (db) {
		switch_core_db_close(db);
	}

	return status;
}

SWITCH_STANDARD_API(version_function)
{
	char version_string[1024];
	switch_snprintf(version_string, sizeof(version_string) - 1, "FreeSwitch Version %s\n", SWITCH_VERSION_FULL);

	stream->write_function(stream, version_string);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(help_function)
{
	char showcmd[1024];
	int all = 0;
	if (switch_strlen_zero(cmd)) {
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

#define HEARTBEAT_SYNTAX "<uuid> [sched] <on|off|<seconds>>"
SWITCH_STANDARD_API(uuid_session_heartbeat_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	uint32_t seconds = 60;
	int argc, tmp;
	switch_core_session_t *l_session = NULL;
	int x = 0, sched = 0;

	if (switch_strlen_zero(cmd) || !(mycmd = strdup(cmd))) {
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

	if (!switch_strlen_zero(cmd) && (fsession = switch_core_session_locate(cmd))) {
		switch_channel_flush_dtmf(switch_core_session_get_channel(fsession));
		switch_core_session_rwunlock(fsession);
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR no such session\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

#define SETVAR_SYNTAX "<uuid> <var> <value>"
SWITCH_STANDARD_API(uuid_setvar_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc == 3 && !switch_strlen_zero(argv[0])) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			char *var_value = argv[2];

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				channel = switch_core_session_get_channel(psession);

				if (switch_strlen_zero(var_name)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
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

#define GETVAR_SYNTAX "<uuid> <var>"
SWITCH_STANDARD_API(uuid_getvar_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 2 && !switch_strlen_zero(argv[0])) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			const char *var_value = NULL;

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				channel = switch_core_session_get_channel(psession);

				if (switch_strlen_zero(var_name)) {
					stream->write_function(stream, "-ERR No variable name specified!\n");
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
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

#define UUID_SEND_DTMF_SYNTAX "<uuid> <dtmf_data>"
SWITCH_STANDARD_API(uuid_send_dtmf_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[2] = { 0 };
	char *uuid = NULL, *dtmf_data = NULL;
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
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
	if (switch_strlen_zero(uuid) || switch_strlen_zero(dtmf_data)) {
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
	switch_safe_free(mycmd);

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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 0 && !switch_strlen_zero(argv[0])) {
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
						if ((xml = switch_event_xmlize(event, "%s", ""))) {
							buf = switch_xml_toxml(xml, SWITCH_FALSE);
							switch_xml_free(xml);
						} else {
							stream->write_function(stream, "-ERR Unable to create xml!\n");
							switch_event_destroy(&event);
							switch_core_session_rwunlock(psession);
							goto done;
						}
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

#define GLOBAL_SETVAR_SYNTAX "<var> <value>"
SWITCH_STANDARD_API(global_setvar_function)
{
	char *mycmd = NULL, *argv[2] = { 0 };
	int argc = 0;

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, '=', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc > 0 && !switch_strlen_zero(argv[0])) {
			char *var_name = argv[0];
			char *var_value = argv[1];

			if (switch_strlen_zero(var_value)) {
				var_value = NULL;
			}
			switch_core_set_variable(var_name, var_value);
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

	if (switch_strlen_zero(cmd)) {
		switch_core_dump_variables(stream);
	} else {
		stream->write_function(stream, "%s", switch_str_nil(switch_core_get_variable(cmd)));
	}
	return SWITCH_STATUS_SUCCESS;
}

#define SYSTEM_SYNTAX "<command>"
SWITCH_STANDARD_API(system_function)
{
    if (switch_strlen_zero(cmd)) {
        stream->write_function(stream, "-USAGE: %s\n", SYSTEM_SYNTAX);
        return SWITCH_STATUS_SUCCESS;
    } 

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Executing command: %s\n", cmd);
    if (switch_system(cmd, SWITCH_TRUE) < 0) {
       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Failed to execute command: %s\n", cmd);
    }
    stream->write_function(stream, "+OK\n");
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(strftime_tz_api_function)
{
	char *format = NULL;
	const char *tz_name = NULL;
	char date[80] = "";

	if (!switch_strlen_zero(cmd)) {
		format = strchr(cmd, ' ');
		tz_name = cmd;
		if (format) {
			*format++ = '\0';
		}
	}
	
	if (switch_strftime_tz(tz_name, format, date, sizeof(date), 0) == SWITCH_STATUS_SUCCESS) { /* The lookup of the zone may fail. */
		stream->write_function(stream, "%s", date);
	} else {
		stream->write_function(stream, "-ERR Invalid Timezone\n");
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(hupall_api_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;
	char *var = NULL;
	char *val = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_MANAGER_REQUEST;

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
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

	if (switch_strlen_zero(var)) {
		switch_core_session_hupall(cause);
	} else {
		switch_core_session_hupall_matching_var(var, val, cause);
	}
	
	if (switch_strlen_zero(var)) {
		stream->write_function(stream, "+OK hangup all channels with cause %s\n", switch_channel_cause2str(cause));
	} else {
		stream->write_function(stream, "+OK hangup all channels matching [%s]=[%s] with cause: %s\n", var, val, switch_channel_cause2str(cause));
	}
	
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_commands_load)
{
	switch_api_interface_t *commands_api_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "uuid_flush_dtmf", "Flush dtmf on a given uuid", uuid_flush_dtmf_function, "<uuid>");
	SWITCH_ADD_API(commands_api_interface, "md5", "md5", md5_function, "<data>");
	SWITCH_ADD_API(commands_api_interface, "hupall", "hupall", hupall_api_function, "<cause> [<var> <value>]");
	SWITCH_ADD_API(commands_api_interface, "strftime_tz", "strftime_tz", strftime_tz_api_function, "<Timezone_name> [format string]");
	SWITCH_ADD_API(commands_api_interface, "originate", "Originate a Call", originate_function, ORIGINATE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "tone_detect", "Start Tone Detection on a channel", tone_detect_session_function, TONE_DETECT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_kill", "Kill Channel", kill_function, KILL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_park", "Park Channel", park_function, PARK_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "reloadacl", "Reload ACL", reload_acl_function, "[reloadxml]");
	switch_console_set_complete("add reloadacl reloadxml");
	SWITCH_ADD_API(commands_api_interface, "reloadxml", "Reload XML", reload_xml_function, "");
	SWITCH_ADD_API(commands_api_interface, "unload", "Unload Module", unload_function, LOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "reload", "Reload Module", reload_function, LOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "load", "Load Module", load_function, LOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_transfer", "Transfer a session", transfer_function, TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "pause", "Pause", pause_function, PAUSE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "break", "Break", break_function, BREAK_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "show", "Show", show_function, SHOW_SYNTAX);
	switch_console_set_complete("add show channels");
	switch_console_set_complete("add show codec");
	switch_console_set_complete("add show application");
	switch_console_set_complete("add show api");
	switch_console_set_complete("add show dialplan");
	switch_console_set_complete("add show file");
	switch_console_set_complete("add show timer");
	switch_console_set_complete("add show calls");
	switch_console_set_complete("add show channels");
	switch_console_set_complete("add show aliases");
	switch_console_set_complete("add show complete");
	SWITCH_ADD_API(commands_api_interface, "complete", "Complete", complete_function, COMPLETE_SYNTAX);
	switch_console_set_complete("add complete add");
	switch_console_set_complete("add complete del");
	SWITCH_ADD_API(commands_api_interface, "alias", "Alias", alias_function, ALIAS_SYNTAX);
	switch_console_set_complete("add alias add");
	switch_console_set_complete("add alias del");
	SWITCH_ADD_API(commands_api_interface, "status", "status", status_function, "");
	SWITCH_ADD_API(commands_api_interface, "uuid_session_heartbeat", "uuid_session_heartbeat", uuid_session_heartbeat_function, HEARTBEAT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_bridge", "uuid_bridge", uuid_bridge_function, "");
	SWITCH_ADD_API(commands_api_interface, "uuid_setvar", "uuid_setvar", uuid_setvar_function, SETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_getvar", "uuid_getvar", uuid_getvar_function, GETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_dump", "uuid_dump", uuid_dump_function, DUMP_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "global_setvar", "global_setvar", global_setvar_function, GLOBAL_SETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "global_getvar", "global_getvar", global_getvar_function, GLOBAL_GETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_displace", "session displace", session_displace_function, "<uuid> [start|stop] <path> [<limit>] [mux]");
	SWITCH_ADD_API(commands_api_interface, "uuid_record", "session record", session_record_function, SESS_REC_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_broadcast", "broadcast", uuid_broadcast_function, BROADCAST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_hold", "hold", uuid_hold_function, HOLD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_display", "change display", uuid_display_function, DISPLAY_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_media", "media", uuid_media_function, MEDIA_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "fsctl", "control messages", ctl_function, CTL_SYNTAX);
	switch_console_set_complete("add fsctl hupall");
	switch_console_set_complete("add fsctl pause");
	switch_console_set_complete("add fsctl resume");
	switch_console_set_complete("add fsctl shutdown");
	switch_console_set_complete("add fsctl shutdown restart");
	switch_console_set_complete("add fsctl shutdown elegant");
	switch_console_set_complete("add fsctl shutdown asap");
	switch_console_set_complete("add fsctl shutdown elegant restart");
	switch_console_set_complete("add fsctl shutdown restart elegant");
	switch_console_set_complete("add fsctl shutdown asap restart");
	switch_console_set_complete("add fsctl shutdown restart asap");
	switch_console_set_complete("add fsctl shutdown cancel");
	switch_console_set_complete("add fsctl sps");
 	switch_console_set_complete("add fsctl sync_clock");
	switch_console_set_complete("add fsctl reclaim_mem");
	switch_console_set_complete("add fsctl max_sessions");
	switch_console_set_complete("add fsctl max_dtmf_duration");
	SWITCH_ADD_API(commands_api_interface, "help", "Show help for all the api commands", help_function, "");
	SWITCH_ADD_API(commands_api_interface, "version", "Show version of the switch", version_function, "");
	SWITCH_ADD_API(commands_api_interface, "sched_hangup", "Schedule a running call to hangup", sched_hangup_function, SCHED_HANGUP_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_broadcast", "Schedule a broadcast event to a running call", sched_broadcast_function,
				   SCHED_BROADCAST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_transfer", "Schedule a broadcast event to a running call", sched_transfer_function,
				   SCHED_TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "create_uuid", "Create a uuid", uuid_function, UUID_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_api", "Schedule an api command", sched_api_function, SCHED_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "unsched_api", "Unschedule an api command", unsched_api_function, UNSCHED_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "bgapi", "Execute an api command in a thread", bgapi_function, "<command>[ <arg>]");
	SWITCH_ADD_API(commands_api_interface, "sched_del", "Delete a Scheduled task", sched_del_function, "<task_id>|<group_id>");
	SWITCH_ADD_API(commands_api_interface, "xml_wrap", "Wrap another api command in xml", xml_wrap_api_function, "<command> <args>");
	SWITCH_ADD_API(commands_api_interface, "is_lan_addr", "see if an ip is a lan addr", lan_addr_function, "<ip>");
	SWITCH_ADD_API(commands_api_interface, "cond", "Eval a conditional", cond_function, "<expr> ? <true val> : <false val>");
	SWITCH_ADD_API(commands_api_interface, "regex", "Eval a regex", regex_function, "<data>|<pattern>[|<subst string>]");
	SWITCH_ADD_API(commands_api_interface, "acl", "compare an ip to an acl list", acl_function, "<ip> <list_name>");
	SWITCH_ADD_API(commands_api_interface, "uuid_chat", "Send a chat message", uuid_chat, UUID_CHAT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "find_user_xml", "find a user", find_user_function, "<key> <user> <domain>");
	SWITCH_ADD_API(commands_api_interface, "user_exists", "find a user", user_exists_function, "<key> <user> <domain>");
	SWITCH_ADD_API(commands_api_interface, "xml_locate", "find some xml", xml_locate_function, "[root | <section> <tag> <tag_attr_name> <tag_attr_val>]");
	SWITCH_ADD_API(commands_api_interface, "user_data", "find user data", user_data_function, "<user>@<domain> [var|param] <name>");
	SWITCH_ADD_API(commands_api_interface, "url_encode", "url encode a string", url_encode_function, "<string>");
	SWITCH_ADD_API(commands_api_interface, "url_decode", "url decode a string", url_decode_function, "<string>");
	SWITCH_ADD_API(commands_api_interface, "module_exists", "check if module exists", module_exists_function, "<module>");
	SWITCH_ADD_API(commands_api_interface, "domain_exists", "check if a domain exists", domain_exists_function, "<domain>");
	SWITCH_ADD_API(commands_api_interface, "uuid_send_dtmf", "send dtmf digits", uuid_send_dtmf_function, UUID_SEND_DTMF_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "eval", "eval (noop)", eval_function, "<expression>");
	SWITCH_ADD_API(commands_api_interface, "system", "Execute a system command", system_function, SYSTEM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "time_test", "time_test", time_test_function, "<mss>");

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
