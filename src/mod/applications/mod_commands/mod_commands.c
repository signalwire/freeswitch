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
 *
 * 
 * mod_commands.c -- Misc. Command Module
 *
 */
#include <switch.h>
#include <switch_version.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_commands_load);
SWITCH_MODULE_DEFINITION(mod_commands, mod_commands_load, NULL, NULL);


SWITCH_STANDARD_API(user_data_function)
{
	switch_xml_t x_domain, xml = NULL, x_user = NULL, x_param, x_params;	
	int argc;
    char *mydata = NULL, *argv[3];
	char *key = NULL, *type = NULL, *user, *domain;
	char delim = ' ';
	const char *err = NULL;
	const char *container = "params", *elem = "param";
	char *params = NULL;

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

	user = argv[0];
	type = argv[1];
	key = argv[2];

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	} else {
		domain = "cluecon.com";
	}
	
	params = switch_mprintf("user=%s&domain=%s&type=%s&key=%s", user, domain, type, key);

	if (switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, params) != SWITCH_STATUS_SUCCESS) {
		err = "can't find user";
		goto end;
	}


 end:

	if (xml) {
		if (err) {
			//stream->write_function(stream,  "-Error %s\n", err);
		} else {
			if (!strcmp(type, "var")) {
				container = "variables";
				elem = "variable";
			}

			if ((x_params = switch_xml_child(x_user, container))) {
				for (x_param = switch_xml_child(x_params, elem); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr(x_param, "name");
					const char *val = switch_xml_attr(x_param, "value");
				
					if (!strcasecmp(var, key)) {
						stream->write_function(stream, "%s", val);
						break;
					}
				
				}
			}
		}
		switch_xml_free(xml);
	}

	free(mydata);
	switch_safe_free(params);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_STANDARD_API(find_user_function)
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
	
	if (stream->event && (host = switch_event_get_header(stream->event, "http-host"))) {
		stream->write_function(stream,  "Content-Type: text/xml\r\n\r\n");
		if ((path_info = switch_event_get_header(stream->event, "http-path-info"))) {
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

	if (err) {
		if (host) {
			stream->write_function(stream,  "<error>%s</error>\n", err);
		} else {
			stream->write_function(stream,  "-Error %s\n", err);
		}
	}

	if (xml && x_user) {
		xmlstr = switch_xml_toxml(x_user, SWITCH_FALSE);
		switch_assert(xmlstr);

		stream->write_function(stream,  "%s", xmlstr);
		free(xmlstr);
		switch_xml_free(xml);
		
	}

	free(mydata);
	return SWITCH_STATUS_SUCCESS;

}


SWITCH_STANDARD_API(xml_locate_function)
{
	switch_xml_t xml = NULL, obj = NULL;
	int argc;
    char *mydata = NULL, *argv[4];
	char *section, *tag, *tag_attr_name, *tag_attr_val, *params = NULL;
	char *xmlstr;
	char *path_info, delim = ' ';
	char *host = NULL;
	const char *err = NULL;

	if (stream->event && (host = switch_event_get_header(stream->event, "http-host"))) {
		stream->write_function(stream,  "Content-Type: text/xml\r\n\r\n");
		if ((path_info = switch_event_get_header(stream->event, "http-path-info"))) {
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
	
	params = switch_mprintf("section=%s&tag=%s&tag_attr_name=%s&tag_attr_val=%s", section, tag, tag_attr_name, tag_attr_val);
	switch_assert(params);
	if (switch_xml_locate(section, tag, tag_attr_name, tag_attr_val, &xml, &obj, params) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream,  "can't find anything\n");
		goto end;
	}


 end:

	if (err) {
		if (host) {
			stream->write_function(stream,  "<error>%s</error>\n", err);
		} else {
			stream->write_function(stream,  "-Error %s\n", err);
		}
	}

	switch_safe_free(params);

	if (xml && obj) {
		xmlstr = switch_xml_toxml(obj, SWITCH_FALSE);
		switch_assert(xmlstr);

		stream->write_function(stream,  "%s", xmlstr);
		free(xmlstr);
		switch_xml_free(xml);
		
	}

	free(mydata);
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
			switch_replace_char(argv[2], '%','$', SWITCH_FALSE);
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
        if (*(expr+1) == '=') {
            *expr++ = '\0';
            o = O_GE;
        } else {
            o = O_GT;
        }
    } else if ((expr = strchr(a, '<'))) {
        if (*(expr+1) == '=') {
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
	stream->write_function(stream, "%s", switch_is_lan_addr(cmd) ? "yes" : "no");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(status_function)
{
	uint8_t html = 0;
	switch_core_time_duration_t duration = {0};
	char *http = NULL;
	int sps = 0, last_sps = 0;
	
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	switch_core_measure_time(switch_core_uptime(), &duration);

	if (stream->event) {
		http = switch_event_get_header(stream->event, "http-host");
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

	stream->write_function(stream, "%"SWITCH_SIZE_T_FMT" session(s) since startup\n", switch_core_session_id() - 1 );
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

#define CTL_SYNTAX "[hupall|pause|resume|shutdown]"
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
		} else if (!strcasecmp(argv[0], "pause")) {
			arg = 1;
			switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
		} else if (!strcasecmp(argv[0], "resume")) {
			arg = 0;
			switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
		} else if (!strcasecmp(argv[0], "shutdown")) {
			arg = 0;
			switch_core_session_ctl(SCSC_SHUTDOWN, &arg);
		} else if (!strcasecmp(argv[0], "reclaim_mem")) {
			switch_core_session_ctl(SCSC_RECLAIM, &arg);
		} else if (!strcasecmp(argv[0], "max_sessions")) {
			if (argc > 1) {
				arg = atoi(argv[1]);
			}
			switch_core_session_ctl(SCSC_MAX_SESSIONS, &arg);
			stream->write_function(stream, "+OK max sessions: %d\n", arg);
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

			if (arg == -1 || arg == SWITCH_LOG_INVALID) {
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
		} else {
			stream->write_function(stream, "-ERR INVALID COMMAND\nUSAGE: fsctl [hupall|pause|resume|shutdown]\n");
			goto end;
		}

		stream->write_function(stream, "+OK\n");
	  end:
		free(mydata);
	} else {
		stream->write_function(stream, "-ERR Memory error\n");
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

#define KILL_SYNTAX "<uuid>"
SWITCH_STANDARD_API(kill_function)
{
	switch_core_session_t *ksession = NULL;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!cmd) {
		stream->write_function(stream, "-USAGE: %s\n", KILL_SYNTAX);
	} else if ((ksession = switch_core_session_locate(cmd))) {
		switch_channel_t *channel = switch_core_session_get_channel(ksession);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
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

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 2 && argc <= 5) {
			char *tuuid = argv[0];
			char *dest = argv[1];
			char *dp = argv[2];
			char *context = argv[3];
			char *arg = NULL;

			if ((tsession = switch_core_session_locate(tuuid))) {

				if (*dest == '-') {
					arg = dest;
					dest = argv[2];
					dp = argv[3];
					context = argv[4];
				}

				if (arg) {
					switch_channel_t *channel = switch_core_session_get_channel(tsession);
					arg++;
					if (!strcasecmp(arg, "bleg")) {
						const char *uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE);
						if (uuid && (other_session = switch_core_session_locate(uuid))) {
							switch_core_session_t *tmp = tsession;
							tsession = other_session;
							other_session = NULL;
							switch_core_session_rwunlock(tmp);
						}
					} else if (!strcasecmp(arg, "both")) {
						const char *uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE);
						if (uuid && (other_session = switch_core_session_locate(uuid))) {
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
				
			} else {
				stream->write_function(stream, "-ERR No Such Channel!\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", TRANSFER_SYNTAX);

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

	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "-ERR INVALID ARGS!\n");
	}


	if (!(rsession = switch_core_session_locate(argv[0]))) {
		stream->write_function(stream, "-ERR Error Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}


	if (argv[4]) {
		uint32_t mto;
		if (*argv[4] == '+') {
			if ((mto = atoi(argv[4]+1)) > 0) {
				to = time(NULL) + mto;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				goto done;
			}
		} else {
			if ((to = atoi(argv[4])) < time(NULL)) {
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
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
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

	if (switch_strlen_zero(cmd) || argc < 2 || argc > 5) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_TRANSFER_SYNTAX);
	} else {
		char *uuid = argv[1];
		char *dest = argv[2];
		char *dp = argv[3];
		char *context = argv[4];
		time_t when;

		if (*argv[0] == '+') {
			when = time(NULL) + atol(argv[0] + 1);
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

	if (switch_strlen_zero(cmd) || argc < 1) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_HANGUP_SYNTAX);
	} else {
		char *uuid = argv[1];
		char *cause_str = argv[2];
		time_t when;
		switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;

		if (*argv[0] == '+') {
			when = time(NULL) + atol(argv[0] + 1);
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

#define MEDIA_SYNTAX "<uuid>"
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

	if (switch_strlen_zero(cmd) || argc < 1) {
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

	if (switch_strlen_zero(cmd) || argc < 3) {
		stream->write_function(stream, "-USAGE: %s\n", SCHED_BROADCAST_SYNTAX);
	} else {
		switch_media_flag_t flags = SMF_NONE;
		time_t when;

		if (*argv[0] == '+') {
			when = time(NULL) + atol(argv[0] + 1);
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

#define HOLD_SYNTAX "<uuid>"
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

	if (switch_strlen_zero(cmd) || argc < 1) {
		stream->write_function(stream, "-USAGE: %s\n", HOLD_SYNTAX);
	} else {
		if (!strcasecmp(argv[0], "off")) {
			status = switch_ivr_unhold_uuid(argv[1]);
		} else {
			status = switch_ivr_hold_uuid(argv[0]);
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

	if (switch_strlen_zero(cmd) || argc != 2) {
		stream->write_function(stream, "-USAGE: %s\n", UUID_SYNTAX);
	} else {
		if (switch_ivr_uuid_bridge(argv[0], argv[1]) != SWITCH_STATUS_SUCCESS) {
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
	
	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (switch_strlen_zero(action) || switch_strlen_zero(path)) {
		goto usage;
	}

	if (!strcasecmp(action, "start")) {
		switch_ivr_record_session(rsession, path, limit, NULL);
	} else if (!strcasecmp(action, "stop")) {
		switch_ivr_stop_record_session(rsession, path);
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
	flags = argv[4];

	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (switch_strlen_zero(action) || switch_strlen_zero(path)) {
		goto usage;
	}

	if (!strcasecmp(action, "start")) {
		switch_ivr_displace_session(rsession, path, limit, flags);
	} else if (!strcasecmp(action, "stop")) {
		switch_ivr_stop_displace_session(rsession, path);
	} else {
		goto usage;
	}

	goto done;

  usage:

	stream->write_function(stream, "-ERR INVALID SYNTAX\n");
	switch_safe_free(mycmd);


  done:

	if (rsession) {
		switch_core_session_rwunlock(rsession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define BREAK_SYNTAX "<uuid>"
SWITCH_STANDARD_API(break_function)
{
	switch_core_session_t *psession = NULL;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", BREAK_SYNTAX);
	} else {
		if ((psession = switch_core_session_locate(cmd))) {
			switch_channel_t *channel = switch_core_session_get_channel(psession);
			switch_channel_set_flag(channel, CF_BREAK);
			switch_core_session_rwunlock(psession);
		} else {
			stream->write_function(stream, "-ERR No Such Channel!\n");
		}
	}

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

	if (switch_strlen_zero(cmd) || argc < 2) {
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
		stream->write_function(stream, "-ERR Illegal Usage\n");
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

	if (switch_ivr_originate(NULL, &caller_session, &cause, aleg, timeout, NULL, cid_name, cid_num, NULL) != SWITCH_STATUS_SUCCESS) {
		if (machine) {
			stream->write_function(stream, "-ERR %s\n", switch_channel_cause2str(cause));
		} else {
			stream->write_function(stream, "-ERR Cannot Create Outgoing Channel! [%s] cause: %s\n", aleg, switch_channel_cause2str(cause));
		}
		goto done;
	}

	caller_channel = switch_core_session_get_channel(caller_session);
	switch_assert(caller_channel != NULL);
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
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

	if (caller_session) {
		switch_core_session_rwunlock(caller_session);
	}

 done:
	switch_safe_free(mycmd);
	return status;
}

static void sch_api_callback(switch_scheduler_task_t *task)
{
	char *cmd, *arg = NULL;
	switch_stream_handle_t stream = { 0 };

	switch_assert(task);

	cmd = (char *) task->cmd_arg;

	if ((arg = strchr(cmd, ' '))) {
		*arg++ = '\0';
	}

	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(cmd, arg, NULL, &stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Command %s(%s):\n%s\n", cmd, arg, switch_str_nil((char *) stream.data));
	switch_safe_free(stream.data);
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
			cnt = switch_scheduler_del_task_id((uint32_t)tmp);
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

		stream->write_function(stream, 
							   "<result>\n"
							   "  <row id=\"1\">\n"
							   "    <data>%s</data>\n"
							   "  </row>\n"
							   "</result>\n",
							   send ? send : "ERROR"
							   );
		switch_safe_free(mystream.data);
		switch_safe_free(edata);
		free(dcommand);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(sched_api_function)
{
	char *tm = NULL, *dcmd, *group;
	time_t when;

	if (!cmd) {
		stream->write_function(stream, "-ERR Invalid syntax\n");
		return SWITCH_STATUS_SUCCESS;
	}
	tm = strdup(cmd);
	switch_assert(tm != NULL);

	if ((group = strchr(tm, ' '))) {
		uint32_t id;

		*group++ = '\0';

		if ((dcmd = strchr(group, ' '))) {
			*dcmd++ = '\0';

			if (*tm == '+') {
				when = time(NULL) + atol(tm + 1);
			} else {
				when = atol(tm);
			}
		
			id = switch_scheduler_add_task(when, sch_api_callback, (char *) __SWITCH_FUNC__, group, 0, strdup(dcmd), SSHF_FREE_ARG);
			stream->write_function(stream, "+OK Added: %u\n", id);
			goto good;
		} 
	}

	stream->write_function(stream, "-ERR Invalid syntax\n");

 good:

	switch_safe_free(tm);

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

	if (!(row = switch_xml_add_child_d(holder->xml, "row", holder->rows++))) {
		return -1;
	}

	snprintf(id, sizeof(id), "%d", holder->rows);

	switch_xml_set_attr(switch_xml_set_flag(row, SWITCH_XML_DUP), strdup("row_id"), strdup(id));

	for(x = 0; x < argc; x++) {
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

			switch_amp_encode(argv[x], aval, sizeof(aval));
			holder->stream->write_function(holder->stream, "<td>");
			holder->stream->write_function(holder->stream, "%s%s", aval, x == (argc - 1) ? "</td></tr>\n" : "</td><td>");
		} else {
			holder->stream->write_function(holder->stream, "%s%s", val, x == (argc - 1) ? "\n" : holder->delim);
		}
	}

	holder->count++;
	return 0;
}

#define SHOW_SYNTAX "codec|application|api|dialplan|file|timer|calls|channels"
SWITCH_STANDARD_API(show_function)
{
	char sql[1024];
	char *errmsg;
	switch_core_db_t *db = switch_core_db_handle();
	struct holder holder = { 0 };
	int help = 0;
	char *mydata = NULL, *argv[6] = {0};
	int argc;
	char *command = NULL, *as = NULL;
	switch_core_flag_t cflags = switch_core_flags();

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (cmd && (mydata = strdup(cmd))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		command = argv[0];
		if (argv[2] && !strcasecmp(argv[1], "as")) {
			as = argv[2];
		}
	}
	
	if (stream->event) {
		holder.http = switch_event_get_header(stream->event, "http-host");
	}

	holder.print_title = 1;

	if (!(cflags & SCF_USE_SQL) && command && !strcasecmp(command, "channels")) {
		stream->write_function(stream, "-ERR SQL DISABLED NO CHANNEL DATA AVAILABLE!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	// If you changes the field qty or order of any of these select
	// statmements, you must also change show_callback and friends to match!
	if (!command) {
		stream->write_function(stream, "-USAGE: %s\n", SHOW_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	} else if (!strcasecmp(command, "codec") || !strcasecmp(command, "dialplan") || !strcasecmp(command, "file") || !strcasecmp(command, "timer")) {
		sprintf(sql, "select type, name from interfaces where type = '%s'", command);
	} else if (!strcasecmp(command, "tasks")) {
		sprintf(sql, "select * from %s", command);
	} else if (!strcasecmp(command, "application") || !strcasecmp(command, "api")) {
		sprintf(sql, "select name, description, syntax from interfaces where type = '%s' and description != ''", command);
	} else if (!strcasecmp(command, "calls")) {
		sprintf(sql, "select * from calls");
	} else if (!strcasecmp(command, "channels")) {
		sprintf(sql, "select * from channels");
	} else if (!strncasecmp(command, "help", 4)) {
		char *cmdname = NULL;

		help = 1;
		holder.print_title = 0;
		if ((cmdname = strchr(command, ' ')) != 0) {
			*cmdname++ = '\0';
			snprintf(sql, sizeof(sql) - 1, "select name, syntax, description from interfaces where type = 'api' and name = '%s'", cmdname);
		} else {
			snprintf(sql, sizeof(sql) - 1, "select name, syntax, description from interfaces where type = 'api'");
		}
	} else {
		stream->write_function(stream, "-USAGE: %s\n", SHOW_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
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

		if (holder.xml) {
			char count[50];
			char *xmlstr;
			snprintf(count, sizeof(count), "%d", holder.count);

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

	switch_safe_free(mydata);
	switch_core_db_close(db);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(version_function)
{
	char version_string[1024];
	snprintf(version_string, sizeof(version_string) - 1, "FreeSwitch Version %s\n", SWITCH_VERSION_FULL);

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
		snprintf(showcmd, sizeof(showcmd) - 1, "help %s", cmd);
	}

	if (all) {
		stream->write_function(stream, "\nValid Commands:\n\n");
	}

	show_function(showcmd, session, stream);

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
		if (argc == 3) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			char *var_value = argv[2];

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				channel = switch_core_session_get_channel(psession);

				switch_assert(channel != NULL);

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
		if (argc >= 2) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			const char *var_value = NULL;

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				channel = switch_core_session_get_channel(psession);
				
				switch_assert(channel != NULL);

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



#define GLOBAL_SETVAR_SYNTAX "<var> <value>"
SWITCH_STANDARD_API(global_setvar_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (!switch_strlen_zero(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, '=', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc > 0) {
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

	if (!switch_strlen_zero(cmd)) {
		stream->write_function(stream, "%s", switch_str_nil(switch_core_get_variable(cmd)));
		goto done;
	}

	stream->write_function(stream, "-USAGE: %s\n", GLOBAL_GETVAR_SYNTAX);
 done:
	return SWITCH_STATUS_SUCCESS;
}
 
SWITCH_MODULE_LOAD_FUNCTION(mod_commands_load)
{
	switch_api_interface_t *commands_api_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "originate", "Originate a Call", originate_function, ORIGINATE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "tone_detect", "Start Tone Detection on a channel", tone_detect_session_function, TONE_DETECT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "killchan", "Kill Channel", kill_function, KILL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "reloadxml", "Reload XML", reload_function, "");
	SWITCH_ADD_API(commands_api_interface, "unload", "Unload Module", unload_function, LOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "load", "Load Module", load_function, LOAD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "transfer", "Transfer Module", transfer_function, TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "pause", "Pause", pause_function, PAUSE_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "break", "Break", break_function, BREAK_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "show", "Show", show_function, SHOW_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "status", "status", status_function, "");
	SWITCH_ADD_API(commands_api_interface, "uuid_bridge", "uuid_bridge", uuid_bridge_function, "");
	SWITCH_ADD_API(commands_api_interface, "uuid_setvar", "uuid_setvar", uuid_setvar_function, SETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "uuid_getvar", "uuid_getvar", uuid_getvar_function, GETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "global_setvar", "global_setvar", global_setvar_function, GLOBAL_SETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "global_getvar", "global_getvar", global_getvar_function, GLOBAL_GETVAR_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "session_displace", "session displace", session_displace_function, "<uuid> [start|stop] <path> [<limit>] [mux]");
	SWITCH_ADD_API(commands_api_interface, "session_record", "session record", session_record_function, SESS_REC_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "broadcast", "broadcast", uuid_broadcast_function, BROADCAST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "hold", "hold", uuid_hold_function, HOLD_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "media", "media", uuid_media_function, MEDIA_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "fsctl", "control messages", ctl_function, CTL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "help", "Show help for all the api commands", help_function, "");
	SWITCH_ADD_API(commands_api_interface, "version", "Show version of the switch", version_function, "");
	SWITCH_ADD_API(commands_api_interface, "sched_hangup", "Schedule a running call to hangup", sched_hangup_function, SCHED_HANGUP_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_broadcast", "Schedule a broadcast event to a running call", sched_broadcast_function, SCHED_BROADCAST_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_transfer", "Schedule a broadcast event to a running call", sched_transfer_function, SCHED_TRANSFER_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "create_uuid", "Create a uuid", uuid_function, UUID_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "sched_api", "Schedule an api command", sched_api_function, "[+]<time> <group_name> <command_string>");
	SWITCH_ADD_API(commands_api_interface, "sched_del", "Delete a Scheduled task", sched_del_function, "<task_id>|<group_id>");
	SWITCH_ADD_API(commands_api_interface, "xml_wrap", "Wrap another api command in xml", xml_wrap_api_function, "<command> <args>");
	SWITCH_ADD_API(commands_api_interface, "is_lan_addr", "see if an ip is a lan addr", lan_addr_function, "<ip>");
	SWITCH_ADD_API(commands_api_interface, "cond", "Eval a conditional", cond_function, "<expr> ? <true val> : <false val>");
	// remove me before final release
	SWITCH_ADD_API(commands_api_interface, "qq", "Eval a conditional", cond_function, "<expr> ? <true val> : <false val>");
	SWITCH_ADD_API(commands_api_interface, "regex", "Eval a regex", regex_function, "<data>|<pattern>[|<subst string>]");
	SWITCH_ADD_API(commands_api_interface, "uuid_chat", "Send a chat message", uuid_chat, UUID_CHAT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "find_user_xml", "find a user", find_user_function, "<key> <user>@<domain>");
	SWITCH_ADD_API(commands_api_interface, "xml_locate", "find some xml", xml_locate_function, "[root | <section> <tag> <tag_attr_name> <tag_attr_val>]");
	SWITCH_ADD_API(commands_api_interface, "user_data", "find user data", user_data_function, "<user>@<domain> [var|param] <name>");

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
