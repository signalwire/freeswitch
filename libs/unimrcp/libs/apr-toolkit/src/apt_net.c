/*
 * Copyright 2008-2015 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <apr_network_io.h>
#include "apt_net.h"
#include "apt_log.h"

/** Get the IP address (in numeric address string format) by hostname */
apt_bool_t apt_ip_get(char **addr, apr_pool_t *pool)
{
	apr_sockaddr_t *sockaddr = NULL;
	char hostname[APRMAXHOSTLEN+1];
	if(apr_gethostname(hostname,sizeof(hostname),pool) != APR_SUCCESS) {
		return FALSE;
	}
	if(apr_sockaddr_info_get(&sockaddr,hostname,APR_INET,0,0,pool) != APR_SUCCESS) {
		return FALSE;
	}
	if(apr_sockaddr_ip_get(addr,sockaddr) != APR_SUCCESS) {
		return FALSE;
	}
	return TRUE;
}

#ifdef WIN32
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#endif

/** Get the IP address (in numeric address string format) by network interface name */
apt_bool_t apt_ip_get_by_iface(const char *iface_name, char **addr, apr_pool_t *pool)
{
	apt_bool_t status = FALSE;
#ifdef WIN32
	/* See the usage of GetAdaptersInfo().
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa366314%28v=vs.85%29.aspx
	 */
	IP_ADAPTER_INFO  *pAdapterInfo;
	PIP_ADAPTER_INFO pAdapter;
	ULONG            ulOutBufLen;
	DWORD            dwRetVal;	

	pAdapterInfo = (IP_ADAPTER_INFO *) malloc(sizeof(IP_ADAPTER_INFO));
	ulOutBufLen = sizeof(IP_ADAPTER_INFO);

	/* Make an initial call to GetAdaptersInfo to get the size needed into the ulOutBufLen variable. */
	if(GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) != ERROR_SUCCESS) {
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);
	}

	/* Make a second call to GetAdaptersInfo, passing pAdapterInfo and ulOutBufLen as parameters. */
	if((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) != ERROR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Adapters Info %d", dwRetVal);
		return FALSE;
	}

	/* Walk through linked list, maintaining head pointer. */
	pAdapter = pAdapterInfo;
	while(pAdapter) {
		/* Match human readable description with specified name. */
		if(strcasecmp(pAdapter->Description,iface_name) == 0) {
			*addr = apr_pstrdup(pool,pAdapter->IpAddressList.IpAddress.String);
			status = TRUE;
			break;
		}

		pAdapter = pAdapter->Next;
	}

	if (pAdapterInfo)
		free(pAdapterInfo);

#else

	struct ifaddrs *ifaddr, *ifa;
	int family, n;
	char host[NI_MAXHOST];

	if(getifaddrs(&ifaddr) == -1) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Interfaces");
		return FALSE;
	}

	/* Walk through linked list, maintaining head pointer. */
	for(ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
		if(ifa->ifa_addr == NULL) continue;

		family = ifa->ifa_addr->sa_family;
		if(family == AF_INET || family == AF_INET6) {
			if(strcasecmp(ifa->ifa_name,iface_name) == 0) {
				if(getnameinfo(ifa->ifa_addr,
								(family == AF_INET) ? sizeof(struct sockaddr_in) :
								sizeof(struct sockaddr_in6),
								host, NI_MAXHOST,
								NULL, 0, NI_NUMERICHOST) == 0) {
					*addr = apr_pstrdup(pool,host);
					status = TRUE;
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Name Info");
				}
				break;
			}
		}
	}

	freeifaddrs(ifaddr);
#endif

	if(status == TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Found Address %s by Interface [%s]", *addr, iface_name);
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Such Interface Found [%s]", iface_name);
	}
	return status;
}

/** Seconds from  Jan 1 1900 to Jan 1 1970 */
#define NTP_TIME_OFFSET 2208988800UL

/** Get current NTP time */
void apt_ntp_time_get(apr_uint32_t *sec, apr_uint32_t *frac)
{
	apr_uint32_t t;
	apr_uint32_t usec;
	
	apr_time_t now = apr_time_now();
	*sec = (apr_uint32_t)apr_time_sec(now) + NTP_TIME_OFFSET;

	usec = (apr_uint32_t) apr_time_usec(now);
	t = (usec * 1825) >> 5;
	*frac = ((usec << 12) + (usec << 8) - t);
}
