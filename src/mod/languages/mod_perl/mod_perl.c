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
 *
 *
 * mod_perl.c -- Perl
 *
 */

#ifdef __ICC
#pragma warning (disable:1419)
#endif
#ifdef _MSC_VER
#include <perlibs.h>
#pragma comment(lib, PERL_LIB)
#endif

#if defined (__SVR4) && defined (__sun)
#include <uconfig.h>
#endif

#include <EXTERN.h>
#include <perl.h>
#include <switch.h>
static char *embedding[] = { "", "-e", "" };
EXTERN_C void xs_init(pTHX);

SWITCH_MODULE_LOAD_FUNCTION(mod_perl_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_perl_shutdown);
SWITCH_MODULE_DEFINITION_EX(mod_perl, mod_perl_load, mod_perl_shutdown, NULL, SMODF_GLOBAL_SYMBOLS);

static STRLEN n_a;

static struct {
	PerlInterpreter *my_perl;
	switch_memory_pool_t *pool;
	char *xml_handler;
} globals;



static int Perl_safe_eval(PerlInterpreter * my_perl, const char *string)
{
	char *err = NULL;

	Perl_eval_pv(my_perl, string, FALSE);

	if ((err = SvPV(get_sv("@", TRUE), n_a)) && !zstr(err)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[%s]\n%s\n", string, err);
		return -1;
	}
	return 0;
}



static int perl_parse_and_execute(PerlInterpreter * my_perl, char *input_code, char *setup_code)
{
	int error = 0;

	if (zstr(input_code)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No code to execute!\n");
		return -1;
	}

	if (setup_code) {
		error = Perl_safe_eval(my_perl, setup_code);
		if (error) {
			return error;
		}
	}

	if (*input_code == '~') {
		char *buff = input_code + 1;
		error = Perl_safe_eval(my_perl, buff);
	} else {
		char *args = strchr(input_code, ' ');
		if (args) {
			char *code = NULL;
			int x, argc;
			char *argv[128] = { 0 };
			*args++ = '\0';

			if ((argc = switch_separate_string(args, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
				switch_stream_handle_t stream = { 0 };
				SWITCH_STANDARD_STREAM(stream);

				stream.write_function(&stream, " @ARGV = ( ");
				for (x = 0; x < argc; x++) {
					stream.write_function(&stream, "'%s'%s", argv[x], x == argc - 1 ? "" : ", ");
				}
				stream.write_function(&stream, " );");
				code = stream.data;
			} else {
				code = switch_mprintf("ARGV = ();");
			}

			if (code) {
				error = Perl_safe_eval(my_perl, code);
				switch_safe_free(code);
			}
		}
		if (!error) {
			char *file = input_code;
			char *err;

			if (!switch_is_file_path(file)) {
				file = switch_mprintf("require '%s/%s';", SWITCH_GLOBAL_dirs.script_dir, file);
				switch_assert(file);
			} else {
				file = switch_mprintf("require '%s';", file);
				switch_assert(file);
			}

			error = Perl_safe_eval(my_perl, file);
			switch_safe_free(file);
		}
	}

	return error;
}

#define HACK_CLEAN_CODE "eval{foreach my $kl(keys %main::) {eval{undef($$kl);} if (defined($$kl) && ($kl =~ /^\\w+[\\w\\d_]+$/))}}"

static void destroy_perl(PerlInterpreter ** to_destroy)
{
	Perl_safe_eval(*to_destroy, HACK_CLEAN_CODE);
	perl_destruct(*to_destroy);
	perl_free(*to_destroy);
	*to_destroy = NULL;
}

static PerlInterpreter *clone_perl(void)
{
	PerlInterpreter *my_perl = perl_clone(globals.my_perl, CLONEf_COPY_STACKS | CLONEf_KEEP_PTR_TABLE);
	PERL_SET_CONTEXT(my_perl);
	return my_perl;
}

#if 0
static perl_parse_and_execute(PerlInterpreter * my_perl, char *input_code, char *setup_code)
{
	int error = 0;

	if (*input_code == '~') {
		char *buff = input_code + 1;
		perl_parse(my_perl, xs_init, 3, embedding, NULL);
		if (setup_code)
			Perl_safe_eval(my_perl, setup_code);
		Perl_safe_eval(my_perl, buff);
	} else {
		int argc = 0;
		char *argv[128] = { 0 };
		char *err;
		argv[0] = "FreeSWITCH";
		argc++;

		argc += switch_separate_string(input_code, ' ', &argv[1], (sizeof(argv) / sizeof(argv[0])) - 1);
		if (!perl_parse(my_perl, xs_init, argc, argv, (char **) NULL)) {
			if (setup_code) {
				if (!Perl_safe_eval(my_perl, setup_code)) {
					perl_run(my_perl);
				}
			}
		}

		if ((err = SvPV(get_sv("@", TRUE), n_a)) && !zstr(err)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
		}


	}
}
#endif








static void perl_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	PerlInterpreter *my_perl = clone_perl();
	char code[1024] = "";

	perl_parse(my_perl, xs_init, 3, embedding, NULL);

	switch_snprintf(code, sizeof(code),
					"use lib '%s/perl';\n"
					"use freeswitch;\n"
					"$SWITCH_ENV{UUID} = \"%s\";\n" "$session = new freeswitch::Session(\"%s\")", SWITCH_GLOBAL_dirs.base_dir, uuid, uuid);

	perl_parse_and_execute(my_perl, data, code);
	destroy_perl(&my_perl);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_perl_shutdown)
{
	if (globals.my_perl) {
		perl_free(globals.my_perl);
		globals.my_perl = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unallocated perl interpreter.\n");
	}
	return SWITCH_STATUS_SUCCESS;
}

struct perl_o {
	switch_stream_handle_t *stream;
	switch_core_session_t *session;
	char *cmd;
	switch_event_t *message;
	int d;
};

static void *SWITCH_THREAD_FUNC perl_thread_run(switch_thread_t *thread, void *obj)
{
	PerlInterpreter *my_perl = clone_perl();
	char code[1024];
	SV *sv = NULL;
	char *uuid = NULL;
	struct perl_o *po = (struct perl_o *) obj;
	char *cmd = po->cmd;
	switch_stream_handle_t *stream = po->stream;
	switch_core_session_t *session = po->session;
	switch_event_t *message = po->message;

	if (session) {
		uuid = switch_core_session_get_uuid(session);
	}

	switch_snprintf(code, sizeof(code),
					"use lib '%s/perl';\n" "use freeswitch;\n" "$SWITCH_ENV{UUID} = \"%s\";\n", SWITCH_GLOBAL_dirs.base_dir, switch_str_nil(uuid)
		);

	perl_parse(my_perl, xs_init, 3, embedding, NULL);
	Perl_safe_eval(my_perl, code);

	if (uuid) {
		switch_snprintf(code, sizeof(code), "$session = new freeswitch::Session(\"%s\")", uuid);
		Perl_safe_eval(my_perl, code);
	}

	if (cmd) {
		if (stream) {
			mod_perl_conjure_stream(my_perl, stream, "stream");
			if (stream->param_event) {
				mod_perl_conjure_event(my_perl, stream->param_event, "env");
			}
		}

		if (message) {
			mod_perl_conjure_event(my_perl, message, "message");
		}

		//Perl_safe_eval(my_perl, cmd);
		perl_parse_and_execute(my_perl, cmd, NULL);
	}

	destroy_perl(&my_perl);

	switch_safe_free(cmd);

	if (po->d) {
		free(po);
	}

	return NULL;
}

int perl_thread(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	struct perl_o *po;

	po = malloc(sizeof(*po));
	memset(po, 0, sizeof(*po));
	po->cmd = strdup(text);
	po->d = 1;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, perl_thread_run, po, globals.pool);

	return 0;
}

SWITCH_STANDARD_API(perlrun_api_function)
{

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR Missing args.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	perl_thread(cmd);
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(perl_api_function)
{

	struct perl_o po = { 0 };

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR Missing args.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	po.cmd = strdup(cmd);
	po.stream = stream;
	po.session = session;
	perl_thread_run(NULL, &po);
}

static switch_xml_t perl_fetch(const char *section,
							   const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data)
{

	char *argv[128] = { 0 };
	int argc = 0;
	switch_xml_t xml = NULL;

	if (!zstr(globals.xml_handler)) {
		PerlInterpreter *my_perl = clone_perl();
		HV *hash;
		char *str;
		switch_event_header_t *hp;
		SV *this;
		char code[1024] = "";

		argv[argc++] = "FreeSWITCH";
		argv[argc++] = globals.xml_handler;

		PERL_SET_CONTEXT(my_perl);

		if (perl_parse(my_perl, xs_init, argc, argv, (char **) NULL)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result!\n");
			return NULL;
		}

		if (!(hash = get_hv("XML_REQUEST", TRUE))) {
			abort();
		}

		if (zstr(section)) {
			section = "";
		}

		this = newSV(strlen(section) + 1);
		sv_setpv(this, section);
		hv_store(hash, "section", 7, this, 0);

		if (zstr(tag_name)) {
			tag_name = "";
		}

		this = newSV(strlen(tag_name) + 1);
		sv_setpv(this, tag_name);
		hv_store(hash, "tag_name", 8, this, 0);

		if (zstr(key_name)) {
			key_name = "";
		}

		this = newSV(strlen(key_name) + 1);
		sv_setpv(this, key_name);
		hv_store(hash, "key_name", 8, this, 0);

		if (zstr(key_value)) {
			key_value = "";
		}

		this = newSV(strlen(key_value) + 1);
		sv_setpv(this, key_value);
		hv_store(hash, "key_value", 9, this, 0);

		if (!(hash = get_hv("XML_DATA", TRUE))) {
			abort();
		}

		if (params) {
			for (hp = params->headers; hp; hp = hp->next) {
				this = newSV(strlen(hp->value) + 1);
				sv_setpv(this, hp->value);
				hv_store(hash, hp->name, strlen(hp->name), this, 0);
			}
		}

		switch_snprintf(code, sizeof(code), "use lib '%s/perl';\n" "use freeswitch;\n", SWITCH_GLOBAL_dirs.base_dir);
		Perl_safe_eval(my_perl, code);

		if (params) {
			mod_perl_conjure_event(my_perl, params, "params");
		}

		perl_run(my_perl);
		str = SvPV(get_sv("XML_STRING", TRUE), n_a);

		if (!zstr(str)) {
			if (zstr(str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
			} else if (!(xml = switch_xml_parse_str(str, strlen(str)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
			}
		}

		destroy_perl(&my_perl);
	}

	return xml;
}


SWITCH_STANDARD_CHAT_APP(perl_chat_function)
{

	struct perl_o po = { 0 };

	if (zstr(data)) {
		return SWITCH_STATUS_FALSE;
	}

	po.cmd = strdup(data);
	po.stream = NULL;
	po.session = NULL;
	po.message = message;
	perl_thread_run(NULL, &po);

	return SWITCH_STATUS_SUCCESS;

}


static switch_status_t do_config(void)
{
	char *cf = "perl.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "xml-handler-script")) {
				globals.xml_handler = switch_core_strdup(globals.pool, val);
			} else if (!strcmp(var, "xml-handler-bindings")) {
				if (!zstr(globals.xml_handler)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, var);
					switch_xml_bind_search_function(perl_fetch, switch_xml_parse_section_string(val), NULL);
				}
			} else if (!strcmp(var, "startup-script")) {
				if (val) {
					perl_thread(val);
				}
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_perl_load)
{
	switch_application_interface_t *app_interface;
	PerlInterpreter *my_perl;
	char code[1024];
	switch_api_interface_t *api_interface;
	switch_chat_application_interface_t *chat_app_interface;

	globals.pool = pool;

	if (!(my_perl = perl_alloc())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate perl interpreter\n");
		return SWITCH_STATUS_MEMERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Allocated perl intrepreter.\n");

	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 3, embedding, NULL);
	perl_run(my_perl);
	globals.my_perl = my_perl;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "perl", NULL, NULL, perl_function, NULL, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "perlrun", "run a script", perlrun_api_function, "<script>");
	SWITCH_ADD_API(api_interface, "perl", "run a script", perl_api_function, "<script>");
	SWITCH_ADD_CHAT_APP(chat_app_interface, "perl", "execute a perl script", "execute a perl script", perl_chat_function, "<script>", SCAF_NONE);

	/* indicate that the module should continue to be loaded */

	do_config();

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
