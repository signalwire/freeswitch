/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Brian K. West <brian@freeswitch.org>
 * Rupa Schomaker <rupa@rupa.com>
 *
 *
 * switch_nat.c NAT Traversal via NAT-PMP or uPNP
 *
 */

#include <switch.h>
#include "../libs/miniupnpc/miniwget.h"
#include "../libs/miniupnpc/miniupnpc.h"
#include "../libs/miniupnpc/upnpcommands.h"
#include "../libs/miniupnpc/upnperrors.h"
#include "../libs/libnatpmp/natpmp.h"

#define MULTICAST_BUFFSIZE 65536
#define IP_LEN 16
#define NAT_REFRESH_INTERVAL 900

typedef struct {
	switch_nat_type_t nat_type;
	char nat_type_str[5];
	struct UPNPUrls urls;
	struct IGDdatas data;
	char *descURL;
	char pub_addr[IP_LEN];
	char pvt_addr[IP_LEN];
	switch_bool_t mapping;
} nat_globals_t;

static nat_globals_t nat_globals;

typedef struct {
	switch_memory_pool_t *pool;
	int running;
	switch_sockaddr_t *maddress;
	switch_socket_t *msocket;
} nat_globals_perm_t;

static nat_globals_perm_t nat_globals_perm;

static switch_bool_t first_init = SWITCH_TRUE;
static switch_bool_t initialized = SWITCH_FALSE;

static switch_status_t get_upnp_pubaddr(char *pub_addr)
{
	if (UPNP_GetExternalIPAddress(nat_globals.urls.controlURL, nat_globals.data.servicetype, pub_addr) == UPNPCOMMAND_SUCCESS) {
		if (!strcmp(pub_addr, "0.0.0.0") || zstr_buf(pub_addr)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "uPNP Device (url: %s) returned an invalid external address of '%s'.  Disabling uPNP\n", nat_globals.urls.controlURL,
							  pub_addr);
			return SWITCH_STATUS_GENERR;
		}
	} else {
		return SWITCH_STATUS_GENERR;
	}
	return SWITCH_STATUS_SUCCESS;
}

static int init_upnp(void)
{
	struct UPNPDev *devlist;
	struct UPNPDev *dev = NULL;
	struct UPNPDev *trydev = NULL;
	char *descXML;
	int descXMLsize = 0;
	const char *minissdpdpath = switch_core_get_variable("local_ip_v4");

	memset(&nat_globals.urls, 0, sizeof(struct UPNPUrls));
	memset(&nat_globals.data, 0, sizeof(struct IGDdatas));

	devlist = upnpDiscover(3000, NULL, minissdpdpath, 0);

	if (devlist) {
		dev = devlist;
		while (dev) {
			if (strstr(dev->st, "InternetGatewayDevice")) {
				break;
			}
			if (!trydev && !switch_stristr("printer", dev->descURL)) {
				trydev = dev;
			}

			dev = dev->pNext;
		}

	}

	if (!dev && trydev) {
		dev = trydev; /* defaulting to first device */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No InternetGatewayDevice, using first entry as default (%s).\n", dev->descURL);
	} else if (devlist && !dev && !trydev) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No InternetGatewayDevice found and I am NOT going to try your printer because printers should not route to the internet, that would be DAFT\n");
	}
	
	if (dev) {
		descXML = miniwget(dev->descURL, &descXMLsize);

		nat_globals.descURL = strdup(dev->descURL);

		if (descXML) {
			parserootdesc(descXML, descXMLsize, &nat_globals.data);
			free(descXML);
			descXML = 0;
			GetUPNPUrls(&nat_globals.urls, &nat_globals.data, dev->descURL);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unable to retrieve device description XML (%s).\n", dev->descURL);
		}

		freeUPNPDevlist(devlist);
	}

	if (get_upnp_pubaddr(nat_globals.pub_addr) == SWITCH_STATUS_SUCCESS) {
		nat_globals.nat_type = SWITCH_NAT_TYPE_UPNP;
		return 0;
	}

	return -2;
}

static int get_pmp_pubaddr(char *pub_addr)
{
	int r = 0, i = 0, max = 5;
	natpmpresp_t response;
	char *pubaddr = NULL;
	fd_set fds;
	natpmp_t natpmp;
	const char *err = NULL;

	if ((r = initnatpmp(&natpmp)) < 0) {
		err = "init failed";
		goto end;
	}

	if ((r = sendpublicaddressrequest(&natpmp)) < 0) {
		err = "pub addr req failed";
		goto end;
	}

	do {
		struct timeval timeout = { 1, 0 };
		i++;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking for PMP %d/%d\n", i, max);

		FD_ZERO(&fds);
		FD_SET(natpmp.s, &fds);

		if ((r = getnatpmprequesttimeout(&natpmp, &timeout)) < 0) {
			err = "get timeout failed";
			goto end;
		}

		if ((r = select(FD_SETSIZE, &fds, NULL, NULL, &timeout)) < 0) {
			err = "select failed";
			goto end;
		}
		r = readnatpmpresponseorretry(&natpmp, &response);
	} while (r == NATPMP_TRYAGAIN && i < max);

	if (r < 0) {
		err = "general error";
		goto end;
	}

	pubaddr = inet_ntoa(response.pnu.publicaddress.addr);
	switch_copy_string(pub_addr, pubaddr, IP_LEN);
	nat_globals.nat_type = SWITCH_NAT_TYPE_PMP;

	closenatpmp(&natpmp);

  end:

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error checking for PMP [%s]\n", err);
	}

	return r;
}

static int init_pmp(void)
{
	return get_pmp_pubaddr(nat_globals.pub_addr);
}

SWITCH_DECLARE(void) switch_nat_set_mapping(switch_bool_t mapping)
{
	nat_globals.mapping = mapping;
}

SWITCH_DECLARE(void) switch_nat_reinit(void)
{
	switch_nat_init(nat_globals_perm.pool, nat_globals.mapping);
}

switch_status_t init_nat_monitor(switch_memory_pool_t *pool)
{
	char *addr = NULL;
	switch_port_t port = 0;

	if (nat_globals.nat_type == SWITCH_NAT_TYPE_UPNP) {
		addr = "239.255.255.250";
		port = 1900;
	} else if (nat_globals.nat_type == SWITCH_NAT_TYPE_PMP) {
		addr = "224.0.0.1";
		port = 5350;
	}

	if (switch_sockaddr_info_get(&nat_globals_perm.maddress, addr, SWITCH_UNSPEC, port, 0, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find address\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_socket_create(&nat_globals_perm.msocket, AF_INET, SOCK_DGRAM, 0, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_socket_opt_set(nat_globals_perm.msocket, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Option Error\n");
		switch_socket_close(nat_globals_perm.msocket);
		return SWITCH_STATUS_TERM;
	}

	if (switch_mcast_join(nat_globals_perm.msocket, nat_globals_perm.maddress, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
		switch_socket_close(nat_globals_perm.msocket);
		return SWITCH_STATUS_TERM;
	}

	if (switch_socket_bind(nat_globals_perm.msocket, nat_globals_perm.maddress) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bind Error\n");
		switch_socket_close(nat_globals_perm.msocket);
		return SWITCH_STATUS_TERM;
	}

	switch_socket_opt_set(nat_globals_perm.msocket, SWITCH_SO_NONBLOCK, TRUE);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NAT thread configured\n");
	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC switch_nat_multicast_runtime(switch_thread_t * thread, void *obj)
{
	char *buf = NULL;
	char newip[16] = "";
	char *pos;
	switch_event_t *event = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NAT thread started\n");

	buf = (char *) malloc(MULTICAST_BUFFSIZE);
	switch_assert(buf);
	nat_globals_perm.running = 1;

	while (nat_globals_perm.running == 1) {
		size_t len = MULTICAST_BUFFSIZE;
		switch_status_t status;
		switch_bool_t do_repub = SWITCH_FALSE;
		memset(buf, 0, len);

		status = switch_socket_recvfrom(nat_globals_perm.maddress, nat_globals_perm.msocket, 0, buf, &len);

		if (!len) {
			if (SWITCH_STATUS_IS_BREAK(status)) {
				switch_yield(5000000);
				continue;
			}

			break;
		}

		if (nat_globals.nat_type == SWITCH_NAT_TYPE_UPNP) {
			/* look for our desc URL and servicetype in the packet */
			if (strstr(buf, nat_globals.descURL) && (buf == NULL || strstr(buf, nat_globals.data.servicetype))) {
				if ((pos = strstr(buf, "NTS:"))) {
					pos = pos + 4;
					while (*pos && *pos == ' ') {
						pos++;
					}
					if (!strncmp(pos, "ssdp:alive", 10)) {
						/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got UPnP keep alive packet: \n%s\n", buf); */
						/* did pub ip change */
						newip[0] = '\0';
						if (get_upnp_pubaddr(newip) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
											  "Unable to get current pubaddr after receiving UPnP keep alive packet.\n");
						}
					} else if (!strncmp(pos, "ssdp:byebye", 11)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
										  "got UPnP signoff packet.  Your NAT gateway is probably going offline.\n");
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got UPnP signoff packet: \n%s\n", buf);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got UNKNOWN UPnP keep alive packet: \n%s\n", buf);
					}
				}
			}
		} else {
			/* got some data in NAT-PMP mode, treat any data as a republish event */
			if (get_pmp_pubaddr(newip) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to get current pubaddr after receiving UPnP keep alive packet.\n");
			}
		}

		if ((strlen(newip) > 0) && strcmp(newip, "0.0.0.0") && strcmp(newip, nat_globals.pub_addr)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Public IP changed from '%s' to '%s'.\n", nat_globals.pub_addr, newip);
			do_repub = SWITCH_TRUE;

			switch_event_create(&event, SWITCH_EVENT_TRAP);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "network-external-address-change");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "network-external-address-previous-v4", nat_globals.pub_addr);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "network-external-address-change-v4", newip);
			switch_event_fire(&event);

			switch_set_string(nat_globals.pub_addr, newip);
			switch_nat_reinit();
		}

		if (do_repub) {
			switch_nat_republish();
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NAT thread ending\n");
	nat_globals_perm.running = 0;

	switch_safe_free(buf);

	return NULL;
}

switch_thread_t *nat_thread_p = NULL;

SWITCH_DECLARE(void) switch_nat_thread_start(void)
{

	switch_threadattr_t *thd_attr;

	if (init_nat_monitor(nat_globals_perm.pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to initialize NAT thread\n");
		return;
	}

	switch_threadattr_create(&thd_attr, nat_globals_perm.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_thread_create(&nat_thread_p, thd_attr, switch_nat_multicast_runtime, NULL, nat_globals_perm.pool);
}

SWITCH_DECLARE(void) switch_nat_thread_stop(void)
{
	/* don't do anything if no thread ptr */
	if (!nat_thread_p) {
		return;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Stopping NAT Task Thread\n");
	if (nat_globals_perm.running == 1) {
		int sanity = 0;
		switch_status_t st;

		nat_globals_perm.running = -1;

		switch_thread_join(&st, nat_thread_p);

		while (nat_globals_perm.running) {
			switch_yield(1000000);	/* can take up to 5s for the thread to terminate, so wait for 10 */
			if (++sanity > 10) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timed out waiting for NAT Task Thread to stop\n");
				break;
			}
		}
	}

	nat_thread_p = NULL;
}


SWITCH_DECLARE(void) switch_nat_init(switch_memory_pool_t *pool, switch_bool_t mapping)
{
	/* try free dynamic data structures prior to resetting to 0 */
	FreeUPNPUrls(&nat_globals.urls);
	switch_safe_free(nat_globals.descURL);

	memset(&nat_globals, 0, sizeof(nat_globals));

	if (first_init) {
		memset(&nat_globals_perm, 0, sizeof(nat_globals_perm));
		nat_globals_perm.pool = pool;
	}

	nat_globals.mapping = mapping;

	switch_find_local_ip(nat_globals.pvt_addr, sizeof(nat_globals.pvt_addr), NULL, AF_INET);


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Scanning for NAT\n");

	init_pmp();

	if (!nat_globals.nat_type) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking for UPnP\n");
		init_upnp();
	}

	if (nat_globals.nat_type) {
		switch_core_set_variable("nat_public_addr", nat_globals.pub_addr);
		switch_core_set_variable("nat_private_addr", nat_globals.pvt_addr);
		switch_core_set_variable("nat_type", nat_globals.nat_type == SWITCH_NAT_TYPE_PMP ? "pmp" : "upnp");
		strncpy(nat_globals.nat_type_str, nat_globals.nat_type == SWITCH_NAT_TYPE_PMP ? "pmp" : "upnp", sizeof(nat_globals.nat_type_str) - 1);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "NAT detected type: %s, ExtIP: '%s'\n",
						  nat_globals.nat_type == SWITCH_NAT_TYPE_PMP ? "pmp" : "upnp", nat_globals.pub_addr);

		if (!nat_thread_p) {
			switch_nat_thread_start();
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No PMP or UPnP NAT devices detected!\n");
	}
	first_init = SWITCH_FALSE;
	initialized = SWITCH_TRUE;
}

static switch_status_t switch_nat_add_mapping_pmp(switch_port_t port, switch_nat_ip_proto_t proto, switch_port_t * external_port)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	natpmpresp_t response;
	int r;
	natpmp_t natpmp;

	initnatpmp(&natpmp);

	if (proto == SWITCH_NAT_TCP) {
		sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_TCP, port, port, 31104000);
	} else if (proto == SWITCH_NAT_UDP) {
		sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_UDP, port, port, 31104000);
	}

	do {
		fd_set fds;
		struct timeval timeout = { 1, 0 };
		FD_ZERO(&fds);
		FD_SET(natpmp.s, &fds);
		getnatpmprequesttimeout(&natpmp, &timeout);
		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		r = readnatpmpresponseorretry(&natpmp, &response);
	} while (r == NATPMP_TRYAGAIN);

	if (r == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "mapped public port %hu protocol %s to localport %hu\n",
						  response.pnu.newportmapping.mappedpublicport,
						  response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
						  (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" : "UNKNOWN"), response.pnu.newportmapping.privateport);
		if (external_port) {
			*external_port = response.pnu.newportmapping.mappedpublicport;
		} else if (response.pnu.newportmapping.mappedpublicport != response.pnu.newportmapping.privateport) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "External port %hu protocol %s was not available, it was instead mapped to %hu\n",
							  response.pnu.newportmapping.privateport,
							  response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
							  (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" : "UNKNOWN"), response.pnu.newportmapping.mappedpublicport);
		}

		status = SWITCH_STATUS_SUCCESS;
	}

	closenatpmp(&natpmp);

	return status;
}

static switch_status_t switch_nat_add_mapping_upnp(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char port_str[IP_LEN];
	int r = UPNPCOMMAND_UNKNOWN_ERROR;

	sprintf(port_str, "%d", port);

	if (proto == SWITCH_NAT_TCP) {
		r = UPNP_AddPortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, port_str,
								nat_globals.pvt_addr, "FreeSWITCH", "TCP", 0);
	} else if (proto == SWITCH_NAT_UDP) {
		r = UPNP_AddPortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, port_str,
								nat_globals.pvt_addr, "FreeSWITCH", "UDP", 0);
	}

	if (r == UPNPCOMMAND_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mapped public port %s protocol %s to localport %s\n", port_str,
						  (proto == SWITCH_NAT_TCP) ? "TCP" : (proto == SWITCH_NAT_UDP ? "UDP" : "UNKNOWN"), port_str);
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

static switch_status_t switch_nat_del_mapping_pmp(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	natpmpresp_t response;
	int r;
	natpmp_t natpmp;

	initnatpmp(&natpmp);

	if (proto == SWITCH_NAT_TCP) {
		sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_TCP, port, port, 0);
	} else if (proto == SWITCH_NAT_UDP) {
		sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_UDP, port, port, 0);
	}

	do {
		fd_set fds;
		struct timeval timeout;
		FD_ZERO(&fds);
		FD_SET(natpmp.s, &fds);
		getnatpmprequesttimeout(&natpmp, &timeout);
		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		r = readnatpmpresponseorretry(&natpmp, &response);
	} while (r == NATPMP_TRYAGAIN);

	if (r == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unmapped public port %hu protocol %s to localport %hu\n", response.pnu.newportmapping.privateport,	/* This might be wrong but its so 0 isn't displayed */
						  response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
						  (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" : "UNKNOWN"), response.pnu.newportmapping.privateport);
		status = SWITCH_STATUS_SUCCESS;
	}

	closenatpmp(&natpmp);

	return status;
}

static switch_status_t switch_nat_del_mapping_upnp(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char port_str[IP_LEN];
	int r = UPNPCOMMAND_UNKNOWN_ERROR;

	sprintf(port_str, "%d", port);

	if (proto == SWITCH_NAT_TCP) {
		r = UPNP_DeletePortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, "TCP", 0);
	} else if (proto == SWITCH_NAT_UDP) {
		r = UPNP_DeletePortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, "UDP", 0);
	}

	if (r == UPNPCOMMAND_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unmapped public port %s protocol %s to localport %s\n", port_str,
						  (proto == SWITCH_NAT_TCP) ? "TCP" : (proto == SWITCH_NAT_UDP ? "UDP" : "UNKNOWN"), port_str);
		status = SWITCH_STATUS_SUCCESS;
	}
	return status;
}

SWITCH_DECLARE(const char *) switch_nat_get_type(void)
{
	return nat_globals.nat_type_str;
}

SWITCH_DECLARE(switch_status_t) switch_nat_add_mapping_internal(switch_port_t port, switch_nat_ip_proto_t proto, switch_port_t * external_port,
																switch_bool_t sticky, switch_bool_t publish)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;

	if (!nat_globals.mapping) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "NAT port mapping disabled\n");
		return status;
	}

	switch (nat_globals.nat_type) {
	case SWITCH_NAT_TYPE_PMP:
		status = switch_nat_add_mapping_pmp(port, proto, external_port);
		break;
	case SWITCH_NAT_TYPE_UPNP:
		if ((status = switch_nat_add_mapping_upnp(port, proto)) == SWITCH_STATUS_SUCCESS) {
			if (external_port) {
				*external_port = port;
			}
		}
		break;
	default:
		break;
	}

	if (publish && status == SWITCH_STATUS_SUCCESS) {
		switch_event_create(&event, SWITCH_EVENT_NAT);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "op", "add");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "port", "%d", port);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "%d", proto);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sticky", (sticky ? "true" : "false"));
		switch_event_fire(&event);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_nat_add_mapping(switch_port_t port, switch_nat_ip_proto_t proto, switch_port_t * external_port,
													   switch_bool_t sticky)
{
	return switch_nat_add_mapping_internal(port, proto, external_port, sticky, SWITCH_TRUE);
}

SWITCH_DECLARE(switch_status_t) switch_nat_del_mapping(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;

	switch (nat_globals.nat_type) {
	case SWITCH_NAT_TYPE_PMP:
		status = switch_nat_del_mapping_pmp(port, proto);
		break;
	case SWITCH_NAT_TYPE_UPNP:
		status = switch_nat_del_mapping_upnp(port, proto);
		break;
	default:
		break;
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_event_create(&event, SWITCH_EVENT_NAT);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "op", "del");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "port", "%d", port);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "%d", proto);
		switch_event_fire(&event);
	}

	return status;
}

SWITCH_DECLARE(void) switch_nat_republish(void)
{
	switch_xml_t natxml = NULL;
	switch_xml_t row = NULL;
	switch_xml_t child = NULL;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Refreshing nat maps\n");

	switch_api_execute("show", "nat_map as xml", NULL, &stream);

	if (!(natxml = switch_xml_parse_str_dup(stream.data))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to parse XML: %s\n", (char *) stream.data);
		switch_safe_free(stream.data);
		return;
	}

	/* iterate the xml and publish the mappings */
	row = switch_xml_find_child(natxml, "row", "row_id", "1");
	while (row != NULL) {
		char *sport = NULL;
		char *sproto = NULL;
		switch_port_t port;
		switch_nat_ip_proto_t proto;

		if ((child = switch_xml_child(row, "port"))) {
			sport = child->txt;
		}
		if ((child = switch_xml_child(row, "proto_num"))) {
			sproto = child->txt;
		}

		if (sport && sproto) {
			port = (switch_port_t) (atoi(sport));
			proto = (switch_nat_ip_proto_t) (atoi(sproto));
			switch_nat_add_mapping_internal(port, proto, NULL, SWITCH_FALSE, SWITCH_FALSE);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to parse port/proto info: XML: %s\n", (char *) stream.data);
		}

		row = switch_xml_next(row);
	}

	switch_safe_free(stream.data);
	switch_xml_free(natxml);
}

SWITCH_STANDARD_SCHED_FUNC(switch_nat_republish_sched)
{
	switch_nat_republish();
	if (nat_globals_perm.running == 1) {
		task->runtime = switch_epoch_time_now(NULL) + NAT_REFRESH_INTERVAL;
	}
}

SWITCH_DECLARE(void) switch_nat_late_init(void)
{
	if (nat_globals_perm.running == 1) {
		switch_scheduler_add_task(switch_epoch_time_now(NULL) + NAT_REFRESH_INTERVAL, switch_nat_republish_sched, "nat_republish", "core", 0, NULL,
								  SSHF_OWN_THREAD);
	}
}

SWITCH_DECLARE(char *) switch_nat_status(void)
{
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	stream.write_function(&stream, "Nat Type: %s, ExtIP: %s\n",
						  (nat_globals.nat_type == SWITCH_NAT_TYPE_UPNP) ? "UPNP" : (nat_globals.nat_type == SWITCH_NAT_TYPE_PMP ? "NAT-PMP" : "UNKNOWN"),
						  nat_globals.pub_addr);

	if (nat_globals.mapping) {
		stream.write_function(&stream, "NAT port mapping enabled.\n");
	} else {
		stream.write_function(&stream, "NAT port mapping disabled.\n");
	}

	switch_api_execute("show", "nat_map", NULL, &stream);

	return stream.data;			/* caller frees */
}

SWITCH_DECLARE(switch_bool_t) switch_nat_is_initialized(void)
{
	return initialized;
}

SWITCH_DECLARE(void) switch_nat_shutdown(void)
{
	switch_nat_thread_stop();
	FreeUPNPUrls(&nat_globals.urls);
	switch_safe_free(nat_globals.descURL);
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
