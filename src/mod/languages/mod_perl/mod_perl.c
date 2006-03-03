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
#include <switch.h>
#include <EXTERN.h>
#include <perl.h>
static char *embedding[] = { "", "-e", "" };
EXTERN_C void xs_init(pTHX);

static const char modname[] = "mod_perl";

static struct {
	PerlInterpreter *my_perl;
} globals;


static void destroy_perl(PerlInterpreter **to_destroy)
{
	perl_destruct(*to_destroy);
	perl_free(*to_destroy);
	*to_destroy = NULL;
}

static PerlInterpreter *clone_perl(void)
{
	return perl_clone(globals.my_perl, CLONEf_COPY_STACKS|CLONEf_KEEP_PTR_TABLE);
}


static void perl_function(switch_core_session *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	char code[1024];
	PerlInterpreter *my_perl = clone_perl();
	sprintf(code, "package fs_perl;\n"
			"$SWITCH_ENV{UUID} = \"%s\";\n"
			"chdir(\"%s/perl\");\n",
			uuid, SWITCH_GLOBAL_dirs.base_dir);

	Perl_eval_pv(my_perl, code, TRUE);


	Perl_eval_pv(my_perl, data, TRUE);
	destroy_perl(&my_perl);
}

static const switch_application_interface perl_application_interface = {
	/*.interface_name */ "perl",
	/*.application_function */ perl_function
};

static switch_loadable_module_interface perl_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &perl_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void) 
{
	if (globals.my_perl) {
		perl_destruct(globals.my_perl);
		perl_free(globals.my_perl);
		globals.my_perl = NULL;
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Unallocated perl interpreter.\n");
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	PerlInterpreter *my_perl;
	char code[1024];
	
	if (!(my_perl = perl_alloc())) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate perl intrepreter\n");
		switch_core_destroy();
		return SWITCH_STATUS_MEMERR;
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Allocated perl intrepreter.\n");

	PERL_SET_CONTEXT(my_perl);
	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 3, embedding, NULL);
	perl_run(my_perl);
	globals.my_perl = my_perl;
	sprintf(code, "use lib '%s/perl';use fs_perl;use freeswitch\n", SWITCH_GLOBAL_dirs.base_dir);
    Perl_eval_pv(my_perl, code, TRUE);


	/* connect my internal structure to the blank pointer passed to me */
	*interface = &perl_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
