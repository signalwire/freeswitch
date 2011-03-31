/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_enum.c -- ENUM
 *
 */

#include <switch.h>
#ifdef _MSC_VER
#define ssize_t int
#endif
#include <ldns/ldns.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_enum_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_enum_shutdown);
SWITCH_MODULE_DEFINITION(mod_enum, mod_enum_load, mod_enum_shutdown, NULL);

static switch_mutex_t *MUTEX = NULL;

struct enum_record {
	int order;
	int preference;
	char *service;
	char *route;
	int supported;
	struct enum_record *next;
	struct enum_record *tail;
};
typedef struct enum_record enum_record_t;

struct route {
	char *service;
	char *regex;
	char *replace;
	struct route *next;
};
typedef struct route enum_route_t;

static switch_event_node_t *NODE = NULL;

static struct {
	char *root;
	char *server;
	char *isn_root;
	enum_route_t *route_order;
	switch_memory_pool_t *pool;
	int auto_reload;
	int timeout;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_root, globals.root);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_server, globals.server);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_isn_root, globals.isn_root);

static void add_route(char *service, char *regex, char *replace)
{
	enum_route_t *route, *rp;

	route = switch_core_alloc(globals.pool, sizeof(*route));


	route->service = switch_core_strdup(globals.pool, service);
	route->regex = switch_core_strdup(globals.pool, regex);
	route->replace = switch_core_strdup(globals.pool, replace);

	switch_mutex_lock(MUTEX);
	if (!globals.route_order) {
		globals.route_order = route;
	} else {
		for (rp = globals.route_order; rp && rp->next; rp = rp->next);
		rp->next = route;
	}
	switch_mutex_unlock(MUTEX);
}

static switch_status_t load_config(void)
{
	char *cf = "enum.conf";
	switch_xml_t cfg, xml = NULL, param, settings, route, routes;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			const char *var = switch_xml_attr_soft(param, "name");
			const char *val = switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "default-root")) {
				set_global_root(val);
			} else if (!strcasecmp(var, "use-server")) {
				set_global_server(val);
			} else if (!strcasecmp(var, "auto-reload")) {
				globals.auto_reload = switch_true(val);
			} else if (!strcasecmp(var, "query-timeout")) {
				globals.timeout = atoi(val);
			} else if (!strcasecmp(var, "default-isn-root")) {
				set_global_isn_root(val);
			} else if (!strcasecmp(var, "log-level-trace")) {

			}
		}
	}

	if ((routes = switch_xml_child(cfg, "routes"))) {
		for (route = switch_xml_child(routes, "route"); route; route = route->next) {
			char *service = (char *) switch_xml_attr_soft(route, "service");
			char *regex = (char *) switch_xml_attr_soft(route, "regex");
			char *replace = (char *) switch_xml_attr_soft(route, "replace");

			if (service && regex && replace) {
				add_route(service, regex, replace);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Route!\n");
			}
		}
	}

  done:
#ifdef _MSC_VER
	if (!globals.server) {
		HKEY hKey;
		DWORD data_sz;
		char* buf;
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
			"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", 
			0, KEY_QUERY_VALUE, &hKey);

		if (hKey) {
			RegQueryValueEx(hKey, "DhcpNameServer", NULL, NULL, NULL, &data_sz);
			if (data_sz) {
				buf = (char*)malloc(data_sz + 1);

				RegQueryValueEx(hKey, "DhcpNameServer", NULL, NULL, (LPBYTE)buf, &data_sz);
				RegCloseKey(hKey);

				if(buf[data_sz - 1] != 0) {
					buf[data_sz] = 0;
				}
				globals.server = buf;
			}
		}
	}
#endif


	if (xml) {
		switch_xml_free(xml);
	}

	if (!globals.root) {
		set_global_root("e164.org");
	}

	if (!globals.isn_root) {
		set_global_isn_root("freenum.org");
	}

	return status;
}

static char *reverse_number(const char *in, const char *root)
{
	switch_size_t len;
	char *out = NULL;
	const char *y;
	char *z;

	if (!(in && root)) {
		return NULL;
	}

	len = (strlen(in) * 2) + strlen(root) + 1;
	if ((out = malloc(len))) {
		memset(out, 0, len);

		z = out;
		for (y = in + (strlen(in) - 1); y; y--) {
			if (*y > 47 && *y < 58) {
				*z++ = *y;
				*z++ = '.';
			}
			if (y == in) {
				break;
			}
		}
		strcat(z, root);
	}

	return out;
}


static void add_result(enum_record_t **results, int order, int preference, char *service, char *route, int supported)
{
	enum_record_t *new_result;

	new_result = malloc(sizeof(*new_result));
	switch_assert(new_result);

	memset(new_result, 0, sizeof(*new_result));
	new_result->order = order;
	new_result->preference = preference;
	new_result->service = strdup(service);
	new_result->route = strdup(route);
	new_result->supported = supported;
	

	if (!*results) {
		*results = new_result;
		(*results)->tail = new_result;
	} else {
		(*results)->tail->next = new_result;
		(*results)->tail = new_result;
	}

}


static void free_results(enum_record_t ** results)
{
	enum_record_t *fp, *rp;

	for (rp = *results; rp;) {
		fp = rp;
		rp = rp->next;
		switch_safe_free(fp->service);
		switch_safe_free(fp->route);
		switch_safe_free(fp);
	}
	*results = NULL;
}


static ldns_rdf *ldns_rdf_new_addr_frm_str(const char *str)
{
	ldns_rdf *a;

	ldns_str2rdf_a(&a, str);

	if (!a) {
		/* maybe ip6 */
		ldns_str2rdf_aaaa(&a, str);
		if (!a) {
			return NULL;
		}
	}
	return a;
}

#define strip_quotes(_s) if (*_s == '"') _s++; if (end_of(_s) == '"') end_of(_s) = '\0'

static void parse_naptr(const ldns_rr *naptr, const char *number, enum_record_t **results)
{
	char *str = ldns_rr2str(naptr);
	char *argv[11] = { 0 };
	int i, argc;
	char *pack[4] = { 0 };
	int packc;

	char *p;
	int order = 10;
	int preference = 100;
	char *service = NULL;
	char *packstr;

	char *regex, *replace;
	
	if (zstr(str)) {
		return;
	}

	for (p = str; p && *p; p++) {
		if (*p == '\t') *p = ' ';
		if (*p == ' ' && *(p+1) == '.') *p = '\0';
	}


	argc = switch_split(str, ' ', argv);

	for (i = 0; i < argc; i++) {
		if (i > 0) {
			strip_quotes(argv[i]);
		}
	}

	service = argv[7];
	packstr = argv[8];

	if (zstr(service) || zstr(packstr)) {
		goto end;
	}
	
	if (!zstr(argv[4])) {
		order = atoi(argv[4]);
	}

	if (!zstr(argv[5])) {
		preference = atoi(argv[5]);
	}


	if ((packc = switch_split(packstr, '!', pack))) {
		regex = pack[1];
		replace = pack[2];
	} else {
		goto end;
	}
	
	for (p = replace; p && *p; p++) {
		if (*p == '\\') {
			*p = '$';
		}
	}

	if (service && regex && replace) {
		switch_regex_t *re = NULL, *re2 = NULL;
		int proceed = 0, ovector[30];
		char substituted[1024] = "";
		char rbuf[1024] = "";
		char *uri;
		enum_route_t *route;
		int supported = 0;

		if ((proceed = switch_regex_perform(number, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
			if (strchr(regex, '(')) {
				switch_perform_substitution(re, proceed, replace, number, substituted, sizeof(substituted), ovector);
				uri = substituted;
			} else {
				uri = replace;
			}
			
			switch_mutex_lock(MUTEX);
			for (route = globals.route_order; route; route = route->next) {
				if (strcasecmp(service, route->service)) {
					continue;
				}

				if ((proceed = switch_regex_perform(uri, route->regex, &re2, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
					if (strchr(route->regex, '(')) {
						switch_perform_substitution(re2, proceed, route->replace, uri, rbuf, sizeof(rbuf), ovector);
						uri = rbuf;
					} else {
						uri = route->replace;
					}
					supported++;
					add_result(results, order, preference, service, uri, supported);
				}
				switch_regex_safe_free(re2);
			}
			switch_mutex_unlock(MUTEX);			

			if (!supported) {
				add_result(results, order, preference, service, uri, 0);
			}

			switch_regex_safe_free(re);
		}
	}

 end:

	switch_safe_free(str);
	
	return;
}


switch_status_t ldns_lookup(const char *number, const char *root, const char *server_name, enum_record_t **results)
{
	ldns_resolver *res = NULL;
	ldns_rdf *domain = NULL;
	ldns_pkt *p = NULL;
	ldns_rr_list *naptr = NULL;
	ldns_status s = LDNS_STATUS_ERR;
	ldns_rdf *serv_rdf;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *name = NULL;

	if (!(name = reverse_number(number, root))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parse Error!\n");
		goto end;
	}
	
	if (!(domain = ldns_dname_new_frm_str(name))) {
		goto end;
	}

	if (!zstr(server_name)) {
		res = ldns_resolver_new();
		switch_assert(res);
		
		if ((serv_rdf = ldns_rdf_new_addr_frm_str(server_name))) {
			s = ldns_resolver_push_nameserver(res, serv_rdf);
			ldns_rdf_deep_free(serv_rdf);
		}
	} else {
		/* create a new resolver from /etc/resolv.conf */
		s = ldns_resolver_new_frm_file(&res, NULL);
	}

	if (s != LDNS_STATUS_OK) {
		goto end;
	}

	if ((p = ldns_resolver_query(res,
								 domain,
								 LDNS_RR_TYPE_NAPTR,
								 LDNS_RR_CLASS_IN,
								 LDNS_RD))) {
		/* retrieve the NAPTR records from the answer section of that
		 * packet
		 */

		if ((naptr = ldns_pkt_rr_list_by_type(p, LDNS_RR_TYPE_NAPTR, LDNS_SECTION_ANSWER))) {
			size_t i;

			ldns_rr_list_sort(naptr); 

			for (i = 0; i < ldns_rr_list_rr_count(naptr); i++) {
				parse_naptr(ldns_rr_list_rr(naptr, i), number, results);
			}

			//ldns_rr_list_print(stdout, naptr);
			ldns_rr_list_deep_free(naptr);
			status = SWITCH_STATUS_SUCCESS;
		}
	}

 end:

	switch_safe_free(name);
	
	if (domain) {
		ldns_rdf_deep_free(domain);
	}

	if (p) {
		ldns_pkt_free(p);
	}

	if (res) {
		ldns_resolver_deep_free(res);
	}

	return status;
}

static switch_status_t enum_lookup(char *root, char *in, enum_record_t **results)
{
	switch_status_t sstatus = SWITCH_STATUS_SUCCESS;
	char *mnum = NULL, *mroot = NULL, *p;
	char *server = NULL;

	*results = NULL;

	mnum = switch_mprintf("%s%s", *in == '+' ? "" : "+", in);

	if ((p = strchr(mnum, '*'))) {
		*p++ = '\0';
		mroot = switch_mprintf("%s.%s", p, root ? root : globals.isn_root);
		root = mroot;
	}

	if (zstr(root)) {
		root = globals.root;
	}


	if (!(server = switch_core_get_variable("enum-server"))) {
		server = globals.server;
	}

	ldns_lookup(mnum, root, server, results);

	switch_safe_free(mnum);
	switch_safe_free(mroot);

	return sstatus;
}

SWITCH_STANDARD_DIALPLAN(enum_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	enum_record_t *results = NULL, *rp;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *dp = (char *) arg;

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ENUM Lookup on %s\n", caller_profile->destination_number);

	if (enum_lookup(dp, caller_profile->destination_number, &results) == SWITCH_STATUS_SUCCESS) {
		if ((extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			free_results(&results);
			return NULL;
		}
		switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "true");


		for (rp = results; rp; rp = rp->next) {
			if (!rp->supported) {
				continue;
			}

			switch_caller_extension_add_application(session, extension, "bridge", rp->route);
		}


		free_results(&results);
	}

	return extension;
}

SWITCH_STANDARD_APP(enum_app_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *dest = NULL, *root = NULL;
	enum_record_t *results, *rp;
	char rbuf[1024] = "";
	char vbuf[1024] = "";
	char *rbp = rbuf;
	switch_size_t l = 0, rbl = sizeof(rbuf);
	uint32_t cnt = 1;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int last_order = -1, last_pref = -2;
	char *last_delim = "|";

	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		root = argv[1];
		if (enum_lookup(root, dest, &results) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *vars;
			
			if (switch_channel_get_variables(channel, &vars) == SWITCH_STATUS_SUCCESS) {
				switch_event_header_t *hi;
				for (hi = vars->headers; hi; hi = hi->next) {
					char *vvar = hi->name;
					if (vvar && !strncmp(vvar, "enum_", 5)) {
						switch_channel_set_variable(channel, (char *) vvar, NULL);
					}
				}
				switch_event_destroy(&vars);
			}

			for (rp = results; rp; rp = rp->next) {
				if (!rp->supported) {
					continue;
				}
				switch_snprintf(vbuf, sizeof(vbuf), "enum_route_%d", cnt++);
				switch_channel_set_variable_var_check(channel, vbuf, rp->route, SWITCH_FALSE);
				if (rp->preference == last_pref && rp->order == last_order) {
					*last_delim = ',';
				}
				switch_snprintf(rbp, rbl, "%s|", rp->route);
				last_delim = end_of_p(rbp);
				last_order = rp->order;
				last_pref = rp->preference;
				l = strlen(rp->route) + 1;
				rbp += l;
				rbl -= l;
			}

			switch_snprintf(vbuf, sizeof(vbuf), "%d", cnt - 1);
			switch_channel_set_variable_var_check(channel, "enum_route_count", vbuf, SWITCH_FALSE);
			*(rbuf + strlen(rbuf) - 1) = '\0';
			switch_channel_set_variable_var_check(channel, "enum_auto_route", rbuf, SWITCH_FALSE);
			free_results(&results);
		}
	}
}

SWITCH_STANDARD_API(enum_api)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *dest = NULL, *root = NULL;
	enum_record_t *results, *rp;
	char rbuf[1024] = "";
	char *rbp = rbuf;
	switch_size_t l = 0, rbl = sizeof(rbuf);
	int last_order = -1, last_pref = -2;
	char *last_delim = "|";
	int ok = 0;

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", "none");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(mydata = strdup(cmd))) {
		abort();
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		root = argv[1];
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Looking up %s@%s\n", dest, root);
		if (enum_lookup(root, dest, &results) == SWITCH_STATUS_SUCCESS) {
			for (rp = results; rp; rp = rp->next) {
				if (!rp->supported) {
					continue;
				}
				if (rp->preference == last_pref && rp->order == last_order) {
					*last_delim = ',';
				}
				switch_snprintf(rbp, rbl, "%s|", rp->route);
				last_delim = end_of_p(rbp);
				last_order = rp->order;
				last_pref = rp->preference;
				l = strlen(rp->route) + 1;
				rbp += l;
				rbl -= l;

			}
			*(rbuf + strlen(rbuf) - 1) = '\0';
			stream->write_function(stream, "%s", rbuf);
			free_results(&results);
			ok++;
		}
	}

	switch_safe_free(mydata);

	if (!ok) {
		stream->write_function(stream, "%s", "none");
	}

	return SWITCH_STATUS_SUCCESS;
}

static void do_load(void)
{
	switch_mutex_lock(MUTEX);
	if (globals.pool) {
		switch_core_destroy_memory_pool(&globals.pool);
	}

	memset(&globals, 0, sizeof(globals));
	switch_core_new_memory_pool(&globals.pool);
	globals.timeout = 10;
	load_config();
	switch_mutex_unlock(MUTEX);

}

SWITCH_STANDARD_API(enum_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	enum_record_t *results, *rp;
	char *mydata = NULL;
	char *dest = NULL, *root = NULL;

	if (session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function cannot be called from the dialplan.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!cmd || !(mydata = strdup(cmd))) {
		stream->write_function(stream, "Usage: enum [reload | <number> [<root>] ]\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		root = argv[1];
		switch_assert(dest);

		if (!strcasecmp(dest, "reload")) {
			do_load();
			stream->write_function(stream, "+OK ENUM Reloaded.\n");
			return SWITCH_STATUS_SUCCESS;

		}

		if (!enum_lookup(root, dest, &results) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "No Match!\n");
			return SWITCH_STATUS_SUCCESS;
		}

		stream->write_function(stream,
							   "\nOffered Routes:\n"
							   "Order\tPref\tService   \tRoute\n" "==============================================================================\n");

		for (rp = results; rp; rp = rp->next) {
			stream->write_function(stream, "%d\t%d\t%-10s\t%s\n", rp->order, rp->preference, rp->service, rp->route);
		}


		stream->write_function(stream,
							   "\nSupported Routes:\n"
							   "Order\tPref\tService   \tRoute\n" "==============================================================================\n");


		for (rp = results; rp; rp = rp->next) {
			if (rp->supported) {
				stream->write_function(stream, "%d\t%d\t%-10s\t%s\n", rp->order, rp->preference, rp->service, rp->route);
			}
		}

		free_results(&results);
	} else {
		stream->write_function(stream, "Invalid Input!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static void event_handler(switch_event_t *event)
{
	if (globals.auto_reload) {
		switch_mutex_lock(MUTEX);
		do_load();
		switch_mutex_unlock(MUTEX);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ENUM Reloaded\n");
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_enum_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;


	switch_mutex_init(&MUTEX, SWITCH_MUTEX_NESTED, pool);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_TERM;
	}


	memset(&globals, 0, sizeof(globals));
	do_load();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "enum", "ENUM", enum_function, "");
	SWITCH_ADD_API(api_interface, "enum_auto", "ENUM", enum_api, "");
	SWITCH_ADD_APP(app_interface, "enum", "Perform an ENUM lookup", "Perform an ENUM lookup", enum_app_function, "[reload | <number> [<root>]]",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_DIALPLAN(dp_interface, "enum", enum_dialplan_hunt);


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_enum_shutdown)
{
	switch_event_unbind(&NODE);

	if (globals.pool) {
		switch_core_destroy_memory_pool(&globals.pool);
	}

	switch_safe_free(globals.root);
	switch_safe_free(globals.server);
	switch_safe_free(globals.isn_root);
	
	return SWITCH_STATUS_UNLOAD;
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
