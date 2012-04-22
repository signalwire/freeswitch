#include <switch.h>
#include "freeswitch_perl.h"
#include "mod_perl_extra.h"

static STRLEN n_a;

#define init_me() cb_function = hangup_func_str = NULL; hangup_func_arg = NULL; hh = mark = 0; my_perl = NULL; cb_arg = NULL

using namespace PERL;

Session::Session():CoreSession()
{
	init_me();
}

Session::Session(char *uuid, CoreSession *a_leg):CoreSession(uuid, a_leg)
{
	init_me();
	if (session && allocated) {
		suuid = switch_core_session_sprintf(session, "main::uuid_%s\n", switch_core_session_get_uuid(session));
		for (char *p = suuid; p && *p; p++) {
			if (*p == '-') {
				*p = '_';
			}
			if (*p == '\n') {
				*p = '\0';
			}
		}
	}
}

Session::Session(switch_core_session_t *new_session):CoreSession(new_session)
{
	init_me();
	if (session) {
		suuid = switch_core_session_sprintf(session, "main::uuid_%s\n", switch_core_session_get_uuid(session));
		for (char *p = suuid; p && *p; p++) {
			if (*p == '-') {
				*p = '_';
			}
		}
	} else {
		//handle failure
	}
}
static switch_status_t perl_hanguphook(switch_core_session_t *session_hungup);

void Session::destroy(void)
{
	
	if (!allocated) {
		return;
	}

	if (session) {
		if (!channel) {
			channel = switch_core_session_get_channel(session);
		}
		switch_channel_set_private(channel, "CoreSession", NULL);
		switch_core_event_hook_remove_state_change(session, perl_hanguphook);
	}

	switch_safe_free(cb_function);
	switch_safe_free(cb_arg);
	switch_safe_free(hangup_func_str);
	switch_safe_free(hangup_func_arg);	

	CoreSession::destroy();
}

Session::~Session()
{
	destroy();
}

bool Session::begin_allow_threads()
{
	do_hangup_hook();
	return true;
}

bool Session::end_allow_threads()
{
	do_hangup_hook();
	return true;
}

void Session::setPERL(PerlInterpreter * pi)
{

	my_perl = pi;
}

void Session::setME(SV *p)
{
	sanity_check_noreturn;

	me = p;
}

PerlInterpreter *Session::getPERL()
{
	if (!my_perl) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh!\n");
	}
	return my_perl;
}


bool Session::ready()
{
	bool r;

	sanity_check(false);
	r = switch_channel_ready(channel) != 0;
	do_hangup_hook();

	return r;
}

void Session::check_hangup_hook()
{
	if (hangup_func_str && (hook_state == CS_HANGUP || hook_state == CS_ROUTING)) {
		hh++;
	}
}

void Session::do_hangup_hook()
{
	if (hh && !mark) {
		const char *err = NULL;
		mark++;
		char *code;
		if (!getPERL()) {
			return;
		}

		if (hangup_func_arg) {
            code = switch_mprintf("%s($%s,\"%s\",%s)", hangup_func_str, suuid, hook_state == CS_HANGUP ? "hangup" : "transfer", hangup_func_arg);
        } else {
            code = switch_mprintf("%s($%s,\"%s\")", hangup_func_str, suuid, hook_state == CS_HANGUP ? "hangup" : "transfer");
        }

        Perl_eval_pv(my_perl, code, TRUE);
        free(code);
	}
}

static switch_status_t perl_hanguphook(switch_core_session_t *session_hungup)
{
	switch_channel_t *channel = switch_core_session_get_channel(session_hungup);
	CoreSession *coresession = NULL;
	switch_channel_state_t state = switch_channel_get_state(channel);

	if ((coresession = (CoreSession *) switch_channel_get_private(channel, "CoreSession"))) {
		if (coresession->hook_state != state) {
			coresession->hook_state = state;
			coresession->check_hangup_hook();
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


void Session::setHangupHook(char *func, char *arg)
{

	sanity_check_noreturn;

	switch_safe_free(hangup_func_str);

	if (func) {
		hangup_func_str = strdup(func);
		switch_channel_set_private(channel, "CoreSession", this);
		hook_state = switch_channel_get_state(channel);
		switch_core_event_hook_add_state_change(session, perl_hanguphook);
		if (arg) {
			hangup_func_arg = strdup(func);
		}
	}
}

void Session::unsetInputCallback(void)
{
	sanity_check_noreturn;
	switch_safe_free(cb_function);
	switch_safe_free(cb_arg);
	switch_channel_set_private(channel, "CoreSession", NULL);
	args.input_callback = NULL;
	ap = NULL;
	
}

void Session::setInputCallback(char *cbfunc, char *funcargs)
{

	sanity_check_noreturn;

	switch_safe_free(cb_function);
	if (cbfunc) {
		cb_function = strdup(cbfunc);
	}

	switch_safe_free(cb_arg);
	if (funcargs) {
		cb_arg = strdup(funcargs);
	}

	args.buf = this;
	switch_channel_set_private(channel, "CoreSession", this);

	args.input_callback = dtmf_callback;
	ap = &args;
}

switch_status_t Session::run_dtmf_callback(void *input, switch_input_type_t itype)
{
	if (!getPERL()) {
		return SWITCH_STATUS_FALSE;;
	}

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			char str[32] = "";
			int arg_count = 2;
			HV *hash;
			SV *this_sv;
			char *code;
			
			if (!(hash = get_hv("__dtmf", TRUE))) {
				abort();
			}

			str[0] = dtmf->digit;
			this_sv = newSV(strlen(str) + 1);
			sv_setpv(this_sv, str);
			hv_store(hash, "digit", 5, this_sv, 0);

			switch_snprintf(str, sizeof(str), "%d", dtmf->duration);
			this_sv = newSV(strlen(str) + 1);
			sv_setpv(this_sv, str);
			hv_store(hash, "duration", 8, this_sv, 0);
			
			code = switch_mprintf("eval { $__RV = &%s($%s, 'dtmf', \\%%__dtmf, %s);};", cb_function, suuid, switch_str_nil(cb_arg));
			Perl_eval_pv(my_perl, code, FALSE);
			free(code);

			return process_callback_result(SvPV(get_sv("__RV", TRUE), n_a));
		}
		break;
	case SWITCH_INPUT_TYPE_EVENT:
		{
			switch_event_t *event = (switch_event_t *) input;
			int arg_count = 2;
			char *code;
			switch_uuid_t uuid;
			char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
			char var_name[SWITCH_UUID_FORMATTED_LENGTH + 25];
			char *p;

			switch_uuid_get(&uuid);
			switch_uuid_format(uuid_str, &uuid);

			switch_snprintf(var_name, sizeof(var_name), "main::__event_%s", uuid_str);
			for(p = var_name; p && *p; p++) {
				if (*p == '-') {
					*p = '_';
				}
			}

			mod_perl_conjure_event(my_perl, event, var_name);
			code = switch_mprintf("eval {$__RV = &%s($%s, 'event', $%s, '%s');};$%s = undef;", 
								  cb_function, suuid, var_name, switch_str_nil(cb_arg), var_name);
			Perl_eval_pv(my_perl, code, FALSE);
			free(code);

			return process_callback_result(SvPV(get_sv("__RV", TRUE), n_a));
		}
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
