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
 * $Id: mpf_rtp_attribs.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "apt_string_table.h"
#include "mpf_rtp_attribs.h"

/** String table of RTP attributes (mpf_rtp_attrib_e) */
static const apt_str_table_item_t mpf_rtp_attrib_table[] = {
	{{"rtpmap",   6},1},
	{{"sendonly", 8},8},
	{{"recvonly", 8},2},
	{{"sendrecv", 8},4},
	{{"mid",      3},0},
	{{"ptime",    5},0}
};


MPF_DECLARE(const apt_str_t*) mpf_rtp_attrib_str_get(mpf_rtp_attrib_e attrib_id)
{
	return apt_string_table_str_get(mpf_rtp_attrib_table,RTP_ATTRIB_COUNT,attrib_id);
}

MPF_DECLARE(mpf_rtp_attrib_e) mpf_rtp_attrib_id_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mpf_rtp_attrib_table,RTP_ATTRIB_COUNT,attrib);
}

MPF_DECLARE(const apt_str_t*) mpf_rtp_direction_str_get(mpf_stream_direction_e direction)
{
	mpf_rtp_attrib_e attrib_id = RTP_ATTRIB_UNKNOWN;
	switch(direction) {
		case STREAM_DIRECTION_SEND:
			attrib_id = RTP_ATTRIB_SENDONLY;
			break;
		case STREAM_DIRECTION_RECEIVE:
			attrib_id = RTP_ATTRIB_RECVONLY;
			break;
		case STREAM_DIRECTION_DUPLEX:
			attrib_id = RTP_ATTRIB_SENDRECV;
			break;
		default:
			break;
	}
	return apt_string_table_str_get(mpf_rtp_attrib_table,RTP_ATTRIB_COUNT,attrib_id);
}
