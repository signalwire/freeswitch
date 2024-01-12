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

#include "sofia-resolv/sres.h"
#include "sofia-resolv/sres_record.h"

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

switch_bool_t dig_srv_at_simple_verify(struct dig *dig,
			char const *tport,
			sres_record_t **answers, const char *ip, switch_bool_t ipv6);

switch_bool_t dig_all_srvs_simple(struct dig *dig,
								char const *host, const char *ip, switch_bool_t ipv6); 

sres_record_t ** dig_addr_simple(struct dig *dig,
								char const *host,
								uint16_t type); 

switch_bool_t sofia_sip_resolve_compare(const char *domainname, const char *ip);

switch_bool_t verify_ip(sres_record_t **answers, const char *ip, switch_bool_t ipv6);

