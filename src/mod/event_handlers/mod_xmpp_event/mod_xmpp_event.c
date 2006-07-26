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
 *
 *
 * mod_xmpp_event.c -- XMPP Event Logger
 *
 */
#include <switch.h>
#include <iksemel.h>

static const char modname[] = "mod_xmpp_event";

static int RUNNING = 0;
static iksfilter *my_filter;
static int opt_timeout = 30;
static int opt_use_tls = 0;

/* stuff we keep per session */
struct session {
	iksparser *parser;
	iksid *acc;
	char *pass;
	int features;
	int authorized;
	int counter;
	int job_done;
};

static struct {
	char *jid;
	char *passwd;
	char *target_jid;
	int debug;
	struct session session;
} globals;

static void event_handler(switch_event_t *event)
{
	char buf[1024];
	iks *msg;
	int loops = 0;

	if (!RUNNING) {
		return;
	}

	while (!globals.session.authorized) {
		switch_yield(100000);
		if (loops++ > 5) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Nothing to do with this Event!\n");
			return;
		}
	}

	switch (event->event_id) {
	default:
		switch_event_serialize(event, buf, sizeof(buf), NULL);
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nEVENT\n--------------------------------\n%s\n", buf);
		msg = iks_make_msg(IKS_TYPE_NONE, globals.target_jid, buf);
		iks_insert_attrib(msg, "subject", "Event");
		iks_send(globals.session.parser, msg);
		iks_delete(msg);
		break;
	}
}



SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_jid, globals.jid)
	SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_target_jid, globals.target_jid)
	SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_passwd, globals.passwd)


	 static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *cf = "xmpp_event.conf";
	switch_xml_t cfg, xml, settings, param;
	int count = 0;

	memset(&globals, 0, sizeof(globals));

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "jid")) {
				set_global_jid(val);
				count++;
			} else if (!strcmp(var, "target-jid")) {
				set_global_target_jid(val);
				count++;
			} else if (!strcmp(var, "passwd")) {
				set_global_passwd(val);
				count++;
			} else if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			}
		}
	}

	switch_xml_free(xml);

	if (count == 3) {
		/* TBD use config to pick what events to bind to */
		if (switch_event_bind((char *) modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
			return SWITCH_STATUS_GENERR;
		}

		status = SWITCH_STATUS_SUCCESS;
	}

	return status;

}

static int on_result(struct session *sess, ikspak * pak)
{

	return IKS_FILTER_EAT;
}

static int on_stream(struct session *sess, int type, iks * node)
{
	sess->counter = opt_timeout;

	switch (type) {
	case IKS_NODE_START:
		if (opt_use_tls && !iks_is_secure(sess->parser)) {
			iks_start_tls(sess->parser);
		}
		break;
	case IKS_NODE_NORMAL:
		if (strcmp("stream:features", iks_name(node)) == 0) {
			sess->features = iks_stream_features(node);
			if (opt_use_tls && !iks_is_secure(sess->parser))
				break;
			if (sess->authorized) {
				iks *t;
				if (sess->features & IKS_STREAM_BIND) {
					t = iks_make_resource_bind(sess->acc);
					iks_send(sess->parser, t);
					iks_delete(t);
				}
				if (sess->features & IKS_STREAM_SESSION) {
					t = iks_make_session();
					iks_insert_attrib(t, "id", "auth");
					iks_send(sess->parser, t);
					iks_delete(t);
				}
			} else {
				if (sess->features & IKS_STREAM_SASL_MD5)
					iks_start_sasl(sess->parser, IKS_SASL_DIGEST_MD5, sess->acc->user, sess->pass);
				else if (sess->features & IKS_STREAM_SASL_PLAIN)
					iks_start_sasl(sess->parser, IKS_SASL_PLAIN, sess->acc->user, sess->pass);
			}
		} else if (strcmp("failure", iks_name(node)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "sasl authentication failed\n");
		} else if (strcmp("success", iks_name(node)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "server connected\n");
			sess->authorized = 1;
			iks_send_header(sess->parser, sess->acc->server);
		} else {
			ikspak *pak;

			pak = iks_packet(node);
			iks_filter_packet(my_filter, pak);
			if (sess->job_done == 1)
				return IKS_HOOK;
		}
		break;
#if 0
	case IKS_NODE_STOP:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "server disconnected\n");
		break;

	case IKS_NODE_ERROR:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "stream error\n");
		break;
#endif

	}

	if (node)
		iks_delete(node);
	return IKS_OK;
}

static int on_msg(void *user_data, ikspak * pak)
{
	char *cmd = iks_find_cdata(pak->x, "body");
	char *arg = NULL;
	switch_stream_handle_t stream = {0};
	char retbuf[2048] = "";
	char *p;

	if ((p = strchr(cmd, '\r')) != 0) {
		*p++ = '\0';
	} else if ((p = strchr(cmd, '\n')) != 0) {
		*p++ = '\0';
	}

	if ((arg = strchr(cmd, ' ')) != 0) {
		*arg++ = '\0';
	}

	stream.data = retbuf;
	stream.end = stream.data;
	stream.data_size = sizeof(retbuf);
	stream.write_function = switch_console_stream_write;
	switch_api_execute(cmd, arg, NULL, &stream);

	return 0;
}

static int on_error(void *user_data, ikspak * pak)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "authorization failed\n");
	return IKS_FILTER_EAT;
}

static void on_log(struct session *sess, const char *data, size_t size, int is_incoming)
{
	if (iks_is_secure(sess->parser))
		fprintf(stderr, "Sec");
	if (is_incoming)
		fprintf(stderr, "RECV");
	else
		fprintf(stderr, "SEND");
	fprintf(stderr, "[%s]\n", data);
}

static void j_setup_filter(struct session *sess)
{
	if (my_filter)
		iks_filter_delete(my_filter);
	my_filter = iks_filter_new();
	iks_filter_add_rule(my_filter, on_msg, 0,
						IKS_RULE_TYPE, IKS_PAK_MESSAGE,
						IKS_RULE_SUBTYPE, IKS_TYPE_CHAT, IKS_RULE_FROM, globals.target_jid, IKS_RULE_DONE);
	iks_filter_add_rule(my_filter, (iksFilterHook *) on_result, sess,
						IKS_RULE_TYPE, IKS_PAK_IQ,
						IKS_RULE_SUBTYPE, IKS_TYPE_RESULT, IKS_RULE_ID, "auth", IKS_RULE_DONE);
	iks_filter_add_rule(my_filter, on_error, sess,
						IKS_RULE_TYPE, IKS_PAK_IQ,
						IKS_RULE_SUBTYPE, IKS_TYPE_ERROR, IKS_RULE_ID, "auth", IKS_RULE_DONE);
}

static void xmpp_connect(char *jabber_id, char *pass)
{
	while (RUNNING == 1) {
		int e;

		memset(&globals.session, 0, sizeof(globals.session));
		globals.session.parser = iks_stream_new(IKS_NS_CLIENT, &globals.session, (iksStreamHook *) on_stream);
		if (globals.debug)
			iks_set_log_hook(globals.session.parser, (iksLogHook *) on_log);
		globals.session.acc = iks_id_new(iks_parser_stack(globals.session.parser), jabber_id);
		if (NULL == globals.session.acc->resource) {
			/* user gave no resource name, use the default */
			char tmp[512];
			sprintf(tmp, "%s@%s/%s", globals.session.acc->user, globals.session.acc->server, modname);
			globals.session.acc = iks_id_new(iks_parser_stack(globals.session.parser), tmp);
		}
		globals.session.pass = pass;

		j_setup_filter(&globals.session);

		e = iks_connect_tcp(globals.session.parser, globals.session.acc->server, IKS_JABBER_PORT);
		switch (e) {
		case IKS_OK:
			break;
		case IKS_NET_NODNS:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hostname lookup failed\n");
		case IKS_NET_NOCONN:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "connection failed\n");
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "io error %d\n", e);
			switch_sleep(5000000);
			continue;
		}

		globals.session.counter = opt_timeout;
		while (RUNNING == 1) {
			e = iks_recv(globals.session.parser, 1);

			if (globals.session.job_done) {
				break;
			}

			if (IKS_HOOK == e) {
				break;
			}

			if (IKS_OK != e) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "io error %d\n", e);
				switch_sleep(5000000);
				break;
			}

			if (!globals.session.authorized) {
				if (IKS_NET_TLSFAIL == e) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tls handshake failed\n");
					switch_sleep(5000000);
					break;
				}

				if (globals.session.counter == 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "network timeout\n");
					switch_sleep(5000000);
					break;
				}
			}
		}

		iks_disconnect(globals.session.parser);
		iks_parser_delete(globals.session.parser);
		globals.session.authorized = 0;
	}
	RUNNING = 0;

}

static switch_loadable_module_interface_t xmpp_event_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &xmpp_event_module_interface;

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{

	if (RUNNING) {
		RUNNING = -1;
		while (RUNNING) {
			switch_yield(1000);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	RUNNING = 1;
	xmpp_connect(globals.jid, globals.passwd);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "disconnecting client %d\n", RUNNING);
	return RUNNING ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_TERM;
}
