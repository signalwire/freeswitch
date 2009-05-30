/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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

typedef struct {
	switch_memory_pool_t *pool;
	switch_nat_type_t nat_type;
	natpmp_t natpmp;
	struct UPNPUrls urls;
	struct IGDdatas data;
	char pub_addr[16];
	char pvt_addr[16];
	
} nat_globals_t;

static nat_globals_t nat_globals;

static int init_upnp (void)
{
	struct UPNPDev * devlist;
	struct UPNPDev * dev;
	char * descXML;
	int descXMLsize = 0;
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
	int r = -2;

	memset(&nat_globals.urls, 0, sizeof(struct UPNPUrls));
	memset(&nat_globals.data, 0, sizeof(struct IGDdatas));
	devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);

	if (devlist) {
		dev = devlist;
		while (dev) {
			if (strstr (dev->st, "InternetGatewayDevice"))
				break;
			dev = dev->pNext;
		}
		if (!dev) {
			dev = devlist; /* defaulting to first device */
		}
		
		descXML = miniwget(dev->descURL, &descXMLsize);

		if (descXML) {
			parserootdesc (descXML, descXMLsize, &nat_globals.data);
			free (descXML); descXML = 0;
			GetUPNPUrls (&nat_globals.urls, &nat_globals.data, dev->descURL);
		}

		freeUPNPDevlist(devlist);
	}

	if ((r = UPNP_GetExternalIPAddress(nat_globals.urls.controlURL,
									   nat_globals.data.servicetype,
									   nat_globals.pub_addr)) == UPNPCOMMAND_SUCCESS) {

		nat_globals.nat_type = SWITCH_NAT_TYPE_UPNP;
		return 0;
	}

	return -2;
}

static int init_pmp(void)
{
	int r = 0;
	natpmpresp_t response;
	char *pubaddr = NULL;
	fd_set fds;
	struct timeval timeout;

	initnatpmp(&nat_globals.natpmp);
	r = sendpublicaddressrequest(&nat_globals.natpmp);

	if (r < 0) {
		goto end;
	}

	do {
		FD_ZERO(&fds);
		FD_SET(nat_globals.natpmp.s, &fds);
		getnatpmprequesttimeout(&nat_globals.natpmp, &timeout);
		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		r = readnatpmpresponseorretry(&nat_globals.natpmp, &response);
	} while(r == NATPMP_TRYAGAIN);

	if (r < 0) {
		goto end;
	}

	pubaddr = inet_ntoa(response.pnu.publicaddress.addr);
	switch_set_string(nat_globals.pub_addr, pubaddr);
	nat_globals.nat_type = SWITCH_NAT_TYPE_PMP;
	
	if (r) {
		closenatpmp(&nat_globals.natpmp);
	}

 end:

	return r;
}

SWITCH_DECLARE(void) switch_nat_init(switch_memory_pool_t *pool)
{
	memset(&nat_globals, 0, sizeof(nat_globals));
	nat_globals.pool = pool;

	switch_find_local_ip(nat_globals.pvt_addr, sizeof(nat_globals.pvt_addr), AF_INET);

	init_pmp();

	if (!nat_globals.nat_type) {
		init_upnp();
	}
	
	if (nat_globals.nat_type) {
		switch_core_set_variable("nat_public_addr", nat_globals.pub_addr);
		switch_core_set_variable("nat_private_addr", nat_globals.pvt_addr);
		switch_core_set_variable("nat_type", nat_globals.nat_type == SWITCH_NAT_TYPE_PMP ? "pmp" : "upnp");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No NAT Detected!\n");
	}
}

static switch_status_t switch_nat_add_mapping_pmp(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	natpmpresp_t response;
	int r;

	if (proto == SWITCH_NAT_TCP) {
		sendnewportmappingrequest(&nat_globals.natpmp, NATPMP_PROTOCOL_TCP, port, port, 31104000);
	} else if(proto == SWITCH_NAT_UDP) {
		sendnewportmappingrequest(&nat_globals.natpmp, NATPMP_PROTOCOL_UDP, port, port, 31104000);
	}

	do {
		fd_set fds;
		struct timeval timeout;
		FD_ZERO(&fds);
		FD_SET(nat_globals.natpmp.s, &fds);
		getnatpmprequesttimeout(&nat_globals.natpmp, &timeout);
		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		r = readnatpmpresponseorretry(&nat_globals.natpmp, &response);
	} while(r == NATPMP_TRYAGAIN);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mapped public port %hu protocol %s to localport %hu\n",
					  response.pnu.newportmapping.mappedpublicport,
					  response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
					  (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" : "UNKNOWN"),
					  response.pnu.newportmapping.privateport);
	status = SWITCH_STATUS_SUCCESS;

	return status;
}

static switch_status_t switch_nat_add_mapping_upnp(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char port_str[16];
	int r = UPNPCOMMAND_UNKNOWN_ERROR;

	sprintf(port_str, "%d", port);

	if (proto == SWITCH_NAT_TCP) {
		r = UPNP_AddPortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, port_str, nat_globals.pvt_addr, 0, "TCP", 0);
	} else if(proto == SWITCH_NAT_UDP) {
		r = UPNP_AddPortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, port_str, nat_globals.pvt_addr, 0, "UDP", 0);
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

	if (proto == SWITCH_NAT_TCP) {
		sendnewportmappingrequest(&nat_globals.natpmp, NATPMP_PROTOCOL_TCP, port, port, 0);
	} else if(proto == SWITCH_NAT_UDP) {
		sendnewportmappingrequest(&nat_globals.natpmp, NATPMP_PROTOCOL_UDP, port, port, 0);
	}

	do {
		fd_set fds;
		struct timeval timeout;
		FD_ZERO(&fds);
		FD_SET(nat_globals.natpmp.s, &fds);
		getnatpmprequesttimeout(&nat_globals.natpmp, &timeout);
		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		r = readnatpmpresponseorretry(&nat_globals.natpmp, &response);
	} while(r == NATPMP_TRYAGAIN);
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unmapped public port %hu protocol %s to localport %hu\n",
					  response.pnu.newportmapping.mappedpublicport,
					  response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
					  (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" : "UNKNOWN"),
					  response.pnu.newportmapping.privateport);
	status = SWITCH_STATUS_SUCCESS;

	return status;
}

static switch_status_t switch_nat_del_mapping_upnp(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char port_str[16];
	int r = UPNPCOMMAND_UNKNOWN_ERROR;

	sprintf(port_str, "%d", port);

	if (proto == SWITCH_NAT_TCP) {
		r = UPNP_DeletePortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, "TCP", 0);
	} else if(proto == SWITCH_NAT_UDP) {
		r = UPNP_DeletePortMapping(nat_globals.urls.controlURL, nat_globals.data.servicetype, port_str, "UDP", 0);
	}

	if (r == UPNPCOMMAND_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unmapped public port %s protocol %s to localport %s\n", port_str,
						  (proto == SWITCH_NAT_TCP) ? "TCP" : (proto == SWITCH_NAT_UDP ? "UDP" : "UNKNOWN"), port_str);
		status = SWITCH_STATUS_SUCCESS;
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_nat_add_mapping(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch (nat_globals.nat_type) {
	case SWITCH_NAT_TYPE_PMP:
		status = switch_nat_add_mapping_pmp(port, proto);
		break;
	case SWITCH_NAT_TYPE_UPNP:
		status = switch_nat_add_mapping_upnp(port, proto);
		break;
	default:
		break;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_nat_del_mapping(switch_port_t port, switch_nat_ip_proto_t proto)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

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

	return status;
}

SWITCH_DECLARE(void) switch_nat_shutdown(void)
{
	if (nat_globals.nat_type == SWITCH_NAT_TYPE_PMP) {
		closenatpmp(&nat_globals.natpmp);
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
