/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <seven@signalwire.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * switch_packetizer.c unit test
 *
 */

#include <test/switch_test.h>
#include <switch_packetizer.h>

#define SLICE_SIZE 4

FST_CORE_BEGIN("conf")
{
	FST_SUITE_BEGIN(switch_packetizer)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(test_packetizer_bitstream)
		{
			switch_packetizer_t *packetizer = switch_packetizer_create(SPT_H264_BITSTREAM, SLICE_SIZE);
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
			switch_frame_t frame = {0};
			switch_status_t status;
			uint8_t h264data[] = {0, 0, 0, 1, 0x67, 1, 2, 0, 0, 0, 1, 0x68, 1, 2, 0, 0, 0, 1, 0x65, 1, 2, 3, 4, 5, 6};

			frame.data = data;
			frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			switch_set_flag(&frame, SFF_ENCODED);

			status = switch_packetizer_feed(packetizer, h264data, sizeof(h264data));
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 4);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 4);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(frame.datalen == 4);
			switch_packetizer_close(&packetizer);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_packetizer_sized_bitstream_has_sps_pps)
		{
			switch_packetizer_t *packetizer = switch_packetizer_create(SPT_H264_SIZED_BITSTREAM, SLICE_SIZE);
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
			switch_frame_t frame = {0};
			switch_status_t status;
			uint8_t h264data[] = {0, 0, 0, 3, 0x67, 1, 2, 0, 0, 0, 3, 0x68, 1, 2, 0, 0, 0, 7, 0x65, 1, 2, 3, 4, 5, 6};

			frame.data = data;
			frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			switch_set_flag(&frame, SFF_ENCODED);

			status = switch_packetizer_feed(packetizer, h264data, sizeof(h264data));
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 4);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 4);
			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(frame.datalen == 4);
			fst_check(frame.m == 1);
			switch_packetizer_close(&packetizer);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_packetizer_sized_bitstream_no_sps_pps)
		{
			switch_packetizer_t *packetizer = switch_packetizer_create(SPT_H264_SIZED_BITSTREAM, SLICE_SIZE);
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
			switch_frame_t frame = {0};
			switch_status_t status;
			uint8_t h264data[] = {0, 0, 0, 3, 0x06, 1, 2, 0, 0, 0, 3, 0x09, 1, 2, 0, 0, 0, 7, 0x65, 1, 2, 3, 4, 5, 6};
			uint8_t extradata[] = {0x01, 0x64, 0x00, 0x1e, 0xff, 0xe1, 0x00, 0x03, 0x67, 0x64, 0x00, 0xe1, 0x00, 0x03, 0x68, 0x01, 0x02};
//																	1 fps       3 bytes                 1pps        3 bytes
			frame.data = data;
			frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			switch_set_flag(&frame, SFF_ENCODED);

			status = switch_packetizer_feed_extradata(packetizer, extradata, sizeof(extradata));
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			status = switch_packetizer_feed(packetizer, h264data, sizeof(h264data));
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u\n", frame.datalen);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x06);

			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u %x\n", frame.datalen, *(uint8_t *)frame.data);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x09);

			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u %x\n", frame.datalen, *(uint8_t *)frame.data);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x07);

			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u %x\n", frame.datalen, *(uint8_t *)frame.data);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 3);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x08);

			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u %x\n", frame.datalen, *(uint8_t *)frame.data);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 4);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x1c);
			fst_check(frame.m == 0);

			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u %x\n", frame.datalen, *(uint8_t *)frame.data);
			fst_requires(status == SWITCH_STATUS_MORE_DATA);
			fst_requires(frame.datalen == 4);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x1c);
			fst_check(frame.m == 0);

			frame.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u %x\n", frame.datalen, *(uint8_t *)frame.data);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(frame.datalen == 4);
			fst_check((*(uint8_t *)frame.data & 0x1f) == 0x1c);
			fst_check(frame.m == 1);
			switch_packetizer_close(&packetizer);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_packetizer_invalid)
		{
			switch_packetizer_t *packetizer = switch_packetizer_create(SPT_H264_BITSTREAM, SLICE_SIZE);
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
			switch_frame_t frame = {0};
			switch_status_t status;
			uint8_t h264data[] = {0, 0, 2, 9, 0x67, 1, 2, 0, 0, 0, 0, 0x68, 1, 2, 0, 0, 0, 0, 0x65, 1, 2, 3, 4, 5, 6};

			frame.data = data;
			frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			switch_set_flag(&frame, SFF_ENCODED);

			status = switch_packetizer_feed(packetizer, h264data, sizeof(h264data));
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			status = switch_packetizer_read(packetizer, &frame);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "status = %d datalen = %u\n", status, frame.datalen);
			fst_requires(status == SWITCH_STATUS_FALSE);
			switch_packetizer_close(&packetizer);
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
