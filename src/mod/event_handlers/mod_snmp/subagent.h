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
 * Portions created by Seventh Signal Ltd. & Co. KG and its employees are Copyright (C)
 * Seventh Signal Ltd. & Co. KG, All Rights Reserverd.
 *
 * Contributor(s):
 * Daniel Swarbrick <daniel.swarbrick@gmail.com>
 * Stefan Knoblich <s.knoblich@axsentis.de>
 * 
 * subagent.h -- SNMP Agent Definitions
 *
 */

#ifndef subagent_H
#define subagent_H

/* .1.3.6.1.4.1.27880.1.1 */
#define ID_VERSION_STR			1
#define ID_UUID				2

/* .1.3.6.1.4.1.27880.1.2 */
#define SS_UPTIME			1
#define SS_SESSIONS_SINCE_STARTUP	2
#define SS_CURRENT_SESSIONS		3
#define SS_MAX_SESSIONS			4
#define SS_CURRENT_CALLS		5
#define SS_SESSIONS_PER_SECOND		6
#define SS_MAX_SESSIONS_PER_SECOND	7
#define SS_PEAK_SESSIONS_PER_SECOND	8
#define SS_PEAK_SESSIONS_PER_FIVEMIN	9
#define SS_PEAK_SESSIONS		10
#define SS_PEAK_SESSIONS_FIVEMIN	11

/* .1.3.6.1.4.1.27880.1.9 */
#define CH_INDEX			1
#define CH_UUID				2
#define CH_DIRECTION			3
#define CH_CREATED			4
#define CH_NAME				5
#define CH_STATE			6
#define CH_CID_NAME			7
#define CH_CID_NUM			8
#define CH_IP_ADDR_TYPE			9
#define CH_IP_ADDR			10
#define CH_DEST				11
#define CH_APPLICATION			12
#define CH_APPLICATION_DATA		13
#define CH_DIALPLAN			14
#define CH_CONTEXT			15
#define CH_READ_CODEC			16
#define CH_READ_RATE			17
#define CH_READ_BITRATE			18
#define CH_WRITE_CODEC			19
#define CH_WRITE_RATE			20
#define CH_WRITE_BITRATE		21

/* Why aren't these in net-snmp-includes.h ? */
#define INETADDRESSTYPE_UNKNOWN		0
#define INETADDRESSTYPE_IPV4		1
#define INETADDRESSTYPE_IPV6		2
#define INETADDRESSTYPE_IPV4Z		3
#define INETADDRESSTYPE_IPV6Z		4
#define INETADDRESSTYPE_DNS		16

typedef struct {
	uint32_t idx;
	char uuid[38];
	char direction[32];
	time_t created_epoch;
	char name[1024];
	char state[64];
	char cid_name[1024];
	char cid_num[256];
	ip_t ip_addr;
	uint8_t addr_family;
	char dest[1024];
	char application[128];
	char application_data[4096];
	char dialplan[128];
	char context[128];
	char read_codec[128];
	uint32_t read_rate;
	uint32_t read_bitrate;
	char write_codec[128];
	uint32_t write_rate;
	uint32_t write_bitrate;
} chan_entry_t;

void init_subagent(switch_memory_pool_t *pool);
Netsnmp_Node_Handler handle_identity;
Netsnmp_Node_Handler handle_systemStats;
Netsnmp_Node_Handler handle_channelList;

#endif /* subagent_H */
