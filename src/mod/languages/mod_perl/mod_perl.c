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

#include <EXTERN.h>
#include <perl.h>
#include <switch.h>
static char *embedding[] = { "", "-e", "" };
EXTERN_C void xs_init(pTHX);

SWITCH_MODULE_LOAD_FUNCTION(mod_perl_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_perl_shutdown);
SWITCH_MODULE_DEFINITION(mod_perl, mod_perl_load, mod_perl_shutdown, NULL);

static STRLEN n_a;

static struct {
	PerlInterpreter *my_perl;
	switch_memory_pool_t *pool;
	char *xml_handler;
} globals;

static int Perl_safe_eval(PerlInterpreter *my_perl, const char *string, int tf)
{
	char *err = NULL;

	Perl_eval_pv(my_perl, string, FALSE);

	if ((err = SvPV(get_sv("@", TRUE), n_a)) && !switch_strlen_zero(err)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
		return -1;
	}
	return 0;
}

static void destroy_perl(PerlInterpreter ** to_destroy)
{
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

static perl_parse_and_execute (PerlInterpreter *my_perl, char *input_code, char *setup_code)
{
	int error = 0;

	if (*input_code == '~') {
		char *buff = input_code + 1;
		perl_parse(my_perl, xs_init, 3, embedding, NULL);
		if (setup_code) Perl_safe_eval(my_perl, setup_code, FALSE);
		Perl_safe_eval(my_perl, buff, TRUE);
	} else {
		int argc = 0;
		char *argv[128] = { 0 };
		char *err;
		argv[0] = "FreeSWITCH";
		argc++;
		
		argc += switch_separate_string(input_code, ' ', &argv[1], (sizeof(argv) / sizeof(argv[0])) - 1);
		if (!perl_parse(my_perl, xs_init, argc, argv, (char **)NULL)) {
			if (setup_code) {
				if (!Perl_safe_eval(my_perl, setup_code, FALSE)) {
					perl_run(my_perl);
				}
			}
		}
		
		if ((err = SvPV(get_sv("@", TRUE), n_a)) && !switch_strlen_zero(err)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
		}
		

	}
}

static void perl_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	PerlInterpreter *my_perl = clone_perl();
	
	char code[1024];
	switch_snprintf(code, sizeof(code), 
			"use lib '%s/perl';\n"
			"use freeswitch;\n"
			"$SWITCH_ENV{UUID} = \"%s\";\n"
			"$session = new freeswitch::Session(\"%s\")"
			, 
			SWITCH_GLOBAL_dirs.base_dir,
			uuid,
			uuid);

	perl_parse_and_execute(my_perl, data, code);
	Perl_safe_eval(my_perl, "undef $session;", FALSE);
	Perl_safe_eval(my_perl, "undef (*);", FALSE);
	destroy_perl(&my_perl);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_perl_shutdown)
{
	if (globals.my_perl) {
		perl_destruct(globals.my_perl);
		perl_free(globals.my_perl);
		globals.my_perl = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unallocated perl interpreter.\n");
	}
	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC perl_thread_run(switch_thread_t *thread, void *obj)
{
	char *input_code = (char *) obj;
	PerlInterpreter *my_perl = clone_perl();
	char code[1024];

	switch_snprintf(code, sizeof(code), 
			"use lib '%s/perl';\n"
			"use freeswitch;\n"
			, 
			SWITCH_GLOBAL_dirs.base_dir 
			);

	perl_parse_and_execute(my_perl, input_code, code);
	
	if (input_code) {
		free(input_code);
	}

	Perl_safe_eval(my_perl, "undef(*);", FALSE);
	destroy_perl(&my_perl);

	return NULL;
}

int perl_thread(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, perl_thread_run, strdup(text), globals.pool);

	return 0;
}

SWITCH_STANDARD_API(perlrun_api_function) {
	perl_thread(cmd);
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(perl_api_function) {

	PerlInterpreter *my_perl = clone_perl();
	char code[1024];
	SV *sv = NULL;
	char *uuid = NULL;

	if (session) {
		uuid = switch_core_session_get_uuid(session);
	}

	switch_snprintf(code, sizeof(code), 
			"use lib '%s/perl';\n"
			"use freeswitch;\n"			
			"$SWITCH_ENV{UUID} = \"%s\";\n"
			"use IO::String;\n"
			"$handle = IO::String->new($__OUT);\n"
			"select($handle);"
			,

			SWITCH_GLOBAL_dirs.base_dir,
			switch_str_nil(uuid)
			);

	perl_parse(my_perl, xs_init, 3, embedding, NULL);
	Perl_safe_eval(my_perl, code, FALSE);

	if (uuid) {
		switch_snprintf(code, sizeof(code), "$session = new freeswitch::Session(\"%s\")", uuid);
		Perl_safe_eval(my_perl, code, FALSE);
	}

	if (cmd) {
		Perl_safe_eval(my_perl, cmd, FALSE);
	}

	stream->write_function(stream, "%s", switch_str_nil(SvPV(get_sv("__OUT", FALSE), n_a)));

	if (uuid) {
		switch_snprintf(code, sizeof(code), "undef $session;", uuid);
		Perl_safe_eval(my_perl, code, FALSE);
	}

	Perl_safe_eval(my_perl, "undef(*);", FALSE);
	destroy_perl(&my_perl);

	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_t perl_fetch(const char *section, 
							   const char *tag_name, 
							   const char *key_name, 
							   const char *key_value, 
							   switch_event_t *params,
							   void *user_data)
{

	char *argv[128] = { 0 };
	int argc = 0;
	switch_xml_t xml = NULL;

	if (!switch_strlen_zero(globals.xml_handler)) {
		PerlInterpreter *my_perl = clone_perl();
		HV *hash;
		char *str;
		switch_event_header_t *hp;
		SV *this;
		char code[1024] = "";

		argv[argc++] = "FreeSWITCH";
		argv[argc++] = globals.xml_handler;
		
		PERL_SET_CONTEXT(my_perl);
		
		if (perl_parse(my_perl, xs_init, argc, argv, (char **)NULL)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result!\n");
			return NULL;
		}

		if (!(hash = get_hv("XML_REQUEST", TRUE))) {
			abort();
		}

		if (switch_strlen_zero(section)) {
			section = "";
		}

		this = newSV(strlen(section)+1);
		sv_setpv(this, section);
		hv_store(hash, "section", 7, this, 0);
		
		if (switch_strlen_zero(tag_name)) {
			tag_name = "";
		}

		this = newSV(strlen(tag_name)+1);
		sv_setpv(this, tag_name);
		hv_store(hash, "tag_name", 8, this, 0);

		if (switch_strlen_zero(key_name)) {
			key_name = "";
		}

		this = newSV(strlen(key_name)+1);
		sv_setpv(this, key_name);
		hv_store(hash, "key_name", 8, this, 0);

		if (switch_strlen_zero(key_value)) {
			key_value = "";
		}

		this = newSV(strlen(key_value)+1);
		sv_setpv(this, key_value);
		hv_store(hash, "key_value", 9, this, 0);
		
		if (!(hash = get_hv("XML_DATA", TRUE))) {
			abort();
		}

		if (params) {
			for (hp = params->headers; hp; hp = hp->next) {
				this = newSV(strlen(hp->value)+1);
				sv_setpv(this, hp->value);
				hv_store(hash, hp->name, strlen(hp->name), this, 0);
			}
		}

		switch_snprintf(code, sizeof(code), 
						"use lib '%s/perl';\n"
						"use freeswitch;\n"			
						,
						SWITCH_GLOBAL_dirs.base_dir
						);
		Perl_safe_eval(my_perl, code, FALSE);

		perl_run(my_perl);
		str = SvPV(get_sv("XML_STRING", FALSE), n_a);

		if (str) {
			if (switch_strlen_zero(str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
			} else if (!(xml = switch_xml_parse_str(str, strlen(str)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
			}
		}
		
		destroy_perl(&my_perl);
	}

	return xml;
}

static switch_status_t do_config(void)
{
	char *cf = "perl.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "xml-handler-script")) {
				globals.xml_handler = switch_core_strdup(globals.pool, val);
			} else if (!strcmp(var, "xml-handler-bindings")) {
				if (!switch_strlen_zero(globals.xml_handler)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, var);
					switch_xml_bind_search_function(perl_fetch, switch_xml_parse_section_string(val), NULL);
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

	globals.pool = pool;

	if (!(my_perl = perl_alloc())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate perl intrepreter\n");
		return SWITCH_STATUS_MEMERR;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Allocated perl intrepreter.\n");

	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 3, embedding, NULL);
	perl_run(my_perl);
	globals.my_perl = my_perl;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "perl", NULL, NULL, perl_function, NULL, SAF_NONE);
	SWITCH_ADD_API(api_interface, "perlrun", "run a script", perlrun_api_function, "<script>");
	SWITCH_ADD_API(api_interface, "perl", "run a script", perl_api_function, "<script>");
	/* indicate that the module should continue to be loaded */

	do_config();

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
