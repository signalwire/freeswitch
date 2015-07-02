/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**
 * This is an example program for @b sresolv library in synchronous mode.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Original Created: Tue Jul 16 18:50:14 2002 ppessi
 */

/**@page sip-dig Resolve SIP URIs.
 *
 * @section sip_dig_synopsis Synopsis
 * <tt>sip-dig [OPTIONS] uri...</tt>
 *
 * @section sip_dig_description Description
 * The @em sip-dig utility resolves SIP URIs as described in @RFC3263. It
 * queries NAPTR, SRV and A/AAAA records and prints out the resulting
 * transport addresses.
 *
 * The default transports are: UDP, TCP, SCTP, TLS and TLS-SCTP. The SIPS
 * URIs are resolved using only TLS transports, TLS and TLS-SCTP. If not
 * otherwise indicated by NAPTR or SRV records, the sip-dig uses UDP and TCP
 * as transports for SIP and TLS for SIPS URIs.
 *
 * The results are printed intended, with a preference followed by weight,
 * then protocol name, port number and IP address in numeric format.
 *
 * @section sip_dig_options Command Line Options
 * The @e sip-dig utility accepts following command line options:
 * <dl>
 * <dt>-p <em>protoname</em></dt>
 * <dd>Use named transport protocol. The <em>protoname</em> can be either
 * well-known, e.g., "udp", or it can specify NAPTR service and SRV
 * identifier, e.g., "tls-udp/SIPS+D2U/_sips._udp.".
 * </dd>
 * <dt>--udp</dt>
 * <dd>Use UDP transport protocol.
 * </dd>
 * <dt>--tcp</dt>
 * <dd>Use TCP transport protocol.
 * </dd>
 * <dt>--tls</dt>
 * <dd>Use TLS over TCP transport protocol.
 * </dd>
 * <dt>--sctp</dt>
 * <dd>Use SCTP transport protocol.
 * </dd>
 * <dt>--tls-sctp</dt>
 * <dd>Use TLS over SCTP transport protocol.
 * </dd>
 * <dt>--no-sctp</dt>
 * <dd>Ignore SCTP or TLS-SCTP records in the list of default transports.
 * This option has no effect if transport protocols has been explicitly
 * listed.
 * </dd>
 * <dt>-4</dt>
 * <dd>Query IP4 addresses (A records)
 * </dd>
 * <dt>-6</dt>
 * <dd>Query IP6 addresses (AAAA records).
 * </dd>
 * <dt>-v</dt>
 * <dd>Be verbatim.
 * </dd>
 * <dt></dt>
 * <dd>
 * </dd>
 * </dl>
 *
 * @section sip_dig_return Return Codes
 * <table>
 * <tr><td>0<td>when successful (a 2XX-series response is received)
 * <tr><td>1<td>when unsuccessful (a 3XX..6XX-series response is received)
 * <tr><td>2<td>initialization failure
 * </table>
 *
 * @section sip_dig_examples Examples
 *
 * Resolve sip:openlaboratory.net, prefer TLS over TCP, TCP over UDP:
 * @code
 * $ sip-dig --tls --tcp --udp sip:openlaboratory.net
 *	1 0.333 tls 5061 212.213.221.127
 *	2 0.333 tcp 5060 212.213.221.127
 *	3 0.333 udp 5060 212.213.221.127
 * @endcode
 *
 * Resolve sips:example.net with TLS over SCTP (TLS-SCTP) and TLS:
 * @code
 * $ sip-dig -p tls-sctp --tls sips:example.net
 *	1 0.500 tls-udp 5061 172.21.55.26
 *	2 0.500 tls 5061 172.21.55.26
 * @endcode
 *
 * @section sip_dig_environment Environment
 * #SRESOLV_DEBUG, SRESOLV_CONF
 *
 * @section sip_dig_bugs Reporting Bugs
 * Report bugs to <sofia-sip-devel@lists.sourceforge.net>.
 *
 * @section sip_dig_author Author
 * Written by Pekka Pessi <pekka -dot pessi -at- nokia -dot- com>
 *
 * @section sip_dig_copyright Copyright
 * Copyright (C) 2006 Nokia Corporation.
 *
 * This program is free software; see the source for copying conditions.
 * There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 */


#include "switch.h"
#ifndef WIN32
#include "../../config.h"
#endif

#include "sofia-sip/su.h"

#include "sofia-resolv/sres.h"
#include "sofia-resolv/sres_record.h"

#include "sofia-sip/url.h"
#include "sofia-sip/su_alloc.h"
#include "sofia-sip/su_string.h"
#include "sofia-sip/hostdomain.h"

char const name[] = "sip-dig";

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum { N_TPORT = 16 };

struct transport { char const *name, *service, *srv; };

struct dig {
	sres_resolver_t *sres;

	unsigned preference, ip4, ip6, sips, print;

	struct transport tports[N_TPORT + 1];
};

int dig_naptr(struct dig *dig, char const *host, double weight, switch_stream_handle_t *stream);

int dig_all_srvs(struct dig *dig, char const *tport, char const *host,
				 double weight, switch_stream_handle_t *stream);

int dig_srv(struct dig *dig, char const *tport, char const *host,
			double weight, switch_stream_handle_t *stream);

int dig_srv_at(struct dig *dig,
			   char const *tport, sres_record_t **answers,
			   double weight, int pweight,
			   int priority, switch_stream_handle_t *stream);

int dig_addr(struct dig *dig,
			 char const *tport, char const *host, char const *port,
			 double weight, switch_stream_handle_t *stream);

void print_addr_results(struct transport const *tports,
						char const *tport, char const *tport2,
						sres_record_t **answers, int type, int af,
						char const *port,
						double weight, int preference, switch_stream_handle_t *stream);

void print_result(char const *addr, char const *port, char const *tport,
				  double weight,
				  unsigned preference,
				  switch_stream_handle_t *stream);

int prepare_transport(struct dig *dig, char const *tport);

int count_transports(struct dig *dig,
					 char const *tp1,
					 char const *tp2);

void _usage(int exitcode, switch_stream_handle_t *stream)
{
	stream->write_function(stream, "%s", "usage: sofia_dig [OPTIONS] [@dnsserver] uri\n");
}

#define usage(_x) _usage(_x, stream); goto fail

switch_status_t sip_dig_function(_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)

{
	int o_sctp = 1, o_tls_sctp = 1, o_verbatim = 1;
	int family = 0, multiple = 0;
	char const *string;
	url_t *uri = NULL;

	#define DIG_MAX_ARGS 50
	char const *host;
	char const *port;
	char *transport = NULL, tport[32];
	char *argv_[DIG_MAX_ARGS + 1] = { 0 };
	int argc;
	int i;
	char *mycmd = NULL;
	char **argv;
	struct dig dig[1] = {{ NULL }};
	su_home_t *home = NULL;
	int xml = 0;

	home = su_home_new(sizeof(*home));

	argv = argv_;
	argv++;

	if (zstr(cmd)) {
		usage(1);
	}

	mycmd = strdup(cmd);

	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv_) / sizeof(argv_[0])) - 1);
	argv = argv_;
	argc++;
	argv[0] = "sofia_dig";
	i = 1;

	if (argc < 2 || argc == (DIG_MAX_ARGS + 1)) {
		usage(1);
	}
	
	if (!strcasecmp(argv[i], "xml")) {
		switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "xml", "true");
		i++;
		xml++;
	}

	while (argv[i] && argv[i][0] == '-') {
		if (strcmp(argv[i], "-v") == 0) {
			o_verbatim++;
		} else if (strcmp(argv[i], "-6") == 0) {
			dig->ip6 = ++family;
		} else if (strcmp(argv[i], "-4") == 0) {
			dig->ip4 = ++family;
		} else if (strncmp(argv[i], "-p", 2) == 0) {
			char const *proto;

			if (argv[i][2] == '=') {
				proto = argv[i] + 3;
			} else if (argv[i][2]) {
				proto = argv[i] + 2;
			} else {
				i++;
				proto = argv[i];
			}

			if (proto == NULL) {
				usage(2);
			}

			if (prepare_transport(dig, proto) < 0) {
				goto fail;
			}
		} else if (strcmp(argv[i], "--udp") == 0) {
			prepare_transport(dig, "udp");
		} else if (strcmp(argv[i], "--tcp") == 0) {
			prepare_transport(dig, "tcp");
		} else if (strcmp(argv[i], "--tls") == 0) {
			prepare_transport(dig, "tls");
		} else if (strcmp(argv[i], "--sctp") == 0) {
			prepare_transport(dig, "sctp");
		} else if (strcmp(argv[i], "--tls-sctp") == 0) {
			prepare_transport(dig, "tls-sctp");
		} else if (strcmp(argv[i], "--tls-udp") == 0) {
			prepare_transport(dig, "tls-udp");
		} else if (strcmp(argv[i], "--no-sctp") == 0) {
			o_sctp = 0, o_tls_sctp = 0;
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(0);
		} else if (strcmp(argv[i], "-h") == 0) {
			usage(0);
		} else if (strcmp(argv[i], "-?") == 0) {
			usage(0);
		} else if (strcmp(argv[i], "-") == 0) {
			i++;
			break;
		} else {
			usage(2);
		}
		i++;
	}


	if (xml) {
		stream->write_function(stream, "%s", "<routes>\n");
	} else {
		stream->write_function(stream, "%10s\t%10s\t%10s\t%10s\t%10s\n", "Preference", "Weight", "Transport", "Port", "Address");
		stream->write_function(stream, "================================================================================\n");
	}

	if (!family)
		dig->ip4 = 1, dig->ip6 = 2;


	if (!argv[i]) {
		usage(2);
	}

	multiple = argv[i] && argv[i +1];

	if (!count_transports(dig, NULL, NULL)) {
		prepare_transport(dig, "udp");
		prepare_transport(dig, "tcp");
		if (o_sctp)
			 prepare_transport(dig, "sctp");
		prepare_transport(dig, "tls");
		if (o_tls_sctp)
			prepare_transport(dig, "tls-sctp");
	}

	dig->sres = sres_resolver_new(getenv("SRESOLV_CONF"));

	if (!dig->sres) {
		usage(1);
	}
	
	for (; i <= argc && (string = argv[i]); i++) {
		if (multiple)
			stream->write_function(stream, "%s", string);

		uri = url_hdup(home, (void *)string);

		if (uri && uri->url_type == url_unknown)
			url_sanitize(uri);

		if (uri && uri->url_type == url_any)
			continue;

		if (!uri || (uri->url_type != url_sip && uri->url_type != url_sips)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: invalid uri\n", string);
			continue;
		}

		port = url_port(uri);
		if (port && !port[0]) port = NULL;
		if (url_param(uri->url_params, "transport=", tport, sizeof tport) > 0)
			transport = tport;

		host = uri->url_host;

		if (host_is_ip_address(host)) {
			if (transport) {
				print_result(host, port, transport, 1.0, 1, stream);
			}
			else if (uri->url_type == url_sips) {
				print_result(host, port, "tls", 1.0, 1, stream);
			}
			else {
				print_result(host, port, "udp", 1.0, 1, stream);
				print_result(host, port, "tcp", 1.0, 2, stream);
			}
			continue;
		}

		if (!host_is_domain(host)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: invalid host\n", string);
			continue;
		}

		dig->sips = uri->url_type == url_sips;
		dig->preference = 1;

		if (!port && !transport && dig_naptr(dig, host, 1.0, stream))
			continue /* resolved naptr */;
		else if (!port && dig_all_srvs(dig, transport, host, 1.0, stream))
			continue /* resolved srv */;
		else if (dig_addr(dig, transport, host, port, 1.0, stream))
			continue /* resolved a/aaaa */;

		stream->write_function(stream, "-ERR: %s: not found\n", string);
	}

	if (xml) {
		stream->write_function(stream, "%s", "</routes>\n");
	}


 fail:
	su_home_unref(home);
	sres_resolver_unref(dig->sres);
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}


int transport_is_secure(char const *tportname)
{
	return su_casenmatch(tportname, "tls", 3);
}

int prepare_transport(struct dig *dig, char const *tport)
{
	struct transport *tports = dig->tports;
	int j;

	for (j = 0; j < N_TPORT; j++) {
		if (!tports[j].name)
			break;
		if (su_casematch(tports[j].name, tport))
			return 1;
	}

	if (j == N_TPORT)
		return 0;

	if (strchr(tport, '/')) {
		char *service = strchr(tport, '/');
		char *srv = strchr(service + 1, '/');

		if (!srv || srv[strlen(srv) - 1] != '.') {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: invalid transport specifier \"%s\"\n"
						
							  "\tspecifier should have name/service/srv-id\n"
							  "\twhere name is protocol name (e.g, \"tls-udp\")\n"
							  "\t      service specifies service as per RFC 2915 (e.g., \"SIPS+D2U\")\n"
							  "\t      srv-id is prefix for SRV lookup (e.g., \"_sips._udp.\")\n%s", 
							  name, 
							  tport,
							  srv ? "\t      and it should end with a dot \".\"\n" : "");

			return -1;
		}

		*service++ = '\0', *srv++ = '\0';

		tports[j].name = tport,
			tports[j].service = service;
		tports[j].srv = srv;
	}
	else if (su_casematch(tport, "udp")) {
		tports[j].name = "udp";
		tports[j].service = "SIP+D2U";
		tports[j].srv = "_sip._udp.";
	}
	else if (su_casematch(tport, "tcp")) {
		tports[j].name = "tcp";
		tports[j].service = "SIP+D2T";
		tports[j].srv = "_sip._tcp.";
	}
	else if (su_casematch(tport, "tls")) {
		tports[j].name = "tls";
		tports[j].service = "SIPS+D2T";
		tports[j].srv = "_sips._tcp.";
	}
	else if (su_casematch(tport, "sctp")) {
		tports[j].name = "sctp";
		tports[j].service = "SIP+D2S";
		tports[j].srv = "_sip._sctp.";
	}
	else if (su_casematch(tport, "tls-sctp")) {
		tports[j].name = "tls-sctp";
		tports[j].service = "SIPS+D2S";
		tports[j].srv = "_sips._sctp.";
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: unknown transport \"%s\"\n", name, tport);
		return -1;
	}

	j++;

	tports[j].service = tports[j].srv = tports[j].name = NULL;

	return 1;
}

int
count_transports(struct dig *dig,
				 char const *tport,
				 char const *tport2)
{

	int i, tcount = 0;
	struct transport const *tports = dig->tports;

	for (i = 0; tports[i].name; i++) {
		if (dig->sips && !transport_is_secure(tports[i].name))
			continue;
		if (!tport || su_casematch(tport, tports[i].name))
			tcount++;
		else if (tport2 && su_casematch(tport2, tports[i].name))
			tcount++;
	}

	return tcount;
}

struct transport const *
transport_by_service(struct transport const *tports, char const *s)
{
	int i;

	for (i = 0; tports[i].name; i++) {
		if (su_casematch(tports[i].service, s))
			return tports + i;
	}

	return NULL;
}

int dig_naptr(struct dig *dig,
			  char const *host,
			  double weight,
			  switch_stream_handle_t *stream)
{
	sres_record_t **answers = NULL;
	struct transport const *tp;
	int i, error;
	int order = 0, count = 0, nacount = 0, scount = 0;

	error = sres_blocking_query(dig->sres, sres_type_naptr, host, 0, &answers);
	if (error < 0)
		return 0;

	/* Sort by priority */
	sres_sort_answers(dig->sres, answers);

	/* Count number of matching naptrs */
	for (i = 0; answers[i]; i++) {
		sres_naptr_record_t const *na = answers[i]->sr_naptr;

		if (na->na_record->r_type != sres_type_naptr || na->na_record->r_status)
			continue;

		if (dig->print)
			stream->write_function(stream, "%s\n\t%d IN NAPTR %u %u \"%s\" \"%s\" \"%s\" %s\n",
								   na->na_record->r_name, na->na_record->r_ttl,
								   na->na_order, na->na_prefer,
								   na->na_flags, na->na_services,
								   na->na_regexp, na->na_replace);

		if (!su_casematch(na->na_flags, "s") && !su_casematch(na->na_flags, "a"))
			continue;

		if (nacount && order != na->na_order)
			continue;

		if (dig->sips && !su_casenmatch(na->na_services, "SIPS+", 5))
			continue;

		if (!transport_by_service(dig->tports, na->na_services))
			continue;

		order = na->na_order, nacount++;
	}

	if (nacount == 0) {
		sres_free_answers(dig->sres, answers);
		return 0;
	}

	for (i = 0; answers[i]; i++) {
		sres_naptr_record_t const *na = answers[i]->sr_naptr;

		if (na->na_record->r_type != sres_type_naptr || na->na_record->r_status)
			continue;
		if (order != na->na_order)
			continue;
		if (!su_casematch(na->na_flags, "s") && !su_casematch(na->na_flags, "a"))
			continue;
		if (dig->sips && !su_casenmatch(na->na_services, "SIPS+", 5))
			continue;

		tp = transport_by_service(dig->tports, na->na_services);
		if (!tp)
			continue;

		if (su_casematch(na->na_flags, "s")) {
			scount = dig_srv(dig, tp->name, na->na_replace, weight / nacount, stream);
		}
		else if (su_casematch(na->na_flags, "a")) {
			scount = dig_addr(dig, tp->name, na->na_replace, NULL, weight / nacount, stream);
		}
		else
			scount = 0;

		count += scount;
	}

	return count;
}

int dig_all_srvs(struct dig *dig,
				 char const *tport,
				 char const *host,
				 double weight,
				 switch_stream_handle_t *stream)
{
	int i, j, n;
	int tcount, count = 0, scount;
	char *domain;

	struct {
		char const *proto; sres_record_t **answers;
	} srvs[N_TPORT + 1] = {{ NULL }};

	tcount = count_transports(dig, tport, NULL);
	if (!tcount)
		return 0;

	for (i = 0, n = 0; dig->tports[i].name; i++) {
		if (tport && !su_casematch(dig->tports[i].name, tport))
			continue;

		if (dig->sips && !transport_is_secure(dig->tports[i].name))
			continue;

		domain = su_strcat(NULL, dig->tports[i].srv, host);

		if (domain) {
			if (sres_blocking_query(dig->sres, sres_type_srv, domain, 0,
									&srvs[n].answers) >= 0) {
				srvs[n++].proto = dig->tports[i].name;
			}
			free(domain);
		}
	}

	if (n == 0)
		return 0;

	for (i = 0; i < n; i++) {
		unsigned priority = 0, pweight = 0, m = 0;
		sres_record_t **answers = srvs[i].answers;
		char const *tport = srvs[i].proto;

		for (j = 0; answers[j]; j++) {
			sres_srv_record_t const *srv = answers[j]->sr_srv;

			if (srv->srv_record->r_type != sres_type_srv)
				continue;
			if (srv->srv_record->r_status != 0)
				continue;

			if (srv->srv_priority != priority && pweight != 0) {
				scount = dig_srv_at(dig, tport, answers, weight / n, pweight,
									priority, stream);
				if (scount) dig->preference++;
				count += scount;
				pweight = 0, m = 0;
			}

			priority = srv->srv_priority, pweight += srv->srv_weight, m++;
		}

		if (m) {
			scount = dig_srv_at(dig, tport, answers, weight / n, pweight, priority, stream);
			if (scount)
				dig->preference++;
			count += scount;
		}
	}

	return count;
}

int dig_srv(struct dig *dig,
			char const *tport,
			char const *domain,
			double weight,
			switch_stream_handle_t *stream)
{
	sres_record_t **answers = NULL;
	int j, n, error;
	int count = 0, scount = 0;

	uint32_t priority, pweight;

	assert(tport && domain);

	error = sres_blocking_query(dig->sres, sres_type_srv, domain, 0, &answers);
	if (error < 0)
		return 0;

	/* Sort by priority */
	sres_sort_answers(dig->sres, answers);

	priority = 0; pweight = 0; n = 0;

	for (j = 0; answers[j]; j++) {
		sres_srv_record_t const *srv = answers[j]->sr_srv;

		if (srv->srv_record->r_type != sres_type_srv)
			continue;
		if (srv->srv_record->r_status != 0)
			continue;

		if (srv->srv_priority != priority && pweight != 0) {
			scount = dig_srv_at(dig, tport, answers, weight, pweight,
								priority, stream);
			if (scount) dig->preference++;
			count += scount;
			pweight = 0, n = 0;
		}

		priority = srv->srv_priority, pweight += srv->srv_weight, n++;
	}

	if (n) {
		scount = dig_srv_at(dig, tport, answers, weight, pweight, priority, stream);
		if (scount) dig->preference++;
		count += scount;
	}

	sres_free_answers(dig->sres, answers);

	return count;
}

int dig_srv_at(struct dig *dig,
			   char const *tport,
			   sres_record_t **answers,
			   double weight, int pweight,
			   int priority,
			   switch_stream_handle_t *stream)
{
	int count = 0;
	int i;
	char port[8];

	if (pweight == 0)
		pweight = 1;

	for (i = 0; answers[i]; i++) {
		sres_srv_record_t const *srv = answers[i]->sr_srv;
		if (srv->srv_record->r_type != sres_type_srv)
			continue;
		if (srv->srv_record->r_status != 0)
			continue;
		if (srv->srv_priority != priority)
			continue;
		snprintf(port, sizeof port, "%u", srv->srv_port);

		count += dig_addr(dig, tport, srv->srv_target, port,
						  weight * srv->srv_weight / pweight, stream);
	}

	return count;
}

int dig_addr(struct dig *dig,
			 char const *tport,
			 char const *host,
			 char const *port,
			 double weight,
			 switch_stream_handle_t *stream)
{
	int error, i;
	char const *tport2 = NULL;
	sres_record_t **answers1 = NULL, **answers2 = NULL;
	unsigned count1 = 0, count2 = 0, tcount = 0;
	uint16_t type1 = 0, type2 = 0, family1 = 0, family2 = 0;

	if (dig->ip6 > dig->ip4) {
		type1 = sres_type_aaaa, family1 = AF_INET6;
		if (dig->ip4)
			type2 = sres_type_a, family2 = AF_INET;
	}
	else {
		type1 = sres_type_a, family1 = AF_INET;
		if (dig->ip6)
			type2 = sres_type_aaaa, family2 = AF_INET6;
	}

	if (tport == NULL) {
		if (dig->sips)
			tport = "tls";
		else
			tport = "udp", tport2 = "tcp";
	}

	tcount = count_transports(dig, tport, tport2);
	if (!tcount)
		return 0;

	if (type1) {
		error = sres_blocking_query(dig->sres, type1, host, 0, &answers1);
		if (error >= 0)
			for (i = 0; answers1[i]; i++) {
				sres_common_t *r = answers1[i]->sr_record;
				count1 += r->r_type == type1 &&	r->r_status == 0;
			}
	}

	if (type2) {
		error = sres_blocking_query(dig->sres, type2, host, 0, &answers2);
		if (error >= 0)
			for (i = 0; answers2[i]; i++) {
				sres_common_t *r = answers2[i]->sr_record;
				count2 += r->r_type == type2 &&	r->r_status == 0;
			}
	}

	if (count1 + count2) {
		double w = weight / (count1 + count2) / tcount;

		if (count1)
			print_addr_results(dig->tports, tport, tport2,
							   answers1, type1, family1, port,
							   w, dig->preference, stream);
		if (count2)
			print_addr_results(dig->tports, tport, tport2,
							   answers2, type2, family2, port,
							   w, dig->preference, stream);
	}

	sres_free_answers(dig->sres, answers1);
	sres_free_answers(dig->sres, answers2);

	return count1 + count2;
}

void
print_addr_results(struct transport const *tports,
				   char const *tport, char const *tport2,
				   sres_record_t **answers, int type, int af,
				   char const *port,
				   double weight, int preference, switch_stream_handle_t *stream)
{
	int i, j;
	char addr[64];

	for (i = 0; answers[i]; i++) {
		if (answers[i]->sr_record->r_type != type)
			continue;
		if (answers[i]->sr_record->r_status != 0)
			continue;

		su_inet_ntop(af, &answers[i]->sr_a->a_addr, addr, sizeof addr);

		for (j = 0; tports[j].name; j++) {
			if (su_casematch(tport, tports[j].name))
				print_result(addr, port, tport, weight, preference, stream);
			if (su_casematch(tport2, tports[j].name))
				print_result(addr, port, tport2, weight, preference, stream);
		}
	}
}

void print_result(char const *addr,
				  char const *port,
				  char const *tport,
				  double weight,
				  unsigned preference,
				  switch_stream_handle_t *stream)
{
	int xml = switch_true(switch_event_get_header(stream->param_event, "xml"));

	if (!port || !port[0])
		port = transport_is_secure(tport) ? "5061" : "5060";

	if (xml) {
		stream->write_function(stream, 
							   " <route>\n"
							   "  <preference>%u</preference>\n"
							   "  <weight>%.3f</weight>\n"
							   "  <transport>%s</transport>\n"
							   "  <port>%s</port>\n"
							   "  <address>%s</address>\n"
							   " </route>\n",
							   preference, weight, tport, port, addr);
	} else {
		stream->write_function(stream, "%10u\t%10.3f\t%10s\t%10s\t%10s\n", preference, weight, tport, port, addr);
	}
}


