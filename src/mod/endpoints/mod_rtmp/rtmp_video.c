/*
 * mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2015, Seven Du.
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
 * The Original Code is rtmp_video for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Seven Du,
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 * Da Xiong <wavecb@gmail.com>
 *
 * rtmp_video.c -- RTMP video
 *
 */

#include <rtmp_video.h>


amf0_data * amf0_array_shift(amf0_data * data) {
	return (data != NULL) ? amf0_array_delete(data, amf0_array_first(data)) : NULL;
}


void rtmp2rtp_helper_init(rtmp2rtp_helper_t *helper)
{
	memset(helper, 0, sizeof(rtmp2rtp_helper_t));
	helper->nal_list = amf0_array_new();
	helper->pps = NULL;
	helper->sps = NULL;

}

void rtp2rtmp_helper_init(rtp2rtmp_helper_t *helper)
{
	memset(helper, 0, sizeof(rtp2rtmp_helper_t));
	helper->pps = NULL;
	helper->sps = NULL;
	helper->send = SWITCH_FALSE;
	helper->send_avc = SWITCH_FALSE;
	switch_buffer_create_dynamic(&helper->rtmp_buf, 10240, 10240, 0);
	switch_buffer_create_dynamic(&helper->fua_buf,  10240, 10240, 0);
}

void rtmp2rtp_helper_destroy(rtmp2rtp_helper_t *helper)
{
	amf0_data_free(helper->nal_list);
	amf0_data_free(helper->sps);
	amf0_data_free(helper->pps);
	helper = NULL;
}

void rtp2rtmp_helper_destroy(rtp2rtmp_helper_t *helper)
{

	amf0_data_free(helper->avc_conf);
	amf0_data_free(helper->sps);
	amf0_data_free(helper->pps);
	if (helper->rtmp_buf) switch_buffer_destroy(&helper->rtmp_buf);
	if (helper->fua_buf)  switch_buffer_destroy(&helper->fua_buf);
	helper = NULL;
}

switch_status_t on_rtmp_tech_init(switch_core_session_t *session, rtmp_private_t *tech_pvt)
{

	//for video
	tech_pvt->video_read_frame.packet = tech_pvt->video_databuf;
	tech_pvt->video_read_frame.data = tech_pvt->video_databuf + 12;
	tech_pvt->video_read_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE - 12;

	switch_mutex_init(&tech_pvt->video_readbuf_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	switch_buffer_create_dynamic(&tech_pvt->video_readbuf, 1024, 1024, 2048000);

	rtmp2rtp_helper_init(&tech_pvt->video_read_helper);
	rtp2rtmp_helper_init(&tech_pvt->video_write_helper);
	tech_pvt->video_write_helper.last_mark = 1;
	tech_pvt->video_codec = 0xB2;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t on_rtmp_destroy(rtmp_private_t *tech_pvt)
{

	if (tech_pvt) {
		//for video

		if (switch_core_codec_ready(&tech_pvt->video_read_codec)) {
			switch_core_codec_destroy(&tech_pvt->video_read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->video_write_codec)) {
			switch_core_codec_destroy(&tech_pvt->video_write_codec);
		}

		rtmp2rtp_helper_destroy(&tech_pvt->video_read_helper);
		rtp2rtmp_helper_destroy(&tech_pvt->video_write_helper);
		switch_buffer_destroy(&tech_pvt->video_readbuf);

		switch_media_handle_destroy(tech_pvt->session);
	}

	return SWITCH_STATUS_SUCCESS;
}


/*Rtmp packet to rtp frame*/
switch_status_t rtmp_rtmp2rtpH264(rtmp2rtp_helper_t  *read_helper, uint8_t* data, uint32_t len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint8_t *end = data + len;

	if (data[0] == 0x17 && data[1] == 0) {
		switch_byte_t *pdata = data + 2;
		int cfgVer = pdata[3];
		if (cfgVer == 1) {
			int i = 0;
			int numSPS = 0;
			int numPPS = 0;
			int lenSize = (pdata[7] & 0x03) + 1;
			int lenSPS;
			int lenPPS;
			//sps
			numSPS = pdata[8] & 0x1f;
			pdata += 9;
			for (i = 0; i < numSPS; i++) {
				lenSPS = ntohs(*(uint16_t *)pdata);
				pdata += 2;

				if (lenSPS > end - pdata) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "corrupted data\n");
					return SWITCH_STATUS_FALSE;
				}

				if (read_helper->sps == NULL) {
					read_helper->sps = amf0_string_new(pdata, lenSPS);
				}
				pdata += lenSPS;
			}
			//pps
			numPPS = pdata[0];
			pdata += 1;
			for (i = 0; i < numPPS; i++) {
				lenPPS = ntohs(*(uint16_t *)pdata);
				pdata += 2;
				if (lenPPS > end - pdata) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "corrupted data\n");
					return SWITCH_STATUS_FALSE;
				}

				if (read_helper->pps == NULL) {
					read_helper->pps = amf0_string_new(pdata, lenPPS);
				}
				pdata += lenPPS;
			}

			read_helper->lenSize = lenSize;

			// add sps to list
			if (read_helper->sps != NULL) {
				amf0_data *sps = amf0_string_new(
					amf0_string_get_uint8_ts(read_helper->sps),
					amf0_string_get_size(read_helper->sps));

				amf0_array_push(read_helper->nal_list, sps);

			}
			// add pps to list
			if (read_helper->pps != NULL) {
				amf0_data *pps = amf0_string_new(
				amf0_string_get_uint8_ts(read_helper->pps),
				amf0_string_get_size(read_helper->pps));
				amf0_array_push(read_helper->nal_list, pps);
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Unsuported cfgVer=%d" , cfgVer);
		}
	} else if ((data[0] == 0x17 || data[0] == 0x27) && data[1] == 1) {
		if (read_helper->sps && read_helper->pps) {
			switch_byte_t * pdata = data + 5;
			uint32_t  pdata_len = len - 5;
			uint32_t  lenSize = read_helper->lenSize;
			switch_byte_t  *nal_buf = NULL;
			uint32_t        nal_len = 0;

			while (pdata_len > 0) {
				uint32_t nalSize = 0;
				switch (lenSize) {
				case 1:
					nalSize = pdata[lenSize - 1] & 0xff;
					break;
				case 2:
					nalSize = ((pdata[lenSize - 2] & 0xff) << 8) | (pdata[lenSize - 1] & 0xff);
					break;
				case 4:
					nalSize = (pdata[lenSize - 4] & 0xff) << 24 |
						(pdata[lenSize - 3] & 0xff) << 16 |
						(pdata[lenSize - 2] & 0xff) << 8  |
						(pdata[lenSize - 1] & 0xff);
					break;
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid length size: %d" , lenSize);
					return SWITCH_STATUS_FALSE;
				}

				nal_buf = pdata + lenSize;
				nal_len = nalSize;

				//next nal
				pdata = pdata + lenSize + nalSize;
				pdata_len -= (lenSize + nalSize);
			}

			if ((nal_len > 0 && nal_len < len) && nal_buf != NULL) {

				switch_byte_t * remaining = nal_buf;
				int32_t  remaining_len = nal_len;
				int nalType = remaining[0] & 0x1f;
				int nri = remaining[0] & 0x60;

				if (nalType == 5 || nalType == 1) {
					if (remaining_len < MAX_RTP_PAYLOAD_SIZE) {
						amf0_array_push(read_helper->nal_list,  amf0_string_new(remaining, remaining_len));
					} else {
						switch_byte_t start = (uint8_t) 0x80;
						remaining += 1;
						remaining_len -= 1;

						while (remaining_len > 0) {
							int32_t payload_len = (MAX_RTP_PAYLOAD_SIZE - 2) < remaining_len ? (MAX_RTP_PAYLOAD_SIZE - 2) : remaining_len;

							switch_byte_t payload[MAX_RTP_PAYLOAD_SIZE];
							switch_byte_t end;

							memcpy(payload + 2, remaining, payload_len);
							remaining_len -= payload_len;
							remaining += payload_len;

							end = (switch_byte_t) ((remaining_len > 0) ? 0 : 0x40);
							payload[0] = nri | 28; // FU-A
							payload[1] = start | end | nalType;

							amf0_array_push(read_helper->nal_list, amf0_string_new(payload, payload_len + 2));

							start = 0;
						}
					}
				}

			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing rtmp data\n");
	}

	return status;
}

switch_bool_t sps_changed(amf0_data *data, uint8_t *new, int datalen)
{
	uint8_t *old;
	int i = 0;;

	if (!data) return SWITCH_TRUE;
	if (datalen != amf0_string_get_size(data)) return SWITCH_TRUE;

	old = amf0_string_get_uint8_ts(data);

	while(i < datalen) {
		if (*(old + i) != *(new + i)) return SWITCH_TRUE;
		i++;
	}

	return SWITCH_FALSE;
}

switch_status_t rtmp_rtp2rtmpH264(rtp2rtmp_helper_t *helper, switch_frame_t *frame)
{
	uint8_t* packet = frame->packet;
	// uint32_t len = frame->packetlen;
	switch_rtp_hdr_t *raw_rtp = (switch_rtp_hdr_t *)packet;
	switch_byte_t *payload = frame->data;
	int datalen = frame->datalen;
	int nalType = payload[0] & 0x1f;
	uint32_t size = 0;
	uint16_t rtp_seq = 0;
	uint32_t rtp_ts = 0;
	static const uint8_t rtmp_header17[] = {0x17, 1, 0, 0, 0};
	static const uint8_t rtmp_header27[] = {0x27, 1, 0, 0, 0};

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
	// 	"read: %-4u: %02x %02x ts:%u seq:%u %s\n",
	// 	len, payload[0], payload[1], rtp_ts, rtp_seq, raw_rtp->m ? " mark" : "");

	if (switch_test_flag(frame, SFF_RAW_RTP) && !switch_test_flag(frame, SFF_RAW_RTP_PARSE_FRAME)) {
		rtp_seq = ntohs(raw_rtp->seq);
		rtp_ts = ntohl(raw_rtp->ts);

		if (helper->last_seq && helper->last_seq + 1 != rtp_seq) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "possible video rtp packet loss? seq: %u - %u - 1 = %d ts: %u - %u = %d\n",
				ntohs(raw_rtp->seq), helper->last_seq, (int)(rtp_seq - helper->last_seq - 1),
				ntohl(raw_rtp->ts), helper->last_recv_ts, (int)(rtp_ts - helper->last_recv_ts));

			if (nalType != 7) {
				if (helper->sps) {
					amf0_data_free(helper->sps);
					helper->sps = NULL;
				}
				helper->last_recv_ts = rtp_ts;
				helper->last_mark = raw_rtp->m;
				helper->last_seq = rtp_seq;
				goto wait_sps;
			}
		}
	}

	if (helper->last_recv_ts != frame->timestamp) {
		switch_buffer_zero(helper->rtmp_buf);
		switch_buffer_zero(helper->fua_buf);
	}

	helper->last_recv_ts = frame->timestamp;
	helper->last_mark = frame->m;
	helper->last_seq = rtp_seq;

	switch (nalType) {
	case 7: //sps
		if (sps_changed(helper->sps, payload, datalen)) {
			amf0_data_free(helper->sps);
			helper->sps = amf0_string_new(payload, datalen);
			helper->sps_changed++;
		} else {
			helper->sps_changed = 0;
		}
		break;
	case 8: //pps
		amf0_data_free(helper->pps);
		helper->pps = amf0_string_new(payload, datalen);
		break;
	case 1: //Non IDR
		size = htonl(datalen);
		if (switch_buffer_inuse(helper->rtmp_buf) == 0)
			switch_buffer_write(helper->rtmp_buf, rtmp_header27,  sizeof(rtmp_header27));
		switch_buffer_write(helper->rtmp_buf, &size, sizeof(uint32_t));
		switch_buffer_write(helper->rtmp_buf, payload, datalen);
		break;
	case 5: //IDR
		size = htonl(datalen);
		if (switch_buffer_inuse(helper->rtmp_buf) == 0)
			switch_buffer_write(helper->rtmp_buf, rtmp_header17, sizeof(rtmp_header17));
		switch_buffer_write(helper->rtmp_buf, &size, sizeof(uint32_t));
		switch_buffer_write(helper->rtmp_buf, payload, datalen);
		break;
	case 28: //FU-A
		{
			uint8_t *q = payload;
			uint8_t h264_start_bit = q[1] & 0x80;
			uint8_t h264_end_bit   = q[1] & 0x40;
			uint8_t h264_type      = q[1] & 0x1F;
			uint8_t h264_nri       = (q[0] & 0x60) >> 5;
			uint8_t h264_key       = (h264_nri << 5) | h264_type;

			if (h264_start_bit) {
				/* write NAL unit code */
				switch_buffer_write(helper->fua_buf, &h264_key, sizeof(h264_key));
			}

			switch_buffer_write(helper->fua_buf, q + 2, datalen - 2);

			if (h264_end_bit) {
				const void * nal_data;

				uint32_t used = switch_buffer_inuse(helper->fua_buf);
				uint32_t used_big = htonl(used);
				switch_buffer_peek_zerocopy(helper->fua_buf, &nal_data);

				nalType = ((uint8_t*)nal_data)[0] & 0x1f;
				if (switch_buffer_inuse(helper->rtmp_buf) == 0) {
					if (nalType == 5)
						switch_buffer_write(helper->rtmp_buf, rtmp_header17, sizeof(rtmp_header17));
					else
						switch_buffer_write(helper->rtmp_buf, rtmp_header27,  sizeof(rtmp_header27));
				}

				switch_buffer_write(helper->rtmp_buf, &used_big, sizeof(uint32_t));
				switch_buffer_write(helper->rtmp_buf, nal_data, used);
				switch_buffer_zero(helper->fua_buf);
			}

		}
		break;
	case 24:
		 {// for aggregated SPS and PPSs
			uint8_t *q = payload + 1;
			uint16_t nalu_size = 0;
			int nt = 0;
			int nidx = 0;
			while (nidx < datalen - 1) {
				/* get NALU size */
				nalu_size = (q[nidx] << 8) | (q[nidx + 1]);

				nidx += 2;

				if (nalu_size == 0) {
					nidx++;
					continue;
				}

				/* write NALU data */
				nt = q[nidx] & 0x1f;
				switch (nt) {
				case 1: //Non IDR
					size = htonl(nalu_size);
					if (switch_buffer_inuse(helper->rtmp_buf) == 0)
							switch_buffer_write(helper->rtmp_buf, rtmp_header27,  sizeof(rtmp_header27));
					switch_buffer_write(helper->rtmp_buf, &size, sizeof(uint32_t));
					switch_buffer_write(helper->rtmp_buf, q + nidx, nalu_size);
					break;
				case 5:	// IDR
					size = htonl(nalu_size);
					if (switch_buffer_inuse(helper->rtmp_buf) == 0)
						switch_buffer_write(helper->rtmp_buf, rtmp_header17, sizeof(rtmp_header17));

					switch_buffer_write(helper->rtmp_buf, &size, sizeof(uint32_t));
					switch_buffer_write(helper->rtmp_buf, q + nidx, nalu_size);
					break;
				case 7: //sps
					amf0_data_free(helper->sps);
					helper->sps = amf0_string_new( q + nidx, nalu_size);
					break;
				case 8: //pps
					amf0_data_free(helper->pps);
					helper->pps = amf0_string_new(q + nidx, nalu_size);
					break;
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported NAL %d in STAP-A\n", nt);
					break;
				}
				nidx += nalu_size;
			}
		}
		break;

	case 6:
		break;

	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported NAL %d\n", nalType);
		break;
	}

	// build the avc seq
	if (helper->sps_changed && helper->sps != NULL && helper->pps != NULL) {

		int i = 0;
		uint16_t size;
		uint8_t *sps = amf0_string_get_uint8_ts(helper->sps);
		unsigned char buf[AMF_MAX_SIZE * 2]; /* make sure the buffer is big enough */

		buf[i++] = 0x17;   // i = 0
		buf[i++] = 0;      // 0 for sps/pps packet
		buf[i++] = 0;      // timestamp
		buf[i++] = 0;      // timestamp
		buf[i++] = 0;      // timestamp
		buf[i++] = 1;      // AVC Decode Configuration Version
		buf[i++] = sps[1]; // H264 profile 0x42 = Baseline
		buf[i++] = sps[2]; // Compatiable Level
		buf[i++] = sps[3]; // H264 profile 0x1e = profile 30, 0x1f = profile 31
		buf[i++] = 0xff;   // 111111 11   0B11 = 3 = lengthSizeMinusOne, LengtSize = 4
		buf[i++] = 0xe1;   // i = 10, number of sps = 1

		// 2 bytes sps size
		size = htons(amf0_string_get_size(helper->sps));
		memcpy(buf + i, &size, 2);
		i += 2;
		// sps data
		memcpy(buf + i, sps, amf0_string_get_size(helper->sps));
		buf[i] = 0x67; // set sps header, eyebeam sends 0x27, we set nri = 3, set it to be most important
		i += amf0_string_get_size(helper->sps);

		buf[i++] = 0x01; // number of pps

		// 2 bytes pps size
		size = htons(amf0_string_get_size(helper->pps));
		memcpy(buf + i, &size, 2);
		i += 2;
		// pps data
		memcpy(buf + i, amf0_string_get_uint8_ts(helper->pps), amf0_string_get_size(helper->pps));
		buf[i] = 0x68; // set pps header
		i += amf0_string_get_size(helper->pps);

		amf0_data_free(helper->avc_conf);
		helper->avc_conf = amf0_string_new(buf, i);
		helper->send_avc = SWITCH_TRUE;
	}

	if (frame->m) {
		if (helper->avc_conf) {
			helper->send = SWITCH_TRUE;
		} else {

wait_sps:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "waiting for sps and pps\n");
			switch_buffer_zero(helper->rtmp_buf);
			switch_buffer_zero(helper->fua_buf);
		    helper->send = SWITCH_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{

	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;
	rtmp_session_t *rsession = NULL;
	switch_time_t ts = 0;
	rtp2rtmp_helper_t *helper;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	helper = &tech_pvt->video_write_helper;
	assert(helper != NULL);

	rsession = tech_pvt->rtmp_session;

	if (rsession == NULL) {
		return SWITCH_STATUS_FALSE;
	}

	//emulate lost packets
	// if (frame->seq > 0 && frame->seq % 20 == 0) return SWITCH_STATUS_SUCCESS;

	switch_thread_rwlock_wrlock(rsession->rwlock);

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "TFLAG_IO not set\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_test_flag(tech_pvt, TFLAG_DETACHED) || !switch_test_flag(tech_pvt->rtmp_session, SFLAG_VIDEO)) {
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	}

	if (!tech_pvt->rtmp_session || !tech_pvt->video_codec || !tech_pvt->write_channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing mandatory value\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (tech_pvt->rtmp_session->state >= RS_DESTROY) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (frame->flags & SFF_CNG) {
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	}

	rtmp_rtp2rtmpH264(helper, frame);

	if (helper->send) {
		uint16_t used = switch_buffer_inuse(helper->rtmp_buf);
		const void *rtmp_data = NULL;

		switch_buffer_peek_zerocopy(helper->rtmp_buf, &rtmp_data);

		if (!tech_pvt->stream_start_ts) {
			tech_pvt->stream_start_ts = switch_micro_time_now() / 1000;
			ts = 0;
		} else {
			ts = (switch_micro_time_now() / 1000) - tech_pvt->stream_start_ts;
		}

#if 0
		{ /* use timestamp read from the frame */
			uint32_t timestamp = frame->timestamp & 0xFFFFFFFF;
			ts = timestamp / 90;
		}
#endif

		if (ts == tech_pvt->stream_last_ts) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "dup ts: %" SWITCH_TIME_T_FMT "\n", ts);
			ts += 1;
			if (ts == 1) ts = 0;
		}

		tech_pvt->stream_last_ts = ts;

		if (!rtmp_data) {
			goto skip;
		}

		if (((uint8_t *)rtmp_data)[0] == 0x17 && helper->send_avc) {
			uint8_t *avc_conf = amf0_string_get_uint8_ts(helper->avc_conf);

			rtmp_send_message(tech_pvt->rtmp_session, RTMP_DEFAULT_STREAM_VIDEO, ts,
				RTMP_TYPE_VIDEO, tech_pvt->rtmp_session->media_streamid, avc_conf, amf0_string_get_size(helper->avc_conf), 0);
			helper->send_avc = SWITCH_FALSE;
		}

		status = rtmp_send_message(tech_pvt->rtmp_session, RTMP_DEFAULT_STREAM_VIDEO, ts,
			RTMP_TYPE_VIDEO, tech_pvt->rtmp_session->media_streamid, rtmp_data, used, 0);

		// if dropped_video_frame > N then ask the far end for a new IDR for each N frames
		if (rsession->dropped_video_frame > 0 && rsession->dropped_video_frame % 90 == 0) {
			switch_core_session_t *other_session;
			if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_request_video_refresh(session);
				switch_core_session_rwunlock(other_session);
			}
		}
skip:
		switch_buffer_zero(helper->rtmp_buf);
		switch_buffer_zero(helper->fua_buf);
		helper->send = SWITCH_FALSE;
	}

end:
	switch_thread_rwlock_unlock(rsession->rwlock);
	return status;
}


switch_status_t rtmp_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;
	uint16_t len;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (tech_pvt->rtmp_session->state >= RS_DESTROY) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(tech_pvt, TFLAG_DETACHED)) {
		switch_yield(20000);
		goto cng;
	}

	tech_pvt->video_read_frame.flags = SFF_RAW_RTP;
	tech_pvt->video_read_frame.codec = &tech_pvt->video_read_codec;

	if (amf0_array_size(tech_pvt->video_read_helper.nal_list) > 0) {
		goto wr_frame;
	}

	if (switch_buffer_inuse(tech_pvt->video_readbuf) < 2) {
		switch_yield(20000);
		switch_cond_next();
	}

	if (switch_buffer_inuse(tech_pvt->video_readbuf) < 2) {
		switch_yield(20000);
		goto cng;
	} else {
		switch_mutex_lock(tech_pvt->video_readbuf_mutex);
		switch_buffer_peek(tech_pvt->video_readbuf, &len, 2);
		if (switch_buffer_inuse(tech_pvt->video_readbuf) >= len) {
			if (len == 0) {
				switch_mutex_unlock(tech_pvt->video_readbuf_mutex);
				switch_yield(20000);
				goto cng;
			} else {
				const void *data = NULL;
				switch_buffer_toss(tech_pvt->video_readbuf, 2);
				switch_buffer_read(tech_pvt->video_readbuf, &tech_pvt->video_read_ts, 4);
				tech_pvt->video_read_ts *= 90;
				switch_buffer_peek_zerocopy(tech_pvt->video_readbuf, &data);
				rtmp_rtmp2rtpH264(&tech_pvt->video_read_helper, (uint8_t *)data, len);
				switch_buffer_toss(tech_pvt->video_readbuf, len);

				if (amf0_array_size(tech_pvt->video_read_helper.nal_list) == 0) {
					switch_mutex_unlock(tech_pvt->video_readbuf_mutex);
					switch_yield(20000);
					goto cng;
				}
			}
		}
		switch_mutex_unlock(tech_pvt->video_readbuf_mutex);
	}

wr_frame:
	{
		amf0_data *amf_data;
		amf_data = amf0_array_shift(tech_pvt->video_read_helper.nal_list);
		if (amf_data) {
			int data_size = amf0_string_get_size(amf_data);
			if (data_size > 1500) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "data size too large: %d\n", data_size);
				amf0_data_free(amf_data);
				goto cng;
			}

			memcpy(tech_pvt->video_read_frame.data, amf0_string_get_uint8_ts(amf_data), data_size);
			tech_pvt->video_read_frame.datalen = data_size;
			tech_pvt->video_read_frame.packetlen = data_size + 12;
			amf0_data_free(amf_data);
		} else {
			switch_yield(20000);
			goto cng;
		}
	}

	{ /* set the marker bit on the last packet*/
		uint8_t *p = tech_pvt->video_read_frame.data;
		uint8_t fragment_type = p[0] & 0x1f;
		uint8_t end_bit = p[1] & 0x40;
		switch_rtp_hdr_t *rtp_hdr = tech_pvt->video_read_frame.packet;

		if (fragment_type == 28) {
			tech_pvt->video_read_frame.m = end_bit == 0x40 ? SWITCH_TRUE : SWITCH_FALSE;
		} else {
			tech_pvt->video_read_frame.m = SWITCH_TRUE;
		}

		rtp_hdr->version = 2;
		rtp_hdr->p = 0;
		rtp_hdr->x = 0;
		rtp_hdr->ts = htonl(tech_pvt->video_read_ts);
		rtp_hdr->m = tech_pvt->video_read_frame.m;
		rtp_hdr->seq = htons(tech_pvt->seq++);
		if (rtp_hdr->ssrc == 0) rtp_hdr->ssrc = (uint32_t) (intptr_t) tech_pvt;
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read %2x %2x %u %u\n", p[0], p[1], tech_pvt->video_read_ts, rtp_hdr->ssrc);
	}

	*frame = &tech_pvt->video_read_frame;
	(*frame)->img = NULL;
	return SWITCH_STATUS_SUCCESS;

cng:
	tech_pvt->video_read_frame.datalen = 0;
	tech_pvt->video_read_frame.flags = SFF_CNG;
	tech_pvt->video_read_frame.codec = &tech_pvt->video_read_codec;

	*frame = &tech_pvt->video_read_frame;

	return SWITCH_STATUS_SUCCESS;
}
