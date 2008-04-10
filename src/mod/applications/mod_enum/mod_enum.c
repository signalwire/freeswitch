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
 * mod_enum.c -- ENUM
 *
 */

#include <switch.h>
#include <udns.h>

#ifndef WIN32
#define closesocket close
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_enum_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_enum_shutdown);
SWITCH_MODULE_DEFINITION(mod_enum, mod_enum_load, mod_enum_shutdown, NULL);

struct enum_record {
	int order;
	int preference;
	char *service;
	char *route;
	struct enum_record *next;
};
typedef struct enum_record enum_record_t;

struct query {
	const char *name;			/* original query string */
	char *number;
	unsigned char dn[DNS_MAXDN];
	enum dns_type qtyp;			/* type of the query */
	enum_record_t *results;
	int errs;
};
typedef struct query enum_query_t;

struct route {
	char *service;
	char *regex;
	char *replace;
	struct route *next;
};
typedef struct route enum_route_t;

static enum dns_class qcls = DNS_C_IN;

static struct {
	char *root;
	char *isn_root;
	switch_hash_t *routes;
	enum_route_t *route_order;
	switch_memory_pool_t *pool;
	int timeout;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_root, globals.root);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_isn_root, globals.isn_root);

static void add_route(char *service, char *regex, char *replace)
{
	enum_route_t *route, *rp;

	if (!(route = malloc(sizeof(*route)))) {
		return;
	}

	memset(route, 0, sizeof(*route));

	route->service = strdup(service);
	route->regex = strdup(regex);
	route->replace = strdup(replace);

	if (!globals.route_order) {
		globals.route_order = route;
	} else {
		for (rp = globals.route_order; rp && rp->next; rp = rp->next);
		rp->next = route;
	}

	if (!(rp = switch_core_hash_find(globals.routes, service))) {
		switch_core_hash_insert(globals.routes, route->service, route);
	}
}


static switch_status_t load_config(void)
{
	char *cf = "enum.conf";
	switch_xml_t cfg, xml = NULL, param, settings, route, routes;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_core_hash_init(&globals.routes, globals.pool);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "default-root")) {
				set_global_root(val);
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


static char *reverse_number(char *in, char *root)
{
	switch_size_t len;
	char *out = NULL;
	char *y, *z;

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

static void dnserror(enum_query_t * q, int errnum)
{
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to lookup %s record for %s: %s\n",
					  dns_typename(q->qtyp), dns_dntosp(q->dn), dns_strerror(errnum));
	q->errs++;
}


static void add_result(enum_query_t * q, int order, int preference, char *service, char *route)
{
	enum_record_t *new_result, *rp, *prev = NULL;

	if (!(new_result = malloc(sizeof(*new_result)))) {
		return;
	}

	memset(new_result, 0, sizeof(*new_result));

	new_result->order = order;
	new_result->preference = preference;
	new_result->service = strdup(service);
	new_result->route = strdup(route);

	if (!q->results) {
		q->results = new_result;
		return;
	}

	rp = q->results;

	while (rp && strcasecmp(rp->service, new_result->service)) {
		prev = rp;
		rp = rp->next;
	}

	while (rp && !strcasecmp(rp->service, new_result->service) && new_result->order > rp->order) {
		prev = rp;
		rp = rp->next;
	}

	while (rp && !strcasecmp(rp->service, new_result->service) && new_result->preference > rp->preference) {
		prev = rp;
		rp = rp->next;
	}

	if (prev) {
		new_result->next = rp;
		prev->next = new_result;
	} else {
		new_result->next = rp;
		q->results = new_result;
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

static void parse_rr(const struct dns_parse *p, enum_query_t * q, struct dns_rr *rr)
{
	const unsigned char *pkt = p->dnsp_pkt;
	const unsigned char *end = p->dnsp_end;
	const unsigned char *dptr = rr->dnsrr_dptr;
	const unsigned char *dend = rr->dnsrr_dend;
	unsigned char *dn = rr->dnsrr_dn;
	const unsigned char *c;
	char flags;
	int order;
	int preference;
	char *service = NULL;
	char *regex = NULL;
	char *replace = NULL;
	char *ptr;
	int argc = 0;
	char *argv[4] = { 0 };
	int n;
	char string_arg[3][256] = {{0}};

	switch (rr->dnsrr_typ) {
	case DNS_T_NAPTR:/* prio weight port targetDN */
		c = dptr;
		c += 4; /* order, pref */

		for (n = 0; n < 3; ++n) {
			if (c >= dend) {
				goto xperr;
			} else { 
				c += *c + 1;
			}
		}

		if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 || c != dend) {
			goto xperr;
		}

		c = dptr;
		order = dns_get16(c+0);
		preference = dns_get16(c+2);
		flags = (char) dns_get16(c + 4);
		c += 4;

		for(n = 0; n < 3; n++) {
			uint32_t len = *c++, cpylen = len;
			switch_assert(string_arg[n]);
			if (len > sizeof(string_arg[n]) - 1) {
				cpylen = sizeof(string_arg[n]) - 1;
			}
			strncpy(string_arg[n], (char *)c, cpylen);
			*(string_arg[n] + len) = '\0';
			c += len;
		}

		service = string_arg[1];

		if ((argc = switch_separate_string(string_arg[2], '!', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			regex = argv[1];
			replace = argv[2];
		} else {
			goto xperr;
		}

		for (ptr = replace; ptr && *ptr; ptr++) {
			if (*ptr == '\\') {
				*ptr = '$';
			}
		}

		if (flags && service && regex && replace) {
			switch_regex_t *re = NULL;
			int proceed = 0, ovector[30];
			char substituted[1024] = "";
			char rbuf[1024] = "";
			char *uri;
			enum_route_t *route;

			switch_regex_safe_free(re);

			if ((proceed = switch_regex_perform(q->number, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				if (strchr(regex, '(')) {
					switch_perform_substitution(re, proceed, replace, q->number, substituted, sizeof(substituted), ovector);
					uri = substituted;
				} else {
					uri = replace;
				}

				if ((route = (enum_route_t *) switch_core_hash_find(globals.routes, service))) {
					switch_regex_safe_free(re);
					if ((proceed = switch_regex_perform(uri, route->regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
						if (strchr(route->regex, '(')) {
							switch_perform_substitution(re, proceed, route->replace, uri, rbuf, sizeof(rbuf), ovector);
							uri = rbuf;
						} else {
							uri = route->replace;
						}
					}
				}
				
				add_result(q, order, preference, service, uri);
			}

			switch_regex_safe_free(re);
		}

		break;

	default:
		break;
	}

	return;

	xperr:
	//printf("<parse error>\n");
	return;
}

static void dnscb(struct dns_ctx *ctx, void *result, void *data)
{
	int r = dns_status(ctx);
	enum_query_t *q = data;
	struct dns_parse p;
	struct dns_rr rr;
	unsigned nrr;
	unsigned char dn[DNS_MAXDN];
	const unsigned char *pkt, *cur, *end, *qdn;
	if (!result) {
		dnserror(q, r);
		return;
	}
	pkt = result;
	end = pkt + r;
	cur = dns_payload(pkt);
	dns_getdn(pkt, &cur, end, dn, sizeof(dn));
	dns_initparse(&p, NULL, pkt, cur, end);
	p.dnsp_qcls = 0;
	p.dnsp_qtyp = 0;
	qdn = dn;
	nrr = 0;
	while ((r = dns_nextrr(&p, &rr)) > 0) {
		if (!dns_dnequal(qdn, rr.dnsrr_dn))
			continue;
		if ((qcls == DNS_C_ANY || qcls == rr.dnsrr_cls) && (q->qtyp == DNS_T_ANY || q->qtyp == rr.dnsrr_typ))
			++nrr;
		else if (rr.dnsrr_typ == DNS_T_CNAME && !nrr) {
			if (dns_getdn(pkt, &rr.dnsrr_dptr, end, p.dnsp_dnbuf, sizeof(p.dnsp_dnbuf)) <= 0 || rr.dnsrr_dptr != rr.dnsrr_dend) {
				r = DNS_E_PROTOCOL;
				break;
			} else {
				qdn = p.dnsp_dnbuf;
			}
		}
	}
	if (!r && !nrr)
		r = DNS_E_NODATA;
	if (r < 0) {
		dnserror(q, r);
		free(result);
		return;
	}

	dns_rewind(&p, NULL);
	p.dnsp_qtyp = q->qtyp;
	p.dnsp_qcls = qcls;
	while (dns_nextrr(&p, &rr)) {
		parse_rr(&p, q, &rr);
	}

	free(result);
}


static switch_status_t enum_lookup(char *root, char *in, enum_record_t ** results)
{
	switch_status_t sstatus = SWITCH_STATUS_SUCCESS;
	char *name = NULL;
	enum_query_t query = { 0 };
	enum dns_type l_qtyp = DNS_T_NAPTR;
	int i = 0, abs = 0, j = 0;
	dns_socket fd = (dns_socket) - 1;
	fd_set fds;
	struct timeval tv = { 0 };
	time_t now = 0;
	struct dns_ctx *nctx = NULL;
	char *num, *mnum = NULL, *mroot = NULL, *p;

	mnum = switch_mprintf("%s%s", *in == '+' ? "" : "+", in);
	
	if ((p = strchr(mnum, '*'))) {
		*p++ = '\0';
		mroot = switch_mprintf("%s.%s", p, root ? root : globals.isn_root);
		root = mroot;
	}

	if (switch_strlen_zero(root)) {
		root = globals.root;
	}

	num = mnum;
	if (!(name = reverse_number(num, root))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parse Error!\n");
		sstatus = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (!(nctx = dns_new(NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Memory Error!\n");
		sstatus = SWITCH_STATUS_MEMERR;
		goto done;
	}

	fd = dns_open(nctx);

	if (fd < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FD Error!\n");
		sstatus = SWITCH_STATUS_FALSE;
		goto done;
	}

	dns_ptodn(name, (unsigned int) strlen(name), query.dn, sizeof(query.dn), &abs);
	query.name = name;
	query.number = num;
	query.qtyp = l_qtyp;

	if (abs) {
		abs = DNS_NOSRCH;
	}

	if (!dns_submit_dn(nctx, query.dn, qcls, l_qtyp, abs, 0, dnscb, &query)) {
		dnserror(&query, dns_status(nctx));
	}

	FD_ZERO(&fds);
	now = 0;

	while ((i = dns_timeouts(nctx, 1, now)) > 0) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4389 4127)
#endif
		FD_SET(fd, &fds);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

		j += i;

		if (j > globals.timeout || query.results || query.errs) {
			break;
		}

		tv.tv_sec = i;
		tv.tv_usec = 0;
		i = select((int) (fd + 1), &fds, 0, 0, &tv);
		now = switch_timestamp(NULL);
		if (i > 0) {
			dns_ioevent(nctx, now);
		}
	}

	if (!query.results) {
		sstatus = SWITCH_STATUS_FALSE;
	}

	*results = query.results;
	query.results = NULL;

  done:

	if (fd > -1) {
		closesocket(fd);
		fd = (dns_socket) - 1;
	}

	if (nctx) {
		dns_free(nctx);
	}

	switch_safe_free(name);
	switch_safe_free(mnum);
	switch_safe_free(mroot);

	return sstatus;
}


SWITCH_STANDARD_DIALPLAN(enum_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	enum_record_t *results, *rp;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	enum_route_t *rtp;
	char *dp = (char *) arg;

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ENUM Lookup on %s\n", caller_profile->destination_number);

	if (enum_lookup(dp, caller_profile->destination_number, &results) == SWITCH_STATUS_SUCCESS) {
		if ((extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
			free_results(&results);
			return NULL;
		}
		switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "true");

		for (rtp = globals.route_order; rtp; rtp = rtp->next) {
			for (rp = results; rp; rp = rp->next) {
				if (!strcasecmp(rtp->service, rp->service)) {
					switch_caller_extension_add_application(session, extension, "bridge", rp->route);
				}
			}
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
	enum_route_t *rtp;
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
			switch_event_header_t *hi;
			if ((hi = switch_channel_variable_first(channel))) {
				for (; hi; hi = hi->next) {
					char *vvar = hi->name;
					if (vvar && !strncmp(vvar, "enum_", 5)) {
						switch_channel_set_variable(channel, (char *) vvar, NULL);
					}
				}
				switch_channel_variable_last(channel);
			}

			for (rtp = globals.route_order; rtp; rtp = rtp->next) {
				for (rp = results; rp; rp = rp->next) {
					if (!strcasecmp(rtp->service, rp->service)) {
						switch_snprintf(vbuf, sizeof(vbuf), "enum_route_%d", cnt++);
						switch_channel_set_variable(channel, vbuf, rp->route);						
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
				}
			}
			switch_snprintf(vbuf, sizeof(vbuf), "%d", cnt);
			switch_channel_set_variable(channel, "enum_route_count", vbuf);
			*(rbuf + strlen(rbuf) - 1) = '\0';
			switch_channel_set_variable(channel, "enum_auto_route", rbuf);
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
	enum_route_t *rtp;
	char rbuf[1024] = "";
	char *rbp = rbuf;
	switch_size_t l = 0, rbl = sizeof(rbuf);
	int last_order = -1, last_pref = -2;
	char *last_delim = "|";

	if (!(mydata = strdup(cmd))) {
		abort();
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		root = argv[1];
		if (enum_lookup(root, dest, &results) == SWITCH_STATUS_SUCCESS) {
			for (rtp = globals.route_order; rtp; rtp = rtp->next) {
				for (rp = results; rp; rp = rp->next) {
					if (!strcasecmp(rtp->service, rp->service)) {
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
				}
			}
			*(rbuf + strlen(rbuf) - 1) = '\0';
			stream->write_function(stream, "%s", rbuf);
			free_results(&results);
		}
	}

	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(enum_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	enum_record_t *results, *rp;
	char *mydata = NULL;
	char *dest = NULL, *root = NULL;
	enum_route_t *rtp;

	if (session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function cannot be called from the dialplan.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!cmd || !(mydata = strdup(cmd))) {
		stream->write_function(stream, "Usage: enum <number> [<root>]\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		root = argv[1];

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

		for (rtp = globals.route_order; rtp; rtp = rtp->next) {
			for (rp = results; rp; rp = rp->next) {
				if (!strcasecmp(rtp->service, rp->service)) {
					stream->write_function(stream, "%d\t%d\t%-10s\t%s\n", rp->order, rp->preference, rp->service, rp->route);
				}
			}
		}

		free_results(&results);
	} else {
		stream->write_function(stream, "Invalid Input!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_enum_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;

	if (dns_init(0) < 0) {
		return SWITCH_STATUS_FALSE;
	}

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	globals.timeout = 10;
	load_config();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "enum", "ENUM", enum_function, "");
	SWITCH_ADD_API(api_interface, "enum_auto", "ENUM", enum_api, "");
	SWITCH_ADD_APP(app_interface, "enum", "Perform an ENUM lookup", "Perform an ENUM lookup", enum_app_function, "<number> [<root>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_DIALPLAN(dp_interface, "enum", enum_dialplan_hunt);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_enum_shutdown)
{
	switch_core_hash_destroy(&globals.routes);
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
