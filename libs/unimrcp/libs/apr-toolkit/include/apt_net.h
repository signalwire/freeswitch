/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: apt_net.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_NET_H
#define APT_NET_H

/**
 * @file apt_net.h
 * @brief Network Utilities
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/**
 * Get the IP address (in numeric address string format) by hostname.
 * @param addr the IP address to return
 * @param pool the pool to allocate memory from
 */
apt_bool_t apt_ip_get(char **addr, apr_pool_t *pool);

/**
 * Get the IP address (in numeric address string format) by network interface name.
 * @param iface_name the network interface name (eth0, Local Area Connection, etc)
 * @param addr the IP address to return
 * @param pool the pool to allocate memory from
 */
apt_bool_t apt_ip_get_by_iface(const char *iface_name, char **addr, apr_pool_t *pool);

/**
 * Get current NTP time
 * @param sec the seconds of the NTP time to return
 * @param frac the fractions of the NTP time to return
 */
void apt_ntp_time_get(apr_uint32_t *sec, apr_uint32_t *frac);

APT_END_EXTERN_C

#endif /* APT_NET_H */
