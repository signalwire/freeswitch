/*
 * Copyright 2008 Arsen Chaloyan
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

/** Get the IP address (in numeric address string format) by hostname */
apt_bool_t apt_ip_get(char **addr, apr_pool_t *pool)
{
	apr_sockaddr_t *sockaddr = NULL;
	char *hostname = apr_palloc(pool,APRMAXHOSTLEN+1);
	if(apr_gethostname(hostname,APRMAXHOSTLEN,pool) != APR_SUCCESS) {
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
