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
 * Andrew Thompson <andrew@hijacked.us>
 * Rob Charlton <rob.charlton@savageminds.com>
 * Karl Anderson <karl@2600hz.com>
 *
 * Original from mod_erlang_event.
 * ei_helpers.c -- helper functions for ei
 *
 */
#include "mod_kazoo.h"

#define kz_resize(l) {\
char *dp;\
olen += (len + l + block);\
cpos = c - data;\
if ((dp = realloc(data, olen))) {\
	data = dp;\
	c = data + cpos;\
	memset(c, 0, olen - cpos);\
 }}                           \


void kz_check_set_profile_var(switch_channel_t *channel, char* var, char *val)
{
	int idx = 0;
	while(kazoo_globals.profile_vars_prefixes[idx] != NULL) {
		char *prefix = kazoo_globals.profile_vars_prefixes[idx];
		if (!strncasecmp(var, prefix, strlen(prefix))) {
			switch_channel_set_profile_var(channel, var + strlen(prefix), val);

		}
		idx++;
	}
}

SWITCH_DECLARE(switch_status_t) kz_switch_core_merge_variables(switch_event_t *event)
{
	switch_event_t *global_vars;
	switch_status_t status = switch_core_get_variables(&global_vars);
	if(status == SWITCH_STATUS_SUCCESS) {
		switch_event_merge(event, global_vars);
		switch_event_destroy(&global_vars);
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) kz_switch_core_base_headers_for_expand(switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_GENERR;
	*event = NULL;
	if(switch_event_create(event, SWITCH_EVENT_GENERAL) == SWITCH_STATUS_SUCCESS) {
		status = kz_switch_core_merge_variables(*event);
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) kz_expand_api_execute(const char *cmd, const char *arg, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_api_interface_t *api;
	switch_status_t status;
	char *arg_used;
	char *cmd_used;

	switch_assert(stream != NULL);
	switch_assert(stream->data != NULL);
	switch_assert(stream->write_function != NULL);

	if (strcasecmp(cmd, "console_complete")) {
		cmd_used = switch_strip_whitespace(cmd);
		arg_used = switch_strip_whitespace(arg);
	} else {
		cmd_used = (char *) cmd;
		arg_used = (char *) arg;
	}

	if (cmd_used && (api = switch_loadable_module_get_api_interface(cmd_used)) != 0) {
		if ((status = api->function(arg_used, session, stream)) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "COMMAND RETURNED ERROR!\n");
		}
		UNPROTECT_INTERFACE(api);
	} else {
		status = SWITCH_STATUS_FALSE;
		stream->write_function(stream, "INVALID COMMAND!\n");
	}

	if (cmd_used != cmd) {
		switch_safe_free(cmd_used);
	}

	if (arg_used != arg) {
		switch_safe_free(arg_used);
	}

	return status;
}


SWITCH_DECLARE(char *) kz_event_expand_headers_check(switch_event_t *event, const char *in, switch_event_t *var_list, switch_event_t *api_list, uint32_t recur)
{
	char *p, *c = NULL;
	char *data, *indup, *endof_indup;
	size_t sp = 0, len = 0, olen = 0, vtype = 0, br = 0, cpos, block = 128;
	const char *sub_val = NULL;
	char *cloned_sub_val = NULL, *expanded_sub_val = NULL;
	char *func_val = NULL;
	int nv = 0;
	char *gvar = NULL, *sb = NULL;

	if (recur > 100) {
		return (char *) in;
	}

	if (zstr(in)) {
		return (char *) in;
	}

	nv = switch_string_var_check_const(in) || switch_string_has_escaped_data(in);

	if (!nv) {
		return (char *) in;
	}

	nv = 0;
	olen = strlen(in) + 1;
	indup = strdup(in);
	endof_indup = end_of_p(indup) + 1;

	if ((data = malloc(olen))) {
		memset(data, 0, olen);
		c = data;
		for (p = indup; p && p < endof_indup && *p; p++) {
			int global = 0;
			vtype = 0;

			if (*p == '\\') {
				if (*(p + 1) == '$') {
					nv = 1;
					p++;
					if (*(p + 1) == '$') {
						p++;
					}
				} else if (*(p + 1) == '\'') {
					p++;
					continue;
				} else if (*(p + 1) == '\\') {
					if (len + 1 >= olen) {
						kz_resize(1);
					}

					*c++ = *p++;
					len++;
					continue;
				}
			}

			if (*p == '$' && !nv) {
				if (*(p + 1) == '$') {
					p++;
					global++;
				}

				if (*(p + 1)) {
					if (*(p + 1) == '{') {
						vtype = global ? 3 : 1;
					} else {
						nv = 1;
					}
				} else {
					nv = 1;
				}
			}

			if (nv) {
				if (len + 1 >= olen) {
					kz_resize(1);
				}

				*c++ = *p;
				len++;
				nv = 0;
				continue;
			}

			if (vtype) {
				char *s = p, *e, *vname, *vval = NULL;
				size_t nlen;

				s++;

				if ((vtype == 1 || vtype == 3) && *s == '{') {
					br = 1;
					s++;
				}

				e = s;
				vname = s;
				while (*e) {
					if (br == 1 && *e == '}') {
						br = 0;
						*e++ = '\0';
						break;
					}

					if (br > 0) {
						if (e != s && *e == '{') {
							br++;
						} else if (br > 1 && *e == '}') {
							br--;
						}
					}

					e++;
				}
				p = e > endof_indup ? endof_indup : e;

				vval = NULL;
				for(sb = vname; sb && *sb; sb++) {
					if (*sb == ' ') {
						vval = sb;
						break;
					} else if (*sb == '(') {
						vval = sb;
						br = 1;
						break;
					}
				}

				if (vval) {
					e = vval - 1;
					*vval++ = '\0';

					while (*e == ' ') {
						*e-- = '\0';
					}
					e = vval;

					while (e && *e) {
						if (*e == '(') {
							br++;
						} else if (br > 1 && *e == ')') {
							br--;
						} else if (br == 1 && *e == ')') {
							*e = '\0';
							break;
						}
						e++;
					}

					vtype = 2;
				}

				if (vtype == 1 || vtype == 3) {
					char *expanded = NULL;
					int offset = 0;
					int ooffset = 0;
					char *ptr;
					int idx = -1;

					if ((expanded = kz_event_expand_headers_check(event, (char *) vname, var_list, api_list, recur+1)) == vname) {
						expanded = NULL;
					} else {
						vname = expanded;
					}
					if ((ptr = strchr(vname, ':'))) {
						*ptr++ = '\0';
						offset = atoi(ptr);
						if ((ptr = strchr(ptr, ':'))) {
							ptr++;
							ooffset = atoi(ptr);
						}
					}

					if ((ptr = strchr(vname, '[')) && strchr(ptr, ']')) {
						*ptr++ = '\0';
						idx = atoi(ptr);
					}

					if (vtype == 3 || !(sub_val = switch_event_get_header_idx(event, vname, idx))) {
						switch_safe_free(gvar);
						if ((gvar = switch_core_get_variable_dup(vname))) {
							sub_val = gvar;
						}

						if (var_list && !switch_event_check_permission_list(var_list, vname)) {
							sub_val = "<Variable Expansion Permission Denied>";
						}


						if ((expanded_sub_val = kz_event_expand_headers_check(event, sub_val, var_list, api_list, recur+1)) == sub_val) {
							expanded_sub_val = NULL;
						} else {
							sub_val = expanded_sub_val;
						}
					}

					if (sub_val) {
						if (offset || ooffset) {
							cloned_sub_val = strdup(sub_val);
							switch_assert(cloned_sub_val);
							sub_val = cloned_sub_val;
						}

						if (offset >= 0) {
							sub_val += offset;
						} else if ((size_t) abs(offset) <= strlen(sub_val)) {
							sub_val = cloned_sub_val + (strlen(cloned_sub_val) + offset);
						}

						if (ooffset > 0 && (size_t) ooffset < strlen(sub_val)) {
							if ((ptr = (char *) sub_val + ooffset)) {
								*ptr = '\0';
							}
						}
					}

					switch_safe_free(expanded);
				} else {
					switch_stream_handle_t stream = { 0 };
					char *expanded = NULL;

					SWITCH_STANDARD_STREAM(stream);

					if (stream.data) {
						char *expanded_vname = NULL;

						if ((expanded_vname = kz_event_expand_headers_check(event, (char *) vname, var_list, api_list, recur+1)) == vname) {
							expanded_vname = NULL;
						} else {
							vname = expanded_vname;
						}

						if ((expanded = kz_event_expand_headers_check(event, vval, var_list, api_list, recur+1)) == vval) {
							expanded = NULL;
						} else {
							vval = expanded;
						}

						if (!switch_core_test_flag(SCF_API_EXPANSION) || (api_list && !switch_event_check_permission_list(api_list, vname))) {
							func_val = NULL;
							sub_val = "<API execute Permission Denied>";
						} else {
							stream.param_event = event;
							if (kz_expand_api_execute(vname, vval, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
								func_val = stream.data;
								sub_val = func_val;
							} else {
								free(stream.data);
							}
						}

						switch_safe_free(expanded);
						switch_safe_free(expanded_vname);

					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
						free(data);
						free(indup);
						return (char *) in;
					}
				}
				if ((nlen = sub_val ? strlen(sub_val) : 0)) {
					if (len + nlen >= olen) {
						kz_resize(nlen);
					}

					len += nlen;
					strcat(c, sub_val);
					c += nlen;
				}

				switch_safe_free(func_val);
				switch_safe_free(cloned_sub_val);
				switch_safe_free(expanded_sub_val);
				sub_val = NULL;
				vname = NULL;
				vtype = 0;
				br = 0;
			}

			if (sp) {
				if (len + 1 >= olen) {
					kz_resize(1);
				}

				*c++ = ' ';
				sp = 0;
				len++;
			}

			if (*p == '$') {
				p--;
			} else {
				if (len + 1 >= olen) {
					kz_resize(1);
				}

				*c++ = *p;
				len++;
			}
		}
	}
	free(indup);
	switch_safe_free(gvar);

	return data;
}

SWITCH_DECLARE(char *) kz_event_expand_headers(switch_event_t *event, const char *in)
{
	return kz_event_expand_headers_check(event, in, NULL, NULL, 0);
}

SWITCH_DECLARE(char *) kz_event_expand(const char *in)
{
	switch_event_t *event = NULL;
	char *ret = NULL;
	kz_switch_core_base_headers_for_expand(&event);
	ret = kz_event_expand_headers_check(event, in, NULL, NULL, 0);
	switch_event_destroy(&event);
	return ret;
}

char *kazoo_expand_header(switch_memory_pool_t *pool, switch_event_t *event, char *val)
{
	char *expanded;
	char *dup = NULL;

	expanded = kz_event_expand_headers(event, val);
	dup = switch_core_strdup(pool, expanded);

	if (expanded != val) {
		free(expanded);
	}

	return dup;
}

char* kz_switch_event_get_first_of(switch_event_t *event, const char *list[])
{
	switch_event_header_t *header = NULL;
	int i = 0;
	while(list[i] != NULL) {
		if((header = switch_event_get_header_ptr(event, list[i])) != NULL)
			break;
		i++;
	}
	if(header != NULL) {
		return header->value;
	} else {
		return "nodomain";
	}
}

SWITCH_DECLARE(switch_status_t) kz_switch_event_add_variable_name_printf(switch_event_t *event, switch_stack_t stack, const char *val, const char *fmt, ...)
{
	int ret = 0;
	char *varname;
	va_list ap;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(event != NULL);


	va_start(ap, fmt);
	ret = switch_vasprintf(&varname, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return SWITCH_STATUS_MEMERR;
	}

	status = switch_event_add_header_string(event, stack, varname, val);

	free(varname);

	return status;
}

SWITCH_DECLARE(switch_xml_t) kz_xml_child(switch_xml_t xml, const char *name)
{
	xml = (xml) ? xml->child : NULL;
	while (xml && strcasecmp(name, xml->name))
		xml = xml->sibling;
	return xml;
}

void kz_xml_process(switch_xml_t cfg)
{
	switch_xml_t xml_process;
	for (xml_process = kz_xml_child(cfg, "X-PRE-PROCESS"); xml_process; xml_process = xml_process->next) {
		const char *cmd = switch_xml_attr(xml_process, "cmd");
		const char *data = switch_xml_attr(xml_process, "data");
		if(cmd != NULL && !strcasecmp(cmd, "set") && data) {
			char *name = (char *) data;
			char *val = strchr(name, '=');

			if (val) {
				char *ve = val++;
				while (*val && *val == ' ') {
					val++;
				}
				*ve-- = '\0';
				while (*ve && *ve == ' ') {
					*ve-- = '\0';
				}
			}

			if (name && val) {
				switch_core_set_variable(name, val);
			}
		}
	}

}

void kz_event_decode(switch_event_t *event)
{
	switch_event_header_t *hp;
	int i;
	for (hp = event->headers; hp; hp = hp->next) {
		if (hp->idx) {
			for(i = 0; i < hp->idx; i++) {
				switch_url_decode(hp->array[i]);
			}
		} else {
			switch_url_decode(hp->value);
		}
	}
}

char * kz_expand_vars(char *xml_str) {
	return kz_expand_vars_pool(xml_str, NULL);
}

char * kz_expand_vars_pool(char *xml_str, switch_memory_pool_t *pool) {
	char *var, *val;
	char *rp = xml_str; /* read pointer */
	char *ep, *wp, *buff; /* end pointer, write pointer, write buffer */

	if (!(strstr(xml_str, "$${"))) {
		return xml_str;
	}

	switch_zmalloc(buff, strlen(xml_str) * 2);
	wp = buff;
	ep = buff + (strlen(xml_str) * 2) - 1;

	while (*rp && wp < ep) {
		if (*rp == '$' && *(rp + 1) == '$' && *(rp + 2) == '{') {
			char *e = switch_find_end_paren(rp + 2, '{', '}');

			if (e) {
				rp += 3;
				var = rp;
				*e++ = '\0';
				rp = e;

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "trying to expand %s \n", var);
				if ((val = switch_core_get_variable_dup(var))) {
					char *p;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "expanded %s to %s\n", var, val);
					for (p = val; p && *p && wp <= ep; p++) {
						*wp++ = *p;
					}
					switch_safe_free(val);
				}
				continue;
			}
		}

		*wp++ = *rp++;
	}

	*wp++ = '\0';

	if(pool) {
		char * ret = switch_core_strdup(pool, buff);
		switch_safe_free(buff);
		return ret;
	} else {
		switch_safe_free(xml_str);
		return buff;
	}

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
